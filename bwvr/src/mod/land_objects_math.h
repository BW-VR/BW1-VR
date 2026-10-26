// THE OBJECTS ON THE LAND WITH SSE (land_objects.cpp, docs/backend.md, "The
// objects on the land"). Pure math with no game, so that the checks in
// bwvr/test/controller_math/ can test it.
//
// THE LOOP. fn_00852C40 draws a mesh with a second texture that the normals
// of the mesh place (a reflection map), for the draws of fn_0080F700. With
// one matrix for the mesh it makes, for each vertex, on the x87 unit:
//
//   x = ((m6 z + m3 y) + m0 x) + m9         the camera place, as in the
//   y = ((m7 z + m1 x) + m4 y) + m10        other mesh loops of the engine
//   z = ((m8 z + m2 x) + m5 y) + m11
//   i = ((l2 nz + l1 ny) + l0 nx) k255      the light, rounded to a whole
//                                           number by the rounding of the unit
//   color = LitColor(i)                     as in fast_transform_math.h
//   eu = ((((a2 nz + a1 ny) + a0 nx) ku) + 1) half
//   ev = ((((b2 nz + b1 ny) + b0 nx) kv) + 1) half
//   u2 = eu su + ou,  v2 = ev sv + ov       the place in the reflection map
//
// and the clip flags and the projection of fn_00856D40: 0x20 nearer than the
// near plane, 0x10 right of the view and else 8 left of it, 4 below and else
// 2 above. A vertex with no flag gets r = 1 / z, x = (r x + 1) half_x, the
// screen y half_y - (y r) half_y, w = near r and z = 1 - w, with the clamp of
// the screen. A vertex with a flag keeps its camera x, y and z, and the
// value at offset 8 does not change.
//
// The output is a vertex of 40 bytes (D3DTLVERTEX with a second pair of
// texture coordinates): x, y, z, w (rhw), color, specular, u, v, u2, v2. The
// input is a vertex of 32 bytes: x, y, z, u, v, nx, ny, nz.
//
// THE SAME BITS. As fast_transform_math.h: each operation in the order of
// the game, the caller checks the precision and the rounding of the x87 unit
// and of SSE (fast_transform_math::Exact) and sets MXCSR with no flush to
// zero.
#pragma once

#include <emmintrin.h>
#include <stdint.h>
#include <string.h>

#include "fast_transform_math.h"

