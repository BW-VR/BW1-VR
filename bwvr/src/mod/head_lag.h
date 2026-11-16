// The measure of the age of the head pose of a picture. See head_lag.cpp.
#pragma once

#include <stdint.h>

struct HeadLagFrameData {
  bool headset;  // false for the invented head
  // The newest head pose of the frame: the pose of BeginFrame in a headset,
  // the invented head now on the desktop. Meters and a quaternion, LOCAL.
  float newest_center[3];
  float newest_head[4];
  // The pose that the camera of the game used in this frame.
  bool have_used;
  float used_center[3];
  float used_head[4];
  bool located;  // the camera took the pose of its own frame
  // In a headset: the display time of the frame, and the time of the pose of
  // the camera, in nanoseconds.
  bool have_times;
  int64_t display_time_ns;
  int64_t used_time_ns;
  // The display time of the frame less the time that the camera call
  // expected, in milliseconds.
  bool have_guess;
  double guess_error_ms;
};

void HeadLagFrame(const HeadLagFrameData& data);
void HeadLagReport(int key, bool headset);
