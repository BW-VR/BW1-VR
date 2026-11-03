// The draw of the meshes of a 3D part with SSE (part_draw.cpp, docs/backend.md,
// "The parts with SSE").

#pragma once

#include <stdint.h>

// part_draw of bwvr.ini: 0 the code of the game, 1 SSE, with the bits of the
// game, 2 a test aid: the code of the game runs, and the mod compares what
// the SSE code makes with it. With test_keys=1, Ctrl Shift F10 steps
// part_draw through 1, 0 and 2.
void PartDrawSetOptions(int mode, bool test_keys);
void PartDrawInstall();
// h_FinishFrame, after the present: the test key and the report.
void PartDrawFrame();
// The debug page: the value (0 game, 1 SSE) or kDebugCannot, and the switch.
int PartDrawValue();
void PartDrawToggle(const char* keys);
