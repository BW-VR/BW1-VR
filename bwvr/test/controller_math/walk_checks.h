// THE WALK WITH THE STICK (docs/god-view.md, "The walk with the stick"): the
// key, the push, the forward direction of the head and of the controller,
// the speed in the room, the step of a frame and the view after a step
// (walk_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/blinder_math.h"
#include "../../src/mod/table_math.h"
#include "../../src/mod/walk_math.h"
#include "../../src/mod/zoom_math.h"

namespace walk_checks {

// The matrix of a head in the room (the columns are its right, its up and its
// forward): yaw degrees to the right, then pitch degrees down, then roll
// degrees to the right.
inline void Head(float yaw_right, float pitch_down, float roll_right,
                 float out[9]) {
  const float k = 3.14159265f / 180.0f;
  const float cy = cosf(yaw_right * k), sy = sinf(yaw_right * k);
  const float cp = cosf(pitch_down * k), sp = sinf(pitch_down * k);
  const float cr = cosf(roll_right * k), sr = sinf(roll_right * k);
  const float right0[3] = {cy, 0.0f, -sy};
  const float up0[3] = {sy * sp, cp, cy * sp};
  const float forward[3] = {sy * cp, -sp, cy * cp};
  for (int i = 0; i < 3; ++i) {
    // A roll to the right puts the right axis down.
    out[i * 3 + 0] = cr * right0[i] - sr * up0[i];
    out[i * 3 + 1] = sr * right0[i] + cr * up0[i];
    out[i * 3 + 2] = forward[i];
  }
}

// THE FLY (move_type=1): the key, the kinds of the test switch, the level
// right, the speed in 3D, the step of a frame, the view after a step, and
// the ground limits of the grab on a fly down and up.
inline void FlyChecks() {
  using namespace table_math;
  using namespace walk_math;
  printf("\nThe fly with the stick\n");

  Check(Type(0) == kTypeWalk && Type(1) == kTypeFly && Type(5) == kTypeWalk &&
            Type(-1) == kTypeWalk,
        "move_type 1 is the fly, and each other value is the walk", Type(5),
        kTypeWalk);
  Check(Kind(0, 1) == kKindWalk && Kind(0, 0) == kKindWalkLevel &&
            Kind(1, 1) == kKindFly && Kind(1, 0) == kKindFly,
        "the kinds: the walk, the level walk, and the fly with each "
        "move_stick_follow_land", Kind(0, 0), kKindWalkLevel);
  Check(NextKind(kKindWalk) == kKindWalkLevel &&
            NextKind(kKindWalkLevel) == kKindFly &&
            NextKind(kKindFly) == kKindWalk,
        "the test switch goes from the walk to the level walk, to the fly and "
        "back", NextKind(kKindFly), kKindWalk);

  // A head that looks 45 degrees down: forward goes down and ahead.
  const float top = 1.5f;
  float head[9], forward[3], flat[2], right[2], speed[3];
  walk_checks::Head(0.0f, 45.0f, 0.0f, head);
  Check(zoom_math::HeadForward(head, forward) && HeadForward(head, flat),
        "a head 45 degrees down has a direction in 3D and on the land", 1, 1);
  Near("it points down 45 degrees", DegreesDown(forward), 45.0, 1e-3);
  const float fallback[2] = {flat[1], -flat[0]};
  FlyRight(forward, fallback, kAimLeast, right);
  Near("its level right is the right of the room, x", right[0], 1.0, 1e-5);
  Near("its level right is the right of the room, z", right[1], 0.0, 1e-5);
  const float ahead[2] = {0.0f, 1.0f};
  FlySpeed(forward, right, ahead, top, speed);
  Near("a full push forward goes down", speed[1], -top * 0.70711, 1e-4);
  Near("and ahead by the same speed", speed[2], top * 0.70711, 1e-4);
  Near("and not to the side", speed[0], 0.0, 1e-5);
  Near("the speed of a full push is the top speed",
       sqrtf(speed[0] * speed[0] + speed[1] * speed[1] + speed[2] * speed[2]),
       top, 1e-4);
  const float side[2] = {1.0f, 0.0f};
  FlySpeed(forward, right, side, top, speed);
  Near("a full push to the right moves level", speed[1], 0.0, 1e-6);
  Near("and to the right at the top speed", speed[0], top, 1e-5);
  const float back[2] = {0.0f, -1.0f};
  FlySpeed(forward, right, back, top, speed);
  Near("a full push back goes up and back", speed[1], top * 0.70711, 1e-4);

  // A head that looks straight down, turned 40 degrees to the right: the
  // flat part of forward is too short, so the level right of the head counts.
  walk_checks::Head(40.0f, 90.0f, 0.0f, head);
  zoom_math::HeadForward(head, forward);
  HeadForward(head, flat);
  const float down_right[2] = {flat[1], -flat[0]};
  FlyRight(forward, down_right, kAimLeast, right);
  Near("a look straight down strafes along the right of the head, x",
       right[0], cosf(40.0f * 3.14159265f / 180.0f), 1e-4);
  Near("a look straight down strafes along the right of the head, z",
       right[1], -sinf(40.0f * 3.14159265f / 180.0f), 1e-4);
  FlySpeed(forward, right, ahead, top, speed);
  Near("a full push forward with a look straight down goes straight down",
       speed[1], -top, 1e-4);

  // A controller that points 60 degrees up.
  const float k = 3.14159265f / 180.0f;
  const float up_aim[3] = {0.0f, sinf(60.0f * k) * 2.0f, cosf(60.0f * k) * 2.0f};
  float aim[3];
  Check(Unit(up_aim, aim), "a direction of length 2 gives a unit vector", 1,
        1);
  Near("the unit vector has length 1",
       sqrtf(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]), 1.0, 1e-5);
  const float zero[3] = {0.0f, 0.0f, 0.0f};
  Check(!Unit(zero, aim), "a direction with no length has no unit "
        "vector", 0, 0);
  Unit(up_aim, aim);
  Near("a controller 60 degrees up points up", DegreesDown(aim), -60.0, 1e-3);
  FlyRight(aim, fallback, kAimLeast, right);
  FlySpeed(aim, right, ahead, top, speed);
  Near("a full push forward goes up", speed[1], top * sinf(60.0f * k), 1e-4);

