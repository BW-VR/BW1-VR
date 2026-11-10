// The settings window and the welcome page. See settings_panel.h,
// docs/controllers.md, "The settings window", and docs/ui-layer.md, "The
// settings window".
//
// WHY. The settings of the player are in the launcher, before the game
// starts. In the headset a player has no launcher and no keyboard. So the
// settings that a player changes most are in a window in the room, and a
// change acts at once where the mod can do that.
//
// THE SETTINGS WINDOW is the debug window (debug_window.cpp) with the rows of
// settings_math::kTable: the same place, the same tabs, toggles, arrows,
// scroll bar and notes, the same bake, and the same closes (Close, B of the
// pointing hand, the menu button, the ring, the spell book, a dialog box, a
// cut scene). The item "settings" of the ring opens it. Its words are the
// words of the launcher, so a player knows them.
//
// A CHANGE writes the key into bwvr.ini at once, as the launcher writes it
// (WritePrivateProfileString, the value as a number). A setting that the mod
// can change live goes to the change function of its area at once, and that
// area writes its own log line. A setting that the mod reads only at the
// start says "at the next start" on its row. The window never writes the
// registry or a file of the game.
//
// THE WELCOME PAGE shows one time: the first start of the mod with no
// welcome_done=1 in bwvr.ini. It waits until the controllers can act for 3
// seconds with no dialog box, no cut scene and no other panel. It says what
// the mod is, names three controls, and has two buttons: Settings opens the
// settings window, and Play closes the page. A press of either button, B
// and the menu button write welcome_done=1. A close by the ring, a dialog
// box or a cut scene does not: the page shows again when they are gone.
//
//   settings_panel=1      1: page 2 of the ring as made has the item
//                         "settings" in the slot of "names". 0: page 2 has
//                         "names", and the word "settings" is an empty slot
//   settings_lean=1       1: the window has only the rows that a player
//                         changes (settings_math::kTable, lean). 0: it also
//                         has Teleport, Recenter hold, Hand, Open hand
//                         stroke, Hints, Card look and Subtitle card, with
//                         the tab Cards, and the words "at the next start"
//                         under a row that acts at the next start
//   welcome_page=1        0: the welcome page never shows
//   welcome_done=0        the mod writes 1 after the welcome page
//   welcome_test_save=0   N: saves the first N pictures of the welcome page
//                         as bwvr-welcome-<frame>.bmp (a test key)
//   welcome_test_parts    a test key: the buttons for the word "aimdebug" of
//                         the scripted controller, "settings" or "play"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "../vr/xr_screen.h"
#include "card_scroll.h"
#include "controllers.h"
#include "controls_page.h"
#include "cutscene_window.h"
#include "debug_menu.h"
#include "debug_window.h"
#include "debug_window_math.h"
#include "haptics.h"
#include "panel_kit.h"
#include "places_panel.h"
#include "radial_menu.h"
#include "settings_panel.h"
#include "settings_panel_math.h"
#include "spell_book.h"

extern void ModLog(const char* format, ...);

uint64_t ControllersFrameCount();
// eye_capture.cpp
void EyeCaptureSaveUi(const char* name, uint64_t frame, int style,
                      const void* pixels, int pitch, uint32_t width,
                      uint32_t height);

// The change functions of the areas.
void ComfortCameraSetSnap(int degrees, bool smooth, const char* by);
void ComfortCameraSetTurnSpeed(int degrees_per_second, const char* by);
void ComfortCameraSetBlink(int ms, const char* by);
bool TeleportSetOn(bool on, const char* by);
void TabletopGodSetMoveStick(int rule, const char* by);
void TabletopGodSetMoveType(int type, const char* by);
void TabletopGodSetMoveSpeed(int cm, const char* by);
void TabletopGodSetZoomSmooth(int smooth, const char* by);
void VignetteSet(int mode, const char* by);
void ControllersSetRecenterHold(int rule, const char* by);
void ControllersSetLeftHand(int key, const char* by);
void ControllersSetOpenStroke(int on, const char* by);
void HintCardSetLearn(int learn, const char* by);
void HintCardSetStyle(int style, const char* by);
void OverlaySetTrail(bool on, const char* by);

