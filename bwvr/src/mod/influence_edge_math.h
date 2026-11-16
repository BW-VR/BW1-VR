// The influence at the hand (docs/miracles.md, "The influence at the hand"):
// the pure math. No game code here. The caller gives the test of the game
// (inside the influence of the player at a land point) as a function.
#pragma once

#include <math.h>

namespace influence_math {

// THE FADE. The opacity of a hand goes to 1 inside the influence and to
// `outside` outside it, in a straight line that takes `seconds` for the
// whole way. A frame longer than 0.1 s counts as 0.1 s.
inline float FadeStep(float now, bool inside, float outside, float seconds,
                      float dt) {
  if (!(dt > 0.0f)) return now;
  if (dt > 0.1f) dt = 0.1f;
  const float want = inside ? 1.0f : outside;
  const float span = 1.0f - outside;
  if (!(seconds > 0.0f) || !(span > 0.0f)) return want;
  const float step = span * dt / seconds;
  if (now < want) return now + step > want ? want : now + step;
  return now - step < want ? want : now - step;
}

// THE EDGE NEAR THE HAND. `directions` rays go out from the land point x, z
// of the hand, at equal angles, to `range` world units. Each ray finds the
// first point where the influence changes, in `steps` equal steps and then
// `halvings` halvings of the last step. found[k] says if ray k found the
// edge, and edge[k] is the point. Returns the count of rays that found it.
//
// A first test of the points at the end of each ray and at half of it says
// if an edge is near at all. The edge of a town is a line of some hundred
// units, and the range is some ten units, so a test with no change there
// ends the search: most frames cost 2 * directions tests.
const int kMaxDirections = 32;
template <typename Inside>
int FindEdge(Inside inside, float x, float z, float range, int directions,
             int steps, int halvings, bool found[], float edge[][2]) {
  if (directions > kMaxDirections) directions = kMaxDirections;
  if (directions < 3 || !(range > 0.0f) || steps < 1) return 0;
  const bool here = inside(x, z);
  const float kTwoPi = 6.2831853f;
  bool near_edge = false;
  for (int k = 0; k < directions && !near_edge; ++k) {
    const float a = kTwoPi * static_cast<float>(k) /
                    static_cast<float>(directions);
    const float c = cosf(a), s = sinf(a);
    if (inside(x + c * range, z + s * range) != here ||
        inside(x + c * range * 0.5f, z + s * range * 0.5f) != here) {
      near_edge = true;
    }
  }
  for (int k = 0; k < directions; ++k) found[k] = false;
  if (!near_edge) return 0;
  int count = 0;
  for (int k = 0; k < directions; ++k) {
    const float a = kTwoPi * static_cast<float>(k) /
                    static_cast<float>(directions);
    const float c = cosf(a), s = sinf(a);
    float before = 0.0f;
    for (int i = 1; i <= steps; ++i) {
      const float r = range * static_cast<float>(i) /
                      static_cast<float>(steps);
      if (inside(x + c * r, z + s * r) == here) {
        before = r;
        continue;
      }
      float lo = before, hi = r;
      for (int h = 0; h < halvings; ++h) {
        const float mid = 0.5f * (lo + hi);
        if (inside(x + c * mid, z + s * mid) == here) {
          lo = mid;
        } else {
          hi = mid;
        }
      }
      const float at = 0.5f * (lo + hi);
      edge[k][0] = x + c * at;
      edge[k][1] = z + s * at;
      found[k] = true;
      ++count;
      break;
    }
  }
  return count;
}

// THE GLOW AT THE EDGE. A soft round patch of light lies on the edge at the
// point of the edge nearest to the hand, at the height of the hand, so the
// player sees where the hand meets the limit.
struct WallTriangle {
  float p[3][3];
  float light[3];
};

inline float Clamp01(float v) {
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// The edge point nearest to the hand among the rays that found the edge, and
// its distance along the land. False when no ray found it.
inline bool NearestEdge(const float hand[3], const bool found[],
                        const float edge[][2], int directions, float out[2],
                        float* distance) {
  float nearest = -1.0f;
  for (int k = 0; k < directions; ++k) {
    if (!found[k]) continue;
    const float dx = edge[k][0] - hand[0], dz = edge[k][1] - hand[2];
    const float d = sqrtf(dx * dx + dz * dz);
    if (nearest < 0.0f || d < nearest) {
      nearest = d;
      out[0] = edge[k][0];
      out[1] = edge[k][1];
    }
  }
  if (nearest < 0.0f) return false;
  *distance = nearest;
  return true;
}

// The strength of the glow, 0 to 1, for a hand `distance` from the edge,
// inside or outside the influence:
//
//   inside   0 at `range` and 1 at the edge, along t * t * (3 - 2 * t) with
//            t = 1 - distance / range. So the start is faint: a hand at a
//            quarter of the range from its start has 0.16
//   outside  1 at the edge, down to `outside_share` in `touch`, and then
//            down to 0 at `outside_range`: the faint patch where the hand
//            went through, which shows the way back
inline float GlowStrength(bool inside, float distance, float range,
                          float touch, float outside_range,
                          float outside_share) {
  if (!(distance >= 0.0f)) distance = 0.0f;
  if (inside) {
    if (!(range > 0.0f)) return 0.0f;
    const float t = Clamp01(1.0f - distance / range);
    return t * t * (3.0f - 2.0f * t);
  }
  const float near_touch =
      touch > 0.0f ? Clamp01(1.0f - distance / touch) : 0.0f;
  const float faint = outside_range > 0.0f
                          ? outside_share *
                                Clamp01(1.0f - distance / outside_range)
                          : 0.0f;
  return near_touch > faint ? near_touch : faint;
}

// The patch: a disc of `radius` in the vertical plane through the edge point
// `at` (x and z), with its middle at the height `y`. The plane faces the hand
// at x and z of `hand`. The light is `light` in the middle, 0.55 of it at half
// the radius and 0 at the rim, so the patch is soft. Returns the triangles,
// at most `room`: kPatchTriangles for a patch.
const int kPatchSides = 16;
const int kPatchTriangles = kPatchSides * 3;
inline int BuildPatch(const float at[2], float y, const float hand[3],
                      float radius, float light, WallTriangle* out,
                      int room) {
  if (room < kPatchTriangles || !(radius > 0.0f) || !(light > 0.0f)) return 0;
  float nx = at[0] - hand[0], nz = at[1] - hand[2];
  float length = sqrtf(nx * nx + nz * nz);
  if (!(length > 1e-6f)) {
    nx = 1.0f;
    nz = 0.0f;
    length = 1.0f;
  }
  // Along the wall, level.
  const float ux = -nz / length, uz = nx / length;
  const float kTwoPi = 6.2831853f;
  const float levels[3] = {light, 0.55f * light, 0.0f};
  const float radii[3] = {0.0f, 0.5f * radius, radius};
  float ring[3][kPatchSides][3];
  for (int r = 0; r < 3; ++r) {
    for (int k = 0; k < kPatchSides; ++k) {
      const float a = kTwoPi * static_cast<float>(k) /
                      static_cast<float>(kPatchSides);
      const float c = cosf(a) * radii[r], s = sinf(a) * radii[r];
      ring[r][k][0] = at[0] + ux * c;
      ring[r][k][1] = y + s;
      ring[r][k][2] = at[1] + uz * c;
    }
  }
  int n = 0;
  const auto add = [&](const float* a, float la, const float* b, float lb,
                       const float* c, float lc) {
    WallTriangle& w = out[n++];
    for (int i = 0; i < 3; ++i) {
      w.p[0][i] = a[i];
      w.p[1][i] = b[i];
      w.p[2][i] = c[i];
    }
    w.light[0] = la;
    w.light[1] = lb;
    w.light[2] = lc;
  };
  for (int k = 0; k < kPatchSides; ++k) {
    const int j = (k + 1) % kPatchSides;
    add(ring[0][0], levels[0], ring[1][k], levels[1], ring[1][j], levels[1]);
    add(ring[1][k], levels[1], ring[2][k], levels[2], ring[2][j], levels[2]);
    add(ring[1][k], levels[1], ring[2][j], levels[2], ring[1][j], levels[1]);
  }
  return n;
}

// THE HOLOGRAM (influence_wall_look=1). The influence of a player is the union
// of discs on the land: the disc of the Temple (Citadel) and one disc for each
// town of the player [V, Influence::CalculatePlayerRawInfluence 0x005CD230].
// The game draws the same discs as its influence ring (InfluenceCircle). So
// the edge is a set of arcs, and the wall of the edge is a set of vertical
// cylinders. The hologram lies on that wall, near the hand: a grid that is
// fixed to the wall, and rings that go out from the point of the wall that is
// nearest to the hand.
struct Circle {
  float x, z, r;  // the middle on the land and the radius, in world units
};

// True when x, z is in circle c by more than `margin` units.
inline bool InCircle(const Circle& c, float x, float z, float margin) {
  const float dx = x - c.x, dz = z - c.z;
  const float r = c.r - margin;
  return r > 0.0f && dx * dx + dz * dz < r * r;
}

// True when x, z is in one of the n circles but `skip`, by more than
// `margin` units.
inline bool InOther(const Circle* c, int n, int skip, float x, float z,
                    float margin) {
  for (int j = 0; j < n; ++j) {
    if (j != skip && InCircle(c[j], x, z, margin)) return true;
  }
  return false;
}

// The point of the edge of the union nearest to the land point x, z of the
// hand. The edge is the arcs of the circles that are not in another circle,
// so the nearest point is the nearest of: the point of each circle straight
// out from its middle through the hand, when no other circle holds it, and
// each point where two circles cross, when no third circle holds it.
// `circle` is the circle whose wall holds the point (for a crossing, the one
// whose rim is nearer to the hand), `inside` is true when the hand is in the
// union, and `margin` is the units that make a point "in" a circle.
struct EdgePoint {
  int circle;
  float at[2];
  float distance;
  bool inside;
};
inline bool NearestUnionEdge(const Circle* c, int n, float x, float z,
                             float margin, EdgePoint* out) {
  if (n <= 0) return false;
  bool inside = false;
  for (int i = 0; i < n; ++i) {
    if (InCircle(c[i], x, z, 0.0f)) inside = true;
  }
  float best = -1.0f;
  const auto take = [&](int i, float px, float pz) {
    const float dx = px - x, dz = pz - z;
    const float d = sqrtf(dx * dx + dz * dz);
    if (best < 0.0f || d < best) {
      best = d;
      out->circle = i;
      out->at[0] = px;
      out->at[1] = pz;
    }
  };
  for (int i = 0; i < n; ++i) {
    if (!(c[i].r > 0.0f)) continue;
    const float dx = x - c[i].x, dz = z - c[i].z;
    const float length = sqrtf(dx * dx + dz * dz);
    if (!(length > 1e-4f)) continue;
    const float px = c[i].x + dx / length * c[i].r;
    const float pz = c[i].z + dz / length * c[i].r;
    if (!InOther(c, n, i, px, pz, margin)) take(i, px, pz);
  }
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      const float dx = c[j].x - c[i].x, dz = c[j].z - c[i].z;
      const float d = sqrtf(dx * dx + dz * dz);
      if (!(d > 1e-4f) || d >= c[i].r + c[j].r ||
          d <= fabsf(c[i].r - c[j].r)) {
        continue;
      }
      const float a = (c[i].r * c[i].r - c[j].r * c[j].r + d * d) / (2.0f * d);
      const float h2 = c[i].r * c[i].r - a * a;
      if (!(h2 > 0.0f)) continue;
      const float h = sqrtf(h2);
      const float mx = c[i].x + dx / d * a, mz = c[i].z + dz / d * a;
      for (int s = -1; s <= 1; s += 2) {
        const float px = mx - dz / d * h * static_cast<float>(s);
        const float pz = mz + dx / d * h * static_cast<float>(s);
        bool held = false;
        for (int k = 0; k < n && !held; ++k) {
          if (k != i && k != j && InCircle(c[k], px, pz, margin)) held = true;
        }
        if (held) continue;
        const float ri = fabsf(sqrtf((x - c[i].x) * (x - c[i].x) +
                                     (z - c[i].z) * (z - c[i].z)) -
                               c[i].r);
        const float rj = fabsf(sqrtf((x - c[j].x) * (x - c[j].x) +
                                     (z - c[j].z) * (z - c[j].z)) -
                               c[j].r);
        take(ri <= rj ? i : j, px, pz);
      }
    }
  }
  if (best < 0.0f) return false;
  out->distance = best;
  out->inside = inside;
  return true;
}

