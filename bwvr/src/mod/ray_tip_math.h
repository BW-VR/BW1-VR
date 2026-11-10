// The math of the glow at the end of the menu ray, as pure functions with no
// game state.
//
// overlay.cpp (the ray in the eye pictures) and ray_card.cpp (the ray over
// the panels) call these with the end of the ray of the pointing hand, and
// test/controller_math calls them with numbers it makes up. Nothing here
// reads a global or touches the game. See docs/hand-model.md, "The glow at
// the end of the menu ray".
//
// THE LOOK. The end of the ray is a round soft glow that faces the eye. Its
// light is full at the middle and falls smoothly to none at the edge, so it
// has no outline. The middle has the color of the ray, and the outer part
// turns to a warm gold, so the glow also shows on the bright paper of a
// panel. The middle hides kOpacity of the picture behind it, as the core of
// the glow alone of the ray does (ray_glow_math.h).
//
// The glow is steady. The bands of the ray show that the ray is live, and a
// steady end is easier to hold on a small button.
//
// THE SIZE. The radius from the middle to the edge is kShare of the distance
// from the eye, so the glow has the same size in the view near and far. It
// is kMinM at the least and kMaxM at the most.
//
// FROM EACH EYE IT IS ROUND. In the eye pictures the glow is a disc of
// triangles that is built again in each eye pass, square to the line to the
// eye of that pass (BuildSpot). Over the panels it is a quad layer of its own
// that faces the middle of the head (Place): the eyes are 3 cm from the
// middle, so at 0.5 m each eye sees the quad less than 4 degrees from face
// on, and its circle is less than 0.3 percent narrower.

#pragma once

#include <math.h>
#include <stdint.h>

#include "glow_math.h"
#include "ray_card_math.h"