namespace {

using namespace settings_math;
namespace kit = panel_kit;

const char* const kBy = "the VR Settings window";

// --- the ini ---------------------------------------------------------------------

bool g_read = false;
char g_ini[MAX_PATH] = "";
bool g_on = true;
bool g_lean = true;  // settings_lean
int g_value[kSettings];
int g_start[kSettings];  // the values of the start of this game
int g_welcome_page = 1;
int g_welcome_done = 0;
int g_welcome_save = 0;
int g_welcome_saved = 0;
int g_style_ini = 1;
const int kMostTests = 16;
int g_tests[kMostTests];
int g_test_count = 0;
int g_tests_used = 0;

uint64_t g_changes = 0, g_live = 0, g_later = 0, g_write_failed = 0;

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  const DWORD length = GetModuleFileNameA(nullptr, g_ini, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(g_ini, '\\') : nullptr;
  if (slash == nullptr || strlen(g_ini) + 9 >= MAX_PATH) {
    g_ini[0] = 0;
  } else {
    slash[1] = 0;
    strcat_s(g_ini, MAX_PATH, "bwvr.ini");
  }
  const auto key = [](const char* name, int fallback) {
    return g_ini[0] != 0
               ? static_cast<int>(GetPrivateProfileIntA("bwvr", name, fallback,
                                                        g_ini))
               : fallback;
  };
  g_on = key("settings_panel", 1) != 0;
  g_lean = key("settings_lean", 1) != 0;
  for (int i = 0; i < kSettings; ++i) {
    g_value[i] = ShownValue(i, key(kTable[i].key, kTable[i].fallback));
    g_start[i] = g_value[i];
  }
  g_welcome_page = key("welcome_page", 1);
  g_welcome_done = key("welcome_done", 0);
  g_welcome_save = key("welcome_test_save", 0);
  g_style_ini = key("hint_card_style", 1) != 0 ? 1 : 0;
  char text[128] = "";
  if (g_ini[0] != 0) {
    GetPrivateProfileStringA("bwvr", "welcome_test_parts", "", text,
                             sizeof(text), g_ini);
  }
  for (const char* p = text; *p != 0 && g_test_count < kMostTests;) {
    while (*p == ' ' || *p == ',') ++p;
    const char* end = p;
    while (*end != 0 && *end != ' ' && *end != ',') ++end;
    const size_t n = static_cast<size_t>(end - p);
    if (n == 8 && strncmp(p, "settings", 8) == 0) {
      g_tests[g_test_count++] = kWelcomeSettings;
    } else if (n == 4 && strncmp(p, "play", 4) == 0) {
      g_tests[g_test_count++] = kWelcomePlay;
    }
    p = end;
  }
  ModLog("settings     : settings_panel=%d: %s. settings_lean=%d: %s. "
         "welcome_page=%d, welcome_done=%d: %s", g_on ? 1 : 0,
         g_on ? "page 2 of the ring as made has the item \"settings\""
              : "the ring has no item \"settings\"",
         g_lean ? 1 : 0,
         g_lean ? "the window has the rows that a player changes"
                : "the window has each row of the table",
         g_welcome_page, g_welcome_done,
         WelcomeShows(g_welcome_page, g_welcome_done)
             ? "the welcome page shows at this start"
             : "no welcome page");
  if (g_welcome_save > 0 || g_test_count > 0) {
    ModLog("settings     : welcome_test_save=%d, welcome_test_parts has %d "
           "buttons (a test)", g_welcome_save, g_test_count);
  }
}

}  // namespace

// The key into bwvr.ini, as the launcher writes it. False when Windows
// refused the write. The page Controls (controls_page.cpp) writes its keys
// here too.
bool SettingsPanelWriteKey(const char* key, int value) {
  ReadSettings();
  char text[16];
  IniText(value, text, sizeof(text));
  return g_ini[0] != 0 &&
         WritePrivateProfileStringA("bwvr", key, text, g_ini) != FALSE;
}

