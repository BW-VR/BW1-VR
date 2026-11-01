// Foveated resolution: an outer image with fewer pixels and a centre image
// with more, for one eye picture (vk_scene.cpp, docs/backend.md, "Foveated
// resolution"). Pure: no Windows, no Vulkan. Checks in
// test/controller_math/fovea_res_checks.h.
//
// The game gives each vertex in pixels of its render, width by height (the
// eye picture). PackAtlas and CenterFov put the two images as layers of
// their own into the eye picture (fovea_res_layer). The outer image has the
// whole view at outer_pct percent of the pixels on each side. The centre image has a rect of the view around
// the centre of the fovea at center_pct percent: its half width and half
// height are inset_deg degrees from the centre. A pass maps the pixels of
// the game into its image with a scale and an origin, so the same draws go
// into both images.
#pragma once

#include <math.h>
#include <stdint.h>

namespace fovea_res_math {

struct Layout {
  uint32_t outer_w, outer_h;    // the outer image
  uint32_t center_w, center_h;  // the centre image
  // The rect of the centre in pixels of the game.
  float inset_x0, inset_y0, inset_w, inset_h;
  // pixel of an image = (pixel of the game - origin) * scale
  float outer_scale_x, outer_scale_y;
  float center_scale_x, center_scale_y;
};

inline uint32_t Scaled(float pixels, float pct) {
  const float v = pixels * pct / 100.0f + 0.5f;
  return v < 16.0f ? 16u : static_cast<uint32_t>(v);
}

// The layout for a picture of width by height with the view tan_h by tan_v
// (tan of the half angles) and the centre of the fovea at the tangents gx,
// gy (x right, y up). outer_pct 5 to 100, center_pct 25 to 300, inset_deg
// 5 to 80.
inline Layout MakeLayout(uint32_t width, uint32_t height, float tan_h,
                         float tan_v, float gx, float gy, float outer_pct,
                         float center_pct, float inset_deg) {
  Layout l;
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  l.outer_w = Scaled(w, outer_pct);
  l.outer_h = Scaled(h, outer_pct);
  l.outer_scale_x = static_cast<float>(l.outer_w) / w;
  l.outer_scale_y = static_cast<float>(l.outer_h) / h;
  // The half size of the rect: inset_deg on each side of the centre.
  const float t = tanf(inset_deg / 57.2957795f);
  float half_w = t / tan_h * 0.5f * w;
  float half_h = t / tan_v * 0.5f * h;
  if (half_w > 0.5f * w) half_w = 0.5f * w;
  if (half_h > 0.5f * h) half_h = 0.5f * h;
  if (half_w < 8.0f) half_w = 8.0f;
  if (half_h < 8.0f) half_h = 8.0f;
  // The centre in pixels, and the rect kept in the picture.
  const float cx = (gx / tan_h + 1.0f) * 0.5f * w;
  const float cy = (1.0f - gy / tan_v) * 0.5f * h;
  float x0 = cx - half_w, y0 = cy - half_h;
  if (x0 < 0.0f) x0 = 0.0f;
  if (y0 < 0.0f) y0 = 0.0f;
  if (x0 + 2.0f * half_w > w) x0 = w - 2.0f * half_w;
  if (y0 + 2.0f * half_h > h) y0 = h - 2.0f * half_h;
  l.inset_x0 = floorf(x0);
  l.inset_y0 = floorf(y0);
  l.inset_w = floorf(2.0f * half_w);
  l.inset_h = floorf(2.0f * half_h);
  l.center_w = Scaled(l.inset_w, center_pct);
  l.center_h = Scaled(l.inset_h, center_pct);
  l.center_scale_x = static_cast<float>(l.center_w) / l.inset_w;
  l.center_scale_y = static_cast<float>(l.center_h) / l.inset_h;
  return l;
}

// The tangent y (up) of a centre down_deg below straight ahead
// (fovea_res_center_down_deg). 0 to 60 degrees: a value outside goes to the
// nearest end.
inline float DownGaze(float down_deg) {
  if (!(down_deg > 0.0f)) return 0.0f;
  if (down_deg > 60.0f) down_deg = 60.0f;
  return -tanf(down_deg / 57.2957795f);
}

// The share of the pixels of the two images against the picture.
inline float PixelShare(const Layout& l, uint32_t width, uint32_t height) {
  return (static_cast<float>(l.outer_w) * l.outer_h +
          static_cast<float>(l.center_w) * l.center_h) /
         (static_cast<float>(width) * height);
}

// A rect of pixels of the game in an image with an origin and a scale: the
// rect of the image, x0, y0, x1, y1, not cut to the image.
inline void MapRect(float x0, float y0, float x1, float y1, float origin_x,
                    float origin_y, float scale_x, float scale_y, float out[4]) {
  out[0] = (x0 - origin_x) * scale_x;
  out[1] = (y0 - origin_y) * scale_y;
  out[2] = (x1 - origin_x) * scale_x;
  out[3] = (y1 - origin_y) * scale_y;
}

// THE MIDDLE AS ITS OWN LAYER (fovea_res_layer, docs/backend.md, "The
// middle as its own layer"). The eye picture holds the outer image and the
// centre image side by side, each at its own size, and the headset app shows
// each as a projection layer of its own: the outer image over the whole
// view, and the centre image over its rect with an alpha edge. So the
// compositor samples the pixels of the centre image directly, and a centre
// above 100 percent reaches the panel.
struct Atlas {
  bool mres;  // the walk had foveated resolution
  bool on;    // the eye picture holds the two images, each for a layer
  // The rects in the eye picture: x, y, width, height.
  int32_t outer[4];
  int32_t center[4];
  // The rect of the centre in the view, as parts of the eye picture: x0, y0,
  // x1, y1 (0 to 1, y down).
  float view[4];
  // Pixels of each image for a pixel of the game, across.
  float outer_scale;
  float center_scale;
  // The size of the eye picture of the rects.
  uint32_t width, height;
};

// The pixels between the outer rect and the centre rect. They are black with
// an alpha of 0, so a compositor that samples a rect with a bilinear filter
// at its edge reads no pixel of the other image.
constexpr int32_t kAtlasGap = 2;

// The rects of the layout l in an eye picture of width by height: the outer
// image at the top left, the centre image kAtlasGap pixels to its right, or
// under it when it fits there with more pixels. A centre that fits in neither
// place at its size gets a smaller size that fits (center_w, center_h and the
// centre scales of l change). on is false when even a centre of 16 pixels
// does not fit: the eye picture is then made from the two images as with
// fovea_res_layer=0.
inline Atlas PackAtlas(uint32_t width, uint32_t height, Layout* l) {
  Atlas a = {};
  a.mres = true;
  a.outer_scale = l->outer_scale_x;
  a.center_scale = l->center_scale_x;
  a.width = width;
  a.height = height;
  if (width == 0 || height == 0 || l->outer_w > width || l->outer_h > height ||
      l->center_w == 0 || l->center_h == 0) {
    return a;
  }
  const float w = static_cast<float>(width), h = static_cast<float>(height);
  const float cw = static_cast<float>(l->center_w);
  const float ch = static_cast<float>(l->center_h);
  // The part of the size of the centre that fits at each place, at most 1.
  auto fit = [](float room_w, float room_h, float cw, float ch) {
    float f = room_w / cw;
    if (room_h / ch < f) f = room_h / ch;
    return f > 1.0f ? 1.0f : (f < 0.0f ? 0.0f : f);
  };
  const float gap = static_cast<float>(kAtlasGap);
  const float right = fit(w - static_cast<float>(l->outer_w) - gap, h, cw, ch);
  const float below = fit(w, h - static_cast<float>(l->outer_h) - gap, cw, ch);
  const bool to_right = right >= below;
  const float f = to_right ? right : below;
  if (f < 1.0f) {
    const uint32_t nw = static_cast<uint32_t>(cw * f);
    const uint32_t nh = static_cast<uint32_t>(ch * f);
    if (nw < 16 || nh < 16) return a;
    l->center_w = nw;
    l->center_h = nh;
    l->center_scale_x = static_cast<float>(nw) / l->inset_w;
    l->center_scale_y = static_cast<float>(nh) / l->inset_h;
  }
  a.on = true;
  a.center_scale = l->center_scale_x;
  a.outer[0] = 0;
  a.outer[1] = 0;
  a.outer[2] = static_cast<int32_t>(l->outer_w);
  a.outer[3] = static_cast<int32_t>(l->outer_h);
  a.center[0] = to_right ? static_cast<int32_t>(l->outer_w) + kAtlasGap : 0;
  a.center[1] = to_right ? 0 : static_cast<int32_t>(l->outer_h) + kAtlasGap;
  a.center[2] = static_cast<int32_t>(l->center_w);
  a.center[3] = static_cast<int32_t>(l->center_h);
  a.view[0] = l->inset_x0 / w;
  a.view[1] = l->inset_y0 / h;
  a.view[2] = (l->inset_x0 + l->inset_w) / w;
  a.view[3] = (l->inset_y0 + l->inset_h) / h;
  return a;
}

// The angles of view of the centre layer (radians: left, right, up, down)
// from the angles of the layer of the whole view and the rect of the centre
// in parts of the picture (Atlas::view). A projection layer maps its picture
// linearly onto the tangents of its angles, so a part of the picture is a
// part of the span of the tangents.
inline void CenterFov(const float fov[4], const float view[4], float out[4]) {
  const float tl = tanf(fov[0]), tr = tanf(fov[1]);
  const float tu = tanf(fov[2]), td = tanf(fov[3]);
  out[0] = atanf(tl + view[0] * (tr - tl));
  out[1] = atanf(tl + view[2] * (tr - tl));
  out[2] = atanf(tu - view[1] * (tu - td));
  out[3] = atanf(tu - view[3] * (tu - td));
}

// The atlas for a swapchain of sw by sh that took the eye picture of a: the
// same rects when the sizes are the same. Else the eye picture went into the
// swapchain with a blit to its size, so each rect is scaled to the swapchain,
// rounded inward (a rect keeps its place within one pixel and reads no pixel
// of the gap), and kept in the swapchain. on is false when a rect has no
// pixel then.
inline Atlas FitAtlas(const Atlas& a, uint32_t sw, uint32_t sh) {
  if (!a.on || (a.width == sw && a.height == sh)) return a;
  Atlas f = a;
  f.width = sw;
  f.height = sh;
  if (a.width == 0 || a.height == 0 || sw == 0 || sh == 0) {
    f.on = false;
    return f;
  }
  const float sx = static_cast<float>(sw) / static_cast<float>(a.width);
  const float sy = static_cast<float>(sh) / static_cast<float>(a.height);
  auto scale = [&](const int32_t in[4], int32_t out[4]) {
    int32_t x0 = static_cast<int32_t>(ceilf(static_cast<float>(in[0]) * sx));
    int32_t y0 = static_cast<int32_t>(ceilf(static_cast<float>(in[1]) * sy));
    int32_t x1 = static_cast<int32_t>(floorf(static_cast<float>(in[0] + in[2]) * sx));
    int32_t y1 = static_cast<int32_t>(floorf(static_cast<float>(in[1] + in[3]) * sy));
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > static_cast<int32_t>(sw)) x1 = static_cast<int32_t>(sw);
    if (y1 > static_cast<int32_t>(sh)) y1 = static_cast<int32_t>(sh);
    out[0] = x0;
    out[1] = y0;
    out[2] = x1 > x0 ? x1 - x0 : 0;
    out[3] = y1 > y0 ? y1 - y0 : 0;
    return out[2] > 0 && out[3] > 0;
  };
  const bool outer = scale(a.outer, f.outer);
  const bool center = scale(a.center, f.center);
  f.on = outer && center;
  return f;
}

