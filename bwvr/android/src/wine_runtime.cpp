// Wine in the headset app (wine_runtime.h, docs/android.md, "Wine in the
// app").
#include "wine_runtime.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "app_log.h"
#include "game_folder.h"

extern char** environ;

namespace {

double NowMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

bool Exists(const std::string& path) {
  struct stat st;
  return lstat(path.c_str(), &st) == 0;
}

// mkdir -p, with the mode 0700: only the app reads its tree and its prefix.
bool MakeDirs(const std::string& path) {
  for (size_t at = 1; at <= path.size(); ++at) {
    if (at == path.size() || path[at] == '/') {
      const std::string part = path.substr(0, at);
      if (mkdir(part.c_str(), 0700) != 0 && errno != EEXIST) return false;
    }
  }
  return true;
}

int RemoveOne(const char* path, const struct stat*, int type, struct FTW*) {
  return type == FTW_DP ? rmdir(path) : unlink(path);
}

// Deletes a folder of the app (the old tree) with each file in it. It does
// not follow a symbolic link.
bool RemoveTree(const std::string& path) {
  if (!Exists(path)) return true;
  return nftw(path.c_str(), RemoveOne, 32, FTW_DEPTH | FTW_PHYS) == 0;
}

std::string ReadAsset(AAssetManager* assets, const std::string& name, bool* found) {
  *found = false;
  AAsset* a = AAssetManager_open(assets, name.c_str(), AASSET_MODE_BUFFER);
  if (a == nullptr) return std::string();
  *found = true;
  std::string text(static_cast<size_t>(AAsset_getLength(a)), '\0');
  const int n = AAsset_read(a, text.empty() ? nullptr : &text[0], text.size());
  AAsset_close(a);
  if (n < 0) return std::string();
  text.resize(static_cast<size_t>(n));
  return text;
}

// Reads exactly n bytes of a stream of the asset manager. False at the end
// or a fault.
bool ReadFull(AAsset* a, uint8_t* to, size_t n) {
  size_t done = 0;
  while (done < n) {
    const int k = AAsset_read(a, to + done, n - done);
    if (k <= 0) return false;
    done += static_cast<size_t>(k);
  }
  return true;
}

bool CopyFile(const std::string& from, const std::string& to) {
  const int in = open(from.c_str(), O_RDONLY | O_CLOEXEC);
  if (in < 0) return false;
  const std::string tmp = to + ".tmp";
  const int out = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  bool ok = out >= 0;
  static uint8_t buffer[1 << 16];
  while (ok) {
    const ssize_t n = read(in, buffer, sizeof(buffer));
    if (n == 0) break;
    ok = n > 0 && write(out, buffer, static_cast<size_t>(n)) == n;
  }
  close(in);
  if (out >= 0 && close(out) != 0) ok = false;
  return ok && rename(tmp.c_str(), to.c_str()) == 0;
}


}  // namespace

const char* WineRuntime::Abi() {
#if defined(__aarch64__)
  return "arm64-v8a";
#elif defined(__x86_64__)
  return "x86_64";
#else
  return "";
#endif
}

void WineRuntime::Start(AAssetManager* assets, const std::string& files,
                        const std::string& app_folder, const std::string& game_folder,
                        const Options& options) {
  assets_ = assets;
  files_ = files;
  game_ = game_folder;
  abi_ = Abi();
  options_ = options;
  paths_ = wine_rules::MakePaths(files, app_folder, abi_);
  std::vector<std::string> inherited;
  for (char** e = environ; e != nullptr && *e != nullptr; ++e) inherited.push_back(*e);
  env_ = wine_rules::Environment(inherited, paths_, files, options.debug);
  if (!options.setup) {
    AppLog("wine         : wine_setup=0. The app does not set up Wine");
    return;
  }
  thread_ = std::thread(&WineRuntime::Run, this);
}

std::string WineRuntime::Status() {
  std::lock_guard<std::mutex> hold(lock_);
  return status_;
}

void WineRuntime::SetStatus(const std::string& text) {
  std::lock_guard<std::mutex> hold(lock_);
  status_ = text;
}

