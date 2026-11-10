// Words that fit in their box. See text_fit.h.

#include "text_fit.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

extern void ModLog(const char* format, ...);

// The check of each card and panel (text_fit_test=1).
void SpellBookTextFitTest();
void HintCardTextFitTest();
void RadialMenuTextFitTest();
void DebugWindowTextFitTest();
void VrKeyboardTextFitTest();
void PlacesPanelTextFitTest();

namespace text_fit {

namespace {

using text_fit_math::Fit;
using text_fit_math::kStepPercent;
using text_fit_math::kSteps;

// THE LADDERS. One for each font that a card or a panel gives to Draw. The
// fonts of the mod live as long as the mod, so the handle is the key. A
// ladder is made whole under the lock and does not change after that, so a
// warm thread of a panel and the frame thread can share the table.
struct Ladder {
  HFONT base;
  HFONT font[kSteps];
  int ascent[kSteps];
  int height[kSteps];
};
const int kMostLadders = 160;
Ladder g_ladders[kMostLadders] = {};
int g_ladder_count = 0;
bool g_said_full = false;
SRWLOCK g_lock = SRWLOCK_INIT;

const Ladder* FindLadder(HFONT base) {
  for (int i = 0; i < g_ladder_count; ++i) {
    if (g_ladders[i].base == base) return &g_ladders[i];
  }
  return nullptr;
}

const Ladder* MakeLadder(HDC dc, HFONT base);

const Ladder* LadderOf(HDC dc, HFONT base) {
  if (base == nullptr) return nullptr;
  AcquireSRWLockShared(&g_lock);
  const Ladder* found = FindLadder(base);
  ReleaseSRWLockShared(&g_lock);
  if (found != nullptr) return found;
  AcquireSRWLockExclusive(&g_lock);
  found = FindLadder(base);
  if (found == nullptr) found = MakeLadder(dc, base);
  ReleaseSRWLockExclusive(&g_lock);
  return found;
}

const Ladder* MakeLadder(HDC dc, HFONT base) {
  if (g_ladder_count >= kMostLadders) {
    if (!g_said_full) {
      g_said_full = true;
      ModLog("text fit     : the table of fonts is full (%d), so a new font "
             "gets no smaller steps", kMostLadders);
    }
    return nullptr;
  }
  LOGFONTW lf;
  if (GetObjectW(base, sizeof(lf), &lf) != sizeof(lf)) return nullptr;
  Ladder& l = g_ladders[g_ladder_count];
  memset(&l, 0, sizeof(l));
  l.base = base;
  HGDIOBJ old = SelectObject(dc, base);
  for (int s = 0; s < kSteps; ++s) {
    HFONT f = base;
    if (s > 0) {
      LOGFONTW smaller = lf;
      smaller.lfHeight = lf.lfHeight * kStepPercent[s] / 100;
      smaller.lfWidth = lf.lfWidth * kStepPercent[s] / 100;
      if (smaller.lfHeight == 0) smaller.lfHeight = lf.lfHeight;
      f = CreateFontIndirectW(&smaller);
      if (f == nullptr) f = l.font[s - 1];
    }
    l.font[s] = f;
    SelectObject(dc, f);
    TEXTMETRICW m;
    if (GetTextMetricsW(dc, &m)) {
      l.ascent[s] = m.tmAscent;
      l.height[s] = m.tmHeight;
    }
  }
  SelectObject(dc, old);
  ++g_ladder_count;
  return &l;
}

struct Width {
  HDC dc;
  const Ladder* ladder;
  int operator()(int step, const wchar_t* s, int n) const {
    if (n <= 0) return 0;
    SelectObject(dc, ladder->font[step]);
    SIZE size = {0, 0};
    GetTextExtentPoint32W(dc, s, n, &size);
    return size.cx;
  }
};

Fit FitWith(HDC dc, const Ladder* l, const wchar_t* text, int w, int h,
            int most_lines) {
  Fit f = {};
  f.lines = 1;
  if (text == nullptr) return f;
  const int count = static_cast<int>(wcslen(text));
  if (l == nullptr) {
    // No ladder: the text as it is, on one line.
    f.start[0] = 0;
    f.length[0] = count;
    return f;
  }
  HGDIOBJ old = GetCurrentObject(dc, OBJ_FONT);
  const Width width = {dc, l};
  f = text_fit_math::FitText(text, count, kSteps, width, w, h, l->height,
                             most_lines);
  SelectObject(dc, old);
  return f;
}

void Narrow(const wchar_t* in, char* out, size_t size) {
  size_t n = 0;
  for (; in != nullptr && n + 1 < size && in[n] != 0; ++n) {
    out[n] = (in[n] >= 32 && in[n] < 127) ? static_cast<char>(in[n]) : '?';
  }
  out[n] = 0;
}

// One log line for each cut text and place.
const int kMostCutLogs = 120;
uint32_t g_cut_seen[kMostCutLogs] = {};
int g_cut_logs = 0;

uint32_t HashOf(const char* where, const wchar_t* text) {
  uint32_t h = 2166136261u;
  for (const char* p = where; p != nullptr && *p != 0; ++p) {
    h = (h ^ static_cast<uint8_t>(*p)) * 16777619u;
  }
  for (const wchar_t* p = text; p != nullptr && *p != 0; ++p) {
    h = (h ^ static_cast<uint32_t>(*p)) * 16777619u;
  }
  return h;
}

void LogCut(const char* where, const wchar_t* text, int w, int h,
            const Fit& f) {
  const uint32_t key = HashOf(where, text);
  AcquireSRWLockExclusive(&g_lock);
  bool seen = g_cut_logs >= kMostCutLogs;
  for (int i = 0; i < g_cut_logs && !seen; ++i) seen = g_cut_seen[i] == key;
  if (!seen) g_cut_seen[g_cut_logs++] = key;
  ReleaseSRWLockExclusive(&g_lock);
  if (seen) return;
  char words[160];
  Narrow(text, words, sizeof(words));
  ModLog("text fit     : %s: the text \"%s\" does not fit in %d by %d px, "
         "even at %d percent of its size on %d lines, so it is cut",
         where != nullptr ? where : "a card", words, w, h,
         kStepPercent[f.step], f.lines);
}

uint64_t g_smaller = 0, g_more_lines = 0, g_cuts = 0;

// The check.
int g_test = -1;
int g_check_texts = 0, g_check_smaller = 0, g_check_lines = 0,
    g_check_cut = 0;
int g_check_logs = 0;
const int kMostCheckLogs = 300;

}  // namespace

Fit Measure(HDC dc, const wchar_t* text, int box_w, int box_h,
            int most_lines) {
  const Ladder* l =
      LadderOf(dc, static_cast<HFONT>(GetCurrentObject(dc, OBJ_FONT)));
  return FitWith(dc, l, text, box_w, box_h, most_lines);
}

HFONT StepFont(HDC dc, HFONT font, int step) {
  const Ladder* l = LadderOf(dc, font);
  if (l == nullptr || step < 0 || step >= kSteps) return font;
  return l->font[step];
}

int LinePitch(HDC dc, HFONT font, int step) {
  const Ladder* l = LadderOf(dc, font);
  if (l == nullptr) return 0;
  if (step < 0) step = 0;
  if (step >= kSteps) step = kSteps - 1;
  return l->height[step];
}

Fit Draw(HDC dc, const wchar_t* text, const RECT& box, UINT format,
         int most_lines, const char* where) {
  HFONT base = static_cast<HFONT>(GetCurrentObject(dc, OBJ_FONT));
  const Ladder* l = LadderOf(dc, base);
  const int w = box.right - box.left, h = box.bottom - box.top;
  const Fit f = FitWith(dc, l, text, w, h, most_lines);
  if (text == nullptr || text[0] == 0) return f;
  const UINT side = format & (DT_LEFT | DT_CENTER | DT_RIGHT);
  const UINT keep = format & DT_NOCLIP;
  if (l == nullptr) {
    RECT r = box;
    DrawTextW(dc, text, -1, &r, DT_SINGLELINE | DT_NOPREFIX | side | keep |
                                    (format & (DT_VCENTER | DT_TOP)));
    return f;
  }
  SelectObject(dc, l->font[f.step]);
  const int pitch = l->height[f.step];
  int top;
  if (format & DT_VCENTER) {
    top = box.top + (h - f.lines * pitch) / 2;
  } else {
    top = box.top + l->ascent[0] - l->ascent[f.step];
  }
  for (int k = 0; k < f.lines; ++k) {
    RECT r = {box.left, top + k * pitch, box.right, top + (k + 1) * pitch};
    DrawTextW(dc, text + f.start[k], f.length[k], &r,
              DT_SINGLELINE | DT_NOPREFIX | DT_TOP | side | keep);
  }
  SelectObject(dc, base);
  if (f.step > 0) ++g_smaller;
  if (f.lines > 1) ++g_more_lines;
  if (f.cut) {
    ++g_cuts;
    LogCut(where, text, w, h, f);
  }
  return f;
}

Fit Check(HDC dc, HFONT font, const wchar_t* text, int box_w, int box_h,
          int most_lines, const char* where) {
  const Ladder* l = LadderOf(dc, font);
  const Fit f = FitWith(dc, l, text, box_w, box_h, most_lines);
  if (text == nullptr || text[0] == 0) return f;
  ++g_check_texts;
  if (f.cut) {
    ++g_check_cut;
    LogCut(where, text, box_w, box_h, f);
    return f;
  }
  if (f.step > 0) ++g_check_smaller;
  if (f.lines > 1) ++g_check_lines;
  if ((f.step > 0 || f.lines > 1) && g_check_logs < kMostCheckLogs) {
    ++g_check_logs;
    char words[160];
    Narrow(text, words, sizeof(words));
    ModLog("text fit     : check: %s: \"%s\" fits in %d by %d px at %d "
           "percent of its size on %d line%s", where, words, box_w, box_h,
           kStepPercent[f.step], f.lines, f.lines == 1 ? "" : "s");
  }
  return f;
}

void Report() {
  if (g_smaller + g_more_lines + g_cuts == 0) return;
  ModLog("text fit     : this window %llu texts had a smaller font, %llu "
         "went on more lines, and %llu were cut", g_smaller, g_more_lines,
         g_cuts);
  g_smaller = g_more_lines = g_cuts = 0;
}

bool TestDue() {
  if (g_test < 0) {
    g_test = 0;
    char path[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\')
                                                  : nullptr;
    if (slash != nullptr && strlen(path) + 9 < MAX_PATH) {
      slash[1] = 0;
      strcat_s(path, MAX_PATH, "bwvr.ini");
      g_test = GetPrivateProfileIntA("bwvr", "text_fit_test", 0, path) != 0
                   ? 1
                   : 0;
    }
  }
  if (g_test != 1) return false;
  g_test = 2;
  return true;
}

void TestDone() {
  ModLog("text fit     : check: text_fit_test=1: %d texts of the cards and "
         "panels measured in their boxes: %d at a smaller font, %d on more "
         "lines, %d cut", g_check_texts, g_check_smaller, g_check_lines,
         g_check_cut);
}

}  // namespace text_fit

void TextFitTestFrame() {
  if (!text_fit::TestDue()) return;
  SpellBookTextFitTest();
  HintCardTextFitTest();
  RadialMenuTextFitTest();
  DebugWindowTextFitTest();
  VrKeyboardTextFitTest();
  PlacesPanelTextFitTest();
  text_fit::TestDone();
}
