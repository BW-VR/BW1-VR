// THE PARTS OF THE SCENE WALK (walk_parts, a test aid, docs/backend.md, "The
// parts of the scene walk"). Each 10 s a block of lines in bwvr.log with the
// calls, the inclusive and the exclusive ms in a frame of each timed function
// in the scene walk. The rules are in walk_parts_math.h.
#pragma once

#include <stdint.h>

// mod_main.cpp, at the start. mode: walk_parts=0 (the default) no hook and no
// line. 1: each timer. 2: no timer on each call of the device and of the
// Vulkan renderer. `clock` is walk_parts_clock: 1 the time stamp counter of
// the CPU, 0 QueryPerformanceCounter. `add` is walk_parts_add: more
// addresses of functions to time, with a comma between them, or empty.
void WalkPartsSetOptions(int mode, int clock, const char* add);
bool WalkPartsOn();
// stereo.cpp, beside TimeLogWalk: a scene walk starts and ends. The first
// walks make the hooks.
void WalkPartsWalk(bool begin);
// h_FinishFrame, beside TimeLogPresent: the present of a frame.
void WalkPartsPresent();
