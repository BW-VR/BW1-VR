// THE HEAD BLINDER (docs/god-view.md, "The head under the land"): how far the
// head is over the land, the strength of the overlay from that, its fade,
// and the picture of one eye. Pure functions: no Windows and no game. The
// checks are in test/controller_math/blinder_checks.h.

#pragma once

#include <math.h>
#include <stdint.h>

namespace blinder_math {

// THE CLEARANCE: how far the head is over the land, in meters of the room.
// Below 0 the head is under the land. ground holds the height of the land at
// the head point and at the points around it, in world units, and the
// highest one counts, so a steep slope in front of the face counts too.
// units_per_meter is the scale of the view.
inline float Clearance(float head_y, const float* ground, int count,
                       float units_per_meter) {
  if (count <= 0 || !(units_per_meter > 0.0f)) return 1000.0f;
  float top = ground[0];
  for (int i = 1; i < count; ++i) {
    if (ground[i] > top) top = ground[i];
  }
  return (head_y - top) / units_per_meter;
}

// The points around the head where the land counts: the head point and four
// points at kAroundMeters of the room to the right, the left, the front and
// the back, on the land. out gets x and z for kPoints points.
const int kPoints = 5;
const float kAroundMeters = 0.06f;
inline void AroundPoints(float x, float z, float units_per_meter,
                         float out[kPoints][2]) {
  const float d = kAroundMeters * units_per_meter;
  const float dx[kPoints] = {0.0f, d, -d, 0.0f, 0.0f};
  const float dz[kPoints] = {0.0f, 0.0f, 0.0f, d, -d};
  for (int i = 0; i < kPoints; ++i) {
    out[i][0] = x + dx[i];
    out[i][1] = z + dz[i];
  }
}

// THE STRENGTH, from 0 (no overlay) to 1 (the eye sees only the overlay).
// From kStartMeters over the land the strength goes up along a smooth curve,
// and at the land and under it the strength is 1.
const float kStartMeters = 0.05f;
inline float Strength(float clearance_m) {
  if (!(clearance_m == clearance_m)) return 0.0f;
  if (clearance_m <= 0.0f) return 1.0f;
  if (clearance_m >= kStartMeters) return 0.0f;
  const float t = 1.0f - clearance_m / kStartMeters;
  return t * t * (3.0f - 2.0f * t);
}

// THE FADE. The level goes up to its target in kFadeInSeconds and down in
// kFadeOutSeconds, in a straight line.
const float kFadeInSeconds = 0.1f;
const float kFadeOutSeconds = 0.25f;
inline float Fade(float level, float target, float seconds) {
  if (!(target > 0.0f)) target = 0.0f;
  if (target > 1.0f) target = 1.0f;
  if (!(level > 0.0f)) level = 0.0f;
  if (level > 1.0f) level = 1.0f;
  if (!(seconds > 0.0f)) return level;
  if (level < target) {
    level += seconds / kFadeInSeconds;
    return level > target ? target : level;
  }
  level -= seconds / kFadeOutSeconds;
  return level < target ? target : level;
}

// THE OVERLAY OF ONE EYE. The opacity at an angle from the middle of the eye
// follows the level: at a low level only the edge of the view has the
// overlay, the clear middle gets smaller as the level goes up, and at level
// 1 each angle is opaque. The edge is kEdgeDegrees wide.
const float kEdgeDegrees = 25.0f;
const float kClearAtZero = 75.0f;  // the clear angle at level 0, off the view
inline float OpacityAt(float level, float degrees) {
  if (!(level > 0.0f)) return 0.0f;
  if (level > 1.0f) level = 1.0f;
  // The clear angle goes from kClearAtZero at level 0 to -kEdgeDegrees at
  // level 1, so at level 1 the middle is opaque too.
  const float clear = kClearAtZero * (1.0f - level) - kEdgeDegrees * level;
  float t = (degrees - clear) / kEdgeDegrees;
  if (!(t > 0.0f)) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  return t * t * (3.0f - 2.0f * t);
}

// THE COLOR: a warm gold in the middle of the eye and a deep red at the edge,
// as the sky of the game at dusk. 0xRRGGBB.
const uint32_t kGold = 0xC8862E;
const uint32_t kRed = 0x7A1E0C;
const float kRedDegrees = 50.0f;  // the edge color from this angle
inline uint32_t ColorAt(float degrees) {
  float t = degrees / kRedDegrees;
  if (!(t > 0.0f)) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  uint32_t out = 0;
  for (int shift = 0; shift <= 16; shift += 8) {
    const float a = static_cast<float>((kGold >> shift) & 0xFF);
    const float b = static_cast<float>((kRed >> shift) & 0xFF);
    const uint32_t c = static_cast<uint32_t>(a + (b - a) * t + 0.5f);
    out |= (c > 255u ? 255u : c) << shift;
  }
  return out;
}

// THE PICTURE. A fan about the middle of the eye, rings each kRingDegrees
// out to kLastDegrees, and one ring far out past the edge of the render with
// the opacity and the color of the last ring. Each ring has kSegments points.
const int kSegments = 40;
const float kRingDegrees = 5.0f;
const int kRings = 16;  // 5 to 80 degrees
const float kLastDegrees = kRingDegrees * kRings;
const int kBlinderTriangles = kSegments + kRings * kSegments * 2;
const int kBlinderVertices = kBlinderTriangles * 3;

struct BlinderPoint {
  float x, y;      // pixels of the render, y down
  uint32_t argb;   // the color with the opacity in the top byte
};

// The triangles of one eye. (cx, cy) is the middle of the eye in pixels, fx
// and fy the pixels for a tangent of 1 across and down, and far_px the radius
// of the far ring. out has room for kBlinderVertices points. Returns the
// count of points, three for each triangle, and 0 when there is nothing to
// draw.
inline int BuildBlinder(float level, float cx, float cy, float fx, float fy,
                        float far_px, BlinderPoint* out) {
  if (!(level > 0.0f) || !(fx > 0.0f) || !(fy > 0.0f)) return 0;
  const float kPi = 3.14159265f;
  // Ring 0 is the middle point, rings 1 to kRings are each kRingDegrees, and
  // ring kRings + 1 is the far ring.
  const int rings = kRings + 2;
  float rx[kRings + 2], ry[kRings + 2];
  uint32_t argb[kRings + 2];
  for (int r = 0; r < rings; ++r) {
    const float degrees = r <= kRings ? kRingDegrees * r : kLastDegrees;
    const float t = tanf(degrees * kPi / 180.0f);
    rx[r] = fx * t;
    ry[r] = fy * t;
    float a = OpacityAt(level, degrees) * 255.0f + 0.5f;
    if (a > 255.0f) a = 255.0f;
    argb[r] = (static_cast<uint32_t>(a) << 24) | ColorAt(degrees);
  }
  const float last = rx[kRings] > ry[kRings] ? rx[kRings] : ry[kRings];
  float grow = last > 0.0f ? far_px / last : 1.0f;
  if (grow < 1.0f) grow = 1.0f;
  rx[kRings + 1] = rx[kRings] * grow;
  ry[kRings + 1] = ry[kRings] * grow;
  int n = 0;
  for (int k = 0; k < kSegments; ++k) {
    const float a0 = 2.0f * kPi * static_cast<float>(k) / kSegments;
    const float a1 = 2.0f * kPi * static_cast<float>(k + 1) / kSegments;
    const float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
    // The fan of the middle.
    out[n++] = {cx, cy, argb[0]};
    out[n++] = {cx + c0 * rx[1], cy + s0 * ry[1], argb[1]};
    out[n++] = {cx + c1 * rx[1], cy + s1 * ry[1], argb[1]};
    for (int r = 1; r + 1 < rings; ++r) {
      const BlinderPoint i0 = {cx + c0 * rx[r], cy + s0 * ry[r], argb[r]};
      const BlinderPoint i1 = {cx + c1 * rx[r], cy + s1 * ry[r], argb[r]};
      const BlinderPoint o0 = {cx + c0 * rx[r + 1], cy + s0 * ry[r + 1],
                               argb[r + 1]};
      const BlinderPoint o1 = {cx + c1 * rx[r + 1], cy + s1 * ry[r + 1],
                               argb[r + 1]};
      out[n++] = i0;
      out[n++] = o0;
      out[n++] = o1;
      out[n++] = i0;
      out[n++] = o1;
      out[n++] = i1;
    }
  }
  return n;
}

}  // namespace blinder_math
