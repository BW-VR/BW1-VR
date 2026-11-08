// The debug window. See debug_window.h, docs/controllers.md, "The debug
// window", and docs/ui-layer.md, "The debug window".
//
// WHY. The debug page holds the switches of the test keys (debug_menu.cpp)
// for a live A/B in the headset. On the ring it is a walk through pages of
// seals, a group and then 6 switches at a time, and a seal has no room to
// say what its switch does. The window shows a whole group as a list, with
// a control for each switch and a note that says what it does.
//
// WHAT. The item "debug" of the ring opens the window at the place of the
// panel of places (places_panel.cpp): 1 m in front of the head, 12 degrees
// below eye level, fixed in the room. At its left is a tab for each group,
// and at its right the switches of the group, one row for each:
//
//   a toggle      a switch with two values. A press anywhere on the row
//                 changes it
//   two arrows    a switch with more values. The right arrow makes the step
//                 of its test key. The left arrow makes the step back of the
//                 switch when the table gives one (throw speed, land speed),
//                 and else the same step as the right arrow: the change
//                 function of a test key steps one way only
//   no control    a switch that cannot change now (kDebugCannot): the row
//                 is dim
//
// A group with more than 10 rows has a scroll bar. The ray drags its thumb,
// a press on its track goes one view up or down, and the stick of the
// pointing hand forward and back scrolls the list while the ray is on the
// window (it does not zoom then). When the ray rests on a row for 0.35 s, a
// note of paper shows what the switch is and what it does, and its ini key.
// Close, B of the pointing hand and the menu button close the window. The
// ring, the spell book, a dialog box and a cut scene close it too.
//
// A STEP of a switch is the step of its test key (DebugSwitchStep): the
// same change, the same pulse and the same log line. So the window gives no
// pulse of its own for a step, only the flash of the control.
//
// THE BAKE. The page with the title is drawn one time, with an ink rule
// above and below the list. Each tab and Close in each look is drawn the
// first time that it shows, and a copy is kept (panel_kit::Tiles). Each row
// of the group is drawn into a picture of its own when its look or its
// value changes. Each row has the same paper (the paper of the first row of
// the list), so a copy of a row can go to each place of the list, and a
// scroll is a copy of the rows that show. The scroll bar, the note and the
// words of the bottom row are drawn when they change.
//
// THE PICTURE uses the quad layer of the ring (kCardRing): the ring and the
// window never show at one time.
//
// THE LIST. The rows come from a table of WindowRows (debug_window.h): the
// switches of the debug page here, or the settings of the player
// (settings_panel.cpp). The title is on the card, so each list has its own.
// The copies of the tabs and of the rows are kept apart for each list.
//
//   debug_window=1             0: the item "debug" goes to the debug pages
//                              of the ring (debug_menu.cpp)
//   hint_card_style=1          1: the parchment. 0: the dark window
//   panel_warm=1               1: the thread of the warm draws the page and
//                              the tabs of a first picture
//                              (DebugWindowWarm). 0: the first picture
//                              draws them on the frame thread
//   debug_window_test_save=0   N: saves the first N pictures of the window
//                              with a new group, a new value, a note or a
//                              scroll as bwvr-debugwin-<frame>.bmp (a test
//                              key)
//   debug_test_parts           a test key: the parts for the word "aimdebug"
//                              of the scripted controller, for example
//                              "tab2 right0 row3 thumb trackend close"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <new>

#include "../vr/xr_screen.h"
#include "card_scroll.h"
#include "controllers.h"
#include "cutscene_window.h"
#include "debug_menu.h"
#include "debug_window.h"
#include "debug_window_math.h"
#include "haptics.h"
#include "panel_kit.h"
#include "radial_menu.h"
#include "settings_panel.h"
#include "spell_book.h"

extern void ModLog(const char* format, ...);

uint64_t ControllersFrameCount();
// eye_capture.cpp
void EyeCaptureSaveUi(const char* name, uint64_t frame, int style,
                      const void* pixels, int pitch, uint32_t width,
                      uint32_t height);

namespace {

using namespace debugwin_math;
namespace kit = panel_kit;

// --- the rows of the debug page ---------------------------------------------------

void DebugLastLine(int i, wchar_t* out, int size) {
  const char* k = DebugSwitchKey(i);
  if (k[0] != 0) {
    swprintf_s(out, size, L"Key: %hs", k);
  } else {
    wcscpy_s(out, size, L"No ini key: a state of a test only");
  }
}

const WindowRows kDebugRows = {
    0,
    "debug window ",
    L"Debug switches",
    L"Point at a switch to read what it does",
    DebugGroupCount,
    DebugGroupName,
    DebugSwitchCount,
    DebugSwitchGroup,
    DebugSwitchInWindow,
    DebugSwitchWindowLabel,
    DebugSwitchName,
    DebugSwitchTip,
    DebugLastLine,
    DebugSwitchValue,
    DebugSwitchValueCount,
    DebugSwitchValueText,
    DebugSwitchStep,
    nullptr,
};

// The rows that show, or that showed the last time.
const WindowRows* g_src = &kDebugRows;

// A log line that starts with the tag of the rows.
void WinLog(const char* format, ...) {
  char text[600];
  va_list args;
  va_start(args, format);
  _vsnprintf_s(text, sizeof(text), _TRUNCATE, format, args);
  va_end(args);
  ModLog("%s: %s", g_src->tag, text);
}

// --- the settings ----------------------------------------------------------------

bool g_read = false;
int g_style_ini = 1;
int g_save = 0;
int g_saved = 0;
const int kMostTests = 48;
int g_tests[kMostTests];
int g_test_count = 0;
int g_tests_used = 0;

// The stick: no scroll below this push, and the speed at a full push.
const float kStickRest = 0.25f;
const float kStickFull = 1400.0f;  // pixels a second, 18 rows

bool IniPath(char path[MAX_PATH]) {
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return false;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return false;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return true;
}

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  char path[MAX_PATH];
  char text[512] = "";
  if (IniPath(path)) {
    g_style_ini =
        GetPrivateProfileIntA("bwvr", "hint_card_style", 1, path) != 0 ? 1 : 0;
    g_save = static_cast<int>(
        GetPrivateProfileIntA("bwvr", "debug_window_test_save", 0, path));
    GetPrivateProfileStringA("bwvr", "debug_test_parts", "", text,
                             sizeof(text), path);
  }
  g_test_count = ParseParts(text, g_tests, kMostTests);
  if (g_save > 0) {
    WinLog("debug_window_test_save=%d saves pictures of the "
           "window (a test)", g_save);
  }
  if (g_test_count > 0) {
    WinLog("debug_test_parts has %d parts for the word "
           "aimdebug (a test)", g_test_count);
  }
}

double NowMs() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart) * 1000.0 /
         static_cast<double>(frequency.QuadPart);
}

void Narrow(const wchar_t* in, char* out, size_t size) {
  size_t n = 0;
  for (; n + 1 < size && in != nullptr && in[n] != 0; ++n) {
    out[n] = (in[n] >= 32 && in[n] < 127) ? static_cast<char>(in[n]) : '?';
  }
  out[n] = 0;
}

// --- the state -------------------------------------------------------------------

kit::Room g_room = {};
// The trigger stays with the window after a press that closed it, until it
// goes up. The pump reads these, so they are LONGs.
volatile LONG g_holds = 0;
volatile LONG g_stick = 0;
bool g_hold_after = false;

// The group that shows, and its rows: the switch of each row, and its kind
// and value, latched once a frame.
int g_group = 0;
int g_rows = 0;
int g_switch[kMostRows];
unsigned char g_kind[kMostRows];
int g_value[kMostRows];

// The page of the group that shows (WindowRows::page), or null for a group
// of rows, and the list that drops down from a part of the page.
const WindowPage* g_page = nullptr;
bool g_list_open = false;
int g_list_part = -1;  // the part of the page whose list shows
PageList g_list = {};
int g_list_box[4] = {};  // the paper of the list, in pixels of the page

// The scroll: whole pixels for the picture, and the stick adds parts of a
// pixel.
int g_offset = 0;
float g_offset_f = 0.0f;
// The group and the offset of the list that does not show.
int g_group_of[kWindowSlots] = {};
int g_offset_of[kWindowSlots] = {};
bool g_drag = false;
int g_grab = 0;
bool g_stick_moving = false;
int g_stick_from = 0;
double g_last_ms = 0.0;

// The note: the row under the ray, since when, and the row whose note shows.
int g_rest_row = -1;
double g_rest_since = 0.0;
int g_note_row = -1;

// The words of the bottom row after a press, until then.
wchar_t g_status[128] = L"";
ULONGLONG g_status_until = 0;
const ULONGLONG kStatusMs = 2500;

uint64_t g_opens = 0, g_presses = 0, g_steps = 0, g_refused = 0;
uint64_t g_draws = 0, g_hover_draws = 0;
double g_draw_ms = 0.0, g_draw_most_ms = 0.0, g_hover_most_ms = 0.0;
int g_logs = 0;
const int kMaxLogs = 300;

bool LogRoom() {
  if (g_logs >= kMaxLogs) return false;
  ++g_logs;
  return true;
}

void SetHolds() {
  InterlockedExchange(&g_holds, (g_room.open || g_hold_after) ? 1 : 0);
}

