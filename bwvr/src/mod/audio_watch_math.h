// THE WAIT IN THE AUDIO TURN (audio_turn_watch, docs/audio.md, "7.7 The
// watch of the audio turn"): the stack of the open calls, the table of the
// time by call and call site, the sums of an audio turn, of a frame and of a
// window of 10 s, and the lines of the log. No Windows code, so the math test
// of the PC checks it (test/controller_math/audio_watch_checks.h).
//
// An audio turn is one call of GAudio::ProcessAudioGameTurn on the frame
// thread. Timer 0 is the audio turn itself, so its exclusive time is the part
// that no watched call covers: the code of the game and of the mod, and each
// call that has no timer. A watched call is a function that the audio turn
// reaches: an import of the game from LHaudiodllR.dll, an import of that DLL
// from QMixer.dll, a wait call of the two DLLs, a DirectSound call of QMixer
// and the lock of the mod. Its exclusive time is its time from the call to
// the return less the time of each watched call that it makes.
//
// The stack works in ticks of the clock and with integers only: the exit of a
// timer runs between the return of a function and its caller, and the x87
// unit can then hold the value that the function returns.
#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

namespace audio_watch_math {

constexpr int kMaxTimers = 192;
constexpr int kMaxDepth = 32;
// Entries of a table: one for each timer, call site and calling timer.
constexpr int kMaxEntries = 192;
// The most bytes of arguments that a function takes off the stack at its
// return (ret N). The exit of a timer finds its call by the place of the
// return address on the stack.
constexpr uint32_t kMaxArgBytes = 256;
// An audio turn of this many ms or more is a long audio turn.
constexpr double kLongAudioMs = 5.0;
// A line lists an entry only when its exclusive time is this many ms or more.
constexpr double kShownMs = 0.05;
// The most entries in the text of a long frame, and in the line of a window.
constexpr int kFrameShown = 8;
constexpr int kWindowShown = 16;

// One open call.
struct Open {
  int id = 0;
  int parent = -1;    // the timer that called it, or -1
  int64_t start = 0;  // the clock at the call
  int64_t child = 0;  // the time of the watched calls that it made
  uint32_t ret = 0;   // the return address of the caller
  uint32_t slot = 0;  // the place of the return address on the stack
};

// A call that the stack closed.
struct Closed {
  int id = 0;
  int parent = -1;
  uint32_t ret = 0;
  int64_t excl = 0;  // ticks
};

struct Stack {
  Open open[kMaxDepth];
  int depth = 0;
  uint64_t lost = 0;  // open calls that no return closed (a fault, a long jump)

  // A call of timer `id` at the clock `now`. False when the stack is full:
  // then the call is not timed.
  bool Push(int id, int64_t now, uint32_t ret, uint32_t slot) {
    if (depth >= kMaxDepth || id < 0 || id >= kMaxTimers) return false;
    Open& o = open[depth];
    o.id = id;
    o.parent = depth > 0 ? open[depth - 1].id : -1;
    o.start = now;
    o.child = 0;
    o.ret = ret;
    o.slot = slot;
    ++depth;
    return true;
  }

  // The top call closes at the clock `now`.
  Closed CloseTop(int64_t now) {
    const Open& o = open[--depth];
    int64_t in = now - o.start;
    if (in < 0) in = 0;
    int64_t ex = in - o.child;
    if (ex < 0) ex = 0;
    if (depth > 0) open[depth - 1].child += in;
    Closed c;
    c.id = o.id;
    c.parent = o.parent;
    c.ret = o.ret;
    c.excl = ex;
    return c;
  }

  // An open call that no return closed: it goes away with no time, and its
  // time stays in the call that called it.
  void DropTop() {
    --depth;
    ++lost;
  }

  // The index of the open call that a return at the stack place `esp`
  // closes: the topmost call with its return address at `esp` - 4 - N, for N
  // from 0 to kMaxArgBytes. When no call fits, the topmost call of a
  // function. A call with the slot 0 (the audio turn) has no return address
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
  uint32_t Return(int64_t now, uint32_t esp, Closed* closed) {
    const int i = Match(esp);
    if (i < 0) return 0;
    while (depth - 1 > i) DropTop();
    const uint32_t ret = open[i].ret;
    *closed = CloseTop(now);
    return ret;
  }

