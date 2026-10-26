// THE PARTS OF THE SCENE WALK (walk_parts, docs/backend.md, "The parts of the
// scene walk"): the stack of the open timers, the inclusive and the exclusive
// time of each timer in a frame, the mean and the most of a window, the order
// of the timers and the lines of the log. No Windows code.
//
// A timer is a function. Its inclusive time is the time from its call to its
// return. Its exclusive time is its inclusive time less the inclusive time of
// each timer that it calls. Timer 0 is the scene walk itself, so its
// exclusive time is the part of the walk that no other timer covers. The sum
// of the exclusive times of all timers is the walk.
//
// The stack works in ticks of the clock and with integers only: the exit of a
// timer runs between the return of a function of the game and its caller, and
// the x87 unit can then hold the value that the function returns.
#pragma once

#include <stdint.h>
#include <stdio.h>

#include "time_log_math.h"

namespace walk_parts_math {

constexpr int kMaxTimers = 192;
constexpr int kMaxDepth = 64;
// The most bytes of arguments that a function takes off the stack at its
// return (ret N). The exit of a timer finds its entry by the place of the
// return address on the stack.
constexpr uint32_t kMaxArgBytes = 256;

// One open call of a timer.
struct Open {
  int id = 0;
  int64_t start = 0;  // the clock at the call
  int64_t child = 0;  // the inclusive time of the timers that it called
  uint32_t ret = 0;   // the return address of the caller
  uint32_t slot = 0;  // the place of the return address on the stack
};

// The stack of the open timers and the sums of the frame that is open.
struct Stack {
  Open open[kMaxDepth];
  int depth = 0;
  int nest[kMaxTimers] = {};  // the open calls of each timer
  uint32_t calls[kMaxTimers] = {};
  int64_t incl[kMaxTimers] = {};
  int64_t excl[kMaxTimers] = {};
  uint64_t lost = 0;  // open calls that no return closed (a fault, a long jump)

  // A call of timer `id` at the clock `now`. False when the stack is full:
  // then the call is not timed.
  bool Push(int id, int64_t now, uint32_t ret, uint32_t slot) {
    if (depth >= kMaxDepth || id < 0 || id >= kMaxTimers) return false;
    Open& o = open[depth++];
    o.id = id;
    o.start = now;
    o.child = 0;
    o.ret = ret;
    o.slot = slot;
    ++nest[id];
    ++calls[id];
    return true;
  }

  // The open call closes at the clock `now`. The inclusive time of a timer
  // counts only for its outermost open call, so a call of a timer inside
  // itself does not count its time twice.
  void CloseTop(int64_t now) {
    Open& o = open[--depth];
    int64_t in = now - o.start;
    if (in < 0) in = 0;
    int64_t ex = in - o.child;
    if (ex < 0) ex = 0;
    excl[o.id] += ex;
    if (--nest[o.id] == 0) incl[o.id] += in;
    if (depth > 0) open[depth - 1].child += in;
  }

  // An open call that no return closed: it goes away with no time, and its
  // time stays in the call that called it.
  void DropTop() {
    Open& o = open[--depth];
    --nest[o.id];
    ++lost;
  }

  // The index of the open call that a return at the stack place `esp`
  // closes: the topmost call with its return address at `esp` - 4 - N, for N
  // from 0 to kMaxArgBytes. When no call fits, the topmost call of a
  // function. A call with the slot 0 (the scene walk) has no return address
  // and never fits. -1 when no call fits.
  int Match(uint32_t esp) const {
    for (int i = depth - 1; i >= 0; --i) {
      if (open[i].slot == 0) continue;
      const uint32_t d = esp - open[i].slot;
      if (esp >= open[i].slot + 4 && d <= 4 + kMaxArgBytes) return i;
    }
    for (int i = depth - 1; i >= 0; --i) {
      if (open[i].slot != 0) return i;
    }
    return -1;
  }

  // A return at the stack place `esp` and the clock `now`. The open calls
  // above the one that it closes are lost. Gives the return address of the
  // caller, or 0 when no open call of a function is on the stack.
  uint32_t Return(int64_t now, uint32_t esp) {
    const int i = Match(esp);
    if (i < 0) return 0;
    while (depth - 1 > i) DropTop();
    const uint32_t ret = open[i].ret;
    CloseTop(now);
    return ret;
  }

