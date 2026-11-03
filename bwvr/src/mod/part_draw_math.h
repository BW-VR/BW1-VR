// THE DRAW OF A MESH WITH SSE (docs/backend.md, "The parts with SSE"). Pure
// math with no game, so that the checks in bwvr/test/controller_math/ can
// compare it with the x87 code of the game.
//
// The engine draws a 3D part as a list of meshes. After the vertex loops
// ("The vertex loops with SSE") each mesh still runs two pieces of x87
// code. This file has an SSE version of each:
//
//   BuildTriangles      The loop after the vertices in fn_00856D40,
//                       fn_0085AB10, fn_0084BA90 and fn_00859530: the test of
//                       the back face of each triangle and the list of the
//                       triangles that stay.
//   TransformOffsets    The vertex loops of fn_00859530, the mesh of a part
//                       with an offset for each vertex (a person or an
//                       animal): the offset, the matrix, the clip flags, the
//                       projection and the light of the normal.
//
// THE SAME BITS. As in fast_transform_math.h: Direct3D sets the x87 unit to
// a precision of 24 bits, so the game rounds each operation to a float, and
// an SSE operation on floats does the same. Each function makes the
// operations of the game in the order of the game. The caller checks the
// precision and the rounding first (fast_transform_math::Exact).
#pragma once

#include <emmintrin.h>
#include <stdint.h>
#include <string.h>

#include "fast_transform_math.h"

