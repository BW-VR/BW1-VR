// The math of the anti-aliasing of the Vulkan renderer (vk_scene.cpp,
// docs/backend.md, "Anti-aliasing"). Pure functions. The checks are in
// test/controller_math/vk_msaa_checks.h.

#pragma once

#include <stdint.h>

namespace vk_msaa_math {

// The sample count of a value of the key vk_msaa: 1, 2, 4 or 8. A value
// between two counts takes the lower count.
inline uint32_t CountOfKey(int value) {
  if (value >= 8) return 8;
  if (value >= 4) return 4;
  if (value >= 2) return 2;
  return 1;
}

// The largest count at or below `asked` that `flags` has. The bits of
// `flags` are the bits of VkSampleCountFlags: bit value 2 is 2 samples, bit
// value 4 is 4 samples. One sample is always possible.
inline uint32_t Supported(uint32_t asked, uint32_t flags) {
  for (uint32_t count = CountOfKey(static_cast<int>(asked)); count > 1;
       count >>= 1) {
    if ((flags & count) != 0) return count;
  }
  return 1;
}

// The step of a count on the debug page: 0 off, 1 2x, 2 4x, 3 8x.
inline int StepOf(uint32_t count) {
  return count >= 8 ? 3 : count >= 4 ? 2 : count >= 2 ? 1 : 0;
}

// The next count of the row of the debug page, after 8 again 1.
inline uint32_t NextCount(uint32_t count) {
  return count >= 8 ? 1u : CountOfKey(static_cast<int>(count) * 2);
}

// The base 2 logarithm of a count, for the key of a pipeline.
inline uint32_t LogOf(uint32_t count) {
  return count >= 8 ? 3u : count >= 4 ? 2u : count >= 2 ? 1u : 0u;
}

// Below this change of the alpha across a pixel the alpha has no edge there,
// and the plain alpha test decides. The same value is in vk_scene.frag.
const float kFlatAlpha = 0.001f;

// The part of a pixel that an alpha test with a "greater" compare covers.
// The alpha is sharp around the reference: the edge is where the alpha is
// the reference, the same place as for the plain test, and it is one pixel
// wide. So a far tree with a low alpha in its small mipmaps keeps the area
// that the plain test gives it. `width` is the change of the alpha across
// the pixel (fwidth). `passes` is the result of the plain test.
inline float CoverageOf(float alpha, float reference, float width,
                        bool passes) {
  if (width < kFlatAlpha) return passes ? 1.0f : 0.0f;
  const float c = (alpha - reference) / width + 0.5f;
  return c < 0.0f ? 0.0f : c > 1.0f ? 1.0f : c;
}

// The sample mask of a coverage: the nearest count of samples, from sample
// 0 up. 0 means that the pixel has no sample.
inline uint32_t SampleMask(float coverage, uint32_t samples) {
  const float scaled = coverage * static_cast<float>(samples) + 0.5f;
  uint32_t n = scaled <= 0.0f ? 0u : static_cast<uint32_t>(scaled);
  if (n > samples) n = samples;
  return n >= 32u ? 0xFFFFFFFFu : (1u << n) - 1u;
}

}  // namespace vk_msaa_math
