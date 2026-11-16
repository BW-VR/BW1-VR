// The drawing of the controllers of the player on the page Controls of the
// settings window: the shapes of each kind of controller, the place of each
// control, and the place of the words beside them. See controls_page.cpp and
// docs/ui-layer.md, "The page Controls". No Windows and no game here, so the
// math test compiles this file (button_map_checks.h).
//
// OUR OWN ART. Each kind is a few circles, capsules and rounded boxes in ink,
// drawn in code. The shapes are generic: no logo and no outline of a product
// of a maker. Each control is at about its place on the controller, seen from
// above with the front at the top, so the player finds it with the thumb.
//
// THE FRAME. A controller is drawn in a box of kArtWidth by kArtHeight units,
// as the right controller. The left controller is its mirror: x goes to
// kArtWidth - x. A unit is about one pixel of the picture.

#pragma once

#include <math.h>

#include "button_map_math.h"

namespace controller_art {

// --- the picture -----------------------------------------------------------------

// The drawing is 4 rows of the list of the window (debug_window_math.h,
// kWellWidth by kRowPitch each).
const int kWidth = 692;
const int kBands = 4;
const int kBandHeight = 76;
const int kHeight = kBands * kBandHeight;

// The line at the top: the kind of the controllers. Each controller has its
// side and its hand under it.
const int kHeadTop = 2;
const int kHeadBottom = 30;
const int kFootTop = 280;
const int kFootBottom = kHeight - 2;

// The box of a controller in the picture, and the scale from units to
// pixels. The left controller is at the left of the middle, the right one at
// the right, with its words at the outer side.
const float kArtWidth = 150.0f;
const float kArtHeight = 262.0f;
const int kArtTop = 34;
const int kArtPixels = 242;  // the height of the box in the picture
const int kArtGap = 14;      // between the two boxes
const int kWordsGap = 12;    // from a box to its words
inline float Scale() { return kArtPixels / kArtHeight; }
inline int ArtLeft(bool left) {
  const int w = static_cast<int>(kArtWidth * Scale() + 0.5f);
  return left ? kWidth / 2 - kArtGap / 2 - w : kWidth / 2 + kArtGap / 2;
}
inline int ArtRight(bool left) {
  return ArtLeft(left) + static_cast<int>(kArtWidth * Scale() + 0.5f);
}
// The column of the words: x0 to x1.
inline void WordsColumn(bool left, int* x0, int* x1) {
  if (left) {
    *x0 = 6;
    *x1 = ArtLeft(true) - kWordsGap;
  } else {
    *x0 = ArtRight(false) + kWordsGap;
    *x1 = kWidth - 6;
  }
}

// A point of the frame of a controller in pixels of the picture.
inline void ToPixel(bool left, float ux, float uy, float* x, float* y) {
  const float s = Scale();
  const float mx = left ? kArtWidth - ux : ux;
  *x = ArtLeft(left) + mx * s;
  *y = kArtTop + uy * s;
}
// A pixel of the picture in units of the frame of a controller.
inline void ToUnits(bool left, float x, float y, float* ux, float* uy) {
  const float s = Scale();
  const float mx = (x - ArtLeft(left)) / s;
  *ux = left ? kArtWidth - mx : mx;
  *uy = (y - kArtTop) / s;
}

// --- the shapes ------------------------------------------------------------------

enum ShapeKind { kCircle = 0, kCapsule = 1, kBox = 2, kRing = 3 };

// circle: the middle x0, y0 and radius r.
// capsule: the line from x0, y0 to x1, y1 and radius r.
// box: the middle x0, y0, the half sizes x1, y1, the round of the corners r.
// ring: the middle x0, y0, radius r to the middle of the band, half width x1.
struct Shape {
  int kind;
  float x0, y0, x1, y1, r;
};

// The signed distance from a point to a shape, in units: less than 0 in it.
inline float Distance(const Shape& s, float x, float y) {
  switch (s.kind) {
    case kCircle: {
      const float dx = x - s.x0, dy = y - s.y0;
      return sqrtf(dx * dx + dy * dy) - s.r;
    }
    case kCapsule: {
      const float px = x - s.x0, py = y - s.y0;
      const float bx = s.x1 - s.x0, by = s.y1 - s.y0;
      const float bb = bx * bx + by * by;
      float t = bb > 0.0f ? (px * bx + py * by) / bb : 0.0f;
      t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
      const float dx = px - bx * t, dy = py - by * t;
      return sqrtf(dx * dx + dy * dy) - s.r;
    }
    case kBox: {
      const float qx = fabsf(x - s.x0) - s.x1 + s.r;
      const float qy = fabsf(y - s.y0) - s.y1 + s.r;
      const float ox = qx > 0.0f ? qx : 0.0f, oy = qy > 0.0f ? qy : 0.0f;
      const float in = qx > qy ? qx : qy;
      return sqrtf(ox * ox + oy * oy) + (in < 0.0f ? in : 0.0f) - s.r;
    }
    default: {
      const float dx = x - s.x0, dy = y - s.y0;
      return fabsf(sqrtf(dx * dx + dy * dy) - s.r) - s.x1;
    }
  }
}

// The box of a shape in units: x0, y0, x1, y1.
inline void Bounds(const Shape& s, float out[4]) {
  switch (s.kind) {
    case kCapsule:
      out[0] = (s.x0 < s.x1 ? s.x0 : s.x1) - s.r;
      out[1] = (s.y0 < s.y1 ? s.y0 : s.y1) - s.r;
      out[2] = (s.x0 > s.x1 ? s.x0 : s.x1) + s.r;
      out[3] = (s.y0 > s.y1 ? s.y0 : s.y1) + s.r;
      return;
    case kBox:
      out[0] = s.x0 - s.x1;
      out[1] = s.y0 - s.y1;
      out[2] = s.x0 + s.x1;
      out[3] = s.y0 + s.y1;
      return;
    case kRing:
      out[0] = s.x0 - s.r - s.x1;
      out[1] = s.y0 - s.r - s.x1;
      out[2] = s.x0 + s.r + s.x1;
      out[3] = s.y0 + s.r + s.x1;
      return;
    default:
      out[0] = s.x0 - s.r;
      out[1] = s.y0 - s.r;
      out[2] = s.x0 + s.r;
      out[3] = s.y0 + s.r;
      return;
  }
}

// --- the controls ----------------------------------------------------------------

// The controls of the drawing. The first four are the buttons of
// button_map.h in their order. The stick click is a press of the stick, so
// it has the shape of the stick.
enum Control {
  kFirst = button_map::kFirst,
  kSecond = button_map::kSecond,
  kStickClick = button_map::kStickClick,
  kMenu = button_map::kMenu,
  kTrigger = 4,
  kGrip = 5,
  kStick = 6,
  kPad = 7,  // a pad that the mod does not bind: drawn, with no words
  kControls = 8
};

// The control of a fixed control of button_map.h.
inline int ControlOfFixed(int fixed) {
  return fixed == button_map::kTrigger ? kTrigger
         : fixed == button_map::kGrip  ? kGrip
                                       : kStick;
}

struct Art {
  int bodies;
  Shape body[3];  // their union is the outline of the controller
  Shape control[kControls];
  bool has[kControls];  // the kind has the control (for each side)
};

// The art of a kind, as the right controller.
inline Art ArtOf(int kind) {
  Art a = {};
  const auto put = [&](int c, int k, float x0, float y0, float x1, float y1,
                       float r) {
    a.control[c] = Shape{k, x0, y0, x1, y1, r};
    a.has[c] = true;
  };
  switch (kind) {
    case button_map::kIndex:
      a.bodies = 1;
      a.body[0] = Shape{kCapsule, 75, 42, 75, 224, 34};
      put(kStick, kCircle, 75, 60, 0, 0, 17);
      put(kFirst, kCircle, 58, 100, 0, 0, 10);
      put(kSecond, kCircle, 92, 100, 0, 0, 10);
      put(kPad, kBox, 75, 146, 20, 24, 10);
      put(kTrigger, kCapsule, 60, 9, 90, 9, 7);
      put(kGrip, kCapsule, 42, 176, 42, 236, 8);
      break;
    case button_map::kWand:
      a.bodies = 2;
      a.body[0] = Shape{kRing, 75, 58, 9, 0, 48};
      a.body[1] = Shape{kCapsule, 75, 112, 75, 230, 30};
      put(kMenu, kCircle, 75, 128, 0, 0, 7);
      put(kStick, kCircle, 75, 170, 0, 0, 24);
      put(kTrigger, kCapsule, 42, 132, 42, 152, 7);
      put(kGrip, kCapsule, 45, 206, 45, 230, 7);
      break;
    case button_map::kMixed:
      a.bodies = 2;
      a.body[0] = Shape{kRing, 75, 54, 8, 0, 46};
      a.body[1] = Shape{kCapsule, 75, 104, 75, 228, 32};
      put(kStick, kCircle, 62, 122, 0, 0, 13);
      put(kMenu, kCircle, 95, 118, 0, 0, 6);
      put(kPad, kCircle, 82, 162, 0, 0, 17);
      put(kTrigger, kCapsule, 42, 112, 44, 130, 7);
      put(kGrip, kCapsule, 44, 190, 44, 232, 8);
      break;
    case button_map::kSimple:
      a.bodies = 1;
      a.body[0] = Shape{kCapsule, 75, 40, 75, 228, 32};
      put(kTrigger, kCircle, 75, 78, 0, 0, 17);
      put(kMenu, kCircle, 75, 130, 0, 0, 8);
      break;
    default:  // Touch, and any other profile
      a.bodies = 2;
      a.body[0] = Shape{kCircle, 74, 80, 0, 0, 62};
      a.body[1] = Shape{kCapsule, 86, 124, 98, 232, 28};
      put(kStick, kCircle, 56, 66, 0, 0, 19);
      put(kFirst, kCircle, 94, 106, 0, 0, 12);
      put(kSecond, kCircle, 106, 70, 0, 0, 12);
      put(kMenu, kCircle, 56, 112, 0, 0, 8);
      put(kTrigger, kCapsule, 48, 14, 80, 12, 8);
      put(kGrip, kCapsule, 62, 150, 68, 224, 9);
      break;
  }
  if (a.has[kStick]) {
    a.control[kStickClick] = a.control[kStick];
    a.has[kStickClick] = true;
  }
  return a;
}

// True when the controller of a side has the control: the drawing shows it.
inline bool Shows(int kind, bool left, int control) {
  if (control < button_map::kButtons) {
    return button_map::Has(kind, left, control);
  }
  if (control == kTrigger) return true;
  if (control == kGrip || control == kStick) return kind != button_map::kSimple;
  return ArtOf(kind).has[control];
}

// The point of a control that its line of words goes to, in units: the edge
// of the shape on the side of the words, so the line does not cross it. The
// words are at the outer side of each controller, and the left controller is
// the mirror of the frame, so that side is the high x of the frame for both.
inline void Anchor(const Shape& s, float* x, float* y) {
  float b[4];
  Bounds(s, b);
  *y = (b[1] + b[3]) * 0.5f;
  *x = b[2];
}

// --- the words -------------------------------------------------------------------

// THE LINES OF WORDS of a controller go down its column. Each line wants the
// height of its control, and the lines keep kLinePitch apart, in the box of
// the art. The answer is the y of the middle of each line, in the order of
// `want` (which goes down).
const int kLinePitch = 30;
inline void PlaceLines(const float* want, int count, float top, float bottom,
                       float* out) {
  for (int i = 0; i < count; ++i) {
    float y = want[i];
    if (y < top) y = top;
    if (i > 0 && y < out[i - 1] + kLinePitch) y = out[i - 1] + kLinePitch;
    out[i] = y;
  }
  // Lines past the bottom go up, and push the lines above them.
  for (int i = count - 1; i >= 0; --i) {
    const float most = i == count - 1 ? bottom : out[i + 1] - kLinePitch;
    if (out[i] > most) out[i] = most;
  }
}

}  // namespace controller_art
