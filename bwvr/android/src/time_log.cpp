#include "time_log.h"

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <string>

#include "../../src/mod/vk_scene.h"
#include "app_log.h"
#include "copy_time.h"
#include "walk_probe.h"

namespace {

namespace tl = time_log_math;
namespace ps = proc_stat;
namespace wp = walk_parts;

// THE GPU PARTS OF THE WALK: the name of a pass by its key, and of a part
// between the passes.
std::string PassName(const wp::PassKey& k) {
  char text[160];
  snprintf(text, sizeof(text), "a pass of %s, %u by %u, %s%s",
           k.views == 0 ? "one layer" : (k.views == 1 ? "one eye" : "both eyes"), k.width,
           k.height, k.clear ? "cleared" : "loaded",
           k.rate ? ", with the map of the rates"
                  : (k.density ? ", the one pass with a density map" : ""));
  return text;
}
const char* GapName(int g) {
  switch (g) {
    case wp::kUploads: return "the uploads";
    case wp::kCopies: return "the copies and blits between the passes";
    case wp::kReadBacks: return "the read backs";
    case wp::kBarriers: return "the barriers, the start and the end";
    case wp::kAfterLimit: return "the rest after 48 timestamps";
    default: return "the passes of other keys";
  }
}
// THE PARTS OF THE LAYERS, in the order of XrApp::LayerPart.
const char* const kLayerPartNames[XrApp::kParts] = {
    "the begin of the command buffer", "the eye pictures", "the panel image", "the cards",
    "the walls", "the screen quad", "the keyboard", "the cursor", "the menu",
    "the pixels of the CPU into the staging buffers", "the lines of the log", "the rest",
    "the swapchains and the submit"};

// Set by the thread of the session, read by each thread that starts.
std::atomic<bool> g_name_threads{false};

double ClockMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

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

// The causes of time_log_math::Cause. With the frame loop the play thread
// makes the pictures, and its waits are for the GPU and for the lock of the
// queue.
const char* CauseName(int c, bool frame_loop) {
  switch (c) {
    case 0: return "the GPU";
    case 1: return frame_loop ? "the wait for the lock of the queue" : "xrWaitFrame or xrEndFrame";
    default: return frame_loop ? "the play thread" : "the CPU of the app";
  }
}

}  // namespace

void TimeLogNameThread(const char* name) {
  if (g_name_threads.load()) pthread_setname_np(pthread_self(), name);
}

void TimeLog::Start(bool on) {
  on_ = on;
  g_name_threads.store(on);
  // The renderer adds up the times of the GPU of its walks only for this log.
  VkSceneSetGpuTake(on);
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
  any_shown_ = false;
  age_now_ = 0;
  fresh_ = false;
  ResetWindow(now_ms);
  // The play thread starts after this call, so the renderer is free here.
  VkSceneGpuTime drop;
  VkSceneTakeGpuTime(&drop);
  copy_time::Window copies;
  copy_time::TakeWindow(&copies);
  walk_parts::Totals parts;
  WalkProbeTake(&parts);
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
  dropped_ = replaced_ = 0;
  slot_wait_.Reset();
  gate_.Reset();
  gate_dropped_ = 0;
  play_cpu_ms_ = 0.0;
  age_.Reset();
}

void TimeLog::Play(const PlayStats& s) {
  if (!on_ || !game_on_) return;
  pictures_ += s.pictures;
  frames_ += s.frames;
  for (int i = 0; i < 3; ++i) skipped_[i] += s.skipped[i];
  play_.Merge(s.play);
  walk_gpu_.Merge(s.walk_gpu);
  walk_wait_.Merge(s.walk_wait);
  slot_wait_.Merge(s.slot_wait);
  gate_.Merge(s.gate);
  gate_dropped_ += s.gate_dropped;
  xr_.busy.Merge(s.busy);
  dropped_ += s.dropped;
  replaced_ += s.replaced;
  play_cpu_ms_ += s.cpu_ms;
}

