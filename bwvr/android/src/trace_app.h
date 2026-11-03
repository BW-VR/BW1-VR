// THE TIMELINE OF THE HEADSET APP (trace=1, src/common/trace_events.h, docs/
// android.md, "The timeline", docs/backend.md, "The timeline of the game and
// the app"). The recorder of the app: bars, points and counters for each
// thread, with the clock CLOCK_BOOTTIME in ns. Wine reads the same clock for
// QueryPerformanceCounter, so the game and the app share one timeline.
//
//   trace=0               no recorder and no cost (the default). The callers
//                         read g_app_trace first
//   trace=1               each thread records into a ring of its own, and the
//                         button combination of the left controller saves
//   trace_seconds=N       a save writes the last N seconds (10)
//   trace_min_us=N        a bar shorter than N us is not recorded (30)
//   trace_combo_ms=N      the left X button and the click of the left stick,
//                         held together for N ms, ask for a save (300)
//
// A SAVE has three sources: the combination (the app asks the game through the
// head of the input channel), a long frame or the key of the game (the game
// counts a save in the head), and nothing else. The app writes its part, waits
// for the part of the game (bwvr-trace-game.json in the game folder), and
// joins both into trace-<time>.json in the folder of the app. A thread of low
// priority does the work. A short pulse of both controllers says that the
// file is written.
#pragma once

#include <stdint.h>

#include "../../src/common/input_channel.h"
#include "../../src/common/trace_events.h"

extern bool g_app_trace;

// CLOCK_BOOTTIME in ns.
int64_t AppTraceNow();

// After the log is open, before the threads start. files: the folder for the
// files of the app. game: the folder of the game. Does nothing with on=false.
void AppTraceStart(bool on, int seconds, int min_us, int combo_ms, const char* files,
                   const char* game);

// The ring of the calling thread gets a name and an order in the file.
void AppTraceNameThread(const char* name, int sort, unsigned events = 0);

void AppTraceBar(const trace_events::Def* def, int64_t begin, int64_t end, float a = 0.0f,
                 float b = 0.0f, float c = 0.0f, float d = 0.0f);
void AppTracePoint(const trace_events::Def* def, float a = 0.0f, float b = 0.0f, float c = 0.0f,
                   float d = 0.0f);
void AppTraceCounter(const trace_events::Def* def, float a = 0.0f, float b = 0.0f, float c = 0.0f,
                     float d = 0.0f);

// The thread of the session, at each frame after the slot is written: the
// button combination, and the saves of the game. channel: the mapped input
// channel, or nullptr. Gives true when both controllers must give a pulse.
bool AppTraceSession(const input_channel::SlotData& slot, input_channel::File* channel);

// The thread of the session at the end of the app.
void AppTraceStop();

// The bar of a scope on the calling thread.
struct AppTraceScope {
  const trace_events::Def* def;
  int64_t begin;
  float a, b;
  explicit AppTraceScope(const trace_events::Def* d, float x = 0.0f, float y = 0.0f)
      : def(d), begin(g_app_trace ? AppTraceNow() : 0), a(x), b(y) {}
  ~AppTraceScope() {
    if (begin != 0) AppTraceBar(def, begin, AppTraceNow(), a, b);
  }
  AppTraceScope(const AppTraceScope&) = delete;
  AppTraceScope& operator=(const AppTraceScope&) = delete;
};