namespace land_objects_math {

const int kInStride = 32;
const int kOutStride = 40;

struct EnvConsts {
  fast_transform_math::Consts base;  // the matrix, the screen, the colors
  fast_transform_math::Light light;  // the light and the color of the mesh
  float k255;     // 0x008AB270
  float a[3];     // the row of u2 for nx, ny and nz (locals of fn_00852C40)
  float b[3];     // the row of v2 for nx, ny and nz
  float ku, kv;   // 0x00C37EA4, 0x00C37EA8
  float half;     // 0x009A3B98
  float su, ou;   // the scale and the offset of u2 (the slot at 0x00EA0284)
  float sv, ov;   // the scale and the offset of v2
};

namespace detail {

using fast_transform_math::detail::Camera;
using fast_transform_math::detail::Columns;
using fast_transform_math::detail::Screen;
using fast_transform_math::detail::Wide;

// Four vertices of a mesh from `src` to `dst` and their flags to `f4`.
inline void Block(const EnvConsts& c, const Wide& w, const uint8_t* src, uint8_t* dst,
                  uint32_t* f4) {
  namespace fd = fast_transform_math::detail;
  const Columns in4 = fd::Load4(src, true);
  const Camera v = fd::Transform(w, in4.x, in4.y, in4.z, fd::kMeshOrder);
  // The light.
  const __m128 dot = _mm_mul_ps(
      _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(c.light.direction[2]), in4.nz),
                            _mm_mul_ps(_mm_set1_ps(c.light.direction[1]), in4.ny)),
                 _mm_mul_ps(_mm_set1_ps(c.light.direction[0]), in4.nx)),
      _mm_set1_ps(c.k255));
  int32_t level[4];
  _mm_storeu_si128(reinterpret_cast<__m128i*>(level), _mm_cvtps_epi32(dot));
  // The place in the reflection map.
  const __m128 one = _mm_set1_ps(1.0f);
  const __m128 half = _mm_set1_ps(c.half);
  const __m128 eu = _mm_mul_ps(
      _mm_add_ps(
          _mm_mul_ps(
              _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(c.a[2]), in4.nz),
                                    _mm_mul_ps(_mm_set1_ps(c.a[1]), in4.ny)),
                         _mm_mul_ps(_mm_set1_ps(c.a[0]), in4.nx)),
              _mm_set1_ps(c.ku)),
          one),
      half);
  const __m128 ev = _mm_mul_ps(
      _mm_add_ps(
          _mm_mul_ps(
              _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(c.b[2]), in4.nz),
                                    _mm_mul_ps(_mm_set1_ps(c.b[1]), in4.ny)),
                         _mm_mul_ps(_mm_set1_ps(c.b[0]), in4.nx)),
              _mm_set1_ps(c.kv)),
          one),
      half);
  float u2[4], v2[4];
  _mm_storeu_ps(u2, _mm_add_ps(_mm_mul_ps(eu, _mm_set1_ps(c.su)), _mm_set1_ps(c.ou)));
  _mm_storeu_ps(v2, _mm_add_ps(_mm_mul_ps(ev, _mm_set1_ps(c.sv)), _mm_set1_ps(c.ov)));
  // The clip flags, as fast_transform_math.h with kClip.
  const __m128 neg_z = _mm_xor_ps(v.z, _mm_set1_ps(-0.0f));
  const __m128 nearer = _mm_cmpnge_ps(v.z, w.near_plane);
  const __m128 right = _mm_cmpgt_ps(v.x, v.z);
  const __m128 left = _mm_andnot_ps(right, _mm_cmpgt_ps(neg_z, v.x));
  const __m128 below = _mm_cmpgt_ps(v.y, v.z);
  const __m128 above = _mm_andnot_ps(below, _mm_cmpgt_ps(neg_z, v.y));
  const __m128i f = _mm_or_si128(
      _mm_or_si128(_mm_and_si128(_mm_castps_si128(nearer), _mm_set1_epi32(0x20)),
                   _mm_and_si128(_mm_castps_si128(right), _mm_set1_epi32(0x10))),
      _mm_or_si128(_mm_and_si128(_mm_castps_si128(left), _mm_set1_epi32(0x08)),
                   _mm_or_si128(_mm_and_si128(_mm_castps_si128(below), _mm_set1_epi32(0x04)),
                                _mm_and_si128(_mm_castps_si128(above), _mm_set1_epi32(0x02)))));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(f4), f);
  const int clipped =
      ~_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(f, _mm_setzero_si128())));
  Screen s = fd::Project(w, v, fd::kYRThenHalf);
  _MM_TRANSPOSE4_PS(s.x, s.y, s.z, s.rhw);
  const __m128 projected[4] = {s.x, s.y, s.z, s.rhw};
  __m128 cx = v.x, cy = v.y, cz = v.z, cw = v.z;
  _MM_TRANSPOSE4_PS(cx, cy, cz, cw);
  const __m128 camera[4] = {cx, cy, cz, cw};
  // The lane of z in a row, for the value at offset 8 that stays.
  const __m128 lane2 = _mm_castsi128_ps(_mm_set_epi32(0, -1, 0, 0));
  for (int i = 0; i < 4; ++i) {
    uint8_t* out = dst + i * kOutStride;
    float* row = reinterpret_cast<float*>(out);
    if ((clipped & (1 << i)) != 0) {
      const __m128 old = _mm_loadu_ps(row);
      _mm_storeu_ps(row, _mm_or_ps(_mm_andnot_ps(lane2, camera[i]), _mm_and_ps(lane2, old)));
    } else {
      _mm_storeu_ps(row, projected[i]);
    }
    const uint32_t color = fast_transform_math::LitColor(level[i], c.light);
    const uint8_t* in = src + i * kInStride;
    memcpy(out + 16, &color, 4);
    memcpy(out + 20, &c.base.specular, 4);
    memcpy(out + 24, in + 12, 8);
    memcpy(out + 32, &u2[i], 4);
    memcpy(out + 36, &v2[i], 4);
  }
}

}  // namespace detail

// The vertices `first` to `last` (not included) of the mesh with one matrix
// of fn_00852C40. `in`, `out` and `flags` are the starts of the whole mesh,
// so that parts of one mesh can go to different threads.
inline void EnvMesh(const EnvConsts& c, const uint8_t* in, uint32_t first, uint32_t last,
                    uint8_t* out, uint32_t* flags) {
  const fast_transform_math::detail::Wide w(c.base);
  uint32_t i = first;
  for (; i + 4 <= last; i += 4) {
    detail::Block(c, w, in + i * kInStride, out + i * kOutStride, flags + i);
  }
  const uint32_t n = last - i;
  if (n == 0) return;
  // The last 1 to 3 vertices by way of a copy of 4. The copy of the output
  // starts as the output, so that the bytes that the block does not write
  // stay.
  uint8_t in4[4 * kInStride];
  uint8_t out4[4 * kOutStride];
  uint32_t flags4[4];
  for (uint32_t k = 0; k < 4; ++k) {
    const uint32_t from = i + (k < n ? k : n - 1);
    memcpy(in4 + k * kInStride, in + from * kInStride, kInStride);
    memcpy(out4 + k * kOutStride, out + from * kOutStride, kOutStride);
  }
  detail::Block(c, w, in4, out4, flags4);
  memcpy(out + i * kOutStride, out4, n * kOutStride);
  memcpy(flags + i, flags4, n * 4);
}

