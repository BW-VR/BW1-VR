// THE DEBUG WINDOW (docs/ui-layer.md, "The debug window"), and the list that
// scrolls of the panel kit. main.cpp includes this file after Check and
// Near, just before main.
#pragma once

#include <math.h>

#include "../../src/mod/debug_window_math.h"

namespace debug_window_checks {

inline bool Overlap(const int a[4], const int b[4]) {
  return a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3];
}

inline void DebugWindowChecks() {
  using namespace debugwin_math;
  namespace pk = panel_kit_math;
  printf("\nThe debug window\n");

  // THE LIST THAT SCROLLS (panel_kit_math.h).
  Check(pk::MaxOffset(500, 760) == 0 && pk::MaxOffset(1824, 760) == 1064,
        "a list that fits has no scroll, and a longer one scrolls by the rest",
        pk::MaxOffset(1824, 760), 1064);
  Check(pk::ClampOffset(-5, 1824, 760) == 0 &&
            pk::ClampOffset(5000, 1824, 760) == 1064 &&
            pk::ClampOffset(300, 1824, 760) == 300,
        "the offset stays from 0 to the largest offset",
        pk::ClampOffset(5000, 1824, 760), 1064);
  {
    int top = -1, length = -1;
    pk::Thumb(1824, 760, 0, 760, 56, &top, &length);
    const int want = 760 * 760 / 1824;
    Check(top == 0 && length == want,
          "the thumb is the share of the list in the view, at the top for "
          "offset 0",
          length, want);
    pk::Thumb(1824, 760, 1064, 760, 56, &top, &length);
    Check(top + length == 760, "at the last offset the thumb is at the bottom",
          top + length, 760);
    pk::Thumb(100000, 760, 0, 760, 56, &top, &length);
    Check(length == 56, "a very long list has a thumb of the least length",
          length, 56);
    pk::Thumb(500, 760, 0, 760, 56, &top, &length);
    Check(top == 0 && length == 760, "a list that fits fills the track",
          length, 760);
    // The drag is the other way of the thumb.
    bool round = true;
    int worst = 0;
    for (int o = 0; o <= 1064; o += 7) {
      pk::Thumb(1824, 760, o, 760, 56, &top, &length);
      const int back = pk::OffsetOfThumb(1824, 760, 760, 56, top);
      const int miss = back > o ? back - o : o - back;
      if (miss > worst) worst = miss;
      // One pixel of the thumb is 1064 / (760 - 316) pixels of the list.
      if (miss > 3) round = false;
    }
    Check(round,
          "the offset of the place of the thumb is the offset of the thumb, "
          "to 3 pixels",
          worst, 0);
    Check(pk::OffsetOfThumb(1824, 760, 760, 56, -40) == 0 &&
              pk::OffsetOfThumb(1824, 760, 760, 56, 2000) == 1064,
          "a thumb dragged past an end stops at that end",
          pk::OffsetOfThumb(1824, 760, 760, 56, 2000), 1064);
  }
  // THE STICK: no scroll at rest, the speed grows with the push, forward
  // goes up the list.
  Check(pk::StickScroll(0.2f, 0.25f, 1400.0f, 0.016f) == 0.0f &&
            pk::StickScroll(-0.2f, 0.25f, 1400.0f, 0.016f) == 0.0f,
        "a push below the rest does not scroll", 0, 0);
  Near("a full push forward for 0.5 s goes 700 pixels up the list",
       pk::StickScroll(1.0f, 0.25f, 1400.0f, 0.5f), -700.0, 0.01);
  Near("a full push back for 0.5 s goes 700 pixels down the list",
       pk::StickScroll(-1.0f, 0.25f, 1400.0f, 0.5f), 700.0, 0.01);
  Check(fabsf(pk::StickScroll(0.6f, 0.25f, 1400.0f, 0.1f)) <
            fabsf(pk::StickScroll(0.9f, 0.25f, 1400.0f, 0.1f)),
        "a larger push scrolls faster", 1, 1);

  // THE KINDS OF A ROW.
  Check(WordCount(L"off|on") == 2 && WordCount(L"") == 0 &&
            WordCount(L"a|b|c|d|e") == 5 && WordCount(nullptr) == 0,
        "the count of the words of the values", WordCount(L"a|b|c|d|e"), 5);
  Check(KindOfRow(2, false) == kRowToggle && KindOfRow(3, false) == kRowArrows &&
            KindOfRow(0, false) == kRowArrows && KindOfRow(2, true) == kRowDim,
        "two values give a toggle, more values or a number give arrows, and a "
        "switch that cannot change is dim",
        KindOfRow(0, false), kRowArrows);

  // THE LAYOUT. The tabs, the list, the bar and the bottom row are on the
  // paper and apart. The parts of a row are in the row and apart.
  {
    bool fits = true, apart = true;
    int close[4], cbox[4], note[4];
    CloseRect(close);
    pk::RectBox(close, 3, kWidth, kHeight, cbox);
    NoteBox(note);
    const int well[4] = {kWellLeft, kWellTop, kWellRight, kWellBottom};
    const int bar[4] = {kBarLeft - 3, kWellTop - 3, kBarRight + 3,
                        kWellBottom + 3};
    for (int g = 0; g < kMostTabs - 1; ++g) {
      int r[4], b[4];
      TabRect(g, r);
      pk::RectBox(r, 3, kWidth, kHeight, b);
      if (r[0] < 30 || r[1] < 30 || b[3] > kBottomTop - 4) fits = false;
      if (Overlap(b, well) || Overlap(b, bar) || Overlap(b, cbox)) {
        apart = false;
      }
      for (int h = g + 1; h < kMostTabs - 1; ++h) {
        int s[4], t[4];
        TabRect(h, s);
        pk::RectBox(s, 3, kWidth, kHeight, t);
        if (Overlap(b, t)) apart = false;
      }
    }
    if (Overlap(well, bar) || Overlap(well, cbox) || Overlap(note, cbox) ||
        kWellBottom + 4 > kBottomTop || cbox[2] > kWidth - 20 ||
        cbox[3] > kHeight - 20) {
      apart = false;
    }
    Check(fits && apart,
          "9 tabs, the list, the scroll bar, the note and Close are on the "
          "paper, and no two of them overlap",
          apart, 1);
    int l[4], ra[4], la[4], v[4], p[4], t[4];
    LabelBox(l);
    LeftArrowRect(la);
    RightArrowRect(ra);
    ValueBox(kRowArrows, v);
    PillRect(p);
    ValueBox(kRowToggle, t);
    const bool row_ok =
        l[2] <= la[0] && la[0] + la[2] + 3 <= v[0] && v[2] + 3 <= ra[0] &&
        ra[0] + ra[2] + 3 <= kWellWidth && t[2] + 3 <= p[0] &&
        p[0] + p[2] + 3 <= kWellWidth && la[1] >= 3 &&
        la[1] + la[3] + 3 <= kRowPitch - 3 && p[1] + p[3] + 3 <= kRowPitch - 3;
    Check(row_ok,
          "in a row, the label, the arrows, the value and the slot of a toggle "
          "are apart, with their rims, above the rule at the foot",
          row_ok, 1);
    Check(kView == 760 && kWellBottom == 872 && kViewRows == 10,
          "the view shows 10 rows of 76 pixels", kView, 760);
  }

  // THE PART UNDER THE RAY.
  {
    const int rows = 24;
    unsigned char kinds[kMostRows];
    for (int r = 0; r < kMostRows; ++r) {
      kinds[r] = static_cast<unsigned char>(r % 3);
    }
    bool all = true;
    for (int part = 0; part < kParts; ++part) {
      const int r = RowOf(part);
      if (part >= 9 && part < kFirstRowPart && part != kPartClose &&
          part != kPartThumb && part != kPartTrack) {
        continue;
      }
      if (r >= rows) continue;
      for (int offset = 0; offset <= 1064; offset += 266) {
        float u = 0.0f, v = 0.0f;
        if (!PartMiddle(part, 9, rows, kinds, offset, &u, &v)) continue;
        if (PartAt(u, v, 9, rows, kinds, offset) != part) all = false;
      }
    }
    Check(all,
          "the middle of each part in the view finds that part, at each "
          "offset",
          all, 1);
    float u = 0.0f, v = 0.0f;
    Check(!PartMiddle(RowPartOf(15, kOnRow), 9, rows, kinds, 0, &u, &v) &&
              PartMiddle(RowPartOf(15, kOnRow), 9, rows, kinds, 6 * kRowPitch,
                         &u, &v),
          "a row below the view is not there, and after a scroll it is", 1,
          1);
    // A row two rows down the view, with the list 30 pixels up.
    const float y = (kWellTop + 2 * kRowPitch + 40 - 30 + 0.5f) / kHeight;
    const float x = (kWellLeft + 100.0f) / kWidth;
    Check(PartAt(x, y, 9, rows, kinds, 30) == RowPartOf(2, kOnRow),
          "the ray finds the row under it at an offset of the list",
          PartAt(x, y, 9, rows, kinds, 30), RowPartOf(2, kOnRow));
    // A toggle row has no arrows: its control area is the row.
    int a[4];
    LeftArrowRect(a);
    const float ax = (kWellLeft + a[0] + 10.0f) / kWidth;
    const float ay = (kWellTop + a[1] + 10.0f) / kHeight;
    Check(PartAt(ax, ay, 9, rows, kinds, 0) == RowPartOf(0, kOnRow) &&
              PartAt(ax, (kWellTop + kRowPitch + a[1] + 10.0f) / kHeight, 9,
                     rows, kinds, 0) == RowPartOf(1, kOnLeft),
          "the place of a left arrow is the row in a toggle row, and the "
          "arrow in a row of arrows",
          PartAt(ax, ay, 9, rows, kinds, 0), RowPartOf(0, kOnRow));
    Check(PartAt((kBarLeft + 10.0f) / kWidth, (kWellTop + 10.0f) / kHeight, 9,
                 rows, kinds, 0) == kPartThumb &&
              PartAt((kBarLeft + 10.0f) / kWidth,
                     (kWellBottom - 10.0f) / kHeight, 9, rows, kinds, 0) ==
                  kPartTrack &&
              PartAt((kBarLeft + 10.0f) / kWidth,
                     (kWellBottom - 10.0f) / kHeight, 9, 5, kinds, 0) == -1,
          "the thumb and the track are under the ray, and a list that fits "
          "has no bar",
          1, 1);
    Check(PartAt(1.2f, 0.5f, 9, rows, kinds, 0) == -1 &&
              PartAt((kWellLeft + 100.0f) / kWidth,
                     (kWellTop + 6 * kRowPitch) / static_cast<float>(kHeight),
                     9, 5, kinds, 0) == -1,
          "off the panel, and below the last row, there is no part", 1, 1);
  }

  // THE DRAG AND THE TRACK.
  {
    const int rows = 24;
    int track[4], thumb[4];
    BarBoxes(rows, 0, track, thumb);
    const int grab = (thumb[3] - thumb[1]) / 2;
    const int down = DragOffset(rows, thumb[1] + grab + 100.0f, grab);
    BarBoxes(rows, down, track, thumb);
    Check(thumb[1] >= kWellTop + 99 && thumb[1] <= kWellTop + 101,
          "a drag of 100 pixels moves the thumb 100 pixels", thumb[1] - kWellTop,
          100);
    Check(DragOffset(rows, 2000.0f, grab) ==
              pk::MaxOffset(ContentOf(rows), kView),
          "a drag past the bottom shows the last row",
          DragOffset(rows, 2000.0f, grab), pk::MaxOffset(ContentOf(rows), kView));
    Check(PageOffset(rows, 0, kWellBottom - 5.0f) == kView - kRowPitch &&
              PageOffset(rows, kView - kRowPitch, kWellTop + 2.0f) == 0,
          "a press on the track goes one view less one row down, and back up",
          PageOffset(rows, 0, kWellBottom - 5.0f), kView - kRowPitch);
    Check(OffsetToShow(12, rows, 0) == 13 * kRowPitch - kView &&
              OffsetToShow(1, rows, 5 * kRowPitch) == kRowPitch &&
              OffsetToShow(3, rows, 0) == 0,
          "the offset that shows a row moves the least", 1, 1);
  }

  // THE NOTE: in the list, below the row when it fits, else above it.
  {
    int box[4];
    NotePlace(RowTop(1, 0), 200, box);
    Check(box[1] == kWellTop + 2 * kRowPitch && box[3] == box[1] + 200 &&
              box[0] >= kWellLeft && box[2] <= kWellRight,
          "the note of a row near the top goes below it, in the list", box[1],
          kWellTop + 2 * kRowPitch);
    NotePlace(RowTop(9, 0), 200, box);
    Check(box[3] == kWellTop + 9 * kRowPitch && box[1] >= kWellTop,
          "the note of the last row goes above it", box[3],
          kWellTop + 9 * kRowPitch);
    bool inside = true;
    for (int r = 0; r < 10; ++r) {
      for (int h = 80; h <= kNoteMost; h += 40) {
        NotePlace(RowTop(r, 0), h, box);
        if (box[1] < kWellTop || box[3] > kWellBottom) inside = false;
        const int top = RowTop(r, 0);
        if (box[1] < top + kRowPitch && top < box[3]) inside = false;
      }
    }
    Check(inside,
          "each note of up to half the view is in the list and does not cover "
          "its row",
          inside, 1);
  }

  // THE TEST KEY.
  {
    int parts[8];
    const int n = ParseParts("tab2, right0 row13 left5 thumb trackend close "
                             "row99 nothing",
                             parts, 8);
    Check(n == 7 && parts[0] == 2 && parts[1] == RowPartOf(0, kOnRight) &&
              parts[2] == RowPartOf(13, kOnRow) &&
              parts[3] == RowPartOf(5, kOnLeft) && parts[4] == kPartThumb &&
              parts[5] == kPartTrack && parts[6] == kPartClose,
          "the words of debug_test_parts, and a word that is not known is not "
          "in the list",
          n, 7);
  }

  // THE PLACE is the place of the panel of places.
  Check(kDistance == 1.0f && kMeters == 0.56f &&
            fabsf(kPitch + 12.0f * 3.14159265f / 180.0f) < 0.0001f,
        "the window is 1 m out, 12 degrees down and 56 cm across", kMeters,
        0.56);
}

}  // namespace debug_window_checks