  // The return address of the caller of the topmost open call of a function,
  // or `fallback` when there is none. A hook of the mod that reads its own
  // return address gets the address of the exit thunk when a timer is on the
  // function. This gives the address that the game would see.
  uint32_t TopCaller(uint32_t fallback) const {
    for (int i = depth - 1; i >= 0; --i) {
      if (open[i].slot != 0) return open[i].ret;
    }
    return fallback;
  }
};

// The time of the calls of one timer from one call site and one calling
// timer.
struct Entry {
  int id = 0;
  int parent = -1;
  uint32_t ret = 0;
  uint32_t calls = 0;
  double excl_ms = 0.0;
  double most_ms = 0.0;
};

struct Table {
  Entry e[kMaxEntries];
  int n = 0;
  uint32_t dropped = 0;  // calls that found the table full

  void Clear() { *this = Table(); }

  void Add(const Entry& in) {
    for (int i = 0; i < n; ++i) {
      Entry& x = e[i];
      if (x.id != in.id || x.ret != in.ret || x.parent != in.parent) continue;
      x.calls += in.calls;
      x.excl_ms += in.excl_ms;
      if (in.most_ms > x.most_ms) x.most_ms = in.most_ms;
      return;
    }
    if (n >= kMaxEntries) {
      dropped += in.calls;
      return;
    }
    e[n++] = in;
  }

  void AddCall(const Closed& c, double ms_per_tick) {
    Entry in;
    in.id = c.id;
    in.parent = c.parent;
    in.ret = c.ret;
    in.calls = 1;
    in.excl_ms = static_cast<double>(c.excl) * ms_per_tick;
    in.most_ms = in.excl_ms;
    Add(in);
  }

  void Merge(const Table& o) {
    for (int i = 0; i < o.n; ++i) Add(o.e[i]);
    dropped += o.dropped;
  }
};

// The audio turns of a frame: the sums of the audio turns of one present.
struct Frame {
  Table table;
  double audio_ms = 0.0;
  uint32_t turns = 0;

  void AddTurn(const Table& t, double ms) {
    table.Merge(t);
    audio_ms += ms;
    ++turns;
  }
  void Clear() { *this = Frame(); }
};

// The audio turns of a window of 10 s. The table has the long audio turns
// only.
struct Window {
  uint64_t turns = 0;
  double turn_ms = 0.0;
  double worst_ms = 0.0;
  uint64_t long_turns = 0;
  double long_ms = 0.0;
  Table table;
  uint64_t outside_calls = 0;  // calls of a stub on the frame thread outside an audio turn
  uint64_t other_thread_calls = 0;  // calls of a stub on another thread

  void Reset() { *this = Window(); }

