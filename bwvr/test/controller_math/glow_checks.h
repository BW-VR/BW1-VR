// THE GLOW RIBBONS, THE TRAIL OF A GESTURE AND THE GLOW OF A RAY
// (docs/hand-model.md, "The glow ribbons", and docs/miracles.md, "The trail
// at the fingertip"). main.cpp includes this file after Check and Near, just
// before main.
#pragma once

#include "../../src/mod/gesture_trail_math.h"
#include "../../src/mod/glow_math.h"
#include "../../src/mod/ray_glow_math.h"

namespace glow_checks {

inline float Dot3(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline glow_math::Point MakePoint(float x, float y, float z, float half) {
  glow_math::Point p;
  p.p[0] = x;
  p.p[1] = y;
  p.p[2] = z;
  p.half_width = half;
  for (int k = 0; k < 3; ++k) {
    p.core[k] = 0.8f;
    p.rim[k] = 0.3f;
  }
  return p;
}

inline void RibbonChecks() {
  using namespace glow_math;
  printf("\nThe glow ribbons\n");

  Check(RibbonTriangles(0) == 0 && RibbonTriangles(1) == 0,
        "a ribbon of fewer than 2 points has no triangle", RibbonTriangles(1),
        0);
  Check(RibbonTriangles(2) == 8 && RibbonTriangles(10) == 72,
        "a ribbon of n points has 8 triangles for each of the n - 1 segments",
        RibbonTriangles(10), 72);

  // A straight line along x, the eye above it: the side axis is along z, and
  // the corners are at -1, -0.5, 0, 0.5 and 1 half widths.
  Point line[3] = {MakePoint(0, 0, 0, 0.1f), MakePoint(1, 0, 0, 0.1f),
                   MakePoint(2, 0, 0, 0.1f)};
  const float above[3] = {1.0f, 5.0f, 0.0f};
  static Triangle tri[64];
  int n = BuildRibbon(line, 3, above, tri, 64);
  Check(n == 16, "3 points give 16 triangles", n, 16);
  float lowest = 1e9f, highest = -1e9f;
  bool flat = true;
  for (int t = 0; t < n; ++t) {
    for (int v = 0; v < 3; ++v) {
      const float z = tri[t].p[v][2];
      if (z < lowest) lowest = z;
      if (z > highest) highest = z;
      if (fabsf(tri[t].p[v][1]) > 1e-6f) flat = false;
    }
  }
  Near("the ribbon spans minus one half width across", lowest, -0.1, 1e-5);
  Near("the ribbon spans plus one half width across", highest, 0.1, 1e-5);
  Check(flat, "a ribbon above a line in the xz plane lies in that plane", flat,
        1);

  // The light: core on the middle line, rim half way, none at the edge.
  bool light_ok = true;
  for (int t = 0; t < n; ++t) {
    for (int v = 0; v < 3; ++v) {
      const float across = fabsf(tri[t].p[v][2]) / 0.1f;
      const float want = across < 0.01f ? 0.8f : across < 0.6f ? 0.3f : 0.0f;
      if (fabsf(tri[t].c[v][0] - want) > 1e-5f) light_ok = false;
    }
  }
  Check(light_ok, "the light is the core in the middle, the rim half way and "
        "none at the edge", light_ok, 1);

  // The ribbon faces the eye: every corner of a curved line lies in the plane
  // that is square to the way to the eye, at the point of the line.
  Point curve[6];
  for (int i = 0; i < 6; ++i) {
    const float a = 0.5f * static_cast<float>(i);
    curve[i] = MakePoint(cosf(a), 0.3f * a, sinf(a), 0.05f);
  }
  const float eye[3] = {0.5f, 2.0f, -3.0f};
  n = BuildRibbon(curve, 6, eye, tri, 64);
  Check(n == RibbonTriangles(6), "6 points give 40 triangles", n, 40);
  // Corner k of point i is the corner of triangle (i * 4 + band) * 2.
  bool faces = true;
  for (int i = 0; i + 1 < 6; ++i) {
    // The first corner of the first band of each segment is at point i.
    const Triangle& first = tri[i * 8];
    const float to_eye[3] = {eye[0] - curve[i].p[0], eye[1] - curve[i].p[1],
                             eye[2] - curve[i].p[2]};
    float off[3] = {first.p[0][0] - curve[i].p[0],
                    first.p[0][1] - curve[i].p[1],
                    first.p[0][2] - curve[i].p[2]};
    if (fabsf(Dot3(off, to_eye)) > 1e-4f) faces = false;
    // And it is a half width from the line.
    if (fabsf(sqrtf(Dot3(off, off)) - 0.05f) > 1e-4f) faces = false;
  }
  Check(faces, "each corner is square to the way to the eye, one half width "
        "from the line", faces, 1);

  // The eye on the axis of the line: the side axis is not known there. The
  // ribbon must still have finite corners of the right width.
  Point axis[3] = {MakePoint(0, 0, 1, 0.1f), MakePoint(0, 0, 2, 0.1f),
                   MakePoint(0, 0, 3, 0.1f)};
  const float on_axis[3] = {0.0f, 0.0f, -5.0f};
  n = BuildRibbon(axis, 3, on_axis, tri, 64);
  bool finite = n == 16;
  for (int t = 0; t < n; ++t) {
    for (int v = 0; v < 3; ++v) {
      for (int k = 0; k < 3; ++k) {
        if (!(fabsf(tri[t].p[v][k]) < 100.0f)) finite = false;
      }
      const float dx = tri[t].p[v][0], dy = tri[t].p[v][1];
      if (sqrtf(dx * dx + dy * dy) > 0.1f + 1e-5f) finite = false;
    }
  }
  Check(finite, "a line that points at the eye still gives a ribbon with "
        "finite corners", finite, 1);

  // The side keeps its sign from point to point.
  Point zigzag[4] = {MakePoint(0, 0, 0, 0.1f), MakePoint(1, 0, 0, 0.1f),
                     MakePoint(1, 0, 1, 0.1f), MakePoint(0, 0, 1, 0.1f)};
  float side[kMaxPoints][3];
  const float high[3] = {0.5f, 5.0f, 0.5f};
  Sides(zigzag, 4, high, side);
  bool same_side = true;
  for (int i = 1; i < 4; ++i) {
    if (Dot3(side[i], side[i - 1]) < 0.0f) same_side = false;
  }
  Check(same_side, "the side axis does not flip from one point to the next",
        same_side, 1);

  // The room.
  static Triangle small[3];
  n = BuildRibbon(curve, 6, eye, small, 3);
  Check(n <= 3, "the build stops at the room it has", n, 3);

  // The place of the eye from the world to camera matrix.
  const float yaw = 0.6f;
  const float c = cosf(yaw), s = sinf(yaw);
  // Rows of a point: out = p * M + t. A turn about y and a translation.
  const float m[12] = {c, 0, -s, 0, 1, 0, s, 0, c, 4.0f, -2.0f, 7.0f};
  float place[3];
  const bool have = CameraPlace(m, place);
  Check(have, "the matrix has an inverse", have, 1);
  float back[3];
  overlay_math::ToCamera(m, place, back);
  Near("the place of the eye is the origin of the camera, x", back[0], 0.0,
       1e-4);
  Near("the place of the eye is the origin of the camera, y", back[1], 0.0,
       1e-4);
  Near("the place of the eye is the origin of the camera, z", back[2], 0.0,
       1e-4);
  const float flat_matrix[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, -1, -2, -3};
  CameraPlace(flat_matrix, place);
  Near("a camera moved by (1, 2, 3) is at (1, 2, 3), x", place[0], 1.0, 1e-5);
  Near("a camera moved by (1, 2, 3) is at (1, 2, 3), y", place[1], 2.0, 1e-5);
  Near("a camera moved by (1, 2, 3) is at (1, 2, 3), z", place[2], 3.0, 1e-5);
  const float singular[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3};
  Check(!CameraPlace(singular, place), "a matrix with no inverse gives no place",
        0, 0);
}

inline void TrailChecks() {
  using namespace gesture_trail_math;
  printf("\nThe trail of a gesture\n");

  Near("a new point is full light", Light(0.0f), 1.0, 1e-6);
  Near("a point at the keep time is no light", Light(kKeepSeconds), 0.0, 1e-6);
  Near("a point past the keep time is no light", Light(kKeepSeconds + 1.0f),
       0.0, 1e-6);
  Near("half the keep time is half the light", Light(kKeepSeconds * 0.5f), 0.5,
       1e-5);
  bool falls = true;
  for (int i = 1; i <= 20; ++i) {
    if (Light(kKeepSeconds * i / 20.0f) > Light(kKeepSeconds * (i - 1) / 20.0f))
      falls = false;
  }
  Check(falls, "the light only falls with age", falls, 1);

  Near("a draw that runs is full light", ReleaseLight(true, 9.0f, 0.0f), 1.0,
       1e-6);
  Near("the release starts at full light", ReleaseLight(false, 5.0f, 5.0f),
       1.0, 1e-6);
  Near("half the release time is half the light",
       ReleaseLight(false, 5.0f + kReleaseSeconds * 0.5f, 5.0f), 0.5, 1e-5);
  Near("past the release time there is no light",
       ReleaseLight(false, 6.0f, 5.0f), 0.0, 1e-6);

  static Trail t;
  Clear(&t);
  const float step = 0.05f;
  float p[3] = {1.0f, 2.0f, 3.0f};
  Step(&t, true, p, 10.0f, step);
  Check(t.down && t.count == 1, "the first frame of a draw makes one point",
        t.count, 1);
  Near("the live point is the point of the draw", t.live[1], 2.0, 1e-6);
  Step(&t, true, p, 10.01f, step);
  Check(t.count == 1, "a still point adds nothing", t.count, 1);
  p[0] += 0.03f;
  Step(&t, true, p, 10.02f, step);
  Check(t.count == 1, "a move less than a step adds nothing", t.count, 1);
  Near("the live point follows the point of the draw", t.live[0], 1.03, 1e-6);
  p[0] += 0.03f;
  Step(&t, true, p, 10.03f, step);
  Check(t.count == 2, "a move of one step adds a point", t.count, 2);
  Near("the new point is the point of the draw", t.point[1][0], 1.06, 1e-6);

  // A point stays kKeepSeconds.
  p[0] += 0.06f;
  Step(&t, true, p, 11.0f, step);
  Check(t.count == 3, "a third point", t.count, 3);
  p[0] += 0.06f;
  Step(&t, true, p, 11.6f, step);
  // The first point is 1.6 s old, the second 1.57 s: both are gone.
  Check(t.count == 2, "the points older than the keep time go", t.count, 2);
  Near("the oldest point that stays is the third", t.point[0][0], 1.12, 1e-5);

  // The release.
  Step(&t, false, p, 11.7f, step);
  Check(!t.down && t.count == 2, "a release ends the draw and keeps the points",
        t.count, 2);
  Near("the release time is the time of the release", t.release_time, 11.7,
       1e-5);
  Step(&t, false, p, 11.7f + kReleaseSeconds * 0.5f, step);
  Check(t.count == 2, "the points stay while the release runs", t.count, 2);
  Step(&t, false, p, 11.7f + kReleaseSeconds + 0.01f, step);
  Check(t.count == 0, "the trail is empty after the release time", t.count, 0);

  // A new draw clears an old trail that is still there.
  Clear(&t);
  p[0] = 0.0f;
  Step(&t, true, p, 20.0f, step);
  p[0] = 0.2f;
  Step(&t, true, p, 20.1f, step);
  Step(&t, false, p, 20.2f, step);
  Check(t.count == 2, "the trail stays after the end of a draw", t.count, 2);
  p[0] = 5.0f;
  Step(&t, true, p, 20.25f, step);
  Check(t.count == 1 && fabsf(t.point[0][0] - 5.0f) < 1e-6f,
        "a new draw starts a new trail", t.count, 1);

  // A full buffer drops the oldest point.
  Clear(&t);
  for (int i = 0; i < kMaxPoints + 10; ++i) {
    p[0] = 0.1f * static_cast<float>(i);
    Step(&t, true, p, 30.0f + 0.001f * i, step);
  }
  Check(t.count == kMaxPoints, "the buffer holds kMaxPoints points", t.count,
        kMaxPoints);
  Near("the oldest point of a full buffer is 10 points on",
       t.point[0][0], 1.0, 1e-4);
  Near("the newest point of a full buffer is the last one",
       t.point[kMaxPoints - 1][0], 0.1 * (kMaxPoints + 9), 1e-3);

  // The same trail at 72, 90 and 120 frames in a second: the point count
  // depends on the path, not on the frame rate.
  int counts[3];
  const int rates[3] = {72, 90, 120};
  for (int r = 0; r < 3; ++r) {
    Clear(&t);
    for (int i = 0; i < rates[r]; ++i) {
      // 1 m in 1 s, along x.
      p[0] = static_cast<float>(i) / static_cast<float>(rates[r]);
      p[1] = p[2] = 0.0f;
      Step(&t, true, p, 40.0f + static_cast<float>(i) / rates[r], 0.05f);
    }
    counts[r] = t.count;
  }
  Check(abs(counts[0] - counts[1]) <= 1 && abs(counts[1] - counts[2]) <= 1,
        "a draw of 1 m gives the same count of points at 72, 90 and 120 Hz",
        counts[2], counts[0]);
}

inline void RayChecks() {
  using namespace ray_glow_math;
  printf("\nThe glow of a ray\n");

  Near("a line of 2 m has the period of the least length", Period(2.0f),
       kPeriodM, 1e-6);
  Check(Period(100.0f) > Period(10.0f) && Period(10.0f) > kPeriodM - 1e-6f,
        "a longer line has wider bands", Period(100.0f), Period(10.0f));
  Check(Samples(0.0f) == 2 && Samples(-1.0f) == 2,
        "a line with no length has 2 samples", Samples(0.0f), 2);
  Check(Samples(2.0f) >= 40 && Samples(2.0f) <= 41,
        "a line of 2 m has 6 samples for each band", Samples(2.0f), 41);
  Check(Samples(1000.0f) == kMaxSamples, "a very long line has the most samples",
        Samples(1000.0f), kMaxSamples);
  bool few = true;
  for (int i = 1; i <= 400; ++i) {
    const int n = Samples(0.05f * static_cast<float>(i));
    if (n < 2 || n > kMaxSamples) few = false;
  }
  Check(few, "the samples are always from 2 to the most", few, 1);
  // A band has about kSamplesPerBand samples on any line.
  const float per_band =
      static_cast<float>(Samples(8.0f) - 1) / (8.0f / Period(8.0f));
  Near("a band of a line of 8 m has 6 samples", per_band, 6.0, 0.3);

  // The pulse.
  bool in_range = true;
  float lowest = 2.0f, highest = -1.0f;
  for (int i = 0; i < 400; ++i) {
    const float v = Pulse(0.01f * i, 0.37f, kPeriodM);
    if (v < lowest) lowest = v;
    if (v > highest) highest = v;
    if (v < kFloor - 1e-5f || v > 1.0f + 1e-5f) in_range = false;
  }
  Check(in_range, "the light is from the floor to 1", in_range, 1);
  Near("the least light is the floor", lowest, kFloor, 1e-3);
  Near("the most light is 1", highest, 1.0, 5e-3);
  Near("the light repeats each period", Pulse(0.77f, 1.3f, kPeriodM),
       Pulse(0.77f + kPeriodM, 1.3f, kPeriodM), 1e-4);
  // The peak of a band is half a period from its start, and it moves outward.
  Near("a band peaks at half a period", Pulse(0.5f * kPeriodM, 0.0f, kPeriodM),
       1.0, 1e-5);
  Near("the band moves outward at kSpeedMps",
       Pulse(0.5f * kPeriodM + kSpeedMps * 0.2f, 0.2f, kPeriodM), 1.0, 1e-4);
  Check(Pulse(0.5f * kPeriodM, 0.2f, kPeriodM) < 1.0f - 0.01f,
        "the band is no longer at its old place", 0, 0);

  Near("no light at the hand", FadeIn(0.0f), 0.0, 1e-6);
  Near("full light from kFadeInM", FadeIn(kFadeInM), 1.0, 1e-6);
  Near("half the light at half kFadeInM", FadeIn(kFadeInM * 0.5f), 0.5, 1e-5);
  Near("full light far out", FadeIn(5.0f), 1.0, 1e-6);

  // A polyline.
  const float line[3][3] = {{0, 0, 0}, {2, 0, 0}, {2, 4, 0}};
  const float cumulative[3] = {0.0f, 2.0f, 6.0f};
  float at[3];
  AlongPolyline(line, cumulative, 3, 1.0f, at);
  Near("half of the first segment, x", at[0], 1.0, 1e-5);
  AlongPolyline(line, cumulative, 3, 2.0f, at);
  Check(fabsf(at[0] - 2.0f) < 1e-5f && fabsf(at[1]) < 1e-5f,
        "the end of the first segment is the second point", at[0], 2.0);
  AlongPolyline(line, cumulative, 3, 4.0f, at);
  Check(fabsf(at[0] - 2.0f) < 1e-5f && fabsf(at[1] - 2.0f) < 1e-5f,
        "half of the second segment", at[1], 2.0);
  AlongPolyline(line, cumulative, 3, 99.0f, at);
  Check(fabsf(at[1] - 4.0f) < 1e-5f, "past the end is the last point", at[1],
        4.0);
  AlongPolyline(line, cumulative, 3, -1.0f, at);
  Check(fabsf(at[0]) < 1e-5f && fabsf(at[1]) < 1e-5f,
        "before the start is the first point", at[0], 0.0);
}

inline void GlowChecks() {
  RibbonChecks();
  TrailChecks();
  RayChecks();
}

}  // namespace glow_checks
