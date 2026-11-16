// The keyboard in the headset: its keys, where each key is on the card, and
// what a key sends. See docs/ui-layer.md, "The keyboard in the headset". No
// Windows and no game here, so the math test compiles this file.

#pragma once

#include <stdint.h>

namespace vr_keyboard_math {

enum KeyKind { kChar = 0, kSpace = 1, kBack = 2, kEnter = 3, kShift = 4 };

struct Key {
  char label;     // the letter or the digit, 0 for a key with a word
  int kind;       // KeyKind
  float x;        // the left edge, in key widths from the left of the row
  int row;        // 0 is the top row
  float width;    // in key widths
};

// Five rows, as on a keyboard of a computer: the digits and Backspace, the
// three rows of letters with Enter and Shift, and the space bar. A row is
// 11.5 key widths.
const int kRows = 5;
const float kRowUnits = 11.5f;
const Key kKeys[] = {
    {'1', kChar, 0.0f, 0, 1.0f},  {'2', kChar, 1.0f, 0, 1.0f},
    {'3', kChar, 2.0f, 0, 1.0f},  {'4', kChar, 3.0f, 0, 1.0f},
    {'5', kChar, 4.0f, 0, 1.0f},  {'6', kChar, 5.0f, 0, 1.0f},
    {'7', kChar, 6.0f, 0, 1.0f},  {'8', kChar, 7.0f, 0, 1.0f},
    {'9', kChar, 8.0f, 0, 1.0f},  {'0', kChar, 9.0f, 0, 1.0f},
    {0, kBack, 10.0f, 0, 1.5f},
    {'Q', kChar, 0.5f, 1, 1.0f},  {'W', kChar, 1.5f, 1, 1.0f},
    {'E', kChar, 2.5f, 1, 1.0f},  {'R', kChar, 3.5f, 1, 1.0f},
    {'T', kChar, 4.5f, 1, 1.0f},  {'Y', kChar, 5.5f, 1, 1.0f},
    {'U', kChar, 6.5f, 1, 1.0f},  {'I', kChar, 7.5f, 1, 1.0f},
    {'O', kChar, 8.5f, 1, 1.0f},  {'P', kChar, 9.5f, 1, 1.0f},
    {'A', kChar, 0.75f, 2, 1.0f}, {'S', kChar, 1.75f, 2, 1.0f},
    {'D', kChar, 2.75f, 2, 1.0f}, {'F', kChar, 3.75f, 2, 1.0f},
    {'G', kChar, 4.75f, 2, 1.0f}, {'H', kChar, 5.75f, 2, 1.0f},
    {'J', kChar, 6.75f, 2, 1.0f}, {'K', kChar, 7.75f, 2, 1.0f},
    {'L', kChar, 8.75f, 2, 1.0f}, {0, kEnter, 9.75f, 2, 1.75f},
    {0, kShift, 0.0f, 3, 1.5f},   {'Z', kChar, 1.5f, 3, 1.0f},
    {'X', kChar, 2.5f, 3, 1.0f},  {'C', kChar, 3.5f, 3, 1.0f},
    {'V', kChar, 4.5f, 3, 1.0f},  {'B', kChar, 5.5f, 3, 1.0f},
    {'N', kChar, 6.5f, 3, 1.0f},  {'M', kChar, 7.5f, 3, 1.0f},
    {0, kSpace, 2.5f, 4, 6.5f},
};
const int kKeyCount = static_cast<int>(sizeof(kKeys) / sizeof(kKeys[0]));

// The rect of key i on a card of width by height pixels, with `margin`
// pixels around the keys and `gap` pixels between two keys: x, y, w, h.
inline void KeyRect(int i, int width, int height, int margin, int gap,
                    int out[4]) {
  const Key& k = kKeys[i];
  const float unit = (width - 2.0f * margin) / kRowUnits;
  const float row_h = (height - 2.0f * margin) / static_cast<float>(kRows);
  const float x0 = margin + k.x * unit;
  const float y0 = margin + k.row * row_h;
  out[0] = static_cast<int>(x0 + gap * 0.5f);
  out[1] = static_cast<int>(y0 + gap * 0.5f);
  out[2] = static_cast<int>(k.width * unit - gap);
  out[3] = static_cast<int>(row_h - gap);
}

// The box of key i with `rim` more pixels at each side, cut to the card: x0,
// y0, x1, y1. The parchment keyboard draws a key and its rim in this box, and
// a new picture of the card copies the box of each key that changed.
inline void KeyBox(int i, int width, int height, int margin, int gap, int rim,
                   int out[4]) {
  int r[4];
  KeyRect(i, width, height, margin, gap, r);
  out[0] = r[0] - rim < 0 ? 0 : r[0] - rim;
  out[1] = r[1] - rim < 0 ? 0 : r[1] - rim;
  out[2] = r[0] + r[2] + rim > width ? width : r[0] + r[2] + rim;
  out[3] = r[1] + r[3] + rim > height ? height : r[1] + r[3] + rim;
}

// The key at u, v of the card (0 to 1 across and down), or -1 for the space
// between the keys and for a point off the card.
inline int KeyAt(float u, float v, int width, int height, int margin,
                 int gap) {
  if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) return -1;
  const float px = u * width;
  const float py = v * height;
  for (int i = 0; i < kKeyCount; ++i) {
    int r[4];
    KeyRect(i, width, height, margin, gap, r);
    if (px >= r[0] && px < r[0] + r[2] && py >= r[1] && py < r[1] + r[3]) {
      return i;
    }
  }
  return -1;
}

