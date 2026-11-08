// THE PAGE CONTROLS WITH THE MODELS (docs/ui-layer.md, "The page Controls"):
// the columns of the page, the boxes, the leaders that never cross, the
// view and the fit of a model. main.cpp includes this file after Check and
// Near, just before main.
#pragma once

#include <math.h>
#include <stdlib.h>

#include "../../src/mod/controls_layout_math.h"
#include "../../src/mod/debug_window_math.h"

namespace controls_layout_checks {

inline void ControlsLayoutChecks() {
  namespace lay = controls_layout;
  printf("\nThe page Controls with the models\n");

  Check(lay::kPageWidth == debugwin_math::kPageWidth &&
            lay::kPageHeight == debugwin_math::kView,
        "the page is the box of the list and of the scroll bar",
        lay::kPageWidth, debugwin_math::kPageWidth);

  // THE COLUMNS: the left model, the left boxes, the right boxes and the
  // right model, apart and on the page.
  int lm[2], lb[2], rb[2], rm[2];
  lay::ModelColumn(true, &lm[0], &lm[1]);
  lay::BoxColumn(true, &lb[0], &lb[1]);
  lay::BoxColumn(false, &rb[0], &rb[1]);
  lay::ModelColumn(false, &rm[0], &rm[1]);
  Check(lm[0] >= 0 && lm[1] < lb[0] && lb[1] < rb[0] && rb[1] < rm[0] &&
            rm[1] <= lay::kPageWidth && lm[0] == lay::kPageWidth - rm[1],
        "the columns are in order, apart, on the page, and the two halves "
        "are mirrors", lm[0], lay::kPageWidth - rm[1]);

  // THE BOXES: 7 boxes stay in the box of the models, apart, and their
  // middle follows the middle of the model.
  int first[4], last[4];
  lay::BoxRect(true, 0, 7, 400.0f, first);
  lay::BoxRect(true, 6, 7, 400.0f, last);
  Check(first[1] >= lay::kModelTop && last[3] <= lay::kModelBottom &&
            fabsf((first[1] + last[3]) * 0.5f - 400.0f) <= 1.0f &&
            lay::kBoxPitch > lay::kBoxHeight,
        "7 boxes keep apart, in the column, round the middle of the model",
        (first[1] + last[3]) * 0.5f, 400.0f);
  lay::BoxRect(true, 0, 7, 100.0f, first);
  lay::BoxRect(true, 6, 7, 100.0f, last);
  Check(first[1] == lay::kModelTop && last[3] <= lay::kModelBottom,
        "the boxes of a model high on the page stay under the headers",
        first[1], lay::kModelTop);

  // THE LEADERS. For many sets of points, the order of the least length has
  // no two leaders that cross.
  bool none_cross = true;
  bool permutation = true;
  unsigned seed = 12345u;
  const auto random = [&seed](float lo, float hi) {
    seed = seed * 1664525u + 1013904223u;
    return lo + (hi - lo) * static_cast<float>((seed >> 8) & 0xFFFF) /
                    65535.0f;
  };
  for (int trial = 0; trial < 300; ++trial) {
    const bool left = trial % 2 == 0;
    const int n = 2 + trial % 6;
    float anchors[lay::kMostBoxes][2], starts[lay::kMostBoxes][2];
    int col[2];
    lay::ModelColumn(left, &col[0], &col[1]);
    for (int i = 0; i < n; ++i) {
      anchors[i][0] =
          random(static_cast<float>(col[0]), static_cast<float>(col[1]));
      anchors[i][1] = random(200.0f, 600.0f);
      int r[4];
      lay::BoxRect(left, i, n, 400.0f, r);
      lay::LeaderStart(left, r, &starts[i][0], &starts[i][1]);
    }
    int order[lay::kMostBoxes];
    lay::LeaderOrder(anchors, starts, n, order);
    bool used[lay::kMostBoxes] = {};
    for (int i = 0; i < n; ++i) {
      if (order[i] < 0 || order[i] >= n || used[order[i]]) {
        permutation = false;
      } else {
        used[order[i]] = true;
      }
    }
    for (int i = 0; i < n && permutation; ++i) {
      for (int j = i + 1; j < n; ++j) {
        if (lay::Cross(anchors[i], starts[order[i]], anchors[j],
                       starts[order[j]])) {
          none_cross = false;
        }
      }
    }
  }
  Check(permutation && none_cross,
        "in 300 sets of 2 to 7 controls, each box has one control and no "
        "two leaders cross", none_cross, 1);
  const float a[2] = {0, 0}, b[2] = {10, 10}, c[2] = {0, 10}, d[2] = {10, 0};
  const float e[2] = {20, 20};
  Check(lay::Cross(a, b, c, d) && !lay::Cross(a, b, b, e) &&
            !lay::Cross(a, c, b, d),
        "two lines that cross, lines with a shared end, and lines apart", 0,
        0);

  // THE PARTS of the page: a box of each side.
  bool left = false;
  int box = -1;
  Check(lay::PartOf(true, 3) == 3 && lay::PartOf(false, 2) == 10 &&
            lay::SideOfPart(10, &left, &box) && !left && box == 2 &&
            !lay::SideOfPart(16, &left, &box) &&
            2 * lay::kMostBoxes <= debugwin_math::kPageParts,
        "the parts of the boxes of the two sides", lay::PartOf(false, 2), 10);

  // THE VIEW: the eye is above the controller and at its inner side, and
  // the two sides are mirrors.
  float r[3], l[3];
  lay::EyeDirection(false, r);
  lay::EyeDirection(true, l);
  Check(r[0] < 0.0f && r[1] > 0.0f && r[2] == 0.0f && l[0] == -r[0] &&
            fabsf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] - 1.0f) < 1e-5f,
        "the eye sees the right controller from above and its inner side, "
        "and the left one as its mirror", r[0], -0.65);

  // THE FIT of a picture into the box of a model: it fills the box on one
  // side, with its margin, in the middle.
  const float extent[4] = {-1.0f, -2.0f, 1.0f, 2.0f};
  const int model[4] = {0, 100, 200, 700};
  float fit[3];
  lay::Fit(extent, model, 6.0f, fit);
  Check(fabsf(fit[0] - 94.0f) < 1e-3f && fabsf(fit[1] - 100.0f) < 1e-3f &&
            fabsf(fit[2] - 400.0f) < 1e-3f,
        "a picture twice as high as wide fits the width of the box, in the "
        "middle", fit[0], 94);
}

}  // namespace controls_layout_checks
