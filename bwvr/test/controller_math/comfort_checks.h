// THE COMFORT OF A MOVE (docs/god-view.md, "The comfort of a move"): the
// vignette and the recenter hold (comfort_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/comfort_math.h"

namespace comfort_checks {

inline void ComfortChecks() {
  using namespace comfort_math;
  printf("\nThe comfort of a move\n");

  // THE KEYS.
  Check(VignetteMode(-1) == 0 && VignetteMode(1) == 1 && VignetteMode(7) == 2,
        "comfort_vignette is 0, 1 or 2", VignetteMode(7), 2);
  Check(NextVignetteMode(0) == 1 && NextVignetteMode(1) == 2 &&
            NextVignetteMode(2) == 0,
        "the switch goes from 0 to 1 to 2 and back to 0", NextVignetteMode(2),
        0);
  Check(RecenterRule(-3) == 0 && RecenterRule(1) == 1 && RecenterRule(5) == 2,
        "recenter_hold is 0, 1 or 2", RecenterRule(5), 2);
  Check(NextRecenterRule(0) == 1 && NextRecenterRule(1) == 2 &&
            NextRecenterRule(2) == 0,
        "the switch of recenter_hold goes from 0 to 1 to 2 and back",
        NextRecenterRule(2), 0);

  // THE SHAPE.
  const VignetteShape off = Shape(kVignetteOff);
  const VignetteShape gentle = Shape(kVignetteGentle);
  const VignetteShape strong = Shape(kVignetteStrong);
  Near("off has no darkness", DarkAt(off, 1.0f, 80.0f), 0.0, 1e-6);
  Near("gentle: the middle is clear", DarkAt(gentle, 1.0f, 0.0f), 0.0, 1e-6);
  Near("gentle: 35 degrees is still clear", DarkAt(gentle, 1.0f, 35.0f), 0.0,
       1e-6);
  Near("gentle: 50 degrees, the edge of a Quest 3, is about 60 percent dark",
       DarkAt(gentle, 1.0f, 50.0f), 0.85 * 0.7210, 2e-3);
  Near("gentle: past 58 degrees it has its full darkness of 0.85",
       DarkAt(gentle, 1.0f, 70.0f), 0.85, 1e-6);
  Near("strong: 45 degrees is black", DarkAt(strong, 1.0f, 45.0f), 1.0, 1e-6);
  Near("strong: 22 degrees is clear", DarkAt(strong, 1.0f, 22.0f), 0.0, 1e-6);
  Near("a level of one half gives half of the darkness",
       DarkAt(strong, 0.5f, 60.0f), 0.5, 1e-6);
  Near("a level of 0 is no darkness", DarkAt(strong, 0.0f, 60.0f), 0.0, 1e-6);
  Check(DarkAt(gentle, 1.0f, 40.0f) < DarkAt(gentle, 1.0f, 45.0f) &&
            DarkAt(gentle, 1.0f, 45.0f) < DarkAt(gentle, 1.0f, 52.0f),
        "the darkness grows from the middle out", DarkAt(gentle, 1.0f, 45.0f),
        0.3);

  // THE FADE: in in 0.15 s, out in 0.3 s, in a straight line.
  Near("a move at full speed: after 75 ms the level is one half",
       Fade(0.0f, 1.0f, 0.075f), 0.5, 1e-5);
  Near("after 150 ms it is full", Fade(0.5f, 1.0f, 0.075f), 1.0, 1e-5);
  Near("it does not go past its target", Fade(0.9f, 0.95f, 0.1f), 0.95, 1e-6);
  Near("a stop: after 150 ms the level is one half",
       Fade(1.0f, 0.0f, 0.15f), 0.5, 1e-5);
  Near("after 300 ms it is gone", Fade(0.5f, 0.0f, 0.15f), 0.0, 1e-5);
  Near("a slower move: the level goes down to the new share",
       Fade(1.0f, 0.4f, 1.0f), 0.4, 1e-6);
  Near("a frame of no time changes nothing", Fade(0.3f, 1.0f, 0.0f), 0.3,
       1e-6);
  {
    // Ninety frames of 1/90 s from rest at a full push, then ninety frames
    // at rest.
    float level = 0.0f;
    int frames_to_full = -1;
    for (int i = 1; i <= 90; ++i) {
      level = Fade(level, 1.0f, 1.0f / 90.0f);
      if (frames_to_full < 0 && level >= 1.0f) frames_to_full = i;
    }
    Check(frames_to_full >= 13 && frames_to_full <= 14,
          "at 90 frames a second the level is full after 14 frames",
          frames_to_full, 14);
    int frames_to_none = -1;
    for (int i = 1; i <= 90; ++i) {
      level = Fade(level, 0.0f, 1.0f / 90.0f);
      if (frames_to_none < 0 && level <= 0.0f) frames_to_none = i;
    }
    Check(frames_to_none >= 26 && frames_to_none <= 28,
          "and it is gone 27 frames after the stop", frames_to_none, 27);
  }

  // THE SHARE OF A MOVE.
  Near("a still view is no move", MoveShare(0.0f, 0.0f, 1.0f, 0.011f), 0.0,
       1e-6);
  Near("the top speed of the walk is a full share",
       MoveShare(1.5f / 90.0f, 0.0f, 1.0f, 1.0f / 90.0f), 1.0, 1e-4);
  Near("half of it is a half share",
       MoveShare(0.75f / 90.0f, 0.0f, 1.0f, 1.0f / 90.0f), 0.5, 1e-4);
  Near("a turn of 45 degrees a second is a half share",
       MoveShare(0.0f, -0.5f, 1.0f, 1.0f / 90.0f), 0.5, 1e-4);
  Near("a zoom of 1.5 times in 500 ms is a full share",
       MoveShare(0.0f, 0.0f, powf(1.5f, 1.0f / 45.0f), 1.0f / 90.0f), 1.0,
       1e-3);
  Near("a zoom out counts as a zoom in",
       MoveShare(0.0f, 0.0f, powf(1.5f, -1.0f / 90.0f), 1.0f / 90.0f), 0.5,
       1e-3);
  Near("the largest part counts",
       MoveShare(0.3f / 90.0f, 0.9f / 90.0f, 1.0f, 1.0f / 90.0f), 0.2, 1e-4);
  Near("a jump in a frame is at most a full share",
       MoveShare(40.0f, 0.0f, 1.0f, 1.0f / 90.0f), 1.0, 1e-6);
  Near("a frame of no time is no move", MoveShare(1.0f, 0.0f, 1.0f, 0.0f),
       0.0, 1e-6);

  // THE PICTURE.
  {
    static VignettePoint points[kVignetteVertices];
    // A render of 2208 by 2208 with 110 degrees across: fx is the half width
    // over tan 55 degrees.
    const float half = 1104.0f;
    const float f = half / tanf(55.0f * 3.14159265f / 180.0f);
    const int n = BuildVignette(gentle, 1.0f, half, half, f, f, 4.0f * half,
                                points);
    Check(n == kVignetteVertices, "the picture has all of its triangles", n,
          kVignetteVertices);
    Check(BuildVignette(gentle, 0.0f, half, half, f, f, 4.0f * half,
                        points) == 0,
          "a level of 0 draws nothing", 0, 0);
    Check(BuildVignette(off, 1.0f, half, half, f, f, 4.0f * half, points) == 0,
          "off draws nothing", 0, 0);
    BuildVignette(gentle, 1.0f, half, half, f, f, 4.0f * half, points);
    // The first point of the first band is ring 0 at angle 0: straight to
    // the right, at 35 degrees.
    const float want = half + f * tanf(35.0f * 3.14159265f / 180.0f);
    Near("the clear ring is at 35 degrees", points[0].x, want, 0.01);
    Near("and it has no darkness", points[0].alpha, 0.0, 1e-6);
    Near("it is on the middle line", points[0].y, half, 0.01);
    // The last band: its outer points are the far ring.
    const VignettePoint& far_point = points[kVignetteVertices - 5];
    const float dx = far_point.x - half, dy = far_point.y - half;
    Check(sqrtf(dx * dx + dy * dy) >= 4.0f * half - 1.0f,
          "the far ring is past the edge of the render",
          sqrtf(dx * dx + dy * dy), 4.0 * half);
    Near("and it has the full darkness", far_point.alpha, 0.85, 1e-5);
    // A render that is wider than it is high: the ring is an ellipse in
    // pixels, a circle in angles.
    const int m = BuildVignette(strong, 1.0f, 1800.0f, 1440.0f, 1200.0f,
                                1500.0f, 7200.0f, points);
    Check(m == kVignetteVertices, "a render that is not square has the picture",
          m, kVignetteVertices);
    Near("a ring has the pixels of its angle across",
         points[0].x - 1800.0f,
         1200.0f * tanf(22.0f * 3.14159265f / 180.0f), 0.01);
  }

  // THE RECENTER HOLD.
  {
    RecenterHold hold;
    Check(!RecenterStep(&hold, true, true, 10.0, 1.0),
          "a click does not recenter at once", 0, 0);
    Check(!RecenterStep(&hold, true, true, 10.9, 1.0),
          "nor after 0.9 s", 0, 0);
    Check(RecenterStep(&hold, true, true, 11.0, 1.0),
          "a hold of 1 s recenters", 1, 1);
    Check(!RecenterStep(&hold, true, true, 13.0, 1.0),
          "a click that stays down recenters one time only", 0, 0);
    Check(!RecenterStep(&hold, false, true, 13.1, 1.0), "the click goes up", 0,
          0);
    Check(!RecenterStep(&hold, true, true, 14.0, 1.0), "a new click", 0, 0);
    Check(!RecenterStep(&hold, true, false, 14.6, 1.0),
          "a push of the stick in the hold", 0, 0);
    Check(!RecenterStep(&hold, true, true, 15.2, 1.0),
          "starts the time again", 0, 0);
    Check(RecenterStep(&hold, true, true, 16.2, 1.0),
          "so the hold recenters 1 s after the stick is back", 1, 1);
    RecenterHold short_one;
    RecenterStep(&short_one, true, true, 20.0, 1.0);
    RecenterStep(&short_one, false, true, 20.5, 1.0);
    Check(!RecenterStep(&short_one, true, true, 21.1, 1.0),
          "two short clicks do not add up", 0, 0);
  }
}

}  // namespace comfort_checks
