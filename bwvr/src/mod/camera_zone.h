// The camera zone of a script of the game in the God view. See
// camera_zone.cpp and docs/god-view.md, "The camera zone".

#pragma once

// From TabletopInstall in the God view: the keys god_zone and
// god_zone_margin, and the hook on the shake of the camera.
void CameraZoneInstall(int on, int margin);

// True when god_zone=1, the God view owns the view and a script set a zone.
bool CameraZoneActive();

// Reads the zone of the game and logs a change. tabletop.cpp calls it on
// each main camera call of the God view.
void CameraZoneWatch();

// The limit of a move of the player's place, the waist point of the God
// view. from is the place before the move, and x and z are the place that
// the move asks for. True when the limit changed x and z. `by` names the
// move for the log, and must be a string that stays.
bool CameraZoneLimitMove(const float from[3], float* x, float* z,
                         const char* by);

// True when a place at x z is in the limit (for a teleport target).
bool CameraZoneAllows(float x, float z);

// The land point that the game camera looks at goes into the zone. True when
// it moved.
bool CameraZoneFocus(float* x, float* z);

// The position of the game camera goes into the zone, along the line to its
// focus. True when it moved.
bool CameraZoneKeepCamera(const float focus[3], float position[3]);

// A push of the player past the limit asks the game for its sound of the
// block, at most once each 500 ms. True once for each such push, with the
// point out of the zone where the game camera goes for one update.
bool CameraZoneTakeBump(float* x, float* z);

// The debug page and its test switch.
int CameraZoneValue();
void CameraZoneToggle();

// One window of the report.
void CameraZoneReport();