// THE TEST OF THE VIEW (LH3DBoundingBox::CheckRegionOnScreen, fn_00868C80,
// the path of the x87 unit). It tests the sphere of the bounds of a mesh
// (the center at +4, +8, +0xC and the radius at +0x1C of the bounds) with the
// matrix of a 3D part (+0x14 to +0x40, rows of 3, and the scale at +0x44):
//
//   r = scale radius
//   w = ((m3 b8 + m6 bc) + b4 m0) + p0,  the same for y with m1 m4 m7 p1 and
//       z with m2 m5 m8 p2 (the place of the center in the world)
//   c = ((k6 wz + k3 wy) + k0 wx) + k9 and so on with the camera matrix k
//       (0x00EA9E40), the depth cz goes to 0x00EA1AF4
//   path 0: cz + r < near (or a NaN): return 0
//   path 1: the eye is inside the sphere (r r > d d to the place p): the call
//       +0xA0 of the part with edx 1, return 1
//   q = 1 / cz, sx = ((cx q) + 1) half_x, sy = (1 - (cy q)) half_y,
//   R = ((((r near) q) W) k) / f, the radius on the screen (W the width of
//       the screen as a float, k 0x008AA3B4, f 0x00C3812C)
//   path 2: R + sx < 0, sx - R > W, R + sy < 0 or sy - R > H: return 0
//   else the object is in the view (0x00EA1AF0 = 1), the pick pixel is on
//       it when R R > the square of its distance to (sx, sy) (0x00EA1AD0 = 1),
//       and path 3 (the circle is inside the screen: sx - R > 0, R + sx < W,
//       sy - R > 0, R + sy < H) calls +0xA0 with edx 0, path 4 with edx 1.
//       Both return 1.
//
// A NaN takes the path of the fcomp of the game: a test of C0 is true for a
// NaN, a test of C0 and C3 is false.

struct CullGlobals {
  float cam[12];        // 0x00EA9E40, rows of 3
  float near_plane;     // 0x00E839E0
  int32_t width;        // 0x00E839E4
  int32_t height;       // 0x00E839E8
  float half_x;         // 0x00E839F0
  float half_y;         // 0x00E839F4
  float eye[3];         // 0x00EA1DB8
  int32_t pick[2];      // 0x00EA1AC8, the pick pixel
  float k;              // 0x008AA3B4
  float f;              // 0x00C3812C
};

struct CullIn {
  float bounds[4];  // +4, +8, +0xC and +0x1C of the bounds
  float part[13];   // +0x14 to +0x44 of the 3D part
};

struct CullOut {
  int path = 0;
  float depth = 0.0f;   // 0x00EA1AF4
  float radius = 0.0f;  // 0x00C37EA0, for the paths 1 to 4
  int under = 0;        // 0x00EA1AD0
  int in_view = 0;      // 0x00EA1AF0
};

// The return value of the game for a path, and the edx of the call +0xA0
// (-1 for no call).
inline int CullReturn(int path) { return path == 0 || path == 2 ? 0 : 1; }
inline int CullEdx(int path) { return path == 1 || path == 4 ? 1 : (path == 3 ? 0 : -1); }

namespace detail {
inline __m128 S(float v) { return _mm_set_ss(v); }
inline __m128 Add(__m128 a, __m128 b) { return _mm_add_ss(a, b); }
inline __m128 Sub(__m128 a, __m128 b) { return _mm_sub_ss(a, b); }
inline __m128 Mul(__m128 a, __m128 b) { return _mm_mul_ss(a, b); }
inline __m128 Div(__m128 a, __m128 b) { return _mm_div_ss(a, b); }
inline float V(__m128 a) { return _mm_cvtss_f32(a); }
// a < b or a NaN: C0 of fcomp. The compares of SSE with a mask are false for
// a NaN.
inline bool Below(__m128 a, __m128 b) { return (_mm_movemask_ps(_mm_cmpge_ss(a, b)) & 1) == 0; }
// a > b, and no NaN: neither C0 nor C3.
inline bool Above(__m128 a, __m128 b) { return (_mm_movemask_ps(_mm_cmpgt_ss(a, b)) & 1) != 0; }
}  // namespace detail

