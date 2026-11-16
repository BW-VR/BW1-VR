// HOW A CARD GOES INTO ITS SWAPCHAIN (docs/ui-layer.md, "The edge and the
// mip levels of a card"). Pure math: no Windows, no OpenXR. xr_screen.cpp
// uses it in SendCard, and test/controller_math/card_send_checks.h checks it.
//
// A card is a picture of the CPU (BGRA, the color multiplied by the alpha)
// on a quad layer. The compositor of the runtime samples the quad. Two
// things go wrong when the swapchain holds only the picture:
//
//   THE EDGE. The sampler of the compositor reads the texels around the
//   edge of the rect of the quad. Past the rect each swapchain image holds
//   an older picture, or no write at all. So the edge of the quad can show a
//   faint line that changes from frame to frame.
//   THE TEXT. A card shows smaller than its picture in the headset (about 21
//   pixels a degree at the middle of a Quest 3 view through Virtual
//   Desktop). With one mip level the sampler skips texels, so thin lines of
//   ink come and go as the head moves.
//
// With the clear edge the swapchain is larger than the picture. The picture
// goes in with kFrame clear texels around it inside the rect of the quad,
// and kApron clear texels more outside the rect. Each upload writes all of
// them, so the sampler reads only clear texels at the edge. The quad grows
// by the frame, so the picture keeps its place and size in the room.
// With the mip levels the swapchain has up to kMostLevels levels. The CPU
// makes each level from the one above it after each new picture: each texel
// is the mean of 2 by 2 texels. The mean is of the bytes, in the gamma of
// the picture, as the mod blends its cards. A mean of colors that are
// multiplied by their alpha needs no other step, and a clear texel stays
// clear.

#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

namespace card_send_math {

// Clear texels inside the rect of the quad, at each side of the picture.
const int kFrame = 8;
// Clear texels outside the rect of the quad, at each side, that each upload
// also writes.
const int kApron = 8;
// At most this many mip levels: the last one is 1/8 of the picture.
const int kMostLevels = 4;
// The part that an upload writes starts and ends on a multiple of this, so
// each level is exactly half of the level above it.
const int kAlign = 1 << (kMostLevels - 1);

inline int32_t AlignDown(int32_t v) { return v & ~(kAlign - 1); }
inline int32_t AlignUp(int32_t v) { return (v + kAlign - 1) & ~(kAlign - 1); }

// The swapchain of a card whose pictures are texture_width by
// texture_height. With no clear edge and one level it is the size of the
// picture, as without this file.
inline void ChainSize(uint32_t texture_width, uint32_t texture_height,
                      bool clear_edge, int levels, uint32_t* width,
                      uint32_t* height) {
  if (!clear_edge && levels <= 1) {
    *width = texture_width;
    *height = texture_height;
    return;
  }
  const uint32_t more = 2u * (kFrame + kApron) + kAlign;
  *width = static_cast<uint32_t>(
      AlignUp(static_cast<int32_t>(texture_width + more)));
  *height = static_cast<uint32_t>(
      AlignUp(static_cast<int32_t>(texture_height + more)));
}

// The mip levels for a swapchain of that size: wanted levels at the most,
// and no level of less than 8 texels at a side.
inline int LevelsFor(uint32_t width, uint32_t height, int wanted) {
  int levels = 1;
  uint32_t side = width < height ? width : height;
  while (levels < wanted && levels < kMostLevels && side >= 16) {
    side /= 2;
    ++levels;
  }
  return levels;
}

// Where one picture goes in the swapchain. All boxes are x, y, width,
// height in texels of level 0.
struct Layout {
  int levels;
  bool padded;           // a clear edge or more than one level
  int32_t rect[4];       // the part of the picture that shows
  int32_t box[4];        // the part of the swapchain that each upload writes
  int32_t content[2];    // where the first texel of rect goes
  int32_t image_rect[4]; // the rect of the quad
  float scale[2];        // the size of the quad over the size of the picture
};

inline bool SameLayout(const Layout& a, const Layout& b) {
  if (a.levels != b.levels || a.padded != b.padded) return false;
  for (int i = 0; i < 4; ++i) {
    if (a.rect[i] != b.rect[i] || a.box[i] != b.box[i] ||
        a.image_rect[i] != b.image_rect[i]) {
      return false;
    }
  }
  return a.content[0] == b.content[0] && a.content[1] == b.content[1];
}

// The layout of the part `rect` (x, y, width, height) of a picture of
// texture_width by texture_height. False for a rect that is not in the
// picture.
inline bool LayoutOf(uint32_t texture_width, uint32_t texture_height,
                     const int32_t rect[4], bool clear_edge, int levels,
                     Layout* out) {
  const int32_t x = rect[0], y = rect[1], w = rect[2], h = rect[3];
  if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
      x + w > static_cast<int32_t>(texture_width) ||
      y + h > static_cast<int32_t>(texture_height)) {
    return false;
  }
  if (levels < 1) levels = 1;
  if (levels > kMostLevels) levels = kMostLevels;
  Layout& l = *out;
  l.levels = levels;
  l.padded = clear_edge || levels > 1;
  for (int i = 0; i < 4; ++i) l.rect[i] = rect[i];
  l.scale[0] = 1.0f;
  l.scale[1] = 1.0f;
  if (!l.padded) {
    for (int i = 0; i < 4; ++i) {
      l.box[i] = rect[i];
      l.image_rect[i] = rect[i];
    }
    l.content[0] = x;
    l.content[1] = y;
    return true;
  }
  const int32_t frame = clear_edge ? kFrame : 0;
  const int32_t margin = clear_edge ? kFrame + kApron : 0;
  l.box[0] = AlignDown(x);
  l.box[1] = AlignDown(y);
  l.box[2] = AlignUp(w + 2 * margin);
  l.box[3] = AlignUp(h + 2 * margin);
  l.content[0] = l.box[0] + margin;
  l.content[1] = l.box[1] + margin;
  l.image_rect[0] = l.content[0] - frame;
  l.image_rect[1] = l.content[1] - frame;
  l.image_rect[2] = w + 2 * frame;
  l.image_rect[3] = h + 2 * frame;
  l.scale[0] = static_cast<float>(l.image_rect[2]) / static_cast<float>(w);
  l.scale[1] = static_cast<float>(l.image_rect[3]) / static_cast<float>(h);
  return true;
}

