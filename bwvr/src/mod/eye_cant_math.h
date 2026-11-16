// Canted views (docs/backend.md, "Canted views"). Pure math with no game
// and no runtime in it. stereo.cpp, intro_vr.cpp and head_pose.cpp use it,
// and test/controller_math checks it.
//
// THE CANT. Some headsets (Pimax) turn the view of each eye outward from the
// head by some degrees: the runtime gives each view its own orientation. On
// a Quest the two orientations are equal.
//
// THE HEAD. The camera of the engine and the Hand take one head for the
// frame: the mean of the two view orientations. Each eye pass then turns its
// camera from that head by the turn of its own view (Relative), in the per
// eye matrix with the roll (EyeMatrix). So each eye picture is drawn from
// the orientation that the projection layer declares for it.
//
// Quaternions are x y z w, in the right handed frame of the runtime: +X
// right, +Y up, -Z forward. Matrices are 3x3, row major, in the left handed
// camera frame of the game (+Z forward), with the axes in the columns, as
// head_pose.cpp builds them.
#pragma once

#include <math.h>

namespace eye_cant {

// Below this angle a view is not canted, and the eye passes stay as they are
// with no cant. Two equal orientations give a relative angle of about 1e-5
// degrees from the rounding of floats.
const float kCantedDeg = 0.1f;

const float kDegrees = 57.29578f;

// b then a: the result applies a first, as head_pose.cpp multiplies.
inline void Multiply(const float b[4], const float a[4], float out[4]) {
  const float w = b[3] * a[3] - b[0] * a[0] - b[1] * a[1] - b[2] * a[2];
  const float x = b[3] * a[0] + b[0] * a[3] + b[1] * a[2] - b[2] * a[1];
  const float y = b[3] * a[1] - b[0] * a[2] + b[1] * a[3] + b[2] * a[0];
  const float z = b[3] * a[2] + b[0] * a[1] - b[1] * a[0] + b[2] * a[3];
  out[0] = x;
  out[1] = y;
  out[2] = z;
  out[3] = w;
}

inline void Conjugate(const float q[4], float out[4]) {
  out[0] = -q[0];
  out[1] = -q[1];
  out[2] = -q[2];
  out[3] = q[3];
}

inline void Normalize(float q[4]) {
  const float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (n < 1e-6f) {
    q[0] = q[1] = q[2] = 0.0f;
    q[3] = 1.0f;
    return;
  }
  for (int i = 0; i < 4; ++i) q[i] /= n;
}

inline void Rotate(const float q[4], const float v[3], float out[3]) {
  const float tx = 2.0f * (q[1] * v[2] - q[2] * v[1]);
  const float ty = 2.0f * (q[2] * v[0] - q[0] * v[2]);
  const float tz = 2.0f * (q[0] * v[1] - q[1] * v[0]);
  out[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
  out[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
  out[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
}

// The orientation half way between two orientations. q and -q are the same
// orientation, so b goes to the side of a first. At one half the normalized
// sum is the same as the spherical mean.
inline void Mean(const float a[4], const float b[4], float out[4]) {
  const float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
  const float sign = dot < 0.0f ? -1.0f : 1.0f;
  for (int i = 0; i < 4; ++i) out[i] = a[i] + sign * b[i];
  Normalize(out);
}

// The turn of an eye from the head, in the frame of the head:
// eye = head * relative.
inline void Relative(const float head[4], const float eye[4], float out[4]) {
  float inverse[4];
  Conjugate(head, inverse);
  Multiply(inverse, eye, out);
  Normalize(out);
}

// The whole angle of a turn, in degrees.
inline float AngleDeg(const float q[4]) {
  float w = q[3] < 0.0f ? -q[3] : q[3];
  if (w > 1.0f) w = 1.0f;
  return 2.0f * acosf(w) * kDegrees;
}

// A turn as yaw, pitch and roll in degrees, in the order of a neck (yaw about
// up, then pitch about right, then roll about forward), as the invented head
// of head_pose.cpp builds it. A positive yaw is to the left, a positive pitch
// is up, and a positive roll tilts the head to the right.
inline void Angles(const float q[4], float* yaw, float* pitch, float* roll) {
  const float back_forward[3] = {0.0f, 0.0f, -1.0f};
  const float local_up[3] = {0.0f, 1.0f, 0.0f};
  float f[3], u[3];
  Rotate(q, back_forward, f);
  Rotate(q, local_up, u);
  float s = f[1];
  if (s > 1.0f) s = 1.0f;
  if (s < -1.0f) s = -1.0f;
  const float y = atan2f(-f[0], -f[2]);
  const float p = asinf(s);
  *yaw = y * kDegrees;
  *pitch = p * kDegrees;
  // The right and the up of the yaw and the pitch alone. For the forward
  // (-sin y cos p, sin p, -cos y cos p) they are (cos y, 0, -sin y) and
  // (sin y sin p, cos p, cos y sin p). The roll is the angle of the real up
  // from that up, about the forward axis.
  const float right0[3] = {cosf(y), 0.0f, -sinf(y)};
  const float up_level[3] = {sinf(y) * sinf(p), cosf(p), cosf(y) * sinf(p)};
  const float along_right =
      u[0] * right0[0] + u[1] * right0[1] + u[2] * right0[2];
  const float along_up =
      u[0] * up_level[0] + u[1] * up_level[1] + u[2] * up_level[2];
  *roll = atan2f(along_right, along_up) * kDegrees;
}

// The turn of a view in the invented head of a desktop run: eye 0 turns to
// the left and eye 1 to the right, each by `degrees`, about the up axis of
// the head.
inline void FakeTurn(int eye, float degrees, float out[4]) {
  const float half = 0.5f * (eye == 0 ? degrees : -degrees) / kDegrees;
  out[0] = 0.0f;
  out[1] = sinf(half);
  out[2] = 0.0f;
  out[3] = cosf(half);
}

// The rotation of a quaternion as a matrix in the left handed camera frame
// of the game: the matrix of head_pose.cpp (ToMatrix, then FlipHandedness).
inline void CameraMatrix(const float q[4], float m[9]) {
  const float xx = q[0] * q[0], yy = q[1] * q[1], zz = q[2] * q[2];
  const float xy = q[0] * q[1], xz = q[0] * q[2], yz = q[1] * q[2];
  const float wx = q[3] * q[0], wy = q[3] * q[1], wz = q[3] * q[2];
  m[0] = 1.0f - 2.0f * (yy + zz);
  m[1] = 2.0f * (xy - wz);
  m[2] = -(2.0f * (xz + wy));
  m[3] = 2.0f * (xy + wz);
  m[4] = 1.0f - 2.0f * (xx + zz);
  m[5] = -(2.0f * (yz - wx));
  m[6] = -(2.0f * (xz - wy));
  m[7] = -(2.0f * (yz + wx));
  m[8] = 1.0f - 2.0f * (xx + yy);
}

inline void MatMul(const float a[9], const float b[9], float out[9]) {
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      out[r * 3 + c] = a[r * 3 + 0] * b[0 * 3 + c] +
                       a[r * 3 + 1] * b[1 * 3 + c] +
                       a[r * 3 + 2] * b[2 * 3 + c];
    }
  }
}

// The roll of the per eye matrix of stereo.cpp: a rotation about the forward
// axis of the camera, in the row vector convention of the camera matrix.
inline void RollMatrix(float roll, float m[9]) {
  const float c = cosf(roll), s = sinf(roll);
  m[0] = c;
  m[1] = s;
  m[2] = 0.0f;
  m[3] = -s;
  m[4] = c;
  m[5] = 0.0f;
  m[6] = 0.0f;
  m[7] = 0.0f;
  m[8] = 1.0f;
}

// THE PER EYE MATRIX OF A CANTED VIEW, for EyeCameraSetPose.
//
// The camera of the engine carries the yaw and the pitch of the head. The
// roll R turns it to the head, and the turn C of the view turns the head to
// the eye: h = R * C. The eye sits `offset_x` world units along the right
// axis of the head, not of the eye. EyeCameraSetPose takes the offset in
// the frame of the eye (t' = t * h - d), so d = offset_x * (row 0 of C).
// With no turn, h is R and d is (offset_x, 0, 0), the matrix of a view that
// is not canted.
inline void EyeMatrix(const float relative[4], float roll, float offset_x,
                      float h[9], float d[3]) {
  float c[9];
  CameraMatrix(relative, c);
  float r[9];
  RollMatrix(roll, r);
  MatMul(r, c, h);
  d[0] = offset_x * c[0];
  d[1] = offset_x * c[1];
  d[2] = offset_x * c[2];
}

}  // namespace eye_cant
