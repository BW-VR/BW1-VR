// The icons of the ring: small pictures that the mod draws in code, so a
// player knows an item at a glance. See docs/controllers.md, "The ring".
// radial_menu.cpp draws them into the letters picture of a seal, so the wax
// shows them as it shows the letters of a word. No picture comes from a file,
// and nothing here is art of the game. No Windows and no game here, so the
// math test compiles this file (ring_icon_checks.h).
//
// AN ICON is a list of shapes in a box of 100 by 100 units, with y down. Each
// shape is a signed distance: below 0 is inside. A shape adds to the icon
// (the nearer of the two distances), or it cuts the icon (the farther of the
// distance and the inverse of the cut). So a hole, or a gap round a part that
// goes over another part, is a cut.
//
// THE SIZE. A seal shows an icon at about 50 px, so 1 unit is about half a
// pixel. A stroke is 8 units or more, and each gap is 7 units or more, so no
// detail of the icon is less than 4 px (as on the hint cards).

#pragma once

#include <math.h>

namespace ring_icon_math {

enum Icon {
  kIconNone,
  kIconLeash,
  kIconPlaces,
  kIconSave,
  kIconLoad,
  kIconSettings,
  kIconRecenter,
  kIconNames,
  kIconDetails,
  kIconPause,
  kIconPlay,
  kIconMore,
  kIconBack,
  kIconHelp,
  kIconSnapshot,
  kIconSlower,
  kIconFaster,
  kIconPlayground,
  kIconTemple,
  kIconEnter,
  kIconLeave,
  kIconCreature,
  kIconTown,
  kIconLand,
  kIconMarkSet,
  kIconMarkGo,
  kIconPlus,
  kIconBin,
  kIconAggression,
  kIconLearning,
  kIconCompassion,
  kIconLeashNext,
  kIconLeashPrevious,
  kIconDebug,
  kIconChallenge,
  kIconLibrary,
  kIconTick,
  kIcons
};

// The kinds of a shape, and their numbers p[0] to p[6].
enum Kind {
  kSegment,  // x0 y0 x1 y1 r: a stroke with round ends
  kDisc,     // x y r
  kRing,     // x y r w: a circle line of width w
  kArc,      // x y r w a0 a1: a part of a ring line from the angle a0 to a1
             // in degrees, with round ends. 0 is to the right, 90 is down
  kBox,      // x0 y0 x1 y1 r: a box with round corners of radius r
  kTriangle, // x0 y0 x1 y1 x2 y2
  kQuad,     // x0 y0 x1 y1 x2 y2 x3 y3: four corners in order
  kStar,     // x y r_out r_in: a star with 5 points, a point at the top
};

enum Op { kAdd, kCut };

struct Shape {
  int kind;
  int op;
  float p[8];
};

// --- the distances ---------------------------------------------------------

inline float Length(float x, float y) { return sqrtf(x * x + y * y); }

inline float SegmentDistance(float px, float py, float ax, float ay, float bx,
                             float by) {
  const float dx = bx - ax, dy = by - ay;
  const float len2 = dx * dx + dy * dy;
  float t = len2 > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0f;
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return Length(px - (ax + t * dx), py - (ay + t * dy));
}

// The signed distance to a polygon of n points (even and odd rule).
inline float PolygonDistance(float px, float py, const float* xy, int n) {
  float d = 1e30f;
  bool inside = false;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    const float ax = xy[2 * j], ay = xy[2 * j + 1];
    const float bx = xy[2 * i], by = xy[2 * i + 1];
    const float e = SegmentDistance(px, py, ax, ay, bx, by);
    if (e < d) d = e;
    if (((by > py) != (ay > py)) &&
        (px < (ax - bx) * (py - by) / (ay - by) + bx)) {
      inside = !inside;
    }
  }
  return inside ? -d : d;
}

