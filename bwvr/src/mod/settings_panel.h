// The settings window and the welcome page. The settings window is the
// debug window (debug_window.cpp) with the settings of the player as its
// rows. The item "settings" of the ring opens it. The welcome page shows one
// time, at the first start of the mod. See settings_panel.cpp,
// docs/controllers.md, "The settings window", and docs/ui-layer.md, "The
// settings window".

#pragma once

// settings_panel=1: page 2 of the ring as made has the item "settings".
bool SettingsPanelOn();

// radial_menu.cpp, at the choice of the item "settings": the window opens
// in front of the head. False when the head has no pose yet.
bool SettingsPanelOpenFromRing();

// The rows of the settings window, for the warm of the window
// (DebugWindowWarm).
struct WindowRows;
const WindowRows* SettingsPanelRows();

// THE WELCOME PAGE. controllers.cpp calls these as it calls the functions
// of the debug window (debug_window.h).

// Once a frame, before DebugWindowFrame: the wait for the first open, the
// ray on the page, a press of the trigger, and the picture of the frame.
void WelcomePageFrame();
// True while the page is open. It uses the quad layer of the ring.
bool WelcomePageOpen();
// True while the trigger of the pointing hand belongs to the page.
bool WelcomePageHoldsTrigger();
// True while the page is open. meters gets the length of the aim ray to the
// page, or 0 when the ray is not on it.
bool WelcomePageRay(float* meters);
// B of the pointing hand and the menu button close the page. True when the
// page was open.
bool WelcomePageCloseBy(const char* why);
// For the word "aimdebug" of the scripted controller while the page is
// open: the middle of the next button of the test key welcome_test_parts
// ("settings play"), in the LOCAL space of OpenXR.
bool WelcomePageTestTarget(float out[3]);

// Once a report window.
void SettingsPanelReport();