// The strength of the hologram, 0 to 1, for a hand `distance` from the edge:
// inside the influence it rises along t * t * (3 - 2 * t) with
// t = 1 - distance / inside_range, and past the edge it falls the same way to
// 0 at outside_range.
inline float HologramStrength(bool inside, float distance, float inside_range,
                              float outside_range) {
  if (!(distance >= 0.0f)) distance = 0.0f;
  const float range = inside ? inside_range : outside_range;
  if (!(range > 0.0f)) return 0.0f;
  const float t = Clamp01(1.0f - distance / range);
  return t * t * (3.0f - 2.0f * t);
}

// The wall that holds the hologram. A circle of the influence (r above 0): the
// wall is the vertical cylinder over it. Or, with r = 0, a vertical plane
// through the edge point with the level direction tx, tz along it.
struct HoloWall {
  float cx, cz, r;
  float qx, qz;  // the edge point nearest to the hand
  float tx, tz;  // the plane only
};

// The look of the hologram, in world units:
//   y          the height of the middle (the height of the hand)
//   radius     the hologram is a disc of this radius on the wall around the
//              edge point
//   light      the light of the brightest line, 0 to 1
//   grid       the gap of the grid lines
//   ring_gap   the gap of the rings
//   ring_phase 0 to 1: how far the rings went out, in parts of the gap
//   line_half  the half width of a line
struct HoloLook {
  float y;
  float radius;
  float light;
  float grid;
  float ring_gap;
  float ring_phase;
  float line_half;
};

