// THE DEPTH TEST EQUAL (vk_equal_depth, docs/backend.md, "The depth test
// EQUAL"). Pure math with no game, so the renderer and the checks use it.
//
// A layer of the game (the snow on a roof, a lit copy over a mesh) draws the
// vertices of the mesh under it with ZFUNC EQUAL: it passes where its depth is
// the depth that the mesh wrote. On the CPU path both draws come from the same
// vertices and have the same bits. A mesh on the GPU path makes its depth in the
// vertex shader, and the layer on the CPU path has the depth of the x87 code of
// the game, so the two can differ by the rounding of a few steps of a float.
// The renderer draws such a layer with LESS_OR_EQUAL and a depth bias toward the
// camera of kEqualBiasUnits steps of the depth image, so a layer passes where
// its depth is the depth under it, or a few steps behind it.
#pragma once

#include <math.h>
#include <stdint.h>

namespace equal_depth_math {

// The bias in units of r, the smallest step of the depth image at the depth of
// the primitive (a float depth image: 2^(exponent of the largest depth - 23)).
// Vulkan moves the depth of the draw by constantFactor * r, and a negative
// factor moves it toward the camera.
const float kEqualBiasUnits = 4.0f;

// D3DCMP_EQUAL.
const uint32_t kZFuncEqual = 3u;
// The value that a record of a draw has for a test EQUAL with the bias. It is no
// value of D3D (1 to 8), so the sort and the merge of the draws keep it apart.
const uint32_t kZFuncEqualBias = 9u;

// The ZFUNC of the record of a draw: kZFuncEqualBias for a test EQUAL that is on,
// when the key is on, else the value of the game.
inline uint32_t RecordZFunc(bool key, uint32_t zenable, uint32_t zfunc) {
  return key && zenable != 0 && zfunc == kZFuncEqual ? kZFuncEqualBias : zfunc;
}

// The step of a float depth image at the depth z (0 to 1).
inline float DepthStep(float z) {
  int exponent = 0;
  frexpf(z > 0.0f ? z : 1e-30f, &exponent);
  // z = m * 2^exponent with m in [0.5, 1): the steps of the mantissa of 23
  // bits are 2^(exponent - 24).
  return ldexpf(1.0f, exponent - 24);
}

// The depth of the layer that the test sees: the depth of the draw moved by the
// bias toward the camera.
inline float BiasedDepth(float layer_z) {
  return layer_z - kEqualBiasUnits * DepthStep(layer_z);
}

// LESS_OR_EQUAL with the bias: does the layer pass over the value in the depth
// image?
inline bool PassesEqual(float layer_z, float image_z) {
  return BiasedDepth(layer_z) <= image_z;
}

// The depth of the game for a point at the distance cz (the camera z), with the
// near plane `near_plane`: z = 1 - near / cz.
inline float GameDepth(float near_plane, float cz) {
  return 1.0f - near_plane * (1.0f / cz);
}

// How far in front of the layer (in the units of the camera z) a surface must be
// to hide the layer, at the distance cz: the bias in the camera z. Equal to the
// step of the depth image there, times the units.
inline double HidingDistance(float near_plane, float cz) {
  const float z = GameDepth(near_plane, cz);
  // dz / dcz = near / cz^2.
  return static_cast<double>(kEqualBiasUnits) * DepthStep(z) * cz * cz / near_plane;
}

}  // namespace equal_depth_math
