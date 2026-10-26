// THE VERTEX LOOP OF THE LAND WITH SSE (docs/backend.md, "The vertex loop of
// the land with SSE"). main.cpp includes this file after Check and Near, just
// before main.
//
// The reference makes one vertex at a time with scalar floats, in the order
// and with the comparisons that land_fast_math.h describes. Each check
// compares the bits of Grid with it, and two vertices with numbers made by
// hand. The desk run with land_fast=2 compares Grid with the loop of the game.
#pragma once

#include <stdint.h>
#include <string.h>

#include "../../src/mod/land_fast_math.h"

namespace land_fast_checks {

using land_fast_math::Consts;
using land_fast_math::Patch;

inline uint32_t Bits(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}

inline float Float(uint32_t u) {
  float f;
  memcpy(&f, &u, 4);
  return f;
}

// One vertex of the reference. r, c, u and v are the sums of the steps.
inline void RefVertex(const Consts& k, const Patch& p, const uint8_t* cell,
                      float r, float c, float u, float v, uint32_t out[8],
                      uint32_t* flag) {
  uint32_t word;
  memcpy(&word, cell, 4);
  const int32_t height = cell[4];
  uint32_t specular = word | 0xFF000000u;
  uint32_t diffuse = k.colors[word >> 24];
  float h = static_cast<float>(height) * k.height_scale;
  if (height <= 3) {
    h = k.zero;
    if (p.clear_low != 0 && height <= 1) specular &= 0x00FFFFFFu;
  }
  const float X = ((k.m[3] * h + k.m[6] * c) + k.m[0] * r) + k.m[9];
  const float Y = ((k.m[4] * h + k.m[7] * c) + k.m[1] * r) + k.m[10];
  const float W = ((k.m[5] * h + k.m[8] * c) + k.m[2] * r) + k.m[11];
  out[0] = Bits(X);
  out[1] = Bits(Y);
  out[3] = Bits(W);
  if (p.light != 0) {
    uint32_t fog, dim;
    if (p.light == 2) {
      fog = k.fog_color;
      dim = static_cast<uint32_t>(k.fog_dim);
    } else {
      float w = W;
      if (!(w >= k.fog_near)) {
        w = k.fog_near;
      } else if (w > k.fog_far) {
        w = k.fog_far;
      }
      const float t = (w - k.fog_near) / k.fog_range;
      const float part = static_cast<float>(256 - k.fog_dim) * t;
      dim = 0x100u - static_cast<uint32_t>(_mm_cvttss_si32(_mm_set_ss(part)));
      const int32_t red = _mm_cvtss_si32(_mm_set_ss(k.fog_rgb[0] * t));
      const int32_t green = _mm_cvtss_si32(_mm_set_ss(k.fog_rgb[1] * t));
      const int32_t blue = _mm_cvtss_si32(_mm_set_ss(k.fog_rgb[2] * t));
      fog = static_cast<uint32_t>(((red << 8 | green) << 8) | blue);
    }
    specular = land_fast_math::detail::AddFog(specular, fog);
    if (dim < 0x100u) diffuse = land_fast_math::detail::Dim(diffuse, dim);
  }
  bool project = true;
  if (p.clip != 0) {
    uint32_t f = !(W >= k.near_plane) ? 0x20u : 0u;
    if (X > W) {
      f |= 0x10u;
    } else if (-W > X) {
      f |= 8u;
    }
    if (Y > W) {
      f |= 4u;
    } else if (-W > Y) {
      f |= 2u;
    }
    *flag = f;
    project = f == 0;
  }
  if (project) {
    const float rhw = k.one / W;
    const float sx = (rhw * X + k.one) * k.half_x;
    out[0] = !(sx >= k.zero) ? 0u : Bits(sx > k.max_x ? k.max_x : sx);
    const float sy = k.half_y - (rhw * Y) * k.half_y;
    out[1] = !(sy >= k.zero) ? 0u : Bits(sy > k.max_y ? k.max_y : sy);
    const float q = k.near_plane * rhw;
    out[3] = Bits(q);
    out[2] = Bits(k.one - q);
  }
  out[4] = diffuse;
  out[5] = specular;
  out[6] = Bits(u);
  out[7] = Bits(v);
}

// The whole grid of the reference, into `out` that holds the old values.
inline uint32_t RefGrid(const Consts& k, const Patch& p, uint8_t* out,
                        uint32_t* flags) {
  const int32_t side = land_fast_math::GridSide(p.step);
  const uint8_t* cells = p.cells;
  float r = p.r0, v = p.v0;
  uint32_t n = 0;
  for (int32_t row = 0; row < side; ++row) {
    float c = p.c0, u = p.u0;
    for (int32_t col = 0; col < side; ++col) {
      uint32_t o[8];
      memcpy(o, out + n * 32, 32);
      RefVertex(k, p, cells, r, c, u, v, o, &flags[n]);
      memcpy(out + n * 32, o, 32);
      ++n;
      cells += p.step * 8;
      c = c + p.d;
      u = u + p.du;
    }
    cells += p.row_skip * 8;
    r = r + p.d;
    v = p.du + v;
  }
  return n;
}

// A simple generator, the same numbers on each run.
struct Numbers {
  uint32_t s;
  uint32_t Next() {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
  }
  float Range(float lo, float hi) {
    return lo + (hi - lo) * static_cast<float>(Next() & 0xFFFF) / 65535.0f;
  }
};

inline void MakeConsts(Consts* k, uint32_t* colors, Numbers* g) {
  for (int i = 0; i < 12; ++i) k->m[i] = g->Range(-1.5f, 1.5f);
  k->m[11] = g->Range(50.0f, 400.0f);
  k->one = 1.0f;
  k->zero = 0.0f;
  k->height_scale = 0.75f;
  k->near_plane = 4.0f;
  k->half_x = 640.0f;
  k->half_y = 360.0f;
  k->max_x = 1279.0f;
  k->max_y = 719.0f;
  k->fog_near = 60.0f;
  k->fog_far = 300.0f;
  k->fog_range = 240.0f;
  k->fog_rgb[0] = 120.0f;
  k->fog_rgb[1] = 140.0f;
  k->fog_rgb[2] = 200.0f;
  k->fog_dim = 96;
  k->fog_color = 0x00304050u;
  for (int i = 0; i < 256; ++i) colors[i] = (g->Next() << 8) ^ g->Next();
  k->colors = colors;
}

inline void LandFastChecks() {
  printf("\nThe vertex loop of the land with SSE\n");
  static uint8_t cells[17 * 17 * 8];
  static uint8_t got[17 * 17 * 32], want[17 * 17 * 32];
  static uint32_t got_flags[17 * 17], want_flags[17 * 17];
  uint32_t colors[256];
  Consts k;
  Patch p;

  // Two vertices with numbers made by hand. The matrix gives X = r, Y = h,
  // W = c.
  {
    memset(&k, 0, sizeof(k));
    k.m[0] = 1.0f;
    k.m[4] = 1.0f;
    k.m[8] = 1.0f;
    k.one = 1.0f;
    k.height_scale = 0.5f;
    k.near_plane = 0.5f;
    k.half_x = 320.0f;
    k.half_y = 240.0f;
    k.max_x = 639.0f;
    k.max_y = 479.0f;
    for (int i = 0; i < 256; ++i) colors[i] = 0x11000000u * (i & 1) + i;
    k.colors = colors;
    memset(cells, 0, sizeof(cells));
    // The first cell: height 10, color index 3. The second: height 2 (the
    // sea), color index 5.
    const uint8_t a[8] = {0x10, 0x20, 0x30, 0x03, 10, 0, 0, 0};
    const uint8_t b[8] = {0x01, 0x02, 0x03, 0x05, 2, 0, 0, 0};
    memcpy(cells, a, 8);
    memcpy(cells + 16 * 8, b, 8);
    memset(&p, 0, sizeof(p));
    p.cells = cells;
    p.step = 16;
    p.row_skip = 16 * 16 - 16;
    p.r0 = 1.0f;
    p.c0 = 2.0f;
    p.d = 16.0f;
    p.du = 0.5f;
    p.u0 = 0.25f;
    p.v0 = 0.125f;
    memset(got, 0, sizeof(got));
    const uint8_t* end = nullptr;
    const uint32_t n = land_fast_math::Grid(k, p, got, got_flags, &end);
    Check(n == 4, "a grid with the step 16 has 2 by 2 vertices", n, 4);
    Check(end == cells + 2 * (2 * 16 * 8 + (16 * 16 - 16) * 8),
          "the pointer of the cells is after the two rows", 0, 0);
    uint32_t o[8];
    memcpy(o, got, 32);
    // X = 1, Y = 5, W = 2: rhw 0.5, x (0.5 + 1) 320 = 480, y 240 - 2.5 240 is
    // below zero so 0, rhw 0.25, z 0.75.
    Check(o[0] == Bits(480.0f), "vertex 1: x is (rhw X + 1) half_x", Float(o[0]), 480);
    Check(o[1] == 0, "vertex 1: y below zero is 0", Float(o[1]), 0);
    Check(o[2] == Bits(0.75f), "vertex 1: z is 1 less near rhw", Float(o[2]), 0.75);
    Check(o[3] == Bits(0.25f), "vertex 1: rhw is near / W", Float(o[3]), 0.25);
    Check(o[4] == colors[3], "vertex 1: the diffuse color from the table", o[4], colors[3]);
    Check(o[5] == 0xFF302010u, "vertex 1: the specular color with alpha 255", o[5], 0xFF302010u);
    Check(o[6] == Bits(0.25f) && o[7] == Bits(0.125f), "vertex 1: u and v", Float(o[6]), 0.25);
    memcpy(o, got + 32, 32);
    // The second vertex: the sea (h 0), c = 2 + 16 = 18. X = 1, Y = 0, W =
    // 18: x (1/18 + 1) 320, y 240.
    const float rhw = 1.0f / 18.0f;
    Check(o[0] == Bits((rhw * 1.0f + 1.0f) * 320.0f), "vertex 2: x", Float(o[0]),
          (rhw + 1.0f) * 320.0f);
    Check(o[1] == Bits(240.0f), "vertex 2: the sea has the height zero, y 240", Float(o[1]), 240);
    Check(o[6] == Bits(0.75f), "vertex 2: u is u0 plus one step", Float(o[6]), 0.75);
    Check(o[5] == 0xFF030201u, "vertex 2: the specular color", o[5], 0xFF030201u);
    // With clear_low a height of 2 keeps the alpha, a height of 1 clears it.
    p.clear_low = 1;
    cells[16 * 8 + 4] = 1;
    land_fast_math::Grid(k, p, got, got_flags, nullptr);
    memcpy(o, got + 32, 32);
    Check(o[5] == 0x00030201u, "clear_low: a height of 1 clears the alpha", o[5], 0x00030201u);
    // With clip the first vertex has Y 5 above W 2, so the flag 4, and it
    // keeps X, Y, W and its old z.
    p.clip = 1;
    memset(got, 0xAB, sizeof(got));
    land_fast_math::Grid(k, p, got, got_flags, nullptr);
    memcpy(o, got, 32);
    Check(got_flags[0] == 4u, "clip: Y above W gives the flag 4", got_flags[0], 4);
    Check(o[0] == Bits(1.0f) && o[1] == Bits(5.0f) && o[3] == Bits(2.0f),
          "clip: a vertex with a flag keeps X, Y and W", Float(o[1]), 5);
    Check(o[2] == 0xABABABABu, "clip: a vertex with a flag keeps its old z", o[2], 0xABABABABu);
    Check(got_flags[1] == 0u, "clip: vertex 2 has no flag", got_flags[1], 0);
  }

  // The bits of Grid and of the reference, for many patches.
  Numbers g = {12345u};
  const int32_t steps[5] = {1, 2, 4, 8, 16};
  int patches = 0, same = 0, flags_same = 0;
  uint32_t vertices = 0;
  for (int round = 0; round < 60; ++round) {
    MakeConsts(&k, colors, &g);
    if (round % 7 == 3) k.fog_far = k.fog_near;  // no range of the fog
    for (size_t i = 0; i < sizeof(cells); ++i) cells[i] = static_cast<uint8_t>(g.Next());
    // Some cells are the sea, some have a height of 0 or 1.
    for (int i = 0; i < 17 * 17; ++i) {
      if (g.Next() % 5 == 0) cells[i * 8 + 4] = static_cast<uint8_t>(g.Next() % 4);
    }
    // Some colors are 0, so the specular color with clear_low is 0.
    for (int i = 0; i < 17 * 17; ++i) {
      if (g.Next() % 9 == 0) memset(cells + i * 8, 0, 3);
    }
    memset(&p, 0, sizeof(p));
    p.cells = cells;
    p.step = steps[round % 5];
    p.row_skip = p.step * 16 - 16;
    p.r0 = g.Range(-200.0f, 200.0f);
    p.c0 = g.Range(-200.0f, 200.0f);
    p.d = static_cast<float>(p.step) * g.Range(5.0f, 15.0f);
    p.du = static_cast<float>(p.step) * 0.0625f;
    p.u0 = g.Range(0.0f, 1.0f);
    p.v0 = g.Range(0.0f, 1.0f);
    p.clip = (round / 5) % 2;
    p.clear_low = (round / 10) % 2;
    p.light = (round / 20) % 3;
    for (size_t i = 0; i < sizeof(got); ++i) got[i] = want[i] = static_cast<uint8_t>(g.Next());
    memset(got_flags, 0x5A, sizeof(got_flags));
    memset(want_flags, 0x5A, sizeof(want_flags));
    const uint32_t n = land_fast_math::Grid(k, p, got, got_flags, nullptr);
    const uint32_t m = RefGrid(k, p, want, want_flags);
    ++patches;
    vertices += n;
    if (n == m && memcmp(got, want, sizeof(got)) == 0) ++same;
    if (n == m && memcmp(got_flags, want_flags, sizeof(got_flags)) == 0) ++flags_same;
  }
  Check(same == patches, "60 patches (steps 1 to 16, clip, clear_low, the three lights): "
        "each vertex of Grid has the bits of the reference", same, patches);
  Check(flags_same == patches, "60 patches: the clip flags of Grid are those of the "
        "reference", flags_same, patches);
  Check(vertices > 3000, "the patches had more than 3000 vertices", vertices, 3000);
  Check(land_fast_math::GridSide(1) == 17 && land_fast_math::GridSide(2) == 9 &&
            land_fast_math::GridSide(4) == 5 && land_fast_math::GridSide(16) == 2,
        "a row has 17, 9, 5 or 2 vertices for the steps 1, 2, 4 and 16", 0, 0);
  Check(land_fast_math::FogExact(96) && !land_fast_math::FogExact(-(1 << 25)),
        "the fog gives the bits of the game for a fog_dim below 2^24", 0, 0);
  Check(land_fast_math::detail::Dim(0x80FF8040u, 128) == 0x807F4020u,
        "a dim of 128 halves each byte of the diffuse color and keeps alpha",
        land_fast_math::detail::Dim(0x80FF8040u, 128), 0x807F4020u);
  Check(land_fast_math::detail::AddFog(0xFFF01008u, 0x00203040u) == 0xFFFF4048u,
        "the fog adds to each byte of the specular color with a limit of 255",
        land_fast_math::detail::AddFog(0xFFF01008u, 0x00203040u), 0xFFFF4048u);
  Check(land_fast_math::detail::AddFog(0u, 0x00203040u) == 0x00203040u,
        "a specular color of 0 gets the fog color", 0, 0);
}

}  // namespace land_fast_checks
