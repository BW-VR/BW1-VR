// THE SHEET OF THE GESTURES (docs/miracles.md, "The sheet of the gestures").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/gesture_sheet_math.h"

namespace gesture_sheet_checks {

inline void GestureSheetChecks() {
  using namespace gesture_sheet_math;
  printf("\nThe sheet of the gestures\n");

  // THE CELLS OF THE GAME: the gesture type is the cell, 16 to a texture, in
  // rows of 4 from the top left (fn_0057D5D0).
  {
    int atlas = -1, x = -1, y = -1;
    Check(CellOf(1, &atlas, &x, &y) && atlas == 0 && x == 64 && y == 0,
          "the spiral (type 1) is the second cell of the first texture", x, 64);
    Check(CellOf(14, &atlas, &x, &y) && atlas == 0 && x == 128 && y == 192,
          "the R shape (type 14) is in the last row of the first texture", y,
          192);
    Check(CellOf(16, &atlas, &x, &y) && atlas == 1 && x == 0 && y == 0,
          "the Cyrillic L (type 16) is the first cell of the second texture",
          atlas, 1);
    Check(CellOf(21, &atlas, &x, &y) && atlas == 1 && x == 64 && y == 64,
          "the house (type 21) is in the second row of the second texture", x,
          64);
    Check(!CellOf(0, &atlas, &x, &y) && !CellOf(32, &atlas, &x, &y),
          "no gesture and a type past the textures have no cell", 0, 0);
  }

  // THE LAYOUT. Each count of rows fits the texture, a row has room for its
  // pictures and its name, and more than 7 rows go into two columns.
  {
    bool fits = true, inside = true;
    for (int n = 0; n <= kMostRows; ++n) {
      const Layout l = LayoutFor(n);
      if (PixelsOf(l.width_m, l.pixels_per_meter) > kTexture ||
          PixelsOf(l.height_m, l.pixels_per_meter) > kTexture ||
          l.width_m * l.pixels_per_meter > kTexture + 0.5f ||
          l.height_m * l.pixels_per_meter > kTexture + 0.5f) {
        fits = false;
      }
      for (int r = 0; r < n; ++r) {
        float left = 0.0f, middle = 0.0f;
        RowAt(l, r, &left, &middle);
        if (left + l.column_m > l.width_m - kMarginM + 1e-5f ||
            middle + 0.5f * kRowM > l.height_m - kMarginM + 1e-5f ||
            middle - 0.5f * kRowM < kTitleM - 1e-5f) {
          inside = false;
        }
      }
    }
    Check(fits, "the sheet fits the texture of 1024 pixels at each count",
          fits, 1);
    Check(inside, "each row is inside the sheet, under the title", inside, 1);
    const Layout seven = LayoutFor(7);
    const Layout eight = LayoutFor(8);
    Check(seven.columns == 1 && eight.columns == 2 && eight.per_column == 4,
          "7 rows are one column, 8 rows are two columns of 4",
          eight.columns * 10 + eight.per_column, 24);
    float start = 0.0f, side = 0.0f, last = 0.0f, last_side = 0.0f;
    PictureAt(0, &start, &side);
    PictureAt(2, &last, &last_side);
    float name_left = 0.0f, name_w = 0.0f;
    const Layout two = LayoutFor(3, 2);
    NameAt(two, &name_left, &name_w);
    Check(side < last_side && last + 0.5f * last_side < name_left,
          "the start of the chain is smaller, and the name is after the "
          "pictures", side < last_side, 1);
    Near("a column holds the start, two gestures and the name, m",
         name_left + name_w, two.column_m, 1e-6);
    const Layout one = LayoutFor(3, 1);
    float one_left = 0.0f, one_w = 0.0f, mid1 = 0.0f, side1 = 0.0f;
    NameAt(one, &one_left, &one_w);
    PictureAt(1, &mid1, &side1);
    Check(one_left > mid1 + 0.5f * side1 && one_left < name_left &&
              one.width_m < two.width_m,
          "with one gesture after the start, the name follows that gesture",
          one_left < name_left, 1);
    const Layout seven_rows = LayoutFor(7);
    Near("a sheet of 6 miracles and again is 25 cm high, cm",
         seven_rows.height_m * 100.0, 25.0, 1e-3);
  }
}

}  // namespace gesture_sheet_checks
