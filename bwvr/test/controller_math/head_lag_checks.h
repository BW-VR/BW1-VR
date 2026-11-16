// The age of the head pose of a picture, and the move of a kept intro
// picture for a turn of the head (head_lag_math.h). main.cpp includes this
// file after Check and Near, just before main.
#pragma once

#include "../../src/mod/head_lag_math.h"

namespace head_lag_checks {

inline void Yaw(float degrees, float q[4]) {
  const float half = 0.5f * degrees / head_lag_math::kDegrees;
  q[0] = 0.0f;
  q[1] = sinf(half);
  q[2] = 0.0f;
  q[3] = cosf(half);
}

inline void Pitch(float degrees, float q[4]) {
  const float half = 0.5f * degrees / head_lag_math::kDegrees;
  q[0] = sinf(half);
  q[1] = 0.0f;
  q[2] = 0.0f;
  q[3] = cosf(half);
}

inline void HeadLagChecks() {
  using namespace head_lag_math;
  printf("\nThe age of the head pose, and the kept intro picture\n");

  // THE ANGLE.
  float a[4], b[4];
  Yaw(0.0f, a);
  Yaw(30.0f, b);
  Near("a yaw of 30 degrees is 30 degrees", AngleDeg(a, b), 30.0, 1e-3);
  const float minus_b[4] = {-b[0], -b[1], -b[2], -b[3]};
  Near("the same orientation with the other sign is 0 degrees",
       AngleDeg(b, minus_b), 0.0, 0.05);

  // THE MEAN STEP OF THE DISPLAY TIMES.
  const int64_t period = 8333333;  // 120 Hz
  double mean = NextMeanStep(0.0, 10000000, period);
  Near("the first step is the mean", mean, 10000000.0, 0.5);
  mean = NextMeanStep(mean, 2000000, period);
  Near("a step under one period counts as one period", mean,
       10000000.0 + (8333333.0 - 10000000.0) / 8.0, 0.5);
  double held = NextMeanStep(10000000.0, 100000000, period);
  Near("a stall of 100 ms counts as 3 periods", held,
       10000000.0 + (25000000.0 - 10000000.0) / 8.0, 0.5);
  Check(NextDisplayTime(1000, 0.0, period) == 1000 + period,
        "with no mean the next display time is one period later", 0, 0);
  Check(NextDisplayTime(1000, 9950000.0, period) == 1000 + 9950000,
        "with a mean the next display time is one mean step later", 0, 0);

  // THE MOVE OF A KEPT PICTURE.
  float x = 0.0f, y = 0.0f;
  float left[4];
  Yaw(10.0f, left);
  Check(KeptPictureShift(a, left, 1.0f, 1.0f, &x, &y),
        "a turn of 10 degrees gives a move", 0, 0);
  Near("a turn to the left takes the kept picture from the left", x,
       -tanf(10.0f / kDegrees), 1e-5);
  Near("a turn to the left gives no move up or down", y, 0.0, 1e-5);
  float up[4];
  Pitch(10.0f, up);
  KeptPictureShift(a, up, 1.0f, 2.0f, &x, &y);
  Near("a look up takes the kept picture from above, with the vertical term",
       y, 2.0f * tanf(10.0f / kDegrees), 1e-5);
  KeptPictureShift(b, b, 1.4281f, 2.5389f, &x, &y);
  Near("no turn gives no move", fabsf(x) + fabsf(y), 0.0, 1e-5);
  float back[4];
  Yaw(180.0f, back);
  Check(!KeptPictureShift(a, back, 1.0f, 1.0f, &x, &y),
        "a view that looks behind the kept view gives no move", 0, 0);

  // A pixel of the new picture at the middle shows the place of the kept
  // picture at the middle of the turn: 10 degrees left at 1.4281 is 0.2518
  // of the half width.
  KeptPictureShift(a, left, 1.4281f, 2.5389f, &x, &y);
  int px = 0, py = 0;
  ShiftPixels(x, y, 2000, 1600, &px, &py);
  Check(px == -252 && py == 0,
        "10 degrees left at 2000 pixels is 252 pixels to the left", px,
        -252.0);
  ShiftPixels(0.0f, 0.1f, 2000, 1600, &px, &py);
  Check(py == -80, "a move up is a smaller row number", py, -80.0);

  // THE COPY.
  head_lag_math::CopyRect r = {};
  Check(ShiftCopy(-252, 0, 2000, 1600, &r) && r.src_x == 0 &&
            r.dst_x == 252 && r.width == 1748 && r.height == 1600,
        "a move to the left copies the kept picture to the right", r.dst_x,
        252.0);
  Check(ShiftCopy(30, -80, 2000, 1600, &r) && r.src_x == 30 &&
            r.dst_x == 0 && r.src_y == 0 && r.dst_y == 80 &&
            r.width == 1970 && r.height == 1520,
        "a move to the right and up", r.width, 1970.0);
  Check(!ShiftCopy(2000, 0, 2000, 1600, &r),
        "a move of the whole width copies nothing", 0, 0);
}

}  // namespace head_lag_checks