// What key i sends, as a keyboard of a computer sends it: the virtual key of
// WM_KEYDOWN and WM_KEYUP, and the character of WM_CHAR (0 for none).
// `shift` makes a letter a capital. Shift itself sends nothing: it changes
// the next letter.
inline bool Output(int i, bool shift, uint32_t* vk, uint32_t* ch) {
  if (i < 0 || i >= kKeyCount) return false;
  const Key& k = kKeys[i];
  switch (k.kind) {
    case kChar:
      *vk = static_cast<uint32_t>(k.label);
      if (k.label >= 'A' && k.label <= 'Z' && !shift) {
        *ch = static_cast<uint32_t>(k.label - 'A' + 'a');
      } else {
        *ch = static_cast<uint32_t>(k.label);
      }
      return true;
    case kSpace:
      *vk = 0x20;  // VK_SPACE
      *ch = ' ';
      return true;
    case kBack:
      *vk = 0x08;  // VK_BACK
      *ch = 0x08;
      return true;
    case kEnter:
      *vk = 0x0D;  // VK_RETURN
      *ch = 0x0D;
      return true;
    default:
      return false;
  }
}

// THE SHIFT. A press of Shift makes the next letter a capital, and a second
// press takes that back. A letter uses the shift up. The shift is on when
// the keyboard opens, so the first letter of a name is a capital.
struct ShiftState {
  bool on = true;
};

inline void ShiftPress(ShiftState* s) { s->on = !s->on; }

// The keyboard opens again: the shift is on.
inline void ShiftOpen(ShiftState* s) { s->on = true; }

// After key i went out: a letter uses up the shift.
inline void ShiftAfter(ShiftState* s, int i) {
  if (i < 0 || i >= kKeyCount) return;
  if (kKeys[i].kind == kChar && kKeys[i].label >= 'A' &&
      kKeys[i].label <= 'Z') {
    s->on = false;
  }
}

// THE PRESS of a key with the trigger: a press starts on the key under the
// ray, and the key goes out at that press. A press that starts off the keys
// sends nothing, and the trigger must go up before the next press.
struct Press {
  bool down = false;  // the trigger was down in the last frame
};

// One frame of the trigger. Returns the key to send now, or -1.
inline int PressStep(Press* p, bool trigger_down, int key_under_ray) {
  const bool edge = trigger_down && !p->down;
  p->down = trigger_down;
  if (!edge) return -1;
  return key_under_ray;
}

}  // namespace vr_keyboard_math
