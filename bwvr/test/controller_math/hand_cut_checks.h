// A HAND IN FRONT OF A CARD (docs/ui-layer.md, "A hand in front of a card").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <string.h>

#include "../../src/mod/hand_cut_math.h"

namespace hand_cut_checks {

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
  }

  // THE MARK of the point of a pick.
  Near("the mark is full inside its radius", MarkCover(2.0f, 4.0f), 1.0,
       1e-6);
  Near("and clear past its edge", MarkCover(5.0f, 4.0f), 0.0, 1e-6);
  Near("and half at its edge", MarkCover(4.0f, 4.0f), 0.5, 1e-6);
}

}  // namespace hand_cut_checks