// The shares of `light` of each part: the soft fill of the disc, the bright
// spot at the edge point, the lines of the grid and the rings.
const float kHoloFill = 0.07f;
const float kHoloSpot = 0.55f;
const float kHoloGridLight = 0.45f;
const float kHoloRingLight = 1.0f;
const int kHoloFillSides = 16;
const int kHoloFillBands = 4;
const int kHoloRingSides = 20;
const int kHoloGridSegments = 6;
// The most triangles of one hologram. BuildHologram stops at `room`.
const int kHoloMaxTriangles = 900;

// The light of a part at the share t of the radius from the middle: full in
// the middle, soft, and 0 at the rim.
inline float HoloFalloff(float t) {
  if (!(t < 1.0f)) return 0.0f;
  if (t < 0.0f) t = 0.0f;
  const float k = 1.0f - t * t;
  return k * k;
}

// The point of the wall at s along the wall from the edge point (world units,
// level) and h over the middle.
inline void HoloPoint(const HoloWall& w, float y, float s, float h,
                      float out[3]) {
  if (w.r > 0.0f) {
    const float a = atan2f(w.qz - w.cz, w.qx - w.cx) + s / w.r;
    out[0] = w.cx + cosf(a) * w.r;
    out[2] = w.cz + sinf(a) * w.r;
  } else {
    out[0] = w.qx + w.tx * s;
    out[2] = w.qz + w.tz * s;
  }
  out[1] = y + h;
}

