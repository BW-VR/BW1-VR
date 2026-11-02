// THE PHASE OF THE TAKE IN THE HEADSET APP (take_late, take_wait_ms,
// docs/android.md, "The long frames"). The pure
// rules of the take of a picture by the session with frame_loop=1. No
// Android code, so the math test of the PC checks them
// (test/controller_math/take_phase_checks.h).
//
//   take_late=0     the take comes after the write of the slot of the input
//                   channel (the default)
//   take_late=1     the take comes after the controllers and the paint of the
//                   menu, just before the layers of xrEndFrame
//   take_wait_ms=0  a take with no new picture shows the picture before (the
//                   default)
//   take_wait_ms=N  a take with no new picture waits up to N ms for the
//                   picture of a frame of the game that started, never past
//                   the room of the frame of the session (WaitEnd)
#pragma once

#include <math.h>
#include <stdint.h>

namespace take_phase_math {

// The most value of take_wait_ms.
constexpr int kMostWaitMs = 20;
// The step of the wait at the take: a look at the ring after each sleep.
constexpr int kWaitStepUs = 200;
// The share of the display period from the end of xrWaitFrame that the frame
// of the session may use up to the end of xrEndFrame with a wait at the take.
// The rest of the period stays free, so the runtime gets the layers in time.
constexpr double kRoomShare = 0.75;
// A frame of the game started for one of the newest kRecentSlots slots of the
// input channel: its picture can come in this frame of the session.
constexpr uint32_t kRecentSlots = 2;
// The display period when the runtime gives none: 72 Hz.
constexpr double kIdlePeriodMs = 1000.0 / 72.0;

// take_late of bwvr.ini: 1 is on, any other value is off.
inline bool TakeLate(int key) { return key == 1; }

// take_wait_ms of bwvr.ini: 0 is off, a value above kMostWaitMs is
// kMostWaitMs.
inline int TakeWaitMs(int key) {
  if (key <= 0) return 0;
  return key > kMostWaitMs ? kMostWaitMs : key;
}

// A WAIT AT THE TAKE IS WORTH IT when the take found no new picture, and the
// mod started a frame of the game for a slot after the slot of the shown
// picture (input_channel::World::input_frame), and that slot is one of the
// newest kRecentSlots slots that the app wrote. Then the picture of that
// frame is in the game, in the way to the app, in the play or on the GPU. A
// world of an old slot (a frame with no scene writes no world) gives no
// wait.
//   world_input  the input frame of the newest world of the mod
//   shown_input  the input frame of the shown picture (0: none)
//   newest_slot  the frame of the newest slot that the app wrote
inline bool WaitWorth(bool fresh, bool have_world, uint32_t world_input, uint32_t shown_input,
                      uint32_t newest_slot) {
  if (fresh || !have_world || world_input == 0) return false;
  if (world_input <= shown_input) return false;
  if (world_input > newest_slot) return false;
  return newest_slot - world_input < kRecentSlots;
}

// THE LATEST END OF A WAIT AT THE TAKE (CLOCK_MONOTONIC, ms): the take plus
// take_wait_ms, but not past the room of the frame of the session. The room
// ends kRoomShare of the display period after the end of xrWaitFrame, less
// the time that the frame needs after the take up to the end of xrEndFrame
// (rest_ms). period_ms 0 or below: kIdlePeriodMs. wait_end_ms below 0 (not
// known): no room, the wait ends at the take.
inline double WaitEnd(double take_ms, int wait_ms, double wait_end_ms, double period_ms,
                      double rest_ms) {
  if (wait_ms <= 0 || wait_end_ms < 0.0) return take_ms;
  const double period = period_ms > 0.0 ? period_ms : kIdlePeriodMs;
  const double rest = rest_ms > 0.0 ? rest_ms : 0.0;
  const double room = wait_end_ms + kRoomShare * period - rest;
  const double end = take_ms + static_cast<double>(wait_ms);
  if (room < take_ms) return take_ms;
  return end < room ? end : room;
}

// THE REST OF THE DISPLAY PERIOD after the end of xrEndFrame: the period from
// the end of xrWaitFrame less the time to the end of xrEndFrame. Below 0 when
// the frame of the session took longer than a period.
inline double RoomAfterEnd(double wait_end_ms, double end_ms, double period_ms) {
  const double period = period_ms > 0.0 ? period_ms : kIdlePeriodMs;
  return wait_end_ms + period - end_ms;
}

// A FRAME OF THE SESSION THAT THE RUNTIME DROPPED: the display time of a
// frame moved more than 1.5 display periods from the frame before. 0 for the
// first frame or with no period.
inline int DroppedFrames(int64_t display_before_ns, int64_t display_ns, int64_t period_ns) {
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

// THE LEAST OF A TIME, -1 while there is none.
struct Least {
  double value = -1.0;
  void Add(double ms) {
    if (value < 0.0 || ms < value) value = ms;
  }
  double Or0() const { return value < 0.0 ? 0.0 : value; }
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
