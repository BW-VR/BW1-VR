// THE PARTS OF THE GAME TURN (turn_parts, turn_parts_math.h, docs/backend.md,
// "The long game turn"). Timers on the large functions that the game turn
// calls. All calls are on the frame thread.

#pragma once

#include <stddef.h>
#include <stdint.h>

// True with turn_parts=1 when the hooks are in. The callers read it first, so
// with turn_parts=0 they make no call below.
extern bool g_turn_parts;

// pass_guard.cpp, in PassGuardInstall on the frame thread: turn_parts=1 puts
// the hooks in, with one MH_ApplyQueued.
void TurnPartsInstall();
// mod_main.cpp, after the last hook of the mod: with trace=1 the timers go in
// here and not in TurnPartsInstall.
void TurnPartsInstallLate();
// frame_timers.cpp, GraphicsStub, with trace=1: the call of GGame::
// ProcessGraphicsEngine on the frame thread starts and ends. The timers on its
// callees count in between (turn_parts.cpp, kGraphicsDefs).
void TurnPartsGraphicsBegin();
void TurnPartsGraphicsEnd();
// trace_rec.cpp, in the log of a save: the calls of the timers that got a clock
// read since the start (trace=1).
uint32_t TurnPartsTimedCalls();
// pass_guard.cpp, h_ProcessNetworkPackets: the game turn starts at the tick
// `begin` of the performance counter, and ends at the tick `end`.
void TurnPartsTurnBegin(int64_t begin);
void TurnPartsTurnEnd(int64_t end);
// time_log.cpp, SlowFrame: the text of the parts of the game turn in the frame
// that ended at this present. It gives the length of the text. TurnPartsFrameEnd
// clears the turns of the frame.
int TurnPartsFrameText(char* out, size_t size);
void TurnPartsFrameEnd();
// time_log.cpp, at the end of its window of 10 s: the line of the parts of the
// long turns of the window. True when a line went to the log.
bool TurnPartsWindowLine(double seconds);
