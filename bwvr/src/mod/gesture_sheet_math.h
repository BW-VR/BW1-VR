// The sheet of the gestures over the palm: the pure math. See
// docs/miracles.md, "The sheet of the gestures". gesture_sheet.cpp uses it,
// and test/controller_math checks it (gesture_sheet_checks.h). Nothing here
// reads a global or touches the game.
//
// THE SHEET is one picture over the palm of the hand that does not point, at
// the place of the spell book. It has a row for each miracle that the
// Worship Sites of the player offer, and a row for the gesture "again". A row
// has the pictures of the gestures of the chain of the miracle (the start of
// the chain small, then the gesture of the miracle) and its name. More than
// kColumnRows rows go into two columns.
//
// THE PICTURES OF THE GAME. The game draws the picture of a gesture from two
// textures (data\textures\S_Gesture0.raw and S_Gesture1.raw, each with an
// alpha file ending in "a"): 256 by 256 pixels, a grid of 4 by 4 cells of 64
// pixels. fn_0057D5D0 takes the gesture type: texture type / 16, cell type %
// 16, the cells in rows from the top left [V].

#pragma once

namespace gesture_sheet_math {

// The textures of the gesture pictures [V].
const int kAtlasSide = 256;
const int kCellSide = 64;
const int kCellsAcross = 4;
const int kCellsInAtlas = 16;
const int kAtlases = 2;

// The texture and the top left pixel of the cell of a gesture type. False for
// a type that has no cell.
inline bool CellOf(int gesture, int* atlas, int* x, int* y) {
  if (gesture <= 0 || gesture >= kAtlases * kCellsInAtlas) return false;
  *atlas = gesture / kCellsInAtlas;
  const int cell = gesture % kCellsInAtlas;
  *x = (cell % kCellsAcross) * kCellSide;
  *y = (cell / kCellsAcross) * kCellSide;
  return true;
}

// THE LAYOUT in meters on the sheet.
const float kTitleM = 0.030f;     // the band of the title at the top
const float kRowM = 0.030f;       // one row
const float kStartM = 0.018f;     // the picture of the start of the chain
const float kGestureM = 0.026f;   // the picture of each other gesture
const float kGapM = 0.004f;
const float kNameM = 0.080f;      // the box of the name
const float kMarginM = 0.010f;    // round the page
const int kMostGestures = 3;      // a chain has at most 3 gestures
const int kColumnRows = 7;        // more rows go into two columns
const int kMostRows = 2 * kColumnRows;
// The width of an empty sheet: the title and the reason.
const float kEmptyWidthM = 0.20f;
const float kEmptyHeightM = 0.060f;
// The picture: 32 pixels for each cm, less when the sheet is wider than the
// texture.
const float kPixelsPerMeter = 3200.0f;
const int kTexture = 1024;

struct Layout {
  int rows;      // the count of the rows
  int columns;   // 1 or 2
  int per_column;
  int after;       // the most gestures after the start of a chain, 1 or 2
  float column_m;  // the width of one column
  float width_m;
  float height_m;
  float pixels_per_meter;
};

// The width of a column: the start, the gestures after it and the name.
// after is the most gestures after the start in a row of the sheet, so the
// names follow the pictures with no empty space.
inline float ColumnWidth(int after = 2) {
  if (after < 1) after = 1;
  if (after > kMostGestures - 1) after = kMostGestures - 1;
  return kStartM + kGapM + after * (kGestureM + kGapM) + kGapM + kNameM;
}

inline Layout LayoutFor(int rows, int after = 2) {
  Layout l;
  if (rows < 0) rows = 0;
  if (rows > kMostRows) rows = kMostRows;
  l.rows = rows;
  l.columns = rows > kColumnRows ? 2 : 1;
  l.per_column = rows == 0 ? 0 : (rows + l.columns - 1) / l.columns;
  if (after < 1) after = 1;
  if (after > kMostGestures - 1) after = kMostGestures - 1;
  l.after = after;
  l.column_m = ColumnWidth(after);
  if (rows == 0) {
    l.width_m = kEmptyWidthM;
    l.height_m = kEmptyHeightM;
  } else {
    l.width_m = 2.0f * kMarginM + l.columns * l.column_m +
                (l.columns - 1) * kMarginM;
    l.height_m = kTitleM + l.per_column * kRowM + kMarginM;
  }
  l.pixels_per_meter = kPixelsPerMeter;
  const float most = static_cast<float>(kTexture);
  if (l.width_m * l.pixels_per_meter > most) {
    l.pixels_per_meter = most / l.width_m;
  }
  if (l.height_m * l.pixels_per_meter > most) {
    l.pixels_per_meter = most / l.height_m;
  }
  return l;
}

// The size of the picture in pixels, even, so the middle of the part that
// shows is the middle of a pixel border.
inline int PixelsOf(float meters, float pixels_per_meter) {
  int p = static_cast<int>(meters * pixels_per_meter + 0.5f);
  p += p & 1;
  if (p > kTexture) p = kTexture;
  if (p < 2) p = 2;
  return p;
}

// The left side and the middle height of row r, in meters from the top left
// of the sheet (y down).
inline void RowAt(const Layout& l, int r, float* left, float* middle) {
  const int column = l.per_column > 0 ? r / l.per_column : 0;
  const int in_column = l.per_column > 0 ? r % l.per_column : 0;
  *left = kMarginM + column * (l.column_m + kMarginM);
  *middle = kTitleM + (in_column + 0.5f) * kRowM;
}

// The box of picture k of a row (0 the start of the chain, 1 and 2 the
// gestures after it): its middle from the left side of the row, and its side,
// in meters.
inline void PictureAt(int k, float* middle_x, float* side) {
  if (k <= 0) {
    *side = kStartM;
    *middle_x = 0.5f * kStartM;
    return;
  }
  *side = kGestureM;
  *middle_x = kStartM + kGapM + (k - 1) * (kGestureM + kGapM) +
              0.5f * kGestureM;
}

// The box of the name of a row: its left side from the left side of the row,
// and its width.
inline void NameAt(const Layout& l, float* left, float* width) {
  *left = kStartM + kGapM + l.after * (kGestureM + kGapM) + kGapM;
  *width = kNameM;
}

}  // namespace gesture_sheet_math
