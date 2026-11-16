// The save picture of the game in VR (snapshot_guard.cpp, docs/backend.md,
// "The save picture"). Pure math, with checks in
// test/controller_math/save_picture_checks.h.
//
// The game makes the picture of a save from its primary surface. In VR the
// mod gives it a surface of its own with a part of the eye picture of the
// last frame: the middle of the view, in the shape of a screen of 4 by 3, at
// kWidth by kHeight pixels. The game then reads it as it reads a screen.
//
// THE DXT1 BLOCKS. The game keeps each save picture as a texture of DXT1
// blocks (LH3DCT). A Blt from a texture of RGB pixels into such a texture
// makes the blocks. The backend of the mod on Vulkan (vk_ddraw.cpp) makes
// them with EncodeDxt1Block.
#pragma once

#include <stdint.h>
#include <string.h>

namespace save_picture {

// The surface that the mod gives the game: 4 by 3, as a screen of the game.
// The half picture of the game reads 256 by 128 points from it, and the full
// picture 400 by 300.
const int kWidth = 512;
const int kHeight = 384;
// THE SIZE OF THE READ (save_picture_fit=1). The game reads 256 by 128
// points for the half picture of a save, and 400 by 300 for the full
// picture, at a step of the width and of the height of the screen over these
// counts, and it takes the one pixel at each point
// (ScriptedScreenShot::Process). With a surface of exactly that size each
// point is the mean of all of its box of the eye picture. A surface of
// kWidth by kHeight gives the half picture one pixel of each 2 by 3.
const int kHalfWidth = 256;
const int kHalfHeight = 128;
const int kFullWidth = 400;
const int kFullHeight = 300;

// The size of the surface for a picture: the size of the read with `fit`,
// else kWidth by kHeight.
inline void ReadSize(bool full, bool fit, int* width, int* height) {
  if (!fit) {
    *width = kWidth;
    *height = kHeight;
  } else if (full) {
    *width = kFullWidth;
    *height = kFullHeight;
  } else {
    *width = kHalfWidth;
    *height = kHalfHeight;
  }
}

// The share of the width of the eye picture that the save picture shows.
// The view of an eye is about 100 degrees wide. The middle 0.6 of it is
// about the view of the game camera on a monitor.
const float kShare = 0.6f;

struct Rect {
  int x, y, w, h;
};

// The part of a picture of width by height that the save picture shows: the
// middle `share` of the width, and the height for 4 by 3. When the picture
// is too low for that, all of its height and the width for 4 by 3. A share
// that is not in (0, 1] is 1.
inline Rect Crop(int width, int height, float share) {
  Rect r = {0, 0, 0, 0};
  if (width <= 0 || height <= 0) return r;
  if (!(share > 0.0f) || share > 1.0f) share = 1.0f;
  int w = static_cast<int>(width * share + 0.5f);
  if (w < 1) w = 1;
  int h = (w * 3 + 2) / 4;
  if (h > height) {
    h = height;
    w = (h * 4 + 1) / 3;
    if (w > width) w = width;
  }
  if (h < 1) h = 1;
  r.x = (width - w) / 2;
  r.y = (height - h) / 2;
  r.w = w;
  r.h = h;
  return r;
}

// The part `part` of a picture of 4 bytes a pixel (BGRA, or RGBA with
// red_first) into out_w by out_h pixels of BGRA with an alpha of 255. Each
// pixel of the result is the mean of the pixels of its box in the part, so
// each pixel of the part counts one time. A box of less than one pixel takes
// the pixel at its start. False, with no write, when the part is not in the
// picture.
inline bool Shrink(const uint8_t* src, int pitch, int width, int height,
                   bool red_first, Rect part, uint8_t* out, int out_w,
                   int out_h) {
  if (src == nullptr || out == nullptr || out_w <= 0 || out_h <= 0 ||
      part.w <= 0 || part.h <= 0 || part.x < 0 || part.y < 0 ||
      part.x + part.w > width || part.y + part.h > height ||
      pitch < width * 4) {
    return false;
  }
  const int red = red_first ? 0 : 2;
  const int blue = red_first ? 2 : 0;
  for (int oy = 0; oy < out_h; ++oy) {
    int y0 = part.y + oy * part.h / out_h;
    int y1 = part.y + (oy + 1) * part.h / out_h;
    if (y1 <= y0) y1 = y0 + 1;
    for (int ox = 0; ox < out_w; ++ox) {
      int x0 = part.x + ox * part.w / out_w;
      int x1 = part.x + (ox + 1) * part.w / out_w;
      if (x1 <= x0) x1 = x0 + 1;
      uint32_t sum[3] = {0, 0, 0};
      for (int y = y0; y < y1; ++y) {
        const uint8_t* p = src + static_cast<size_t>(y) * pitch +
                           static_cast<size_t>(x0) * 4;
        for (int x = x0; x < x1; ++x, p += 4) {
          sum[0] += p[blue];
          sum[1] += p[1];
          sum[2] += p[red];
        }
      }
      const uint32_t n = static_cast<uint32_t>((y1 - y0) * (x1 - x0));
      uint8_t* o = out + (static_cast<size_t>(oy) * out_w + ox) * 4;
      for (int c = 0; c < 3; ++c) {
        o[c] = static_cast<uint8_t>((sum[c] + n / 2) / n);
      }
      o[3] = 255;
    }
  }
  return true;
}

// The mean light of count BGRA pixels (0 black, 1 white) and the share of
// the pixels that are black (each channel under 8), for the log.
inline void Measure(const uint8_t* bgra, int count, float* light,
                    float* dark) {
  *light = 0.0f;
  *dark = 0.0f;
  if (bgra == nullptr || count <= 0) return;
  double sum = 0.0;
  int black = 0;
  for (int i = 0; i < count; ++i) {
    const uint8_t* p = bgra + static_cast<size_t>(i) * 4;
    sum += p[0] + p[1] + p[2];
    if (p[0] < 8 && p[1] < 8 && p[2] < 8) ++black;
  }
  *light = static_cast<float>(sum / (765.0 * count));
  *dark = static_cast<float>(black) / count;
}

// A color of 8 bits a channel (r, g, b) as 5, 6 and 5 bits, and back.
inline uint16_t To565(const uint8_t rgb[3]) {
  return static_cast<uint16_t>(((rgb[0] * 31 + 127) / 255) << 11 |
                               ((rgb[1] * 63 + 127) / 255) << 5 |
                               ((rgb[2] * 31 + 127) / 255));
}

inline void From565(uint16_t c, uint8_t rgb[3]) {
  const uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  rgb[0] = static_cast<uint8_t>((r * 255 + 15) / 31);
  rgb[1] = static_cast<uint8_t>((g * 255 + 31) / 63);
  rgb[2] = static_cast<uint8_t>((b * 255 + 15) / 31);
}

// The four colors of a DXT1 block with two colors c0 > c1.
inline void Palette(uint16_t c0, uint16_t c1, uint8_t out[4][3]) {
  From565(c0, out[0]);
  From565(c1, out[1]);
  for (int k = 0; k < 3; ++k) {
    out[2][k] = static_cast<uint8_t>((2 * out[0][k] + out[1][k] + 1) / 3);
    out[3][k] = static_cast<uint8_t>((out[0][k] + 2 * out[1][k] + 1) / 3);
  }
}

// One DXT1 block of 4 by 4 pixels (RGBA8, row by row) into its 8 bytes. The
// two colors are the ends of the box of the colors of the block, moved in by
// one sixteenth of the box. Each pixel gets the nearest of the four colors.
// The block always has c0 > c1 (four colors and no transparent pixel), or
// one color for a block of one color.
inline void EncodeDxt1Block(const uint8_t rgba[16][4], uint8_t out[8]) {
  uint8_t lo[3] = {255, 255, 255}, hi[3] = {0, 0, 0};
  for (int i = 0; i < 16; ++i) {
    for (int k = 0; k < 3; ++k) {
      if (rgba[i][k] < lo[k]) lo[k] = rgba[i][k];
      if (rgba[i][k] > hi[k]) hi[k] = rgba[i][k];
    }
  }
  for (int k = 0; k < 3; ++k) {
    const int inset = (hi[k] - lo[k]) / 16;
    hi[k] = static_cast<uint8_t>(hi[k] - inset);
    lo[k] = static_cast<uint8_t>(lo[k] + inset);
  }
  // The line of the colors goes along the diagonal of the box that follows
  // them: a channel that falls while the channel of the widest range rises
  // has its ends the other way round.
  int main = 0;
  for (int k = 1; k < 3; ++k) {
    if (hi[k] - lo[k] > hi[main] - lo[main]) main = k;
  }
  int mean[3] = {0, 0, 0};
  for (int i = 0; i < 16; ++i) {
    for (int k = 0; k < 3; ++k) mean[k] += rgba[i][k];
  }
  for (int k = 0; k < 3; ++k) {
    if (k == main) continue;
    int together = 0;
    for (int i = 0; i < 16; ++i) {
      together += (rgba[i][main] * 16 - mean[main]) *
                  (rgba[i][k] * 16 - mean[k]) / 256;
    }
    if (together < 0) {
      const uint8_t t = hi[k];
      hi[k] = lo[k];
      lo[k] = t;
    }
  }
  uint16_t c0 = To565(hi), c1 = To565(lo);
  if (c0 < c1) {
    const uint16_t t = c0;
    c0 = c1;
    c1 = t;
  }
  uint32_t bits = 0;
  if (c0 == c1) {
    // One color: c0 > c1 needs two colors. The second one is one step
    // lower, and each index is 0. A black block has the black as c1, and
    // each index is 1.
    if (c1 > 0) {
      --c1;
    } else {
      c0 = 1;
      bits = 0x55555555u;
    }
  } else {
    uint8_t colors[4][3];
    Palette(c0, c1, colors);
    for (int i = 0; i < 16; ++i) {
      int best = 0;
      int best_d = 1 << 30;
      for (int j = 0; j < 4; ++j) {
        int d = 0;
        for (int k = 0; k < 3; ++k) {
          const int e = rgba[i][k] - colors[j][k];
          d += e * e;
        }
        if (d < best_d) {
          best_d = d;
          best = j;
        }
      }
      bits |= static_cast<uint32_t>(best) << (2 * i);
    }
  }
  out[0] = static_cast<uint8_t>(c0);
  out[1] = static_cast<uint8_t>(c0 >> 8);
  out[2] = static_cast<uint8_t>(c1);
  out[3] = static_cast<uint8_t>(c1 >> 8);
  memcpy(out + 4, &bits, 4);
}

// The sum of the squared errors of the channels of a block with c0 > c1
// against its 16 pixels.
inline int BlockError(const uint8_t rgba[16][4], const uint8_t block[8]) {
  const uint16_t c0 = static_cast<uint16_t>(block[0] | block[1] << 8);
  const uint16_t c1 = static_cast<uint16_t>(block[2] | block[3] << 8);
  uint8_t colors[4][3];
  Palette(c0, c1, colors);
  uint32_t bits = 0;
  memcpy(&bits, block + 4, 4);
  int sum = 0;
  for (int i = 0; i < 16; ++i) {
    const uint8_t* c = colors[(bits >> (2 * i)) & 3];
    for (int k = 0; k < 3; ++k) {
      const int e = rgba[i][k] - c[k];
      sum += e * e;
    }
  }
  return sum;
}

// A block with the two colors c0 > c1: each pixel gets the nearest of the
// four colors.
inline void IndexBlock(const uint8_t rgba[16][4], uint16_t c0, uint16_t c1,
                       uint8_t out[8]) {
  uint8_t colors[4][3];
  Palette(c0, c1, colors);
  uint32_t bits = 0;
  for (int i = 0; i < 16; ++i) {
    int best = 0;
    int best_d = 1 << 30;
    for (int j = 0; j < 4; ++j) {
      int d = 0;
      for (int k = 0; k < 3; ++k) {
        const int e = rgba[i][k] - colors[j][k];
        d += e * e;
      }
      if (d < best_d) {
        best_d = d;
        best = j;
      }
    }
    bits |= static_cast<uint32_t>(best) << (2 * i);
  }
  out[0] = static_cast<uint8_t>(c0);
  out[1] = static_cast<uint8_t>(c0 >> 8);
  out[2] = static_cast<uint8_t>(c1);
  out[3] = static_cast<uint8_t>(c1 >> 8);
  memcpy(out + 4, &bits, 4);
}

// THE FIT (vk_dxt_fit=1). The block of EncodeDxt1Block, then up to two steps
// of least squares: with the colors of the pixels fixed, the two colors that
// give the smallest squared error. Index 0 is all of c0, index 1 all of c1,
// index 2 two thirds of c0 and index 3 one third. A step stays only when its
// error is smaller, so the fit is never worse than EncodeDxt1Block. The
// block always has c0 > c1.
inline void FitDxt1Block(const uint8_t rgba[16][4], uint8_t out[8]) {
  EncodeDxt1Block(rgba, out);
  int best = BlockError(rgba, out);
  for (int step = 0; step < 2 && best > 0; ++step) {
    uint32_t bits = 0;
    memcpy(&bits, out + 4, 4);
    static const float kShare0[4] = {1.0f, 0.0f, 2.0f / 3.0f, 1.0f / 3.0f};
    float aa = 0.0f, ab = 0.0f, bb = 0.0f;
    float ax[3] = {0.0f, 0.0f, 0.0f}, bx[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 16; ++i) {
      const float a = kShare0[(bits >> (2 * i)) & 3];
      const float b = 1.0f - a;
      aa += a * a;
      ab += a * b;
      bb += b * b;
      for (int k = 0; k < 3; ++k) {
        ax[k] += a * rgba[i][k];
        bx[k] += b * rgba[i][k];
      }
    }
    const float det = aa * bb - ab * ab;
    if (!(det > 1e-3f)) return;  // all pixels on one color of the line
    uint8_t hi[3], lo[3];
    for (int k = 0; k < 3; ++k) {
      float c0 = (bb * ax[k] - ab * bx[k]) / det;
      float c1 = (aa * bx[k] - ab * ax[k]) / det;
      c0 = c0 < 0.0f ? 0.0f : c0 > 255.0f ? 255.0f : c0;
      c1 = c1 < 0.0f ? 0.0f : c1 > 255.0f ? 255.0f : c1;
      hi[k] = static_cast<uint8_t>(c0 + 0.5f);
      lo[k] = static_cast<uint8_t>(c1 + 0.5f);
    }
    uint16_t c0 = To565(hi), c1 = To565(lo);
    if (c0 == c1) return;
    if (c0 < c1) {
      const uint16_t t = c0;
      c0 = c1;
      c1 = t;
    }
    uint8_t trial[8];
    IndexBlock(rgba, c0, c1, trial);
    const int error = BlockError(rgba, trial);
    if (error >= best) return;
    best = error;
    memcpy(out, trial, 8);
  }
}

// The 16 pixels of a DXT1 block with c0 > c1 (as EncodeDxt1Block makes
// them), RGB, row by row. For the checks.
inline void DecodeDxt1Block(const uint8_t in[8], uint8_t out[16][3]) {
  const uint16_t c0 = static_cast<uint16_t>(in[0] | in[1] << 8);
  const uint16_t c1 = static_cast<uint16_t>(in[2] | in[3] << 8);
  uint8_t colors[4][3];
  Palette(c0, c1, colors);
  uint32_t bits = 0;
  memcpy(&bits, in + 4, 4);
  for (int i = 0; i < 16; ++i) {
    memcpy(out[i], colors[(bits >> (2 * i)) & 3], 3);
  }
}

}  // namespace save_picture