namespace {

bool WriteKey(const char* key, int value) {
  return SettingsPanelWriteKey(key, value);
}

// --- a change --------------------------------------------------------------------

// The change function of the area, for a setting that acts at once.
void ApplyLive(int id, int v) {
  switch (id) {
    case kSnapTurn:
      ComfortCameraSetSnap(SnapDegrees(v), SnapTurnSmooth(v), kBy);
      break;
    case kTurnSpeed: ComfortCameraSetTurnSpeed(v, kBy); break;
    case kTeleport: TeleportSetOn(v != 0, kBy); break;
    case kMoveType: TabletopGodSetMoveType(v, kBy); break;
    case kMoveStick: TabletopGodSetMoveStick(v, kBy); break;
    case kMoveSpeed: TabletopGodSetMoveSpeed(v, kBy); break;
    case kZoomSmooth: TabletopGodSetZoomSmooth(v, kBy); break;
    case kVignette: VignetteSet(v, kBy); break;
    case kBlink: ComfortCameraSetBlink(BlinkMs(v), kBy); break;
    case kRecenterHold: ControllersSetRecenterHold(v, kBy); break;
    case kLeftHand: ControllersSetLeftHand(v, kBy); break;
    case kGestureTrail: OverlaySetTrail(v != 0, kBy); break;
    case kOpenStroke: ControllersSetOpenStroke(v, kBy); break;
    case kHintLearn: HintCardSetLearn(v, kBy); break;
    case kCardStyle: HintCardSetStyle(v, kBy); break;
    default: break;
  }
}

void Narrow(const wchar_t* in, char* out, size_t size) {
  size_t n = 0;
  for (; n + 1 < size && in != nullptr && in[n] != 0; ++n) {
    out[n] = (in[n] >= 32 && in[n] < 127) ? static_cast<char>(in[n]) : '?';
  }
  out[n] = 0;
}

// --- the rows of the window ------------------------------------------------------
//
// The rows of the table, then the rows of the page Controls
// (controls_page.cpp) as the last group, with controls_page=1. A row i of
// kSettings or more is row i - kSettings of that page.

// The tab of the page Controls: after the tabs of the rows.
int ControlsTab() { return TabCount(g_lean); }
bool IsControls(int i) { return i >= kSettings; }

int Groups() {
  ReadSettings();
  return TabCount(g_lean) + (ControlsPageOn() ? 1 : 0);
}
const wchar_t* GroupName(int g) {
  if (g == ControlsTab()) return L"Controls";
  const int group = GroupOfTab(g, g_lean);
  return group >= 0 ? kGroupName[group] : L"";
}
int Count() {
  return kSettings + (ControlsPageOn() ? ControlsRowCount() : 0);
}
int GroupOf(int i) {
  return IsControls(i) ? ControlsTab() : TabOfGroup(kTable[i].group, g_lean);
}
bool InWindowRow(int i) {
  return IsControls(i) ? ControlsRowIn(i - kSettings)
                       : settings_math::InWindow(i, g_lean);
}
const wchar_t* Label(int i) {
  return IsControls(i) ? ControlsRowLabel(i - kSettings) : kTable[i].label;
}
const wchar_t* Name(int i) {
  return IsControls(i) ? ControlsRowName(i - kSettings) : kTable[i].name;
}
const wchar_t* Tip(int i) {
  return IsControls(i) ? ControlsRowTip(i - kSettings) : kTable[i].tip;
}
// A row that acts at the next start has no words of its own in the lean
// window: the bottom row asks for the restart (IdleNow).
const wchar_t* RowNote(int i) {
  if (IsControls(i) || g_lean) return nullptr;
  return kTable[i].when == kNextStart ? L"at the next start" : nullptr;
}

// The note of a setting has no last line: a player needs no key.
void LastLine(int i, wchar_t* out, int size) {
  if (IsControls(i)) {
    ControlsRowLastLine(i - kSettings, out, size);
    return;
  }
  if (size > 0) out[0] = 0;
}

bool g_said_restart = false;

const wchar_t* IdleNow() {
  ReadSettings();
  const bool restart = RestartNeeded(g_start, g_value);
  if (restart != g_said_restart) {
    g_said_restart = restart;
    ModLog("settings     : %s, frame %llu",
           restart ? "the bottom row of the window asks for a restart of the "
                     "game: a setting that acts at the next start has a new "
                     "value"
                   : "the bottom row no longer asks for a restart",
           ControllersFrameCount());
  }
  return restart ? kRestartLine : nullptr;
}

// Two values that are not off and on: the arrows of a choice.
bool Arrows(int i) {
  return !IsControls(i) && !IsToggle(kTable[i].values);
}

const WindowPage* PageOf(int group) {
  return group == ControlsTab() ? ControlsPage() : nullptr;
}

// The value of a row is the place of the ini value in the list of its
// values, or -1 for a value that the list does not have.
int Value(int i) {
  ReadSettings();
  if (IsControls(i)) {
    // The drawing of the page lights the control of the row under the ray.
    const int hover = DebugWindowHoverSwitch();
    ControlsSetHoverRow(hover >= kSettings ? hover - kSettings : -1);
    return ControlsRowValue(i - kSettings);
  }
  return IndexOfValue(kTable[i].values, g_value[i]);
}

int ValueCountOf(int i) {
  return IsControls(i) ? ControlsRowValueCount(i - kSettings)
                       : ValueCount(kTable[i].values);
}

void ValueText(int i, int index, wchar_t* out, int size) {
  if (IsControls(i)) {
    ControlsRowValueText(i - kSettings, index, out, size);
    return;
  }
  if (!ValueAt(kTable[i].values, index, out, static_cast<size_t>(size),
               nullptr)) {
    swprintf_s(out, size, L"%d", g_value[i]);
  }
}

const uint8_t* Picture(int i, int value, int style) {
  return IsControls(i) ? ControlsRowPicture(i - kSettings, value, style)
                       : nullptr;
}

const wchar_t* FixedWord(int i) {
  return IsControls(i) ? ControlsRowFixedWord(i - kSettings) : nullptr;
}

bool Step(int i, bool back) {
  ReadSettings();
  if (IsControls(i)) return ControlsRowStep(i - kSettings, back);
  const Setting& s = kTable[i];
  const int count = ValueCount(s.values);
  const int index = StepIndex(IndexOfValue(s.values, g_value[i]), count, back);
  int v = 0;
  wchar_t word[40] = L"";
  if (!ValueAt(s.values, index, word, 40, &v)) return false;
  const int before = g_value[i];
  g_value[i] = v;
  ++g_changes;
  const bool wrote = WriteKey(s.key, v);
  char words[48];
  Narrow(word, words, sizeof(words));
  if (wrote) {
    ModLog("settings     : %s goes from %d to %d (%s), written to bwvr.ini. "
           "%s, frame %llu", s.key, before, v, words,
           s.when == kNow ? "It acts at once"
                          : "It acts at the next start of the game",
           ControllersFrameCount());
  } else {
    ++g_write_failed;
    ModLog("settings     : %s goes from %d to %d (%s), and Windows refused "
           "the write to bwvr.ini, so the change lasts for this game only, "
           "frame %llu", s.key, before, v, words, ControllersFrameCount());
  }
  if (s.when == kNow) {
    ++g_live;
    ApplyLive(s.id, v);
  } else {
    ++g_later;
  }
  return true;
}

const WindowRows kSettingsRows = {
    1,
    "settings     ",
    L"VR Settings",
    L"Point at a setting to read what it does.",
    Groups,
    GroupName,
    Count,
    GroupOf,
    InWindowRow,
    Label,
    Name,
    Tip,
    LastLine,
    Value,
    ValueCountOf,
    ValueText,
    Step,
    RowNote,
    Picture,
    FixedWord,
    IdleNow,
    PageOf,
    Arrows,
};

// --- the welcome page ------------------------------------------------------------

kit::Room g_room = {};
volatile LONG g_holds = 0;
bool g_hold_after = false;
bool g_pending = false;   // the page waits for its first open
double g_ready_since = 0.0;
bool g_ready = false;
uint64_t g_welcome_opens = 0;

kit::Canvas g_canvas = {};
kit::Tiles g_tiles = {};
kit::Font g_title_font = {}, g_words_font = {}, g_button_font = {};
bool g_fonts_failed = false;
HFONT g_plain_title = nullptr, g_plain_words = nullptr;

struct Look {
  int style;
  uint8_t button[kWelcomeParts];
};
Look g_drawn;
bool g_have_drawn = false;
// The number of the picture (panel_kit::StageLayer).
uint32_t g_welcome_serial = 0;

double NowSeconds() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart) /
         static_cast<double>(frequency.QuadPart);
}