inline CullOut Cull(const CullIn& in, const CullGlobals& g) {
  using namespace detail;
  CullOut out;
  const float* m = in.part;
  const __m128 b4 = S(in.bounds[0]), b8 = S(in.bounds[1]), bc = S(in.bounds[2]);
  const __m128 r = Mul(S(m[12]), S(in.bounds[3]));
  const __m128 wx = Add(Add(Add(Mul(S(m[3]), b8), Mul(S(m[6]), bc)), Mul(b4, S(m[0]))), S(m[9]));
  const __m128 wy = Add(Add(Add(Mul(S(m[1]), b4), Mul(S(m[4]), b8)), Mul(S(m[7]), bc)), S(m[10]));
  const __m128 wz = Add(Add(Add(Mul(S(m[2]), b4), Mul(S(m[5]), b8)), Mul(S(m[8]), bc)), S(m[11]));
  const float* k = g.cam;
  const __m128 cx = Add(Add(Add(Mul(S(k[6]), wz), Mul(S(k[3]), wy)), Mul(S(k[0]), wx)), S(k[9]));
  const __m128 cy = Add(Add(Add(Mul(S(k[7]), wz), Mul(S(k[4]), wy)), Mul(S(k[1]), wx)), S(k[10]));
  const __m128 cz = Add(Add(Add(Mul(S(k[8]), wz), Mul(S(k[5]), wy)), Mul(S(k[2]), wx)), S(k[11]));
  out.depth = V(cz);
  if (Below(Add(cz, r), S(g.near_plane))) {
    out.path = 0;
    return out;
  }
  out.radius = V(r);
  const __m128 dx = Sub(S(m[9]), S(g.eye[0]));
  const __m128 dy = Sub(S(m[10]), S(g.eye[1]));
  const __m128 dz = Sub(S(m[11]), S(g.eye[2]));
  const __m128 d2 = Add(Add(Mul(dx, dx), Mul(dy, dy)), Mul(dz, dz));
  if (Above(Mul(r, r), d2)) {
    out.path = 1;
    out.in_view = 1;
    out.under = 1;
    return out;
  }
  const __m128 one = S(1.0f);
  const __m128 q = Div(one, cz);
  const __m128 sx = Mul(Add(Mul(cx, q), one), S(g.half_x));
  const __m128 sy = Mul(Sub(one, Mul(cy, q)), S(g.half_y));
  const __m128 w = _mm_cvtsi32_ss(_mm_setzero_ps(), g.width);
  const __m128 h = _mm_cvtsi32_ss(_mm_setzero_ps(), g.height);
  const __m128 R = Div(Mul(Mul(Mul(Mul(r, S(g.near_plane)), q), w), S(g.k)), S(g.f));
  const __m128 a = Add(R, sx);
  const __m128 b = Sub(sx, R);
  const __m128 c = Add(R, sy);
  const __m128 d = Sub(sy, R);
  if (Below(a, _mm_setzero_ps()) || Above(b, w) || Below(c, _mm_setzero_ps()) || Above(d, h)) {
    out.path = 2;
    return out;
  }
  out.in_view = 1;
  const __m128 px = Sub(_mm_cvtsi32_ss(_mm_setzero_ps(), g.pick[0]), sx);
  const __m128 py = Sub(_mm_cvtsi32_ss(_mm_setzero_ps(), g.pick[1]), sy);
  out.under = Above(Mul(R, R), Add(Mul(py, py), Mul(px, px))) ? 1 : 0;
  const bool inside = Above(b, _mm_setzero_ps()) && Below(a, w) && Above(d, _mm_setzero_ps()) &&
                      Below(c, h);
  out.path = inside ? 3 : 4;
  return out;
}

// THE PARTS FOR THE THREADS. A mesh of `count` vertices goes in parts of a
// multiple of 4 vertices to `parts` threads, each part at least `least`
// vertices. Gives the count of parts (1 when the mesh is too small), and the
// first vertex of part p in `starts[p]`, with starts[count of parts] = count.
inline int Split(uint32_t count, int parts, uint32_t least, uint32_t* starts) {
  if (parts < 1) parts = 1;
  if (least < 4) least = 4;
  uint32_t most = count / least;
  if (most < 1) most = 1;
  const int n = parts < static_cast<int>(most) ? parts : static_cast<int>(most);
  const uint32_t each = ((count / n) + 3u) & ~3u;
  uint32_t at = 0;
  for (int p = 0; p < n; ++p) {
    starts[p] = at;
    at = at + each < count ? at + each : count;
  }
  starts[n] = count;
  return n;
}

}  // namespace land_objects_math
