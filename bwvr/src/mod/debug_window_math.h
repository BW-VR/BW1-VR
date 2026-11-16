// The debug window: where each part is on the picture, the part under the
// ray, the list that scrolls, and the place of the note. See
// docs/controllers.md, "The debug window", and docs/ui-layer.md, "The debug
// window". No Windows and no game here, so the math test compiles this file
// (debug_window_checks.h).
//
// THE PICTURE is 1024 by 1024 pixels:
//
//   the title        "Debug switches" at the top
//   the tabs         a column at the left, one tab for each group
//   the list         the switches of the group, one row for each, in a view
//                    of 10 rows. The scroll bar is at its right when the
//                    group has more rows than the view
//   the bottom row   the note of the last change, and Close
//
// A ROW has the label of the switch at its left and its control at its
// right:
//
//   a toggle         a switch with two values: the word of the value and a
//                    slot with a knob, at the left for the value 0 and at
//                    the right for 1. A press anywhere on the row changes it
//   two arrows       a switch with more values: a left arrow, the word of
//                    the value, a right arrow
//   no control       a switch that cannot change now: the row is dim
//
// The rows are kRowPitch pixels high, and the list moves by whole pixels
// under the view (panel_kit_math.h, "The list that scrolls").
//
// A PAGE is a group that is one picture in place of rows (the page Controls
// of the settings window): the box of the list and of the scroll bar. A part
// of a page can have a list that drops down under it, and the page can have
// a button in the bottom row, at the left of Close.

#pragma once

#include <stddef.h>
#include <string.h>
#include <wchar.h>

#include "panel_kit_math.h"