void SetHolds() {
  InterlockedExchange(&g_holds, (g_room.open || g_hold_after) ? 1 : 0);
}

const wchar_t* const kTitle = L"Welcome to the Black & White VR Mod";
const wchar_t* const kWords[2] = {
    L"You can change the settings at any time.",
    L"The mod is a work in progress, so expect some bugs."};
const wchar_t* const kButtonWord[kWelcomeParts] = {L"VR Settings", L"Play"};

// The three controls. The ring button follows button_layout, the hand that
// does not point and the map of its buttons (button_map_math.h).
void ControlLines(wchar_t lines[kWelcomeControls][96]) {
  RingHand hand = {};
  const bool have = ControllersRingHand(&hand);
  const bool new_layout = ControllersButtonLayout() != 0;
  // The ring is on the hand that does not point: Y or X on the left hand,
  // B or A on the right hand.
  const bool left = !have || hand.left;
  const int side = left ? 0 : 1;
  int map[button_map::kButtons];
  ControllersButtonMapOf(side, map);
  const wchar_t* ring = button_map::SlotWord(
      ControllersKindOf(side), left, map,
      new_layout ? button_map::kSecond : button_map::kFirst);
  if (ring == nullptr) {
    ring = new_layout ? (left ? L"Y" : L"B") : (left ? L"X" : L"A");
  }
  wcscpy_s(lines[0], 96, L"Grip of your Hand: take, hold and throw");
  wcscpy_s(lines[1], 96, L"Grip of your other hand: pull the land to move");
  swprintf_s(lines[2], 96, L"%s: open the ring. VR Settings is in the ring",
             ring);
}

