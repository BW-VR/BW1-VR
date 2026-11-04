// THE LATE COMPOSE of the eye picture (vk_compose.cpp, docs/backend.md, "The
// compose of the eye picture"). Pure: no Windows, no Vulkan. Checks in
// test/controller_math/vk_compose_checks.h.
//
// With fovea_res=1 and fovea_res_layer=0 the eye picture of an eye is the
// outer image over the whole picture, and the centre image over its rect
// with a soft edge. The compose draws it in one draw of a grid with 5 by 5
// cells. The rows and the columns of the grid break at the edges of the
// centre rect and at the inner edge of its soft edge, so the 3 by 3 cells in
// the middle are the centre rect and the 16 cells round it are the outer
// image alone. A vertex has its place in pixels of the eye picture (the
// boundary of a pixel is a whole number), the alpha of the centre, and the
// texture coordinates of the two images. The fragment shader mixes the two
// images by the alpha, so no pixel is written twice.
#pragma once

#include <math.h>
#include <stdint.h>

namespace vk_compose_math {

// What the compose needs from a walk.
struct Layout {
  uint32_t width, height;              // the eye picture
  uint32_t outer_w, outer_h;           // the used part of the outer image
  uint32_t outer_image_w, outer_image_h;
  uint32_t center_w, center_h;         // the used part of the centre image
  uint32_t center_image_w, center_image_h;
  float inset_x0, inset_y0, inset_w, inset_h;  // the centre rect in the eye picture
};

// 32 bytes: the layout of the vertex buffer in vk_compose.vert.
struct Vertex {
  float x, y;     // pixels of the eye picture
  float alpha;    // 1 inside the soft edge of the centre, else 0
  float pad;
  float u_outer, v_outer;
  float u_center, v_center;
};

constexpr int kColumns = 5;
constexpr int kRows = 5;
constexpr int kVertices = kColumns * kRows * 6;

// The soft edge of the centre: 8 percent of its smaller side.
inline float EdgeOf(const Layout& l) {
  return 0.08f * (l.inset_w < l.inset_h ? l.inset_w : l.inset_h);
}

// The breaks of the grid across (x) and down (y), 6 values each.
inline void Breaks(const Layout& l, float x[6], float y[6]) {
  const float e = EdgeOf(l);
  x[0] = 0.0f;
  x[1] = l.inset_x0;
  x[2] = l.inset_x0 + e;
  x[3] = l.inset_x0 + l.inset_w - e;
  x[4] = l.inset_x0 + l.inset_w;
  x[5] = static_cast<float>(l.width);
  y[0] = 0.0f;
  y[1] = l.inset_y0;
  y[2] = l.inset_y0 + e;
  y[3] = l.inset_y0 + l.inset_h - e;
  y[4] = l.inset_y0 + l.inset_h;
  y[5] = static_cast<float>(l.height);
}

// The 150 vertices (25 cells of 2 triangles). Gives kVertices. The corners
// of a cell are in the order of the draw of the old compose, so the soft
// edge has the same diagonals.
inline int BuildGrid(const Layout& l, Vertex out[kVertices]) {
  float bx[6], by[6];
  Breaks(l, bx, by);
  const float ou = static_cast<float>(l.outer_w) / static_cast<float>(l.outer_image_w);
  const float ov = static_cast<float>(l.outer_h) / static_cast<float>(l.outer_image_h);
  const float cu = static_cast<float>(l.center_w) / static_cast<float>(l.center_image_w);
  const float cv = static_cast<float>(l.center_h) / static_cast<float>(l.center_image_h);
  const float w = static_cast<float>(l.width), h = static_cast<float>(l.height);
  int n = 0;
  for (int j = 0; j < kRows; ++j) {
    for (int i = 0; i < kColumns; ++i) {
      const int corner[6][2] = {{i, j},         {i + 1, j},
                                {i + 1, j + 1}, {i, j},
                                {i + 1, j + 1}, {i, j + 1}};
      for (int k = 0; k < 6; ++k) {
        const int ci = corner[k][0], cj = corner[k][1];
        Vertex& v = out[n++];
        v.x = bx[ci];
        v.y = by[cj];
        // The break 2 and 3 are the inside of the soft edge, 1 and 4 the
        // edge of the rect: the alpha is 1 at the breaks 2 and 3 of both
        // directions. A vertex outside the rect has the alpha 0.
        v.alpha = (ci == 2 || ci == 3) && (cj == 2 || cj == 3) ? 1.0f : 0.0f;
        v.pad = 0.0f;
        v.u_outer = bx[ci] / w * ou;
        v.v_outer = by[cj] / h * ov;
        v.u_center = (bx[ci] - l.inset_x0) / l.inset_w * cu;
        v.v_center = (by[cj] - l.inset_y0) / l.inset_h * cv;
      }
    }
  }
  return n;
}

// The sRGB transfer function of the shader, in floats. A target of an sRGB
// format encodes what the shader writes, so the shader writes the decoded
// bytes of the game and the target stores them again.
inline float SrgbToLinear(float c) {
  return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}
inline float LinearToSrgb(float c) {
  return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

// The rect of the eye picture that the compose writes: all of it.
inline uint64_t BytesWritten(const Layout& l) {
  return static_cast<uint64_t>(l.width) * l.height * 4u;
}

}  // namespace vk_compose_math