namespace debugwin_math {

const int kWidth = 1024;
const int kHeight = 1024;

// THE PLACE IN THE ROOM is the place of the panel of places
// (places_panel_math.h): 1 m in front of the head, 12 degrees below eye
// level, 56 cm across. A row is 4.2 cm high and an arrow is 3.3 cm across.
const float kDistance = 1.0f;
const float kPitch = -0.20944f;
const float kMeters = 0.56f;

// --- the layout ------------------------------------------------------------------

const int kLeft = 40;
const int kRight = 984;
const int kTitleTop = 24;
const int kTitleBottom = 96;

// The tabs of the groups, in a column at the left.
const int kTabsLeft = kLeft;
const int kTabWidth = 196;
const int kTabsTop = 112;
const int kTabHeight = 62;
const int kTabPitch = 70;
const int kMostTabs = 10;

// The list. Its view shows kViewRows rows.
const int kWellLeft = 252;
const int kWellRight = 944;
const int kWellWidth = kWellRight - kWellLeft;
const int kWellTop = 112;
const int kRowPitch = 76;
const int kViewRows = 10;
const int kView = kViewRows * kRowPitch;
const int kWellBottom = kWellTop + kView;
const int kMostRows = 64;

// The scroll bar at the right of the list.
const int kBarLeft = 952;
const int kBarRight = kRight;
const int kThumbLeast = 56;

// The bottom row: the note at the left, and Close at the right.
const int kBottomTop = 888;
const int kBottomHeight = 72;
const int kCloseWidth = 200;

// THE ROW, in the pixels of the row: x from the left of the list, y from
// the top of the row. A rect is x, y, w, h.
const int kLabelLeft = 16;
const int kControlLeft = 344;
const int kControlRight = 680;
const int kControlTop = 10;
const int kControlHeight = 56;
const int kArrowWidth = 60;
const int kPillWidth = 112;

// The kinds of a row. A picture row shows a band of a drawing of the list
// (WindowRows::picture), and the ray does not press it.
enum RowKind { kRowToggle = 0, kRowArrows = 1, kRowDim = 2, kRowPicture = 3 };

// The kind of a row for a switch with `values` words of its values (0 for
// a switch whose value is a number, -1 for a picture row), or for a switch
// that cannot change now.
inline int KindOfRow(int values, bool cannot) {
  if (values < 0) return kRowPicture;
  if (cannot) return kRowDim;
  return values == 2 ? kRowToggle : kRowArrows;
}

// The count of the words of a list with "|" between them: "off|on" is 2,
// and "" is 0.
inline int WordCount(const wchar_t* words) {
  if (words == nullptr || words[0] == 0) return 0;
  int n = 1;
  for (const wchar_t* p = words; *p != 0; ++p) {
    if (*p == L'|') ++n;
  }
  return n;
}

inline void Rect(int x, int y, int w, int h, int out[4]) {
  out[0] = x;
  out[1] = y;
  out[2] = w;
  out[3] = h;
}

// The parts of a row, in the pixels of the row.
inline void LeftArrowRect(int out[4]) {
  Rect(kControlLeft, kControlTop, kArrowWidth, kControlHeight, out);
}
inline void RightArrowRect(int out[4]) {
  Rect(kControlRight - kArrowWidth, kControlTop, kArrowWidth, kControlHeight,
       out);
}
// The word of the value: between the arrows, or at the left of the slot of
// a toggle. x0, y0, x1, y1.
inline void ValueBox(int kind, int out[4]) {
  if (kind == kRowToggle) {
    out[0] = kControlLeft;
    out[2] = kControlRight - kPillWidth - 8;
  } else {
    out[0] = kControlLeft + kArrowWidth + 4;
    out[2] = kControlRight - kArrowWidth - 4;
  }
  out[1] = kControlTop;
  out[3] = kControlTop + kControlHeight;
}
inline void PillRect(int out[4]) {
  Rect(kControlRight - kPillWidth, kControlTop + 2, kPillWidth,
       kControlHeight - 4, out);
}
inline void LabelBox(int out[4]) {
  out[0] = kLabelLeft;
  out[1] = kControlTop;
  out[2] = kControlLeft - 12;
  out[3] = kControlTop + kControlHeight;
}

// The rect of tab g, of Close, and the box of the note.
inline void TabRect(int g, int out[4]) {
  Rect(kTabsLeft, kTabsTop + g * kTabPitch, kTabWidth, kTabHeight, out);
}
inline void CloseRect(int out[4]) {
  Rect(kRight - kCloseWidth, kBottomTop, kCloseWidth, kBottomHeight, out);
}
inline void NoteBox(int out[4]) {
  out[0] = kLeft;
  out[1] = kBottomTop;
  out[2] = kRight - kCloseWidth - 12;
  out[3] = kBottomTop + kBottomHeight;
}

// THE BUTTON OF A PAGE, at the left of Close, and the box of the note with
// it.
const int kPageButtonWidth = 250;
inline void PageButtonRect(int out[4]) {
  Rect(kRight - kCloseWidth - 16 - kPageButtonWidth, kBottomTop,
       kPageButtonWidth, kBottomHeight, out);
}
inline void NoteBoxBeside(bool page_button, int out[4]) {
  NoteBox(out);
  if (page_button) out[2] = kRight - kCloseWidth - 16 - kPageButtonWidth - 12;
}

// THE PAGE: the box of the list and of the scroll bar.
const int kPageWidth = kRight - kWellLeft;
inline void PageBox(int out[4]) {
  out[0] = kWellLeft;
  out[1] = kWellTop;
  out[2] = kRight;
  out[3] = kWellBottom;
}

// --- the scroll ------------------------------------------------------------------

inline int ContentOf(int rows) { return rows * kRowPitch; }

// The track of the scroll bar, x0, y0, x1, y1, and the thumb in it at the
// offset. False when the rows fit in the view: then there is no bar.
inline bool BarBoxes(int rows, int offset, int track[4], int thumb[4]) {
  track[0] = kBarLeft;
  track[1] = kWellTop;
  track[2] = kBarRight;
  track[3] = kWellBottom;
  if (panel_kit_math::MaxOffset(ContentOf(rows), kView) <= 0) return false;
  int top = 0, length = 0;
  panel_kit_math::Thumb(ContentOf(rows), kView, offset, kView, kThumbLeast,
                        &top, &length);
  thumb[0] = kBarLeft;
  thumb[1] = kWellTop + top;
  thumb[2] = kBarRight;
  thumb[3] = kWellTop + top + length;
  return true;
}

// The top of row r on the picture at the offset. It can be above the view
// or below it.
inline int RowTop(int r, int offset) {
  return kWellTop + r * kRowPitch - offset;
}

// The offset that shows all of row r, with the least move from `offset`.
inline int OffsetToShow(int r, int rows, int offset) {
  const int top = r * kRowPitch;
  int o = offset;
  if (top < o) o = top;
  if (top + kRowPitch > o + kView) o = top + kRowPitch - kView;
  return panel_kit_math::ClampOffset(o, ContentOf(rows), kView);
}

// --- the parts under the ray -----------------------------------------------------

// The parts: the tabs, Close, the track and the thumb of the scroll bar,
// then three for each row: the row itself (its label, its value, all of a
// toggle), its left arrow and its right arrow.
const int kPartClose = kMostTabs;
const int kPartTrack = kMostTabs + 1;
const int kPartThumb = kMostTabs + 2;
const int kPartButton = kMostTabs + 3;  // the button of a page
const int kFirstRowPart = 16;
enum RowPart { kOnRow = 0, kOnLeft = 1, kOnRight = 2, kRowParts = 3 };
const int kParts = kFirstRowPart + kMostRows * kRowParts;
// The parts of a page, then the items of a list that drops down.
const int kPageParts = 40;
const int kFirstPagePart = kParts;
const int kListMost = 8;
const int kFirstListPart = kFirstPagePart + kPageParts;
const int kAllParts = kFirstListPart + kListMost;

inline int PagePartOf(int part) {
  return part >= kFirstPagePart && part < kFirstPagePart + kPageParts
             ? part - kFirstPagePart
             : -1;
}
inline int ListItemOf(int part) {
  return part >= kFirstListPart && part < kAllParts ? part - kFirstListPart
                                                    : -1;
}

inline int RowPartOf(int row, int on) {
  return kFirstRowPart + row * kRowParts + on;
}
// The row of a part, or -1, and which part of the row.
inline int RowOf(int part) {
  if (part < kFirstRowPart || part >= kParts) return -1;
  return (part - kFirstRowPart) / kRowParts;
}
inline int OnOf(int part) {
  if (part < kFirstRowPart || part >= kParts) return -1;
  return (part - kFirstRowPart) % kRowParts;
}

inline bool InRect(float x, float y, const int r[4]) {
  return x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3];
}

