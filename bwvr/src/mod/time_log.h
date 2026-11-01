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

// THE LONG FRAMES (slow_frame_ms, long_frame_math.h, docs/android.md, "The
// long frames"). mod_main.cpp, after TimeLogSetOn: 0 (the default) is off,
// N gives a line for each frame longer than N ms. It needs time_log=1. cpu
// (slow_frame_cpu=1, 0 by default): the line has the CPU time of the frame
// thread from one call of GetThreadTimes at each present.
void TimeLogSetSlow(int slow_frame_ms, bool cpu);
// True with slow_frame_ms above 0 and time_log=1. The callers of the calls
// below read it first, so with slow_frame_ms=0 they make no call.
extern bool g_time_log_slow;
// frame_timers.cpp and pass_guard.cpp: a call of the frame loop of the game
// (long_frame_math::Call) starts (true) or ends (false).
void TimeLogCall(int call, bool begin);