void TimeLog::Shown(bool fresh) {
  if (!on_ || !game_on_) return;
  fresh_ = fresh;
  if (fresh) any_shown_ = true;
  age_now_ = frame_loop::NextAge(age_now_, fresh);
  if (any_shown_) age_.Add(static_cast<double>(age_now_));
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
  // With the frame loop the renderer belongs to the play thread, which gives
  // its times (Play).
  if (!frame_loop_) TakeRenderer();
  ++session_frames_;
  xr_.wait.Merge(t.wait);
  xr_.work.Merge(t.work);
  xr_.end.Merge(t.end);
  xr_.fence_wait.Merge(t.fence_wait);
  xr_.busy.Merge(t.busy);
  for (int k = 0; k < XrApp::kSteps; ++k) xr_.steps[k].Merge(t.steps[k]);
  xr_.acquires += t.acquires;
  xr_.not_ready += t.not_ready;
  xr_.age_ms.Merge(t.age_ms);
  xr_.latency_ms.Merge(t.latency_ms);
  xr_.arrival_ms.Merge(t.arrival_ms);
  for (int k = 0; k < XrApp::kParts; ++k) {
    xr_.parts[k].Merge(t.parts[k]);
    xr_.part_cpu[k].Merge(t.part_cpu[k]);
  }
  xr_.staged += t.staged;
  xr_.staged_bytes += t.staged_bytes;
  xr_.staged_rects += t.staged_rects;
  xr_.direct += t.direct;
  xr_.direct_bytes += t.direct_bytes;
  if (frame_loop_) {
    if (!fresh_) ++no_new_;
    fresh_ = false;
    last_end_ms_ = now_ms;
    return;
  }
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
         "%s, %llu while %s worked. %llu frames of the session had no new frame of the game. "
         "%llu pictures that the play thread dropped (no free slot of the ring), %llu "
         "pictures that a newer picture replaced before a frame of the session",
         seconds, static_cast<unsigned long long>(last_frame), came / seconds,
         static_cast<unsigned long long>(came), static_cast<unsigned long long>(frames_),
         pictures_ / seconds,
         static_cast<unsigned long long>(pictures_), session_frames_ / seconds,
         static_cast<unsigned long long>(skipped), static_cast<unsigned long long>(skipped_[0]),
         CauseName(0, frame_loop_), static_cast<unsigned long long>(skipped_[1]),
         CauseName(1, frame_loop_), static_cast<unsigned long long>(skipped_[2]),
         CauseName(2, frame_loop_), static_cast<unsigned long long>(no_new_),
         static_cast<unsigned long long>(dropped_), static_cast<unsigned long long>(replaced_));
  // THE PLAY THREAD (frame_loop=1): its CPU time for each picture and as a
  // part of one core, and its waits for a slot of the ring.
  char play_text[256];
  if (frame_loop_) {
    snprintf(play_text, sizeof(play_text),
             "The play thread: %.2f ms of CPU a picture, %.1f percent of one core, a wait for "
             "a slot of the ring %.2f and %.2f (%llu waits).",
             pictures_ > 0 ? play_cpu_ms_ / pictures_ : 0.0,
             100.0 * play_cpu_ms_ / (seconds * 1000.0), slot_wait_.Mean(), slot_wait_.most,
             static_cast<unsigned long long>(slot_wait_.count));
  } else {
    snprintf(play_text, sizeof(play_text),
             "No play thread (frame_loop=0): the thread of the session plays the pictures.");
  }
  AppLog("time         : the CPU of the app in ms, the mean and the most: the play of a "
         "picture %.2f and %.2f, the wait in xrWaitFrame %.2f and %.2f, from the end of "
         "xrWaitFrame to the end of xrEndFrame %.2f and %.2f, in xrEndFrame %.2f and %.2f, "
         "a wait of a copy for the GPU "
         "%.2f and %.2f (%llu waits), a wait of a walk for the GPU %.2f and %.2f (%llu "
         "waits). %s The age of the shown picture in frames of the session: %.2f as a mean "
         "and %.0f at most",
         play_.Mean(), play_.most, xr_.wait.Mean(), xr_.wait.most, xr_.work.Mean(),
         xr_.work.most, xr_.end.Mean(), xr_.end.most, xr_.fence_wait.Mean(), xr_.fence_wait.most,
         static_cast<unsigned long long>(xr_.fence_wait.count), walk_wait_.Mean(),
         walk_wait_.most, static_cast<unsigned long long>(walk_wait_.count), play_text,
         age_.Mean(), age_.most);
  // THE STEPS of the thread of the session in a frame, and the age of the
  // shown picture in ms.
  const auto& st = xr_.steps;
  char age[384];
  if (xr_.age_ms.count > 0) {
    snprintf(age, sizeof(age),
             "%.1f ms as a mean and %.1f at most in each frame, %.1f ms as a mean and %.1f at "
             "most in the frame that shows a new picture (%llu). From the arrival of the newest "
             "frame of a new picture in the app to the display time: %.1f ms as a mean and %.1f "
             "at most",
             xr_.age_ms.Mean(), xr_.age_ms.most, xr_.latency_ms.Mean(), xr_.latency_ms.most,
             static_cast<unsigned long long>(xr_.latency_ms.count), xr_.arrival_ms.Mean(),
             xr_.arrival_ms.most);
  } else {
    snprintf(age, sizeof(age), "none (no display time on the clock of the app, or no picture)");
  }
  AppLog("time         : the thread of the session from the end of xrWaitFrame to the end of "
         "xrEndFrame, ms in a frame, the mean and the most: xrBeginFrame %.2f and %.2f, the "
         "actions and the poses %.2f and %.2f, the input channel %.2f and %.2f, the picture of "
         "the ring %.2f and %.2f, the play of a batch %.2f and %.2f, the menu and the "
         "controllers %.2f and %.2f, the layers %.2f and %.2f, xrAcquireSwapchainImage %.2f and "
         "%.2f (%llu calls), xrWaitSwapchainImage %.2f and %.2f (%llu images not free at "
         "once), the submit of the copies %.2f and %.2f, xrReleaseSwapchainImage %.2f and %.2f, "
         "xrEndFrame %.2f and %.2f. Of them the waits for the lock of the queue %.2f and %.2f. "
         "The age of the shown picture from the start of its play to the display time: %s",
         st[XrApp::kStepBegin].Mean(), st[XrApp::kStepBegin].most,
         st[XrApp::kStepActions].Mean(), st[XrApp::kStepActions].most,
         st[XrApp::kStepSlot].Mean(), st[XrApp::kStepSlot].most, st[XrApp::kStepTake].Mean(),
         st[XrApp::kStepTake].most, st[XrApp::kStepPlay].Mean(), st[XrApp::kStepPlay].most,
         st[XrApp::kStepUi].Mean(), st[XrApp::kStepUi].most, st[XrApp::kStepLayers].Mean(),
         st[XrApp::kStepLayers].most, st[XrApp::kStepAcquire].Mean(),
         st[XrApp::kStepAcquire].most, static_cast<unsigned long long>(xr_.acquires),
         st[XrApp::kStepImageWait].Mean(), st[XrApp::kStepImageWait].most,
         static_cast<unsigned long long>(xr_.not_ready), st[XrApp::kStepSubmit].Mean(),
         st[XrApp::kStepSubmit].most, st[XrApp::kStepRelease].Mean(),
         st[XrApp::kStepRelease].most, st[XrApp::kStepEnd].Mean(), st[XrApp::kStepEnd].most,
         st[XrApp::kStepLock].Mean(), st[XrApp::kStepLock].most, age);
  const double gpu_ms = walk_gpu_.sum + xr_.busy.sum;
  AppLog("time         : the GPU in ms: a picture %.2f as a mean (the walks and the command "
         "buffers of the app over the pictures), a walk of the renderer %.2f and %.2f at "
         "most (%llu walks), a command buffer of the app %.2f and %.2f at most (%llu). The "
         "GPU worked %.1f percent of the time for the app",
         pictures_ > 0 ? gpu_ms / pictures_ : 0.0, walk_gpu_.Mean(), walk_gpu_.most,
         static_cast<unsigned long long>(walk_gpu_.count), xr_.busy.Mean(), xr_.busy.most,
         static_cast<unsigned long long>(xr_.busy.count), 100.0 * gpu_ms / (seconds * 1000.0));
  // THE GPU OF EACH COPY (copy_time.h): the time of each kind of copy, its MB
  // and the GB that the GPU wrote in a second of the copy.
  {
    copy_time::Window w;
    copy_time::TakeWindow(&w);
    std::string list;
    for (int k = 0; k < copy_time::kKinds; ++k) {
      const tl::Stat& s = w.ms[k];
      if (s.count == 0) continue;
      const double mb = static_cast<double>(w.bytes[k]) / (1024.0 * 1024.0);
      char part[256];
      snprintf(part, sizeof(part), "%s%s %.2f and %.2f (%llu copies, %.1f MB a copy, %.1f GB "
               "a second)", list.empty() ? "" : ", ", copy_time::Name(k), s.Mean(), s.most,
               static_cast<unsigned long long>(s.count), mb / static_cast<double>(s.count),
               s.sum > 0.0 ? mb / s.sum * 1000.0 / 1024.0 : 0.0);
      list += part;
    }
    AppLog("time         : the GPU of the copies in ms, the mean and the most: %s",
           list.empty() ? "no copy with a time of the GPU" : list.c_str());
  }
  // THE PARTS OF THE LAYERS of the thread of the session (the step "the
  // layers" is their sum but the swapchains), the clock and the CPU time of
  // the thread.
  {
    std::string list;
    double sum = 0.0;
    for (int k = 0; k < XrApp::kPartSwapchain; ++k) {
      char part[160];
      snprintf(part, sizeof(part), "%s%s %.2f and %.2f (CPU %.2f)", list.empty() ? "" : ", ",
               kLayerPartNames[k], xr_.parts[k].Mean(), xr_.parts[k].most,
               xr_.part_cpu[k].Mean());
      list += part;
      sum += xr_.parts[k].Mean();
    }
    AppLog("time         : the parts of the layers in a frame of the session, ms, the mean and "
           "the most, with the CPU time of the thread as a mean: %s. In all %.2f. The pixels of "
           "the CPU: %llu copies from a copy in memory into a staging buffer (%.1f MB, %llu of "
           "them a rect), %llu writes straight into a staging buffer (%.1f MB)",
           list.c_str(), sum, static_cast<unsigned long long>(xr_.staged),
           xr_.staged_bytes / 1048576.0, static_cast<unsigned long long>(xr_.staged_rects),
           static_cast<unsigned long long>(xr_.direct), xr_.direct_bytes / 1048576.0);
  }
  // THE QUEUE AT THE GPU of the play thread (frame_loop_gpu_depth).
  if (frame_loop_) {
    AppLog("time         : the queue at the GPU: frame_loop_gpu_depth=%d, %llu waits of the play "
           "thread for the GPU before a play, %.2f ms as a mean and %.2f at most, %llu frames "
           "of the game that did not show after such a wait (the play took the newest)",
           gpu_depth_, static_cast<unsigned long long>(gate_.count), gate_.Mean(), gate_.most,
           static_cast<unsigned long long>(gate_dropped_));
  }
  // THE GPU PARTS OF THE WALK (gpu_parts=1).
  if (WalkProbeOn()) {
    wp::Totals t;
    WalkProbeTake(&t);
    const double walks = t.Walks() > 0 ? static_cast<double>(t.Walks()) : 1.0;
    std::string list;
    for (int k = 0; k < t.Keys(); ++k) {
      const wp::PartStat& s = t.Pass(k);
      if (s.count == 0) continue;
      char part[256];
      snprintf(part, sizeof(part), "%s%s: %.1f a walk, %.2f and %.2f, %.0f draws a walk",
               list.empty() ? "" : ", ", PassName(t.Key(k)).c_str(), s.count / walks,
               s.ms / walks, s.most, s.draws / walks);
      list += part;
    }
    for (int g = 0; g < wp::kGaps; ++g) {
      const wp::PartStat& s = t.GapStat(g);
      if (s.count == 0) continue;
      char part[200];
      snprintf(part, sizeof(part), "%s%s: %.1f a walk, %.2f and %.2f", list.empty() ? "" : ", ",
               GapName(g), s.count / walks, s.ms / walks, s.most);
      list += part;
    }
    const wp::Counts& c = t.CountsOf();
    // The counts of the renderer since the line before (vk_bind_skip,
    // vk_draw_sort, vk_draw_merge), for each walk of the renderer.
    VkSceneDrawCount now;
    VkSceneDrawCounts(&now);
    static VkSceneDrawCount was = {};  // the line before
    const double rw =
        now.walks > was.walks ? static_cast<double>(now.walks - was.walks) : 1.0;
    AppLog("time         : the GPU parts of the walk in ms (gpu_parts=1), the mean in a walk and "
           "the most in one walk, %llu walks (%llu with no time): %s. In all %.2f in a walk. In "
           "a walk: %.1f passes, %.0f draws, %.0f pipelines, %.0f bindings of descriptor sets, "
           "%.0f descriptor writes, %.1f uploads of %.2f MB. The renderer in a walk: %.0f draws "
           "of the game, %.0f draw commands, %.0f draws with another place (the sort), %.0f "
           "draws in draws of the merge, %.0f binds and states left out (bound already), %llu "
           "draws that a merge left alone (no room in the ring)",
           static_cast<unsigned long long>(t.Walks()), static_cast<unsigned long long>(t.Untimed()),
           list.empty() ? "none" : list.c_str(), t.TotalMs() / walks, c.passes / walks,
           c.draws / walks, c.pipelines / walks, c.sets / walks, c.writes / walks,
           c.uploads / walks, c.upload_bytes / walks / 1048576.0,
           (now.draws - was.draws) / rw, (now.commands - was.commands) / rw,
           (now.moved - was.moved) / rw, (now.joined - was.joined) / rw,
           (now.skipped - was.skipped) / rw,
           static_cast<unsigned long long>(now.no_room - was.no_room));
    was = now;
    // THE ONE PASS (xr_density_pass=1): the walks that drew into a swapchain
    // image since the line before.
    VkSceneDensityCounts pass;
    VkSceneTakeDensityCounts(&pass);
    uint64_t waits = 0;
    double waits_sum = 0.0, waits_most = 0.0;
    XrApp::TakeSwapchainWaits(&waits, &waits_sum, &waits_most);
    if (pass.walks + pass.refused_size + pass.refused_atlas + pass.refused_texel +
            pass.refused_map + pass.refused_seed + waits > 0) {
      AppLog("time         : the one pass (xr_density_pass=1) in this window: %llu walks drew one "
             "time into a swapchain image (%llu draws, %llu maps made, the share of the fragments "
             "that the last map asks for is %.1f percent, which is not the share that the GPU "
             "shades). A part of a walk started again %llu times after a draw "
             "for the panel or a clear of one eye, and %llu of these parts lost their depth. "
             "Walks that went the old way: %llu "
             "for the size of the image, %llu for an atlas, %llu for the texel of the GPU, "
             "%llu for no map or no pipeline, %llu for a walk that is no walk of both eyes with "
             "fovea_res. The play thread waited %llu times for a swapchain image of the pass "
             "(xrWaitSwapchainImage), %.2f ms in all, %.2f ms at most",
             static_cast<unsigned long long>(pass.walks),
             static_cast<unsigned long long>(pass.draws),
             static_cast<unsigned long long>(pass.maps), pass.share * 100.0,
             static_cast<unsigned long long>(pass.cuts),
             static_cast<unsigned long long>(pass.depth_lost),
             static_cast<unsigned long long>(pass.refused_size),
             static_cast<unsigned long long>(pass.refused_atlas),
             static_cast<unsigned long long>(pass.refused_texel),
             static_cast<unsigned long long>(pass.refused_map),
             static_cast<unsigned long long>(pass.refused_seed),
             static_cast<unsigned long long>(waits), waits_sum, waits_most);
    }
    // The meshes on the GPU (gpu_mesh=1 in the game): what the renderer drew
    // since the line before.
    VkSceneMeshCounts mesh;
    VkSceneTakeMeshCounts(&mesh);
    if (mesh.draws + mesh.data_records + mesh.no_data > 0) {
      AppLog("time         : the meshes on the GPU in this window: %llu draws, %llu records of "
             "data (%llu bytes), %u meshes in the arena (%llu bytes), %llu draws with no data "
             "(the data did not come), %llu records with no room in the arena",
             static_cast<unsigned long long>(mesh.draws),
             static_cast<unsigned long long>(mesh.data_records),
             static_cast<unsigned long long>(mesh.data_bytes), mesh.meshes,
             static_cast<unsigned long long>(mesh.arena_bytes),
             static_cast<unsigned long long>(mesh.no_data),
             static_cast<unsigned long long>(mesh.no_room));
    }
  }
  // THE THREADS AND THE CORES.
  Sample now;
  const double read_start = ClockMs();
  Read(&now, now_ms);
  const double read_ms = ClockMs() - read_start;
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
        // A core that went off and on again can have smaller counts.
        if (b.index != c.index || c.total < b.total || c.busy < b.busy) continue;
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
           "in all, %d busy for half the time or more, each in percent: %s. The read of "
           "/proc took %.2f ms",
           last_.game.empty() ? 0.0 : cores(last_.game_ticks, now.game_ticks),
           cores(last_.app_ticks, now.app_ticks),
           cores(last_.other_ticks, now.other_ticks), count, busy, over_half,
           each.empty() ? "none" : each.c_str(), read_ms);
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
  // The threads of the game alone, then the first 24 (a line of the log has
  // 2048 characters at most).
  std::vector<ps::Load> game;
  for (const ps::Load& l : Loads(base, now)) {
    if (l.game) game.push_back(l);
  }
  const size_t count = game.size();
  const std::vector<ps::Load> loads = ps::Top(game, 24);
  std::string list;
  for (const ps::Load& l : loads) {
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
         "%.1f s, the state and the percent of one core (user + system), the first %zu "
         "of %zu: %s",
         (now_ms - arrival_ms) / 1000.0, static_cast<unsigned long long>(last_frame), seconds,
         loads.size(), count, list.empty() ? "none (no process of the game)" : list.c_str());
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