// The part at u, v of the panel (0 to 1 across and down), or -1. `kinds`
// has the kind of each of the `rows` rows of the group.
inline int PartAt(float u, float v, int groups, int rows,
                  const unsigned char* kinds, int offset) {
  if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) return -1;
  const float x = u * kWidth;
  const float y = v * kHeight;
  int r[4];
  for (int g = 0; g < groups && g < kMostTabs; ++g) {
    TabRect(g, r);
    if (InRect(x, y, r)) return g;
  }
  CloseRect(r);
  if (InRect(x, y, r)) return kPartClose;
  int track[4], thumb[4];
  if (BarBoxes(rows, offset, track, thumb) && x >= track[0] && x < track[2] &&
      y >= track[1] && y < track[3]) {
    return y >= thumb[1] && y < thumb[3] ? kPartThumb : kPartTrack;
  }
  if (x < kWellLeft || x >= kWellRight || y < kWellTop || y >= kWellBottom) {
    return -1;
  }
  const int along = static_cast<int>(y) - kWellTop + offset;
  const int row = along / kRowPitch;
  if (row < 0 || row >= rows || row >= kMostRows) return -1;
  const float lx = x - kWellLeft;
  const float ly = static_cast<float>(along - row * kRowPitch);
  if (kinds != nullptr && kinds[row] == kRowArrows) {
    LeftArrowRect(r);
    if (InRect(lx, ly, r)) return RowPartOf(row, kOnLeft);
    RightArrowRect(r);
    if (InRect(lx, ly, r)) return RowPartOf(row, kOnRight);
  }
  return RowPartOf(row, kOnRow);
}

