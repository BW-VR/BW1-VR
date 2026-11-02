// THE PARTS OF THE GAME TURN (turn_parts, docs/backend.md, "The long game
// turn"): which call of a timed function counts, the sums of one game turn by
// part, the sums of a frame, the window of 10 s and the lines of the log. No
// Windows code, so the math test of the PC checks it
// (test/controller_math/turn_parts_checks.h).
//
// A part is a function of the game. The stack of the open timers is
// walk_parts_math::Stack. Timer 0 is the game turn itself (the call of
// GGame::ProcessNetworkPackets that runs a turn), so its exclusive time is the
// part of the turn that no other timer covers: the rest. The exclusive time of
// a part is its time from its call to its return less the time of each part
// that it calls. So the exclusive times of all parts and the rest add up to the
// turn. The inclusive time of a part is the whole time from its call to its
// return.
//
// The stack works in ticks of the clock and with integers only: the exit of a
// timer runs between the return of a function of the game and its caller, and
// the x87 unit can then hold the value that the function returns.
#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "walk_parts_math.h"

namespace turn_parts_math {

// The timers, the rest (id 0) included.
constexpr int kMaxParts = 96;
// A turn of this many ms or more is a long turn.
constexpr double kLongTurnMs = 1.0;
// A line lists a part only when its exclusive or its inclusive time is this
// many ms or more. The other parts are only counted.
constexpr double kShownMs = 0.01;

// THE OUTER CALL. A call of a part counts when it comes on the frame thread in
// a game turn, when the stack has room, and when no call of the same part is
// open (open_calls). So a part that calls itself counts once, at the outer
// call, and its time is the time of the outer call.
inline bool Timed(bool in_turn, bool frame_thread, int open_calls, int depth) {
  return in_turn && frame_thread && open_calls == 0 && depth < walk_parts_math::kMaxDepth;
}

// The sums of one game turn, or of the game turns of one frame: the time in
// ms, and the calls.
struct Turn {
  double total_ms = 0.0;
  double excl_ms[kMaxParts] = {};
  double incl_ms[kMaxParts] = {};
  uint32_t calls[kMaxParts] = {};

  void Clear() { *this = Turn(); }

  // The turn that the stack just closed (timer 0 is closed, so its inclusive
  // time is the turn). `parts` timers are in use.
  void FromStack(const walk_parts_math::Stack& s, int parts, double ms_per_tick) {
    Clear();
    total_ms = static_cast<double>(s.incl[0]) * ms_per_tick;
    for (int i = 0; i < parts && i < kMaxParts; ++i) {
      excl_ms[i] = static_cast<double>(s.excl[i]) * ms_per_tick;
      incl_ms[i] = static_cast<double>(s.incl[i]) * ms_per_tick;
      calls[i] = s.calls[i];
    }
  }

  // The times of another turn go into this one.
  void Add(const Turn& o, int parts) {
    total_ms += o.total_ms;
    for (int i = 0; i < parts && i < kMaxParts; ++i) {
      excl_ms[i] += o.excl_ms[i];
      incl_ms[i] += o.incl_ms[i];
      calls[i] += o.calls[i];
    }
  }

  // The sum of the exclusive times of the parts and of the rest. It is the
  // turn, apart from the rounding of the ticks.
  double ExclSum(int parts) const {
    double sum = 0.0;
    for (int i = 0; i < parts && i < kMaxParts; ++i) sum += excl_ms[i];
    return sum;
  }
};

// THE TURNS OF A FRAME. A frame is the time from one present to the next. The
// turn latches into the frame when it ends, and the present takes the sums of
// the frame and clears them, so a frame without a turn has none. A frame has
// at most one game turn in a game for one player.
struct Frame {
  Turn sum;
  uint32_t turns = 0;

  void AddTurn(const Turn& t, int parts) {
    sum.Add(t, parts);
    ++turns;
  }
  void Clear() {
    sum.Clear();
    turns = 0;
  }
};

// The turns of a window of 10 s. The parts are the sums of the long turns
// only: the turns of kLongTurnMs or more.
struct Window {
  uint64_t turns = 0;
  double turn_ms = 0.0;
  double worst_ms = 0.0;
  uint64_t long_turns = 0;
  double long_ms = 0.0;
  double excl_ms[kMaxParts] = {};
  double incl_ms[kMaxParts] = {};
  double most_ms[kMaxParts] = {};  // the most exclusive ms in one long turn
  uint64_t calls[kMaxParts] = {};
  // What the timers saw in the window, for the case of a window with no turn:
  // the calls of the function of the turn (the game asks if a turn is due,
  // so most calls run no turn), and the calls of the stubs in a game turn on
  // the frame thread, outside a game turn, and on another thread.
  uint64_t turn_function_calls = 0;
  uint64_t stub_in_turn = 0;
  uint64_t stub_outside = 0;
  uint64_t stub_other_thread = 0;

  void Reset() { *this = Window(); }

