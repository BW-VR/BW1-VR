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

  // THE ROUTE: the controls take the rows in their order from the top, a
  // control takes one row, and no two parts of the leaders cross when Route
  // says so. Most sets of 6 or 7 controls on 7 rows have a route.
  {
    int routed = 0, wrong = 0;
    for (int trial = 0; trial < 300; ++trial) {
      const bool left = trial % 2 == 0;
      const int n = 6 + trial % 2;
      const int rows = 7;
      float anchors[lay::kMostBoxes][2], slot_y[lay::kMostBoxes];
      int col[2], boxes[2];
      lay::ModelColumn(left, &col[0], &col[1]);
      lay::BoxColumn(left, &boxes[0], &boxes[1]);
      for (int i = 0; i < n; ++i) {
        anchors[i][0] = random(col[0] + 20.0f, col[1] - 20.0f);
        anchors[i][1] = random(200.0f, 650.0f);
      }
      for (int r = 0; r < rows; ++r) slot_y[r] = lay::SlotMiddle(r, rows, 420.0f);
      const float start_x = static_cast<float>(left ? boxes[0] : boxes[1]);
      const float knee_x = start_x;
      int row_of[lay::kMostBoxes];
      float st[lay::kMostBoxes][2], kn[lay::kMostBoxes][2],
          en[lay::kMostBoxes][2];
      if (!lay::Route(anchors, n, knee_x, start_x, slot_y, rows, row_of, st,
                      kn, en)) {
        continue;
      }
      ++routed;
      for (int u = 0; u < n; ++u) {
        for (int w = 0; w < n; ++w) {
          if (u == w) continue;
          if (row_of[u] == row_of[w]) ++wrong;
          if (lay::Clash(st[u], kn[u], en[u], st[w], kn[w], en[w])) ++wrong;
        }
      }
    }
    Check(wrong == 0 && routed > 100,
          "in 300 sets of 6 or 7 controls, each routed set has one row for "
          "each control, and no leader crosses another or runs within 9 px "
          "of the ring of another", routed, 300);
    // THE OWNER'S CASE: on the left model the stick, the Y button on the
    // face, and the trigger at the side next to the boxes, a little lower
    // than Y. The stick takes the row at its height, so Y would take the
    // row of the trigger, and its line would run over the ring of the
    // trigger. So the trigger takes the row above the row of Y.
    const float owner[3][2] = {
        {320.0f, 405.0f}, {350.0f, 440.0f}, {428.0f, 452.0f}};
    float rows7[lay::kMostBoxes];
    for (int r = 0; r < 7; ++r) rows7[r] = 293.0f + 80.0f * r;
    int row3[3];
    float s3[3][2], k3[3][2], e3[3][2];
    const bool done = lay::Route(owner, 3, 453.0f, 453.0f, rows7, 7, row3,
                                 s3, k3, e3);
    Check(done && row3[2] < row3[1] &&
              !lay::Clash(s3[1], k3[1], e3[1], s3[2], k3[2], e3[2]),
          "the trigger at the side takes the row above Y, so the line to Y "
          "does not run over the ring of the trigger", row3[2], row3[1] - 1);
    int rows_l[4], rows_r[4];
    lay::BoxRect(true, 0, 7, 420.0f, rows_l);
    lay::BoxRect(false, 0, 7, 420.0f, rows_r);
    Check(rows_l[1] == rows_r[1] &&
              fabsf(lay::SlotMiddle(0, 7, 420.0f) -
                    (rows_l[1] + rows_l[3]) * 0.5f) < 0.01f,
          "the rows of the two sides are at the same heights", rows_l[1],
          rows_r[1]);
  }

  // THE PARTS of the page: a box of each side.
  bool left = false;
  int box = -1;
  Check(lay::PartOf(true, 3) == 3 && lay::PartOf(false, 2) == 10 &&
            lay::SideOfPart(10, &left, &box) && !left && box == 2 &&
            !lay::SideOfPart(16, &left, &box) &&
            2 * lay::kMostBoxes <= debugwin_math::kPageParts,
        "the parts of the boxes of the two sides", lay::PartOf(false, 2), 10);

  // THE VIEW: the eye is in front of the controller, above it and at its
  // inner side, and the two sides are mirrors.
  float r[3], l[3];
  lay::EyeDirection(false, r);
  lay::EyeDirection(true, l);
  Check(r[0] < 0.0f && r[1] > 0.0f && r[2] < 0.0f && l[0] == -r[0] &&
            fabsf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] - 1.0f) < 1e-5f,
        "the eye sees the right controller from the front, above and its "
        "inner side, and the left one as its mirror", r[0], -0.73);

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
