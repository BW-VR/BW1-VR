#include "long_frames.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include <time.h>

#include "app_log.h"
#include "input_live.h"

namespace lf = long_frame_math;
namespace tp = take_phase_math;

namespace {

constexpr double kWindowMs = 10000.0;

double ClockMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

const char* MissName(lf::Miss m) {
  switch (m) {
    case lf::kGpu:
      return "the GPU had not finished its copies at the take";
    case lf::kPlay:
      return "its frame of the game came before the take, and the play of the app ended "
             "after it";
    case lf::kLong:
      return "a long frame of the game";
    default:
      return "a frame of the game of normal length that came after the take";
  }
}

}  // namespace

// THE KEPT LINES.

void KeptLines::Add(const char* format, ...) {
  const int slot = order_.Push();
  if (slot < 0) return;
  char text[2000];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  slot_[slot] = text;
}

void KeptLines::WriteOne() {
  const int slot = order_.Pop();
  if (slot >= 0) AppLog("%s", slot_[slot].c_str());
}

// THE GAME FRAME CLOCK (the reader thread).

void GameFrameClock::Start(int slow_frame_ms) {
  limit_ms_ = lf::Limit(slow_frame_ms);
  gate_.limit_ms = limit_ms_;
  if (limit_ms_ == 0) return;
  AppLog("game frame   : slow_frame_ms=%d. The reader gives each gap of more than %d ms "
         "between two frames of the game one line, with the time of the main thread of "
         "the game on a core and in the queue for a core (schedstat), at most %d lines a "
         "second, and one line each 10 s. The lines go to the log after the end of their "
         "window, one at each frame",
         limit_ms_, limit_ms_, lf::kLinesPerSecond);
}

bool GameFrameClock::ReadThread(uint64_t* run_ns, uint64_t* wait_ns, uint64_t* slices) {
  const int pid = pid_.load(std::memory_order_relaxed);
  if (pid != open_pid_) {
    if (fd_ >= 0) close(fd_);
    fd_ = -1;
    have_ = false;
    open_pid_ = pid;
    if (pid > 0) {
      char path[64];
      snprintf(path, sizeof(path), "/proc/%d/task/%d/schedstat", pid, pid);
      fd_ = open(path, O_RDONLY | O_CLOEXEC);
      if (fd_ < 0 && !said_fail_) {
        said_fail_ = true;
        AppLog("game frame   : cannot open %s. The lines have no time of the main thread",
               path);
      }
    }
  }
  if (fd_ < 0) return false;
  char text[96];
  const ssize_t n = pread(fd_, text, sizeof(text) - 1, 0);
  if (n <= 0) {
    close(fd_);
    fd_ = -1;
    return false;
  }
  text[n] = 0;
  return lf::ParseSchedstat(text, run_ns, wait_ns, slices);
}

void GameFrameClock::NewWindow(double now_ms) {
  window_ms_ = now_ms;
  gap_.Reset();
  run_ms_ = queue_ms_ = timed_ms_ = 0.0;
  long_ms_ = long_run_ms_ = long_queue_ms_ = long_timed_ms_ = 0.0;
  gate_.NewWindow();
}

double GameFrameClock::Arrive(uint64_t frame, double now_ms) {
  if (limit_ms_ == 0) return 0.0;
  // A new game: no gap from the last frame of the game before, a new window,
  // and the lines of the old window may go.
  if (reset_.exchange(false, std::memory_order_relaxed)) {
    last_ms_ = -1.0;
    have_ = false;
    gate_.last_long_ms = -1.0;
    kept_.WindowEnds();
    NewWindow(now_ms);
  }
  if (window_ms_ < 0.0) window_ms_ = now_ms;
  const double gap = last_ms_ >= 0.0 ? now_ms - last_ms_ : 0.0;
  last_ms_ = now_ms;
  uint64_t run = 0, wait = 0, slices = 0;
  const bool had = have_;
  const bool read = ReadThread(&run, &wait, &slices);
  double run_ms = -1.0, queue_ms = -1.0;
  uint64_t steps = 0;
  if (read && had && run >= run_ns_ && wait >= wait_ns_) {
    run_ms = static_cast<double>(run - run_ns_) / 1e6;
    queue_ms = static_cast<double>(wait - wait_ns_) / 1e6;
    steps = slices - slices_;
  }
  if (read) {
    run_ns_ = run;
    wait_ns_ = wait;
    slices_ = slices;
    have_ = true;
  }
  if (gap > 0.0) {
    gap_.Add(gap);
    if (run_ms >= 0.0) {
      run_ms_ += run_ms;
      queue_ms_ += queue_ms;
      timed_ms_ += gap;
    }
    bool say = false;
    if (gate_.Frame(gap, now_ms, &say)) {
      long_ms_ += gap;
      if (run_ms >= 0.0) {
        long_run_ms_ += run_ms;
        long_queue_ms_ += queue_ms;
        long_timed_ms_ += gap;
      }
      if (say) {
        if (run_ms >= 0.0) {
          kept_.Add("game frame   : the frame %llu of the game came %.2f ms after the frame "
                    "before. In that time the main thread of the game ran %.2f ms on a "
                    "core, waited %.2f ms for a core, and was off a core %.2f ms (%llu time "
                    "slices)",
                    static_cast<unsigned long long>(frame), gap, run_ms, queue_ms,
                    lf::OffCpuMs(gap, run_ms, queue_ms),
                    static_cast<unsigned long long>(steps));
        } else {
          kept_.Add("game frame   : the frame %llu of the game came %.2f ms after the frame "
                    "before. No time of the main thread of the game",
                    static_cast<unsigned long long>(frame), gap);
        }
      }
    }
  }
  if (now_ms - window_ms_ >= kWindowMs) WindowEnds(now_ms);
  return gap;
}