int Groups() {
  const int n = g_src->groups();
  return n < kMostTabs ? n : kMostTabs;
}

// The kind of row of switch i of the list `src` at `value`.
unsigned char KindIn(const WindowRows* src, int i, int value) {
  int count = src->value_count(i);
  if (count < 0 && src->picture == nullptr) count = 0;
  int kind = KindOfRow(count, value == kDebugCannot);
  if (kind == kRowToggle && src->arrows != nullptr && src->arrows(i)) {
    kind = kRowArrows;
  }
  return static_cast<unsigned char>(kind);
}

unsigned char KindOf(int i, int value) { return KindIn(g_src, i, value); }

void Latch() {
  for (int r = 0; r < g_rows; ++r) {
    const int i = g_switch[r];
    g_value[r] = g_src->value(i);
    g_kind[r] = KindOf(i, g_value[r]);
  }
}

// The word of a row that cannot change: the word of the list, or "not now".
const wchar_t* DimWordIn(const WindowRows* src, int i) {
  const wchar_t* w = src->fixed_word != nullptr ? src->fixed_word(i) : nullptr;
  return w != nullptr ? w : L"not now";
}

const wchar_t* DimWord(int i) { return DimWordIn(g_src, i); }

// A band of a drawing over the row. dst is the left of the row, with the
// pitch of the picture of the window, and lines y0 to y1 of the band change.
void LayPictureIn(const WindowRows* rows, int i, int value, int style,
                  uint8_t* dst, int y0, int y1) {
  const uint8_t* src = rows->picture(i, value, style);
  if (src == nullptr) return;
  for (int y = y0; y < y1; ++y) {
    const uint8_t* s = src + static_cast<size_t>(y) * kWellWidth * 4;
    uint8_t* d = dst + static_cast<size_t>(y) * kWidth * 4;
    for (int x = 0; x < kWellWidth; ++x, s += 4, d += 4) {
      const int a = s[3];
      if (a == 0) continue;
      for (int k = 0; k < 3; ++k) {
        d[k] = static_cast<uint8_t>(s[k] + d[k] * (255 - a) / 255);
      }
    }
  }
}

void LayPicture(int i, int value, int style, uint8_t* dst, int y0, int y1) {
  LayPictureIn(g_src, i, value, style, dst, y0, y1);
}

void CloseList() {
  g_list_open = false;
  g_list_part = -1;
}

void FillGroup(int group) {
  g_group = group;
  g_rows = 0;
  g_page = g_src->page != nullptr ? g_src->page(group) : nullptr;
  CloseList();
  const int count = g_src->count();
  for (int i = 0; i < count && g_rows < kMostRows; ++i) {
    if (g_src->group(i) == group && g_src->in_window(i)) {
      g_switch[g_rows++] = i;
    }
  }
  g_offset = 0;
  g_offset_f = 0.0f;
  g_drag = false;
  g_stick_moving = false;
  g_rest_row = -1;
  g_note_row = -1;
  if (g_page != nullptr) g_rows = 0;
  Latch();
}

// The part under the ray. An open list takes the ray first: the paper of the
// list is no part, and a part out of the list only closes the list when the
// trigger presses it (Act).
int HitPart(float u, float v, void*) {
  if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) return -1;
  const float x = u * kWidth, y = v * kHeight;
  if (g_list_open) {
    const float px = x - kWellLeft, py = y - kWellTop;
    const int item = ListItemAt(g_list_box, g_list.count, px, py);
    if (item >= 0) return kFirstListPart + item;
    if (InList(g_list_box, px, py)) return -1;
  }
  if (g_page != nullptr) {
    int r[4];
    if (g_page->button != nullptr) {
      PageButtonRect(r);
      if (InRect(x, y, r)) return kPartButton;
    }
    int box[4];
    PageBox(box);
    if (x >= box[0] && x < box[2] && y >= box[1] && y < box[3]) {
      const int part = g_page->hit(x - kWellLeft, y - kWellTop);
      return part >= 0 && part < kPageParts ? kFirstPagePart + part : -1;
    }
  }
  return PartAt(u, v, Groups(), g_rows, g_kind, g_offset);
}

const char* PartName(int part, char* out, size_t size) {
  char label[40];
  if (part < 0) {
    sprintf_s(out, size, "no part");
  } else if (ListItemOf(part) >= 0) {
    const int item = ListItemOf(part);
    Narrow(item < g_list.count ? g_list.word[item] : L"", label,
           sizeof(label));
    sprintf_s(out, size, "the item %d \"%s\" of the list", item, label);
  } else if (PagePartOf(part) >= 0) {
    sprintf_s(out, size, "the part %d of the page", PagePartOf(part));
  } else if (part == kPartButton) {
    Narrow(g_page != nullptr && g_page->button != nullptr ? g_page->button
                                                          : L"",
           label, sizeof(label));
    sprintf_s(out, size, "the button %s", label);
  } else if (part < kMostTabs) {
    Narrow(g_src->group_name(part), label, sizeof(label));
    sprintf_s(out, size, "the tab %s", label);
  } else if (part == kPartClose) {
    sprintf_s(out, size, "Close");
  } else if (part == kPartThumb) {
    sprintf_s(out, size, "the thumb of the scroll bar");
  } else if (part == kPartTrack) {
    sprintf_s(out, size, "the track of the scroll bar");
  } else {
    const int r = RowOf(part);
    if (r < 0 || r >= g_rows) {
      sprintf_s(out, size, "no row");
      return out;
    }
    Narrow(g_src->label(g_switch[r]), label, sizeof(label));
    const int on = OnOf(part);
    sprintf_s(out, size, "%s%s (row %d)",
              on == kOnLeft    ? "the left arrow of "
              : on == kOnRight ? "the right arrow of "
                               : "",
              label, r);
  }
  return out;
}

void Close(const char* why, bool hold) {
  if (!g_room.open) return;
  kit::Close(&g_room);
  g_hold_after = hold;
  g_drag = false;
  g_note_row = -1;
  CloseList();
  SetHolds();
  InterlockedExchange(&g_stick, 0);
  WinLog("the window closes: %s, frame %llu", why,
         ControllersFrameCount());
}

// --- the picture -----------------------------------------------------------------

struct RowLook {
  int value;
  uint8_t kind;
  uint8_t row;    // a panel_kit::Look: all of a toggle row
  uint8_t left;   // the left arrow
  uint8_t right;  // the right arrow
};

bool SameRow(const RowLook& a, const RowLook& b) {
  return a.value == b.value && a.kind == b.kind && a.row == b.row &&
         a.left == b.left && a.right == b.right;
}

struct Look {
  int style;
  int slot;  // the list: WindowRows::slot
  int group;
  int rows;
  int offset;
  int note_row;
  int bar;  // the look of the thumb, or -1 with no scroll bar
  uint8_t tab[kMostTabs];
  uint8_t close;
  wchar_t status[128];
  RowLook row[kMostRows];
  // A page: what it shows (WindowPage::value), the part whose list shows,
  // the look of each item of the list, and the look of the button of the
  // page (kNoButton for none).
  int page;
  unsigned page_value;
  int list_part;
  uint8_t item[kListMost];
  uint8_t button;
  // The bottom row is a notice (WindowRows::idle_now): it has an icon.
  bool notice;
};
const uint8_t kNoButton = 255;
Look g_drawn;
bool g_have_drawn = false;
uint32_t g_serial = 0;  // the number of the picture (panel_kit::StageLayer)
Look g_saved_look;
bool g_have_saved = false;

kit::Canvas g_canvas = {};
kit::Tiles g_tab_tiles[kWindowSlots] = {};  // the tabs, then Close
const int kTabParts = kMostTabs + 1;
uint8_t* g_band = nullptr;     // the paper of a row, with no words
uint8_t* g_scratch = nullptr;  // a row while it is drawn
uint8_t* g_note = nullptr;     // the note while it is drawn
uint8_t* g_list_paper = nullptr;  // the paper of a list while it is drawn
const int kListPaperHeight = kListMost * kListItem + 2 * kListPad +
                             card_scroll::kPaperTop + card_scroll::kPaperBottom;
uint8_t* g_tile[kMostRows];    // each row of the group, as it shows now
RowLook g_tile_look[kMostRows];
bool g_tile_ok[kMostRows];
int g_tile_group = -1;
const size_t kTileBytes = static_cast<size_t>(kWellWidth) * kRowPitch * 4;
kit::Font g_title_font = {}, g_tab_font = {}, g_label_font = {},
          g_value_font = {}, g_status_font = {}, g_note_title = {},
          g_note_font = {}, g_small_font = {};
bool g_fonts_failed = false;
HFONT g_plain_font = nullptr, g_plain_small = nullptr, g_plain_tiny = nullptr;

card_scroll::Picture RowPicture(uint8_t* bgra) {
  const card_scroll::Picture p = {bgra, kWidth, kWellWidth, kRowPitch};
  return p;
}

// A rule of ink: the pixels of the box get darker.
void Rule(uint8_t* bgra, int pitch, int x0, int x1, int y0, int y1, int keep) {
  for (int y = y0; y < y1; ++y) {
    uint8_t* p = bgra + (static_cast<size_t>(y) * pitch + x0) * 4;
    for (int x = x0; x < x1; ++x, p += 4) {
      for (int k = 0; k < 3; ++k) p[k] = static_cast<uint8_t>(p[k] * keep / 256);
    }
  }
}

