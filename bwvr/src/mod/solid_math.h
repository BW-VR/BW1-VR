// The Hand and the head stop at the solid objects: the pure math. See
// hand_land.cpp, head_blinder.cpp, docs/hand-model.md, "The Hand stops at the
// land", and docs/god-view.md, "The head under the land". The checks are in
// test/controller_math/solid_checks.h.
//
// THE SOLIDS. A house and a rock are the box of the mesh of the object, with
// the turn of the object (hand_direct.cpp, HandDirectFeelBody). A tree is
// only its trunk: an upright cylinder of near_math::kTrunkShare of the radius
// of the tree, from the land to half the height of the tree, as hand feel has
// it. The leaves do not stop the hand.
//
// THE RULE. Each point of the drawn hand keeps its gap out of each solid. When
// the real hand goes into a solid, the drawn hand goes out of it along one
// direction, until no point of it is in the solid. For a box the direction is
// the outward direction of one face: the face with the least push at the
// first touch. That face holds while the touch holds, so the drawn hand stays
// on that face and slides along it with the real hand. The drawn hand does
// not jump to the far face when the real hand goes past the middle of the
// box. A face that points down does not count: a hand cannot come up into a
// solid from under the land. For a trunk the direction is level, from the
// axis of the trunk to the palm of the real hand, so the drawn hand goes
// round the trunk with the real hand. It keeps its side when the palm goes
// past the axis.
//
// The push has no filter and moves the hand only along that direction, so
// the drawn hand moves only when the real hand moves.

#pragma once

#include <math.h>

#include "hand_land_math.h"

