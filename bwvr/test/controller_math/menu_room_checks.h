// THE MENUS IN THE ROOM AND THE KEYBOARD IN THE HEADSET (docs/ui-layer.md,
// "The menus in the room" and "The keyboard in the headset").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/menu_room_math.h"
#include "../../src/mod/vr_keyboard_math.h"

namespace menu_room_checks {

inline void MenuRoomChecks() {
  using namespace menu_room_math;
  printf("\nThe menus in the room\n");

  const float level[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  const float head[3] = {0.2f, 1.6f, -0.1f};

  // THE PLACE. A head that looks straight ahead: the menu rect of 1280 by
  // 960 pixels at 16 pixels a degree, 1.4 m ahead, 10 degrees below eye
  // level. 80 degrees across at 1.4 m is 2 * 1.4 * tan(40) = 2.349 m.
  Panel panel;
  const float pitch = -10.0f * 0.01745329f;
  Check(PlaceMenu(head, YawOf(level), 1.4f, pitch, 1280.0f, 960.0f, 16.0f,
                  &panel),
        "the menu panel has a place", 1, 1);
  Near("the panel is 80 degrees across at 1.4 m", panel.width, 2.3494, 1e-3);
  Near("its height keeps the shape of the rect", panel.height,
       2.3494 * 960.0 / 1280.0, 1e-3);
  Near("its center is straight ahead", panel.position[0], 0.2, 1e-5);
  Near("its center is 10 degrees below the eyes", panel.position[1],
       1.6 - 1.4 * sin(10.0 * 0.01745329), 1e-4);
  Near("its center is 1.4 m ahead along the ray", panel.position[2],
       -0.1 - 1.4 * cos(10.0 * 0.01745329), 1e-4);

  // A head that is turned 90 degrees to the left and looks down 50 degrees:
  // the panel follows the yaw only, so it has no roll and no more pitch.
  float down_left[4];
  {
    const float yaw = 90.0f * 0.01745329f;
    const float look = -50.0f * 0.01745329f;
    const float qy[4] = {0.0f, sinf(yaw * 0.5f), 0.0f, cosf(yaw * 0.5f)};
    const float qx[4] = {sinf(look * 0.5f), 0.0f, 0.0f, cosf(look * 0.5f)};
    controller_math::QuatMultiply(qy, qx, down_left);
  }
  Near("the yaw of a head that looks down is the yaw of the head",
       YawOf(down_left) * 57.29578, 90.0, 1e-3);
  Panel turned;
  PlaceMenu(head, YawOf(down_left), 1.4f, pitch, 1280.0f, 960.0f, 16.0f,
            &turned);
  Near("a turn to the left puts the panel to the left of the head",
       turned.position[0], 0.2 - 1.4 * cos(10.0 * 0.01745329), 1e-4);
  Near("and not ahead", turned.position[2], -0.1, 1e-4);
  Near("at the same height as straight ahead", turned.position[1],
       panel.position[1], 1e-4);

  // THE RAY. A ray from the head straight at the center of the panel.
  {
    float dir[3] = {panel.position[0] - head[0], panel.position[1] - head[1],
                    panel.position[2] - head[2]};
    const float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    for (int i = 0; i < 3; ++i) dir[i] /= len;
    float u = 0.0f, v = 0.0f, t = 0.0f;
    Check(Hit(head, dir, panel, &u, &v, &t), "a ray at the center meets the panel",
          1, 1);
    Near("at u 0.5", u, 0.5, 1e-4);
    Near("at v 0.5", v, 0.5, 1e-4);
    Near("1.4 m from the head", t, 1.4, 1e-4);
    const int rect[4] = {640, 960, 1280, 960};
    float x = 0.0f, y = 0.0f;
    Pixel(u, v, rect, &x, &y);
    Near("the center of the panel is the center of the rect, x", x, 1280.0,
         0.05);
    Near("the center of the panel is the center of the rect, y", y, 1440.0,
         0.05);
  }

  // A ray at the top left corner of the panel gives the first pixel of the
  // rect, and a ray past the right edge gives the last column.
  {
    float corner[3];
    PointAt(panel, 0.0f, 0.0f, corner);
    float dir[3] = {corner[0] - head[0], corner[1] - head[1],
                    corner[2] - head[2]};
    const float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    for (int i = 0; i < 3; ++i) dir[i] /= len;
    float u = 1.0f, v = 1.0f, t = 0.0f;
    Hit(head, dir, panel, &u, &v, &t);
    Near("the top left corner is u 0", u, 0.0, 1e-4);
    Near("the top left corner is v 0", v, 0.0, 1e-4);
    const int rect[4] = {640, 960, 1280, 960};
    float x = 0.0f, y = 0.0f;
    Pixel(u, v, rect, &x, &y);
    Near("the corner is the first pixel of the rect, x", x, 640.0, 0.05);
    Near("the corner is the first pixel of the rect, y", y, 960.0, 0.05);
    Pixel(1.3f, 0.5f, rect, &x, &y);
    Near("a ray past the right edge stops at the last column", x, 1919.0,
         1e-4);
    Check(!Inside(1.3f, 0.5f), "a ray past the edge is not inside", 0, 0);
    Check(Inside(0.0f, 1.0f), "the edge itself is inside", 1, 1);
  }

  // A hand below and to the right of the head points at a point of the
  // panel: the ray from the hand finds the same point.
  {
    float target[3];
    PointAt(panel, 0.3f, 0.7f, target);
    const float hand[3] = {0.45f, 1.1f, -0.35f};
    float dir[3] = {target[0] - hand[0], target[1] - hand[1],
                    target[2] - hand[2]};
    const float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    for (int i = 0; i < 3; ++i) dir[i] /= len;
    float u = 0.0f, v = 0.0f, t = 0.0f;
    Check(Hit(hand, dir, panel, &u, &v, &t),
          "a ray from the hand meets the panel", 1, 1);
    Near("the hand finds u 0.3", u, 0.3, 1e-4);
    Near("the hand finds v 0.7", v, 0.7, 1e-4);
    Near("the length is the distance to the point", t, len, 1e-4);
  }

  // No hit: a ray away from the panel, a ray along its plane, and a ray
  // from behind it.
  {
    const float back[3] = {0.0f, 0.0f, 1.0f};
    float u, v, t;
    Check(!Hit(head, back, panel, &u, &v, &t),
          "a ray away from the panel does not meet it", 0, 0);
    const float along[3] = {1.0f, 0.0f, 0.0f};
    const float on_plane[3] = {panel.position[0], panel.position[1],
                               panel.position[2]};
    Check(!Hit(on_plane, along, panel, &u, &v, &t),
          "a ray along the plane does not meet it", 0, 0);
    const float behind[3] = {0.2f, 1.4f, -3.0f};
    const float forward_again[3] = {0.0f, 0.0f, 1.0f};
    Check(!Hit(behind, forward_again, panel, &u, &v, &t),
          "a ray from behind the panel meets its back, which is no hit", 0, 0);
  }

  // The aim of a hand pose is its -z axis.
  {
    float dir[3];
    AimForward(level, dir);
    Near("the aim of a level pose is -z", dir[2], -1.0, 1e-6);
  }
}

inline void KeyboardChecks() {
  using namespace vr_keyboard_math;
  printf("\nThe keyboard in the headset\n");
  const int w = 1024, h = 384, margin = 14, gap = 8;

  // Each row fits on the card, and no two keys overlap.
  bool fits = true, apart = true;
  for (int i = 0; i < kKeyCount; ++i) {
    int a[4];
    KeyRect(i, w, h, margin, gap, a);
    if (a[0] < margin || a[1] < margin || a[0] + a[2] > w - margin + 1 ||
        a[1] + a[3] > h - margin + 1 || a[2] <= 0 || a[3] <= 0) {
      fits = false;
    }
    for (int j = i + 1; j < kKeyCount; ++j) {
      int b[4];
      KeyRect(j, w, h, margin, gap, b);
      if (a[0] < b[0] + b[2] && b[0] < a[0] + a[2] && a[1] < b[1] + b[3] &&
          b[1] < a[1] + a[3]) {
        apart = false;
      }
    }
  }
  Check(fits, "each key is on the card", fits, 1);
  Check(apart, "no two keys overlap", apart, 1);

  // The boxes of the parchment keys, with their rim of 3 px, do not overlap,
  // so a copy of one box does not change a different key.
  bool boxes_apart = true;
  for (int i = 0; i < kKeyCount; ++i) {
    int a[4];
    KeyBox(i, w, h, margin, gap, 3, a);
    for (int j = i + 1; j < kKeyCount; ++j) {
      int b[4];
      KeyBox(j, w, h, margin, gap, 3, b);
      if (a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3]) {
        boxes_apart = false;
      }
    }
  }
  Check(boxes_apart, "no two key boxes with a rim of 3 px overlap",
        boxes_apart, 1);

  // The key at the center of each key is that key.
  bool found = true;
  for (int i = 0; i < kKeyCount; ++i) {
    int r[4];
    KeyRect(i, w, h, margin, gap, r);
    const float u = (r[0] + r[2] * 0.5f) / w;
    const float v = (r[1] + r[3] * 0.5f) / h;
    if (KeyAt(u, v, w, h, margin, gap) != i) found = false;
  }
  Check(found, "the center of each key finds that key", found, 1);
  Check(KeyAt(0.001f, 0.001f, w, h, margin, gap) == -1,
        "the margin of the card is no key", 0, 0);
  Check(KeyAt(1.2f, 0.5f, w, h, margin, gap) == -1,
        "a point off the card is no key", 0, 0);

  // What the keys send, as a keyboard of a computer sends it.
  int q = -1, back = -1, enter = -1, space = -1, shift = -1, seven = -1;
  for (int i = 0; i < kKeyCount; ++i) {
    if (kKeys[i].label == 'Q') q = i;
    if (kKeys[i].label == '7') seven = i;
    if (kKeys[i].kind == kBack) back = i;
    if (kKeys[i].kind == kEnter) enter = i;
    if (kKeys[i].kind == kSpace) space = i;
    if (kKeys[i].kind == kShift) shift = i;
  }
  uint32_t vk = 0, ch = 0;
  bool sent = Output(q, false, &vk, &ch) && vk == 'Q';
  Check(sent && ch == 'q', "Q with no shift sends the key Q and the character q",
        ch, 'q');
  sent = Output(q, true, &vk, &ch) && vk == 'Q';
  Check(sent && ch == 'Q', "Q with the shift sends the character Q", ch, 'Q');
  sent = Output(seven, true, &vk, &ch) && vk == '7';
  Check(sent && ch == '7', "a digit is the same with the shift", ch, '7');
  sent = Output(back, false, &vk, &ch) && ch == 0x08;
  Check(sent && vk == 0x08, "Backspace sends VK_BACK and the character 8", vk,
        0x08);
  sent = Output(enter, false, &vk, &ch) && ch == 0x0D;
  Check(sent && vk == 0x0D, "Enter sends VK_RETURN and the character 13", vk,
        0x0D);
  sent = Output(space, false, &vk, &ch) && ch == ' ';
  Check(sent && vk == 0x20, "Space sends VK_SPACE and a space", vk, 0x20);
  Check(!Output(shift, false, &vk, &ch), "Shift sends nothing", 0, 0);

  // The shift: on when the keyboard opens, used up by one letter, and a
  // press of Shift changes it.
  ShiftState s;
  ShiftOpen(&s);
  Check(s.on, "the shift is on when the keyboard opens", s.on, 1);
  ShiftAfter(&s, seven);
  Check(s.on, "a digit does not use up the shift", s.on, 1);
  ShiftAfter(&s, q);
  Check(!s.on, "a letter uses up the shift", s.on, 0);
  ShiftPress(&s);
  Check(s.on, "a press of Shift puts it on", s.on, 1);
  ShiftPress(&s);
  Check(!s.on, "a second press takes it back", s.on, 0);

  // The press: one key for each pull of the trigger, at the start of the
  // pull, and none for a pull that starts off the keys.
  Press p;
  Check(PressStep(&p, false, q) == -1, "no pull: no key", 0, 0);
  Check(PressStep(&p, true, q) == q, "a pull on Q sends Q", 1, 1);
  Check(PressStep(&p, true, q) == -1, "a held pull sends no second Q", 0, 0);
  Check(PressStep(&p, true, back) == -1,
        "a held pull that moves to Backspace sends nothing", 0, 0);
  PressStep(&p, false, -1);
  Check(PressStep(&p, true, -1) == -1, "a pull off the keys sends nothing", 0,
        0);
  PressStep(&p, false, -1);
  Check(PressStep(&p, true, enter) == enter, "the next pull on Enter sends it",
        1, 1);
}

}  // namespace menu_room_checks