bool WineRuntime::Fault(const std::string& text) {
  AppLog("wine         : FAULT: %s", text.c_str());
  SetStatus("Wine: " + text);
  return false;
}

void WineRuntime::Stop() {
  // The thread stops between its steps, and a program that it waits for
  // gets SIGKILL. wineserver -k then ends each other process of Wine, also
  // the ones of an earlier start of the app.
  stop_ = true;
  if (thread_.joinable()) thread_.join();
  if (!paths_.server.empty() && Exists(paths_.server)) {
    double ms = 0.0;
    const int code = Spawn({paths_.server, "-k"}, 20, &ms, false);
    AppLog("wine         : wineserver -k ended with %d after %.0f ms", code, ms);
  }
}

void WineRuntime::Run() {
  bool found = false;
  const std::string apk_id = wine_rules::Trim(ReadAsset(assets_, "wine/" + abi_ + ".id", &found));
  if (!found || apk_id.empty()) {
    AppLog("wine         : the APK has no Wine tree for %s. The app does not set up Wine",
           abi_.c_str());
    return;
  }
  AppLog("wine         : the APK has the Wine tree %s for %s", apk_id.c_str(), abi_.c_str());
  const std::string have = wine_rules::Trim(ReadTextFile(paths_.id_file));
  if (wine_rules::NeedsUnpack(have, apk_id)) {
    AppLog("wine         : the tree in %s is %s. Unpack the tree of the APK",
           paths_.tree.c_str(), have.empty() ? "not there" : have.c_str());
    SetStatus("Setting up Wine: unpacking the runtime...");
    if (!Unpack(apk_id)) return;
  } else {
    AppLog("wine         : the tree in %s is the tree of the APK. No unpack",
           paths_.tree.c_str());
  }
  if (stop_ || !CheckTree()) return;
  // The marker of the prefix is the last step of a good MakePrefix, so a
  // stopped or failed wineboot gives a new prefix at the next start.
  std::string made = wine_rules::Trim(ReadTextFile(paths_.prefix_id));
  if (wine_rules::NeedsPrefix(made)) {
    SetStatus("Setting up Wine: making the prefix...");
    if (!MakePrefix(apk_id)) return;
    made = apk_id;
  } else {
    AppLog("wine         : the prefix %s is there (made with the tree %s)",
           paths_.prefix.c_str(), made.c_str());
  }
  if (stop_) return;
  if (wine_rules::NeedsCpuCopy(abi_, made, apk_id)) {
    const std::string dll = paths_.system32 + "/" + wine_rules::CpuDll(abi_);
    if (!MakeDirs(paths_.system32) || !CopyFile(paths_.cpu_dll, dll)) {
      Fault("cannot copy " + std::string(wine_rules::CpuDll(abi_)) + " into system32");
      return;
    }
    AppLog("wine         : %s of the tree %s is in system32 of the prefix",
           wine_rules::CpuDll(abi_), apk_id.c_str());
  }
  if (made != apk_id) {
    if (!WriteTextFile(paths_.prefix_id, apk_id + "\n")) {
      Fault("cannot write the marker of the prefix");
      return;
    }
  }
  if (!LinkGame()) return;
  SetStatus("Wine ready.");
  AppLog("wine         : Wine is ready");
  if (options_.probe && !stop_) Probe();
}

