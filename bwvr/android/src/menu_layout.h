// THE LAYOUT OF THE MENU PICTURE and the hit of a controller ray on it
// (docs/android.md, "The menu"). It has no Android code, so the math test of
// the PC checks it (test/controller_math/menu_checks.h).
//
// The menu is one picture of kWidth by kHeight pixels. In the headset it is
// a quad of the compositor in front of the player. On a flat screen it fills
// the window. The paint (menu_paint.cpp) and the hit test use the same
// numbers.
#pragma once

#include <math.h>

namespace menu_layout {

constexpr int kWidth = 1600;
constexpr int kHeight = 1200;

// The title band.
constexpr int kTitleBaseline = 120;
// The rows.
constexpr int kRowTop = 190;
constexpr int kRowHeight = 68;
constexpr int kMaxRows = 11;
constexpr int kLeft = 110;
constexpr int kRight = 1490;
// The bar of a slider and the box of a check box, at the right of a row.
constexpr int kBarLeft = 1060;
constexpr int kBarRight = kRight;
constexpr int kCheckSize = 44;
// The help text and the status line.
constexpr int kHelpTop = 960;
constexpr int kHelpBottom = 1112;
constexpr int kStatusBaseline = 1162;

inline int RowTop(int row) { return kRowTop + row * kRowHeight; }

// The row at pixel y, or -1.
inline int RowAt(double y, int rows) {
  if (y < kRowTop) return -1;
  const int row = static_cast<int>((y - kRowTop) / kRowHeight);
  return row < rows && row < kMaxRows ? row : -1;
}

inline bool OnBar(double x) { return x >= kBarLeft - 20 && x <= kBarRight + 20; }

inline double BarFraction(double x) {
  const double f = (x - kBarLeft) / static_cast<double>(kBarRight - kBarLeft);
  return f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
}

// THE QUAD IN THE HEADSET, in the LOCAL space of OpenXR (meters, y up, the
// player looks along -z at the start). The quad faces +z.
struct Quad {
  float center[3];
  float width;
  float height;
};

// 1.4 m wide at 1.4 m in front of the head and a little below it: the
// angle of a menu of the game on a monitor.
inline Quad MenuQuad() {
  Quad q;
  q.center[0] = 0.0f;
  q.center[1] = -0.15f;
  q.center[2] = -1.4f;
  q.width = 1.4f;
  q.height = q.width * kHeight / kWidth;
  return q;
}

// The pixel where a ray hits the quad. False when the ray points away from
// it or misses it.
inline bool RayToPixel(const float origin[3], const float direction[3],
                       const Quad& q, double* px, double* py) {
  if (direction[2] >= -1e-4f) return false;
  const double t = (q.center[2] - origin[2]) / direction[2];
  if (t <= 0.0) return false;
  const double hx = origin[0] + t * direction[0];
  const double hy = origin[1] + t * direction[1];
  const double u = (hx - (q.center[0] - q.width * 0.5)) / q.width;
  const double v = ((q.center[1] + q.height * 0.5) - hy) / q.height;
  if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0) return false;
  *px = u * kWidth;
  *py = v * kHeight;
  return true;
}

// The point on the quad of a pixel, in the LOCAL space, for the cursor.
inline void PixelToPoint(const Quad& q, double px, double py, float out[3]) {
  out[0] = static_cast<float>(q.center[0] - q.width * 0.5 + px / kWidth * q.width);
  out[1] = static_cast<float>(q.center[1] + q.height * 0.5 - py / kHeight * q.height);
  out[2] = q.center[2];
}

// The direction -z of a pose (the aim of a controller) from its quaternion
// (x, y, z, w).
inline void Forward(const float q[4], float out[3]) {
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  out[0] = -(2.0f * (x * z + w * y));
  out[1] = -(2.0f * (y * z - w * x));
  out[2] = -(1.0f - 2.0f * (x * x + y * y));
}

}  // namespace menu_layout
