// THE MAP OF THE BUTTONS AND THE DRAWING OF THE CONTROLLERS
// (docs/controllers.md, "The map of the buttons", and docs/ui-layer.md, "The
// page Controls"): the kind of each profile, the buttons of each kind, the
// map and its swaps, the jobs of the slots as made, the words, and the
// shapes and the lines of words of the drawing. main.cpp includes this file
// after Check and Near, just before main.
#pragma once

#include <math.h>
#include <string.h>
#include <wchar.h>

#include "../../src/mod/button_map_math.h"
#include "../../src/mod/controller_art_math.h"
#include "../../src/mod/debug_window_math.h"

namespace button_map_checks {

inline bool Same(const wchar_t* a, const wchar_t* b) {
  if (a == nullptr || b == nullptr) return a == b;
  return wcscmp(a, b) == 0;
}

inline void ButtonMapChecks() {
  using namespace button_map;
  printf("\nThe map of the buttons and the drawing of the controllers\n");

  // The kinds of the five profiles that the mod suggests, the fake
  // controller and no profile yet.
  const bool kinds =
      KindOfProfile("/interaction_profiles/oculus/touch_controller") ==
          kTouch &&
      KindOfProfile("/interaction_profiles/valve/index_controller") ==
          kIndex &&
      KindOfProfile("/interaction_profiles/htc/vive_controller") == kWand &&
      KindOfProfile("/interaction_profiles/microsoft/motion_controller") ==
          kMixed &&
      KindOfProfile("/interaction_profiles/khr/simple_controller") ==
          kSimple &&
      KindOfProfile("fake") == kTouch && KindOfProfile("") == kTouch &&
      KindOfProfile(nullptr) == kTouch;
  Check(kinds, "each profile has its kind, and the fake controller is Touch",
        kinds, 1);

  // The buttons follow the bindings of xr_screen.cpp.
  const bool has =
      Has(kTouch, true, kMenu) && !Has(kTouch, false, kMenu) &&
      Has(kTouch, false, kFirst) && !Has(kIndex, true, kMenu) &&
      Has(kIndex, false, kSecond) && !Has(kWand, false, kFirst) &&
      Has(kWand, false, kStickClick) && Has(kWand, false, kMenu) &&
      !Has(kMixed, true, kSecond) && Has(kMixed, true, kMenu) &&
      !Has(kSimple, true, kStickClick) && Has(kSimple, false, kMenu);
  Check(has, "the buttons of each kind are the buttons of its bindings", has,
        1);
  Check(!Movable(kWand, false, kStickClick) && Movable(kMixed, false,
                                                       kStickClick),
        "the pad click of a wand stays with its pad", 1, 1);

  // Each of the 24 maps goes to its number and back.
  int round = 0, count = 0;
  for (int a = 0; a < 4; ++a)
    for (int b = 0; b < 4; ++b)
      for (int c = 0; c < 4; ++c)
        for (int d = 0; d < 4; ++d) {
          if (a == b || a == c || a == d || b == c || b == d || c == d) {
            continue;
          }
          ++count;
          const int m[4] = {a, b, c, d};
          int back[4];
          if (ParseMap(MapValue(m), back) && memcmp(m, back, sizeof(m)) == 0) {
            ++round;
          }
        }
  Check(round == 24 && count == 24, "each of the 24 maps goes to its number "
        "and back", round, 24);
  int m[4];
  const bool bad = !ParseMap(1123, m) && IsAsMade(m) && !ParseMap(5234, m) &&
                   !ParseMap(0, m) && !ParseMap(12345, m) &&
                   !ParseMap(1240, m) && IsAsMade(m);
  Check(bad, "a number that is not a map gives the map as made", bad, 1);
  Check(ParseMap(kAsMade, m) && IsAsMade(m), "1234 is the map as made", 1, 1);

  // 2134: the first face button gives the slot of the second, and the
  // second the slot of the first.
  ParseMap(2134, m);
  const bool down[4] = {true, false, false, false};
  bool slot[4];
  Apply(m, down, slot);
  Check(!slot[kFirst] && slot[kSecond] && !slot[kStickClick] && !slot[kMenu],
        "with 2134 a press of A is the slot of B", slot[kSecond], 1);
  Check(ButtonOfSlot(m, kFirst) == kSecond && ButtonOfSlot(m, kSecond) ==
                                                  kFirst,
        "with 2134 B gives the slot of A", ButtonOfSlot(m, kFirst), kSecond);

  // A step goes round the slots of the controller, and the map stays a
  // permutation.
  int map[4];
  AsMade(map);
  bool perm = true;
  int seen = 0;
  for (int step = 0; step < 6; ++step) {
    const int to = StepSlot(kTouch, false, map[kSecond], false);
    GiveSlot(map, kSecond, to);
    int back[4];
    if (!ParseMap(MapValue(map), back)) perm = false;
    if (map[kMenu] != kMenu) perm = false;
    if (map[kSecond] == kFirst) seen |= 1;
    if (map[kSecond] == kStickClick) seen |= 4;
    if (map[kSecond] == kSecond) seen |= 2;
  }
  Check(perm && seen == 7,
        "the steps of right B go round A, B and the stick click, and the "
        "right menu slot stays", seen, 7);
  Check(StepSlot(kTouch, true, kFirst, true) == kMenu &&
            StepSlot(kTouch, true, kMenu, false) == kFirst,
        "on the left Touch controller the step goes round four buttons", 1,
        1);
  Check(StepSlot(kWand, false, kMenu, false) < 0 &&
            StepSlot(kSimple, true, kMenu, false) < 0,
        "a wand and a simple controller have no second button to step to", 1,
        1);

  // A map that moves a button that the controller does not have acts as
  // made.
  ParseMap(1243, m);
  const bool fits = FitsController(m, kTouch, true) &&
                    !FitsController(m, kTouch, false) &&
                    !FitsController(m, kIndex, true);
  Check(fits && !FitsController(m, kWand, true) &&
            FitsController(m, kMixed, true),
        "a map fits only a controller that can move each of its buttons",
        fits, 1);

  // The jobs as made: the table of controllers.md, 5.1.
  Jobs j = {};
  j.layout = 1;
  j.teleport = true;
  j.gesture_button = 1;
  j.recenter_hold = 1;
  j.book = true;
  j.ring_button = 1;
  j.world_grab = true;
  j.walk = true;
  const bool made =
      SlotAction(j, kTouch, true, false, kFirst) == kTeleport &&
      SlotAction(j, kTouch, true, false, kSecond) == kGesture &&
      SlotAction(j, kTouch, true, false, kStickClick) == kNoJob &&
      SlotAction(j, kTouch, false, true, kFirst) == kBook &&
      SlotAction(j, kTouch, false, true, kSecond) == kRing &&
      SlotAction(j, kTouch, false, true, kStickClick) == kRecenter &&
      SlotAction(j, kTouch, false, true, kMenu) == kGameMenu &&
      SlotAction(j, kIndex, false, true, kFirst) == kGameMenu;
  Check(made, "the jobs of the slots as made are the jobs of the guide",
        made, 1);
  Jobs j0 = j;
  j0.layout = 0;
  j0.teleport = false;
  j0.gesture_button = 2;
  j0.recenter_hold = 2;
  const bool old = SlotAction(j0, kTouch, false, true, kFirst) == kRing &&
                   SlotAction(j0, kTouch, false, true, kSecond) == kGameMenu &&
                   SlotAction(j0, kTouch, true, false, kFirst) == kJump &&
                   SlotAction(j0, kTouch, true, false, kStickClick) ==
                       kGesture &&
                   SlotAction(j0, kTouch, true, true, kMenu) == kGameMenu;
  Check(old, "button_layout=0, teleport=0 and gesture_button=2 move the jobs "
        "as the guide says", old, 1);

  // The words of the buttons as made are the letters of the hints.
  int asmade[4];
  AsMade(asmade);
  const bool words = Same(SlotWord(kTouch, false, asmade, kFirst), L"A") &&
                     Same(SlotWord(kTouch, true, asmade, kSecond), L"Y") &&
                     Same(SlotWord(kTouch, true, asmade, kMenu), L"Menu") &&
                     SlotWord(kTouch, false, asmade, kMenu) == nullptr &&
                     Same(SlotWord(kIndex, true, asmade, kFirst), L"A") &&
                     Same(SlotWord(kWand, true, asmade, kStickClick),
                          L"Pad click");
  ParseMap(3214, m);
  Check(words && Same(SlotWord(kTouch, false, m, kFirst), L"Stick click"),
        "the word of a slot is the word of the button that gives it", words,
        1);
  wchar_t label[24];
  ButtonLabel(kTouch, false, kFirst, label, 24);
  bool labels = Same(label, L"Right A");
  ButtonLabel(kTouch, true, kStickClick, label, 24);
  labels = labels && Same(label, L"Left stick click");
  ButtonLabel(kWand, false, kStickClick, label, 24);
  labels = labels && Same(label, L"Right pad click");
  ButtonLabel(kTouch, false, kMenu, label, 24);
  labels = labels && label[0] == 0;
  Check(labels, "the labels of the rows name the side and the button", labels,
        1);

  // The ring of a controller with no face buttons is on its stick or pad
  // with each layout, and on a Touch controller only with button_layout=0.
  Jobs pad = j;
  pad.stick_ring = true;
  const bool wand = Same(FixedAction(pad, kWand, false, kStick), L"Ring") &&
                    Same(FixedAction(pad, kMixed, false, kStick), L"Ring") &&
                    Same(FixedAction(pad, kTouch, false, kStick), L"Walk") &&
                    Same(FixedAction(pad, kWand, false, kTrigger),
                         L"Ring choice");
  pad.layout = 0;
  Check(wand && Same(FixedAction(pad, kTouch, false, kStick), L"Ring"),
        "the pad of a wand is the ring, and the stick of Touch walks with "
        "button_layout=1", wand, 1);

  // Each word fits its place: a job between the arrows (13 letters), a
  // label in its row (18 letters).
  bool fit = true;
  for (int a = 0; a < kActions; ++a) {
    if (wcslen(kActionWord[a]) > 13) fit = false;
  }
  Jobs all[3] = {j, j0, j};
  all[2].swap_buttons = true;
  all[2].ring_button = 3;
  all[2].stick_ring = true;
  for (int k = 0; k < 3; ++k) {
    for (int p = 0; p < 2; ++p) {
      for (int f = 0; f < kFixed; ++f) {
        for (int kind = 0; kind < kKinds; ++kind) {
          if (wcslen(FixedAction(all[k], kind, p != 0, f)) > 13) fit = false;
        }
      }
    }
  }
  for (int kind = 0; kind < kKinds; ++kind) {
    for (int side = 0; side < 2; ++side) {
      for (int b = 0; b < kButtons; ++b) {
        ButtonLabel(kind, side == 0, b, label, 24);
        if (wcslen(label) > 18) fit = false;
      }
      for (int f = 0; f < kFixed; ++f) {
        const wchar_t* w = FixedLabel(kind, side == 0, f);
        if (w != nullptr && wcslen(w) > 18) fit = false;
      }
    }
  }
  Check(fit, "each job has 13 letters at most and each label 18", fit, 1);

  // THE DRAWING.
  namespace art = controller_art;
  Check(art::kWidth == debugwin_math::kWellWidth &&
            art::kBandHeight == debugwin_math::kRowPitch,
        "a band of the drawing is a row of the list of the window",
        art::kWidth, debugwin_math::kWellWidth);
  int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
  art::WordsColumn(true, &x0, &x1);
  art::WordsColumn(false, &y0, &y1);
  const bool place = art::ArtLeft(true) > x1 && art::ArtRight(true) <
                         art::ArtLeft(false) &&
                     art::ArtRight(false) < y0 && x0 >= 0 &&
                     y1 <= art::kWidth && x1 - x0 >= 160 && y1 - y0 >= 160;
  Check(place, "the two controllers are in the middle, and each column of "
        "words is at its outer side, 160 px or more", x1 - x0, 160);
  float px = 0, py = 0, ux = 0, uy = 0;
  art::ToPixel(true, 30.0f, 100.0f, &px, &py);
  art::ToUnits(true, px, py, &ux, &uy);
  Near("a point of the left controller goes to a pixel and back", ux, 30.0,
       1e-3);
  art::ToPixel(false, 150.0f, 262.0f, &px, &py);
  Check(px <= art::ArtRight(false) + 1 && py <= art::kFootTop,
        "the frame of a controller ends above the words under it", py,
        art::kFootTop);

  // Each kind: its shapes are in its frame, each button is on the body, and
  // no two buttons touch.
  bool inside = true, on_body = true, apart = true;
  for (int kind = 0; kind < kKinds; ++kind) {
    const art::Art a = art::ArtOf(kind);
    for (int n = 0; n < a.bodies + art::kControls; ++n) {
      const bool body = n < a.bodies;
      const int c = n - a.bodies;
      if (!body && !a.has[c]) continue;
      float b[4];
      art::Bounds(body ? a.body[n] : a.control[c], b);
      if (b[0] < 0 || b[1] < 0 || b[2] > art::kArtWidth ||
          b[3] > art::kArtHeight) {
        inside = false;
      }
    }
    for (int c = 0; c < art::kControls; ++c) {
      if (!a.has[c] || c == art::kTrigger || c == art::kGrip) continue;
      float d = 1e9f;
      for (int n = 0; n < a.bodies; ++n) {
        const float e = art::Distance(a.body[n], a.control[c].x0,
                                      a.control[c].y0);
        if (e < d) d = e;
      }
      if (d > -a.control[c].r) on_body = false;
      for (int o = c + 1; o < art::kControls; ++o) {
        if (!a.has[o] || o == art::kTrigger || o == art::kGrip ||
            o == art::kStickClick || c == art::kStickClick) {
          continue;
        }
        if (a.control[c].kind != art::kCircle ||
            a.control[o].kind != art::kCircle) {
          continue;
        }
        const float dx = a.control[c].x0 - a.control[o].x0;
        const float dy = a.control[c].y0 - a.control[o].y0;
        if (sqrtf(dx * dx + dy * dy) <
            a.control[c].r + a.control[o].r + 4.0f) {
          apart = false;
        }
      }
    }
    // The drawing shows each button of the kind.
    for (int side = 0; side < 2; ++side) {
      for (int b = 0; b < kButtons; ++b) {
        if (Has(kind, side == 0, b) && !a.has[b]) on_body = false;
      }
    }
  }
  Check(inside, "the shapes of each kind are in the frame", inside, 1);
  Check(on_body, "each button of each kind is drawn, on the body", on_body,
        1);
  Check(apart, "no two round controls touch", apart, 1);

  // The distances of the shapes.
  const art::Shape ring = {art::kRing, 0, 0, 1, 0, 10};
  const art::Shape box = {art::kBox, 0, 0, 10, 5, 2};
  Near("a point on the middle of a ring band is 1 unit in it",
       art::Distance(ring, 10.0f, 0.0f), -1.0, 1e-4);
  Near("a point 3 units right of a box is 3 units out",
       art::Distance(box, 13.0f, 0.0f), 3.0, 1e-4);

  // The lines of words: in order, kLinePitch apart, between the top and
  // the bottom.
  const float want[7] = {30, 64, 64, 70, 104, 112, 270};
  float lines[7];
  art::PlaceLines(want, 7, 40.0f, 268.0f, lines);
  bool ordered = true;
  for (int i = 0; i < 7; ++i) {
    if (lines[i] < 40.0f - 1e-3f || lines[i] > 268.0f + 1e-3f) {
      ordered = false;
    }
    if (i > 0 && lines[i] - lines[i - 1] < art::kLinePitch - 1e-3f) {
      ordered = false;
    }
  }
  Check(ordered, "seven lines of words keep apart in the box of the art",
        ordered, 1);
}

}  // namespace button_map_checks
