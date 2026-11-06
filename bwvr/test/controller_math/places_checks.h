// THE PANEL OF PLACES (docs/ui-layer.md, "The panel of places").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <math.h>

#include "../../src/mod/places_panel_math.h"

namespace places_checks {

inline void PlacesChecks() {
  using namespace places_math;
  printf("\nThe panel of places\n");

  // THE BUTTONS. 4 places, 4 buttons for each of the 7 bookmarks, Back and
  // Close. Each one has a kind, and each kind its word.
  Check(kButtons == 34, "the panel has 34 buttons", kButtons, 34);
  {
    bool round = true;
    for (int b = 0; b < kButtons; ++b) {
      if (ButtonOf(KindOf(b), MarkOf(b)) != b) round = false;
    }
    Check(round, "each button comes back from its kind and its bookmark",
          round, 1);
    Check(KindOf(kFirstMark) == kGo && MarkOf(kFirstMark) == 0 &&
              KindOf(kBackButton - 1) == kRemove &&
              MarkOf(kBackButton - 1) == 6 && KindOf(kBackButton) == kBack &&
              KindOf(kCloseButton) == kClose && MarkOf(kCloseButton) == -1,
          "the bookmark rows go from Go 1 to Remove 7, then Back and Close",
          MarkOf(kBackButton - 1), 6);
  }

  // THE LAYOUT. Each button is on the paper, inside the torn edge, and no
  // two boxes with the rim overlap. So a copy of one box does not change a
  // different button. No seal and no note overlaps a button.
  {
    bool fits = true, apart = true;
    for (int i = 0; i < kButtons; ++i) {
      int r[4], a[4];
      ButtonRect(i, r);
      ButtonBox(i, a);
      if (r[0] < 20 || r[1] < 20 || r[0] + r[2] > kWidth - 20 ||
          r[1] + r[3] > kHeight - 20 || r[2] < 100 || r[3] < 60) {
        fits = false;
      }
      for (int j = i + 1; j < kButtons; ++j) {
        int b[4];
        ButtonBox(j, b);
        if (a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3]) {
          apart = false;
        }
      }
      for (int m = 0; m < kMarks; ++m) {
        int s[4];
        SealBox(m, s);
        if (a[0] < s[2] && s[0] < a[2] && a[1] < s[3] && s[1] < a[3]) {
          apart = false;
        }
      }
      int n[4];
      NoteBox(n);
      if (a[0] < n[2] && n[0] < a[2] && a[1] < n[3] && n[1] < a[3]) {
        apart = false;
      }
    }
    Check(fits, "each button is on the paper, at least 100 by 60 px", fits,
          1);
    Check(apart, "no two parts of the panel overlap", apart, 1);
  }

  // THE RAY ON A BUTTON. The middle of each button finds it. The paper
  // between two buttons and a point off the panel find none.
  {
    bool found = true;
    for (int b = 0; b < kButtons; ++b) {
      float u = 0.0f, v = 0.0f;
      ButtonMiddle(b, &u, &v);
      if (ButtonAt(u, v) != b) found = false;
    }
    Check(found, "the middle of each button finds that button", found, 1);
    int r[4];
    ButtonRect(ButtonOf(kGo, 0), r);
    const float gap_u = (r[0] + r[2] + kMarkGap * 0.5f) / kWidth;
    const float row_v = (r[1] + r[3] * 0.5f) / kHeight;
    Check(ButtonAt(gap_u, row_v) == -1,
          "the paper between Go and Set here finds no button",
          ButtonAt(gap_u, row_v), -1);
    Check(ButtonAt(0.5f, 0.05f) == -1 && ButtonAt(-0.1f, 0.5f) == -1 &&
              ButtonAt(0.5f, 1.2f) == -1,
          "the title and a point off the panel find no button", 1, 1);
  }

  // THE KEYS OF A BOOKMARK (GGame::ProcessKey 0x0063EF20): the key of the
  // number, with no modifier, Ctrl, Shift, or Ctrl and Shift.
  {
    int key = 0, mods = 0;
    bool ok = MarkKey(kGo, 0, &key, &mods);
    Check(ok && key == 0x02 && mods == 0, "Go 1 is the key 1 (scan code 2)",
          key, 0x02);
    ok = MarkKey(kSet, 2, &key, &mods);
    Check(ok && key == 0x04 && mods == radial_math::kModCtrl,
          "Set here 3 is Ctrl and the key 3", mods, radial_math::kModCtrl);
    ok = MarkKey(kFollow, 6, &key, &mods);
    Check(ok && key == 0x08 && mods == radial_math::kModShift,
          "Follow 7 is Shift and the key 7", key, 0x08);
    ok = MarkKey(kRemove, 4, &key, &mods);
    Check(ok && key == 0x06 &&
              mods == (radial_math::kModCtrl | radial_math::kModShift),
          "Remove 5 is Ctrl, Shift and the key 5", mods, 0x30);
    Check(!MarkKey(kTemple, 0, &key, &mods) && !MarkKey(kGo, 7, &key, &mods),
          "a place and a bookmark past 7 have no bookmark key", 1, 1);
  }

  // THE PLACES go through the items of the ring.
  Check(RingItemOf(kTemple) == radial_math::kItemTemple &&
            RingItemOf(kCreature) == radial_math::kItemCreature &&
            RingItemOf(kTown) == radial_math::kItemTown &&
            RingItemOf(kLand) == radial_math::kItemLand &&
            RingItemOf(kBack) == radial_math::kItemMarkBack &&
            RingItemOf(kGo) == radial_math::kItemNone,
        "the places and Back are the items temple, creature, town, land and "
        "mark_back of the ring", 1, 1);

