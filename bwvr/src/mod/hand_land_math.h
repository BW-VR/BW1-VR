// The Hand stops at the land: the pure math. See hand_land.cpp and
// docs/hand-model.md, "The Hand stops at the land".
//
// THE RULE. The drawn hand is the real hand, one to one, over the land. When
// the real hand goes into the land, the drawn hand goes straight up until no
// point of it is in the land. So it stays on the land, and it slides along
// the land with the real hand. When the real hand comes up, the lift is 0
// again, and the drawn hand is the real hand.
//
// THE POINTS. The hand is a set of points in the hand frame of overlay_math.h
// (lengths in hand lengths). Each point has the least gap that it keeps over
// the land: the radius of the finger for a joint of a finger, and the
// clearance of the Hand's point for a corner of the skin (a finger tip, a
// corner of the palm). The lift is the largest amount by which a point is
// under its height. Nothing moves the hand sideways, and the lift has no
// filter, so the drawn hand moves only when the real hand moves. While the
// fingers bend on the land (hand feel), only the palm and the knuckles stop
// the hand (HandPoints).

#pragma once

#include <math.h>

#include "overlay_math.h"

namespace hand_land {

// One point of the hand in the world, and the least gap over the land at
// it, in world units.
struct Sample {
  float p[3];
  float gap;
};

// The most points of one hand: the 8 corners of the palm, for each finger
// its 4 joints and the middle of each of its 3 segments, and the Hand's
// point.
const int kPalmCorners = 8;
const int kFingerSamples = overlay_math::kJoints + 1 + overlay_math::kJoints;
const int kMostSamples =
    kPalmCorners + overlay_math::kFingers * kFingerSamples + 1;

// The corners of the palm in the hand frame: the box of the low poly palm,
// to 90 percent of the way to the wrist (the wrist of the mesh fades out).
inline void PalmCorner(int i, float out[3]) {
  const float x0 = overlay_math::kPalmX - overlay_math::kPalmHalfThick;
  const float x1 = overlay_math::kPalmX + overlay_math::kPalmHalfThick;
  const float y0 = overlay_math::kKnuckleY - overlay_math::kPalmPast;
  const float y1 =
      overlay_math::kKnuckleY + 0.9f * overlay_math::kPalmLength;
  const float z = overlay_math::kPalmHalfWide;
  out[0] = (i & 1) != 0 ? x1 : x0;
  out[1] = (i & 2) != 0 ? y1 : y0;
  out[2] = (i & 4) != 0 ? z : -z;
}

// The points of one finger in the hand frame from its joints (the knuckle,
// the two middle joints, the tip), and the gap of each in hand lengths: the
// radius of the finger at a joint and at the middle of a segment, and
// tip_gap at the tip, which is a corner of the skin. Gives the count.
inline int FingerSamples(const float joints[overlay_math::kJoints + 1][3],
                         float radius, float tip_gap,
                         float points[kFingerSamples][3],
                         float gaps[kFingerSamples]) {
  int n = 0;
  for (int j = 0; j <= overlay_math::kJoints; ++j) {
    for (int k = 0; k < 3; ++k) points[n][k] = joints[j][k];
    gaps[n] = j == overlay_math::kJoints ? tip_gap : radius;
    ++n;
  }
  for (int j = 0; j < overlay_math::kJoints; ++j) {
    for (int k = 0; k < 3; ++k) {
      points[n][k] = 0.5f * (joints[j][k] + joints[j + 1][k]);
    }
    gaps[n] = radius;
    ++n;
  }
  return n;
}

// The kinds of the points, for the log.
const int kKindPalm = 0;
const int kKindJoint = 1;
const int kKindMiddle = 2;
const int kKindTip = 3;
const int kKindPoint = 4;

// WHICH POINTS STOP THE HAND, in the hand frame, with the gap of each in
// hand lengths. joints has the 4 joints of each finger, point is the Hand's
// point (the tip of the index finger at the rest curl), and skin is the gap
// of a corner of the skin.
//
// With fingers that bend on the land (hand feel, hand_feel_land=1), only the
// part of the hand that cannot bend stops it: the corners of the palm and
// the knuckle of each finger. The fingers bend on the land by hand feel, as
// before, and the Hand's point stops 5 mm over the land by itself
// (hand_direct.cpp, ResolveLand). With no such fingers each point of each
// finger and the Hand's point stop the hand too. Gives the count.
inline int HandPoints(
    const float joints[overlay_math::kFingers][overlay_math::kJoints + 1][3],
    bool fingers_bend, float skin, const float point[3],
    float out[kMostSamples][3], float gap[kMostSamples],
    int kind[kMostSamples]) {
  int n = 0;
  for (int i = 0; i < kPalmCorners; ++i) {
    PalmCorner(i, out[n]);
    gap[n] = skin;
    kind[n++] = kKindPalm;
  }
  for (int f = 0; f < overlay_math::kFingers; ++f) {
    const float radius = overlay_math::kShape[f].radius;
    if (fingers_bend) {
      for (int k = 0; k < 3; ++k) out[n][k] = joints[f][0][k];
      gap[n] = radius;
      kind[n++] = kKindJoint;
      continue;
    }
    float points[kFingerSamples][3], gaps[kFingerSamples];
    const int count = FingerSamples(joints[f], radius, skin, points, gaps);
    for (int i = 0; i < count; ++i) {
      for (int k = 0; k < 3; ++k) out[n][k] = points[i][k];
      gap[n] = gaps[i];
      kind[n++] = i == overlay_math::kJoints ? kKindTip
                  : i < overlay_math::kJoints ? kKindJoint
                                              : kKindMiddle;
    }
  }
  if (!fingers_bend) {
    for (int k = 0; k < 3; ++k) out[n][k] = point[k];
    gap[n] = skin;
    kind[n++] = kKindPoint;
  }
  return n;
}

// THE LIFT: how far the hand must go up so that each point keeps its gap
// over the land. land(x, z) gives the height of the land. 0 when the hand is
// over the land. A point with a height of the land that is not a number does
// not count.
template <typename Land>
inline float Lift(const Sample* samples, int count, Land land,
                  int* lowest = nullptr) {
  float lift = 0.0f;
  int at = -1;
  for (int i = 0; i < count; ++i) {
    const float y = land(samples[i].p[0], samples[i].p[2]);
    if (!(y == y)) continue;
    const float need = y + samples[i].gap - samples[i].p[1];
    if (need > lift) {
      lift = need;
      at = i;
    }
  }
  if (lowest != nullptr) *lowest = at;
  return lift;
}

// The drawn place of a point of the real hand.
inline void Drawn(const float real[3], float lift, float out[3]) {
  out[0] = real[0];
  out[1] = real[1] + (lift > 0.0f ? lift : 0.0f);
  out[2] = real[2];
}

// THE POINT OF THE GAME follows the drawn hand only while it is the tip of
// the index finger of that hand: with a reach of one to one, the reach point
// is that tip. A reach point that is farther than most_m meters of the room
// from the tip keeps its own place.
inline bool PointOnHand(const float point[3], const float tip[3],
                        float units_per_meter, float most_m) {
  if (!(units_per_meter > 0.0f)) return false;
  const float d[3] = {point[0] - tip[0], point[1] - tip[1],
                      point[2] - tip[2]};
  const float most = most_m * units_per_meter;
  return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= most * most;
}

// THE PULSE when the hand first touches the land. One pulse when the land
// holds the hand up and it did not for kQuietSeconds. So a hand that rests
// on the land, or slides on it, gives no second pulse. quiet keeps the time
// with no touch. The rule is the rule of hand feel (MeetPulse).
const float kQuietSeconds = 0.20f;

inline bool TouchPulse(bool touching, float dt, float* quiet) {
  if (!touching) {
    if (dt > 0.0f) *quiet += dt;
    if (*quiet > 10.0f) *quiet = 10.0f;
    return false;
  }
  const bool pulse = *quiet >= kQuietSeconds;
  *quiet = 0.0f;
  return pulse;
}

}  // namespace hand_land
