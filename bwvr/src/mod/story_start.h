// A story starts at the head (docs/god-view.md, "A story starts at the
// head"). See story_start.cpp.
#pragma once

#include <stdint.h>

// story_start_at_head, from bwvr.ini. mod_main.cpp sets it before Install.
void StoryStartSetOptions(bool on);
void StoryStartInstall();
// stereo.cpp: once a frame after the frame, the test key (Ctrl, Shift and
// S), and the report.
void StoryStartFrame(uint64_t frame);
void StoryStartToggle(const char* keys);
void StoryStartReport();
