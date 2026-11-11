// The gain of the grab of the world with one hand in the God view
// (docs/god-view.md, section 5.1, "The gain of the grab"). Pure functions:
// no Windows and no game. world_grab.cpp uses them, and the checks are in
// test/controller_math/grab_gain_checks.h.
//
// TWO PARTS, each on its own axis:
//
//   up      the distance part. When the land is far below the hand, a pull
//           up of the hand brings the land up by more than the hand moved:
//           gain = d / reach, at least 1 and at most `most`, with d the
//           land below the hand in meters of the room. So land within
//           `reach` of the hand is held 1:1, and land 20 m below comes up
//           in three pulls of 60 cm.
//   across  the scale part: the factor of the speed of the stick
//           (walk_math::ScaleFactor), at least 1. So at the villager scale a
//           move of the hand across moves the land up to 4 times as far,
//           and from the scale of factor 1 up the grab is 1:1 across.
//
// The distance part acts on the height only. With the land 20 m below the
// hand its gain is 40, and a small move of the hand to the side would throw
// the view meters to the side in each pull. The scale part acts across only.
// In height the hand holds the ground at the waist: a gain there would move
// the land away from the hand at each small move up or down.
//
// A move down of the hand (the land goes away) is 1:1, so the land never
// drops away fast. A hand at rest moves nothing at each gain.
#pragma once

#include <math.h>

namespace grab_gain_math {

// The gain of the distance part for a land below_m below the hand.
inline float DistanceGain(float below_m, float reach_m, float most) {
  if (!(reach_m > 0.0f)) return 1.0f;
  float gain = below_m / reach_m;
  if (!(gain > 1.0f)) gain = 1.0f;
  if (most > 1.0f && gain > most) gain = most;
  if (!(most > 1.0f)) gain = 1.0f;
  return gain;
}

// How far the land goes up in the room for a move up of the hand of step_m,
// with the land below_m below the hand at the start of the move. The gain
// changes with the distance during the move, so this is the exact answer of
//
//     d(land up) = gain(d) * d(hand up),   d = the land below the hand
//
// in three parts: at the cap the land comes up `most` times the hand, then
// the distance over reach falls as exp(-hand / reach), and within reach the
// land comes up 1:1. So the land never comes nearer than reach below the
// hand by the gain, one long step and many short steps give the same
// result, and a step of 0 gives 0. A step down (less than 0) is 1:1.
inline float LandUp(float below_m, float step_m, float reach_m, float most) {
  if (!(step_m > 0.0f)) return step_m == step_m ? step_m : 0.0f;
  if (!(reach_m > 0.0f) || !(most > 1.0f) || !(below_m > reach_m)) {
    return step_m;
  }
  float d = below_m;
  float left = step_m;
  float up = 0.0f;
  const float cap_at = most * reach_m;
  if (d > cap_at) {
    // The hand travel that brings the land to the cap distance.
    float take = (d - cap_at) / (most - 1.0f);
    if (take > left) take = left;
    up += most * take;
    d -= (most - 1.0f) * take;
    left -= take;
  }
  if (left > 0.0f && d > reach_m) {
    const float end = reach_m + (d - reach_m) * expf(-left / reach_m);
    up += left + (d - end);
    left = 0.0f;
  }
  return up + left;
}

// The gain of the scale part from the factor of the stick.
inline float ScaleGain(float stick_factor) {
  return stick_factor > 1.0f ? stick_factor : 1.0f;
}

// The step of the hand that the grab uses, from the step of the hand in the
// room (right, up, forward, in meters): across times the scale gain, up by
// LandUp. out may be step.
inline void GrabStep(const float step[3], float below_m, float scale_gain,
                     float reach_m, float most, float out[3]) {
  const float up = LandUp(below_m, step[1], reach_m, most);
  out[0] = step[0] * scale_gain;
  out[2] = step[2] * scale_gain;
  out[1] = up;
}

}  // namespace grab_gain_math
