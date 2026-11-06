// The controllers of the player and the map of their buttons: the kind of
// controller of an interaction profile, the buttons that each kind has, the
// map that the player makes on the page Controls of the settings window, and
// the job of each button. See docs/controllers.md, "The map of the
// buttons", and docs/ui-layer.md, "The page Controls". No Windows and no
// game here, so the math test compiles this file (button_map_checks.h).
//
// THE SLOTS. The code of the mod reads four buttons of each controller:
// the first face button (A or X), the second face button (B or Y), the
// stick click and the menu button. Each job reads one of these slots of the
// Hand or of the other hand (controllers.md, 5.1). The map gives each slot
// a button of the same controller. The map of the Hand and the map of the
// other hand are apart, so a job stays with its hand when the Hand changes
// sides.
//
// THE MAP is a number of four digits, one for each button in the order
// first, second, stick click, menu. Each digit is the slot (1 to 4) that the
// button gives. 1234 gives each slot its own button: the layout as made. A
// map is always a permutation, so each job has one button and no job is
// lost. A change on the page swaps the slots of two buttons.

#pragma once

#include <stddef.h>
#include <string.h>
#include <wchar.h>

namespace button_map {

// --- the kinds of controller -----------------------------------------------------

// The kinds, in the order of the families of controller_hints.cpp.
enum Kind { kTouch = 0, kIndex = 1, kWand = 2, kMixed = 3, kSimple = 4,
            kKinds = 5 };

// The words of the log, and the words of the page.
const char* const kKindLog[kKinds] = {"Quest Touch", "Index", "Vive wand",
                                      "Windows Mixed Reality", "generic"};
const wchar_t* const kKindName[kKinds] = {
    L"Touch controllers", L"Index controllers", L"Vive wands",
    L"Mixed Reality controllers", L"Simple controllers"};

// The kind of an interaction profile. The mod suggests bindings for five
// profiles (xr_screen.cpp), so the runtime reports one of them. Touch is
// also the kind of the fake controller ("fake"), of a profile that the
// runtime has not given yet (""), and of any other profile.
inline int KindOfProfile(const char* profile) {
  if (profile == nullptr) return kTouch;
  if (strstr(profile, "simple") != nullptr) return kSimple;
  if (strstr(profile, "index") != nullptr) return kIndex;
  if (strstr(profile, "vive") != nullptr) return kWand;
  if (strstr(profile, "microsoft") != nullptr) return kMixed;
  return kTouch;
}

// --- the buttons -----------------------------------------------------------------

enum Button { kFirst = 0, kSecond = 1, kStickClick = 2, kMenu = 3,
              kButtons = 4 };

// The buttons that a kind gives to the mod (the bindings of xr_screen.cpp).
// A Touch controller gives no menu button of the right hand (the runtime
// keeps it), and an Index controller gives no menu button.
inline bool Has(int kind, bool left, int button) {
  switch (button) {
    case kFirst:
    case kSecond:
      return kind == kTouch || kind == kIndex;
    case kStickClick:
      return kind != kSimple;
    case kMenu:
      return kind == kWand || kind == kMixed || kind == kSimple ||
             (kind == kTouch && left);
    default:
      return false;
  }
}

// The buttons that the map can move. The pad click of a Vive wand stays: the
// pad turns and zooms only while it is clicked (StickLive in
// controllers.cpp), so the pad and its click are one control.
inline bool Movable(int kind, bool left, int button) {
  if (kind == kWand && button == kStickClick) return false;
  return Has(kind, left, button);
}

// The word of a button, as on the controller, or null when the kind does not
// have it.
inline const wchar_t* ButtonWord(int kind, bool left, int button) {
  if (!Has(kind, left, button)) return nullptr;
  switch (button) {
    case kFirst:
      return kind == kTouch && left ? L"X" : L"A";
    case kSecond:
      return kind == kTouch && left ? L"Y" : L"B";
    case kStickClick:
      return kind == kWand ? L"Pad click" : L"Stick click";
    default:
      return L"Menu";
  }
}

// The label of the row of a button: "Right A", "Left stick click".
inline void ButtonLabel(int kind, bool left, int button, wchar_t* out,
                        size_t size) {
  if (out == nullptr || size == 0) return;
  out[0] = 0;
  const wchar_t* word = ButtonWord(kind, left, button);
  if (word == nullptr) return;
  // After the side, a word of more than one letter starts small: "Left
  // stick click", but "Left X".
  const wchar_t* side = left ? L"Left " : L"Right ";
  size_t n = 0;
  for (const wchar_t* p = side; *p != 0 && n + 1 < size; ++p) out[n++] = *p;
  for (const wchar_t* p = word; *p != 0 && n + 1 < size; ++p) {
    const bool lower = p == word && word[1] != 0;
    out[n++] = lower ? static_cast<wchar_t>(*p - L'A' + L'a') : *p;
  }
  out[n] = 0;
}

// --- the map ---------------------------------------------------------------------

const int kAsMade = 1234;

inline void AsMade(int map[kButtons]) {
  for (int b = 0; b < kButtons; ++b) map[b] = b;
}

inline bool IsAsMade(const int map[kButtons]) {
  for (int b = 0; b < kButtons; ++b) {
    if (map[b] != b) return false;
  }
  return true;
}

// The map of a number of bwvr.ini: map[button] is the slot (0 to 3) that the
// button gives. False, and the map as made, for a number that is not a
// permutation of 1, 2, 3 and 4.
inline bool ParseMap(int value, int map[kButtons]) {
  AsMade(map);
  if (value < 1234 || value > 4321) return false;
  int digits[kButtons];
  int v = value;
  for (int b = kButtons - 1; b >= 0; --b) {
    digits[b] = v % 10 - 1;
    v /= 10;
  }
  bool seen[kButtons] = {false, false, false, false};
  for (int b = 0; b < kButtons; ++b) {
    if (digits[b] < 0 || digits[b] >= kButtons || seen[digits[b]]) {
      return false;
    }
    seen[digits[b]] = true;
  }
  for (int b = 0; b < kButtons; ++b) map[b] = digits[b];
  return true;
}

// The number of bwvr.ini of a map.
inline int MapValue(const int map[kButtons]) {
  int v = 0;
  for (int b = 0; b < kButtons; ++b) v = v * 10 + map[b] + 1;
  return v;
}

// A map in 8 bits: 2 bits for the slot of each button.
inline unsigned MapBits(const int map[kButtons]) {
  unsigned bits = 0;
  for (int b = 0; b < kButtons; ++b) {
    bits |= static_cast<unsigned>(map[b] & 3) << (b * 2);
  }
  return bits;
}

// The button that gives a slot, or -1.
inline int ButtonOfSlot(const int map[kButtons], int slot) {
  for (int b = 0; b < kButtons; ++b) {
    if (map[b] == slot) return b;
  }
  return -1;
}

// True when the map moves only buttons that the controller can move. A map
// that moves another button could put a job on a button that the
// controller does not have, so the mod uses the map as made then.
inline bool FitsController(const int map[kButtons], int kind, bool left) {
  for (int b = 0; b < kButtons; ++b) {
    if (map[b] != b && !(Movable(kind, left, b) &&
                         Movable(kind, left, map[b]))) {
      return false;
    }
  }
  return true;
}

// The slots of a controller from its buttons: slot[map[b]] = button[b].
inline void Apply(const int map[kButtons], const bool button[kButtons],
                  bool slot[kButtons]) {
  for (int s = 0; s < kButtons; ++s) slot[s] = false;
  for (int b = 0; b < kButtons; ++b) {
    if (map[b] >= 0 && map[b] < kButtons) slot[map[b]] = button[b];
  }
}

// Button `button` gives `slot` now, and the button that gave that slot
// gives the old slot of `button`. So the map stays a permutation.
inline void GiveSlot(int map[kButtons], int button, int slot) {
  if (button < 0 || button >= kButtons || slot < 0 || slot >= kButtons) {
    return;
  }
  const int other = ButtonOfSlot(map, slot);
  if (other < 0 || other == button) return;
  map[other] = map[button];
  map[button] = slot;
}

// The slots that a button of a controller can give: the slots of the
// buttons that the controller can move, in the order of the slots. The
// answer is their count.
inline int SlotChoices(int kind, bool left, int out[kButtons]) {
  int n = 0;
  for (int s = 0; s < kButtons; ++s) {
    if (Movable(kind, left, s)) out[n++] = s;
  }
  return n;
}

// The next slot of the list for a button that gives `slot` now, or the slot
// before with `back`, round the list. -1 when the list has one slot or less.
inline int StepSlot(int kind, bool left, int slot, bool back) {
  int choices[kButtons];
  const int n = SlotChoices(kind, left, choices);
  if (n < 2) return -1;
  int at = -1;
  for (int k = 0; k < n; ++k) {
    if (choices[k] == slot) at = k;
  }
  if (at < 0) return choices[0];
  return choices[back ? (at + n - 1) % n : (at + 1) % n];
}

// --- the jobs --------------------------------------------------------------------

enum Action {
  kNoJob = 0,
  kTeleport,
  kJump,
  kGesture,
  kRecenter,
  kRing,
  kBook,
  kGameMenu,
  kActions
};

// The words of the page. At most 13 letters, so a word fits between the
// arrows of a row.
const wchar_t* const kActionWord[kActions] = {
    L"No job",   L"Teleport", L"Jump",       L"Gesture",
    L"Recenter", L"Ring",     L"Spell book", L"Game menu"};

// The settings that give the slots their jobs (controllers.md, 5.1).
struct Jobs {
  int layout;          // button_layout
  bool teleport;       // teleport: A teleports, else A jumps
  int gesture_button;  // 0 off, 1 the second face button, 2 the stick click
  int recenter_hold;   // 0 off, 1 the other hand, 2 the Hand
  bool book;           // spell_book
  int ring_button;     // 0 no ring, else radial_menu_button (1 the face
                       // button of the layout, 2 the stick click, 3 the
                       // trigger)
  // The fixed controls.
  bool swap_buttons;   // controller_swap_buttons
  bool world_grab;     // the grip of the other hand grabs the world
  bool walk;           // the stick of the other hand walks
  bool stick_ring;     // the stick of the other hand drives the ring
};

// The job of a slot of a controller of `kind`. `pointing` is the Hand.
// `left` is the left controller: its menu button sends Esc with either hand
// (LatchEsc in controllers.cpp).
inline int SlotAction(const Jobs& j, int kind, bool pointing, bool left,
                      int slot) {
  if (slot == kMenu) return left ? kGameMenu : kNoJob;
  if (pointing) {
    switch (slot) {
      case kFirst:
        return j.teleport ? kTeleport : kJump;
      case kSecond:
        return j.gesture_button == 1 ? kGesture : kNoJob;
      case kStickClick:
        if (j.gesture_button == 2) return kGesture;
        return j.recenter_hold == 2 ? kRecenter : kNoJob;
      default:
        return kNoJob;
    }
  }
  const bool face_ring = j.ring_button == 1;
  switch (slot) {
    case kFirst:
      if (j.layout == 0) return face_ring ? kRing : kNoJob;
      // An Index controller has no menu button: this button sends Esc.
      if (kind == kIndex) return kGameMenu;
      return j.book && kind == kTouch ? kBook : kNoJob;
    case kSecond:
      if (j.layout == 0) return kGameMenu;
      return face_ring ? kRing : kNoJob;
    case kStickClick:
      if (j.ring_button == 2) return kRing;
      return j.recenter_hold == 1 ? kRecenter : kNoJob;
    default:
      return kNoJob;
  }
}

// The job of a button: the job of the slot that the map gives it.
inline int ButtonAction(const Jobs& j, int kind, bool pointing, bool left,
                        const int map[kButtons], int button) {
  if (button < 0 || button >= kButtons) return kNoJob;
  return SlotAction(j, kind, pointing, left, map[button]);
}

// --- the fixed controls ----------------------------------------------------------
//
// The trigger, the grip and the stick of each hand stay with their jobs:
//
//   the trigger and the grip of the Hand   the two buttons of the mouse of the
//                                          game. The grip is the hand that
//                                          takes and throws, so a closed hand
//                                          holds (controller_swap_buttons
//                                          swaps the two)
//   the grip of the other hand             it grabs the land, with a closed
//                                          hand as for a thing in the Hand
//   the sticks                             they push in a direction: a turn,
//                                          a zoom or a walk. A button has no
//                                          direction
//   the trigger of the other hand          it chooses an item of the ring
//                                          with button_layout=0, and it moves
//                                          the index finger of the hand that
//                                          the player sees

enum Fixed { kTrigger = 0, kGrip = 1, kStick = 2, kFixed = 3 };

// The kinds that have each fixed control. A simple controller has one
// button, select, which is its trigger.
inline bool HasFixed(int kind, int control) {
  if (control == kTrigger) return true;
  return kind != kSimple;
}

// The label of the row of a fixed control: "Right trigger", "Left pad".
inline const wchar_t* FixedLabel(int kind, bool left, int control) {
  if (!HasFixed(kind, control)) return nullptr;
  if (control == kTrigger && kind == kSimple) {
    return left ? L"Left select" : L"Right select";
  }
  if (control == kStick && kind == kWand) {
    return left ? L"Left pad" : L"Right pad";
  }
  static const wchar_t* const kLabel[2][kFixed] = {
      {L"Right trigger", L"Right grip", L"Right stick"},
      {L"Left trigger", L"Left grip", L"Left stick"}};
  return kLabel[left ? 1 : 0][control];
}

// The job of a fixed control, in at most 13 letters.
inline const wchar_t* FixedAction(const Jobs& j, bool pointing, int control) {
  if (pointing) {
    switch (control) {
      case kTrigger:
        return j.swap_buttons ? L"Take, throw" : L"Click";
      case kGrip:
        return j.swap_buttons ? L"Click" : L"Take, throw";
      default:
        return L"Turn and zoom";
    }
  }
  switch (control) {
    case kTrigger:
      if (j.ring_button == 3) return L"Ring";
      return j.layout == 0 && j.stick_ring ? L"Ring choice" : L"No job";
    case kGrip:
      return j.world_grab ? L"Move the land" : L"No job";
    default:
      if (j.stick_ring) return L"Ring";
      return j.walk ? L"Walk" : L"No job";
  }
}

// The word of the button that gives a slot of a controller, as the hints
// show it, or null when the controller does not have that button.
inline const wchar_t* SlotWord(int kind, bool left, const int map[kButtons],
                               int slot) {
  return ButtonWord(kind, left, ButtonOfSlot(map, slot));
}

}  // namespace button_map
