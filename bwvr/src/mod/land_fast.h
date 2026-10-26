// The vertex loop of the land with SSE (land_fast.cpp, docs/backend.md, "The
// vertex loop of the land with SSE").

#pragma once

// land_fast of bwvr.ini: 0 the loop of the game on the x87 unit, 1 the loop
// with SSE, 2 a test aid: the loop of the game runs, and the mod compares a
// copy that the SSE loop makes with it.
// game_sse_path of bwvr.ini (a test key): -1 the game chooses its paths by
// the CPU, 0 the paths of the x87 unit (as on a CPU of family 6, and on the
// Quest under FEX), 1 the SSE paths of the game.
void LandFastSetOptions(int mode, int sse_path);
void LandFastInstall();
