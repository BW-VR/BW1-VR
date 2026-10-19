// The carry of the other hand. See left_hold.cpp and docs/controllers.md,
// 5.7, "The carry of the other hand". controllers.cpp owns the key
// left_hold and its test key.

#pragma once

#include <stdint.h>

// The other hand carries an object now.
bool LeftHoldCarrying();
// Once a controllers frame, from the pump of the buttons, with left_hold=1.
// take: the borrowed Hand takes for the other hand now (LeftTakeStep,
// kLeftTake). drop: the trigger of the other hand opened while it carries.
// usable: the controllers can act now. A carry that cannot go on lets the
// object go.
void LeftHoldFrame(bool take, bool drop, bool usable, uint64_t frame);
// The other hand lets go of the object that it carries, with no speed.
void LeftHoldLetGo(const char* why);
// After the hooks of the Hand are in: the call after the draw setup of the
// Hand, which puts the carried object in the other hand.
void LeftHoldInstall();