void GameFrameClock::WindowEnds(double now_ms) {
  const double seconds = (now_ms - window_ms_) / 1000.0;
  kept_.Add("game frame   : %.1f s, %llu gaps between frames of the game, ms: the mean %.2f "
            "and the most %.2f. In %.0f ms of them with schedstat the main thread of the "
            "game ran %.0f ms on a core and waited %.0f ms for a core. %llu gaps were longer "
            "than %d ms (%llu with a line): %.0f ms, and in %.0f ms of them with schedstat "
            "the main thread ran %.0f ms and waited %.0f ms for a core",
            seconds, static_cast<unsigned long long>(gap_.count), gap_.Mean(), gap_.most,
            timed_ms_, run_ms_, queue_ms_, static_cast<unsigned long long>(gate_.long_frames),
            limit_ms_, static_cast<unsigned long long>(gate_.lines), long_ms_,
            long_timed_ms_, long_run_ms_, long_queue_ms_);
  kept_.WindowEnds();
  NewWindow(now_ms);
}

// THE LATCH LOG (the thread of the session).

void LatchLog::Start(int slow_frame_ms) {
  limit_ms_ = lf::Limit(slow_frame_ms);
  if (limit_ms_ == 0) return;
  AppLog("late picture : slow_frame_ms=%d. Each picture of the game after a frame of the "
         "session with no new picture gives one line with the cause and how late it was, "
         "at most %d lines a second, and one line each 10 s (frame_loop=1). The lines go "
         "to the log after the end of their window, one at each frame of the session",
         limit_ms_, lf::kLinesPerSecond);
}

void LatchLog::GameStarts(double now_ms) {
  if (limit_ms_ == 0) return;
  latches_ = lf::Latches();
  window_ms_ = now_ms;
}

void LatchLog::Take(double take_ms, bool fresh, double arrival_ms, double ready_ms,
                    double gap_ms, uint64_t frame, double period_ms) {
  if (limit_ms_ == 0) return;
  if (window_ms_ < 0.0) window_ms_ = take_ms;
  ++takes_;
  const lf::LateResult r =
      latches_.Take(take_ms, fresh, arrival_ms, ready_ms, gap_ms, period_ms);
  if (!fresh) {
    if (latches_.misses > 0) ++missed_;
    return;
  }
  if (r.misses == 0) {
    if (r.slack_ms >= 0.0) {
      slack_.Add(r.slack_ms);
      if (slack_least_ < 0.0 || r.slack_ms < slack_least_) slack_least_ = r.slack_ms;
    }
    return;
  }
  by_kind_[r.kind] += static_cast<uint64_t>(r.misses);
  if (r.kind == lf::kLate) {
    late_.Add(r.late_ms);
    ++late_bins_[lf::LateBin(r.late_ms)];
  } else if (r.kind == lf::kLong) {
    long_gap_.Add(gap_ms);
  }
  if (!rate_.May(take_ms)) return;
  kept_.Add("late picture : %d frames of the session had no new picture before the frame "
            "%llu of the game: %s. It came %.2f ms after the first take that missed it, and "
            "its copies ended %.2f ms after that take. Its frame of the game came %.2f ms "
            "after the frame before",
            r.misses, static_cast<unsigned long long>(frame), MissName(r.kind), r.late_ms,
            r.ready_ms, gap_ms);
}

