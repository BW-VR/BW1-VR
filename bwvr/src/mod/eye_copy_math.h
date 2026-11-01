// THE COPIES OF THE EYE PICTURES IN THE HEADSET APP (docs/android.md, "The
// copies of the eye pictures"). The pure rules, with no Vulkan and no
// OpenXR, so the math test of the PC checks them
// (test/controller_math/eye_copy_checks.h). The renderer of the mod
// (vk_scene.cpp, vk_atlas_area) and the app (picture_ring.cpp, xr_app.cpp,
// main.cpp) use them.
//
//   xr_eye_rects=1     a copy of an eye picture with an atlas
//                      (fovea_res_layer=1) takes only the part that the
//                      layers show (ShownPart). 0: the whole picture
//   vk_atlas_area=1    the renderer writes only that part of the eye
//                      picture when it puts the two images into it. 0: it
//                      clears and writes the whole eye picture
//   xr_eye_direct=1    the play thread copies the eye pictures straight into
//                      two swapchains of the slot of the ring, and the
//                      session copies nothing for them. 0: a copy into the
//                      slot and a copy of the session into the swapchains
//   frame_loop_hold=N  the session shows each picture for at least N frames
//                      of the session (HoldTakes). 0: the newest picture at
//                      each frame
#pragma once

#include <stdint.h>

#include "fovea_res_math.h"

namespace eye_copy_math {

// A rect of pixels from x y, w by h.
struct Rect {
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
};

inline Rect Whole(uint32_t width, uint32_t height) {
  Rect r;
  r.w = static_cast<int32_t>(width);
  r.h = static_cast<int32_t>(height);
  return r;
}

// The atlas a is the atlas of a picture of width by height.
inline bool AtlasOf(const fovea_res_math::Atlas& a, uint32_t width, uint32_t height) {
  return a.on && a.width == width && a.height == height && width > 0 && height > 0;
}

// THE PART THAT THE LAYERS SHOW. With an atlas the layers show the outer rect
// and the centre rect. A compositor with a bilinear filter reads up to one
// pixel past the edge of a rect, into the gap of kAtlasGap pixels, which is
// black with an alpha of 0. So the part is the smallest rect from 0 0 that
// holds both rects and kAtlasGap pixels to the right of and under each, cut
// to the picture. The outer rect is at 0 0 and the centre rect is to its
// right or under it, so the part also holds the gap to the left of and over
// the centre rect. With no atlas: the whole picture.
inline Rect ShownPart(const fovea_res_math::Atlas& a, uint32_t width, uint32_t height) {
  const Rect whole = Whole(width, height);
  if (!AtlasOf(a, width, height)) return whole;
  const int64_t gap = fovea_res_math::kAtlasGap;
  int64_t x1 = static_cast<int64_t>(a.outer[0]) + a.outer[2];
  int64_t y1 = static_cast<int64_t>(a.outer[1]) + a.outer[3];
  const int64_t cx1 = static_cast<int64_t>(a.center[0]) + a.center[2];
  const int64_t cy1 = static_cast<int64_t>(a.center[1]) + a.center[3];
  if (cx1 > x1) x1 = cx1;
  if (cy1 > y1) y1 = cy1;
  x1 += gap;
  y1 += gap;
  if (x1 > static_cast<int64_t>(width)) x1 = width;
  if (y1 > static_cast<int64_t>(height)) y1 = height;
  if (x1 <= 0 || y1 <= 0 || a.outer[0] < 0 || a.outer[1] < 0 || a.center[0] < 0 ||
      a.center[1] < 0) {
    return whole;
  }
  Rect r;
  r.w = static_cast<int32_t>(x1);
  r.h = static_cast<int32_t>(y1);
  return r;
}

// The part of a picture that a copy takes (xr_eye_rects): the shown part with
// the key, else the whole picture.
inline Rect CopyPart(bool key, const fovea_res_math::Atlas& a, uint32_t width,
                     uint32_t height) {
  return key ? ShownPart(a, width, height) : Whole(width, height);
}

// The bytes that a copy of the rect writes, for `layers` layers of RGBA8.
inline uint64_t Bytes(const Rect& r, uint32_t layers) {
  if (r.w <= 0 || r.h <= 0) return 0;
  return static_cast<uint64_t>(r.w) * static_cast<uint64_t>(r.h) * 4ull * layers;
}

// THE EYES STRAIGHT INTO THE SWAPCHAINS (xr_eye_direct=1). The size of the
// two swapchains of a slot, made for its first picture of width by height:
// the shown part of that picture (the whole picture with no atlas).
inline void DirectSize(const fovea_res_math::Atlas& a, uint32_t width, uint32_t height,
                       uint32_t* set_width, uint32_t* set_height) {
  const Rect r = ShownPart(a, width, height);
  *set_width = static_cast<uint32_t>(r.w);
  *set_height = static_cast<uint32_t>(r.h);
}

// A picture of width by height goes straight into two swapchains of set_width
// by set_height: a picture with an atlas when its shown part fits in them, a
// picture with no atlas when it has their size. The rects of the layers are
// then the rects of the atlas, or the whole swapchain.
inline bool DirectFits(uint32_t set_width, uint32_t set_height,
                       const fovea_res_math::Atlas& a, uint32_t width, uint32_t height) {
  if (set_width == 0 || set_height == 0 || width == 0 || height == 0) return false;
  if (!AtlasOf(a, width, height)) return width == set_width && height == set_height;
  const Rect r = ShownPart(a, width, height);
  return static_cast<uint32_t>(r.w) <= set_width && static_cast<uint32_t>(r.h) <= set_height;
}

// THE EVEN RATE OF THE PICTURES (frame_loop_hold of bwvr.ini, 0 by default).
// N of 2 or more: the session takes a new picture only when the shown picture
// showed in N frames of the session or more. So when the GPU makes a picture
// in about each second frame, each picture shows in 2 frames (36 pictures a
// second at 72 Hz), not in 1 or 2 frames in turn. 0 and 1: the session takes
// the newest picture at each frame. The most is 8.
constexpr int kMostHold = 8;
inline int Hold(int key) { return key < 0 ? 0 : (key > kMostHold ? kMostHold : key); }
// shown: the frames of the session that showed the shown picture, before this
// frame. any: a picture showed.
inline bool HoldTakes(int hold, uint64_t shown, bool any) {
  return !any || hold <= 1 || shown >= static_cast<uint64_t>(hold);
}
// The count after a frame: 1 in a frame that took a new picture, else one
// more.
inline uint64_t NextShown(uint64_t shown, bool fresh) { return fresh ? 1 : shown + 1; }

}  // namespace eye_copy_math
