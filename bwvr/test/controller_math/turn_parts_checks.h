// The parts of the game turn, a test: the outer call, the sums of a turn, of
// a frame and of a window, the order and the lines (src/mod/turn_parts_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <stdint.h>
#include <string.h>

#include "../../src/mod/turn_parts_math.h"

namespace turn_parts_checks {

inline bool Has(const char* text, const char* part) { return strstr(text, part) != nullptr; }

inline void TurnPartsChecks() {
  namespace tp = turn_parts_math;
  namespace wp = walk_parts_math;
  printf("\nThe parts of the game turn (turn_parts)\n");

  // THE OUTER CALL.
  Check(tp::Timed(true, true, 0, 0), "a call in the turn on the frame thread counts", 1, 1);
  Check(!tp::Timed(false, true, 0, 0), "a call outside the turn does not count", 0, 0);
  Check(!tp::Timed(true, false, 0, 0), "a call on another thread does not count", 0, 0);
  Check(!tp::Timed(true, true, 1, 0), "a call of a part inside the same part does not count", 0,
        0);
  Check(!tp::Timed(true, true, 0, wp::kMaxDepth), "a call with a full stack does not count", 0,
        0);
  Check(tp::Timed(true, true, 0, wp::kMaxDepth - 1), "a call with room for one more counts", 1, 1);

  // A TURN OF 10 ms at 1000 ticks for each ms: timer 1 (the phase) from 1 to
  // 9 ms with timer 2 inside it from 2 to 5 ms and a call of timer 2 inside
  // that one, and timer 3 from 6 to 8 ms inside timer 1.
  static wp::Stack s;
  s = wp::Stack();
  s.Push(0, 0, 0, 0);
  s.Push(1, 1000, 0x401000, 0x1000);
  s.Push(2, 2000, 0x402000, 0x0F00);
  // The inner call of timer 2: the rule says no, so the game runs on without
  // a push, and the stack has one call of timer 2.
  Check(!tp::Timed(true, true, s.nest[2], s.depth), "the inner call of timer 2 is not pushed",
        s.nest[2], 1);
  s.Return(5000, 0x0F04);
  s.Push(3, 6000, 0x403000, 0x0F00);
  s.Return(8000, 0x0F04);
  s.Return(9000, 0x1004);
  s.CloseTop(10000);
  Check(s.depth == 0, "the turn closes", s.depth, 0);

  static tp::Turn t;
  t.FromStack(s, 4, 0.001);
  Near("the turn is 10 ms", t.total_ms, 10.0, 1e-9);
  Near("timer 1: 8 ms inclusive, 3 ms exclusive", t.excl_ms[1], 3.0, 1e-9);
  Near("timer 1: inclusive", t.incl_ms[1], 8.0, 1e-9);
  Near("timer 2: 3 ms", t.excl_ms[2], 3.0, 1e-9);
  Near("timer 3: 2 ms", t.excl_ms[3], 2.0, 1e-9);
  Near("the rest: 2 ms", t.excl_ms[0], 2.0, 1e-9);
  Near("the parts and the rest add up to the turn", t.ExclSum(4), t.total_ms, 1e-9);
  Check(t.calls[2] == 1 && t.calls[1] == 1 && t.calls[0] == 1, "one call of each part", t.calls[2],
        1);
  Near("the rest is 20 percent of the turn", tp::Percent(t.excl_ms[0], t.total_ms), 20.0, 1e-9);
  Near("the percent of nothing is 0", tp::Percent(1.0, 0.0), 0.0, 0.0);

  // THE FRAME: two turns in a frame add up, a new frame has none.
  static tp::Frame f;
  f = tp::Frame();
  f.AddTurn(t, 4);
  f.AddTurn(t, 4);
  Check(f.turns == 2, "two turns in the frame", f.turns, 2);
  Near("the frame has 20 ms of turn", f.sum.total_ms, 20.0, 1e-9);
  Check(f.sum.calls[3] == 2, "the frame has two calls of timer 3", f.sum.calls[3], 2);
  f.Clear();
  Check(f.turns == 0 && f.sum.calls[3] == 0 && f.sum.total_ms == 0.0, "a new frame has no turn",
        f.turns, 0);

  // THE ORDER: the largest exclusive time first, the rest and the parts with
  // no call left out.
  int ids[tp::kMaxParts];
  uint64_t calls[4] = {1, 1, 1, 0};
  const double excl[4] = {9.0, 3.0, 5.0, 7.0};
  int n = tp::Order(excl, calls, 4, ids);
  Check(n == 2 && ids[0] == 2 && ids[1] == 1, "the order: timer 2 (5 ms), timer 1 (3 ms)", ids[0],
        2);

  // THE NAMES and the lines.
  const char* names[4] = {"outside_parts", "phase", "inner", "late"};
  static char text[2048];
  f.Clear();
  tp::FrameText(text, sizeof(text), f, 4, names);
  Check(Has(text, "No game turn"), "a frame with no turn says so", 1, 1);
  f.AddTurn(t, 4);
  tp::FrameText(text, sizeof(text), f, 4, names);
  Check(Has(text, "10.00 ms") && Has(text, "phase 3.00 (1 call, inclusive 8.00)") &&
            Has(text, "inner 3.00 (1 call)") && Has(text, "outside the parts 2.00 (20 percent)"),
        "the text of a frame: the parts, the calls, the inclusive time of a part that calls others",
        1, 1);
  Check(strstr(text, "inner 3.00 (1 call, inclusive") == nullptr,
        "no inclusive time for a part that calls none", 1, 1);
  Check(strstr(text, "inner") < strstr(text, "late"), "the text has the larger part first", 1, 1);
  // A buffer that is too small gets a cut text and no write past its end.
  static char tiny[40];
  memset(tiny, 'x', sizeof(tiny));
  tp::FrameText(tiny, 30, f, 4, names);
  Check(strlen(tiny) < 30 && tiny[39] == 'x', "a small buffer gets a cut text", 1, 1);

  // THE WINDOW: a short turn counts in the turns but not in the parts. A long
  // turn adds its parts.
  static tp::Window w;
  w = tp::Window();
  static tp::Turn shortt;
  shortt.Clear();
  shortt.total_ms = 0.5;
  shortt.excl_ms[0] = 0.5;
  shortt.calls[0] = 1;
  w.AddTurn(shortt, 4);
  Check(w.turns == 1 && w.long_turns == 0, "a short turn is not a long turn", w.long_turns, 0);
  tp::WindowLine(text, sizeof(text), w, 4, names, 10.0);
  Check(Has(text, "No long turn"), "a window with no long turn says so", 1, 1);
  w.AddTurn(t, 4);
  Check(w.turns == 2 && w.long_turns == 1, "a turn of 10 ms is a long turn", w.long_turns, 1);
  Near("the long turns together: 10 ms", w.long_ms, 10.0, 1e-9);
  Near("the worst turn: 10 ms", w.worst_ms, 10.0, 1e-9);
  Near("the exclusive time of timer 2 in the long turns", w.excl_ms[2], 3.0, 1e-9);
  Near("the rest of the short turn is not in the parts", w.excl_ms[0], 2.0, 1e-9);
  tp::WindowLine(text, sizeof(text), w, 4, names, 10.0);
  Check(Has(text, "2 game turns (mean 5.25 ms, worst 10.00 ms)") &&
            Has(text, "1 of them 1 ms or more (together 10.00 ms)") &&
            Has(text, "phase 3.00 ms (30.0 percent, 1 call, most 3.00, inclusive 8.00)") &&
            Has(text, "outside the parts 2.00 ms (20.0 percent, most 2.00)"),
        "the line of a window: the turns, the parts of the long turns, the rest", 1, 1);
  w.turn_function_calls = 450;
  w.stub_in_turn = 70;
  w.stub_outside = 3;
  w.stub_other_thread = 2;
  tp::WindowLine(text, sizeof(text), w, 4, names, 10.0);
  Check(Has(text, "The turn function ran 450 times. The timers saw 70 calls in a game turn on the "
                  "frame thread, 3 outside a game turn and 2 on another thread."),
        "the line of a window says what the timers saw", 1, 1);
  w.Reset();
  Check(w.turn_function_calls == 0 && w.stub_in_turn == 0, "a new window has no count of calls",
        static_cast<double>(w.stub_in_turn), 0);
  Check(w.turns == 0 && w.long_turns == 0 && w.excl_ms[1] == 0.0, "a new window is empty",
        w.turns, 0);
}

}  // namespace turn_parts_checks