bool WineRuntime::Unpack(const std::string& apk_id) {
  const double start = NowMs();
  const std::string asset = "wine/" + abi_ + ".tar";
  AAsset* a = AAssetManager_open(assets_, asset.c_str(), AASSET_MODE_STREAMING);
  if (a == nullptr) return Fault("the APK has no " + asset);
  const std::string fresh = files_ + "/wine.new";
  if (!RemoveTree(fresh) || !MakeDirs(fresh)) {
    AAsset_close(a);
    return Fault("cannot make " + fresh);
  }
  uint8_t header[512];
  static uint8_t buffer[1 << 16];
  uint64_t files = 0, folders = 0, bytes = 0;
  bool ok = true, end = false;
  std::string why;
  while (ok && !end && !stop_) {
    wine_rules::TarEntry e;
    if (!ReadFull(a, header, sizeof(header)) || !wine_rules::ParseTarHeader(header, &e)) {
      ok = false;
      why = "a broken header";
      break;
    }
    if (e.end) {
      end = true;
      break;
    }
    if (!wine_rules::SafeName(e.name)) {
      ok = false;
      why = "the name " + e.name + " points out of the tree";
      break;
    }
    const std::string path = fresh + "/" + e.name;
    if (e.type == '5') {
      ok = MakeDirs(path);
      ++folders;
    } else if (e.type == '0') {
      const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                          (e.mode & 0700) | 0600);
      ok = fd >= 0;
      uint64_t left = wine_rules::TarDataBytes(e), data = e.size;
      while (ok && left > 0) {
        const size_t n = left < sizeof(buffer) ? static_cast<size_t>(left) : sizeof(buffer);
        ok = ReadFull(a, buffer, n);
        const size_t keep = data < n ? static_cast<size_t>(data) : n;
        if (ok && keep > 0) ok = write(fd, buffer, keep) == static_cast<ssize_t>(keep);
        data -= keep;
        left -= n;
      }
      if (fd >= 0 && close(fd) != 0) ok = false;
      ++files;
      bytes += e.size;
    } else {
      // The trees of pack_wine.sh have only files and folders, so a link
      // or another kind of entry stops the unpack.
      ok = false;
      why = std::string("the entry ") + e.name + " has the type " + e.type;
      break;
    }
    if (!ok && why.empty()) why = "cannot write " + path + " (errno " + std::to_string(errno) + ")";
  }
  AAsset_close(a);
  if (stop_) return false;
  if (!ok || !end) return Fault("the unpack of " + asset + " failed: " + why);
  if (!RemoveTree(paths_.tree) || rename(fresh.c_str(), paths_.tree.c_str()) != 0) {
    return Fault("cannot put the new tree in place");
  }
  if (!WriteTextFile(paths_.id_file, apk_id + "\n")) return Fault("cannot write the build id");
  AppLog("wine         : unpacked %llu files (%.1f MB) and %llu folders in %.1f s",
         static_cast<unsigned long long>(files), bytes / 1048576.0,
         static_cast<unsigned long long>(folders), (NowMs() - start) / 1000.0);
  return true;
}

// The tree must have the loader, wineserver, ntdll of i386 and the CPU DLL,
// and none of the DLLs that stay out ("Only Vulkan").
bool WineRuntime::CheckTree() {
  const std::string arch = wine_rules::WineArch(abi_);
  const std::string need[] = {paths_.loader, paths_.server,
                              paths_.dlls + "/i386-windows/ntdll.dll",
                              paths_.dlls + "/" + arch + "-unix/ntdll.so"};
  for (const std::string& f : need) {
    if (!Exists(f)) return Fault("the tree has no " + f.substr(paths_.tree.size() + 1));
  }
  if (!paths_.cpu_dll.empty() && !Exists(paths_.cpu_dll)) {
    return Fault("the tree has no " + std::string(wine_rules::CpuDll(abi_)));
  }
  const std::string& lib = paths_.dlls;
  DIR* top = opendir(lib.c_str());
  if (top == nullptr) return Fault("the tree has no " + lib.substr(paths_.tree.size() + 1));
  std::vector<std::string> bad;
  while (dirent* d = readdir(top)) {
    if (d->d_name[0] == '.') continue;
    const std::string dir = lib + "/" + d->d_name;
    DIR* sub = opendir(dir.c_str());
    if (sub == nullptr) continue;
    while (dirent* f = readdir(sub)) {
      if (wine_rules::IsLeftOut(f->d_name)) bad.push_back(std::string(d->d_name) + "/" + f->d_name);
    }
    closedir(sub);
  }
  closedir(top);
  if (!bad.empty()) return Fault("the tree has " + bad[0] + ", which must stay out");
  return true;
}