inline float ShapeDistance(const Shape& s, float x, float y) {
  const float* p = s.p;
  switch (s.kind) {
    case kSegment:
      return SegmentDistance(x, y, p[0], p[1], p[2], p[3]) - p[4];
    case kDisc:
      return Length(x - p[0], y - p[1]) - p[2];
    case kRing:
      return fabsf(Length(x - p[0], y - p[1]) - p[2]) - 0.5f * p[3];
    case kArc: {
      const float dx = x - p[0], dy = y - p[1];
      float a = atan2f(dy, dx) * 57.29578f;
      // The angle of the point from a0, in 0 to 360.
      float from = a - p[4];
      while (from < 0.0f) from += 360.0f;
      while (from >= 360.0f) from -= 360.0f;
      if (from <= p[5] - p[4]) {
        return fabsf(Length(dx, dy) - p[2]) - 0.5f * p[3];
      }
      const float a0 = p[4] * 0.01745329f, a1 = p[5] * 0.01745329f;
      const float e0 = Length(x - (p[0] + p[2] * cosf(a0)),
                              y - (p[1] + p[2] * sinf(a0)));
      const float e1 = Length(x - (p[0] + p[2] * cosf(a1)),
                              y - (p[1] + p[2] * sinf(a1)));
      return (e0 < e1 ? e0 : e1) - 0.5f * p[3];
    }
    case kBox: {
      const float cx = 0.5f * (p[0] + p[2]), cy = 0.5f * (p[1] + p[3]);
      const float qx = fabsf(x - cx) - (0.5f * (p[2] - p[0]) - p[4]);
      const float qy = fabsf(y - cy) - (0.5f * (p[3] - p[1]) - p[4]);
      const float ox = qx > 0.0f ? qx : 0.0f, oy = qy > 0.0f ? qy : 0.0f;
      const float in = qx > qy ? qx : qy;
      return Length(ox, oy) + (in < 0.0f ? in : 0.0f) - p[4];
    }
    case kTriangle:
      return PolygonDistance(x, y, p, 3);
    case kQuad:
      return PolygonDistance(x, y, p, 4);
    case kStar: {
      float xy[20];
      for (int i = 0; i < 10; ++i) {
        const float r = (i & 1) ? p[3] : p[2];
        const float a = (-90.0f + 36.0f * i) * 0.01745329f;
        xy[2 * i] = p[0] + r * cosf(a);
        xy[2 * i + 1] = p[1] + r * sinf(a);
      }
      return PolygonDistance(x, y, xy, 10);
    }
    default:
      return 1e30f;
  }
}

// --- the icons -------------------------------------------------------------

// The stroke of a line icon: 8 units wide.
const float kR = 4.5f;

#define RING_ICON_SHAPES(name) const Shape name[] =

