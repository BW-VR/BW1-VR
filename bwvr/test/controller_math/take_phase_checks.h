// The phase of the take in the headset app, a test (the line "take phase",
// src/mod/take_phase_math.h, docs/android.md, "The long frames"): the room of
// the frame of the session, the display periods that the runtime skipped, the
// write to the start, the spread and the least of a time, and the table of
// the frames. main.cpp includes this file after Check and Near, just before
// main.
#pragma once

#include "../../src/mod/take_phase_math.h"

namespace take_phase_checks {

inline void RoomChecks() {
  namespace tp = take_phase_math;
  printf("\nThe phase of the take: the room of the frame of the session\n");
  const double p = 1000.0 / 72.0;
  Near("xrEndFrame ended 1.7 ms after xrWaitFrame: 12.19 ms of the period stay",
       tp::RoomAfterEnd(1000.0, 1001.7, p), p - 1.7, 1e-9);
  Check(tp::RoomAfterEnd(1000.0, 1015.0, p) < 0.0, "a frame longer than a period: below 0",
        tp::RoomAfterEnd(1000.0, 1015.0, p), -1.111);
  Near("no display period: 72 Hz", tp::RoomAfterEnd(1000.0, 1001.7, 0.0), p - 1.7, 1e-9);
  const int64_t pn = 13888889;
  const int64_t t0 = 1000000000;
  Check(tp::SkippedPeriods(0, t0, pn, true) == 0, "the first frame: no skip",
        tp::SkippedPeriods(0, t0, pn, true), 0);
  Check(tp::SkippedPeriods(t0, t0 + pn, pn, true) == 0, "one period: no skip",
        tp::SkippedPeriods(t0, t0 + pn, pn, true), 0);
  Check(tp::SkippedPeriods(t0, t0 + pn * 3 / 2, pn, true) == 0, "1.5 periods: no skip",
        tp::SkippedPeriods(t0, t0 + pn * 3 / 2, pn, true), 0);
  Check(tp::SkippedPeriods(t0, t0 + 2 * pn, pn, true) == 1, "two periods: one skip",
        tp::SkippedPeriods(t0, t0 + 2 * pn, pn, true), 1);
  Check(tp::SkippedPeriods(t0, t0 + 4 * pn + 100000, pn, true) == 3,
        "four periods: three skips", tp::SkippedPeriods(t0, t0 + 4 * pn + 100000, pn, true), 3);
  Check(tp::SkippedPeriods(t0, t0 + 4 * pn, pn, false) == 0,
        "four periods after a frame of the session that was late itself: no skip",
        tp::SkippedPeriods(t0, t0 + 4 * pn, pn, false), 0);
  Check(tp::SkippedPeriods(t0, t0 + 2 * pn, 0, true) == 0, "no period: no skip",
        tp::SkippedPeriods(t0, t0 + 2 * pn, 0, true), 0);
  Check(tp::SkippedPeriods(t0, t0 - 100000000, pn, true) == 0,
        "a display time that goes back: no skip", tp::SkippedPeriods(t0, t0 - 100000000, pn, true),
        0);
}
inline void StartChecks() {
  namespace tp = take_phase_math;
  printf("\nThe phase of the take: the write to the start\n");
  Check(!tp::BusyAtWrite(995.0, 1000.0), "the frame before came before the write: free", 0, 0);
  Check(tp::BusyAtWrite(1002.0, 1000.0), "the frame before came after the write: busy", 1, 1);
  Check(tp::BusyAtWrite(-1.0, 1000.0), "no frame before: busy", 1, 1);
  Check(tp::StartBin(0.3) == 0 && tp::StartBin(0.5) == 0, "up to 0.5 ms: bin 0", tp::StartBin(0.5), 0);
  Check(tp::StartBin(0.8) == 1 && tp::StartBin(1.0) == 1, "up to 1 ms: bin 1", tp::StartBin(1.0), 1);
  Check(tp::StartBin(1.7) == 2, "up to 2 ms: bin 2", tp::StartBin(1.7), 2);
  Check(tp::StartBin(6.0) == 3, "more: bin 3", tp::StartBin(6.0), 3);
}

inline void SpreadChecks() {
  namespace tp = take_phase_math;
  printf("\nThe phase of the take: the spread of a time and the table of the frames\n");
  tp::Spread s;
  Check(s.Mean() == 0.0 && s.Dev() == 0.0, "no time: the mean and the deviation are 0", s.Mean(), 0);
  s.Add(2.0);
  Check(s.Dev() == 0.0, "one time: no deviation", s.Dev(), 0);
  s.Add(4.0);
  s.Add(4.0);
  s.Add(4.0);
  s.Add(5.0);
  s.Add(5.0);
  s.Add(7.0);
  s.Add(9.0);
  Near("the mean of 2 4 4 4 5 5 7 9", s.Mean(), 5.0, 1e-12);
  Near("the deviation of 2 4 4 4 5 5 7 9", s.Dev(), 2.0, 1e-12);
  Check(s.most == 9.0 && s.count == 8, "the most and the count", s.most, 9.0);
  s.Add(-1.0);
  Check(s.count == 9 && s.most == 9.0, "a time below 0 counts as 0", static_cast<double>(s.count), 9);
  tp::Least l;
  Check(l.Or0() == 0.0, "no least: 0", l.Or0(), 0);
  l.Add(3.0);
  l.Add(1.5);
  l.Add(2.0);
  Near("the least of 3 1.5 2", l.Or0(), 1.5, 1e-12);
  tp::Least below;
  below.Add(5.0);
  below.Add(-1.0);
  below.Add(8.0);
  Near("the least of 5 -1 8 is -1", below.Or0(), -1.0, 1e-12);

  tp::FrameTimes<4> t;
  t.Put(10, 100.0);
  t.Put(11, 114.0);
  Near("the time of frame 10", t.Get(10), 100.0, 1e-12);
  Near("the time of frame 11", t.Get(11), 114.0, 1e-12);
  Check(t.Get(12) == 0.0 && t.Get(0) == 0.0, "no time of frame 12 and of frame 0", t.Get(12), 0);
  t.Put(14, 156.0);
  Check(t.Get(10) == 0.0, "frame 14 takes the place of frame 10", t.Get(10), 0);
  Near("the time of frame 14", t.Get(14), 156.0, 1e-12);
  t.Clear();
  Check(t.Get(14) == 0.0, "a clear table has no time", t.Get(14), 0);
}

inline void TakePhaseChecks() {
  RoomChecks();
  StartChecks();
  SpreadChecks();
}

}  // namespace take_phase_checks
