#include "thread_cores.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>

#include "app_log.h"

namespace {

namespace cr = core_rules;
namespace ps = proc_stat;

// The session thread and the play thread of the app (MarkSession, MarkPlay).
std::atomic<int> g_session_tid{0};
std::atomic<int> g_play_tid{0};

int Gettid() { return static_cast<int>(syscall(__NR_gettid)); }

double ClockMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

// A small file of /proc or /sys as text, or "" when it cannot be read.
std::string ReadSmall(const char* path) {
  const int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) return std::string();
  std::string text;
  char buffer[4096];
  for (;;) {
    const ssize_t n = read(fd, buffer, sizeof(buffer));
    if (n <= 0) break;
    text.append(buffer, static_cast<size_t>(n));
    if (text.size() > (1u << 16)) break;
  }
  close(fd);
  return text;
}

std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\n' || s[a] == '\r' || s[a] == '\t')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\n' || s[b - 1] == '\r' || s[b - 1] == '\t')) --b;
  return s.substr(a, b - a);
}

bool IsNumber(const char* name) {
  if (*name == '\0') return false;
  for (const char* p = name; *p != '\0'; ++p) {
    if (*p < '0' || *p > '9') return false;
  }
  return true;
}

// The threads of a process: each line of /proc/<pid>/task/<tid>/stat.
void ReadTasks(int pid, std::map<int, ps::Task>* out) {
  out->clear();
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/task", pid);
  DIR* dir = opendir(path);
  if (dir == nullptr) return;
  while (dirent* e = readdir(dir)) {
    if (!IsNumber(e->d_name)) continue;
    char file[96];
    snprintf(file, sizeof(file), "/proc/%d/task/%s/stat", pid, e->d_name);
    ps::Task t;
    if (ps::ParseTask(ReadSmall(file), &t)) (*out)[t.id] = t;
  }
  closedir(dir);
}

bool EndsWithExe(const std::string& text) {
  const char* want = "runblack.exe";
  const size_t n = strlen(want);
  if (text.size() < n) return false;
  return strcasecmp(text.c_str() + text.size() - n, want) == 0;
}

// The cores of a thread or a process (sched_getaffinity), or empty.
std::vector<int> AffinityOf(int tid) {
  cpu_set_t set;
  CPU_ZERO(&set);
  if (sched_getaffinity(tid, sizeof(set), &set) != 0) return {};
  std::vector<int> out;
  for (int c = 0; c < CPU_SETSIZE && c < 1024; ++c) {
    if (CPU_ISSET(c, &set)) out.push_back(c);
  }
  return out;
}

int SetAffinity(int tid, const std::vector<int>& cores) {
  cpu_set_t set;
  CPU_ZERO(&set);
  for (int c : cores) {
    if (c >= 0 && c < CPU_SETSIZE) CPU_SET(c, &set);
  }
  return sched_setaffinity(tid, sizeof(set), &set) == 0 ? 0 : errno;
}

// The cores of the device: the online cores with their top speed and their
// capacity.
std::vector<cr::Core> ReadCores() {
  std::vector<cr::Core> out;
  for (int c : cr::ParseCpuList(ReadSmall("/sys/devices/system/cpu/online"))) {
    cr::Core core;
    core.index = c;
    char path[96];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", c);
    core.max_khz = atol(Trim(ReadSmall(path)).c_str());
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpu_capacity", c);
    core.capacity = atol(Trim(ReadSmall(path)).c_str());
    out.push_back(core);
  }
  return out;
}

// The value of "Cpus_allowed_list:" in /proc/<pid>/status, or "".
std::string AllowedList(int pid) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/status", pid);
  const std::string text = ReadSmall(path);
  const size_t at = text.find("Cpus_allowed_list:");
  if (at == std::string::npos) return std::string();
  const size_t end = text.find('\n', at);
  return Trim(text.substr(at + 18, end == std::string::npos ? std::string::npos : end - at - 18));
}

std::string CpusetOf(int pid) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/cpuset", pid);
  const std::string s = Trim(ReadSmall(path));
  return s.empty() ? std::string("(cannot be read)") : s;
}

const char* PlaceName(cr::Place p) {
  switch (p) {
    case cr::Place::kFast: return "fast";
    case cr::Place::kOther: return "other";
    default: return "free";
  }
}

