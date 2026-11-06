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

  // The table of the stub.
  {
    float heights[kSeaBytes];
    FillHeights(false, heights);
    bool game = true;
    for (int b = 0; b < kSeaBytes; ++b) game = game && heights[b] == 0.0f;
    Check(game, "sea_depth=0 gives the game's table", 0, 0);
    FillHeights(true, heights);
    Near("the table has the floor", heights[1], FloorHeight(1, true), 0);
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
