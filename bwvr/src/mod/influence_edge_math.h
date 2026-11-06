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

// THE WALL. A strip of light stands on the edge: one quad between each two
// rays that are next to each other, when both found the edge and the two
// points are less than `join` apart. It goes up from the land at each edge
// point by `height`. The light of a corner is `light` times:
//
//   close   1 at the hand, 0 at `range` from it along the land, squared
//   bottom  1 at the land and 0 at the top
//   strength  1 - (the distance of the hand from the nearest edge point) /
//             range, so the wall grows while the hand comes to the edge
//
// `ground` gives the height of the land at x, z. Returns the count of
// triangles, at most `room`.
struct WallTriangle {
  float p[3][3];
  float light[3];
};

inline float Clamp01(float v) {
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

inline float Strength(const float hand[3], const bool found[],
                      const float edge[][2], int directions, float range) {
  if (!(range > 0.0f)) return 0.0f;
  float nearest = -1.0f;
  for (int k = 0; k < directions; ++k) {
    if (!found[k]) continue;
    const float dx = edge[k][0] - hand[0], dz = edge[k][1] - hand[2];
    const float d = sqrtf(dx * dx + dz * dz);
    if (nearest < 0.0f || d < nearest) nearest = d;
  }
  if (nearest < 0.0f) return 0.0f;
  return Clamp01(1.0f - nearest / range);
}

template <typename Ground>
int BuildWall(Ground ground, const float hand[3], const bool found[],
              const float edge[][2], int directions, float range,
              float height, float join, float light, WallTriangle* out,
              int room) {
  const float strength = Strength(hand, found, edge, directions, range);
  if (!(strength > 0.0f) || directions < 3) return 0;
  int n = 0;
  for (int k = 0; k < directions; ++k) {
    const int j = (k + 1) % directions;
    if (!found[k] || !found[j]) continue;
    const float dx = edge[j][0] - edge[k][0], dz = edge[j][1] - edge[k][1];
    if (!(sqrtf(dx * dx + dz * dz) < join)) continue;
    if (n + 2 > room) break;
    float corner[4][3];
    float level[4];
    const int ends[2] = {k, j};
    for (int e = 0; e < 2; ++e) {
      const float* at = edge[ends[e]];
      const float y = ground(at[0], at[1]);
      const float hx = at[0] - hand[0], hz = at[1] - hand[2];
      float close = Clamp01(1.0f - sqrtf(hx * hx + hz * hz) / range);
      close *= close;
      // e 0 and 1 are the bottom corners, 2 and 3 the top corners.
      corner[e][0] = at[0];
      corner[e][1] = y;
      corner[e][2] = at[1];
      corner[e + 2][0] = at[0];
      corner[e + 2][1] = y + height;
      corner[e + 2][2] = at[1];
      level[e] = light * close * strength;
      level[e + 2] = 0.0f;
    }
    const int tri[2][3] = {{0, 1, 3}, {0, 3, 2}};
    for (int t = 0; t < 2; ++t) {
      WallTriangle& w = out[n++];
      for (int v = 0; v < 3; ++v) {
        for (int c = 0; c < 3; ++c) w.p[v][c] = corner[tri[t][v]][c];
        w.light[v] = level[tri[t][v]];
      }
    }
  }
  return n;
}

}  // namespace influence_math