void LatchLog::Cadence(bool fresh, int delay, uint64_t replaced, uint64_t dropped) {
  if (limit_ms_ == 0) return;
  if (fresh) ++delay_bins_[frame_loop::DelayBin(delay)];
  replaced_ += replaced;
  dropped_ += dropped;
}

void LatchLog::Tick(double now_ms) {
  if (limit_ms_ == 0) return;
  kept_.WriteOne();
  if (window_ms_ < 0.0 || now_ms - window_ms_ < kWindowMs) return;
  const double seconds = (now_ms - window_ms_) / 1000.0;
  kept_.Add("late picture : %.1f s, %llu takes of the session, %llu of them with no new "
            "picture: %llu by the GPU, %llu by the play of the app, %llu by a long frame of "
            "the game (the gap %.2f ms as a mean, %.2f at most), %llu by a frame of normal "
            "length. Those frames of normal length came after the take: %llu up to 1 ms, "
            "%llu up to 2 ms, %llu up to 4 ms, %llu more (%.2f ms as a mean, %.2f at most). "
            "The pictures in time ended their copies %.2f ms before the take as a mean, %.2f "
            "at the least (%llu pictures). THE CADENCE: the new pictures by the takes after "
            "the submit of their copies and before the take that showed them: %llu with 0, "
            "%llu with 1, %llu with 2 or more. Pictures that did not show: %llu that a take freed (a newer picture "
            "replaced them), %llu that the play thread dropped (no free slot)",
            seconds, static_cast<unsigned long long>(takes_),
            static_cast<unsigned long long>(missed_),
            static_cast<unsigned long long>(by_kind_[lf::kGpu]),
            static_cast<unsigned long long>(by_kind_[lf::kPlay]),
            static_cast<unsigned long long>(by_kind_[lf::kLong]), long_gap_.Mean(),
            long_gap_.most, static_cast<unsigned long long>(by_kind_[lf::kLate]),
            static_cast<unsigned long long>(late_bins_[0]),
            static_cast<unsigned long long>(late_bins_[1]),
            static_cast<unsigned long long>(late_bins_[2]),
            static_cast<unsigned long long>(late_bins_[3]), late_.Mean(), late_.most,
            slack_.Mean(), slack_.count > 0 ? slack_least_ : 0.0,
            static_cast<unsigned long long>(slack_.count),
            static_cast<unsigned long long>(delay_bins_[0]),
            static_cast<unsigned long long>(delay_bins_[1]),
            static_cast<unsigned long long>(delay_bins_[2]),
            static_cast<unsigned long long>(replaced_),
            static_cast<unsigned long long>(dropped_));
  kept_.WindowEnds();
  window_ms_ = now_ms;
  takes_ = missed_ = 0;
  for (uint64_t& d : delay_bins_) d = 0;
  replaced_ = dropped_ = 0;
  for (uint64_t& k : by_kind_) k = 0;
  for (uint64_t& b : late_bins_) b = 0;
  late_.Reset();
  slack_.Reset();
  long_gap_.Reset();
  slack_least_ = -1.0;
}

// THE PHASE OF THE TAKE.

void TakePhase::Start(bool on) {
  on_ = on;
  if (!on_) return;
  AppLog("take phase   : one line each 10 s with the time from the write of a slot to the "
         "start of the frame of the game that took it, from that start to the end of the GPU "
         "work of its picture, and the times of the frame of the session around the take "
         "(frame_loop=1). The lines go to the log after the end of "
         "their window, one at each frame of the session");
}

void TakePhase::GameStarts(double now_ms, const InputLive* channel) {
  if (!on_) return;
  {
    std::lock_guard<std::mutex> g(lock_);
    writes_.Clear();
    starts_.Clear();
    last_arrival_ = -1.0;
    start_waited_.Reset();
    start_busy_.Reset();
    for (uint64_t& b : start_bins_) b = 0;
    start_no_write_ = 0;
    pending_ = Pending();
    way_.Reset();
    play_wait_.Reset();
    play_.Reset();
    gpu_.Reset();
    total_.Reset();
    gpu_waited_ = no_gpu_ = no_start_ = 0;
  }
  channel_.store(channel, std::memory_order_release);
  reset_.store(true, std::memory_order_release);
  window_ms_ = now_ms;
  polls_at_ = polls_.load(std::memory_order_relaxed);
  to_take_.Reset();
  to_end_.Reset();
  xr_wait_.Reset();
  room_.Reset();
  lead_.Reset();
  room_least_ = tp::Least();
  lead_least_ = tp::Least();
  display_before_ = 0;
  before_in_time_ = false;
  skipped_ = frames_ = 0;
}

