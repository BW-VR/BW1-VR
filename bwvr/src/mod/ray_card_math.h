// The math of the ray over the panels in the room, as pure functions with no
// game state.
//
// ray_card.cpp calls these with the ray of the pointing hand, and
// test/controller_math calls them with numbers it makes up. Nothing here reads
// a global or touches the game. See docs/ui-layer.md, "The ray over the
// panels".
//
// WHY. A panel in the room is a quad layer: the panel of places, the settings
// window, the debug window, the welcome page, the menu panel and the keyboard
// of the menus in the room. The runtime draws each quad layer over the
// projection layer, and the ray of the pointing hand is in the projection
// layer. So a panel hides the ray in front of it. The ray then goes out as a
// quad layer of its own, after all the panels.
//
// THE STRIP is a flat quad along the ray in the LOCAL space, in meters. Its y
// axis goes along the ray, from the hand at its bottom edge to the end. Its z
// axis (the front) is the part of the way to the head that is square to the
// ray, so the strip turns about the ray to face the head. Its x axis is
// across. The strip goes on past the end of the ray by the radius of the
// mark, so the round mark at the end has its middle at the end point.
//
// THE PICTURE is kAcross by kAlong pixels, BGRA, top row first, with the
// color multiplied by the alpha. The top row is the far end. A glow can have
// more light than alpha: the runtime then adds the light, as the glow ribbon
// of the eye pictures does (overlay.cpp).

#pragma once

#include <math.h>
#include <stdint.h>

#include "ray_glow_math.h"

