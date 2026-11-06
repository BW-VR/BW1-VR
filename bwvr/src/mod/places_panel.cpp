// The panel of places. See places_panel.h and docs/ui-layer.md, "The panel
// of places".
//
// WHY. The plain game goes to a bookmark with the keys 1 to 7, sets one with
// Ctrl, follows its object with Shift and removes it with Ctrl and Shift
// (docs/controls-audit.md, item 1). The ring has no room for 28 items, and
// players did not find the bookmarks on it. So one item of the ring opens a
// page with a button for each of them, and for the places of the game.
//
// WHAT. The item "places" of the ring opens the panel 1 m in front of the
// head, 12 degrees below eye level, tilted to face the head
// (places_panel_math.h). It stays at that place of the room while it is
// open, as the menu panel and the keyboard do: the player moves and the
// panel does not. The aim ray of the pointing hand shows, and the button
// under it is gold, with a light pulse at each new button. A pull of the
// trigger presses the button under the ray, with a stronger pulse. While
// the panel is open the trigger belongs to the panel, so no press reaches
// the game.
//
// HOW A BUTTON ACTS. Each button is a key of the plain game, or an item of
// the ring, so each gate of the game holds:
//
//   Temple, Creature, Town, Land, Back   the items temple, creature, town,
//                                        land and mark_back of the ring
//   Go, Set here, Follow, Remove         the key of the number of the
//                                        bookmark, with no modifier, Ctrl,
//                                        Shift, or Ctrl and Shift
//
// The keys go into the key buffer of the game at the next pass of the pump
// (radial_menu.cpp). A button that cannot act now is dim, and the note
// between Back and Close says why. A press of a dim button gives the pulse
// "refused". Go, Follow, the places and Back close the panel. Set here and
// Remove keep it open, so the player sees the seal of the bookmark change.
// The ring, the spell book, a dialog box, a cut scene and the Temple close
// it too.
//
// THE LOOK follows hint_card_style, as the keyboard in the headset does.
// With 1 the panel is a page of the parchment of the cards
// (card_scroll.h): each button is a key of ink, gold under the ray and
// bright gold for 120 ms after a press, and each bookmark has a red wax seal
// with its number. The seal of a bookmark that is not set is pale. With 0
// the panel is dark with grey buttons.
//
// THE BAKE. The page with the title is drawn one time. Each button in each
// of its looks, and each seal, is drawn the first time that it shows, and a
// copy of its box is kept. A new picture copies the box of each part whose
// look changed. Only the note is drawn each time that it changes.
//
// THE PICTURE uses the quad layer of the ring (kCardRing): the ring and the
// panel never show at one time.
//
//   places_panel=1            0: page 2 of the ring has the bookmark items
//                             mark_set, mark_go and mark_back, and no panel
//   hint_card_style=1         1: the parchment. 0: the dark panel
//   places_panel_test_save=0  N: saves the first N new pictures of the panel
//                             as bwvr-places-<frame>.bmp (a test key)
//   places_test_buttons       a test key: the buttons for the word
//                             "aimplaces" of the scripted controller, for
//                             example "set3 go3 remove3 temple"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <new>

#include "../vr/xr_screen.h"
#include "card_scroll.h"
#include "controllers.h"
#include "cutscene_window.h"
#include "haptics.h"
#include "menu_room.h"
#include "menu_room_math.h"
#include "places_panel.h"
#include "places_panel_math.h"
#include "radial_menu.h"
#include "spell_book.h"

extern void ModLog(const char* format, ...);

uint64_t ControllersFrameCount();
void ControllersGesturePulse(float amplitude, float seconds);
uint64_t HeadPoseOriginSerial();
// hint_card.cpp: hint_card_style now, or below 0 while the cards are off.
int HintCardStyleValue();
// eye_capture.cpp
void EyeCaptureSaveUi(const char* name, uint64_t frame, int style,
                      const void* pixels, int pitch, uint32_t width,
                      uint32_t height);

