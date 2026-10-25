#include "time_log.h"

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "../../src/mod/vk_scene.h"
#include "app_log.h"

namespace {

namespace tl = time_log_math;
namespace ps = proc_stat;

bool g_name_threads = false;

// A small file of /proc as text, or "" when it cannot be read.
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

const char* CauseName(int c) {
  switch (c) {
    case 0: return "the GPU";
    case 1: return "xrWaitFrame or xrEndFrame";
    default: return "the CPU of the app";
  }
}

}  // namespace

void TimeLogNameThread(const char* name) {
  if (g_name_threads) pthread_setname_np(pthread_self(), name);
}

void TimeLog::Start(bool on) {
  on_ = on;
  g_name_threads = on;
  const long hz = sysconf(_SC_CLK_TCK);
  ticks_per_second_ = hz > 0 ? hz : 100;
  AppLog("time         : time_log=%d. %s", on ? 1 : 0,
         on ? "While the game runs, each 10 s the lines \"time\" say where the time of a "
              "frame goes, with the CPU time of each thread of the game and of the app "
              "and the load of each core"
            : "No lines of the times");
}

void TimeLog::GameStarts(double now_ms) {
  if (!on_) return;
  game_on_ = true;
  game_pid_ = 0;
  last_ = Sample();
  before_ = Sample();
  arrived_set_ = false;
  stop_said_for_ = -1.0;
  last_end_ms_ = -1.0;
  ResetWindow(now_ms);
  VkSceneGpuTime drop;
  VkSceneTakeGpuTime(&drop);
}

void TimeLog::GameEnds() { game_on_ = false; }

void TimeLog::ResetWindow(double now_ms) {
  window_ms_ = now_ms;
  session_frames_ = no_new_ = pictures_ = frames_ = 0;
  skipped_[0] = skipped_[1] = skipped_[2] = 0;
  play_.Reset();
  xr_ = XrApp::FrameTimes();
  walk_gpu_.Reset();
  walk_wait_.Reset();
}

// The times of the GPU of the renderer since the last call: the walks and
// their waits for the fence of their slot.
void TimeLog::TakeRenderer() {
  VkSceneGpuTime g;
  VkSceneTakeGpuTime(&g);
  tl::Stat w;
  w.count = g.walks;
  w.sum = g.ms;
  w.most = g.most_ms;
  walk_gpu_.Merge(w);
  tl::Stat f;
  f.count = g.fence_waits;
  f.sum = g.fence_wait_ms;
  f.most = g.fence_wait_most_ms;
  walk_wait_.Merge(f);
  since_gpu_ += g.fence_wait_ms;
}

void TimeLog::Batch(double now_ms, size_t frames, double play_ms, double wait_ms) {
  if (!on_ || !game_on_ || frames == 0) return;
  TakeRenderer();
  since_wait_ += wait_ms;
  if (last_end_ms_ >= 0.0) since_total_ += now_ms - last_end_ms_;
  ++pictures_;
  frames_ += frames;
  play_.Add(play_ms);
  if (frames > 1) {
    const tl::Cause c = tl::CauseOf(since_total_, since_gpu_, since_wait_);
    skipped_[static_cast<int>(c)] += frames - 1;
  }
  since_total_ = since_gpu_ = since_wait_ = 0.0;
  batch_now_ = true;
  batch_ms_ = now_ms;
}

void TimeLog::SessionFrame(double now_ms, const XrApp::FrameTimes& t) {
  if (!on_ || !game_on_) return;
  TakeRenderer();
  ++session_frames_;
  xr_.wait.Merge(t.wait);
  xr_.work.Merge(t.work);
  xr_.end.Merge(t.end);
  xr_.fence_wait.Merge(t.fence_wait);
  xr_.busy.Merge(t.busy);
  since_gpu_ += t.fence_wait.sum;
  // xrEndFrame comes after the batch of this frame, so it counts for the
  // next batch.
  since_wait_ += t.end.sum;
  if (batch_now_) {
    // The wait of this frame came before its batch, and counts for it.
    since_total_ += now_ms - batch_ms_;
  } else {
    ++no_new_;
    since_wait_ += t.wait.sum;
    if (last_end_ms_ >= 0.0) since_total_ += now_ms - last_end_ms_;
  }
  batch_now_ = false;
  last_end_ms_ = now_ms;
}