  // THE STEP OF A FRAME.
  float step[3];
  const float fast[3] = {0.0f, -1.5f, 1.5f};
  FlyStep(fast, 0.0f, step);
  Near("a frame with no time gives no step",
       fabsf(step[0]) + fabsf(step[1]) + fabsf(step[2]), 0.0, 1e-9);
  FlyStep(fast, 0.01f, step);
  Near("a frame of 10 ms goes 1.5 cm down", step[1], -0.015, 1e-6);
  const float still[2] = {0.0f, 0.0f};
  FlySpeed(forward, right, still, top, speed);
  Near("a stick at rest gives no speed",
       fabsf(speed[0]) + fabsf(speed[1]) + fabsf(speed[2]), 0.0, 1e-9);

  // THE VIEW AFTER A STEP. At 100 units per meter, a step of 10 cm down and
  // 20 cm ahead puts the waist point 10 units lower and 20 units ahead.
  TablePose pose;
  pose.center[0] = 100.0f;
  pose.center[1] = 50.0f;
  pose.center[2] = 200.0f;
  pose.heading = 0.0f;
  pose.scale = 100.0f;
  const float move[3] = {0.0f, -0.1f, 0.2f};
  TablePose next;
  Fly(pose, move, &next);
  Near("a step down lowers the waist point", next.center[1], 40.0, 1e-4);
  Near("a step ahead moves it ahead", next.center[2], 220.0, 1e-4);
  Near("and not to the side", next.center[0], 100.0, 1e-4);
  Near("the scale stays", next.scale, 100.0, 1e-6);
  // With the heading 90 degrees to the right, ahead in the room is +x.
  pose.heading = 90.0f * k;
  Fly(pose, move, &next);
  Near("with a heading of 90 degrees a step ahead goes along x",
       next.center[0], 120.0, 1e-3);
  Near("and the height goes down the same", next.center[1], 40.0, 1e-4);

  // THE CEILING. The land is at 0, the room and the head at 0. At 100 units
  // for each meter steps up go past 2.5 m, the limit of the grab, and stop
  // with the head 10000 units over the land (C.y 10000).
  const float ceiling = kFlyCeilingUnits;
  float y = 10.0f;
  for (int i = 0; i < 80; ++i) {
    y = FlyCeiling(y + 5.0f, y, 0.0f, 0.0f, 100.0f, 0.0f, ceiling);
  }
  Near("a fly up goes past 2.5 m below the head", y, 410.0, 1e-3);
  for (int i = 0; i < 400; ++i) {
    y = FlyCeiling(y + 50.0f, y, 0.0f, 0.0f, 100.0f, 0.0f, ceiling);
  }
  Near("and it stops with the head 10000 units over the land", y, 10000.0,
       1e-2);
  // At the villager scale (1 unit for each meter) the ceiling is the same
  // height in units: 10000 m of the room.
  y = 2.0f;
  for (int i = 0; i < 300; ++i) {
    y = FlyCeiling(y + 50.0f, y, 0.0f, 0.0f, 1.0f, 0.0f, ceiling);
  }
  Near("at 1 unit for each meter a fly up also stops at 10000 units", y,
       10000.0, 1e-2);
  Near("a view over the ceiling does not go down by itself",
       FlyCeiling(12000.0f, 12000.0f, 0.0f, 0.0f, 1.0f, 0.0f, ceiling),
       12000.0, 1e-3);
  Near("and it cannot go up",
       FlyCeiling(12100.0f, 12000.0f, 0.0f, 0.0f, 1.0f, 0.0f, ceiling),
       12000.0, 1e-3);
  Near("but it can go down",
       FlyCeiling(11900.0f, 12000.0f, 0.0f, 0.0f, 1.0f, 0.0f, ceiling),
       11900.0, 1e-3);

