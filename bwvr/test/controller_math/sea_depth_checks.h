// The sea with depth (sea_depth_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/sea_depth_math.h"

namespace sea_depth_checks {

inline void SeaDepthChecks() {
  using namespace sea_depth;
  printf("\nThe sea with depth\n");

  // With the floor off, each byte of the sea is at the height 0 and has the
  // alpha of the game's cut.
  for (int b = 0; b < kSeaBytes; ++b) {
    Near("the floor off keeps the sea at the height 0", FloorHeight(b, false),
         0, 0);
  }
  Check(FloorAlpha(0, false) == 0 && FloorAlpha(1, false) == 0,
        "the floor off: the bytes 0 and 1 are clear, as in the game", 0, 0);
  Check(FloorAlpha(2, false) == 255 && FloorAlpha(3, false) == 255,
        "the floor off: the bytes 2 and 3 are opaque, as in the game", 0, 0);

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

  // The alpha of the floor falls with its depth, and ends at the game's cut.
  Near("the byte 3 shows two thirds of the floor", FloorAlpha(3, true), 170,
       0);
  Near("the byte 2 shows one third of the floor", FloorAlpha(2, true), 85, 0);
  Check(FloorAlpha(1, true) == 0 && FloorAlpha(0, true) == 0,
        "the bytes 1 and 0 are clear, as in the game", 0, 0);
  Near("a byte of the land is opaque", FloorAlpha(4, true), 255, 0);
  for (int b = 1; b < kSeaBytes; ++b) {
    Check(FloorAlpha(b, true) >= FloorAlpha(b - 1, true),
          "a deeper byte shows less of the floor", b, 0);
    Check(FloorAlpha(b, true) <= FloorAlpha(b, false),
          "the floor never hides more of the sea than the game", b, 0);
  }

  // The tables of the stubs.
  {
    float heights[kSeaBytes];
    uint8_t alphas[kSeaBytes];
    FillTables(false, heights, alphas);
    bool game = true;
    for (int b = 0; b < kSeaBytes; ++b) {
      game = game && heights[b] == 0.0f && alphas[b] == FloorAlpha(b, false);
    }
    Check(game, "sea_depth=0 gives the game's tables", 0, 0);
    FillTables(true, heights, alphas);
    Near("the table has the floor", heights[1], FloorHeight(1, true), 0);
    Near("the table has the fade", alphas[3], 170, 0);
  }

  // The water line on the edge from a vertex of the byte 4 (2.68 units over
  // the water) to a vertex of the byte 3. On the game's flat sea the land
  // meets the water at the vertex of the byte 3. With the floor it crosses
  // the water 80 percent along the edge, 8 units of the 10 of a cell.
  Near("the game: the land meets the water at the vertex of the sea",
       WaterLine(4 * kByteStep, FloorHeight(3, false)), 1, 1e-6);
  const float line = WaterLine(4 * kByteStep, FloorHeight(3, true));
  Near("the floor: the land crosses the water at 80 percent", line, 0.8, 1e-5);
  Near("an edge under the water does not cross it",
       WaterLine(FloorHeight(3, true), FloorHeight(0, true)), -1, 0);
  Near("an edge over the water does not cross it", WaterLine(2.68f, 5.0f), -1,
       0);
  // The fade has no step at the water line: the land draws over the sea, and
  // the alpha there is between the alphas of the two vertices.
  Near("the alpha at the water line", EdgeAlpha(255, FloorAlpha(3, true), line),
       187, 1e-3);
}

}  // namespace sea_depth_checks
