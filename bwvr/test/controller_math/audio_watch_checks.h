// The watch of the audio turn, a test: the stack of the open calls with the
// exclusive time, the table by call site, the audio turns of a frame and of a
// window, and the lines (src/mod/audio_watch_math.h). main.cpp includes this
// file after Check and Near, just before main.
#pragma once

#include <stdint.h>
#include <string.h>

#include "../../src/mod/audio_watch_math.h"

namespace audio_watch_checks {

inline bool Has(const char* text, const char* part) { return strstr(text, part) != nullptr; }

inline void Site(uint32_t address, char* out, size_t size) {
  snprintf(out, size, "QM+0x%X", address & 0xFFFF);
}

inline void AudioWatchChecks() {
  namespace aw = audio_watch_math;
  printf("\nThe watch of the audio turn (audio_turn_watch)\n");

  // AN AUDIO TURN OF 10 ms at 1000 ticks for each ms. Timer 7 (a call of the
  // game into the audio DLL, 2 arguments of 4 bytes) from 1 to 9 ms. Inside
  // it timer 8 (a wait) from 3 to 5 ms, and timer 9 (a DirectSound call
  // with no return address on the stack) from 6 to 7 ms.
  static aw::Stack s;
  s = aw::Stack();
  Check(s.Push(0, 0, 0, 0), "the audio turn is pushed", 1, 1);
  s.Push(7, 1000, 0x401000, 0x1000);
  s.Push(8, 3000, 0x5A0010, 0x0F00);
  aw::Closed c;
  const uint32_t ret = s.Return(5000, 0x0F04, &c);
  Check(ret == 0x5A0010, "the return gives the address of the caller", ret, 0x5A0010);
  Check(c.id == 8 && c.parent == 7 && c.excl == 2000, "timer 8: id, calling timer, exclusive ticks",
        static_cast<double>(c.excl), 2000);
  s.Push(9, 6000, 0x5A0020, 0);
  const aw::Closed d = s.CloseTop(7000);
  Check(d.id == 9 && d.parent == 7 && d.ret == 0x5A0020 && d.excl == 1000,
        "a call with no return address on the stack closes by CloseTop", static_cast<double>(d.excl), 1000);
  const uint32_t ret7 = s.Return(9000, 0x1000 + 4 + 8, &c);
  Check(ret7 == 0x401000 && c.excl == 5000,
        "timer 7: the 8 ms less the wait and the call inside it is 5 ms",
        static_cast<double>(c.excl), 5000);
  const aw::Closed root = s.CloseTop(10000);
  Check(root.id == 0 && root.excl == 2000 && s.depth == 0,
        "the audio turn keeps the 2 ms that no watched call covers",
        static_cast<double>(root.excl), 2000);

  // THE CALLER THAT A HOOK SEES. A call with a stub has its return address
  // replaced. The call with the slot 0 does not.
  s = aw::Stack();
  s.Push(0, 0, 0, 0);
  Check(s.TopCaller(0x77) == 0x77, "no call on the stack: the fallback", s.TopCaller(0x77), 0x77);
  s.Push(7, 1000, 0x401234, 0x1000);
  s.Push(9, 1500, 0x5A0020, 0);
  Check(s.TopCaller(0x77) == 0x401234, "the topmost call of a function gives its caller",
        s.TopCaller(0x77), 0x401234);

  // A RETURN THAT FINDS NO CALL, AND A FULL STACK.
  s = aw::Stack();
  s.Push(0, 0, 0, 0);
  Check(s.Return(100, 0x2000, &c) == 0, "a return with no open call of a function gives 0",
        s.Return(100, 0x2000, &c), 0);
  s = aw::Stack();
  for (int i = 0; i < aw::kMaxDepth; ++i) s.Push(1, i, 0, 0);
  Check(!s.Push(1, 99, 0, 0), "a full stack takes no more", 0, 0);

  // A CALL THAT NO RETURN CLOSED: its time stays in its caller.
  s = aw::Stack();
  s.Push(0, 0, 0, 0);
  s.Push(7, 1000, 0x401000, 0x1000);
  s.Push(8, 2000, 0x5A0010, 0x0E00);
  s.Return(9000, 0x1004, &c);
  Check(c.id == 7 && s.lost == 1 && s.depth == 1,
        "the return of the outer call drops the inner call that did not return", static_cast<double>(s.lost), 1);

  // THE TABLE: the same timer, site and calling timer adds up.
  static aw::Table t;
  t.Clear();
  aw::Closed a;
  a.id = 8;
  a.parent = 7;
  a.ret = 0x5A0010;
  a.excl = 2000;
  t.AddCall(a, 0.001);
  a.excl = 30000;
  t.AddCall(a, 0.001);
  a.ret = 0x5A0020;
  a.excl = 100;
  t.AddCall(a, 0.001);
  Check(t.n == 2, "two call sites make two entries", t.n, 2);
  Check(t.e[0].calls == 2 && t.e[0].excl_ms > 31.99 && t.e[0].excl_ms < 32.01 &&
            t.e[0].most_ms > 29.99 && t.e[0].most_ms < 30.01,
        "an entry adds the calls and the ms and keeps the most", t.e[0].excl_ms, 32.0);
  static aw::Table full;
  full.Clear();
  for (int i = 0; i < aw::kMaxEntries + 3; ++i) {
    aw::Closed z;
    z.id = 1;
    z.ret = 0x1000 + i;
    z.excl = 10;
    full.AddCall(z, 0.001);
  }
  Check(full.n == aw::kMaxEntries && full.dropped == 3, "a full table counts the calls it dropped",
        full.dropped, 3);

  // THE FRAME AND THE WINDOW. A turn of 4 ms is not long, one of 40 ms is.
  static aw::Frame f;
  f.Clear();
  f.AddTurn(t, 40.0);
  Check(f.turns == 1 && f.audio_ms == 40.0 && f.table.n == 2, "a frame takes the table of its turn",
        f.audio_ms, 40.0);
  f.Clear();
  Check(f.turns == 0 && f.table.n == 0, "a new frame is empty", f.turns, 0);
  static aw::Window w;
  w.Reset();
  w.AddTurn(t, 4.0);
  Check(w.turns == 1 && w.long_turns == 0 && w.table.n == 0,
        "a window keeps no parts of a turn under 5 ms", static_cast<double>(w.long_turns), 0);
  w.AddTurn(t, 40.0);
  w.AddTurn(t, 3.0);
  Check(w.turns == 3 && w.long_turns == 1 && w.worst_ms == 40.0 && w.long_ms == 40.0 &&
            w.table.n == 2,
        "a window keeps the parts of a turn of 5 ms or more", static_cast<double>(w.long_turns), 1);

  // THE TEXTS.
  const char* names[16] = {"audio_turn", "ds:Lock", "x", "x", "x", "x", "x", "x",
                           "k32:WaitForSingleObject", "x", "x", "x", "x", "x", "x", "x"};
  names[7] = "QM:QSWaveMixSetVolume";
  static char text[4096];
  f.Clear();
  f.AddTurn(t, 40.0);
  aw::FrameText(text, sizeof(text), f, names, &Site);
  Check(Has(text, "The audio turn took 40.00 ms") &&
            Has(text, "k32:WaitForSingleObject from QM+0x10 in QM:QSWaveMixSetVolume 32.00 ms "
                      "(2 calls, most 30.00)"),
        "the text of a frame: the turn, the part, the call site, the calling part", 1, 1);
  const char* span_names[2] = {"Lock", "waveOutWrite"};
  aw::Span spans[2];
  spans[0].calls = 100;
  spans[0].sum_ms = 50.0;
  spans[0].most_ms = 3.0;
  aw::WindowLine(text, sizeof(text), w, names, &Site, span_names, spans, 2, 10.0);
  Check(Has(text, "10.0 s, 3 audio turns (mean 15.67 ms, worst 40.00 ms), 1 of them 5 ms or more "
                  "(together 40.00 ms)") &&
            Has(text, "k32:WaitForSingleObject from QM+0x10 in QM:QSWaveMixSetVolume 32.00 ms "
                      "(80.0 percent, 2 calls, most 30.00)") &&
            Has(text, "The mixer thread: Lock 100 calls (mean 0.500 ms, most 3.000 ms),"
                      " waveOutWrite 0 calls"),
        "the line of a window: the turns, the parts, the mixer thread", 1, 1);
  w.Reset();
  aw::WindowLine(text, sizeof(text), w, names, &Site, span_names, spans, 2, 10.0);
  Check(Has(text, "No long audio turn, so no part to show."), "a window with no long turn says so",
        1, 1);
}

}  // namespace audio_watch_checks
