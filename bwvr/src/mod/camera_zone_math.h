// THE CAMERA ZONE OF THE GAME IN THE GOD VIEW (docs/god-view.md, "The camera
// zone"). Pure math: no Windows, no game. The checks are in
// test/controller_math/zone_checks.h.
//
// A script of the game sets a camera zone with the script word "set camera
// zone" (GScript::SetCameraZone 0x006ED890). The zone is a polygon on the
// map, a list of points of x and z. CameraModeNew3::Update tests the
// position of the game camera against it on each update. A position outside
// the zone moves the camera back in, with a sound, a shake and a pulse of the
// force field.
//
// In the God view the player's place may go a margin past the zone. The
// game camera stays in the zone, so the game does not move it.
//
// A zone is an array of n points, x0 z0 x1 z1 and so on. The last point
// joins the first one.

#pragma once

#include <math.h>

namespace camera_zone_math {

// The default margin past the zone, in world units. 100 units is 1 m of the
// room at the start scale of the God view. The zone of the Creature select
// (Land1Choose.exc) is about 235 by 250 units, so the player can stand 1 m
// past its edge and see all three Creatures and the Hand.
const int kMarginDefault = 100;
const int kMarginMost = 2000;

// How far into the zone the game camera goes, in world units. The game tests
// a position on the edge itself, so the camera stays a little inside it.
const float kInset = 5.0f;

// The least distance across the map from the game camera to its focus. The
// test of the game casts a ray from the camera toward its focus. With the
// two at one place the ray has no direction, the test finds the camera
// outside the zone on each update, and the game pushes it on each update.
// The plain game keeps its camera 13 to 35 units from its focus.
const float kLeastAcross = 40.0f;

// True when the point is inside the zone (the even and odd rule). A zone with
// fewer than three points has no inside.
inline bool Inside(const float* xz, int n, float x, float z) {
  if (xz == nullptr || n < 3) return false;
  bool inside = false;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    const float xi = xz[i * 2], zi = xz[i * 2 + 1];
    const float xj = xz[j * 2], zj = xz[j * 2 + 1];
    if ((zi > z) != (zj > z)) {
      const float cross = xj + (z - zj) * (xi - xj) / (zi - zj);
      if (x < cross) inside = !inside;
    }
  }
  return inside;
}

// The distance from the point to the nearest point of the edge of the zone.
// The nearest point goes to near_x and near_z. A zone with fewer than three
// points gives 0 and the point itself.
inline float EdgeDistance(const float* xz, int n, float x, float z,
                          float* near_x, float* near_z) {
  *near_x = x;
  *near_z = z;
  if (xz == nullptr || n < 3) return 0.0f;
  float best = -1.0f;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    const float ax = xz[j * 2], az = xz[j * 2 + 1];
    const float bx = xz[i * 2], bz = xz[i * 2 + 1];
    const float ex = bx - ax, ez = bz - az;
    const float length = ex * ex + ez * ez;
    float t = 0.0f;
    if (length > 1e-12f) t = ((x - ax) * ex + (z - az) * ez) / length;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const float px = ax + ex * t, pz = az + ez * t;
    const float d = (x - px) * (x - px) + (z - pz) * (z - pz);
    if (best < 0.0f || d < best) {
      best = d;
      *near_x = px;
      *near_z = pz;
    }
  }
  return sqrtf(best);
}

// How far the point is outside the zone. 0 for a point inside it.
inline float OutsideDistance(const float* xz, int n, float x, float z) {
  if (Inside(xz, n, x, z)) return 0.0f;
  float nx = 0.0f, nz = 0.0f;
  return EdgeDistance(xz, n, x, z, &nx, &nz);
}

