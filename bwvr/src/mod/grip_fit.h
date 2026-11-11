// The frame of the controller on the grip pose of the runtime, and the trim
// of the player. See grip_fit.cpp, grip_fit_math.h and
// docs/controller-hints.md, "9.15 The place of the controller".

#pragma once

#include <stdint.h>

#include "../vr/xr_screen.h"

// Once a frame, from ControllersFrame, after the hands of the frame are read
// and before any code uses them. The grip pose of each hand becomes the
// frame of the model of its controller (grip_fit), and the trim of the
// player moves the grip pose and the aim pose (grip_trim_*_mm). fake is
// true for the hands of the scripted controller.
void GripFitFrame(bwvr::HandInput hands[2], bool fake, uint64_t frame);

// The switch of the debug page for grip_fit: 1 the table of the fits, 0 the
// grip pose of the runtime.
int GripFitValue();
void GripFitChange(const char* by);

// The settings window (settings_panel.cpp): a trim, in mm, at once. axis is
// 0 ahead, 1 up, 2 out.
void GripFitSetTrim(int axis, int mm, const char* by);
