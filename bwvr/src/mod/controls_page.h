// The page Controls of the settings window: a drawing of the controllers of
// the player, a row for each button with its job, and a row for each control
// that stays fixed. The arrows of a row give its button a different job of
// the same controller. See controls_page.cpp, docs/controllers.md, "The map
// of the buttons", and docs/ui-layer.md, "The page Controls".
//
// settings_panel.cpp puts the rows of the page after its own rows, as the
// group Controls. k is the row of the page, from 0. The functions have the
// meaning of the functions of WindowRows (debug_window.h).

#pragma once

#include <stdint.h>

// controls_page=1: the settings window has the tab Controls.
bool ControlsPageOn();

int ControlsRowCount();
bool ControlsRowIn(int k);
const wchar_t* ControlsRowLabel(int k);
const wchar_t* ControlsRowName(int k);
const wchar_t* ControlsRowTip(int k);
void ControlsRowLastLine(int k, wchar_t* out, int size);
int ControlsRowValue(int k);
int ControlsRowValueCount(int k);
void ControlsRowValueText(int k, int value, wchar_t* out, int size);
bool ControlsRowStep(int k, bool back);
const uint8_t* ControlsRowPicture(int k, int value, int style);
const wchar_t* ControlsRowFixedWord(int k);

// The row of the page under the ray, or -1: the drawing lights its control.
void ControlsSetHoverRow(int k);

// Once a report window.
void ControlsPageReport();
