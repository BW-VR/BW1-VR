// The measure of the age of the head pose of a picture (docs/backend.md,
// "The head pose of the frame"). stereo.cpp gives it each frame, after the
// head of the frame: the newest pose of the frame and the pose that the
// camera of the game used. It writes one line in each report window: in how
// many frames the camera took the pose of its own frame, how old that pose
// was at the display time, and how far the newest pose of the frame was from
// it. The step watch of the scale check (view_check.cpp) gives the frame in
// which the eye camera follows a step of the invented head.
//
// It reads and writes nothing of the game.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "head_lag.h"
#include "head_lag_math.h"

extern void ModLog(const char* format, ...);

namespace {

struct Window {
  uint64_t frames;
  uint64_t own;     // the camera took the pose of its own frame
  uint64_t timed;   // frames with a display time
  double age_sum_ms, age_most_ms;
  uint64_t guesses;
  double guess_sum_ms, guess_most_ms;
  double apart_sum_m, apart_most_m;
  double turn_sum_deg, turn_most_deg;
  uint64_t lagged;  // the newest pose was not the pose of the camera
};
Window g_window = {};

}  // namespace

void HeadLagFrame(const HeadLagFrameData& d) {
  Window& w = g_window;
  ++w.frames;
  if (d.located) ++w.own;
  if (d.have_used) {
    const float apart = head_lag_math::Distance3(d.newest_center,
                                                 d.used_center);
    const float turn = head_lag_math::AngleDeg(d.newest_head, d.used_head);
    w.apart_sum_m += apart;
    w.turn_sum_deg += turn;
    if (apart > w.apart_most_m) w.apart_most_m = apart;
    if (turn > w.turn_most_deg) w.turn_most_deg = turn;
    if (apart > 0.0001f || turn > 0.01f) ++w.lagged;
  }
  if (d.have_times && d.display_time_ns > 0 && d.used_time_ns > 0) {
    const double age =
        static_cast<double>(d.display_time_ns - d.used_time_ns) / 1e6;
    ++w.timed;
    w.age_sum_ms += age;
    if (age > w.age_most_ms) w.age_most_ms = age;
  }
  if (d.have_guess) {
    const double off = d.guess_error_ms < 0.0 ? -d.guess_error_ms
                                              : d.guess_error_ms;
    ++w.guesses;
    w.guess_sum_ms += off;
    if (off > w.guess_most_ms) w.guess_most_ms = off;
  }
}

void HeadLagReport(int key, bool headset) {
  Window& w = g_window;
  if (w.frames == 0) return;
  const double n = static_cast<double>(w.frames);
  char age[200] = "";
  if (w.timed > 0) {
    sprintf_s(age,
              " The pose of the camera was for a time %.2f ms before the "
              "display time of its picture in the mean, and %.2f ms at the "
              "most.",
              w.age_sum_ms / static_cast<double>(w.timed), w.age_most_ms);
  }
  char guess[160] = "";
  if (w.guesses > 0) {
    sprintf_s(guess,
              " The guess of the display time was off by %.2f ms in the "
              "mean, and %.2f ms at the most.",
              w.guess_sum_ms / static_cast<double>(w.guesses),
              w.guess_most_ms);
  }
  ModLog("head lag     : head_pose_frame=%d, %s. In %llu frames the camera of "
         "the game took the head pose of its own frame in %llu frames and the "
         "pose of the frame before in %llu.%s%s The newest pose of the frame "
         "was %.2f mm and %.3f degrees from the pose of the camera in the "
         "mean, and %.2f mm and %.3f degrees at the most. %llu frames had a "
         "difference",
         key, headset ? "the headset" : "the invented head", w.frames, w.own,
         w.frames - w.own, age, guess, w.apart_sum_m / n * 1000.0,
         w.turn_sum_deg / n, w.apart_most_m * 1000.0, w.turn_most_deg,
         w.lagged);
  memset(&w, 0, sizeof(w));
}