void DrawPage(kit::Canvas* c, void*) {
  g_title_font = kit::WordsFont(c, 34);
  g_words_font = kit::WordsFont(c, 24);
  g_button_font = kit::WordsFont(c, 26);
  if (g_title_font.font == nullptr || g_words_font.font == nullptr) return;
  const card_scroll::Picture page = kit::PagePicture(c);
  const int title[4] = {kWelcomeLeft, kWelcomeTitleTop, kWelcomeRight,
                        kWelcomeTitleBottom};
  kit::InkWordsIn(c, page, g_title_font, kTitle, title, 3, kit::kCenter);
  const int half = (kWelcomeWordsBottom - kWelcomeWordsTop) / 2;
  for (int k = 0; k < 2; ++k) {
    const int box[4] = {kWelcomeLeft, kWelcomeWordsTop + k * half,
                        kWelcomeRight, kWelcomeWordsTop + (k + 1) * half};
    kit::InkWordsIn(c, page, g_words_font, kWords[k], box, 7 + k,
                    kit::kCenter);
  }
  wchar_t lines[kWelcomeControls][96];
  ControlLines(lines);
  for (int k = 0; k < kWelcomeControls; ++k) {
    int box[4];
    WelcomeControlBox(k, box);
    kit::InkWordsIn(c, page, g_words_font, lines[k], box, 11 + k,
                    kit::kLeftAlign);
  }
}

