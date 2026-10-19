// The camera of the player that the Creature reads: the head of the player in
// the God view. See creature_head.cpp and docs/creature.md, "The Creature
// turns to the head".

#pragma once

#include <stdint.h>

// hand_direct.cpp, at the start, with the other keys of the Creature.
void CreatureHeadInstall();

// True when the hooks are in. hand_direct.cpp then latches the player on
// each frame.
bool CreatureHeadHooked();

// hand_direct.cpp, once a frame, after the latch of the player: the frame
// number for the log, and the test key.
void CreatureHeadFrame(uint64_t frame);

// Once a report window.
void CreatureHeadReport();
