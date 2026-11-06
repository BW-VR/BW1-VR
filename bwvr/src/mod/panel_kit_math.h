// The parts of a panel in the room that the ray of the pointing hand
// presses: the place in the room, the hit test of a rect, and the press of
// the trigger. See panel_kit.h and docs/ui-layer.md, "The panel of places".
// No Windows and no game here, so the math test compiles this file.

#pragma once

#include "menu_room_math.h"

namespace panel_kit_math {

using menu_room_math::Panel;

// THE PLACE. A panel of width by height meters, `distance` meters in front
// of the yaw of the head, `pitch` radians above eye level (below is
// negative), turned to face the head, with no roll.
inline void PlaceAt(const float head[3], float yaw, float distance,
                    float pitch, float width, float height, Panel* out) {
  menu_room_math::PoseAt(head, yaw, distance, pitch, out);
  out->width = width;
  out->height = height;
}

// THE HIT TEST. The rect (x, y, w, h in pixels of a picture of width by
// height) at u, v of the panel (0 to 1 across and down), or -1 for none and
// for a point off the panel.
inline int HitRects(float u, float v, int width, int height,
                    const int (*rects)[4], int count) {
  if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) return -1;
  const float px = u * width;
  const float py = v * height;
  for (int i = 0; i < count; ++i) {
    const int* r = rects[i];
    if (px >= r[0] && px < r[0] + r[2] && py >= r[1] && py < r[1] + r[3]) {
      return i;
    }
  }
  return -1;
}

// The box of a rect with `rim` more pixels at each side, cut to the
// picture: x0, y0, x1, y1. A new picture copies the box of each part whose
// look changed.
inline void RectBox(const int r[4], int rim, int width, int height,
                    int out[4]) {
  out[0] = r[0] - rim < 0 ? 0 : r[0] - rim;
  out[1] = r[1] - rim < 0 ? 0 : r[1] - rim;
  out[2] = r[0] + r[2] + rim > width ? width : r[0] + r[2] + rim;
  out[3] = r[1] + r[3] + rim > height ? height : r[1] + r[3] + rim;
}

// THE PRESS with the trigger: a press starts on the part under the ray and
// acts at once. A press that starts off the parts does nothing, and the
// trigger must go up before the next press. The trigger is down above 0.6
// and up below 0.4.
struct Press {
  bool down = false;  // the trigger is down
  bool was = false;   // it was down in the last frame
};

// One frame of the trigger. Returns the part to act on now, or -1.
inline int PressStep(Press* p, float trigger, int part_under_ray) {
  if (trigger > 0.6f) p->down = true;
  if (trigger < 0.4f) p->down = false;
  const bool edge = p->down && !p->was;
  p->was = p->down;
  return edge ? part_under_ray : -1;
}

// The panel opens: a trigger that is down now is not a press.
inline void PressOpen(Press* p) { p->down = p->was = true; }

}  // namespace panel_kit_math
