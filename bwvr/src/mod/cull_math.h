// THE MODEL CULL DISTANCE (cull_distance.cpp, and docs/backend.md, "Models
// are drawn further out"). Pure math: no Windows, no game. The checks are in
// test/controller_math/cull_checks.h.
//
// The game draws the map objects of a land block only when the block is
// nearer than 700 units (0x00C37200). The fog of the sky goes from 400 to
// 900 units in clear weather, and from 15 to 350 in rain. cull_distance
// multiplies the 700, and with cull_fog=1 the fog start and end get the same
// factor. So the far edge of the objects is at the same share of the fog as
// in the game, in each weather.

#pragma once

namespace cull_math {

// The values of cull_distance: the game's own, 2, 3, 5 and 10 times. 10
// times is 7000 units, past the far corner of a land from its middle.
const int kCullCount = 5;
const float kCullFactors[kCullCount] = {1.0f, 2.0f, 3.0f, 5.0f, 10.0f};
const int kCullDefault = 3;

// The value of the ini, kept in the range. A value out of the range gets the
// default.
inline int CullIndex(int ini) {
  return ini >= 0 && ini < kCullCount ? ini : kCullDefault;
}

inline float CullFactor(int index) {
  return kCullFactors[CullIndex(index)];
}

// The next value of the debug switch, after the last one the first.
inline int CullNext(int index) { return (CullIndex(index) + 1) % kCullCount; }

// The gain of the map object distance in the God view. The view already
// multiplies the game's distance by its own gain (the scale over the life
// size scale) while it owns the frame and that gain is more than 1. The cull
// factor comes on top, at each scale and also in the frames that the view
// does not own.
inline float VanishGain(bool view_owns, float view_gain, float cull) {
  const float view = view_owns && view_gain > 1.0f ? view_gain : 1.0f;
  const float extra = cull > 1.0f ? cull : 1.0f;
  return view * extra;
}

// The factor of the fog: the cull factor with cull_fog=1, else 1.
inline float FogFactor(bool follow, float cull) {
  return follow && cull > 1.0f ? cull : 1.0f;
}

// The fog start and end that the sky asks for, times the factor.
inline void ScaleFog(float start, float end, float factor, float* out_start,
                     float* out_end) {
  if (!(factor > 1.0f)) factor = 1.0f;
  *out_start = start * factor;
  *out_end = end * factor;
}

// How much of the fog a point at this depth has, from 0 (none) to 1 (all), as
// the fog of the game computes it (fn_007FEB30): linear from the start to
// the end.
inline float FogShare(float depth, float start, float end) {
  if (!(end > start)) return depth >= end ? 1.0f : 0.0f;
  if (depth <= start) return 0.0f;
  if (depth >= end) return 1.0f;
  return (depth - start) / (end - start);
}

}  // namespace cull_math
