// THE PHASE OF THE TAKE IN THE HEADSET APP (the line "take phase",
// docs/android.md, "The long frames"). The pure rules of the measure of the
// take of a picture by the session with frame_loop=1: the time from the write
// of a slot to the start of the frame of the game, from that start to the end
// of the GPU work of its picture, and the room of the frame of the session.
// No Android code, so the math test of the PC checks them
// (test/controller_math/take_phase_checks.h).
#pragma once

#include <math.h>
#include <stdint.h>

namespace take_phase_math {

// The display period when the runtime gives none: 72 Hz.
constexpr double kIdlePeriodMs = 1000.0 / 72.0;

// THE REST OF THE DISPLAY PERIOD after the end of xrEndFrame: the period from
// the end of xrWaitFrame less the time to the end of xrEndFrame. Below 0 when
// the frame of the session took longer than a period.
inline double RoomAfterEnd(double wait_end_ms, double end_ms, double period_ms) {
  const double period = period_ms > 0.0 ? period_ms : kIdlePeriodMs;
  return wait_end_ms + period - end_ms;
}

// THE DISPLAY PERIODS THAT THE RUNTIME SKIPPED: the display time of a frame of
// the session moved more than 1.5 display periods from the frame of the
// session before, and that frame before was in time (from the end of its
// xrWaitFrame to the end of its xrEndFrame in less than a period). After a
// frame of the session that was late itself (a stop of the session, a long
// frame of the app) the runtime moves the display time for the app, so that
// step does not count. 0 for the first frame or with no period.
inline int SkippedPeriods(int64_t display_before_ns, int64_t display_ns, int64_t period_ns,
                          bool before_in_time) {
  if (!before_in_time) return 0;
  if (display_before_ns <= 0 || period_ns <= 0 || display_ns <= display_before_ns) return 0;
  const int64_t step = display_ns - display_before_ns;
  if (2 * step <= 3 * period_ns) return 0;
  // The display periods with no frame, rounded.
  return static_cast<int>((step + period_ns / 2) / period_ns) - 1;
}

// THE TIME FROM THE WRITE OF A SLOT TO THE START OF THE FRAME OF THE GAME
// that took it, by the frame before: when the picture of the frame before
// came to the app before the write, the game was at the end of that frame or
// in the wait of the pace at the write. Else the game was still in the frame
// before at the write, and the start waited for it.
inline bool BusyAtWrite(double before_arrival_ms, double write_ms) {
  return before_arrival_ms < 0.0 || before_arrival_ms > write_ms;
}

// The edges of the counts of the time from the write to the start, ms.
constexpr int kStartBins = 4;
constexpr double kStartEdge[kStartBins - 1] = {0.5, 1.0, 2.0};
inline int StartBin(double ms) {
  int b = 0;
  while (b < kStartBins - 1 && ms > kStartEdge[b]) ++b;
  return b;
}

// THE SPREAD OF A TIME: the count, the mean, the standard deviation and the
// most. A time below 0 counts as 0.
struct Spread {
  uint64_t count = 0;
  double sum = 0.0;
  double squares = 0.0;
  double most = 0.0;
  void Add(double ms) {
    if (ms < 0.0) ms = 0.0;
    ++count;
    sum += ms;
    squares += ms * ms;
    if (ms > most) most = ms;
  }
  double Mean() const { return count > 0 ? sum / static_cast<double>(count) : 0.0; }
  double Dev() const {
    if (count < 2) return 0.0;
    const double mean = Mean();
    const double v = squares / static_cast<double>(count) - mean * mean;
    return v > 0.0 ? sqrt(v) : 0.0;
  }
  void Reset() { *this = Spread(); }
};

// THE LEAST OF A TIME, also below 0. Or0 gives 0 while there is none.
struct Least {
  bool have = false;
  double value = 0.0;
  void Add(double ms) {
    if (!have || ms < value) value = ms;
    have = true;
  }
  double Or0() const { return have ? value : 0.0; }
};

// THE TABLE OF THE FRAMES OF THE GAME by their number, the newest N: a time
// for each frame, 0 when the table has no time of that frame.
template <int N>
struct FrameTimes {
  uint64_t frame[N] = {};
  double ms[N] = {};
  void Put(uint64_t f, double t) {
    const int i = static_cast<int>(f % static_cast<uint64_t>(N));
    frame[i] = f;
    ms[i] = t;
  }
  double Get(uint64_t f) const {
    if (f == 0) return 0.0;
    const int i = static_cast<int>(f % static_cast<uint64_t>(N));
    return frame[i] == f ? ms[i] : 0.0;
  }
  void Clear() { *this = FrameTimes(); }
};

}  // namespace take_phase_math
