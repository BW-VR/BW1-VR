// THE PALM THAT TURNS UP, the book on the palm and the row of the game's
// gesture pictures (docs/miracles.md, "The gesture row over the palm" and
// "The book on the palm").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/gesture_panel_math.h"

namespace gesture_panel_checks {

inline void GesturePanelChecks() {
  using namespace gesture_panel_math;
  printf("\nThe palm that turns up and the gesture row\n");

  // THE PALM. The fake controller gives the left hand yaw, pitch and roll.
  // A roll of -90 degrees turns +x of the left grip, the normal of its palm,
  // to the up of the room. The word "lrot" uses these turns.
  {
    float q[4];
    controller_math::FakeOrientation(0.0f, 0.0f, -90.0f, q);
    Near("a left hand with a roll of -90 degrees has its palm straight up",
         PalmUp(q, true), 1.0, 1e-4);
    controller_math::FakeOrientation(0.0f, -30.0f, -90.0f, q);
    Near("the same hand with a pitch of -30 degrees is 30 degrees from up",
         PalmUp(q, true), cosf(30.0f / 57.29578f), 1e-4);
    controller_math::FakeOrientation(0.0f, -30.0f, 90.0f, q);
    Check(PalmUp(q, true) < -0.8f,
          "a roll of +90 degrees turns the left palm down", PalmUp(q, true),
          -0.866);
    controller_math::FakeOrientation(0.0f, -30.0f, 0.0f, q);
    Near("the base pose of the fake left hand has its palm to the side",
         PalmUp(q, true), 0.0, 1e-4);
    // The right hand: -x is its palm, so the mirror roll turns it up.
    controller_math::FakeOrientation(0.0f, 0.0f, 90.0f, q);
    Near("a right hand with a roll of +90 degrees has its palm up",
         PalmUp(q, false), 1.0, 1e-4);
  }

  // THE BAND. Up at 40 degrees, down only past 60 degrees.
  {
    const float at45 = CosOf(45.0f), at35 = CosOf(35.0f), at55 = CosOf(55.0f),
                at65 = CosOf(65.0f);
    bool up = false;
    up = PalmUpStep(up, at45, kUpOnDeg, kUpOffDeg);
    Check(!up, "a palm 45 degrees from up is not up yet", up, 0);
    up = PalmUpStep(up, at35, kUpOnDeg, kUpOffDeg);
    Check(up, "a palm 35 degrees from up is up", up, 1);
    up = PalmUpStep(up, at55, kUpOnDeg, kUpOffDeg);
    Check(up, "the palm stays up at 55 degrees", up, 1);
    up = PalmUpStep(up, at65, kUpOnDeg, kUpOffDeg);
    Check(!up, "the palm is not up at 65 degrees", up, 0);
  }

  // THE BOOK ON THE PALM.
  {
    const float side = 0.0f, high = CosOf(20.0f), tilted = CosOf(65.0f),
                down = CosOf(150.0f);
    PalmBook p = {false, false, 0.0};
    double t = 0.0;
    int e = PalmBookStep(&p, side, true, false, false, false, t);
    Check(e == kPalmNothing && p.armed, "a palm to the side arms the open", e,
          kPalmNothing);
    e = PalmBookStep(&p, high, true, false, false, false, t += 0.011);
    Check(e == kPalmNothing, "the palm that turns up does not open at once",
          e, kPalmNothing);
    e = PalmBookStep(&p, high, true, false, false, false, t += 0.2);
    Check(e == kPalmNothing, "0.2 s up does not open the book", e,
          kPalmNothing);
    e = PalmBookStep(&p, high, true, false, false, false, t += 0.21);
    Check(e == kPalmOpen, "0.41 s up opens the book", e, kPalmOpen);
    // The book is open, by the palm.
    e = PalmBookStep(&p, tilted, true, true, true, false, t += 0.1);
    Check(e == kPalmNothing, "a palm 65 degrees from up keeps the book", e,
          kPalmNothing);
    e = PalmBookStep(&p, down, true, true, true, true, t += 0.1);
    Check(e == kPalmNothing, "a pull holds the book, also with the palm down",
          e, kPalmNothing);
    e = PalmBookStep(&p, down, true, true, true, false, t += 0.1);
    Check(e == kPalmClose, "the palm down closes the book that it opened", e,
          kPalmClose);
    // A grip on the land: the time starts again when the grip ends.
    PalmBook g = {false, true, 0.0};
    t = 0.0;
    for (int i = 0; i < 60; ++i) {
      e = PalmBookStep(&g, high, false, false, false, false, t += 0.011);
      if (e != kPalmNothing) break;
    }
    Check(e == kPalmNothing, "a palm up that holds the land opens no book", e,
          kPalmNothing);
    e = PalmBookStep(&g, high, true, false, false, false, t += 0.011);
    Check(e == kPalmNothing, "the end of the grip does not open it at once",
          e, kPalmNothing);
    e = PalmBookStep(&g, high, true, false, false, false, t += 0.41);
    Check(e == kPalmOpen, "0.4 s more up and free opens it", e, kPalmOpen);
    // A book that the button opened: the palm does not close it, and after
    // the button closed it the palm must go down first.
    PalmBook b = {true, true, 0.0};
    t = 0.0;
    e = PalmBookStep(&b, down, true, true, false, false, t += 0.1);
    Check(e == kPalmNothing, "the palm does not close a book of the button",
          e, kPalmNothing);
    e = PalmBookStep(&b, high, true, true, false, false, t += 0.1);
    e = PalmBookStep(&b, high, true, false, false, false, t += 0.1);
    e = PalmBookStep(&b, high, true, false, false, false, t += 1.0);
    Check(e == kPalmNothing,
          "after the button closed the book, a palm that stays up does not "
          "open it again", e, kPalmNothing);
    e = PalmBookStep(&b, side, true, false, false, false, t += 0.1);
    e = PalmBookStep(&b, high, true, false, false, false, t += 0.1);
    e = PalmBookStep(&b, high, true, false, false, false, t += 0.41);
    Check(e == kPalmOpen, "the palm down and up again opens it", e,
          kPalmOpen);
  }

  // THE ROW. The squares of the game at a screen of 2760 pixels: 96.6
  // pixels, 13.8 pixels apart, from the right edge.
  {
    const float width = 2760.0f, side = 0.035f * width, gap = 0.005f * width;
    const float pitch = side + gap;
    RowMap m;
    // Six pictures: the row is 30 cm.
    const float x1 = width * (1.0f - 0.015f), x0 = x1 - 6 * side - 5 * gap;
    const bool ok = MapRow(x0, x1, 2208.0f - 2 * side, 2208.0f, 6, pitch,
                           0.30f, &m);
    Check(ok, "a row of six maps", ok, 1);
    Near("six pictures are 30 cm from the start of one to the next past the "
         "last", 6 * pitch * m.meters_per_pixel, 0.30, 1e-5);
    float r = 0.0f, u = 0.0f;
    PixelOnPlane(m, 0.5f * (x0 + x1), 2208.0f - side, &r, &u);
    Near("the middle of the row is the middle of the plane", r, 0.0, 1e-5);
    Near("the middle of the row in its height is the middle", u, 0.0, 1e-5);
    PixelOnPlane(m, x1, 2208.0f, &r, &u);
    Check(r > 0.0f && u < 0.0f,
          "the lower right of the row is right and down on the plane", r, 0.1);
    // One picture keeps the size of a row of three.
    RowMap one;
    MapRow(x1 - side, x1, 2208.0f - 2 * side, 2208.0f, 1, pitch, 0.30f, &one);
    Near("one picture is 10 cm from its start to the next", pitch *
         one.meters_per_pixel, 0.10, 1e-5);
    Check(!MapRow(x0, x1, 0.0f, 1.0f, 0, pitch, 0.30f, &m),
          "no picture gives no row", 0, 0);
  }
}

}  // namespace gesture_panel_checks
