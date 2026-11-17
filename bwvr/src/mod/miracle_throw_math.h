// The throw of a miracle: pure math, no game state. See docs/throw.md, "A
// thrown miracle". miracle_throw.cpp uses it, and the math test checks it.
//
// THE GAME'S SPEED OF THE HAND [V, runblack.exe 1.42]. With a miracle in the
// Hand the Grain state (HandStateGrain_vfunc3 0x005B3310) keeps a filter of
// the speed of the Hand. Each update moves the filtered velocity toward the
// velocity of CHand+0x78 in that update, by the part 1 - exp(-r dt), with
// r = -10 ln(1 - 0.8) = 16.09 a second (the float at 0x00BF014C): a time
// constant of 62 ms. The cast of a thrown miracle sends that filtered
// velocity (CHand+0x48C8), with no cap, in packet 0x4D (FUN_005D3340,
// 0x005D351C), and a Fireball leaves the Hand with it.
//
// So the game's velocity is the mean of the last 60 ms or so of the motion of
// the Hand's point, read when the grip opened. A swing that curves gives a
// direction of the past, and a hand that slows at the opening gives less
// speed. The throw of an object takes the top speed of the grip in the swing
// (throw_math::PeakVelocity). A thrown miracle takes the same velocity.

#pragma once

#include <math.h>

#include "controller_math.h"
#include "throw_math.h"

