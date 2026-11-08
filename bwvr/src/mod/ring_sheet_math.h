// The sheets of the ring: where each tile of the sheet of the places and of
// the sheet of the leashes is on the picture, what it does, and its box on
// the plane of the ring. See docs/controllers.md, "The ring". No Windows and
// no game here, so the math test compiles this file (ring_checks.h).
//
// THE PICTURE is the picture of the ring: 1024 by 1024 pixels, 32 pixels to
// 1 cm at 100 percent. The middle of a sheet is the middle of the picture,
// and it has no tile: the hand starts there, and a release there chooses
// nothing. The card in the middle says what the tile under the hand does.
//
// THE PLACES (from the top):
//
//   the places     a row of seals: Back at the left, apart, then Creature,
//                  Town, Temple, Inside, Land
//   the card       in the middle
//   the bookmarks  a seal with the number of each bookmark that is set, then
//                  a seal with a plus that adds one
//   the bins       a small seal with a bin under each bookmark: it removes it
//
// THE LEASHES: three tiles in a row above the middle, Compassion, Learning,
// Aggression (from the good to the evil), each with a picture of its leash.

#pragma once

#include "radial_menu_math.h"

namespace ring_sheet_math {

const int kSize = 1024;
const float kMiddle = 512.0f;

enum TileKind {
  kTilePlace,   // arg: the ring item of the place (radial_math::Item)
  kTileMark,    // arg: the bookmark, 0 to 6: a press goes there
  kTileAdd,     // a press adds a bookmark where the player points
  kTileRemove,  // arg: the bookmark, 0 to 6: a press removes it
  kTileLeash,   // arg: the kind of the leash, 1 to 3
};

struct Tile {
  int kind;
  int arg;
  int cx, cy;      // the middle of its seal, in pixels
  int size;        // the seal is this wide and high
  int box[4];      // where the hand picks it: x0 y0 x1 y1 in pixels
};

struct Layout {
  int sheet;       // radial_math::SheetId
  int count;
  Tile tile[radial_math::kMaxTiles];
  int card_y;      // the middle of the card, in pixels
};

// --- the places ------------------------------------------------------------------

const int kPlaces = 6;
const int kPlaceItems[kPlaces] = {
    radial_math::kItemCreature, radial_math::kItemTown,
    radial_math::kItemTemple,   radial_math::kItemEnter,
    radial_math::kItemLand,     radial_math::kItemMarkBack};
const int kPlaceY = 240;
const int kPlaceSize = 100;
const int kPlacePitch = 150;
// The places go round this middle, and Back is at kBackX, with a gap of
// kBackGap px between its box and the box of the first place.
const int kPlacesX = 580;
const int kBackX = 90;
const int kPlaceBoxUp = 66;     // the box goes this far up from the middle
const int kPlaceBoxDown = 96;   // and this far down: the word under the seal
const int kMarkY = 690;
const int kMarkSize = 92;
const int kMarkPitch = 124;
const int kMarkBoxHalf = 56;
const int kBinY = 806;
const int kBinSize = 68;
const int kBinBoxHalf = 40;

inline void AddTile(Layout* out, int kind, int arg, int cx, int cy, int size,
                    int half_w, int up, int down) {
  if (out->count >= radial_math::kMaxTiles) return;
  Tile& t = out->tile[out->count++];
  t.kind = kind;
  t.arg = arg;
  t.cx = cx;
  t.cy = cy;
  t.size = size;
  t.box[0] = cx - half_w;
  t.box[1] = cy - up;
  t.box[2] = cx + half_w;
  t.box[3] = cy + down;
}

// The sheet of the places for the bookmarks that are set now. The seal with
// the plus shows also when all 7 are set: then it is dim.
inline void PlacesLayout(const bool set[radial_math::kMarks], Layout* out) {
  out->sheet = radial_math::kSheetPlaces;
  out->count = 0;
  out->card_y = static_cast<int>(kMiddle);
  // The 5 places, then Back apart at the left: it goes back, it is no place.
  for (int i = 0; i < kPlaces; ++i) {
    const int cx =
        i == kPlaces - 1
            ? kBackX
            : static_cast<int>(kPlacesX + (i - 0.5f * (kPlaces - 2)) *
                                              kPlacePitch);
    AddTile(out, kTilePlace, kPlaceItems[i], cx, kPlaceY, kPlaceSize,
            kPlacePitch / 2, kPlaceBoxUp, kPlaceBoxDown);
  }
  int marks = 0;
  for (int i = 0; i < radial_math::kMarks; ++i) marks += set[i] ? 1 : 0;
  const int seals = marks + 1;
  int n = 0;
  const auto x_of = [&](int k) {
    return static_cast<int>(kMiddle + (k - 0.5f * (seals - 1)) * kMarkPitch);
  };
  for (int i = 0; i < radial_math::kMarks; ++i) {
    if (!set[i]) continue;
    AddTile(out, kTileMark, i, x_of(n), kMarkY, kMarkSize, kMarkPitch / 2,
            kMarkBoxHalf, kMarkBoxHalf);
    AddTile(out, kTileRemove, i, x_of(n), kBinY, kBinSize, kMarkPitch / 2,
            kBinBoxHalf, kBinBoxHalf);
    ++n;
  }
  AddTile(out, kTileAdd, 0, x_of(n), kMarkY, kMarkSize, kMarkPitch / 2,
          kMarkBoxHalf, kMarkBoxHalf);
}

// --- the leashes -----------------------------------------------------------------

const int kLeashKinds[3] = {3, 2, 1};  // Compassion, Learning, Aggression
const int kLeashY = 262;
const int kLeashWidth = 276;
const int kLeashHeight = 300;
const int kLeashPitch = 300;
const int kLeashPicture = 176;  // the picture of the leash in a tile
const int kLeashPictureTop = 26;  // from the top of a tile to its picture

inline void LeashLayout(Layout* out) {
  out->sheet = radial_math::kSheetLeash;
  out->count = 0;
  out->card_y = 640;
  for (int i = 0; i < 3; ++i) {
    const int cx = static_cast<int>(kMiddle + (i - 1) * kLeashPitch);
    AddTile(out, kTileLeash, kLeashKinds[i], cx, kLeashY, kLeashWidth,
            kLeashPitch / 2, kLeashHeight / 2, kLeashHeight / 2);
  }
}

// --- the plane -------------------------------------------------------------------

// The boxes of the tiles on the plane of the sheet: meters to the right and
// up from its middle. meters_per_pixel is 1 / 3200 at 100 percent.
inline void SheetOf(const Layout& layout, float meters_per_pixel,
                    radial_math::Sheet* out) {
  out->count = layout.count;
  for (int i = 0; i < layout.count; ++i) {
    const int* b = layout.tile[i].box;
    out->box[i][0] = (b[0] - kMiddle) * meters_per_pixel;
    out->box[i][1] = (kMiddle - b[3]) * meters_per_pixel;
    out->box[i][2] = (b[2] - kMiddle) * meters_per_pixel;
    out->box[i][3] = (kMiddle - b[1]) * meters_per_pixel;
  }
}

// The middle of the box of tile i on the plane, in meters, for the tests and
// for the log.
inline void TileMiddle(const Layout& layout, int i, float meters_per_pixel,
                       float* x, float* y) {
  const int* b = layout.tile[i].box;
  *x = (0.5f * (b[0] + b[2]) - kMiddle) * meters_per_pixel;
  *y = (kMiddle - 0.5f * (b[1] + b[3])) * meters_per_pixel;
}

// --- what a tile sends -----------------------------------------------------------

// THE KEYS OF A BOOKMARK (docs/controls-audit.md, section 3): the key of the
// number with no modifier goes there (fn_0043A070), and with Ctrl and Shift
// removes it (fn_00439D40). False for a tile that sends no such key.
inline bool MarkKey(const Tile& tile, int* key, int* mods) {
  if (tile.arg < 0 || tile.arg >= radial_math::kMarks) return false;
  if (tile.kind == kTileMark) {
    *key = 0x02 + tile.arg;
    *mods = 0;
    return true;
  }
  if (tile.kind == kTileRemove) {
    *key = 0x02 + tile.arg;
    *mods = radial_math::kModCtrl | radial_math::kModShift;
    return true;
  }
  return false;
}

// The first bookmark that is not set, or -1 when all 7 are set.
inline int FreeMark(const bool set[radial_math::kMarks]) {
  for (int i = 0; i < radial_math::kMarks; ++i) {
    if (!set[i]) return i;
  }
  return -1;
}

}  // namespace ring_sheet_math