// The page: the fonts, the title, and a rule above and below the list. The
// kit has loaded the fonts of the cards (card_scroll::Start) before this.
void DrawPage(kit::Canvas* c, void*) {
  g_title_font = kit::WordsFont(c, 36);
  g_tab_font = kit::WordsFont(c, 22);
  g_label_font = kit::WordsFont(c, 24);
  g_value_font = kit::SealFont(c, 21);
  g_status_font = kit::WordsFont(c, 21);
  g_note_title = kit::WordsFont(c, 22);
  g_note_font = kit::WordsFont(c, 19);
  g_small_font = kit::WordsFont(c, 15);
  Rule(c->page, c->width, kWellLeft, kWellRight, kWellTop - 4, kWellTop - 1,
       150);
  Rule(c->page, c->width, kWellLeft, kWellRight, kWellBottom + 1,
       kWellBottom + 4, 150);
}

bool ParchmentStart() {
  if (g_fonts_failed) return false;
  if (!kit::StartParchment(&g_canvas, &DrawPage, nullptr)) return false;
  if (g_band == nullptr) {
    const size_t bytes = static_cast<size_t>(kWidth) * kRowPitch * 4;
    g_band = new (std::nothrow) uint8_t[bytes];
    g_scratch = new (std::nothrow) uint8_t[bytes];
    g_note = new (std::nothrow)
        uint8_t[static_cast<size_t>(kWidth) * kNoteMost * 4];
    g_list_paper = new (std::nothrow)
        uint8_t[static_cast<size_t>(kWidth) * kListPaperHeight * 4];
    if (g_band != nullptr) {
      memset(g_band, 0, bytes);
      // The paper of the first row of the list.
      for (int y = 0; y < kRowPitch; ++y) {
        memcpy(g_band + static_cast<size_t>(y) * kWidth * 4,
               g_canvas.page +
                   (static_cast<size_t>(kWellTop + y) * kWidth + kWellLeft) *
                       4,
               static_cast<size_t>(kWellWidth) * 4);
      }
    }
  }
  if (g_title_font.font == nullptr || g_tab_font.font == nullptr ||
      g_label_font.font == nullptr || g_value_font.font == nullptr ||
      g_status_font.font == nullptr || g_note_title.font == nullptr ||
      g_note_font.font == nullptr || g_small_font.font == nullptr ||
      g_band == nullptr || g_list_paper == nullptr ||
      g_scratch == nullptr || g_note == nullptr ||
      !kit::MakeTiles(&g_tab_tiles[0], kTabParts, kit::kLooks) ||
      !kit::MakeTiles(&g_tab_tiles[1], kTabParts, kit::kLooks)) {
    g_fonts_failed = true;
    WinLog("no font or no memory for the parchment, so the "
           "window is dark");
    return false;
  }
  return true;
}

void TabRectOf(int part, int r[4]) {
  if (part == kMostTabs) {
    CloseRect(r);
  } else {
    TabRect(part, r);
  }
}

// context is the list of the tab (the warm), or null for the list that
// shows.
void DrawTab(kit::Canvas* c, int part, int look, void* context) {
  const WindowRows* rows =
      context != nullptr ? static_cast<const WindowRows*>(context) : g_src;
  int r[4];
  TabRectOf(part, r);
  kit::Button(c, r, look, part == kMostTabs ? L"Close" : rows->group_name(part),
              part == kMostTabs ? g_label_font : g_tab_font, part * 7 + 3);
}

// Row r of the group in its look, for switch i of the list `src`: into the
// picture of its row. `rule` puts a rule at its foot.
void DrawRowOf(const WindowRows* src, int i, bool rule, int r,
               const RowLook& look) {
  const card_scroll::Picture to = RowPicture(g_scratch);
  for (int y = 0; y < kRowPitch; ++y) {
    memcpy(g_scratch + static_cast<size_t>(y) * kWidth * 4,
           g_band + static_cast<size_t>(y) * kWidth * 4,
           static_cast<size_t>(kWellWidth) * 4);
  }
  const int seed = i * 5 + 1;
  int box[4];
  LabelBox(box);
  const wchar_t* small = src->row_note != nullptr ? src->row_note(i) : nullptr;
  if (look.kind == kRowPicture) {
    LayPictureIn(src, i, look.value, 1, g_scratch, 0, kRowPitch);
  } else if (small != nullptr) {
    // The label in the top part of the box, the small words under it.
    const int top[4] = {box[0], box[1] - 4, box[2], box[1] + 32};
    kit::InkWordsIn(&g_canvas, to, g_label_font, src->label(i), top, seed,
                    kit::kLeftAlign);
    const int under[4] = {box[0], box[1] + 34, box[2], box[3] + 4};
    kit::InkWordsIn(&g_canvas, to, g_small_font, small, under, seed + 4,
                    kit::kLeftAlign);
  } else {
    kit::InkWordsIn(&g_canvas, to, g_label_font, src->label(i), box, seed,
                    kit::kLeftAlign);
  }
  wchar_t word[32];
  if (look.kind == kRowPicture) {
    // A picture has no control.
  } else if (look.kind == kRowDim) {
    ValueBox(kRowArrows, box);
    kit::InkWordsIn(&g_canvas, to, g_value_font, DimWordIn(src, i), box,
                    seed + 1, kit::kCenter);
    const int all[4] = {0, 0, kWellWidth, kRowPitch - 3};
    const card_scroll::Picture under = RowPicture(g_band);
    kit::FadeBox(to, under, all);
  } else if (look.kind == kRowToggle) {
    src->value_text(i, look.value, word, 32);
    ValueBox(kRowToggle, box);
    kit::InkWordsIn(&g_canvas, to, g_value_font, word, box, seed + 1,
                    kit::kRightAlign);
    int pill[4];
    PillRect(pill);
    kit::Toggle(&g_canvas, to, pill, look.row, look.value != 0, seed + 2);
  } else {
    src->value_text(i, look.value, word, 32);
    ValueBox(kRowArrows, box);
    kit::InkWordsIn(&g_canvas, to, g_value_font, word, box, seed + 1,
                    kit::kCenter);
    int a[4];
    LeftArrowRect(a);
    kit::ArrowKey(&g_canvas, to, a, look.left, true, seed + 2);
    RightArrowRect(a);
    kit::ArrowKey(&g_canvas, to, a, look.right, false, seed + 3);
  }
  if (rule) {
    Rule(g_scratch, kWidth, 10, kWellWidth - 10, kRowPitch - 3, kRowPitch - 1,
         200);
  }
  if (g_tile[r] == nullptr) g_tile[r] = new (std::nothrow) uint8_t[kTileBytes];
  if (g_tile[r] == nullptr) return;
  const size_t row = static_cast<size_t>(kWellWidth) * 4;
  for (int y = 0; y < kRowPitch; ++y) {
    memcpy(g_tile[r] + y * row, g_scratch + static_cast<size_t>(y) * kWidth * 4,
           row);
  }
  g_tile_look[r] = look;
  g_tile_ok[r] = true;
}

void DrawRow(int r, const RowLook& look) {
  // A rule at the foot of the row: the join of two rows is under it. The
  // bands of one picture have no rule between them.
  const bool rule = look.kind != kRowPicture ||
                    (r + 1 < g_rows && g_kind[r + 1] != kRowPicture);
  DrawRowOf(g_src, g_switch[r], rule, r, look);
}

// The note of row r over the card. The note of the last switch is kept, so
// a change of the row under it lays the same note again.
int g_note_switch = -1;
bool g_note_style = false;
void DrawNote(int r, int offset) {
  const int i = g_switch[r];
  wchar_t key[160];
  g_src->last_line(i, key, 160);
  const wchar_t* lines[3] = {g_src->name(i), g_src->tip(i), key};
  // A note with no last line has two lines.
  const int count = key[0] != 0 ? 3 : 2;
  int height = kit::NoteHeight(&g_canvas, kNoteWidth, lines, count,
                               g_note_title, g_note_font);
  if (height > kNoteMost) height = kNoteMost;
  int box[4];
  NotePlace(RowTop(r, offset), height, box);
  const card_scroll::Picture note = {g_note, kWidth, kNoteWidth, height};
  const int which = g_src->slot * 1000 + i;
  if (g_note_switch != which || g_note_style != g_canvas.parchment_ready) {
    kit::Note(&g_canvas, note, lines, count, g_note_title, g_note_font,
              i + 11);
    g_note_switch = which;
    g_note_style = g_canvas.parchment_ready;
  }
  kit::Over(&g_canvas, note, box[0], box[1]);
}

void BarBox(int out[4]) {
  out[0] = kBarLeft - kit::kRim;
  out[1] = kWellTop - kit::kRim;
  out[2] = kBarRight + kit::kRim;
  out[3] = kWellBottom + kit::kRim;
}