// One thread in a line of a second.
struct Row {
  int id = 0;
  std::string name;
  std::string mark;  // "(the main thread)" and the like, or ""
  int core = -1;
  double percent = 0.0;
  double user = 0.0;
  double system = 0.0;
  cr::Place place = cr::Place::kFree;
};

}  // namespace

void ThreadCores::MarkSession() { g_session_tid.store(Gettid()); }
void ThreadCores::MarkPlay() { g_play_tid.store(Gettid()); }

void ThreadCores::GameStarts(const Options& options) {
  GameEnds();
  options_ = options;
  options_.bind = cr::ParseBind(options.bind);
  const long hz = sysconf(_SC_CLK_TCK);
  ticks_per_second_ = hz > 0 ? hz : 100;
  game_pid_ = 0;
  game_before_.clear();
  app_before_.clear();
  game_placed_.clear();
  app_placed_.clear();
  faults_.clear();
  moved_ = 0;
  // The play thread of this game starts after Play.
  g_play_tid.store(0);
  // The cores of the device and the cores that the app can use.
  const std::vector<cr::Core> cores = ReadCores();
  const int self = static_cast<int>(getpid());
  const std::vector<int> allowed = AffinityOf(self);
  plan_ = cr::MakePlan(options_.bind, options_.bind_cores, cores, allowed);
  std::string speeds;
  for (const cr::Core& c : cores) {
    char part[96];
    snprintf(part, sizeof(part), "%score %d %ld MHz %ld", speeds.empty() ? "" : ", ", c.index,
             c.max_khz / 1000, c.capacity);
    speeds += part;
  }
  AppLog("cores        : thread_cores=%d, thread_bind=%d, thread_bind_cores=\"%s\". The online "
         "cores, each with its top speed (cpuinfo_max_freq, 0: not known) and its capacity "
         "(cpu_capacity, 0: not known): %s",
         options_.log ? 1 : 0, options_.bind, options_.bind_cores.c_str(),
         speeds.empty() ? "none can be read" : speeds.c_str());
  AppLog("cores        : the app (process %d) can use the cores %s (sched_getaffinity, "
         "Cpus_allowed_list %s), its cpuset is %s. The session thread is %d, the play thread "
         "%d (0: not known yet)",
         self, cr::CpuListText(allowed).c_str(), AllowedList(self).c_str(),
         CpusetOf(self).c_str(), g_session_tid.load(), g_play_tid.load());
  if (options_.bind != 0 && plan_.bind == 0) {
    AppLog("cores        : thread_bind=%d binds no thread: %s", options_.bind, plan_.why.c_str());
  } else if (plan_.bind != 0) {
    AppLog("cores        : thread_bind=%d. The fast cores %s get the main thread of the game, "
           "the session thread and the play thread of the app. %s",
           plan_.bind, cr::CpuListText(plan_.fast).c_str(),
           plan_.other.empty()
               ? "Each other thread stays on the cores that it has"
               : ("Each other thread of the game and of the app gets the cores " +
                  cr::CpuListText(plan_.other))
                     .c_str());
  }
  if (!options_.log && plan_.bind == 0) return;
  stop_.store(false);
  running_ = pthread_create(&thread_, nullptr, &ThreadCores::Main, this) == 0;
  if (!running_) AppLog("cores        : the thread of the cores did not start");
}

void ThreadCores::GameEnds() {
  if (!running_) return;
  stop_.store(true);
  pthread_join(thread_, nullptr);
  running_ = false;
  Restore();
}

void* ThreadCores::Main(void* self) {
  pthread_setname_np(pthread_self(), "bwvr cores");
  static_cast<ThreadCores*>(self)->Run();
  return nullptr;
}

void ThreadCores::Run() {
  double last = ClockMs();
  while (!stop_.load()) {
    // A sleep of 1 s in steps, so the end of the game does not wait long.
    for (int i = 0; i < 10 && !stop_.load(); ++i) usleep(100000);
    if (stop_.load()) break;
    const double now = ClockMs();
    Second((now - last) / 1000.0);
    last = now;
  }
}