void TimeLog::Read(Sample* s, double now_ms) {
  s->ms = now_ms;
  const int self = static_cast<int>(getpid());
  const uid_t uid = getuid();
  // The game process: runblack.exe of the user of the app. Its parent is 1,
  // so the app finds it in /proc. A process of Wine has the name of its exe.
  char path[64];
  if (game_pid_ > 0) {
    snprintf(path, sizeof(path), "/proc/%d", game_pid_);
    struct stat st;
    if (stat(path, &st) != 0) game_pid_ = 0;
  }
  s->other_ticks = 0;
  s->game_ticks = 0;
  s->app_ticks = 0;
  DIR* dir = opendir("/proc");
  if (dir != nullptr) {
    while (dirent* e = readdir(dir)) {
      if (!IsNumber(e->d_name)) continue;
      char where[64];
      snprintf(where, sizeof(where), "/proc/%s", e->d_name);
      struct stat st;
      if (stat(where, &st) != 0 || st.st_uid != uid) continue;
      const int pid = atoi(e->d_name);
      char file[80];
      snprintf(file, sizeof(file), "/proc/%s/stat", e->d_name);
      ps::Task t;
      if (!ps::ParseTask(ReadSmall(file), &t)) continue;
      if (game_pid_ == 0 && pid != self) {
        bool game = EndsWithExe(t.name);
        if (!game) {
          snprintf(file, sizeof(file), "/proc/%s/cmdline", e->d_name);
          const std::string cmd = ReadSmall(file);
          game = EndsWithExe(std::string(cmd.c_str()));
        }
        if (game) {
          game_pid_ = pid;
          AppLog("time         : the game is the process %d (%s). Its main thread has the "
                 "id %d",
                 pid, t.name.c_str(), pid);
        }
      }
      const uint64_t ticks = t.user + t.system;
      if (pid == self) {
        s->app_ticks = ticks;
      } else if (pid == game_pid_) {
        s->game_ticks = ticks;
      } else {
        s->other_ticks += ticks;
      }
    }
    closedir(dir);
  }
  ReadTasks(self, &s->app);
  if (game_pid_ > 0) {
    ReadTasks(game_pid_, &s->game);
  } else {
    s->game.clear();
  }
  // The cores.
  s->cpus.clear();
  const std::string text = ReadSmall("/proc/stat");
  if (text.empty() && said_no_stat_ == 0) {
    said_no_stat_ = 1;
    AppLog("time         : the app cannot read /proc/stat, so the lines have no load of "
           "the cores");
  }
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    ps::Cpu c;
    if (ps::ParseCpu(text.substr(at, end - at), &c)) s->cpus.push_back(c);
    at = end + 1;
  }
}

std::vector<ps::Load> TimeLog::Loads(const Sample& before, const Sample& now) const {
  std::vector<ps::Load> loads;
  const int self = static_cast<int>(getpid());
  // A game process that the sample before did not have has no time of its
  // threads in the window yet.
  for (const auto& kv : now.game) {
    if (before.game.empty()) break;
    auto b = before.game.find(kv.first);
    const bool had = b != before.game.end();
    loads.push_back(ps::LoadOf(game_pid_, true, had ? b->second : ps::Task(), had, kv.second));
  }
  for (const auto& kv : now.app) {
    auto b = before.app.find(kv.first);
    const bool had = b != before.app.end();
    loads.push_back(ps::LoadOf(self, false, had ? b->second : ps::Task(), had, kv.second));
  }
  return loads;
}

