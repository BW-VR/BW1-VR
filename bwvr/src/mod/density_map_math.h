// The fragment density map of the one pass (xr_density_pass=1, vk_scene.cpp,
// docs/backend.md, "The one pass with a density map"). Pure: no Windows, no
// Vulkan. Checks in test/controller_math/density_map_checks.h.
//
// The pass draws the walk one time into the eye picture. A map with one
// texel for each block of texel_w by texel_h pixels (VK_EXT_fragment_density_map,
// R8G8_UNORM, a layer for each eye) tells the GPU how many fragments each
// block gets: the full density in a rect around the centre of the fovea, a
// ring of about the geometric mean of the full density and the outer density
// around that rect, and the outer density (fovea_res_outer_pct) everywhere
// else. The rect is the one of fovea_res (fovea_res_math::MakeLayout), so the
// old keys (fovea_res_center_deg, fovea_res_outer_pct) set the map too.
//
// A block that touches a rect takes the density of that rect, so no part of
// the middle gets less than its density. The values are the density along
// each axis: 255 is 1 by 1 pixel for each fragment. A value a little below a
// half (127) and a quarter (63) gives 2 by 2 and 4 by 4, because a driver that
// asks for at least the density of the map takes the next size up from a value
// that is a little above. The driver rounds a value to a size of fragment
// that it has. A GPU with tiles (Turnip) gives one fragment size to each bin,
// so the shape of the sizes on the picture can differ from the map: what the
// map asks for is open until a headset shows what the GPU shades.
#pragma once

#include <math.h>
#include <stdint.h>

#include "fovea_res_math.h"