  // THE FLOOR, with the clearance of the head blinder.
  const float least = kFlyClearMeters;
  Check(least > blinder_math::kStartMeters,
        "the floor of the fly is above the start of the head blinder", 1, 1);
  Near("a step to 8 cm over the land goes up 2 cm",
       FlyFloor(50.0f, 0.08f, 0.12f, least, 100.0f), 52.0, 1e-4);
  Near("a step to 50 cm over the land stays",
       FlyFloor(50.0f, 0.50f, 0.12f, least, 100.0f), 50.0, 1e-6);
  Near("a head bent down to 4 cm is held at 4 cm on a step to 2 cm",
       FlyFloor(50.0f, 0.02f, 0.04f, least, 100.0f), 52.0, 1e-4);
  Near("and it is not lifted out on a step to 6 cm",
       FlyFloor(50.0f, 0.06f, 0.04f, least, 100.0f), 50.0, 1e-6);
  Near("a clearance that is no number changes nothing",
       FlyFloor(50.0f, sqrtf(-1.0f), 0.04f, least, 100.0f), 50.0, 1e-6);
  // A fly level across the land into a slope of 45 degrees, at 100 units
  // for each meter, steps of 2.5 cm of the room, with the head 20 cm over
  // the land at the start. The land rises from x = 100. The clearance is the
  // one of the head blinder: the highest land at the head and 6 cm around
  // it. The head never comes nearer than the floor, and it slides up along
  // the slope.
  {
    const auto land = [](float x) { return x > 100.0f ? x - 100.0f : 0.0f; };
    const auto clearance = [&](float x, float cy) {
      float points[blinder_math::kPoints][2];
      blinder_math::AroundPoints(x, 0.0f, 100.0f, points);
      float ground[blinder_math::kPoints];
      for (int i = 0; i < blinder_math::kPoints; ++i) {
        ground[i] = land(points[i][0]);
      }
      return blinder_math::Clearance(cy, ground, blinder_math::kPoints,
                                     100.0f);
    };
    float x = 0.0f, cy = 20.0f, worst = 1000.0f;
    for (int i = 0; i < 200; ++i) {
      const float before = clearance(x, cy);
      x += 2.5f;
      cy = FlyFloor(cy, clearance(x, cy), before, least, 100.0f);
      const float now = clearance(x, cy);
      if (now < worst) worst = now;
    }
    Near("a fly into a slope keeps the head at the floor or over it", worst,
         least, 1e-4);
    Near("and the view goes up along the slope", cy, land(x + 6.0f) + 10.0f,
         1e-2);
  }

  // THE SLIDE PAST A SOLID. A trunk of radius 1 on the axis x 0, z 0, and a
  // box of half size 1 at the middle (0, 0, 5). The shell is 0.3.
  {
    const auto trunk = [](const float q[3]) {
      return sqrtf(q[0] * q[0] + q[2] * q[2]) - 1.0f;
    };
    const auto box = [](const float q[3]) {
      const float c[3] = {q[0], q[1], q[2] - 5.0f};
      float out = 0.0f, in = -1e30f;
      for (int i = 0; i < 3; ++i) {
        const float d = fabsf(c[i]) - 1.0f;
        if (d > 0.0f) out += d * d;
        if (d > in) in = d;
      }
      return sqrtf(out) + (in < 0.0f ? in : 0.0f);
    };
    const float need = 0.3f, h = 0.001f;
    float out[3];
    const float p0[3] = {0.0f, 0.0f, -2.0f};
    const float far_step[3] = {0.0f, 0.0f, 0.1f};
    Check(FlySlide(trunk, p0, far_step, need, h, true, out) == kSlideFree &&
              out[2] == 0.1f,
          "a step that stays out of the shell goes as it is", 1, 1);
    const float rest[3] = {0.0f, 0.0f, 0.0f};
    const float at_shell[3] = {0.0f, 0.0f, -1.3f};
    FlySlide(trunk, at_shell, rest, need, h, true, out);
    Near("a step of 0 stays 0",
         fabsf(out[0]) + fabsf(out[1]) + fabsf(out[2]), 0.0, 1e-9);
    const float straight[3] = {0.0f, 0.0f, 0.05f};
    Check(FlySlide(trunk, at_shell, straight, need, h, true, out) ==
              kSlideStop,
          "a push straight at the middle of a trunk stops", 1, 1);
    Near("and it does not go into the shell", trunk(at_shell) - 0.3f +
         out[2], 0.0, 1e-3);
    const float slant[3] = {0.07f, 0.0f, 0.07f};
    Check(FlySlide(trunk, at_shell, slant, need, h, true, out) == kSlideAlong,
          "a push at a slant slides", 1, 1);
    Near("it keeps the part along the trunk", out[0], 0.07, 1e-3);
    Near("and loses the part into it, with no part away from it", out[2],
         0.0, 1e-3);
    // Many steps at 45 degrees past the trunk: the head goes round it and on,
    // and it never comes into the shell.
    float q[3] = {-1.0f, 0.0f, -2.0f};
    float worst = 1000.0f;
    int stops = 0;
    for (int i = 0; i < 200; ++i) {
      const float s45[3] = {0.02f, 0.0f, 0.02f};
      if (FlySlide(trunk, q, s45, need, h, true, out) == kSlideStop) ++stops;
      for (int j = 0; j < 3; ++j) q[j] += out[j];
      const float d = trunk(q);
      if (d < worst) worst = d;
    }
    Check(q[2] > 1.0f && stops == 0,
          "a fly at a slant goes round the trunk and past it", q[2], 1.0f);
    Near("and the head stays out of the shell", worst < 0.3f ? worst : 0.3f,
         0.3, 1e-3);
    // A fly forward and up at a wall of the box goes up the wall and over it.
    float w[3] = {0.0f, 0.0f, 3.5f};
    worst = 1000.0f;
    for (int i = 0; i < 120; ++i) {
      const float up[3] = {0.0f, 0.05f, 0.05f};
      FlySlide(box, w, up, need, h, true, out);
      for (int j = 0; j < 3; ++j) w[j] += out[j];
      const float d = box(w);
      if (d < worst) worst = d;
    }
    Check(w[2] > 6.3f && w[1] > 1.3f,
          "a fly forward and up at a wall goes up and over the house", w[2],
          6.3f);
    Near("and the head stays out of the shell of the house",
         worst < 0.3f ? worst : 0.3f, 0.3, 1e-3);
    const float wall[3] = {0.0f, 0.0f, 3.7f};
    Check(FlySlide(box, wall, straight, need, h, true, out) == kSlideStop,
          "a push straight at a wall stops", 1, 1);
    // fly_solid_slide=0: the step stops short along its own direction.
    Check(FlySlide(trunk, at_shell, slant, need, h, false, out) ==
              kSlideStop &&
              fabsf(out[0] - out[2]) < 1e-6f && out[0] < 0.07f,
          "with the slide off a push at a slant stops short", out[0], 0.07f);
  }

