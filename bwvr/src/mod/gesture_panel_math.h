// The palm that turns up, and the row of the game's gesture pictures over it:
// the pure math. See docs/miracles.md, "The gesture row over the palm" and
// "The book on the palm". gesture_panel.cpp and spell_book.cpp use it, and
// test/controller_math checks it (gesture_panel_checks.h). Nothing here
// reads a global or touches the game.
//
// THE PALM. The normal of the open palm is +x of the grip pose of the left
// hand and -x of the right hand, as OpenXR gives the grip pose (BookCenter in
// spell_book_math.h). The palm is up when that normal is near the up of the
// room. Two limits make a band, so a palm at the limit does not flicker.
//
// THE ROW. The game puts its gesture pictures in a row at the lower right of
// its screen, in pixels (DisplayGesture, PSysGesture.cpp). The mod keeps the
// layout of the game and puts that row on a plane over the palm: a pixel of
// the game screen becomes a point of the plane.

#pragma once

#include <math.h>

#include "controller_math.h"

namespace gesture_panel_math {

// The palm is up when its normal is at most kUpOnDeg from the up of the room.
// It stays up until the normal is more than kUpOffDeg from up.
const float kUpOnDeg = 40.0f;
const float kUpOffDeg = 60.0f;
// The book on the palm (spell_book_palm=1): the palm stays up this long
// before the book opens. The book that the palm opened closes when the palm
// is more than kBookOffDeg from up.
const float kBookHoldSeconds = 0.4f;
const float kBookOffDeg = 70.0f;

// The cosine of the angle from the up of the room to the normal of the palm.
// 1 is a palm straight up, 0 a palm to the side, -1 a palm down. left: the
// hand is the left hand.
inline float PalmUp(const float orientation[4], bool left) {
  const float local[3] = {left ? 1.0f : -1.0f, 0.0f, 0.0f};
  float normal[3];
  controller_math::QuatRotate(orientation, local, normal);
  return normal[1];
}

inline float CosOf(float degrees) { return cosf(degrees / 57.29578f); }

// One step of the band. up is the state of the last frame. Returns the state
// of this frame.
inline bool PalmUpStep(bool up, float cos_up, float on_deg, float off_deg) {
  if (up) return cos_up >= CosOf(off_deg);
  return cos_up >= CosOf(on_deg);
}

// THE BOOK ON THE PALM. One step a frame. The palm must be up and free for
// kBookHoldSeconds before the book opens. Free: the hand holds nothing (no
// grip on the land, no ring), and the row of the gestures does not want the
// palm. A palm that is not free starts the time again, so a grip that ends
// with the palm up does not open the book at once.
struct PalmBook {
  bool up;          // the palm is up, with the band of the open
  bool armed;       // the palm was down or to the side since the last open
  double up_since;  // the time at which the palm became up and free
};

enum PalmEvent { kPalmNothing, kPalmOpen, kPalmClose };

// now: seconds. free: the palm can open the book now. open_by_palm: the book
// is open, and the palm opened it. hold: the book must stay open now (a pull
// of a bubble out of the book).
inline int PalmBookStep(PalmBook* p, float cos_up, bool free, bool open,
                        bool open_by_palm, bool hold, double now) {
  const bool was_up = p->up;
  p->up = PalmUpStep(p->up, cos_up, kUpOnDeg, kUpOffDeg);
  if (open) {
    p->up_since = now;
    if (open_by_palm && !hold && cos_up < CosOf(kBookOffDeg)) {
      p->armed = true;
      return kPalmClose;
    }
    // A book that X opened: the palm must go down one time before it can
    // open the book again.
    if (!open_by_palm) p->armed = false;
    return kPalmNothing;
  }
  if (!p->up) {
    p->armed = true;
    p->up_since = now;
    return kPalmNothing;
  }
  if (!was_up || !free) {
    p->up_since = now;
    return kPalmNothing;
  }
  if (p->armed && now - p->up_since >= kBookHoldSeconds) {
    p->armed = false;
    return kPalmOpen;
  }
  return kPalmNothing;
}

// THE ROW. The game gives each picture a square of side kSideShare times the
// width of its screen, kGapShare apart, kEdgeShare from the right edge, at the
// bottom (fn_0068AED0). The bubble of the miracle is over its square, its
// middle half a side over the top of the square (fn_0068B7E0).
struct RowMap {
  float middle_x, middle_y;  // the pixel at the middle of the plane
  float meters_per_pixel;
};

// The row is width_m wide for 3 pictures or more. Fewer pictures keep the
// size of a row of 3, so one picture is not 30 cm. x0 and x1: the left edge of
// the leftmost square and the right edge of the rightmost square, in pixels.
// top and bottom: the top of the squares less one side (the bubbles), and
// the bottom of the squares. pitch: the pixels from one square to the next.
inline bool MapRow(float x0, float x1, float top, float bottom, int count,
                   float pitch, float width_m, RowMap* out) {
  if (count <= 0 || !(pitch > 0.0f) || !(width_m > 0.0f) || !(x1 > x0)) {
    return false;
  }
  const int slots = count < 3 ? 3 : count;
  out->meters_per_pixel = width_m / (slots * pitch);
  out->middle_x = 0.5f * (x0 + x1);
  out->middle_y = 0.5f * (top + bottom);
  return true;
}

// A pixel of the game screen on the plane: meters to the right and up from
// the middle of the plane. The pixels of the screen go down.
inline void PixelOnPlane(const RowMap& m, float px, float py, float* right,
                         float* up) {
  *right = (px - m.middle_x) * m.meters_per_pixel;
  *up = (m.middle_y - py) * m.meters_per_pixel;
}

}  // namespace gesture_panel_math