namespace density_map_math {

// The inputs of a map: the eye picture, its view (tan of the half angles),
// the centre of the fovea as tangents (x right, y up), the half size of the
// full rect in degrees, the width of the ring in degrees (0: no ring) and the
// outer density in percent (5 to 100).
struct Params {
  uint32_t width = 0, height = 0;
  float tan_h = 0.0f, tan_v = 0.0f;
  float gaze_x = 0.0f, gaze_y = 0.0f;
  float center_deg = 20.0f;
  float ring_deg = 0.0f;
  float outer_pct = 25.0f;
};

// The three densities as bytes of a UNORM value.
struct Levels {
  uint8_t full, ring, outer;
};

// The byte rounds down, so 25 percent is 63 and 50 percent is 127.
inline uint8_t ByteOf(float pct) {
  if (pct < 1.0f) pct = 1.0f;
  if (pct >= 100.0f) return 255;
  return static_cast<uint8_t>(pct * 2.55f);
}

// The ring is the geometric mean of the full density and the outer density,
// 50 percent for an outer density of 25. It is never below the outer density.
inline float RingPct(float outer_pct) {
  if (outer_pct >= 100.0f) return 100.0f;
  if (outer_pct < 1.0f) outer_pct = 1.0f;
  return sqrtf(100.0f * outer_pct);
}

inline Levels LevelsOf(const Params& p) {
  Levels l;
  l.full = 255;
  l.outer = ByteOf(p.outer_pct);
  l.ring = p.ring_deg > 0.0f ? ByteOf(RingPct(p.outer_pct)) : l.outer;
  return l;
}

// The count of texels of a size of picture, rounded up.
inline uint32_t TexelCount(uint32_t pixels, uint32_t texel) {
  return texel == 0 ? 0 : (pixels + texel - 1) / texel;
}

// The size of a texel: the finest the GPU has. A map of that texel size is
// never smaller than the picture over the largest texel.
inline void TexelSize(uint32_t min_w, uint32_t min_h, uint32_t* w, uint32_t* h) {
  *w = min_w == 0 ? 1 : min_w;
  *h = min_h == 0 ? 1 : min_h;
}

// A rect of pixels: x0, y0, x1, y1.
struct Rect {
  float x0, y0, x1, y1;
};

struct Rects {
  Rect full;  // the rect of fovea_res, kept in the picture
  Rect ring;  // the same rect with the ring around it, kept in the picture
};

inline Rects RectsOf(const Params& p) {
  const fovea_res_math::Layout l = fovea_res_math::MakeLayout(
      p.width, p.height, p.tan_h, p.tan_v, p.gaze_x, p.gaze_y, 100.0f, 100.0f, p.center_deg);
  const float w = static_cast<float>(p.width), h = static_cast<float>(p.height);
  Rects r;
  r.full = {l.inset_x0, l.inset_y0, l.inset_x0 + l.inset_w, l.inset_y0 + l.inset_h};
  r.ring = r.full;
  if (p.ring_deg > 0.0f) {
    // The ring is ring_deg wider on each side, as the angle from the centre.
    const float a = p.center_deg / 57.2957795f;
    const float b = (p.center_deg + p.ring_deg) / 57.2957795f;
    const float more_x = (tanf(b > 1.5f ? 1.5f : b) - tanf(a)) / p.tan_h * 0.5f * w;
    const float more_y = (tanf(b > 1.5f ? 1.5f : b) - tanf(a)) / p.tan_v * 0.5f * h;
    r.ring = {r.full.x0 - more_x, r.full.y0 - more_y, r.full.x1 + more_x, r.full.y1 + more_y};
    if (r.ring.x0 < 0.0f) r.ring.x0 = 0.0f;
    if (r.ring.y0 < 0.0f) r.ring.y0 = 0.0f;
    if (r.ring.x1 > w) r.ring.x1 = w;
    if (r.ring.y1 > h) r.ring.y1 = h;
  }
  return r;
}

inline bool Touches(const Rect& r, float x0, float y0, float x1, float y1) {
  return x0 < r.x1 && x1 > r.x0 && y0 < r.y1 && y1 > r.y0;
}

// The density byte of the texel (tx, ty).
inline uint8_t TexelByte(const Rects& r, const Levels& l, uint32_t tx, uint32_t ty,
                         uint32_t texel_w, uint32_t texel_h) {
  const float x0 = static_cast<float>(tx * texel_w), x1 = static_cast<float>((tx + 1) * texel_w);
  const float y0 = static_cast<float>(ty * texel_h), y1 = static_cast<float>((ty + 1) * texel_h);
  if (Touches(r.full, x0, y0, x1, y1)) return l.full;
  if (Touches(r.ring, x0, y0, x1, y1)) return l.ring;
  return l.outer;
}

// The map of one layer: two bytes for each texel (the density across and the
// density down, equal), map_w by map_h texels, row after row. out has
// map_w * map_h * 2 bytes.
inline void Build(uint8_t* out, uint32_t map_w, uint32_t map_h, uint32_t texel_w,
                  uint32_t texel_h, const Params& p) {
  const Rects r = RectsOf(p);
  const Levels l = LevelsOf(p);
  for (uint32_t ty = 0; ty < map_h; ++ty) {
    for (uint32_t tx = 0; tx < map_w; ++tx) {
      const uint8_t b = TexelByte(r, l, tx, ty, texel_w, texel_h);
      uint8_t* at = out + (static_cast<size_t>(ty) * map_w + tx) * 2;
      at[0] = b;
      at[1] = b;
    }
  }
}

// The map changes only when a rect moves to other texels or a level changes.
// This key has the rects in texels (rounded outward) and the levels, so two
// walks with the same key need the same map.
struct Key {
  int32_t full[4];
  int32_t ring[4];
  uint8_t levels[3];
  uint32_t map_w, map_h;
  bool operator==(const Key& o) const {
    for (int i = 0; i < 4; ++i) {
      if (full[i] != o.full[i] || ring[i] != o.ring[i]) return false;
    }
    for (int i = 0; i < 3; ++i) {
      if (levels[i] != o.levels[i]) return false;
    }
    return map_w == o.map_w && map_h == o.map_h;
  }
  bool operator!=(const Key& o) const { return !(*this == o); }
};

inline Key KeyOf(const Params& p, uint32_t texel_w, uint32_t texel_h) {
  const Rects r = RectsOf(p);
  const Levels l = LevelsOf(p);
  Key k = {};
  auto texels = [&](const Rect& a, int32_t out[4]) {
    out[0] = static_cast<int32_t>(floorf(a.x0 / static_cast<float>(texel_w)));
    out[1] = static_cast<int32_t>(floorf(a.y0 / static_cast<float>(texel_h)));
    out[2] = static_cast<int32_t>(ceilf(a.x1 / static_cast<float>(texel_w)));
    out[3] = static_cast<int32_t>(ceilf(a.y1 / static_cast<float>(texel_h)));
  };
  texels(r.full, k.full);
  texels(r.ring, k.ring);
  k.levels[0] = l.full;
  k.levels[1] = l.ring;
  k.levels[2] = l.outer;
  k.map_w = TexelCount(p.width, texel_w);
  k.map_h = TexelCount(p.height, texel_h);
  return k;
}

// The share of the fragments that a map asks for, against a map with the full
// density everywhere (0 to 1): a block with the density d along each axis asks
// for d * d of its fragments. For the log. A GPU with tiles gives one fragment
// size to each bin, so the shape of the sizes can differ from the map, and the
// share that it shades is not this value.
inline float FragmentShare(const uint8_t* map, uint32_t map_w, uint32_t map_h, uint32_t texel_w,
                           uint32_t texel_h, uint32_t width, uint32_t height) {
  double sum = 0.0;
  for (uint32_t ty = 0; ty < map_h; ++ty) {
    const uint32_t y1 = (ty + 1) * texel_h > height ? height : (ty + 1) * texel_h;
    const uint32_t rows = y1 > ty * texel_h ? y1 - ty * texel_h : 0;
    for (uint32_t tx = 0; tx < map_w; ++tx) {
      const uint32_t x1 = (tx + 1) * texel_w > width ? width : (tx + 1) * texel_w;
      const uint32_t cols = x1 > tx * texel_w ? x1 - tx * texel_w : 0;
      const double d = map[(static_cast<size_t>(ty) * map_w + tx) * 2] / 255.0;
      sum += d * d * static_cast<double>(rows) * static_cast<double>(cols);
    }
  }
  const double all = static_cast<double>(width) * static_cast<double>(height);
  return all > 0.0 ? static_cast<float>(sum / all) : 0.0f;
}

}  // namespace density_map_math