// THE LIMIT OF A MOVE OF THE PLAYER'S PLACE. The place was at from_x from_z
// and the move takes it to x z. The place may go margin units past the zone.
// A place that is already farther out (the game put the view there) may stay
// as far out, and no move takes it farther. So the limit never makes a jump.
//
// A move past the limit stops at the limit: the place goes to the point of
// the limit nearest to the place that the move asked for. So the part of a
// move along the edge stays, and the part out of the zone goes. True when the
// limit changed the move.
inline bool LimitMove(const float* xz, int n, float margin, float from_x,
                      float from_z, float* x, float* z) {
  if (xz == nullptr || n < 3) return false;
  if (!(margin >= 0.0f)) margin = 0.0f;
  if (Inside(xz, n, *x, *z)) return false;
  float nx = 0.0f, nz = 0.0f;
  const float out = EdgeDistance(xz, n, *x, *z, &nx, &nz);
  if (out <= margin) return false;
  float allowed = OutsideDistance(xz, n, from_x, from_z);
  if (allowed < margin) allowed = margin;
  if (out <= allowed) return false;
  const float k = allowed / out;
  *x = nx + (*x - nx) * k;
  *z = nz + (*z - nz) * k;
  return true;
}

// The middle of the zone: the mean of its points.
inline void Middle(const float* xz, int n, float* x, float* z) {
  *x = 0.0f;
  *z = 0.0f;
  if (xz == nullptr || n < 1) return;
  for (int i = 0; i < n; ++i) {
    *x += xz[i * 2];
    *z += xz[i * 2 + 1];
  }
  *x /= static_cast<float>(n);
  *z /= static_cast<float>(n);
}

// A point in the zone near the point x z. A point inside stays. A point
// outside goes to the nearest point of the edge and inset units past it,
// into the zone. When that is not inside (a sharp corner), it goes from the
// edge toward the middle of the zone. True when the point moved.
inline bool PointInside(const float* xz, int n, float inset, float* x,
                        float* z) {
  if (xz == nullptr || n < 3) return false;
  if (Inside(xz, n, *x, *z)) return false;
  float nx = 0.0f, nz = 0.0f;
  const float out = EdgeDistance(xz, n, *x, *z, &nx, &nz);
  if (out > 1e-4f) {
    const float tx = nx + (nx - *x) / out * inset;
    const float tz = nz + (nz - *z) / out * inset;
    if (Inside(xz, n, tx, tz)) {
      *x = tx;
      *z = tz;
      return true;
    }
  }
  float mx = 0.0f, mz = 0.0f;
  Middle(xz, n, &mx, &mz);
  const float dx = mx - nx, dz = mz - nz;
  const float d = sqrtf(dx * dx + dz * dz);
  if (d > 1e-4f) {
    const float step = inset < d ? inset : d * 0.5f;
    const float tx = nx + dx / d * step, tz = nz + dz / d * step;
    if (Inside(xz, n, tx, tz)) {
      *x = tx;
      *z = tz;
      return true;
    }
  }
  if (Inside(xz, n, mx, mz)) {
    *x = mx;
    *z = mz;
    return true;
  }
  *x = nx;
  *z = nz;
  return true;
}

// The first part of the line from a to b that is in the zone, from a point a
// in the zone: the share of the line, 0 to 1, at which it first meets the
// edge. 1 when it does not meet it.
inline float FirstEdge(const float* xz, int n, float ax, float az, float bx,
                       float bz) {
  float first = 1.0f;
  if (xz == nullptr || n < 3) return first;
  const float dx = bx - ax, dz = bz - az;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    const float px = xz[j * 2], pz = xz[j * 2 + 1];
    const float ex = xz[i * 2] - px, ez = xz[i * 2 + 1] - pz;
    const float den = dx * ez - dz * ex;
    if (fabsf(den) < 1e-12f) continue;
    const float t = ((px - ax) * ez - (pz - az) * ex) / den;
    const float u = ((px - ax) * dz - (pz - az) * dx) / den;
    if (t >= 0.0f && t < first && u >= 0.0f && u <= 1.0f) first = t;
  }
  return first;
}

