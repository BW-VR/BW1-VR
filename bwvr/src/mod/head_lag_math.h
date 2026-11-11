// The age of the head pose of a picture (docs/backend.md, "The head pose of
// the frame"), and the move of a kept intro picture for a turn of the head
// (docs/intro.md, "The trails and a turn of the head"). Pure math with no
// game and no runtime in it. stereo.cpp, head_lag.cpp and intro_vr.cpp use
// it, and test/controller_math checks it.
//
// Quaternions are x y z w, in the right handed frame of the runtime: +X
// right, +Y up, -Z forward.
#pragma once

#include <math.h>
#include <stdint.h>

namespace head_lag_math {

const float kDegrees = 57.29578f;

// The angle between two orientations, in degrees, 0 to 180.
inline float AngleDeg(const float a[4], const float b[4]) {
  float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
  if (dot < 0.0f) dot = -dot;
  if (dot > 1.0f) dot = 1.0f;
  return 2.0f * acosf(dot) * kDegrees;
}

inline float Distance3(const float a[3], const float b[3]) {
  const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
  return sqrtf(x * x + y * y + z * z);
}

// THE STEP OF THE DISPLAY TIMES. The runtime gives a display period, but the
// display times of two frames can be farther apart (a missed frame, or a
// runtime with its own clock, as Virtual Desktop). The mean step is a mean
// of the last steps with a weight of 1/8 for the newest. A step goes into it
// clamped to 1 to 3 display periods, so one long stall does not move it far.
// mean_ns 0 means no step yet.
inline double NextMeanStep(double mean_ns, int64_t step_ns, int64_t period_ns) {
  if (period_ns <= 0 || step_ns <= 0) return mean_ns;
  double step = static_cast<double>(step_ns);
  const double least = static_cast<double>(period_ns);
  const double most = 3.0 * least;
  if (step < least) step = least;
  if (step > most) step = most;
  if (!(mean_ns > 0.0)) return step;
  return mean_ns + (step - mean_ns) / 8.0;
}

// The display time that the next frame will get: the last display time and
// one mean step. With no mean, one display period.
inline int64_t NextDisplayTime(int64_t last_ns, double mean_ns,
                               int64_t period_ns) {
  const double step =
      mean_ns > 0.0 ? mean_ns : static_cast<double>(period_ns);
  return last_ns + static_cast<int64_t>(step + 0.5);
}

// A STEP OF THE INVENTED HEAD: a move of 2 cm or more, or a turn of 2
// degrees or more, from one frame to the next. A desktop run steps the head
// by 14 cm or by 30 degrees in one frame.
const float kStepM = 0.02f;
const float kStepDeg = 2.0f;
inline bool IsStep(float moved_m, float turned_deg) {
  return moved_m >= kStepM || turned_deg >= kStepDeg;
}

// The frames that the trace of a step keeps.
const int kTraceFrames = 6;

// THE FRAME IN WHICH THE CAMERA FOLLOWED A STEP. moves and turns are the
// move and the turn of the camera from the frame before the step, in the
// frame of the step and in the frames after it. The camera followed in the
// first frame that has half or more of the move and of the turn of the last
// frame. -1 when the last frame has no move and no turn.
inline int FollowFrame(const float moves[kTraceFrames],
                       const float turns[kTraceFrames], float least_move,
                       float least_turn_deg) {
  const float move_end = moves[kTraceFrames - 1];
  const float turn_end = turns[kTraceFrames - 1];
  const bool moved = move_end >= least_move;
  const bool turned = turn_end >= least_turn_deg;
  if (!moved && !turned) return -1;
  for (int i = 0; i < kTraceFrames; ++i) {
    const bool move_ok = !moved || moves[i] >= 0.5f * move_end;
    const bool turn_ok = !turned || turns[i] >= 0.5f * turn_end;
    if (move_ok && turn_ok) return i;
  }
  return kTraceFrames - 1;
}

// Turns v by the quaternion q.
inline void Rotate(const float q[4], const float v[3], float out[3]) {
  // t = 2 * cross(q.xyz, v), out = v + w * t + cross(q.xyz, t)
  const float tx = 2.0f * (q[1] * v[2] - q[2] * v[1]);
  const float ty = 2.0f * (q[2] * v[0] - q[0] * v[2]);
  const float tz = 2.0f * (q[0] * v[1] - q[1] * v[0]);
  out[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
  out[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
  out[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
}

// THE MOVE OF A KEPT PICTURE FOR A TURN OF THE HEAD.
//
// A kept picture was drawn with the view kept_view. The new picture is drawn
// with the view now_view, over the kept one. The middle of the new picture
// looks at the place that the kept picture has at (*x, *y), in units of the
// half width and the half height of the picture, x to the right and y up.
// So a pixel of the new picture takes the pixel of the kept picture that is
// that far to the right and up from it. inv_half_tan_x and inv_half_tan_y
// are the terms of the projection of the game (1 over the tangent of the
// half angle). A pure turn moves each pixel by the same amount at the
// middle. Away from the middle, and for a roll, the move is not the same,
// and this gives the move of the middle only. False when the middle of the
// new view looks behind the kept view, or the terms are not valid.
inline bool KeptPictureShift(const float kept_view[4], const float now_view[4],
                             float inv_half_tan_x, float inv_half_tan_y,
                             float* x, float* y) {
  *x = 0.0f;
  *y = 0.0f;
  if (!(inv_half_tan_x > 0.0f) || !(inv_half_tan_y > 0.0f)) return false;
  const float forward[3] = {0.0f, 0.0f, -1.0f};
  float world[3];
  Rotate(now_view, forward, world);
  const float kept_inverse[4] = {-kept_view[0], -kept_view[1], -kept_view[2],
                                 kept_view[3]};
  float in_kept[3];
  Rotate(kept_inverse, world, in_kept);
  const float depth = -in_kept[2];
  if (!(depth > 0.05f)) return false;
  *x = inv_half_tan_x * in_kept[0] / depth;
  *y = inv_half_tan_y * in_kept[1] / depth;
  return true;
}

// The same move in whole pixels of a picture of width by height, x to the
// right and y down, as the copies of the picture count them.
inline void ShiftPixels(float x, float y, uint32_t width, uint32_t height,
                        int* px, int* py) {
  const float fx = x * 0.5f * static_cast<float>(width);
  const float fy = -y * 0.5f * static_cast<float>(height);
  *px = static_cast<int>(fx >= 0.0f ? fx + 0.5f : fx - 0.5f);
  *py = static_cast<int>(fy >= 0.0f ? fy + 0.5f : fy - 0.5f);
}

// THE COPY OF A MOVED PICTURE. The pixel (u, v) of the new picture takes the
// pixel (u + px, v + py) of the kept one. The rectangle of the source and
// the one of the target, as a left, a top and a size. False when nothing of
// the kept picture is in the new one.
struct CopyRect {
  int src_x, src_y, dst_x, dst_y;
  uint32_t width, height;
};
inline bool ShiftCopy(int px, int py, uint32_t width, uint32_t height,
                      CopyRect* out) {
  const int w = static_cast<int>(width);
  const int h = static_cast<int>(height);
  const int ax = px < 0 ? -px : px;
  const int ay = py < 0 ? -py : py;
  if (ax >= w || ay >= h) return false;
  out->src_x = px > 0 ? px : 0;
  out->src_y = py > 0 ? py : 0;
  out->dst_x = px < 0 ? -px : 0;
  out->dst_y = py < 0 ? -py : 0;
  out->width = static_cast<uint32_t>(w - ax);
  out->height = static_cast<uint32_t>(h - ay);
  return true;
}

}  // namespace head_lag_math
