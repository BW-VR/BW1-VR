// THE SPLIT OF A BAR INTO THE GAME AND THE DEVICE (trace_device, bar_split.cpp,
// docs/backend.md, "The timeline of the game and the app"). The pure rules, with
// no Windows code, so the math test checks them.
//
// The device of the mod (vk_ddraw.cpp) adds each call of Direct3D 7 to a
// running sum that never goes back (Acc): the calls and the ticks of the clock
// of the CPU for each kind of call. A timer of turn_parts keeps the sum at its
// call, and at its return the difference is the device time inside the bar.
// The bar then keeps the part of that time that no bar in it covers (the
// exclusive part), so the own time of a bar, which is its length less the
// bars in it, splits into the game (the rest) and the device (this part).
//
// A bar that the timeline does not keep (shorter than trace_min_us) is not in
// the file, so its device part stays in the bar that called it.
#pragma once

#include <stdint.h>

#include "render_chain_math.h"
#include "walk_parts_math.h"

namespace bar_split_math {

namespace rc = render_chain_math;

// A timer that is called more than this many times in a frame is a helper, and
// the timeline leaves it out from then on.
constexpr uint32_t kCallsCap = 200;

// The calls of the device and their ticks, for each kind (render_chain_math
// Call: the draws, the render states, the stage states, the textures and the
// other calls).
struct Acc {
  uint64_t ticks[rc::kCalls] = {};
  uint64_t calls[rc::kCalls] = {};

  // The difference to an earlier sum. A part never goes below 0.
  Acc Minus(const Acc& earlier) const {
    Acc r;
    for (int i = 0; i < rc::kCalls; ++i) {
      r.ticks[i] = ticks[i] > earlier.ticks[i] ? ticks[i] - earlier.ticks[i] : 0;
      r.calls[i] = calls[i] > earlier.calls[i] ? calls[i] - earlier.calls[i] : 0;
    }
    return r;
  }
  void Add(const Acc& other) {
    for (int i = 0; i < rc::kCalls; ++i) {
      ticks[i] += other.ticks[i];
      calls[i] += other.calls[i];
    }
  }
  uint64_t Ticks() const {
    uint64_t t = 0;
    for (int i = 0; i < rc::kCalls; ++i) t += ticks[i];
    return t;
  }
  uint64_t Calls() const {
    uint64_t c = 0;
    for (int i = 0; i < rc::kCalls; ++i) c += calls[i];
    return c;
  }
  // The calls that set a state of the device: the render states and the stage
  // states.
  uint64_t StateCalls() const { return calls[rc::kRenderState] + calls[rc::kStageState]; }
};

// The open bars: for each depth of the stack of the timers, the sum at the
// call and the part that the bars in it took.
struct Stack {
  Acc at_call[walk_parts_math::kMaxDepth];
  Acc taken[walk_parts_math::kMaxDepth];

  // The bar at `depth` starts. `now` is the running sum.
  void Push(int depth, const Acc& now) {
    if (depth < 0 || depth >= walk_parts_math::kMaxDepth) return;
    at_call[depth] = now;
    taken[depth] = Acc();
  }

  // The bar at `depth` ends. `recorded`: the timeline keeps the bar. Gives the
  // exclusive part: the device calls in the bar that no kept bar in it has.
  // The bar below takes the whole bar when it is kept, else only what the kept
  // bars in it took, so a short bar leaves its part in the bar that called it.
  Acc Pop(int depth, const Acc& now, bool recorded) {
    if (depth < 0 || depth >= walk_parts_math::kMaxDepth) return Acc();
    const Acc inclusive = now.Minus(at_call[depth]);
    const Acc exclusive = inclusive.Minus(taken[depth]);
    if (depth > 0) taken[depth - 1].Add(recorded ? inclusive : taken[depth]);
    return exclusive;
  }
};

// THE RATE OF THE CLOCK OF THE CPU in us a tick, from its ticks and the
// performance counter between a first read and now. The seed (a short spin at
// the install) holds until the two reads are `min_us` apart.
struct ClockRate {
  double us_per_tick = 0.0;
  bool have_base = false;
  uint64_t tsc0 = 0;
  int64_t qpc0 = 0;

  void Seed(double us) { us_per_tick = us; }
  // One read of both clocks. qpc_hz: the ticks a second of the performance counter.
  void Read(uint64_t tsc, int64_t qpc, int64_t qpc_hz, double min_us) {
    if (qpc_hz <= 0) return;
    if (!have_base) {
      have_base = true;
      tsc0 = tsc;
      qpc0 = qpc;
      return;
    }
    if (tsc <= tsc0 || qpc <= qpc0) return;
    const double us = static_cast<double>(qpc - qpc0) * 1e6 / static_cast<double>(qpc_hz);
    if (us >= min_us) us_per_tick = us / static_cast<double>(tsc - tsc0);
  }
  double Us(uint64_t ticks) const { return static_cast<double>(ticks) * us_per_tick; }
};

// The calls of a timer in a frame: true when the timeline leaves the timer out
// from now on.
inline bool LeaveOut(uint32_t calls_in_frame) { return calls_in_frame > kCallsCap; }

}  // namespace bar_split_math