namespace {

using namespace places_math;

// --- the settings ----------------------------------------------------------------

bool g_read = false;
bool g_on = true;
int g_style_ini = 1;
int g_save = 0;
int g_saved = 0;
const int kMostTests = 32;
int g_tests[kMostTests];
int g_test_count = 0;
int g_tests_used = 0;

int IniKey(const char* key, int fallback) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return fallback;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return fallback;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

void IniText(const char* key, char* out, DWORD size) {
  out[0] = 0;
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  GetPrivateProfileStringA("bwvr", key, "", out, size, path);
}

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  g_on = IniKey("places_panel", 1) != 0;
  g_style_ini = IniKey("hint_card_style", 1) != 0 ? 1 : 0;
  g_save = IniKey("places_panel_test_save", 0);
  char text[256];
  IniText("places_test_buttons", text, sizeof(text));
  g_test_count = ParseButtons(text, g_tests, kMostTests);
  ModLog("places       : places_panel=%d. %s%s", g_on ? 1 : 0,
         g_on ? "The ring item \"places\" opens a panel of the places and the "
                "7 bookmarks of the game, and the ray and the trigger press "
                "its buttons"
              : "No panel of places: page 2 of the ring has the bookmark "
                "items",
         g_save > 0 ? ". places_panel_test_save saves the first new pictures "
                      "of the panel (a test)"
                    : "");
  if (g_test_count > 0) {
    ModLog("places       : places_test_buttons has %d buttons for the word "
           "aimplaces (a test)", g_test_count);
  }
}

int CardStyle() {
  const int live = HintCardStyleValue();
  if (live >= 0) return live != 0 ? 1 : 0;
  return g_style_ini;
}

double NowMs() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart) * 1000.0 /
         static_cast<double>(frequency.QuadPart);
}

// --- the state -------------------------------------------------------------------

bool g_open = false;
// The trigger stays with the panel after a press that closed it, until it
// goes up. The pump reads this, so it is a LONG.
volatile LONG g_holds = 0;
bool g_hold_after = false;
menu_room_math::Panel g_panel = {};
uint64_t g_serial = 0;
int g_hover = -1;
int g_flash = -1;
ULONGLONG g_flash_until = 0;
Press g_press;
float g_ray_meters = 0.0f;
// The note of the last press, until then.
wchar_t g_press_note[64] = L"";
ULONGLONG g_press_note_until = 0;
const ULONGLONG kFlashMs = 120;
const ULONGLONG kPressNoteMs = 2000;

// What each button can do now: latched once a frame.
bool g_can[kButtons];
const wchar_t* g_why[kButtons];
bool g_set[kMarks];
wchar_t g_why_mark[kMarks][40];

uint64_t g_opens = 0, g_presses = 0, g_refused = 0, g_keys = 0;
uint64_t g_draws = 0;
double g_draw_ms = 0.0, g_draw_most_ms = 0.0;
int g_logs = 0;
const int kMaxLogs = 200;

bool LogRoom() {
  if (g_logs >= kMaxLogs) return false;
  ++g_logs;
  return true;
}

void SetHolds() {
  InterlockedExchange(&g_holds, (g_open || g_hold_after) ? 1 : 0);
}

const char* ButtonName(int b, char* out, size_t size) {
  const int kind = KindOf(b);
  if (kind < 0) return "no button";
  const int mark = MarkOf(b);
  if (mark >= 0) {
    sprintf_s(out, size, "%s %d", kKindId[kind], mark + 1);
  } else {
    sprintf_s(out, size, "%s", kKindId[kind]);
  }
  return out;
}

// What the button does, for the note while the ray is on it.
void HoverNote(int b, wchar_t* out, size_t size) {
  const int kind = KindOf(b);
  const int n = MarkOf(b) + 1;
  switch (kind) {
    case kTemple: wcscpy_s(out, size, L"Go to the Temple"); break;
    case kCreature: wcscpy_s(out, size, L"Go to the Creature"); break;
    case kTown: wcscpy_s(out, size, L"Go to the next town"); break;
    case kLand: wcscpy_s(out, size, L"Go to the middle of the land"); break;
    case kGo: swprintf_s(out, size, L"Go to bookmark %d", n); break;
    case kSet: swprintf_s(out, size, L"Set bookmark %d at the Hand", n); break;
    case kFollow:
      swprintf_s(out, size, L"Follow the object of bookmark %d", n);
      break;
    case kRemove: swprintf_s(out, size, L"Remove bookmark %d", n); break;
    case kBack: wcscpy_s(out, size, L"Go back to the last place"); break;
    case kClose: wcscpy_s(out, size, L"Close the panel"); break;
    default: out[0] = 0; break;
  }
}