// The step of the upright lines of the grid along the wall, and the place of
// the edge point along the wall from the start of the steps. An upright line
// is at each whole step, s = k * step - along from the edge point. On a
// circle the step is the nearest one that goes a whole number of times round,
// so the grid has no seam, and the place counts from the angle 0 of the
// circle.
inline void HoloGridStep(const HoloWall& w, float grid, float* step,
                         float* along) {
  const float kTwoPi = 6.2831853f;
  *step = grid;
  if (w.r > 0.0f) {
    const float round = kTwoPi * w.r;
    float count = floorf(round / grid + 0.5f);
    if (count < 1.0f) count = 1.0f;
    *step = round / count;
    float a = atan2f(w.qz - w.cz, w.qx - w.cx);
    if (a < 0.0f) a += kTwoPi;
    *along = a * w.r;
  } else {
    *along = w.qx * w.tx + w.qz * w.tz;
  }
}

// THE BUILD. `show(x, z)` is false where the wall is not an edge (in another
// circle of the union): a point there has no light. A triangle with no light
// is not made. Returns the triangles in `out`, at most `room`.
template <typename Show>
int BuildHologram(const HoloWall& w, const HoloLook& look, Show show,
                  WallTriangle* out, int room) {
  const float R = look.radius;
  if (!(R > 0.0f) || !(look.light > 0.0f) || room <= 0) return 0;
  const float kTwoPi = 6.2831853f;
  int n = 0;
  // One vertex: its place on the wall and its light.
  struct V {
    float p[3];
    float l;
  };
  const auto vertex = [&](float s, float h, float light) {
    V v;
    HoloPoint(w, look.y, s, h, v.p);
    v.l = show(v.p[0], v.p[2]) ? light : 0.0f;
    return v;
  };
  const auto add = [&](const V& a, const V& b, const V& c) {
    if (n >= room) return;
    if (!(a.l > 0.0f) && !(b.l > 0.0f) && !(c.l > 0.0f)) return;
    WallTriangle& t = out[n++];
    for (int i = 0; i < 3; ++i) {
      t.p[0][i] = a.p[i];
      t.p[1][i] = b.p[i];
      t.p[2][i] = c.p[i];
    }
    t.light[0] = a.l;
    t.light[1] = b.l;
    t.light[2] = c.l;
  };
  // A soft line from (s0, h0) to (s1, h1): a strip of three vertices across,
  // with the light in the middle and none at the sides. `pieces` pieces
  // along it, each with the light of the falloff at its place.
  const auto line = [&](float s0, float h0, float s1, float h1, int pieces,
                        float share) {
    const float ds = s1 - s0, dh = h1 - h0;
    const float length = sqrtf(ds * ds + dh * dh);
    if (!(length > 1e-6f) || pieces < 1) return;
    const float ns = -dh / length * look.line_half;
    const float nh = ds / length * look.line_half;
    V last[3];
    for (int i = 0; i <= pieces; ++i) {
      const float f = static_cast<float>(i) / static_cast<float>(pieces);
      const float s = s0 + ds * f, h = h0 + dh * f;
      const float light =
          look.light * share * HoloFalloff(sqrtf(s * s + h * h) / R);
      V now[3] = {vertex(s - ns, h - nh, 0.0f), vertex(s, h, light),
                  vertex(s + ns, h + nh, 0.0f)};
      if (i > 0) {
        add(last[0], now[0], now[1]);
        add(last[0], now[1], last[1]);
        add(last[1], now[1], now[2]);
        add(last[1], now[2], last[2]);
      }
      for (int k = 0; k < 3; ++k) last[k] = now[k];
    }
  };
  // THE FILL: a disc of bands around the edge point, with the spot in the
  // middle.
  {
    V middle = vertex(0.0f, 0.0f, look.light * (kHoloFill + kHoloSpot));
    V ring[kHoloFillBands + 1][kHoloFillSides];
    for (int b = 1; b <= kHoloFillBands; ++b) {
      const float t = static_cast<float>(b) / kHoloFillBands;
      const float spot = b == 1 ? kHoloSpot * 0.25f : 0.0f;
      const float light = look.light * (kHoloFill * HoloFalloff(t) + spot);
      for (int k = 0; k < kHoloFillSides; ++k) {
        const float a = kTwoPi * static_cast<float>(k) / kHoloFillSides;
        ring[b][k] = vertex(cosf(a) * R * t, sinf(a) * R * t, light);
      }
    }
    for (int k = 0; k < kHoloFillSides; ++k) {
      const int j = (k + 1) % kHoloFillSides;
      add(middle, ring[1][k], ring[1][j]);
      for (int b = 1; b < kHoloFillBands; ++b) {
        add(ring[b][k], ring[b + 1][k], ring[b + 1][j]);
        add(ring[b][k], ring[b + 1][j], ring[b][j]);
      }
    }
  }
  // THE RINGS around the edge point. A ring comes in at the middle and gets
  // its full light at one gap out.
  if (look.ring_gap > 0.0f) {
    for (int k = 0;; ++k) {
      const float rho = (static_cast<float>(k) + look.ring_phase) *
                        look.ring_gap;
      if (rho >= R) break;
      if (!(rho > 0.0f)) continue;
      const float birth = Clamp01(rho / look.ring_gap);
      const float light =
          look.light * kHoloRingLight * HoloFalloff(rho / R) * birth;
      V last[3];
      for (int i = 0; i <= kHoloRingSides; ++i) {
        const float a = kTwoPi * static_cast<float>(i) / kHoloRingSides;
        const float c = cosf(a), s = sinf(a);
        V now[3] = {vertex(c * (rho - look.line_half),
                           s * (rho - look.line_half), 0.0f),
                    vertex(c * rho, s * rho, light),
                    vertex(c * (rho + look.line_half),
                           s * (rho + look.line_half), 0.0f)};
        if (i > 0) {
          add(last[0], now[0], now[1]);
          add(last[0], now[1], last[1]);
          add(last[1], now[1], now[2]);
          add(last[1], now[2], last[2]);
        }
        for (int m = 0; m < 3; ++m) last[m] = now[m];
      }
    }
  }
  // THE GRID, fixed to the wall: level lines at whole steps of the height of
  // the world, and upright lines at whole steps along the wall
  // (HoloGridStep). The lines do not move when the hand moves: only the disc
  // that shows them moves.
  if (look.grid > 0.0f) {
    float step = look.grid, along = 0.0f;
    HoloGridStep(w, look.grid, &step, &along);
    const float first = ceilf((along - R) / step);
    for (float k = first; k * step <= along + R; k += 1.0f) {
      const float s = k * step - along;
      const float h = sqrtf(R * R - s * s);
      if (!(h > look.line_half)) continue;
      line(s, -h, s, h, kHoloGridSegments, kHoloGridLight);
    }
    const float low = ceilf((look.y - R) / look.grid);
    for (float k = low; k * look.grid <= look.y + R; k += 1.0f) {
      const float h = k * look.grid - look.y;
      const float s = sqrtf(R * R - h * h);
      if (!(s > look.line_half)) continue;
      line(-s, h, s, h, kHoloGridSegments, kHoloGridLight);
    }
  }
  return n;
}

}  // namespace influence_math
