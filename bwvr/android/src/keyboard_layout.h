// THE KEYBOARD OF THE APP (docs/android.md, "The keyboard of the app"): the
// layout of its picture, its quad under the screen quad, and the hit of a
// controller ray on it. It has no Android code, so the math test of the PC
// checks it (test/controller_math/keyboard_checks.h).
//
// When a dialog box of the game takes text (the name of a new profile), the
// mod says so in the kScreen of the frame (kScreenText,
// src/mod/scene_stream.h). The app then shows this keyboard on a quad under
// the screen quad. The ray of the hand that points and its trigger type: the
// key goes to the mod through the ring of the keys of the input channel
// (src/common/input_channel.h), and the mod gives it to the game as the
// game takes a key of a keyboard.
#pragma once

#include <math.h>
#include <stdint.h>

#include "../../src/common/input_channel.h"
#include "../../src/mod/controller_math.h"

namespace keyboard_layout {

// The picture.
constexpr int kWidth = 1600;
constexpr int kHeight = 640;

// The keys: a unit key is kUnit wide and kRowHeight high, with kGap between
// two keys. Each row is centered.
constexpr int kUnit = 118;
constexpr int kRowHeight = 104;
constexpr int kGap = 14;
constexpr int kTop = 30;

struct Key {
  uint32_t code;      // the code that goes to the mod
  const char* label;  // the text on the key
  float units;        // the width in unit keys
};

// The rows, top to bottom. Lower case letters: the name of a profile of the
// game takes each letter.
constexpr int kRows = 5;
constexpr int kMaxKeys = 12;
struct Row {
  int count;
  Key keys[kMaxKeys];
};
inline const Row* Rows() {
  static const Row kRowTable[kRows] = {
      {11,
       {{'1', "1", 1}, {'2', "2", 1}, {'3', "3", 1}, {'4', "4", 1}, {'5', "5", 1},
        {'6', "6", 1}, {'7', "7", 1}, {'8', "8", 1}, {'9', "9", 1}, {'0', "0", 1},
        {input_channel::kKeyBack, "Back", 1.8f}}},
      {10,
       {{'q', "q", 1}, {'w', "w", 1}, {'e', "e", 1}, {'r', "r", 1}, {'t', "t", 1},
        {'y', "y", 1}, {'u', "u", 1}, {'i', "i", 1}, {'o', "o", 1}, {'p', "p", 1}}},
      {10,
       {{'a', "a", 1}, {'s', "s", 1}, {'d', "d", 1}, {'f', "f", 1}, {'g', "g", 1},
        {'h', "h", 1}, {'j', "j", 1}, {'k', "k", 1}, {'l', "l", 1},
        {input_channel::kKeyEnter, "Enter", 2.0f}}},
      {7,
       {{'z', "z", 1}, {'x', "x", 1}, {'c', "c", 1}, {'v', "v", 1}, {'b', "b", 1},
        {'n', "n", 1}, {'m', "m", 1}}},
      {1, {{' ', "Space", 6.0f}}},
  };
  return kRowTable;
}

// The count of the keys of all rows.
inline int KeyCount() {
  int n = 0;
  for (int r = 0; r < kRows; ++r) n += Rows()[r].count;
  return n;
}

// The rect of a key: left, top, right, bottom in pixels of the picture.
struct Rect {
  float l, t, r, b;
};
inline float RowWidth(const Row& row) {
  float w = 0.0f;
  for (int k = 0; k < row.count; ++k) w += row.keys[k].units * kUnit;
  return w + static_cast<float>(row.count - 1) * kGap;
}

// The key of index i (0 to KeyCount() - 1, row by row) and its rect. False
// for another index.
inline bool KeyAt(int index, Key* key, Rect* rect) {
  if (index < 0) return false;
  int i = index;
  for (int r = 0; r < kRows; ++r) {
    const Row& row = Rows()[r];
    if (i >= row.count) {
      i -= row.count;
      continue;
    }
    float x = (kWidth - RowWidth(row)) * 0.5f;
    for (int k = 0; k < i; ++k) x += row.keys[k].units * kUnit + kGap;
    const float y = static_cast<float>(kTop + r * (kRowHeight + kGap));
    *key = row.keys[i];
    rect->l = x;
    rect->t = y;
    rect->r = x + row.keys[i].units * kUnit;
    rect->b = y + kRowHeight;
    return true;
  }
  return false;
}

// The index of the key at pixel x, y of the picture, or -1 (between two keys
// or off the keys).
inline int KeyIndexAt(float x, float y) {
  const int n = KeyCount();
  for (int i = 0; i < n; ++i) {
    Key key;
    Rect rect;
    if (KeyAt(i, &key, &rect) && x >= rect.l && x < rect.r && y >= rect.t &&
        y < rect.b) {
      return i;
    }
  }
  return -1;
}

// THE QUAD of the keyboard in the LOCAL space, from the quad of the screen
// (the pose and the size of its kScreen). It is kWidthOfScreen of the width
// of the screen quad, its top is kGapMeters under the bottom of the screen
// quad, and it turns about its x axis so that its front faces the origin of
// the LOCAL space, where the head is at the start.
constexpr float kWidthOfScreen = 0.6f;
constexpr float kGapMeters = 0.05f;
struct Quad {
  float position[3];
  float orientation[4];
  float size[2];
};
inline bool QuadUnder(const float screen_position[3], const float screen_size[2],
                      Quad* q) {
  if (!(screen_size[0] > 0.0f) || !(screen_size[1] > 0.0f)) return false;
  q->size[0] = screen_size[0] * kWidthOfScreen;
  q->size[1] = q->size[0] * kHeight / kWidth;
  q->position[0] = screen_position[0];
  q->position[1] = screen_position[1] - screen_size[1] * 0.5f - kGapMeters - q->size[1] * 0.5f;
  q->position[2] = screen_position[2];
  // A turn about x by a: the front (0, 0, 1) goes to (0, -sin a, cos a). The
  // front must point from the quad to the origin.
  const float y = -q->position[1];
  const float z = -q->position[2];
  const float a = z > 1e-4f ? -atanf(y / z) : 0.0f;
  q->orientation[0] = sinf(a * 0.5f);
  q->orientation[1] = 0.0f;
  q->orientation[2] = 0.0f;
  q->orientation[3] = cosf(a * 0.5f);
  return true;
}

// THE QUAD of the keyboard in a frame of the scene, from the menu panel quad
// of the mod (the pose and the size of quads[0] of its kPanel). The menu
// panel quad is large (the menu rect, 1280 by 960 pixels at 16 pixels a
// degree is about 80 by 60 degrees), and the dialog box of the game is a
// small part of it in its middle. So the keyboard is not under the whole
// quad but in front of its lower part: kWidthOfPanel of its width, the top
// kPanelDrop of its height under its center, kPanelFrontMeters in front of
// it toward the head, and with the turn of the panel quad. The mod knows this
// quad too: a ray on the keyboard is not on the panel (controllers.cpp).
constexpr float kWidthOfPanel = 0.5f;
constexpr float kPanelDrop = 0.35f;
constexpr float kPanelFrontMeters = 0.03f;
inline bool QuadOnPanel(const float panel_position[3], const float panel_orientation[4],
                        const float panel_size[2], Quad* q) {
  if (!(panel_size[0] > 0.0f) || !(panel_size[1] > 0.0f)) return false;
  q->size[0] = panel_size[0] * kWidthOfPanel;
  q->size[1] = q->size[0] * kHeight / kWidth;
  const float local[3] = {0.0f, -panel_size[1] * kPanelDrop - q->size[1] * 0.5f,
                          kPanelFrontMeters};
  float turned[3];
  controller_math::QuatRotate(panel_orientation, local, turned);
  for (int i = 0; i < 3; ++i) q->position[i] = panel_position[i] + turned[i];
  for (int i = 0; i < 4; ++i) q->orientation[i] = panel_orientation[i];
  return true;
}

// The pixel of the picture where a ray hits the quad. False when it misses.
inline bool RayToPixel(const float origin[3], const float direction[3], const Quad& q,
                       float* x, float* y) {
  const controller_math::QuadHit hit =
      controller_math::RayQuad(origin, direction, q.position, q.orientation, q.size);
  if (!hit.inside) return false;
  *x = hit.u * kWidth;
  *y = hit.v * kHeight;
  return true;
}

// The point on the quad of a pixel of the picture, in the LOCAL space, for
// the cursor of the app.
inline void PixelToPoint(const Quad& q, float x, float y, float out[3]) {
  const float local[3] = {(x / kWidth - 0.5f) * q.size[0], (0.5f - y / kHeight) * q.size[1],
                          0.0f};
  float turned[3];
  controller_math::QuatRotate(q.orientation, local, turned);
  for (int i = 0; i < 3; ++i) out[i] = q.position[i] + turned[i];
}

}  // namespace keyboard_layout
