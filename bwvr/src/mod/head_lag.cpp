// The measure of the age of the head pose of a picture (docs/backend.md,
// "The head pose of the frame"). stereo.cpp gives it each frame, after the
// head of the frame: the newest pose of the frame, the pose that the camera
// of the game used, and the camera that the game built. It writes:
//
// - one line in each report window: in how many frames the camera took the
//   pose of its own frame, how old that pose was at the display time, and
//   how far the newest pose of the frame was from it,
// - with the invented head of a desktop run, one line for each step of the
//   head (at most kTraceLines in a game): the move and the turn of the
//   camera in the frame of the step and in the 5 frames after it, and the
//   frame in which the camera followed.
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

using head_lag_math::kTraceFrames;

const int kTraceLines = 20;

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

bool g_have_newest = false;
float g_newest_center[3] = {0, 0, 0};
float g_newest_head[4] = {0, 0, 0, 1};

bool g_have_camera = false;
float g_camera[3] = {0, 0, 0};
float g_camera_forward[3] = {0, 0, 1};

struct Trace {
  bool on;
  uint64_t frame;
  int key;
  float step_m, step_deg;
  float before[3], before_forward[3];
  int count;
  float moves[kTraceFrames];
  float turns[kTraceFrames];
};
Trace g_trace = {};
int g_trace_lines = 0;

float ForwardAngleDeg(const float a[3], const float b[3]) {
  float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  if (dot > 1.0f) dot = 1.0f;
  if (dot < -1.0f) dot = -1.0f;
  return acosf(dot) * head_lag_math::kDegrees;
}

void TraceLine(const Trace& t) {
  if (g_trace_lines >= kTraceLines) return;
  ++g_trace_lines;
  char moves[160] = "";
  char turns[160] = "";
  int used_m = 0, used_t = 0;
  for (int i = 0; i < kTraceFrames; ++i) {
    used_m += sprintf_s(moves + used_m, sizeof(moves) - used_m, "%s%.4f",
                        i > 0 ? " " : "", t.moves[i]);
    used_t += sprintf_s(turns + used_t, sizeof(turns) - used_t, "%s%.2f",
                        i > 0 ? " " : "", t.turns[i]);
  }
  const int follow =
      head_lag_math::FollowFrame(t.moves, t.turns, 1e-4f, 0.05f);
  char said[128];
  if (follow < 0) {
    sprintf_s(said, "The camera did not follow in these frames");
  } else if (follow == 0) {
    sprintf_s(said, "The camera followed in the frame of the step");
  } else {
    sprintf_s(said, "The camera followed in frame %llu, %d %s late",
              t.frame + static_cast<uint64_t>(follow), follow,
              follow == 1 ? "frame" : "frames");
  }
  ModLog("head lag     : the invented head moved %.1f cm and turned %.1f "
         "degrees in frame %llu (head_pose_frame=%d). From frame %llu to "
         "frame %llu the camera of the game moved %s units and turned %s "
         "degrees from its place in the frame before the step. %s",
         t.step_m * 100.0f, t.step_deg, t.frame, t.key, t.frame,
         t.frame + kTraceFrames - 1, moves, turns, said);
}

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

  // THE TRACE OF A STEP OF THE INVENTED HEAD. The newest pose of this frame
  // against the newest pose of the frame before.
  if (g_trace.on && d.have_camera) {
    Trace& t = g_trace;
    t.moves[t.count] = head_lag_math::Distance3(d.camera, t.before);
    t.turns[t.count] = ForwardAngleDeg(d.camera_forward, t.before_forward);
    if (++t.count == kTraceFrames) {
      TraceLine(t);
      t.on = false;
    }
  }
  if (!d.headset && !g_trace.on && g_have_newest && g_have_camera &&
      d.have_camera && g_trace_lines < kTraceLines) {
    const float moved =
        head_lag_math::Distance3(d.newest_center, g_newest_center);
    const float turned =
        head_lag_math::AngleDeg(d.newest_head, g_newest_head);
    if (head_lag_math::IsStep(moved, turned)) {
      Trace& t = g_trace;
      memset(&t, 0, sizeof(t));
      t.on = true;
      t.frame = d.frame;
      t.key = d.key;
      t.step_m = moved;
      t.step_deg = turned;
      memcpy(t.before, g_camera, sizeof(t.before));
      memcpy(t.before_forward, g_camera_forward, sizeof(t.before_forward));
      t.moves[0] = head_lag_math::Distance3(d.camera, t.before);
      t.turns[0] = ForwardAngleDeg(d.camera_forward, t.before_forward);
      t.count = 1;
    }
  }
  memcpy(g_newest_center, d.newest_center, sizeof(g_newest_center));
  memcpy(g_newest_head, d.newest_head, sizeof(g_newest_head));
  g_have_newest = true;
  if (d.have_camera) {
    memcpy(g_camera, d.camera, sizeof(g_camera));
    memcpy(g_camera_forward, d.camera_forward, sizeof(g_camera_forward));
    g_have_camera = true;
  } else {
    g_have_camera = false;
  }
}

// After a cut (a load, a menu, the intro), the next frame starts no trace.
void HeadLagCut() {
  g_have_newest = false;
  g_have_camera = false;
  g_trace.on = false;
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