// The middle of a part in u, v at the offset, for the scripted controller.
// False for a part that is not on the picture now.
inline bool PartMiddle(int part, int groups, int rows,
                       const unsigned char* kinds, int offset, float* u,
                       float* v) {
  int r[4];
  float x = 0.0f, y = 0.0f;
  if (part >= 0 && part < groups && part < kMostTabs) {
    TabRect(part, r);
    x = r[0] + r[2] * 0.5f;
    y = r[1] + r[3] * 0.5f;
  } else if (part == kPartClose) {
    CloseRect(r);
    x = r[0] + r[2] * 0.5f;
    y = r[1] + r[3] * 0.5f;
  } else if (part == kPartThumb || part == kPartTrack) {
    int track[4], thumb[4];
    if (!BarBoxes(rows, offset, track, thumb)) return false;
    x = (track[0] + track[2]) * 0.5f;
    // The track: the end that is farther from the thumb.
    const bool down = track[3] - thumb[3] >= thumb[1] - track[1];
    y = part == kPartThumb ? (thumb[1] + thumb[3]) * 0.5f
                           : (down ? track[3] - 4.0f : track[1] + 4.0f);
  } else {
    const int row = RowOf(part);
    const int on = OnOf(part);
    if (row < 0 || row >= rows) return false;
    const int top = RowTop(row, offset);
    if (top < kWellTop || top + kRowPitch > kWellBottom) return false;
    if (on == kOnLeft || on == kOnRight) {
      if (kinds == nullptr || kinds[row] != kRowArrows) return false;
      if (on == kOnLeft) {
        LeftArrowRect(r);
      } else {
        RightArrowRect(r);
      }
      x = kWellLeft + r[0] + r[2] * 0.5f;
      y = top + r[1] + r[3] * 0.5f;
    } else {
      int b[4];
      LabelBox(b);
      x = kWellLeft + (b[0] + b[2]) * 0.5f;
      y = top + kRowPitch * 0.5f;
    }
  }
  *u = x / kWidth;
  *v = y / kHeight;
  return true;
}

// THE THUMB UNDER THE RAY. A drag keeps the point of the thumb that the
// ray took: `grab` is the pixels from the top of the thumb to that point.
// y is the pixel of the ray now. The answer is the new offset.
inline int DragOffset(int rows, float y, int grab) {
  const int top = static_cast<int>(y) - grab - kWellTop;
  return panel_kit_math::OffsetOfThumb(ContentOf(rows), kView, kView,
                                       kThumbLeast, top);
}

// A press on the track above the thumb goes one view up, and below it one
// view down.
inline int PageOffset(int rows, int offset, float y) {
  int track[4], thumb[4];
  if (!BarBoxes(rows, offset, track, thumb)) return 0;
  const int step = kView - kRowPitch;
  const int to = y < thumb[1] ? offset - step : offset + step;
  return panel_kit_math::ClampOffset(to, ContentOf(rows), kView);
}

// --- the list that drops down ----------------------------------------------------

// THE LIST of a part of a page: a small scroll of paper under the part, with
// one item for each choice, in the pixels of the page. The items are a little
// wider than the part (kListLeast at least), and the paper has kListSide at
// each side
// for its rolls and kListPad above and below. The list goes above the part
// when it does not fit below it in the page, with its rolls and its shadow.
// x0, y0, x1, y1 of the paper.
const int kListItem = 46;
const int kListPad = 14;
const int kListSide = 34;
const int kListLeast = 180;
// The rolls and the shadow of the paper go this far past the paper, and
// they stay on the page.
const int kListAbove = 12;
const int kListBelow = 22;
inline void ListPlace(const int part_box[4], int count, int out[4]) {
  if (count < 1) count = 1;
  if (count > kListMost) count = kListMost;
  int items = part_box[2] - part_box[0] + 24;
  if (items < kListLeast) items = kListLeast;
  const int width = items + 2 * kListSide;
  const int height = count * kListItem + 2 * kListPad;
  int x0 = (part_box[0] + part_box[2]) / 2 - width / 2;
  if (x0 + width > kPageWidth) x0 = kPageWidth - width;
  if (x0 < 0) x0 = 0;
  int y0 = part_box[3] + 2;
  if (y0 + height + kListBelow > kView) y0 = part_box[1] - 2 - height;
  if (y0 < kListAbove) y0 = kListAbove;
  out[0] = x0;
  out[1] = y0;
  out[2] = x0 + width;
  out[3] = y0 + height;
}
// The rect (x, y, w, h) of item i in a list at `list` (ListPlace).
inline void ListItemRect(const int list[4], int i, int out[4]) {
  Rect(list[0] + kListSide, list[1] + kListPad + i * kListItem + 3,
       list[2] - list[0] - 2 * kListSide, kListItem - 6, out);
}
// The item at x, y of the page, or -1.
inline int ListItemAt(const int list[4], int count, float x, float y) {
  for (int i = 0; i < count && i < kListMost; ++i) {
    int r[4];
    ListItemRect(list, i, r);
    // The gap between two items belongs to the nearer one.
    const int top = r[1] - 3, bottom = r[1] + r[3] + 3;
    if (x >= r[0] && x < r[0] + r[2] && y >= top && y < bottom) return i;
  }
  return -1;
}
inline bool InList(const int list[4], float x, float y) {
  return x >= list[0] && x < list[2] && y >= list[1] && y < list[3];
}

