// The Hand stops at the land and at the solid objects: the drawn hands stay
// on the land and on a house, a rock and a tree trunk when the real hands go
// into them. See hand_land.cpp and docs/hand-model.md, "The Hand stops at the
// land".

#pragma once

// The place of the drawn hand (0 the left hand, 1 the right hand) from the
// real hand in this frame, in world units: the push out of the solids and
// the lift over the land. 0 when the hand is free. The value is latched once
// a frame, on the first call. False when both stops are off or the hand is
// not known.
bool HandLandOffset(int hand, float offset[3]);

// For hand_direct.cpp: the lift of the pointing hand over the land for the
// Hand's point of this frame. False (and 0) when the point is not the tip of
// the index finger of the drawn hand, for example with a reach that is not
// one to one. The solids do not move the Hand's point: it goes into a body
// to choose it.
bool HandLandPointLift(const float point[3], float* lift);

// True while the land holds the hand up in this frame (the lift is more than
// 0), for the pulse of hand feel. A solid does not count: this file sends the
// pulse of a solid.
bool HandLandHeld(int hand);

// For hand_direct.cpp (hand_object_stop): the middle of each real hand that
// wants the solids near it, and the bound in world units. Gives the count, at
// most 2.
int HandLandSolidPoints(float points[][3], float bound[]);

// The switches of the debug page (debug_menu.h): hand_land_stop and
// hand_object_stop.
int HandLandStopValue();
void HandLandStopChange(const char* by);
int HandObjectStopValue();
void HandObjectStopChange(const char* by);

// Once a report window.
void HandLandReport();
