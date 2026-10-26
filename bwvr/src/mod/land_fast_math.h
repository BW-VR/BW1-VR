// The vertex loop of the land with SSE (docs/backend.md, "The vertex loop of
// the land with SSE"). Pure math with no game, so that the checks in
// bwvr/test/controller_math/ can compare it with a reference for one vertex.
//
// THE LOOP. fn_00874AA0 draws one patch of the land: a grid of 17 by 17
// cells, or each 2nd, 4th, 8th or 16th cell for a far patch (the step). The
// loop makes a D3DTLVERTEX of LH3DP3::Table1 for each cell of the grid, one
// at a time on the x87 unit. Grid makes four cells of a row at a time.
//
// A CELL has 8 bytes: a color word (bits 0 to 23 the specular color, bits 24
// to 31 the index of the diffuse color in a table of 256 colors) and a
// height byte. A height of 3 or less is the sea: the vertex has the height
// `zero`. For such a patch (clear_low) a height of 1 or less also clears the
// alpha of the specular color, which is the fog of Direct3D.
//
// THE MATH OF ONE VERTEX. The point is (r, h, c): r is the coordinate of the
// row (patch + 0x90C, plus the step of the grid for each row), h the height
// times `height_scale`, c the coordinate of the column (patch + 0x910, plus
// the step for each column). The game adds the step one time for each row
// and each column, so the coordinates are sums of floats, and Grid makes the
// same sums. The camera matrix has rows of 3: X = ((m3 h + m6 c) + m0 r) +
// m9, Y and W the same with the next column of the matrix.
//
// - light 0: no fog. light 2: the fog color and the dim of the diffuse color
//   are constants. Another light: the fog follows W. W clamped to fog_near
//   and fog_far gives t = (W - fog_near) / fog_range. The dim is 256 less
//   the integer part of (256 - fog_dim) t. The fog color has the rounded
//   values of fog_rgb times t. The specular color gets the fog color, each
//   byte added with a limit of 255. A specular color of 0 gets the fog color
//   with no add. A dim less than 256 scales each byte of the diffuse color
//   by dim / 256.
// - clip 0: each vertex gets a projection. clip 1: each vertex gets the clip
//   flags in LH3DP3::Table0 (0x20 nearer than the near plane, 0x10 and 8
//   right and left, 4 and 2 top and bottom), and only a vertex with no flag
//   gets a projection. A vertex with a flag keeps X, Y and W, and its z is
//   what was there before.
// - The projection: rhw = one / W, x = (rhw X + one) half_x, y = half_y -
//   (rhw Y) half_y, each clamped to 0 and max, then rhw = near rhw and z =
//   one - rhw.
// - u and v are the sums of the steps of the texture, as r and c.
//
// THE SAME BITS. As fast_transform_math.h: the game runs with the x87 unit at
// a precision of 24 bits, so each operation of the game rounds to a float,
// and an SSE operation does the same. Grid makes the operations of the game
// in the order of the game. The comparisons of the game are fcomp with
// fnstsw: a test of C0 is true for a NaN, a test of C0 and C3 is false for a
// NaN. The integer part is a truncation (the _ftol of the game), the fog
// bytes round to the nearest (fistp). The caller checks the precision and
// the rounding of the x87 unit and of SSE first (fast_transform_math::Exact).
#pragma once

#include <emmintrin.h>
#include <stdint.h>
#include <string.h>

