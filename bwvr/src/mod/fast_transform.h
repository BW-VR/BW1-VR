// The two vertex loops of the engine with SSE (fast_transform.cpp,
// docs/backend.md, "The vertex loops with SSE").

#pragma once

#include <stdint.h>

// fast_transform of bwvr.ini: 1 runs the loops with SSE, 0 on the x87 unit.
void FastTransformSetOptions(int on);
// fast_transform_check=1 (a test key): each loop runs on the x87 unit, and
// the mod compares a copy that the SSE loop makes with it.
void FastTransformSetCheck(int on);
void FastTransformInstall();
bool FastTransformOn();
void FastTransformSet(bool on);
// The test key: fast_transform goes between 1 and 0.
void FastTransformToggle(const char* keys);
// The value of fast_transform for the debug page, or kDebugCannot.
int FastTransformValue();
void FastTransformReport(uint64_t frames);
// walk_parts.cpp: the function of the SSE loop `i` (0 to 3) and its name, or
// nullptr for an `i` out of the range.
void* FastTransformLoop(int i, const char** name);
