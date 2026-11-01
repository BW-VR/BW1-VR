#include "long_frames.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "app_log.h"

namespace lf = long_frame_math;

namespace {

constexpr double kWindowMs = 10000.0;

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
  char text[800];
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
            "at the least (%llu pictures)",
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
            static_cast<unsigned long long>(slack_.count));
  kept_.WindowEnds();
  window_ms_ = now_ms;
  takes_ = missed_ = 0;
  for (uint64_t& k : by_kind_) k = 0;
  for (uint64_t& b : late_bins_) b = 0;
  late_.Reset();
  slack_.Reset();
  long_gap_.Reset();
  slack_least_ = -1.0;
}
