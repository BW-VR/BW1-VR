// The layout of the page Controls of the settings window with the models of
// the controllers (controls_page_models=1): the place of each model and of
// each box of a control, and the leaders from the controls to their boxes.
// See controls_page.cpp and docs/ui-layer.md, "The page Controls". No
// Windows and no game here, so the math test compiles this file
// (controls_layout_checks.h).
//
// THE PAGE is the box of the list and of the scroll bar of the window
// (debug_window_math.h, kPageWidth by kView pixels). Each controller has a
// half of the page. Its model is at the outer side, and the boxes of its
// controls are in a column at the inner side, so the two columns of boxes
// are side by side in the middle of the page:
//
//   | left model | left boxes | right boxes | right model |
//
// The model is seen as a player knows it: from the front, from above and
// from its inner side, with the face buttons and the stick toward the eye,
// and the trigger and the grip (at the inner side) toward the boxes.
//
// THE GRID. The boxes of both sides are on the same rows (SlotTop): as many
// rows as the side with the most controls has.
//
// THE LEADERS (Route). Each box has a straight line of ink from the side of
// the box that faces the model to its control. The controls take the rows
// in their order from the top of the model, each as near to its own height
// as the rows let it, so the lines are short and run side by side. The boxes
// are next to the side of the model that faces them, where the trigger and
// the grip are, so a line crosses the body only to reach a button on the
// face. Two lines that clash (they cross, or one runs over the ring at the
// end of the other) swap their rows (Clash). When lines still clash, the
// caller uses the order of the least length (LeaderOrder): two straight
// lines of such an order never cross, because a swap of the boxes of two
// lines that cross makes both lines shorter.

#pragma once

#include <math.h>

namespace controls_layout {

// --- the page --------------------------------------------------------------------

const int kPageWidth = 732;
const int kPageHeight = 760;

// The headers of the two columns of boxes.
const int kHeadTop = 14;
const int kHeadBottom = 82;

// The column of boxes and the box of the model of each side. x0 and x1 in
// pixels of the page.
const int kModelWidth = 196;
const int kBoxWidth = 160;
const int kSideGap = 5;     // from the model to its boxes
const int kMiddleGap = 10;  // between the two columns of boxes
const int kModelTop = 100;
const int kModelBottom = 744;
inline void ModelColumn(bool left, int* x0, int* x1) {
  if (left) {
    *x0 = kPageWidth / 2 - kMiddleGap / 2 - kBoxWidth - kSideGap - kModelWidth;
  } else {
    *x0 = kPageWidth / 2 + kMiddleGap / 2 + kBoxWidth + kSideGap;
  }
  *x1 = *x0 + kModelWidth;
}
inline void BoxColumn(bool left, int* x0, int* x1) {
  if (left) {
    *x0 = kPageWidth / 2 - kMiddleGap / 2 - kBoxWidth;
  } else {
    *x0 = kPageWidth / 2 + kMiddleGap / 2;
  }
  *x1 = *x0 + kBoxWidth;
}

// THE BOXES of a side: `count` boxes of kBoxHeight, kBoxPitch apart, with
// their middle at the middle of the model. The answer is the top of box i.
const int kBoxHeight = 64;
const int kBoxPitch = 80;
const int kMostBoxes = 8;
inline int BoxTop(int i, int count, float middle) {
  const float total = static_cast<float>((count - 1) * kBoxPitch + kBoxHeight);
  float top = middle - total * 0.5f;
  if (top < kModelTop) top = static_cast<float>(kModelTop);
  if (top + total > kModelBottom) top = kModelBottom - total;
  return static_cast<int>(top + 0.5f) + i * kBoxPitch;
}
// The box (x0, y0, x1, y1) of box i of a side.
inline void BoxRect(bool left, int i, int count, float middle, int out[4]) {
  BoxColumn(left, &out[0], &out[2]);
  out[1] = BoxTop(i, count, middle);
  out[3] = out[1] + kBoxHeight;
}
// THE ROWS of the grid: `rows` rows round `middle`, the same for both sides.
// The answer is the middle of row i.
inline float SlotMiddle(int i, int rows, float middle) {
  return BoxTop(i, rows, middle) + kBoxHeight * 0.5f;
}

// The point of a box where its leader starts: the middle of its side that
// faces the model.
inline void LeaderStart(bool left, const int box[4], float* x, float* y) {
  *x = static_cast<float>(left ? box[0] : box[2]);
  *y = (box[1] + box[3]) * 0.5f;
}

// --- the leaders -----------------------------------------------------------------

// The length of the line from a to b.
inline float Length(const float a[2], const float b[2]) {
  const float dx = b[0] - a[0], dy = b[1] - a[1];
  return sqrtf(dx * dx + dy * dy);
}

// THE ORDER of the leaders: anchors[k] is the point of control k on the
// model, starts[i] the start of the leader of box i. order[k] gets the box
// of control k, so the sum of the lengths is the least. count is at most
// kMostBoxes. A table over the sets of the boxes that are in use: the cost
// of a set is the least sum for the first controls in that set.
inline float LeaderOrder(const float anchors[][2], const float starts[][2],
                         int count, int order[]) {
  if (count <= 0) return 0.0f;
  if (count > kMostBoxes) count = kMostBoxes;
  const int sets = 1 << count;
  float best[1 << kMostBoxes];
  int from[1 << kMostBoxes];
  for (int m = 0; m < sets; ++m) {
    best[m] = 1e30f;
    from[m] = -1;
  }
  best[0] = 0.0f;
  for (int m = 0; m < sets; ++m) {
    if (best[m] >= 1e30f) continue;
    int k = 0;  // the next control: the count of the boxes in use
    for (int b = 0; b < count; ++b) k += (m >> b) & 1;
    if (k >= count) continue;
    for (int b = 0; b < count; ++b) {
      if ((m >> b) & 1) continue;
      const int next = m | (1 << b);
      const float cost = best[m] + Length(anchors[k], starts[b]);
      if (cost < best[next]) {
        best[next] = cost;
        from[next] = b;
      }
    }
  }
  int m = sets - 1;
  for (int k = count - 1; k >= 0; --k) {
    const int b = from[m];
    order[k] = b;
    m &= ~(1 << b);
  }
  return best[sets - 1];
}

// True when the segment a to b crosses the segment c to d at a point that is
// not an end of both. Lines that only touch at a shared end do not cross.
inline bool Cross(const float a[2], const float b[2], const float c[2],
                  const float d[2]) {
  const auto side = [](const float p[2], const float q[2], const float r[2]) {
    return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0]);
  };
  const float d1 = side(c, d, a), d2 = side(c, d, b);
  const float d3 = side(a, b, c), d4 = side(a, b, d);
  return ((d1 > 0.0f && d2 < 0.0f) || (d1 < 0.0f && d2 > 0.0f)) &&
         ((d3 > 0.0f && d4 < 0.0f) || (d3 < 0.0f && d4 > 0.0f));
}

