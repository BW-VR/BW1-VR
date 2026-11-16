// THE GLOW AT THE END OF THE MENU RAY (docs/hand-model.md, "The glow at the
// end of the menu ray"). main.cpp includes this file after Check and Near,
// just before main.
#pragma once

#include "../../src/mod/ray_card_math.h"
#include "../../src/mod/ray_tip_math.h"

namespace ray_tip_checks {

inline float Length3(const float v[3]) {
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

inline void RayTipChecks() {
  using namespace ray_tip_math;
  printf("\nThe glow at the end of the menu ray\n");
  const float rgb[3] = {1.00f, 0.95f, 0.75f};

  // THE SIZE: the same share of the distance near and far, in its limits.
  Near("at 1 m the radius is 1.3 cm", RadiusM(1.0f), 0.013, 1e-6);
  Near("at 2 m the radius is 2.6 cm", RadiusM(2.0f), 0.026, 1e-6);
  Near("the glow has the same size in the view at 1 m and 2 m",
       RadiusM(2.0f) / 2.0f, RadiusM(1.0f) / 1.0f, 1e-6);
  Near("near the eye the radius is 6 mm at the least", RadiusM(0.1f), kMinM,
       1e-6);
  Near("far away the radius is 8 cm at the most", RadiusM(50.0f), kMaxM, 1e-6);

  // THE LIGHT: full at the middle, none at the edge, no step on the way, and
  // smooth at the edge, so there is no outline.
  float mid[3], edge[3], near_edge[3], rim[3];
  Light(0.0f, rgb, mid);
  Light(1.0f, rgb, edge);
  Light(0.95f, rgb, near_edge);
  Light(0.5f, rgb, rim);
  Near("the middle has the color of the ray", mid[2], rgb[2], 1e-6);
  Check(edge[0] == 0.0f && edge[1] == 0.0f && edge[2] == 0.0f,
        "the edge has no light", edge[0], 0.0);
  Check(near_edge[0] < 0.02f, "5 percent in from the edge the light is nearly none",
        near_edge[0], 0.0);
  bool down = true, no_step = true;
  float most_step = 0.0f;
  for (int i = 1; i <= 200; ++i) {
    float a[3], b[3];
    Light(0.005f * static_cast<float>(i - 1), rgb, a);
    Light(0.005f * static_cast<float>(i), rgb, b);
    if (b[0] > a[0] + 1e-6f) down = false;
    const float step = a[0] - b[0];
    if (step > most_step) most_step = step;
  }
  if (most_step > 0.02f) no_step = false;
  Check(down, "the light falls from the middle to the edge", down, 1);
  Check(no_step, "the light has no step: at most 2 percent in each half percent",
        most_step, 0.02);
  // The outer part is gold: less blue than the ray has, for the paper.
  Check(rim[2] / rim[0] < rgb[2] / rgb[0] - 0.1f,
        "half way out the glow is more gold than the ray", rim[2] / rim[0],
        rgb[2] / rgb[0]);
  float mid_alpha_light[3];
  Light(0.0f, rgb, mid_alpha_light);
  Near("the middle hides 55 percent of the picture behind it",
       Alpha(mid_alpha_light), 0.55, 1e-6);
  Near("the edge hides nothing", Alpha(edge), 0.0, 1e-6);

  // THE DISC IN THE EYE PICTURES: round and square to the line to the eye,
  // moved toward the eye by its radius.
  {
    static glow_math::Triangle tris[kTriangles];
    const float center[3] = {100.0f, 20.0f, -300.0f};
    const float eye[3] = {130.0f, 60.0f, -120.0f};
    const float radius = 2.5f;
    const int n = BuildSpot(center, radius, eye, rgb, tris, kTriangles);
    Check(n == kTriangles, "the disc has all its triangles", n, kTriangles);
    float v[3] = {eye[0] - center[0], eye[1] - center[1], eye[2] - center[2]};
    const float d = Length3(v);
    for (int k = 0; k < 3; ++k) v[k] /= d;
    float pulled[3];
    for (int k = 0; k < 3; ++k) pulled[k] = center[k] + v[k] * radius;
    float most_r = 0.0f, least_r = 1e9f, most_plane = 0.0f;
    float bright = 0.0f, outer = 1.0f;
    for (int t = 0; t < n; ++t) {
      for (int c = 0; c < 3; ++c) {
        float w[3];
        for (int k = 0; k < 3; ++k) w[k] = tris[t].p[c][k] - pulled[k];
        const float plane = fabsf(w[0] * v[0] + w[1] * v[1] + w[2] * v[2]);
        if (plane > most_plane) most_plane = plane;
        const float r = Length3(w);
        if (r > radius * 0.99f) {
          if (r > most_r) most_r = r;
          if (r < least_r) least_r = r;
          if (tris[t].c[c][0] < outer) outer = tris[t].c[c][0];
        }
        if (r < 1e-4f && tris[t].c[c][0] > bright) bright = tris[t].c[c][0];
      }
    }
    Near("each corner of the disc is in the plane square to the eye", most_plane,
         0.0, 1e-3);
    Near("the outer ring is round: its farthest corner", most_r, radius, 1e-3);
    Near("the outer ring is round: its nearest corner", least_r, radius, 1e-3);
    Near("the middle of the disc has the full light", bright, rgb[0], 1e-6);
    Near("the outer ring of the disc has no light", outer, 0.0, 1e-6);
    // Two eyes 6.4 units apart each see a disc square to their own line.
    const float eye2[3] = {136.4f, 60.0f, -120.0f};
    BuildSpot(center, radius, eye2, rgb, tris, kTriangles);
    float v2[3] = {eye2[0] - center[0], eye2[1] - center[1],
                   eye2[2] - center[2]};
    const float d2 = Length3(v2);
    for (int k = 0; k < 3; ++k) v2[k] /= d2;
    float w2[3];
    for (int k = 0; k < 3; ++k) {
      w2[k] = tris[kSides].p[1][k] - (center[k] + v2[k] * radius);
    }
    Near("the disc of the second eye is square to its own line",
         w2[0] * v2[0] + w2[1] * v2[1] + w2[2] * v2[2], 0.0, 1e-3);
    // The middle moves at most half the way to the eye.
    const float close_eye[3] = {100.0f, 20.0f, -298.0f};
    BuildSpot(center, radius, close_eye, rgb, tris, kTriangles);
    Near("an eye near the end: the middle moves half the way", tris[0].p[0][2],
         -299.0, 1e-4);
    Check(BuildSpot(center, radius, center, rgb, tris, kTriangles) == 0,
          "an eye at the end gives no disc", 0, 0);
    Check(BuildSpot(center, radius, eye, rgb, tris, 7) == 7,
          "the disc writes no more than the room", 7, 7);
  }

  // THE QUAD LAYER OVER THE PANELS: at the end, facing the head, as wide as
  // the glow.
  {
    const float end[3] = {0.3f, 1.1f, -1.2f};
    const float head[3] = {0.0f, 1.6f, 0.0f};
    ray_card_math::Quad q;
    float distance = 0.0f;
    const bool placed = Place(end, head, &q, &distance);
    Check(placed, "the end of a ray of the panels has a quad", placed, 1);
    const float to_head[3] = {head[0] - end[0], head[1] - end[1],
                              head[2] - end[2]};
    Near("the distance to the head", distance, Length3(to_head), 1e-5);
    Near("the quad is as wide as the glow", q.width, 2.0f * RadiusM(distance),
         1e-6);
    Near("the quad is square", q.height, q.width, 1e-6);
    Near("the middle of the quad is at the end", q.position[2], end[2], 1e-6);
    const float ex[3] = {1, 0, 0}, ey[3] = {0, 1, 0}, ez[3] = {0, 0, 1};
    float ax[3], ay[3], az[3], xy[3];
    ray_card_math::Rotate(q.orientation, ex, ax);
    ray_card_math::Rotate(q.orientation, ey, ay);
    ray_card_math::Rotate(q.orientation, ez, az);
    float u[3] = {to_head[0], to_head[1], to_head[2]};
    ray_card_math::Unit(u);
    Near("the front of the quad faces the head", ray_card_math::Dot(az, u), 1.0,
         1e-5);
    ray_card_math::Cross(ax, ay, xy);
    Check(ray_card_math::Dot(xy, az) > 0.999f, "the axes are a right hand set",
          ray_card_math::Dot(xy, az), 1);
    // An eye 3.2 cm from the middle of the head sees the quad nearly face on.
    float eye_way[3];
    for (int k = 0; k < 3; ++k) eye_way[k] = head[k] - end[k] + ex[k] * 0.032f;
    ray_card_math::Unit(eye_way);
    const float seen = ray_card_math::Dot(az, eye_way);
    Check(seen > 0.998f,
          "an eye beside the middle of the head sees the circle less than 0.3 "
          "percent narrower", seen, 1.0);
    // A ray end straight above or below the head still has a quad.
    const float above[3] = {0.0f, 2.6f, 0.0f};
    Check(Place(above, head, &q, &distance), "an end straight above the head",
          1, 1);
    Check(!Place(head, head, &q, &distance), "an end at the head has no quad", 0,
          0);
  }

  // THE PICTURE: round, clear at the edge and in the corners, in BGRA.
  {
    float m[4], e[4], corner[4], left[4], top[4];
    const int h = kPixels / 2;
    Texel(rgb, h, h, m);
    Texel(rgb, 0, h, e);
    Texel(rgb, 0, 0, corner);
    Texel(rgb, h - 10, h, left);
    Texel(rgb, h, h - 10, top);
    Check(m[0] > 0.95f && m[3] > 0.5f, "the middle of the picture is bright",
          m[0], 1.0);
    Check(e[3] < 0.01f && corner[3] == 0.0f && corner[0] == 0.0f,
          "the edge and the corners of the picture are clear", e[3], 0.0);
    Near("the picture is round: the same light to the side and up", left[0],
         top[0], 1e-6);
    static uint8_t picture[kPixels * kPixels * 4];
    Draw(rgb, picture);
    const uint8_t* p = picture + (static_cast<size_t>(h) * kPixels + h) * 4;
    Near("Draw puts the red in the third byte", p[2], m[0] * 255.0, 1.0);
    Near("Draw puts the blue in the first byte", p[0], m[2] * 255.0, 1.0);
    Near("Draw puts the alpha in the fourth byte", p[3], m[3] * 255.0, 1.0);
  }

  // THE STRIP WITH NO MARK (ray_tip=1): it stops at the end of the ray, and
  // has no ink at the end.
  {
    using ray_card_math::Look;
    const Look no_mark = ray_card_math::LookOf(true, true, 0.012f, 0.002f, false);
    const Look mark = ray_card_math::LookOf(true, true, 0.012f, 0.002f, true);
    Near("the strip with no mark has no end radius", no_mark.end_radius, 0.0,
         1e-9);
    Check(mark.end_radius > 0.0f, "the strip of ray_tip=0 keeps its mark",
          mark.end_radius, ray_card_math::kEndM);
    const float from[3] = {0.1f, 1.0f, -0.3f};
    const float to[3] = {0.1f, 1.0f, -1.3f};
    const float head[3] = {0.1f, 1.4f, 0.0f};
    ray_card_math::Quad q;
    float length = 0.0f;
    ray_card_math::Place(from, to, head, no_mark, &q, &length);
    Near("the strip with no mark stops at the end", q.height, 1.0, 1e-5);
    float px[4];
    const int across = ray_card_math::kAcross;
    const int ring_col = static_cast<int>(
        (0.5f + 0.5f * 0.8f * ray_card_math::kEndM / no_mark.half_width) *
        static_cast<float>(across));
    ray_card_math::Texel(no_mark, rgb, 1.0f, 0.0f, ring_col, 0, px);
    Check(!(px[3] > 0.6f && px[0] < 0.35f),
          "the strip with no mark has no dark ink at the end", px[0], 1.0);
  }
}

}  // namespace ray_tip_checks
