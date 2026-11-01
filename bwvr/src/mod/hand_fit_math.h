// THE FIT OF THE HAND ON THE CONTROLLER (docs/hand-model.md, "The fit of the
// hand on the controller"), as pure functions with no game state.
//
// Each runtime and each controller puts the grip pose at a different place
// in the hand. The player moves and turns the hand of the mod against the
// grip pose until it covers the real hand. controllers.cpp applies the fit
// to the grip pose of each hand once a frame, as it reads the hands, so each
// reader of the grip pose (the Hand, its point, the hands of the overlay,
// the cards at the wrists) gets the same hand. The aim pose does not change.
//
// The grip frame of OpenXR: +x to the right, +y up, -z forward along the
// controller, for both hands. The values are for the right hand. The left
// hand gets the mirror image (x changes sign, and so do the yaw and the
// roll), so one set of values fits both hands.

#pragma once

#include <math.h>

namespace hand_fit_math {

struct HandFit {
  float out_mm;      // + to the back of the hand: right on the right hand
  float up_mm;       // + up
  float forward_mm;  // + forward, along the fingers of a closed hand
  float pitch_deg;   // + turns the front of the hand up
  float yaw_deg;     // + turns the front of the hand in, to the other hand
  float roll_deg;    // + turns the top of the hand in, to the other hand
};

inline bool IsZero(const HandFit& fit) {
  return fit.out_mm == 0.0f && fit.up_mm == 0.0f && fit.forward_mm == 0.0f &&
         fit.pitch_deg == 0.0f && fit.yaw_deg == 0.0f && fit.roll_deg == 0.0f;
}

// out = a * b, quaternions as x y z w.
inline void QuatMultiply(const float a[4], const float b[4], float out[4]) {
  const float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
  const float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
  const float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
  const float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
  out[0] = x;
  out[1] = y;
  out[2] = z;
  out[3] = w;
}

// v turned by the unit quaternion q.
inline void QuatRotate(const float q[4], const float v[3], float out[3]) {
  const float tx = 2.0f * (q[1] * v[2] - q[2] * v[1]);
  const float ty = 2.0f * (q[2] * v[0] - q[0] * v[2]);
  const float tz = 2.0f * (q[0] * v[1] - q[1] * v[0]);
  out[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
  out[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
  out[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
}

// A turn of `deg` degrees about the unit axis.
inline void AxisTurn(float ax, float ay, float az, float deg, float out[4]) {
  const float half = deg * 3.14159265f / 360.0f;
  const float s = sinf(half);
  out[0] = ax * s;
  out[1] = ay * s;
  out[2] = az * s;
  out[3] = cosf(half);
}

// The turn of the fit in the grip frame of one hand:
// Ry(yaw) * Rx(pitch) * Rz(roll). The yaw comes last, so it turns the line
// that the pitch put up or down. On the right hand a turn in is a plus turn
// about +y (the front goes to -x) and about +z (the top goes to -x). The left
// hand is the mirror image, so its yaw and roll turn the other way.
inline void FitTurn(const HandFit& fit, bool left, float out[4]) {
  const float side = left ? -1.0f : 1.0f;
  float yaw[4], pitch[4], roll[4], yaw_pitch[4];
  AxisTurn(0.0f, 1.0f, 0.0f, fit.yaw_deg * side, yaw);
  AxisTurn(1.0f, 0.0f, 0.0f, fit.pitch_deg, pitch);
  AxisTurn(0.0f, 0.0f, 1.0f, fit.roll_deg * side, roll);
  QuatMultiply(yaw, pitch, yaw_pitch);
  QuatMultiply(yaw_pitch, roll, out);
}

// The offset of the fit in the grip frame of one hand, in meters.
inline void FitOffset(const HandFit& fit, bool left, float out[3]) {
  out[0] = fit.out_mm * 0.001f * (left ? -1.0f : 1.0f);
  out[1] = fit.up_mm * 0.001f;
  out[2] = -fit.forward_mm * 0.001f;
}

// The grip pose with the fit: the grip point moves by the offset in the grip
// frame, and the hand turns about that new point. So a turn does not move
// the place that the offset chose.
inline void ApplyHandFit(const HandFit& fit, bool left, float position[3],
                         float orientation[4]) {
  float offset[3], moved[3], turn[4], turned[4];
  FitOffset(fit, left, offset);
  QuatRotate(orientation, offset, moved);
  for (int i = 0; i < 3; ++i) position[i] += moved[i];
  FitTurn(fit, left, turn);
  QuatMultiply(orientation, turn, turned);
  const float length = sqrtf(turned[0] * turned[0] + turned[1] * turned[1] +
                             turned[2] * turned[2] + turned[3] * turned[3]);
  for (int i = 0; i < 4; ++i) {
    orientation[i] = length > 1e-6f ? turned[i] / length : (i == 3 ? 1.0f : 0.0f);
  }
}

}  // namespace hand_fit_math
