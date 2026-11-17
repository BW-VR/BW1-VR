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

// THE FLIGHT OF A THROWN FIREBALL (miracle_flight=1). The arc of ball 0 is
// the arc of a thrown object for the same swing, scale and k: the plan of
// the ball and the plan of the object (throw_math::RealThrowPlan) agree, and
// the steps of the rule of the game, with the start velocity of the mod, stay
// on that arc.
inline void BallFlightChecks() {
  using namespace miracle_throw_math;
  printf("\nThe flight of a thrown Fireball\n");
  const float room[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                         0.0f, 0.0f, 1.0f};
  const float life = 7.5f;
  // A throw of 5 m/s at 40 degrees from 1.0 m over flat land: a real flight
  // of 0.885 s that lands 3.39 m away (the checks of throw_checks.h).
  const float angle = 40.0f / 57.29578f;
  const float v_room[3] = {0.0f, 5.0f * sinf(angle) * life,
                           5.0f * cosf(angle) * life};
  const float scales[] = {7.5f, 44.4f, 100.0f, 1000.0f};
  const float slows[] = {1.0f, 2.0f, 4.0f};
  for (float s : scales) {
    for (float k : slows) {
      throw_math::RealThrow rt;
      throw_math::RealThrowPlan(v_room, v_room, room, 1.5f, life, s, 124.0f,
                                2.0f, &rt, k);
      BallFlight bf;
      BallFlightPlan(rt.world, s, k, &bf);
      char name[160];
      sprintf_s(name, sizeof(name),
                "at %.1f units per meter and k %.0f: the ball has the k of "
                "an object", s, k);
      Near(name, bf.slow, rt.slow, 1e-6);
      float worst_v = 0.0f;
      for (int i = 0; i < 3; ++i) {
        worst_v = fmaxf(worst_v, fabsf(bf.v0[i] - rt.flight[i]));
      }
      sprintf_s(name, sizeof(name),
                "at %.1f units per meter and k %.0f: ball 0 starts with the "
                "velocity of the flight of an object", s, k);
      Check(worst_v <= 1e-4f * (1.0f + controller_math::Length(rt.flight)),
            name, worst_v, 0.0);
      sprintf_s(name, sizeof(name),
                "at %.1f units per meter and k %.0f: the gravity of the ball "
                "is the gravity of the flight of an object", s, k);
      Near(name, bf.gravity, rt.gravity * throw_math::kGravity,
           1e-4 * bf.gravity);
      // The steps of the rule, from the start velocity of the mod, against
      // the arc of the object. The error is in meters of the room.
      const float p0[3] = {10.0f, 1.0f * s, 20.0f};
      float p[3] = {p0[0], p0[1], p0[2]};
      const float dt = kBallTurn / static_cast<float>(bf.steps);
      float v[3];
      RuleStartVelocity(bf.v0, bf.gravity, dt, v);
      double t = 0.0;
      float worst = 0.0f;
      const double real_time = 0.885 * rt.slow;
      while (t < real_time) {
        RuleFreeStep(p, v, bf.gravity, dt);
        t += dt;
        float want[3], vw[3];
        throw_math::RealArc(p0, rt.flight, rt.gravity, t, want, vw);
        float d[3] = {p[0] - want[0], p[1] - want[1], p[2] - want[2]};
        worst = fmaxf(worst, controller_math::Length(d) / s);
      }
      sprintf_s(name, sizeof(name),
                "at %.1f units per meter and k %.0f: %d steps of the rule a "
                "turn stay within 1 mm of the arc of the object", s, k,
                bf.steps);
      Check(worst < 0.001f, name, worst, 0.0);
      sprintf_s(name, sizeof(name),
                "at %.1f units per meter and k %.0f: a step of the rule "
                "moves the ball 2 cm of the room or less, or the turn has %d "
                "steps", s, k, kBallMostSteps);
      const float step_room =
          (controller_math::Length(bf.v0) + 0.5f * bf.gravity) * dt / s;
      Check(step_room <= kBallStepRoom + 1e-4f || bf.steps == kBallMostSteps,
            name, step_room, kBallStepRoom);
      // Where the arc meets flat land at 0.
      double land_t = 0.0;
      float at[3];
      const bool meets = BallArcMeetsLand(
          p0, bf.v0, bf.gravity, 0.0, 0.01, 30.0,
          [](float, float) { return 0.0f; }, &land_t, at);
      sprintf_s(name, sizeof(name),
                "at %.1f units per meter and k %.0f: the arc meets the land "
                "after k times 0.885 s", s, k);
      Check(meets && fabs(land_t - real_time) < 0.002 * rt.slow, name, land_t,
            real_time);
      sprintf_s(name, sizeof(name),
                "at %.1f units per meter and k %.0f: the arc meets the land "
                "3.39 m of the room away", s, k);
      const float across = sqrtf((at[0] - p0[0]) * (at[0] - p0[0]) +
                                 (at[2] - p0[2]) * (at[2] - p0[2])) / s;
      Near(name, across, 3.39, 0.01);
    }
  }
  // A LIGHT BALL: 40 percent of the gravity and 130 percent of the speed.
  // The same throw then flies 1.3 times the velocity of the object under 0.4
  // times its gravity, in the same direction. From 1.0 m over flat land the
  // arc in the room is the arc of 6.5 m/s at 40 degrees under 3.924 m/s^2:
  // 2.347 s of the real flight and 11.68 m, at each scale.
  for (float s : scales) {
    const float k = 2.0f;
    throw_math::RealThrow rt;
    throw_math::RealThrowPlan(v_room, v_room, room, 1.5f, life, s, 124.0f,
                              2.0f, &rt, k);
    BallFlight bf;
    BallFlightPlan(rt.world, s, k, &bf, 0.4f, 1.3f);
    char name[160];
    float worst_v = 0.0f;
    for (int i = 0; i < 3; ++i) {
      worst_v = fmaxf(worst_v, fabsf(bf.v0[i] - 1.3f * rt.flight[i]));
    }
    sprintf_s(name, sizeof(name),
              "at %.1f units per meter, a light ball starts with 1.3 times "
              "the velocity of an object", s);
    Check(worst_v <= 1e-4f * (1.0f + controller_math::Length(rt.flight)),
          name, worst_v, 0.0);
    sprintf_s(name, sizeof(name),
              "at %.1f units per meter, a light ball has 0.4 times the "
              "gravity of an object", s);
    Near(name, bf.gravity, 0.4f * rt.gravity * throw_math::kGravity,
         1e-4 * bf.gravity);
    const float p0[3] = {10.0f, 1.0f * s, 20.0f};
    float p[3] = {p0[0], p0[1], p0[2]};
    const float dt = kBallTurn / static_cast<float>(bf.steps);
    float v[3];
    RuleStartVelocity(bf.v0, bf.gravity, dt, v);
    const double vy = 6.5 * sin(40.0 / 57.29578), vx = 6.5 * cos(40.0 / 57.29578);
    const double g = 0.4 * throw_math::kGravity;
    const double real_time = (vy + sqrt(vy * vy + 2.0 * g * 1.0)) / g;
    float worst = 0.0f;
    for (double t = 0.0; t < real_time * rt.slow;) {
      RuleFreeStep(p, v, bf.gravity, dt);
      t += dt;
      float want[3], vw[3];
      BallArcAt(p0, bf.v0, bf.gravity, t, want, vw);
      float d[3] = {p[0] - want[0], p[1] - want[1], p[2] - want[2]};
      worst = fmaxf(worst, controller_math::Length(d) / s);
    }
    sprintf_s(name, sizeof(name),
              "at %.1f units per meter, %d steps of the rule a turn stay "
              "within 1 mm of the arc of a light ball", s, bf.steps);
    Check(worst < 0.001f, name, worst, 0.0);
    const float step_room =
        (controller_math::Length(bf.v0) + 0.5f * bf.gravity) * dt / s;
    sprintf_s(name, sizeof(name),
              "at %.1f units per meter, a step of a light ball moves it 2.5 "
              "cm of the room or less", s);
    Check(step_room <= 0.025f, name, step_room, 0.025);
    double land_t = 0.0;
    float at[3];
    const bool meets = BallArcMeetsLand(
        p0, bf.v0, bf.gravity, 0.0, 0.01, 30.0,
        [](float, float) { return 0.0f; }, &land_t, at);
    sprintf_s(name, sizeof(name),
              "at %.1f units per meter, the arc of a light ball meets the "
              "land after k times 2.347 s", s);
    Check(meets && fabs(land_t - real_time * rt.slow) < 0.002 * rt.slow, name,
          land_t, real_time * rt.slow);
    sprintf_s(name, sizeof(name),
              "at %.1f units per meter, the arc of a light ball meets the "
              "land 11.68 m of the room away", s);
    const float across = sqrtf((at[0] - p0[0]) * (at[0] - p0[0]) +
                               (at[2] - p0[2]) * (at[2] - p0[2])) / s;
    Near(name, across, vx * real_time, 0.01);
    Near("the range of a light ball is 11.68 m", vx * real_time, 11.68, 0.01);
  }
  // A share of the gravity under 5 percent is cut to 5 percent.
  {
    const float cast[3] = {0.0f, 10.0f, 30.0f};
    BallFlight bf, rock;
    BallFlightPlan(cast, 44.4f, 2.0f, &bf, 0.0f, 1.0f);
    BallFlightPlan(cast, 44.4f, 2.0f, &rock);
    Near("a gravity share of 0 is cut to 5 percent", bf.gravity,
         0.05f * rock.gravity, 1e-4 * rock.gravity);
  }
  // At 7.5 units per meter k = 4 is cut to sqrt(7.5) = 2.74, as for an
  // object.
  {
    const float cast[3] = {0.0f, 10.0f, 30.0f};
    BallFlight bf;
    BallFlightPlan(cast, 7.5f, 4.0f, &bf);
    Near("at 7.5 units per meter k 4 is cut to 2.74", bf.slow, 2.7386, 1e-3);
  }
  // A long throw has the most steps.
  {
    const float cast[3] = {0.0f, 0.0f, 100.0f * 20.0f};
    BallFlight bf;
    BallFlightPlan(cast, 100.0f, 1.0f, &bf);
    Check(bf.steps == kBallMostSteps,
          "a throw of 20 m/s at k 1 has the most steps of the rule in a turn",
          bf.steps, kBallMostSteps);
  }

  // A FREE STEP AND A CONTACT. The rule in the air takes g dt off the
  // velocity up and does not change the rest. A bounce, a stop on the land
  // and a deflection do not look like that.
  {
    const float before[3] = {30.0f, 12.0f, -40.0f};
    float p[3] = {0.0f, 0.0f, 0.0f};
    float v[3] = {before[0], before[1], before[2]};
    RuleFreeStep(p, v, 109.0f, 0.01f);
    Check(RuleStepWasFree(before, v, 109.0f, 0.01f),
          "a step in the air is free", 1, 1);
    const float kept[3] = {before[0], before[1], before[2]};
    Check(!RuleStepWasFree(before, kept, 109.0f, 0.01f),
          "a step with no gravity is a contact", 0, 0);
    const float bounce[3] = {27.0f, -1.2f, -36.0f};
    Check(!RuleStepWasFree(before, bounce, 109.0f, 0.01f),
          "a bounce is a contact", 0, 0);
    const float turned[3] = {-30.0f, 12.0f - 1.09f, -40.0f};
    Check(!RuleStepWasFree(before, turned, 109.0f, 0.01f),
          "a deflection across the land is a contact", 0, 0);
  }

  // THE FAN ON THE ARC. Ball i of the game is 7 degrees to the right of
  // ball 0, 2 degrees higher and 20 percent slower. On the arc it keeps the
  // same angles and the same ratio about the arc of ball 0.
  {
    const float up0 = 0.3f;
    const float game0[3] = {0.0f, 100.0f * sinf(up0), 100.0f * cosf(up0)};
    const float side = 7.0f / 57.29578f;
    const float up_i = up0 + 2.0f / 57.29578f;
    const float game_i[3] = {80.0f * cosf(up_i) * sinf(side),
                             80.0f * sinf(up_i),
                             80.0f * cosf(up_i) * cosf(side)};
    const float yaw = -30.0f / 57.29578f;
    const float lift = 10.0f / 57.29578f;
    const float arc0[3] = {150.0f * cosf(lift) * sinf(yaw),
                           150.0f * sinf(lift),
                           150.0f * cosf(lift) * cosf(yaw)};
    float out[3];
    BallFanVelocity(game0, game_i, arc0, out);
    Near("the fan on the arc: ball i is 7 degrees to the right of ball 0",
         SideDegrees(arc0, out), 7.0, 0.01);
    Near("the fan on the arc: ball i is 2 degrees higher than ball 0",
         UpDegrees(out) - UpDegrees(arc0), 2.0, 0.01);
    Near("the fan on the arc: ball i has 80 percent of the speed of ball 0",
         controller_math::Length(out), 120.0, 0.01);
  }
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

  // THE BALLS OF A FIREBALL. The speed map of the game, and the turn of each
  // ball of the three of Fireball Increase. The desktop runs measured the
  // last ball at the start of its flight: 7.24 degrees to the right for a
  // cast of 162.3 units a second, 9.00 for 59.5 and 6.84 for 190.0.
  Near("a cast of 30 units a second gives a ball of 30", FireballSpeed(30.0f),
       30.0, 1e-4);
  Near("a cast of 162.26 gives a ball of 92.10", FireballSpeed(162.26f),
       92.10, 0.01);
  Near("a cast of 59.53 gives a ball of 53.57", FireballSpeed(59.53f), 53.57,
       0.01);
  Near("a cast of 1000 gives the top speed of 200", FireballSpeed(1000.0f),
       200.0, 1e-4);
  Near("the fan at no speed is 0.2 rad", FireballFanRadians(0.0f), 0.2, 1e-6);
  Near("the fan at the top speed is 0.04 rad", FireballFanRadians(250.0f),
       0.04, 1e-6);
  Near("ball 0 of 3 flies along the cast",
       FireballBallSideDegrees(0, 3, 162.26f), 0.0, 1e-6);
  Near("ball 1 of 3 at 162.3 units a second: 7.24 degrees to the left",
       FireballBallSideDegrees(1, 3, 162.26f), -7.24, 0.02);
  Near("ball 2 of 3 at 162.3 units a second: 7.24 degrees to the right",
       FireballBallSideDegrees(2, 3, 162.26f), 7.24, 0.02);
  Near("ball 2 of 3 at 59.5 units a second: 9.00 degrees to the right",
       FireballBallSideDegrees(2, 3, 59.53f), 9.00, 0.02);
  Near("ball 2 of 3 at 190.0 units a second: 6.77 degrees to the right",
       FireballBallSideDegrees(2, 3, 189.97f), 6.77, 0.02);
  Near("the plain Fireball has one ball on the cast",
       FireballBallSideDegrees(1, 1, 162.26f), 0.0, 1e-6);
  {
    double sum = 0.0;
    for (int i = 0; i < 8; ++i) sum += FireballBallSideDegrees(i, 8, 100.0f);
    Near("the eight balls of Extreme turn left and right by the same sum", sum,
         0.0, 1e-3);
  }

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

  BallFlightChecks();
}

}  // namespace miracle_throw_checks
