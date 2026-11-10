// THE GAIN OF THE GRAB (docs/god-view.md, section 5.1, "The gain of the
// grab"): the distance part up, the scale part across, no motion with the
// hand at rest, and no overshoot (grab_gain_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/grab_gain_math.h"
#include "../../src/mod/walk_math.h"

namespace grab_gain_checks {

// Pulls of the hand up, each of `pull` meters in steps of 1 cm, from a land
// `below` meters below the hand. Between two pulls the hand goes back down
// with the grip open, which moves nothing. Returns the land below the hand
// after the pulls, and the least distance in any step.
inline float Pulls(float below, int pulls, float pull, float reach,
                   float most, float* least) {
  float d = below;
  *least = d;
  for (int p = 0; p < pulls; ++p) {
    const int steps = static_cast<int>(pull / 0.01f + 0.5f);
    for (int i = 0; i < steps; ++i) {
      const float up = grab_gain_math::LandUp(d, 0.01f, reach, most);
      d += 0.01f - up;  // the hand goes up 1 cm, the land up by `up`
      if (d < *least) *least = d;
    }
  }
  return d;
}

inline void GrabGainChecks() {
  printf("\nThe gain of the grab\n");
  using namespace grab_gain_math;
  const float reach = 0.50f, most = 50.0f;

  // THE DISTANCE PART.
  Near("land at the hand is held 1:1", DistanceGain(0.05f, reach, most), 1.0,
       1e-6);
  Near("land within the reach is held 1:1", DistanceGain(0.50f, reach, most),
       1.0, 1e-6);
  Near("land 20 m below the hand has a gain of 40",
       DistanceGain(20.0f, reach, most), 40.0, 1e-4);
  Near("the cap holds land 40 m below at 50", DistanceGain(40.0f, reach, most),
       50.0, 1e-4);
  Near("a hand under the land has a gain of 1",
       DistanceGain(-0.3f, reach, most), 1.0, 1e-6);
  Near("a land near the hand comes up 1:1", LandUp(0.3f, 0.10f, reach, most),
       0.10, 1e-6);
  Near("a hand at rest moves nothing, also over land 20 m below",
       LandUp(20.0f, 0.0f, reach, most), 0.0, 1e-6);
  Near("a hand that goes down takes the land down 1:1",
       LandUp(20.0f, -0.10f, reach, most), -0.10, 1e-6);
  Near("with the cap at 1 the land comes up 1:1",
       LandUp(20.0f, 0.10f, reach, 1.0f), 0.10, 1e-6);
  // One step and many steps give the same land.
  {
    float many = 0.0f, d = 20.0f;
    for (int i = 0; i < 60; ++i) {
      const float up = LandUp(d, 0.01f, reach, most);
      many += up;
      d += 0.01f - up;
    }
    Near("one pull of 60 cm in one step or in 60 steps, m",
         LandUp(20.0f, 0.60f, reach, most), many, 1e-3);
  }
  // Land 20 m below the hand: three pulls of 60 cm bring it to about 1 m
  // below the hand, and it never comes nearer than the reach.
  {
    float least = 0.0f;
    const float after = Pulls(20.0f, 3, 0.60f, reach, most, &least);
    Check(after < 1.10f, "three pulls of 60 cm bring land 20 m down to less "
          "than 1.1 m below the hand", after, 1.10f);
    Check(least >= reach - 1e-4f, "and the land never comes nearer than the "
          "reach by the gain (no overshoot)", least, reach);
    Near("the land after three pulls, m (0.5 + 19.5 e^-3.6)", after,
         0.5 + 19.5 * exp(-3.6), 0.02);
    const float again = Pulls(after, 1, 0.60f, reach, most, &least);
    Check(again < after && again >= reach - 1e-4f,
          "a fourth pull brings it nearer, and still not past the reach",
          again, reach);
  }
  // Far land under the cap: 40 m comes up 50 times the hand until 25 m.
  Near("land 40 m below comes up 50 times a step of 10 cm",
       LandUp(40.0f, 0.10f, reach, most), 5.0, 1e-4);

  // THE SCALE PART: the factor of the stick, at least 1.
  const auto across = [](float scale) {
    return ScaleGain(walk_math::ScaleFactor(scale, 16.0f, 0.5f, 0.6f, 4.0f));
  };
  Near("across at 1 unit per meter, 4 times the hand", across(1.0f), 4.0,
       1e-5);
  Near("across at 7.5, 1.46 times", across(7.5f), 1.46059, 1e-4);
  Near("across at 16, 1:1", across(16.0f), 1.0, 1e-6);
  Near("across at 100, 1:1 (no gain under 1)", across(100.0f), 1.0, 1e-6);
  {
    const float step[3] = {0.10f, 0.05f, -0.20f};
    float out[3];
    GrabStep(step, 0.2f, 4.0f, reach, most, out);
    Near("a step across at a gain of 4, right", out[0], 0.40, 1e-6);
    Near("forward", out[2], -0.80, 1e-6);
    Near("and up 1:1 with the land at the hand", out[1], 0.05, 1e-6);
    const float still[3] = {0.0f, 0.0f, 0.0f};
    GrabStep(still, 20.0f, 4.0f, reach, most, out);
    Near("a hand at rest gives no step at each gain",
         fabsf(out[0]) + fabsf(out[1]) + fabsf(out[2]), 0.0, 1e-7);
  }
}

}  // namespace grab_gain_checks