void ThreadCores::FindGame() {
  if (game_pid_ > 0) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d", game_pid_);
    struct stat st;
    if (stat(path, &st) == 0) return;
    AppLog("cores        : the game process %d is gone", game_pid_);
    game_pid_ = 0;
    game_before_.clear();
    game_placed_.clear();
  }
  const int self = static_cast<int>(getpid());
  const uid_t uid = getuid();
  DIR* dir = opendir("/proc");
  if (dir == nullptr) return;
  while (dirent* e = readdir(dir)) {
    if (!IsNumber(e->d_name)) continue;
    const int pid = atoi(e->d_name);
    if (pid == self) continue;
    char where[64];
    snprintf(where, sizeof(where), "/proc/%s", e->d_name);
    struct stat st;
    if (stat(where, &st) != 0 || st.st_uid != uid) continue;
    char file[80];
    snprintf(file, sizeof(file), "/proc/%s/stat", e->d_name);
    ps::Task t;
    if (!ps::ParseTask(ReadSmall(file), &t)) continue;
    bool game = EndsWithExe(t.name);
    if (!game) {
      snprintf(file, sizeof(file), "/proc/%s/cmdline", e->d_name);
      const std::string cmd = ReadSmall(file);
      game = EndsWithExe(std::string(cmd.c_str()));
    }
    if (!game) continue;
    game_pid_ = pid;
    AppLog("cores        : the game is the process %d (%s). It can use the cores %s "
           "(sched_getaffinity, Cpus_allowed_list %s), its cpuset is %s",
           pid, t.name.c_str(), cr::CpuListText(AffinityOf(pid)).c_str(),
           AllowedList(pid).c_str(), CpusetOf(pid).c_str());
    break;
  }
  closedir(dir);
}

void ThreadCores::Bind(int process, bool game, const std::map<int, ps::Task>& tasks) {
  std::map<int, cr::Place>& placed = game ? game_placed_ : app_placed_;
  const int session = g_session_tid.load();
  const int play = g_play_tid.load();
  for (const auto& it : tasks) {
    const ps::Task& t = it.second;
    if (!game && t.name == "bwvr cores") continue;
    const bool key = game ? t.id == process : (t.id == session || t.id == play);
    const cr::Place place = cr::PlaceOf(plan_, key);
    if (place == cr::Place::kFree) continue;
    auto had = placed.find(t.id);
    if (had != placed.end() && had->second == place) continue;
    const std::vector<int>& cores = cr::CoresOf(plan_, place);
    const int err = SetAffinity(t.id, cores);
    if (err != 0) {
      if (faults_[err]++ == 0) {
        AppLog("cores        : thread_bind: sched_setaffinity of the thread %d (%s) of the %s "
               "to the cores %s failed with errno %d (%s). The log names each other errno "
               "one time",
               t.id, t.name.c_str(), game ? "game" : "app", cr::CpuListText(cores).c_str(), err,
               strerror(err));
      }
      // No second try for this thread in this place.
      placed[t.id] = place;
      continue;
    }
    placed[t.id] = place;
    ++moved_;
    if (key) {
      const char* what = game ? "the main thread of the game"
                              : (t.id == session ? "the session thread of the app"
                                                 : "the play thread of the app");
      AppLog("cores        : thread_bind: %s (%d, %s) is on the fast cores %s now "
             "(sched_getaffinity gives %s)",
             what, t.id, t.name.c_str(), cr::CpuListText(cores).c_str(),
             cr::CpuListText(AffinityOf(t.id)).c_str());
    }
  }
}

void ThreadCores::Restore() {
  if (app_placed_.empty()) return;
  int back = 0, failed = 0;
  for (const auto& it : app_placed_) {
    if (SetAffinity(it.first, plan_.allowed) == 0) {
      ++back;
    } else {
      ++failed;
    }
  }
  AppLog("cores        : the end of the game: %d threads of the app have the cores %s again, "
         "%d of them did not (a thread that ended)",
         back, cr::CpuListText(plan_.allowed).c_str(), failed);
  app_placed_.clear();
}