void TakePhase::Wrote(uint32_t slot, double ms) {
  if (!on_) return;
  std::lock_guard<std::mutex> g(lock_);
  writes_.Put(slot, ms);
}

void TakePhase::Poll() {
  if (!on_) return;
  polls_.fetch_add(1, std::memory_order_relaxed);
  if (reset_.exchange(false, std::memory_order_acquire)) seen_frame_ = 0;
  const InputLive* channel = channel_.load(std::memory_order_acquire);
  if (channel == nullptr) return;
  input_channel::World w;
  if (!channel->ReadWorld(&w) || w.frame == 0 || w.frame == seen_frame_) return;
  // The first world of a game can be of the game before: it only sets the
  // frame that the reader saw.
  const bool first = seen_frame_ == 0;
  seen_frame_ = w.frame;
  if (first) return;
  const double now = ClockMs();
  std::lock_guard<std::mutex> g(lock_);
  starts_.Put(w.frame, now);
  const double write = writes_.Get(w.input_frame);
  if (write <= 0.0 || now < write) {
    ++start_no_write_;
    return;
  }
  if (tp::BusyAtWrite(last_arrival_, write)) {
    start_busy_.Add(now - write);
  } else {
    start_waited_.Add(now - write);
    ++start_bins_[tp::StartBin(now - write)];
  }
}

void TakePhase::Arrive(uint64_t frame, double ms) {
  (void)frame;
  if (!on_ || ms <= 0.0) return;
  std::lock_guard<std::mutex> g(lock_);
  last_arrival_ = ms;
}

void TakePhase::Flush(double done_ms, bool have_done, bool waited) {
  if (!pending_.on) return;
  pending_.on = false;
  if (pending_.start <= 0.0) {
    ++no_start_;
    return;
  }
  way_.Add(pending_.arrival - pending_.start);
  play_wait_.Add(pending_.play - pending_.arrival);
  play_.Add(pending_.submit - pending_.play);
  if (!have_done) {
    ++no_gpu_;
    return;
  }
  gpu_.Add(done_ms - pending_.submit);
  total_.Add(done_ms - pending_.start);
  if (waited) ++gpu_waited_;
}

void TakePhase::Played(uint64_t frame, double arrival_ms, double play_ms, double submit_ms) {
  if (!on_) return;
  std::lock_guard<std::mutex> g(lock_);
  // A picture before with no end of its GPU work (frame_loop_gpu_depth=0).
  Flush(0.0, false, false);
  pending_.on = true;
  pending_.frame = frame;
  pending_.start = starts_.Get(frame);
  pending_.arrival = arrival_ms;
  pending_.play = play_ms;
  pending_.submit = submit_ms;
  if (arrival_ms <= 0.0) pending_.start = 0.0;
}

void TakePhase::GpuDone(double ms, bool waited) {
  if (!on_) return;
  std::lock_guard<std::mutex> g(lock_);
  Flush(ms, true, waited);
}

void TakePhase::SessionFrame(double wait_end_ms, double xr_wait_ms, double take_ms,
                             double end_ms, double period_ms, int64_t display_ns,
                             int64_t period_ns, double display_ms) {
  if (!on_) return;
  ++frames_;
  if (take_ms >= 0.0) {
    if (wait_end_ms >= 0.0) to_take_.Add(take_ms - wait_end_ms);
    to_end_.Add(end_ms - take_ms);
  }
  double room = 0.0;
  if (wait_end_ms >= 0.0) {
    room = tp::RoomAfterEnd(wait_end_ms, end_ms, period_ms);
    room_.Add(room);
    room_least_.Add(room);
  }
  xr_wait_.Add(xr_wait_ms);
  if (display_ms > 0.0) {
    lead_.Add(display_ms - end_ms);
    lead_least_.Add(display_ms - end_ms);
  }
  skipped_ += static_cast<uint64_t>(
      tp::SkippedPeriods(display_before_, display_ns, period_ns, before_in_time_));
  display_before_ = display_ns;
  before_in_time_ = wait_end_ms >= 0.0 && room > 0.0;
}

