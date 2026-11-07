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

  // THE VIEWS: each control that the drawing shows is inside its picture,
  // clear of the fade at the edge, on both sides. The trigger and the grip
  // have a side view, the select button of a simple controller has not.
  bool inside = true, sides = true;
  float worst = 1e9f;
  for (int kind = 0; kind < map::kKinds; ++kind) {
    for (int side = 0; side < 2; ++side) {
      const bool left = side == 0;
      for (int c = 0; c < art::kControls; ++c) {
        if (c == art::kStickClick || !art::Shows(kind, left, c)) continue;
        hp::View v;
        if (!hp::ViewOf(kind, left, c, &v)) {
          inside = false;
          continue;
        }
        const bool want = (c == art::kTrigger || c == art::kGrip) &&
                          kind != map::kSimple;
        if (hp::SideView(kind, c) != want) sides = false;
        float b[4];
        art::Bounds(v.lit, b);
        // The box in px of the picture. The mirror keeps its width.
        float x0, y0, x1, y1;
        hp::ToPixel(v, false, b[0], b[1], &x0, &y0);
        hp::ToPixel(v, false, b[2], b[3], &x1, &y1);
        const float x_margin = x0 < hp::kSize - x1 ? x0 : hp::kSize - x1;
        const float y_margin = y0 < hp::kSize - y1 ? y0 : hp::kSize - y1;
        const float margin = x_margin < y_margin ? x_margin : y_margin;
        if (margin < worst) worst = margin;
        if (margin < 3.0f) inside = false;
      }
    }
  }
  Check(inside, "each control is 3 px or more inside its picture (the worst "
        "margin in px)", worst, 3.0);
  Check(sides, "the trigger and the grip have a side view, and the select "
        "button of a simple controller a top view", sides, 1);
  // The window of a face button of a Touch controller holds its whole ring.
  bool ring = true;
  {
    const art::Art touch = art::ArtOf(map::kTouch);
    float r[4];
    art::Bounds(touch.body[0], r);
    const int buttons[3] = {art::kFirst, art::kSecond, art::kStick};
    for (int i = 0; i < 3; ++i) {
      hp::View v;
      hp::ViewOf(map::kTouch, false, buttons[i], &v);
      float x0, y0, x1, y1;
      hp::ToPixel(v, false, r[0], r[1], &x0, &y0);
      hp::ToPixel(v, false, r[2], r[3], &x1, &y1);
      if (x0 < 0.0f || y0 < 0.0f || x1 > hp::kSize || y1 > hp::kSize) {
        ring = false;
      }
    }
  }
  Check(ring, "the picture of a face button or the stick of a Touch "
        "controller holds the whole ring", ring, 1);
  // A side view holds the whole controller.
  bool whole = true;
  for (int kind = 0; kind < map::kSimple; ++kind) {
    hp::View v;
    hp::ViewOf(kind, false, art::kGrip, &v);
    for (int n = 0; n < v.bodies; ++n) {
      float b[4];
      art::Bounds(v.body[n], b);
      float x0, y0, x1, y1;
      hp::ToPixel(v, false, b[0], b[1], &x0, &y0);
      hp::ToPixel(v, false, b[2], b[3], &x1, &y1);
      if (x0 < 2.0f || y0 < 2.0f || x1 > hp::kSize - 2.0f ||
          y1 > hp::kSize - 2.0f) {
        whole = false;
      }
    }
  }
  Check(whole, "a side view holds the whole controller", whole, 1);
  Near("the middle of a picture has no fade", hp::EdgeFade(30.0f, 30.0f), 1.0,
       1e-6);
  Near("a corner of a picture fades out", hp::EdgeFade(0.5f, 0.5f), 0.0,
       1e-6);

  // THE PICTURE.
  static uint8_t right[hp::kSize * hp::kSize * 4];
  static uint8_t left[hp::kSize * hp::kSize * 4];
  const hp::Look look = hp::LookOf(true, 1);
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
  // The middle of the lit trigger has the color of the tone: the green wax.
  hp::View view;
  hp::ViewOf(map::kIndex, false, art::kTrigger, &view);
  float fx = 0.0f, fy = 0.0f;
  hp::ToPixel(view, false, (view.lit.x0 + view.lit.x1) * 0.5f,
              (view.lit.y0 + view.lit.y1) * 0.5f, &fx, &fy);
  const int px = static_cast<int>(fx), py = static_cast<int>(fy);
  const uint8_t* lit = right + (py * hp::kSize + px) * 4;
  Check(lit[3] >= 240 && lit[1] > lit[2] && lit[1] > lit[0],
        "the middle of the lit control is opaque and green", lit[1], 124);
  // The body has a pale fill on the scroll, so its shape reads on the paper.
  const art::Shape& bar = view.body[0];
  hp::ToPixel(view, false, bar.x0 + (bar.x1 - bar.x0) * 0.3f,
              bar.y0 + (bar.y1 - bar.y0) * 0.3f, &fx, &fy);
  const uint8_t* body =
      right + (static_cast<int>(fy) * hp::kSize + static_cast<int>(fx)) * 4;
  Check(body[3] >= 220 && body[2] >= 220, "the body on the scroll is pale "
        "and opaque (red)", body[2], 232);
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
  // Over: the picture, and the card times what the picture lets through.
  const int want = lit[1] + (200 * (255 - lit[3]) + 127) / 255;
  Check(kept[0] == 200 && kept[3] == 200 && over[1] == want,
        "a clear pixel keeps the card, and a pixel of the picture goes over "
        "it", over[1], want);
  // A card of 45 by 40 px in a buffer of 80 by 70: the put writes no pixel
  // out of the card.
  for (int i = 0; i < 80 * 70 * 4; ++i) card[i] = 200;
  hp::Put(right, card, 80, 45, 40, 10, 5);
  int outside = 0, written = 0;
  for (int y = 0; y < 70; ++y) {
    for (int x = 0; x < 80; ++x) {
      const bool changed = card[(y * 80 + x) * 4 + 3] != 200;
      if (changed && (x >= 45 || y >= 40)) ++outside;
      if (changed && x < 45 && y < 40) ++written;
    }
  }
  Check(outside == 0 && written > 0,
        "a put that the card clips writes in the card only (pixels out of "
        "it)", outside, 0);
}

}  // namespace hint_picture_checks
