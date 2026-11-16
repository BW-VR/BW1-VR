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

// The most items of a list of the game: the 34 miracles and power levels of
// the book of all miracles fit.
const int kMaxItems = 36;

// THE BOOK OF ALL MIRACLES (debug_all_book=1). An item is a seed type at a
// power level: the key is the seed type plus kKeyTypes times the level (0
// the plain miracle, 1 the "Increase" and 2 the "Extreme" of the game). The
// game has 30 seed types and 3 levels.
const int kKeyTypes = 30;
const int kKeyLevels = 3;

inline int KeyOf(int type, int level) { return type + kKeyTypes * level; }
inline int TypeOfKey(int key) { return key < 0 ? -1 : key % kKeyTypes; }
inline int LevelOfKey(int key) { return key < 0 ? 0 : key / kKeyTypes; }

// THE POWER UP of the game for a level. The game's POWER_UP_TYPE is -1
// (POWER_UP_TYPE_NONE) for the plain miracle, 0 for the "Increase" and 1 for
// the "Extreme" (Enum.h of the decompilation). A seed casts the magic type at
// GSpellSeedInfo+0x124 + 4 * (power up + 1). OneOffSpellSeed::
// CreateSpellIntoHand and SpellSeed+0x6C take the power up, not the level.
inline int PowerUpOfLevel(int level) { return level - 1; }
inline int LevelOfPowerUp(int power_up) {
  return power_up < 0 ? 0 : power_up + 1;
}

// The items of the book of all miracles. on[t] is the flag of the game for
// seed type t (GSpellSeedInfo+0x17C), levels[t] the count of its magic types
// (GSpellSeedInfo+0x124, 1 to 3), and creature[t] true for a Creature
// miracle. A type that the game has off is not in the book. The order: the
// miracles with 3 levels, then with 2, then the single miracles of the god,
// then the Creature miracles, each group in the order of the seed types.
// Each miracle is followed by its levels. The count of the keys.
inline int AllKeys(const bool on[kKeyTypes], const int levels[kKeyTypes],
                   const bool creature[kKeyTypes], int out[], int most) {
  int count = 0;
  // group 0, 1, 2: the god's miracles with 3, 2 and 1 levels. 3: Creature.
  for (int group = 0; group < 4; ++group) {
    for (int t = 0; t < kKeyTypes; ++t) {
      if (!on[t]) continue;
      int n = levels[t];
      if (n < 1) n = 1;
      if (n > kKeyLevels) n = kKeyLevels;
      const int of = creature[t] ? 3 : kKeyLevels - n;
      if (of != group) continue;
      for (int level = 0; level < n && count < most; ++level) {
        out[count++] = KeyOf(t, level);
      }
    }
  }
  return count;
}

// THE PAGES of the book of all miracles. A page has the 6 miracles of a
// Village Centre at most, in the cells of the normal book of 6 (3 columns
// of 6.5 cm, 2 rows), and a third row with the cell "Back" at the left and
// the cell "Next" at the right. A family (a miracle and its power levels)
// stays on one page, so a miracle and its levels are side by side.
// starts[p] is the index of the first key of page p, and starts[pages] is
// the count of the keys. The count of the pages, at most `most`.
const int kPageItems = 6;
const int kPageCells = 9;
const int kCellBack = 6;
const int kCellNext = 8;
const int kMostPages = 12;
const int kKeyBack = 1000;   // the cell that goes to the page before
const int kKeyNext = 1001;   // the cell that goes to the next page
const int kKeyEmpty = -1;    // a cell with nothing in it
inline bool IsNavKey(int key) { return key == kKeyBack || key == kKeyNext; }

inline int PagesOf(const int keys[], int count, int starts[], int most) {
  int pages = 0, on_page = 0, end = 0;
  for (int i = 0; i < count;) {
    int family = 1;
    while (i + family < count &&
           TypeOfKey(keys[i + family]) == TypeOfKey(keys[i])) {
      ++family;
    }
    if (pages == 0 || on_page + family > kPageItems) {
      if (pages >= most) break;
      starts[pages++] = i;
      on_page = 0;
    }
    on_page += family;
    i += family;
    end = i;
  }
  starts[pages] = end;
  return pages;
}

// The 9 cells of page `page` (0 the first): its keys, kKeyEmpty in a cell
// with no key, and the cells "Back" and "Next".
inline void PageCells(const int keys[], const int starts[], int pages,
                      int page, int cells[kPageCells]) {
  for (int c = 0; c < kPageCells; ++c) cells[c] = kKeyEmpty;
  if (pages <= 0) return;
  if (page < 0) page = 0;
  if (page >= pages) page = pages - 1;
  int c = 0;
  for (int i = starts[page]; i < starts[page + 1] && c < kPageItems; ++i) {
    cells[c++] = keys[i];
  }
  cells[kCellBack] = kKeyBack;
  cells[kCellNext] = kKeyNext;
}