// THE CLEARANCE of two leaders, each from a start over a knee to an end (a
// ring on its control). They clash when two of their parts cross, or when a
// part of one passes within kRingClear pixels of the end of the other: then
// the line runs over that ring, and the eye cannot tell the two apart.
const float kRingClear = 9.0f;
inline float DistanceToSegment(const float p[2], const float a[2],
                               const float b[2]) {
  const float bx = b[0] - a[0], by = b[1] - a[1];
  const float bb = bx * bx + by * by;
  float t = bb > 0.0f ? ((p[0] - a[0]) * bx + (p[1] - a[1]) * by) / bb : 0.0f;
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  const float dx = p[0] - a[0] - bx * t, dy = p[1] - a[1] - by * t;
  return sqrtf(dx * dx + dy * dy);
}
inline bool Clash(const float sa[2], const float ka[2], const float ea[2],
                  const float sb[2], const float kb[2], const float eb[2]) {
  const float* pa[2][2] = {{sa, ka}, {ka, ea}};
  const float* pb[2][2] = {{sb, kb}, {kb, eb}};
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      if (Cross(pa[i][0], pa[i][1], pb[j][0], pb[j][1])) return true;
    }
    if (DistanceToSegment(eb, pa[i][0], pa[i][1]) < kRingClear) return true;
    if (DistanceToSegment(ea, pb[i][0], pb[i][1]) < kRingClear) return true;
  }
  return false;
}