// The box of level k, in texels of that level.
inline void LevelBox(const Layout& l, int k, int32_t out[4]) {
  for (int i = 0; i < 4; ++i) out[i] = l.box[i] >> k;
}

// The bytes of the buffer of all the levels of a layout: level 0 first, each
// level a block of its box, rows with no gap.
inline size_t LevelOffset(const Layout& l, int k) {
  size_t offset = 0;
  for (int i = 0; i < k; ++i) {
    offset += static_cast<size_t>(l.box[2] >> i) * (l.box[3] >> i) * 4;
  }
  return offset;
}
inline size_t LevelsBytes(const Layout& l) { return LevelOffset(l, l.levels); }

// Level 0 of the buffer from the picture: clear texels around rect, and the
// rect from `pixels` (rows of `pitch` bytes). swap_red_blue swaps bytes 0
// and 2 of each texel, for a swapchain with red first. With `same` the
// buffer holds the last picture of the same layout: only the rows of the
// rect that changed are written, and the clear texels stay. *changed_y0 and
// *changed_y1 get the rows of the box that changed (y1 is past the last
// one), on multiples of kAlign. No change gives y0 == y1.
inline void Fill(const Layout& l, const uint8_t* pixels, uint32_t pitch,
                 bool swap_red_blue, bool same, uint8_t* buffer,
                 int32_t* changed_y0, int32_t* changed_y1) {
  const int32_t bw = l.box[2], bh = l.box[3];
  const size_t row_bytes = static_cast<size_t>(bw) * 4;
  const int32_t cx = l.content[0] - l.box[0], cy = l.content[1] - l.box[1];
  const int32_t w = l.rect[2], h = l.rect[3];
  const size_t rect_bytes = static_cast<size_t>(w) * 4;
  if (!same) memset(buffer, 0, row_bytes * bh);
  int32_t first = bh, last = -1;
  for (int32_t r = 0; r < h; ++r) {
    const uint8_t* in = pixels + static_cast<size_t>(l.rect[1] + r) * pitch +
                        static_cast<size_t>(l.rect[0]) * 4;
    uint8_t* out = buffer + static_cast<size_t>(cy + r) * row_bytes +
                   static_cast<size_t>(cx) * 4;
    bool differs = !same;
    if (!swap_red_blue) {
      if (!differs) differs = memcmp(out, in, rect_bytes) != 0;
      if (differs) memcpy(out, in, rect_bytes);
    } else {
      for (int32_t i = 0; i < w; ++i) {
        const uint8_t b = in[i * 4 + 2], g = in[i * 4 + 1],
                      red = in[i * 4 + 0], a = in[i * 4 + 3];
        uint8_t* o = out + i * 4;
        if (o[0] != b || o[1] != g || o[2] != red || o[3] != a) {
          differs = true;
          o[0] = b;
          o[1] = g;
          o[2] = red;
          o[3] = a;
        }
      }
    }
    if (differs) {
      if (cy + r < first) first = cy + r;
      if (cy + r > last) last = cy + r;
    }
  }
  if (!same) {
    first = 0;
    last = bh - 1;
  }
  if (last < first) {
    *changed_y0 = 0;
    *changed_y1 = 0;
    return;
  }
  *changed_y0 = AlignDown(first);
  *changed_y1 = AlignUp(last + 1);
  if (*changed_y1 > bh) *changed_y1 = bh;
}

