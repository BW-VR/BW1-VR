// CANTED VIEWS (docs/backend.md, "Canted views").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/eye_cant_math.h"

// head_pose.cpp
void HeadPoseSetFakeCant(float degrees);
void HeadPoseStripRoll(const float in[4], float out[4]);

namespace eye_cant_checks {

// The largest difference of two 3x3 matrices.
inline float MatrixError(const float a[9], const float b[9]) {
  float most = 0.0f;
  for (int i = 0; i < 9; ++i) {
    const float d = fabsf(a[i] - b[i]);
    if (d > most) most = d;
  }
  return most;
}

// A row vector times a 3x3 matrix, the convention of the camera matrix.
inline void RowTimes(const float v[3], const float m[9], float out[3]) {
  for (int c = 0; c < 3; ++c) {
    out[c] = v[0] * m[0 * 3 + c] + v[1] * m[1 * 3 + c] + v[2] * m[2 * 3 + c];
  }
}

// A head of a neck: yaw, then pitch, then roll, in degrees, as the invented
// head of head_pose.cpp builds it.
inline void Neck(float yaw, float pitch, float roll, float out[4]) {
  const float y = 0.5f * yaw / eye_cant::kDegrees;
  const float p = 0.5f * pitch / eye_cant::kDegrees;
  const float r = 0.5f * roll / eye_cant::kDegrees;
  const float qy[4] = {0.0f, sinf(y), 0.0f, cosf(y)};
  const float qp[4] = {sinf(p), 0.0f, 0.0f, cosf(p)};
  const float qr[4] = {0.0f, 0.0f, -sinf(r), cosf(r)};
  float yp[4];
  eye_cant::Multiply(qy, qp, yp);
  eye_cant::Multiply(yp, qr, out);
}

inline void EyeCantChecks() {
  using namespace eye_cant;
  printf("\nCanted views\n");

  // THE CANT OF THE INVENTED HEAD. Each view turns 10 degrees outward: eye 0
  // to the left, eye 1 to the right. The mean of the two is the head.
  float turn0[4], turn1[4], mean[4];
  FakeTurn(0, 10.0f, turn0);
  FakeTurn(1, 10.0f, turn1);
  Mean(turn0, turn1, mean);
  Near("the mean of two views with a cant of 10 degrees is the head",
       AngleDeg(mean), 0.0, 1e-3);
  float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
  Angles(turn0, &yaw, &pitch, &roll);
  Near("the view of eye 0 turns 10 degrees to the left", yaw, 10.0, 1e-3);
  Near("with no pitch", pitch, 0.0, 1e-3);
  Near("and no roll", roll, 0.0, 1e-3);
  Angles(turn1, &yaw, &pitch, &roll);
  Near("the view of eye 1 turns 10 degrees to the right", yaw, -10.0, 1e-3);

  // Angles reads back the yaw, the pitch and the roll of a neck.
  float neck[4];
  Neck(30.0f, -20.0f, 15.0f, neck);
  Angles(neck, &yaw, &pitch, &roll);
  Near("the angles of a neck: yaw 30", yaw, 30.0, 1e-3);
  Near("pitch -20", pitch, -20.0, 1e-3);
  Near("roll 15", roll, 15.0, 1e-3);

  // The invented head of head_pose.cpp with fake_eye_cant_deg=10, turned and
  // tilted: the turn of each view from the mean is the cant, in the frame of
  // the head.
  HeadPoseSetFake(true, 30.0f, -20.0f, 15.0f);
  HeadPoseSetFakeCant(10.0f);
  float position[2][3], view[2][4];
  HeadPoseFakeEye(0, position[0], view[0]);
  HeadPoseFakeEye(1, position[1], view[1]);
  float head[4];
  Mean(view[0], view[1], head);
  float relative[4];
  Relative(neck, head, relative);
  Near("the mean of the two views of the invented head is its neck",
       AngleDeg(relative), 0.0, 1e-3);
  float turn[2][4];
  Relative(head, view[0], turn[0]);
  Relative(head, view[1], turn[1]);
  Angles(turn[0], &yaw, &pitch, &roll);
  Near("from the head, eye 0 of the invented head turns 10 degrees to the "
       "left", yaw, 10.0, 1e-3);
  Near("and only about the up axis of the head", fabsf(pitch) + fabsf(roll),
       0.0, 1e-3);
  Angles(turn[1], &yaw, &pitch, &roll);
  Near("and eye 1 10 degrees to the right", yaw, -10.0, 1e-3);
  Near("the cant does not move the eyes: they are still 66.8 mm apart",
       sqrtf((position[1][0] - position[0][0]) *
                 (position[1][0] - position[0][0]) +
             (position[1][1] - position[0][1]) *
                 (position[1][1] - position[0][1]) +
             (position[1][2] - position[0][2]) *
                 (position[1][2] - position[0][2])),
       0.0668, 1e-5);
  HeadPoseSetFakeCant(0.0f);
  float flat_view[2][4];
  HeadPoseFakeEye(0, position[0], flat_view[0]);
  HeadPoseFakeEye(1, position[1], flat_view[1]);
  Relative(flat_view[0], flat_view[1], relative);
  Check(AngleDeg(relative) < kCantedDeg,
        "with fake_eye_cant_deg=0 the two views have one orientation, as on "
        "a Quest", AngleDeg(relative), 0.0);
  HeadPoseSetFake(false, 0.0f, 0.0f, 0.0f);

  // THE ROLL MATRIX of stereo.cpp is the roll of a neck: the head tilts to
  // the right for a positive angle.
  float roll_q[4];
  Neck(0.0f, 0.0f, 15.0f, roll_q);
  float from_q[9], rz[9];
  CameraMatrix(roll_q, from_q);
  RollMatrix(15.0f / kDegrees, rz);
  Near("the roll matrix is the matrix of a roll of the neck",
       MatrixError(from_q, rz), 0.0, 1e-5);

  // THE PER EYE MATRIX. The camera of the engine has the yaw and the pitch
  // of the head and no roll. Its matrix times the per eye matrix must be the
  // matrix of the view of the eye: the roll, then the cant.
  {
    float level[4];
    Neck(30.0f, -20.0f, 0.0f, level);
    float b0[9];
    CameraMatrix(level, b0);
    for (int e = 0; e < 2; ++e) {
      float cant[4], eye[4], t[4];
      FakeTurn(e, 10.0f, cant);
      eye_cant::Multiply(neck, cant, eye);
      Relative(neck, eye, t);
      float h[9], d[3];
      EyeMatrix(t, 15.0f / kDegrees, e == 0 ? -3.6f : 3.6f, h, d);
      float b1[9], want[9];
      eye_cant::MatMul(b0, h, b1);
      CameraMatrix(eye, want);
      Near(e == 0 ? "eye 0: the camera of the engine with the per eye matrix "
                    "is the view of the eye, with roll and cant"
                  : "eye 1: the same",
           MatrixError(b1, want), 0.0, 1e-5);

      // THE OFFSET. The eye is 3.6 units along the right axis of the head
      // (the camera of the engine with the roll), not of the eye. So
      // t * h - d must be -(eye place) * B1.
      const float place[3] = {12.0f, -4.0f, 30.0f};
      float t0[3];
      RowTimes(place, b0, t0);
      for (int i = 0; i < 3; ++i) t0[i] = -t0[i];
      float with_h[3];
      RowTimes(t0, h, with_h);
      float head_basis[9];
      eye_cant::MatMul(b0, rz, head_basis);
      const float x = e == 0 ? -3.6f : 3.6f;
      const float moved[3] = {place[0] + x * head_basis[0],
                              place[1] + x * head_basis[3],
                              place[2] + x * head_basis[6]};
      float t1[3];
      RowTimes(moved, b1, t1);
      float most = 0.0f;
      for (int i = 0; i < 3; ++i) {
        const float err = fabsf((with_h[i] - d[i]) - (-t1[i]));
        if (err > most) most = err;
      }
      Near(e == 0 ? "eye 0: the eye sits on the right axis of the head"
                  : "eye 1: the eye sits on the right axis of the head",
           most, 0.0, 1e-4);
    }
  }

  // NO CANT: the matrix of a view that is not canted, as before.
  {
    const float none[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float h[9], d[3];
    EyeMatrix(none, 15.0f / kDegrees, -3.6f, h, d);
    Near("with no cant the per eye matrix is the roll alone",
         MatrixError(h, rz), 0.0, 1e-6);
    Near("and the offset is on the x axis alone",
         fabsf(d[0] + 3.6f) + fabsf(d[1]) + fabsf(d[2]), 0.0, 1e-6);
  }

  // THE RENDER WITH NO ROLL (head_roll=0). The layer declares each view with
  // no roll, and the camera of the engine is the head with no roll. The turn
  // from the one to the other takes the camera to the declared view.
  {
    float cant[4], eye[4];
    FakeTurn(1, 10.0f, cant);
    eye_cant::Multiply(neck, cant, eye);
    float declared[4], head_level[4];
    HeadPoseStripRoll(eye, declared);
    HeadPoseStripRoll(neck, head_level);
    float t[4];
    Relative(head_level, declared, t);
    float h[9], d[3];
    EyeMatrix(t, 0.0f, 3.6f, h, d);
    float b0[9], b1[9], want[9];
    CameraMatrix(head_level, b0);
    eye_cant::MatMul(b0, h, b1);
    CameraMatrix(declared, want);
    Near("with no roll the camera of the engine with the per eye matrix is "
         "the declared view", MatrixError(b1, want), 0.0, 1e-5);
  }

  // Two equal views are not canted, so the eye passes stay as they are.
  {
    float a[4];
    Neck(-70.0f, 35.0f, -8.0f, a);
    float m[4], t[4];
    Mean(a, a, m);
    Relative(m, a, t);
    Check(AngleDeg(t) < kCantedDeg,
          "two equal views give no cant", AngleDeg(t), 0.0);
    // q and -q are one orientation: the mean must not fall between them.
    const float minus[4] = {-a[0], -a[1], -a[2], -a[3]};
    Mean(a, minus, m);
    Relative(m, a, t);
    Check(AngleDeg(t) < kCantedDeg,
          "a view and its negative quaternion have the same mean",
          AngleDeg(t), 0.0);
  }
}

}  // namespace eye_cant_checks
