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

// Fills a convex polygon of n points (pixels, x right and y down) into a
// mask of w by h bytes, with 2 by 2 samples in each pixel: 0, 64, 128, 192
// or 255. A pixel keeps the largest value. Each row of samples takes the
// span of the polygon on its line, so the cost goes with the rows and the
// pixels of the polygon, not with its box. *box grows to the pixels that
// the polygon touches: x0, y0, x1, y1, with x1 and y1 past the last pixel.
inline void FillConvex(const float (*pt)[2], int n, uint8_t* mask, int w,
                       int h, int box[4]) {
  if (n < 3) return;
  float lo_y = pt[0][1], hi_y = pt[0][1];
  for (int i = 1; i < n; ++i) {
    if (pt[i][1] < lo_y) lo_y = pt[i][1];
    if (pt[i][1] > hi_y) hi_y = pt[i][1];
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
    bool row_any = false;
    uint8_t* row = mask + static_cast<size_t>(y) * w;
    for (int x = x0; x < x1; ++x) {
      const float s0 = x + 0.25f, s1 = x + 0.75f;
      int count = 0;
      if (top) count += (s0 >= a0 && s0 <= a1) + (s1 >= a0 && s1 <= a1);
      if (low) count += (s0 >= b0 && s0 <= b1) + (s1 >= b0 && s1 <= b1);
      if (count == 0) continue;
      const uint8_t v = count >= 4 ? 255 : static_cast<uint8_t>(count * 64);
      if (v > row[x]) row[x] = v;
      if (!row_any) {
        row_any = true;
        if (x < box[0]) box[0] = x;
        if (y < box[1]) box[1] = y;
        if (y + 1 > box[3]) box[3] = y + 1;
      }
      if (x + 1 > box[2]) box[2] = x + 1;
    }
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