namespace part_draw_math {

using fast_transform_math::Consts;
using fast_transform_math::Light;

// ---------------------------------------------------------------------------
// BuildTriangles

// The room of g_TriangleList (0x00EA5DE0) up to g_TriangleListSize
// (0x00EA9EA4), in words.
const uint32_t kListCapacity = (0x00EA9EA4u - 0x00EA5DE0u) / 2;

// True when the triangle faces the camera:
//   (y2 - y0) (x1 - x0) - (y1 - y0) (x2 - x0) > 0
// on the screen x and y of the vertices (offset 0 and 4 of 32 bytes).
// A NaN is not in front.
inline bool FrontFacing(const uint8_t* table1, uint32_t i0, uint32_t i1,
                        uint32_t i2) {
  const float* v0 = reinterpret_cast<const float*>(table1 + static_cast<size_t>(i0) * 32);
  const float* v1 = reinterpret_cast<const float*>(table1 + static_cast<size_t>(i1) * 32);
  const float* v2 = reinterpret_cast<const float*>(table1 + static_cast<size_t>(i2) * 32);
  const __m128 a = _mm_sub_ss(_mm_load_ss(v2 + 1), _mm_load_ss(v0 + 1));
  const __m128 b = _mm_sub_ss(_mm_load_ss(v1), _mm_load_ss(v0));
  const __m128 c = _mm_sub_ss(_mm_load_ss(v1 + 1), _mm_load_ss(v0 + 1));
  const __m128 d = _mm_sub_ss(_mm_load_ss(v2), _mm_load_ss(v0));
  const __m128 r = _mm_sub_ss(_mm_mul_ss(a, b), _mm_mul_ss(c, d));
  return _mm_comigt_ss(r, _mm_setzero_ps()) != 0;
}

// The triangles of a mesh, in the order of the game. `indices` has 3 words
// for each triangle. `flags` is the clip flags (LH3DP3::Table0) of the
// vertices, or nullptr for a mesh with no flags. `table1` is the vertices
// (LH3DP3::Table1). `no_cull` is bit 0 of the byte at +5 of the mesh: no
// test of the back face.
//
// A triangle with no flag stays when `no_cull` is set or when it faces the
// camera. A triangle with flags that have a plane in common is outside the
// screen and goes away. Else the game clips it: `clip(i0, i1, i2, size)` does
// that and gives the new size of the list. The list is `list`, the size is
// in words. Gives the new size.
template <typename Clip>
inline uint32_t BuildTriangles(const uint16_t* indices, int32_t triangles,
                               bool no_cull, const uint32_t* flags,
                               const uint8_t* table1, uint16_t* list,
                               uint32_t size, Clip&& clip) {
  for (int32_t k = 0; k < triangles; ++k) {
    const uint32_t i0 = indices[3 * k];
    const uint32_t i1 = indices[3 * k + 1];
    const uint32_t i2 = indices[3 * k + 2];
    if (flags != nullptr) {
      const uint32_t f0 = flags[i0], f1 = flags[i1], f2 = flags[i2];
      if ((f0 | f1 | f2) != 0) {
        if ((f0 & f1 & f2) == 0) size = clip(i0, i1, i2, size);
        continue;
      }
    }
    if (no_cull || FrontFacing(table1, i0, i1, i2)) {
      list[size] = static_cast<uint16_t>(i0);
      list[size + 1] = static_cast<uint16_t>(i1);
      list[size + 2] = static_cast<uint16_t>(i2);
      size += 3;
    }
  }
  return size;
}

// ---------------------------------------------------------------------------
// TransformOffsets

// The offsets of fn_00859530 come from 0x00F05180. kXyz: three floats for
// each vertex, added to x, y and z. kY: one float for each vertex, added to
// y.
enum OffsetKind { kXyz = 1, kY = 2 };

namespace detail {

namespace ft = fast_transform_math::detail;

// The order of the additions of fn_00859530. In the loop with three offsets
// (kXyz):
//   x = ((m6 z + m0 x) + m3 y) + m9
//   y = ((m7 z + m4 y) + m1 x) + m10
// In the loop with one offset (kY):
//   x = ((m6 z + m3 y) + m0 x) + m9
//   y = ((m1 x + m7 z) + m4 y) + m10
// and in both:
//   z = ((m8 z + m5 y) + m2 x) + m11
inline ft::Camera TransformOffset(const ft::Wide& w, __m128 x, __m128 y,
                                  __m128 z, OffsetKind kind) {
  ft::Camera out;
  if (kind == kXyz) {
    out.x = _mm_add_ps(
        _mm_add_ps(_mm_add_ps(_mm_mul_ps(w.m[6], z), _mm_mul_ps(x, w.m[0])),
                   _mm_mul_ps(w.m[3], y)),
        w.m[9]);
    out.y = _mm_add_ps(
        _mm_add_ps(_mm_add_ps(_mm_mul_ps(w.m[7], z), _mm_mul_ps(y, w.m[4])),
                   _mm_mul_ps(w.m[1], x)),
        w.m[10]);
  } else {
    out.x = _mm_add_ps(
        _mm_add_ps(_mm_add_ps(_mm_mul_ps(w.m[6], z), _mm_mul_ps(y, w.m[3])),
                   _mm_mul_ps(x, w.m[0])),
        w.m[9]);
    out.y = _mm_add_ps(
        _mm_add_ps(_mm_add_ps(_mm_mul_ps(w.m[1], x), _mm_mul_ps(w.m[7], z)),
                   _mm_mul_ps(y, w.m[4])),
        w.m[10]);
  }
  out.z = _mm_add_ps(
      _mm_add_ps(_mm_add_ps(_mm_mul_ps(w.m[8], z), _mm_mul_ps(w.m[5], y)),
                 _mm_mul_ps(w.m[2], x)),
      w.m[11]);
  return out;
}

}  // namespace detail

// The vertices from `base` of `count`, 1 to 3, through a copy of 4.
template <typename Block>
inline void RestOffsets(const uint8_t* in, const float* offsets, int per_vertex,
                        uint32_t count, uint32_t base, uint8_t* out,
                        uint32_t* flags, Block block) {
  const uint32_t n = count - base;
  if (n == 0) return;
  const int kStride = 32;
  uint8_t in4[4 * 32];
  uint8_t out4[4 * 32];
  float off4[12];
  uint32_t flags4[4];
  for (uint32_t i = 0; i < 4; ++i) {
    const uint32_t from = base + (i < n ? i : n - 1);
    memcpy(in4 + i * kStride, in + from * kStride, kStride);
    memcpy(out4 + i * kStride, out + from * kStride, kStride);
    memcpy(off4 + i * per_vertex, offsets + from * per_vertex,
           per_vertex * sizeof(float));
  }
  block(in4, off4, out4, flags4);
  memcpy(out + base * kStride, out4, n * kStride);
  memcpy(flags + base, flags4, n * 4);
}

// The loop of fn_00859530 for `count` vertices. `in` is the input (32 bytes
// each), `offsets` the offsets, `out` the vertices of the table (D3DTLVERTEX,
// 32 bytes) and `flags` the clip flags. The color is lit by the normal:
// level = trunc(((l0 nx + l2 nz) + l1 ny) 255), the truncation of the cvttsd2si
// in __ftol of the game.
inline void TransformOffsets(const Consts& c, const Light& l, OffsetKind kind,
                             const uint8_t* in, const float* offsets,
                             uint32_t count, uint8_t* out, uint32_t* flags) {
  namespace ft = fast_transform_math::detail;
  const ft::Wide w(c);
  const __m128 sign = _mm_set1_ps(-0.0f);
  const __m128i bit20 = _mm_set1_epi32(0x20), bit10 = _mm_set1_epi32(0x10);
  const __m128i bit08 = _mm_set1_epi32(0x08), bit04 = _mm_set1_epi32(0x04);
  const __m128i bit02 = _mm_set1_epi32(0x02);
  const __m128 lane2 = _mm_castsi128_ps(_mm_set_epi32(0, -1, 0, 0));
  const __m128 l0 = _mm_set1_ps(l.direction[0]);
  const __m128 l1 = _mm_set1_ps(l.direction[1]);
  const __m128 l2 = _mm_set1_ps(l.direction[2]);
  const __m128 full = _mm_set1_ps(255.0f);
  const int per_vertex = kind == kXyz ? 3 : 1;
  auto block = [&](const uint8_t* src, const float* off, uint8_t* dst,
                   uint32_t* f4) {
    const ft::Columns in4 = ft::Load4(src, true);
    __m128 x = in4.x, y = in4.y, z = in4.z;
    if (kind == kXyz) {
      const __m128 ox = _mm_setr_ps(off[0], off[3], off[6], off[9]);
      const __m128 oy = _mm_setr_ps(off[1], off[4], off[7], off[10]);
      const __m128 oz = _mm_setr_ps(off[2], off[5], off[8], off[11]);
      x = _mm_add_ps(ox, x);
      y = _mm_add_ps(y, oy);
      z = _mm_add_ps(z, oz);
    } else {
      y = _mm_add_ps(y, _mm_loadu_ps(off));
    }
    const ft::Camera v = detail::TransformOffset(w, x, y, z, kind);
    const __m128 dot = _mm_mul_ps(
        _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, in4.nx), _mm_mul_ps(l2, in4.nz)),
                   _mm_mul_ps(l1, in4.ny)),
        full);
    int32_t level[4];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(level), _mm_cvttps_epi32(dot));
    const __m128 neg_z = _mm_xor_ps(v.z, sign);
    const __m128 nearer = _mm_cmpnge_ps(v.z, w.near_plane);
    const __m128 right = _mm_cmpgt_ps(v.x, v.z);
    const __m128 left = _mm_andnot_ps(right, _mm_cmpgt_ps(neg_z, v.x));
    const __m128 below = _mm_cmpgt_ps(v.y, v.z);
    const __m128 above = _mm_andnot_ps(below, _mm_cmpgt_ps(neg_z, v.y));
    const __m128i f = _mm_or_si128(
        _mm_or_si128(_mm_and_si128(_mm_castps_si128(nearer), bit20),
                     _mm_and_si128(_mm_castps_si128(right), bit10)),
        _mm_or_si128(_mm_and_si128(_mm_castps_si128(left), bit08),
                     _mm_or_si128(_mm_and_si128(_mm_castps_si128(below), bit04),
                                  _mm_and_si128(_mm_castps_si128(above), bit02))));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(f4), f);
    const int clipped = ~_mm_movemask_ps(
        _mm_castsi128_ps(_mm_cmpeq_epi32(f, _mm_setzero_si128())));
    ft::Screen s = ft::Project(w, v, ft::kYRThenHalf);
    _MM_TRANSPOSE4_PS(s.x, s.y, s.z, s.rhw);
    const __m128 projected[4] = {s.x, s.y, s.z, s.rhw};
    __m128 cx = v.x, cy = v.y, cz = v.z, cw = v.z;
    _MM_TRANSPOSE4_PS(cx, cy, cz, cw);
    const __m128 camera[4] = {cx, cy, cz, cw};
    for (int i = 0; i < 4; ++i) {
      float* row = reinterpret_cast<float*>(dst + i * ft::kStride);
      if ((clipped & (1 << i)) != 0) {
        const __m128 old = _mm_loadu_ps(row);
        _mm_storeu_ps(row, _mm_or_ps(_mm_andnot_ps(lane2, camera[i]),
                                     _mm_and_ps(lane2, old)));
      } else {
        _mm_storeu_ps(row, projected[i]);
      }
      ft::StoreTail(dst + i * ft::kStride, fast_transform_math::LitColor(level[i], l),
                    c.specular, src + i * ft::kStride);
    }
  };
  uint32_t base = 0;
  for (; base + 4 <= count; base += 4) {
    block(in + base * ft::kStride, offsets + base * per_vertex,
          out + base * ft::kStride, flags + base);
  }
  RestOffsets(in, offsets, per_vertex, count, base, out, flags, block);
}

}  // namespace part_draw_math
