// The math of the circle that the pointer ray draws on the land, as pure
// functions with no game state.
//
// miracle.cpp and gesture_air.cpp call these with the land point of the ray
// of the pointing hand, and test/controller_math calls them with numbers it
// makes up. Nothing here reads a global or touches the game. See
// docs/miracles.md, "The circle of the pointer ray".
//
// THE PATH. While the grip draws the circle of a Storm or a shield, the land
// point of the ray of each frame goes into a path in the world. A point goes
// in when it is kStepShare of its distance from the start of the ray away
// from the last point. When the path is full, each second point goes, so the
// path keeps its shape and its two ends.
//
// THE PLAN. The game matches the circle from the pixels of the samples
// (gesture_air.cpp). The mod gives it the land path as a view from above:
// the forward direction of the ray at the start of the draw is up on the
// screen, and its right is right. So the game reads the shape on the land,
// and the angle of the view does not flatten it. A circle across kSweepShare
// of the distance of the ray is a third of the render height, about what a
// mouse gives.
//
// THE CIRCLE. The game makes the place of a circle the middle of the box of
// the path, and its size 1.05 times its width (fn_0057A5E0). On the land the
// mod takes the middle of the widest span of the path, and 1.05 times that
// span. A Storm and a shield then keep the size between the least and the
// most of their magic info (+0x58 and +0x5C of GMagicRadiusSpellInfo, see
// SpellStormAndTornado::InitWithPos and SpellShield::InitWithPos).

#pragma once

#include <math.h>

namespace circle_aim_math {

// The game's size of a circle is this times its width.
const float kSizeOfWidth = 1.05f;
// A circle across this share of the distance of the ray is a third of the
// render height.
const float kSweepShare = 0.35f;
// The distance of the ray that the plan takes at the least, in meters of the
// room, so a ray at the feet does not give huge pixels.
const float kLeastDistanceM = 0.5f;
// A point of the path goes in at this share of the distance of the ray.
const float kStepShare = 0.004f;

// THE PATH: points in the world, oldest first.
const int kMaxPath = 256;
struct Path {
  float point[kMaxPath][3];
  int count;
};

inline void Clear(Path* p) { p->count = 0; }

inline float FlatDistance(const float a[3], const float b[3]) {
  const float x = a[0] - b[0], z = a[2] - b[2];
  return sqrtf(x * x + z * z);
}

// Adds the land point when it is step away from the last point. A full path
// keeps each second point first. Returns true when the point went in.
inline bool Add(Path* p, const float land[3], float step) {
  if (p->count > 0 && FlatDistance(p->point[p->count - 1], land) < step) {
    return false;
  }
  if (p->count == kMaxPath) {
    int n = 0;
    for (int i = 0; i < kMaxPath; i += 2) {
      for (int k = 0; k < 3; ++k) p->point[n][k] = p->point[i][k];
      ++n;
    }
    // The newest point stays.
    for (int k = 0; k < 3; ++k) {
      p->point[n - 1][k] = p->point[kMaxPath - 1][k];
    }
    p->count = n;
  }
  for (int k = 0; k < 3; ++k) p->point[p->count][k] = land[k];
  ++p->count;
  return true;
}

// THE CIRCLE OF THE PATH: the middle of its widest span across the land (x
// and z), and that span. The height of the middle is the mean height of the
// two ends. False with fewer than two points.
inline bool CircleOf(const Path& p, float middle[3], float* widest) {
  middle[0] = middle[1] = middle[2] = 0.0f;
  *widest = 0.0f;
  if (p.count < 2) return false;
  int a = 0, b = 0;
  float most = -1.0f;
  for (int i = 0; i < p.count; ++i) {
    for (int j = i + 1; j < p.count; ++j) {
      const float dx = p.point[i][0] - p.point[j][0];
      const float dz = p.point[i][2] - p.point[j][2];
      const float d = dx * dx + dz * dz;
      if (d > most) {
        most = d;
        a = i;
        b = j;
      }
    }
  }
  for (int k = 0; k < 3; ++k) {
    middle[k] = 0.5f * (p.point[a][k] + p.point[b][k]);
  }
  *widest = sqrtf(most);
  return true;
}

// The size that the game gives a circle of this width.
inline float SizeOf(float widest) { return kSizeOfWidth * widest; }

// The radius that a Storm or a shield takes for a circle of this size, as
// InitWithPos keeps it: not more than most, and not less than least. A most
// or a least of 0 or less is no limit.
inline float SpellRadius(float size, float least, float most) {
  float r = size;
  if (most > 0.0f && r > most) r = most;
  if (least > 0.0f && r < least) r = least;
  return r;
}

// True when the circle of this size is larger than the spell can be.
inline bool TooLarge(float size, float most) {
  return most > 0.0f && size > most;
}

// THE PLAN of a draw: the axes on the land, the first land point and its
// pixel, and the pixels for a world unit.
struct Plan {
  float forward[2];  // x and z of the forward direction on the land
  float right[2];    // x and z of its right
  float origin[3];   // the first land point
  float start_x, start_y;
  float pixels_per_unit;
};

// The plan from the direction of the ray, the first land point, the distance
// of the ray to it in world units, the world units in a meter of the room,
// and the size of the render. The world of the game has x to the right, y up
// and z forward, so the right of a forward (x, z) is (z, -x). A ray straight
// down has no forward on the land, so the plan takes +z. False with no
// render or no scale.
inline bool PlanFrom(const float direction[3], const float land[3],
                     float distance, float units_per_meter, float width,
                     float height, Plan* out) {
  if (!(height > 0.0f) || !(width > 0.0f) || !(units_per_meter > 0.0f)) {
    return false;
  }
  float fx = direction[0], fz = direction[2];
  const float flat = sqrtf(fx * fx + fz * fz);
  if (flat > 1e-3f) {
    fx /= flat;
    fz /= flat;
  } else {
    fx = 0.0f;
    fz = 1.0f;
  }
  out->forward[0] = fx;
  out->forward[1] = fz;
  out->right[0] = fz;
  out->right[1] = -fx;
  for (int k = 0; k < 3; ++k) out->origin[k] = land[k];
  out->start_x = 0.5f * width;
  out->start_y = 0.5f * height;
  float d = distance;
  const float least = kLeastDistanceM * units_per_meter;
  if (!(d > least)) d = least;
  out->pixels_per_unit = (height / 3.0f) / (kSweepShare * d);
  return true;
}

// The pixel of a land point in the plan. The screen y is down, so forward is
// a smaller y.
inline void PlanPixel(const Plan& plan, const float land[3], float* x,
                      float* y) {
  const float dx = land[0] - plan.origin[0];
  const float dz = land[2] - plan.origin[2];
  const float r = dx * plan.right[0] + dz * plan.right[1];
  const float f = dx * plan.forward[0] + dz * plan.forward[1];
  *x = plan.start_x + r * plan.pixels_per_unit;
  *y = plan.start_y - f * plan.pixels_per_unit;
}

}  // namespace circle_aim_math
