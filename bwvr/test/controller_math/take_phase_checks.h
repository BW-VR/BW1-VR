// The phase of the take in the headset app, a test (take_late, take_wait_ms,
// src/mod/take_phase_math.h, docs/android.md, "The long frames"): the
// keys, the rule of the wait at the take and its room, the frames of the
// session that the runtime dropped, the spread of a time and the table of
// the frames. main.cpp includes this file after Check and Near, just before
// main.
#pragma once

#include "../../src/mod/take_phase_math.h"

namespace take_phase_checks {

inline void KeyChecks() {
  namespace tp = take_phase_math;
  printf("\nThe phase of the take: the keys\n");
  Check(!tp::TakeLate(0) && tp::TakeLate(1) && !tp::TakeLate(2) && !tp::TakeLate(-1),
        "take_late: only 1 is on", tp::TakeLate(1) ? 1 : 0, 1);
  Check(tp::TakeWaitMs(0) == 0 && tp::TakeWaitMs(-3) == 0, "take_wait_ms 0 and below 0: off",
        tp::TakeWaitMs(-3), 0);
  Check(tp::TakeWaitMs(4) == 4, "take_wait_ms=4 stays 4", tp::TakeWaitMs(4), 4);
  Check(tp::TakeWaitMs(500) == tp::kMostWaitMs, "take_wait_ms=500 is the most value",
        tp::TakeWaitMs(500), tp::kMostWaitMs);
}

inline void WorthChecks() {
  namespace tp = take_phase_math;
  printf("\nThe phase of the take: a wait is worth it\n");
  // The app wrote slot 101 just before the take. The shown picture is of
  // slot 99. The frame of the game for slot 100 runs.
  Check(tp::WaitWorth(false, true, 100, 99, 101),
        "the frame of the slot before the newest runs: wait", 1, 1);
  Check(tp::WaitWorth(false, true, 101, 99, 101),
        "the frame of the newest slot started (the frame of slot 100 ended): wait", 1, 1);
  Check(!tp::WaitWorth(true, true, 100, 99, 101), "a new picture: no wait", 0, 0);
  Check(!tp::WaitWorth(false, false, 100, 99, 101), "no world of the mod: no wait", 0, 0);
  Check(!tp::WaitWorth(false, true, 99, 99, 101),
        "the newest frame of the game is the frame of the shown picture: no wait", 0, 0);
  Check(!tp::WaitWorth(false, true, 98, 99, 101), "a world older than the shown picture: no wait",
        0, 0);
  Check(!tp::WaitWorth(false, true, 99, 0, 101),
        "a frame of slot 99 runs two slots behind (a long frame): no wait", 0, 0);
  Check(!tp::WaitWorth(false, true, 0, 0, 101), "a world with no slot: no wait", 0, 0);
  Check(!tp::WaitWorth(false, true, 102, 99, 101), "a world of a slot that the app did not write: no wait",
        0, 0);
  Check(tp::WaitWorth(false, true, 100, 0, 101),
        "a shown screen picture (no slot) and a new frame of the scene: wait", 1, 1);
}

inline void WaitEndChecks() {
  namespace tp = take_phase_math;
  printf("\nThe phase of the take: the latest end of a wait\n");
  const double p = 1000.0 / 72.0;  // 13.889 ms
  // xrWaitFrame ended at 1000 ms, the take at 1001.5 ms, the rest of the
  // frame after the take 0.5 ms. The room ends at 1000 + 0.75 * 13.889 - 0.5.
  const double room = 1000.0 + 0.75 * p - 0.5;
  Near("take_wait_ms=4: the take plus 4 ms", tp::WaitEnd(1001.5, 4, 1000.0, p, 0.5), 1005.5, 1e-9);
  Near("take_wait_ms=20: the room ends it", tp::WaitEnd(1001.5, 20, 1000.0, p, 0.5), room, 1e-9);
  Near("take_wait_ms=0: no wait", tp::WaitEnd(1001.5, 0, 1000.0, p, 0.5), 1001.5, 1e-9);
  Near("the end of xrWaitFrame is not known: no wait", tp::WaitEnd(1001.5, 4, -1.0, p, 0.5), 1001.5,
       1e-9);
  Near("a take after the room: no wait", tp::WaitEnd(1012.0, 4, 1000.0, p, 2.0), 1012.0, 1e-9);
  Near("no display period: 72 Hz", tp::WaitEnd(1001.5, 20, 1000.0, 0.0, 0.5), room, 1e-9);
  Near("a rest below 0 counts as 0", tp::WaitEnd(1001.5, 20, 1000.0, p, -3.0), room + 0.5, 1e-9);
  // At 120 Hz the room is shorter.
  const double p120 = 1000.0 / 120.0;
  Near("120 Hz, take_wait_ms=8: the room ends it",
       tp::WaitEnd(1001.0, 8, 1000.0, p120, 0.5), 1000.0 + 0.75 * p120 - 0.5, 1e-9);
}

inline void RoomChecks() {
  namespace tp = take_phase_math;
  printf("\nThe phase of the take: the room of the frame of the session\n");
  const double p = 1000.0 / 72.0;
  Near("xrEndFrame ended 1.7 ms after xrWaitFrame: 12.19 ms of the period stay",
       tp::RoomAfterEnd(1000.0, 1001.7, p), p - 1.7, 1e-9);
  Check(tp::RoomAfterEnd(1000.0, 1015.0, p) < 0.0, "a frame longer than a period: below 0",
        tp::RoomAfterEnd(1000.0, 1015.0, p), -1.111);
  const int64_t pn = 13888889;
  Check(tp::DroppedFrames(0, 1000000000, pn) == 0, "the first frame: no drop",
        tp::DroppedFrames(0, 1000000000, pn), 0);
  Check(tp::DroppedFrames(1000000000, 1000000000 + pn, pn) == 0, "one period: no drop",
        tp::DroppedFrames(1000000000, 1000000000 + pn, pn), 0);
  Check(tp::DroppedFrames(1000000000, 1000000000 + pn * 3 / 2, pn) == 0,
        "1.5 periods: no drop", tp::DroppedFrames(1000000000, 1000000000 + pn * 3 / 2, pn), 0);
  Check(tp::DroppedFrames(1000000000, 1000000000 + 2 * pn, pn) == 1, "two periods: one drop",
        tp::DroppedFrames(1000000000, 1000000000 + 2 * pn, pn), 1);
  Check(tp::DroppedFrames(1000000000, 1000000000 + 4 * pn + 100000, pn) == 3,
        "four periods: three drops", tp::DroppedFrames(1000000000, 1000000000 + 4 * pn + 100000, pn), 3);
  Check(tp::DroppedFrames(1000000000, 1000000000 + 2 * pn, 0) == 0, "no period: no drop",
        tp::DroppedFrames(1000000000, 1000000000 + 2 * pn, 0), 0);
  Check(tp::DroppedFrames(1000000000, 900000000, pn) == 0, "a display time that goes back: no drop",
        tp::DroppedFrames(1000000000, 900000000, pn), 0);
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
  KeyChecks();
  WorthChecks();
  WaitEndChecks();
  RoomChecks();
  StartChecks();
  SpreadChecks();
}

}  // namespace take_phase_checks