bool WineRuntime::MakePrefix(const std::string& tree_id) {
  AppLog("wine         : make the prefix %s", paths_.prefix.c_str());
  RemoveTree(paths_.prefix);
  if (!MakeDirs(paths_.prefix) || !MakeDirs(files_ + "/tmp")) {
    return Fault("cannot make " + paths_.prefix);
  }
  if (!WriteTextFile(paths_.user_reg, wine_rules::UserRegSeed())) {
    return Fault("cannot write user.reg");
  }
  // The CPU DLL of ARM64 must be in system32 before wineboot: wineboot
  // runs the 32 bit part of wine.inf in a 32 bit process.
  if (wine_rules::CopiesCpuDll(abi_)) {
    const std::string dll = paths_.system32 + "/" + wine_rules::CpuDll(abi_);
    if (!MakeDirs(paths_.system32) || !CopyFile(paths_.cpu_dll, dll)) {
      return Fault("cannot copy " + std::string(wine_rules::CpuDll(abi_)) + " into system32");
    }
  }
  double boot_ms = 0.0, wait_ms = 0.0;
  const int boot = Spawn({paths_.loader, "wineboot", "--init"}, 900, &boot_ms);
  AppLog("wine         : wineboot --init ended with %d after %.1f s", boot, boot_ms / 1000.0);
  if (stop_) return false;
  // The wine server writes the registry when its last process ends.
  const int wait = Spawn({paths_.server, "-w"}, 300, &wait_ms);
  AppLog("wine         : wineserver -w ended with %d after %.1f s", wait, wait_ms / 1000.0);
  if (boot != 0) return Fault("wineboot ended with " + std::to_string(boot) + ". See wine.log");
  if (!Exists(paths_.prefix + "/system.reg")) return Fault("wineboot made no system.reg");
  if (!WriteTextFile(paths_.prefix_id, tree_id + "\n")) {
    return Fault("cannot write the marker of the prefix");
  }
  AppLog("wine         : the prefix took %.1f s", (boot_ms + wait_ms) / 1000.0);
  return true;
}

bool WineRuntime::LinkGame() {
  char target[4096];
  const ssize_t n = readlink(paths_.game_link.c_str(), target, sizeof(target) - 1);
  if (n > 0) {
    target[n] = 0;
    if (game_ == target) return true;
  }
  unlink(paths_.game_link.c_str());
  if (symlink(game_.c_str(), paths_.game_link.c_str()) != 0) {
    return Fault("cannot link the drive G: to the game folder (errno " +
                 std::to_string(errno) + ")");
  }
  AppLog("wine         : the drive G: is the game folder %s", game_.c_str());
  return true;
}

void WineRuntime::Probe() {
  const std::string exe = game_ + "/" + wine_rules::kProbeExe;
  if (!Exists(exe)) {
    AppLog("wine         : wine_probe=1, but the game folder has no %s", wine_rules::kProbeExe);
    return;
  }
  const std::string result = game_ + "/" + wine_rules::kProbeResult;
  unlink(result.c_str());
  SetStatus("Wine ready. The test program runs...");
  double ms = 0.0;
  const int code = Spawn({paths_.loader, std::string("G:\\") + wine_rules::kProbeExe}, 600, &ms);
  AppLog("wine         : wine_probe=1. %s ended with %d after %.1f s", wine_rules::kProbeExe,
         code, ms / 1000.0);
  const std::string text = ReadTextFile(result);
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    const std::string line = wine_rules::Trim(text.substr(at, end - at));
    if (!line.empty()) AppLog("wine probe   : %s", line.c_str());
    at = end + 1;
  }
  if (text.empty()) AppLog("wine         : the test program wrote no %s", wine_rules::kProbeResult);
  SetStatus("Wine ready. The test program ended with " + std::to_string(code) + ".");
}

