// THE CIRCLE OF THE POINTER RAY (docs/miracles.md, "The circle of the
// pointer ray"). main.cpp includes this file after Check and Near, just
// before main.
#pragma once

#include "../../src/mod/circle_aim_math.h"

namespace circle_aim_checks {

inline void CircleAimChecks() {
  using namespace circle_aim_math;
  printf("\nThe circle of the pointer ray\n");

  // A circle of radius 40 units around 1000 2000 on the land, 64 points.
  Path path;
  Clear(&path);
  int added = 0;
  for (int i = 0; i < 64; ++i) {
    const float a = 6.2831853f * static_cast<float>(i) / 64.0f;
    const float p[3] = {1000.0f + 40.0f * cosf(a), 55.0f,
                        2000.0f + 40.0f * sinf(a)};
    if (Add(&path, p, 1.0f)) ++added;
  }
  Check(added == 64 && path.count == 64, "each point of the circle goes in",
        path.count, 64);
  float middle[3], widest = 0.0f;
  const bool have = CircleOf(path, middle, &widest);
  Check(have, "a path gives a circle", have ? 1 : 0, 1);
  Near("the middle of the circle: x", middle[0], 1000.0f, 0.5);
  Near("the middle of the circle: z", middle[2], 2000.0f, 0.5);
  Near("the middle keeps the height of the land", middle[1], 55.0f, 0.01);
  Near("its widest span is the diameter", widest, 80.0f, 0.1);
  Near("the game's size is 1.05 times the width", SizeOf(widest), 84.0f,
       0.2);

  // A slow part of the draw puts many points on one side. The middle of the
  // widest span does not move to that side, as a mean of the points does.
  Path slow;
  Clear(&slow);
  for (int i = 0; i < 48; ++i) {
    const float a = 0.5f * 3.1415927f * static_cast<float>(i) / 48.0f;
    const float p[3] = {40.0f * cosf(a), 0.0f, 40.0f * sinf(a)};
    Add(&slow, p, 0.0f);
  }
  for (int i = 1; i <= 12; ++i) {
    const float a = 0.5f * 3.1415927f +
                    1.5f * 3.1415927f * static_cast<float>(i) / 12.0f;
    const float p[3] = {40.0f * cosf(a), 0.0f, 40.0f * sinf(a)};
    Add(&slow, p, 0.0f);
  }
  CircleOf(slow, middle, &widest);
  Check(fabsf(middle[0]) < 3.0f && fabsf(middle[2]) < 3.0f,
        "a slow quarter does not pull the middle off the center",
        static_cast<int>(fabsf(middle[0]) + fabsf(middle[2])), 0);

  // A point nearer than the step does not go in.
  Path step_path;
  Clear(&step_path);
  const float p0[3] = {0.0f, 0.0f, 0.0f};
  const float p1[3] = {0.3f, 0.0f, 0.3f};
  const float p2[3] = {2.0f, 0.0f, 0.0f};
  Add(&step_path, p0, 1.0f);
  const bool inside_step = Add(&step_path, p1, 1.0f);
  const bool past_step = Add(&step_path, p2, 1.0f);
  Check(!inside_step && past_step && step_path.count == 2,
        "a point nearer than the step does not go in", step_path.count, 2);

  // A full path keeps each second point, and the newest point.
  Path full;
  Clear(&full);
  for (int i = 0; i < kMaxPath + 1; ++i) {
    const float p[3] = {static_cast<float>(i), 0.0f, 0.0f};
    Add(&full, p, 0.5f);
  }
  Check(full.count == kMaxPath / 2 + 1,
        "a full path keeps half of its points and adds the new one",
        full.count, kMaxPath / 2 + 1);
  Near("the first point stays", full.point[0][0], 0.0f, 0.001);
  Near("the newest point is the last", full.point[full.count - 1][0],
       static_cast<float>(kMaxPath), 0.001);
  Near("the point before it is the last of the old path",
       full.point[full.count - 2][0], static_cast<float>(kMaxPath - 1),
       0.001);

  // THE LIMITS of the spell, as InitWithPos keeps them.
  Near("a size under the least gets the least", SpellRadius(3.4f, 20.0f, 60.0f),
       20.0f, 0.001);
  Near("a size between the limits stays", SpellRadius(20.8f, 20.0f, 60.0f),
       20.8f, 0.001);
  Near("a size over the most gets the most", SpellRadius(150.0f, 20.0f, 60.0f),
       60.0f, 0.001);
  Near("a most of 0 is no limit", SpellRadius(150.0f, 20.0f, 0.0f), 150.0f,
       0.001);
  Check(TooLarge(61.0f, 60.0f) && !TooLarge(60.0f, 60.0f) &&
            !TooLarge(500.0f, 0.0f),
        "too large is only over the most", 1, 1);

  // THE PLAN. A ray to +z: forward is up on the screen, and +x is right.
  Plan plan;
  const float down_z[3] = {0.0f, -0.5f, 0.866f};
  const float land[3] = {500.0f, 30.0f, 700.0f};
  const bool planned =
      PlanFrom(down_z, land, 100.0f, 10.0f, 3600.0f, 2880.0f, &plan);
  Check(planned, "a ray with a render gives a plan", planned ? 1 : 0, 1);
  float x = 0.0f, y = 0.0f;
  PlanPixel(plan, land, &x, &y);
  Near("the first land point is the middle of the render: x", x, 1800.0f,
       0.01);
  Near("the first land point is the middle of the render: y", y, 1440.0f,
       0.01);
  // kSweepShare of the distance (35 units) is a third of the height.
  const float ahead[3] = {500.0f, 30.0f, 735.0f};
  PlanPixel(plan, ahead, &x, &y);
  Near("a point 35 units ahead is a third of the height up", y,
       1440.0f - 960.0f, 0.1);
  Near("and has the same x", x, 1800.0f, 0.01);
  const float side[3] = {535.0f, 30.0f, 700.0f};
  PlanPixel(plan, side, &x, &y);
  Near("a point to +x is to the right", x, 1800.0f + 960.0f, 0.1);
  // A ray to +x: its right is -z (up x forward, in the left handed world).
  const float to_x[3] = {1.0f, -0.4f, 0.0f};
  PlanFrom(to_x, land, 100.0f, 10.0f, 3600.0f, 2880.0f, &plan);
  const float minus_z[3] = {500.0f, 30.0f, 665.0f};
  PlanPixel(plan, minus_z, &x, &y);
  Near("for a ray to +x, a point to -z is to the right", x, 2760.0f, 0.1);
  Near("and is level with the start", y, 1440.0f, 0.1);
  // A ray at the feet does not give huge pixels.
  PlanFrom(down_z, land, 1.0f, 10.0f, 3600.0f, 2880.0f, &plan);
  Near("a ray of 1 unit takes the least distance (5 units)",
       plan.pixels_per_unit, 960.0f / (kSweepShare * 5.0f), 0.01);
  // A ray straight down takes +z as forward.
  const float straight[3] = {0.0f, -1.0f, 0.0f};
  PlanFrom(straight, land, 100.0f, 10.0f, 3600.0f, 2880.0f, &plan);
  Near("a ray straight down has +z as forward", plan.forward[1], 1.0f, 1e-6);
  Check(!PlanFrom(down_z, land, 100.0f, 10.0f, 3600.0f, 0.0f, &plan),
        "no render gives no plan", 0, 0);

  // A round path on the land is round in the plan: its pixel box is square.
  PlanFrom(down_z, land, 100.0f, 10.0f, 3600.0f, 2880.0f, &plan);
  float lo_x = 1e9f, hi_x = -1e9f, lo_y = 1e9f, hi_y = -1e9f;
  for (int i = 0; i < 32; ++i) {
    const float a = 6.2831853f * static_cast<float>(i) / 32.0f;
    const float p[3] = {500.0f + 20.0f * cosf(a), 30.0f,
                        720.0f + 20.0f * sinf(a)};
    PlanPixel(plan, p, &x, &y);
    if (x < lo_x) lo_x = x;
    if (x > hi_x) hi_x = x;
    if (y < lo_y) lo_y = y;
    if (y > hi_y) hi_y = y;
  }
  Near("a circle on the land is as wide as high in the plan",
       (hi_x - lo_x) / (hi_y - lo_y), 1.0f, 0.01);
}

}  // namespace circle_aim_checks
