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
// it too. With places_close_buttons=1, B of the pointing hand and the menu
// button close it, as they close the debug window.
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
// The panel is built on panel_kit.h: the place in the room, the ray and the
// trigger, the parchment and the copies of the parts are there.
//
//   places_panel=1            0: page 2 of the ring has the bookmark items
//                             mark_set, mark_go and mark_back, and no panel
//   places_close_buttons=1    1: B of the pointing hand and the menu button
//                             close the panel (controllers.cpp). 0: they do
//                             not
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

#include "card_scroll.h"
#include "controllers.h"
#include "cutscene_window.h"
#include "haptics.h"
#include "panel_kit.h"
#include "places_panel.h"
#include "places_panel_math.h"
#include "radial_menu.h"
#include "spell_book.h"

extern void ModLog(const char* format, ...);

uint64_t ControllersFrameCount();
// eye_capture.cpp
void EyeCaptureSaveUi(const char* name, uint64_t frame, int style,
                      const void* pixels, int pitch, uint32_t width,
                      uint32_t height);

namespace {

using namespace places_math;
namespace kit = panel_kit;

// --- the settings ----------------------------------------------------------------

bool g_read = false;
bool g_on = true;
bool g_close_buttons = true;  // places_close_buttons
int g_style_ini = 1;
int g_save = 0;
int g_saved = 0;
const int kMostTests = 32;
int g_tests[kMostTests];
int g_test_count = 0;
int g_tests_used = 0;

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
  char text[256] = "";
  if (IniPath(path)) {
    g_on = GetPrivateProfileIntA("bwvr", "places_panel", 1, path) != 0;
    g_close_buttons =
        GetPrivateProfileIntA("bwvr", "places_close_buttons", 1, path) != 0;
    g_style_ini =
        GetPrivateProfileIntA("bwvr", "hint_card_style", 1, path) != 0 ? 1 : 0;
    g_save = static_cast<int>(
        GetPrivateProfileIntA("bwvr", "places_panel_test_save", 0, path));
    GetPrivateProfileStringA("bwvr", "places_test_buttons", "", text,
                             sizeof(text), path);
  }
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
  if (g_on) {
    ModLog("places       : places_close_buttons=%d: %s", g_close_buttons ? 1 : 0,
           g_close_buttons ? "B of the pointing hand and the menu button close "
                             "the panel"
                           : "B and the menu button do not close the panel");
  }
  if (g_test_count > 0) {
    ModLog("places       : places_test_buttons has %d buttons for the word "
           "aimplaces (a test)", g_test_count);
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

// --- the state -------------------------------------------------------------------

kit::Room g_room = {};
// The trigger stays with the panel after a press that closed it, until it
// goes up. The pump reads this, so it is a LONG.
volatile LONG g_holds = 0;
bool g_hold_after = false;
// The note of the last press, until then.
wchar_t g_press_note[64] = L"";
ULONGLONG g_press_note_until = 0;
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
  InterlockedExchange(&g_holds, (g_room.open || g_hold_after) ? 1 : 0);
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
  if (!g_room.open) return;
  kit::Close(&g_room);
  g_hold_after = hold;
  SetHolds();
  ModLog("places       : the panel closes: %s, frame %llu", why,
         ControllersFrameCount());
}

int HitButton(float u, float v, void*) { return ButtonAt(u, v); }

// --- the picture -----------------------------------------------------------------

struct Look {
  uint8_t button[kButtons];  // a panel_kit::Look
  uint8_t seal[kMarks];      // 1 for a bookmark that is set
  wchar_t note[64];
  int style;
};
Look g_drawn;
bool g_have_drawn = false;

kit::Canvas g_canvas = {};
kit::Tiles g_button_tiles = {};
kit::Tiles g_seal_tiles = {};
HFONT g_plain_font = nullptr;
HFONT g_plain_small = nullptr;
kit::Font g_title_font = {}, g_header_font = {}, g_button_font = {},
          g_note_font = {}, g_digit_font = {};
const char* g_digit_face = "";
bool g_fonts_failed = false;

// The page: the fonts, then the title and the header. The kit has loaded
// the fonts of the cards (card_scroll::Start) before this.
void DrawPage(kit::Canvas* c, void*) {
  g_title_font = kit::WordsFont(c, 38);
  g_header_font = kit::SealFont(c, 22);
  g_button_font = kit::WordsFont(c, 26);
  g_note_font = kit::WordsFont(c, 22);
  g_digit_font = kit::DigitsFont(c, 30, &g_digit_face);
  const int title[4] = {kLeft, kTitleTop, kRight, kTitleBottom};
  kit::InkWords(c, g_title_font, L"Places", title, 5, true);
  const int header[4] = {kLeft, kHeaderTop, kRight, kHeaderBottom};
  kit::InkWords(c, g_header_font, L"Bookmarks", header, 9, true);
}

bool ParchmentStart() {
  if (g_fonts_failed) return false;
  if (!kit::StartParchment(&g_canvas, &DrawPage, nullptr)) return false;
  if (g_title_font.font == nullptr || g_header_font.font == nullptr ||
      g_button_font.font == nullptr || g_note_font.font == nullptr ||
      g_digit_font.font == nullptr ||
      !kit::MakeTiles(&g_button_tiles, kButtons, kit::kLooks) ||
      !kit::MakeTiles(&g_seal_tiles, kMarks, 2)) {
    g_fonts_failed = true;
    ModLog("places       : no font or no memory for the parchment, so the "
           "panel is dark");
    return false;
  }
  static bool said = false;
  if (!said) {
    said = true;
    ModLog("places       : the numbers of the seals use %s, so that they are "
           "as high as a capital letter", g_digit_face);
  }
  return true;
}

void DrawButton(kit::Canvas* c, int b, int look, void*) {
  int r[4];
  ButtonRect(b, r);
  kit::Button(c, r, look, kKindWord[KindOf(b)], g_button_font, b * 7 + 3);
}

// A red wax seal with the number in the light ink of the wax. The seal of a
// bookmark that is not set is pale.
void DrawSeal(kit::Canvas* c, int m, int set, void*) {
  int s[4];
  SealBox(m, s);
  kit::PageToCard(c, s);
  const card_scroll::Picture picture = {c->card, c->width, c->width,
                                        c->height};
  // The seal has its own small shadow inside the box.
  const int w[4] = {s[0] + 4, s[1] + 2, s[2] - 6, s[3] - 6};
  card_scroll::Seal(picture, w[0], w[1], w[2], w[3], 0, m * 11 + 5);
  const size_t row = static_cast<size_t>(w[2] - w[0]) * 4;
  for (int y = w[1]; y < w[3]; ++y) {
    memset(c->bits + static_cast<size_t>(y) * c->width + w[0], 0, row);
  }
  wchar_t digit[4];
  swprintf_s(digit, L"%d", m + 1);
  const int baseline = w[1] + (w[3] - w[1] + g_digit_font.cap) / 2;
  SelectObject(c->dc, g_digit_font.font);
  SetTextColor(c->dc, RGB(255, 255, 255));
  RECT rect = {w[0], baseline - g_digit_font.ascent, w[2],
               baseline - g_digit_font.ascent + g_digit_font.height};
  DrawTextW(c->dc, digit, -1, &rect,
            DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_TOP);
  GdiFlush();
  card_scroll::SealInk(picture, c->bits, w[0], w[1], w[2], w[3], 0);
  if (set == 0) kit::Fade(c, s);
}

void DrawNote(const wchar_t* note) {
  int n[4];
  NoteBox(n);
  kit::PageToCard(&g_canvas, n);
  kit::InkWords(&g_canvas, g_note_font, note, n, 13, false);
}

void Compose(const Look& look, uint64_t frame) {
  const double start = NowMs();
  const bool first = !g_have_drawn || g_drawn.style != 1;
  if (first) kit::ClearCard(&g_canvas);
  int drawn = 0, copied = 0;
  for (int b = 0; b < kButtons; ++b) {
    if (!first && look.button[b] == g_drawn.button[b]) continue;
    int box[4];
    ButtonBox(b, box);
    if (kit::ShowPart(&g_canvas, &g_button_tiles, b, look.button[b], box,
                      &DrawButton, nullptr)) {
      ++copied;
    } else {
      ++drawn;
    }
  }
  for (int m = 0; m < kMarks; ++m) {
    if (!first && look.seal[m] == g_drawn.seal[m]) continue;
    int box[4];
    SealBox(m, box);
    if (kit::ShowPart(&g_canvas, &g_seal_tiles, m, look.seal[m], box,
                      &DrawSeal, nullptr)) {
      ++copied;
    } else {
      ++drawn;
    }
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

// THE DARK LOOK (hint_card_style=0): all of it each time that it changes.
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
    g_plain_small = font(28, FW_NORMAL);
  }
  const int all[4] = {0, 0, kWidth, kHeight};
  kit::PlainFill(c, all, RGB(22, 24, 28));
  const int title[4] = {0, kTitleTop, kWidth, kTitleBottom};
  kit::PlainText(c, g_plain_font, RGB(235, 235, 235), L"Places", title);
  const int header[4] = {0, kHeaderTop, kWidth, kHeaderBottom};
  kit::PlainText(c, g_plain_small, RGB(190, 190, 190), L"Bookmarks", header);
  for (int m = 0; m < kMarks; ++m) {
    int s[4];
    SealBox(m, s);
    kit::PlainFill(c, s,
                   look.seal[m] != 0 ? RGB(150, 40, 36) : RGB(60, 52, 50));
    wchar_t digit[4];
    swprintf_s(digit, L"%d", m + 1);
    kit::PlainText(c, g_plain_font,
                   look.seal[m] != 0 ? RGB(250, 230, 210) : RGB(130, 120, 115),
                   digit, s);
  }
  for (int b = 0; b < kButtons; ++b) {
    int r[4];
    ButtonRect(b, r);
    kit::PlainButton(c, r, look.button[b], kKindWord[KindOf(b)],
                     g_plain_small);
  }
  int n[4];
  NoteBox(n);
  kit::PlainText(c, g_plain_small, RGB(230, 190, 110), look.note, n);
  kit::PlainDone(c);
}

void BuildLook(Look* look) {
  memset(look, 0, sizeof(*look));
  look->style = kit::CardStyle(g_style_ini);
  const int flash = kit::Flashing(&g_room);
  const int hover = g_room.hover;
  for (int b = 0; b < kButtons; ++b) {
    look->button[b] = static_cast<uint8_t>(
        !g_can[b] ? kit::kDim
                  : (b == flash ? kit::kFlash
                                : (b == hover ? kit::kHover : kit::kPlain)));
  }
  for (int m = 0; m < kMarks; ++m) look->seal[m] = g_set[m] ? 1 : 0;
  if (g_press_note[0] != 0 && GetTickCount64() < g_press_note_until) {
    wcsncpy_s(look->note, g_press_note, _TRUNCATE);
  } else if (hover >= 0 && !g_can[hover]) {
    wcsncpy_s(look->note, g_why[hover], _TRUNCATE);
  } else if (hover >= 0) {
    HoverNote(hover, look->note, 64);
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
  kit::Pressed(&g_room, b);
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

bool PlacesPanelOpen() { return g_room.open; }

bool PlacesPanelHoldsTrigger() {
  return InterlockedCompareExchange(&g_holds, 0, 0) != 0;
}

bool PlacesPanelRay(float* meters) {
  *meters = g_room.open ? g_room.meters : 0.0f;
  return g_room.open;
}

bool PlacesPanelCloseBy(const char* why) {
  ReadSettings();
  if (!g_close_buttons || !g_room.open) return false;
  Close(why, false);
  return true;
}

bool PlacesPanelOpenFromRing() {
  ReadSettings();
  if (!g_on || !kit::Open(&g_room, kDistance, kPitch, kMeters, kMeters)) {
    return false;
  }
  g_hold_after = false;
  g_press_note[0] = 0;
  SetHolds();
  ++g_opens;
  const menu_room_math::Panel& p = g_room.panel;
  ModLog("places       : the panel opens, %.2f by %.2f m, its middle at %.2f "
         "%.2f %.2f m, frame %llu", p.width, p.height, p.position[0],
         p.position[1], p.position[2], ControllersFrameCount());
  return true;
}

bool PlacesPanelTestTarget(float out[3]) {
  ReadSettings();
  if (!g_room.open || g_test_count == 0) return false;
  const int b = g_tests[g_tests_used++ % g_test_count];
  float u = 0.0f, v = 0.0f;
  ButtonMiddle(b, &u, &v);
  kit::PointOf(g_room, u, v, out);
  char name[24];
  ModLog("places       : the word aimplaces aims at %s, at %.3f %.3f %.3f m",
         ButtonName(b, name, sizeof(name)), out[0], out[1], out[2]);
  return true;
}

void PlacesPanelFrame() {
  ReadSettings();
  if (!g_on) return;
  const uint64_t frame = ControllersFrameCount();

  // After a press that closed the panel the trigger stays with the panel
  // until it goes up.
  if (!g_room.open) {
    if (g_hold_after) {
      float p[3], q[4], trigger = 0.0f;
      if (!ControllersPointingAim(p, q, &trigger) || trigger < 0.4f) {
        g_hold_after = false;
        SetHolds();
      }
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
  } else if (!RadialMenuItemCan(radial_math::kItemPlaces, &note)) {
    char why[64];
    WideCharToMultiByte(CP_ACP, 0, note, -1, why, sizeof(why), nullptr,
                        nullptr);
    Close(why[0] != 0 ? why : "the game cannot take it now", false);
  }
  if (!g_room.open) return;

  if (kit::FollowRecenter(&g_room)) {
    ModLog("places       : the panel goes in front of the head after a "
           "recenter, its middle at %.2f %.2f %.2f m, frame %llu",
           g_room.panel.position[0], g_room.panel.position[1],
           g_room.panel.position[2], frame);
  }

  Latch();
  const int before = g_room.hover;
  float trigger = 0.0f;
  const int pressed = kit::Step(&g_room, &HitButton, nullptr, &trigger);
  if (g_room.hover != before && LogRoom()) {
    char name[24];
    ModLog("places       : the ray is on %s%s, frame %llu",
           ButtonName(g_room.hover, name, sizeof(name)),
           g_room.hover >= 0 && !g_can[g_room.hover]
               ? ", which cannot act now"
               : "",
           frame);
  }
  if (pressed >= 0) Act(pressed, frame);
  if (!g_room.open) return;

  // The picture of this frame.
  if (!kit::Start(&g_canvas, kWidth, kHeight, "places      ")) return;
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
  if (fresh && g_saved < g_save) {
    ++g_saved;
    char name[24];
    ModLog("places       : frame %llu, the saved picture has the ray on %s",
           frame, ButtonName(g_room.hover, name, sizeof(name)));
    EyeCaptureSaveUi("places", frame, 1, pixels, kWidth * 4, kWidth, kHeight);
  }
  if (!kit::StageLayer(g_room, pixels, kWidth, kHeight)) {
    Close("the runtime refused the layer of the ring", false);
  }
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
