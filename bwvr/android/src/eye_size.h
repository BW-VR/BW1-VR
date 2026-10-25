// THE SIZE OF THE EYE PICTURES (eye_size_max, docs/android.md, "The size of
// the eye pictures"). No Android code.
//
// The runtime gives the size of an eye that it asks for
// (recommendedImageRectWidth and recommendedImageRectHeight). The app writes
// that size as headset_width and headset_height, the size of the render of
// the game comes from it (resolution_pct, ScaleToPixels of settings_table.h),
// and the swapchains of the projection layer, the panel image and the screen
// quad have the size of the render. A Quest 3 can ask for much more than its
// panel has (4096 by 4291 for each eye), so the cap keeps all of them at the
// size of the panel.
#pragma once

#include <stdint.h>

namespace eye_size {

// The panel of a Quest 3, for each eye.
constexpr uint32_t kPanelWidth = 2064;
constexpr uint32_t kPanelHeight = 2208;

struct Size {
  uint32_t width = 0;
  uint32_t height = 0;
  bool capped = false;  // the size is smaller than the size of the runtime
};

// The size that the app takes from the size of the runtime, width by height.
// With a cap (max_width and max_height above 0): the largest size with the
// aspect of the runtime that fits in max_width by max_height, each side
// rounded to the nearest pixel. A size of the runtime that fits stays as it
// is, and the cap never makes a size larger. With no cap (a max of 0): the
// size of the runtime.
inline Size Cap(uint32_t width, uint32_t height, uint32_t max_width, uint32_t max_height) {
  Size s;
  s.width = width;
  s.height = height;
  if (max_width == 0 || max_height == 0 || width == 0 || height == 0) return s;
  if (width <= max_width && height <= max_height) return s;
  const uint64_t w = width, h = height;
  // The side that reaches its max first sets the scale: the width when
  // max_width / width <= max_height / height.
  if (static_cast<uint64_t>(max_width) * h <= static_cast<uint64_t>(max_height) * w) {
    s.width = max_width;
    s.height = static_cast<uint32_t>((h * max_width + w / 2) / w);
  } else {
    s.height = max_height;
    s.width = static_cast<uint32_t>((w * max_height + h / 2) / h);
  }
  if (s.width > max_width) s.width = max_width;
  if (s.height > max_height) s.height = max_height;
  if (s.width == 0) s.width = 1;
  if (s.height == 0) s.height = 1;
  s.capped = true;
  return s;
}

// eye_size_max of bwvr.ini: 1 (the default) caps the size at the panel of a
// Quest 3, 0 takes the size of the runtime with no cap.
inline Size FromKey(int eye_size_max, uint32_t width, uint32_t height) {
  if (eye_size_max == 0) return Cap(width, height, 0, 0);
  return Cap(width, height, kPanelWidth, kPanelHeight);
}

}  // namespace eye_size
