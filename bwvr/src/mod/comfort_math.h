// THE COMFORT OF A MOVE (docs/god-view.md, "The comfort of a move"): the
// vignette and the recenter hold. Pure functions: no Windows
// and no game. The checks are in test/controller_math/comfort_checks.h.

#pragma once

#include <math.h>

namespace comfort_math {

// --- the vignette (comfort_vignette) ---------------------------------------------

const int kVignetteOff = 0;
const int kVignetteGentle = 1;
const int kVignetteStrong = 2;

// The value of the key from a number: 0, 1 or 2.
inline int VignetteMode(int value) {
  if (value < kVignetteOff) return kVignetteOff;
  return value > kVignetteStrong ? kVignetteStrong : value;
}

// The next value for the switch of the debug page: from 0 to 1 to 2 and
// back to 0.
inline int NextVignetteMode(int value) {
  return VignetteMode(value) >= kVignetteStrong ? kVignetteOff
                                                : VignetteMode(value) + 1;
}

// THE SHAPE. An angle from the middle of the eye, in degrees. Up to clear_deg
// the view stays clear. From there to dark_deg the edge goes dark along a
// smooth curve, and past dark_deg it has the full darkness. darkest is the
// darkness of a move at full speed: 1 is black.
struct VignetteShape {
  float clear_deg;
  float dark_deg;
  float darkest;
};

// Gentle: a Quest 3 shows about 50 degrees to each side of the middle, so
// the edge of its view gets about 60 percent of the darkness, and the middle
// 70 degrees stay clear. Strong: black from 45 degrees, and clear only in the
// middle 44 degrees.
inline VignetteShape Shape(int mode) {
  switch (VignetteMode(mode)) {
    case kVignetteGentle:
      return {35.0f, 58.0f, 0.85f};
    case kVignetteStrong:
      return {22.0f, 45.0f, 1.0f};
    default:
      return {0.0f, 0.0f, 0.0f};
  }
}

// A smooth step from 0 at t 0 to 1 at t 1, flat at both ends.
inline float Smooth(float t) {
  if (!(t > 0.0f)) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  return t * t * (3.0f - 2.0f * t);
}

// The darkness (the alpha of black) at an angle from the middle of the eye,
// for a level of the vignette from 0 (none) to 1 (a move at full speed).
inline float DarkAt(const VignetteShape& shape, float level, float degrees) {
  if (!(level > 0.0f) || !(shape.darkest > 0.0f)) return 0.0f;
  if (level > 1.0f) level = 1.0f;
  const float span = shape.dark_deg - shape.clear_deg;
  const float t = span > 0.0f ? (degrees - shape.clear_deg) / span
                              : (degrees >= shape.dark_deg ? 1.0f : 0.0f);
  return level * shape.darkest * Smooth(t);
}

// THE FADE. The level goes up to its target in kFadeInSeconds and down to it
// in kFadeOutSeconds, in a straight line. A step of the view (a snap turn, a
// zoom step, a teleport) puts the level at 1 at once (Step), because the
// step itself has no time.
const float kFadeInSeconds = 0.15f;
const float kFadeOutSeconds = 0.3f;

inline float Fade(float level, float target, float seconds) {
  if (!(target > 0.0f)) target = 0.0f;
  if (target > 1.0f) target = 1.0f;
  if (!(level > 0.0f)) level = 0.0f;
  if (level > 1.0f) level = 1.0f;
  if (!(seconds > 0.0f)) return level;
  if (level < target) {
    level += seconds / kFadeInSeconds;
    return level > target ? target : level;
  }
  level -= seconds / kFadeOutSeconds;
  return level < target ? target : level;
}

// THE SHARE OF A MOVE: how fast the view moved in a frame, from 0 (still) to
// 1 (the full speed of a control). The three full speeds: the top speed of
// the walk (move_stick_speed_cm=150), a turn of 90 degrees a second, and one
// zoom step of 1.5 times in the 500 ms of god_zoom_smooth_ms. The largest of
// the three counts.
const float kFullMetersPerSecond = 1.5f;
const float kFullDegreesPerSecond = 90.0f;
const float kFullZoomPerSecond = 0.8109302f;  // ln(1.5) / 0.5 s

// room_meters: how far the land under the head moved across the room.
// turn_degrees: how far the view turned. ratio: the scale after the frame
// over the scale before it.
inline float MoveShare(float room_meters, float turn_degrees, float ratio,
                       float seconds) {
  if (!(seconds > 0.0f)) return 0.0f;
  float share = 0.0f;
  const float walk = fabsf(room_meters) / seconds / kFullMetersPerSecond;
  if (walk > share) share = walk;
  const float turn = fabsf(turn_degrees) / seconds / kFullDegreesPerSecond;
  if (turn > share) share = turn;
  if (ratio > 0.0f) {
    const float zoom = fabsf(logf(ratio)) / seconds / kFullZoomPerSecond;
    if (zoom > share) share = zoom;
  }
  if (!(share == share)) return 0.0f;
  return share > 1.0f ? 1.0f : share;
}

// THE PICTURE. Rings about the middle of the eye: the first at clear_deg
// with no darkness, kBands rings to dark_deg, and one ring far out past the
// edge of the render with the darkness of dark_deg. Each ring has
// kSegments points. Two rings make a band of 2 * kSegments triangles, so the
// picture is (kBands + 1) * kSegments * 2 triangles.
const int kSegments = 48;
const int kBands = 4;
const int kRings = kBands + 2;
const int kVignetteTriangles = (kRings - 1) * kSegments * 2;
const int kVignetteVertices = kVignetteTriangles * 3;

struct VignettePoint {
  float x, y;   // pixels of the render, y down
  float alpha;  // the darkness, 0 to 1
};

// The angle of ring r, in degrees. The last ring is the far ring, and it has
// the angle of dark_deg.
inline float RingDegrees(const VignetteShape& shape, int r) {
  if (r >= kRings - 1) return shape.dark_deg;
  return shape.clear_deg +
         (shape.dark_deg - shape.clear_deg) * static_cast<float>(r) / kBands;
}

// The triangles of the picture for one eye. (cx, cy) is the middle of the
// eye in pixels. fx and fy are the pixels for a tangent of 1 across and
// down: the half width of the render times inv_half_tan_fov_x, and the half
// height times inv_half_tan_fov_y. far_px is the radius of the far ring.
// out has room for kVignetteVertices points. Returns the count of points,
// three for each triangle, and 0 when there is nothing to draw.
inline int BuildVignette(const VignetteShape& shape, float level, float cx,
                         float cy, float fx, float fy, float far_px,
                         VignettePoint* out) {
  if (!(level > 0.0f) || !(shape.darkest > 0.0f)) return 0;
  if (!(fx > 0.0f) || !(fy > 0.0f) || !(shape.dark_deg < 89.0f)) return 0;
  const float kPi = 3.14159265f;
  float rx[kRings], ry[kRings], alpha[kRings];
  for (int r = 0; r < kRings; ++r) {
    const float degrees = RingDegrees(shape, r);
    const float t = tanf(degrees * kPi / 180.0f);
    rx[r] = fx * t;
    ry[r] = fy * t;
    alpha[r] = DarkAt(shape, level, degrees);
  }
  // The far ring goes past the edge of the render in each direction.
  const float last = rx[kRings - 2] > ry[kRings - 2] ? rx[kRings - 2]
                                                      : ry[kRings - 2];
  float grow = last > 0.0f ? far_px / last : 1.0f;
  if (grow < 1.0f) grow = 1.0f;
  rx[kRings - 1] = rx[kRings - 2] * grow;
  ry[kRings - 1] = ry[kRings - 2] * grow;
  int n = 0;
  for (int r = 0; r + 1 < kRings; ++r) {
    for (int k = 0; k < kSegments; ++k) {
      const float a0 = 2.0f * kPi * static_cast<float>(k) / kSegments;
      const float a1 = 2.0f * kPi * static_cast<float>(k + 1) / kSegments;
      const VignettePoint i0 = {cx + cosf(a0) * rx[r], cy + sinf(a0) * ry[r],
                                alpha[r]};
      const VignettePoint i1 = {cx + cosf(a1) * rx[r], cy + sinf(a1) * ry[r],
                                alpha[r]};
      const VignettePoint o0 = {cx + cosf(a0) * rx[r + 1],
                                cy + sinf(a0) * ry[r + 1], alpha[r + 1]};
      const VignettePoint o1 = {cx + cosf(a1) * rx[r + 1],
                                cy + sinf(a1) * ry[r + 1], alpha[r + 1]};
      out[n++] = i0;
      out[n++] = o0;
      out[n++] = o1;
      out[n++] = i0;
      out[n++] = o1;
      out[n++] = i1;
    }
  }
  return n;
}

// --- the recenter hold (recenter_hold) -------------------------------------------

// recenter_hold: which stick click recenters with a hold.
const int kRecenterOff = 0;
const int kRecenterOther = 1;     // the hand that does not point
const int kRecenterPointing = 2;  // the pointing hand

inline int RecenterRule(int value) {
  if (value < kRecenterOff) return kRecenterOff;
  return value > kRecenterPointing ? kRecenterPointing : value;
}

inline int NextRecenterRule(int value) {
  return RecenterRule(value) >= kRecenterPointing ? kRecenterOff
                                                  : RecenterRule(value) + 1;
}

// THE HOLD. The click must stay down for hold_seconds with the stick at rest
// (still). A push of the stick starts the time again, so a click that comes
// with a walk or a turn never recenters (on a Vive wand the pad click is a
// part of each push). After a recenter the click must go up before the next
// hold.
struct RecenterHold {
  bool down = false;   // a hold counts now
  double since = 0.0;  // when it started, in seconds
  bool done = false;   // this hold recentered
};

// One step, with the time now in seconds. True on the step that recenters.
inline bool RecenterStep(RecenterHold* hold, bool click, bool still,
                         double now, double hold_seconds) {
  if (!click) {
    hold->down = false;
    hold->done = false;
    return false;
  }
  if (hold->done) return false;
  if (!still) {
    hold->down = false;
    return false;
  }
  if (!hold->down) {
    hold->down = true;
    hold->since = now;
  }
  if (now - hold->since >= hold_seconds) {
    hold->done = true;
    hold->down = false;
    return true;
  }
  return false;
}

}  // namespace comfort_math
