// The panel of places: its buttons, where each button is on the picture,
// what each button sends to the game, and the place of the panel in the
// room. See docs/ui-layer.md, "The panel of places". No Windows and no game
// here, so the math test compiles this file (places_checks.h).
//
// THE PICTURE is 1024 by 1024 pixels, from the top:
//
//   the title          "Places"
//   the places         Temple, Creature, Town, Land
//   the header         "Bookmarks"
//   7 bookmark rows    a wax seal with the number, then Go, Set here,
//                      Follow, Remove
//   the bottom row     Back, the note, Close
//
// The note says what the button under the ray does, or why it cannot act
// now, or what the last press did.

#pragma once

#include <stddef.h>
#include <string.h>

#include "menu_room_math.h"
#include "radial_menu_math.h"

namespace places_math {

const int kWidth = 1024;
const int kHeight = 1024;

// THE PLACE IN THE ROOM. The panel opens 1 m in front of the head, 12
// degrees below eye level, tilted to face the head, and it stays there. It
// is 56 cm across (31 degrees at 1 m). So it goes from about 3 degrees above
// eye level to about 27 degrees below it. A hand that points at it is lower
// than its bottom edge, so the panel (a quad layer over the world) does not
// hide that hand. A button is 11 by 4 cm, near the size of a key of the
// keyboard in the headset.
const float kDistance = 1.0f;          // meters from the head
const float kPitch = -0.20944f;        // 12 degrees below eye level
const float kMeters = 0.56f;           // the width and the height

// The kinds of the buttons.
enum Kind {
  kTemple,
  kCreature,
  kTown,
  kLand,
  kGo,
  kSet,
  kFollow,
  kRemove,
  kBack,
  kClose,
  kKinds
};

const int kMarks = radial_math::kMarks;  // the 7 bookmarks of the game
const int kPlaces = 4;
const int kMarkKinds = 4;                // Go, Set here, Follow, Remove
// The buttons: the 4 places, 4 for each bookmark, then Back and Close.
const int kFirstMark = kPlaces;
const int kBackButton = kFirstMark + kMarks * kMarkKinds;
const int kCloseButton = kBackButton + 1;
const int kButtons = kCloseButton + 1;

// The words on the buttons, and the words of the test key
// places_test_buttons (a bookmark word has its number after it: "go3").
const wchar_t* const kKindWord[kKinds] = {
    L"Temple", L"Creature", L"Town", L"Land",  L"Go",
    L"Set here", L"Follow", L"Remove", L"Back", L"Close"};
const char* const kKindId[kKinds] = {"temple", "creature", "town",   "land",
                                     "go",     "set",      "follow", "remove",
                                     "back",   "close"};

inline int KindOf(int button) {
  if (button < 0 || button >= kButtons) return -1;
  if (button < kFirstMark) return kTemple + button;
  if (button < kBackButton) return kGo + (button - kFirstMark) % kMarkKinds;
  return button == kBackButton ? kBack : kClose;
}

// The bookmark of a button, 0 to 6, or -1 for a button with no bookmark.
inline int MarkOf(int button) {
  if (button < kFirstMark || button >= kBackButton) return -1;
  return (button - kFirstMark) / kMarkKinds;
}

inline int ButtonOf(int kind, int mark) {
  if (kind >= kTemple && kind <= kLand) return kind - kTemple;
  if (kind >= kGo && kind <= kRemove) {
    if (mark < 0 || mark >= kMarks) return -1;
    return kFirstMark + mark * kMarkKinds + (kind - kGo);
  }
  if (kind == kBack) return kBackButton;
  if (kind == kClose) return kCloseButton;
  return -1;
}

// --- the layout ------------------------------------------------------------------

const int kLeft = 40;                  // the left edge of the content
const int kRight = 984;                // and its right edge
const int kTitleTop = 34;
const int kTitleBottom = 102;
const int kPlacesTop = 110;
const int kRowHeight = 84;             // the places and the bottom row
const int kHeaderTop = 204;
const int kHeaderBottom = 250;
const int kMarksTop = 254;
const int kMarkPitch = 84;             // from one bookmark row to the next
const int kMarkHeight = 72;
const int kSealWidth = 80;             // the wax seal with the number
const int kMarkButtonsLeft = kLeft + kSealWidth + 16;
const int kPlaceGap = 16;
const int kMarkGap = 14;
const int kBottomTop = kMarksTop + kMarks * kMarkPitch + 10;
const int kBottomWidth = 220;          // Back and Close
// The rim of a button of the parchment look goes this far out from it
// (card_scroll::kKeyRim).
const int kRim = 3;

// The rect of button i: x, y, w, h.
inline void ButtonRect(int i, int out[4]) {
  out[0] = out[1] = out[2] = out[3] = 0;
  const int kind = KindOf(i);
  if (kind < 0) return;
  if (kind <= kLand) {
    const int w = (kRight - kLeft - (kPlaces - 1) * kPlaceGap) / kPlaces;
    out[0] = kLeft + (kind - kTemple) * (w + kPlaceGap);
    out[1] = kPlacesTop;
    out[2] = w;
    out[3] = kRowHeight;
    return;
  }
  if (kind <= kRemove) {
    const int w = (kRight - kMarkButtonsLeft - (kMarkKinds - 1) * kMarkGap) /
                  kMarkKinds;
    out[0] = kMarkButtonsLeft + (kind - kGo) * (w + kMarkGap);
    out[1] = kMarksTop + MarkOf(i) * kMarkPitch;
    out[2] = w;
    out[3] = kMarkHeight;
    return;
  }
  out[0] = kind == kBack ? kLeft : kRight - kBottomWidth;
  out[1] = kBottomTop;
  out[2] = kBottomWidth;
  out[3] = kRowHeight;
}

// The box of button i with its rim, cut to the picture: x0, y0, x1, y1. A
// new picture copies the box of each button whose look changed.
inline void ButtonBox(int i, int out[4]) {
  int r[4];
  ButtonRect(i, r);
  out[0] = r[0] - kRim < 0 ? 0 : r[0] - kRim;
  out[1] = r[1] - kRim < 0 ? 0 : r[1] - kRim;
  out[2] = r[0] + r[2] + kRim > kWidth ? kWidth : r[0] + r[2] + kRim;
  out[3] = r[1] + r[3] + kRim > kHeight ? kHeight : r[1] + r[3] + kRim;
}

// The wax seal of bookmark m: x0, y0, x1, y1.
inline void SealBox(int mark, int out[4]) {
  const int y = kMarksTop + mark * kMarkPitch;
  out[0] = kLeft;
  out[1] = y + 6;
  out[2] = kLeft + kSealWidth;
  out[3] = y + kMarkHeight - 6;
}

// The note between Back and Close: x0, y0, x1, y1.
inline void NoteBox(int out[4]) {
  out[0] = kLeft + kBottomWidth + 12;
  out[1] = kBottomTop;
  out[2] = kRight - kBottomWidth - 12;
  out[3] = kBottomTop + kRowHeight;
}

// The button at u, v of the panel (0 to 1 across and down), or -1 for the
// paper between the buttons and for a point off the panel.
inline int ButtonAt(float u, float v) {
  if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) return -1;
  const float px = u * kWidth;
  const float py = v * kHeight;
  for (int i = 0; i < kButtons; ++i) {
    int r[4];
    ButtonRect(i, r);
    if (px >= r[0] && px < r[0] + r[2] && py >= r[1] && py < r[1] + r[3]) {
      return i;
    }
  }
  return -1;
}