namespace ray_card_math {

const int kAcross = 32;
const int kAlong = 1024;
// The radius of the mark at the end, in meters of the room. The inner disc
// has the color of the ray. The ring round it is dark ink, so the mark shows
// on the bright paper of a panel and on the dark land.
const float kEndM = 0.008f;
const float kEndInnerShare = 0.6f;
const float kInk[3] = {0.08f, 0.06f, 0.04f};
const float kInkAlpha = 0.75f;
// A strip with no glow is this many tube radii across from the middle to
// the edge, so the mark at the end has room.
const float kPlainHalfInRadii = 3.0f;

// THE LOOK of the strip, the look of the ray in the eye pictures:
//   ray_glow=1, ray_glow_only=1   the glow alone (ray_glow_math.h)
//   ray_glow=1, ray_glow_only=0   an opaque tube under a glow that adds light
//   ray_glow=0                    the opaque tube
struct Look {
  float half_width;   // meters, from the middle line to the edge of the strip
  bool glow;
  bool only;          // the glow alone
  bool tube;
  float tube_radius;  // meters
  float tube_level;   // the share of the color on the tube
  float end_radius;   // meters
};

// glow_half_m is the half width of the glow over a tube, as the ray in the
// eye pictures has it, and tube_radius_m the radius of its tube. With mark
// false the strip has no mark and stops at the end of the ray: the glow of
// ray_tip_math.h is the end (ray_tip=1).
inline Look LookOf(bool glow, bool only, float glow_half_m,
                   float tube_radius_m, bool mark = true) {
  Look look;
  look.glow = glow;
  look.only = glow && only;
  look.tube = !look.only;
  look.tube_radius = tube_radius_m;
  look.tube_level = 0.85f;
  look.half_width =
      !glow ? tube_radius_m * kPlainHalfInRadii
            : glow_half_m * (look.only ? ray_glow_math::kOnlyWidth : 1.0f);
  look.end_radius =
      !mark ? 0.0f : (kEndM < look.half_width ? kEndM : look.half_width);
  return look;
}

struct Quad {
  float position[3];     // the middle
  float orientation[4];  // x y z w, the front of the quad is its +z side
  float width;           // across
  float height;          // along
};

inline float Dot(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline void Cross(const float a[3], const float b[3], float out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

inline bool Unit(float v[3]) {
  const float l = sqrtf(Dot(v, v));
  if (!(l > 1e-9f)) return false;
  for (int k = 0; k < 3; ++k) v[k] /= l;
  return true;
}

// The rotation of the axes x, y and z (unit, square to each other, x = y
// cross z) as a quaternion x y z w.
inline void FromAxes(const float x[3], const float y[3], const float z[3],
                     float q[4]) {
  const float m00 = x[0], m01 = y[0], m02 = z[0];
  const float m10 = x[1], m11 = y[1], m12 = z[1];
  const float m20 = x[2], m21 = y[2], m22 = z[2];
  const float trace = m00 + m11 + m22;
  if (trace > 0.0f) {
    const float s = sqrtf(trace + 1.0f) * 2.0f;
    q[3] = 0.25f * s;
    q[0] = (m21 - m12) / s;
    q[1] = (m02 - m20) / s;
    q[2] = (m10 - m01) / s;
  } else if (m00 > m11 && m00 > m22) {
    const float s = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
    q[3] = (m21 - m12) / s;
    q[0] = 0.25f * s;
    q[1] = (m01 + m10) / s;
    q[2] = (m02 + m20) / s;
  } else if (m11 > m22) {
    const float s = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
    q[3] = (m02 - m20) / s;
    q[0] = (m01 + m10) / s;
    q[1] = 0.25f * s;
    q[2] = (m12 + m21) / s;
  } else {
    const float s = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
    q[3] = (m10 - m01) / s;
    q[0] = (m02 + m20) / s;
    q[1] = (m12 + m21) / s;
    q[2] = 0.25f * s;
  }
}

// A vector turned by the quaternion q (x y z w).
inline void Rotate(const float q[4], const float v[3], float out[3]) {
  const float u[3] = {q[0], q[1], q[2]};
  float t[3];
  Cross(u, v, t);
  for (int k = 0; k < 3; ++k) t[k] *= 2.0f;
  float c[3];
  Cross(u, t, c);
  for (int k = 0; k < 3; ++k) out[k] = v[k] + q[3] * t[k] + c[k];
}

// The quad of the strip for the ray from `from` to `to`, seen from the head
// at `head`, all in the LOCAL space in meters. *length is the length of the
// ray. False for a ray shorter than 1 cm.
inline bool Place(const float from[3], const float to[3], const float head[3],
                  const Look& look, Quad* out, float* length) {
  float y[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  const float l = sqrtf(Dot(y, y));
  if (!(l > 0.01f)) return false;
  for (int k = 0; k < 3; ++k) y[k] /= l;
  float z[3];
  for (int k = 0; k < 3; ++k) z[k] = head[k] - 0.5f * (from[k] + to[k]);
  const float to_head = sqrtf(Dot(z, z));
  const float along = Dot(z, y);
  for (int k = 0; k < 3; ++k) z[k] -= along * y[k];
  // Less than 1 mm in a meter square to the ray is the noise of the floats.
  if (!(sqrtf(Dot(z, z)) > 1e-3f * to_head) || !Unit(z)) {
    // The ray points at the head: any front square to the ray.
    const float up[3] = {0.0f, 1.0f, 0.0f};
    const float side[3] = {1.0f, 0.0f, 0.0f};
    Cross(y, fabsf(y[1]) < 0.9f ? up : side, z);
    if (!Unit(z)) return false;
  }
  float x[3];
  Cross(y, z, x);
  FromAxes(x, y, z, out->orientation);
  out->width = 2.0f * look.half_width;
  out->height = l + look.end_radius;
  for (int k = 0; k < 3; ++k) {
    out->position[k] = from[k] + y[k] * (0.5f * out->height);
  }
  *length = l;
  return true;
}

// HOW THE HEAD SEES THE STRIP. The strip holds the ray, so its front can only
// turn about the ray. off_best_deg is the angle between the front and the
// part of the way to the head that is square to the ray: 0 is the best turn.
// sight_deg is the angle between the ray and the line from the strip middle
// to the head: the head sees the strip at this angle from its plane, 90 is
// face on. A ray that points away from the head has a small sight_deg, and
// no turn about the ray makes it more. The glow ribbon of the eye pictures
// (glow_math::Sides) has the same plane, so it is seen at the same angle.
struct Facing {
  float off_best_deg;
  float sight_deg;
};

inline Facing FacingOf(const Quad& quad, const float from[3],
                       const float to[3], const float head[3]) {
  Facing out = {0.0f, 90.0f};
  float y[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  float w[3];
  for (int k = 0; k < 3; ++k) w[k] = head[k] - quad.position[k];
  if (!Unit(y)) return out;
  const float lw = sqrtf(Dot(w, w));
  if (!(lw > 1e-6f)) return out;
  const float along = Dot(w, y);
  float square[3];
  for (int k = 0; k < 3; ++k) square[k] = w[k] - along * y[k];
  const float z[3] = {0.0f, 0.0f, 1.0f};
  float front[3];
  Rotate(quad.orientation, z, front);
  const float c = Unit(square) ? Dot(front, square) : 1.0f;
  const float s = fabsf(along) / lw;
  out.off_best_deg = acosf(c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c)) *
                     57.29578f;
  out.sight_deg = acosf(s > 1.0f ? 1.0f : s) * 57.29578f;
  return out;
}

inline float Clamp01(float v) {
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// The coverage of a round or straight edge at `inside` meters inside it,
// with a soft edge of one texel.
inline float Cover(float inside, float texel) {
  return Clamp01(inside / texel + 0.5f);
}

// One row of the picture: its distance from the hand, and the light of the
// glow at the middle line and at the rim there.
struct Row {
  float s;
  float core;
  float rim;
};

// row 0 to kAlong - 1 goes from the far end to the hand. length is the
// length of the ray in meters, and seconds the clock of the glow
// (overlay.cpp reads it once a frame).
inline Row RowOf(const Look& look, float length, float seconds, int row) {
  Row out;
  const float total = length + look.end_radius;
  out.s = total * (static_cast<float>(kAlong) -
                   (static_cast<float>(row) + 0.5f)) /
          static_cast<float>(kAlong);
  out.core = out.rim = 0.0f;
  if (look.glow && out.s <= length) {
    const float band = ray_glow_math::Pulse(out.s, seconds,
                                            ray_glow_math::Period(length));
    const float fade = ray_glow_math::FadeIn(out.s);
    out.core = ray_glow_math::CoreLight(band, look.only) * fade;
    out.rim = ray_glow_math::RimLight(band, look.only) * fade;
  }
  return out;
}

// One pixel of the picture in the row `r`: rgba gets the color multiplied by
// the alpha, and the alpha. column 0 to kAcross - 1 goes across.
inline void TexelOf(const Look& look, const float rgb[3], float length,
                    const Row& r, int column, float rgba[4]) {
  const float total = length + look.end_radius;
  const float s = r.s;
  const float x = (static_cast<float>(column) + 0.5f) /
                      static_cast<float>(kAcross) * 2.0f - 1.0f;
  const float d = fabsf(x) * look.half_width;
  const float texel_x = 2.0f * look.half_width / static_cast<float>(kAcross);
  const float texel_y = total / static_cast<float>(kAlong);
  float most = rgb[0] > rgb[1] ? rgb[0] : rgb[1];
  if (rgb[2] > most) most = rgb[2];
  float c[3] = {0.0f, 0.0f, 0.0f};
  float a = 0.0f;
  if (s <= length) {
    if (look.glow) {
      const float light = ray_glow_math::Across(x, r.core, r.rim);
      for (int k = 0; k < 3; ++k) c[k] = rgb[k] * light;
      a = look.only ? ray_glow_math::kOnlyOpacity * light * most : 0.0f;
    }
    if (look.tube) {
      const float cover = Cover(look.tube_radius - d, texel_x);
      for (int k = 0; k < 3; ++k) {
        c[k] = rgb[k] * look.tube_level * cover + c[k] * (1.0f - cover);
      }
      a = cover + a * (1.0f - cover);
    }
  }
  if (!(look.end_radius > 0.0f)) {
    for (int k = 0; k < 3; ++k) rgba[k] = Clamp01(c[k]);
    rgba[3] = Clamp01(a);
    return;
  }
  // THE MARK AT THE END: the ink ring, then the disc of the color over it.
  const float e = s - length;
  const float from_end = sqrtf(e * e + d * d);
  const float texel = texel_x > texel_y ? texel_x : texel_y;
  const float ring = Cover(look.end_radius - from_end, texel) * kInkAlpha;
  for (int k = 0; k < 3; ++k) c[k] = kInk[k] * ring + c[k] * (1.0f - ring);
  a = ring + a * (1.0f - ring);
  const float disc =
      Cover(look.end_radius * kEndInnerShare - from_end, texel);
  for (int k = 0; k < 3; ++k) c[k] = rgb[k] * disc + c[k] * (1.0f - disc);
  a = disc + a * (1.0f - disc);
  for (int k = 0; k < 3; ++k) rgba[k] = Clamp01(c[k]);
  rgba[3] = Clamp01(a);
}

// One pixel, with its row.
inline void Texel(const Look& look, const float rgb[3], float length,
                  float seconds, int column, int row, float rgba[4]) {
  TexelOf(look, rgb, length, RowOf(look, length, seconds, row), column, rgba);
}

// The whole picture into out, kAcross * kAlong * 4 bytes.
inline void Draw(const Look& look, const float rgb[3], float length,
                 float seconds, uint8_t* out) {
  for (int row = 0; row < kAlong; ++row) {
    uint8_t* p = out + static_cast<size_t>(row) * kAcross * 4;
    const Row r = RowOf(look, length, seconds, row);
    for (int column = 0; column < kAcross; ++column, p += 4) {
      float rgba[4];
      TexelOf(look, rgb, length, r, column, rgba);
      p[0] = static_cast<uint8_t>(rgba[2] * 255.0f + 0.5f);
      p[1] = static_cast<uint8_t>(rgba[1] * 255.0f + 0.5f);
      p[2] = static_cast<uint8_t>(rgba[0] * 255.0f + 0.5f);
      p[3] = static_cast<uint8_t>(rgba[3] * 255.0f + 0.5f);
    }
  }
}

}  // namespace ray_card_math
