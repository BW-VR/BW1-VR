// THE GLOW ALONE AND THE RAY OVER THE PANELS (docs/hand-model.md, "The glow
// ribbons", and docs/ui-layer.md, "The ray over the panels"). main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/ray_card_math.h"
#include "../../src/mod/ray_glow_math.h"

namespace ray_card_checks {

// The share of the half width at which the light across is half the core
// light, found by steps.
inline float HalfLight(float core, float rim) {
  float x = 0.0f;
  while (x < 1.0f && ray_glow_math::Across(x, core, rim) > 0.5f * core) {
    x += 0.0005f;
  }
  return x;
}

inline void GlowAloneChecks() {
  using namespace ray_glow_math;
  printf("\nThe glow alone\n");

  Near("the light across is the core light on the middle line",
       Across(0.0f, 0.9f, 0.4f), 0.9, 1e-6);
  Near("the light across is the rim light at the rim", Across(kRimAt, 0.9f,
       0.4f), 0.4, 1e-6);
  Near("the light across is none at the edge", Across(1.0f, 0.9f, 0.4f), 0.0,
       1e-6);
  Near("the light across is the same at the two sides",
       Across(-0.3f, 0.9f, 0.4f), Across(0.3f, 0.9f, 0.4f), 1e-6);
  bool down = true;
  for (int i = 1; i <= 100; ++i) {
    const float x = 0.01f * static_cast<float>(i);
    if (Across(x, 1.0f, kOnlyRim) > Across(x - 0.01f, 1.0f, kOnlyRim) + 1e-6f) {
      down = false;
    }
  }
  Check(down, "the light falls from the middle line to the edge", down, 1);

  // Over a tube the core follows the band. The glow alone keeps 0.70 of it
  // between two bands.
  Near("over a tube the core light is the band", CoreLight(0.4f, false), 0.4,
       1e-6);
  Near("the glow alone keeps 0.70 of the core between two bands",
       CoreLight(kFloor, true), 0.70, 1e-6);
  Near("the glow alone has the full core at a band", CoreLight(1.0f, true),
       1.0, 1e-6);
  Near("the rim of the glow alone between two bands", RimLight(kFloor, true),
       0.22, 1e-6);
  Check(RimLight(1.0f, true) < RimLight(1.0f, false),
        "the rim of the glow alone is darker than over a tube",
        RimLight(1.0f, true), RimLight(1.0f, false));

  // The profile at the peak of a band, as the commit gives it.
  const float old_half = HalfLight(CoreLight(1.0f, false), RimLight(1.0f, false));
  const float new_half = HalfLight(CoreLight(1.0f, true), RimLight(1.0f, true));
  printf("  the profile: over a tube the half light is %.3f half widths from "
         "the middle, %.2f cm for the menu ray. The glow alone: %.3f, %.2f cm\n",
         old_half, old_half * 1.2f, new_half, new_half * 1.2f * kOnlyWidth);
  Near("over a tube the half light is 0.62 half widths out", old_half, 0.615,
       2e-3);
  Near("the glow alone has the half light 0.55 half widths out", new_half,
       0.545, 2e-3);
  Near("the glow alone is 20 percent narrower", kOnlyWidth, 0.80, 1e-6);
  Check(new_half * kOnlyWidth < 0.75f * old_half,
        "the bright band of the glow alone is 25 percent narrower or more",
        new_half * kOnlyWidth, old_half);
}

inline void RayCardChecks() {
  using namespace ray_card_math;
  printf("\nThe ray over the panels\n");

  // THE LOOKS.
  const float glow_half = 0.012f, tube = 0.002f;
  const Look only = LookOf(true, true, glow_half, tube);
  const Look over = LookOf(true, false, glow_half, tube);
  const Look plain = LookOf(false, true, glow_half, tube);
  Check(only.glow && only.only && !only.tube,
        "the glow alone has no tube", only.tube, 0);
  Check(over.glow && !over.only && over.tube,
        "the glow over a tube has the tube", over.tube, 1);
  Check(!plain.glow && !plain.only && plain.tube,
        "with no glow the strip is the tube", plain.tube, 1);
  Near("the strip of the glow alone is 0.96 cm from the middle to the edge",
       only.half_width, 0.0096, 1e-6);
  Near("the strip of a tube under a glow is the glow, 1.2 cm",
       over.half_width, 0.012, 1e-6);
  Near("the strip of a plain tube is 3 radii, 6 mm", plain.half_width, 0.006,
       1e-6);
  Check(plain.end_radius <= plain.half_width && only.end_radius <= only.half_width,
        "the mark at the end fits in the strip", plain.end_radius,
        plain.half_width);

  // THE PLACE. A ray of 1 m straight ahead (-z) at the height of the hand, a
  // head 40 cm above it and 30 cm behind its start.
  const float from[3] = {0.1f, 1.0f, -0.3f};
  const float to[3] = {0.1f, 1.0f, -1.3f};
  const float head[3] = {0.1f, 1.4f, 0.0f};
  Quad q;
  float length = 0.0f;
  bool placed = Place(from, to, head, only, &q, &length);
  Check(placed, "a ray of 1 m has a strip", placed, 1);
  Near("the length of the ray", length, 1.0, 1e-5);
  Near("the strip is as wide as the look", q.width, 2.0f * only.half_width,
       1e-6);
  Near("the strip goes on past the end by the mark", q.height,
       1.0f + only.end_radius, 1e-5);
  const float ex[3] = {1, 0, 0}, ey[3] = {0, 1, 0}, ez[3] = {0, 0, 1};
  float ax[3], ay[3], az[3];
  Rotate(q.orientation, ex, ax);
  Rotate(q.orientation, ey, ay);
  Rotate(q.orientation, ez, az);
  Near("the y axis of the strip goes along the ray", ay[2], -1.0, 1e-5);
  Near("the front of the strip faces up to the head", az[1], 1.0, 1e-5);
  Near("the x axis of the strip is across the ray", fabsf(ax[0]), 1.0, 1e-5);
  float bottom[3];
  for (int k = 0; k < 3; ++k) {
    bottom[k] = q.position[k] - ay[k] * 0.5f * q.height;
  }
  Near("the bottom edge of the strip is at the hand",
       sqrtf((bottom[0] - from[0]) * (bottom[0] - from[0]) +
             (bottom[1] - from[1]) * (bottom[1] - from[1]) +
             (bottom[2] - from[2]) * (bottom[2] - from[2])),
       0.0, 1e-5);
  const float qlen = sqrtf(q.orientation[0] * q.orientation[0] +
                           q.orientation[1] * q.orientation[1] +
                           q.orientation[2] * q.orientation[2] +
                           q.orientation[3] * q.orientation[3]);
  Near("the orientation is a unit quaternion", qlen, 1.0, 1e-5);

  // A turned ray: up and to the right, the head to the left. The front is
  // square to the ray and toward the head, and the axes are a right hand set.
  const float from2[3] = {0.2f, 0.9f, -0.4f};
  const float to2[3] = {0.8f, 1.3f, -1.1f};
  const float head2[3] = {-0.3f, 1.5f, 0.1f};
  placed = Place(from2, to2, head2, only, &q, &length);
  Rotate(q.orientation, ex, ax);
  Rotate(q.orientation, ey, ay);
  Rotate(q.orientation, ez, az);
  float ray[3] = {to2[0] - from2[0], to2[1] - from2[1], to2[2] - from2[2]};
  Unit(ray);
  float mid_to_head[3];
  for (int k = 0; k < 3; ++k) mid_to_head[k] = head2[k] - q.position[k];
  float xy[3];
  Cross(ax, ay, xy);
  Check(placed && fabsf(Dot(ay, ray) - 1.0f) < 1e-4f,
        "a turned ray: the y axis goes along it", Dot(ay, ray), 1.0);
  Check(fabsf(Dot(az, ray)) < 1e-4f, "a turned ray: the front is square to it",
        Dot(az, ray), 0.0);
  Check(Dot(az, mid_to_head) > 0.0f,
        "a turned ray: the front is toward the head", Dot(az, mid_to_head), 1);
  Check(Dot(xy, az) > 0.999f, "the axes are a right hand set", Dot(xy, az), 1);
  // A ray that points at the head still has a strip.
  const float at_head[3] = {0.1f, 1.4f, 2.0f};
  // The head is on the line of the ray.
  placed = Place(from, at_head, from, only, &q, &length);
  Rotate(q.orientation, ez, az);
  float at[3] = {at_head[0] - from[0], at_head[1] - from[1],
                 at_head[2] - from[2]};
  Unit(at);
  Check(placed && fabsf(Dot(az, at)) < 1e-4f,
        "a ray that points at the head has a strip square to it", Dot(az, at),
        0.0);
  // THE CASE OF A DESKTOP RUN: the head at 0 0 0 and a ray of 2 m from
  // 0 0 -1 that points 5 degrees from the line of sight. The front is the
  // best turn about the ray, and no turn about the ray faces the head more.
  {
    const float f3[3] = {0.0f, 0.0f, -1.0f};
    const float t3[3] = {-0.277f, -0.209f, -2.970f};
    const float h3[3] = {0.0f, 0.0f, 0.0f};
    Quad q3;
    float l3 = 0.0f;
    const bool p3 = Place(f3, t3, h3, only, &q3, &l3);
    const Facing face = FacingOf(q3, f3, t3, h3);
    Check(p3 && face.off_best_deg < 0.1f,
          "a run case: the front is 0 degrees from the best turn to the head",
          face.off_best_deg, 0.0);
    Near("a run case: the head sees the strip at 5 degrees from its plane",
         face.sight_deg, 5.0, 0.1);
    float front[3], w[3];
    Rotate(q3.orientation, ez, front);
    for (int k = 0; k < 3; ++k) w[k] = h3[k] - q3.position[k];
    Unit(w);
    const float chosen = Dot(front, w);
    float u3[3] = {t3[0] - f3[0], t3[1] - f3[1], t3[2] - f3[2]};
    Unit(u3);
    float a3[3], b3[3];
    Rotate(q3.orientation, ex, a3);
    Cross(u3, a3, b3);
    float best = -2.0f;
    for (int i = 0; i < 360; ++i) {
      const float r = 0.0174533f * static_cast<float>(i);
      float n[3];
      for (int k = 0; k < 3; ++k) n[k] = cosf(r) * b3[k] + sinf(r) * a3[k];
      const float d = Dot(n, w);
      if (d > best) best = d;
    }
    Check(chosen >= best - 1e-4f,
          "a run case: no turn about the ray faces the head more", chosen,
          best);
    Near("a run case: the best front is 85 degrees from the way to the head",
         acosf(chosen) * 57.29578f, 85.0, 0.1);
  }
  // A ray up and out from a hand below the eyes, 30 degrees up from the
  // ground, is seen at about 20 degrees.
  {
    const float f4[3] = {0.0f, -0.4f, -0.4f};
    const float t4[3] = {0.0f, -0.4f + 0.5f, -0.4f - 0.866f};
    const float h4[3] = {0.0f, -0.4f, 0.6f};
    Quad q4;
    float l4 = 0.0f;
    Place(f4, t4, h4, only, &q4, &l4);
    const Facing face = FacingOf(q4, f4, t4, h4);
    Check(face.off_best_deg < 0.1f && face.sight_deg > 15.0f,
          "a ray up and out from the hand is seen at more than 15 degrees",
          face.sight_deg, 30.0);
  }
  // A ray of less than 1 cm has none.
  const float near_end[3] = {0.1f, 1.0f, -0.305f};
  Check(!Place(from, near_end, head, only, &q, &length),
        "a ray of 5 mm has no strip", 0, 0);

  // THE PICTURE of a ray of 1 m at a time with a band at 0.5 m.
  const float rgb[3] = {1.00f, 0.95f, 0.75f};
  const float len = 1.0f;
  // The row at a distance s from the hand.
  const auto row_at = [](const Look& look, float s) {
    const float total = 1.0f + look.end_radius;
    int row = static_cast<int>(static_cast<float>(kAlong) * (1.0f - s / total));
    if (row < 0) row = 0;
    if (row >= kAlong) row = kAlong - 1;
    return row;
  };
  float mid[4], edge[4], rimpx[4], hand[4], past[4], end[4];
  const int middle = kAcross / 2;
  // The time at which a band is at 0.5 m: Pulse peaks at half a period.
  const float t = (0.5f - 0.5f * ray_glow_math::kPeriodM) /
                  ray_glow_math::kSpeedMps;
  Texel(only, rgb, len, t, middle, row_at(only, 0.5f), mid);
  Texel(only, rgb, len, t, 0, row_at(only, 0.5f), edge);
  Texel(only, rgb, len, t, kAcross / 4, row_at(only, 0.5f), rimpx);
  Texel(only, rgb, len, t, middle, kAlong - 1, hand);
  Texel(only, rgb, len, t, 0, 0, past);
  Texel(only, rgb, len, t, middle, row_at(only, 1.0f), end);
  Check(mid[0] > 0.9f, "the glow alone is bright on the middle line", mid[0],
        1.0);
  Near("the middle line hides 55 percent of the picture behind it", mid[3],
       0.55 * mid[0], 0.03);
  Check(mid[3] < 0.9f, "the middle line of the glow alone is not opaque",
        mid[3], 0.55);
  Check(edge[0] < 0.08f && edge[3] < 0.05f,
        "the edge of the strip is nearly clear", edge[3], 0.0);
  Check(rimpx[0] < mid[0] && rimpx[0] > edge[0],
        "the rim is darker than the middle and brighter than the edge",
        rimpx[0], mid[0]);
  Check(hand[0] < 0.05f && hand[3] < 0.05f,
        "the glow is none at the hand", hand[0], 0.0);
  Check(past[3] < 0.01f, "past the mark at the end the strip is clear",
        past[3], 0.0);
  Check(end[3] > 0.99f && end[0] > 0.99f && end[1] > 0.9f,
        "the middle of the mark at the end is opaque, in the color of the ray",
        end[3], 1.0);
  // The ring of the mark: dark ink.
  float ring[4];
  const float ring_s = 1.0f;
  const float ring_d = only.end_radius * 0.8f;
  const int ring_col = static_cast<int>(
      (0.5f + 0.5f * ring_d / only.half_width) * static_cast<float>(kAcross));
  Texel(only, rgb, len, t, ring_col, row_at(only, ring_s), ring);
  Check(ring[3] > 0.6f && ring[0] < 0.35f,
        "the ring of the mark is dark ink", ring[0], kInk[0]);

  // The tube under a glow and the plain tube are opaque on the middle line.
  float tube_mid[4], tube_out[4], plain_mid[4];
  Texel(over, rgb, len, t, middle, row_at(over, 0.5f), tube_mid);
  Texel(over, rgb, len, t, 2, row_at(over, 0.5f), tube_out);
  Texel(plain, rgb, len, t, middle, row_at(plain, 0.5f), plain_mid);
  Check(tube_mid[3] > 0.99f, "the tube under a glow is opaque", tube_mid[3],
        1.0);
  Check(tube_out[3] < 0.01f && tube_out[0] > 0.0f,
        "outside the tube the glow only adds light", tube_out[3], 0.0);
  Check(plain_mid[3] > 0.99f, "the plain tube is opaque", plain_mid[3], 1.0);
  Near("the plain tube has 0.85 of the color", plain_mid[0], 0.85, 0.01);

  // Draw fills each pixel as Texel gives it.
  static uint8_t picture[kAcross * kAlong * 4];
  Draw(only, rgb, len, t, picture);
  const int r = row_at(only, 0.5f);
  const uint8_t* p = picture + (static_cast<size_t>(r) * kAcross + middle) * 4;
  Near("Draw puts the red of Texel in the third byte", p[2],
       mid[0] * 255.0, 1.0);
  Near("Draw puts the alpha of Texel in the fourth byte", p[3],
       mid[3] * 255.0, 1.0);
}

}  // namespace ray_card_checks
