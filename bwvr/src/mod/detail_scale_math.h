// THE DETAIL DISTANCE BY THE SIZE OF THE WORLD (detail_by_scale of bwvr.ini,
// docs/backend.md, "Models keep their detail further out"). The player makes the world
// small and sees it as a whole, and there the game's own distances do not disturb. So
// the factor of the distances follows the scale of the table.
#pragma once

#include <math.h>

namespace detail_scale_math {

// World units for one meter of the room, so a large value is a small world. Each value
// is the scale where a factor starts to disturb when the world grows: 1x at units_1x,
// 2x at units_2x, 3x at units_3x.
struct Curve {
  float units_1x = 600.0f;
  float units_2x = 250.0f;
  float units_3x = 120.0f;
};

inline bool Valid(const Curve& c) {
  return c.units_1x > c.units_2x && c.units_2x > c.units_3x && c.units_3x > 0.0f;
}

// The part from the scale `from` to the scale `to` on a scale of logarithms, because a
// step of the zoom is a ratio.
inline float PartOf(float units, float from, float to) {
  return logf(from / units) / logf(from / to);
}

// The factor for a scale. `most` is the factor of detail_distance, and the factor is
// never above it. With no table the scale is 0, and the factor is `most`. No step in
// the curve makes all models change at one scale.
inline float FactorOf(float units, const Curve& c, float most) {
  if (!Valid(c) || units <= 0.0f) return most;
  // Below the scale of 3x the factor comes to 5x within this ratio of the scale.
  const float kToFive = 1.5f;
  float factor;
  if (units >= c.units_1x) {
    factor = 1.0f;
  } else if (units >= c.units_2x) {
    factor = 1.0f + PartOf(units, c.units_1x, c.units_2x);
  } else if (units >= c.units_3x) {
    factor = 2.0f + PartOf(units, c.units_2x, c.units_3x);
  } else if (units >= c.units_3x / kToFive) {
    factor = 3.0f + 2.0f * PartOf(units, c.units_3x, c.units_3x / kToFive);
  } else {
    factor = 5.0f;
  }
  return factor < most ? factor : most;
}

}  // namespace detail_scale_math
