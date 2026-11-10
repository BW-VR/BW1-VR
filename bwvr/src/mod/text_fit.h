// Words that fit in their box: a smaller font, then more lines, and never an
// ellipsis. See text_fit_math.h for the rule, and docs/ui-layer.md, "Words
// that fit".
//
// Each card and panel of the mod draws a text that can be too wide for its
// box with Draw in place of DrawTextW. Draw uses the font that is selected in
// the DC. Its smaller steps are copies of that font (GetObjectW and
// CreateFontIndirectW), made one time and kept. A text that does not fit even
// at the floor is cut: the log has one line for each such text and place.
//
// THE CHECK (text_fit_test=1, a test key). At the first frame of the game,
// each card and panel measures the texts that it can show in its own boxes
// (TextFitTestFrame), and the log has a line for each text that needs a
// smaller font or more lines, a line for each cut, and the counts.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "text_fit_math.h"

namespace text_fit {

// The fit of the text in the box with the font of the DC. format has the
// side (DT_LEFT, DT_CENTER or DT_RIGHT). most_lines is the most lines that
// the box takes, 1 for a box of one row.
text_fit_math::Fit Measure(HDC dc, const wchar_t* text, int box_w, int box_h,
                           int most_lines);

// Draws the text in the box: each line on its own, with DrawTextW. Vertical
// place: DT_VCENTER puts the lines in the middle of the box. DT_TOP puts the
// baseline of one line where the font of the DC has it at the top of the box,
// so a smaller font keeps the baseline, and more lines go down from there.
// DT_NOCLIP is kept. `where` names the box for the log of a cut. The font of
// the DC is the same after the call. Returns the fit.
text_fit_math::Fit Draw(HDC dc, const wchar_t* text, const RECT& box,
                        UINT format, int most_lines, const char* where);

// The font of a step of the ladder of `font`. Step 0 is the font itself.
HFONT StepFont(HDC dc, HFONT font, int step);
// The height of a line of a step of the ladder of `font` (tmHeight).
int LinePitch(HDC dc, HFONT font, int step);

// For the check: one text in a box with the font, as Draw would place it.
// It logs a text that needs a smaller font or more lines, and a cut. Returns
// the fit.
text_fit_math::Fit Check(HDC dc, HFONT font, const wchar_t* text, int box_w,
                         int box_h, int most_lines, const char* where);

// The counts of this report window: texts drawn at a smaller size, on more
// lines, and cut. The report line of the interface.
void Report();

// text_fit_test=1: true one time, at the first call after the start. The
// check of each card and panel runs then.
bool TestDue();
// The line with the counts of the check.
void TestDone();

}  // namespace text_fit

// controllers.cpp: one call each frame. With text_fit_test=1 it runs the
// check of each card and panel one time.
void TextFitTestFrame();
