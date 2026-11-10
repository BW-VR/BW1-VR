// The fit of words in a box: the pure math. See text_fit.h and
// docs/ui-layer.md, "Words that fit". text_fit.cpp uses it, and
// test/controller_math checks it (text_fit_checks.h). Nothing here reads a
// global or touches GDI: the width of a run of letters comes from the caller.
//
// THE RULE. A text of the mod never ends in an ellipsis. A text that is too
// wide for its box first gets a smaller font, one step at a time, down to the
// floor. When it is still too wide at the floor, it wraps to more lines, as
// many as the box has room for, with the largest font that fits. Only a text
// that does not fit at the floor on the most lines is cut, and the caller
// logs it.
//
// THE LADDER. A font of the design and its smaller steps, in percent of its
// size. The floor is 70 percent: a capital letter of a card at 70 percent is
// still more than 0.5 degrees high at the place of the card.

#pragma once

#include <wchar.h>

namespace text_fit_math {

const int kSteps = 4;
const int kStepPercent[kSteps] = {100, 90, 80, 70};
const int kMostLines = 4;

struct Fit {
  int step;                 // the font of the ladder, 0 is the design size
  int lines;                // the count of lines, 1 or more
  int start[kMostLines];    // the first letter of each line in the text
  int length[kMostLines];   // the letters of each line, with no space at the end
  bool cut;                 // nothing fits: the floor, and the text is cut
};

inline bool IsSpace(wchar_t c) { return c == L' ' || c == L'\t'; }

// The lines of the text at one step of the ladder, with width(step, start,
// length) in pixels and the box w pixels wide. A line breaks at a space. The
// count can be more than `room`: then only the first `room` lines are in
// `out`. *too_wide is true when one word alone is wider than the box.
template <class Width>
int Wrap(const wchar_t* text, int count, int step, const Width& width, int w,
         int room, Fit* out, bool* too_wide) {
  *too_wide = false;
  int lines = 0;
  int i = 0;
  while (i < count && IsSpace(text[i])) ++i;
  while (i < count) {
    // The longest run of whole words from i that fits.
    int end = i;       // the end of the best run, with no space at its end
    int j = i;
    while (j < count) {
      int k = j;
      while (k < count && !IsSpace(text[k])) ++k;  // the end of a word
      if (width(step, text + i, k - i) <= w) {
        end = k;
      } else {
        break;
      }
      j = k;
      while (j < count && IsSpace(text[j])) ++j;
    }
    if (end == i) {
      // One word alone is too wide: it takes its line.
      *too_wide = true;
      end = i;
      while (end < count && !IsSpace(text[end])) ++end;
    }
    if (lines < room && lines < kMostLines) {
      out->start[lines] = i;
      out->length[lines] = end - i;
    }
    ++lines;
    i = end;
    while (i < count && IsSpace(text[i])) ++i;
  }
  if (lines == 0) {
    out->start[0] = 0;
    out->length[0] = 0;
    lines = 1;
  }
  return lines;
}

// The fit of a text in a box of w by h pixels. line_height[step] is the
// height of a line of each step, `steps` the count of steps of the ladder
// (1 to kSteps), and most_lines the most lines that the box takes.
template <class Width>
Fit FitText(const wchar_t* text, int count, int steps, const Width& width,
            int w, int h, const int* line_height, int most_lines) {
  Fit f = {};
  if (steps < 1) steps = 1;
  if (steps > kSteps) steps = kSteps;
  if (most_lines < 1) most_lines = 1;
  if (most_lines > kMostLines) most_lines = kMostLines;
  bool wide = false;
  // One line: the largest step that fits.
  for (int s = 0; s < steps; ++s) {
    Fit t = {};
    t.step = s;
    t.lines = Wrap(text, count, s, width, w, 1, &t, &wide);
    if (t.lines == 1 && !wide) return t;
  }
  // More lines: the fewest lines, and for them the largest step, that fit
  // in the height of the box.
  for (int n = 2; n <= most_lines; ++n) {
    for (int s = 0; s < steps; ++s) {
      if (line_height[s] * n > h) continue;
      Fit t = {};
      t.step = s;
      t.lines = Wrap(text, count, s, width, w, n, &t, &wide);
      if (t.lines <= n && !wide) return t;
    }
  }
  // Nothing fits: the floor, on as many lines as the box has room for.
  int room = line_height[steps - 1] > 0 ? h / line_height[steps - 1] : 1;
  if (room < 1) room = 1;
  if (room > most_lines) room = most_lines;
  f.step = steps - 1;
  const int all = Wrap(text, count, f.step, width, w, room, &f, &wide);
  f.lines = all < room ? all : room;
  f.cut = true;
  return f;
}

}  // namespace text_fit_math
