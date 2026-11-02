// Where the time of the scene walk goes (profile_sample, a test key). See
// scene_sample.cpp and docs/backend.md, "Where the time of the scene walk
// goes".

#pragma once

#include <stdint.h>

// stereo.cpp, on the frame thread: a scene walk starts (true) or ends
// (false). The first start reads the key and starts the thread of the
// samples.
void SceneSampleWalk(bool in);
// stereo.cpp, each 450 frames, outside the scene walks: the summary of the
// samples since the last one. scene_ms is the time of the scene walks in a
// frame, for the ms of each module.
void SceneSampleReport(uint64_t frames, double scene_ms);
// time_log.cpp, at each present with slow_frame_ms above 0: the frame that
// ended was long (slow) and took frame_ms. With profile_sample_slow its
// samples then go to the next report, else they go.
void SceneSampleFrameEnd(bool slow, double frame_ms);
// pass_guard.cpp, around each call of GGame::ProcessNetworkPackets (the game
// turn), with profile_sample_turn above 0: the turn starts, with the stack
// pointer of the hook (the samples read the stack up to there), and it ends
// after turn_ms. With the key at 0 each call does nothing.
void SceneSampleTurnBegin(const void* stack);
void SceneSampleTurnEnd(double turn_ms);