void TimeLog::ThreadLines(const Sample& before, const Sample& now, const char* why) {
  const double seconds = before.ms >= 0.0 ? (now.ms - before.ms) / 1000.0 : 0.0;
  const std::vector<ps::Load> top = ps::Top(Loads(before, now), 8);
  std::string list;
  for (const ps::Load& l : top) {
    char part[160];
    snprintf(part, sizeof(part), "%s%s %s %d%s %.1f (%.1f + %.1f) core %d %c",
             list.empty() ? "" : ", ", l.game ? "game" : "app", l.now.name.c_str(), l.now.id,
             ps::IsMainThread(l) ? " (the main thread)" : "",
             ps::Percent(l.user + l.system, seconds, ticks_per_second_),
             ps::Percent(l.user, seconds, ticks_per_second_),
             ps::Percent(l.system, seconds, ticks_per_second_), l.now.processor,
             l.now.state);
    list += part;
  }
  AppLog("time         : %s, the threads in %.1f s, percent of one core (user + system), "
         "the core and the state, the first 8 of %zu: %s",
         why, seconds, now.game.size() + now.app.size(), list.empty() ? "none" : list.c_str());
}

void TimeLog::Lines(double now_ms, uint64_t arrived, uint64_t last_frame) {
  const double seconds = (now_ms - window_ms_) / 1000.0;
  if (seconds <= 0.0) return;
  const uint64_t skipped = skipped_[0] + skipped_[1] + skipped_[2];
  // The frames of the game: the frames that the reader took from the ring in
  // the window, at the time when they came.
  const uint64_t came = arrived_set_ && arrived >= arrived_at_window_ ? arrived - arrived_at_window_ : 0;
  AppLog("time         : %.1f s to frame %llu: the game %.1f frames a second (%llu frames "
         "came), the app took %llu frames and made %.1f pictures a second (%llu), %.1f "
         "frames of the session a second. %llu frames of the game did not show because "
         "the app was still at the picture before: %llu while it waited for %s, %llu in "
         "%s, %llu while %s worked. %llu frames of the session had no new frame of the game",
         seconds, static_cast<unsigned long long>(last_frame), came / seconds,
         static_cast<unsigned long long>(came), static_cast<unsigned long long>(frames_),
         pictures_ / seconds,
         static_cast<unsigned long long>(pictures_), session_frames_ / seconds,
         static_cast<unsigned long long>(skipped), static_cast<unsigned long long>(skipped_[0]),
         CauseName(0), static_cast<unsigned long long>(skipped_[1]), CauseName(1),
         static_cast<unsigned long long>(skipped_[2]), CauseName(2),
         static_cast<unsigned long long>(no_new_));
  AppLog("time         : the CPU of the app in ms, the mean and the most: the play of a "
         "picture %.2f and %.2f, the wait in xrWaitFrame %.2f and %.2f, from the end of "
         "xrWaitFrame to the end of xrEndFrame %.2f and %.2f, in xrEndFrame %.2f and %.2f, "
         "a wait of a copy for the GPU "
         "%.2f and %.2f (%llu waits), a wait of a walk for the GPU %.2f and %.2f (%llu "
         "waits)",
         play_.Mean(), play_.most, xr_.wait.Mean(), xr_.wait.most, xr_.work.Mean(),
         xr_.work.most, xr_.end.Mean(), xr_.end.most, xr_.fence_wait.Mean(), xr_.fence_wait.most,
         static_cast<unsigned long long>(xr_.fence_wait.count), walk_wait_.Mean(),
         walk_wait_.most, static_cast<unsigned long long>(walk_wait_.count));
  const double gpu_ms = walk_gpu_.sum + xr_.busy.sum;
  AppLog("time         : the GPU in ms: a picture %.2f as a mean (the walks and the command "
         "buffers of the app over the pictures), a walk of the renderer %.2f and %.2f at "
         "most (%llu walks), a command buffer of the app %.2f and %.2f at most (%llu). The "
         "GPU worked %.1f percent of the time for the app",
         pictures_ > 0 ? gpu_ms / pictures_ : 0.0, walk_gpu_.Mean(), walk_gpu_.most,
         static_cast<unsigned long long>(walk_gpu_.count), xr_.busy.Mean(), xr_.busy.most,
         static_cast<unsigned long long>(xr_.busy.count), 100.0 * gpu_ms / (seconds * 1000.0));
  // THE THREADS AND THE CORES.
  Sample now;
  Read(&now, now_ms);
  if (last_.ms >= 0.0) {
    const double s = (now.ms - last_.ms) / 1000.0;
    ThreadLines(last_, now, "each 10 s");
    const auto cores = [&](uint64_t a, uint64_t b) {
      return b >= a ? ps::Percent(b - a, s, ticks_per_second_) / 100.0 : 0.0;
    };
    std::string each;
    double busy = 0.0;
    int count = 0, over_half = 0;
    for (const ps::Cpu& c : now.cpus) {
      if (c.index < 0) continue;
      for (const ps::Cpu& b : last_.cpus) {
        if (b.index != c.index) continue;
        const uint64_t total = c.total - b.total;
        const double pct = total > 0 ? 100.0 * (c.busy - b.busy) / total : 0.0;
        char part[16];
        snprintf(part, sizeof(part), "%s%.0f", each.empty() ? "" : " ", pct);
        each += part;
        busy += pct / 100.0;
        ++count;
        if (pct >= 50.0) ++over_half;
      }
    }
    AppLog("time         : the processes in cores: the game %.2f, the app %.2f, the other "
           "processes of the user of the app (Wine) %.2f. The cores: %d, %.2f of them busy "
           "in all, %d busy for half the time or more, each in percent: %s",
           last_.game.empty() ? 0.0 : cores(last_.game_ticks, now.game_ticks),
           cores(last_.app_ticks, now.app_ticks),
           cores(last_.other_ticks, now.other_ticks), count, busy, over_half,
           each.empty() ? "none" : each.c_str());
  }
  before_ = last_;
  last_ = now;
}

