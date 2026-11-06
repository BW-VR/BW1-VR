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
}

}  // namespace scene_checks
