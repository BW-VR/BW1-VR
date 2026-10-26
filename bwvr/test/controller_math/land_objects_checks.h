// THE OBJECTS ON THE LAND WITH SSE (docs/backend.md, "The objects on the
// land"). main.cpp includes this file after Check and Near, just before main.
//
// The reference makes one vertex at a time with scalar floats, in the order
// and with the comparisons that land_objects_math.h describes. Each check
// compares the bits of EnvMesh with it, for whole meshes and for meshes in
// parts, and checks Split. The desk run with land_objects=2 compares EnvMesh
// with the loop of the game.
#pragma once

#include <emmintrin.h>
#include <stdint.h>
#include <string.h>

#include "../../src/mod/land_objects_math.h"

namespace land_objects_checks {

using land_objects_math::EnvConsts;

inline float F(uint32_t u) {
  float f;
  memcpy(&f, &u, 4);
  return f;
}

// One vertex of the reference: `in` 32 bytes, `out` 40 bytes, `flag` the
// clip flags.
inline void RefVertex(const EnvConsts& c, const float* in, uint8_t* out, uint32_t* flag) {
  const float* m = c.base.m;
  const float x = in[0], y = in[1], z = in[2];
  const float nx = in[5], ny = in[6], nz = in[7];
  const float X = ((m[6] * z + m[3] * y) + m[0] * x) + m[9];
  const float Y = ((m[7] * z + m[1] * x) + m[4] * y) + m[10];
  const float Z = ((m[8] * z + m[2] * x) + m[5] * y) + m[11];
  const float* l = c.light.direction;
  const float dot = ((l[2] * nz + l[1] * ny) + l[0] * nx) * c.k255;
  const int32_t level = _mm_cvtss_si32(_mm_set_ss(dot));
  const uint32_t color = fast_transform_math::LitColor(level, c.light);
  const float eu = ((((c.a[2] * nz + c.a[1] * ny) + c.a[0] * nx) * c.ku) + 1.0f) * c.half;
  const float ev = ((((c.b[2] * nz + c.b[1] * ny) + c.b[0] * nx) * c.kv) + 1.0f) * c.half;
  const float u2 = eu * c.su + c.ou;
  const float v2 = ev * c.sv + c.ov;
  uint32_t f = !(Z >= c.base.near_plane) ? 0x20u : 0u;
  if (X > Z) {
    f |= 0x10;
  } else if (-Z > X) {
    f |= 0x08;
  }
  if (Y > Z) {
    f |= 0x04;
  } else if (-Z > Y) {
    f |= 0x02;
  }
  *flag = f;
  float* o = reinterpret_cast<float*>(out);
  if (f == 0) {
    const float r = 1.0f / Z;
    float sx = (r * X + 1.0f) * c.base.half_x;
    if (!(sx >= 0.0f)) {
      sx = 0.0f;
    } else if (sx > c.base.max_x) {
      sx = c.base.max_x;
    }
    float sy = c.base.half_y - (Y * r) * c.base.half_y;
    if (!(sy >= 0.0f)) {
      sy = 0.0f;
    } else if (sy > c.base.max_y) {
      sy = c.base.max_y;
    }
    const float w = c.base.near_plane * r;
    o[0] = sx;
    o[1] = sy;
    o[2] = 1.0f - w;
    o[3] = w;
  } else {
    o[0] = X;
    o[1] = Y;
    o[3] = Z;
  }
  memcpy(out + 16, &color, 4);
  memcpy(out + 20, &c.base.specular, 4);
  memcpy(out + 24, in + 3, 8);
  memcpy(out + 32, &u2, 4);
  memcpy(out + 36, &v2, 4);
}

// A number from a small generator, from -range to range.
struct Gen {
  uint32_t s;
  float Next(float range) {
    s = s * 1664525u + 1013904223u;
    return (static_cast<float>(s >> 8) / 8388608.0f - 1.0f) * range;
  }
};

inline EnvConsts MakeConsts(Gen& g) {
  EnvConsts c;
  for (int i = 0; i < 9; ++i) c.base.m[i] = g.Next(1.0f);
  c.base.m[9] = g.Next(30.0f);
  c.base.m[10] = g.Next(30.0f);
  c.base.m[11] = 40.0f + g.Next(30.0f);
  c.base.near_plane = 1.317f;
  c.base.half_x = 2040.0f;
  c.base.half_y = 1632.0f;
  c.base.max_x = 4079.0f;
  c.base.max_y = 3263.0f;
  c.base.color = 0xFF8090A0u;
  c.base.specular = 0x7F102030u;
  c.light.direction[0] = 0.48f;
  c.light.direction[1] = -0.6f;
  c.light.direction[2] = 0.64f;
  c.light.ambient = 96;
  c.light.color = c.base.color;
  c.k255 = 255.0f;
  for (int i = 0; i < 3; ++i) {
    c.a[i] = g.Next(1.0f);
    c.b[i] = g.Next(1.0f);
  }
  c.ku = 0.5f;
  c.kv = -0.5f;
  c.half = 0.5f;
  c.su = 0.25f;
  c.ou = 0.5f;
  c.sv = 0.125f;
  c.ov = 0.75f;
  return c;
}

inline void LandObjectsChecks() {
  namespace lo = land_objects_math;
  printf("\nThe objects on the land with SSE (land_objects)\n");

  const uint32_t mxcsr = _mm_getcsr();
  _mm_setcsr(mxcsr & ~0xE040u);  // round to the nearest, no flush to zero

  static uint8_t in[64 * lo::kInStride];
  static uint8_t got[64 * lo::kOutStride];
  static uint8_t want[64 * lo::kOutStride];
  static uint32_t got_flags[64];
  static uint32_t want_flags[64];
  int meshes = 0, same = 0, clipped = 0, projected = 0;
  Gen g = {12345u};
  const uint32_t counts[] = {1, 2, 3, 4, 5, 7, 8, 13, 37, 64};
  for (int round = 0; round < 30; ++round) {
    const EnvConsts c = MakeConsts(g);
    const uint32_t n = counts[round % 10];
    for (uint32_t i = 0; i < n; ++i) {
      float* v = reinterpret_cast<float*>(in + i * lo::kInStride);
      v[0] = g.Next(60.0f);
      v[1] = g.Next(60.0f);
      v[2] = g.Next(60.0f);
      v[3] = g.Next(1.0f);
      v[4] = g.Next(1.0f);
      v[5] = g.Next(1.0f);
      v[6] = g.Next(1.0f);
      v[7] = g.Next(1.0f);
    }
    // The bytes that the loop does not write: the same old values in both.
    for (uint32_t i = 0; i < 64 * lo::kOutStride; ++i) {
      got[i] = want[i] = static_cast<uint8_t>(i * 7 + round);
    }
    for (uint32_t i = 0; i < n; ++i) {
      RefVertex(c, reinterpret_cast<const float*>(in + i * lo::kInStride),
                want + i * lo::kOutStride, want_flags + i);
      if (want_flags[i] == 0) {
        ++projected;
      } else {
        ++clipped;
      }
    }
    if (round % 2 == 0) {
      lo::EnvMesh(c, in, 0, n, got, got_flags);
    } else {
      // In parts, as on the threads.
      uint32_t starts[5];
      const int parts = lo::Split(n, 3, 4, starts);
      for (int p = 0; p < parts; ++p) lo::EnvMesh(c, in, starts[p], starts[p + 1], got, got_flags);
    }
    ++meshes;
    if (memcmp(got, want, 64 * lo::kOutStride) == 0 &&
        memcmp(got_flags, want_flags, n * 4) == 0) {
      ++same;
    }
  }
  Check(same == meshes, "30 meshes of 1 to 64 vertices, whole and in parts: the same bits as "
        "the reference", same, meshes);
  Check(clipped > 50 && projected > 50, "the meshes have vertices with and without clip flags",
        clipped < projected ? clipped : projected, 50);

  // One vertex with numbers by hand: the matrix is the unit matrix with the
  // place (0, 0, 10), the vertex at (1, 2, 0) with the normal (0, 0, 1).
  EnvConsts c = MakeConsts(g);
  const float unit[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 10};
  memcpy(c.base.m, unit, sizeof(unit));
  c.a[0] = 0.0f;
  c.a[1] = 0.0f;
  c.a[2] = 1.0f;
  c.b[0] = 0.0f;
  c.b[1] = 0.0f;
  c.b[2] = 0.0f;
  const float one[8] = {1.0f, 2.0f, 0.0f, 0.25f, 0.75f, 0.0f, 0.0f, 1.0f};
  memcpy(in, one, sizeof(one));
  lo::EnvMesh(c, in, 0, 1, got, got_flags);
  float o[10];
  memcpy(o, got, sizeof(o));
  // r = 0.1: x = (0.1 + 1) 2040, y = 1632 - 0.2 1632, w = 1.317 0.1.
  Near("x on the screen", o[0], 2244.0, 0.01);
  Near("y on the screen", o[1], 1305.6, 0.01);
  Near("rhw", o[3], 0.1317, 1e-6);
  Near("z = 1 - rhw", o[2], 0.8683, 1e-6);
  // The light: 0.64 255 = 163.2, so i = 163, level = (159 163 >> 8) + 96.
  uint32_t color;
  memcpy(&color, got + 16, 4);
  const int32_t level = ((255 - 96) * 163 >> 8) + 96;
  const uint32_t want_color =
      ((((0x800000u * level) & 0xFF0000FFu) | ((0x9000u * level) & 0xFF0000u) |
        ((0xA0u * level) & 0xFF00u)) >> 8) | 0xFF000000u;
  Check(color == want_color, "the light of the vertex", color, want_color);
  // u2 = ((1 0.5) + 1) 0.5 0.25 + 0.5, v2 = (0 + 1) 0.5 0.125 + 0.75.
  Near("u2 from the normal", o[8], 0.6875, 1e-6);
  Near("v2 from the normal", o[9], 0.8125, 1e-6);
  Check(got_flags[0] == 0, "the vertex has no clip flag", got_flags[0], 0);
  // The vertex behind the camera keeps its camera x, y and z.
  const float behind[8] = {1.0f, 2.0f, -20.0f, 0.25f, 0.75f, 0.0f, 0.0f, 1.0f};
  memcpy(in, behind, sizeof(behind));
  memset(got, 0xAB, lo::kOutStride);
  lo::EnvMesh(c, in, 0, 1, got, got_flags);
  memcpy(o, got, sizeof(o));
  uint32_t kept;
  memcpy(&kept, got + 8, 4);
  Check((got_flags[0] & 0x20) != 0 && o[3] == -10.0f && o[0] == 1.0f && kept == 0xABABABABu,
        "a vertex behind the camera has the flag 0x20 and keeps its camera place", o[3], -10.0);

  // THE TEST OF THE VIEW. The camera at the origin looks along +z: the
  // camera matrix is the unit matrix. The screen is 640 by 480, half 320 and
  // 240, the near plane 1, k 1 and f 1, so R = r 640 / z.
  lo::CullGlobals cg = {};
  const float unit_cam[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
  memcpy(cg.cam, unit_cam, sizeof(unit_cam));
  cg.near_plane = 1.0f;
  cg.width = 640;
  cg.height = 480;
  cg.half_x = 320.0f;
  cg.half_y = 240.0f;
  cg.k = 1.0f;
  cg.f = 1.0f;
  cg.pick[0] = 320;
  cg.pick[1] = 240;
  lo::CullIn ci = {};
  // The bounds: center 0, radius 1. The part: the unit matrix, scale 1, at
  // (0, 0, 100).
  ci.bounds[3] = 1.0f;
  const float part[13] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 100, 1};
  memcpy(ci.part, part, sizeof(part));
  lo::CullOut co = lo::Cull(ci, cg);
  Check(co.path == 3 && co.in_view == 1 && co.under == 1 && co.depth == 100.0f &&
            co.radius == 1.0f,
        "a sphere straight ahead: in the view, under the pick pixel, path 3", co.path, 3);
  Check(lo::CullReturn(3) == 1 && lo::CullEdx(3) == 0, "path 3 returns 1 and calls with edx 0",
        lo::CullEdx(3), 0);
  cg.pick[0] = 0;
  co = lo::Cull(ci, cg);
  Check(co.path == 3 && co.under == 0, "the pick pixel at the edge: not under it", co.under, 0);
  // Behind the camera.
  ci.part[11] = -100.0f;
  co = lo::Cull(ci, cg);
  Check(co.path == 0 && co.depth == -100.0f && lo::CullReturn(0) == 0,
        "a sphere behind the camera: path 0", co.path, 0);
  // The eye inside the sphere.
  ci.part[11] = 1.5f;
  ci.part[12] = 2.0f;
  co = lo::Cull(ci, cg);
  Check(co.path == 1 && co.in_view == 1 && co.under == 1 && co.radius == 2.0f &&
            lo::CullEdx(1) == 1,
        "the eye inside the sphere: path 1, edx 1", co.path, 1);
  // Far to the right: sx = 320 + 32000 / z... off the screen.
  ci.part[12] = 1.0f;
  ci.part[11] = 100.0f;
  ci.part[9] = 300.0f;
  co = lo::Cull(ci, cg);
  Check(co.path == 2 && co.in_view == 0 && co.radius == 1.0f && lo::CullReturn(2) == 0,
        "a sphere far to the right of the view: path 2", co.path, 2);
  // At the right edge: sx = 320 + 320 * 99 / 100 = 636.8, R = 6.4.
  ci.part[9] = 99.0f;
  co = lo::Cull(ci, cg);
  Check(co.path == 4 && co.in_view == 1 && lo::CullEdx(4) == 1,
        "a sphere over the right edge of the screen: path 4, edx 1", co.path, 4);
  // A NaN radius takes path 0, as the fcomp of the game.
  ci.part[9] = 0.0f;
  ci.bounds[3] = F(0x7FC00000u);
  co = lo::Cull(ci, cg);
  Check(co.path == 0, "a NaN radius: path 0", co.path, 0);

  // Split.
  uint32_t starts[9];
  int parts = lo::Split(100, 3, 4, starts);
  Check(parts == 3 && starts[0] == 0 && starts[1] == 36 && starts[2] == 72 && starts[3] == 100,
        "100 vertices in 3 parts: 0, 36, 72, 100", starts[1], 36);
  parts = lo::Split(300, 4, 256, starts);
  Check(parts == 1 && starts[1] == 300, "300 vertices with at least 256 in a part: one part",
        parts, 1);
  parts = lo::Split(2000, 8, 256, starts);
  bool ok = parts == 7 && starts[parts] == 2000;
  for (int p = 0; p < parts; ++p) ok = ok && starts[p] % 4 == 0 && starts[p] <= starts[p + 1];
  Check(ok, "2000 vertices with at least 256 in a part: 7 parts, each from a multiple of 4",
        parts, 7);

  _mm_setcsr(mxcsr);
}

}  // namespace land_objects_checks
