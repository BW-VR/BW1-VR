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
// mod_main.cpp, at the end of the installs of the first frame, after the last hook of
// the mod, on the frame thread: with walk_parts=1 or 2 and walk_parts_early=1 (the
// default) it puts all hooks in with one MH_ApplyQueued. It reads its keys from
// bwvr.ini itself.
void WalkPartsInstallEarly();
// stereo.cpp, beside TimeLogWalk: a scene walk starts and ends. The first
// walks make the hooks.
void WalkPartsWalk(bool begin);
// THE HOOKS OF THE MOD THAT READ THEIR RETURN ADDRESS. A stub of a timer would
// change it, so each of these hooks times itself with a WalkPartsScope.
enum WalkPartsHook {
  kWalkHookFinishFrame = 0,  // h_FinishFrame, mod_main.cpp
  kWalkHookDrawTriangle,     // h_DrawTriangle, draw_hooks.cpp
  kWalkHookDrawAndClip,      // h_DrawAndClip, draw_hooks.cpp
  kWalkHookDrawAndClip2D,    // h_DrawAndClip2D, draw_hooks.cpp
  kWalkHookSortedFlush,      // h_SortedFlush, eye_stale.cpp
  kWalkHookCount
};
// The call of the hook starts. Gives the place of its timer on the stack of
// the timers, or -1 when no timer runs.
int WalkPartsHookBegin(int hook);
void WalkPartsHookEnd(int open);
struct WalkPartsScope {
  explicit WalkPartsScope(int hook) : open(WalkPartsHookBegin(hook)) {}
  ~WalkPartsScope() { WalkPartsHookEnd(open); }
  WalkPartsScope(const WalkPartsScope&) = delete;
  WalkPartsScope& operator=(const WalkPartsScope&) = delete;
  int open;
};
// h_FinishFrame, beside TimeLogPresent: the present of a frame.
void WalkPartsPresent();
