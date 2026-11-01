// THE LONG FRAMES OF THE GAME (slow_frame_ms, docs/android.md, "The long
// frames"). The pure rules of the lines of the long frames in the mod and in
// the headset app. No Windows code and no Android code, so the math test of
// the PC checks them (test/controller_math/long_frame_checks.h).
//
//   slow_frame_ms=0  no line of a long frame (the default)
//   slow_frame_ms=N  each frame of the game that is longer than N ms gives
//                    one line, at most kLinesPerSecond lines in a second
#pragma once

#include <stdint.h>

namespace long_frame_math {

// The most lines of long frames in one second of the clock.
constexpr int kLinesPerSecond = 5;

// slow_frame_ms of bwvr.ini: 0 is off. A value above 1000 is 1000.
inline int Limit(int key) {
  if (key <= 0) return 0;
  return key > 1000 ? 1000 : key;
}

// THE RATE OF THE LINES: a line may go while the second of the clock in
// which it comes has fewer than kLinesPerSecond lines. A second starts at
// the first line after the second before ended.
struct LineRate {
  double second_start = -1.0;  // the clock at the start of the second of the lines
  int lines_in_second = 0;
  bool May(double now_ms) {
    if (second_start < 0.0 || now_ms - second_start >= 1000.0 || now_ms < second_start) {
      second_start = now_ms;
      lines_in_second = 0;
    }
    if (lines_in_second >= kLinesPerSecond) return false;
    ++lines_in_second;
    return true;
  }
};

// THE GATE OF THE LINES. A frame that is longer than the limit is a long
// frame. A long frame gets a line by the rate of the lines (LineRate). The
// counts are of the window of the 10 s line of the time log.
struct Gate {
  int limit_ms = 0;
  LineRate rate;
  uint64_t long_frames = 0;    // the long frames of the window
  uint64_t lines = 0;          // the lines of the window
  double last_long_ms = -1.0;  // the clock at the end of the long frame before, or -1
  double since_last_ms = -1.0; // for the frame of the last call: the time since the long frame before, or -1

  // A frame of frame_ms ended at the clock now_ms. True when it is a long
  // frame. *say: true when its line may go.
  bool Frame(double frame_ms, double now_ms, bool* say) {
    *say = false;
    if (limit_ms <= 0 || frame_ms <= static_cast<double>(limit_ms)) return false;
    ++long_frames;
    since_last_ms = last_long_ms >= 0.0 ? now_ms - last_long_ms : -1.0;
    last_long_ms = now_ms;
    if (rate.May(now_ms)) {
      ++lines;
      *say = true;
    }
    return true;
  }
  void NewWindow() { long_frames = lines = 0; }
};

// THE ORDER OF THE KEPT LINES: the slots of a ring of N lines. A line of a
// frame waits (Push) until its window ends (WindowEnds). Then the lines of
// that window go to the log one at a time (Pop, one call for each frame), so
// no frame has more than one write of such a line. A line for a full ring
// is lost (counted).
template <int N>
struct KeptOrder {
  int head = 0;    // the slot of the oldest line
  int count = 0;   // the lines in the ring
  int ready = 0;   // of them, the oldest that may go
  uint64_t lost = 0;
  // The slot for a new line, or -1 when the ring is full.
  int Push() {
    if (count >= N) {
      ++lost;
      return -1;
    }
    const int slot = (head + count) % N;
    ++count;
    return slot;
  }
  // The window ended: each line in the ring may go.
  void WindowEnds() { ready = count; }
  // The slot of the next line to write, or -1. The slot stays valid until
  // the next Push.
  int Pop() {
    if (ready <= 0) return -1;
    const int slot = head;
    head = (head + 1) % N;
    --count;
    --ready;
    return slot;
  }
};

// THE CPU TIME OF A THREAD from a clock of the system in units of 100 ns
// (GetThreadTimes). Take gives the time since the call before and keeps the
// least step above 0 that the clock made: the resolution of the clock.
struct CpuClock {
  bool have = false;
  uint64_t last = 0;
  uint64_t least_step = 0;  // 0 while the clock made no step
  uint64_t Take(uint64_t now) {
    uint64_t step = 0;
    if (have && now > last) step = now - last;
    if (step > 0 && (least_step == 0 || step < least_step)) least_step = step;
    have = true;
    last = now;
    return step;
  }
};

// THE CALLS OF THE FRAME LOOP OF THE GAME in a frame (frame_timers.cpp and
// pass_guard.cpp): the time in each call since the last present. A call that
// a present splits counts in each frame for its part, as the scene walk of
// time_log_math::Frames does.
enum Call { kInput = 0, kTurn = 1, kGraphics = 2, kFlip = 3, kFlush = 4, kCalls = 5 };

struct Calls {
  double start[kCalls] = {-1.0, -1.0, -1.0, -1.0, -1.0};
  double open[kCalls] = {0.0, 0.0, 0.0, 0.0, 0.0};
  double last[kCalls] = {0.0, 0.0, 0.0, 0.0, 0.0};  // the frame that ended last