void TimeLog::StopLine(double now_ms, double arrival_ms, uint64_t last_frame) {
  Sample now;
  Read(&now, now_ms);
  // The sample of the window before, when the last one is less than 2 s
  // old, so the percents are of a time.
  const Sample& base = (now_ms - last_.ms < 2000.0 && before_.ms >= 0.0) ? before_ : last_;
  const double seconds = (now.ms - base.ms) / 1000.0;
  // The threads of the game, at most 24 (a line of the log has 2048
  // characters at most).
  const std::vector<ps::Load> loads = ps::Top(Loads(base, now), 64);
  int listed = 0;
  std::string list;
  for (const ps::Load& l : loads) {
    if (!l.game || listed >= 24) continue;
    ++listed;
    char part[160];
    snprintf(part, sizeof(part), "%s%s %d%s %c %.1f (%.1f + %.1f) core %d",
             list.empty() ? "" : ", ", l.now.name.c_str(), l.now.id,
             ps::IsMainThread(l) ? " (the main thread)" : "", l.now.state,
             ps::Percent(l.user + l.system, seconds, ticks_per_second_),
             ps::Percent(l.user, seconds, ticks_per_second_),
             ps::Percent(l.system, seconds, ticks_per_second_), l.now.processor);
    list += part;
  }
  AppLog("time         : no new frame of the game came to the reader for %.1f s (the newest "
         "frame that the app played is frame %llu). The threads of the game in the last "
         "%.1f s, the state and the percent of one core (user + system): %s",
         (now_ms - arrival_ms) / 1000.0, static_cast<unsigned long long>(last_frame), seconds,
         list.empty() ? "none (no process of the game)" : list.c_str());
}

void TimeLog::Tick(double now_ms, uint64_t arrived, double arrival_ms, uint64_t last_frame) {
  if (!on_ || !game_on_) return;
  if (last_.ms < 0.0) Read(&last_, now_ms);
  if (!arrived_set_) {
    arrived_set_ = true;
    arrived_at_window_ = arrived;
  }
  // THE STOP OF THE GAME: no new frame came to the reader for 5 s. One line
  // for each stop, with the state of each thread of the game. The reader
  // takes the frames from the ring in a thread of its own, so a slow loop of
  // the session gives no such line.
  if (arrival_ms > 0.0 && now_ms - arrival_ms >= 5000.0 && stop_said_for_ != arrival_ms) {
    stop_said_for_ = arrival_ms;
    StopLine(now_ms, arrival_ms, last_frame);
  }
  if (now_ms - window_ms_ >= tl::kWindowMs) {
    Lines(now_ms, arrived, last_frame);
    ResetWindow(now_ms);
    arrived_at_window_ = arrived;
  }
}