// Rows y0 to y1 of a level (of `width` texels, y0 and y1 even) make rows
// y0 / 2 to y1 / 2 of the next level: each texel the mean of 2 by 2.
inline void Halve(const uint8_t* in, int32_t width, uint8_t* out, int32_t y0,
                  int32_t y1) {
  const int32_t half = width / 2;
  const size_t in_row = static_cast<size_t>(width) * 4;
  const size_t out_row = static_cast<size_t>(half) * 4;
  for (int32_t y = y0 / 2; y < y1 / 2; ++y) {
    const uint8_t* a = in + static_cast<size_t>(2 * y) * in_row;
    const uint8_t* b = a + in_row;
    uint8_t* o = out + static_cast<size_t>(y) * out_row;
    for (int32_t x = 0; x < half; ++x, a += 8, b += 8, o += 4) {
      for (int c = 0; c < 4; ++c) {
        o[c] = static_cast<uint8_t>((a[c] + a[c + 4] + b[c] + b[c + 4] + 2) >> 2);
      }
    }
  }
}

// Levels 1 and down from level 0, for the rows y0 to y1 of level 0 that
// changed (from Fill).
inline void BuildLevels(const Layout& l, uint8_t* buffer, int32_t y0,
                        int32_t y1) {
  for (int k = 1; k < l.levels && y1 > y0; ++k) {
    const uint8_t* in = buffer + LevelOffset(l, k - 1);
    uint8_t* out = buffer + LevelOffset(l, k);
    Halve(in, l.box[2] >> (k - 1), out, y0, y1);
    y0 /= 2;
    y1 /= 2;
  }
}

// The number of texels of level 0 with a color byte larger than the alpha:
// a picture with the color not multiplied by the alpha has many.
inline uint32_t StraightTexels(const Layout& l, const uint8_t* buffer) {
  uint32_t n = 0;
  const size_t texels = static_cast<size_t>(l.box[2]) * l.box[3];
  for (size_t i = 0; i < texels; ++i) {
    const uint8_t* t = buffer + i * 4;
    if (t[0] > t[3] || t[1] > t[3] || t[2] > t[3]) ++n;
  }
  return n;
}

// --- a model of the compositor, for the checks and the test pictures ------
//
// A view of the quad straight on, at `texels_per_pixel` texels of level 0
// for each pixel of the view. The sampler reads with clamp to the edge of
// its image, as most compositors do.

struct Image {
  const uint8_t* bgra;  // the color multiplied by the alpha
  int32_t width;
  int32_t height;
};

// A bilinear sample at the point u, v in texels (0, 0 is the corner of the
// first texel). out gets B, G, R, A from 0 to 1.
inline void Bilinear(const Image& image, float u, float v, float out[4]) {
  const float x = u - 0.5f, y = v - 0.5f;
  const float fx = floorf(x), fy = floorf(y);
  const float tx = x - fx, ty = y - fy;
  const int32_t x0 = static_cast<int32_t>(fx), y0 = static_cast<int32_t>(fy);
  for (int c = 0; c < 4; ++c) out[c] = 0.0f;
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < 2; ++i) {
      int32_t xi = x0 + i, yj = y0 + j;
      if (xi < 0) xi = 0;
      if (yj < 0) yj = 0;
      if (xi >= image.width) xi = image.width - 1;
      if (yj >= image.height) yj = image.height - 1;
      const float weight = (i ? tx : 1.0f - tx) * (j ? ty : 1.0f - ty);
      const uint8_t* t =
          image.bgra + (static_cast<size_t>(yj) * image.width + xi) * 4;
      for (int c = 0; c < 4; ++c) out[c] += weight * (t[c] / 255.0f);
    }
  }
}

