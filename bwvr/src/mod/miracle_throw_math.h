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

}  // namespace miracle_throw_math
