// THE SPELL BOOK (docs/miracles.md, "The spell book").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/spell_book_math.h"

namespace spell_book_checks {

inline void SpellBookChecks() {
  using namespace spell_book_math;
  printf("\nThe spell book\n");

  // THE LAYOUT. Each count from 0 to the most fits the texture, the cells do
  // not overlap, and each item is inside the book.
  {
    bool fits = true, inside = true, apart = true, rows = true;
    for (int n = 0; n <= kMaxItems; ++n) {
      const Layout l = LayoutFor(n);
      if (PixelsOf(l.width_m) > kTexture || PixelsOf(l.height_m) > kTexture ||
          l.width_m * kPixelsPerMeter > kTexture ||
          l.height_m * kPixelsPerMeter > kTexture) {
        fits = false;
      }
      if (n > 0 && l.columns * l.rows < n) rows = false;
      if (n > 0 && (l.rows - 1) * l.columns >= n) rows = false;
      for (int i = 0; i < n; ++i) {
        float x = 0.0f, y = 0.0f;
        ItemCenter(l, i, &x, &y);
        const float half = 0.5f * l.cell_m;
        if (fabsf(x) + half > 0.5f * l.width_m + 1e-5f ||
            y + half > 0.5f * l.height_m - kTitleM + 1e-5f ||
            y - half < -0.5f * l.height_m - 1e-5f) {
          inside = false;
        }
        for (int k = i + 1; k < n; ++k) {
          float x2 = 0.0f, y2 = 0.0f;
          ItemCenter(l, k, &x2, &y2);
          const float d = sqrtf((x - x2) * (x - x2) + (y - y2) * (y - y2));
          if (d < l.cell_m - 1e-5f) apart = false;
        }
      }
    }
    Check(fits, "the book fits the texture of 1024 pixels at each count", fits,
          1);
    Check(rows, "the rows hold each item, with no empty row", rows, 1);
    Check(inside, "each cell is inside the book, under the title", inside, 1);
    Check(apart, "the middles of two items are at least one cell apart",
          apart, 1);
    const Layout three = LayoutFor(3);
    Check(three.columns == 3 && three.rows == 1,
          "three miracles are one row of three", three.columns, 3);
    const Layout thirty = LayoutFor(30);
    Check(thirty.columns == 6 && thirty.rows == 5,
          "the 30 miracles of the game are 6 columns of 5 rows",
          thirty.columns * 10 + thirty.rows, 65);
    Near("a cell of a book of 30 is 4 cm", thirty.cell_m, 0.04, 1e-6);
    const Layout none = LayoutFor(0);
    Check(none.count == 0 && none.width_m == kEmptyWidthM &&
              none.height_m == kEmptyHeightM,
          "a book with no miracle has the title and one line", none.count, 0);
    // The last row of 5 items in 3 columns has its 2 items in the middle.
    const Layout five = LayoutFor(5);
    float x3 = 0.0f, y3 = 0.0f, x4 = 0.0f, y4 = 0.0f;
    ItemCenter(five, 3, &x3, &y3);
    ItemCenter(five, 4, &x4, &y4);
    Near("the last row is in the middle of the book", x3 + x4, 0.0, 1e-6);
    Check(PixelsOf(0.1234f) % 2 == 0, "the pixels of a side are even",
          PixelsOf(0.1234f), 396);
  }

  // THE ORBS. The orb of each item is inside its cell at each count, and
  // its middle is 0.10 of a cell above the middle of the cell.
  {
    bool inside = true;
    for (int n = 1; n <= kMaxItems; ++n) {
      const Layout l = LayoutFor(n);
      for (int i = 0; i < n; ++i) {
        float cx = 0.0f, cy = 0.0f, ox = 0.0f, oy = 0.0f;
        ItemCenter(l, i, &cx, &cy);
        OrbCenter(l, i, &ox, &oy);
        const float r = kBubbleShare * l.cell_m;
        if (fabsf(ox - cx) + r > 0.5f * l.cell_m + 1e-6f ||
            oy + r > cy + 0.5f * l.cell_m + 1e-6f ||
            oy - r < cy - 0.5f * l.cell_m - 1e-6f) {
          inside = false;
        }
      }
    }
    Check(inside, "the orb of each item is inside its cell", inside, 1);
    const Layout l = LayoutFor(4);
    float cx = 0.0f, cy = 0.0f, ox = 0.0f, oy = 0.0f;
    ItemCenter(l, 2, &cx, &cy);
    OrbCenter(l, 2, &ox, &oy);
    Near("the orb is 0.10 of a cell above the middle of its cell, m",
         oy - cy, 0.10 * l.cell_m, 1e-6);
    Near("an orb in a cell of 6.5 cm is 4.3 cm across, cm",
         2.0 * kBubbleShare * l.cell_m * 100.0, 4.29, 1e-3);
  }

  // THE PLACE OF THE BOOK. With the grip pose of no turn, the book of the
  // left hand is to its right (the palm of the left hand faces +x), and the
  // book of the right hand is to its left.
  {
    const float grip[3] = {-0.2f, -0.3f, -0.35f};
    const float none[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float left[3], right[3];
    BookCenter(grip, none, true, left);
    BookCenter(grip, none, false, right);
    Near("the book of the left hand is 12 cm to the right of the grip",
         left[0] - grip[0], kPalmOutM, 1e-6);
    Near("the book of the right hand is 12 cm to the left of the grip",
         right[0] - grip[0], -kPalmOutM, 1e-6);
    Near("the book is 2 cm up from the grip", left[1] - grip[1], kPalmUpM,
         1e-6);
    // The palm turned up: a turn of the left hand by 90 degrees about +z
    // takes +x of the grip to +y.
    const float s = sinf(0.5f * 1.5707963f), c = cosf(0.5f * 1.5707963f);
    const float up[4] = {0.0f, 0.0f, s, c};
    float over[3];
    BookCenter(grip, up, true, over);
    Near("with the palm up the book is 12 cm over the palm", over[1] - grip[1],
         kPalmOutM, 1e-4);
    // The book moves with the hand, one to one.
    const float moved[3] = {grip[0] + 0.05f, grip[1] - 0.02f, grip[2] + 0.1f};
    float after[3];
    BookCenter(moved, none, true, after);
    Near("a step of the hand is the same step of the book",
         (after[0] - left[0]) + (after[1] - left[1]) * 10.0f +
             (after[2] - left[2]) * 100.0f,
         0.05 - 0.2 + 10.0, 1e-4);
  }

  // THE PLANE faces the head, with no roll, and a point on it comes back.
  {
    const float center[3] = {-0.08f, -0.28f, -0.35f};
    const float head[3] = {0.0f, 0.0f, 0.0f};
    Plane p;
    PlaneAt(center, head, &p);
    Near("the right of the book is level (no roll)", p.right[1], 0.0, 1e-5);
    const float to_head[3] = {head[0] - center[0], head[1] - center[1],
                              head[2] - center[2]};
    const float length = sqrtf(to_head[0] * to_head[0] +
                               to_head[1] * to_head[1] +
                               to_head[2] * to_head[2]);
    Near("the front of the book faces the head",
         (p.normal[0] * to_head[0] + p.normal[1] * to_head[1] +
          p.normal[2] * to_head[2]) / length,
         1.0, 1e-5);
    float point[3], back[3];
    PointOn(p, 0.03f, -0.02f, point);
    InPlane(p, point, back);
    Near("a point on the book comes back to its place",
         fabsf(back[0] - 0.03f) + fabsf(back[1] + 0.02f) + fabsf(back[2]), 0.0,
         1e-5);
  }

  // THE PICK.
  {
    const Layout l = LayoutFor(6);  // 3 columns, 2 rows, cells of 6.5 cm
    float x = 0.0f, y = 0.0f;
    ItemCenter(l, 4, &x, &y);
    const float at[3] = {x, y, 0.0f};
    Check(Pick(l, at, -1) == 4, "the hand at the middle of an item picks it",
          Pick(l, at, -1), 4);
    const float near_edge[3] = {x + 0.4f * l.cell_m, y, 0.01f};
    Check(Pick(l, near_edge, -1) == 4,
          "the hand 40 percent of a cell from the middle picks it",
          Pick(l, near_edge, -1), 4);
    const float border[3] = {x + 0.5f * l.cell_m, y, 0.0f};
    Check(Pick(l, border, -1) == -1,
          "the hand at the border of two items picks nothing new",
          Pick(l, border, -1), -1);
    Check(Pick(l, border, 4) == 4,
          "the item of the last frame stays at the border",
          Pick(l, border, 4), 4);
    float x5 = 0.0f, y5 = 0.0f;
    ItemCenter(l, 5, &x5, &y5);
    const float next[3] = {x5 - 0.1f * l.cell_m, y5, 0.0f};
    Check(Pick(l, next, 4) == 5,
          "the hand at the middle of the next item picks it",
          Pick(l, next, 4), 5);
    const float far_front[3] = {x, y, 0.07f};
    Check(Pick(l, far_front, -1) == -1,
          "a hand 7 cm in front of the book picks nothing",
          Pick(l, far_front, -1), -1);
    Check(Pick(l, far_front, 4) == 4,
          "the item of the last frame stays to 9 cm in front",
          Pick(l, far_front, 4), 4);
    const float away[3] = {x, y - 0.8f * l.cell_m, 0.0f};
    Check(Pick(l, away, 4) == -1, "a hand under the book lets the item go",
          Pick(l, away, 4), -1);
    const Layout empty = LayoutFor(0);
    const float zero[3] = {0.0f, 0.0f, 0.0f};
    Check(Pick(empty, zero, -1) == -1, "an empty book picks nothing",
          Pick(empty, zero, -1), -1);
  }

  // THE OPEN AND THE CLOSE.
  {
    bool open = false;
    OpenInput in = {};
    in.can_open = true;
    Check(OpenStep(&open, in) == kNothing && !open,
          "no press, no book", open, 0);
    in.press = true;
    Check(OpenStep(&open, in) == kOpened && open, "a press opens the book",
          open, 1);
    in.press = false;
    Check(OpenStep(&open, in) == kNothing && open, "the book stays open",
          open, 1);
    in.press = true;
    Check(OpenStep(&open, in) == kClosedButton && !open,
          "a second press closes the book", open, 0);
    OpenStep(&open, in);
    in.press = false;
    in.ring = true;
    Check(OpenStep(&open, in) == kClosedRing && !open,
          "the ring closes the book", open, 0);
    in.ring = false;
    in.press = true;
    OpenStep(&open, in);
    in.press = false;
    in.menu = true;
    Check(OpenStep(&open, in) == kClosedMenu && !open,
          "the game menu closes the book", open, 0);
    in.press = true;
    Check(OpenStep(&open, in) == kRefused && !open,
          "a press with the game menu up does not open the book", open, 0);
    in.menu = false;
    OpenStep(&open, in);
    in.press = false;
    in.plucked = true;
    Check(OpenStep(&open, in) == kClosedPluck && !open,
          "a pluck closes the book", open, 0);
    in.plucked = false;
    in.press = true;
    OpenStep(&open, in);
    in.press = false;
    in.can_open = false;
    Check(OpenStep(&open, in) == kClosedCannot && !open,
          "the book closes when it cannot stay", open, 0);
    in.press = true;
    Check(OpenStep(&open, in) == kRefused && !open,
          "a press in the Temple is refused", open, 0);
  }

  // THE GRIP ON THE BOOK.
  {
    GripClaim g = {};
    bool pluck = false;
    Check(!GripStep(&g, true, true, -1, &pluck) && !pluck,
          "a grip with no item picked stays the game's", pluck, 0);
    GripStep(&g, false, true, 2, &pluck);
    Check(GripStep(&g, true, true, 2, &pluck) && pluck,
          "a grip on a picked item is a pluck", pluck, 1);
    Check(GripStep(&g, true, false, -1, &pluck) && !pluck,
          "the book holds the grip after the book closes", pluck, 0);
    Check(!GripStep(&g, false, false, -1, &pluck),
          "an open grip is the game's again", 0, 0);
    Check(!GripStep(&g, true, false, 2, &pluck) && !pluck,
          "a grip with the book closed stays the game's", pluck, 0);
    GripClaim held = {};
    GripStep(&held, true, false, -1, &pluck);
    Check(!GripStep(&held, true, true, 2, &pluck) && !pluck,
          "a grip that was down before the book opened is no pluck", pluck, 0);
  }

  // THE PULL.
  {
    Check(PullShare(0.02f, 0.02f, 0.10f) == 0.0f,
          "the pull starts at 0 at the gap of the grip", 0, 0);
    Check(PullShare(0.01f, 0.02f, 0.10f) == 0.0f,
          "a hand nearer to the cell than at the grip pulls nothing", 0, 0);
    Check(fabsf(PullShare(0.07f, 0.02f, 0.10f) - 0.5f) < 1e-5f,
          "5 cm of a pull of 10 cm is half the pull", 0, 0);
    Check(PullShare(0.20f, 0.02f, 0.10f) == 1.0f,
          "the share stops at 1 past the pull length", 0, 0);
    Check(PullShare(0.0f, 0.0f, 0.0f) == 1.0f,
          "a pull length of 0 is a take at once", 0, 0);
    const float cell[3] = {0.0f, 0.0f, 0.0f};
    const float hand[3] = {0.10f, -0.04f, 0.02f};
    float bubble[3];
    PullBubble(cell, hand, bubble);
    Check(fabsf(bubble[0] - 0.05f) < 1e-6f && fabsf(bubble[1] + 0.02f) < 1e-6f &&
              fabsf(bubble[2] - 0.01f) < 1e-6f,
          "the bubble is half way from its cell to the hand", 0, 0);
    Check(PullBuzz(0.0f) == 0.0f && PullBuzz(1.0f) == 1.0f &&
              PullBuzz(0.5f) < 0.5f && PullBuzz(0.5f) > PullBuzz(0.25f),
          "the buzz goes up with the pull, faster near the end", 0, 0);
    Check(BackScale(0.0f) == 1.0f,
          "the bubble starts its way back at the place of the release", 0, 0);
    Check(BackScale(3.1415926f / kBackRate) < -0.2f,
          "the bubble goes through its cell one time", 0, 0);
    Check(fabsf(BackScale(BackLandSeconds())) < 1e-4f,
          "the bubble is at its cell at the land time", 0, 0);
    Check(BackScale(kBackSeconds) == 0.0f && BackScale(1.0f) == 0.0f,
          "the bubble stays at its cell after the spring", 0, 0);
    Check(fabsf(BackScale(kBackSeconds - 0.001f)) < 0.05f,
          "the spring is almost still at its end, so the stop is not seen", 0,
          0);
  }
  {
    // The hole of the game's bubble in the page. A page 40 cm in front of
    // the head, facing it.
    Plane page;
    const float center[3] = {0.0f, 0.0f, -0.40f};
    const float head[3] = {0.0f, 0.0f, 0.0f};
    PlaneAt(center, head, &page);
    Hole hole;
    const float on_page[3] = {0.05f, 0.02f, -0.40f};
    Check(BubbleHole(page, head, on_page, 0.03f, &hole) &&
              fabsf(hole.x - 0.05f) < 1e-4f && fabsf(hole.y - 0.02f) < 1e-4f &&
              fabsf(hole.r - 0.03f) < 1e-4f,
          "a ball on the page makes a hole of its own size at its place", 0, 0);
    const float near_head[3] = {0.05f, 0.0f, -0.20f};
    Check(BubbleHole(page, head, near_head, 0.03f, &hole) &&
              fabsf(hole.x - 0.10f) < 1e-4f &&
              fabsf(hole.r - (0.06f + kHalfEyesM)) < 1e-4f,
          "a ball half way to the head makes a hole two times as far out and "
          "as large, and larger for the two eyes", 0, 0);
    const float behind[3] = {0.0f, 0.0f, -0.45f};
    Check(!BubbleHole(page, head, behind, 0.03f, &hole),
          "a ball behind the page makes no hole", 0, 0);
    const float half_in[3] = {0.0f, 0.0f, -0.42f};
    Check(BubbleHole(page, head, half_in, 0.03f, &hole) && hole.r > 0.02f,
          "a ball with a part in front of the page makes a hole", 0, 0);
  }

  // THE RING ROUND A BUBBLE stays inside its cell at each size of a cell
  // (the cells of 4, 5 and 6.5 cm are 128 to 208 px), the gold ring is wider
  // than the thin ring, and the thin ring keeps the bubble as it is.
  {
    bool inside = true, thin = true, gold = true, fill = true;
    for (int cell = 100; cell <= 240; cell += 2) {
      const float c = static_cast<float>(cell);
      const float top = 40.0f;
      const float by = top + 0.5f * c - 0.10f * c;
      const float br = 0.33f * c;
      const Ring lit = BubbleRing(by, br, top, c, true);
      const Ring plain = BubbleRing(by, br, top, c, false);
      if (by - lit.outer < top + 4.0f - 1e-4f) inside = false;
      if (by - plain.outer < top) inside = false;
      if (lit.outer - lit.fill < 0.10f * c - 1e-4f ||
          lit.outer - lit.fill <= plain.outer - plain.fill) {
        gold = false;
      }
      if (lit.fill > br || lit.fill < 0.75f * br) fill = false;
      if (fabsf(plain.outer - (br + 0.06f * c)) > 1e-4f || plain.fill != br) {
        thin = false;
      }
    }
    Check(inside, "the ring of a bubble stays inside its cell", inside, 1);
    Check(gold, "the gold ring is 0.10 of a cell or more, wider than the "
          "thin ring", gold, 1);
    Check(fill, "the fill of the bubble under the hand keeps 3/4 of its "
          "radius or more", fill, 1);
    Check(thin, "the thin ring is 0.06 of a cell, and the bubble keeps its "
          "radius", thin, 1);
  }
}

}  // namespace spell_book_checks
