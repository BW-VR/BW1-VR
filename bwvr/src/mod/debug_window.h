// The debug window: the switches of the debug page (debug_menu.cpp) in a
// window of parchment, fixed in the room in front of the player. The item
// "debug" of the ring opens it with debug_window=1, and the ray and the
// trigger of the pointing hand change its switches. The settings window
// (settings_panel.cpp) is the same window with the settings of the player. See debug_window.cpp,
// docs/controllers.md, "The debug window", and docs/ui-layer.md, "The debug
// window".

#pragma once

#include <stdint.h>

// THE ROWS of the window. The window shows one list at a time: the switches
// of the debug page (debug_menu.cpp), or the settings of the player
// (settings_panel.cpp). The layout, the parts, the bake and the closes are
// the same for each list.
struct WindowRows {
  int slot;              // 0 the debug switches, 1 the settings: the copies
                         // of the parts of each list are kept apart
  const char* tag;       // the start of each log line, 13 letters
  const wchar_t* title;  // at the top of the window
  const wchar_t* idle;   // the bottom row while no change shows
  int (*groups)();
  const wchar_t* (*group_name)(int group);
  int (*count)();
  int (*group)(int i);
  bool (*in_window)(int i);
  const wchar_t* (*label)(int i);  // the label of the row
  const wchar_t* (*name)(int i);   // the first line of the note
  const wchar_t* (*tip)(int i);    // what it does
  // The last line of the note: the ini key, and when a change acts.
  void (*last_line)(int i, wchar_t* out, int size);
  int (*value)(int i);  // the value now, or kDebugCannot
  int (*value_count)(int i);  // the words of the values, 0 for a number
  void (*value_text)(int i, int value, wchar_t* out, int size);
  bool (*step)(int i, bool back);  // false when it cannot change now
  // Small words under the label of the row, for example "at the next
  // start", or null. The bottom row adds them after a change.
  const wchar_t* (*row_note)(int i);
  // Null, or for a row whose value_count is -1: its band of a drawing,
  // kWellWidth by kRowPitch pixels (debug_window_math.h), BGRA with the
  // color multiplied by the alpha, in the look `style` (hint_card_style). The
  // window lays it over the paper of the row. A new value draws the row
  // again.
  const uint8_t* (*picture)(int i, int value, int style);
  // Null, or the word of a row that cannot change (value kDebugCannot), in
  // the place of "not now": the job of a control that stays fixed.
  const wchar_t* (*fixed_word)(int i);
};
const int kWindowSlots = 2;

// radial_menu.cpp, at the choice of the item "debug": the window opens in
// front of the head with the debug switches. False when the head has no
// pose yet.
bool DebugWindowOpenFromRing();

// The window opens in front of the head with the rows of `rows`. An open
// window changes to those rows. False when the head has no pose yet.
bool DebugWindowOpenWith(const WindowRows* rows);

// The rows that show now, or null when the window is closed.
const WindowRows* DebugWindowRows();

// The switch of the row under the ray, or -1. For a picture that lights the
// part of a row (controls_page.cpp).
int DebugWindowHoverSwitch();

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
