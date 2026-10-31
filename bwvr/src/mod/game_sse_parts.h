// The SSE paths of the game, one at a time (game_sse_parts.cpp,
// game_sse_parts_math.h, docs/backend.md, "The SSE paths of the game, one at
// a time").
#pragma once

// game_sse_parts of bwvr.ini: 0 (the default) no patch, and each part of the
// game reads the flag of the game (the class of the CPU, and game_sse_path).
// Else a mask of bits: each part with its bit takes the SSE path of the game,
// and each part with no bit takes the x87 path, on each CPU. -1 is each part.
// game_sse_check of bwvr.ini (a test aid): 1 runs each part with its bit two
// times, on the x87 path and on the SSE path, and the log has the largest
// difference of the outputs. 0 (the default) no compare.
// test_keys: Ctrl, Shift and S (no other test key has S) change between the
// mask and the flags of the game, from the next frame.
void GameSsePartsSetOptions(int parts, int check, bool test_keys);
// Startup, before each other hook: a hook of the mod on a function with a
// reader then copies the patched reader.
void GameSsePartsInstall();
// h_FinishFrame, after the FinishFrame of the game: the test key, the latch
// of the flags at the end of a frame, and the lines of the compare.
void GameSsePartsFrame();