  void AddTurn(const Turn& t, int parts) {
    ++turns;
    turn_ms += t.total_ms;
    if (t.total_ms > worst_ms) worst_ms = t.total_ms;
    if (t.total_ms < kLongTurnMs) return;
    ++long_turns;
    long_ms += t.total_ms;
    for (int i = 0; i < parts && i < kMaxParts; ++i) {
      excl_ms[i] += t.excl_ms[i];
      incl_ms[i] += t.incl_ms[i];
      calls[i] += t.calls[i];
      if (t.excl_ms[i] > most_ms[i]) most_ms[i] = t.excl_ms[i];
    }
  }
};

// The parts in the order of their exclusive ms, the largest first, the rest
// (id 0) left out. Only the parts with `calls` above 0 come. Gives the count.
inline int Order(const double* excl_ms, const uint64_t* calls, int parts, int* ids) {
  int n = 0;
  for (int i = 1; i < parts && i < kMaxParts; ++i) {
    if (calls[i] > 0) ids[n++] = i;
  }
  for (int a = 1; a < n; ++a) {
    const int id = ids[a];
    int b = a;
    while (b > 0 && excl_ms[ids[b - 1]] < excl_ms[id]) {
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

// Appends a formatted text to out[*used]. Never writes past `size`.
inline void Append(char* out, size_t size, size_t* used, const char* format, ...) {
  if (*used >= size) return;
  va_list args;
  va_start(args, format);
  const int n = vsnprintf(out + *used, size - *used, format, args);
  va_end(args);
  if (n < 0) return;
  *used += static_cast<size_t>(n);
  if (*used >= size) *used = size - 1;
}

// The text of the turns of one frame for the line of a long frame: the
// exclusive ms and the calls of each part that ran, then the rest.
inline int FrameText(char* out, size_t size, const Frame& f, int parts,
                     const char* const* names) {
  size_t used = 0;
  if (size == 0) return 0;
  out[0] = 0;
  if (f.turns == 0) {
    Append(out, size, &used, "No game turn in this frame");
    return static_cast<int>(used);
  }
  uint64_t calls[kMaxParts] = {};
  for (int i = 0; i < parts && i < kMaxParts; ++i) calls[i] = f.sum.calls[i];
  int ids[kMaxParts];
  const int n = Order(f.sum.excl_ms, calls, parts, ids);
  Append(out, size, &used, "The game turn took %.2f ms. Its parts in ms, exclusive, with the calls",
         f.sum.total_ms);
  Append(out, size, &used, " (inclusive when it differs):");
  int shown = 0;
  for (int k = 0; k < n; ++k) {
    const int i = ids[k];
    if (f.sum.excl_ms[i] < kShownMs && f.sum.incl_ms[i] < kShownMs) continue;
    Append(out, size, &used, "%s %s %.2f (%u", shown++ == 0 ? "" : ",", names[i],
           f.sum.excl_ms[i], f.sum.calls[i]);
    Append(out, size, &used, "%s", f.sum.calls[i] == 1 ? " call" : " calls");
    if (f.sum.incl_ms[i] > f.sum.excl_ms[i] + 0.005) {
      Append(out, size, &used, ", inclusive %.2f", f.sum.incl_ms[i]);
    }
    Append(out, size, &used, ")");
  }
  if (n > shown) {
    Append(out, size, &used, "%s %d more parts under %.2f ms", shown > 0 ? "," : "", n - shown,
           kShownMs);
  }
  Append(out, size, &used, "%s outside the parts %.2f (%.0f percent)", n > 0 ? "," : "",
         f.sum.excl_ms[0], Percent(f.sum.excl_ms[0], f.sum.total_ms));
  return static_cast<int>(used);
}

// The line of a window of 10 s: the turns, and the parts of the long turns.
inline int WindowLine(char* out, size_t size, const Window& w, int parts,
                      const char* const* names, double seconds) {
  size_t used = 0;
  if (size == 0) return 0;
  out[0] = 0;
  Append(out, size, &used,
         "%.1f s, %llu game turns (mean %.2f ms, worst %.2f ms), %llu of them %.0f ms or more "
         "(together %.2f ms).",
         seconds, static_cast<unsigned long long>(w.turns),
         w.turns > 0 ? w.turn_ms / static_cast<double>(w.turns) : 0.0, w.worst_ms,
         static_cast<unsigned long long>(w.long_turns), kLongTurnMs, w.long_ms);
  Append(out, size, &used,
         " The turn function ran %llu times. The timers saw %llu calls in a game turn on the "
         "frame thread, %llu outside a game turn and %llu on another thread.",
         static_cast<unsigned long long>(w.turn_function_calls),
         static_cast<unsigned long long>(w.stub_in_turn),
         static_cast<unsigned long long>(w.stub_outside),
         static_cast<unsigned long long>(w.stub_other_thread));
  if (w.long_turns == 0) {
    Append(out, size, &used, " No long turn, so no part to show");
    return static_cast<int>(used);
  }
  int ids[kMaxParts];
  const int n = Order(w.excl_ms, w.calls, parts, ids);
  Append(out, size, &used,
         " The parts of these turns, exclusive ms in all of them, the share, the calls, the most "
         "in one turn and the inclusive ms:");
  int shown = 0;
  for (int k = 0; k < n; ++k) {
    const int i = ids[k];
    if (w.excl_ms[i] < kShownMs && w.incl_ms[i] < kShownMs) continue;
    Append(out, size, &used, "%s %s %.2f ms (%.1f percent, %llu %s, most %.2f, inclusive %.2f)",
           shown++ == 0 ? "" : ",", names[i], w.excl_ms[i], Percent(w.excl_ms[i], w.long_ms),
           static_cast<unsigned long long>(w.calls[i]), w.calls[i] == 1 ? "call" : "calls",
           w.most_ms[i], w.incl_ms[i]);
  }
  if (n > shown) {
    Append(out, size, &used, "%s %d more parts under %.2f ms", shown > 0 ? "," : "", n - shown,
           kShownMs);
  }
  Append(out, size, &used, "%s outside the parts %.2f ms (%.1f percent, most %.2f)",
         n > 0 ? "," : "", w.excl_ms[0], Percent(w.excl_ms[0], w.long_ms), w.most_ms[0]);
  return static_cast<int>(used);
}

}  // namespace turn_parts_math
