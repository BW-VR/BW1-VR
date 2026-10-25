// THE TIME LOG OF THE MOD (time_log, docs/backend.md, "The time log of the
// mod"). Each 10 s one line in bwvr.log with the mean and the most of the
// parts of a frame of the game: the frame from one present to the next, the
// scene walk, the write of the live stream, the wait of the pace and the
// rest. All calls are on the frame thread.

#pragma once

#include <stdint.h>

// mod_main.cpp, at the start: time_log=1 (the default) is on, 0 is off (no
// clock reads and no line).
void TimeLogSetOn(bool on);
bool TimeLogOn();
// stereo.cpp: a scene walk of an eye pass starts and ends.
void TimeLogWalk(bool begin);
// h_FinishFrame, after the FinishFrame of the game: the present of a frame.
// Each 10 s it writes the line.
void TimeLogPresent();
// vk_stream.cpp: the write of a frame into the ring of the live stream took
// `ms`. xr_input.cpp: the wait of the pace took `ms`.
void TimeLogStream(double ms);
void TimeLogPace(double ms);