  void Begin(int c, double now) {
    if (c >= 0 && c < kCalls) start[c] = now;
  }
  void End(int c, double now) {
    if (c < 0 || c >= kCalls) return;
    if (start[c] >= 0.0 && now > start[c]) open[c] += now - start[c];
    start[c] = -1.0;
  }
  // A present at the clock now: the times of the frame go to `last`.
  void Present(double now) {
    for (int c = 0; c < kCalls; ++c) {
      if (start[c] >= 0.0) {
        if (now > start[c]) open[c] += now - start[c];
        start[c] = now;
      }
      last[c] = open[c];
      open[c] = 0.0;
    }
  }
  // The time of the frame of frame_ms outside these calls, not below 0.
  double Outside(double frame_ms) const {
    double in = 0.0;
    for (int c = 0; c < kCalls; ++c) in += last[c];
    return frame_ms > in ? frame_ms - in : 0.0;
  }
};

// THE LINE OF /proc/<pid>/task/<tid>/schedstat of Linux: the time on a CPU
// in ns, the time in the queue for a CPU in ns, and the count of the time
// slices. False when the text does not start with three numbers.
inline bool ParseSchedstat(const char* text, uint64_t* run_ns, uint64_t* wait_ns,
                           uint64_t* slices) {
  uint64_t v[3] = {0, 0, 0};
  const char* p = text;
  for (int i = 0; i < 3; ++i) {
    while (*p == ' ' || *p == '\t') ++p;
    if (*p < '0' || *p > '9') return false;
    uint64_t n = 0;
    while (*p >= '0' && *p <= '9') {
      n = n * 10 + static_cast<uint64_t>(*p - '0');
      ++p;
    }
    v[i] = n;
  }
  *run_ns = v[0];
  *wait_ns = v[1];
  *slices = v[2];
  return true;
}

// THE PARTS OF A FRAME OF THE GAME BY THE CLOCK OF ITS THREAD: of the time
// between two frames (gap_ms), the thread ran run_ms on a CPU and waited
// queue_ms for a CPU. The rest is the time in which it slept or waited for
// an event. Not below 0.
inline double OffCpuMs(double gap_ms, double run_ms, double queue_ms) {
  const double off = gap_ms - run_ms - queue_ms;
  return off > 0.0 ? off : 0.0;
}

// THE LATE PICTURES IN THE HEADSET APP (frame_loop=1). At each take of the
// session (the latch of a frame of the session) the app takes the newest
// picture that the GPU finished, or none. When a take finds no new picture,
// the next new picture says why:
//   kGpu   its copies went to the GPU before the first take that missed it,
//          and the GPU had not finished them
//   kLong  its frame of the game came more than 1.5 display periods after
//          the frame of the game before it: a long frame of the game
//   kPlay  its frame of the game came before that take, and the play of the
//          app ended its copies after it
//   kLong  its frame of the game came after that take, more than 1.5 display
//          periods after the frame of the game before it: a long frame of
//          the game
//   kLate  its frame of the game came after that take, and it is a frame of
//          normal length: it missed the take by late_ms
enum Miss { kGpu = 0, kPlay = 1, kLong = 2, kLate = 3, kMisses = 4 };

// The edges of the counts of the late pictures by how late they were, ms.
constexpr int kLateBins = 4;
constexpr double kLateEdge[kLateBins - 1] = {1.0, 2.0, 4.0};
inline int LateBin(double late_ms) {
  int b = 0;
  while (b < kLateBins - 1 && late_ms > kLateEdge[b]) ++b;
  return b;
}

struct LateResult {
  int misses = 0;       // the takes with no new picture before this take
  Miss kind = kLate;    // why, when misses > 0
  double late_ms = 0;   // the arrival of the picture after the first take that missed it
  double ready_ms = 0;  // the end of its copies after that take
  double slack_ms = -1; // misses == 0: the time from the end of its copies to this take
};

struct Latches {
  double first_miss_ms = -1.0;  // the clock of the first take with no new picture, or -1
  int misses = 0;               // the takes with no new picture since the last new picture
  bool any = false;             // a new picture came in this game

  // A take at the clock take_ms. fresh: it has a new picture, with the
  // arrival of its newest frame of the game, the end of its copies, the time
  // from the arrival of the frame of the game before (gap_ms, 0 when not
  // known) and the display period (period_ms, 0 when not known: 72 Hz).
  LateResult Take(double take_ms, bool fresh, double arrival_ms, double ready_ms,
                  double gap_ms, double period_ms) {
    LateResult r;
    if (!fresh) {
      if (any) {
        if (misses == 0) first_miss_ms = take_ms;
        ++misses;
      }
      return r;
    }
    any = true;
    r.misses = misses;
    if (misses > 0) {
      const double period = period_ms > 0.0 ? period_ms : 1000.0 / 72.0;
      r.late_ms = arrival_ms - first_miss_ms;
      r.ready_ms = ready_ms - first_miss_ms;
      if (ready_ms > 0.0 && ready_ms <= first_miss_ms) {
        r.kind = kGpu;
      } else if (arrival_ms > 0.0 && arrival_ms <= first_miss_ms) {
        r.kind = kPlay;
      } else if (gap_ms > 1.5 * period) {
        r.kind = kLong;
      } else {
        r.kind = kLate;
      }
    } else if (ready_ms > 0.0) {
      r.slack_ms = take_ms - ready_ms;
    }
    misses = 0;
    first_miss_ms = -1.0;
    return r;
  }
};

}  // namespace long_frame_math