bool ParchmentStart() {
  if (g_fonts_failed) return false;
  if (!kit::StartParchment(&g_canvas, &DrawPage, nullptr)) return false;
  if (g_title_font.font == nullptr || g_words_font.font == nullptr ||
      g_button_font.font == nullptr ||
      !kit::MakeTiles(&g_tiles, kWelcomeParts, kit::kLooks)) {
    g_fonts_failed = true;
    ModLog("settings     : no font or no memory for the parchment, so the "
           "welcome page is dark");
    return false;
  }
  return true;
}

void DrawButton(kit::Canvas* c, int part, int look, void*) {
  int r[4];
  WelcomeButtonRect(part, r);
  kit::Button(c, r, look, kButtonWord[part], g_button_font, part * 5 + 2);
}

void Compose(const Look& look) {
  kit::Canvas* c = &g_canvas;
  const bool first = !g_have_drawn || g_drawn.style != 1;
  if (first) kit::ClearCard(c);
  for (int part = 0; part < kWelcomeParts; ++part) {
    if (!first && look.button[part] == g_drawn.button[part]) continue;
    int r[4], box[4];
    WelcomeButtonRect(part, r);
    panel_kit_math::RectBox(r, kit::kRim, kWelcomeWidth, kWelcomeHeight, box);
    kit::ShowPart(c, &g_tiles, part, look.button[part], box, &DrawButton,
                  nullptr);
  }
}

void RasterPlain(const Look& look) {
  kit::Canvas* c = &g_canvas;
  if (g_plain_title == nullptr) {
    const auto font = [](int em, int weight) {
      return CreateFontW(-em, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                         ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
                         L"Segoe UI");
    };
    g_plain_title = font(40, FW_SEMIBOLD);
    g_plain_words = font(30, FW_NORMAL);
  }
  const int all[4] = {0, 0, kWelcomeWidth, kWelcomeHeight};
  kit::PlainFill(c, all, RGB(22, 24, 28));
  const int title[4] = {0, kWelcomeTitleTop, kWelcomeWidth,
                        kWelcomeTitleBottom};
  kit::PlainText(c, g_plain_title, RGB(235, 235, 235), kTitle, title);
  const int half = (kWelcomeWordsBottom - kWelcomeWordsTop) / 2;
  for (int k = 0; k < 2; ++k) {
    const int box[4] = {0, kWelcomeWordsTop + k * half, kWelcomeWidth,
                        kWelcomeWordsTop + (k + 1) * half};
    kit::PlainText(c, g_plain_words, RGB(220, 220, 220), kWords[k], box);
  }
  wchar_t lines[kWelcomeControls][96];
  ControlLines(lines);
  SelectObject(c->dc, g_plain_words);
  SetTextColor(c->dc, RGB(230, 190, 110));
  for (int k = 0; k < kWelcomeControls; ++k) {
    int box[4];
    WelcomeControlBox(k, box);
    RECT rect = {box[0], box[1], box[2], box[3]};
    DrawTextW(c->dc, lines[k], -1, &rect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }
  for (int part = 0; part < kWelcomeParts; ++part) {
    int r[4];
    WelcomeButtonRect(part, r);
    kit::PlainButton(c, r, look.button[part], kButtonWord[part],
                     g_plain_words);
  }
  kit::PlainDone(c);
}

void BuildLook(Look* look) {
  memset(look, 0, sizeof(*look));
  look->style = kit::CardStyle(g_style_ini);
  const int flash = kit::Flashing(&g_room);
  for (int part = 0; part < kWelcomeParts; ++part) {
    look->button[part] = static_cast<uint8_t>(
        part == flash ? kit::kFlash
                      : (part == g_room.hover ? kit::kHover : kit::kPlain));
  }
}

int HitPart(float u, float v, void*) { return WelcomePartAt(u, v); }

void Close(const char* why, bool done, bool hold) {
  if (!g_room.open) return;
  kit::Close(&g_room);
  g_hold_after = hold;
  SetHolds();
  if (done) {
    g_pending = false;
    g_welcome_done = 1;
    const bool wrote = WriteKey("welcome_done", 1);
    ModLog("settings     : the welcome page closes: %s. welcome_done=1 %s, "
           "frame %llu", why,
           wrote ? "is in bwvr.ini, so the page does not show again"
                 : "did not go into bwvr.ini: Windows refused the write",
           ControllersFrameCount());
  } else {
    ModLog("settings     : the welcome page closes: %s. It shows again "
           "later, frame %llu", why, ControllersFrameCount());
  }
  g_ready = false;
}

// The page can open: the controllers can act, and no dialog box, cut scene,
// ring, spell book or other panel shows.
bool CanOpen() {
  RingHand hand = {};
  if (!ControllersRingHand(&hand) || !hand.active || !hand.usable ||
      hand.dialog) {
    return false;
  }
  return !CutsceneWindowFlat() && !RadialMenuOpen() && !SpellBookOpen() &&
         !PlacesPanelOpen() && !DebugWindowOpen();
}

}  // namespace

