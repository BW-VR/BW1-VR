// The parts of the scene walk, a test: the stack of the open timers, the
// inclusive and the exclusive time, the window, the order and the lines
// (src/mod/walk_parts_math.h). main.cpp includes this file after Check and
// Near, just before main.
#pragma once

#include <stdint.h>
#include <string.h>

#include "../../src/mod/walk_parts_math.h"

namespace walk_parts_checks {

inline void WalkPartsChecks() {
  namespace wp = walk_parts_math;
  printf("\nThe parts of the scene walk (walk_parts)\n");

  // A walk of 100 ticks: timer 1 from 10 to 60 with timer 2 inside it from
  // 20 to 50, and timer 3 from 70 to 90. The return addresses are on a stack
  // that grows down: the slot of an inner call is below the slot of its
  // caller.
  static wp::Stack s;
  s = wp::Stack();
  bool pushed = s.Push(0, 0, 0, 0);
  Check(pushed && s.depth == 1, "the walk opens", s.depth, 1);
  pushed = s.Push(1, 10, 0x401000, 0x1000);
  Check(pushed && s.depth == 2, "timer 1 opens", s.depth, 2);
  pushed = s.Push(2, 20, 0x402000, 0x0F00);
  Check(pushed && s.depth == 3, "timer 2 opens inside timer 1", s.depth, 3);
  // Timer 2 is __stdcall with 8 bytes of arguments: the stack place after
  // its return is its slot + 4 + 8.
  uint32_t ret = s.Return(50, 0x0F00 + 12);
  Check(ret == 0x402000 && s.depth == 2, "the return of timer 2 gives its return address",
        static_cast<double>(ret), 0x402000);
  ret = s.Return(60, 0x1000 + 4);
  Check(ret == 0x401000 && s.depth == 1, "the return of timer 1 gives its return address",
        static_cast<double>(ret), 0x401000);
  s.Push(3, 70, 0x403000, 0x1000);
  s.Return(90, 0x1004);
  s.CloseTop(100);
  Check(s.depth == 0, "the walk closes", s.depth, 0);
  Check(s.incl[1] == 50 && s.excl[1] == 20, "timer 1: inclusive 50, exclusive 50 - 30",
        static_cast<double>(s.excl[1]), 20);
  Check(s.incl[2] == 30 && s.excl[2] == 30, "timer 2: inclusive and exclusive 30",
        static_cast<double>(s.excl[2]), 30);
  Check(s.incl[0] == 100 && s.excl[0] == 30, "the walk: 100, and 30 outside each timer",
        static_cast<double>(s.excl[0]), 30);
  int64_t sum = 0;
  for (int i = 0; i < 4; ++i) sum += s.excl[i];
  Check(sum == 100, "the sum of the exclusive times is the walk", static_cast<double>(sum), 100);
  Check(s.calls[1] == 1 && s.calls[2] == 1 && s.calls[3] == 1, "one call of each timer",
        s.calls[3], 1);

  // A timer inside itself: its inclusive time counts one time, its
  // exclusive time is each part outside the inner call.
  s = wp::Stack();
  s.Push(0, 0, 0, 0);
  s.Push(5, 0, 0x405000, 0x1000);
  s.Push(5, 10, 0x405100, 0x0F00);
  s.Return(30, 0x0F04);
  s.Return(40, 0x1004);
  s.CloseTop(40);
  Check(s.incl[5] == 40 && s.excl[5] == 40 && s.calls[5] == 2,
        "a timer inside itself: inclusive 40 (not 60), exclusive 40, two calls",
        static_cast<double>(s.incl[5]), 40);

  // A call that no return closed (a long jump): the return of its caller
  // finds the caller by its slot, and the lost call has no time.
  s = wp::Stack();
  s.Push(0, 0, 0, 0);
  s.Push(1, 0, 0x401000, 0x1000);
  s.Push(2, 5, 0x402000, 0x0800);
  ret = s.Return(20, 0x1004);
  Check(ret == 0x401000 && s.depth == 1 && s.lost == 1,
        "a lost call: the return of its caller closes the caller", static_cast<double>(s.lost),
        1);
  Check(s.incl[2] == 0 && s.excl[1] == 20, "the lost call has no time, its caller has 20",
        static_cast<double>(s.excl[1]), 20);
  Check(s.nest[2] == 0, "the lost call is not open", s.nest[2], 0);
  // A return with no open call of a function gives 0. The walk itself has
  // no return address.
  ret = s.Return(30, 0x2000);
  Check(ret == 0 && s.depth == 1, "a return with only the walk open gives 0",
        static_cast<double>(ret), 0);
  // A full stack does not time the call.
  s = wp::Stack();
  bool all = true;
  for (int i = 0; i < wp::kMaxDepth; ++i) all = all && s.Push(1, i, 1, 0x1000 - 16 * i);
  Check(all && !s.Push(1, 99, 1, 0x100), "the stack takes kMaxDepth calls and no more", s.depth,
        wp::kMaxDepth);

  // The window: two frames, ms_per_tick 0.001 (a clock of 1 MHz).
  static wp::Window w;
  w = wp::Window();
  w.ms_per_tick = 0.001;
  s = wp::Stack();
  s.calls[0] = 1;
  s.incl[0] = 10000;
  s.excl[0] = 1000;
  s.calls[1] = 2;
  s.incl[1] = 6000;
  s.excl[1] = 6000;
  s.calls[2] = 10;
  s.incl[2] = 3000;
  s.excl[2] = 3000;
  w.AddFrame(s, 3);
  s.NewFrame();
  Check(s.calls[1] == 0 && s.incl[1] == 0, "a new frame has no sums", s.calls[1], 0);
  s.calls[0] = 1;
  s.incl[0] = 14000;
  s.excl[0] = 1000;
  s.calls[2] = 20;
  s.incl[2] = 13000;
  s.excl[2] = 13000;
  w.AddFrame(s, 3);
  Check(w.frames == 2, "two frames", static_cast<double>(w.frames), 2);
  Near("the walk: a mean of 12 ms", w.incl[0].Mean(), 12.0, 1e-9);
  Near("the walk: the most 14 ms", w.incl[0].most, 14.0, 1e-9);
  Near("timer 1: a mean of 3 ms (6 and 0)", w.excl[1].Mean(), 3.0, 1e-9);
  Near("timer 2: a mean of 8 ms", w.excl[2].Mean(), 8.0, 1e-9);
  Near("the timed functions: 11 ms of 12", w.TimedMs(3), 11.0, 1e-9);
  Near("the timed calls: a mean of 16 in a frame", w.pairs.Mean(), 16.0, 1e-9);
  int ids[wp::kMaxTimers];
  const int n = wp::Order(w, 3, ids);
  Check(n == 2 && ids[0] == 2 && ids[1] == 1, "the order: timer 2 (8 ms), then timer 1 (3 ms)",
        ids[0], 2);
  char line[512];
  wp::HeadLine(line, sizeof(line), w, 3, 10.0, 100.0);
  Check(strstr(line, "2 frames with a scene walk") != nullptr &&
            strstr(line, "12.00 and 14.00") != nullptr &&
            strstr(line, "11.00 ms (91.7 percent") != nullptr &&
            strstr(line, "outside each timer 1.00 ms (8.3 percent)") != nullptr &&
            strstr(line, "16 timed calls in a frame (most 20), at 100 ns for each call about "
                         "0.00 ms") != nullptr,
        "the head line has the walk, the share of the timers and their cost", 0, 0);
  wp::TimerLine(line, sizeof(line), w, 2, 1, 0x007FF610, "the land");
  Check(strcmp(line, " 1. 0x007FF610 the land: calls in a frame 15.0 (most 20), exclusive ms "
                     "8.000 and 13.000, inclusive ms 8.000 and 13.000") == 0,
        "the line of a timer", 0, 0);
  w.Reset();
  Check(w.frames == 0 && w.ms_per_tick == 0.001, "a reset keeps the tick", 0, 0);
}

}  // namespace walk_parts_checks