// The ideal sample: the mean of the texels under the square of a pixel,
// each with the share of the square that it covers. A sampler cannot do
// better, so the shimmer of this view is the floor for the others.
inline void Area(const Image& image, float u, float v, float size,
                 float out[4]) {
  const float u0 = u - 0.5f * size, u1 = u + 0.5f * size;
  const float v0 = v - 0.5f * size, v1 = v + 0.5f * size;
  for (int c = 0; c < 4; ++c) out[c] = 0.0f;
  float total = 0.0f;
  for (int32_t y = static_cast<int32_t>(floorf(v0)); y < v1; ++y) {
    const float wy = fminf(v1, y + 1.0f) - fmaxf(v0, static_cast<float>(y));
    if (wy <= 0.0f) continue;
    int32_t yc = y < 0 ? 0 : (y >= image.height ? image.height - 1 : y);
    for (int32_t x = static_cast<int32_t>(floorf(u0)); x < u1; ++x) {
      const float wx = fminf(u1, x + 1.0f) - fmaxf(u0, static_cast<float>(x));
      if (wx <= 0.0f) continue;
      int32_t xc = x < 0 ? 0 : (x >= image.width ? image.width - 1 : x);
      const uint8_t* t =
          image.bgra + (static_cast<size_t>(yc) * image.width + xc) * 4;
      const float weight = wx * wy;
      total += weight;
      for (int c = 0; c < 4; ++c) out[c] += weight * (t[c] / 255.0f);
    }
  }
  if (total > 0.0f) {
    for (int c = 0; c < 4; ++c) out[c] /= total;
  }
}

// A view of `columns` by `rows` pixels of the part `part` (x, y, width,
// height in texels of level 0) of a chain of `levels` images (level k is
// half of level k - 1). shift moves the view by a part of a pixel, as a
// small move of the head does. With more than one level the sampler picks
// the level of the scale and blends two levels (trilinear). With one level
// it reads level 0 only (bilinear). With 0 levels it takes the ideal sample
// of level 0 (Area). A pixel past the part is the background. out gets B, G, R
// over a background of `background` (0 to 1), from 0 to 1.
inline void View(const Image* chain, int levels, const float part[4],
                 float texels_per_pixel, float shift_x, float shift_y,
                 int32_t columns, int32_t rows, float background,
                 float* out) {
  float lod = levels > 1 ? log2f(texels_per_pixel) : 0.0f;
  if (lod < 0.0f) lod = 0.0f;
  if (levels > 0 && lod > static_cast<float>(levels - 1)) {
    lod = static_cast<float>(levels - 1);
  }
  const int k0 = static_cast<int>(lod);
  const int k1 = k0 + 1 < levels ? k0 + 1 : k0;
  const float blend = levels > 0 ? lod - static_cast<float>(k0) : 0.0f;
  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < columns; ++c) {
      float* o = out + (static_cast<size_t>(r) * columns + c) * 3;
      const float u = (c + 0.5f + shift_x) * texels_per_pixel;
      const float v = (r + 0.5f + shift_y) * texels_per_pixel;
      if (u < 0.0f || v < 0.0f || u >= part[2] || v >= part[3]) {
        o[0] = o[1] = o[2] = background;
        continue;
      }
      float s0[4], s1[4];
      if (levels == 0) {
        Area(chain[0], part[0] + u, part[1] + v, texels_per_pixel, s0);
        for (int i = 0; i < 4; ++i) s1[i] = s0[i];
      } else {
        const float scale0 = 1.0f / static_cast<float>(1 << k0);
        const float scale1 = 1.0f / static_cast<float>(1 << k1);
        Bilinear(chain[k0], (part[0] + u) * scale0, (part[1] + v) * scale0,
                 s0);
        Bilinear(chain[k1], (part[0] + u) * scale1, (part[1] + v) * scale1,
                 s1);
      }
      for (int i = 0; i < 3; ++i) {
        const float color = s0[i] + blend * (s1[i] - s0[i]);
        const float alpha = s0[3] + blend * (s1[3] - s0[3]);
        o[i] = color + background * (1.0f - alpha);
      }
    }
  }
}

