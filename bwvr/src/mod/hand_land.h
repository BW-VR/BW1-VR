// The Hand stops at the land: the drawn hands stay on the land when the real
// hands go into it. See hand_land.cpp and docs/hand-model.md, "The Hand stops
// at the land".

#pragma once

// How far the drawn hand (0 the left hand, 1 the right hand) is over the
// real hand in this frame, in world units: 0 when the hand is over the land.
// The value is latched once a frame, on the first call. False when the stop
// is off or the hand is not known.
bool HandLandLift(int hand, float* lift);

// For hand_direct.cpp: the lift of the pointing hand for the Hand's point of
// this frame. False (and 0) when the point is not the tip of the index
// finger of the drawn hand, for example with a reach that is not one to one.
bool HandLandPointLift(const float point[3], float* lift);

// True while the land holds the hand up in this frame (the lift is more than
// 0), for the pulse of hand feel.
bool HandLandHeld(int hand);

// The switch of the debug page (debug_menu.h): hand_land_stop.
int HandLandStopValue();
void HandLandStopChange(const char* by);

// Once a report window.
void HandLandReport();
