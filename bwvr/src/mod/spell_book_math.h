// The spell book: the pure math. See docs/miracles.md, "The spell book".
// spell_book.cpp uses it, and test/controller_math checks it
// (spell_book_checks.h). Nothing here reads a global or touches the game.
//
// THE BOOK is one picture over the hand that does not point. It holds the
// miracles that the player can ask for now, in a grid of cells. The center of
// the book is at a fixed offset from the grip pose of that hand, so the book
// moves with the hand, one to one. The picture faces the head with no roll,
// as the ring does, so the player can read it at each turn of the hand.
//
// THE PICK. The grip point of the pointing hand near the middle of a cell
// picks that item. The pick stays on its item until the hand is clearly away
// from it, so the highlight does not flicker at a border.

#pragma once

#include <math.h>

#include "controller_math.h"

namespace spell_book_math {

// The most items: the 34 miracles of the book of all miracles fit in 6 rows
// of 6 cells.
const int kMaxItems = 36;

// THE BOOK OF ALL MIRACLES (debug_all_miracles=1 and debug_all_book=1). An
// item is a seed type at a power level: the key is the seed type plus
// kKeyTypes times the level (0 the plain miracle, 1 the "Increase" and 2 the
// "Extreme" of the game). The game has 30 seed types and 3 levels.
const int kKeyTypes = 30;
const int kKeyLevels = 3;

inline int KeyOf(int type, int level) { return type + kKeyTypes * level; }
inline int TypeOfKey(int key) { return key < 0 ? -1 : key % kKeyTypes; }
inline int LevelOfKey(int key) { return key < 0 ? 0 : key / kKeyTypes; }

// The items of the book of all miracles, in the order of the seed types of
// the game, each one followed by its power levels. on[t] is the flag of the
// game for seed type t (GSpellSeedInfo+0x17C), and levels[t] is the count of
// its magic types (GSpellSeedInfo+0x124, 1 to 3). A type that the game has
// off is not in the book. The count of the keys.
inline int AllKeys(const bool on[kKeyTypes], const int levels[kKeyTypes],
                   int out[], int most) {
  int count = 0;
  for (int t = 0; t < kKeyTypes; ++t) {
    if (!on[t]) continue;
    int n = levels[t];
    if (n < 1) n = 1;
    if (n > kKeyLevels) n = kKeyLevels;
    for (int level = 0; level < n && count < most; ++level) {
      out[count++] = KeyOf(t, level);
    }
  }
  return count;
}

// The picture: 32 pixels for each cm, as the ring. The ring and the book
// share one quad layer, and the swapchain of that layer is 1024 by 1024.
const float kPixelsPerMeter = 3200.0f;
const int kTexture = 1024;

// The band at the top of the book: the title, or the line of an empty book.
const float kTitleM = 0.030f;
// The width of a book with no item, or with few items.
const float kLeastWidthM = 0.16f;
// The size of a book with no item: the title and the reason, each on a line
// of its own at the size of the design.
const float kEmptyWidthM = 0.20f;
const float kEmptyHeightM = 0.060f;

// The center of the book from the grip point, in the axes of the grip pose:
// along the normal of the open palm (+x of the grip of the left hand, -x of
// the right hand, as OpenXR gives the grip pose), and a little up along +y.
// With the palm up the book is over the palm, and with the palm to the side
// it is beside the hand, between the two hands.
const float kPalmOutM = 0.12f;
const float kPalmUpM = 0.02f;

// The pick: a hand within kPickFraction of a cell of the middle of an item
// picks it, and the item stays until the hand is kKeepFraction of a cell
// away. The hand must also be within kDepthM in front of or behind the book,
// and kKeepDepthM to keep the item.
const float kPickFraction = 0.45f;
const float kKeepFraction = 0.65f;
const float kDepthM = 0.06f;
const float kKeepDepthM = 0.09f;

struct Layout {
  int count;
  int columns;
  int rows;
  float cell_m;    // the side of one cell
  float width_m;   // the whole book
  float height_m;
};

// The cell size goes down as the count goes up, so all 30 miracles of the
// game fit in the picture.
inline Layout LayoutFor(int count) {
  Layout l;
  if (count < 0) count = 0;
  if (count > kMaxItems) count = kMaxItems;
  l.count = count;
  if (count == 0) {
    l.columns = 0;
    l.rows = 0;
    l.cell_m = 0.0f;
    l.width_m = kEmptyWidthM;
    l.height_m = kEmptyHeightM;
    return l;
  }
  l.columns = count <= 3    ? count
              : count <= 4  ? 2
              : count <= 9  ? 3
              : count <= 16 ? 4
              : count <= 25 ? 5
                            : 6;
  l.rows = (count + l.columns - 1) / l.columns;
  l.cell_m = count <= 9 ? 0.065f : (count <= 16 ? 0.05f : 0.04f);
  l.width_m = l.columns * l.cell_m;
  if (l.width_m < kLeastWidthM) l.width_m = kLeastWidthM;
  l.height_m = kTitleM + l.rows * l.cell_m;
  return l;
}

// The middle of item i on the book: meters to the right and up from the
// center of the book. The first item is at the top left, and the items go
// to the right, then down.
inline void ItemCenter(const Layout& l, int i, float* x, float* y) {
  *x = 0.0f;
  *y = 0.0f;
  if (l.columns <= 0 || i < 0 || i >= l.count) return;
  const int column = i % l.columns;
  const int row = i / l.columns;
  // The last row has its items in the middle.
  const int in_row =
      row == l.rows - 1 ? l.count - row * l.columns : l.columns;
  *x = (column - 0.5f * (in_row - 1)) * l.cell_m;
  *y = 0.5f * l.height_m - kTitleM - (row + 0.5f) * l.cell_m;
}

// THE BUBBLE of a cell: its radius is kBubbleShare of a cell, and its middle
// is kBubbleRise of a cell above the middle of the cell. The name is under
// it. With spell_book_orbs=1 the orb of the game is there, on the plane of
// the book.
const float kBubbleShare = 0.33f;
const float kBubbleRise = 0.10f;

// The middle of the bubble of item i, in meters from the center of the book.
inline void OrbCenter(const Layout& l, int i, float* x, float* y) {
  ItemCenter(l, i, x, y);
  if (l.columns > 0 && i >= 0 && i < l.count) *y += kBubbleRise * l.cell_m;
}

// The size of the picture in pixels for a layout. Even numbers, so the
// part that shows has its center at the center of the texture.
inline int PixelsOf(float meters) {
  int p = static_cast<int>(meters * kPixelsPerMeter + 0.5f);
  p += p & 1;
  if (p > kTexture) p = kTexture;
  if (p < 2) p = 2;
  return p;
}

// The center of the book, in the same space as the grip pose. left: the
// hand is the left hand.
inline void BookCenter(const float grip[3], const float orientation[4],
                       bool left, float out[3]) {
  const float local[3] = {left ? kPalmOutM : -kPalmOutM, kPalmUpM, 0.0f};
  float turned[3];
  controller_math::QuatRotate(orientation, local, turned);
  for (int i = 0; i < 3; ++i) out[i] = grip[i] + turned[i];
}

// The plane of the book: its center faces the head, with no roll.
struct Plane {
  float center[3];
  float orientation[4];  // x y z w
  float right[3];        // +x of the quad: to the right, as the player sees it
  float up[3];           // +y of the quad
  float normal[3];       // +z of the quad: toward the head
};

inline void PlaneAt(const float center[3], const float head[3], Plane* out) {
  for (int i = 0; i < 3; ++i) out->center[i] = center[i];
  controller_math::FacePose(center, head, out->orientation);
  const float x[3] = {1.0f, 0.0f, 0.0f};
  const float y[3] = {0.0f, 1.0f, 0.0f};
  const float z[3] = {0.0f, 0.0f, 1.0f};
  controller_math::QuatRotate(out->orientation, x, out->right);
  controller_math::QuatRotate(out->orientation, y, out->up);
  controller_math::QuatRotate(out->orientation, z, out->normal);
}

// A point on the plane, from meters to the right and up.
inline void PointOn(const Plane& p, float x, float y, float out[3]) {
  for (int i = 0; i < 3; ++i) {
    out[i] = p.center[i] + p.right[i] * x + p.up[i] * y;
  }
}

// A point in the space of the plane: to the right, up, and toward the head.
inline void InPlane(const Plane& p, const float point[3], float out[3]) {
  const float d[3] = {point[0] - p.center[0], point[1] - p.center[1],
                      point[2] - p.center[2]};
  out[0] = d[0] * p.right[0] + d[1] * p.right[1] + d[2] * p.right[2];
  out[1] = d[0] * p.up[0] + d[1] * p.up[1] + d[2] * p.up[2];
  out[2] = d[0] * p.normal[0] + d[1] * p.normal[1] + d[2] * p.normal[2];
}

// The item under the hand: hand is the hand in the space of the plane
// (InPlane). current is the item of the last frame, or -1. The result is the
// item of this frame, or -1.
inline int Pick(const Layout& l, const float hand[3], int current) {
  if (l.count <= 0 || l.cell_m <= 0.0f) return -1;
  const float depth = fabsf(hand[2]);
  float best_gap = 1e9f;
  int best = -1;
  float current_gap = 1e9f;
  for (int i = 0; i < l.count; ++i) {
    float x = 0.0f, y = 0.0f;
    ItemCenter(l, i, &x, &y);
    const float dx = hand[0] - x, dy = hand[1] - y;
    const float gap = sqrtf(dx * dx + dy * dy);
    if (gap < best_gap) {
      best_gap = gap;
      best = i;
    }
    if (i == current) current_gap = gap;
  }
  const float pick = kPickFraction * l.cell_m;
  const float keep = kKeepFraction * l.cell_m;
  // The item of the last frame stays while the hand is near it, unless the
  // hand is at the middle of a different item.
  if (current >= 0 && current < l.count && current_gap <= keep &&
      depth <= kKeepDepthM) {
    if (best != current && best_gap <= 0.5f * pick && depth <= kDepthM) {
      return best;
    }
    return current;
  }
  if (best >= 0 && best_gap <= pick && depth <= kDepthM) return best;
  return -1;
}

// THE OPEN AND THE CLOSE. One step a frame.
struct OpenInput {
  bool press;      // the button of the book went down on this frame
  bool can_open;   // the hand is active, the controllers can act, and the
                   // game is in a state where the player can ask for a
                   // miracle (not in the Temple, no cut scene)
  bool ring;       // the ring is open or still shows
  bool menu;       // a dialog box or the game menu is up
  bool plucked;    // the pointing hand took a miracle from the book
};

enum OpenEvent {
  kNothing,
  kOpened,
  kRefused,        // a press that cannot open the book now
  kClosedButton,
  kClosedRing,
  kClosedMenu,
  kClosedPluck,
  kClosedCannot,   // the hand or the game cannot keep the book now
};

inline int OpenStep(bool* open, const OpenInput& in) {
  if (*open) {
    int why = kNothing;
    if (in.ring) {
      why = kClosedRing;
    } else if (in.menu) {
      why = kClosedMenu;
    } else if (in.plucked) {
      why = kClosedPluck;
    } else if (!in.can_open) {
      why = kClosedCannot;
    } else if (in.press) {
      why = kClosedButton;
    }
    if (why != kNothing) *open = false;
    return why;
  }
  if (!in.press) return kNothing;
  if (!in.can_open || in.ring || in.menu) return kRefused;
  *open = true;
  return kOpened;
}

// The grip of the pointing hand on the book. The press of the grip on a
// picked item is the pluck, and the book keeps the grip from the game until
// the grip opens. A grip that closes with no item picked stays the game's.
struct GripClaim {
  bool before;  // the grip was down on the last pump
  bool claim;   // the book holds this press of the grip
};

// Returns true while the book holds the grip. *pluck is true on the pump of
// the press.
inline bool GripStep(GripClaim* g, bool grip, bool open, int picked,
                     bool* pluck) {
  *pluck = false;
  const bool edge = grip && !g->before;
  g->before = grip;
  if (!grip) {
    g->claim = false;
    return false;
  }
  if (edge && open && picked >= 0) {
    g->claim = true;
    *pluck = true;
  }
  return g->claim;
}

// THE PULL (spell_book_pull_cm above 0). The grip on an item does not take
// the miracle at once. The bubble of the item comes out of the book on an
// elastic band: it stays between its cell and the hand, at kPullFollow of the
// way to the hand. When the gap from the cell to the hand is the pull length
// more than at the grip, the bubble snaps into the hand and the book sends
// the request. A grip that opens before that puts the bubble back: it springs
// to its cell.
const float kPullFollow = 0.5f;
// The spring back: the offset of the bubble from its cell, times
// exp(-kBackDamp t) cos(kBackRate t). It goes through the cell one time and
// comes back to it.
const float kBackDamp = 9.0f;
const float kBackRate = 25.0f;
const float kBackSeconds = 0.40f;

// How much of the pull is done, 0 to 1. gap is the gap from the cell to the
// hand now, start_gap the gap at the grip, pull_m the pull length.
inline float PullShare(float gap, float start_gap, float pull_m) {
  if (!(pull_m > 0.0f)) return 1.0f;
  const float share = (gap - start_gap) / pull_m;
  return share < 0.0f ? 0.0f : (share > 1.0f ? 1.0f : share);
}

// The place of the bubble while the hand pulls it.
inline void PullBubble(const float cell[3], const float hand[3],
                       float out[3]) {
  for (int i = 0; i < 3; ++i) out[i] = cell[i] + (hand[i] - cell[i]) * kPullFollow;
}

// The strength of the buzz of the pull, 0 to 1: it goes up faster near the
// end, so the player feels the snap come.
inline float PullBuzz(float share) {
  if (!(share > 0.0f)) return 0.0f;
  if (share > 1.0f) share = 1.0f;
  return share * sqrtf(share);
}

// The share of the offset at the release that the bubble has at t seconds
// after the release. 0 at kBackSeconds and after.
inline float BackScale(float t) {
  if (!(t >= 0.0f)) return 1.0f;
  if (t >= kBackSeconds) return 0.0f;
  return expf(-kBackDamp * t) * cosf(kBackRate * t);
}

// The bubble first comes back to its cell at this time after the release.
inline float BackLandSeconds() { return 1.5707963f / kBackRate; }

// THE GAME'S BUBBLE. The game draws the bubble that the hand pulls out in its
// own picture of each eye: a glass ball of this radius in the room. The book
// is a layer over that picture, so the book has a hole where the ball is in
// front of the page.
const float kBubbleRadiusM = 0.03f;
// Half of the space between the eyes. The hole is one picture for both eyes,
// so it is this much larger for each cm that the ball is off the page.
const float kHalfEyesM = 0.032f;

// The hole of the ball in the page, in meters from the middle of the page
// (x to the right, y up) as the head center sees it. False when no part of
// the ball is in front of the page.
struct Hole {
  float x, y, r;
};

inline bool BubbleHole(const Plane& p, const float head[3], const float at[3],
                       float radius, Hole* out) {
  float h[3], b[3];
  for (int i = 0; i < 3; ++i) {
    h[i] = head[i] - p.center[i];
    b[i] = at[i] - p.center[i];
  }
  const float dh = h[0] * p.normal[0] + h[1] * p.normal[1] + h[2] * p.normal[2];
  const float db = b[0] * p.normal[0] + b[1] * p.normal[1] + b[2] * p.normal[2];
  if (!(dh > 0.01f) || db <= -radius || db >= dh - 0.01f) return false;
  // The ray from the head through the ball meets the page at k times the
  // way to the ball.
  const float k = dh / (dh - db);
  float on[3];
  for (int i = 0; i < 3; ++i) on[i] = h[i] + (b[i] - h[i]) * k;
  out->x = on[0] * p.right[0] + on[1] * p.right[1] + on[2] * p.right[2];
  out->y = on[0] * p.up[0] + on[1] * p.up[1] + on[2] * p.up[2];
  out->r = radius * k + kHalfEyesM * fabsf(1.0f - k);
  return true;
}

// THE RING ROUND A BUBBLE, in pixels of the picture, y down. The bubble has
// its middle at bubble_y and the radius bubble_r, and its cell starts at
// cell_top and is cell pixels high. Each item has a thin ring of 0.06 of a
// cell round its bubble. The bubble is 0.10 of a cell above the middle of
// its cell, so the thin ring just fits under the top of the cell. The item
// under the hand has a gold ring of 0.10 of a cell or more. It ends 4 px
// inside the cell (1 px inside the lit cell, which is 3 px inside the cell)
// or 0.16 of a cell out from the bubble, and the fill of the bubble gives
// way to it.
struct Ring {
  float outer;  // the outer edge of the ring
  float fill;   // the radius of the fill of the bubble, and the inner edge
};

inline Ring BubbleRing(float bubble_y, float bubble_r, float cell_top,
                       float cell, bool lit) {
  if (!lit) return {bubble_r + 0.06f * cell, bubble_r};
  const float wide = bubble_r + 0.16f * cell;
  const float room = bubble_y - (cell_top + 4.0f);
  Ring r;
  r.outer = room < wide ? room : wide;
  const float inner = r.outer - 0.10f * cell;
  r.fill = inner < bubble_r ? inner : bubble_r;
  return r;
}

}  // namespace spell_book_math
