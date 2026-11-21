// The anti-aliasing of the Vulkan renderer, a test (vk_msaa_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/vk_msaa_math.h"

namespace vk_msaa_checks {

inline void Is(bool ok, const char* what) { Check(ok, what, ok ? 1 : 0, 1); }

inline bool Close(float a, float b, float tolerance) {
  return fabsf(a - b) < tolerance;
}

inline void VkMsaaChecks() {
  using namespace vk_msaa_math;
  printf("\nThe anti-aliasing of the Vulkan renderer\n");

  Is(CountOfKey(0) == 1 && CountOfKey(1) == 1 && CountOfKey(-3) == 1,
        "a key of 1 or less is one sample");
  Is(CountOfKey(2) == 2 && CountOfKey(3) == 2,
        "a key of 2 or 3 is 2 samples");
  Is(CountOfKey(4) == 4 && CountOfKey(7) == 4,
        "a key of 4 to 7 is 4 samples");
  Is(CountOfKey(8) == 8 && CountOfKey(64) == 8,
        "a key of 8 or more is 8 samples");

  Is(Supported(8, 1 | 2 | 4 | 8) == 8, "8 samples when the device has 8");
  Is(Supported(8, 1 | 2 | 4) == 4, "8 asked, the device has 4: 4 samples");
  Is(Supported(4, 1 | 2 | 8) == 2, "a count above the asked one is not used");
  Is(Supported(4, 1) == 1, "a device with one sample gives one sample");
  Is(Supported(1, 1 | 2 | 4 | 8) == 1, "one sample asked is one sample");
  Is(Supported(2, 0) == 1, "no flags give one sample");

  Is(StepOf(1) == 0 && StepOf(2) == 1 && StepOf(4) == 2 && StepOf(8) == 3,
        "the step of each count on the debug page");
  Is(NextCount(1) == 2 && NextCount(2) == 4 && NextCount(4) == 8 &&
            NextCount(8) == 1,
        "the row of the debug page goes 1, 2, 4, 8 and back to 1");
  Is(LogOf(1) == 0 && LogOf(2) == 1 && LogOf(4) == 2 && LogOf(8) == 3,
        "the logarithm of each count for the key of a pipeline");

  // The edge of the alpha test stays at the reference.
  Is(Close(CoverageOf(0.5f, 0.5f, 0.2f, true), 0.5f, 1e-6f),
        "the alpha at the reference covers half of the pixel");
  Is(CoverageOf(0.9f, 0.5f, 0.2f, true) == 1.0f,
        "an alpha far above the reference covers the whole pixel");
  Is(CoverageOf(0.1f, 0.5f, 0.2f, false) == 0.0f,
        "an alpha far below the reference covers nothing");
  Is(Close(CoverageOf(0.55f, 0.5f, 0.2f, true), 0.75f, 1e-5f),
        "the coverage is linear across the width of the edge");

  // A flat alpha has no edge: the plain test decides, also at the reference.
  Is(CoverageOf(0.5f, 0.5f, 0.0f, true) == 1.0f,
        "a flat alpha that passes the plain test covers the whole pixel");
  Is(CoverageOf(0.5f, 0.5f, 0.0f, false) == 0.0f,
        "a flat alpha that fails the plain test covers nothing");
  Is(CoverageOf(1.0f, 0.0f, kFlatAlpha * 0.5f, true) == 1.0f,
        "an opaque texel with no change of the alpha keeps the whole pixel");

  // A symmetric ramp across the reference keeps the area of the plain test:
  // the mean coverage over the ramp is one half.
  {
    const int kSteps = 1000;
    double sum = 0.0;
    for (int i = 0; i < kSteps; ++i) {
      const float a = 0.3f + 0.4f * (static_cast<float>(i) + 0.5f) / kSteps;
      sum += CoverageOf(a, 0.5f, 0.4f, a > 0.5f);
    }
    Is(Close(static_cast<float>(sum / kSteps), 0.5f, 1e-3f),
          "a ramp across the reference keeps the area of the plain test");
  }

  Is(SampleMask(0.0f, 4) == 0u, "no coverage is no sample");
  Is(SampleMask(1.0f, 4) == 0xFu, "full coverage is each of 4 samples");
  Is(SampleMask(1.0f, 8) == 0xFFu, "full coverage is each of 8 samples");
  Is(SampleMask(0.5f, 4) == 0x3u, "half coverage is 2 of 4 samples");
  Is(SampleMask(0.25f, 4) == 0x1u, "a quarter is 1 of 4 samples");
  Is(SampleMask(0.1f, 4) == 0u, "a coverage below half a sample is none");
  Is(SampleMask(2.0f, 2) == 0x3u, "a coverage above 1 stays at all samples");
}

}  // namespace vk_msaa_checks
