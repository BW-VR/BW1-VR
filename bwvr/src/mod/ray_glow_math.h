// The math of the glow of a ray, as pure functions with no game state.
//
// overlay.cpp calls these with the lines of the menu ray, the tether of the
// far grab and the arc of the teleport, and test/controller_math calls them
// with numbers it makes up. Nothing here reads a global or touches the game.
// See docs/hand-model.md, "Overlay renderer", "The glow of a ray".
//
// THE LOOK. A line gets a glow ribbon along its length (glow_math.h). The
// light on the ribbon is never less than kFloor. Bright bands run along the
// line from the hand outward, one band each kPeriodM of the room, at kSpeedMps.
// A band is a smooth bump, so the light changes slowly along the line. Only
// the look of the line moves. The line itself and the world stay where they are.
//
// A very long line (a ray to a wall) has fewer, wider bands, so the ribbon
// does not need more than kMaxSamples points.

#pragma once

#include <math.h>

namespace ray_glow_math {

const float kPeriodM = 0.30f;
const float kSpeedMps = 0.90f;
const float kFloor = 0.40f;
// The light is none at the hand and full this far from it.
const float kFadeInM = 0.06f;
const int kMaxSamples = 64;
// A band has this many samples.
const float kSamplesPerBand = 6.0f;

// The distance, in meters of the room, between two bands on a line of the
// given length.
inline float Period(float length_m) {
  const float wide = length_m / (static_cast<float>(kMaxSamples - 1) /
                                 kSamplesPerBand);
  return wide > kPeriodM ? wide : kPeriodM;
}

// The count of samples of a line of the given length: kSamplesPerBand for each
// band, at least 2 and at most kMaxSamples.
inline int Samples(float length_m) {
  if (!(length_m > 0.0f)) return 2;
  const float n = length_m / Period(length_m) * kSamplesPerBand + 1.0f;
  if (n < 2.0f) return 2;
  if (n > static_cast<float>(kMaxSamples)) return kMaxSamples;
  return static_cast<int>(ceilf(n));
}

// The light at a distance from the hand, kFloor to 1, at a time.
// The bands move outward: the band that is at distance d at time t was at
// d - kSpeedMps * dt at time t - dt.
inline float Pulse(float distance_m, float seconds, float period_m) {
  const float phase = (distance_m - kSpeedMps * seconds) / period_m;
  const float f = phase - floorf(phase);
  const float bump = 0.5f - 0.5f * cosf(6.2831853f * f);
  return kFloor + (1.0f - kFloor) * bump * bump;
}

// The share of the light near the hand: 0 at the hand, 1 from kFadeInM.
inline float FadeIn(float distance_m) {
  if (!(distance_m > 0.0f)) return 0.0f;
  if (distance_m >= kFadeInM) return 1.0f;
  const float a = distance_m / kFadeInM;
  return a * a * (3.0f - 2.0f * a);
}

// A point of a polyline at a distance along it. cumulative[i] is the length
// from point 0 to point i. A distance past the end gives the last point.
inline void AlongPolyline(const float (*points)[3], const float* cumulative,
                          int count, float distance, float out[3]) {
  if (count <= 0) return;
  if (!(distance > 0.0f) || count == 1) {
    for (int k = 0; k < 3; ++k) out[k] = points[0][k];
    return;
  }
  int i = 1;
  while (i < count - 1 && cumulative[i] < distance) ++i;
  const float span = cumulative[i] - cumulative[i - 1];
  float t = span > 1e-12f ? (distance - cumulative[i - 1]) / span : 1.0f;
  if (t > 1.0f) t = 1.0f;
  if (t < 0.0f) t = 0.0f;
  for (int k = 0; k < 3; ++k) {
    out[k] = points[i - 1][k] + t * (points[i][k] - points[i - 1][k]);
  }
}

}  // namespace ray_glow_math
