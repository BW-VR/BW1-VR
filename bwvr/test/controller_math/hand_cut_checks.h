// A HAND IN FRONT OF A CARD (docs/ui-layer.md, "A hand in front of a card").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <string.h>

#include "../../src/mod/hand_cut_math.h"

namespace hand_cut_checks {

// The fill with a count of the samples in each pixel of each row, as a
// reference for the fill of FillConvex with its full pixels in one fill.
inline void FillEachPixel(const float (*pt)[2], int n, uint8_t* mask, int w,
                          int h, int box[4]) {
  using namespace hand_cut_math;
  float lo_y = pt[0][1], hi_y = pt[0][1];
  for (int i = 1; i < n; ++i) {
    if (pt[i][1] < lo_y) lo_y = pt[i][1];
    if (pt[i][1] > hi_y) hi_y = pt[i][1];
  }
  int y0 = static_cast<int>(floorf(lo_y)), y1 = static_cast<int>(ceilf(hi_y));
  if (y0 < 0) y0 = 0;
  if (y1 > h) y1 = h;
  Edges edges;
  EdgesOf(pt, n, &edges);
  for (int y = y0; y < y1; ++y) {
    float a0 = 0.0f, a1 = 0.0f, b0 = 0.0f, b1 = 0.0f;
    const bool top = EdgeSpan(edges, y + 0.25f, &a0, &a1);
    const bool low = EdgeSpan(edges, y + 0.75f, &b0, &b1);
    for (int x = 0; x < w; ++x) {
      const int count = RowSamples(x, top, a0, a1, low, b0, b1);
      if (count == 0) continue;
      const uint8_t v = count >= 4 ? 255 : static_cast<uint8_t>(count * 64);
      if (v > mask[y * w + x]) mask[y * w + x] = v;
      if (x < box[0]) box[0] = x;
      if (y < box[1]) box[1] = y;
      if (x + 1 > box[2]) box[2] = x + 1;
      if (y + 1 > box[3]) box[3] = y + 1;
    }
  }
}

inline void HandCutChecks() {
  using namespace hand_cut_math;
  printf("\nA hand in front of a card\n");
  // A page 45 cm in front of the head, facing it: x right, y up, and the
  // normal toward the head (+z). The eyes are 6.4 cm apart.
  const Page page = {{0.0f, 0.0f, -0.45f},
                     {1.0f, 0.0f, 0.0f},
                     {0.0f, 1.0f, 0.0f},
                     {0.0f, 0.0f, 1.0f}};
  const float left_eye[3] = {-0.032f, 0.0f, 0.0f};
  const float right_eye[3] = {0.032f, 0.0f, 0.0f};

  // THE CUT of a triangle at the plane.
  {
    const float front[3][3] = {{0.0f, 0.0f, -0.40f},
                               {0.02f, 0.0f, -0.40f},
                               {0.0f, 0.02f, -0.40f}};
    float out[4][3];
    Check(ClipFront(front, page, out) == 3,
          "a triangle in front of the page is whole", 3, 3);
    const float behind[3][3] = {{0.0f, 0.0f, -0.50f},
                                {0.02f, 0.0f, -0.50f},
                                {0.0f, 0.02f, -0.50f}};
    Check(ClipFront(behind, page, out) == 0,
          "a triangle behind the page has no part in front", 0, 0);
    const float across[3][3] = {{0.0f, 0.0f, -0.40f},
                                {0.02f, 0.0f, -0.50f},
                                {0.0f, 0.02f, -0.40f}};
    const int n = ClipFront(across, page, out);
    Check(n == 4, "a triangle across the page keeps 4 points in front", n, 4);
    float least = 1.0f;
    for (int i = 0; i < n; ++i) {
      const float f = Front(page, out[i]);
      if (f < least) least = f;
    }
    Near("and no point of that part is behind the page, m", least, 0.0, 1e-6);
  }

  // ONTO THE PAGE from each eye.
  {
    // A point 5 cm in front of the page, straight ahead.
    const float point[3] = {0.0f, 0.0f, -0.40f};
    float lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f;
    const bool ok = OntoPage(left_eye, page, point, &lx, &ly) &&
                    OntoPage(right_eye, page, point, &rx, &ry);
    Check(ok, "a point between the eyes and the page goes onto the page",
          ok ? 1 : 0, 1);
    // The left eye sees the point to the right of its place on the page:
    // x = -0.032 + 0.032 * 45 / 40.
    Near("the left eye sees it 4 mm to the right of the middle, m", lx,
         -0.032 + 0.032 * 45.0 / 40.0, 1e-5);
    Near("the right eye sees it 4 mm to the left, m", rx,
         0.032 - 0.032 * 45.0 / 40.0, 1e-5);
    Near("and both at the same height, m", ly - ry, 0.0, 1e-6);
    const float on_page[3] = {0.05f, 0.02f, -0.45f};
    float x = 0.0f, y = 0.0f;
    OntoPage(left_eye, page, on_page, &x, &y);
    Near("a point on the page stays where it is (x), m", x, 0.05, 1e-5);
    Near("a point on the page stays where it is (y), m", y, 0.02, 1e-5);
    const float behind[3] = {0.0f, 0.0f, -0.50f};
    Check(!OntoPage(left_eye, page, behind, &x, &y),
          "a point behind the page makes no hole", 0, 0);
    const float back_eye[3] = {0.0f, 0.0f, -0.60f};
    Check(!OntoPage(back_eye, page, point, &x, &y),
          "an eye behind the page sees no hole", 0, 0);
  }

  // THE FILL of a polygon into a mask.
  {
    static uint8_t mask[64 * 64];
    memset(mask, 0, sizeof(mask));
    const float square[4][2] = {{10.0f, 10.0f}, {20.0f, 10.0f},
                                {20.0f, 20.0f}, {10.0f, 20.0f}};
    int box[4] = {64, 64, 0, 0};
    FillConvex(square, 4, mask, 64, 64, box);
    int full = 0, part = 0;
    for (int i = 0; i < 64 * 64; ++i) {
      if (mask[i] == 255) ++full;
      else if (mask[i] > 0) ++part;
    }
    Check(full == 100 && part == 0,
          "a square of 10 by 10 px on whole pixels fills 100 pixels", full,
          100);
    Check(box[0] == 10 && box[1] == 10 && box[2] == 20 && box[3] == 20,
          "and its box is the square", box[2] - box[0], 10);
    // The other turn of the same square fills the same pixels.
    static uint8_t again[64 * 64];
    memset(again, 0, sizeof(again));
    const float turned[4][2] = {{10.0f, 10.0f}, {10.0f, 20.0f},
                                {20.0f, 20.0f}, {20.0f, 10.0f}};
    int box2[4] = {64, 64, 0, 0};
    FillConvex(turned, 4, again, 64, 64, box2);
    Check(memcmp(mask, again, sizeof(mask)) == 0,
          "the turn of the points does not change the fill", 1, 1);
    // A half pixel edge gives a part cover.
    memset(mask, 0, sizeof(mask));
    const float half[4][2] = {{10.0f, 10.0f}, {10.5f, 10.0f},
                              {10.5f, 11.0f}, {10.0f, 11.0f}};
    int box3[4] = {64, 64, 0, 0};
    FillConvex(half, 4, mask, 64, 64, box3);
    Check(mask[10 * 64 + 10] == 128,
          "half a pixel gives half of the cover", mask[10 * 64 + 10], 128);

    // The fill of the full pixels in one fill gives the same mask and box
    // as a count of the samples of each pixel: 3000 triangles and 4-gons,
    // some across the edges of the mask and some out of it.
    static uint8_t ref[64 * 64];
    uint32_t seed = 12345u;
    auto next = [&seed]() {
      seed = seed * 1664525u + 1013904223u;
      return static_cast<float>((seed >> 8) & 0xFFFF) / 65535.0f;
    };
    int differ = 0, boxes = 0, spans = 0;
    for (int k = 0; k < 3000; ++k) {
      const int n = 3 + (k & 1);
      float poly[4][2];
      const float cx = -20.0f + 104.0f * next(), cy = -20.0f + 104.0f * next();
      const float r = 0.3f + 30.0f * next();
      const float turn = 6.2831853f * next();
      for (int i = 0; i < n; ++i) {
        // A convex polygon: points round a circle in the same turn.
        const float a = turn + 6.2831853f * (i + 0.8f * next()) / n;
        poly[i][0] = cx + r * cosf(a);
        poly[i][1] = cy + r * sinf(a);
      }
      memset(mask, 0, sizeof(mask));
      memset(ref, 0, sizeof(ref));
      int b1[4] = {64, 64, 0, 0}, b2[4] = {64, 64, 0, 0};
      FillConvex(poly, n, mask, 64, 64, b1);
      FillEachPixel(poly, n, ref, 64, 64, b2);
      if (memcmp(mask, ref, sizeof(mask)) != 0) ++differ;
      if (memcmp(b1, b2, sizeof(b1)) != 0) ++boxes;
      // The span from the slopes of the edges is the span of SpanAt.
      Edges e;
      EdgesOf(poly, n, &e);
      for (int s = 0; s < 8; ++s) {
        const float sy = cy - r + 2.0f * r * next();
        float p0 = 0.0f, p1 = 0.0f, q0 = 0.0f, q1 = 0.0f;
        const bool p = SpanAt(poly, n, sy, &p0, &p1);
        const bool q = EdgeSpan(e, sy, &q0, &q1);
        if (p != q || (p && (fabsf(p0 - q0) > 1e-3f || fabsf(p1 - q1) > 1e-3f))) {
          ++spans;
        }
      }
    }
    Check(spans == 0, "the span from the slopes of the edges is the span of "
          "each edge, within 0.001 px", spans, 0);
    Check(differ == 0, "the fill of the full pixels in one fill gives the "
          "mask of a count of each pixel", differ, 0);
    Check(boxes == 0, "and the same box", boxes, 0);
    // THE GROWTH by 1 pixel with steps of 16 gives the largest of 3 of each
    // pixel, as a pixel by pixel count, for spans of 1 to 70 pixels at each
    // start, and the count of the pixels that are not 0.
    {
      uint8_t in[3][96], row[96], col[96];
      int grow_bad = 0, count_bad = 0;
      for (int start = 0; start < 8; ++start) {
        for (int len = 1; len <= 70; ++len) {
          for (int r = 0; r < 3; ++r) {
            for (int i = 0; i < 96; ++i) {
              const float v = next();
              in[r][i] = v < 0.5f ? 0 : static_cast<uint8_t>(v * 255.0f);
            }
          }
          memset(row, 7, sizeof(row));
          GrowRow(in[1], row, start, start + len);
          const int nonzero = GrowColumn(len > 2 ? in[0] : nullptr, in[1],
                                         in[2], col, start, start + len);
          int want_count = 0;
          for (int x = start; x < start + len; ++x) {
            uint8_t m = in[1][x];
            if (x > start && in[1][x - 1] > m) m = in[1][x - 1];
            if (x + 1 < start + len && in[1][x + 1] > m) m = in[1][x + 1];
            if (row[x] != m) ++grow_bad;
            uint8_t c = in[1][x];
            if (len > 2 && in[0][x] > c) c = in[0][x];
            if (in[2][x] > c) c = in[2][x];
            if (col[x] != c) ++grow_bad;
            if (c > 0) ++want_count;
          }
          if (start > 0 && row[start - 1] != 7) ++grow_bad;
          if (row[start + len] != 7) ++grow_bad;
          if (nonzero != want_count) ++count_bad;
        }
      }
      Check(grow_bad == 0, "the growth of a mask in steps of 16 gives the "
            "largest of 3 of each pixel, and no pixel out of its span",
            grow_bad, 0);
      Check(count_bad == 0, "and the count of the pixels that are not 0",
            count_bad, 0);
    }
    memset(mask, 0, sizeof(mask));
    const float out[3][2] = {{-40.0f, 10.0f}, {-5.0f, 12.0f},
                             {-20.0f, 300.0f}};
    int box4[4] = {64, 64, 0, 0};
    FillConvex(out, 3, mask, 64, 64, box4);
    Check(box4[0] == 64 && box4[2] == 0,
          "a triangle left of the mask touches no pixel", box4[2], 0);
  }

  // THE MARK of the point of a pick.
  Near("the mark is full inside its radius", MarkCover(2.0f, 4.0f), 1.0,
       1e-6);
  Near("and clear past its edge", MarkCover(5.0f, 4.0f), 0.0, 1e-6);
  Near("and half at its edge", MarkCover(4.0f, 4.0f), 0.5, 1e-6);

  // THE BALL OF A PULL on a page 40 cm in front of the eye, facing it.
  {
    Page p;
    const float c[3] = {0.0f, 0.0f, -0.40f};
    const float r[3] = {1.0f, 0.0f, 0.0f};
    const float u[3] = {0.0f, 1.0f, 0.0f};
    const float n[3] = {0.0f, 0.0f, 1.0f};
    memcpy(p.center, c, sizeof(c));
    memcpy(p.right, r, sizeof(r));
    memcpy(p.up, u, sizeof(u));
    memcpy(p.normal, n, sizeof(n));
    const float eye[3] = {0.0f, 0.0f, 0.0f};
    float x = 0.0f, y = 0.0f, rad = 0.0f;
    const float on[3] = {0.05f, 0.02f, -0.40f};
    Check(BallOnPage(eye, p, on, 0.03f, &x, &y, &rad) &&
              fabsf(x - 0.05f) < 1e-5f && fabsf(y - 0.02f) < 1e-5f &&
              fabsf(rad - 0.03f) < 1e-5f,
          "a ball on the page has a disc of its own size at its place", 0, 0);
    const float half[3] = {0.05f, 0.0f, -0.20f};
    Check(BallOnPage(eye, p, half, 0.03f, &x, &y, &rad) &&
              fabsf(x - 0.10f) < 1e-5f && fabsf(rad - 0.06f) < 1e-5f,
          "a ball half way to the eye has a disc two times as far out and as "
          "large: the ball covers it", 0, 0);
    const float right_eye[3] = {0.032f, 0.0f, 0.0f};
    const float middle[3] = {0.0f, 0.0f, -0.20f};
    Check(BallOnPage(right_eye, p, middle, 0.03f, &x, &y, &rad) &&
              fabsf(x + 0.032f) < 1e-5f,
          "each eye has the disc where that eye sees the ball", 0, 0);
    const float behind[3] = {0.0f, 0.0f, -0.45f};
    Check(!BallOnPage(eye, p, behind, 0.03f, &x, &y, &rad),
          "a ball behind the page has no disc", 0, 0);
  }
}

}  // namespace hand_cut_checks