bool SettingsPanelOn() {
  ReadSettings();
  return g_on;
}

bool SettingsPanelOpenFromRing() {
  ReadSettings();
  return DebugWindowOpenWith(&kSettingsRows);
}

const WindowRows* SettingsPanelRows() { return &kSettingsRows; }

int SettingsPanelWarmRows(int group, int* switches, int* values, int most) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return 0;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  int n = 0;
  ReadSettings();
  const int wanted = GroupOfTab(group, g_lean);
  for (int i = 0; i < kSettings && n < most; ++i) {
    if (kTable[i].group != wanted || !settings_math::InWindow(i, g_lean)) {
      continue;
    }
    const int v = static_cast<int>(GetPrivateProfileIntA(
        "bwvr", kTable[i].key, kTable[i].fallback, path));
    switches[n] = i;
    values[n] = IndexOfValue(kTable[i].values, ShownValue(i, v));
    ++n;
  }
  return n;
}

bool WelcomePageOpen() { return g_room.open; }

bool WelcomePageHoldsTrigger() {
  return InterlockedCompareExchange(&g_holds, 0, 0) != 0;
}

bool WelcomePageRay(float* meters) {
  *meters = g_room.open ? g_room.meters : 0.0f;
  return g_room.open;
}

bool WelcomePageCloseBy(const char* why) {
  if (!g_room.open) return false;
  Close(why, true, false);
  return true;
}

bool WelcomePageTestTarget(float out[3]) {
  ReadSettings();
  if (!g_room.open || g_test_count == 0) return false;
  const int part = g_tests[g_tests_used++ % g_test_count];
  float u = 0.0f, v = 0.0f;
  WelcomePartMiddle(part, &u, &v);
  kit::PointOf(g_room, u, v, out);
  ModLog("settings     : the word aimdebug aims at the button %s of the "
         "welcome page, at %.3f %.3f %.3f m",
         part == kWelcomeSettings ? "VR Settings" : "Play", out[0], out[1],
         out[2]);
  return true;
}