  // THE SPEED RAMP.
  Near("a full push counts its time", RampSeconds(0.5f, 1.0f, 0.25f), 0.75,
       1e-6);
  Near("a push of 90 percent counts", RampSeconds(0.5f, 0.9f, 0.25f), 0.75,
       1e-6);
  Near("a push of 85 percent sets the time to 0",
       RampSeconds(5.0f, 0.85f, 0.25f), 0.0, 1e-9);
  Near("a frame with no time keeps the time", RampSeconds(5.0f, 1.0f, 0.0f),
       5.0, 1e-6);
  Near("in the first second the speed stays", RampFactor(1.0f, 32.0f), 1.0,
       1e-6);
  Near("after 3 s it is 2 times", RampFactor(3.0f, 32.0f), 2.0, 1e-4);
  Near("after 5 s it is 4 times", RampFactor(5.0f, 32.0f), 4.0, 1e-4);
  Near("after 11 s it is 32 times", RampFactor(11.0f, 32.0f), 32.0, 1e-3);
  Near("and it stops at 32 times", RampFactor(30.0f, 32.0f), 32.0, 1e-6);
  Near("a most of 1 is no ramp", RampFactor(30.0f, 1.0f), 1.0, 1e-6);
  // At the villager scale a full push is 1.5 units each second. 1000 units
  // take about 29 s at 60 frames a second.
  {
    float held = 0.0f, went = 0.0f;
    int frames = 0;
    while (went < 1000.0f && frames < 60 * 120) {
      held = RampSeconds(held, 1.0f, 1.0f / 60.0f);
      went += 1.5f * RampFactor(held, 32.0f) / 60.0f;
      ++frames;
    }
    Near("at 1 unit for each meter, 1000 units take about 29 s",
         frames / 60.0f, 29.0, 0.5);
  }
}