RING_ICON_SHAPES(kShapesLeash){
    // A leash: the loop for the hand, the line, and the ring of the collar.
    {kRing, kAdd, {66, 28, 19, 10}},
    {kSegment, kAdd, {52, 42, 31, 66, 5}},
    {kRing, kAdd, {24, 76, 14, 9}},
};
RING_ICON_SHAPES(kShapesPlaces){
    // A map pin.
    {kDisc, kAdd, {50, 38, 28}},
    {kTriangle, kAdd, {26, 52, 74, 52, 50, 94}},
    {kDisc, kCut, {50, 38, 11}},
};
RING_ICON_SHAPES(kShapesSave){
    // An arrow down into a tray.
    {kSegment, kAdd, {16, 62, 16, 86, kR}},
    {kSegment, kAdd, {16, 86, 84, 86, kR}},
    {kSegment, kAdd, {84, 86, 84, 62, kR}},
    {kSegment, kAdd, {50, 10, 50, 62, kR}},
    {kSegment, kAdd, {30, 44, 50, 64, kR}},
    {kSegment, kAdd, {70, 44, 50, 64, kR}},
};
RING_ICON_SHAPES(kShapesLoad){
    // An arrow up out of a tray.
    {kSegment, kAdd, {16, 62, 16, 86, kR}},
    {kSegment, kAdd, {16, 86, 84, 86, kR}},
    {kSegment, kAdd, {84, 86, 84, 62, kR}},
    {kSegment, kAdd, {50, 66, 50, 12, kR}},
    {kSegment, kAdd, {30, 32, 50, 12, kR}},
    {kSegment, kAdd, {70, 32, 50, 12, kR}},
};
RING_ICON_SHAPES(kShapesSettings){
    // A gear with 8 teeth.
    {kDisc, kAdd, {50, 50, 31}},
    {kSegment, kAdd, {50, 22, 50, 10, 8}},
    {kSegment, kAdd, {69.8f, 30.2f, 78.3f, 21.7f, 8}},
    {kSegment, kAdd, {78, 50, 90, 50, 8}},
    {kSegment, kAdd, {69.8f, 69.8f, 78.3f, 78.3f, 8}},
    {kSegment, kAdd, {50, 78, 50, 90, 8}},
    {kSegment, kAdd, {30.2f, 69.8f, 21.7f, 78.3f, 8}},
    {kSegment, kAdd, {22, 50, 10, 50, 8}},
    {kSegment, kAdd, {30.2f, 30.2f, 21.7f, 21.7f, 8}},
    {kDisc, kCut, {50, 50, 13}},
};
RING_ICON_SHAPES(kShapesRecenter){
    // A sight: a circle, four marks and a dot in the middle.
    {kRing, kAdd, {50, 50, 27, 9}},
    {kSegment, kAdd, {50, 6, 50, 24, kR}},
    {kSegment, kAdd, {50, 76, 50, 94, kR}},
    {kSegment, kAdd, {6, 50, 24, 50, kR}},
    {kSegment, kAdd, {76, 50, 94, 50, kR}},
    {kDisc, kAdd, {50, 50, 8}},
};
RING_ICON_SHAPES(kShapesNames){
    // A villager with a label of words over the head.
    {kBox, kAdd, {10, 2, 90, 34, 8}},
    {kBox, kCut, {24, 14, 76, 22, 2}},
    {kDisc, kAdd, {50, 54, 12}},
    {kBox, kAdd, {26, 70, 74, 100, 16}},
    {kBox, kCut, {0, 96, 100, 120, 0}},
};
RING_ICON_SHAPES(kShapesDetails){
    // The letter i in a circle.
    {kRing, kAdd, {50, 50, 39, 9}},
    {kDisc, kAdd, {50, 29, 7}},
    {kSegment, kAdd, {50, 46, 50, 74, 5.5f}},
};
RING_ICON_SHAPES(kShapesPause){
    {kBox, kAdd, {24, 16, 42, 84, 4}},
    {kBox, kAdd, {58, 16, 76, 84, 4}},
};
RING_ICON_SHAPES(kShapesPlay){
    {kTriangle, kAdd, {28, 14, 28, 86, 86, 50}},
};
RING_ICON_SHAPES(kShapesMore){
    {kDisc, kAdd, {18, 50, 10}},
    {kDisc, kAdd, {50, 50, 10}},
    {kDisc, kAdd, {82, 50, 10}},
};
RING_ICON_SHAPES(kShapesBack){
    // An arrow that turns back to the left.
    {kSegment, kAdd, {30, 28, 52, 28, kR}},
    {kArc, kAdd, {52, 52, 24, 9, -90, 90}},
    {kSegment, kAdd, {52, 76, 34, 76, kR}},
    {kSegment, kAdd, {34, 14, 18, 28, kR}},
    {kSegment, kAdd, {34, 42, 18, 28, kR}},
};
RING_ICON_SHAPES(kShapesHelp){
    // A question mark.
    {kArc, kAdd, {50, 32, 20, 10, 170, 410}},
    {kSegment, kAdd, {65.3f, 44.9f, 50, 58, 5}},
    {kSegment, kAdd, {50, 58, 50, 66, 5}},
    {kDisc, kAdd, {50, 84, 7}},
};
RING_ICON_SHAPES(kShapesSnapshot){
    // A camera.
    {kBox, kAdd, {10, 30, 90, 84, 9}},
    {kBox, kAdd, {32, 18, 62, 34, 4}},
    {kDisc, kCut, {50, 57, 20}},
    {kDisc, kAdd, {50, 57, 11}},
};
RING_ICON_SHAPES(kShapesSlower){
    {kSegment, kAdd, {48, 20, 22, 50, 5}},
    {kSegment, kAdd, {22, 50, 48, 80, 5}},
    {kSegment, kAdd, {80, 20, 54, 50, 5}},
    {kSegment, kAdd, {54, 50, 80, 80, 5}},
};
RING_ICON_SHAPES(kShapesFaster){
    {kSegment, kAdd, {52, 20, 78, 50, 5}},
    {kSegment, kAdd, {78, 50, 52, 80, 5}},
    {kSegment, kAdd, {20, 20, 46, 50, 5}},
    {kSegment, kAdd, {46, 50, 20, 80, 5}},
};
RING_ICON_SHAPES(kShapesPlayground){
    {kStar, kAdd, {50, 54, 46, 19}},
};
RING_ICON_SHAPES(kShapesTemple){
    // A roof, four columns and a step.
    {kTriangle, kAdd, {10, 34, 90, 34, 50, 8}},
    {kBox, kAdd, {14, 34, 86, 42, 1}},
    {kBox, kAdd, {18, 47, 29, 80, 1}},
    {kBox, kAdd, {36, 47, 47, 80, 1}},
    {kBox, kAdd, {53, 47, 64, 80, 1}},
    {kBox, kAdd, {71, 47, 82, 80, 1}},
    {kBox, kAdd, {8, 84, 92, 94, 2}},
};
RING_ICON_SHAPES(kShapesEnter){
    // A door with an arch, and an arrow that goes in.
    {kBox, kAdd, {34, 34, 90, 94, 0}},
    {kDisc, kAdd, {62, 34, 28}},
    {kBox, kCut, {44, 40, 80, 94, 0}},
    {kDisc, kCut, {62, 40, 18}},
    {kSegment, kCut, {4, 68, 62, 68, 11}},
    {kSegment, kAdd, {6, 68, 56, 68, kR}},
    {kSegment, kAdd, {42, 54, 58, 68, kR}},
    {kSegment, kAdd, {42, 82, 58, 68, kR}},
};
RING_ICON_SHAPES(kShapesLeave){
    // A door with an arch, and an arrow that goes out.
    {kBox, kAdd, {10, 34, 66, 94, 0}},
    {kDisc, kAdd, {38, 34, 28}},
    {kBox, kCut, {20, 40, 56, 94, 0}},
    {kDisc, kCut, {38, 40, 18}},
    {kSegment, kCut, {38, 68, 94, 68, 11}},
    {kSegment, kAdd, {40, 68, 92, 68, kR}},
    {kSegment, kAdd, {78, 54, 93, 68, kR}},
    {kSegment, kAdd, {78, 82, 93, 68, kR}},
};
RING_ICON_SHAPES(kShapesCreature){
    // A paw.
    {kDisc, kAdd, {50, 66, 19}},
    {kDisc, kAdd, {37, 74, 14}},
    {kDisc, kAdd, {63, 74, 14}},
    {kDisc, kAdd, {17, 44, 10}},
    {kDisc, kAdd, {36, 24, 11}},
    {kDisc, kAdd, {64, 24, 11}},
    {kDisc, kAdd, {83, 44, 10}},
};
RING_ICON_SHAPES(kShapesTown){
    // Two houses.
    {kBox, kAdd, {58, 58, 92, 90, 0}},
    {kTriangle, kAdd, {52, 60, 98, 60, 75, 34}},
    {kBox, kCut, {6, 44, 62, 98, 0}},
    {kTriangle, kCut, {0, 56, 68, 56, 34, 14}},
    {kBox, kAdd, {12, 50, 56, 90, 0}},
    {kTriangle, kAdd, {6, 52, 62, 52, 34, 22}},
    {kBox, kCut, {28, 68, 40, 92, 2}},
};
RING_ICON_SHAPES(kShapesLand){
    // Two hills and the sun.
    {kTriangle, kAdd, {4, 86, 42, 24, 80, 86}},
    {kTriangle, kCut, {40, 92, 68, 40, 100, 92}},
    {kTriangle, kAdd, {50, 86, 68, 52, 96, 86}},
    {kDisc, kAdd, {80, 22, 10}},
    {kBox, kAdd, {4, 86, 96, 94, 2}},
};
RING_ICON_SHAPES(kShapesMarkSet){
    // A pin and a plus.
    {kDisc, kAdd, {38, 40, 24}},
    {kTriangle, kAdd, {18, 52, 58, 52, 38, 92}},
    {kDisc, kCut, {38, 40, 9}},
    {kDisc, kCut, {78, 26, 22}},
    {kSegment, kAdd, {78, 12, 78, 40, kR}},
    {kSegment, kAdd, {64, 26, 92, 26, kR}},
};
RING_ICON_SHAPES(kShapesMarkGo){
    // A pin and an arrow to it.
    {kDisc, kAdd, {62, 40, 24}},
    {kTriangle, kAdd, {42, 52, 82, 52, 62, 92}},
    {kDisc, kCut, {62, 40, 9}},
    {kSegment, kAdd, {6, 76, 34, 76, kR}},
    {kSegment, kAdd, {22, 64, 36, 76, kR}},
    {kSegment, kAdd, {22, 88, 36, 76, kR}},
};
RING_ICON_SHAPES(kShapesPlus){
    {kSegment, kAdd, {50, 14, 50, 86, 7}},
    {kSegment, kAdd, {14, 50, 86, 50, 7}},
};
RING_ICON_SHAPES(kShapesBin){
    // A bin: the lid with its grip, and the body with two slots.
    {kSegment, kAdd, {14, 24, 86, 24, kR}},
    {kSegment, kAdd, {38, 22, 40, 10, kR}},
    {kSegment, kAdd, {40, 10, 60, 10, kR}},
    {kSegment, kAdd, {60, 10, 62, 22, kR}},
    {kQuad, kAdd, {20, 34, 80, 34, 72, 92, 28, 92}},
    {kSegment, kCut, {41, 44, 42, 82, 4}},
    {kSegment, kCut, {59, 44, 58, 82, 4}},
};
RING_ICON_SHAPES(kShapesAggression){
    // Three marks of a claw.
    {kSegment, kAdd, {38, 10, 18, 88, 5.5f}},
    {kSegment, kAdd, {60, 10, 40, 88, 5.5f}},
    {kSegment, kAdd, {82, 10, 62, 88, 5.5f}},
};
RING_ICON_SHAPES(kShapesLearning){
    // An open book.
    {kQuad, kAdd, {8, 22, 46, 32, 46, 88, 8, 76}},
    {kQuad, kAdd, {54, 32, 92, 22, 92, 76, 54, 88}},
};
RING_ICON_SHAPES(kShapesCompassion){
    // A heart.
    {kDisc, kAdd, {32, 36, 21}},
    {kDisc, kAdd, {68, 36, 21}},
    {kTriangle, kAdd, {13.5f, 46, 86.5f, 46, 50, 90}},
};
RING_ICON_SHAPES(kShapesLeashNext){
    {kRing, kAdd, {40, 60, 26, 10}},
    {kSegment, kAdd, {78, 8, 78, 40, kR}},
    {kSegment, kAdd, {62, 24, 94, 24, kR}},
};
RING_ICON_SHAPES(kShapesLeashPrevious){
    {kRing, kAdd, {40, 60, 26, 10}},
    {kSegment, kAdd, {62, 24, 94, 24, kR}},
};
RING_ICON_SHAPES(kShapesDebug){
    // A beetle.
    {kDisc, kAdd, {50, 60, 23}},
    {kDisc, kAdd, {50, 28, 12}},
    {kSegment, kAdd, {30, 46, 12, 36, 4}},
    {kSegment, kAdd, {28, 62, 8, 62, 4}},
    {kSegment, kAdd, {30, 76, 14, 88, 4}},
    {kSegment, kAdd, {70, 46, 88, 36, 4}},
    {kSegment, kAdd, {72, 62, 92, 62, 4}},
    {kSegment, kAdd, {70, 76, 86, 88, 4}},
    {kSegment, kCut, {50, 44, 50, 84, 3.5f}},
};
RING_ICON_SHAPES(kShapesChallenge){
    // A flag.
    {kSegment, kAdd, {24, 10, 24, 92, kR}},
    {kTriangle, kAdd, {28, 12, 28, 54, 86, 33}},
};
RING_ICON_SHAPES(kShapesLibrary){
    // Books on a shelf.
    {kBox, kAdd, {12, 22, 30, 86, 2}},
    {kBox, kAdd, {36, 30, 54, 86, 2}},
    {kQuad, kAdd, {58, 36, 74, 30, 92, 82, 76, 88}},
    {kBox, kAdd, {6, 88, 94, 96, 2}},
};

