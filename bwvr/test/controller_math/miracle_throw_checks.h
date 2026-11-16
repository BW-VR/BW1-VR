// A THROWN MIRACLE (docs/throw.md, "A thrown miracle").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/miracle_throw_math.h"
#include "../../src/mod/throw_math.h"

namespace miracle_throw_checks {

// The velocity of a swing of 0.3 s with a top speed of 4 m/s at 0.15 s, in
// the room's axes (meters): the speed is 4 sin(pi t / 0.3), and the direction
// turns 70 degrees to the left in the swing, from straight ahead (+z).
inline void SwingVelocity(double t, double v[3]) {
  const double kPi = 3.14159265358979;
  v[0] = v[1] = v[2] = 0.0;
  if (t < 0.0 || t > 0.3) return;
  const double speed = 4.0 * sin(kPi * t / 0.3);
  const double angle = -70.0 * t / 0.3 * kPi / 180.0;
  v[0] = speed * sin(angle);
  v[2] = speed * cos(angle);
}

inline void MiracleThrowChecks() {
  using namespace miracle_throw_math;
  printf("\nA thrown miracle\n");

  // THE SIDE OF A DIRECTION, in the axes of the game (x right, y up, z
  // forward).
  const float ahead[3] = {0.0f, 0.0f, 1.0f};
  const float right[3] = {1.0f, 0.0f, 0.0f};
  const float left[3] = {-1.0f, 0.0f, 0.0f};
  const float up_ahead[3] = {0.0f, 1.0f, 1.0f};
  const float straight_up[3] = {0.0f, 1.0f, 0.0f};
  Near("a direction along +x is 90 degrees to the right of +z",
       SideDegrees(ahead, right), 90.0, 0.01);
  Near("a direction along -x is 90 degrees to the left of +z",
       SideDegrees(ahead, left), -90.0, 0.01);
  Near("+z is to the left of +x", SideDegrees(right, ahead), -90.0, 0.01);
  Near("a direction up and ahead is not to the side of ahead",
       SideDegrees(ahead, up_ahead), 0.0, 0.01);
  Check(SideDegrees(ahead, straight_up) == 0.0f,
        "straight up has no side", SideDegrees(ahead, straight_up), 0.0);
  Near("up and ahead is 45 degrees up", UpDegrees(up_ahead), 45.0, 0.01);

  // THE GAME'S FILTER. A constant velocity for one time constant (1 / r, 62
  // ms) in steps of 1 ms gives 1 - 1/e of it, and dt = 0 changes nothing.
  {
    float f[3] = {0.0f, 0.0f, 0.0f};
    const float v[3] = {0.0f, 0.0f, 10.0f};
    const int steps = static_cast<int>(1000.0f / kGrainRate + 0.5f);
    for (int k = 0; k < steps; ++k) GrainFilterStep(f, v, 0.001f);
    Near("the game's filter has 63 percent of a step after 62 ms", f[2],
         10.0 * (1.0 - exp(-1.0)), 0.1);
    const float before = f[2];
    GrainFilterStep(f, v, 0.0f);
    Check(f[2] == before, "an update of no time changes nothing", f[2],
          before);
  }

  // THE VELOCITY IN THE WORLD. A room turned 90 degrees about up (its +z is
  // the world's +x), 100 units in a meter at the life size scale, and 34.3
  // units in a meter in the world: a swing of 3 m/s ahead in the room is
  // 102.9 units a second along world +x.
  {
    const float room[9] = {0.0f, 0.0f, 1.0f,   //
                           0.0f, 1.0f, 0.0f,   //
                           -1.0f, 0.0f, 0.0f};
    const float peak_room[3] = {0.0f, 0.0f, 300.0f};
    float world[3];
    SwingWorld(peak_room, room, 100.0f, 34.3f, world);
    Near("3 m/s ahead in a turned room is 102.9 units a second along +x",
         world[0], 102.9, 0.01);
    Near("and nothing across it", fabs(world[1]) + fabs(world[2]), 0.0, 1e-4);
    SwingWorld(peak_room, room, 0.0f, 34.3f, world);
    Check(world[0] == 0.0f && world[2] == 0.0f,
          "no scale gives no velocity", world[0], 0.0);
  }

  // A SWING THAT CURVES. The swing above, sampled at 90 Hz, with the grip
  // open at the top speed and the cast one frame later. The game's filter of
  // the Hand gives a direction of the past: to the right of the swing at its
  // top speed, and slower. The top speed of the grip (the throw of an object)
  // has the direction and the speed of the swing.
  {
    const double dt = 1.0 / 90.0;
    const int count = 54;
    controller_math::MotionSample samples[count];
    double p[3] = {0.0, 0.0, 0.0};
    for (int k = 0; k < count; ++k) {
      samples[k].time = k * dt;
      for (int i = 0; i < 3; ++i) samples[k].point[i] = static_cast<float>(p[i]);
      double v[3];
      SwingVelocity(k * dt, v);
      for (int i = 0; i < 3; ++i) p[i] += v[i] * dt;
    }
    const double opened = 13 * dt;  // the sample nearest to the top speed
    const double cast = opened + dt;
    float filtered[3] = {0.0f, 0.0f, 0.0f};
    for (int k = 1; k < count && samples[k].time <= cast + 1e-9; ++k) {
      float v[3];
      for (int i = 0; i < 3; ++i) {
        v[i] = static_cast<float>(
            (samples[k].point[i] - samples[k - 1].point[i]) / dt);
      }
      GrainFilterStep(filtered, v, static_cast<float>(dt));
    }
    float peak[3];
    double at = 0.0;
    const bool found = throw_math::PeakVelocity(
        samples, count, opened - throw_math::kPeakWindow, cast,
        throw_math::kPeakHalfSpan, peak, &at);
    double top_d[3];
    SwingVelocity(0.15, top_d);
    const float top[3] = {static_cast<float>(top_d[0]), 0.0f,
                          static_cast<float>(top_d[2])};
    const float game_side = SideDegrees(top, filtered);
    const float game_speed = controller_math::Length(filtered);
    const float peak_side = found ? SideDegrees(top, peak) : 99.0f;
    const float peak_speed = found ? controller_math::Length(peak) : 0.0f;
    Check(game_side > 6.0f && game_side < 12.0f,
          "a curved swing: the game's filter is 6 to 12 degrees to the right",
          game_side, 8.6);
    Check(game_speed < 0.8f * 4.0f,
          "a curved swing: the game's filter has less than 80 percent of the "
          "top speed",
          game_speed, 2.95);
    Check(found && fabsf(peak_side) < 4.0f,
          "a curved swing: the top speed of the grip is within 4 degrees of "
          "the swing",
          peak_side, 0.0);
    Check(peak_speed > 3.9f,
          "a curved swing: the top speed of the grip is the speed of the swing",
          peak_speed, 4.0);
  }
}

}  // namespace miracle_throw_checks