// The alpha that the outer ring of pixels of a view shows: the largest
// alpha of a pixel of the view whose middle is within one pixel of the edge
// of the part. A clear edge gives 0.
inline float EdgeAlpha(const Image* chain, int levels, const float part[4],
                       float texels_per_pixel, float shift_x, float shift_y) {
  float lod = levels > 1 ? log2f(texels_per_pixel) : 0.0f;
  if (lod < 0.0f) lod = 0.0f;
  if (lod > static_cast<float>(levels - 1)) lod = static_cast<float>(levels - 1);
  const int k0 = static_cast<int>(lod);
  const int k1 = k0 + 1 < levels ? k0 + 1 : k0;
  const float blend = lod - static_cast<float>(k0);
  const int32_t columns = static_cast<int32_t>(part[2] / texels_per_pixel) + 1;
  const int32_t rows = static_cast<int32_t>(part[3] / texels_per_pixel) + 1;
  float most = 0.0f;
  for (int32_t r = 0; r < rows; ++r) {
    for (int32_t c = 0; c < columns; ++c) {
      const float u = (c + 0.5f + shift_x) * texels_per_pixel;
      const float v = (r + 0.5f + shift_y) * texels_per_pixel;
      if (u < 0.0f || v < 0.0f || u >= part[2] || v >= part[3]) continue;
      const bool outer = u < texels_per_pixel || v < texels_per_pixel ||
                         u >= part[2] - texels_per_pixel ||
                         v >= part[3] - texels_per_pixel;
      if (!outer) continue;
      float s0[4], s1[4];
      const float scale0 = 1.0f / static_cast<float>(1 << k0);
      const float scale1 = 1.0f / static_cast<float>(1 << k1);
      Bilinear(chain[k0], (part[0] + u) * scale0, (part[1] + v) * scale0, s0);
      Bilinear(chain[k1], (part[0] + u) * scale1, (part[1] + v) * scale1, s1);
      const float alpha = s0[3] + blend * (s1[3] - s0[3]);
      if (alpha > most) most = alpha;
    }
  }
  return most;
}

// THE SHIMMER of a view: how much the light of a part of the card changes
// when the head moves by a part of a pixel. A sampler that reads each texel
// with its share gives about the same light to a block of pixels at each
// shift, because the block holds the same ink. A sampler that skips texels
// hits a thin line of ink at one shift and misses it at the next, so the
// light of the block jumps. For each block of kShimmerBlock by
// kShimmerBlock pixels: the largest less the smallest mean of its light
// over `shifts` views, shifted by 1/shifts of a pixel across and down. The
// answer is the mean of that over the blocks, in steps of 1/255. Part of it
// comes from the move of the ink across the edges of the blocks: the ideal
// sampler (0 levels) gives that part alone. scratch
// holds a view (columns * rows * 3 floats), low and high a float for each
// block.
const int kShimmerBlock = 8;

inline void ShimmerSize(const float part[4], float texels_per_pixel,
                        int32_t* columns, int32_t* rows, int32_t* blocks) {
  *columns = static_cast<int32_t>(part[2] / texels_per_pixel);
  *rows = static_cast<int32_t>(part[3] / texels_per_pixel);
  const int32_t bc = (*columns - 2) / kShimmerBlock;
  const int32_t br = (*rows - 2) / kShimmerBlock;
  *blocks = bc > 0 && br > 0 ? bc * br : 0;
}

inline float Shimmer(const Image* chain, int levels, const float part[4],
                     float texels_per_pixel, int shifts, float background,
                     float* scratch, float* low, float* high) {
  int32_t columns = 0, rows = 0, blocks = 0;
  ShimmerSize(part, texels_per_pixel, &columns, &rows, &blocks);
  const int32_t bc = (columns - 2) / kShimmerBlock;
  if (blocks <= 0 || shifts < 2) return 0.0f;
  const int32_t br = blocks / bc;
  for (int32_t i = 0; i < blocks; ++i) {
    low[i] = 1e9f;
    high[i] = -1e9f;
  }
  for (int s = 0; s < shifts; ++s) {
    const float t = static_cast<float>(s) / static_cast<float>(shifts);
    View(chain, levels, part, texels_per_pixel, t, t, columns, rows,
         background, scratch);
    for (int32_t j = 0; j < br; ++j) {
      for (int32_t i = 0; i < bc; ++i) {
        float sum = 0.0f;
        for (int y = 0; y < kShimmerBlock; ++y) {
          const float* p =
              scratch + ((static_cast<size_t>(1 + j * kShimmerBlock + y)) *
                             columns +
                         (1 + i * kShimmerBlock)) * 3;
          for (int x = 0; x < kShimmerBlock; ++x, p += 3) {
            sum += (p[0] + p[1] + p[2]) / 3.0f;
          }
        }
        const float mean = sum / (kShimmerBlock * kShimmerBlock);
        float& lo = low[j * bc + i];
        float& hi = high[j * bc + i];
        if (mean < lo) lo = mean;
        if (mean > hi) hi = mean;
      }
    }
  }
  double total = 0.0;
  for (int32_t i = 0; i < blocks; ++i) total += high[i] - low[i];
  return static_cast<float>(total / blocks * 255.0);
}

}  // namespace card_send_math
