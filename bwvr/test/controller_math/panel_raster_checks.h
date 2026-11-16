// THE RASTERIZER OF THE PANEL PICTURES (panel_raster.h): the view, the
// cover of a triangle and the top left rule, the depth test, the blend, the
// texture, the outline, and the picture. main.cpp includes this file after
// Check and Near, just before main.
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "../../src/mod/panel_raster.h"

namespace panel_raster_checks {

namespace pr = panel_raster;

struct Buffers {
  std::vector<float> color;
  std::vector<float> depth;
  std::vector<uint8_t> id;
  std::vector<uint8_t> scratch;
  pr::Target t;
  void Start(int w, int h, int ss) {
    color.assign(pr::ColorFloats(w, h, ss), 0.0f);
    depth.assign(pr::Samples(w, h, ss), 0.0f);
    id.assign(pr::Samples(w, h, ss), 0);
    scratch.assign(pr::Samples(w, h, ss), 0);
    const float clear[4] = {0, 0, 0, 0};
    pr::Start(&t, w, h, ss, color.data(), depth.data(), id.data(), clear);
  }
};

inline pr::Corner At(float x, float y, float depth, float r, float g,
                     float b, float a) {
  pr::Corner c;
  c.x = x;
  c.y = y;
  c.depth = depth;
  c.color[0] = r * a;
  c.color[1] = g * a;
  c.color[2] = b * a;
  c.color[3] = a;
  c.u = 0.0f;
  c.v = 0.0f;
  return c;
}

inline void PanelRasterChecks() {
  printf("\nThe rasterizer of the panel pictures\n");

  // THE VIEW. An eye 2 units in front of the origin (on +z) that looks at
  // it, with y up: the origin is at the middle, +x goes right, +y goes up
  // (fewer samples), and the depth is the distance.
  pr::View v;
  const float eye[3] = {0, 0, 2}, at[3] = {0, 0, 0}, up[3] = {0, 1, 0};
  const bool made = pr::LookAt(eye, at, up, 100.0f, 0.0f, 200, 100, &v);
  const float unit_x[3] = {1, 0, 0}, unit_y[3] = {0, 1, 0};
  const float back_point[3] = {0, 0, 3}, far_point[3] = {1, 1, -5};
  const float along[3] = {0, 0, 1};
  float o[3], px[3], py[3];
  const bool all = made && pr::Project(v, at, o) &&
                   pr::Project(v, unit_x, px) && pr::Project(v, unit_y, py);
  Check(all && fabsf(o[0] - 100) < 1e-3f && fabsf(o[1] - 50) < 1e-3f &&
            fabsf(o[2] - 2) < 1e-4f,
        "a perspective view puts the point it looks at in the middle of the "
        "picture, at its distance", o[0], 100);
  Check(all && fabsf(px[0] - 150) < 1e-3f && fabsf(py[1] - 0) < 1e-3f,
        "a unit to the right at a depth of 2 with a focal of 100 samples is "
        "50 samples to the right, and a unit up is 50 samples up",
        px[0], 150);
  float behind[3];
  Check(!pr::Project(v, back_point, behind),
        "a point behind the eye does not project", 0, 0);
  pr::View ortho;
  pr::LookAt(eye, at, up, 0.0f, 10.0f, 200, 100, &ortho);
  float q[3];
  pr::Project(ortho, far_point, q);
  Check(fabsf(q[0] - 110) < 1e-3f && fabsf(q[1] - 40) < 1e-3f &&
            fabsf(q[2] - 7) < 1e-4f,
        "an orthographic view has the same scale at each depth", q[0], 110);
  pr::View bad;
  Check(!pr::LookAt(eye, at, along, 100.0f, 0.0f, 10, 10,
                    &bad),
        "a view with up along the line of sight is refused", 0, 0);

  // THE COVER. A square of 40 by 40 samples as two triangles that share a
  // diagonal: each sample in it is drawn one time, by the top left rule.
  Buffers b;
  b.Start(64, 64, 1);
  pr::State s = pr::Opaque(1);
  s.blend = pr::kOver;
  s.depth_test = false;
  s.cull_back = false;
  const float a = 0.5f;
  const pr::Corner t1[3] = {At(10, 10, 1, 1, 0, 0, a), At(10, 50, 1, 1, 0, 0, a),
                            At(50, 50, 1, 1, 0, 0, a)};
  const pr::Corner t2[3] = {At(10, 10, 1, 1, 0, 0, a), At(50, 50, 1, 1, 0, 0, a),
                            At(50, 10, 1, 1, 0, 0, a)};
  const int n1 = pr::Triangle(&b.t, t1, s);
  const int n2 = pr::Triangle(&b.t, t2, s);
  int once = 0, twice = 0, outside = 0;
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      const float alpha = b.color[(static_cast<size_t>(y) * 64 + x) * 4 + 3];
      const bool in = x >= 10 && x < 50 && y >= 10 && y < 50;
      if (in && fabsf(alpha - a) < 1e-4f) ++once;
      if (alpha > a + 1e-3f) ++twice;
      if (!in && alpha > 0.0f) ++outside;
    }
  }
  Check(n1 + n2 == 1600 && once == 1600 && twice == 0 && outside == 0,
        "two triangles of a square of 40 by 40 samples draw each of its 1600 "
        "samples one time, and no sample past it (the top left rule)",
        n1 + n2, 1600);

  // THE FRONT. A mesh triangle that goes counterclockwise as the eye sees
  // it is a front face. With the cull of back faces, the other order draws
  // nothing.
  b.Start(64, 64, 1);
  pr::State cull = pr::Opaque(1);
  const pr::Corner ccw[3] = {At(10, 10, 1, 1, 1, 1, 1), At(10, 50, 1, 1, 1, 1, 1),
                             At(50, 50, 1, 1, 1, 1, 1)};
  const pr::Corner cw[3] = {ccw[0], ccw[2], ccw[1]};
  const int front = pr::Triangle(&b.t, ccw, cull);
  const int back = pr::Triangle(&b.t, cw, cull);
  float mesh_front[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  float sp[3][3];
  for (int k = 0; k < 3; ++k) pr::Project(v, mesh_front[k], sp[k]);
  const float area = pr::Area2(sp[0][0], sp[0][1], sp[1][0], sp[1][1],
                               sp[2][0], sp[2][1]);
  Check(front > 0 && back == 0 && area < 0.0f,
        "a triangle counterclockwise to the eye is drawn and the other order "
        "is culled, and a mesh triangle that faces the eye has a negative "
        "area on the picture", front, 800);

  // THE DEPTH. A far red square and a near green square, in both orders:
  // the near one shows, and keeps its part.
  bool depth_ok = true;
  for (int order = 0; order < 2; ++order) {
    b.Start(16, 16, 1);
    pr::State opaque = pr::Opaque(1);
    opaque.cull_back = false;
    const pr::Corner far1[3] = {At(0, 0, 5, 1, 0, 0, 1), At(0, 16, 5, 1, 0, 0, 1),
                                At(16, 16, 5, 1, 0, 0, 1)};
    const pr::Corner far2[3] = {At(0, 0, 5, 1, 0, 0, 1), At(16, 16, 5, 1, 0, 0, 1),
                                At(16, 0, 5, 1, 0, 0, 1)};
    const pr::Corner near1[3] = {At(0, 0, 2, 0, 1, 0, 1),
                                 At(0, 16, 2, 0, 1, 0, 1),
                                 At(16, 16, 2, 0, 1, 0, 1)};
    const pr::Corner near2[3] = {At(0, 0, 2, 0, 1, 0, 1),
                                 At(16, 16, 2, 0, 1, 0, 1),
                                 At(16, 0, 2, 0, 1, 0, 1)};
    pr::State near_state = opaque;
    near_state.id = 2;
    if (order == 0) {
      pr::Triangle(&b.t, far1, opaque);
      pr::Triangle(&b.t, far2, opaque);
      pr::Triangle(&b.t, near1, near_state);
      pr::Triangle(&b.t, near2, near_state);
    } else {
      pr::Triangle(&b.t, near1, near_state);
      pr::Triangle(&b.t, near2, near_state);
      pr::Triangle(&b.t, far1, opaque);
      pr::Triangle(&b.t, far2, opaque);
    }
    const float* c = &b.color[(8 * 16 + 8) * 4];
    if (!(c[1] > 0.99f && c[0] < 0.01f && b.id[8 * 16 + 8] == 2 &&
          fabsf(b.depth[8 * 16 + 8] - 2.0f) < 1e-4f)) {
      depth_ok = false;
    }
  }
  Check(depth_ok, "the nearer square shows in either order of the draws, "
        "with its part and its depth", depth_ok, 1);

  // THE PERSPECTIVE. A line from depth 1 to depth 3 across 40 samples: its
  // middle on the picture is at depth 1.5 (1 / depth goes in a line), and a
  // color from 0 to 1 is 0.25 there.
  b.Start(48, 8, 1);
  pr::State plain = pr::Opaque(1);
  plain.cull_back = false;
  const pr::Corner slope[3] = {At(0, 0, 1, 0, 0, 0, 1), At(0, 8, 1, 0, 0, 0, 1),
                               At(40, 4, 3, 1, 0, 0, 1)};
  pr::Triangle(&b.t, slope, plain);
  // The sample at x 20 has its middle at 20.5.
  const float d = b.depth[4 * 48 + 20];
  const float want_inv = 1.0f + (1.0f / 3.0f - 1.0f) * (20.5f / 40.0f);
  const float red = b.color[(4 * 48 + 20) * 4];
  const float want_red = (20.5f / 40.0f) / 3.0f / want_inv;
  Check(fabsf(d - 1.0f / want_inv) < 1e-3f && fabsf(red - want_red) < 1e-3f,
        "the depth and the color go across a triangle with the right "
        "perspective", d, 1.0f / want_inv);

  // THE PICTURE. A triangle over half of a pixel of 4 by 4 samples: the
  // pixel is half covered. An opaque white pixel is 255 in each byte.
  b.Start(2, 1, 4);
  pr::State white = pr::Opaque(1);
  white.cull_back = false;
  const pr::Corner half1[3] = {At(0, 0, 1, 1, 1, 1, 1), At(0, 4, 1, 1, 1, 1, 1),
                               At(2, 4, 1, 1, 1, 1, 1)};
  const pr::Corner half2[3] = {At(0, 0, 1, 1, 1, 1, 1), At(2, 4, 1, 1, 1, 1, 1),
                               At(2, 0, 1, 1, 1, 1, 1)};
  const pr::Corner full1[3] = {At(4, 0, 1, 1, 1, 1, 1), At(4, 4, 1, 1, 1, 1, 1),
                               At(8, 4, 1, 1, 1, 1, 1)};
  const pr::Corner full2[3] = {At(4, 0, 1, 1, 1, 1, 1), At(8, 4, 1, 1, 1, 1, 1),
                               At(8, 0, 1, 1, 1, 1, 1)};
  pr::Triangle(&b.t, half1, white);
  pr::Triangle(&b.t, half2, white);
  pr::Triangle(&b.t, full1, white);
  pr::Triangle(&b.t, full2, white);
  uint8_t pixels[8];
  pr::Resolve(b.t, pixels, 2);
  uint8_t part[2], share[2];
  pr::ResolveParts(b.t, part, share);
  Check(abs(pixels[3] - 128) <= 1 && abs(pixels[0] - 128) <= 1 &&
            pixels[4] == 255 && pixels[7] == 255,
        "a pixel half covered by its samples is half white and half clear, "
        "and a pixel fully covered is opaque white", pixels[3], 128);
  Check(part[0] == 1 && abs(share[0] - 127) <= 1 && part[1] == 1 &&
            share[1] == 255,
        "the part of a pixel and its share of the samples", share[0], 127);

  // THE TEXTURE. A texture of one color multiplies the color of the
  // corners.
  b.Start(8, 8, 1);
  const uint8_t texel[4] = {64, 128, 255, 255};  // B, G, R, A
  pr::Texture tex = {texel, 1, 1, 1};
  pr::State textured = pr::Opaque(1);
  textured.cull_back = false;
  textured.texture = &tex;
  const pr::Corner tt[3] = {At(0, 0, 1, 1, 1, 1, 1), At(0, 8, 1, 1, 1, 1, 1),
                            At(8, 8, 1, 1, 1, 1, 1)};
  pr::Triangle(&b.t, tt, textured);
  const float* tc = &b.color[(6 * 8 + 2) * 4];
  Check(fabsf(tc[0] - 1.0f) < 1e-3f && fabsf(tc[1] - 128 / 255.0f) < 1e-3f &&
            fabsf(tc[2] - 64 / 255.0f) < 1e-3f,
        "a texture of one color gives that color", tc[1], 128 / 255.0f);

  // THE OUTLINE. A square in front of the empty paper gets a ring of ink at
  // its edge, and its middle stays clear. A square in front of a far square
  // of another part gets ink on the far square at the edge.
  b.Start(40, 40, 1);
  pr::State sq = pr::Opaque(3);
  sq.cull_back = false;
  const pr::Corner s1[3] = {At(10, 10, 1, 1, 1, 1, 1), At(10, 30, 1, 1, 1, 1, 1),
                            At(30, 30, 1, 1, 1, 1, 1)};
  const pr::Corner s2[3] = {At(10, 10, 1, 1, 1, 1, 1), At(30, 30, 1, 1, 1, 1, 1),
                            At(30, 10, 1, 1, 1, 1, 1)};
  pr::Triangle(&b.t, s1, sq);
  pr::Triangle(&b.t, s2, sq);
  const float ink[4] = {0, 0, 0, 1};
  const int inked = pr::Outline(&b.t, ink, 2, 0.5f, b.scratch.data());
  const auto alpha_at = [&](int x, int y) {
    return b.color[(static_cast<size_t>(y) * 40 + x) * 4 + 3];
  };
  const auto red_at = [&](int x, int y) {
    return b.color[(static_cast<size_t>(y) * 40 + x) * 4];
  };
  Check(inked > 0 && alpha_at(9, 20) > 0.99f && red_at(9, 20) < 0.01f &&
            alpha_at(8, 20) > 0.99f && alpha_at(6, 20) == 0.0f &&
            red_at(20, 20) > 0.99f && red_at(10, 20) > 0.99f,
        "the outline is a ring of ink of 2 samples on the paper round the "
        "square, and the square stays white", inked, 160);
  b.Start(40, 40, 1);
  pr::State back_state = pr::Opaque(1);
  back_state.cull_back = false;
  const pr::Corner b1[3] = {At(0, 0, 5, 1, 1, 1, 1), At(0, 40, 5, 1, 1, 1, 1),
                            At(40, 40, 5, 1, 1, 1, 1)};
  const pr::Corner b2[3] = {At(0, 0, 5, 1, 1, 1, 1), At(40, 40, 5, 1, 1, 1, 1),
                            At(40, 0, 5, 1, 1, 1, 1)};
  pr::Triangle(&b.t, b1, back_state);
  pr::Triangle(&b.t, b2, back_state);
  pr::Triangle(&b.t, s1, sq);
  pr::Triangle(&b.t, s2, sq);
  pr::Outline(&b.t, ink, 2, 10.0f, b.scratch.data());
  Check(red_at(9, 20) < 0.01f && red_at(5, 20) > 0.99f &&
            red_at(20, 20) > 0.99f && red_at(2, 2) > 0.99f,
        "a part in front of another part gets ink on the far part at its "
        "edge, and the far part keeps its color away from the edge",
        red_at(9, 20), 0);
}

}  // namespace panel_raster_checks