// The middle of button i in u, v.
inline void ButtonMiddle(int i, float* u, float* v) {
  int r[4];
  ButtonRect(i, r);
  *u = (r[0] + r[2] * 0.5f) / kWidth;
  *v = (r[1] + r[3] * 0.5f) / kHeight;
}

// --- what a button does ----------------------------------------------------------

// THE KEYS OF A BOOKMARK. GGame::ProcessKey 0x0063EF20 reads the modifier
// word of the key buffer for the keys 1 to 7: none goes to the bookmark
// (fn_0043A070), Ctrl sets it at the Hand (fn_00439E50), Shift follows its
// object (fn_0043A390), and Ctrl with Shift removes it (fn_00439D40). The
// game acts on Go, Follow and Remove only for a bookmark that is set. False
// for a button that is not a button of a bookmark.
inline bool MarkKey(int kind, int mark, int* key, int* mods) {
  if (mark < 0 || mark >= kMarks || kind < kGo || kind > kRemove) return false;
  *key = 0x02 + mark;  // the scan code of the key 1 is 2
  *mods = kind == kGo       ? 0
          : kind == kSet    ? radial_math::kModCtrl
          : kind == kFollow ? radial_math::kModShift
                            : radial_math::kModCtrl | radial_math::kModShift;
  return true;
}