namespace miracle_throw_math {

// The rate of the game's filter, a second: -10 ln(1 - 0.8).
const float kGrainRate = 16.0944f;

// One update of the game's filter of the speed of the Hand: the filtered
// velocity moves toward the velocity of the update by 1 - exp(-r dt).
inline void GrainFilterStep(float filtered[3], const float velocity[3],
                            float dt) {
  if (!(dt > 0.0f)) return;
  const float part = 1.0f - expf(-kGrainRate * dt);
  for (int i = 0; i < 3; ++i) filtered[i] += (velocity[i] - filtered[i]) * part;
}

// The velocity of the swing in the world, in world units per second: the top
// speed of the grip in the room's axes (units at the life size scale, life
// units in a meter), turned by the room basis and scaled to s units in a
// meter. This is s v, the speed of the real hand in the world.
inline void SwingWorld(const float peak_room[3], const float room[9],
                       float life, float s, float world[3]) {
  for (int i = 0; i < 3; ++i) world[i] = 0.0f;
  if (!(life > 0.0f) || !(s > 0.0f)) return;
  float along[3];
  controller_math::Apply(room, false, peak_room, along);
  for (int i = 0; i < 3; ++i) world[i] = along[i] * (s / life);
}

// The speed across the land.
inline float FlatLength(const float v[3]) {
  return sqrtf(v[0] * v[0] + v[2] * v[2]);
}

// The angle about world up from the direction `from` to the direction `to`,
// in degrees, from -180 to 180. Positive when `to` is to the right of `from`.
// The axes of the game have x to the right, y up and z forward, so a turn from
// +z toward +x is a turn to the right. 0 when one of the two has no part
// across the land.
inline float SideDegrees(const float from[3], const float to[3]) {
  if (!(FlatLength(from) > 1e-4f) || !(FlatLength(to) > 1e-4f)) return 0.0f;
  const float cross = from[2] * to[0] - from[0] * to[2];
  const float dot = from[0] * to[0] + from[2] * to[2];
  return atan2f(cross, dot) * 57.29578f;
}

// The angle of a direction over the land, in degrees: positive up.
inline float UpDegrees(const float v[3]) {
  const float flat = FlatLength(v);
  if (!(flat > 1e-4f) && !(fabsf(v[1]) > 1e-4f)) return 0.0f;
  return atan2f(v[1], flat) * 57.29578f;
}

// THE BALLS OF A FIREBALL [V, runblack.exe 1.42]. The particle system of a
// Fireball makes its balls with CreateWithInitialDirection::ModifyAtomCollection
// (0x0069E950). For the player's cast it takes the direction of the velocity
// that the cast got, 0.3 rad (Elevation) higher (fn_0069E6B0), and a speed
// from the speed of the cast (fn_0069E620, below). Ball 0 has that direction
// and speed. The power levels have more balls (NumAtoms of
// SF_FireBallThrowPU: 3 for Increase, PU2: 8 for Extreme). Ball i of n, i from
// 1, turns by cos(phi) times the fan about up and by sin(phi) times the fan
// up, with phi = 2 pi (i - 1) / (n - 1), and its speed is up to 30 percent less
// (SpeedRandomFrac). The fan goes from 0.2 rad at no speed to 0.04 rad at the
// top speed (HorozScatterAtMinSpeed, AtMaxSpeed, and the same up). So the
// three balls of Increase fly ahead, to the left and to the right by the same
// angle. The plain Fireball has one ball.

// fn_0069E620: the speed of a ball from the speed of the cast, in units a
// second. Up to 50 it is the same, then a line to 200 at 450, and 200 above.
inline float FireballSpeed(float cast_speed) {
  if (!(cast_speed > 0.0f)) return 0.0f;
  if (cast_speed < 50.0f) return cast_speed;
  if (cast_speed >= 450.0f) return 200.0f;
  return 50.0f + (cast_speed - 50.0f) * (150.0f / 400.0f);
}

// The fan of the balls of a power level, in radians, for the speed of a ball:
// 0.2 at no speed, 0.04 at the top speed of 200.
inline float FireballFanRadians(float ball_speed) {
  float part = ball_speed / 200.0f;
  if (!(part > 0.0f)) part = 0.0f;
  if (part > 1.0f) part = 1.0f;
  return 0.2f + (0.04f - 0.2f) * part;
}

// The turn about up of ball i of n, in degrees, positive to the right (as
// SideDegrees). The game adds the fan to the angle atan2(z, x), which is a
// turn to the left, so a positive cos(phi) is a turn to the left.
inline float FireballBallSideDegrees(int ball, int balls, float cast_speed) {
  if (ball < 1 || balls < 2) return 0.0f;
  const float phi = 6.2831853f * static_cast<float>(ball - 1) /
                    static_cast<float>(balls - 1);
  return -cosf(phi) * FireballFanRadians(FireballSpeed(cast_speed)) *
         57.29578f;
}

// THE FLIGHT OF A BALL [V, runblack.exe 1.42]. The rule of group 0 of a
// thrown Fireball, UpdateRuleGravityWithFloor::ModifyAtomCollection
// (0x006A1880), moves each ball once in each game turn, with the time dt of
// the float at 0x00D4E0EC. That is the turn of 100 ms (0x00D01A38) that
// GJPSysInterface::Process_ gives the particle system. One step of the rule:
//
//   1. The drag: v += (w - v) Damping dt, with w the wind times
//      WindMagnification times 0.1. With no wind: v *= 1 - Damping dt.
//   2. The move: p += v dt.
//   3. The land: when the bottom of the ball is under the land at p, the rule
//      puts the ball on the land and bounces it (the impact of the Fireball).
//      Else v.y -= Gravity GravityScale dt, and the rule adds less gravity
//      while v.y is near -MaxSpeed.
//   4. A Physical Shield that holds the ball deflects it.
//
// The Fireball has Gravity 30, Damping 0.1 and MaxSpeed 100 (the particle
// files SF_FireBallThrow, PU and PU2).
//
// With no drag and no cap the step is the step below. Its places after n
// steps are on the curve p0 + (v0 - g dt / 2) t - g t^2 / 2 + g t dt / 2.
// So a start velocity of v0 - g dt / 2 along up puts each step on the arc
// p0 + v0 t - g t^2 / 2, with no error.

// The time of one step of the rule, in seconds.
const float kBallTurn = 0.1f;
// A step of the flight moves the ball this far in the room at most, in
// meters. A game turn then has more steps of the rule, so that the rule finds
// the land near the place where the arc meets it.
const float kBallStepRoom = 0.02f;
// The most steps of the rule in one game turn.
const int kBallMostSteps = 16;

// One step of the rule in the air with no drag and no cap, at the gravity g
// (Gravity times GravityScale).
inline void RuleFreeStep(float p[3], float v[3], float g, float dt) {
  for (int i = 0; i < 3; ++i) p[i] += v[i] * dt;
  v[1] -= g * dt;
}

// The velocity to give the rule at the arc time t, so that its steps of dt
// are on the arc: the velocity of the arc, less g dt / 2 along up.
inline void RuleStartVelocity(const float v_arc[3], float g, float dt,
                              float out[3]) {
  for (int i = 0; i < 3; ++i) out[i] = v_arc[i];
  out[1] -= 0.5f * g * dt;
}

// True when one step of the rule had no contact: the velocity across the
// land did not change, and gravity took g dt off the velocity up. A contact
// with the land or a deflection by a shield changes the velocity in another
// way, or keeps the gravity off.
inline bool RuleStepWasFree(const float before[3], const float after[3],
                            float g, float dt) {
  const float tolerance = 1e-3f;
  for (int i = 0; i < 3; i += 2) {
    if (!(fabsf(after[i] - before[i]) <= tolerance * (1.0f + fabsf(before[i]))))
      return false;
  }
  const float want = before[1] - g * dt;
  return fabsf(after[1] - want) <= tolerance * (1.0f + fabsf(want));
}

// THE ARC OF A THROWN FIREBALL (miracle_flight=1, docs/throw.md, "The flight
// of a thrown Fireball"). The cast got s v world units a second, the
// velocity of the real hand in the world (miracle_throw 1 or 2). Ball 0 then
// flies the arc of a thrown object of the same swing: s v / k under
// s g / k^2, with the time factor k of throw_slow_x10, cut as for an object
// (throw_math::SlowFactor).
struct BallFlight {
  float slow;     // the k that is in use
  float gravity;  // world units a second squared: s g / k^2
  float v0[3];    // ball 0 at the cast, world units a second: s v / k
  int steps;      // steps of the rule in one game turn
};

inline void BallFlightPlan(const float cast[3], float s, float k,
                           BallFlight* out) {
  *out = BallFlight{};
  const float root = s > 1.0f ? sqrtf(s) : 1.0f;
  float rate = 1.0f;
  out->slow = throw_math::SlowFactor(k, root, &rate);
  out->gravity = s * throw_math::kGravity / (out->slow * out->slow);
  for (int i = 0; i < 3; ++i) out->v0[i] = cast[i] / out->slow;
  // The speed of the ball in the room, with the fall of the first half
  // second.
  const float room =
      s > 0.0f
          ? (controller_math::Length(out->v0) + 0.5f * out->gravity) / s
          : 0.0f;
  int steps = static_cast<int>(ceilf(room * kBallTurn / kBallStepRoom));
  if (steps < 1) steps = 1;
  if (steps > kBallMostSteps) steps = kBallMostSteps;
  out->steps = steps;
}

// The place and the velocity of a ball on its arc, t seconds after the
// cast, from the place p0 of the cast. The arc of throw_math::RealArc with
// the gravity of the flight.
inline void BallArcAt(const float p0[3], const float v0[3], float gravity,
                      double t, float p[3], float v[3]) {
  throw_math::RealArc(p0, v0, gravity / throw_math::kGravity, t, p, v);
}

// THE FAN ON THE ARC. The game gives ball i of a power level a turn about
// up, a turn up and less speed than ball 0 (see "The balls of a Fireball").
// The arc of ball i keeps them: the velocity of the arc of ball 0, turned by
// the angles between ball i and ball 0 of the game, and times the ratio of
// their speeds.
inline void BallFanVelocity(const float game0[3], const float game_i[3],
                            const float arc0[3], float out[3]) {
  const float speed0 = controller_math::Length(game0);
  const float speed_i = controller_math::Length(game_i);
  const float ratio = speed0 > 1e-4f ? speed_i / speed0 : 1.0f;
  const float side = SideDegrees(game0, game_i) / 57.29578f;
  const float up = (UpDegrees(game_i) - UpDegrees(game0) + UpDegrees(arc0)) /
                   57.29578f;
  const float* heading = FlatLength(arc0) > 1e-4f ? arc0 : game0;
  const float yaw = atan2f(heading[0], heading[2]) + side;
  const float speed = controller_math::Length(arc0) * ratio;
  out[0] = speed * cosf(up) * sinf(yaw);
  out[1] = speed * sinf(up);
  out[2] = speed * cosf(up) * cosf(yaw);
}

// WHERE THE ARC MEETS THE LAND. land(x, z) gives the height of the land. The
// search goes along the arc in steps of `step` seconds up to `longest`, then
// halves the last step 12 times. False when the arc stays over the land.
template <class Land>
bool BallArcMeetsLand(const float p0[3], const float v0[3], float gravity,
                      double start, double step, double longest, Land land,
                      double* t_out, float at[3]) {
  float p[3], v[3];
  double before = start;
  for (double t = start; t <= longest + 1e-9; t += step) {
    BallArcAt(p0, v0, gravity, t, p, v);
    if (p[1] > land(p[0], p[2])) {
      before = t;
      continue;
    }
    if (t == start) {
      *t_out = t;
      for (int i = 0; i < 3; ++i) at[i] = p[i];
      return true;
    }
    double low = before, high = t;
    for (int k = 0; k < 12; ++k) {
      const double mid = 0.5 * (low + high);
      BallArcAt(p0, v0, gravity, mid, p, v);
      if (p[1] > land(p[0], p[2])) {
        low = mid;
      } else {
        high = mid;
      }
    }
    BallArcAt(p0, v0, gravity, high, p, v);
    *t_out = high;
    for (int i = 0; i < 3; ++i) at[i] = p[i];
    return true;
  }
  return false;
}

}  // namespace miracle_throw_math
