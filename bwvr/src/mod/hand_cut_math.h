// A hand in front of a card: the pure math. See hand_cut.h and
// docs/ui-layer.md, "A hand in front of a card". hand_cut.cpp uses it, and
// test/controller_math checks it (hand_cut_checks.h). Nothing here reads a
// global or touches the game.
//
// THE ORDER. A card is a quad layer, so the runtime draws it over the eye
// pictures, and over the hands in them. The real order is: the part of a
// hand that is nearer to the eye than the page is in front of the page, and
// the part that is farther is behind it. For each eye, each triangle of the
// hand is cut to its part in front of the plane of the page. That part goes
// onto the page along the lines from the eye: there the page has a hole, so
// that eye sees its eye picture, with the hand in it.

#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

namespace hand_cut_math {

// The page: its middle, and its right, up and normal in the room. The normal
// points to the side of the head.
struct Page {
  float center[3];
  float right[3];
  float up[3];
  float normal[3];
};

inline float Dot(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// The distance of a point in front of the page (to the side of the normal),
// in meters. Below 0 for a point behind the page.
inline float Front(const Page& p, const float point[3]) {
  const float d[3] = {point[0] - p.center[0], point[1] - p.center[1],
                      point[2] - p.center[2]};
  return Dot(d, p.normal);
}

// The part of a triangle in front of the page: 0 points when all of it is
// behind, 3 or 4 points (a convex polygon in the turn of the triangle).
inline int ClipFront(const float tri[3][3], const Page& p, float out[4][3]) {
  float f[3];
  for (int i = 0; i < 3; ++i) f[i] = Front(p, tri[i]);
  int n = 0;
  for (int i = 0; i < 3; ++i) {
    const int j = (i + 1) % 3;
    const bool in_i = f[i] > 0.0f, in_j = f[j] > 0.0f;
    if (in_i && n < 4) {
      for (int k = 0; k < 3; ++k) out[n][k] = tri[i][k];
      ++n;
    }
    if (in_i != in_j && n < 4) {
      const float t = f[i] / (f[i] - f[j]);
      for (int k = 0; k < 3; ++k) {
        out[n][k] = tri[i][k] + (tri[j][k] - tri[i][k]) * t;
      }
      ++n;
    }
  }
  return n >= 3 ? n : 0;
}

// A point in front of the page goes onto the page along the line from the
// eye: x to the right and y up, in meters from the middle of the page.
// False when the eye is not in front of the page, or the point is not
// between the eye and the page.
inline bool OntoPage(const float eye[3], const Page& p, const float point[3],
                     float* x, float* y) {
  const float fe = Front(p, eye);
  const float fp = Front(p, point);
  if (!(fe > 0.01f) || fp < 0.0f || fp >= fe - 0.001f) return false;
  const float k = fe / (fe - fp);
  float on[3];
  for (int i = 0; i < 3; ++i) {
    on[i] = eye[i] + (point[i] - eye[i]) * k - p.center[i];
  }
  *x = Dot(on, p.right);
  *y = Dot(on, p.up);
  return true;
}

// A ball in front of the page, as one eye sees it on the page: the middle
// of its disc (x to the right and y up, in meters from the middle of the
// page) and its radius. The disc is the ball itself as that eye sees it, so
// a hole of this disc is covered by the ball in the eye picture of that eye.
// False when no part of the ball is in front of the page, or the eye is not
// in front of it.
inline bool BallOnPage(const float eye[3], const Page& p, const float ball[3],
                       float radius, float* x, float* y, float* r) {
  const float fe = Front(p, eye);
  const float fb = Front(p, ball);
  if (!(fe > 0.01f) || !(radius > 0.0f) || fb <= -radius ||
      fb >= fe - 0.001f) {
    return false;
  }
  const float k = fe / (fe - fb);
  float on[3];
  for (int i = 0; i < 3; ++i) {
    on[i] = eye[i] + (ball[i] - eye[i]) * k - p.center[i];
  }
  *x = Dot(on, p.right);
  *y = Dot(on, p.up);
  *r = radius * k;
  return true;
}

// The span of a convex polygon of n points on the line y = sy: *x0 to *x1.
// False when the line does not meet the polygon.
inline bool SpanAt(const float (*pt)[2], int n, float sy, float* x0,
                   float* x1) {
  bool any = false;
  float lo = 0.0f, hi = 0.0f;
  for (int i = 0; i < n; ++i) {
    const int j = (i + 1) % n;
    const float ya = pt[i][1], yb = pt[j][1];
    if ((ya > sy) == (yb > sy)) continue;
    const float x = pt[i][0] + (sy - ya) * (pt[j][0] - pt[i][0]) / (yb - ya);
    if (!any || x < lo) lo = x;
    if (!any || x > hi) hi = x;
    any = true;
  }
  *x0 = lo;
  *x1 = hi;
  return any;
}

// The samples of pixel x of a row that are in the spans of the row: 0 to 4.
// top and low tell if the line at a quarter and at three quarters of the
// row meets the polygon, with the spans a0 to a1 and b0 to b1.
inline int RowSamples(int x, bool top, float a0, float a1, bool low, float b0,
                      float b1) {
  const float s0 = x + 0.25f, s1 = x + 0.75f;
  int count = 0;
  if (top) count += (s0 >= a0 && s0 <= a1) + (s1 >= a0 && s1 <= a1);
  if (low) count += (s0 >= b0 && s0 <= b1) + (s1 >= b0 && s1 <= b1);
  return count;
}

// Fills a convex polygon of n points (pixels, x right and y down) into a
// mask of w by h bytes, with 2 by 2 samples in each pixel: 0, 64, 128, 192
// or 255. A pixel keeps the largest value. Each row of samples takes the
// span of the polygon on its line, so the cost goes with the rows and the
// pixels of the polygon, not with its box. A polygon with its box out of the
// mask costs no row. In a row, the pixels with all 4 samples inside both
// spans are one fill of 255, and only the pixels at the ends of the row
// count their samples. *box grows to the pixels that the polygon touches:
// x0, y0, x1, y1, with x1 and y1 past the last pixel.
inline void FillConvex(const float (*pt)[2], int n, uint8_t* mask, int w,
                       int h, int box[4]) {
  if (n < 3) return;
  float lo_x = pt[0][0], hi_x = pt[0][0];
  float lo_y = pt[0][1], hi_y = pt[0][1];
  for (int i = 1; i < n; ++i) {
    if (pt[i][0] < lo_x) lo_x = pt[i][0];
    if (pt[i][0] > hi_x) hi_x = pt[i][0];
    if (pt[i][1] < lo_y) lo_y = pt[i][1];
    if (pt[i][1] > hi_y) hi_y = pt[i][1];
  }
  // A sample of pixel x is at x + 0.25 or more and x + 0.75 or less.
  if (!(hi_x >= 0.25f) || !(lo_x <= w - 0.25f) || !(hi_y >= 0.25f) ||
      !(lo_y <= h - 0.25f)) {
    return;
  }
  int y0 = static_cast<int>(floorf(lo_y)), y1 = static_cast<int>(ceilf(hi_y));
  if (y0 < 0) y0 = 0;
  if (y1 > h) y1 = h;
  for (int y = y0; y < y1; ++y) {
    float a0 = 0.0f, a1 = 0.0f, b0 = 0.0f, b1 = 0.0f;
    const bool top = SpanAt(pt, n, y + 0.25f, &a0, &a1);
    const bool low = SpanAt(pt, n, y + 0.75f, &b0, &b1);
    if (!top && !low) continue;
    float lo = top ? a0 : b0, hi = top ? a1 : b1;
    if (low && b0 < lo) lo = b0;
    if (low && b1 > hi) hi = b1;
    int x0 = static_cast<int>(floorf(lo - 0.75f));
    int x1 = static_cast<int>(ceilf(hi));
    if (x0 < 0) x0 = 0;
    if (x1 > w) x1 = w;
    if (x0 >= x1) continue;
    // The full pixels: x + 0.25 at the start of both spans or past it, and
    // x + 0.75 at the end of both spans or before it.
    int f0 = x0, f1 = x0;
    if (top && low) {
      const float in0 = (a0 > b0 ? a0 : b0) - 0.25f;
      const float in1 = (a1 < b1 ? a1 : b1) - 0.75f;
      if (in1 >= in0) {
        f0 = static_cast<int>(ceilf(in0));
        f1 = static_cast<int>(floorf(in1)) + 1;
        if (f0 < x0) f0 = x0;
        if (f0 > x1) f0 = x1;
        if (f1 > x1) f1 = x1;
        if (f1 < f0) f1 = f0;
      }
    }
    uint8_t* row = mask + static_cast<size_t>(y) * w;
    int first = -1, last = -1;
    for (int x = x0; x < f0; ++x) {
      const int count = RowSamples(x, top, a0, a1, low, b0, b1);
      if (count == 0) continue;
      const uint8_t v = count >= 4 ? 255 : static_cast<uint8_t>(count * 64);
      if (v > row[x]) row[x] = v;
      if (first < 0) first = x;
      last = x;
    }
    if (f1 > f0) {
      memset(row + f0, 255, static_cast<size_t>(f1 - f0));
      if (first < 0) first = f0;
      last = f1 - 1;
    }
    for (int x = f1; x < x1; ++x) {
      const int count = RowSamples(x, top, a0, a1, low, b0, b1);
      if (count == 0) continue;
      const uint8_t v = count >= 4 ? 255 : static_cast<uint8_t>(count * 64);
      if (v > row[x]) row[x] = v;
      if (first < 0) first = x;
      last = x;
    }
    if (first < 0) continue;
    if (first < box[0]) box[0] = first;
    if (y < box[1]) box[1] = y;
    if (y + 1 > box[3]) box[3] = y + 1;
    if (last + 1 > box[2]) box[2] = last + 1;
  }
}

// The pixels of a mark of the place of a hand on a page, a share of 0 to 1
// at the distance r (pixels) from its middle, for a mark of the radius
// `radius`: full inside, and soft over 1 pixel at its edge.
inline float MarkCover(float r, float radius) {
  const float c = radius + 0.5f - r;
  return c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
}

}  // namespace hand_cut_math
