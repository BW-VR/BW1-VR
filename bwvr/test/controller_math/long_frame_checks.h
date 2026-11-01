// The long frames of the game, a test (slow_frame_ms, src/mod/long_frame_math.h,
// docs/android.md, "The long frames"): the gate of the lines, the clock of
// the CPU time of a thread, the calls of the frame loop that a present
// splits, the line of schedstat, and the causes of a late picture in the
// headset app. main.cpp includes this file after Check and Near, just before
// main.
#pragma once

#include "../../src/mod/long_frame_math.h"
#include "../../src/mod/time_log_math.h"

namespace long_frame_checks {

inline void GateChecks() {
  namespace lf = long_frame_math;
  printf("\nThe long frames: the gate of the lines (slow_frame_ms)\n");
  Check(lf::Limit(0) == 0 && lf::Limit(-5) == 0, "slow_frame_ms=0 and below 0: off",
        lf::Limit(-5), 0);
  Check(lf::Limit(20) == 20 && lf::Limit(5000) == 1000,
        "slow_frame_ms=20 stays 20, 5000 is 1000", lf::Limit(5000), 1000);

  lf::Gate off;
  bool say = true;
  Check(!off.Frame(500.0, 1000.0, &say) && !say && off.long_frames == 0,
        "the limit 0: no frame is long, and no line", static_cast<double>(off.long_frames), 0);

  lf::Gate g;
  g.limit_ms = 20;
  Check(!g.Frame(20.0, 1000.0, &say) && !say,
        "a frame of exactly the limit is not long", say ? 1 : 0, 0);
  Check(!g.Frame(14.0, 1014.0, &say) && !say, "a frame of 14 ms is not long", say ? 1 : 0, 0);
  Check(g.Frame(38.4, 1052.4, &say) && say && g.since_last_ms < 0.0,
        "a frame of 38.4 ms is long, gets a line, and is the first long frame",
        g.since_last_ms, -1.0);
  // Ten long frames in the same second: five lines.
  int lines = 1;
  for (int i = 1; i < 10; ++i) {
    g.Frame(25.0, 1052.4 + 25.0 * i, &say);
    if (say) ++lines;
  }
  Check(lines == lf::kLinesPerSecond && g.long_frames == 10 && g.lines == 5,
        "ten long frames in one second: 10 counted, 5 lines", lines, lf::kLinesPerSecond);
  Near("the time since the long frame before", g.since_last_ms, 25.0, 1e-9);
  // The next second of the clock gives lines again.
  const bool next_long = g.Frame(30.0, 2100.0, &say);
  Check(next_long && say && g.lines == 6, "a long frame in the next second gets a line again",
        static_cast<double>(g.lines), 6);
  Near("its time since the long frame before", g.since_last_ms, 2100.0 - (1052.4 + 225.0),
       1e-9);
  g.NewWindow();
  Check(g.long_frames == 0 && g.lines == 0 && g.last_long_ms == 2100.0,
        "a new window: the counts are 0, the time of the last long frame stays",
        static_cast<double>(g.long_frames), 0);
}

inline void CpuAndCallChecks() {
  namespace lf = long_frame_math;
  printf("\nThe long frames: the CPU time of the thread and the calls of the frame loop\n");
  lf::CpuClock c;
  Check(c.Take(1000) == 0 && c.least_step == 0, "the first read gives no time",
        static_cast<double>(c.least_step), 0);
  Check(c.Take(1000) == 0 && c.least_step == 0, "no step of the clock: 0, and no least step",
        static_cast<double>(c.least_step), 0);
  // A clock of Windows: steps of 15.625 ms (156250 units of 100 ns).
  const uint64_t step = c.Take(1000 + 156250);
  Check(step == 156250 && c.least_step == 156250, "a step of 15.625 ms",
        static_cast<double>(step), 156250);
  // A clock of Linux with USER_HZ 100: steps of 10 ms.
  c.Take(1000 + 156250 + 100000);
  Check(c.least_step == 100000, "a step of 10 ms is the least step now",
        static_cast<double>(c.least_step), 100000);
  Check(c.Take(5) == 0 && c.least_step == 100000,
        "a clock that goes back gives 0 and keeps the least step",
        static_cast<double>(c.least_step), 100000);

  lf::Calls k;
  // A frame: the input 1 ms, the game turn 3 ms, then ProcessGraphicsEngine
  // from 10 ms with the present at 20 ms in it, and its end at 22 ms.
  k.Begin(lf::kInput, 0.0);
  k.End(lf::kInput, 1.0);
  k.Begin(lf::kTurn, 2.0);
  k.End(lf::kTurn, 5.0);
  k.Begin(lf::kGraphics, 10.0);
  k.Present(20.0);
  Near("the input of the frame", k.last[lf::kInput], 1.0, 1e-9);
  Near("the game turn of the frame", k.last[lf::kTurn], 3.0, 1e-9);
  Near("ProcessGraphicsEngine up to the present", k.last[lf::kGraphics], 10.0, 1e-9);
  Near("outside the calls, of a frame of 20 ms", k.Outside(20.0), 6.0, 1e-9);
  k.End(lf::kGraphics, 22.0);
  k.Begin(lf::kFlip, 23.0);
  k.End(lf::kFlip, 24.0);
  k.Present(30.0);
  Near("the next frame has the end of ProcessGraphicsEngine after the present",
       k.last[lf::kGraphics], 2.0, 1e-9);
  Near("and its FlipScreen", k.last[lf::kFlip], 1.0, 1e-9);
  Near("and no game turn", k.last[lf::kTurn], 0.0, 1e-9);
  Near("outside the calls is not below 0", k.Outside(1.0), 0.0, 1e-9);

  // The parts of the last frame in time_log_math::Frames.
  time_log_math::Frames f;
  f.Present(100.0);
  Check(f.last_frame == 0.0, "the first present has no frame", f.last_frame, 0.0);
  f.WalkBegin(102.0);
  f.Add(time_log_math::kPace, 3.0);
  f.Present(114.0);
  Near("the last frame: 14 ms", f.last_frame, 14.0, 1e-9);
  Near("its scene walk up to the present", f.last_part[time_log_math::kScene], 12.0, 1e-9);
  Near("its wait of the pace", f.last_part[time_log_math::kPace], 3.0, 1e-9);
  Near("its rest is not below 0", f.last_rest, 0.0, 1e-9);
}

inline void SchedstatChecks() {
  namespace lf = long_frame_math;
  printf("\nThe long frames: the line of schedstat of a thread\n");
  uint64_t run = 0, wait = 0, slices = 0;
  const bool parsed = lf::ParseSchedstat("123456789 2345678 901\n", &run, &wait, &slices);
  Check(parsed && run == 123456789 && wait == 2345678 && slices == 901,
        "three numbers: the run time, the wait for a CPU, the slices",
        static_cast<double>(wait), 2345678);
  Check(!lf::ParseSchedstat("", &run, &wait, &slices), "an empty line is not valid", 0, 0);
  Check(!lf::ParseSchedstat("12 x 3", &run, &wait, &slices), "a word is not valid", 0, 0);
  Near("the time off a CPU: 38.4 - 20.1 - 3.3", lf::OffCpuMs(38.4, 20.1, 3.3), 15.0, 1e-9);
  Near("the time off a CPU is not below 0", lf::OffCpuMs(10.0, 9.0, 3.0), 0.0, 1e-9);
}

inline void LatchChecks() {
  namespace lf = long_frame_math;
  printf("\nThe long frames: why a frame of the session had no new picture\n");
  const double p = 1000.0 / 72.0;
  lf::Latches l;
  lf::LateResult r = l.Take(0.0, false, 0, 0, 0, p);
  Check(l.misses == 0 && r.misses == 0, "no miss before the first picture of the game",
        l.misses, 0);
  r = l.Take(13.9, true, 5.0, 9.0, 13.9, p);
  Check(r.misses == 0, "the first picture", r.misses, 0);
  Near("its slack: the take less the end of its copies", r.slack_ms, 4.9, 1e-9);

  // A frame of the game of 38 ms: two takes with no picture.
  l.Take(27.8, false, 0, 0, 0, p);
  l.Take(41.7, false, 0, 0, 0, p);
  r = l.Take(55.6, true, 43.0, 46.0, 38.0, p);
  Check(r.misses == 2 && r.kind == lf::kLong, "two misses, then a picture of a long frame",
        r.misses, 2);
  Near("it came this much after the first take that missed it", r.late_ms, 43.0 - 27.8, 1e-9);
  Near("and its copies ended this much after it", r.ready_ms, 46.0 - 27.8, 1e-9);

  // A frame of normal length that missed by 1.5 ms.
  l.Take(69.4, false, 0, 0, 0, p);
  r = l.Take(83.3, true, 70.9, 72.0, 15.3, p);
  Check(r.misses == 1 && r.kind == lf::kLate, "a frame of normal length that came late",
        r.misses, 1);
  Near("late by 1.5 ms", r.late_ms, 1.5, 1e-9);
  Check(lf::LateBin(r.late_ms) == 1, "1.5 ms is in the bin up to 2 ms", lf::LateBin(r.late_ms), 1);
  Check(lf::LateBin(0.5) == 0 && lf::LateBin(3.0) == 2 && lf::LateBin(9.0) == 3,
        "the bins: up to 1, 2, 4 ms and more", lf::LateBin(9.0), 3);

  // The frame came before the take, and the copies ended after it: the play.
  l.Take(97.2, false, 0, 0, 0, p);
  r = l.Take(111.1, true, 96.0, 98.0, 13.9, p);
  Check(r.kind == lf::kPlay, "the frame came before the take, the copies after it: the play",
        static_cast<int>(r.kind), static_cast<int>(lf::kPlay));

  // The copies ended before the take, and the GPU had not finished them.
  l.Take(125.0, false, 0, 0, 0, p);
  r = l.Take(138.9, true, 120.0, 123.0, 13.9, p);
  Check(r.kind == lf::kGpu, "the copies ended before the take: the GPU",
        static_cast<int>(r.kind), static_cast<int>(lf::kGpu));

  // No display period: 72 Hz. A gap of 22 ms is long (1.5 periods are 20.8).
  l.Take(152.8, false, 0, 0, 0, 0.0);
  r = l.Take(166.7, true, 160.0, 161.0, 22.0, 0.0);
  Check(r.kind == lf::kLong, "with no period a gap of 22 ms is long at 72 Hz",
        static_cast<int>(r.kind), static_cast<int>(lf::kLong));
  Check(l.misses == 0 && l.first_miss_ms < 0.0, "a picture ends the misses", l.misses, 0);
}

inline void KeptChecks() {
  namespace lf = long_frame_math;
  printf("\nThe long frames: the order of the kept lines\n");
  lf::KeptOrder<4> k;
  Check(k.Pop() == -1, "no line: nothing to write", k.Pop(), -1);
  const int a = k.Push();
  const int b = k.Push();
  Check(a == 0 && b == 1 && k.Pop() == -1, "two lines of an open window: none may go yet",
        k.count, 2);
  k.WindowEnds();
  const int c = k.Push();  // a line of the next window
  Check(k.Pop() == a, "the window ended: the oldest line goes first", a, 0);
  Check(k.Pop() == b, "then the second line, one at each call", b, 1);
  Check(k.Pop() == -1 && k.count == 1, "the line of the next window waits for its end",
        k.count, 1);
  k.WindowEnds();
  Check(k.Pop() == c, "its window ended: it goes", c, 2);
  for (int i = 0; i < 4; ++i) k.Push();
  Check(k.Push() == -1 && k.lost == 1, "a full ring: the fifth line is lost and counted",
        static_cast<double>(k.lost), 1);
  k.WindowEnds();
  int went = 0;
  while (k.Pop() >= 0) ++went;
  Check(went == 4 && k.count == 0, "the four lines go, the ring goes round", went, 4);
}

inline void LongFrameChecks() {
  KeptChecks();
  GateChecks();
  CpuAndCallChecks();
  SchedstatChecks();
  LatchChecks();
}

}  // namespace long_frame_checks
