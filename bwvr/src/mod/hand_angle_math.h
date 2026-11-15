// The angle of the drawn hand on the controller, as pure functions with no
// game state. grip_fit.cpp calls these with the poses of the runtime, and
// test/controller_math calls them with poses that it makes up
// (hand_angle_checks.h). See docs/hand.md, "The angle of the hand".
//
// A pose is in the frame OpenXR uses: x right, y up and -z forward. A
// quaternion is x y z w. The hand frame of overlay_math.h is the frame of a
// pose with z the other way: +x is the back of the right hand, -y is the open
// fingers, and -z of the pose goes from the little finger to the index
// finger.
//
// THE PALM OF THE RUNTIME. The grip pose of OpenXR has -z through the tube
// of the closed fingers, from the little finger to the thumb. A real hand
// does not hold a handle at a right angle to its fingers: the handle lies
// across the palm at a slant, and the open fingers lean toward the thumb end
// of the handle. The palm pose of the Oculus runtime (XR_EXT_palm_pose) gives
// the real hand: -z of the palm pose goes along the straight index finger,
// and +x is the normal of the palm. Virtual Desktop has the constants of the
// Oculus runtime (rebindControllerActions in action.cpp of
// VirtualDesktop-OpenXR). From them the palm pose is the grip pose turned 70
// degrees about x, so the open fingers are 20 degrees from -y of the grip,
// toward the thumb end of the handle, and the palm normal is x of the grip.
//
// THE FRAME OF THE MODEL. The fit of the controller model (grip_fit_math.h)
// turns the grip pose 17.9 degrees about -x for the Touch Plus: the other
// way. The hand was on that frame, so its fingers were 37.9 degrees from the
// fingers of the palm pose, about the palm normal, toward the little finger.
//
// THE ANGLE OF THE HAND (hand_angle=1) is the palm pose of the runtime: the
// grip pose of the runtime turned 20 degrees about x. The hand turns about
// the grip point of the frame of the model, so the knuckles stay at the
// handle. The controller model stays on the frame of the model.
//
// THE TRIM of the player turns the hand after that, in the hand frame:
//
//   yaw    about the palm normal. Plus turns the fingers toward the thumb
//   pitch  about the line of the knuckles. Plus turns the fingers toward the
//          palm
//   roll   about the fingers. Plus turns the palm toward the thumb
//
// The left hand is the mirror image, so a plus trim turns it the same way
// for the hand.

#pragma once

#include <math.h>

#include "controller_math.h"

namespace hand_angle_math {

const float kPi = 3.14159265f;

// The turn from the grip pose to the palm pose of the Oculus runtime, about
// x, in degrees: the palm pose has its fingers along -z, the hand frame along
// -y, and 90 - 70 is 20.
const float kPalmTurnDeg = 20.0f;

// The trim of the player, in steps of 5 degrees, from -45 to 45 degrees.
const int kTrimStepDeg = 5;
const int kTrimLimitDeg = 45;

struct Trim {
  int yaw_deg;
  int pitch_deg;
  int roll_deg;
};

inline int ClampTrim(int deg) {
  if (deg > kTrimLimitDeg) return kTrimLimitDeg;
  if (deg < -kTrimLimitDeg) return -kTrimLimitDeg;
  return deg;
}

inline bool TrimZero(const Trim& t) {
  return t.yaw_deg == 0 && t.pitch_deg == 0 && t.roll_deg == 0;
}

// A turn of deg degrees about the axis 0 (x), 1 (y) or 2 (z).
inline void AxisTurn(int axis, float deg, float q[4]) {
  const float half = 0.5f * deg * kPi / 180.0f;
  q[0] = q[1] = q[2] = 0.0f;
  q[axis] = sinf(half);
  q[3] = cosf(half);
}

inline void Normalize(float q[4]) {
  const float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (!(n > 1e-6f)) return;
  for (int i = 0; i < 4; ++i) q[i] /= n;
}

// The turn of the trim in the frame of the pose. The palm normal is x and
// the fingers are -y for both hands. The thumb end of the handle is -z.
// The palm of the right hand faces -x, and the palm of the left hand +x.
inline void TrimTurn(const Trim& t, bool left, float q[4]) {
  const float side = left ? 1.0f : -1.0f;
  float yaw[4], pitch[4], roll[4], both[4];
  AxisTurn(0, static_cast<float>(t.yaw_deg), yaw);
  AxisTurn(2, side * static_cast<float>(t.pitch_deg), pitch);
  AxisTurn(1, side * static_cast<float>(t.roll_deg), roll);
  controller_math::QuatMultiply(yaw, pitch, both);
  controller_math::QuatMultiply(both, roll, q);
  Normalize(q);
}

// The orientation of the hand. runtime_q is the grip pose of the runtime,
// frame_q the frame of the controller model (the grip pose after the fit).
// palm 1 gives the palm pose of the runtime, 0 the frame of the model. The
// trim comes after.
inline void HandOrientation(const float runtime_q[4], const float frame_q[4],
                            bool palm, const Trim& t, bool left,
                            float out[4]) {
  float base[4];
  if (palm) {
    float turn[4];
    AxisTurn(0, kPalmTurnDeg, turn);
    controller_math::QuatMultiply(runtime_q, turn, base);
  } else {
    for (int i = 0; i < 4; ++i) base[i] = frame_q[i];
  }
  float trim[4];
  TrimTurn(t, left, trim);
  controller_math::QuatMultiply(base, trim, out);
  Normalize(out);
}

// The turn from the orientation a to the orientation b, in the frame of a:
// its angle in degrees and its axis (a unit vector, or 0 for no turn).
inline float TurnBetween(const float a[4], const float b[4], float axis[3]) {
  const float back[4] = {-a[0], -a[1], -a[2], a[3]};
  float r[4];
  controller_math::QuatMultiply(back, b, r);
  if (r[3] < 0.0f) {
    for (int i = 0; i < 4; ++i) r[i] = -r[i];
  }
  const float s = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
  for (int i = 0; i < 3; ++i) axis[i] = s > 1e-7f ? r[i] / s : 0.0f;
  float w = r[3];
  if (w > 1.0f) w = 1.0f;
  return 2.0f * acosf(w) * 180.0f / kPi;
}

// The angle between two unit vectors, in degrees.
inline float AngleOf(const float a[3], const float b[3]) {
  float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  if (d > 1.0f) d = 1.0f;
  if (d < -1.0f) d = -1.0f;
  return acosf(d) * 180.0f / kPi;
}

// The open fingers and the palm normal (x) of an orientation.
inline void Fingers(const float q[4], float out[3]) {
  const float minus_y[3] = {0.0f, -1.0f, 0.0f};
  controller_math::QuatRotate(q, minus_y, out);
}
inline void PalmAxis(const float q[4], float out[3]) {
  const float x[3] = {1.0f, 0.0f, 0.0f};
  controller_math::QuatRotate(q, x, out);
}

}  // namespace hand_angle_math
