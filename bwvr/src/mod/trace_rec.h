// THE TIMELINE OF THE GAME (trace, trace_events.h, docs/backend.md, "The
// timeline of the game and the app"). A recorder of bars, points and counters
// for each thread of the game, and a save of the last seconds into a file of
// the Trace Event Format of Chrome (ui.perfetto.dev reads it).
//
//   trace=0               no recorder and no cost (the default). The callers
//                         read g_trace first, so they make no call below
//   trace=1               each thread records into a ring of its own. The
//                         recorder makes no allocation and no write of a file
//                         in a call (the ring of a thread is made at its
//                         first event)
//   trace_seconds=N       a save writes the last N seconds (10)
//   trace_auto_ms=N       a frame of the game longer than N ms starts a save,
//                         at most one a minute (40, 0 is off)
//   trace_min_us=N        a bar shorter than N us is not recorded (30)
//   trace_wait_us=N       a wait of the frame thread shorter than N us is not
//                         recorded (100)
//   trace_ring_k=N        the ring of the frame thread has N K events (128).
//                         The other threads have 16 K events
//   trace_keyboard=1      Ctrl Shift F11 saves (the desk run)
//   trace_test_save_s=N   a test aid: one save N s after the first frame
//
// A save comes from the app (a button combination of the controllers, written
// to the input channel), from the game (a long frame, the key, the test aid),
// or from the key of the keyboard. The game writes its part next to the log
// and the app joins the two parts. Without the app the game writes the whole
// file.

#pragma once

#include <stdint.h>

#include "../common/trace_events.h"

// True with trace=1. The callers read it first.
extern bool g_trace;

// The clock of the events: QueryPerformanceCounter ticks.
int64_t TraceNow();

// Reads the keys and starts the recorder. After ModLog is ready, on the frame
// thread (mod_main.cpp, before PassGuardInstall).
void TraceInstall();

// The ring of the calling thread gets a name and an order in the file. Call it
// at the start of a thread. events: the size of the ring, 0 for the default.
void TraceNameThread(const char* name, int sort, unsigned events = 0);

// The events. Each call is for the thread that calls.
void TraceBar(const trace_events::Def* def, int64_t begin, int64_t end, float a = 0.0f,
              float b = 0.0f, float c = 0.0f, float d = 0.0f);
void TracePoint(const trace_events::Def* def, float a = 0.0f, float b = 0.0f, float c = 0.0f,
                float d = 0.0f);
void TraceCounter(const trace_events::Def* def, float a = 0.0f, float b = 0.0f, float c = 0.0f,
                  float d = 0.0f);
// A bar of the wait of the frame thread: shorter than trace_wait_us it is not
// recorded.
void TraceWait(const trace_events::Def* def, int64_t begin, int64_t end, float a = 0.0f);

// mod_main.cpp, at each present of the game (TimeLogPresent): the bar of the
// frame, the counter of the frames a second, the automatic save and the keys.
void TraceFramePresent();

// stereo.cpp, at each scene walk: this frame is a frame of the scene (only
// such a frame can start an automatic save).
void TraceSceneFrame();

// A save of the last seconds, ending now. why: a text for the log.
void TraceSave(const char* why);

// The bar of a scope on the calling thread.
struct TraceScope {
  const trace_events::Def* def;
  int64_t begin;
  float a;
  explicit TraceScope(const trace_events::Def* d, float v = 0.0f)
      : def(d), begin(g_trace ? TraceNow() : 0), a(v) {}
  ~TraceScope() {
    if (begin != 0) TraceBar(def, begin, TraceNow(), a);
  }
  TraceScope(const TraceScope&) = delete;
  TraceScope& operator=(const TraceScope&) = delete;
};
