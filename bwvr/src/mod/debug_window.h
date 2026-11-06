// The debug window: the switches of the debug page (debug_menu.cpp) in a
// window of parchment, fixed in the room in front of the player. The item
// "debug" of the ring opens it with debug_window=1, and the ray and the
// trigger of the pointing hand change its switches. See debug_window.cpp,
// docs/controllers.md, "The debug window", and docs/ui-layer.md, "The debug
// window".

#pragma once

// radial_menu.cpp, at the choice of the item "debug": the window opens in
// front of the head. False when the head has no pose yet.
bool DebugWindowOpenFromRing();

// True while the window is open. It uses the quad layer of the ring, so
// RadialMenuShows is true then too.
bool DebugWindowOpen();

// controllers.cpp, once a frame, after PlacesPanelFrame: the ray on the
// window, a press of the trigger, the stick, and the picture of the frame.
void DebugWindowFrame();

// controllers.cpp, the pump: true while the trigger of the pointing hand
// belongs to the window. That is while the window is open, and after a
// press that closed it until the trigger goes up.
bool DebugWindowHoldsTrigger();

// controllers.cpp, ControllersMenuRay: true while the window is open.
// meters gets the length of the aim ray to the window, or 0 when the ray is
// not on the window.
bool DebugWindowRay(float* meters);

// controllers.cpp, PumpStick: true while the ray is on the open window.
// Then the stick of the pointing hand forward and back scrolls the list,
// and it does not zoom.
bool DebugWindowTakesStick();

// controllers.cpp: B of the pointing hand and the menu button close the
// window. True when the window was open: then the press does no more.
bool DebugWindowCloseBy(const char* why);

// controllers.cpp, for the word "aimdebug" of the scripted controller: the
// middle of the next part of the test key debug_test_parts, in the LOCAL
// space of OpenXR. False when the window is closed, the list is empty, or
// the part is not in the view.
bool DebugWindowTestTarget(float out[3]);

// Once a report window.
void DebugWindowReport();
