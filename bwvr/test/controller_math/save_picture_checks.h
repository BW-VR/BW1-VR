// THE SAVE PICTURE IN VR (docs/backend.md, "The save picture"). main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/save_picture_math.h"

namespace save_picture_checks {

inline int MaxError(const uint8_t rgba[16][4], const uint8_t block[8]) {
  uint8_t back[16][3];
  save_picture::DecodeDxt1Block(block, back);
  int worst = 0;
  for (int i = 0; i < 16; ++i) {
    for (int k = 0; k < 3; ++k) {
      const int e = rgba[i][k] > back[i][k] ? rgba[i][k] - back[i][k]
                                            : back[i][k] - rgba[i][k];
      if (e > worst) worst = e;
    }
  }
  return worst;
}

inline void SavePictureChecks() {
  using namespace save_picture;
  printf("\nThe save picture in VR\n");

  // The part of the view.
  Rect r = Crop(2760, 2208, kShare);
  Near("the part of a Quest eye is 0.6 of its width", r.w, 1656, 0);
  Near("and 4 by 3", r.h, 1242, 0);
  Near("in the middle across", r.x, 552, 0);
  Near("and in the middle up and down", r.y, 483, 0);
  r = Crop(1000, 300, 1.0f);
  Near("a low picture gives all of its height", r.h, 300, 0);
  Near("and the width for 4 by 3", r.w, 400, 0);
  Near("in the middle", r.x, 300, 0);
  r = Crop(800, 600, 0.0f);
  Near("a share of 0 is all of the picture", r.w, 800, 0);
  Near("all of its height", r.h, 600, 0);
  r = Crop(0, 600, 0.5f);
  Near("no picture gives no part", r.w, 0, 0);

  // The shrink: each pixel the mean of its box.
  uint8_t src[8 * 4 * 4];
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 8; ++x) {
      uint8_t* p = src + (y * 8 + x) * 4;
      p[0] = static_cast<uint8_t>(x < 4 ? 10 : 200);  // the first byte
      p[1] = 100;
      p[2] = static_cast<uint8_t>(y < 2 ? 0 : 50);
      p[3] = 0;
    }
  }
  uint8_t out[2 * 2 * 4];
  const Rect all = {0, 0, 8, 4};
  Check(Shrink(src, 32, 8, 4, false, all, out, 2, 2),
        "a shrink of a whole picture works", 1, 1);
  Near("the left box has the mean of its left half", out[0], 10, 0);
  Near("the right box the mean of its right half", out[4], 200, 0);
  Near("the green stays", out[1], 100, 0);
  Near("the red of the low boxes", out[2 * 4 + 2], 50, 0);
  Near("each pixel is opaque", out[3], 255, 0);
  Check(Shrink(src, 32, 8, 4, true, all, out, 2, 2),
        "a shrink of RGBA works", 1, 1);
  Near("with red first, the first byte is the red", out[2], 10, 0);
  Near("and the blue comes from the third byte", out[2 * 4], 50, 0);
  const Rect odd = {2, 1, 4, 2};
  Check(Shrink(src, 32, 8, 4, false, odd, out, 1, 1),
        "a shrink of a part works", 1, 1);
  Near("the box over the edge has the mean of both sides", out[0], 105, 0);
  const Rect outside = {6, 0, 4, 4};
  Check(!Shrink(src, 32, 8, 4, false, outside, out, 1, 1),
        "a part out of the picture is refused", 1, 1);
  Check(!Shrink(src, 16, 8, 4, false, all, out, 1, 1),
        "a pitch smaller than a row is refused", 1, 1);
  uint8_t big[16 * 8 * 4];
  Check(Shrink(src, 32, 8, 4, false, all, big, 16, 8),
        "a box of less than a pixel takes the pixel at its start", 1, 1);
  Near("so a grow keeps the left half", big[0], 10, 0);
  Near("and the right half", big[15 * 4], 200, 0);

  float light = 0.0f, dark = 0.0f;
  uint8_t pixels[2 * 4] = {0, 0, 0, 255, 255, 255, 255, 255};
  Measure(pixels, 2, &light, &dark);
  Near("a black and a white pixel have half light", light, 0.5, 1e-6);
  Near("and one of two is black", dark, 0.5, 1e-6);

  // The DXT1 blocks.
  uint8_t flat[16][4];
  for (int i = 0; i < 16; ++i) {
    flat[i][0] = 120;
    flat[i][1] = 60;
    flat[i][2] = 200;
    flat[i][3] = 255;
  }
  uint8_t block[8];
  EncodeDxt1Block(flat, block);
  const uint16_t c0 = static_cast<uint16_t>(block[0] | block[1] << 8);
  const uint16_t c1 = static_cast<uint16_t>(block[2] | block[3] << 8);
  Check(c0 > c1, "a block of one color has four colors (c0 > c1)", c0, c1);
  Near("and comes back within the step of 5 bits", MaxError(flat, block), 0, 4);
  uint8_t black[16][4] = {};
  EncodeDxt1Block(black, block);
  const uint16_t b0 = static_cast<uint16_t>(block[0] | block[1] << 8);
  const uint16_t b1 = static_cast<uint16_t>(block[2] | block[3] << 8);
  Check(b0 > b1, "a black block has four colors too", b0, b1);
  Near("and comes back black", MaxError(black, block), 0, 0);
  uint8_t ramp[16][4];
  for (int i = 0; i < 16; ++i) {
    ramp[i][0] = static_cast<uint8_t>(i * 17);
    ramp[i][1] = static_cast<uint8_t>(255 - i * 17);
    ramp[i][2] = 128;
    ramp[i][3] = 255;
  }
  EncodeDxt1Block(ramp, block);
  Check(MaxError(ramp, block) <= 48,
        "a ramp from red to green comes back within 48 of 255",
        MaxError(ramp, block), 48);
  uint8_t two[16][4];
  for (int i = 0; i < 16; ++i) {
    const bool white = (i & 1) != 0;
    two[i][0] = two[i][1] = two[i][2] = static_cast<uint8_t>(white ? 255 : 0);
    two[i][3] = 255;
  }
  EncodeDxt1Block(two, block);
  Near("a block of black and white comes back within 16",
       MaxError(two, block), 0, 16);
  uint8_t back[16][3];
  DecodeDxt1Block(block, back);
  Check(back[0][0] < 64 && back[1][0] > 192,
        "and each pixel keeps its side", back[0][0], back[1][0]);

  // The fit of least squares (vk_dxt_fit).
  uint8_t fit[8];
  FitDxt1Block(two, fit);
  Near("the fit gives a block of black and white back exactly",
       MaxError(two, fit), 0, 0);
  FitDxt1Block(flat, fit);
  Near("the fit of one color comes back within the step of 5 bits",
       MaxError(flat, fit), 0, 4);
  FitDxt1Block(ramp, fit);
  EncodeDxt1Block(ramp, block);
  Check(BlockError(ramp, fit) <= BlockError(ramp, block),
        "the fit of a ramp is not worse than the box", BlockError(ramp, fit),
        BlockError(ramp, block));
  // Blocks of the kind of a picture: a gradient with noise, in many forms.
  uint32_t seed = 12345u;
  int worse = 0, fit_c0 = 0;
  double sum_fit = 0.0, sum_box = 0.0;
  for (int b = 0; b < 2000; ++b) {
    uint8_t px[16][4];
    seed = seed * 1664525u + 1013904223u;
    const int base[3] = {static_cast<int>(seed >> 24),
                         static_cast<int>((seed >> 16) & 0xFF),
                         static_cast<int>((seed >> 8) & 0xFF)};
    seed = seed * 1664525u + 1013904223u;
    const int slope = static_cast<int>(seed >> 27) - 16;
    for (int i = 0; i < 16; ++i) {
      seed = seed * 1664525u + 1013904223u;
      const int noise = static_cast<int>(seed >> 28) - 8;
      for (int k = 0; k < 3; ++k) {
        int v = base[k] + slope * ((i % 4) + (i / 4)) * (k + 1) / 2 + noise;
        v = v < 0 ? 0 : v > 255 ? 255 : v;
        px[i][k] = static_cast<uint8_t>(v);
      }
      px[i][3] = 255;
    }
    EncodeDxt1Block(px, block);
    FitDxt1Block(px, fit);
    const int e_box = BlockError(px, block), e_fit = BlockError(px, fit);
    if (e_fit > e_box) ++worse;
    const uint16_t f0 = static_cast<uint16_t>(fit[0] | fit[1] << 8);
    const uint16_t f1 = static_cast<uint16_t>(fit[2] | fit[3] << 8);
    if (f0 > f1) ++fit_c0;
    sum_box += e_box;
    sum_fit += e_fit;
  }
  Near("the fit is never worse than the box in 2000 blocks", worse, 0, 0);
  Near("and each block of the fit has four colors (c0 > c1)", fit_c0, 2000, 0);
  Check(sum_fit < 0.9 * sum_box,
        "and the fit has at least 10 percent less squared error",
        static_cast<int>(sum_fit), static_cast<int>(sum_box));

  // The size of the read of the game (save_picture_fit).
  int w = 0, h = 0;
  ReadSize(false, true, &w, &h);
  Near("a save reads 256 points across", w, 256, 0);
  Near("and 128 down", h, 128, 0);
  ReadSize(true, true, &w, &h);
  Near("a full picture reads 400 points across", w, 400, 0);
  Near("and 300 down", h, 300, 0);
  ReadSize(false, false, &w, &h);
  Near("without the fit the surface is 512 across", w, kWidth, 0);
  Near("and 384 down", h, kHeight, 0);
}

}  // namespace save_picture_checks