int WineRuntime::Spawn(const std::vector<std::string>& args, int timeout_s, double* ms,
                       bool stoppable) {
  const double start = NowMs();
  *ms = 0.0;
  std::vector<char*> argv, envp;
  for (const std::string& s : args) argv.push_back(const_cast<char*>(s.c_str()));
  argv.push_back(nullptr);
  for (const std::string& s : env_) envp.push_back(const_cast<char*>(s.c_str()));
  envp.push_back(nullptr);
  // The highest file descriptor that the child closes. getrlimit is not a
  // call for the child of a process with threads, so it comes first.
  struct rlimit files;
  const int top = getrlimit(RLIMIT_NOFILE, &files) == 0 && files.rlim_cur != RLIM_INFINITY &&
                          files.rlim_cur < 1048576
                      ? static_cast<int>(files.rlim_cur)
                      : 65536;

  // wine.log holds the programs of one start of the app: the first program
  // of a start makes it empty.
  const int log = open(paths_.log.c_str(),
                       O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | (log_started_ ? 0 : O_TRUNC),
                       0660);
  log_started_ = true;
  if (log >= 0) {
    std::string head = "== bwvr: start";
    for (const std::string& s : args) head += " " + s;
    head += "\n";
    if (write(log, head.data(), head.size()) < 0) {
      // wine.log is only for a person to read: the program runs also with
      // no line in it.
    }
  }
  const int null_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
  // A pipe that closes at the execve: when the execve fails, the child
  // writes its errno into it, so the log can say why.
  int pipe_fd[2] = {-1, -1};
  if (pipe2(pipe_fd, O_CLOEXEC) != 0) pipe_fd[0] = pipe_fd[1] = -1;
  const pid_t pid = fork();
  if (pid == 0) {
    // THE CHILD of a process with threads: only calls that are safe after
    // a fork. Wine takes signals for its own use, so the child gets each
    // signal with its default action and none blocked, not the state of
    // the thread of the app.
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK, &none, nullptr);
    struct sigaction plain;
    memset(&plain, 0, sizeof(plain));
    plain.sa_handler = SIG_DFL;
    for (int s = 1; s < 32; ++s) {
      if (s != SIGKILL && s != SIGSTOP) sigaction(s, &plain, nullptr);
    }
    if (null_fd >= 0) dup2(null_fd, 0);
    if (log >= 0) {
      dup2(log, 1);
      dup2(log, 2);
    }
    // No other file of the app goes into Wine: each descriptor from 3 up
    // closes, except the pipe of the errno (it closes at the execve). The
    // system call and not close(): the fdsan of bionic owns some of them.
    for (int fd = 3; fd < top; ++fd) {
      if (fd != pipe_fd[1]) syscall(__NR_close, fd);
    }
    execve(argv[0], argv.data(), envp.data());
    const int e = errno;
    if (pipe_fd[1] >= 0 && write(pipe_fd[1], &e, sizeof(e)) < 0) _exit(126);
    _exit(127);
  }
  if (pipe_fd[1] >= 0) close(pipe_fd[1]);
  if (log >= 0) close(log);
  if (null_fd >= 0) close(null_fd);
  if (pid < 0) {
    AppLog("wine         : fork for %s failed (errno %d)", argv[0], errno);
    if (pipe_fd[0] >= 0) close(pipe_fd[0]);
    return -1;
  }
  int exec_errno = 0;
  ssize_t got = -1;
  if (pipe_fd[0] >= 0) {
    do {
      got = read(pipe_fd[0], &exec_errno, sizeof(exec_errno));
    } while (got < 0 && errno == EINTR);
    close(pipe_fd[0]);
  }
  if (got == static_cast<ssize_t>(sizeof(exec_errno))) {
    AppLog("wine         : %s did not start (errno %d: %s)", argv[0], exec_errno,
           strerror(exec_errno));
    waitpid(pid, nullptr, 0);
    *ms = NowMs() - start;
    return -1;
  }
  int status = 0;
  for (;;) {
    const pid_t w = waitpid(pid, &status, WNOHANG);
    if (w == pid) break;
    if (w < 0 && errno != EINTR) {
      *ms = NowMs() - start;
      return -1;
    }
    if (stoppable && stop_) {
      AppLog("wine         : the app ends, so it stops %s", argv[0]);
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      *ms = NowMs() - start;
      return -1;
    }
    if (NowMs() - start > timeout_s * 1000.0) {
      AppLog("wine         : %s did not end in %d s. The app stops it", argv[0], timeout_s);
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      *ms = NowMs() - start;
      return -1;
    }
    usleep(20000);
  }
  *ms = NowMs() - start;
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  AppLog("wine         : %s ended with the signal %d", argv[0],
         WIFSIGNALED(status) ? WTERMSIG(status) : 0);
  return -1;
}