void TakePhase::Tick(double now_ms) {
  if (!on_) return;
  kept_.WriteOne();
  if (window_ms_ < 0.0 || now_ms - window_ms_ < kWindowMs) return;
  const double seconds = (now_ms - window_ms_) / 1000.0;
  const uint64_t polls = polls_.load(std::memory_order_relaxed);
  const double polls_per_second =
      seconds > 0.0 ? static_cast<double>(polls - polls_at_) / seconds : 0.0;
  polls_at_ = polls;
  tp::Spread waited, busy, way, play_wait, play, gpu, total;
  uint64_t bins[tp::kStartBins];
  uint64_t no_write, gpu_waited, no_gpu, no_start;
  {
    std::lock_guard<std::mutex> g(lock_);
    waited = start_waited_;
    busy = start_busy_;
    for (int b = 0; b < tp::kStartBins; ++b) bins[b] = start_bins_[b];
    no_write = start_no_write_;
    way = way_;
    play_wait = play_wait_;
    play = play_;
    gpu = gpu_;
    total = total_;
    gpu_waited = gpu_waited_;
    no_gpu = no_gpu_;
    no_start = no_start_;
    start_waited_.Reset();
    start_busy_.Reset();
    for (uint64_t& b : start_bins_) b = 0;
    start_no_write_ = 0;
    way_.Reset();
    play_wait_.Reset();
    play_.Reset();
    gpu_.Reset();
    total_.Reset();
    gpu_waited_ = no_gpu_ = no_start_ = 0;
  }
  char lead[96];
  if (lead_.count > 0) {
    snprintf(lead, sizeof(lead), "%.2f ms as a mean and %.2f at the least", lead_.Mean(),
             lead_least_.Or0());
  } else {
    snprintf(lead, sizeof(lead), "not known (no display time on the clock of the app)");
  }
  kept_.Add(
      "take phase   : %.1f s. THE WRITE OF A SLOT TO THE START "
      "OF THE FRAME OF THE GAME that took it: %llu frames whose frame before came before the "
      "write, %.2f ms as a mean (the deviation %.2f) and %.2f at most (%llu up to 0.5 ms, %llu "
      "up to 1 ms, %llu up to 2 ms, %llu more), %llu frames whose frame before came after the "
      "write, %.2f ms as a mean and %.2f at most, %llu starts with no time of the write. The "
      "reader looked %.0f times a second. THE START OF A FRAME OF THE GAME TO THE END OF THE "
      "GPU WORK OF ITS PICTURE: %llu pictures, %.2f ms as a mean (the deviation %.2f) and "
      "%.2f at most. Of it the frame and the way to the app %.2f (%.2f) and %.2f, the wait "
      "for the play %.2f (%.2f) and %.2f, the play %.2f (%.2f) and %.2f, the GPU %.2f (%.2f) "
      "and %.2f (%llu with a wait of the play thread for the GPU, %llu with no end of the "
      "GPU work, %llu with no start). THE FRAME OF THE SESSION, %llu frames: the end of "
      "xrWaitFrame to the take %.2f ms as a mean and %.2f at most, the take to the end of "
      "xrEndFrame %.2f and %.2f, the wait in xrWaitFrame %.2f and %.2f. The rest of the "
      "display period after the end of xrEndFrame %.2f ms as a mean and %.2f at the least. "
      "The display time after the end of xrEndFrame: %s. %llu display periods that the "
      "runtime skipped after a frame of the session in time (the display time moved more "
      "than 1.5 periods)",
      seconds, static_cast<unsigned long long>(waited.count),
      waited.Mean(), waited.Dev(), waited.most, static_cast<unsigned long long>(bins[0]),
      static_cast<unsigned long long>(bins[1]), static_cast<unsigned long long>(bins[2]),
      static_cast<unsigned long long>(bins[3]), static_cast<unsigned long long>(busy.count),
      busy.Mean(), busy.most, static_cast<unsigned long long>(no_write), polls_per_second,
      static_cast<unsigned long long>(total.count), total.Mean(), total.Dev(), total.most,
      way.Mean(), way.Dev(), way.most, play_wait.Mean(), play_wait.Dev(), play_wait.most,
      play.Mean(), play.Dev(), play.most, gpu.Mean(), gpu.Dev(), gpu.most,
      static_cast<unsigned long long>(gpu_waited), static_cast<unsigned long long>(no_gpu),
      static_cast<unsigned long long>(no_start), static_cast<unsigned long long>(frames_),
      to_take_.Mean(), to_take_.most, to_end_.Mean(), to_end_.most, xr_wait_.Mean(),
      xr_wait_.most, room_.Mean(), room_least_.Or0(), lead,
      static_cast<unsigned long long>(skipped_));
  kept_.WindowEnds();
  window_ms_ = now_ms;
  to_take_.Reset();
  to_end_.Reset();
  xr_wait_.Reset();
  room_.Reset();
  lead_.Reset();
  room_least_ = tp::Least();
  lead_least_ = tp::Least();
  skipped_ = frames_ = 0;
}
