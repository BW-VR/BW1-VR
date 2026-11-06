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

}  // namespace influence_math