void WelcomePageFrame() {
  static bool started = false;
  ReadSettings();
  if (!started) {
    started = true;
    g_pending = WelcomeShows(g_welcome_page, g_welcome_done);
  }
  const uint64_t frame = ControllersFrameCount();

  if (!g_room.open) {
    if (g_hold_after) {
      float p[3], q[4], trigger = 0.0f;
      if (!ControllersPointingAim(p, q, &trigger) || trigger < 0.4f) {
        g_hold_after = false;
        SetHolds();
      }
    }
    if (!g_pending) return;
    // The wait for the first open.
    const double now = NowSeconds();
    if (!CanOpen()) {
      g_ready = false;
      return;
    }
    if (!g_ready) {
      g_ready = true;
      g_ready_since = now;
    }
    if (now - g_ready_since < kWelcomeWaitSeconds) return;
    if (!kit::Open(&g_room, debugwin_math::kDistance, debugwin_math::kPitch,
                   kWelcomeMeters, kWelcomeMetersHigh)) {
      return;
    }
    g_hold_after = false;
    SetHolds();
    ++g_welcome_opens;
    const menu_room_math::Panel& p = g_room.panel;
    ModLog("settings     : the welcome page opens, %.2f by %.2f m, its "
           "middle at %.2f %.2f %.2f m, frame %llu", p.width, p.height,
           p.position[0], p.position[1], p.position[2], frame);
  }

  // What closes the page for now.
  RingHand hand;
  const bool have_hand = ControllersRingHand(&hand);
  if (RadialMenuOpen()) {
    Close("the ring opens", false, false);
  } else if (SpellBookOpen()) {
    Close("the spell book opens", false, false);
  } else if (have_hand && !hand.usable) {
    Close("the controllers cannot act now", false, false);
  } else if (have_hand && hand.dialog) {
    Close("a dialog box is up", false, false);
  } else if (CutsceneWindowFlat()) {
    Close("a cut scene plays", false, false);
  }
  if (!g_room.open) return;
  kit::FollowRecenter(&g_room);

  const int before = g_room.hover;
  float trigger = 0.0f;
  const int pressed = kit::Step(&g_room, &HitPart, nullptr, &trigger);
  if (g_room.hover != before && g_room.hover >= 0) {
    ModLog("settings     : the ray is on the button %s of the welcome page, "
           "frame %llu", g_room.hover == kWelcomeSettings ? "VR Settings" : "Play",
           frame);
  }
  if (pressed == kWelcomePlay) {
    kit::Pressed(&g_room, pressed);
    ModLog("settings     : the trigger presses Play, frame %llu", frame);
    Close("the player pressed Play", true, true);
    return;
  }
  if (pressed == kWelcomeSettings) {
    kit::Pressed(&g_room, pressed);
    ModLog("settings     : the trigger presses VR Settings, frame %llu", frame);
    Close("the player pressed VR Settings", true, true);
    if (!DebugWindowOpenWith(&kSettingsRows)) {
      ModLog("settings     : the settings window does not open: the head has "
             "no pose, frame %llu", frame);
    }
    return;
  }

  // The picture of this frame.
  if (!kit::Start(&g_canvas, kWelcomeWidth, kWelcomeHeight, "welcome page "))
    return;
  Look look;
  BuildLook(&look);
  const uint8_t* pixels = reinterpret_cast<const uint8_t*>(g_canvas.bits);
  bool fresh = false;
  const bool same = g_have_drawn && memcmp(&look.button, &g_drawn.button,
                                           sizeof(look.button)) == 0;
  if (look.style != 0 && ParchmentStart()) {
    look.style = 1;
    if (!same || g_drawn.style != 1) {
      LARGE_INTEGER f, t0, t1;
      QueryPerformanceFrequency(&f);
      QueryPerformanceCounter(&t0);
      Compose(look);
      QueryPerformanceCounter(&t1);
      fresh = true;
      ModLog("settings     : frame %llu, a new picture of the welcome page "
             "took %.2f ms", frame,
             static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000.0 /
                 static_cast<double>(f.QuadPart));
    }
    pixels = g_canvas.card;
  } else {
    look.style = 0;
    if (!same || g_drawn.style != 0) {
      RasterPlain(look);
      fresh = true;
    }
  }
  g_drawn = look;
  g_have_drawn = true;
  if (fresh) g_welcome_serial = bwvr::XrScreen::NewCardSerial();
  if (fresh && g_welcome_saved < g_welcome_save) {
    ++g_welcome_saved;
    EyeCaptureSaveUi("welcome", frame, 1, pixels, kWelcomeWidth * 4,
                     kWelcomeWidth, kWelcomeHeight);
  }
  if (!kit::StageLayer(g_room, pixels, kWelcomeWidth, kWelcomeHeight,
                       g_welcome_serial)) {
    Close("the runtime refused the layer of the ring", false, false);
  }
}

void SettingsPanelReport() {
  if (g_changes + g_welcome_opens == 0) return;
  ModLog("settings     : %llu changes in the settings window this window: "
         "%llu acted at once, %llu act at the next start, %llu writes that "
         "Windows refused. The welcome page opened %llu times", g_changes,
         g_live, g_later, g_write_failed, g_welcome_opens);
  g_changes = g_live = g_later = g_write_failed = g_welcome_opens = 0;
}
