// THE HAND STOPS AT THE LAND (docs/hand-model.md, "The Hand stops at the
// land"): the points of the hand, the lift over the land, the slide along the
// land, the return to the real hand, the Hand's point and the pulse. main.cpp
// includes this file after Check and Near, just before main.
//
// The test has no game files. The hand is the low poly hand of
// overlay_math.h, 19 cm long at 100 units a meter, and the land is a plane or
// a slope.
#pragma once

#include "../../src/mod/hand_land_math.h"

namespace hand_land_checks {

namespace hl = hand_land;
namespace om = overlay_math;

const float kUnits = 100.0f;           // units a meter
const float kLength = 0.19f * kUnits;  // the hand, in units
const float kSkin = 0.005f * kUnits;   // the gap of a corner of the skin

// The points of the right hand at a grip place, as hand_land.cpp makes them:
// the palm corners, each finger at the curl, and the Hand's point. With
// fingers that bend on the land, the palm corners and the knuckles only.
inline int Samples(const float position[3], const float rotation[9],
                   float curl, hl::Sample* out, bool bend = false,
                   int* kinds = nullptr) {
  float joints[om::kFingers][om::kJoints + 1][3];
  for (int f = 0; f < om::kFingers; ++f) om::FingerJoints(f, curl, joints[f]);
  float tip[3];
  om::FingerTip(1, om::kRestCurl, tip);
  float local[hl::kMostSamples][3], gap[hl::kMostSamples];
  int kind[hl::kMostSamples];
  const int n =
      hl::HandPoints(joints, bend, kSkin / kLength, tip, local, gap, kind);
  for (int i = 0; i < n; ++i) {
    om::PlacePoint(position, rotation, kLength, false, local[i], out[i].p);
    out[i].gap = gap[i] * kLength;
    if (kinds != nullptr) kinds[i] = kind[i];
  }
  return n;
}

// The least height over the land of the points, less their gaps: 0 when the
// hand rests on the land.
template <typename Land>
inline float LeastClear(const hl::Sample* s, int n, float lift, Land land) {
  float least = 1e30f;
  for (int i = 0; i < n; ++i) {
    const float y = s[i].p[1] + lift - land(s[i].p[0], s[i].p[2]) - s[i].gap;
    if (y < least) least = y;
  }
  return least;
}

inline float Flat(float, float) { return 0.0f; }
inline float Slope(float x, float) { return 0.25f * x; }
inline float Nothing(float, float) { return NAN; }

inline void HandLandChecks() {
  printf("-- the Hand stops at the land\n");

  // The palm faces down: the hand frame x (the back of the hand) is up, the
  // fingers (-y of the frame) go along +z of the world.
  const float palm_down[9] = {0.0f, 0.0f, 1.0f,   // world x
                              1.0f, 0.0f, 0.0f,   // world y
                              0.0f, -1.0f, 0.0f};  // world z
  // The fingers point down: the frame -y is the world -y.
  const float tip_down[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                             0.0f, 0.0f, 1.0f};

  Check(hl::kMostSamples == 8 + 5 * 7 + 1,
        "a hand has 44 points: 8 corners of the palm, 7 for each finger and "
        "the Hand's point", hl::kMostSamples, 44);
  {
    const float joints[4][3] = {{0, 0, 0}, {0, -1, 0}, {0, -2, 0}, {0, -3, 0}};
    float points[hl::kFingerSamples][3], gaps[hl::kFingerSamples];
    const int n = hl::FingerSamples(joints, 0.05f, 0.02f, points, gaps);
    Check(n == 7 && gaps[0] == 0.05f && gaps[3] == 0.02f && gaps[4] == 0.05f &&
              points[4][1] == -0.5f && points[6][1] == -2.5f,
          "a finger gives its 4 joints with its radius (the tip with the gap "
          "of the skin) and the middle of each segment", n, 7);
  }

  hl::Sample s[hl::kMostSamples];
  // OVER THE LAND: one to one.
  {
    const float at[3] = {0.0f, 20.0f, 0.0f};
    const int n = Samples(at, palm_down, om::kRestCurl, s);
    const float lift = hl::Lift(s, n, Flat);
    Near("a hand 20 units over the land has no lift", lift, 0.0, 1e-6);
    float drawn[3];
    hl::Drawn(at, lift, drawn);
    Check(drawn[0] == at[0] && drawn[1] == at[1] && drawn[2] == at[2],
          "and the drawn hand is the real hand", drawn[1], at[1]);
  }
  // INTO THE LAND, straight down: the drawn hand stays on the land at the
  // point where it went in.
  {
    float rest = 0.0f;
    {
      const float at[3] = {0.0f, 20.0f, 0.0f};
      const int n = Samples(at, palm_down, om::kRestCurl, s);
      rest = at[1] - LeastClear(s, n, 0.0f, Flat);
    }
    // rest: the height of the grip at which the flat hand just touches.
    const float depths[3] = {0.5f, 10.0f, 60.0f};
    for (int d = 0; d < 3; ++d) {
      const float at[3] = {10.0f, rest - depths[d], 5.0f};
      const int n = Samples(at, palm_down, om::kRestCurl, s);
      const float lift = hl::Lift(s, n, Flat);
      Near("a palm down hand pushed into flat land is lifted by the depth",
           lift, depths[d], 1e-3);
      Near("and its lowest point rests on the land with its gap",
           LeastClear(s, n, lift, Flat), 0.0, 1e-3);
      float drawn[3];
      hl::Drawn(at, lift, drawn);
      Check(drawn[0] == at[0] && drawn[2] == at[2],
            "and the drawn hand does not move sideways", drawn[0], at[0]);
    }
  }
  // THE TIP FIRST: a hand that points down stops on its tip, and the Hand's
  // point keeps 5 mm over the land.
  {
    const float at[3] = {0.0f, 5.0f, 0.0f};
    const int n = Samples(at, tip_down, om::kRestCurl, s);
    const float lift = hl::Lift(s, n, Flat);
    Check(lift > 0.0f, "a hand that points down with its grip 5 units over "
                       "the land goes up", lift, 0);
    Near("and nothing of it is in the land", LeastClear(s, n, lift, Flat), 0.0,
         1e-3);
    Check(s[n - 1].p[1] + lift >= kSkin - 1e-3f,
          "and the Hand's point is 5 mm or more over the land",
          s[n - 1].p[1] + lift, kSkin);
  }
  // THE SLIDE: the real hand goes 30 units sideways, 10 units under a slope.
  // The drawn hand follows it sideways and stays on the slope.
  {
    float last_drawn_x = -1e9f;
    bool on_slope = true, follows = true;
    for (int step = 0; step <= 6; ++step) {
      const float x = 5.0f * static_cast<float>(step);
      const float at[3] = {x, Slope(x, 0.0f) - 10.0f, 0.0f};
      const int n = Samples(at, palm_down, 0.5f, s);
      const float lift = hl::Lift(s, n, Slope);
      on_slope = on_slope && fabsf(LeastClear(s, n, lift, Slope)) < 1e-3f;
      float drawn[3];
      hl::Drawn(at, lift, drawn);
      follows = follows && drawn[0] == x && drawn[0] > last_drawn_x;
      last_drawn_x = drawn[0];
    }
    Check(on_slope, "a hand 10 units under a slope stays on the slope in each "
                    "step of a slide", 0, 0);
    Check(follows, "and it slides with the real hand", last_drawn_x, 30.0);
  }
  // THE RETURN: the real hand comes up, and the lift goes to 0 with no lag.
  {
    float rest = 0.0f;
    {
      const float at[3] = {0.0f, 20.0f, 0.0f};
      const int n = Samples(at, palm_down, om::kRestCurl, s);
      rest = -LeastClear(s, n, 0.0f, Flat) + 20.0f;
    }
    float lifts[4];
    const float heights[4] = {rest - 8.0f, rest - 2.0f, rest + 0.1f,
                              rest + 6.0f};
    for (int i = 0; i < 4; ++i) {
      const float at[3] = {0.0f, heights[i], 0.0f};
      const int n = Samples(at, palm_down, om::kRestCurl, s);
      lifts[i] = hl::Lift(s, n, Flat);
    }
    Near("8 units in the land: a lift of 8", lifts[0], 8.0, 1e-3);
    Near("2 units in the land: a lift of 2", lifts[1], 2.0, 1e-3);
    Near("just over the land: the real hand again", lifts[2], 0.0, 1e-6);
    Near("6 units over the land: the real hand", lifts[3], 0.0, 1e-6);
    // The same pose gives the same lift: nothing moves by itself.
    const float at[3] = {0.0f, heights[0], 0.0f};
    const int n = Samples(at, palm_down, om::kRestCurl, s);
    Check(hl::Lift(s, n, Flat) == lifts[0],
          "a still hand on still land has the same lift in each frame",
          lifts[0], lifts[0]);
  }
  // FINGERS THAT BEND ON THE LAND (hand feel): the palm and the knuckles
  // stop the hand, and the fingers are left to bend.
  {
    int kinds[hl::kMostSamples];
    const float at[3] = {0.0f, 20.0f, 0.0f};
    const int n = Samples(at, tip_down, om::kRestCurl, s, true, kinds);
    bool rigid = true;
    for (int i = 0; i < n; ++i) {
      rigid = rigid && (kinds[i] == hl::kKindPalm || kinds[i] == hl::kKindJoint);
    }
    Check(n == 8 + 5 && rigid,
          "with fingers that bend, the stop has the 8 corners of the palm and "
          "the 5 knuckles only", n, 13);
    // The fingers point down, and the grip is 8 units over the land: the
    // tips are in the land, and the knuckles are not.
    const float low[3] = {0.0f, 8.0f, 0.0f};
    const int m = Samples(low, tip_down, om::kRestCurl, s, true);
    Near("so a hand with its finger tips in the land and its knuckles over "
         "it has no lift: the fingers bend", hl::Lift(s, m, Flat), 0.0, 1e-6);
    const int all = Samples(low, tip_down, om::kRestCurl, s, false);
    Check(hl::Lift(s, all, Flat) > 0.0f,
          "with fingers that do not bend the same hand goes up",
          hl::Lift(s, all, Flat), 0);
    // The knuckles go into the land: the hand stops on them.
    const float lower[3] = {0.0f, -5.0f, 0.0f};
    const int k = Samples(lower, tip_down, om::kRestCurl, s, true);
    const float lift = hl::Lift(s, k, Flat);
    Check(lift > 0.0f, "a hand with its knuckles in the land goes up", lift,
          0);
    Near("until the palm and the knuckles rest on the land",
         LeastClear(s, k, lift, Flat), 0.0, 1e-3);
  }
  // A land with no number does not lift.
  {
    const float at[3] = {0.0f, -50.0f, 0.0f};
    const int n = Samples(at, palm_down, om::kRestCurl, s);
    int lowest = 7;
    Near("a land height that is no number gives no lift",
         hl::Lift(s, n, Nothing, &lowest), 0.0, 0.0);
    Check(lowest == -1, "and no lowest point", lowest, -1);
  }
  float drawn[3];
  const float real[3] = {1.0f, 2.0f, 3.0f};
  hl::Drawn(real, -4.0f, drawn);
  Check(drawn[1] == 2.0f, "a lift less than 0 does not move the hand down",
        drawn[1], 2.0);

  // THE HAND'S POINT goes with the drawn hand only at the tip.
  {
    const float tip[3] = {100.0f, 50.0f, 100.0f};
    const float near_point[3] = {100.5f, 50.0f, 100.5f};
    const float far_point[3] = {100.0f, 50.0f, 110.0f};
    Check(hl::PointOnHand(near_point, tip, kUnits, 0.02f),
          "a point 0.7 cm of the room from the tip is on the hand", 0.7, 2.0);
    Check(!hl::PointOnHand(far_point, tip, kUnits, 0.02f),
          "a reach point 10 cm from the tip is not", 10.0, 2.0);
    Check(!hl::PointOnHand(near_point, tip, 0.0f, 0.02f),
          "and with no scale no point is", 0, 0);
  }

  // THE PULSE: once at the first touch, none while the hand rests or slides,
  // none after a short gap, and one after a gap of 0.2 s or more.
  {
    float quiet = 10.0f;
    int pulses = 0;
    for (int i = 0; i < 60; ++i) pulses += hl::TouchPulse(true, 0.011f, &quiet);
    Check(pulses == 1, "a hand that rests on the land for 60 frames gets one "
                       "pulse", pulses, 1);
    for (int i = 0; i < 9; ++i) hl::TouchPulse(false, 0.011f, &quiet);
    pulses = hl::TouchPulse(true, 0.011f, &quiet);
    Check(pulses == 0, "a touch after 0.1 s off the land gets none", pulses,
          0);
    for (int i = 0; i < 20; ++i) hl::TouchPulse(false, 0.011f, &quiet);
    pulses = hl::TouchPulse(true, 0.011f, &quiet);
    Check(pulses == 1, "a touch after 0.22 s off the land gets one", pulses,
          1);
  }
}

}  // namespace hand_land_checks
