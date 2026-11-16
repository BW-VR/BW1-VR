// THE SEA WITH DEPTH (sea_depth.cpp, and docs/backend.md, "The sea with
// depth"). Pure math: no Windows, no game. The checks are in
// test/controller_math/sea_depth_checks.h.
//
// The land draw (fn_00874AA0) gives each vertex the height of its altitude
// byte times 0.67 units. A byte of 3 or less is sea, and the game draws it
// at the height 0, in the plane of the sea. In a block with water
// (block +0x928) a vertex with a byte of 0 or 1 gets an alpha of 0, so the
// sea shows through it, and a vertex with a byte of 2 or 3 stays opaque.
// The sea draws before the land, so this alpha is the fade of the coast.
//
// sea_depth=1, in a block with water only:
//  - A vertex of the sea goes under the plane of the sea, by the game's own
//    step of 0.67 units for each byte under 4.
//  - Its alpha falls with its depth, so the floor fades into the sea that
//    draws behind it. The fade ends at the byte 1, where the cut of the game
//    is, so the edges of the land in the open sea stay clear.

#pragma once

#include <stdint.h>

namespace sea_depth {

// The bytes of the sea: 0 to 3.
const int kSeaBytes = 4;

// The game's height of one altitude byte (the float at 0x00C3720C).
const float kByteStep = 0.67f;

// The byte where the alpha of the floor gets to 0: the cut of the game.
const int kClearByte = 1;

// The height of a vertex of the sea with this byte, with the floor on or
// off. Off: 0, as the game draws it.
inline float FloorHeight(int byte, bool floor) {
  if (!floor || byte < 0 || byte >= kSeaBytes) return 0.0f;
  return -kByteStep * static_cast<float>(kSeaBytes - byte);
}

// The alpha of a vertex of the sea with this byte in a block with water.
// Off: the cut of the game (0 for the bytes 0 and 1, else 255). On: 255
// times one minus the depth over the depth of kClearByte, so 170 and 85 for
// the bytes 3 and 2, and 0 for the bytes 1 and 0.
inline uint8_t FloorAlpha(int byte, bool floor) {
  if (byte >= kSeaBytes) return 255;
  if (!floor) return byte < 2 ? 0 : 255;
  if (byte <= kClearByte) return 0;
  const float part = FloorHeight(byte, true) / FloorHeight(kClearByte, true);
  const float alpha = 255.0f * (1.0f - part);
  return static_cast<uint8_t>(alpha + 0.5f);
}

// The tables that the stubs of the land draw read: one height and one alpha
// for each byte of the sea.
inline void FillTables(bool floor, float heights[kSeaBytes],
                       uint8_t alphas[kSeaBytes]) {
  for (int b = 0; b < kSeaBytes; ++b) {
    heights[b] = FloorHeight(b, floor);
    alphas[b] = FloorAlpha(b, floor);
  }
}

// Where the plane of the sea crosses the edge of a land triangle from a
// vertex at height a to a vertex at height b, as a part of the edge from a.
// -1 when the edge does not cross it.
inline float WaterLine(float a, float b) {
  if ((a > 0.0f) == (b > 0.0f) || a == b) return -1.0f;
  return a / (a - b);
}

// The alpha along an edge, at the part t from the vertex with alpha a to the
// vertex with alpha b, as the device gives it between two vertices.
inline float EdgeAlpha(float a, float b, float t) { return a + (b - a) * t; }

}  // namespace sea_depth
