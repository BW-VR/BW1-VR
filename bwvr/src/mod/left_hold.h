// The carry of the other hand. See left_hold.cpp and docs/controllers.md,
// 5.7, "The carry of the other hand". controllers.cpp owns the key
// left_hold and its test key.

#pragma once

#include <stdint.h>

// The other hand carries an object now.
bool LeftHoldCarrying();
// An object that the Hand can hold is at the palm of the other hand, for a
// take from the side while the Hand of the game is busy.
bool LeftHoldSideTarget();
// Once a controllers frame, from the pump of the buttons, with left_hold=1.
// take: the borrowed Hand takes for the other hand now (LeftTakeStep,
// kLeftTake). side_take: take the object at the palm of the other hand with
// no borrow (kLeftSide). drop: the trigger of the other hand opened while it
// carries. usable: the controllers can act now. A carry that cannot go on
// lets the object go.
void LeftHoldFrame(bool take, bool side_take, bool drop, bool usable,
                   uint64_t frame);
// THE GLOW OF THE OTHER HAND (left_glow). Once a controllers frame: on is
// false while the other hand can take nothing now. The object in the reach
// of the other hand, the same search as the take from the side, is the
// target of that hand for this frame.
void LeftHoldGlowFrame(bool on, uint64_t frame);
// hand_glow.cpp: the target of the other hand of this frame or the frame
// before, or null, and the tip of its index finger with the length of the
// hand.
void* LeftHoldGlowTarget(uint64_t frame);
bool LeftHoldGlowTip(float tip[3], float* length);
// The other hand lets go of the object that it carries, with no speed.
void LeftHoldLetGo(const char* why);
// After the hooks of the Hand are in: the call after the draw setup of the
// Hand, which puts the carried object in the other hand.
void LeftHoldInstall();