// The facts of this frame: which buttons can act, and why not.
void Latch() {
  const wchar_t* marks_why = L"";
  const bool marks = RadialMenuMarks(g_set, &marks_why);
  for (int b = 0; b < kButtons; ++b) {
    const int kind = KindOf(b);
    const int mark = MarkOf(b);
    g_why[b] = L"";
    if (kind == kClose) {
      g_can[b] = true;
    } else if (mark >= 0) {
      g_can[b] = MarkCan(kind, marks, g_set[mark]);
      if (!marks) {
        g_why[b] = marks_why;
      } else if (!g_can[b]) {
        swprintf_s(g_why_mark[mark], L"Bookmark %d is not set", mark + 1);
        g_why[b] = g_why_mark[mark];
      }
    } else {
      const wchar_t* note = L"";
      g_can[b] = RadialMenuItemCan(RingItemOf(kind), &note);
      g_why[b] = note != nullptr && note[0] != 0 ? note : L"Not now";
    }
  }
}

void Close(const char* why, bool hold) {
  if (!g_open) return;
  g_open = false;
  g_hover = -1;
  g_flash = -1;
  g_hold_after = hold;
  SetHolds();
  ModLog("places       : the panel closes: %s, frame %llu", why,
         ControllersFrameCount());
}

// --- the picture -----------------------------------------------------------------

// The looks of a button.
const int kLookPlain = 0;
const int kLookHover = 1;
const int kLookFlash = 2;
const int kLookDim = 3;
const int kLooks = 4;

struct Look {
  uint8_t button[kButtons];
  uint8_t seal[kMarks];  // 1 for a bookmark that is set
  wchar_t note[64];
  int style;
};
Look g_drawn;            // the picture in g_card, style -1 for none
bool g_have_drawn = false;

HDC g_dc = nullptr;
HBITMAP g_dib = nullptr;
uint32_t* g_bits = nullptr;  // the GDI picture: the dark look, and letters
bool g_raster_failed = false;
HFONT g_plain_font = nullptr;
HFONT g_plain_small = nullptr;

const size_t kPictureBytes = static_cast<size_t>(kWidth) * kHeight * 4;
uint8_t* g_page = nullptr;
uint8_t* g_card = nullptr;
uint8_t* g_tiles[kLooks][kButtons] = {};
uint8_t* g_seal_tiles[2][kMarks] = {};
bool g_parchment_failed = false;
bool g_parchment_ready = false;

// The fonts of the parchment, with the ascent and the height of each.
struct Font {
  HFONT font;
  int cap;
  int ascent;
  int height;
};
Font g_title_font = {}, g_header_font = {}, g_button_font = {},
     g_note_font = {}, g_digit_font = {};