namespace solid_math {

const int kBox = 0;
const int kTrunk = 1;

struct Solid {
  int kind;
  // A box: its middle. A trunk: the point of its axis on the land.
  float middle[3];
  float axis[9];  // a box: its three axes of length 1, one in each row
  float half[3];  // a box: half its size along each axis
  float radius;   // a trunk
  float height;   // a trunk: from the land up
};

// THE TOUCH of one hand and one solid, from frame to frame.
struct Touch {
  bool on;
  int face;       // a box: 2 * axis, and 1 more for the minus side
  float dir[3];   // the direction of the push, of length 1
};

// A face of a box that points down more than this does not count.
const float kDownFace = -0.7f;

// A SMALL SOLID DOES NOT COUNT: a box with no half size of kLeastHalfM
// meters of the room or more (a pebble under the palm at a zoom out), and a
// trunk with a radius of less than kLeastTrunkM.
const float kLeastHalfM = 0.02f;
const float kLeastTrunkM = 0.01f;

inline bool Counts(const Solid& s, float units_per_meter) {
  if (!(units_per_meter > 0.0f)) return false;
  if (s.kind == kTrunk) {
    return s.radius >= kLeastTrunkM * units_per_meter && s.height > 0.0f;
  }
  float most = s.half[0];
  if (s.half[1] > most) most = s.half[1];
  if (s.half[2] > most) most = s.half[2];
  return most >= kLeastHalfM * units_per_meter && s.half[0] >= 0.0f &&
         s.half[1] >= 0.0f && s.half[2] >= 0.0f;
}

inline float Dot(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// The outward direction of a face of a box.
inline void FaceDir(const Solid& s, int face, float out[3]) {
  const float* a = &s.axis[(face / 2) * 3];
  const float sign = (face & 1) != 0 ? -1.0f : 1.0f;
  for (int k = 0; k < 3; ++k) out[k] = sign * a[k];
}

// A point in the frame of a box.
inline void Local(const Solid& s, const float p[3], float out[3]) {
  const float d[3] = {p[0] - s.middle[0], p[1] - s.middle[1],
                      p[2] - s.middle[2]};
  for (int j = 0; j < 3; ++j) out[j] = Dot(d, &s.axis[j * 3]);
}

// THE PUSH OUT OF A BOX along one face: how far the points (moved by offset)
// must go along the outward direction of the face, so that each point that
// is in the box with its gap is out of it. 0 when no point is in the box.
inline float BoxPush(const Solid& s, const hand_land::Sample* points,
                     int count, const float offset[3], int face) {
  const int j = face / 2;
  const float sign = (face & 1) != 0 ? -1.0f : 1.0f;
  float most = 0.0f;
  for (int i = 0; i < count; ++i) {
    const float p[3] = {points[i].p[0] + offset[0],
                        points[i].p[1] + offset[1],
                        points[i].p[2] + offset[2]};
    float l[3];
    Local(s, p, l);
    const float gap = points[i].gap;
    bool in = true;
    for (int k = 0; k < 3; ++k) {
      if (!(fabsf(l[k]) < s.half[k] + gap)) in = false;
    }
    if (!in) continue;
    const float need = s.half[j] + gap - sign * l[j];
    if (need > most) most = need;
  }
  return most;
}

// THE FACE OF THE FIRST TOUCH: the face with the least push, of the faces
// that do not point down. -1 when no face counts.
inline int FirstFace(const Solid& s, const hand_land::Sample* points,
                     int count, const float offset[3]) {
  int best = -1;
  float least = 1e30f;
  for (int face = 0; face < 6; ++face) {
    float dir[3];
    FaceDir(s, face, dir);
    if (dir[1] < kDownFace) continue;
    const float push = BoxPush(s, points, count, offset, face);
    if (push < least) {
      least = push;
      best = face;
    }
  }
  return best;
}

// THE PUSH OUT OF A TRUNK along the level direction d: how far the points
// (moved by offset) must go along d, so that each point that is in the trunk
// with its gap is out of it. A point above the trunk (in the leaves) or under
// its foot does not count. 0 when no point is in the trunk.
inline float TrunkPush(const Solid& s, const hand_land::Sample* points,
                       int count, const float offset[3], const float d[3]) {
  float most = 0.0f;
  for (int i = 0; i < count; ++i) {
    const float p[3] = {points[i].p[0] + offset[0],
                        points[i].p[1] + offset[1],
                        points[i].p[2] + offset[2]};
    if (!(p[1] >= s.middle[1] && p[1] <= s.middle[1] + s.height)) continue;
    const float qx = p[0] - s.middle[0], qz = p[2] - s.middle[2];
    const float r = s.radius + points[i].gap;
    const float c = qx * qx + qz * qz - r * r;
    if (!(c < 0.0f)) continue;
    const float b = qx * d[0] + qz * d[2];
    const float t = -b + sqrtf(b * b - c);
    if (t > most) most = t;
  }
  return most;
}

// The level direction from the axis of a trunk to a point. A point on the
// axis keeps the fallback.
inline void TrunkDir(const Solid& s, const float p[3], const float fallback[3],
                     float out[3]) {
  const float x = p[0] - s.middle[0], z = p[2] - s.middle[2];
  const float length = sqrtf(x * x + z * z);
  if (!(length > 1e-6f)) {
    for (int k = 0; k < 3; ++k) out[k] = fallback[k];
    return;
  }
  out[0] = x / length;
  out[1] = 0.0f;
  out[2] = z / length;
}

// THE PUSH OF ONE SOLID: the amount along touch->dir. palm is the middle of
// the palm of the real hand. start says whether a new touch can start (the
// first pass of a frame). A touch that is off and finds a point in the solid
// starts, with its face or its direction, when its push is start_most or
// less. A hand that is deeper at once (a teleport or a zoom put the real hand
// into the solid) starts no touch, and it stays the real hand until it comes
// out. The caller ends a touch that has no push in the frame.
inline float Push(const Solid& s, const hand_land::Sample* points, int count,
                  const float palm[3], bool start, float start_most,
                  const float offset[3], Touch* touch) {
  if (s.kind == kBox) {
    if (!touch->on) {
      if (!start || !(BoxPush(s, points, count, offset, 0) > 0.0f)) {
        return 0.0f;
      }
      const int face = FirstFace(s, points, count, offset);
      if (face < 0) return 0.0f;
      const float push = BoxPush(s, points, count, offset, face);
      if (!(push <= start_most)) return 0.0f;
      touch->on = true;
      touch->face = face;
      FaceDir(s, face, touch->dir);
      return push;
    }
    return BoxPush(s, points, count, offset, touch->face);
  }
  float d[3];
  if (!touch->on) {
    if (!start) return 0.0f;
    const float x_axis[3] = {1.0f, 0.0f, 0.0f};
    TrunkDir(s, palm, x_axis, d);
    const float push = TrunkPush(s, points, count, offset, d);
    if (!(push > 0.0f) || !(push <= start_most)) return 0.0f;
    touch->on = true;
    for (int k = 0; k < 3; ++k) touch->dir[k] = d[k];
    return push;
  }
  if (start) {
    TrunkDir(s, palm, touch->dir, d);
    if (Dot(d, touch->dir) > 0.0f) {
      for (int k = 0; k < 3; ++k) touch->dir[k] = d[k];
    }
  }
  return TrunkPush(s, points, count, offset, touch->dir);
}

// THE OFFSET OF THE DRAWN HAND from the real hand for a set of solids. Two
// passes: a push out of one solid can put the hand into another one near it.
// start_most is the largest push that starts a touch (Push).
// Only the first pass starts a touch. A touch with no push in this frame
// ends. touches has one entry for each solid, and held (when it is given)
// gets true for each solid that pushes the hand in this frame. At most
// kMostSolids solids count. Gives true when a solid holds the hand.
const int kPasses = 2;
const int kMostSolids = 32;

inline bool Offset(const Solid* solids, int count,
                   const hand_land::Sample* points, int point_count,
                   const float palm[3], float start_most, Touch* touches,
                   float offset[3], bool* held_out = nullptr) {
  for (int k = 0; k < 3; ++k) offset[k] = 0.0f;
  if (count > kMostSolids) count = kMostSolids;
  bool any = false;
  bool local[kMostSolids];
  bool* held = held_out != nullptr ? held_out : local;
  for (int i = 0; i < count; ++i) held[i] = false;
  for (int pass = 0; pass < kPasses; ++pass) {
    for (int i = 0; i < count; ++i) {
      const float push = Push(solids[i], points, point_count, palm, pass == 0,
                              start_most, offset, &touches[i]);
      if (!(push > 0.0f)) continue;
      held[i] = true;
      any = true;
      for (int k = 0; k < 3; ++k) offset[k] += push * touches[i].dir[k];
    }
  }
  for (int i = 0; i < count; ++i) {
    if (!held[i]) touches[i].on = false;
  }
  return any;
}

// THE DISTANCE from a point to a solid, in world units: less than 0 inside.
inline float Distance(const Solid& s, const float p[3]) {
  float q[3];
  int axes = 3;
  if (s.kind == kBox) {
    float l[3];
    Local(s, p, l);
    for (int j = 0; j < 3; ++j) q[j] = fabsf(l[j]) - s.half[j];
  } else {
    const float x = p[0] - s.middle[0], z = p[2] - s.middle[2];
    const float top = s.middle[1] + s.height;
    q[0] = sqrtf(x * x + z * z) - s.radius;
    const float under = s.middle[1] - p[1], over = p[1] - top;
    q[1] = under > over ? under : over;
    axes = 2;
  }
  float out = 0.0f, in = -1e30f;
  for (int j = 0; j < axes; ++j) {
    if (q[j] > 0.0f) out += q[j] * q[j];
    if (q[j] > in) in = q[j];
  }
  return sqrtf(out) + (in < 0.0f ? in : 0.0f);
}

// THE WALLS OF A HOUSE BELOW THE LIFE SIZE SCALE (solid_house_inset_pct).
// The box of the mesh of a house holds the eaves of its roof, which go past
// its walls. At a small scale the player walks beside the walls, under the
// eaves, so the box of a house gets smaller by share of each half size that
// is level: an axis that points up less than kLevelAxis keeps its height.
// share is from 0 (the whole box) to 0.5.
const float kLevelAxis = 0.7f;

inline void InsetLevel(Solid* s, float share) {
  if (s->kind != kBox || !(share > 0.0f)) return;
  if (share > 0.5f) share = 0.5f;
  for (int j = 0; j < 3; ++j) {
    if (fabsf(s->axis[j * 3 + 1]) < kLevelAxis) s->half[j] *= 1.0f - share;
  }
}

// The solid nearest to a point: the least distance, in world units, and its
// index in which (-1 with no solid, and then the distance is 1e30).
inline float Least(const Solid* solids, int count, const float p[3],
                   int* which) {
  float least = 1e30f;
  *which = -1;
  for (int i = 0; i < count; ++i) {
    const float d = Distance(solids[i], p);
    if (d < least) {
      least = d;
      *which = i;
    }
  }
  return least;
}

}  // namespace solid_math