namespace land_fast_math {

// The values that the loop reads from the globals of the game.
struct Consts {
  float m[12];            // the camera matrix of the land, rows of 3, 0x00EA9E40
  float one;              // 0x008AA390
  float zero;             // 0x008AA398
  float height_scale;     // 0x00C3720C
  float near_plane;       // 0x00E839E0
  float half_x;           // 0x00E839F0
  float half_y;           // 0x00E839F4
  float max_x;            // g_MaxScreen.x, 0x00C2AB00
  float max_y;            // g_MaxScreen.y, 0x00C2AB04
  float fog_near;         // 0x00C37220
  float fog_far;          // 0x00C37224
  float fog_range;        // 0x00E9B6DC
  float fog_rgb[3];       // 0x00C37214, red, green, blue
  int32_t fog_dim;        // 0x00C37228
  uint32_t fog_color;     // the fog color of light 2, 0x00E9B6D8
  const uint32_t* colors; // the 256 diffuse colors, 0x00EDD90C
};

// One patch, with the values that the game has at the start of the loop.
struct Patch {
  const uint8_t* cells;   // the patch, 8 bytes for each cell
  int32_t step;           // 1 << (patch + 0x930)
  int32_t row_skip;       // the cells to skip at the end of a row, [ebp-44h]
  float r0;               // patch + 0x90C
  float c0;               // patch + 0x910
  float d;                // the step of r and c, [ebp-28h]
  float u0;               // [ebp-3Ch]
  float v0;               // [ebp-10h]
  float du;               // the step of u and v, [ebp-14h]
  int32_t clip;           // patch + 0x91C
  int32_t clear_low;      // patch + 0x928
  int32_t light;          // patch + 0x940
};

// The count of cells in a row (and of rows) of a grid.
inline int32_t GridSide(int32_t step) {
  if (step <= 0) return 0;
  return (17 + step - 1) / step;
}

// True when the fog of Grid gives the bits of the game: the game loads
// 256 - fog_dim as an integer with no rounding, SSE as a float.
inline bool FogExact(int32_t fog_dim) {
  const int64_t v = 256 - static_cast<int64_t>(fog_dim);
  return v > -(1 << 24) && v < (1 << 24);
}

namespace detail {

inline __m128 Blend(__m128 mask, __m128 yes, __m128 no) {
  return _mm_or_ps(_mm_and_ps(mask, yes), _mm_andnot_ps(mask, no));
}

inline uint32_t AddByte(uint32_t a, uint32_t b) {
  const uint32_t s = a + b;
  return s > 0xFFu ? 0xFFu : s;
}

// The specular color with the fog color added, each byte with a limit.
inline uint32_t AddFog(uint32_t specular, uint32_t fog) {
  if (specular == 0) return fog;
  const uint32_t red = AddByte((specular >> 16) & 0xFFu, (fog >> 16) & 0xFFu);
  const uint32_t green = AddByte((specular >> 8) & 0xFFu, (fog >> 8) & 0xFFu);
  const uint32_t blue = AddByte(specular & 0xFFu, fog & 0xFFu);
  return (specular & 0xFF000000u) | (red << 16) | (green << 8) | blue;
}

// Each byte of the diffuse color times dim / 256, alpha kept. dim is an
// unsigned value below 256. The products wrap as the 32 bit products of
// the game.
inline uint32_t Dim(uint32_t color, uint32_t dim) {
  const uint32_t red = ((color & 0xFF0000u) * dim) & 0xFF0000FFu;
  const uint32_t green = ((color & 0xFF00u) * dim) & 0xFF0000u;
  const uint32_t blue = ((color & 0xFFu) * dim) & 0xFF00u;
  return ((red | green | blue) >> 8) | (color & 0xFF000000u);
}

}  // namespace detail

// Makes the vertices of one patch into `out` (32 bytes each, as Table1) and,
// with clip, the clip flags into `flags` (as Table0). Gives the count of
// vertices, and in *cells_end the place after the last row of cells, where
// the loop of the game leaves its pointer.
inline uint32_t Grid(const Consts& k, const Patch& p, uint8_t* out,
                     uint32_t* flags, const uint8_t** cells_end) {
  const int32_t side = GridSide(p.step);
  const uint8_t* row_cells = p.cells;
  if (side <= 0 || side > 17) {
    if (cells_end != nullptr) *cells_end = row_cells;
    return 0;
  }
  // The coordinates and the u values of the columns are the same in each
  // row: the sums of the steps from c0 and u0.
  float cs[20], us[20];
  {
    __m128 c = _mm_set_ss(p.c0), u = _mm_set_ss(p.u0);
    const __m128 d = _mm_set_ss(p.d), du = _mm_set_ss(p.du);
    for (int32_t j = 0; j < 20; ++j) {
      _mm_store_ss(&cs[j], c);
      _mm_store_ss(&us[j], u);
      c = _mm_add_ss(c, d);
      u = _mm_add_ss(u, du);
    }
  }
  const __m128 m3 = _mm_set1_ps(k.m[3]), m4 = _mm_set1_ps(k.m[4]),
               m5 = _mm_set1_ps(k.m[5]);
  const __m128 m6 = _mm_set1_ps(k.m[6]), m7 = _mm_set1_ps(k.m[7]),
               m8 = _mm_set1_ps(k.m[8]);
  const __m128 m9 = _mm_set1_ps(k.m[9]), m10 = _mm_set1_ps(k.m[10]),
               m11 = _mm_set1_ps(k.m[11]);
  const __m128 one = _mm_set1_ps(k.one), zero = _mm_set1_ps(k.zero);
  const __m128 scale = _mm_set1_ps(k.height_scale);
  const __m128 near_plane = _mm_set1_ps(k.near_plane);
  const __m128 half_x = _mm_set1_ps(k.half_x), half_y = _mm_set1_ps(k.half_y);
  const __m128 max_x = _mm_set1_ps(k.max_x), max_y = _mm_set1_ps(k.max_y);
  const __m128 sign = _mm_castsi128_ps(_mm_set1_epi32(static_cast<int>(0x80000000u)));
  const __m128 fog_near = _mm_set1_ps(k.fog_near), fog_far = _mm_set1_ps(k.fog_far);
  const __m128 fog_range = _mm_set1_ps(k.fog_range);
  const __m128 fog_r = _mm_set1_ps(k.fog_rgb[0]), fog_g = _mm_set1_ps(k.fog_rgb[1]),
               fog_b = _mm_set1_ps(k.fog_rgb[2]);
  const __m128 fog_span = _mm_cvtsi32_ss(_mm_setzero_ps(), 256 - k.fog_dim);
  const __m128 span = _mm_shuffle_ps(fog_span, fog_span, 0);

  __m128 r = _mm_set_ss(p.r0), v = _mm_set_ss(p.v0);
  const __m128 d = _mm_set_ss(p.d), du = _mm_set_ss(p.du);
  uint32_t n = 0;
  for (int32_t row = 0; row < side; ++row) {
    float rf, vf;
    _mm_store_ss(&rf, r);
    _mm_store_ss(&vf, v);
    const __m128 rr = _mm_set1_ps(rf);
    const __m128 m0r = _mm_mul_ps(_mm_set1_ps(k.m[0]), rr);
    const __m128 m1r = _mm_mul_ps(_mm_set1_ps(k.m[1]), rr);
    const __m128 m2r = _mm_mul_ps(_mm_set1_ps(k.m[2]), rr);
    const uint8_t* cell = row_cells;
    for (int32_t j0 = 0; j0 < side; j0 += 4) {
      const int32_t lanes = side - j0 < 4 ? side - j0 : 4;
      // The inputs of the lanes. A lane past the row repeats the last cell.
      uint32_t word[4];
      int32_t height[4];
      uint32_t specular[4], diffuse[4];
      for (int32_t i = 0; i < 4; ++i) {
        const uint8_t* at = cell + (i < lanes ? i : lanes - 1) * p.step * 8;
        memcpy(&word[i], at, 4);
        height[i] = at[4];
        specular[i] = word[i] | 0xFF000000u;
        if (height[i] <= 3 && p.clear_low != 0 && height[i] <= 1) {
          specular[i] &= 0x00FFFFFFu;
        }
        diffuse[i] = k.colors[word[i] >> 24];
      }
      cell += lanes * p.step * 8;
      const __m128i hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(height));
      const __m128 sea = _mm_castsi128_ps(_mm_cmplt_epi32(hi, _mm_set1_epi32(4)));
      const __m128 h = detail::Blend(sea, zero, _mm_mul_ps(_mm_cvtepi32_ps(hi), scale));
      const __m128 c = _mm_loadu_ps(&cs[j0]);
      const __m128 X =
          _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(m3, h), _mm_mul_ps(m6, c)), m0r), m9);
      const __m128 Y =
          _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(m4, h), _mm_mul_ps(m7, c)), m1r), m10);
      const __m128 W =
          _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(m5, h), _mm_mul_ps(m8, c)), m2r), m11);

      if (p.light != 0) {
        uint32_t fog[4], dim[4];
        if (p.light == 2) {
          for (int32_t i = 0; i < 4; ++i) {
            fog[i] = k.fog_color;
            dim[i] = static_cast<uint32_t>(k.fog_dim);
          }
        } else {
          __m128 w = detail::Blend(_mm_cmpgt_ps(W, fog_far), fog_far, W);
          w = detail::Blend(_mm_cmpnge_ps(W, fog_near), fog_near, w);
          const __m128 t = _mm_div_ps(_mm_sub_ps(w, fog_near), fog_range);
          const __m128i part = _mm_cvttps_epi32(_mm_mul_ps(span, t));
          const __m128i red = _mm_cvtps_epi32(_mm_mul_ps(fog_r, t));
          const __m128i green = _mm_cvtps_epi32(_mm_mul_ps(fog_g, t));
          const __m128i blue = _mm_cvtps_epi32(_mm_mul_ps(fog_b, t));
          const __m128i rgb = _mm_or_si128(
              _mm_slli_epi32(_mm_or_si128(_mm_slli_epi32(red, 8), green), 8), blue);
          _mm_storeu_si128(reinterpret_cast<__m128i*>(fog), rgb);
          int32_t parts[4];
          _mm_storeu_si128(reinterpret_cast<__m128i*>(parts), part);
          for (int32_t i = 0; i < 4; ++i) {
            dim[i] = 0x100u - static_cast<uint32_t>(parts[i]);
          }
        }
        for (int32_t i = 0; i < 4; ++i) {
          specular[i] = detail::AddFog(specular[i], fog[i]);
          if (dim[i] < 0x100u) diffuse[i] = detail::Dim(diffuse[i], dim[i]);
        }
      }

      // The projection.
      const __m128 rhw = _mm_div_ps(one, W);
      // A value below zero, or a NaN, becomes the bits 0. Else a value above
      // the max becomes the max.
      __m128 sx = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(rhw, X), one), half_x);
      const __m128 sx_low = _mm_cmpnge_ps(sx, zero);
      sx = _mm_andnot_ps(sx_low, detail::Blend(_mm_cmpgt_ps(sx, max_x), max_x, sx));
      __m128 sy = _mm_sub_ps(half_y, _mm_mul_ps(_mm_mul_ps(rhw, Y), half_y));
      const __m128 sy_low = _mm_cmpnge_ps(sy, zero);
      sy = _mm_andnot_ps(sy_low, detail::Blend(_mm_cmpgt_ps(sy, max_y), max_y, sy));
      const __m128 q = _mm_mul_ps(near_plane, rhw);
      const __m128 sz = _mm_sub_ps(one, q);

      uint8_t* o = out + n * 32;
      __m128 keep = _mm_setzero_ps();  // the lanes with a clip flag
      if (p.clip != 0) {
        const __m128 neg = _mm_xor_ps(W, sign);
        const __m128i f_near = _mm_and_si128(_mm_castps_si128(_mm_cmpnge_ps(W, near_plane)),
                                             _mm_set1_epi32(0x20));
        const __m128 right = _mm_cmpgt_ps(X, W);
        const __m128 left = _mm_andnot_ps(right, _mm_cmpgt_ps(neg, X));
        const __m128 top = _mm_cmpgt_ps(Y, W);
        const __m128 bottom = _mm_andnot_ps(top, _mm_cmpgt_ps(neg, Y));
        __m128i f = f_near;
        f = _mm_or_si128(f, _mm_and_si128(_mm_castps_si128(right), _mm_set1_epi32(0x10)));
        f = _mm_or_si128(f, _mm_and_si128(_mm_castps_si128(left), _mm_set1_epi32(8)));
        f = _mm_or_si128(f, _mm_and_si128(_mm_castps_si128(top), _mm_set1_epi32(4)));
        f = _mm_or_si128(f, _mm_and_si128(_mm_castps_si128(bottom), _mm_set1_epi32(2)));
        keep = _mm_castsi128_ps(_mm_xor_si128(_mm_cmpeq_epi32(f, _mm_setzero_si128()),
                                              _mm_set1_epi32(-1)));
        uint32_t fl[4];
        _mm_storeu_si128(reinterpret_cast<__m128i*>(fl), f);
        for (int32_t i = 0; i < lanes; ++i) flags[n + i] = fl[i];
      }
      float old_z[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      for (int32_t i = 0; i < lanes; ++i) memcpy(&old_z[i], o + i * 32 + 8, 4);
      __m128 v0 = detail::Blend(keep, X, sx);
      __m128 v1 = detail::Blend(keep, Y, sy);
      __m128 v2 = detail::Blend(keep, _mm_loadu_ps(old_z), sz);
      __m128 v3 = detail::Blend(keep, W, q);
      __m128 v4 = _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(diffuse)));
      __m128 v5 = _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(specular)));
      __m128 v6 = _mm_loadu_ps(&us[j0]);
      __m128 v7 = _mm_set1_ps(vf);
      _MM_TRANSPOSE4_PS(v0, v1, v2, v3);
      _MM_TRANSPOSE4_PS(v4, v5, v6, v7);
      const __m128 lo[4] = {v0, v1, v2, v3};
      const __m128 up[4] = {v4, v5, v6, v7};
      for (int32_t i = 0; i < lanes; ++i) {
        _mm_storeu_ps(reinterpret_cast<float*>(o + i * 32), lo[i]);
        _mm_storeu_ps(reinterpret_cast<float*>(o + i * 32 + 16), up[i]);
      }
      n += static_cast<uint32_t>(lanes);
    }
    row_cells = cell + p.row_skip * 8;
    r = _mm_add_ss(r, d);
    v = _mm_add_ss(du, v);
  }
  if (cells_end != nullptr) *cells_end = row_cells;
  return n;
}

}  // namespace land_fast_math
