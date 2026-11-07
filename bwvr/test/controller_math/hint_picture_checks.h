// THE PICTURES OF THE BUTTONS ON THE HINT CARDS (docs/controller-hints.md,
// "The pictures of the buttons"): the control of each button word, the
// window of each control, and the picture. main.cpp includes this file after
// Check and Near, just before main.
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/mod/button_map_math.h"
#include "../../src/mod/controller_art_math.h"
#include "../../src/mod/hint_picture_math.h"

namespace hint_picture_checks {

inline void HintPictureChecks() {
  namespace hp = hint_picture;
  namespace art = controller_art;
  namespace map = button_map;
  printf("\nThe pictures of the buttons on the hint cards\n");

  // THE WORDS of controller_hints.cpp (BuildWords) and the controls.
  const bool touch =
      hp::ControlOfWord(map::kTouch, false, L"A") == art::kFirst &&
      hp::ControlOfWord(map::kTouch, false, L"B") == art::kSecond &&
      hp::ControlOfWord(map::kTouch, true, L"X") == art::kFirst &&
      hp::ControlOfWord(map::kTouch, true, L"Y") == art::kSecond &&
      hp::ControlOfWord(map::kTouch, false, L"X") == -1 &&
      hp::ControlOfWord(map::kTouch, false, L"Trigger") == art::kTrigger &&
      hp::ControlOfWord(map::kTouch, true, L"Grip") == art::kGrip &&
      hp::ControlOfWord(map::kTouch, false, L"Stick") == art::kStick &&
      hp::ControlOfWord(map::kTouch, false, L"Stick click") ==
          art::kStickClick &&
      hp::ControlOfWord(map::kTouch, true, L"Menu") == art::kMenu &&
      hp::ControlOfWord(map::kTouch, false, L"Menu") == -1;
  Check(touch, "each word of a Touch controller names its control, and a "
        "button of the other side or the right menu button has no picture",
        touch, 1);
  const bool marks =
      hp::ControlOfWord(map::kTouch, false, L"A x2") == art::kFirst &&
      hp::ControlOfWord(map::kTouch, false, L"Grip hold") == art::kGrip &&
      hp::ControlOfWord(map::kTouch, false, L"Trigger x3") == -1 &&
      hp::ControlOfWord(map::kTouch, false, L"Gripper") == -1 &&
      hp::ControlOfWord(map::kTouch, false, L"") == -1 &&
      hp::ControlOfWord(map::kTouch, false, nullptr) == -1;
  Check(marks, "a word with \" x2\" or \" hold\" names the same control, and "
        "another word names none", marks, 1);
  const bool others =
      hp::ControlOfWord(map::kIndex, true, L"A") == art::kFirst &&
      hp::ControlOfWord(map::kIndex, false, L"B") == art::kSecond &&
      hp::ControlOfWord(map::kIndex, false, L"Menu") == -1 &&
      hp::ControlOfWord(map::kWand, false, L"Pad click") ==
          art::kStickClick &&
      hp::ControlOfWord(map::kWand, true, L"Menu") == art::kMenu &&
      hp::ControlOfWord(map::kWand, false, L"A") == -1 &&
      hp::ControlOfWord(map::kMixed, false, L"Stick") == art::kStick &&
      hp::ControlOfWord(map::kSimple, false, L"Select") == art::kTrigger &&
      hp::ControlOfWord(map::kSimple, false, L"Grip") == -1 &&
      hp::ControlOfWord(map::kSimple, false, L"Menu") == art::kMenu;
  Check(others, "the words of Index, the wand, Mixed Reality and the simple "
        "controller name their controls", others, 1);

  // THE WINDOW: each control that the drawing shows is inside its picture,
  // clear of the fade at the edge, on both sides.
  bool inside = true;
  float worst = 1e9f;
  for (int kind = 0; kind < map::kKinds; ++kind) {
    for (int side = 0; side < 2; ++side) {
      const bool left = side == 0;
      const art::Art a = art::ArtOf(kind);
      for (int c = 0; c < art::kControls; ++c) {
        if (c == art::kStickClick || !art::Shows(kind, left, c)) continue;
        float cx = 0.0f, cy = 0.0f;
        hp::Window(kind, c, &cx, &cy);
        float b[4];
        art::Bounds(a.control[c], b);
        const float s = hp::Scale();
        // The box in px of the picture. The mirror keeps its width.
        const float bx0 = (b[0] - cx) * s + hp::kSize * 0.5f;
        const float bx1 = (b[2] - cx) * s + hp::kSize * 0.5f;
        const float by0 = (b[1] - cy) * s + hp::kSize * 0.5f;
        const float by1 = (b[3] - cy) * s + hp::kSize * 0.5f;
        const float x_margin = bx0 < hp::kSize - bx1 ? bx0 : hp::kSize - bx1;
        const float y_margin = by0 < hp::kSize - by1 ? by0 : hp::kSize - by1;
        const float margin = x_margin < y_margin ? x_margin : y_margin;
        if (margin < worst) worst = margin;
        if (margin < 3.0f) inside = false;
      }
    }
  }
  Check(inside, "each control is 3 px or more inside its picture (the worst "
        "margin in px)", worst, 3.0);
  float tx = 0.0f, ty = 0.0f;
  hp::Window(map::kTouch, art::kTrigger, &tx, &ty);
  Check(ty - hp::kWindow * 0.5f >= -hp::kPast - 1e-3f &&
            ty - hp::kWindow * 0.5f < 0.0f,
        "the window of the trigger at the top goes past the drawing by at "
        "most kPast", ty - hp::kWindow * 0.5f, -hp::kPast);
  Near("the middle of a picture has no fade", hp::EdgeFade(30.0f, 30.0f), 1.0,
       1e-6);
  Near("a corner of a picture fades out", hp::EdgeFade(0.5f, 0.5f), 0.0,
       1e-6);

  // THE PICTURE.
  static uint8_t right[hp::kSize * hp::kSize * 4];
  static uint8_t left[hp::kSize * hp::kSize * 4];
  hp::Look look = {{52, 30, 14}, {36, 124, 60}, {52, 30, 14},
                   {120, 80, 30}, 0.07f, 0.6f};
  const bool made = hp::Bake(map::kIndex, false, art::kTrigger, look, right) &&
                    hp::Bake(map::kIndex, true, art::kTrigger, look, left);
  Check(made, "the trigger of an Index controller has a picture", made, 1);
  bool mirror = true;
  for (int y = 0; y < hp::kSize; ++y) {
    for (int x = 0; x < hp::kSize; ++x) {
      const uint8_t* r = right + (y * hp::kSize + x) * 4;
      const uint8_t* l = left + (y * hp::kSize + (hp::kSize - 1 - x)) * 4;
      for (int c = 0; c < 4; ++c) {
        if (abs(r[c] - l[c]) > 1) mirror = false;
      }
    }
  }
  Check(mirror, "the picture of the left controller is the mirror of the "
        "right one", mirror, 1);
  // The middle of the lit trigger has the color of the tone.
  const art::Art index = art::ArtOf(map::kIndex);
  float cx = 0.0f, cy = 0.0f;
  hp::Window(map::kIndex, art::kTrigger, &cx, &cy);
  const art::Shape& trigger = index.control[art::kTrigger];
  const float mx = (trigger.x0 + trigger.x1) * 0.5f;
  const float my = (trigger.y0 + trigger.y1) * 0.5f;
  const int px = static_cast<int>((mx - cx) * hp::Scale() + hp::kSize * 0.5f);
  const int py = static_cast<int>((my - cy) * hp::Scale() + hp::kSize * 0.5f);
  const uint8_t* lit = right + (py * hp::kSize + px) * 4;
  Check(lit[3] >= 240 && lit[1] > lit[2] && lit[1] > lit[0],
        "the middle of the lit control is opaque and green",
        lit[1], 124);
  const uint8_t* corner = right;
  Check(corner[3] == 0, "a corner of the picture is clear", corner[3], 0);
  static uint8_t none[hp::kSize * hp::kSize * 4];
  const bool no_menu = !hp::Bake(map::kIndex, false, art::kMenu, look, none) &&
                       none[(30 * hp::kSize + 30) * 4 + 3] == 0;
  Check(no_menu, "a control that the drawing does not show has no picture",
        no_menu, 1);

  // THE PUT over a picture of a card.
  static uint8_t card[80 * 70 * 4];
  for (int i = 0; i < 80 * 70 * 4; ++i) card[i] = 200;
  hp::Put(right, card, 80, 80, 70, 10, 5);
  const uint8_t* kept = card + (5 * 80 + 10) * 4;  // a clear corner
  const uint8_t* over = card + ((5 + py) * 80 + 10 + px) * 4;
  Check(kept[0] == 200 && kept[3] == 200 && over[1] == lit[1] &&
            over[3] == 255,
        "a clear pixel keeps the card, and an opaque one takes the picture",
        over[1], lit[1]);
  // A card of 30 by 20 px in a buffer of 80 by 70: the put writes no pixel
  // out of the card.
  for (int i = 0; i < 80 * 70 * 4; ++i) card[i] = 200;
  hp::Put(right, card, 80, 30, 20, 10, 5);
  int outside = 0, written = 0;
  for (int y = 0; y < 70; ++y) {
    for (int x = 0; x < 80; ++x) {
      const bool changed = card[(y * 80 + x) * 4 + 3] != 200;
      if (changed && (x >= 30 || y >= 20)) ++outside;
      if (changed && x < 30 && y < 20) ++written;
    }
  }
  Check(outside == 0 && written > 0,
        "a put that the card clips writes in the card only (pixels out of "
        "it)", outside, 0);
}

}  // namespace hint_picture_checks
