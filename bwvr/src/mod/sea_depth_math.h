// THE SEA WITH DEPTH (sea_depth.cpp, and docs/backend.md, "The sea with
// depth"). Pure math: no Windows, no game. The checks are in
// test/controller_math/sea_depth_checks.h.
//
// The land draw (fn_00874AA0) gives each vertex the height of its altitude
// byte times 0.67 units. A byte of 3 or less is sea, and the game draws it
// at the height 0, in the plane of the sea. In a block with water
// (block +0x928) a vertex with a byte of 0 or 1 gets an alpha of 0, so the
// sea shows through it, and a vertex with a byte of 2 or 3 stays opaque.
//
// sea_depth=1: a vertex of the sea goes under the plane of the sea, by the
// game's own step of 0.67 units for each byte under 4. So the land goes on
// down past the water line, and the sea, which then draws after the land
// with the depth test, cuts it at the real water line.

#pragma once

#include <stdint.h>

namespace sea_depth {

// The bytes of the sea: 0 to 3.
const int kSeaBytes = 4;

// The game's height of one altitude byte (the float at 0x00C3720C).
const float kByteStep = 0.67f;

// The height of a vertex of the sea with this byte, with the floor on or
// off. Off: 0, as the game draws it.
inline float FloorHeight(int byte, bool floor) {
  if (!floor || byte < 0 || byte >= kSeaBytes) return 0.0f;
  return -kByteStep * static_cast<float>(kSeaBytes - byte);
}

// The table that the stub of the land draw reads: one height for each byte
// of the sea.
inline void FillHeights(bool floor, float heights[kSeaBytes]) {
  for (int b = 0; b < kSeaBytes; ++b) heights[b] = FloorHeight(b, floor);
}

// Where the plane of the sea crosses the edge of a land triangle from a
// vertex at height a to a vertex at height b, as a part of the edge from a.
// -1 when the edge does not cross it.
inline float WaterLine(float a, float b) {
  if ((a > 0.0f) == (b > 0.0f) || a == b) return -1.0f;
  return a / (a - b);
}

}  // namespace sea_depth
