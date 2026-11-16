// THE HEAD BLINDER (docs/god-view.md, "The head under the land"): the
// clearance, the strength, the fade and the picture (blinder_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/blinder_math.h"

namespace blinder_checks {

inline void BlinderChecks() {
  using namespace blinder_math;
  printf("\nThe head blinder\n");

  // THE CLEARANCE, at 100 units for each meter.
  {
    const float flat[kPoints] = {30.0f, 30.0f, 30.0f, 30.0f, 30.0f};
    Near("a head 60 cm over flat land is 0.6 m clear",
         Clearance(90.0f, flat, kPoints, 100.0f), 0.6, 1e-5);
    Near("a head 2 cm under the land is -0.02 m clear",
         Clearance(28.0f, flat, kPoints, 100.0f), -0.02, 1e-5);
    const float slope[kPoints] = {30.0f, 33.0f, 27.0f, 31.0f, 29.0f};
    Near("on a slope the highest point counts",
         Clearance(35.0f, slope, kPoints, 100.0f), 0.02, 1e-5);
    Near("the same head at 7.5 units for each meter",
         Clearance(35.0f, slope, kPoints, 7.5f), 2.0f / 7.5f, 1e-5);
    Check(Clearance(35.0f, slope, 0, 100.0f) > 100.0f,
          "no land point is no land", 1, 1);
    float p[kPoints][2];
    AroundPoints(100.0f, 200.0f, 100.0f, p);
    Near("the head point is the first point", p[0][0] + p[0][1], 300.0, 1e-4);
    Near("a point around it is 6 cm of the room away", p[1][0] - 100.0f, 6.0,
         1e-4);
  }

  // THE STRENGTH.
  Near("5 cm over the land is no overlay", Strength(0.05f), 0.0, 1e-6);
  Near("1 m over the land is no overlay", Strength(1.0f), 0.0, 1e-6);
  Near("2.5 cm over the land is half", Strength(0.025f), 0.5, 1e-5);
  Near("at the land the overlay is full", Strength(0.0f), 1.0, 1e-6);
  Near("under the land the overlay is full", Strength(-0.3f), 1.0, 1e-6);
  Check(Strength(0.04f) < Strength(0.03f) && Strength(0.03f) < Strength(0.01f),
        "the strength grows as the head goes down", Strength(0.03f), 0.35);

  // THE FADE: in in 0.1 s, out in 0.25 s.
  Near("into the land: after 50 ms the level is one half",
       Fade(0.0f, 1.0f, 0.05f), 0.5, 1e-5);
  Near("after 100 ms it is full", Fade(0.5f, 1.0f, 0.05f), 1.0, 1e-5);
  Near("out of the land: after 125 ms it is one half",
       Fade(1.0f, 0.0f, 0.125f), 0.5, 1e-5);
  Near("after 250 ms it is gone", Fade(0.5f, 0.0f, 0.125f), 0.0, 1e-5);
  Near("a frame of no time changes nothing", Fade(0.4f, 1.0f, 0.0f), 0.4,
       1e-6);

  // THE OPACITY.
  Near("level 0 is clear at each angle", OpacityAt(0.0f, 60.0f), 0.0, 1e-6);
  Near("level 1 is opaque in the middle", OpacityAt(1.0f, 0.0f), 1.0, 1e-6);
  Near("level 1 is opaque at the edge", OpacityAt(1.0f, 55.0f), 1.0, 1e-6);
  Near("a low level keeps the view of a headset clear",
       OpacityAt(0.05f, 50.0f), 0.0, 1e-6);
  Near("level one half: the middle is clear", OpacityAt(0.5f, 20.0f), 0.0,
       1e-6);
  Near("level one half: 50 degrees is opaque", OpacityAt(0.5f, 50.0f), 1.0,
       1e-6);
  Check(OpacityAt(0.3f, 40.0f) < OpacityAt(0.3f, 50.0f),
        "the edge is more opaque than the middle", OpacityAt(0.3f, 50.0f),
        0.5);

  // THE COLOR.
  Check(ColorAt(0.0f) == kGold && ColorAt(50.0f) == kRed &&
            ColorAt(80.0f) == kRed,
        "gold in the middle, red from 50 degrees", 1, 1);
  {
    const uint32_t c = ColorAt(25.0f);
    const int r = static_cast<int>((c >> 16) & 0xFF);
    Check(r > 0x7A && r < 0xC8, "between the two at 25 degrees", r, 0xA1);
  }

  // THE PICTURE.
  {
    static BlinderPoint points[kBlinderVertices];
    const float half = 1104.0f;
    const float f = half / tanf(55.0f * 3.14159265f / 180.0f);
    Check(BuildBlinder(0.0f, half, half, f, f, 4.0f * half, points) == 0,
          "level 0 draws nothing", 0, 0);
    const int n = BuildBlinder(1.0f, half, half, f, f, 4.0f * half, points);
    Check(n == kBlinderVertices, "the picture has all of its triangles", n,
          kBlinderVertices);
    Check((points[0].argb >> 24) == 255u && points[0].x == half &&
              points[0].y == half,
          "at level 1 the middle point is opaque", points[0].argb >> 24, 255);
    bool all = true;
    for (int i = 0; i < n; ++i) all = all && (points[i].argb >> 24) == 255u;
    Check(all, "at level 1 each corner is opaque", all, 1);
    BuildBlinder(0.5f, half, half, f, f, 4.0f * half, points);
    Check((points[0].argb >> 24) == 0u,
          "at level one half the middle is clear", points[0].argb >> 24, 0);
    // The last band of the first segment: its outer points are the far ring.
    const BlinderPoint& far_point = points[3 + (kRings - 1) * 6 + 1];
    const float dx = far_point.x - half, dy = far_point.y - half;
    Check(sqrtf(dx * dx + dy * dy) >= 4.0f * half - 1.0f,
          "the far ring is past the edge of the render",
          sqrtf(dx * dx + dy * dy), 4.0 * half);
  }
}

}  // namespace blinder_checks
