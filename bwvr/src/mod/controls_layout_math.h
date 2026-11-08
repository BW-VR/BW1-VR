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
// The model is seen from above and from its inner side, so the face buttons
// show, and the trigger and the grip (at the inner side) face the boxes.
//
// THE LEADERS. Each box has a line of ink to its control on the model. The
// boxes have fixed places in their column, and the controls go to the boxes
// so that the sum of the lengths of the lines is the least (LeaderOrder). Two
// straight lines of such an order never cross: when two lines cross, a swap
// of their two boxes makes both lines shorter.

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
const int kModelWidth = 190;
const int kBoxWidth = 164;
const int kSideGap = 6;     // from the model to its boxes
const int kMiddleGap = 12;  // between the two columns of boxes
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
// along the handle, +x to the right): the eye is above it and at its inner
// side (-x), and -z of the grip is up in the picture. So the inner side of the right controller is at the left of its
// picture, toward its boxes. The left controller has the mirror view (+x).
// The answer is the direction from the middle of the model to the eye, of
// length 1.
inline void EyeDirection(bool left, float out[3]) {
  const float x = left ? 0.85f : -0.85f, y = 1.0f, z = 0.0f;
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