// --- the note --------------------------------------------------------------------

// THE NOTE of a row (its tooltip) is `height` pixels high and kNoteWidth
// across, in the list. It goes below the row when it fits there, else above
// the row. It does not cover the tabs, the scroll bar or the bottom row.
// x0, y0, x1, y1. `top` is the top of the row on the picture.
const int kNoteLeft = kWellLeft + 24;
const int kNoteWidth = kWellRight - 4 - kNoteLeft;
const int kNoteMost = kView / 2;  // the most height of a note
inline void NotePlace(int top, int height, int out[4]) {
  if (height > kNoteMost) height = kNoteMost;
  const int t = top < kWellTop ? kWellTop : top;
  const int b = top + kRowPitch > kWellBottom ? kWellBottom : top + kRowPitch;
  out[0] = kNoteLeft;
  out[2] = kNoteLeft + kNoteWidth;
  if (b + height <= kWellBottom) {
    out[1] = b;
  } else if (t - height >= kWellTop) {
    out[1] = t - height;
  } else {
    out[1] = kWellTop;
  }
  out[3] = out[1] + height;
}

// The ray rests on one row this long before its note shows.
const double kNoteRestMs = 350.0;

// --- the test key ----------------------------------------------------------------

// The words of debug_test_parts for the word "aimdebug" of the scripted
// controller: "tab3" (the tab of group 3, from 0), "row5", "left5",
// "right5" (row 5 of the group shown, from 0), "thumb", "trackend" (the end
// of the track that is farther from the thumb), "close", "page4" (part 4
// of a page), "item2" (item 2 of the list that shows, from 0) and "reset"
// (the button of a page). A word that is not known gives -1.
inline int PartOfWord(const char* word, size_t length) {
  const auto is = [&](const char* w) {
    return length == strlen(w) && strncmp(word, w, length) == 0;
  };
  if (is("thumb")) return kPartThumb;
  if (is("trackend")) return kPartTrack;
  if (is("close")) return kPartClose;
  if (is("reset")) return kPartButton;
  const char* const prefixes[6] = {"tab", "row", "left", "right", "page",
                                   "item"};
  for (int k = 0; k < 6; ++k) {
    const size_t n = strlen(prefixes[k]);
    if (length <= n || length > n + 2 || strncmp(word, prefixes[k], n) != 0) {
      continue;
    }
    int number = 0;
    for (size_t i = n; i < length; ++i) {
      if (word[i] < '0' || word[i] > '9') return -1;
      number = number * 10 + (word[i] - '0');
    }
    if (k == 0) return number < kMostTabs ? number : -1;
    if (k == 4) return number < kPageParts ? kFirstPagePart + number : -1;
    if (k == 5) return number < kListMost ? kFirstListPart + number : -1;
    if (number >= kMostRows) return -1;
    return RowPartOf(number, k == 1 ? kOnRow : (k == 2 ? kOnLeft : kOnRight));
  }
  return -1;
}

// The words of a list with spaces or commas. out gets the parts, and the
// answer is their count.
inline int ParseParts(const char* text, int* out, int most) {
  int count = 0;
  const char* p = text != nullptr ? text : "";
  while (*p != 0 && count < most) {
    while (*p == ' ' || *p == ',' || *p == '\t') ++p;
    const char* end = p;
    while (*end != 0 && *end != ' ' && *end != ',' && *end != '\t') ++end;
    if (end > p) {
      const int part = PartOfWord(p, static_cast<size_t>(end - p));
      if (part >= 0) out[count++] = part;
    }
    p = end;
  }
  return count;
}

}  // namespace debugwin_math