void ThreadCores::Second(double seconds) {
  FindGame();
  const int self = static_cast<int>(getpid());
  std::map<int, ps::Task> app, game;
  ReadTasks(self, &app);
  if (game_pid_ > 0) ReadTasks(game_pid_, &game);
  if (plan_.bind != 0) {
    Bind(self, false, app);
    if (game_pid_ > 0) Bind(game_pid_, true, game);
  }
  const bool have_before = !game_before_.empty() || !app_before_.empty();
  if (options_.log && have_before && seconds > 0.0) {
    const int session = g_session_tid.load();
    const int play = g_play_tid.load();
    auto rows = [&](const std::map<int, ps::Task>& now, const std::map<int, ps::Task>& before,
                    bool is_game, int* idle, double* sum) {
      std::vector<Row> out;
      for (const auto& it : now) {
        const ps::Task& t = it.second;
        auto b = before.find(t.id);
        const ps::Load l =
            ps::LoadOf(is_game ? game_pid_ : self, is_game, b == before.end() ? t : b->second,
                       b != before.end(), t);
        Row r;
        r.id = t.id;
        r.name = t.name;
        r.core = t.processor;
        r.user = ps::Percent(l.user, seconds, ticks_per_second_);
        r.system = ps::Percent(l.system, seconds, ticks_per_second_);
        r.percent = r.user + r.system;
        if (is_game && t.id == game_pid_) r.mark = " (the main thread)";
        if (!is_game && t.id == session) r.mark = " (the session thread)";
        if (!is_game && t.id == play) r.mark = " (the play thread)";
        const std::map<int, cr::Place>& placed = is_game ? game_placed_ : app_placed_;
        auto p = placed.find(t.id);
        r.place = p == placed.end() ? cr::Place::kFree : p->second;
        *sum += r.percent;
        if (r.percent <= 0.0) {
          ++*idle;
          continue;
        }
        out.push_back(r);
      }
      std::stable_sort(out.begin(), out.end(),
                       [](const Row& a, const Row& b) { return a.percent > b.percent; });
      return out;
    };
    auto text = [&](const std::vector<Row>& list, size_t most) {
      std::string s;
      for (size_t i = 0; i < list.size() && i < most; ++i) {
        const Row& r = list[i];
        char part[192];
        snprintf(part, sizeof(part), "%s%s %d%s core %d %.0f (%.0f + %.0f)%s%s",
                 s.empty() ? "" : ", ", r.name.c_str(), r.id, r.mark.c_str(), r.core, r.percent,
                 r.user, r.system, plan_.bind != 0 ? " " : "",
                 plan_.bind != 0 ? PlaceName(r.place) : "");
        s += part;
      }
      return s.empty() ? std::string("none") : s;
    };
    int game_idle = 0, app_idle = 0;
    double game_sum = 0.0, app_sum = 0.0;
    const std::vector<Row> g = rows(game, game_before_, true, &game_idle, &game_sum);
    const std::vector<Row> a = rows(app, app_before_, false, &app_idle, &app_sum);
    std::vector<int> core_of;
    std::vector<double> percent_of;
    for (const Row& r : g) {
      core_of.push_back(r.core);
      percent_of.push_back(r.percent);
    }
    std::string shares;
    for (const cr::CoreShare& s : cr::Shares(core_of, percent_of)) {
      char part[48];
      snprintf(part, sizeof(part), "%score %d %.0f", shares.empty() ? "" : ", ", s.core,
               s.percent);
      shares += part;
    }
    if (game_pid_ > 0) {
      AppLog("cores        : %.1f s, the game %d threads, %.2f cores in all. By the last core of "
             "each thread, percent of one core: %s. The threads with CPU time, percent of one "
             "core (user + system)%s: %s. %d threads had no CPU time",
             seconds, static_cast<int>(game.size()), game_sum / 100.0,
             shares.empty() ? "none" : shares.c_str(),
             plan_.bind != 0 ? " and the place of the binding" : "", text(g, 24).c_str(),
             game_idle);
    } else {
      AppLog("cores        : %.1f s, no game process yet", seconds);
    }
    AppLog("cores        : %.1f s, the app %d threads, %.2f cores in all. The first 8 with CPU "
           "time: %s. %d threads had no CPU time%s",
           seconds, static_cast<int>(app.size()), app_sum / 100.0, text(a, 8).c_str(), app_idle,
           moved_ > 0 ? (". thread_bind moved " + std::to_string(moved_) +
                         " threads in this second")
                            .c_str()
                      : "");
  }
  moved_ = 0;
  game_before_ = game;
  app_before_ = app;
}