// The page after a turn: "Next" on the last page goes to the first, and
// "Back" on the first goes to the last.
inline int TurnPage(int page, int pages, bool next) {
  if (pages <= 0) return 0;
  page += next ? 1 : -1;
  if (page >= pages) page = 0;
  if (page < 0) page = pages - 1;
  return page;
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

// THE SHAPES of a book. kShapeGrid: rows and columns. kShapeRing
// (spell_book_ring=1, up to 6 items): the items on a ring, as the ring menu
// has them. kShapeRingPage: a page of the book of all miracles, the 6 cells
// of a page on a ring of 6 (a hexagon) and the seals "Back" and "Next" side
// by side in its middle, where the pointing hand reaches them without a move
// past the hand that holds the book.
const int kShapeGrid = 0;
const int kShapeRing = 1;
const int kShapeRingPage = 2;
const int kRingMost = 6;

// THE ORDER of the items on a ring (spell_book_ring_order). The ring of 6
// has three rows: two items at the top, one at each side of the middle, and
// two at the bottom. kOrderRows: the reading order, left to right in each
// row, the rows from the top. kOrderSides: the first three items down the
// left side and the next three down the right side, for a page with two
// families of three (a miracle, its Increase and its Extreme), so each
// family reads from the top down. kOrderClock (spell_book_ring_order=0):
// the first item at the upper left and the next ones clockwise.
const int kOrderClock = 0;
const int kOrderRows = 1;
const int kOrderSides = 2;
// The radius of the ring, in cells: two items next to each other on a ring
// of 6 are this far apart.
const float kRingCells = 1.2f;
// A seal in the middle of the ring: the middle of each seal is this share of
// a cell to the left or right of the middle, and its bubble has this radius.
const float kSealOut = 0.34f;
const float kSealShare = 0.24f;

struct Layout {
  int count;
  int columns;
  int rows;
  float cell_m;    // the side of one cell
  float width_m;   // the whole book
  float height_m;
  int shape;
  float ring_m;    // the radius of the ring
  int order;       // kOrderClock, kOrderRows or kOrderSides, for a ring
};

// The cell size goes down as the count goes up, so all 30 miracles of the
// game fit in the picture. A ring needs a count of 1 to 6, and a ring page
// the count of the cells of a page.
inline Layout LayoutFor(int count, int shape = kShapeGrid,
                        int order = kOrderRows) {
  Layout l;
  if (count < 0) count = 0;
  if (count > kMaxItems) count = kMaxItems;
  l.count = count;
  l.shape = kShapeGrid;
  l.ring_m = 0.0f;
  l.order = order;
  if (count == 0) {
    l.columns = 0;
    l.rows = 0;
    l.cell_m = 0.0f;
    l.width_m = kEmptyWidthM;
    l.height_m = kEmptyHeightM;
    return l;
  }
  if ((shape == kShapeRing && count <= kRingMost) ||
      (shape == kShapeRingPage && count == kPageCells)) {
    l.shape = shape;
    l.columns = 3;
    l.rows = 3;
    l.cell_m = 0.065f;
    l.ring_m = kRingCells * l.cell_m;
    // The items at the left and the right are one radius out, and the
    // rows at the top and the bottom are sin 60 of the radius up and down.
    l.width_m = 2.0f * l.ring_m + l.cell_m;
    if (l.width_m < kLeastWidthM) l.width_m = kLeastWidthM;
    l.height_m = kTitleM + 2.0f * 0.8660254f * l.ring_m + l.cell_m;
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
// THE SLOTS of a ring: slot s is at 120 - 60 s degrees round the middle, so
// slot 0 is at the upper left, slot 1 at the upper right, slot 2 at the
// right, and so on clockwise to slot 5 at the left. The ring has a flat top
// and bottom, so the middle row has room for the seals between the items at
// the left and the right, and the names under the items do not meet them. A
// ring of fewer than 6 items keeps its items apart and the ring even: the
// order changes the item of a slot, not the slots in use.
inline int RingSlot(int count, int i, int order = kOrderRows) {
  static const int kClock[7][6] = {{0, 0, 0, 0, 0, 0}, {-1, 0, 0, 0, 0, 0},
                                   {5, 2, 0, 0, 0, 0}, {0, 2, 4, 0, 0, 0},
                                   {0, 1, 3, 4, 0, 0}, {0, 1, 2, 3, 4, 0},
                                   {0, 1, 2, 3, 4, 5}};
  static const int kRows[7][6] = {{0, 0, 0, 0, 0, 0}, {-1, 0, 0, 0, 0, 0},
                                  {5, 2, 0, 0, 0, 0}, {0, 2, 4, 0, 0, 0},
                                  {0, 1, 4, 3, 0, 0}, {0, 1, 2, 4, 3, 0},
                                  {0, 1, 5, 2, 4, 3}};
  static const int kSides[6] = {0, 5, 4, 1, 2, 3};
  if (count < 1 || count > kRingMost || i < 0 || i >= count) return -1;
  if (order == kOrderClock) return kClock[count][i];
  if (order == kOrderSides && count == kRingMost) return kSides[i];
  return kRows[count][i];
}

// The order of a page of the book of all miracles from its cells: down the
// two sides (kOrderSides) when the 6 miracles of the page are two families
// of three, else the reading order (kOrderRows).
inline int PageOrder(const int cells[], int count) {
  if (count < kPageItems) return kOrderRows;
  for (int c = 0; c < kPageItems; ++c) {
    if (cells[c] < 0 || IsNavKey(cells[c])) return kOrderRows;
  }
  const int first = TypeOfKey(cells[0]), second = TypeOfKey(cells[3]);
  if (first == second) return kOrderRows;
  for (int c = 0; c < 3; ++c) {
    if (TypeOfKey(cells[c]) != first || TypeOfKey(cells[c + 3]) != second) {
      return kOrderRows;
    }
  }
  return kOrderSides;
}

// A cell of a ring page that holds a seal ("Back" or "Next").
inline bool SealCell(const Layout& l, int i) {
  return l.shape == kShapeRingPage && (i == kCellBack || i == kCellNext);
}

// The middle of slot s (0 to 5) of a ring, and with s = -1 the middle of the
// ring: meters to the right and up from the center of the book. The top row
// of the ring touches the band of the title.
inline void RingSlotCenter(const Layout& l, int s, float* x, float* y) {
  const float mid = 0.5f * l.height_m - kTitleM - 0.5f * l.cell_m -
                    0.8660254f * l.ring_m;
  *x = 0.0f;
  *y = mid;
  if (s < 0 || s >= kRingMost) return;
  const float a = (120.0f - 60.0f * s) * 0.017453293f;
  *x = l.ring_m * cosf(a);
  *y = mid + l.ring_m * sinf(a);
}

inline void ItemCenter(const Layout& l, int i, float* x, float* y) {
  *x = 0.0f;
  *y = 0.0f;
  if (l.columns <= 0 || i < 0 || i >= l.count) return;
  if (l.shape != kShapeGrid) {
    int slot = -1;
    if (l.shape == kShapeRing) {
      slot = RingSlot(l.count, i, l.order);
    } else if (i < kPageItems) {
      slot = RingSlot(kPageItems, i, l.order);
    }
    RingSlotCenter(l, slot, x, y);
    if (slot < 0 && l.shape == kShapeRingPage) {
      if (i == kCellBack) *x = -kSealOut * l.cell_m;
      if (i == kCellNext) *x = kSealOut * l.cell_m;
    }
    return;
  }
  const int column = i % l.columns;
  const int row = i / l.columns;
  // The last row has its items in the middle.
  const int in_row =
      row == l.rows - 1 ? l.count - row * l.columns : l.columns;
  *x = (column - 0.5f * (in_row - 1)) * l.cell_m;
  *y = 0.5f * l.height_m - kTitleM - (row + 0.5f) * l.cell_m;
}

// THE SOCKETS of a ring (spell_book_look=1): the 6 slots of a ring always
// show, so each page is even. item[s] is the item that holds slot s, or -1
// for an empty socket. keys are the keys of the items: a cell of a ring page
// with kKeyEmpty holds no slot. A ring of 1 item has it in the middle, so
// its 6 sockets are empty. A grid has no slots: all are -1.
inline void SlotItems(const Layout& l, const int keys[], int item[kRingMost]) {
  for (int s = 0; s < kRingMost; ++s) item[s] = -1;
  if (l.shape == kShapeRing) {
    for (int i = 0; i < l.count; ++i) {
      const int s = RingSlot(l.count, i, l.order);
      if (s >= 0) item[s] = i;
    }
  } else if (l.shape == kShapeRingPage) {
    for (int i = 0; i < kPageItems && i < l.count; ++i) {
      if (keys[i] == kKeyEmpty) continue;
      const int s = RingSlot(kPageItems, i, l.order);
      if (s >= 0) item[s] = i;
    }
  }
}

// THE SCROLL (spell_book_look=1). The picture of the book has room round the
// layout for a roll of the paper at the top and at the bottom and for the
// knobs of their rods at the sides. The layout keeps its middle at the middle
// of the picture, so each item keeps its place on the book, and the pick and
// the orbs do not change. In pixels of the picture:
const int kScrollPadX = 44;      // at the left and at the right of the layout
const int kScrollPadY = 54;      // at the top and at the bottom
const int kScrollPaperIn = 12;   // the paper is this much inside the layout
const int kScrollRollTop = 28;   // the middle of the top roll from the top
const int kScrollRollBottom = 30;  // the middle of the bottom roll from the
                                   // bottom
// A socket: the rim and the ring of ink round the hole of the orb go this far
// out (card_scroll::kSocketOut), and the name starts under them.
const int kSocketOutPx = 13;

// The size of the picture of a layout in pixels, with the room of the scroll
// when scroll is true.
inline int PictureWidth(const Layout& l, bool scroll);
inline int PictureHeight(const Layout& l, bool scroll);

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

inline int PictureWidth(const Layout& l, bool scroll) {
  const int p = PixelsOf(l.width_m) + (scroll ? 2 * kScrollPadX : 0);
  return p > kTexture ? kTexture : p;
}

inline int PictureHeight(const Layout& l, bool scroll) {
  const int p = PixelsOf(l.height_m) + (scroll ? 2 * kScrollPadY : 0);
  return p > kTexture ? kTexture : p;
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
