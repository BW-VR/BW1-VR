// THE VIEW FOR VIDEOS (video_view.cpp, docs/backend.md, "The view for
// videos"): the area of the left eye picture that the window of the game
// shows, the smoothed direction of the head for that area, the corner marks
// around it, and the place of the video in the window. Pure math, with the
// checks in test/controller_math/video_view_checks.h.
//
// Each rect is in pixels of a picture, x to the right and y down, with x0 and
// y0 inside and x1 and y1 at the far edge.

#pragma once

#include <math.h>

namespace video_view_math {

// The shape of the video.
const float kAspect = 16.0f / 9.0f;

// video_view_down_pct: the middle of the area is this many percent of the
// height of the eye picture below the middle of the picture.
const int kDownMin = 0;
const int kDownMax = 40;
const int kDownStep = 5;
const int kDownDefault = 20;

// video_view_fov_deg: the horizontal angle of the area at the middle of the
// eye picture.
const int kFovMin = 40;
const int kFovMax = 110;
const int kFovDefault = 90;

// video_view_smooth_ms: the time constant of the smoothed head. 0: the area
// follows the head with no smoothing.
const int kSmoothMax = 1000;
const int kSmoothDefault = 150;

// The smoothed direction stays within this angle of the head. So the area
// moves at most this far in the eye picture, and the marks in the headset
// stay outside each place of the area.
const float kBandDeg = 3.0f;

// The marks: pixels of space between the area and a mark, so the linear
// filter of the video does not read a mark.
const float kMarkGap = 3.0f;

struct Rect {
  float x0, y0, x1, y1;
};

inline int ClampInt(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

inline int ClampDown(int pct) { return ClampInt(pct, kDownMin, kDownMax); }
inline int ClampFov(int deg) { return ClampInt(deg, kFovMin, kFovMax); }
inline int ClampSmooth(int ms) { return ClampInt(ms, 0, kSmoothMax); }

// One step of the height. lower: the area goes down one step. The step stops
// at each end of the range.
inline int StepDown(int pct, bool lower) {
  return ClampDown(ClampDown(pct) + (lower ? kDownStep : -kDownStep));
}

// THE AREA with no smoothing offset, in pixels of an eye picture of width by
// height. fx: the pixels of a tangent of 1 across the picture. margin_x and
// margin_y: the most that the smoothed head moves the area (Moved). The area
// is fov_deg wide and has the shape of the video. When the area and its
// margins are larger than the picture, the area is smaller. When the move
// down takes the area and its margin past the bottom, the move is less.
inline Rect Area(float width, float height, float fx, int fov_deg,
                 int down_pct, float margin_x, float margin_y) {
  const float half_angle = 0.5f * static_cast<float>(ClampFov(fov_deg)) *
                           3.14159265f / 180.0f;
  float half_w = tanf(half_angle) * fx;
  float half_h = half_w / kAspect;
  const float room_w = 0.5f * width - margin_x;
  const float room_h = 0.5f * height - margin_y;
  if (!(room_w > 1.0f) || !(room_h > 1.0f) || !(half_w > 0.0f)) {
    const Rect none = {0.0f, 0.0f, width, height};
    return none;
  }
  float k = 1.0f;
  if (half_w > room_w) k = room_w / half_w;
  if (half_h * k > room_h) k = room_h / half_h;
  half_w *= k;
  half_h *= k;
  const float cx = 0.5f * width;
  float cy = 0.5f * height +
             static_cast<float>(ClampDown(down_pct)) * 0.01f * height;
  const float low = height - margin_y - half_h;
  if (cy > low) cy = low;
  const float high = margin_y + half_h;
  if (cy < high) cy = high;
  const Rect r = {cx - half_w, cy - half_h, cx + half_w, cy + half_h};
  return r;
}

// The area moved by dx and dy pixels, and kept inside the picture.
inline Rect Moved(const Rect& a, float dx, float dy, float width,
                  float height) {
  Rect r = {a.x0 + dx, a.y0 + dy, a.x1 + dx, a.y1 + dy};
  if (r.x0 < 0.0f) {
    r.x1 -= r.x0;
    r.x0 = 0.0f;
  }
  if (r.x1 > width) {
    r.x0 -= r.x1 - width;
    r.x1 = width;
  }
  if (r.y0 < 0.0f) {
    r.y1 -= r.y0;
    r.y0 = 0.0f;
  }
  if (r.y1 > height) {
    r.y0 -= r.y1 - height;
    r.y1 = height;
  }
  return r;
}

// THE HEAD. Quaternions are x y z w, in the space of OpenXR: x right, y up,
// forward -z.
inline void Rotate(const float q[4], const float v[3], float out[3]) {
  // out = v + 2 w (u x v) + 2 u x (u x v), u = q.xyz
  const float ux = q[0], uy = q[1], uz = q[2], w = q[3];
  const float cx = uy * v[2] - uz * v[1];
  const float cy = uz * v[0] - ux * v[2];
  const float cz = ux * v[1] - uy * v[0];
  const float dx = uy * cz - uz * cy;
  const float dy = uz * cx - ux * cz;
  const float dz = ux * cy - uy * cx;
  out[0] = v[0] + 2.0f * (w * cx + dx);
  out[1] = v[1] + 2.0f * (w * cy + dy);
  out[2] = v[2] + 2.0f * (w * cz + dz);
}

inline void RotateInverse(const float q[4], const float v[3], float out[3]) {
  const float c[4] = {-q[0], -q[1], -q[2], q[3]};
  Rotate(c, v, out);
}

inline void Forward(const float q[4], float out[3]) {
  const float ahead[3] = {0.0f, 0.0f, -1.0f};
  Rotate(q, ahead, out);
}

inline float Dot(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline bool Normalize(float v[3]) {
  const float n = sqrtf(Dot(v, v));
  if (!(n > 1e-6f)) return false;
  v[0] /= n;
  v[1] /= n;
  v[2] /= n;
  return true;
}

// The smoothed direction of the view of the video, in the space of the head
// poses.
struct Smooth {
  float f[3];
  bool have;
};

// One frame: the smoothed direction goes toward `forward` (a unit vector) by
// 1 - exp(-seconds / tau). It then stays within band_rad of `forward`. A
// tau of 0 or less gives `forward` itself. The first step takes `forward`.
inline void SmoothStep(Smooth* s, const float forward[3], float seconds,
                       float tau, float band_rad) {
  if (!s->have || !(tau > 0.0f)) {
    for (int i = 0; i < 3; ++i) s->f[i] = forward[i];
    s->have = true;
    return;
  }
  if (seconds < 0.0f) seconds = 0.0f;
  const float k = 1.0f - expf(-seconds / tau);
  float v[3];
  for (int i = 0; i < 3; ++i) v[i] = s->f[i] + (forward[i] - s->f[i]) * k;
  if (!Normalize(v)) {
    for (int i = 0; i < 3; ++i) s->f[i] = forward[i];
    return;
  }
  // The band: an angle above band_rad goes back to band_rad, in the plane of
  // the two directions.
  float c = Dot(v, forward);
  if (c > 1.0f) c = 1.0f;
  const float angle = acosf(c);
  if (angle > band_rad) {
    // v = forward cos(band) + p sin(band), p the unit part of v across
    // forward.
    float p[3];
    for (int i = 0; i < 3; ++i) p[i] = v[i] - forward[i] * c;
    if (Normalize(p)) {
      const float cb = cosf(band_rad), sb = sinf(band_rad);
      for (int i = 0; i < 3; ++i) v[i] = forward[i] * cb + p[i] * sb;
    } else {
      for (int i = 0; i < 3; ++i) v[i] = forward[i];
    }
  }
  for (int i = 0; i < 3; ++i) s->f[i] = v[i];
}

// The angle between the smoothed direction and `forward`, in degrees.
inline float LagDeg(const Smooth& s, const float forward[3]) {
  float c = Dot(s.f, forward);
  if (c > 1.0f) c = 1.0f;
  if (c < -1.0f) c = -1.0f;
  return acosf(c) * 180.0f / 3.14159265f;
}

// The direction `dir` in the frame of a head with orientation q, as the
// tangents of the eye picture: tx to the right, ty up. False when the
// direction is not in front of the head.
inline bool Tangents(const float q[4], const float dir[3], float* tx,
                     float* ty) {
  float d[3];
  RotateInverse(q, dir, d);
  if (!(d[2] < -1e-3f)) return false;
  *tx = d[0] / -d[2];
  *ty = d[1] / -d[2];
  return true;
}

// THE MARKS: a corner of two bars at each corner of the area grown by grow_x
// and grow_y, and kMarkGap more. So each mark is outside each place of the
// area that Moved can give. thick: the width of a bar. length: the length of
// a bar from the corner. Writes 8 rects.
const int kMarkRects = 8;
inline int Marks(const Rect& a, float grow_x, float grow_y, float thick,
                 float length, Rect out[kMarkRects]) {
  const float x0 = a.x0 - grow_x - kMarkGap, x1 = a.x1 + grow_x + kMarkGap;
  const float y0 = a.y0 - grow_y - kMarkGap, y1 = a.y1 + grow_y + kMarkGap;
  int n = 0;
  for (int corner = 0; corner < 4; ++corner) {
    const bool right = (corner & 1) != 0;
    const bool bottom = (corner & 2) != 0;
    // The outer corner of the mark.
    const float cx = right ? x1 + thick : x0 - thick;
    const float cy = bottom ? y1 + thick : y0 - thick;
    const float sx = right ? -1.0f : 1.0f;  // toward the area
    const float sy = bottom ? -1.0f : 1.0f;
    // The bar across: from the corner along x.
    Rect across = {cx, cy, cx + sx * (length + thick), cy + sy * thick};
    // The bar down: from the corner along y.
    Rect down = {cx, cy, cx + sx * thick, cy + sy * (length + thick)};
    Rect* bars[2] = {&across, &down};
    for (int b = 0; b < 2; ++b) {
      Rect r = *bars[b];
      if (r.x0 > r.x1) {
        const float t = r.x0;
        r.x0 = r.x1;
        r.x1 = t;
      }
      if (r.y0 > r.y1) {
        const float t = r.y0;
        r.y0 = r.y1;
        r.y1 = t;
      }
      out[n++] = r;
    }
  }
  return n;
}

// True when the two rects have a part in common.
inline bool Overlap(const Rect& a, const Rect& b) {
  return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

// THE PLACE IN THE WINDOW: the largest rect with the shape `aspect` (width
// over height) in a client area of width by height, in the middle. The rest
// of the window is black.
inline Rect Fit(float width, float height, float aspect) {
  Rect r = {0.0f, 0.0f, width, height};
  if (!(aspect > 0.0f) || !(width > 0.0f) || !(height > 0.0f)) return r;
  if (width / height > aspect) {
    const float w = height * aspect;
    r.x0 = floorf(0.5f * (width - w));
    r.x1 = r.x0 + floorf(w + 0.5f);
  } else {
    const float h = width / aspect;
    r.y0 = floorf(0.5f * (height - h));
    r.y1 = r.y0 + floorf(h + 0.5f);
  }
  return r;
}

}  // namespace video_view_math