RING_ICON_SHAPES(kShapesTick){
    {kSegment, kAdd, {14, 52, 40, 78, 8}},
    {kSegment, kAdd, {40, 78, 86, 22, 8}},
};

#undef RING_ICON_SHAPES

struct IconShapes {
  const Shape* shapes;
  int count;
};

#define RING_ICON_ENTRY(a) {a, static_cast<int>(sizeof(a) / sizeof(a[0]))}

inline IconShapes ShapesOf(int icon) {
  static const IconShapes kTable[kIcons] = {
      {nullptr, 0},
      RING_ICON_ENTRY(kShapesLeash),
      RING_ICON_ENTRY(kShapesPlaces),
      RING_ICON_ENTRY(kShapesSave),
      RING_ICON_ENTRY(kShapesLoad),
      RING_ICON_ENTRY(kShapesSettings),
      RING_ICON_ENTRY(kShapesRecenter),
      RING_ICON_ENTRY(kShapesNames),
      RING_ICON_ENTRY(kShapesDetails),
      RING_ICON_ENTRY(kShapesPause),
      RING_ICON_ENTRY(kShapesPlay),
      RING_ICON_ENTRY(kShapesMore),
      RING_ICON_ENTRY(kShapesBack),
      RING_ICON_ENTRY(kShapesHelp),
      RING_ICON_ENTRY(kShapesSnapshot),
      RING_ICON_ENTRY(kShapesSlower),
      RING_ICON_ENTRY(kShapesFaster),
      RING_ICON_ENTRY(kShapesPlayground),
      RING_ICON_ENTRY(kShapesTemple),
      RING_ICON_ENTRY(kShapesEnter),
      RING_ICON_ENTRY(kShapesLeave),
      RING_ICON_ENTRY(kShapesCreature),
      RING_ICON_ENTRY(kShapesTown),
      RING_ICON_ENTRY(kShapesLand),
      RING_ICON_ENTRY(kShapesMarkSet),
      RING_ICON_ENTRY(kShapesMarkGo),
      RING_ICON_ENTRY(kShapesPlus),
      RING_ICON_ENTRY(kShapesBin),
      RING_ICON_ENTRY(kShapesAggression),
      RING_ICON_ENTRY(kShapesLearning),
      RING_ICON_ENTRY(kShapesCompassion),
      RING_ICON_ENTRY(kShapesLeashNext),
      RING_ICON_ENTRY(kShapesLeashPrevious),
      RING_ICON_ENTRY(kShapesDebug),
      RING_ICON_ENTRY(kShapesChallenge),
      RING_ICON_ENTRY(kShapesLibrary),
      RING_ICON_ENTRY(kShapesTick),
  };
  if (icon < 0 || icon >= kIcons) return kTable[0];
  return kTable[icon];
}

#undef RING_ICON_ENTRY

// The signed distance of the icon at x, y of its box of 100 units. 1e30 for
// no icon.
inline float IconDistance(int icon, float x, float y) {
  const IconShapes s = ShapesOf(icon);
  float d = 1e30f;
  for (int i = 0; i < s.count; ++i) {
    const float e = ShapeDistance(s.shapes[i], x, y);
    if (s.shapes[i].op == kAdd) {
      if (e < d) d = e;
    } else if (-e > d) {
      d = -e;
    }
  }
  return d;
}

// How much of the pixel at px, py (its middle) the icon covers, 0 to 1. The
// box of the icon is `size` pixels wide and high, with its top left corner
// at x0, y0.
inline float IconCover(int icon, float x0, float y0, float size, float px,
                       float py) {
  if (!(size > 0.0f)) return 0.0f;
  const float units = 100.0f / size;
  const float d = IconDistance(icon, (px - x0) * units, (py - y0) * units);
  const float cover = 0.5f - d / units;
  return cover < 0.0f ? 0.0f : (cover > 1.0f ? 1.0f : cover);
}

}  // namespace ring_icon_math
