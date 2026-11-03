// The split of a bar into the game and the device, a test (bar_split_math.h).
// main.cpp includes this file after Check, just before main.
#pragma once

#include "../../src/mod/bar_split_math.h"

namespace bar_split_checks {

inline void BarSplitChecks() {
  using namespace bar_split_math;
  namespace rc = render_chain_math;
  printf("\nThe split of a bar into the game and the device\n");

  // A running sum: 3 draws (300 ticks) and 5 render states (50 ticks).
  Acc a;
  a.calls[rc::kDraw] = 3;
  a.ticks[rc::kDraw] = 300;
  a.calls[rc::kRenderState] = 5;
  a.ticks[rc::kRenderState] = 50;
  Acc b = a;
  b.calls[rc::kDraw] += 2;
  b.ticks[rc::kDraw] += 220;
  b.calls[rc::kTexture] += 4;
  b.ticks[rc::kTexture] += 40;
  const Acc d = b.Minus(a);
  Check(d.calls[rc::kDraw] == 2 && d.ticks[rc::kDraw] == 220, "the difference keeps the draws", 2, 2);
  Check(d.Calls() == 6 && d.Ticks() == 260, "the difference has 6 calls and 260 ticks", 6, 6);
  Check(a.Minus(b).Calls() == 0 && a.Minus(b).Ticks() == 0,
        "a difference to a later sum is 0, never below", 0, 0);
  Check(a.StateCalls() == 5, "the state calls are the render states and the stage states", 5, 5);

  // A bar with a bar in it. The outer bar sees 6 calls, the inner bar 4.
  {
    Stack s;
    Acc run;
    s.Push(0, run);
    run.calls[rc::kDraw] += 1;
    run.ticks[rc::kDraw] += 10;  // in the outer bar only
    s.Push(1, run);
    run.calls[rc::kDraw] += 3;
    run.ticks[rc::kDraw] += 90;  // in the inner bar
    const Acc inner = s.Pop(1, run, true);
    run.calls[rc::kTexture] += 2;
    run.ticks[rc::kTexture] += 20;  // in the outer bar only
    const Acc outer = s.Pop(0, run, true);
    Check(inner.Calls() == 3 && inner.Ticks() == 90, "the inner bar has its own device calls", 3, 3);
    Check(outer.Calls() == 3 && outer.Ticks() == 30,
          "the outer bar has the device calls that the inner bar does not have", 3, 3);
    Check(inner.Ticks() + outer.Ticks() == run.Ticks(),
          "the exclusive parts add up to the whole sum", static_cast<double>(run.Ticks()),
          static_cast<double>(inner.Ticks() + outer.Ticks()));
  }

  // A bar that the timeline does not keep leaves its device part in the bar
  // that called it. Its own kept bar in it is still taken out.
  {
    Stack s;
    Acc run;
    s.Push(0, run);
    s.Push(1, run);  // short bar, not kept
    run.ticks[rc::kDraw] += 40;
    run.calls[rc::kDraw] += 1;
    s.Push(2, run);  // kept bar in it
    run.ticks[rc::kDraw] += 100;
    run.calls[rc::kDraw] += 2;
    const Acc deep = s.Pop(2, run, true);
    run.ticks[rc::kDraw] += 5;
    run.calls[rc::kDraw] += 1;
    const Acc shorter = s.Pop(1, run, false);
    const Acc outer = s.Pop(0, run, true);
    Check(deep.Ticks() == 100, "the kept bar has its 100 ticks", 100, static_cast<double>(deep.Ticks()));
    Check(shorter.Ticks() == 45, "the short bar has its own 45 ticks", 45,
          static_cast<double>(shorter.Ticks()));
    Check(outer.Ticks() == 45 && outer.Calls() == 2,
          "the bar that called the short bar gets the 45 ticks of it", 45,
          static_cast<double>(outer.Ticks()));
    Check(deep.Ticks() + outer.Ticks() == run.Ticks(),
          "the parts of the kept bars add up to the whole sum", static_cast<double>(run.Ticks()),
          static_cast<double>(deep.Ticks() + outer.Ticks()));
  }

  // A depth outside the stack gives nothing and does not write.
  {
    Stack s;
    Acc run;
    run.ticks[0] = 5;
    s.Push(-1, run);
    s.Push(walk_parts_math::kMaxDepth, run);
    Check(s.Pop(-1, run, true).Ticks() == 0 && s.Pop(walk_parts_math::kMaxDepth, run, true).Ticks() == 0,
          "a depth outside the stack gives an empty part", 0, 0);
  }

  // The rate of the clock of the CPU.
  {
    ClockRate r;
    r.Seed(0.001);
    Check(r.Us(1000) == 1.0, "the seed gives 1 us for 1000 ticks", 1.0, r.Us(1000));
    // A counter of 10 MHz, a clock of the CPU of 3 GHz.
    r.Read(1000, 100, 10000000, 200000.0);
    Check(r.us_per_tick == 0.001, "the first read only sets the base", 0.001, r.us_per_tick);
    r.Read(1000 + 3000000, 100 + 10000, 10000000, 200000.0);
    Check(r.us_per_tick == 0.001, "a base of 1 ms is below the minimum, so the seed stays", 0.001,
          r.us_per_tick);
    // 1 s later: 3e9 ticks, 1e7 counts.
    r.Read(1000 + 3000000000ull, 100 + 10000000, 10000000, 200000.0);
    Check(r.us_per_tick > 0.000333 && r.us_per_tick < 0.000334,
          "after 1 s the rate is 1/3000 us a tick", 1.0 / 3000.0, r.us_per_tick);
    ClockRate z;
    z.Seed(0.5);
    z.Read(0, 0, 0, 1.0);
    z.Read(10, 10, 0, 1.0);
    Check(z.us_per_tick == 0.5, "a counter with no rate keeps the seed", 0.5, z.us_per_tick);
    ClockRate back;
    back.Seed(0.5);
    back.Read(1000, 100, 1000000, 1.0);
    back.Read(900, 500, 1000000, 1.0);
    Check(back.us_per_tick == 0.5, "a clock of the CPU that goes back keeps the rate", 0.5,
          back.us_per_tick);
  }

  Check(!LeaveOut(200), "200 calls in a frame stay in", 0, 0);
  Check(LeaveOut(201), "201 calls in a frame are a helper", 1, 1);
}

}  // namespace bar_split_checks