bool RasterStart() {
  if (g_dc != nullptr) return true;
  if (g_raster_failed) return false;
  g_dc = CreateCompatibleDC(nullptr);
  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = kWidth;
  info.bmiHeader.biHeight = -kHeight;  // top first
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  if (g_dc != nullptr) {
    g_dib = CreateDIBSection(g_dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  }
  const auto font = [](int em, int weight) {
    return CreateFontW(-em, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
                       L"Segoe UI");
  };
  g_plain_font = font(36, FW_SEMIBOLD);
  g_plain_small = font(28, FW_NORMAL);
  if (g_dc == nullptr || g_dib == nullptr || bits == nullptr ||
      g_plain_font == nullptr || g_plain_small == nullptr) {
    g_raster_failed = true;
    ModLog("places       : GDI gave no picture for the panel, so the panel "
           "does not show");
    return false;
  }
  g_bits = static_cast<uint32_t*>(bits);
  SelectObject(g_dc, g_dib);
  SetBkMode(g_dc, TRANSPARENT);
  return true;
}

// The words of a button.
void Word(int b, wchar_t* out, size_t size) {
  wcscpy_s(out, size, kKindWord[KindOf(b)]);
}

// THE DARK LOOK (hint_card_style=0): all of it each time that it changes.
void RasterPlain(const Look& look) {
  const auto fill = [](int x0, int y0, int x1, int y1, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    RECT rect = {x0, y0, x1, y1};
    FillRect(g_dc, &rect, brush);
    DeleteObject(brush);
  };
  const auto text = [](HFONT font, COLORREF color, const wchar_t* words,
                       int x0, int y0, int x1, int y1) {
    SelectObject(g_dc, font);
    SetTextColor(g_dc, color);
    RECT rect = {x0, y0, x1, y1};
    DrawTextW(g_dc, words, -1, &rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  };
  fill(0, 0, kWidth, kHeight, RGB(22, 24, 28));
  text(g_plain_font, RGB(235, 235, 235), L"Places", 0, kTitleTop, kWidth,
       kTitleBottom);
  text(g_plain_small, RGB(190, 190, 190), L"Bookmarks", 0, kHeaderTop, kWidth,
       kHeaderBottom);
  for (int m = 0; m < kMarks; ++m) {
    int s[4];
    SealBox(m, s);
    fill(s[0], s[1], s[2], s[3],
         look.seal[m] != 0 ? RGB(150, 40, 36) : RGB(60, 52, 50));
    wchar_t digit[4];
    swprintf_s(digit, L"%d", m + 1);
    text(g_plain_font,
         look.seal[m] != 0 ? RGB(250, 230, 210) : RGB(130, 120, 115), digit,
         s[0], s[1], s[2], s[3]);
  }
  for (int b = 0; b < kButtons; ++b) {
    int r[4];
    ButtonRect(b, r);
    COLORREF face = RGB(62, 66, 76), ink = RGB(235, 235, 235);
    switch (look.button[b]) {
      case kLookHover: face = RGB(205, 172, 96); ink = RGB(20, 20, 20); break;
      case kLookFlash: face = RGB(250, 225, 150); ink = RGB(20, 20, 20); break;
      case kLookDim: face = RGB(40, 42, 48); ink = RGB(110, 110, 110); break;
      default: break;
    }
    fill(r[0], r[1], r[0] + r[2], r[1] + r[3], face);
    wchar_t word[16];
    Word(b, word, 16);
    text(g_plain_small, ink, word, r[0], r[1], r[0] + r[2], r[1] + r[3]);
  }
  int n[4];
  NoteBox(n);
  text(g_plain_small, RGB(230, 190, 110), look.note, n[0], n[1], n[2], n[3]);
  GdiFlush();
  for (size_t i = 0; i < static_cast<size_t>(kWidth) * kHeight; ++i) {
    g_bits[i] |= 0xFF000000u;
  }
}

// --- the parchment (hint_card_style=1) -----------------------------------------

Font MakeFont(HFONT font, int cap) {
  Font f = {font, cap, 0, 0};
  if (font == nullptr) return f;
  TEXTMETRICW metrics;
  SelectObject(g_dc, font);
  GetTextMetricsW(g_dc, &metrics);
  f.ascent = metrics.tmAscent;
  f.height = metrics.tmHeight;
  return f;
}

// White letters on black in the GDI picture, in the box, with the capital
// letter in the middle of the box.
void Letters(const Font& f, const wchar_t* words, int x0, int y0, int x1,
             int y1) {
  const size_t row = static_cast<size_t>(x1 - x0) * 4;
  for (int y = y0; y < y1; ++y) {
    memset(g_bits + static_cast<size_t>(y) * kWidth + x0, 0, row);
  }
  const int baseline = y0 + (y1 - y0 + f.cap) / 2;
  SelectObject(g_dc, f.font);
  SetTextColor(g_dc, RGB(255, 255, 255));
  RECT rect = {x0, baseline - f.ascent, x1, baseline - f.ascent + f.height};
  // The clip keeps the letters in the box: the box is all that changes.
  HRGN clip = CreateRectRgn(x0, y0, x1, y1);
  SelectClipRgn(g_dc, clip);
  DrawTextW(g_dc, words, -1, &rect,
            DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_TOP);
  SelectClipRgn(g_dc, nullptr);
  DeleteObject(clip);
  GdiFlush();
}

void PageToCard(int x0, int y0, int x1, int y1) {
  const size_t row = static_cast<size_t>(x1 - x0) * 4;
  for (int y = y0; y < y1; ++y) {
    const size_t at = (static_cast<size_t>(y) * kWidth + x0) * 4;
    memcpy(g_card + at, g_page + at, row);
  }
}

// A part that cannot act now: its box goes 65 percent of the way back to
// the paper.
void Fade(int x0, int y0, int x1, int y1) {
  for (int y = y0; y < y1; ++y) {
    const size_t at = (static_cast<size_t>(y) * kWidth + x0) * 4;
    uint8_t* c = g_card + at;
    const uint8_t* p = g_page + at;
    for (int i = 0; i < (x1 - x0) * 4; ++i) {
      c[i] = static_cast<uint8_t>(p[i] + ((c[i] - p[i]) * 90) / 256);
    }
  }
}

bool ParchmentStart() {
  if (g_parchment_ready) return true;
  if (g_parchment_failed) return false;
  g_parchment_failed = true;
  if (!card_scroll::Start(g_dc)) {
    ModLog("places       : GDI gave no font for the parchment, so the panel "
           "is dark");
    return false;
  }
  const char* digit_face = "";
  g_title_font = MakeFont(card_scroll::WordsFont(g_dc, 38), 38);
  g_header_font = MakeFont(card_scroll::SealFont(g_dc, 22), 22);
  g_button_font = MakeFont(card_scroll::WordsFont(g_dc, 26), 26);
  g_note_font = MakeFont(card_scroll::WordsFont(g_dc, 22), 22);
  g_digit_font = MakeFont(card_scroll::DigitsFont(g_dc, 30, &digit_face), 30);
  g_page = new (std::nothrow) uint8_t[kPictureBytes];
  g_card = new (std::nothrow) uint8_t[kPictureBytes];
  if (g_title_font.font == nullptr || g_header_font.font == nullptr ||
      g_button_font.font == nullptr || g_note_font.font == nullptr ||
      g_digit_font.font == nullptr || g_page == nullptr || g_card == nullptr) {
    delete[] g_page;
    delete[] g_card;
    g_page = g_card = nullptr;
    ModLog("places       : no font or no memory for the parchment, so the "
           "panel is dark");
    return false;
  }
  const double start = NowMs();
  const card_scroll::Picture page = {g_page, kWidth, kWidth, kHeight};
  card_scroll::Page(page, 4, 4, kWidth - 4, kHeight - 4, 17);
  Letters(g_title_font, L"Places", kLeft, kTitleTop, kRight, kTitleBottom);
  card_scroll::Ink(page, g_bits, kLeft, kTitleTop, kRight, kTitleBottom, 5);
  Letters(g_header_font, L"Bookmarks", kLeft, kHeaderTop, kRight,
          kHeaderBottom);
  card_scroll::Ink(page, g_bits, kLeft, kHeaderTop, kRight, kHeaderBottom, 9);
  g_parchment_failed = false;
  g_parchment_ready = true;
  ModLog("places       : the page of the parchment panel took %.2f ms (%s). "
         "The numbers of the seals use %s", NowMs() - start,
         card_scroll::FontNote(), digit_face);
  return true;
}

void CardToTile(const int box[4], uint8_t* tile) {
  const size_t row = static_cast<size_t>(box[2] - box[0]) * 4;
  for (int y = box[1]; y < box[3]; ++y) {
    memcpy(tile + static_cast<size_t>(y - box[1]) * row,
           g_card + (static_cast<size_t>(y) * kWidth + box[0]) * 4, row);
  }
}

void TileToCard(const int box[4], const uint8_t* tile) {
  const size_t row = static_cast<size_t>(box[2] - box[0]) * 4;
  for (int y = box[1]; y < box[3]; ++y) {
    memcpy(g_card + (static_cast<size_t>(y) * kWidth + box[0]) * 4,
           tile + static_cast<size_t>(y - box[1]) * row, row);
  }
}

void DrawButton(int b, int look) {
  int box[4], r[4];
  ButtonBox(b, box);
  ButtonRect(b, r);
  PageToCard(box[0], box[1], box[2], box[3]);
  const card_scroll::Picture picture = {g_card, kWidth, kWidth, kHeight};
  const int x0 = r[0], y0 = r[1], x1 = r[0] + r[2], y1 = r[1] + r[3];
  const int key_look = look == kLookHover   ? card_scroll::kKeyHover
                       : look == kLookFlash ? card_scroll::kKeyFlash
                                            : card_scroll::kKeyPlain;
  card_scroll::Key(picture, x0, y0, x1, y1, key_look, b * 7 + 3);
  wchar_t word[16];
  Word(b, word, 16);
  Letters(g_button_font, word, x0, y0, x1, y1);
  card_scroll::Ink(picture, g_bits, x0, y0, x1, y1, b * 7 + 3);
  if (look == kLookDim) Fade(box[0], box[1], box[2], box[3]);
}

void DrawSeal(int m, bool set) {
  int s[4];
  SealBox(m, s);
  PageToCard(s[0], s[1], s[2], s[3]);
  const card_scroll::Picture picture = {g_card, kWidth, kWidth, kHeight};
  // The seal has its own small shadow inside the box.
  const int x0 = s[0] + 4, y0 = s[1] + 2, x1 = s[2] - 6, y1 = s[3] - 6;
  card_scroll::Seal(picture, x0, y0, x1, y1, 0, m * 11 + 5);
  wchar_t digit[4];
  swprintf_s(digit, L"%d", m + 1);
  Letters(g_digit_font, digit, x0, y0, x1, y1);
  card_scroll::SealInk(picture, g_bits, x0, y0, x1, y1, 0);
  if (!set) Fade(s[0], s[1], s[2], s[3]);
}

void DrawNote(const wchar_t* note) {
  int n[4];
  NoteBox(n);
  PageToCard(n[0], n[1], n[2], n[3]);
  if (note[0] == 0) return;
  const card_scroll::Picture picture = {g_card, kWidth, kWidth, kHeight};
  Letters(g_note_font, note, n[0], n[1], n[2], n[3]);
  card_scroll::Ink(picture, g_bits, n[0], n[1], n[2], n[3], 13);
}

// The look of each part in a tile: a copy, or a draw that makes the tile.
// True for a copy.
bool ShowButton(int b, int look) {
  int box[4];
  ButtonBox(b, box);
  uint8_t*& tile = g_tiles[look][b];
  if (tile != nullptr) {
    TileToCard(box, tile);
    return true;
  }
  DrawButton(b, look);
  tile = new (std::nothrow)
      uint8_t[static_cast<size_t>(box[2] - box[0]) * (box[3] - box[1]) * 4];
  if (tile != nullptr) CardToTile(box, tile);
  return false;
}

bool ShowSeal(int m, bool set) {
  int s[4];
  SealBox(m, s);
  uint8_t*& tile = g_seal_tiles[set ? 1 : 0][m];
  if (tile != nullptr) {
    TileToCard(s, tile);
    return true;
  }
  DrawSeal(m, set);
  tile = new (std::nothrow)
      uint8_t[static_cast<size_t>(s[2] - s[0]) * (s[3] - s[1]) * 4];
  if (tile != nullptr) CardToTile(s, tile);
  return false;
}

void Compose(const Look& look, uint64_t frame) {
  const double start = NowMs();
  const bool first = !g_have_drawn || g_drawn.style != 1;
  if (first) memcpy(g_card, g_page, kPictureBytes);
  int drawn = 0, copied = 0;
  for (int b = 0; b < kButtons; ++b) {
    if (!first && look.button[b] == g_drawn.button[b]) continue;
    ShowButton(b, look.button[b]) ? ++copied : ++drawn;
  }
  for (int m = 0; m < kMarks; ++m) {
    if (!first && look.seal[m] == g_drawn.seal[m]) continue;
    ShowSeal(m, look.seal[m] != 0) ? ++copied : ++drawn;
  }
  if (first || wcscmp(look.note, g_drawn.note) != 0) {
    DrawNote(look.note);
    ++drawn;
  }
  const double ms = NowMs() - start;
  ++g_draws;
  g_draw_ms += ms;
  if (ms > g_draw_most_ms) g_draw_most_ms = ms;
  if (LogRoom() && g_draws <= 40) {
    ModLog("places       : frame %llu, a new picture of the parchment panel "
           "took %.2f ms: %d parts drawn and %d parts copied", frame, ms,
           drawn, copied);
  }
}

void BuildLook(Look* look) {
  memset(look, 0, sizeof(*look));
  look->style = CardStyle();
  for (int b = 0; b < kButtons; ++b) {
    look->button[b] = static_cast<uint8_t>(
        !g_can[b] ? kLookDim
                  : (b == g_flash ? kLookFlash
                                  : (b == g_hover ? kLookHover : kLookPlain)));
  }
  for (int m = 0; m < kMarks; ++m) look->seal[m] = g_set[m] ? 1 : 0;
  const ULONGLONG now = GetTickCount64();
  if (g_press_note[0] != 0 && now < g_press_note_until) {
    wcsncpy_s(look->note, g_press_note, _TRUNCATE);
  } else if (g_hover >= 0 && !g_can[g_hover]) {
    wcsncpy_s(look->note, g_why[g_hover], _TRUNCATE);
  } else if (g_hover >= 0) {
    HoverNote(g_hover, look->note, 64);
  }
}

bool SameLook(const Look& a, const Look& b) {
  return memcmp(a.button, b.button, sizeof(a.button)) == 0 &&
         memcmp(a.seal, b.seal, sizeof(a.seal)) == 0 &&
         wcscmp(a.note, b.note) == 0 && a.style == b.style;
}

// --- a press ---------------------------------------------------------------------

void Act(int b, uint64_t frame) {
  const int kind = KindOf(b);
  const int mark = MarkOf(b);
  char name[24];
  ButtonName(b, name, sizeof(name));
  ++g_presses;
  if (!g_can[b]) {
    ++g_refused;
    HapticsPulse(haptics_math::kRefused);
    wcsncpy_s(g_press_note, g_why[b], _TRUNCATE);
    g_press_note_until = GetTickCount64() + kPressNoteMs;
    char why[64];
    WideCharToMultiByte(CP_ACP, 0, g_why[b], -1, why, sizeof(why), nullptr,
                        nullptr);
    ModLog("places       : the trigger presses %s, and it does not act: %s, "
           "frame %llu", name, why, frame);
    return;
  }
  ControllersGesturePulse(0.3f, 0.02f);
  g_flash = b;
  g_flash_until = GetTickCount64() + kFlashMs;
  int key = 0, mods = 0;
  bool acted = true;
  if (kind == kClose) {
    ModLog("places       : the trigger presses close, frame %llu", frame);
  } else if (MarkKey(kind, mark, &key, &mods)) {
    ModLog("places       : the trigger presses %s, frame %llu", name, frame);
    RadialMenuQueueKey(key, mods);
    ++g_keys;
    if (kind == kSet) {
      swprintf_s(g_press_note, L"Bookmark %d is set at the Hand", mark + 1);
    } else if (kind == kRemove) {
      swprintf_s(g_press_note, L"Bookmark %d is removed", mark + 1);
    }
  } else {
    ModLog("places       : the trigger presses %s: the ring item %s, frame "
           "%llu", name, radial_math::kItemInfo[RingItemOf(kind)].id, frame);
    acted = RadialMenuItemAct(RingItemOf(kind));
    if (acted) ++g_keys;
  }
  if (!acted) {
    ++g_refused;
    HapticsPulse(haptics_math::kRefused);
    wcscpy_s(g_press_note, L"Not now");
  }
  g_press_note_until = GetTickCount64() + kPressNoteMs;
  if (acted && Closes(kind)) {
    char why[48];
    sprintf_s(why, "the player pressed %s", name);
    Close(why, true);
  }
}

}  // namespace

bool PlacesPanelOn() {
  ReadSettings();
  return g_on;
}

bool PlacesPanelOpen() { return g_open; }

bool PlacesPanelHoldsTrigger() {
  return InterlockedCompareExchange(&g_holds, 0, 0) != 0;
}

bool PlacesPanelRay(float* meters) {
  *meters = g_open ? g_ray_meters : 0.0f;
  return g_open;
}

bool PlacesPanelOpenFromRing() {
  ReadSettings();
  if (!g_on) return false;
  float head[3], orientation[4];
  if (!MenuRoomHeadPose(head, orientation)) return false;
  const float yaw = menu_room_math::YawOf(orientation);
  PlaceAt(head, yaw, &g_panel);
  g_serial = HeadPoseOriginSerial();
  g_open = true;
  g_hold_after = false;
  g_hover = -1;
  g_flash = -1;
  g_press_note[0] = 0;
  // A trigger that is down now is not a press on a button.
  g_press.down = g_press.was = true;
  SetHolds();
  ++g_opens;
  ModLog("places       : the panel opens, %.2f by %.2f m, its middle at %.2f "
         "%.2f %.2f m, the head at %.2f %.2f %.2f m with a yaw of %.1f "
         "degrees, frame %llu", g_panel.width, g_panel.height,
         g_panel.position[0], g_panel.position[1], g_panel.position[2], head[0],
         head[1], head[2], yaw * 57.29578f, ControllersFrameCount());
  return true;
}

bool PlacesPanelTestTarget(float out[3]) {
  ReadSettings();
  if (!g_open || g_test_count == 0) return false;
  const int b = g_tests[g_tests_used++ % g_test_count];
  float u = 0.0f, v = 0.0f;
  ButtonMiddle(b, &u, &v);
  PanelPoint(g_panel, u, v, out);
  char name[24];
  ModLog("places       : the word aimplaces aims at %s, at %.3f %.3f %.3f m",
         ButtonName(b, name, sizeof(name)), out[0], out[1], out[2]);
  return true;
}

void PlacesPanelFrame() {
  ReadSettings();
  if (!g_on) return;
  const uint64_t frame = ControllersFrameCount();
  float aim_p[3], aim_q[4], trigger = 0.0f;
  const bool have_aim = ControllersPointingAim(aim_p, aim_q, &trigger);

  // After a press that closed the panel the trigger stays with the panel
  // until it goes up.
  if (!g_open) {
    if (g_hold_after && (!have_aim || trigger < 0.4f)) {
      g_hold_after = false;
      SetHolds();
    }
    return;
  }

  // What closes the panel.
  RingHand hand;
  const bool have_hand = ControllersRingHand(&hand);
  const wchar_t* note = L"";
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
  } else if (bwvr::XrScreen::CardFailed(bwvr::kCardRing)) {
    Close("the runtime refused the layer of the ring", false);
  } else if (!RadialMenuItemCan(radial_math::kItemPlaces, &note)) {
    char why[64];
    WideCharToMultiByte(CP_ACP, 0, note, -1, why, sizeof(why), nullptr,
                        nullptr);
    Close(why[0] != 0 ? why : "the game cannot take it now", false);
  }
  if (!g_open) return;

  // A recenter of the headset places the panel again in front of the head.
  if (HeadPoseOriginSerial() != g_serial) {
    float head[3], orientation[4];
    if (MenuRoomHeadPose(head, orientation)) {
      PlaceAt(head, menu_room_math::YawOf(orientation), &g_panel);
      ModLog("places       : the panel goes in front of the head after a "
             "recenter, its middle at %.2f %.2f %.2f m, frame %llu",
             g_panel.position[0], g_panel.position[1], g_panel.position[2],
             frame);
    }
    g_serial = HeadPoseOriginSerial();
  }

  Latch();

  // The button under the ray, and a light pulse at each new button.
  int button = -1;
  g_ray_meters = 0.0f;
  if (have_aim) {
    float direction[3];
    menu_room_math::AimForward(aim_q, direction);
    float u = 0.0f, v = 0.0f, t = 0.0f;
    if (menu_room_math::Hit(aim_p, direction, g_panel, &u, &v, &t) &&
        menu_room_math::Inside(u, v)) {
      g_ray_meters = t;
      button = ButtonAt(u, v);
    }
  }
  if (button != g_hover) {
    if (button >= 0) ControllersGesturePulse(0.1f, 0.008f);
    if (LogRoom()) {
      char name[24];
      ModLog("places       : the ray is on %s%s, frame %llu",
             ButtonName(button, name, sizeof(name)),
             button >= 0 && !g_can[button] ? ", which cannot act now" : "",
             frame);
    }
  }
  g_hover = button;

  const int pressed = PressStep(&g_press, have_aim ? trigger : 0.0f, button);
  if (pressed >= 0) Act(pressed, frame);
  if (!g_open) return;
  if (g_flash >= 0 && GetTickCount64() >= g_flash_until) g_flash = -1;

  // The picture of this frame.
  if (!RasterStart()) return;
  Look look;
  BuildLook(&look);
  const uint8_t* pixels = reinterpret_cast<const uint8_t*>(g_bits);
  bool fresh = false;
  if (look.style != 0 && ParchmentStart()) {
    look.style = 1;
    if (!g_have_drawn || !SameLook(look, g_drawn)) {
      Compose(look, frame);
      fresh = true;
    }
    pixels = g_card;
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
  if (fresh && g_saved < g_save) {
    ++g_saved;
    char name[24];
    ModLog("places       : frame %llu, the saved picture has the ray on %s",
           frame, ButtonName(g_hover, name, sizeof(name)));
    EyeCaptureSaveUi("places", frame, 1, pixels, kWidth * 4, kWidth, kHeight);
  }
  bwvr::CardLayer layer = {};
  layer.visible = true;
  layer.pixels = pixels;
  layer.texture_width = kWidth;
  layer.texture_height = kHeight;
  layer.rect[0] = 0;
  layer.rect[1] = 0;
  layer.rect[2] = kWidth;
  layer.rect[3] = kHeight;
  for (int i = 0; i < 3; ++i) layer.position[i] = g_panel.position[i];
  for (int i = 0; i < 4; ++i) layer.orientation[i] = g_panel.orientation[i];
  layer.width = g_panel.width;
  layer.height = g_panel.height;
  bwvr::XrScreen::StageCard(bwvr::kCardRing, layer);
}

void PlacesPanelReport() {
  if (g_opens + g_presses + g_draws == 0) return;
  ModLog("places       : the panel opened %llu times this window: %llu "
         "presses, %llu that did not act, %llu that went to the game",
         g_opens, g_presses, g_refused, g_keys);
  if (g_draws > 0) {
    ModLog("places       : %llu new pictures of the panel, %.2f ms each on "
           "average and %.2f ms at the most", g_draws,
           g_draw_ms / static_cast<double>(g_draws), g_draw_most_ms);
  }
  g_opens = g_presses = g_refused = g_keys = g_draws = 0;
  g_draw_ms = g_draw_most_ms = 0.0;
}