  // WHAT CLOSES THE PANEL, and what is dim.
  Check(Closes(kGo) && Closes(kFollow) && Closes(kTemple) && Closes(kBack) &&
            Closes(kClose) && !Closes(kSet) && !Closes(kRemove),
        "Go, Follow, the places, Back and Close close the panel, Set here "
        "and Remove keep it open", 1, 1);
  Check(MarkCan(kSet, true, false) && !MarkCan(kGo, true, false) &&
            !MarkCan(kFollow, true, false) && !MarkCan(kRemove, true, false) &&
            MarkCan(kGo, true, true) && !MarkCan(kSet, false, false) &&
            !MarkCan(kGo, false, true),
        "a bookmark that is not set has Set here only, and no button acts "
        "while the game takes no bookmark key", 1, 1);

  // THE TRIGGER: one press for each pull, between 0.6 and 0.4.
  {
    Press p;
    p.down = p.was = true;  // down when the panel opens
    int got = PressStep(&p, 0.9f, 5);
    Check(got == -1, "a trigger that is down at the open presses nothing",
          got, -1);
    PressStep(&p, 0.5f, 5);
    got = PressStep(&p, 0.7f, 5);
    Check(got == -1, "it must go below 0.4 first", got, -1);
    PressStep(&p, 0.3f, 5);
    got = PressStep(&p, 0.65f, 5);
    Check(got == 5, "a new pull presses the button under the ray", got, 5);
    got = PressStep(&p, 0.95f, 6);
    Check(got == -1, "a held pull presses one time", got, -1);
    PressStep(&p, 0.1f, -1);
    got = PressStep(&p, 0.8f, -1);
    Check(got == -1, "a pull off the buttons presses nothing", got, -1);
  }

  // THE PLACE. 1 m from the head, 12 degrees below eye level, and the panel
  // faces the head. A ray from the head to the middle of a button meets
  // that button. The bottom edge is above 30 degrees below eye level.
  {
    const float head[3] = {0.1f, 1.6f, -0.2f};
    const float yaw = 0.5f;
    menu_room_math::Panel panel;
    PlaceAt(head, yaw, &panel);
    const float d[3] = {panel.position[0] - head[0],
                        panel.position[1] - head[1],
                        panel.position[2] - head[2]};
    const float length = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    Near("the panel is 1 m from the head", length, 1.0, 1e-4);
    Near("its middle is 12 degrees below eye level",
         asinf(d[1] / length) * 57.29578f, -12.0, 1e-3);
    Near("it is 56 cm across", panel.width, 0.56, 1e-6);
    bool hits = true;
    for (int b = 0; b < kButtons; ++b) {
      float u = 0.0f, v = 0.0f, p[3];
      ButtonMiddle(b, &u, &v);
      menu_room_math::PointAt(panel, u, v, p);
      float dir[3] = {p[0] - head[0], p[1] - head[1], p[2] - head[2]};
      const float l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
      for (int k = 0; k < 3; ++k) dir[k] /= l;
      float hu = 0.0f, hv = 0.0f, t = 0.0f;
      if (!menu_room_math::Hit(head, dir, panel, &hu, &hv, &t) ||
          ButtonAt(hu, hv) != b) {
        hits = false;
      }
    }
    Check(hits, "a ray from the head to the middle of each button meets it",
          hits, 1);
    float bottom[3];
    menu_room_math::PointAt(panel, 0.5f, 1.0f, bottom);
    const float b[3] = {bottom[0] - head[0], bottom[1] - head[1],
                        bottom[2] - head[2]};
    const float bl = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
    const float down = asinf(b[1] / bl) * 57.29578f;
    Check(down > -30.0f && down < -20.0f,
          "the bottom edge is 20 to 30 degrees below eye level, above a hand "
          "that points", static_cast<int>(down), -27);
  }

  // THE KIT (panel_kit_math.h): the hit test of a list of rects, and the box
  // with a rim that stays on the picture.
  {
    const int rects[2][4] = {{10, 10, 100, 50}, {200, 10, 100, 50}};
    Check(panel_kit_math::HitRects(60.0f / 400, 30.0f / 100, 400, 100, rects,
                                   2) == 0 &&
              panel_kit_math::HitRects(250.0f / 400, 30.0f / 100, 400, 100,
                                       rects, 2) == 1 &&
              panel_kit_math::HitRects(150.0f / 400, 30.0f / 100, 400, 100,
                                       rects, 2) == -1 &&
              panel_kit_math::HitRects(1.5f, 0.3f, 400, 100, rects, 2) == -1,
          "the kit finds the rect at a point, and none between two rects or "
          "off the panel", 1, 1);
    int box[4];
    const int edge[4] = {0, 1, 400, 99};
    panel_kit_math::RectBox(edge, 3, 400, 100, box);
    Check(box[0] == 0 && box[1] == 0 && box[2] == 400 && box[3] == 100,
          "a box with its rim stays on the picture", box[3], 100);
  }

  // THE TEST KEY places_test_buttons.
  {
    int list[8];
    const int n = ParseButtons("set3 go3,remove3  temple nonsense close go9",
                               list, 8);
    Check(n == 5 && list[0] == ButtonOf(kSet, 2) && list[1] == ButtonOf(kGo, 2) &&
              list[2] == ButtonOf(kRemove, 2) && list[3] == ButtonOf(kTemple, -1) &&
              list[4] == kCloseButton,
          "the test key gives the buttons of its words, and skips a word "
          "that it does not know", n, 5);
  }
}

}  // namespace places_checks
