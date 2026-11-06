// The sea with depth (sea_depth_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/sea_depth_math.h"

namespace sea_depth_checks {

inline void SeaDepthChecks() {
  using namespace sea_depth;
  printf("\nThe sea with depth\n");

  // With the floor off, each byte of the sea is at the height 0, as the game
  // draws it.
  for (int b = 0; b < kSeaBytes; ++b) {
    Near("the floor off keeps the sea at the height 0", FloorHeight(b, false),
         0, 0);
  }
  // With the floor on, the land goes on down by the game's step of a byte.
  Near("the byte 3 is one step under the water", FloorHeight(3, true), -0.67,
       1e-5);
  Near("the byte 2 is two steps under the water", FloorHeight(2, true), -1.34,
       1e-5);
  Near("the byte 0 is four steps under the water", FloorHeight(0, true),
       -2.68, 1e-5);
  Near("a byte of the land keeps its own height", FloorHeight(4, true), 0, 0);
  Near("a byte under 0 keeps the height 0", FloorHeight(-1, true), 0, 0);
  for (int b = 1; b < kSeaBytes; ++b) {
    Check(FloorHeight(b, true) > FloorHeight(b - 1, true),
          "a lower byte is deeper", b, 0);
    Check(FloorHeight(b, true) < 0.0f, "each byte of the sea is under the water",
          b, 0);
  }

  // The alpha: the game's cut, and the fade of sea_clear.
  Near("the game: the byte 0 is clear", FloorAlpha(0, false), 0, 0);
  Near("the game: the byte 1 is clear", FloorAlpha(1, false), 0, 0);
  Near("the game: the byte 2 is opaque", FloorAlpha(2, false), 255, 0);
  Near("the game: the byte 3 is opaque", FloorAlpha(3, false), 255, 0);
  Near("clear: the byte 0 is clear", FloorAlpha(0, true), 0, 0);
  Near("clear: the byte 1 shows a third", FloorAlpha(1, true), 85, 0);
  Near("clear: the byte 2 shows two thirds", FloorAlpha(2, true), 170, 0);
  Near("clear: the byte 3 is opaque", FloorAlpha(3, true), 255, 0);
  Near("a byte of the land is opaque", FloorAlpha(4, true), 255, 0);

  // The tables of the stubs.
  {
    float heights[kSeaBytes];
    uint8_t alphas[kSeaBytes];
    FillTables(false, false, heights, alphas);
    bool game = true;
    for (int b = 0; b < kSeaBytes; ++b) {
      game = game && heights[b] == 0.0f &&
             alphas[b] == (b < 2 ? 0 : 255);
    }
    Check(game, "both keys at 0 give the game's tables", 0, 0);
    FillTables(true, true, heights, alphas);
    Near("the table has the floor", heights[1], FloorHeight(1, true), 0);
    Near("the table has the fade", alphas[2], 170, 0);
  }

  // The water line on the edge from a vertex of the byte 4 (2.68 units over
  // the water) to a vertex of the byte 3. On the game's flat sea the land
  // meets the water at the vertex of the byte 3. With the floor it crosses
  // the water 80 percent along the edge, 8 units of the 10 of a cell.
  Near("the game: the land meets the water at the vertex of the sea",
       WaterLine(4 * kByteStep, FloorHeight(3, false)), 1, 1e-6);
  Near("the floor: the land crosses the water at 80 percent",
       WaterLine(4 * kByteStep, FloorHeight(3, true)), 0.8, 1e-5);
  Near("an edge under the water does not cross it",
       WaterLine(FloorHeight(3, true), FloorHeight(0, true)), -1, 0);
  Near("an edge over the water does not cross it", WaterLine(2.68f, 5.0f), -1,
       0);
}

}  // namespace sea_depth_checks
