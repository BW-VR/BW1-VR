// WORDS THAT FIT (docs/ui-layer.md, "Words that fit"). A fake font: each
// letter is 10 pixels wide at the size of the design and gets narrower with
// each step of the ladder.
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <wchar.h>

#include "../../src/mod/text_fit_math.h"

namespace text_fit_checks {

struct FakeWidth {
  int operator()(int step, const wchar_t* s, int n) const {
    (void)s;
    return n * 10 * text_fit_math::kStepPercent[step] / 100;
  }
};

inline text_fit_math::Fit FitOf(const wchar_t* text, int w, int h,
                                int most_lines) {
  const int heights[text_fit_math::kSteps] = {40, 36, 32, 28};
  return text_fit_math::FitText(text, static_cast<int>(wcslen(text)),
                                text_fit_math::kSteps, FakeWidth(), w, h,
                                heights, most_lines);
}

inline bool LineIs(const wchar_t* text, const text_fit_math::Fit& f, int k,
                   const wchar_t* want) {
  const int n = static_cast<int>(wcslen(want));
  return f.length[k] == n && wcsncmp(text + f.start[k], want, n) == 0;
}

inline void TextFitChecks() {
  using text_fit_math::Fit;
  printf("\nWords that fit\n");
  // 25 letters: 250 px at the size of the design.
  const wchar_t* title = L"No miracle to ask for now";
  {
    const Fit f = FitOf(title, 260, 40, 1);
    Check(f.step == 0 && f.lines == 1 && !f.cut,
          "a text that fits keeps the font of the design on one line", f.step,
          0);
    Check(LineIs(title, f, 0, title), "and the line is the whole text",
          f.length[0], 25);
  }
  {
    const Fit f = FitOf(title, 230, 40, 1);
    Check(f.step == 1 && f.lines == 1 && !f.cut,
          "10 percent too wide: the next step of the font, on one line",
          f.step, 1);
  }
  {
    const Fit f = FitOf(title, 176, 40, 1);
    Check(f.step == 3 && f.lines == 1 && !f.cut,
          "at 70 percent (the floor) it fits in 176 px", f.step, 3);
  }
  {
    // Too wide at the floor, with room for two lines: two lines at the
    // largest step that fits.
    const Fit f = FitOf(title, 150, 80, 2);
    Check(f.lines == 2 && !f.cut, "too wide at the floor: two lines",
          f.lines, 2);
    Check(f.step == 0, "and the two lines have the font of the design",
          f.step, 0);
    Check(LineIs(title, f, 0, L"No miracle to") &&
              LineIs(title, f, 1, L"ask for now"),
          "the break is at a space, with no space at the end of a line",
          f.length[0], 13);
  }
  {
    // Two lines need 2 x 40 px at the design size and 2 x 36 px at 90
    // percent: a box of 70 px takes them at 80 percent.
    const Fit f = FitOf(title, 150, 70, 2);
    Check(f.lines == 2 && f.step == 2 && !f.cut,
          "a box of 70 px takes two lines of the step at 80 percent",
          f.step, 2);
  }
  {
    const Fit f = FitOf(title, 100, 40, 1);
    Check(f.cut && f.step == 3 && f.lines == 1,
          "no room at the floor on one line: the text is cut", f.cut ? 1 : 0,
          1);
  }
  {
    // Fewer lines come first: 100 px take two lines at the floor before
    // three lines at a larger step.
    const Fit two = FitOf(title, 100, 200, 3);
    Check(!two.cut && two.lines == 2 && two.step == 3,
          "two lines at the floor come before three lines", two.lines, 2);
    const Fit f = FitOf(title, 80, 200, 3);
    Check(!f.cut && f.lines == 3 && f.step == 2,
          "80 px take three lines at 80 percent", f.lines, 3);
    Check(f.length[0] + f.length[1] + f.length[2] + 2 == 25,
          "the three lines hold all the letters but the two spaces of the "
          "breaks", f.length[0] + f.length[1] + f.length[2], 23);
  }
  {
    // A word that is wider than the box at the floor is a cut.
    const wchar_t* word = L"Compassionateness";
    const Fit f = FitOf(word, 100, 200, 4);
    Check(f.cut, "one word wider than the box at the floor is a cut",
          f.cut ? 1 : 0, 1);
  }
  {
    const Fit f = FitOf(L"", 100, 40, 1);
    Check(f.lines == 1 && f.length[0] == 0 && !f.cut,
          "an empty text is one empty line", f.lines, 1);
  }
  {
    const wchar_t* spaced = L"  Food   Wood  ";
    const Fit f = FitOf(spaced, 200, 40, 1);
    Check(f.lines == 1 && LineIs(spaced, f, 0, L"Food   Wood"),
          "spaces at the ends do not count", f.length[0], 11);
  }
  // The rule for every text of every size: the lines hold all the words in
  // order, and each line is in the box at its step.
  {
    const wchar_t* text = L"Food Miracle Grip Hold to pour B Tap to cancel";
    bool ok = true;
    for (int w = 40; w <= 500; w += 7) {
      for (int lines = 1; lines <= 4; ++lines) {
        const Fit f = FitOf(text, w, 40 * lines, lines);
        if (f.cut) continue;
        int letters = 0;
        for (int k = 0; k < f.lines; ++k) {
          if (FakeWidth()(f.step, text + f.start[k], f.length[k]) > w) {
            ok = false;
          }
          if (k > 0 && f.start[k] < f.start[k - 1] + f.length[k - 1]) {
            ok = false;
          }
          letters += f.length[k];
        }
        if (letters + f.lines - 1 != static_cast<int>(wcslen(text))) {
          ok = false;
        }
      }
    }
    Check(ok, "each fit that is not cut keeps every word, in order, inside "
          "the box", ok ? 1 : 0, 1);
  }
}

}  // namespace text_fit_checks
