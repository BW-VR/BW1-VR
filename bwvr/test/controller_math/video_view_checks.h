// THE VIEW FOR VIDEOS (docs/backend.md, "The view for videos"): the area of
// the eye picture, the smoothed head, the corner marks and the place in the
// window (video_view_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/video_view_math.h"

namespace video_view_checks {

// A head turned by yaw degrees to the left and pitch degrees up.
inline void Head(float yaw_deg, float pitch_deg, float q[4]) {
  const float y = yaw_deg * 3.14159265f / 360.0f;
  const float p = pitch_deg * 3.14159265f / 360.0f;
  // yaw about +y, then pitch about +x
  const float cy = cosf(y), sy = sinf(y), cp = cosf(p), sp = sinf(p);
  q[0] = cy * sp;
  q[1] = sy * cp;
  q[2] = -sy * sp;
  q[3] = cy * cp;
}

inline void VideoViewChecks() {
  using namespace video_view_math;
  printf("\nThe view for videos\n");

  // THE KEYS.
  Check(ClampDown(-5) == 0 && ClampDown(20) == 20 && ClampDown(90) == 40,
        "video_view_down_pct is 0 to 40", ClampDown(90), 40);
  Check(StepDown(20, true) == 25 && StepDown(20, false) == 15,
        "a step of the height is 5 percent", StepDown(20, true), 25);
  Check(StepDown(40, true) == 40 && StepDown(0, false) == 0,
        "the step stops at each end", StepDown(40, true), 40);
  Check(ClampFov(10) == 40 && ClampFov(200) == 110,
        "video_view_fov_deg is 40 to 110", ClampFov(200), 110);

  // THE AREA. An eye picture of 2600 by 2000 with a half tangent of 1.6
  // across and 1.25 down.
  const float w = 2600.0f, h = 2000.0f;
  const float fx = 1300.0f / 1.6f, fy = 1000.0f / 1.25f;
  const float band = tanf(kBandDeg * 3.14159265f / 180.0f);
  const float mx = band * fx, my = band * fy;
  const Rect a = Area(w, h, fx, 90, 20, mx, my);
  Near("90 degrees: the area is two tangents of 1 wide", a.x1 - a.x0,
       2.0 * fx, 0.01);
  Near("the area has the shape 16 by 9", (a.x1 - a.x0) / (a.y1 - a.y0),
       16.0 / 9.0, 1e-4);
  Near("the area is in the middle across", 0.5 * (a.x0 + a.x1), 1300.0,
       0.01);
  Near("20 percent: the middle of the area is 400 px below the middle",
       0.5 * (a.y0 + a.y1), 1400.0, 0.01);
  const Rect a0 = Area(w, h, fx, 90, 0, mx, my);
  Near("0 percent: the area is in the middle", 0.5 * (a0.y0 + a0.y1), 1000.0,
       0.01);
  const Rect a40 = Area(w, h, fx, 90, 40, mx, my);
  Near("40 percent: the area and its margin stop at the bottom",
       a40.y1 + my, 2000.0, 0.01);
  Near("the stop keeps the size of the area", a40.y1 - a40.y0, a.y1 - a.y0,
       0.01);
  const Rect big = Area(1000.0f, 600.0f, 500.0f, 110, 20, 20.0f, 20.0f);
  Check(big.x0 >= 20.0f - 1e-3f && big.x1 <= 980.0f + 1e-3f &&
            big.y0 >= 20.0f - 1e-3f && big.y1 <= 580.0f + 1e-3f,
        "an area larger than the picture becomes smaller, with its margins",
        big.x1, 980.0);
  Near("the smaller area keeps the shape 16 by 9",
       (big.x1 - big.x0) / (big.y1 - big.y0), 16.0 / 9.0, 1e-4);

  // THE MOVE of the smoothed head.
  const Rect m = Moved(a, mx, -my, w, h);
  Near("a move by the margin moves the area", m.x0 - a.x0, mx, 1e-3);
  const Rect edge = Moved(a, 5000.0f, 5000.0f, w, h);
  Check(edge.x1 == w && edge.y1 == h &&
            fabsf((edge.x1 - edge.x0) - (a.x1 - a.x0)) < 1e-3f,
        "a large move stops at the edge of the picture with the same size",
        edge.x1, w);

  // THE MARKS are outside each place of the area.
  Rect marks[kMarkRects];
  const int n = Marks(a, mx, my, 3.0f, 60.0f, marks);
  Check(n == 8, "eight bars, two at each corner", n, 8);
  bool clear = true;
  for (int sx = -1; sx <= 1; ++sx) {
    for (int sy = -1; sy <= 1; ++sy) {
      const Rect p = Moved(a, sx * mx, sy * my, w, h);
      for (int i = 0; i < n; ++i) {
        if (Overlap(p, marks[i])) clear = false;
      }
    }
  }
  Check(clear, "no mark is in the area at each place of the smoothed head",
        clear ? 1 : 0, 1);
  Near("a mark is near its corner: the top left mark starts at the corner "
       "less the margin, the gap and the bar",
       marks[0].x0, a.x0 - mx - kMarkGap - 3.0f, 1e-3);
  Near("a bar is 60 px long past its corner, and 3 px thick",
       (marks[0].x1 - marks[0].x0) + (marks[0].y1 - marks[0].y0), 66.0,
       1e-3);

  // THE HEAD. Tangents in the frame of the head.
  float q[4];
  Head(0.0f, 0.0f, q);
  const float ahead[3] = {0.0f, 0.0f, -1.0f};
  float tx = 1.0f, ty = 1.0f;
  Tangents(q, ahead, &tx, &ty);
  Near("a head straight ahead: no offset", fabs(tx) + fabs(ty), 0.0, 1e-6);
  Head(10.0f, 0.0f, q);
  Tangents(q, ahead, &tx, &ty);
  Near("a head 10 degrees to the left sees straight ahead to its right",
       tx, tanf(10.0f * 3.14159265f / 180.0f), 1e-5);
  Head(0.0f, -10.0f, q);
  Tangents(q, ahead, &tx, &ty);
  Near("a head 10 degrees down sees straight ahead above", ty,
       tanf(10.0f * 3.14159265f / 180.0f), 1e-5);

  // THE SMOOTHING at 90 frames a second.
  const float dt = 1.0f / 90.0f;
  const float tau = kSmoothDefault * 0.001f;
  const float band_rad = kBandDeg * 3.14159265f / 180.0f;
  // A shake of 1 degree at 8 Hz about a still direction.
  Smooth s = {{0.0f, 0.0f, -1.0f}, false};
  float most = 0.0f;
  for (int i = 0; i < 180; ++i) {
    const float yaw = 1.0f * sinf(2.0f * 3.14159265f * 8.0f * i * dt);
    Head(yaw, -30.0f, q);
    float f[3];
    Forward(q, f);
    SmoothStep(&s, f, dt, tau, band_rad);
    if (i >= 90) {
      float still[4];
      Head(0.0f, -30.0f, still);
      float g[3];
      Forward(still, g);
      const float lag = LagDeg(s, g);
      if (lag > most) most = lag;
    }
  }
  Check(most < 0.2f,
        "a shake of 1 degree at 8 Hz: the video moves less than 0.2 degrees",
        most, 0.2);
  // A step of the head of 20 degrees.
  Smooth t = {{0.0f, 0.0f, -1.0f}, false};
  Head(0.0f, 0.0f, q);
  float f0[3];
  Forward(q, f0);
  SmoothStep(&t, f0, dt, tau, band_rad);
  Head(20.0f, 0.0f, q);
  float f1[3];
  Forward(q, f1);
  SmoothStep(&t, f1, dt, tau, band_rad);
  Near("a step of 20 degrees: the video lags by the band of 3 degrees",
       LagDeg(t, f1), kBandDeg, 1e-3);
  for (int i = 0; i < 90; ++i) SmoothStep(&t, f1, dt, tau, band_rad);
  Check(LagDeg(t, f1) < 0.01f, "1 s after the step the video is on the head",
        LagDeg(t, f1), 0.0);
  // A turn of 10 degrees a second: the lag is the rate times tau.
  Smooth u = {{0.0f, 0.0f, -1.0f}, false};
  for (int i = 0; i < 180; ++i) {
    Head(10.0f * i * dt, 0.0f, q);
    float f[3];
    Forward(q, f);
    SmoothStep(&u, f, dt, tau, band_rad);
  }
  Head(10.0f * 179 * dt, 0.0f, q);
  float fu[3];
  Forward(q, fu);
  Near("a turn of 10 degrees a second lags about 1.5 degrees", LagDeg(u, fu),
       1.5, 0.1);
  // No smoothing.
  Smooth z = {{0.0f, 0.0f, -1.0f}, true};
  SmoothStep(&z, f1, dt, 0.0f, band_rad);
  Near("video_view_smooth_ms=0: the video is the head", LagDeg(z, f1), 0.0,
       1e-4);

  // THE PLACE IN THE WINDOW.
  const Rect wide = Fit(1920.0f, 1200.0f, kAspect);
  Check(wide.x0 == 0.0f && wide.x1 == 1920.0f && wide.y0 == 60.0f &&
            wide.y1 == 1140.0f,
        "1920 by 1200: the video is 1920 by 1080, with 60 px of black above "
        "and below",
        wide.y0, 60.0);
  const Rect desk = Fit(800.0f, 640.0f, kAspect);
  Check(desk.y0 == 95.0f && desk.y1 == 545.0f,
        "800 by 640: the video is 800 by 450 in the middle", desk.y1, 545.0);
  const Rect tall = Fit(1000.0f, 400.0f, kAspect);
  Near("a wide window: black at the sides", tall.x1 - tall.x0, 711.0, 0.5);
}

}  // namespace video_view_checks