// THE ROUTE of the leaders of one side. anchors[k] is the point of control
// k on the model, start_x the x of the side of the boxes that faces the
// model, and slot_y the middles of the `rows` rows. row_of[k] gets the row
// of control k, and the points of its line go into start, knee and end.
// knee_x gives a knee: a line from the box goes level to knee_x at the
// height of its row, and then to its control. With knee_x equal to start_x
// the line is straight.
//
// The controls take the rows in their order from the top of the model, each
// as near to its height as the rows let it (the heights keep kKneeGap
// apart). When two leaders clash (Clash: they cross, or one runs over the
// ring of the other, as the line to a face button over the ring of the
// trigger at the side), the two controls swap their places in the order,
// and the rows follow again. False when the leaders still clash, or when
// the side has more controls than rows.
const float kKneeGap = 10.0f;
inline bool Route(const float anchors[][2], int count, float knee_x,
                  float start_x, const float slot_y[], int rows, int row_of[],
                  float start[][2], float knee[][2], float end[][2]) {
  if (count <= 0) return true;
  if (count > rows || count > kMostBoxes || rows > kMostBoxes) return false;
  // The controls from the top of the model.
  int order[kMostBoxes];
  for (int k = 0; k < count; ++k) order[k] = k;
  for (int i = 1; i < count; ++i) {
    for (int j = i; j > 0 && anchors[order[j]][1] < anchors[order[j - 1]][1];
         --j) {
      const int t = order[j];
      order[j] = order[j - 1];
      order[j - 1] = t;
    }
  }
  for (int tries = 0; tries < 4 * kMostBoxes; ++tries) {
    float ky[kMostBoxes];
    for (int j = 0; j < count; ++j) {
      ky[j] = anchors[order[j]][1];
      if (j > 0 && ky[j] < ky[j - 1] + kKneeGap) ky[j] = ky[j - 1] + kKneeGap;
    }
    // The rows in the same order, with the least sum of the heights from a
    // row to its knee: a table of the first j controls in the first r rows.
    float best[kMostBoxes + 1][kMostBoxes + 1];
    bool take[kMostBoxes + 1][kMostBoxes + 1];
    for (int j = 0; j <= count; ++j) {
      for (int r = 0; r <= rows; ++r) {
        best[j][r] = 1e30f;
        take[j][r] = false;
      }
    }
    for (int r = 0; r <= rows; ++r) best[0][r] = 0.0f;
    for (int j = 1; j <= count; ++j) {
      for (int r = j; r <= rows; ++r) {
        const float skip = best[j][r - 1];
        const float use =
            best[j - 1][r - 1] + fabsf(slot_y[r - 1] - ky[j - 1]);
        if (use <= skip) {
          best[j][r] = use;
          take[j][r] = true;
        } else {
          best[j][r] = skip;
        }
      }
    }
    for (int j = count, r = rows; j > 0; --r) {
      if (take[j][r]) {
        row_of[order[j - 1]] = r - 1;
        --j;
      }
    }
    for (int j = 0; j < count; ++j) {
      const int k = order[j];
      start[k][0] = start_x;
      start[k][1] = slot_y[row_of[k]];
      knee[k][0] = knee_x;
      knee[k][1] = slot_y[row_of[k]];
      end[k][0] = anchors[k][0];
      end[k][1] = anchors[k][1];
    }
    // The first two leaders that clash swap their places in the order.
    int clash_i = -1, clash_j = -1;
    for (int i = 0; i < count && clash_i < 0; ++i) {
      for (int j = i + 1; j < count; ++j) {
        const int a = order[i], b = order[j];
        if (Clash(start[a], knee[a], end[a], start[b], knee[b], end[b])) {
          clash_i = i;
          clash_j = j;
          break;
        }
      }
    }
    if (clash_i < 0) return true;
    const int t = order[clash_i];
    order[clash_i] = order[clash_j];
    order[clash_j] = t;
  }
  return false;
}

// --- the parts of the page -------------------------------------------------------

// The parts of the page for the hit test of the window: box i of the left
// side is part i, box i of the right side is part kMostBoxes + i.
inline int PartOf(bool left, int box) {
  return (left ? 0 : kMostBoxes) + box;
}
inline bool SideOfPart(int part, bool* left, int* box) {
  if (part < 0 || part >= 2 * kMostBoxes) return false;
  *left = part < kMostBoxes;
  *box = part % kMostBoxes;
  return true;
}

// --- the view of a model ---------------------------------------------------------

// THE VIEW of the right controller in its grip space (meters, y up, -z ahead
// along the handle, +x to the right): the eye is in front of it (-z), above
// it and at its inner side (-x), and -z of the grip is up in the picture. So
// the inner side of the right controller is at the left of its picture,
// toward its boxes. The left controller has the mirror view (+x). The answer
// is the direction from the middle of the model to the eye, of length 1.
inline void EyeDirection(bool left, float out[3]) {
  const float x = left ? 1.25f : -1.25f, y = 1.0f, z = -0.5f;
  const float n = sqrtf(x * x + y * y + z * z);
  out[0] = x / n;
  out[1] = y / n;
  out[2] = z / n;
}

// The scale and the place that fit a picture of `w` by `h` (in the units of
// a view with a focal of 1, from its middle: x0, y0, x1, y1) into the box
// (x0, y0, x1, y1) of the page with `margin` pixels. out: the scale, and the
// x and y of the middle of the view on the page.
inline void Fit(const float extent[4], const int box[4], float margin,
                float out[3]) {
  const float w = extent[2] - extent[0], h = extent[3] - extent[1];
  const float bw = box[2] - box[0] - 2.0f * margin;
  const float bh = box[3] - box[1] - 2.0f * margin;
  float s = 1.0f;
  if (w > 0.0f && h > 0.0f) {
    s = bw / w < bh / h ? bw / w : bh / h;
  }
  out[0] = s;
  out[1] = (box[0] + box[2]) * 0.5f - s * (extent[0] + extent[2]) * 0.5f;
  out[2] = (box[1] + box[3]) * 0.5f - s * (extent[1] + extent[3]) * 0.5f;
}

}  // namespace controls_layout