  // The sums of the frame go to zero. The open calls stay.
  void NewFrame() {
    for (int i = 0; i < kMaxTimers; ++i) {
      calls[i] = 0;
      incl[i] = 0;
      excl[i] = 0;
    }
  }
};

// The mean and the most of each timer in the frames of a window.
struct Window {
  uint64_t frames = 0;
  double ms_per_tick = 0.0;
  time_log_math::Stat calls[kMaxTimers];
  time_log_math::Stat incl[kMaxTimers];
  time_log_math::Stat excl[kMaxTimers];
  time_log_math::Stat pairs;  // the timed calls of a frame, timer 0 not counted

  // The frame of `s` ends: its sums go into the window.
  void AddFrame(const Stack& s, int timers) {
    ++frames;
    uint32_t n = 0;
    for (int i = 0; i < timers && i < kMaxTimers; ++i) {
      calls[i].Add(static_cast<double>(s.calls[i]));
      incl[i].Add(static_cast<double>(s.incl[i]) * ms_per_tick);
      excl[i].Add(static_cast<double>(s.excl[i]) * ms_per_tick);
      if (i > 0) n += s.calls[i];
    }
    pairs.Add(static_cast<double>(n));
  }

  void Reset() {
    const double m = ms_per_tick;
    *this = Window();
    ms_per_tick = m;
  }

  // The mean exclusive ms in a frame of all timers but timer 0.
  double TimedMs(int timers) const {
    double sum = 0.0;
    for (int i = 1; i < timers && i < kMaxTimers; ++i) sum += excl[i].Mean();
    return sum;
  }
};

// The timers with calls in the window, timer 0 not counted, in the order of
// their mean exclusive time, the largest first. Gives the count in `ids`.
inline int Order(const Window& w, int timers, int* ids) {
  int n = 0;
  for (int i = 1; i < timers && i < kMaxTimers; ++i) {
    if (w.calls[i].sum > 0.0) ids[n++] = i;
  }
  for (int a = 1; a < n; ++a) {
    const int id = ids[a];
    int b = a;
    while (b > 0 && w.excl[ids[b - 1]].Mean() < w.excl[id].Mean()) {
      ids[b] = ids[b - 1];
      --b;
    }
    ids[b] = id;
  }
  return n;
}

// The percent of `part` in `whole`, 0 when `whole` is 0.
inline double Percent(double part, double whole) {
  return whole > 0.0 ? 100.0 * part / whole : 0.0;
}

// The first line of a window: the walk, the share of the timers and the cost
// of the timers. `pair_ns` is the cost of one timed call, from the measure at
// the start.
inline int HeadLine(char* out, size_t size, const Window& w, int timers, double seconds,
                    double pair_ns) {
  const double walk = w.incl[0].Mean();
  const double timed = w.TimedMs(timers);
  const double outside = w.excl[0].Mean();
  const double cost = w.pairs.Mean() * pair_ns / 1.0e6;
  return snprintf(out, size,
                  "%.1f s, %llu frames with a scene walk. The walk in a frame, ms, the mean "
                  "and the most: %.2f and %.2f. The timed functions %.2f ms (%.1f percent of "
                  "the walk), outside each timer %.2f ms (%.1f percent). %.0f timed calls in "
                  "a frame (most %.0f), at %.0f ns for each call about %.2f ms (%.1f percent "
                  "of the walk)",
                  seconds, static_cast<unsigned long long>(w.frames), walk, w.incl[0].most, timed,
                  Percent(timed, walk), outside, Percent(outside, walk), w.pairs.Mean(),
                  w.pairs.most, pair_ns, cost, Percent(cost, walk));
}

// The line of one timer: its rank, its address, its name, its calls in a
// frame, and its exclusive and inclusive ms in a frame, the mean and the most.
inline int TimerLine(char* out, size_t size, const Window& w, int id, int rank,
                     uint32_t address, const char* name) {
  return snprintf(out, size,
                  "%2d. 0x%08X %s: calls in a frame %.1f (most %.0f), exclusive ms %.3f and "
                  "%.3f, inclusive ms %.3f and %.3f",
                  rank, address, name, w.calls[id].Mean(), w.calls[id].most, w.excl[id].Mean(),
                  w.excl[id].most, w.incl[id].Mean(), w.incl[id].most);
}

}  // namespace walk_parts_math