namespace ray_tip_math {

const float kShare = 0.013f;
const float kMinM = 0.006f;
const float kMaxM = 0.08f;
const float kOpacity = 0.55f;
// The gold of the outer part, and the share of the radius over which the
// color goes from the color of the ray to the gold.
const float kGold[3] = {1.00f, 0.74f, 0.22f};
const float kGoldFrom = 0.15f;
const float kGoldTo = 0.70f;

// The disc in the eye pictures: kSides corners around, and kRings rings
// from the middle to the edge, at equal steps.
const int kSides = 20;
const int kRings = 5;
// A fan to the first ring, then two triangles for each side in each band
// between two rings.
const int kTriangles = kSides + kSides * 2 * (kRings - 1);

// The picture of the quad layer: kPixels by kPixels.
const int kPixels = 64;

// The radius in meters of the room for an end at distance_m from the eye.
inline float RadiusM(float distance_m) {
  float r = kShare * distance_m;
  if (!(r > kMinM)) r = kMinM;
  if (r > kMaxM) r = kMaxM;
  return r;
}

inline float Smooth(float a, float b, float x) {
  if (x <= a) return 0.0f;
  if (x >= b) return 1.0f;
  const float t = (x - a) / (b - a);
  return t * t * (3.0f - 2.0f * t);
}

// The brightness at a share r of the radius (0 the middle, 1 the edge): 1 at
// the middle, none at the edge, and a slope of 0 at both, so the glow has
// no edge.
inline float Brightness(float r) {
  if (r < 0.0f) r = -r;
  if (r >= 1.0f) return 0.0f;
  const float a = 1.0f - r * r;
  return a * a;
}

// The light at a share r of the radius, for a ray of the color rgb.
inline void Light(float r, const float rgb[3], float out[3]) {
  const float b = Brightness(r);
  const float g = Smooth(kGoldFrom, kGoldTo, r < 0.0f ? -r : r);
  for (int k = 0; k < 3; ++k) {
    out[k] = (rgb[k] + (kGold[k] - rgb[k]) * g) * b;
  }
}

// The alpha of a light: kOpacity times its brightest part, as the glow of a
// ray has it.
inline float Alpha(const float light[3]) {
  float most = light[0] > light[1] ? light[0] : light[1];
  if (light[2] > most) most = light[2];
  return kOpacity * most;
}

// THE DISC IN THE EYE PICTURES, for one eye. center is the end of the ray in
// the world, eye the place of the eye in the world, and radius the radius in
// world units. The middle moves toward the eye by the radius (by half the
// distance at the most), on the line to the eye, so the eye sees it at the
// same place and a wall or the land at the end does not cut the disc. Writes
// at most `most` triangles and returns the count: 0 when the eye is at the
// center.
inline int BuildSpot(const float center[3], float radius, const float eye[3],
                     const float rgb[3], glow_math::Triangle* out, int most) {
  float v[3] = {eye[0] - center[0], eye[1] - center[1], eye[2] - center[2]};
  const float d = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (!(d > 1e-6f) || !(radius > 0.0f)) return 0;
  for (int k = 0; k < 3; ++k) v[k] /= d;
  const float pull = radius < 0.5f * d ? radius : 0.5f * d;
  float c[3];
  for (int k = 0; k < 3; ++k) c[k] = center[k] + v[k] * pull;
  // Two axes in the plane of the disc.
  const float up[3] = {0.0f, 1.0f, 0.0f};
  const float side[3] = {1.0f, 0.0f, 0.0f};
  float a[3];
  overlay_math::Cross(v, fabsf(v[1]) < 0.9f ? up : side, a);
  overlay_math::Normalize(a);
  float b[3];
  overlay_math::Cross(v, a, b);
  overlay_math::Normalize(b);
  // The corners of each ring, ring 0 the middle.
  float p[kRings + 1][kSides][3];
  float light[kRings + 1][3];
  for (int i = 0; i <= kRings; ++i) {
    const float share = static_cast<float>(i) / static_cast<float>(kRings);
    Light(share, rgb, light[i]);
    for (int s = 0; s < kSides; ++s) {
      const float angle =
          6.2831853f * static_cast<float>(s) / static_cast<float>(kSides);
      const float ca = cosf(angle) * radius * share;
      const float sa = sinf(angle) * radius * share;
      for (int k = 0; k < 3; ++k) p[i][s][k] = c[k] + a[k] * ca + b[k] * sa;
    }
  }
  int n = 0;
  const auto put = [&](const float* p0, const float* c0, const float* p1,
                       const float* c1, const float* p2, const float* c2) {
    if (n >= most) return;
    glow_math::Triangle& t = out[n++];
    for (int k = 0; k < 3; ++k) {
      t.p[0][k] = p0[k];
      t.p[1][k] = p1[k];
      t.p[2][k] = p2[k];
      t.c[0][k] = c0[k];
      t.c[1][k] = c1[k];
      t.c[2][k] = c2[k];
    }
  };
  for (int s = 0; s < kSides; ++s) {
    const int e = (s + 1) % kSides;
    put(c, light[0], p[1][s], light[1], p[1][e], light[1]);
  }
  for (int i = 1; i < kRings; ++i) {
    for (int s = 0; s < kSides; ++s) {
      const int e = (s + 1) % kSides;
      put(p[i][s], light[i], p[i + 1][s], light[i + 1], p[i + 1][e],
          light[i + 1]);
      put(p[i][s], light[i], p[i + 1][e], light[i + 1], p[i][e], light[i]);
    }
  }
  return n;
}

// THE QUAD LAYER OVER THE PANELS. The middle is at the end of the ray, and
// the front faces the middle of the head. All in the LOCAL space in meters.
// *distance gets the distance from the head. False when the end is at the
// head.
inline bool Place(const float end[3], const float head[3],
                  ray_card_math::Quad* out, float* distance) {
  float z[3] = {head[0] - end[0], head[1] - end[1], head[2] - end[2]};
  const float d = sqrtf(ray_card_math::Dot(z, z));
  if (!(d > 0.01f) || !ray_card_math::Unit(z)) return false;
  const float up[3] = {0.0f, 1.0f, 0.0f};
  const float side[3] = {1.0f, 0.0f, 0.0f};
  float x[3];
  ray_card_math::Cross(fabsf(z[1]) < 0.9f ? up : side, z, x);
  if (!ray_card_math::Unit(x)) return false;
  float y[3];
  ray_card_math::Cross(z, x, y);
  ray_card_math::FromAxes(x, y, z, out->orientation);
  const float r = RadiusM(d);
  out->width = out->height = 2.0f * r;
  for (int k = 0; k < 3; ++k) out->position[k] = end[k];
  *distance = d;
  return true;
}

// One pixel of the picture of the quad layer: rgba gets the light and the
// alpha. The light is not multiplied by the alpha: the compositor adds the
// light, as for the strip of the ray (ray_card_math.h).
inline void Texel(const float rgb[3], int column, int row, float rgba[4]) {
  const float half = 0.5f * static_cast<float>(kPixels);
  const float x = (static_cast<float>(column) + 0.5f - half) / half;
  const float y = (static_cast<float>(row) + 0.5f - half) / half;
  float light[3];
  Light(sqrtf(x * x + y * y), rgb, light);
  for (int k = 0; k < 3; ++k) rgba[k] = ray_card_math::Clamp01(light[k]);
  rgba[3] = ray_card_math::Clamp01(Alpha(light));
}

// The whole picture into out, kPixels * kPixels * 4 bytes, BGRA.
inline void Draw(const float rgb[3], uint8_t* out) {
  for (int row = 0; row < kPixels; ++row) {
    for (int column = 0; column < kPixels; ++column) {
      float rgba[4];
      Texel(rgb, column, row, rgba);
      uint8_t* p = out + (static_cast<size_t>(row) * kPixels + column) * 4;
      p[0] = static_cast<uint8_t>(rgba[2] * 255.0f + 0.5f);
      p[1] = static_cast<uint8_t>(rgba[1] * 255.0f + 0.5f);
      p[2] = static_cast<uint8_t>(rgba[0] * 255.0f + 0.5f);
      p[3] = static_cast<uint8_t>(rgba[3] * 255.0f + 0.5f);
    }
  }
}

}  // namespace ray_tip_math
