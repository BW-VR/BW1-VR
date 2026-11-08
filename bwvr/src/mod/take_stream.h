// The stream of a take at a distance. See take_stream.cpp and docs/grab.md,
// "The stream of a take at a distance".

#pragma once

#include <stdint.h>

// take_stream: 1 puts the pieces of the stream of a take at a distance on a
// curve from the source to the Hand. 0 leaves the line of the game. test_keys
// gives the A/B key.
void TakeStreamSetOptions(int take_stream, bool test_keys);
// take_stream_hand: 1 a take with the Hand at the source (the direct grab)
// has the curve of a take at a distance too, from the top of the source of
// the game's take. 0 it has the line of the game. trace (the test key
// take_stream_test_trace): a line for each frame of a stream with the top of
// the source, the Hand and the newest piece.
void TakeStreamSetHand(int hand, int trace);
// The hook on the rule of the stream. After FarGrabInstall.
void TakeStreamInstall();
// hand_direct.cpp, once a frame, before the walk of the scene: the frame for
// the latch of the hook, the end of a stream, and the A/B key.
void TakeStreamFrame(uint64_t frame);
