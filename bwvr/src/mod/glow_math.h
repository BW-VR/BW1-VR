// The math of the glow ribbons, as pure functions with no game state.
//
// overlay.cpp calls these with the lines it wants to show, and
// test/controller_math calls them with numbers it makes up. Nothing here reads
// a global or touches the game. See docs/hand-model.md, "Overlay renderer",
// "The glow ribbons".
//
// A RIBBON is a flat strip along a line of points. It faces the eye, so it
// has the same width from every side. Each point has a half width and two
// colors: the light on the middle line (core) and the light half way to each
// edge (rim). The edge itself is dark. The draw adds this light to the picture
// (src one, dest one), so the strip is a soft glow that is bright in the
// middle and has no edge. Across the strip there are kAcross corners, and
// the light between two corners is a straight line.
//
//   corner   offset   light
//   0        -1       none
//   1        -0.5     rim
//   2         0       core
//   3         0.5     rim
//   4         1       none

#pragma once

#include <math.h>

#include "overlay_math.h"

namespace glow_math {

const int kAcross = 5;
const float kOffset[kAcross] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
// Two triangles for each of the kAcross - 1 bands.
const int kTrianglesPerSegment = (kAcross - 1) * 2;
// The most points of one ribbon.
const int kMaxPoints = 128;

struct Point {
  float p[3];
  float half_width;  // in world units
  float core[3];     // the light on the middle line, 0 to 1
  float rim[3];      // the light half way to each edge, 0 to 1
};

// One triangle with a light at each corner.
struct Triangle {
  float p[3][3];
  float c[3][3];
};

// A ribbon of count points has (count - 1) * kTrianglesPerSegment triangles.
inline int RibbonTriangles(int count) {
  return count >= 2 ? (count - 1) * kTrianglesPerSegment : 0;
}

// The side axis of each point: the line there crossed with the way to the
// eye. Where the line points at the eye the axis is not known, so it takes the
// axis of the point before. An axis keeps the side of the point before, so the
// strip never twists. eye is the place of the eye in the world.
inline void Sides(const Point* points, int count, const float eye[3],
                  float side[][3]) {
  bool have = false;
  float last[3] = {0.0f, 0.0f, 0.0f};
  for (int i = 0; i < count; ++i) {
    const float* a = points[i > 0 ? i - 1 : 0].p;
    const float* b = points[i + 1 < count ? i + 1 : count - 1].p;
    float t[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    overlay_math::Normalize(t);
    const float v[3] = {eye[0] - points[i].p[0], eye[1] - points[i].p[1],
                        eye[2] - points[i].p[2]};
    float s[3];
    overlay_math::Cross(t, v, s);
    const float s2 = s[0] * s[0] + s[1] * s[1] + s[2] * s[2];
    const float v2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (s2 > 1e-8f * v2 && s2 > 1e-24f) {
      overlay_math::Normalize(s);
      if (have && s[0] * last[0] + s[1] * last[1] + s[2] * last[2] < 0.0f) {
        for (int k = 0; k < 3; ++k) s[k] = -s[k];
      }
    } else if (have) {
      for (int k = 0; k < 3; ++k) s[k] = last[k];
    } else {
      const float up[3] = {0.0f, 1.0f, 0.0f};
      overlay_math::Cross(t, up, s);
      if (s[0] * s[0] + s[1] * s[1] + s[2] * s[2] < 1e-8f) {
        const float across[3] = {1.0f, 0.0f, 0.0f};
        overlay_math::Cross(t, across, s);
      }
      overlay_math::Normalize(s);
    }
    for (int k = 0; k < 3; ++k) {
      side[i][k] = s[k];
      last[k] = s[k];
    }
    have = true;
  }
}

// The triangles of one ribbon for one eye. Returns the count written, at most
// `most`, and 0 for fewer than 2 points.
inline int BuildRibbon(const Point* points, int count, const float eye[3],
                       Triangle* out, int most) {
  if (count < 2) return 0;
  if (count > kMaxPoints) count = kMaxPoints;
  float side[kMaxPoints][3];
  Sides(points, count, eye, side);
  float corner[kMaxPoints][kAcross][3];
  float light[kMaxPoints][kAcross][3];
  for (int i = 0; i < count; ++i) {
    for (int k = 0; k < kAcross; ++k) {
      for (int c = 0; c < 3; ++c) {
        corner[i][k][c] = points[i].p[c] +
                          side[i][c] * points[i].half_width * kOffset[k];
        light[i][k][c] = k == 2 ? points[i].core[c]
                                : (k == 1 || k == 3) ? points[i].rim[c] : 0.0f;
      }
    }
  }
  int n = 0;
  for (int i = 0; i + 1 < count; ++i) {
    for (int k = 0; k + 1 < kAcross; ++k) {
      if (n + 2 > most) return n;
      const float* p0 = corner[i][k];
      const float* p1 = corner[i + 1][k];
      const float* p2 = corner[i + 1][k + 1];
      const float* p3 = corner[i][k + 1];
      const float* c0 = light[i][k];
      const float* c1 = light[i + 1][k];
      const float* c2 = light[i + 1][k + 1];
      const float* c3 = light[i][k + 1];
      Triangle& a = out[n++];
      Triangle& b = out[n++];
      for (int c = 0; c < 3; ++c) {
        a.p[0][c] = p0[c];
        a.p[1][c] = p1[c];
        a.p[2][c] = p2[c];
        a.c[0][c] = c0[c];
        a.c[1][c] = c1[c];
        a.c[2][c] = c2[c];
        b.p[0][c] = p0[c];
        b.p[1][c] = p2[c];
        b.p[2][c] = p3[c];
        b.c[0][c] = c0[c];
        b.c[1][c] = c2[c];
        b.c[2][c] = c3[c];
      }
    }
  }
  return n;
}

// The place of the eye in the world, from the world to camera matrix of the
// game (4 rows of 3, the translation in the last row): the point that the
// matrix puts at the origin of the camera. False when the matrix has no
// inverse.
inline bool CameraPlace(const float m[12], float out[3]) {
  const float det = m[0] * (m[4] * m[8] - m[5] * m[7]) -
                    m[1] * (m[3] * m[8] - m[5] * m[6]) +
                    m[2] * (m[3] * m[7] - m[4] * m[6]);
  if (!(fabsf(det) > 1e-20f)) return false;
  const float r = 1.0f / det;
  float inv[9];
  inv[0] = (m[4] * m[8] - m[5] * m[7]) * r;
  inv[1] = (m[2] * m[7] - m[1] * m[8]) * r;
  inv[2] = (m[1] * m[5] - m[2] * m[4]) * r;
  inv[3] = (m[5] * m[6] - m[3] * m[8]) * r;
  inv[4] = (m[0] * m[8] - m[2] * m[6]) * r;
  inv[5] = (m[2] * m[3] - m[0] * m[5]) * r;
  inv[6] = (m[3] * m[7] - m[4] * m[6]) * r;
  inv[7] = (m[1] * m[6] - m[0] * m[7]) * r;
  inv[8] = (m[0] * m[4] - m[1] * m[3]) * r;
  // A world point w has the camera point w * M + t. The origin of the camera
  // is the w with w * M + t = 0, so w = -t * inverse(M).
  const float t[3] = {-m[9], -m[10], -m[11]};
  for (int i = 0; i < 3; ++i) {
    out[i] = t[0] * inv[0 + i] + t[1] * inv[3 + i] + t[2] * inv[6 + i];
  }
  return true;
}

}  // namespace glow_math