inline void WalkChecks() {
  using namespace table_math;
  using namespace walk_math;
  printf("\nThe walk with the stick\n");

  // THE KEY.
  Check(Rule(-2) == kRuleOff && Rule(0) == kRuleOff && Rule(1) == kRuleHead &&
            Rule(2) == kRuleController && Rule(9) == kRuleController,
        "move_stick is 0, 1 or 2", Rule(9), kRuleController);
  Check(NextRule(1) == 2 && NextRule(2) == 0 && NextRule(0) == 1,
        "the test key goes from 1 to 2 to 0 and back to 1", NextRule(2), 0);

  // THE PUSH: the dead zone of the smooth zoom (0.3) and its full push
  // (0.95), along the length of the stick.
  const float dead = 0.3f, full = 0.95f;
  float push[2];
  Near("a stick at rest gives no push", Push(0.0f, 0.0f, dead, full, push),
       0.0, 1e-6);
  Near("a push of 0.3 forward is in the dead zone",
       Push(0.0f, 0.3f, dead, full, push), 0.0, 1e-6);
  Near("and it gives no vector", fabsf(push[0]) + fabsf(push[1]), 0.0, 1e-6);
  Near("a push of 0.625 forward is half of the top speed",
       Push(0.0f, 0.625f, dead, full, push), 0.5, 1e-5);
  Near("and it points forward", push[1], 0.5, 1e-5);
  Near("a full push forward is the top speed",
       Push(0.0f, 1.0f, dead, full, push), 1.0, 1e-6);
  Push(0.0f, -1.0f, dead, full, push);
  Near("a full push back points back", push[1], -1.0, 1e-6);
  Push(-1.0f, 0.0f, dead, full, push);
  Near("a full push to the left points left", push[0], -1.0, 1e-6);
  // A diagonal of the scripted stick: both axes at full.
  Near("a diagonal is not faster than a straight push",
       Push(1.0f, 1.0f, dead, full, push), 1.0, 1e-6);
  Near("a diagonal keeps its direction, x", push[0], 0.70711, 1e-4);
  Near("a diagonal keeps its direction, y", push[1], 0.70711, 1e-4);
  // A push of 0.2 on each axis is 0.283 long: in the dead zone, although no
  // axis is at rest.
  Near("a small diagonal is in the dead zone",
       Push(0.2f, 0.2f, dead, full, push), 0.0, 1e-6);
  const float nan = nanf("");
  Near("a value that is no number gives no push",
       Push(nan, 1.0f, dead, full, push), 0.0, 1e-6);

  // FORWARD OF THE HEAD.
  float head[9], forward[2];
  Head(0.0f, 0.0f, 0.0f, head);
  Check(HeadForward(head, forward), "a level head has a direction", 1, 1);
  Near("a head that looks straight ahead: x", forward[0], 0.0, 1e-5);
  Near("a head that looks straight ahead: z", forward[1], 1.0, 1e-5);
  Head(-30.0f, 0.0f, 0.0f, head);
  HeadForward(head, forward);
  Near("a head 30 degrees to the left: degrees", DegreesRight(forward), -30.0,
       1e-3);
  // The word "look" of the scripted controller: 30 degrees left, 20 down.
  Head(-30.0f, 20.0f, 0.0f, head);
  HeadForward(head, forward);
  Near("a look down does not change the direction", DegreesRight(forward),
       -30.0, 1e-3);
  Near("the direction is a unit vector",
       sqrtf(forward[0] * forward[0] + forward[1] * forward[1]), 1.0, 1e-5);
  Head(40.0f, 90.0f, 0.0f, head);
  Check(HeadForward(head, forward),
        "a head that looks straight down has a direction", 1, 1);
  Near("it is the direction of the top of the head", DegreesRight(forward),
       40.0, 1e-3);
  Head(0.0f, 0.0f, 90.0f, head);
  Check(!HeadForward(head, forward), "a head on its side has no direction", 0,
        0);

  // FORWARD OF A CONTROLLER.
  const float k = 3.14159265f / 180.0f;
  const float ahead[3] = {0.0f, 0.0f, 1.0f};
  Check(AimForward(ahead, 0.2f, forward), "a level controller has a direction",
        1, 1);
  Near("a controller that points ahead", DegreesRight(forward), 0.0, 1e-4);
  // 30 degrees to the right and 30 degrees down, as the left hand of the
  // scripted controller with fake_controller_yaw=30.
  const float aim[3] = {sinf(30.0f * k) * cosf(30.0f * k), -sinf(30.0f * k),
                        cosf(30.0f * k) * cosf(30.0f * k)};
  Check(AimForward(aim, 0.2f, forward), "a controller 30 degrees down", 1, 1);
  Near("points 30 degrees to the right on the land", DegreesRight(forward),
       30.0, 1e-3);
  Near("with a unit direction",
       sqrtf(forward[0] * forward[0] + forward[1] * forward[1]), 1.0, 1e-5);
  const float down[3] = {0.0f, -1.0f, 0.0f};
  forward[0] = forward[1] = 7.0f;
  Check(!AimForward(down, 0.2f, forward),
        "a controller that points straight down has no direction", 0, 0);
  Check(forward[0] == 7.0f && forward[1] == 7.0f,
        "and the direction of before is not written", 1, 1);
  // 80 degrees down: the flat part is 0.17 long.
  const float steep[3] = {0.0f, -sinf(80.0f * k), cosf(80.0f * k)};
  Check(!AimForward(steep, 0.2f, forward),
        "a controller 80 degrees down has no direction", 0, 0);
  const float nearly[3] = {0.0f, -sinf(75.0f * k), cosf(75.0f * k)};
  Check(AimForward(nearly, 0.2f, forward),
        "a controller 75 degrees down has a direction", 1, 1);

  // THE SPEED IN THE ROOM. The top speed is 1.5 m for each second.
  const float top = 1.5f;
  float speed[2];
  const float straight[2] = {0.0f, 1.0f};
  const float stick_forward[2] = {0.0f, 1.0f};
  const float stick_back[2] = {0.0f, -1.0f};
  const float stick_right[2] = {1.0f, 0.0f};
  const float stick_half[2] = {0.0f, 0.5f};
  Speed(straight, stick_forward, top, speed);
  Near("forward on the stick moves forward", speed[1], 1.5, 1e-6);
  Near("and not to the side", speed[0], 0.0, 1e-6);
  Speed(straight, stick_back, top, speed);
  Near("back on the stick moves back", speed[1], -1.5, 1e-6);
  Speed(straight, stick_right, top, speed);
  Near("right on the stick moves to the right", speed[0], 1.5, 1e-6);
  Near("and not forward", speed[1], 0.0, 1e-6);
  Speed(straight, stick_half, top, speed);
  Near("half of a push is half of the speed", speed[1], 0.75, 1e-6);
  const float left30[2] = {-sinf(30.0f * k), cosf(30.0f * k)};
  Speed(left30, stick_forward, top, speed);
  Near("with forward 30 degrees to the left, the move is 30 degrees left",
       DegreesRight(speed), -30.0, 1e-3);
  Near("at the top speed", sqrtf(speed[0] * speed[0] + speed[1] * speed[1]),
       1.5, 1e-5);
  Speed(left30, stick_right, top, speed);
  Near("and the stick to the right moves 90 degrees to the right of that",
       DegreesRight(speed), 60.0, 1e-3);
  const float right30[2] = {sinf(30.0f * k), cosf(30.0f * k)};
  Speed(right30, stick_forward, top, speed);
  Near("with forward 30 degrees to the right, the move is 30 degrees right",
       DegreesRight(speed), 30.0, 1e-3);

  // THE STEP OF A FRAME, and a hold of one second at 90 and at 45 frames a
  // second: the distance follows the time and not the frame rate.
  float step[2];
  Speed(straight, stick_forward, top, speed);
  Step(speed, 0.0f, step);
  Near("a frame with no time gives no step", fabsf(step[0]) + fabsf(step[1]),
       0.0, 1e-9);
  Step(speed, 1.0f / 90.0f, step);
  Near("a frame of 11 ms at 1.5 m/s is 1.67 cm", step[1] * 100.0, 1.6667,
       1e-3);
  float far90 = 0.0f, far45 = 0.0f;
  for (int i = 0; i < 90; ++i) {
    Step(speed, zoom_math::FrameSeconds(1.0 / 90.0, 0.05f), step);
    far90 += step[1];
  }
  for (int i = 0; i < 45; ++i) {
    Step(speed, zoom_math::FrameSeconds(1.0 / 45.0, 0.05f), step);
    far45 += step[1];
  }
  Near("one second at 90 frames a second is 1.5 m", far90, 1.5, 1e-4);
  Near("one second at 45 frames a second is 1.5 m", far45, 1.5, 1e-4);
  Step(speed, zoom_math::FrameSeconds(0.5, 0.05f), step);
  Near("a frame of 0.5 s moves as a frame of 50 ms: 7.5 cm", step[1] * 100.0,
       7.5, 1e-4);
  // The stick comes back: the frame has no push, so it has no step.
  const float rest[2] = {0.0f, 0.0f};
  Speed(straight, rest, top, speed);
  Step(speed, 1.0f / 90.0f, step);
  Near("a stick that is back gives no step in that frame",
       fabsf(step[0]) + fabsf(step[1]), 0.0, 1e-9);

  // THE VIEW AFTER A STEP. The God view at 100 units for each meter, the
  // waist point 60 cm below the recenter pose.
  float room[3];
  GodRoom(0.6f, room);
  TablePose pose;
  pose.center[0] = 2000.0f;
  pose.center[1] = 30.0f;
  pose.center[2] = 2500.0f;
  pose.heading = 0.0f;
  pose.scale = 100.0f;
  const float ten_cm[2] = {0.0f, 0.1f};
  TablePose next;
  Walk(pose, ten_cm, &next);
  Near("10 cm forward at 100 units a meter is 10 units: z", next.center[2],
       2510.0, 1e-3);
  Near("and x stays", next.center[0], 2000.0, 1e-3);
  Check(next.center[1] == pose.center[1] && next.heading == pose.heading &&
            next.scale == pose.scale,
        "the height, the heading and the scale stay", 1, 1);
  // The land point that was 10 cm in front of the head is under the head.
  const float head_room[3] = {0.05f, 0.0f, -0.02f};
  const float ahead_room[3] = {0.05f, 0.0f, 0.08f};
  float was[3], is[3];
  PoseWorld(pose, room, ahead_room, was);
  PoseWorld(next, room, head_room, is);
  Near("the land that was 10 cm ahead is under the head: x", is[0], was[0],
       1e-3);
  Near("the land that was 10 cm ahead is under the head: z", is[2], was[2],
       1e-3);
  // A heading of 90 degrees: the room forward is world +x.
  pose.heading = 90.0f * k;
  Walk(pose, ten_cm, &next);
  Near("with a heading of 90 degrees, forward is world +x", next.center[0],
       2010.0, 1e-3);
  Near("and z stays", next.center[2], 2500.0, 1e-3);
  // The speed is a speed of the room: the same step is more land far out.
  pose.heading = 0.0f;
  pose.scale = 4000.0f;
  Walk(pose, ten_cm, &next);
  Near("10 cm at 4000 units a meter is 400 units", next.center[2] - 2500.0f,
       400.0, 1e-2);
  pose.scale = 7.5f;
  Walk(pose, ten_cm, &next);
  Near("10 cm at the life size scale is 0.75 units", next.center[2] - 2500.0f,
       0.75, 1e-4);

  // THE SAME MOTION AS A PULL OF THE WORLD: a grab of one hand that pulls
  // the land 10 cm back and 4 cm to the left gives the same view as a step
  // of 10 cm forward and 4 cm to the right.
  pose.heading = 35.0f * k;
  pose.scale = 66.667f;
  TableGrab grab;
  memset(&grab, 0, sizeof(grab));
  grab.hands = 1;
  grab.pose = pose;
  grab.middle[0] = -0.2f;
  grab.middle[1] = -0.3f;
  grab.middle[2] = 0.35f;
  const float hand_now[3] = {-0.2f - 0.04f, -0.3f, 0.35f - 0.1f};
  const float no_spread[3] = {0.0f, 0.0f, 0.0f};
  float theta = 0.0f, zoom = 1.0f;
  TablePose pulled;
  TableGrabSolve(grab, room, hand_now, no_spread, false, false, 0.08f, 7.5f,
                 4000.0f, &theta, &zoom, &pulled);
  const float walk_step[2] = {0.04f, 0.1f};
  Walk(pose, walk_step, &next);
  Near("a step is the pan of a grab: x", next.center[0], pulled.center[0],
       1e-3);
  Near("a step is the pan of a grab: z", next.center[2], pulled.center[2],
       1e-3);
  float back[2];
  InRoom(pose, next.center[0] - pose.center[0],
         next.center[2] - pose.center[2], back);
  Near("the move in the room from the move in the world: x", back[0], 0.04,
       1e-5);
  Near("the move in the room from the move in the world: z", back[1], 0.1,
       1e-5);

  // A HILL AT THE NEW PLACE (zoom_math::ClearHead). The head is 5 cm above
  // the recenter pose, and the land at the new place is 20 units higher than
  // the waist point: 30 cm at this scale, so 25 cm below the head.
  const float head_y = 0.05f;
  Near("a low hill at the new place leaves the land where it is",
       zoom_math::ClearHead(next.center[1], next.center[1] + 20.0f, room[1],
                            next.scale, head_y, 0.10f),
       next.center[1], 1e-4);
  // A hill of 40 units is 60 cm: 5 cm above the head. The land goes 15 cm
  // down, so the hill is 10 cm below the head.
  const float cleared = zoom_math::ClearHead(
      next.center[1], next.center[1] + 40.0f, room[1], next.scale, head_y,
      0.10f);
  Near("a hill that would be above the head goes 10 cm below it",
       head_y - (room[1] + (next.center[1] + 40.0f - cleared) / next.scale),
       0.10, 1e-4);

  // THE LAND THAT THE VIEW FOLLOWS (FollowStep). Heights in world units, the
  // way in meters of the room, 25 cm for each stage.
  const float length = kFollowLength;
  LandFollow follow;
  FollowStart(30.0f, &follow);
  Near("the start keeps the land where it is: a step over flat land moves "
       "nothing", FollowStep(&follow, 30.0f, 0.02f, length), 0.0, 1e-6);
  Near("a frame with no step moves nothing, also over a hill",
       FollowStep(&follow, 80.0f, 0.0f, length), 0.0, 1e-6);
  Near("and the hill of that frame is not kept", follow.stage[0], 30.0, 1e-5);
  // One step of 10 cm onto land 10 units higher: the exact answer of two
  // stages, 1 - e^-x (1 + x) with x = 0.4.
  const float one = FollowStep(&follow, 40.0f, 0.10f, length);
  const double x = 0.4;
  Near("a step onto higher land starts slowly (two stages)", one,
       10.0 * (1.0 - exp(-x) * (1.0 + x)), 1e-4);
  LandFollow split;
  FollowStart(30.0f, &split);
  float sum = 0.0f;
  for (int i = 0; i < 10; ++i) sum += FollowStep(&split, 40.0f, 0.01f, length);
  Near("ten steps of 1 cm give what one step of 10 cm gives", sum, one, 1e-4);
  // A long climb: the land rises 20 units for each meter of the room. After
  // 3 m the view rises with the land, 0.2 units for each step of 1 cm, and
  // it is 2 * 0.25 m of the climb behind it: 10 units.
  LandFollow climb;
  FollowStart(0.0f, &climb);
  float rise = 0.0f, land = 0.0f;
  for (int i = 0; i < 300; ++i) {
    land += 0.2f;
    rise = FollowStep(&climb, land, 0.01f, length);
  }
  Near("on a long hill the view rises with the land", rise, 0.2, 2e-3);
  Near("and it is the way of two stages behind it", land - climb.stage[1],
       20.0 * 2.0 * 0.25, 0.3);
  // A bump: 20 units for 10 cm of the way, then the land of before. The
  // view rises a little and comes back down: the bump does not shake it.
  LandFollow bump;
  FollowStart(0.0f, &bump);
  float peak = 0.0f, at = 0.0f;
  for (int i = 0; i < 100; ++i) {
    const float under = (i >= 10 && i < 20) ? 20.0f : 0.0f;
    at += FollowStep(&bump, under, 0.01f, length);
    if (at > peak) peak = at;
  }
  Check(peak < 20.0f * 0.15f, "a bump of 10 cm moves the view less than 15 "
        "percent of its height", peak, 20.0f * 0.15f);
  Near("and 80 cm after the bump the view is near its height of before", at,
       0.0, 20.0 * 0.05);
  Near("a step with a height that is not a number moves nothing",
       FollowStep(&bump, sqrtf(-1.0f), 0.01f, length), 0.0, 1e-6);

  // THE SPEED BY THE SCALE (ScaleFactor, move_stick_scale), with the
  // defaults: ref 16 units per meter, power 0.5, low 0.6, high 4.
  Near("at the villager scale (1) the stick is 4 times faster",
       ScaleFactor(1.0f, 16.0f, 0.5f, 0.6f, 4.0f), 4.0, 1e-5);
  Near("at the life size scale (7.5) 1.46 times",
       ScaleFactor(7.5f, 16.0f, 0.5f, 0.6f, 4.0f), 1.46059, 1e-4);
  Near("at 16 the speed of the key",
       ScaleFactor(16.0f, 16.0f, 0.5f, 0.6f, 4.0f), 1.0, 1e-6);
  Near("at 30 0.73 times", ScaleFactor(30.0f, 16.0f, 0.5f, 0.6f, 4.0f),
       0.73030, 1e-4);
  Near("at 100 the least factor, 0.6",
       ScaleFactor(100.0f, 16.0f, 0.5f, 0.6f, 4.0f), 0.6, 1e-6);
  Near("at 4000 the least factor too",
       ScaleFactor(4000.0f, 16.0f, 0.5f, 0.6f, 4.0f), 0.6, 1e-6);
  Near("a largest factor of 2 holds the villager scale at 2",
       ScaleFactor(1.0f, 16.0f, 0.5f, 0.6f, 2.0f), 2.0, 1e-6);
  Near("a scale of 0 gives 1", ScaleFactor(0.0f, 16.0f, 0.5f, 0.6f, 4.0f),
       1.0, 1e-6);
  Near("a power of 0 gives 1 at each scale",
       ScaleFactor(3.0f, 16.0f, 0.0f, 0.6f, 4.0f), 1.0, 1e-6);
  {
    // Smooth and with no steps: the factor never grows with the scale, and
    // a step of the scale of 1 percent changes it by at most 0.5 percent.
    bool falls = true, smooth = true;
    float before = ScaleFactor(1.0f, 16.0f, 0.5f, 0.6f, 4.0f);
    for (float scale = 1.01f; scale < 4000.0f; scale *= 1.01f) {
      const float now = ScaleFactor(scale, 16.0f, 0.5f, 0.6f, 4.0f);
      if (now > before + 1e-6f) falls = false;
      if (fabsf(now - before) > 0.0051f * before) smooth = false;
      before = now;
    }
    Check(falls, "the factor goes down as the scale goes up, 1 to 4000",
          falls, 1);
    Check(smooth, "and it has no step", smooth, 1);
  }

  // WHEN THE FILTER STARTS AGAIN (FollowRestart, move_stick_follow_place).
  // The arguments: the rule, have, the version changed, the view moved, a
  // place at a camera of the game, a recenter.
  const int each = kFollowPlaceEach, version = kFollowPlaceVersion;
  Check(FollowRestart(each, false, false, false, false, false) &&
            FollowRestart(version, false, false, false, false, false),
        "the first walk starts the filter with each rule", 1, 1);
  Check(FollowRestart(each, true, true, false, true, false),
        "rule 1: a place at a camera of the game starts the filter again",
        1, 1);
  Check(!FollowRestart(version, true, false, false, true, false),
        "rule 0: a place at a camera of the game does not", 1, 1);
  Check(FollowRestart(each, true, false, false, false, true) &&
            !FollowRestart(version, true, false, false, false, true),
        "a recenter starts it again with rule 1 only", 1, 1);
  Check(!FollowRestart(each, true, true, false, false, false),
        "rule 1: a turn about the head keeps the filter", 1, 1);
  Check(FollowRestart(version, true, true, false, false, false),
        "rule 0: a turn starts it again", 1, 1);
  Check(FollowRestart(each, true, true, true, false, false) &&
            FollowRestart(version, true, true, true, false, false),
        "a move of the view starts it again with each rule", 1, 1);
  Check(!FollowRestart(each, true, false, false, false, false) &&
            !FollowRestart(version, true, false, false, false, false),
        "the next frame of a walk keeps it with each rule", 1, 1);
  // His session: the last walk stopped on land at 18.0, and a camera of the
  // game put the view over land at 31.2, at 4.41 units per meter. A filter
  // that goes on lifts the view by the difference over the next walks. A
  // filter that starts again lifts nothing over flat land.
  LandFollow stale;
  FollowStart(18.0f, &stale);
  float lifted = 0.0f;
  for (int i = 0; i < 400; ++i) lifted += FollowStep(&stale, 31.2f, 0.01f,
                                                    length);
  Near("a filter that goes on after a place lifts the view 2.99 m",
       lifted / 4.41f, 2.993, 0.01);
  LandFollow fresh;
  FollowStart(31.2f, &fresh);
  float none = 0.0f;
  for (int i = 0; i < 400; ++i) none += FollowStep(&fresh, 31.2f, 0.01f,
                                                  length);
  Near("a filter that starts again at the place lifts nothing", none, 0.0,
       1e-5);
  FlyChecks();
}

}  // namespace walk_checks
