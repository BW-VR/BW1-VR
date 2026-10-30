// The menus of the game on a panel that is fixed in the room: the place of
// the panel, and the point where a ray meets it. See docs/ui-layer.md, "The
// menus in the room". No Windows and no game here, so the math test
// compiles this file.
//
// All poses are in the LOCAL space of the runtime, in meters: x right, y up,
// z back. The front of a quad is its +z side.

#pragma once

#include <math.h>

#include "controller_math.h"

namespace menu_room_math {

// The yaw of a head, from its right axis. The right axis stays level when
// the head looks up or down, so a head that looks straight down has a yaw.
inline float YawOf(const float orientation[4]) {
  const float right[3] = {1.0f, 0.0f, 0.0f};
  float r[3];
  controller_math::QuatRotate(orientation, right, r);
  return atan2f(-r[2], r[0]);
}

// A turn of `yaw` radians about y, as x y z w.
inline void YawTurn(float yaw, float out[4]) {
  out[0] = 0.0f;
  out[1] = sinf(yaw * 0.5f);
  out[2] = 0.0f;
  out[3] = cosf(yaw * 0.5f);
}

// A panel in the room.
struct Panel {
  float position[3];     // the center
  float orientation[4];  // x y z w, the front is the +z side
  float width;           // meters
  float height;
};

// THE PLACE. The anchor is the head of the frame in which the menu opens:
// its position and its yaw. A panel goes `distance` meters from the anchor,
// `pitch` radians above eye level (below is negative), straight ahead of
// the yaw. It turns about its own x axis by the same pitch, so it faces the
// anchor. It has no roll. After that it stays where it is in the room.
inline void PoseAt(const float anchor[3], float yaw, float distance,
                   float pitch, Panel* out) {
  float turn[4];
  YawTurn(yaw, turn);
  controller_math::PanelPose(anchor, turn, distance, pitch, out->position,
                             out->orientation);
}

// The menu panel: it shows pixels_w by pixels_h pixels of the render at
// pixels_per_degree. False when a number is missing.
inline bool PlaceMenu(const float anchor[3], float yaw, float distance,
                      float pitch, float pixels_w, float pixels_h,
                      float pixels_per_degree, Panel* out) {
  if (!(distance > 0.0f) || !(pixels_w > 0.0f) || !(pixels_h > 0.0f)) {
    return false;
  }
  PoseAt(anchor, yaw, distance, pitch, out);
  out->width =
      controller_math::PanelWidth(pixels_w, pixels_per_degree, distance);
  if (!(out->width > 0.0f)) return false;
  out->height = out->width * pixels_h / pixels_w;
  return true;
}

// A RAY AGAINST A PANEL. origin and direction are the aim ray of a hand.
// u is 0 at the left edge and 1 at the right edge, v is 0 at the top and 1
// at the bottom, and t is the distance along the ray in meters when the
// direction has the length 1. False when the ray is parallel to the panel,
// meets its back, or meets the plane behind the hand. u and v can be
// outside 0 to 1: the ray meets the plane past the edge.
inline bool Hit(const float origin[3], const float direction[3],
                const Panel& panel, float* u, float* v, float* t) {
  if (!(panel.width > 0.0f) || !(panel.height > 0.0f)) return false;
  const float front[3] = {0.0f, 0.0f, 1.0f};
  float n[3];
  controller_math::QuatRotate(panel.orientation, front, n);
  const float s =
      direction[0] * n[0] + direction[1] * n[1] + direction[2] * n[2];
  // Zero is parallel, and above zero the ray meets the back side.
  if (!(s < -1e-4f)) return false;
  const float to[3] = {panel.position[0] - origin[0],
                       panel.position[1] - origin[1],
                       panel.position[2] - origin[2]};
  const float along = (to[0] * n[0] + to[1] * n[1] + to[2] * n[2]) / s;
  if (!(along > 0.0f)) return false;
  const float from_center[3] = {origin[0] + along * direction[0] - panel.position[0],
                                origin[1] + along * direction[1] - panel.position[1],
                                origin[2] + along * direction[2] - panel.position[2]};
  const float back[4] = {-panel.orientation[0], -panel.orientation[1],
                         -panel.orientation[2], panel.orientation[3]};
  float k[3];
  controller_math::QuatRotate(back, from_center, k);
  *u = k[0] / panel.width + 0.5f;
  *v = 0.5f - k[1] / panel.height;
  *t = along;
  return true;
}

inline bool Inside(float u, float v) {
  return u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f;
}

// The pixel of the render at u, v of a panel that shows the rect x, y, w, h
// of the render. A point past the edge goes to the edge.
inline void Pixel(float u, float v, const int rect[4], float* x, float* y) {
  const float cu = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
  const float cv = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
  *x = static_cast<float>(rect[0]) + cu * static_cast<float>(rect[2]);
  *y = static_cast<float>(rect[1]) + cv * static_cast<float>(rect[3]);
  // The last pixel of the rect, not the first one past it.
  const float right = static_cast<float>(rect[0] + rect[2]) - 1.0f;
  const float bottom = static_cast<float>(rect[1] + rect[3]) - 1.0f;
  if (*x > right) *x = right;
  if (*y > bottom) *y = bottom;
}

// The aim direction of a hand pose in the LOCAL space: its -z axis.
inline void AimForward(const float orientation[4], float out[3]) {
  const float forward[3] = {0.0f, 0.0f, -1.0f};
  controller_math::QuatRotate(orientation, forward, out);
}

// The point on a panel at u, v, in the room.
inline void PointAt(const Panel& panel, float u, float v, float out[3]) {
  const float local[3] = {(u - 0.5f) * panel.width,
                          (0.5f - v) * panel.height, 0.0f};
  float turned[3];
  controller_math::QuatRotate(panel.orientation, local, turned);
  for (int i = 0; i < 3; ++i) out[i] = panel.position[i] + turned[i];
}

}  // namespace menu_room_math