// The item of the ring that a button uses, or kItemNone: the places and
// Back act as those items act on the ring.
inline int RingItemOf(int kind) {
  switch (kind) {
    case kTemple: return radial_math::kItemTemple;
    case kCreature: return radial_math::kItemCreature;
    case kTown: return radial_math::kItemTown;
    case kLand: return radial_math::kItemLand;
    case kBack: return radial_math::kItemMarkBack;
    default: return radial_math::kItemNone;
  }
}

// A press that moves the view closes the panel. Set here and Remove keep it
// open, so the player sees the bookmark change.
inline bool Closes(int kind) {
  return kind != kSet && kind != kRemove;
}

// Whether a button of a bookmark can act: the game takes the bookmark keys
// now (`usable`), and Go, Follow and Remove need a bookmark that is set.
inline bool MarkCan(int kind, bool usable, bool set) {
  if (!usable) return false;
  return kind == kSet || set;
}

// --- the trigger -----------------------------------------------------------------

// THE PRESS of a button with the trigger: a press starts on the button
// under the ray and acts at once. A press that starts off the buttons does
// nothing, and the trigger must go up before the next press. The trigger is
// down above 0.6 and up below 0.4.
struct Press {
  bool down = false;  // the trigger is down
  bool was = false;   // it was down in the last frame
};

// One frame of the trigger. Returns the button to act on now, or -1.
inline int PressStep(Press* p, float trigger, int button_under_ray) {
  if (trigger > 0.6f) p->down = true;
  if (trigger < 0.4f) p->down = false;
  const bool edge = p->down && !p->was;
  p->was = p->down;
  return edge ? button_under_ray : -1;
}

// --- the place -------------------------------------------------------------------

// The panel for a head at `head` with a yaw of `yaw` radians: in front of
// the yaw, at kDistance and kPitch, facing the head.
inline void PlaceAt(const float head[3], float yaw,
                    menu_room_math::Panel* out) {
  menu_room_math::PoseAt(head, yaw, kDistance, kPitch, out);
  out->width = kMeters;
  out->height = kMeters;
}

// The point of the room at u, v of the panel.
inline void PanelPoint(const menu_room_math::Panel& panel, float u, float v,
                    float out[3]) {
  const float k[3] = {(u - 0.5f) * panel.width, (0.5f - v) * panel.height,
                      0.0f};
  float r[3];
  controller_math::QuatRotate(panel.orientation, k, r);
  for (int i = 0; i < 3; ++i) out[i] = panel.position[i] + r[i];
}

// --- the test key ----------------------------------------------------------------

// The button of a word of places_test_buttons ("temple", "go3", "close"), or
// -1. A bookmark word has the number 1 to 7 after it.
inline int ButtonOfWord(const char* word, size_t length) {
  for (int kind = 0; kind < kKinds; ++kind) {
    const size_t n = strlen(kKindId[kind]);
    if (kind >= kGo && kind <= kRemove) {
      if (length == n + 1 && strncmp(word, kKindId[kind], n) == 0 &&
          word[n] >= '1' && word[n] <= '7') {
        return ButtonOf(kind, word[n] - '1');
      }
    } else if (length == n && strncmp(word, kKindId[kind], n) == 0) {
      return ButtonOf(kind, -1);
    }
  }
  return -1;
}

// The words of a list with spaces or commas. out gets the buttons, and the
// answer is their count. A word that is not known is not in the list.
inline int ParseButtons(const char* text, int* out, int most) {
  int count = 0;
  const char* p = text != nullptr ? text : "";
  while (*p != 0 && count < most) {
    while (*p == ' ' || *p == ',' || *p == '\t') ++p;
    const char* end = p;
    while (*end != 0 && *end != ' ' && *end != ',' && *end != '\t') ++end;
    if (end > p) {
      const int b = ButtonOfWord(p, static_cast<size_t>(end - p));
      if (b >= 0) out[count++] = b;
    }
    p = end;
  }
  return count;
}

}  // namespace places_math
