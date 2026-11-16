// THE HANDS IN THE CUT SCENE WINDOW (cutscene_hands_math.h) AND THE INFLUENCE
// AT THE HAND (influence_edge_math.h). main.cpp includes this file after
// Check and Near, just before main.
#pragma once

#include "../../src/mod/cutscene_hands_math.h"
#include "../../src/mod/influence_edge_math.h"
#include "../../src/mod/overlay_math.h"

namespace scene_checks {

// A screen 2 m ahead of a head at the life size scale (7.5 units a meter),
// in a room with x right, y up and z forward: 2.53 by 2.6 m, eyes 64 mm
// apart, and eye pictures of 2880 by 2880 that lose 72 columns and one edge
// column at each side.
inline cutscene_hands_math::View TestView() {
  const float s = 7.5f;
  cutscene_hands_math::View v;
  const float center[3] = {0.0f, 0.0f, 2.0f * s};
  const float right[3] = {1.0f, 0.0f, 0.0f};
  const float up[3] = {0.0f, 1.0f, 0.0f};
  const float forward[3] = {0.0f, 0.0f, 1.0f};
  for (int k = 0; k < 3; ++k) {
    v.center[k] = center[k];
    v.right[k] = right[k];
    v.up[k] = up[k];
    v.forward[k] = forward[k];
    v.eye[0][k] = 0.0f;
    v.eye[1][k] = 0.0f;
  }
  v.eye[0][0] = -0.032f * s;
  v.eye[1][0] = 0.032f * s;
  v.width = 2.53f * s;
  v.height = 2.6f * s;
  v.column[0] = 73.0f;
  v.column[1] = 1.0f;
  v.columns[0] = 2806.0f;
  v.columns[1] = 2806.0f;
  v.rows = 2880.0f;
  return v;
}

// The pixel of a room point in the picture of one eye, through EyeCamera and
// the projection of the overlay.
inline bool Pixel(const cutscene_hands_math::View& v, int eye,
                  const float p[3], float* x, float* y) {
  float m[12];
  if (!cutscene_hands_math::EyeCamera(v, eye, 1440.0f, 1440.0f, m)) {
    return false;
  }
  float c[3];
  overlay_math::ToCamera(m, p, c);
  if (!(c[2] > 0.0f)) return false;
  const float white[3] = {1.0f, 1.0f, 1.0f};
  overlay_math::ScreenVertex out;
  overlay_math::Project(c, white, 0.1f, 1440.0f, 1440.0f, &out);
  *x = out.x;
  *y = out.y;
  return true;
}

inline void SceneChecks() {
  using namespace cutscene_hands_math;
  printf("\nThe hands in the cut scene window\n");
  const float s = 7.5f;
  const View v = TestView();

  // A POINT ON THE SCREEN is at the pixel of its place on the opening, in
  // each eye.
  {
    const float u = 0.4f * s, w = -0.3f * s;
    const float p[3] = {u, w, 2.0f * s};
    for (int e = 0; e < 2; ++e) {
      float x = 0.0f, y = 0.0f, ex = 0.0f, ey = 0.0f;
      Check(Pixel(v, e, p, &x, &y), "a point on the screen has a pixel", 1,
            1);
      EyePixel(v, e, u, w, &ex, &ey);
      Near(e == 0 ? "the left eye sees a point of the screen at its column"
                  : "the right eye sees a point of the screen at its column",
           x, ex, 0.01);
      Near(e == 0 ? "the left eye sees a point of the screen at its row"
                  : "the right eye sees a point of the screen at its row",
           y, ey, 0.01);
    }
    float gx = 0.0f, gy = 0.0f, lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f;
    GamePixel(v, u, w, &gx, &gy);
    EyePixel(v, 0, u, w, &lx, &ly);
    EyePixel(v, 1, u, w, &rx, &ry);
    Near("the pixel of the game camera is the mean of the two eyes", gx,
         0.5 * (lx + rx), 1e-3);
    Near("the two eye pixels of a point of the screen are the crop apart",
         lx - rx, 72.0, 0.01);
    Near("a point 0.3 m under the middle is 0.3 / 2.6 of the rows under "
         "the middle row",
         gy, 0.5 * 2880.0 + 0.3 / 2.6 * 2880.0, 0.01);
  }

  // A POINT IN FRONT OF THE SCREEN: each eye sees it where the line from the
  // eye through it meets the screen, so the two eyes cross for it.
  {
    const float p[3] = {0.1f * s, -0.4f * s, 0.5f * s};
    for (int e = 0; e < 2; ++e) {
      const float* eye = v.eye[e];
      const float t = (2.0f * s - eye[2]) / (p[2] - eye[2]);
      const float hit_u = eye[0] + t * (p[0] - eye[0]);
      const float hit_w = eye[1] + t * (p[1] - eye[1]);
      float x = 0.0f, y = 0.0f, ex = 0.0f, ey = 0.0f;
      Pixel(v, e, p, &x, &y);
      EyePixel(v, e, hit_u, hit_w, &ex, &ey);
      Near(e == 0 ? "the left eye sees a hand in front along its line"
                  : "the right eye sees a hand in front along its line",
           x, ex, 0.05);
      Near("and at the row of that line", y, ey, 0.05);
    }
    float lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f;
    Pixel(v, 0, p, &lx, &ly);
    Pixel(v, 1, p, &rx, &ry);
    Check(lx - rx > 72.0f,
          "a hand in front of the screen is crossed more than the screen",
          lx - rx, 72.0);
    Near("both eyes see a hand in front at one row", ly, ry, 0.05);
  }

  // A HEAD THAT TURNS does not change the pixels: they depend on the eyes
  // and the screen only. An eye that moves to the right sees a near point
  // move to the left on the screen.
  {
    View moved = v;
    moved.eye[0][0] += 0.1f * s;
    const float p[3] = {0.0f, 0.0f, 0.5f * s};
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    Pixel(v, 0, p, &x0, &y0);
    Pixel(moved, 0, p, &x1, &y1);
    Check(x1 < x0, "an eye to the right sees a near point more to the left",
          x1, x0);
  }

  // THE RAY. A ray straight ahead from the middle of the eyes meets the
  // middle of the opening, a ray to the side misses it, and a ray back gives
  // no hit.
  {
    const float origin[3] = {0.0f, -0.3f * s, 0.4f * s};
    const float ahead[3] = {0.0f, 0.0f, 1.0f};
    float u = 0.0f, w = 0.0f, t = 0.0f;
    bool inside = false;
    Check(HitPlane(v, origin, ahead, &u, &w, &t, &inside) && inside,
          "a ray ahead meets the opening", 1, 1);
    Near("at the height of the hand", w, -0.3 * s, 1e-4);
    Near("1.6 m from the hand", t, 1.6 * s, 1e-3);
    const float side[3] = {0.8f, 0.0f, 0.6f};
    Check(HitPlane(v, origin, side, &u, &w, &t, &inside) && !inside,
          "a ray far to the side meets the plane past the opening", 1, 1);
    const float back[3] = {0.0f, 0.0f, -1.0f};
    Check(!HitPlane(v, origin, back, &u, &w, &t, &inside),
          "a ray back meets nothing", 1, 1);
  }

  // THE DEPTH of a hand is at the front, and keeps its order.
  {
    Check(FrontDepth(0.9f) < 0.03f, "a hand is at the front of the depth",
          FrontDepth(0.9f), 0.03);
    Check(FrontDepth(0.5f) < FrontDepth(0.6f),
          "a nearer part of a hand stays in front", FrontDepth(0.5f),
          FrontDepth(0.6f));
    Check(FrontDepth(0.0f) > 0.0f, "the front depth is above 0",
          FrontDepth(0.0f), 0.0);
  }

  using namespace influence_math;
  printf("\nThe influence at the hand\n");

  // THE FADE goes to 0.45 outside in 0.25 s, at 90 frames a second, and back.
  {
    float o = 1.0f;
    int frames = 0;
    while (o > 0.45f && frames < 1000) {
      o = FadeStep(o, false, 0.45f, 0.25f, 1.0f / 90.0f);
      ++frames;
    }
    Near("the fade out takes 0.25 s", frames / 90.0, 0.25, 0.012);
    Near("and ends at 0.45", o, 0.45, 1e-6);
    o = FadeStep(o, true, 0.45f, 0.25f, 1.0f / 90.0f);
    Check(o > 0.45f, "inside again the hand comes back", o, 0.45);
    Near("a long frame counts as 0.1 s",
         FadeStep(1.0f, false, 0.45f, 0.25f, 2.0f), 1.0 - 0.55 * 0.4, 1e-5);
  }

  // THE EDGE. The influence is a circle of 100 units around the origin.
  const auto circle = [](float x, float z) {
    return x * x + z * z <= 100.0f * 100.0f;
  };
  {
    bool found[kMaxDirections];
    float edge[kMaxDirections][2];
    Check(FindEdge(circle, 0.0f, 0.0f, 15.0f, 24, 6, 5, found, edge) == 0,
          "a hand in the middle finds no edge", 0, 0);
    const int n = FindEdge(circle, 90.0f, 0.0f, 15.0f, 24, 6, 5, found, edge);
    Check(n > 0, "a hand 10 units from the edge finds it", n, 1);
    Check(found[0], "the ray toward the edge finds it", found[0], 1);
    Near("at the edge of the circle", edge[0][0], 100.0, 0.2);
    const float at[3] = {90.0f, 0.0f, 0.0f};
    float point[2] = {0.0f, 0.0f}, d = 0.0f;
    Check(NearestEdge(at, found, edge, 24, point, &d),
          "the nearest edge point is found", 1, 1);
    Near("10 units from the hand", d, 10.0, 0.2);
    Near("straight toward the edge", point[1], 0.0, 0.2);
    Check(!found[12], "the ray away from the edge finds nothing", found[12],
          0);
    const int out = FindEdge(circle, 105.0f, 0.0f, 15.0f, 24, 6, 5, found,
                             edge);
    Check(out > 0 && found[12], "a hand outside finds the edge behind it",
          out, 1);
    Near("at the edge of the circle", edge[12][0], 100.0, 0.2);
  }

  // THE STRENGTH of the glow: none far inside, faint at the start of the
  // range, full at the touch, and a faint patch past the edge that goes.
  {
    const float range = 15.0f, touch = 2.25f, out = 22.5f, share = 0.35f;
    Near("no glow at the start of the range",
         GlowStrength(true, 15.0f, range, touch, out, share), 0.0, 1e-6);
    Near("no glow farther inside",
         GlowStrength(true, 40.0f, range, touch, out, share), 0.0, 1e-6);
    Check(GlowStrength(true, 11.25f, range, touch, out, share) < 0.2f,
          "a quarter into the range the glow is faint",
          GlowStrength(true, 11.25f, range, touch, out, share), 0.2);
    Near("full at the touch",
         GlowStrength(true, 0.0f, range, touch, out, share), 1.0, 1e-6);
    float last = -1.0f;
    bool rises = true;
    for (int i = 0; i <= 30; ++i) {
      const float g =
          GlowStrength(true, 15.0f - 0.5f * i, range, touch, out, share);
      if (g < last) rises = false;
      last = g;
    }
    Check(rises, "the glow rises while the hand comes near", rises, 1);
    Near("just past the edge it is full",
         GlowStrength(false, 0.0f, range, touch, out, share), 1.0, 1e-6);
    Near("past the touch it is the faint patch",
         GlowStrength(false, 2.25f, range, touch, out, share),
         0.35 * (1.0 - 2.25 / 22.5), 1e-5);
    Near("far past the edge it is gone",
         GlowStrength(false, 30.0f, range, touch, out, share), 0.0, 1e-6);
  }

  // THE PATCH lies in the vertical plane of the edge point, facing the hand,
  // with its light in the middle and none at its rim.
  {
    WallTriangle patch[kPatchTriangles];
    const float at[2] = {100.0f, 0.0f};
    const float hand[3] = {92.0f, 40.0f, 0.0f};
    const int n = BuildPatch(at, 40.0f, hand, 9.0f, 0.6f, patch,
                             kPatchTriangles);
    Check(n == kPatchTriangles, "a patch has its triangles", n,
          kPatchTriangles);
    bool flat = true, inside = true;
    float most = 0.0f, rim = 0.0f;
    for (int t = 0; t < n; ++t) {
      for (int k = 0; k < 3; ++k) {
        const float* q = patch[t].p[k];
        if (fabsf(q[0] - 100.0f) > 1e-4f) flat = false;
        const float r = sqrtf((q[1] - 40.0f) * (q[1] - 40.0f) + q[2] * q[2]);
        if (r > 9.0f + 1e-4f) inside = false;
        if (r > 8.99f && patch[t].light[k] > rim) rim = patch[t].light[k];
        if (patch[t].light[k] > most) most = patch[t].light[k];
      }
    }
    Check(flat, "the patch is in the plane of the edge, facing the hand", flat,
          1);
    Check(inside, "the patch is round, 9 units from its middle", inside, 1);
    Near("its middle has the light", most, 0.6, 1e-6);
    Near("its rim has none", rim, 0.0, 1e-6);
    Check(BuildPatch(at, 40.0f, hand, 9.0f, 0.0f, patch, kPatchTriangles) == 0,
          "no light gives no patch", 0, 0);
  }

  printf("\nThe hologram at the edge of the influence\n");

  // THE EDGE OF THE CIRCLES. One circle of 100 units, and the union of two
  // circles of 100 units with their middles 150 units apart: they cross at
  // x 75, z +-66.14.
  {
    const Circle one[1] = {{0.0f, 0.0f, 100.0f}};
    EdgePoint e = {-1, {0.0f, 0.0f}, 0.0f, false};
    Check(NearestUnionEdge(one, 1, 90.0f, 0.0f, 0.01f, &e) && e.inside,
          "a hand inside one circle finds its edge", e.inside, 1);
    Near("the edge point is straight out from the middle", e.at[0], 100.0,
         1e-4);
    Near("10 units from the hand", e.distance, 10.0, 1e-4);
    Check(NearestUnionEdge(one, 1, 105.0f, 0.0f, 0.01f, &e) && !e.inside,
          "a hand outside finds the edge behind it", e.inside, 0);
    Near("5 units from the hand", e.distance, 5.0, 1e-4);
    const Circle two[2] = {{0.0f, 0.0f, 100.0f}, {150.0f, 0.0f, 100.0f}};
    Check(NearestUnionEdge(two, 2, 0.0f, 90.0f, 0.01f, &e) && e.circle == 0,
          "a hand near the rim of the first circle finds that rim", e.circle,
          0);
    Near("10 units from the hand", e.distance, 10.0, 1e-3);
    Check(NearestUnionEdge(two, 2, 75.0f, 0.0f, 0.01f, &e) && e.inside,
          "a hand where the two circles overlap is inside", e.inside, 1);
    Near("its nearest edge is where the rims cross", e.distance,
         sqrtf(100.0f * 100.0f - 75.0f * 75.0f), 1e-2);
    // A hand at x 120 is in the second circle only. The points straight out
    // through it (x 50 and x 100) are in the other circle, and the rim past
    // it (x 250) is 130 units away, so the crossing is the nearest edge.
    Check(NearestUnionEdge(two, 2, 120.0f, 0.0f, 0.01f, &e) && e.inside,
          "a hand in the second circle is inside", e.inside, 1);
    Near("its nearest edge is where the rims cross", e.distance,
         sqrtf(45.0f * 45.0f + 66.1438f * 66.1438f), 1e-2);
    Check(e.circle == 0,
          "the wall there is the circle whose rim is nearer to the hand",
          e.circle, 0);
    Check(!NearestUnionEdge(two, 0, 0.0f, 0.0f, 0.01f, &e),
          "no circles give no edge", 0, 0);
  }

  // THE CAUSE OF THE SHAKE of the round patch, and the steady wall. A hand
  // 10 units inside a circle of 100 units moves along the edge in steps of
  // 0.05 degree. The search of 24 rays finds its edge point on a ray, so the
  // point jumps from ray to ray, and the patch, which faces the hand, turns
  // away from the true face of the wall. The edge point of the circle moves
  // with the hand in small steps, and the wall is the circle itself.
  {
    const auto disc = [](float x, float z) {
      return x * x + z * z <= 100.0f * 100.0f;
    };
    const Circle one[1] = {{0.0f, 0.0f, 100.0f}};
    float worst_jump_search = 0.0f, worst_turn_search = 0.0f;
    float worst_jump_circle = 0.0f, worst_turn_circle = 0.0f;
    float last_search[2] = {0.0f, 0.0f}, last_circle[2] = {0.0f, 0.0f};
    for (int i = 0; i <= 400; ++i) {
      const float a = 0.0008727f * static_cast<float>(i);  // 0.05 degree
      const float hand[3] = {90.0f * cosf(a), 30.0f, 90.0f * sinf(a)};
      bool found[kMaxDirections];
      float edge[kMaxDirections][2];
      float at[2] = {0.0f, 0.0f}, d = 0.0f;
      FindEdge(disc, hand[0], hand[2], 15.0f, 24, 6, 5, found, edge);
      NearestEdge(hand, found, edge, 24, at, &d);
      // The face of the patch: from the hand to its edge point.
      const float face = atan2f(at[1] - hand[2], at[0] - hand[0]);
      const float turn = fabsf(face - a) * 57.29578f;
      if (turn > worst_turn_search) worst_turn_search = turn;
      EdgePoint e;
      NearestUnionEdge(one, 1, hand[0], hand[2], 0.01f, &e);
      const float wall_face = atan2f(e.at[1], e.at[0]);
      const float turn_circle = fabsf(wall_face - a) * 57.29578f;
      if (turn_circle > worst_turn_circle) worst_turn_circle = turn_circle;
      if (i > 0) {
        const float js = sqrtf((at[0] - last_search[0]) *
                                   (at[0] - last_search[0]) +
                               (at[1] - last_search[1]) *
                                   (at[1] - last_search[1]));
        const float jc = sqrtf((e.at[0] - last_circle[0]) *
                                   (e.at[0] - last_circle[0]) +
                               (e.at[1] - last_circle[1]) *
                                   (e.at[1] - last_circle[1]));
        if (js > worst_jump_search) worst_jump_search = js;
        if (jc > worst_jump_circle) worst_jump_circle = jc;
      }
      last_search[0] = at[0];
      last_search[1] = at[1];
      last_circle[0] = e.at[0];
      last_circle[1] = e.at[1];
    }
    printf("  the search: the edge point jumps up to %.2f units for a step of "
           "%.3f units of the hand, and the patch turns up to %.1f degrees "
           "from the wall\n",
           worst_jump_search, 90.0f * 0.0008727f, worst_turn_search);
    Check(worst_jump_search > 1.0f,
          "the search makes the edge point jump (the shake of the patch)",
          worst_jump_search, 1.0);
    Check(worst_turn_search > 5.0f,
          "the patch that faces the hand turns away from the wall",
          worst_turn_search, 5.0);
    Check(worst_jump_circle < 0.1f,
          "the edge point of the circle moves in small steps with the hand",
          worst_jump_circle, 0.1);
    Check(worst_turn_circle < 0.01f,
          "the wall of the circle faces straight out from its middle",
          worst_turn_circle, 0.01);
  }

  // THE STRENGTH of the hologram: 45 cm inside and 40 cm past the edge, at
  // 100 units a meter.
  {
    Near("none 45 cm inside", HologramStrength(true, 45.0f, 45.0f, 40.0f), 0.0,
         1e-6);
    Near("0.42 at 25 cm inside", HologramStrength(true, 25.0f, 45.0f, 40.0f),
         0.4170, 1e-3);
    Near("0.87 at 10 cm inside", HologramStrength(true, 10.0f, 45.0f, 40.0f),
         0.8738, 1e-3);
    Near("0.96 at 5 cm past", HologramStrength(false, 5.0f, 45.0f, 40.0f),
         0.9570, 1e-3);
    Near("0.68 at 15 cm past", HologramStrength(false, 15.0f, 45.0f, 40.0f),
         0.6836, 1e-3);
    Near("full at the edge", HologramStrength(true, 0.0f, 45.0f, 40.0f), 1.0,
         1e-6);
    Near("full just past the edge",
         HologramStrength(false, 0.0f, 45.0f, 40.0f), 1.0, 1e-6);
    Near("none 40 cm past the edge",
         HologramStrength(false, 40.0f, 45.0f, 40.0f), 0.0, 1e-6);
    bool rises = true;
    float last = -1.0f;
    for (int i = 0; i <= 45; ++i) {
      const float g = HologramStrength(true, 45.0f - i, 45.0f, 40.0f);
      if (g < last) rises = false;
      last = g;
    }
    Check(rises, "it rises while the hand comes near", rises, 1);
  }

  // THE BUILD on the wall of a circle of 300 units, at 100 units a meter,
  // at full strength: a disc of 30 cm.
  {
    static WallTriangle holo[kHoloMaxTriangles];
    HoloWall w = {0.0f, 0.0f, 300.0f, 300.0f, 0.0f, 0.0f, 0.0f};
    HoloLook look;
    look.y = 50.0f;
    look.radius = 30.0f;
    look.light = 0.8f;
    look.grid = 7.5f;
    look.ring_gap = 8.0f;
    look.ring_phase = 0.3f;
    look.line_half = 0.4f;
    const auto all = [](float, float) { return true; };
    const int n = BuildHologram(w, look, all, holo, kHoloMaxTriangles);
    printf("  a hologram at full strength has %d triangles\n", n);
    Check(n > 100 && n < kHoloMaxTriangles,
          "it fits in its room with no cut", n, kHoloMaxTriangles);
    bool on_wall = true, in_disc = true, lit = true;
    float most = 0.0f;
    for (int t = 0; t < n; ++t) {
      bool any = false;
      for (int k = 0; k < 3; ++k) {
        const float* p = holo[t].p[k];
        if (fabsf(sqrtf(p[0] * p[0] + p[2] * p[2]) - 300.0f) > 0.01f) {
          on_wall = false;
        }
        const float dy = p[1] - 50.0f, dx = p[0] - 300.0f, dz = p[2];
        if (sqrtf(dx * dx + dy * dy + dz * dz) > 30.0f + 0.5f) in_disc = false;
        if (holo[t].light[k] > 0.0f) any = true;
        if (holo[t].light[k] > most) most = holo[t].light[k];
      }
      if (!any) lit = false;
    }
    Check(on_wall, "each point is on the wall of the circle", on_wall, 1);
    Check(in_disc, "each point is in the disc around the edge point", in_disc,
          1);
    Check(lit, "no triangle is dark", lit, 1);
    Check(most <= 0.8f + 1e-4f && most > 0.5f,
          "the brightest point is bright and not above the light of the look", most, 0.8);
    const int faint = BuildHologram(w, look, all, holo, 40);
    Check(faint == 40, "a small room stops the build at the room", faint, 40);
    look.light = 0.0f;
    Check(BuildHologram(w, look, all, holo, kHoloMaxTriangles) == 0,
          "no light gives no hologram", 0, 0);
  }

  // THE GRID IS FIXED TO THE WALL. Two edge points 2 units apart along the
  // circle (a hand that moves) put each upright line of the grid at the same
  // place of the world, also across the angle 0 of the circle.
  {
    float worst = 0.0f;
    const float starts[2] = {0.4f, -0.0035f};  // radians
    for (int q = 0; q < 2; ++q) {
      const float a1 = starts[q], a2 = starts[q] + 2.0f / 300.0f;
      HoloWall w1 = {0.0f, 0.0f, 300.0f, 300.0f * cosf(a1),
                     300.0f * sinf(a1), 0.0f, 0.0f};
      HoloWall w2 = {0.0f, 0.0f, 300.0f, 300.0f * cosf(a2),
                     300.0f * sinf(a2), 0.0f, 0.0f};
      float step1 = 0.0f, along1 = 0.0f, step2 = 0.0f, along2 = 0.0f;
      HoloGridStep(w1, 7.5f, &step1, &along1);
      HoloGridStep(w2, 7.5f, &step2, &along2);
      const float k = floorf(along1 / step1 + 0.5f);
      for (int j = -2; j <= 2; ++j) {
        const float line = (k + static_cast<float>(j)) * step1;
        float p1[3], p2[3];
        HoloPoint(w1, 50.0f, line - along1, 0.0f, p1);
        // The same line from the second point: across the angle 0 the count
        // of the steps goes round once.
        float line2 = line;
        const float round = 6.2831853f * 300.0f;
        if (along2 - along1 > round * 0.5f) line2 += round;
        if (along1 - along2 > round * 0.5f) line2 -= round;
        HoloPoint(w2, 50.0f, line2 - along2, 0.0f, p2);
        const float d = sqrtf((p1[0] - p2[0]) * (p1[0] - p2[0]) +
                              (p1[2] - p2[2]) * (p1[2] - p2[2]));
        if (d > worst) worst = d;
      }
      Near("the step of the grid goes a whole number of times round",
           fmodf(6.2831853f * 300.0f / step1 + 0.5f, 1.0f), 0.5, 1e-2);
    }
    Check(worst < 0.01f, "an upright line stays at its place in the world",
          worst, 0.01);
  }

  // THE UNION: on the wall of one circle, no light inside the other circle.
  {
    static WallTriangle holo[kHoloMaxTriangles];
    const Circle two[2] = {{0.0f, 0.0f, 100.0f}, {150.0f, 0.0f, 100.0f}};
    // The edge point of the first circle 10 units from where the rims cross.
    const float a = atan2f(66.1438f, 75.0f) + 10.0f / 100.0f;
    HoloWall w = {0.0f, 0.0f, 100.0f, 100.0f * cosf(a), 100.0f * sinf(a),
                  0.0f, 0.0f};
    HoloLook look;
    look.y = 50.0f;
    look.radius = 30.0f;
    look.light = 0.8f;
    look.grid = 7.5f;
    look.ring_gap = 8.0f;
    look.ring_phase = 0.3f;
    look.line_half = 0.4f;
    const auto show = [&two](float x, float z) {
      return !InOther(two, 2, 0, x, z, 0.2f);
    };
    const int n = BuildHologram(w, look, show, holo, kHoloMaxTriangles);
    bool dark_inside = true;
    for (int t = 0; t < n; ++t) {
      for (int k = 0; k < 3; ++k) {
        if (holo[t].light[k] > 0.0f &&
            InCircle(two[1], holo[t].p[k][0], holo[t].p[k][2], 0.2f)) {
          dark_inside = false;
        }
      }
    }
    Check(n > 0 && dark_inside,
          "the part of the wall inside the other circle has no light",
          dark_inside, 1);
  }
}

}  // namespace scene_checks