// THE LIST of a page: a small scroll of paper over the page, with a key for
// each item. The item in use is bright gold, as the tab that shows, and the
// item under the ray is gold.
void DrawList(const Look& look) {
  kit::Canvas* c = &g_canvas;
  int page[4];
  PageBox(page);
  const int x0 = page[0] + g_list_box[0], y0 = page[1] + g_list_box[1];
  const int x1 = page[0] + g_list_box[2], y1 = page[1] + g_list_box[3];
  const int w = x1 - x0 + card_scroll::kEdgeLeft + card_scroll::kEdgeRight;
  const int h = y1 - y0 + card_scroll::kPaperTop + card_scroll::kPaperBottom;
  if (h > kListPaperHeight || w > kWidth) return;
  const card_scroll::Picture paper = {g_list_paper, kWidth, w, h};
  card_scroll::Paper(paper, card_scroll::kEdgeLeft, card_scroll::kPaperTop,
                     card_scroll::kEdgeLeft + (x1 - x0),
                     card_scroll::kPaperTop + (y1 - y0), 17);
  kit::Over(c, paper, x0 - card_scroll::kEdgeLeft, y0 - card_scroll::kPaperTop);
  const card_scroll::Picture card = kit::CardPicture(c);
  for (int i = 0; i < g_list.count && i < kListMost; ++i) {
    int r[4];
    ListItemRect(g_list_box, i, r);
    const int b[4] = {page[0] + r[0], page[1] + r[1], page[0] + r[0] + r[2],
                      page[1] + r[1] + r[3]};
    const int item = look.item[i];
    if (item != kit::kPlain) {
      card_scroll::Key(card, b[0], b[1], b[2], b[3],
                       item == kit::kHover ? card_scroll::kKeyHover
                                           : card_scroll::kKeyFlash,
                       i * 3 + 51);
    }
    kit::InkWordsIn(c, card, g_status_font, g_list.word[i], b, i * 3 + 52,
                    kit::kCenter);
  }
}

// THE ICON OF A NOTICE: a round arrow of red ink (the wax of the seals),
// as the sign of a restart, at cx, cy, with 4 by 4 samples a pixel.
void NoticeIcon(const card_scroll::Picture& to, float cx, float cy) {
  const float radius = 12.0f, half = 2.1f;
  const float red[3] = {150.0f, 38.0f, 26.0f};
  // The arc goes from 50 to 320 degrees (y goes down), and the head of the
  // arrow is at its end.
  const float end = 320.0f * 3.14159265f / 180.0f;
  const float hx = cx + radius * cosf(end), hy = cy + radius * sinf(end);
  // The head points along the arc: the tangent at its end.
  const float tx = -sinf(end), ty = cosf(end);
  const float nx = cosf(end), ny = sinf(end);
  const float tip[2] = {hx + tx * 8.0f, hy + ty * 8.0f};
  const float b1[2] = {hx + nx * 6.5f, hy + ny * 6.5f};
  const float b2[2] = {hx - nx * 6.5f, hy - ny * 6.5f};
  const auto inside = [](const float p[2], const float a[2], const float b[2],
                         const float c[2]) {
    const auto side = [](const float* u, const float* v, const float* w) {
      return (v[0] - u[0]) * (w[1] - u[1]) - (v[1] - u[1]) * (w[0] - u[0]);
    };
    const float d1 = side(a, b, p), d2 = side(b, c, p), d3 = side(c, a, p);
    return (d1 >= 0 && d2 >= 0 && d3 >= 0) || (d1 <= 0 && d2 <= 0 && d3 <= 0);
  };
  for (int y = static_cast<int>(cy - 17); y <= static_cast<int>(cy + 17); ++y) {
    for (int x = static_cast<int>(cx - 17); x <= static_cast<int>(cx + 17);
         ++x) {
      if (x < 0 || y < 0 || x >= to.width || y >= to.height) continue;
      int in = 0;
      for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
          const float p[2] = {x + (sx + 0.5f) / 4.0f, y + (sy + 0.5f) / 4.0f};
          const float dx = p[0] - cx, dy = p[1] - cy;
          float angle = atan2f(dy, dx);
          if (angle < 0.0f) angle += 6.2831853f;
          const float r = sqrtf(dx * dx + dy * dy);
          const bool arc = fabsf(r - radius) <= half &&
                           angle >= 50.0f * 3.14159265f / 180.0f &&
                           angle <= end;
          if (arc || inside(p, tip, b1, b2)) ++in;
        }
      }
      if (in == 0) continue;
      const float a = in / 16.0f;
      uint8_t* d = to.bgra + (static_cast<size_t>(y) * to.pitch + x) * 4;
      d[0] = static_cast<uint8_t>(red[2] * a + d[0] * (1.0f - a) + 0.5f);
      d[1] = static_cast<uint8_t>(red[1] * a + d[1] * (1.0f - a) + 0.5f);
      d[2] = static_cast<uint8_t>(red[0] * a + d[2] * (1.0f - a) + 0.5f);
    }
  }
}

// The words of the bottom row in the box: one line in the middle of the
// box, or two lines of a smaller font when they do not fit in one.
void StatusWords(const card_scroll::Picture& to, const wchar_t* words,
                 const int box[4]) {
  kit::Canvas* c = &g_canvas;
  const int one = kit::InkLines(c, to, g_status_font, L"Ag", box, 13, false);
  if (kit::InkLines(c, to, g_status_font, words, box, 13, false) <= one) {
    kit::InkWordsIn(c, to, g_status_font, words, box, 13, kit::kLeftAlign);
    return;
  }
  const int all = kit::InkLines(c, to, g_note_font, words, box, 13, false);
  const int top = box[1] + (box[3] - box[1] - all) / 2;
  const int b[4] = {box[0], top < box[1] ? box[1] : top, box[2], box[3]};
  kit::InkLines(c, to, g_note_font, words, b, 13, true);
}

// The parchment look: the parts that changed. The log says when only the
// looks of the parts under the ray changed: that is the cost of a hover.
void Compose(const Look& look, uint64_t frame) {
  const double start = NowMs();
  kit::Canvas* c = &g_canvas;
  const bool first =
      !g_have_drawn || g_drawn.style != 1 || g_drawn.slot != look.slot;
  if (first) {
    kit::ClearCard(c);
    // The title of the list, on the card.
    const int title[4] = {kLeft, kTitleTop, kRight, kTitleBottom};
    kit::InkWordsIn(c, kit::CardPicture(c), g_title_font, g_src->title, title,
                    5, kit::kCenter);
  }
  const int tile_group = look.slot * 100 + look.group;
  if (g_tile_group != tile_group) {
    for (int r = 0; r < kMostRows; ++r) g_tile_ok[r] = false;
    g_tile_group = tile_group;
  }
  int drawn = 0, copied = 0;
  for (int part = 0; part < kTabParts; ++part) {
    if (part < kMostTabs && part >= Groups()) continue;
    const int now = part == kMostTabs ? look.close : look.tab[part];
    const int was = part == kMostTabs ? g_drawn.close : g_drawn.tab[part];
    if (!first && now == was) continue;
    int r[4], box[4];
    TabRectOf(part, r);
    panel_kit_math::RectBox(r, kit::kRim, kWidth, kHeight, box);
    if (kit::ShowPart(c, &g_tab_tiles[look.slot], part, now, box, &DrawTab,
                      nullptr)) {
      ++copied;
    } else {
      ++drawn;
    }
  }
  if (first || wcscmp(look.status, g_drawn.status) != 0 ||
      look.button != g_drawn.button || look.notice != g_drawn.notice) {
    int n[4];
    NoteBox(n);
    kit::PageToCard(c, n);
    if (look.button != kNoButton && g_page != nullptr) {
      int r[4];
      PageButtonRect(r);
      kit::Button(c, r, look.button, g_page->button, g_label_font, 41);
    }
    NoteBoxBeside(look.button != kNoButton, n);
    if (look.notice) {
      NoticeIcon(kit::CardPicture(c), n[0] + 15.0f,
                 (n[1] + n[3]) * 0.5f);
      n[0] += 40;
    }
    StatusWords(kit::CardPicture(c), look.status, n);
    ++drawn;
  }
  if (look.page == 0 &&
      (first || look.bar != g_drawn.bar || look.page != g_drawn.page ||
       (look.bar >= 0 && look.offset != g_drawn.offset))) {
    int b[4];
    BarBox(b);
    kit::PageToCard(c, b);
    int track[4], thumb[4];
    if (look.bar >= 0 && BarBoxes(look.rows, look.offset, track, thumb)) {
      kit::ScrollBar(c, kit::CardPicture(c), track, thumb, look.bar, 21);
    }
    ++drawn;
  }
  if (look.page != 0 && g_page != nullptr) {
    if (first || look.page != g_drawn.page || look.group != g_drawn.group ||
        look.page_value != g_drawn.page_value ||
        look.list_part != g_drawn.list_part ||
        memcmp(look.item, g_drawn.item, sizeof(look.item)) != 0) {
      int box[4];
      PageBox(box);
      kit::PageToCard(c, box);
      const int hover = g_list_open ? -1 : PagePartOf(g_room.hover);
      g_page->draw(c, box, 1, hover, look.list_part,
                   PagePartOf(kit::Flashing(&g_room)));
      if (look.list_part >= 0) DrawList(look);
      ++drawn;
    }
  }
  bool well = look.page == 0 &&
              (first || look.group != g_drawn.group ||
               look.page != g_drawn.page || look.offset != g_drawn.offset ||
               look.note_row != g_drawn.note_row);
  for (int r = 0; r < look.rows && !well && look.page == 0; ++r) {
    const int top = RowTop(r, look.offset);
    if (top + kRowPitch <= kWellTop || top >= kWellBottom) continue;
    if (!SameRow(look.row[r], g_drawn.row[r])) well = true;
  }
  if (well) {
    const size_t line = static_cast<size_t>(kWellWidth) * 4;
    for (int y = kWellTop; y < kWellBottom; ++y) {
      const int along = y - kWellTop + look.offset;
      const int r = along / kRowPitch;
      if (r >= look.rows) {
        memcpy(c->card + (static_cast<size_t>(y) * kWidth + kWellLeft) * 4,
               c->page + (static_cast<size_t>(y) * kWidth + kWellLeft) * 4,
               line);
        continue;
      }
      const int ly = along - r * kRowPitch;
      if (ly == 0 || y == kWellTop) {
        if (!g_tile_ok[r] || !SameRow(g_tile_look[r], look.row[r])) {
          DrawRow(r, look.row[r]);
          ++drawn;
        } else {
          ++copied;
        }
      }
      if (g_tile[r] == nullptr) continue;
      memcpy(c->card + (static_cast<size_t>(y) * kWidth + kWellLeft) * 4,
             g_tile[r] + ly * line, line);
    }
    if (look.note_row >= 0 && look.note_row < look.rows) {
      DrawNote(look.note_row, look.offset);
      ++drawn;
    }
  }
  const double ms = NowMs() - start;
  const bool hover_only =
      !first && look.slot == g_drawn.slot && look.group == g_drawn.group &&
      look.offset == g_drawn.offset && look.note_row == g_drawn.note_row &&
      wcscmp(look.status, g_drawn.status) == 0;
  ++g_draws;
  g_draw_ms += ms;
  if (ms > g_draw_most_ms) g_draw_most_ms = ms;
  if (hover_only) {
    ++g_hover_draws;
    if (ms > g_hover_most_ms) g_hover_most_ms = ms;
  }
  if (LogRoom() && g_draws <= 60) {
    WinLog("frame %llu, a new picture of the parchment window "
           "took %.2f ms: %d parts drawn and %d parts copied%s", frame, ms,
           drawn, copied, hover_only ? " (a change under the ray)" : "");
  }
}

