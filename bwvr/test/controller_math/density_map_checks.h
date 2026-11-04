// The fragment density map of the one pass, a test (density_map_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <stdlib.h>
#include <vector>

#include "../../src/mod/density_map_math.h"

namespace density_map_checks {

inline void DensityMapChecks() {
  using namespace density_map_math;
  printf("\nThe one pass: the fragment density map of an eye picture\n");

  // The render of the Frame: 2808 by 2246 pixels, a view of 110 by 100
  // degrees, texels of 32 by 32 pixels.
  Params p;
  p.width = 2808;
  p.height = 2246;
  p.tan_h = tanf(55.0f / 57.2957795f);
  p.tan_v = tanf(50.0f / 57.2957795f);
  p.center_deg = 20.0f;
  p.ring_deg = 8.0f;
  p.outer_pct = 25.0f;

  // The size of the map.
  uint32_t tw = 0, th = 0;
  TexelSize(32, 32, &tw, &th);
  Check(tw == 32 && th == 32, "the texel is the finest the GPU has", tw, 32);
  const uint32_t mw = TexelCount(p.width, tw), mh = TexelCount(p.height, th);
  Check(mw == 88 && mh == 71, "2808 by 2246 pixels need 88 by 71 texels of 32 pixels", mw, 88);
  Check(TexelCount(64, 32) == 2 && TexelCount(65, 32) == 3,
        "a partial texel counts", TexelCount(65, 32), 3);
  Check(TexelCount(100, 0) == 0, "a texel of 0 pixels gives no count", 0, 0);

  // The levels.
  const Levels l = LevelsOf(p);
  Check(l.full == 255, "the full density is 255", l.full, 255);
  Check(l.outer == 64, "25 percent is the byte 64 (4 by 4 pixels for a fragment)", l.outer, 64);
  Check(l.ring == 128 || l.ring == 127,
        "the ring of an outer density of 25 percent is 50 percent (2 by 2)", l.ring, 128);
  Near("the ring is the geometric mean of 100 and 25", RingPct(25.0f), 50.0, 1e-4);
  Check(RingPct(100.0f) == 100.0f, "an outer density of 100 percent has a ring of 100", 1, 1);
  Check(LevelsOf([&] { Params q = p; q.ring_deg = 0.0f; return q; }()).ring == l.outer,
        "with no ring the ring byte is the outer byte", 1, 1);
  Check(ByteOf(0.0f) >= 1 && ByteOf(500.0f) == 255, "a byte stays in 1 to 255", ByteOf(500.0f), 255);

  // The rects: straight ahead the rects are in the middle and symmetric.
  Rects r = RectsOf(p);
  Near("the full rect is centred across", (r.full.x0 + r.full.x1) * 0.5, 1404.0, 1.5);
  Near("the full rect is centred down", (r.full.y0 + r.full.y1) * 0.5, 1123.0, 1.5);
  const double half = tan(20.0 / 57.2957795) / p.tan_h * 0.5 * 2808.0;
  Near("the full rect is 20 degrees on each side across", (r.full.x1 - r.full.x0) * 0.5, half, 1.0);
  Check(r.ring.x0 < r.full.x0 && r.ring.x1 > r.full.x1 && r.ring.y0 < r.full.y0 &&
            r.ring.y1 > r.full.y1,
        "the ring rect is larger than the full rect on each side", 1, 1);
  Near("the ring is 8 more degrees on the left",
       r.full.x0 - r.ring.x0,
       (tan(28.0 / 57.2957795) - tan(20.0 / 57.2957795)) / p.tan_h * 0.5 * 2808.0, 1.0);

  // The map: one layer, two bytes for each texel.
  std::vector<uint8_t> map(static_cast<size_t>(mw) * mh * 2);
  Build(map.data(), mw, mh, tw, th, p);
  auto at = [&](uint32_t tx, uint32_t ty) { return map[(static_cast<size_t>(ty) * mw + tx) * 2]; };
  Check(at(44, 35) == 255, "the middle of the map has the full density", at(44, 35), 255);
  Check(at(0, 0) == 64 && at(87, 70) == 64 && at(0, 70) == 64 && at(87, 0) == 64,
        "each corner has the outer density", at(0, 0), 64);
  Check(map[(35 * mw + 44) * 2] == map[(35 * mw + 44) * 2 + 1],
        "the density across and down are equal", 1, 1);
  // A row through the middle goes full, ring, outer on each side.
  int full = 0, ring = 0, outer = 0, other = 0;
  for (uint32_t tx = 0; tx < mw; ++tx) {
    const uint8_t b = at(tx, 35);
    if (b == l.full) ++full;
    else if (b == l.ring) ++ring;
    else if (b == l.outer) ++outer;
    else ++other;
  }
  Check(other == 0, "a row has only the three densities", other, 0);
  Check(full > 0 && ring > 0 && outer > 0, "a row through the middle has all three", full, 1);
  // The row is symmetric about the middle (the rects are symmetric).
  bool symmetric = true;
  for (uint32_t tx = 0; tx < mw; ++tx) {
    if (at(tx, 35) != at(mw - 1 - tx, 35)) symmetric = false;
  }
  Check(symmetric, "a row through the middle is symmetric", symmetric ? 1 : 0, 1);
  // Walking from the middle to the left edge: full first, then ring, then outer, never back.
  int phase = 0;
  bool order = true;
  for (int tx = 44; tx >= 0; --tx) {
    const uint8_t b = at(static_cast<uint32_t>(tx), 35);
    const int now = b == l.full ? 0 : b == l.ring ? 1 : 2;
    if (now < phase) order = false;
    phase = now;
  }
  Check(order, "from the middle to the edge the density only goes down", order ? 1 : 0, 1);

  // A texel that touches the full rect has the full density.
  bool touches_ok = true;
  for (uint32_t ty = 0; ty < mh; ++ty) {
    for (uint32_t tx = 0; tx < mw; ++tx) {
      const float x0 = static_cast<float>(tx * tw), x1 = static_cast<float>((tx + 1) * tw);
      const float y0 = static_cast<float>(ty * th), y1 = static_cast<float>((ty + 1) * th);
      if (Touches(r.full, x0, y0, x1, y1) && at(tx, ty) != 255) touches_ok = false;
      if (!Touches(r.ring, x0, y0, x1, y1) && at(tx, ty) != l.outer) touches_ok = false;
    }
  }
  Check(touches_ok, "a texel that touches the full rect is full, one outside the ring is outer",
        touches_ok ? 1 : 0, 1);

  // The share of the fragments: the full rect plus the ring plus the outer part.
  const float share = FragmentShare(map.data(), mw, mh, tw, th, p.width, p.height);
  Check(share > 0.10f && share < 0.45f, "the fragments are a part of the picture", share * 100.0, 25.0);

  // The gaze moves the rects and keeps them in the picture.
  Params g = p;
  g.gaze_x = 0.35f;
  g.gaze_y = -0.2f;
  const Rects rg = RectsOf(g);
  Check(rg.full.x0 > r.full.x0 && rg.full.y0 > r.full.y0,
        "a gaze to the right and down moves the full rect that way", 1, 1);
  Near("the rect keeps its size", (rg.full.x1 - rg.full.x0), (r.full.x1 - r.full.x0), 1.5);
  g.gaze_x = 5.0f;
  g.gaze_y = 5.0f;
  const Rects re = RectsOf(g);
  Check(re.full.x1 <= 2808.5f && re.full.x0 >= -0.5f && re.full.y0 >= -0.5f &&
            re.full.y1 <= 2246.5f,
        "a gaze far outside keeps the full rect in the picture", 1, 1);
  Check(re.ring.x1 <= 2808.0f && re.ring.y0 >= 0.0f, "and the ring too", 1, 1);

  // The key: the same texels give the same key, a gaze that moves a texel does not.
  const Key k0 = KeyOf(p, tw, th);
  Params nudge = p;
  nudge.gaze_x = 0.0001f;
  Check(KeyOf(nudge, tw, th) == k0, "a gaze that moves less than a texel changes no key", 1, 1);
  Params moved = p;
  moved.gaze_x = 0.1f;
  Check(KeyOf(moved, tw, th) != k0, "a gaze that moves the rect to other texels changes the key", 1, 1);
  Params other_pct = p;
  other_pct.outer_pct = 50.0f;
  Check(KeyOf(other_pct, tw, th) != k0, "another outer density changes the key", 1, 1);
  Check(k0.map_w == 88 && k0.map_h == 71, "the key has the size of the map", k0.map_w, 88);

  // An outer density of 100 percent is a map with the full density everywhere.
  Params all = p;
  all.outer_pct = 100.0f;
  std::vector<uint8_t> flat(static_cast<size_t>(mw) * mh * 2);
  Build(flat.data(), mw, mh, tw, th, all);
  bool all_full = true;
  for (size_t i = 0; i < flat.size(); ++i) {
    if (flat[i] != 255) all_full = false;
  }
  Check(all_full, "an outer density of 100 percent gives the full density everywhere",
        all_full ? 1 : 0, 1);
  Near("and a share of 1",
       FragmentShare(flat.data(), mw, mh, tw, th, p.width, p.height), 1.0, 1e-6);

  // A render that is not a multiple of the texel: the last texels cover the edge.
  Params odd = p;
  odd.width = 1000;
  odd.height = 700;
  const uint32_t ow = TexelCount(odd.width, tw), oh = TexelCount(odd.height, th);
  std::vector<uint8_t> small_map(static_cast<size_t>(ow) * oh * 2);
  Build(small_map.data(), ow, oh, tw, th, odd);
  Check(ow == 32 && oh == 22, "1000 by 700 pixels need 32 by 22 texels", ow, 32);
  Check(small_map[((oh - 1) * ow + (ow - 1)) * 2] == LevelsOf(odd).outer ||
            small_map[((oh - 1) * ow + (ow - 1)) * 2] == LevelsOf(odd).ring ||
            small_map[((oh - 1) * ow + (ow - 1)) * 2] == 255,
        "the last texel has a density", 1, 1);
}

}  // namespace density_map_checks