// THE GAME CAMERA STAYS IN THE ZONE. The position of the camera is a point
// in the zone. With a focus in the zone, the camera comes along the line to
// its focus, to inset units before the edge, so the camera keeps its
// direction and its pitch, and comes nearer to its focus. When that leaves
// less than kLeastAcross across the map (a focus near the edge, with the
// camera behind it out of the zone), the camera goes kLeastAcross from the
// focus toward the middle of the zone, with the same pitch. Then it looks at
// its focus from the other side. With a focus outside the zone, the position
// goes to a point in the zone near it (PointInside). The game tests only x
// and z. True when the position changed.
inline bool KeepCamera(const float* xz, int n, float inset,
                       const float focus[3], float position[3]) {
  if (xz == nullptr || n < 3) return false;
  if (Inside(xz, n, position[0], position[2])) return false;
  if (!Inside(xz, n, focus[0], focus[2])) {
    return PointInside(xz, n, inset, &position[0], &position[2]);
  }
  const float dx = position[0] - focus[0], dz = position[2] - focus[2];
  const float across = sqrtf(dx * dx + dz * dz);
  if (!(across > 1e-4f)) return false;
  // The rise of the camera for each unit across, so the pitch stays.
  const float rise = (position[1] - focus[1]) / across;
  float t = FirstEdge(xz, n, focus[0], focus[2], position[0], position[2]);
  t -= inset / across;
  if (t < 0.0f) t = 0.0f;
  if (across * t >= kLeastAcross) {
    for (int i = 0; i < 3; ++i) {
      position[i] = focus[i] + (position[i] - focus[i]) * t;
    }
    return true;
  }
  float mx = 0.0f, mz = 0.0f;
  Middle(xz, n, &mx, &mz);
  const float ix = mx - focus[0], iz = mz - focus[2];
  const float in = sqrtf(ix * ix + iz * iz);
  if (in > 1e-3f) {
    // From kLeastAcross down to a quarter of it, the first place in the zone.
    for (float step = kLeastAcross; step >= kLeastAcross * 0.25f;
         step *= 0.5f) {
      const float cx = focus[0] + ix / in * step;
      const float cz = focus[2] + iz / in * step;
      if (Inside(xz, n, cx, cz)) {
        position[0] = cx;
        position[1] = focus[1] + rise * step;
        position[2] = cz;
        return true;
      }
    }
  }
  for (int i = 0; i < 3; ++i) {
    position[i] = focus[i] + (position[i] - focus[i]) * t;
  }
  return true;
}

// THE POINT OF A BUMP: a point out of the zone, `out` units past the edge
// point nearest to the place, so the game moves its camera back in and plays
// its sound of the block. For a place outside, the point is along the line
// from the edge to the place. For a place on the edge or inside, it is along
// the line from the middle of the zone through the edge point.
inline void BumpPoint(const float* xz, int n, float out, float x, float z,
                      float* bump_x, float* bump_z) {
  float nx = x, nz = z;
  const float d = EdgeDistance(xz, n, x, z, &nx, &nz);
  float ux = 0.0f, uz = 0.0f;
  if (!Inside(xz, n, x, z) && d > 1e-3f) {
    ux = (x - nx) / d;
    uz = (z - nz) / d;
  } else {
    float mx = 0.0f, mz = 0.0f;
    Middle(xz, n, &mx, &mz);
    const float ex = nx - mx, ez = nz - mz;
    const float e = sqrtf(ex * ex + ez * ez);
    if (e > 1e-3f) {
      ux = ex / e;
      uz = ez / e;
    } else {
      ux = 1.0f;
    }
  }
  *bump_x = nx + ux * out;
  *bump_z = nz + uz * out;
  // In a hollow of the zone the line from the middle can point in.
  if (Inside(xz, n, *bump_x, *bump_z)) {
    *bump_x = nx - ux * out;
    *bump_z = nz - uz * out;
  }
}

}  // namespace camera_zone_math
