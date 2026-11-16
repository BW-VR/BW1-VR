// The sheet of the gestures over the palm of the hand that does not point:
// the gesture pictures of the game for each miracle that the Worship Sites
// offer (gesture_sheet.cpp, docs/miracles.md, "The sheet of the gestures").
#pragma once

// controllers.cpp, once a frame, after GesturePanelFrame.
void GestureSheetFrame();
// gesture_panel.cpp: true when the sheet takes the palm that turns up at
// rest (gesture_sheet=1, and the pictures of the game are there). Then the
// row of the gestures shows only while the player draws a gesture or a chain
// goes on.
bool GestureSheetOn();
// The switch of the debug page (debug_menu.cpp): gesture_sheet.
int GestureSheetValue();
void GestureSheetChange(const char* by);
// controllers.cpp, once a report window.
void GestureSheetReport();