// THE DARK LOOK (hint_card_style=0): all of it each time that it changes.
void PlainWords(HFONT font, COLORREF color, const wchar_t* words,
                const int box[4], UINT side) {
  kit::Canvas* c = &g_canvas;
  SelectObject(c->dc, font);
  SetTextColor(c->dc, color);
  RECT rect = {box[0], box[1], box[2], box[3]};
  DrawTextW(c->dc, words, -1, &rect,
            side | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void RasterPlain(const Look& look) {
  kit::Canvas* c = &g_canvas;
  if (g_plain_font == nullptr) {
    const auto font = [](int em, int weight) {
      return CreateFontW(-em, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                         ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
                         L"Segoe UI");
    };
    g_plain_font = font(36, FW_SEMIBOLD);
    g_plain_small = font(26, FW_NORMAL);
    g_plain_tiny = font(19, FW_NORMAL);
  }
  const int all[4] = {0, 0, kWidth, kHeight};
  kit::PlainFill(c, all, RGB(22, 24, 28));
  const int title[4] = {0, kTitleTop, kWidth, kTitleBottom};
  kit::PlainText(c, g_plain_font, RGB(235, 235, 235), g_src->title, title);
  for (int g = 0; g < Groups(); ++g) {
    int r[4];
    TabRect(g, r);
    kit::PlainButton(c, r, look.tab[g], g_src->group_name(g), g_plain_small);
  }
  int r[4];
  CloseRect(r);
  kit::PlainButton(c, r, look.close, L"Close", g_plain_small);
  int n[4];
  NoteBoxBeside(look.button != kNoButton, n);
  PlainWords(look.button != kNoButton ? g_plain_tiny : g_plain_small,
             RGB(230, 190, 110), look.status, n, DT_LEFT);
  if (look.button != kNoButton && g_page != nullptr) {
    int b[4];
    PageButtonRect(b);
    kit::PlainButton(c, b, look.button, g_page->button, g_plain_small);
  }
  if (look.page != 0 && g_page != nullptr) {
    int box[4];
    PageBox(box);
    kit::PlainFill(c, box, RGB(30, 33, 38));
    GdiFlush();
    g_page->draw(c, box, 0, g_list_open ? -1 : PagePartOf(g_room.hover),
                 look.list_part, PagePartOf(kit::Flashing(&g_room)));
    if (look.list_part >= 0) {
      const int paper[4] = {box[0] + g_list_box[0], box[1] + g_list_box[1],
                            box[0] + g_list_box[2], box[1] + g_list_box[3]};
      kit::PlainFill(c, paper, RGB(70, 62, 44));
      const int inner[4] = {paper[0] + 2, paper[1] + 2, paper[2] - 2,
                            paper[3] - 2};
      kit::PlainFill(c, inner, RGB(36, 34, 30));
      for (int i = 0; i < g_list.count && i < kListMost; ++i) {
        int item[4];
        ListItemRect(g_list_box, i, item);
        const int at[4] = {box[0] + item[0], box[1] + item[1], item[2],
                           item[3]};
        kit::PlainButton(c, at, look.item[i], g_list.word[i], g_plain_small);
      }
    }
    kit::PlainDone(c);
    return;
  }
  // The rows, in the list only.
  HRGN clip = CreateRectRgn(kWellLeft, kWellTop, kWellRight, kWellBottom);
  SelectClipRgn(c->dc, clip);
  const int well[4] = {kWellLeft, kWellTop, kWellRight, kWellBottom};
  kit::PlainFill(c, well, RGB(30, 33, 38));
  for (int row = 0; row < look.rows; ++row) {
    const int top = RowTop(row, look.offset);
    if (top + kRowPitch <= kWellTop || top >= kWellBottom) continue;
    const RowLook& l = look.row[row];
    const int i = g_switch[row];
    if (l.kind == kRowPicture) {
      // The band over the dark list, in the lines of the list only.
      GdiFlush();
      const int y0 = top < kWellTop ? kWellTop - top : 0;
      const int y1 = top + kRowPitch > kWellBottom ? kWellBottom - top
                                                   : kRowPitch;
      uint8_t* dst = reinterpret_cast<uint8_t*>(c->bits) +
                     (static_cast<size_t>(top) * kWidth + kWellLeft) * 4;
      LayPicture(i, l.value, 0, dst, y0, y1);
      continue;
    }
    const bool dim = l.kind == kRowDim;
    int b[4];
    LabelBox(b);
    const int label[4] = {kWellLeft + b[0], top + b[1], kWellLeft + b[2],
                          top + b[3]};
    const wchar_t* small = g_src->row_note != nullptr ? g_src->row_note(i)
                                                      : nullptr;
    if (small != nullptr) {
      const int upper[4] = {label[0], label[1] - 4, label[2], label[1] + 32};
      PlainWords(g_plain_small, dim ? RGB(110, 110, 110) : RGB(235, 235, 235),
                 g_src->label(i), upper, DT_LEFT);
      const int under[4] = {label[0], label[1] + 32, label[2], label[3] + 4};
      PlainWords(g_plain_tiny, RGB(160, 160, 160), small, under, DT_LEFT);
    } else {
      PlainWords(g_plain_small, dim ? RGB(110, 110, 110) : RGB(235, 235, 235),
                 g_src->label(i), label, DT_LEFT);
    }
    wchar_t word[32];
    g_src->value_text(i, l.value, word, 32);
    ValueBox(dim ? kRowArrows : l.kind, b);
    const int value[4] = {kWellLeft + b[0], top + b[1], kWellLeft + b[2],
                          top + b[3]};
    if (dim) {
      PlainWords(g_plain_small, RGB(110, 110, 110), DimWord(i), value,
                 DT_CENTER);
    } else if (l.kind == kRowToggle) {
      PlainWords(g_plain_small, RGB(230, 190, 110), word, value, DT_RIGHT);
      int p[4];
      PillRect(p);
      const int pill[4] = {kWellLeft + p[0], top + p[1],
                           kWellLeft + p[0] + p[2], top + p[1] + p[3]};
      kit::PlainFill(c, pill, l.row == kit::kHover   ? RGB(205, 172, 96)
                              : l.row == kit::kFlash ? RGB(250, 225, 150)
                                                     : RGB(62, 66, 76));
      const int half = p[2] / 2;
      const int knob[4] = {l.value != 0 ? pill[0] + half + 4 : pill[0] + 4,
                           pill[1] + 4,
                           l.value != 0 ? pill[2] - 4 : pill[0] + half - 4,
                           pill[3] - 4};
      kit::PlainFill(c, knob,
                     l.value != 0 ? RGB(70, 170, 90) : RGB(150, 60, 56));
    } else {
      PlainWords(g_plain_small, RGB(230, 190, 110), word, value, DT_CENTER);
      int a[4];
      LeftArrowRect(a);
      const int left[4] = {kWellLeft + a[0], top + a[1], a[2], a[3]};
      kit::PlainButton(c, left, l.left, L"<", g_plain_small);
      RightArrowRect(a);
      const int right[4] = {kWellLeft + a[0], top + a[1], a[2], a[3]};
      kit::PlainButton(c, right, l.right, L">", g_plain_small);
    }
    const int line[4] = {kWellLeft + 10, top + kRowPitch - 2,
                         kWellRight - 10, top + kRowPitch - 1};
    kit::PlainFill(c, line, RGB(52, 56, 64));
  }
  SelectClipRgn(c->dc, nullptr);
  DeleteObject(clip);
  int track[4], thumb[4];
  if (look.bar >= 0 && BarBoxes(look.rows, look.offset, track, thumb)) {
    kit::PlainFill(c, track, RGB(40, 42, 48));
    const int t[4] = {thumb[0] + 4, thumb[1] + 4, thumb[2] - 4, thumb[3] - 4};
    kit::PlainFill(c, t, look.bar == kit::kHover   ? RGB(205, 172, 96)
                         : look.bar == kit::kFlash ? RGB(250, 225, 150)
                                                   : RGB(90, 96, 108));
  }
  if (look.note_row >= 0 && look.note_row < look.rows) {
    const int i = g_switch[look.note_row];
    wchar_t text[640], last[160];
    g_src->last_line(i, last, 160);
    swprintf_s(text, L"%s\n%s\n%s", g_src->name(i), g_src->tip(i), last);
    SelectObject(c->dc, g_plain_small);
    RECT measure = {0, 0, kNoteWidth - 32, 1};
    const int h = DrawTextW(c->dc, text, -1, &measure,
                            DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT) + 28;
    int box[4];
    NotePlace(RowTop(look.note_row, look.offset), h, box);
    kit::PlainFill(c, box, RGB(70, 62, 44));
    const int inner[4] = {box[0] + 2, box[1] + 2, box[2] - 2, box[3] - 2};
    kit::PlainFill(c, inner, RGB(36, 34, 30));
    SetTextColor(c->dc, RGB(240, 230, 200));
    RECT rect = {box[0] + 16, box[1] + 14, box[2] - 16, box[3] - 8};
    DrawTextW(c->dc, text, -1, &rect, DT_WORDBREAK | DT_NOPREFIX | DT_LEFT);
  }
  kit::PlainDone(c);
}

void BuildLook(Look* look) {
  memset(look, 0, sizeof(*look));
  look->style = kit::CardStyle(g_style_ini);
  look->slot = g_src->slot;
  look->group = g_group;
  look->rows = g_rows;
  look->offset = g_offset;
  look->note_row = g_note_row;
  const int flash = kit::Flashing(&g_room);
  const int hover = g_room.hover;
  for (int g = 0; g < kMostTabs; ++g) {
    look->tab[g] = static_cast<uint8_t>(
        g == g_group ? (hover == g ? kit::kHover : kit::kOn)
                     : (g == flash ? kit::kFlash
                                   : (g == hover ? kit::kHover : kit::kPlain)));
  }
  look->close = static_cast<uint8_t>(
      kPartClose == flash ? kit::kFlash
                          : (kPartClose == hover ? kit::kHover : kit::kPlain));
  int track[4], thumb[4];
  look->bar = -1;
  if (BarBoxes(g_rows, g_offset, track, thumb)) {
    look->bar = g_drag ? kit::kFlash
                       : (hover == kPartThumb ? kit::kHover : kit::kPlain);
  }
  const int hover_row = RowOf(hover);
  const int flash_row = RowOf(flash);
  for (int r = 0; r < g_rows; ++r) {
    RowLook& l = look->row[r];
    l.value = g_value[r];
    l.kind = g_kind[r];
    const auto part_look = [&](int on) -> uint8_t {
      if (flash_row == r && OnOf(flash) == on) return kit::kFlash;
      if (hover_row == r && OnOf(hover) == on) return kit::kHover;
      return kit::kPlain;
    };
    if (l.kind == kRowToggle) {
      l.row = static_cast<uint8_t>(
          flash_row == r ? kit::kFlash
                         : (hover_row == r ? kit::kHover : kit::kPlain));
    } else if (l.kind == kRowArrows) {
      l.left = part_look(kOnLeft);
      l.right = part_look(kOnRight);
    }
  }
  // The page: what it shows, its list and its button.
  look->button = kNoButton;
  look->list_part = -1;
  if (g_page != nullptr) {
    look->page = 1;
    look->list_part = g_list_open ? g_list_part : -1;
    for (int i = 0; i < kListMost; ++i) {
      look->item[i] = static_cast<uint8_t>(
          kFirstListPart + i == flash ? kit::kFlash
          : kFirstListPart + i == hover ? kit::kHover
          : i == g_list.now && g_list_open ? kit::kOn
                                           : kit::kPlain);
    }
    look->page_value = g_page->value(g_list_open ? -1 : PagePartOf(hover),
                                     look->list_part, PagePartOf(flash));
    if (g_page->button != nullptr) {
      look->button = static_cast<uint8_t>(
          !g_page->button_can()   ? kit::kDim
          : kPartButton == flash ? kit::kFlash
          : kPartButton == hover ? kit::kHover
                                 : kit::kPlain);
    }
  }
  const wchar_t* idle_now =
      g_src->idle_now != nullptr ? g_src->idle_now() : nullptr;
  if (g_status[0] != 0 && GetTickCount64() < g_status_until) {
    wcsncpy_s(look->status, g_status, _TRUNCATE);
  } else if (g_page != nullptr && g_list_open) {
    wcscpy_s(look->status, L"Point at a job and pull the trigger.");
  } else if (g_page != nullptr && PagePartOf(hover) >= 0) {
    g_page->status(PagePartOf(hover), look->status, 128);
  } else if (g_page != nullptr && hover == kPartButton &&
             !g_page->button_can()) {
    wcscpy_s(look->status, L"Each button has its default job now.");
  } else if (idle_now != nullptr) {
    wcsncpy_s(look->status, idle_now, _TRUNCATE);
    look->notice = true;
  } else if (g_page != nullptr) {
    g_page->status(-1, look->status, 128);
  } else if (hover_row >= 0 && hover_row < g_rows &&
             g_kind[hover_row] == kRowDim) {
    const int i = g_switch[hover_row];
    if (g_src->fixed_word != nullptr && g_src->fixed_word(i) != nullptr) {
      swprintf_s(look->status, L"%s stays on its job", g_src->label(i));
    } else {
      swprintf_s(look->status, L"%s cannot change now", g_src->label(i));
    }
  } else {
    wcscpy_s(look->status, g_src->idle);
  }
}

bool SameLook(const Look& a, const Look& b) {
  if (a.page != b.page || a.page_value != b.page_value ||
      a.notice != b.notice ||
      a.list_part != b.list_part || a.button != b.button ||
      memcmp(a.item, b.item, sizeof(a.item)) != 0) {
    return false;
  }
  if (a.style != b.style || a.slot != b.slot || a.group != b.group ||
      a.rows != b.rows ||
      a.offset != b.offset || a.note_row != b.note_row || a.bar != b.bar ||
      a.close != b.close || memcmp(a.tab, b.tab, sizeof(a.tab)) != 0 ||
      wcscmp(a.status, b.status) != 0) {
    return false;
  }
  for (int r = 0; r < a.rows; ++r) {
    if (!SameRow(a.row[r], b.row[r])) return false;
  }
  return true;
}

// A picture for debug_window_test_save: the first one, and one with a new
// group, a new value, a note, or a scroll of 3 rows or more.
bool WorthSaving(const Look& look) {
  if (!g_have_saved) return true;
  const Look& s = g_saved_look;
  if (look.group != s.group || look.style != s.style || look.slot != s.slot) {
    return true;
  }
  if (look.note_row >= 0 && look.note_row != s.note_row) return true;
  if (look.page != 0 &&
      (look.page_value != s.page_value || look.list_part != s.list_part ||
       memcmp(look.item, s.item, sizeof(look.item)) != 0)) {
    return true;
  }
  const int moved = look.offset - s.offset;
  if (moved >= 3 * kRowPitch || -moved >= 3 * kRowPitch) return true;
  for (int r = 0; r < look.rows; ++r) {
    if (look.row[r].value != s.row[r].value) return true;
  }
  return false;
}

// --- a press ---------------------------------------------------------------------

void SetStatus(const wchar_t* words) {
  wcsncpy_s(g_status, words, _TRUNCATE);
  g_status_until = GetTickCount64() + kStatusMs;
}

void Act(int part, uint64_t frame) {
  char name[64];
  PartName(part, name, sizeof(name));
  ++g_presses;
  // An open list: an item picks a job, and a press out of the list closes
  // it and does no more.
  if (g_list_open) {
    const int item = ListItemOf(part);
    const int owner = g_list_part;
    CloseList();
    if (item < 0 || item >= g_list.count || g_page == nullptr) {
      WinLog("the trigger presses %s, and the list closes, frame %llu", name,
             frame);
      return;
    }
    kit::Pressed(&g_room, kFirstPagePart + owner);
    wchar_t words[128] = L"";
    if (g_page->choose(owner, item, words, 128)) {
      ++g_steps;
      SetStatus(words);
    }
    WinLog("the trigger presses %s, frame %llu", name, frame);
    return;
  }
  if (part == kPartButton && g_page != nullptr && g_page->button != nullptr) {
    if (!g_page->button_can()) {
      ++g_refused;
      HapticsPulse(haptics_math::kRefused);
      WinLog("the trigger presses %s, which does nothing now, frame %llu",
             name, frame);
      return;
    }
    kit::Pressed(&g_room, part);
    wchar_t words[128] = L"";
    g_page->button_press(words, 128);
    ++g_steps;
    SetStatus(words);
    WinLog("the trigger presses %s, frame %llu", name, frame);
    return;
  }
  if (PagePartOf(part) >= 0 && g_page != nullptr) {
    PageList list;
    if (!g_page->list(PagePartOf(part), &list) || list.count <= 0) {
      ++g_refused;
      HapticsPulse(haptics_math::kRefused);
      WinLog("the trigger presses %s, which has no list, frame %llu", name,
             frame);
      return;
    }
    kit::Pressed(&g_room, part);
    g_list = list;
    g_list_part = PagePartOf(part);
    g_list_open = true;
    ListPlace(list.box, list.count, g_list_box);
    WinLog("the trigger presses %s: its list of %d items opens, frame %llu",
           name, list.count, frame);
    return;
  }
  if (part < kMostTabs) {
    kit::Pressed(&g_room, part);
    if (part != g_group) {
      FillGroup(part);
      char group[32];
      Narrow(g_src->group_name(part), group, sizeof(group));
      WinLog("the trigger presses the tab %s: %d switches, "
             "frame %llu", group, g_rows, frame);
    }
    return;
  }
  if (part == kPartClose) {
    kit::Pressed(&g_room, part);
    WinLog("the trigger presses Close, frame %llu", frame);
    Close("the player pressed Close", true);
    return;
  }
  if (part == kPartThumb) {
    int track[4], thumb[4];
    if (!BarBoxes(g_rows, g_offset, track, thumb)) return;
    kit::Pressed(&g_room, part);
    g_drag = true;
    g_grab = static_cast<int>(g_room.v * kHeight) - thumb[1];
    WinLog("the trigger takes the thumb of the scroll bar at "
           "offset %d, frame %llu", g_offset, frame);
    return;
  }
  if (part == kPartTrack) {
    kit::Pressed(&g_room, part);
    const int from = g_offset;
    g_offset = PageOffset(g_rows, g_offset, g_room.v * kHeight);
    g_offset_f = static_cast<float>(g_offset);
    WinLog("the trigger presses the track of the scroll bar: "
           "the list goes from offset %d to %d of %d, frame %llu", from,
           g_offset, panel_kit_math::MaxOffset(ContentOf(g_rows), kView),
           frame);
    return;
  }
  const int r = RowOf(part);
  const int on = OnOf(part);
  if (r < 0 || r >= g_rows) return;
  const int i = g_switch[r];
  const wchar_t* label = g_src->label(i);
  wchar_t words[96];
  // A press on a picture does nothing.
  if (g_kind[r] == kRowPicture) return;
  if (g_kind[r] == kRowDim) {
    ++g_refused;
    HapticsPulse(haptics_math::kRefused);
    swprintf_s(words, L"%s cannot change now", label);
    SetStatus(words);
    WinLog("the trigger presses %s, which cannot change now, "
           "frame %llu", name, frame);
    return;
  }
  // The label and the value of a row with arrows do nothing.
  if (g_kind[r] == kRowArrows && on == kOnRow) return;
  const bool back = g_kind[r] == kRowArrows && on == kOnLeft;
  WinLog("the trigger presses %s, frame %llu", name, frame);
  if (!g_src->step(i, back)) {
    ++g_refused;
    HapticsPulse(haptics_math::kRefused);
    swprintf_s(words, L"%s cannot change now", label);
    SetStatus(words);
    return;
  }
  ++g_steps;
  kit::Flash(&g_room, g_kind[r] == kRowToggle ? RowPartOf(r, kOnRow) : part);
  g_value[r] = g_src->value(i);
  g_kind[r] = KindOf(i, g_value[r]);
  wchar_t word[32];
  g_src->value_text(i, g_value[r], word, 32);
  const wchar_t* small = g_src->row_note != nullptr ? g_src->row_note(i)
                                                    : nullptr;
  if (small != nullptr) {
    swprintf_s(words, L"%s: %s, %s", label, word, small);
  } else {
    swprintf_s(words, L"%s: %s", label, word);
  }
  SetStatus(words);
}

// The stick of the pointing hand while the ray is on the window.
void StickStep(double dt, uint64_t frame) {
  float push = 0.0f;
  float d = 0.0f;
  if (g_room.on_panel && !g_drag && ControllersPointingStick(&push)) {
    d = panel_kit_math::StickScroll(push, kStickRest, kStickFull,
                                    static_cast<float>(dt));
  }
  const int most = panel_kit_math::MaxOffset(ContentOf(g_rows), kView);
  if (d != 0.0f && most > 0) {
    if (!g_stick_moving) {
      g_stick_moving = true;
      g_stick_from = g_offset;
    }
    g_offset_f += d;
    if (g_offset_f < 0.0f) g_offset_f = 0.0f;
    if (g_offset_f > static_cast<float>(most)) {
      g_offset_f = static_cast<float>(most);
    }
    g_offset = static_cast<int>(g_offset_f + 0.5f);
  } else if (g_stick_moving) {
    g_stick_moving = false;
    if (LogRoom()) {
      WinLog("the stick scrolls the list from offset %d to %d "
             "of %d, frame %llu", g_stick_from, g_offset, most, frame);
    }
  }
}

}  // namespace

bool DebugWindowOpen() { return g_room.open; }

int DebugWindowHoverSwitch() {
  if (!g_room.open) return -1;
  const int r = RowOf(g_room.hover);
  return r >= 0 && r < g_rows ? g_switch[r] : -1;
}

bool DebugWindowHoldsTrigger() {
  return InterlockedCompareExchange(&g_holds, 0, 0) != 0;
}

bool DebugWindowRay(float* meters) {
  *meters = g_room.open ? g_room.meters : 0.0f;
  return g_room.open;
}

bool DebugWindowTakesStick() {
  return InterlockedCompareExchange(&g_stick, 0, 0) != 0;
}

bool DebugWindowCloseBy(const char* why) {
  if (!g_room.open) return false;
  Close(why, false);
  return true;
}

bool DebugWindowOpenFromRing() {
  if (!DebugWindowOn()) return false;
  return DebugWindowOpenWith(&kDebugRows);
}

const WindowRows* DebugWindowRows() { return g_room.open ? g_src : nullptr; }

bool DebugWindowOpenWith(const WindowRows* rows) {
  ReadSettings();
  if (rows == nullptr || rows->slot < 0 || rows->slot >= kWindowSlots) {
    return false;
  }
  if (g_room.open && g_src != rows) Close("the window changes its list", false);
  if (!kit::Open(&g_room, kDistance, kPitch, kMeters, kMeters)) return false;
  g_hold_after = false;
  g_status[0] = 0;
  // The window opens on the group of the last time of the same list, at the
  // same row.
  if (g_src != rows) {
    g_group_of[g_src->slot] = g_group;
    g_offset_of[g_src->slot] = g_offset;
    g_src = rows;
    g_group = g_group_of[rows->slot];
    g_offset = g_offset_of[rows->slot];
  }
  if (g_group < 0 || g_group >= Groups()) g_group = 0;
  const int offset = g_offset;
  FillGroup(g_group);
  g_offset = panel_kit_math::ClampOffset(offset, ContentOf(g_rows), kView);
  g_offset_f = static_cast<float>(g_offset);
  g_last_ms = NowMs();
  SetHolds();
  ++g_opens;
  const menu_room_math::Panel& p = g_room.panel;
  char group[32];
  Narrow(g_src->group_name(g_group), group, sizeof(group));
  WinLog("the window opens, %.2f by %.2f m, its middle at %.2f "
         "%.2f %.2f m, the group %s with %d switches, frame %llu", p.width,
         p.height, p.position[0], p.position[1], p.position[2], group, g_rows,
         ControllersFrameCount());
  return true;
}

bool DebugWindowTestTarget(float out[3]) {
  ReadSettings();
  if (!g_room.open || g_test_count == 0) return false;
  const int part = g_tests[g_tests_used++ % g_test_count];
  char name[64];
  PartName(part, name, sizeof(name));
  float u = 0.0f, v = 0.0f;
  int r[4];
  bool special = false, found = false;
  if (part == kPartButton) {
    special = true;
    found = g_page != nullptr && g_page->button != nullptr;
    PageButtonRect(r);
    r[2] += r[0];
    r[3] += r[1];
  } else if (ListItemOf(part) >= 0) {
    special = true;
    found = g_list_open && ListItemOf(part) < g_list.count;
    ListItemRect(g_list_box, ListItemOf(part), r);
    r[0] += kWellLeft;
    r[1] += kWellTop;
    r[2] += r[0];
    r[3] += r[1];
  } else if (PagePartOf(part) >= 0) {
    special = true;
    found = g_page != nullptr && g_page->box(PagePartOf(part), r);
    r[0] += kWellLeft;
    r[2] += kWellLeft;
    r[1] += kWellTop;
    r[3] += kWellTop;
  }
  if (special) {
    if (!found) {
      WinLog("the word aimdebug: %s is not in the view now", name);
      return false;
    }
    u = (r[0] + r[2]) * 0.5f / kWidth;
    v = (r[1] + r[3]) * 0.5f / kHeight;
  } else if (!PartMiddle(part, Groups(), g_rows, g_kind, g_offset, &u, &v)) {
    WinLog("the word aimdebug: %s is not in the view now",
           name);
    return false;
  }
  kit::PointOf(g_room, u, v, out);
  WinLog("the word aimdebug aims at %s, at %.3f %.3f %.3f m",
         name, out[0], out[1], out[2]);
  return true;
}

void DebugWindowFrame() {
  ReadSettings();
  const uint64_t frame = ControllersFrameCount();

  // After a press that closed the window the trigger stays with it until
  // it goes up.
  if (!g_room.open) {
    InterlockedExchange(&g_stick, 0);
    if (g_hold_after) {
      float p[3], q[4], trigger = 0.0f;
      if (!ControllersPointingAim(p, q, &trigger) || trigger < 0.4f) {
        g_hold_after = false;
        SetHolds();
      }
    }
    return;
  }

  // What closes the window.
  RingHand hand;
  const bool have_hand = ControllersRingHand(&hand);
  if (RadialMenuOpen()) {
    Close("the ring opens", false);
  } else if (SpellBookOpen()) {
    Close("the spell book opens", false);
  } else if (have_hand && !hand.usable) {
    Close("the controllers cannot act now", false);
  } else if (have_hand && hand.dialog) {
    Close("a dialog box is up", false);
  } else if (CutsceneWindowFlat()) {
    Close("a cut scene plays", false);
  }
  if (!g_room.open) return;

  if (kit::FollowRecenter(&g_room)) {
    WinLog("the window goes in front of the head after a "
           "recenter, its middle at %.2f %.2f %.2f m, frame %llu",
           g_room.panel.position[0], g_room.panel.position[1],
           g_room.panel.position[2], frame);
  }

  Latch();
  const int before = g_room.hover;
  float trigger = 0.0f;
  const int pressed = kit::Step(&g_room, &HitPart, nullptr, &trigger);
  if (g_drag) {
    if (!g_room.press.down) {
      g_drag = false;
      WinLog("the trigger lets go of the thumb at offset %d of "
             "%d, frame %llu", g_offset,
             panel_kit_math::MaxOffset(ContentOf(g_rows), kView), frame);
    } else if (g_room.on_panel) {
      g_offset = DragOffset(g_rows, g_room.v * kHeight, g_grab);
      g_offset_f = static_cast<float>(g_offset);
    }
  }
  if (g_room.hover != before && LogRoom()) {
    char name[64];
    WinLog("the ray is on %s, frame %llu",
           PartName(g_room.hover, name, sizeof(name)), frame);
  }
  if (pressed >= 0) Act(pressed, frame);
  if (!g_room.open) return;

  const double now = NowMs();
  double dt = (now - g_last_ms) / 1000.0;
  if (dt < 0.0 || dt > 0.1) dt = 0.1;
  g_last_ms = now;
  InterlockedExchange(&g_stick, g_room.on_panel ? 1 : 0);
  StickStep(dt, frame);

  // The note of the row under the ray, after a rest on it.
  // A picture has no note.
  int rest = g_drag ? -1 : RowOf(g_room.hover);
  if (rest >= 0 && rest < g_rows && g_kind[rest] == kRowPicture) rest = -1;
  if (rest != g_rest_row) {
    g_rest_row = rest;
    g_rest_since = now;
  }
  const int note =
      rest >= 0 && rest < g_rows && now - g_rest_since >= kNoteRestMs ? rest
                                                                      : -1;
  if (note != g_note_row) {
    g_note_row = note;
    if (note >= 0 && LogRoom()) {
      char label[40];
      Narrow(g_src->label(g_switch[note]), label, sizeof(label));
      WinLog("the note of \"%s\" shows (row %d), frame %llu",
             label, note, frame);
    }
  }

  // The picture of this frame. The thread of the warm can have the canvas
  // (DebugWindowWarm).
  card_scroll::WarmWait();
  if (!kit::Start(&g_canvas, kWidth, kHeight, "debug window ")) return;
  Look look;
  BuildLook(&look);
  const uint8_t* pixels = reinterpret_cast<const uint8_t*>(g_canvas.bits);
  bool fresh = false;
  if (look.style != 0 && ParchmentStart()) {
    look.style = 1;
    if (!g_have_drawn || !SameLook(look, g_drawn)) {
      Compose(look, frame);
      fresh = true;
    }
    pixels = g_canvas.card;
  } else {
    look.style = 0;
    if (!g_have_drawn || !SameLook(look, g_drawn)) {
      const double start = NowMs();
      RasterPlain(look);
      const double ms = NowMs() - start;
      ++g_draws;
      g_draw_ms += ms;
      if (ms > g_draw_most_ms) g_draw_most_ms = ms;
      fresh = true;
    }
  }
  g_drawn = look;
  g_have_drawn = true;
  if (fresh) g_serial = bwvr::XrScreen::NewCardSerial();
  if (fresh && g_saved < g_save && WorthSaving(look)) {
    ++g_saved;
    g_saved_look = look;
    g_have_saved = true;
    char name[64], group[32];
    Narrow(g_src->group_name(look.group), group, sizeof(group));
    WinLog("frame %llu, the saved picture has the group %s at "
           "offset %d, the ray on %s, and %s", frame, group, look.offset,
           PartName(g_room.hover, name, sizeof(name)),
           look.note_row >= 0 ? "a note" : "no note");
    EyeCaptureSaveUi("debugwin", frame, 1, pixels, kWidth * 4, kWidth,
                     kHeight);
  }
  if (!kit::StageLayer(g_room, pixels, kWidth, kHeight, g_serial)) {
    Close("the runtime refused the layer of the ring", false);
  }
}

void DebugWindowWarm() {
  char path[MAX_PATH];
  if (!IniPath(path)) return;
  const bool debug =
      GetPrivateProfileIntA("bwvr", "debug_menu", 0, path) != 0 &&
      GetPrivateProfileIntA("bwvr", "debug_window", 1, path) != 0;
  const bool settings =
      GetPrivateProfileIntA("bwvr", "settings_panel", 1, path) != 0;
  if ((!debug && !settings) || !kit::WarmOn()) return;
  const double start = NowMs();
  if (!kit::Start(&g_canvas, kWidth, kHeight, "debug window ") ||
      !ParchmentStart()) {
    return;
  }
  // The tabs and Close of each list as its first picture shows them: the
  // first group is on, and the other tabs are plain.
  const WindowRows* lists[kWindowSlots] = {debug ? &kDebugRows : nullptr,
                                           settings ? SettingsPanelRows()
                                                    : nullptr};
  int parts = 0;
  for (const WindowRows* rows : lists) {
    if (rows == nullptr) continue;
    for (int part = 0; part < kTabParts; ++part) {
      if (part < kMostTabs) {
        const wchar_t* name = rows->group_name(part);
        if (name == nullptr || name[0] == 0) continue;
      }
      int r[4], box[4];
      TabRectOf(part, r);
      panel_kit_math::RectBox(r, kit::kRim, kWidth, kHeight, box);
      kit::ShowPart(&g_canvas, &g_tab_tiles[rows->slot], part,
                    part == 0 ? kit::kOn : kit::kPlain, box, &DrawTab,
                    const_cast<WindowRows*>(rows));
      ++parts;
    }
  }
  // The rows of the first group of the settings, with the values of
  // bwvr.ini, as the first open shows them with no ray on them. The open
  // compares the look of each row (SameRow) and draws again only a row
  // whose value changed. The switches of the debug page hold states of the
  // game, so their rows are drawn at the open.
  int rows_drawn = 0;
  if (settings) {
    const WindowRows* rows = SettingsPanelRows();
    int switches[kMostRows], values[kMostRows];
    const int n = SettingsPanelWarmRows(0, switches, values, kMostRows);
    for (int r = 0; r < n; ++r) {
      RowLook look;
      memset(&look, 0, sizeof(look));
      look.value = values[r];
      look.kind = KindIn(rows, switches[r], values[r]);
      DrawRowOf(rows, switches[r], true, r, look);
      ++rows_drawn;
    }
    g_tile_group = rows->slot * 100 + 0;
  }
  ModLog("debug window : panel_warm=1: the thread of the warm drew the page, "
         "%d tabs and %d rows of the window in %.1f ms (%s%s%s), so the first "
         "picture of the window copies them", parts, rows_drawn,
         NowMs() - start, settings ? "the settings" : "",
         settings && debug ? " and " : "", debug ? "the debug switches" : "");
}

void DebugWindowReport() {
  if (g_opens + g_presses + g_draws == 0) return;
  // The counts are of each list: the debug switches and the settings.
  WinLog("the window opened %llu times this window (%s last): "
         "%llu presses, %llu steps of a row, %llu that did not act", g_opens,
         g_src->slot == 0 ? "the debug switches" : "the settings", g_presses,
         g_steps, g_refused);
  if (g_draws > 0) {
    WinLog("%llu new pictures, %.2f ms each on average and "
           "%.2f ms at the most. %llu were a change under the ray only, "
           "%.2f ms at the most", g_draws,
           g_draw_ms / static_cast<double>(g_draws), g_draw_most_ms,
           g_hover_draws, g_hover_most_ms);
  }
  g_opens = g_presses = g_steps = g_refused = g_draws = g_hover_draws = 0;
  g_draw_ms = g_draw_most_ms = g_hover_most_ms = 0.0;
}
