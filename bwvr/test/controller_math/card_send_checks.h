// THE EDGE AND THE MIP LEVELS OF A CARD (docs/ui-layer.md, "The edge and
// the mip levels of a card"). main.cpp includes this file after Check and
// Near, just before main.
#pragma once

#include <vector>

#include "../../src/vr/card_send_math.h"

namespace card_send_checks {

using namespace card_send_math;

// A test card: a picture of 768 by 320 texels with the card in the rect
// 0, 0, 300, 120. The card is opaque paper with lines of ink 1 texel wide,
// as the thin strokes of letters are. Past the rect the picture holds an
// older, wider card: opaque paper, as a swapchain image can hold.
const uint32_t kTextureWidth = 768;
const uint32_t kTextureHeight = 320;

inline void Paper(uint8_t* t) {
  t[0] = 150;
  t[1] = 200;
  t[2] = 220;
  t[3] = 255;
}

inline std::vector<uint8_t> TestPicture(const int32_t rect[4]) {
  std::vector<uint8_t> picture(kTextureWidth * kTextureHeight * 4, 0);
  for (uint32_t y = 0; y < kTextureHeight; ++y) {
    for (uint32_t x = 0; x < kTextureWidth; ++x) {
      uint8_t* t = &picture[(y * kTextureWidth + x) * 4];
      const bool in_rect = static_cast<int32_t>(x) >= rect[0] &&
                           static_cast<int32_t>(x) < rect[0] + rect[2] &&
                           static_cast<int32_t>(y) >= rect[1] &&
                           static_cast<int32_t>(y) < rect[1] + rect[3];
      if (!in_rect) {
        Paper(t);  // the older card
        continue;
      }
      const int32_t cx = static_cast<int32_t>(x) - rect[0];
      const int32_t cy = static_cast<int32_t>(y) - rect[1];
      // A clear edge of 3 texels, as the scroll has, then paper.
      if (cx < 3 || cy < 3 || cx >= rect[2] - 3 || cy >= rect[3] - 3) continue;
      Paper(t);
      // Lines of ink: a vertical line every 5 texels and a horizontal line
      // every 7, in the middle part, as a block of small letters.
      const bool ink = cx > 20 && cx < rect[2] - 20 && cy > 20 &&
                       cy < rect[3] - 20 && (cx % 5 == 0 || cy % 7 == 0);
      if (ink) {
        t[0] = 30;
        t[1] = 35;
        t[2] = 45;
      }
    }
  }
  return picture;
}

// The chain of images of a buffer of levels.
inline void ChainOf(const Layout& l, const uint8_t* buffer, Image* chain) {
  for (int k = 0; k < l.levels; ++k) {
    chain[k].bgra = buffer + LevelOffset(l, k);
    chain[k].width = l.box[2] >> k;
    chain[k].height = l.box[3] >> k;
  }
}

inline void CardSendChecks() {
  printf("\nThe edge and the mip levels of a card\n");

  const int32_t rect[4] = {0, 0, 300, 120};
  std::vector<uint8_t> picture = TestPicture(rect);

  // --- the layout -----------------------------------------------------------
  {
    uint32_t w = 0, h = 0;
    ChainSize(kTextureWidth, kTextureHeight, false, 1, &w, &h);
    Check(w == kTextureWidth && h == kTextureHeight,
          "with no clear edge and one level the swapchain is the size of the "
          "picture", w, kTextureWidth);
    Layout l;
    Check(LayoutOf(kTextureWidth, kTextureHeight, rect, false, 1, &l),
          "the layout of the old way", 0, 0);
    bool old = !l.padded && l.scale[0] == 1.0f && l.scale[1] == 1.0f;
    for (int i = 0; i < 4; ++i) {
      old = old && l.box[i] == rect[i] && l.image_rect[i] == rect[i];
    }
    Check(old, "the old way writes the rect and shows the rect", old, 1);
  }

  bool fits = true, framed = true, aligned = true, centered = true;
  const int32_t rects[][4] = {{0, 0, 300, 120},   {0, 0, 768, 320},
                              {5, 3, 401, 77},    {467, 199, 301, 121},
                              {13, 0, 1, 1},      {760, 316, 8, 4}};
  for (const auto& r : rects) {
    for (int edge = 0; edge < 2; ++edge) {
      for (int levels = 1; levels <= kMostLevels; ++levels) {
        if (edge == 0 && levels == 1) continue;
        uint32_t cw = 0, ch = 0;
        ChainSize(kTextureWidth, kTextureHeight, edge != 0, levels, &cw, &ch);
        Layout l;
        if (!LayoutOf(kTextureWidth, kTextureHeight, r, edge != 0, levels,
                      &l)) {
          fits = false;
          continue;
        }
        fits = fits && l.box[0] >= 0 && l.box[1] >= 0 &&
               l.box[0] + l.box[2] <= static_cast<int32_t>(cw) &&
               l.box[1] + l.box[3] <= static_cast<int32_t>(ch);
        // The rect of the quad is in the box, and with the clear edge the
        // apron is between the two.
        const int32_t apron = edge ? kApron : 0;
        fits = fits && l.image_rect[0] - apron >= l.box[0] &&
               l.image_rect[1] - apron >= l.box[1] &&
               l.image_rect[0] + l.image_rect[2] + apron <= l.box[0] + l.box[2] &&
               l.image_rect[1] + l.image_rect[3] + apron <= l.box[1] + l.box[3];
        if (edge) {
          framed = framed && l.content[0] - l.image_rect[0] == kFrame &&
                   l.content[1] - l.image_rect[1] == kFrame &&
                   l.image_rect[2] == r[2] + 2 * kFrame &&
                   l.image_rect[3] == r[3] + 2 * kFrame;
        }
        aligned = aligned && l.box[0] % kAlign == 0 && l.box[1] % kAlign == 0 &&
                  l.box[2] % kAlign == 0 && l.box[3] % kAlign == 0;
        // The quad grows by the same texels at each side, so its middle is
        // the middle of the picture: the pose of the card does not change.
        const float mid_quad = l.image_rect[0] + 0.5f * l.image_rect[2];
        const float mid_picture = l.content[0] + 0.5f * r[2];
        centered = centered && fabsf(mid_quad - mid_picture) < 1e-4f &&
                   fabsf(l.scale[0] * r[2] - l.image_rect[2]) < 1e-3f;
      }
    }
  }
  Check(fits, "each box is in its swapchain, and each rect of a quad is in "
        "its box with the apron", fits, 1);
  Check(framed, "with the clear edge the quad has 8 clear texels at each side "
        "of the picture", framed, 1);
  Check(aligned, "each box starts and ends on a multiple of 8, so each level "
        "is half of the one above", aligned, 1);
  Check(centered, "the quad grows the same at each side, so the picture keeps "
        "its place and size", centered, 1);
  {
    int32_t bad[4] = {700, 0, 100, 10};
    Layout l;
    Check(!LayoutOf(kTextureWidth, kTextureHeight, bad, true, 4, &l),
          "a rect past the picture has no layout", 0, 0);
  }
  Check(LevelsFor(1072, 1072, 4) == 4, "a panel of 1024 texels gets 4 levels",
        LevelsFor(1072, 1072, 4), 4);
  Check(LevelsFor(808, 360, 1) == 1, "with no mip levels wanted, one level",
        LevelsFor(808, 360, 1), 1);
  Check(LevelsFor(40, 1064, 4) == 3,
        "a side of 40 texels gets 3 levels, so no level is under 8 texels",
        LevelsFor(40, 1064, 4), 3);

  // --- the levels -------------------------------------------------------------
  Layout l;
  LayoutOf(kTextureWidth, kTextureHeight, rect, true, 4, &l);
  std::vector<uint8_t> buffer(LevelsBytes(l), 0xCD);
  int32_t y0 = 0, y1 = 0;
  Fill(l, picture.data(), kTextureWidth * 4, false, false, buffer.data(), &y0,
       &y1);
  Check(y0 == 0 && y1 == l.box[3], "a new layout fills the whole box", y1,
        l.box[3]);
  BuildLevels(l, buffer.data(), y0, y1);
  {
    // A texel of level 1 is the mean of its 2 by 2 texels of level 0.
    const uint8_t* l0 = buffer.data();
    const uint8_t* l1 = buffer.data() + LevelOffset(l, 1);
    bool mean = true;
    const int32_t w0 = l.box[2], w1 = l.box[2] / 2;
    for (int32_t y = 0; y < l.box[3] / 2; y += 3) {
      for (int32_t x = 0; x < w1; x += 5) {
        for (int c = 0; c < 4; ++c) {
          const int sum = l0[((2 * y) * w0 + 2 * x) * 4 + c] +
                          l0[((2 * y) * w0 + 2 * x + 1) * 4 + c] +
                          l0[((2 * y + 1) * w0 + 2 * x) * 4 + c] +
                          l0[((2 * y + 1) * w0 + 2 * x + 1) * 4 + c];
          mean = mean && l1[(y * w1 + x) * 4 + c] == (sum + 2) / 4;
        }
      }
    }
    Check(mean, "each texel of level 1 is the mean of 2 by 2 texels of level 0",
          mean, 1);
    // The frame and the apron are clear in each level: each texel of a level
    // outside the picture's share of it.
    bool clear = true, premultiplied = true;
    for (int k = 0; k < l.levels; ++k) {
      const uint8_t* t = buffer.data() + LevelOffset(l, k);
      const int32_t w = l.box[2] >> k, h = l.box[3] >> k;
      const int32_t x0 = (l.content[0] - l.box[0]) >> k;
      const int32_t y0k = (l.content[1] - l.box[1]) >> k;
      const int32_t x1 = (l.content[0] - l.box[0] + rect[2] + (1 << k) - 1) >> k;
      const int32_t y1k = (l.content[1] - l.box[1] + rect[3] + (1 << k) - 1) >> k;
      for (int32_t y = 0; y < h; ++y) {
        for (int32_t x = 0; x < w; ++x) {
          const uint8_t* p = t + (y * w + x) * 4;
          if ((x < x0 || x >= x1 || y < y0k || y >= y1k) && p[3] != 0) {
            clear = false;
          }
          if (p[0] > p[3] || p[1] > p[3] || p[2] > p[3]) premultiplied = false;
        }
      }
    }
    Check(clear, "the frame and the apron are clear in each level", clear, 1);
    Check(premultiplied, "each level keeps the color multiplied by the alpha",
          premultiplied, 1);
    Check(StraightTexels(l, buffer.data()) == 0,
          "a picture with the color multiplied by the alpha has no straight "
          "texel", StraightTexels(l, buffer.data()), 0);
    std::vector<uint8_t> straight = buffer;
    straight[((l.content[1] - l.box[1]) * l.box[2] + 40) * 4 + 0] = 200;
    straight[((l.content[1] - l.box[1]) * l.box[2] + 40) * 4 + 3] = 100;
    Check(StraightTexels(l, straight.data()) == 1,
          "a texel with more color than alpha counts as straight",
          StraightTexels(l, straight.data()), 1);
  }
  {
    // The same picture again changes nothing, and a change of one row
    // rebuilds a band with it. The levels are then the same as a new build.
    std::vector<uint8_t> again = buffer;
    Fill(l, picture.data(), kTextureWidth * 4, false, true, again.data(), &y0,
         &y1);
    Check(y0 == y1, "the same picture again changes no row", y1 - y0, 0);
    std::vector<uint8_t> changed = picture;
    for (int x = 40; x < 90; ++x) changed[(55 * kTextureWidth + x) * 4 + 1] = 99;
    Fill(l, changed.data(), kTextureWidth * 4, false, true, again.data(), &y0,
         &y1);
    const int32_t row = l.content[1] - l.box[1] + 55;
    Check(y0 <= row && row < y1 && y1 - y0 == kAlign,
          "a change of one row rebuilds one band of 8 rows", y1 - y0, kAlign);
    BuildLevels(l, again.data(), y0, y1);
    std::vector<uint8_t> fresh(LevelsBytes(l), 0);
    int32_t f0 = 0, f1 = 0;
    Fill(l, changed.data(), kTextureWidth * 4, false, false, fresh.data(), &f0,
         &f1);
    BuildLevels(l, fresh.data(), f0, f1);
    Check(again == fresh, "the levels after a change of one band are the same "
          "as the levels of a new build", again == fresh, 1);
    // With red first the bytes 0 and 2 swap.
    std::vector<uint8_t> swapped(LevelsBytes(l), 0);
    Fill(l, picture.data(), kTextureWidth * 4, true, false, swapped.data(),
         &f0, &f1);
    const size_t at = ((l.content[1] - l.box[1] + 10) * l.box[2] +
                       (l.content[0] - l.box[0]) + 10) * 4;
    Check(swapped[at] == 220 && swapped[at + 2] == 150,
          "a swapchain with red first gets red in byte 0", swapped[at], 220);
  }

  {
    // The cost of a new picture of a panel of 1024 by 1024 texels, the
    // largest card, on this PC: a new layout, and a change of one row.
    const int32_t panel_rect[4] = {0, 0, 1024, 1024};
    std::vector<uint8_t> panel(1024 * 1024 * 4);
    for (size_t i = 0; i < panel.size(); ++i) {
      panel[i] = static_cast<uint8_t>((i * 7) % 251);
    }
    for (size_t i = 3; i < panel.size(); i += 4) panel[i] = 255;
    Layout pl;
    LayoutOf(1024, 1024, panel_rect, true, 4, &pl);
    std::vector<uint8_t> levels(LevelsBytes(pl));
    LARGE_INTEGER hz, t0, t1, t2;
    QueryPerformanceFrequency(&hz);
    int32_t a = 0, b = 0;
    QueryPerformanceCounter(&t0);
    Fill(pl, panel.data(), 1024 * 4, false, false, levels.data(), &a, &b);
    BuildLevels(pl, levels.data(), a, b);
    QueryPerformanceCounter(&t1);
    for (int x = 0; x < 600; ++x) panel[(500 * 1024 + x) * 4] ^= 0x55;
    Fill(pl, panel.data(), 1024 * 4, false, true, levels.data(), &a, &b);
    BuildLevels(pl, levels.data(), a, b);
    QueryPerformanceCounter(&t2);
    const double ms = 1000.0 / static_cast<double>(hz.QuadPart);
    printf("  a new panel of 1024 by 1024 texels: %.2f ms for a new layout, "
           "%.2f ms for a change of one row, on this PC\n",
           (t1.QuadPart - t0.QuadPart) * ms, (t2.QuadPart - t1.QuadPart) * ms);
  }

  // --- the model of the compositor ----------------------------------------------
  // The old way: the swapchain holds the picture with the older card past
  // the rect, one level. The new way: the levels of the buffer.
  Image old_chain[1] = {{picture.data(), static_cast<int32_t>(kTextureWidth),
                         static_cast<int32_t>(kTextureHeight)}};
  const float old_part[4] = {0.0f, 0.0f, 300.0f, 120.0f};
  Image new_chain[kMostLevels];
  ChainOf(l, buffer.data(), new_chain);
  const float new_part[4] = {
      static_cast<float>(l.image_rect[0] - l.box[0]),
      static_cast<float>(l.image_rect[1] - l.box[1]),
      static_cast<float>(l.image_rect[2]), static_cast<float>(l.image_rect[3])};
  Image one_level[1] = {new_chain[0]};

  float edge_old = 0.0f, edge_new = 0.0f;
  const float scales[3] = {1.15f, 2.0f, 3.3f};
  for (float tpp : scales) {
    for (int s = 0; s < 4; ++s) {
      const float t = 0.25f * s;
      const float a = EdgeAlpha(old_chain, 1, old_part, tpp, t, t);
      const float b = EdgeAlpha(new_chain, l.levels, new_part, tpp, t, t);
      if (a > edge_old) edge_old = a;
      if (b > edge_new) edge_new = b;
    }
  }
  printf("  the edge of the quad: an alpha of %.2f at the most the old way, "
         "%.2f with the clear edge and the levels\n", edge_old, edge_new);
  Check(edge_old > 0.3f, "the old way shows the older card at the edge of the "
        "quad", edge_old, 0.3);
  Check(edge_new == 0.0f, "with the clear edge the outer pixels of the quad "
        "are clear at 1.15, 2 and 3.3 texels a pixel", edge_new, 0.0);

  for (float tpp : scales) {
    int32_t columns = 0, rows = 0, blocks = 0;
    ShimmerSize(new_part, tpp, &columns, &rows, &blocks);
    std::vector<float> scratch(static_cast<size_t>(columns + 2) * (rows + 2) * 3);
    std::vector<float> low(blocks + 1), high(blocks + 1);
    const float one = Shimmer(one_level, 1, new_part, tpp, 8, 0.5f,
                              scratch.data(), low.data(), high.data());
    const float mips = Shimmer(new_chain, l.levels, new_part, tpp, 8, 0.5f,
                               scratch.data(), low.data(), high.data());
    const float ideal = Shimmer(one_level, 0, new_part, tpp, 8, 0.5f,
                                scratch.data(), low.data(), high.data());
    printf("  the shimmer at %.2f texels a pixel: %.2f with one level, %.2f "
           "with %d levels, %.2f for the ideal sampler\n", tpp, one, mips,
           l.levels, ideal);
    if (tpp > 1.9f) {
      Check(mips - ideal < 0.5f * (one - ideal),
            "the levels at least halve the shimmer over the ideal when the "
            "card shows 2 or more times smaller than its picture",
            mips - ideal, one - ideal);
    } else {
      Check(mips <= one + 0.05f, "the levels add no shimmer near the size of "
            "the picture", mips, one);
    }
  }
}

}  // namespace card_send_checks