  void AddTurn(const Table& t, double ms) {
    ++turns;
    turn_ms += ms;
    if (ms > worst_ms) worst_ms = ms;
    if (ms < kLongAudioMs) return;
    ++long_turns;
    long_ms += ms;
    table.Merge(t);
  }
};

// The time of the calls of one kind on the mixer thread: the count, the sum
// and the most, in ms.
struct Span {
  uint64_t calls = 0;
  double sum_ms = 0.0;
  double most_ms = 0.0;
};

// A text of a call site, for the lines. The caller gives the module and the
// offset in it.
typedef void (*SiteFn)(uint32_t address, char* out, size_t size);

// The entries in the order of their exclusive ms, the largest first. Gives
// the count, at most `max`.
inline int Order(const Table& t, int* ids, int max) {
  int n = 0;
  for (int i = 0; i < t.n && n < max; ++i) ids[n++] = i;
  for (int a = 1; a < n; ++a) {
    const int id = ids[a];
    int b = a;
    while (b > 0 && t.e[ids[b - 1]].excl_ms < t.e[id].excl_ms) {
      ids[b] = ids[b - 1];
      --b;
    }
    ids[b] = id;
  }
  return n;
}

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

// One entry of a line: the name of the timer, the call site and the calling
// timer, the exclusive ms and the calls.
inline void AppendEntry(char* out, size_t size, size_t* used, const Entry& e,
                        const char* const* names, SiteFn site, bool first, double whole_ms) {
  char where[48] = "";
  if (e.ret != 0 && site != nullptr) site(e.ret, where, sizeof(where));
  Append(out, size, used, "%s %s", first ? "" : ",", names[e.id]);
  if (where[0] != 0) Append(out, size, used, " from %s", where);
  if (e.parent >= 0) Append(out, size, used, " in %s", names[e.parent]);
  Append(out, size, used, " %.2f ms (", e.excl_ms);
  if (whole_ms > 0.0) Append(out, size, used, "%.1f percent, ", Percent(e.excl_ms, whole_ms));
  Append(out, size, used, "%u %s, most %.2f)", e.calls, e.calls == 1 ? "call" : "calls", e.most_ms);
}

// The text of the audio turns of one frame for the line of a long frame.
inline int FrameText(char* out, size_t size, const Frame& f, const char* const* names,
                     SiteFn site) {
  size_t used = 0;
  if (size == 0) return 0;
  out[0] = 0;
  Append(out, size, &used,
         "The audio turn took %.2f ms. Its parts in ms, exclusive, with the call site and the "
         "calling part:",
         f.audio_ms);
  int ids[kMaxEntries];
  const int n = Order(f.table, ids, kMaxEntries);
  int shown = 0;
  for (int k = 0; k < n; ++k) {
    const Entry& e = f.table.e[ids[k]];
    if (e.excl_ms < kShownMs) break;
    if (shown >= kFrameShown) break;
    AppendEntry(out, size, &used, e, names, site, shown == 0, 0.0);
    ++shown;
  }
  if (n > shown) Append(out, size, &used, ", %d more under %.2f ms or not shown", n - shown, kShownMs);
  return static_cast<int>(used);
}

// The line of a span of the mixer thread.
inline void AppendSpan(char* out, size_t size, size_t* used, const char* name, const Span& s,
                       bool first) {
  Append(out, size, used, "%s %s %llu %s", first ? "" : ",", name,
         static_cast<unsigned long long>(s.calls), s.calls == 1 ? "call" : "calls");
  if (s.calls > 0) {
    Append(out, size, used, " (mean %.3f ms, most %.3f ms)",
           s.sum_ms / static_cast<double>(s.calls), s.most_ms);
  }
}

// The line of a window of 10 s: the audio turns, the parts of the long ones,
// and the calls of the mixer thread.
inline int WindowLine(char* out, size_t size, const Window& w, const char* const* names,
                      SiteFn site, const char* const* span_names, const Span* spans,
                      int span_count, double seconds) {
  size_t used = 0;
  if (size == 0) return 0;
  out[0] = 0;
  Append(out, size, &used,
         "%.1f s, %llu audio turns (mean %.2f ms, worst %.2f ms), %llu of them %.0f ms or more "
         "(together %.2f ms). The stubs saw %llu calls on the frame thread outside an audio turn "
         "and %llu on another thread.",
         seconds, static_cast<unsigned long long>(w.turns),
         w.turns > 0 ? w.turn_ms / static_cast<double>(w.turns) : 0.0, w.worst_ms,
         static_cast<unsigned long long>(w.long_turns), kLongAudioMs, w.long_ms,
         static_cast<unsigned long long>(w.outside_calls),
         static_cast<unsigned long long>(w.other_thread_calls));
  if (w.long_turns == 0) {
    Append(out, size, &used, " No long audio turn, so no part to show.");
  } else {
    Append(out, size, &used,
           " The parts of the long audio turns, exclusive ms in all of them, with the call site "
           "and the calling part, the share, the calls and the most in one call:");
    int ids[kMaxEntries];
    const int n = Order(w.table, ids, kMaxEntries);
    int shown = 0;
    for (int k = 0; k < n; ++k) {
      const Entry& e = w.table.e[ids[k]];
      if (e.excl_ms < kShownMs || shown >= kWindowShown) break;
      AppendEntry(out, size, &used, e, names, site, shown == 0, w.long_ms);
      ++shown;
    }
    if (n > shown) {
      Append(out, size, &used, ", %d more under %.2f ms or not shown", n - shown, kShownMs);
    }
    if (w.table.dropped > 0) {
      Append(out, size, &used, ". %u calls found the table full", w.table.dropped);
    }
    Append(out, size, &used, ".");
  }
  Append(out, size, &used, " The mixer thread:");
  for (int i = 0; i < span_count; ++i) {
    AppendSpan(out, size, &used, span_names[i], spans[i], i == 0);
  }
  return static_cast<int>(used);
}

}  // namespace audio_watch_math