// A rect of pixels of the eye picture (x, y, width, height), for example the
// columns of the cut scene window, in the outer image of an atlas: the same
// part of the view, in the pixels of the outer rect.
inline void OuterRect(const Atlas& a, uint32_t width, uint32_t height,
                      const int32_t in[4], int32_t out[4]) {
  if (!a.on || width == 0 || height == 0) {
    for (int i = 0; i < 4; ++i) out[i] = in[i];
    return;
  }
  const float sx = static_cast<float>(a.outer[2]) / static_cast<float>(width);
  const float sy = static_cast<float>(a.outer[3]) / static_cast<float>(height);
  const int32_t x0 = static_cast<int32_t>(floorf(static_cast<float>(in[0]) * sx));
  const int32_t y0 = static_cast<int32_t>(floorf(static_cast<float>(in[1]) * sy));
  int32_t x1 = static_cast<int32_t>(ceilf(static_cast<float>(in[0] + in[2]) * sx));
  int32_t y1 = static_cast<int32_t>(ceilf(static_cast<float>(in[1] + in[3]) * sy));
  if (x1 > a.outer[2]) x1 = a.outer[2];
  if (y1 > a.outer[3]) y1 = a.outer[3];
  out[0] = a.outer[0] + x0;
  out[1] = a.outer[1] + y0;
  out[2] = x1 > x0 ? x1 - x0 : 1;
  out[3] = y1 > y0 ? y1 - y0 : 1;
}

}  // namespace fovea_res_math
