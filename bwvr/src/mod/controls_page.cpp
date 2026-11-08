// The page Controls of the settings window. See controls_page.h,
// docs/controllers.md, "The map of the buttons", and docs/ui-layer.md, "The
// page Controls".
//
// WHY. A player with a different hand, a different controller or a
// different habit wants a job on a different button. The mod knows the kind
// of each controller from its interaction profile (button_map_math.h), so
// the page draws those controllers and names their buttons.
//
// THE ROWS of the group Controls, in this order:
//
//   the drawing      4 rows: both controllers in ink, each control with a
//                    line of words: its name and its job. The control of
//                    the row under the ray is gold
//   the buttons      a row for each button that a job can use: the first
//                    and the second face button, the stick click and the
//                    menu button of the left controller, then of the right
//                    controller. The arrows give the button the next job of
//                    the same controller, and the button that had that job
//                    takes the old job of this button
//   Button defaults  each button gets its job as made
//   the fixed rows   the trigger, the grip and the stick of each hand, with
//                    their jobs, dim. The note of each row says why it stays
//
// A CHANGE writes button_map_hand or button_map_other into bwvr.ini at once
// (SettingsPanelWriteKey), and controllers.cpp uses the new map when each
// button of both controllers is up (ControllersSetButtonMap). The hint cards
// and the help words take the new buttons in the next frame
// (controller_hints.cpp).
//
// THE DRAWING is made in a picture of its own, 692 by 304 pixels (the width
// of the list and 4 rows), one time for each look, and again when a job, the
// kind of a controller or the lit control changes. Each row of the drawing
// is a band of it (WindowRows::picture). The shapes are circles, capsules
// and rounded boxes with a distance test (controller_art_math.h), so the art
// is code. The words go through a GDI picture of the page, as the words of
// the cards do.
//
// THE WARM. The drawing takes about 11 ms. With panel_warm=1 the frame
// thread waits until it knows the interaction profile of both controllers
// (ControlsPageWarmFrame). Then it takes the state of that frame, and a
// thread of the mod draws the drawing with no lit control. When the page
// first shows the same state, its rows use that drawing. The page waits
// for the end of the thread before it uses the drawing (WaitWarm).
//
//   controls_page=1    0: the settings window has no tab Controls. The keys
//                      of the map still act
//   panel_warm=1       0: the first show of the page draws the drawing on
//                      the frame thread

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <new>

#include "button_map_math.h"
#include "card_scroll.h"
#include "controller_art_math.h"
#include "controllers.h"
#include "controls_layout_math.h"
#include "controls_page.h"
#include "debug_menu.h"
#include "debug_window.h"
#include "debug_window_math.h"
#include "hint_model.h"
#include "panel_kit.h"
#include "panel_raster.h"

extern void ModLog(const char* format, ...);
uint64_t ControllersFrameCount();
bool SettingsPanelWriteKey(const char* key, int value);
const char* ControllerHintsForcedProfile();
// Below: the page with the models.
void ControlsPageModelsWarmFrame();
void ControlsPageModelsReport();

namespace {

namespace art = controller_art;
namespace map = button_map;
namespace kit = panel_kit;

const char* const kBy = "the page Controls";
const char* const kMapKey[2] = {"button_map_hand", "button_map_other"};

// --- the rows --------------------------------------------------------------------

const int kPictureRows = art::kBands;
const int kButtonRow = kPictureRows;                    // 8 rows
const int kDefaultsRow = kButtonRow + 2 * map::kButtons;
const int kFixedRow = kDefaultsRow + 1;                 // 6 rows
const int kRows = kFixedRow + 2 * map::kFixed;

bool g_read = false;
bool g_on = true;
bool g_models_on = true;  // controls_page_models
int g_hover = -1;
uint64_t g_steps = 0, g_defaults = 0, g_draws = 0;
int g_style_ini = 1;    // hint_card_style
bool g_warm_on = true;  // panel_warm
double g_draw_ms = 0.0, g_draw_most_ms = 0.0;

void Read() {
  if (g_read) return;
  g_read = true;
  char path[MAX_PATH];
  const DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = n > 0 && n < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash != nullptr && strlen(path) + 9 < MAX_PATH) {
    slash[1] = 0;
    strcat_s(path, "bwvr.ini");
    g_on = GetPrivateProfileIntA("bwvr", "controls_page", 1, path) != 0;
    g_style_ini =
        GetPrivateProfileIntA("bwvr", "hint_card_style", 1, path) != 0 ? 1 : 0;
    g_warm_on = GetPrivateProfileIntA("bwvr", "panel_warm", 1, path) != 0;
    g_models_on =
        GetPrivateProfileIntA("bwvr", "controls_page_models", 1, path) != 0;
  }
  ModLog("controls     : controls_page=%d: %s. controls_page_models=%d: %s",
         g_on ? 1 : 0,
         g_on ? "the settings window has the tab Controls"
              : "the settings window has no tab Controls",
         g_models_on ? 1 : 0,
         g_models_on ? "the page shows the models of the controllers, a box "
                       "with a list for each button, and Reset Defaults"
                     : "the page shows the drawing of the controllers and a "
                       "row with arrows for each button");
}

double NowMs() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return 1000.0 * static_cast<double>(count.QuadPart) /
         static_cast<double>(frequency.QuadPart);
}

void Narrow(const wchar_t* in, char* out, size_t size) {
  size_t n = 0;
  for (; n + 1 < size && in != nullptr && in[n] != 0; ++n) {
    out[n] = (in[n] >= 32 && in[n] < 127) ? static_cast<char>(in[n]) : '?';
  }
  out[n] = 0;
}

// What the page shows now: the kind and the map of each controller, the
// hand that points and the settings of the jobs.
struct State {
  int kind[2];
  int map[2][map::kButtons];
  int pointing;  // the side of the Hand: 0 left, 1 right
  map::Jobs jobs;
};

// The state of this frame. The rows ask for it many times in a frame, so it
// is kept until the next frame or the next change.
State g_state;
uint64_t g_state_frame = ~0ull;

State Now() {
  const uint64_t frame = ControllersFrameCount();
  if (frame == g_state_frame) return g_state;
  State s;
  memset(&s, 0, sizeof(s));
  const char* forced = ControllerHintsForcedProfile();
  for (int c = 0; c < 2; ++c) {
    s.kind[c] = forced != nullptr && forced[0] != 0
                    ? map::KindOfProfile(forced)
                    : ControllersKindOf(c);
    ControllersButtonMapOf(c, s.map[c]);
  }
  s.pointing = ControllersPointingSide();
  ControllersButtonJobs(&s.jobs);
  g_state = s;
  g_state_frame = frame;
  return s;
}

// The side and the button of a row of a button.
bool ButtonRow(int k, int* side, int* button) {
  if (k < kButtonRow || k >= kDefaultsRow) return false;
  *side = (k - kButtonRow) / map::kButtons;
  *button = (k - kButtonRow) % map::kButtons;
  return true;
}

bool FixedRow(int k, int* side, int* fixed) {
  if (k < kFixedRow || k >= kRows) return false;
  *side = (k - kFixedRow) / map::kFixed;
  *fixed = (k - kFixedRow) % map::kFixed;
  return true;
}

// A button whose row has arrows: the controller can move it, and it has a
// second job to change to.
bool CanStep(const State& s, int side, int button) {
  int choices[map::kButtons];
  return map::Movable(s.kind[side], side == 0, button) &&
         map::SlotChoices(s.kind[side], side == 0, choices) >= 2;
}

int ActionOf(const State& s, int side, int button) {
  return map::ButtonAction(s.jobs, s.kind[side], side == s.pointing, side == 0,
                           s.map[side], button);
}

// The control of the drawing that a row lights, as side * 8 + control, or
// -1.
int LitOf(int k) {
  int side = 0, which = 0;
  if (ButtonRow(k, &side, &which)) return side * 8 + which;
  if (FixedRow(k, &side, &which)) {
    return side * 8 + art::ControlOfFixed(which);
  }
  return -1;
}

// --- the drawing -----------------------------------------------------------------

struct Paint {
  float r, g, b;
};

// The ink of the cards (card_scroll.cpp), and the gold of a key under the
// ray. The dark look has light ink.
const Paint kInk[2] = {{226.0f, 226.0f, 226.0f}, {52.0f, 30.0f, 14.0f}};
const Paint kGold[2] = {{205.0f, 172.0f, 96.0f}, {214.0f, 150.0f, 36.0f}};

inline void Over(uint8_t* out, const Paint& c, float a) {
  if (a <= 0.0f) return;
  if (a > 1.0f) a = 1.0f;
  const float k = 1.0f - a;
  out[0] = static_cast<uint8_t>(c.b * a + out[0] * k + 0.5f);
  out[1] = static_cast<uint8_t>(c.g * a + out[1] * k + 0.5f);
  out[2] = static_cast<uint8_t>(c.r * a + out[2] * k + 0.5f);
  out[3] = static_cast<uint8_t>(255.0f * a + out[3] * k + 0.5f);
}

inline float Cover(float d) {
  return d <= -0.5f ? 1.0f : (d >= 0.5f ? 0.0f : 0.5f - d);
}

uint8_t* g_picture[2] = {nullptr, nullptr};  // the dark look, the parchment
DWORD g_warm_id = 0;  // the thread of the warm of the drawing
int g_drawn_value[2] = {-1, -1};
kit::Canvas g_letters = {};
HFONT g_font[2][3] = {};  // each look: the head, the lines, the letters
bool g_no_fonts = false;

// The union of the shapes as an outline of `stroke` pixels, with a wash of
// `wash` inside.
void Shapes(uint8_t* to, bool left, const art::Shape* shapes, int count,
            float stroke, const Paint& ink, float ink_a, const Paint& wash,
            float wash_a) {
  float box[4] = {1e9f, 1e9f, -1e9f, -1e9f};
  for (int n = 0; n < count; ++n) {
    float b[4];
    art::Bounds(shapes[n], b);
    for (int k = 0; k < 2; ++k) {
      if (b[k] < box[k]) box[k] = b[k];
      if (b[k + 2] > box[k + 2]) box[k + 2] = b[k + 2];
    }
  }
  float x0, y0, x1, y1;
  art::ToPixel(left, box[0], box[1], &x0, &y0);
  art::ToPixel(left, box[2], box[3], &x1, &y1);
  const float pad = stroke + 2.0f;
  int px0 = static_cast<int>((x0 < x1 ? x0 : x1) - pad);
  int px1 = static_cast<int>((x0 < x1 ? x1 : x0) + pad) + 1;
  int py0 = static_cast<int>(y0 - pad);
  int py1 = static_cast<int>(y1 + pad) + 1;
  if (px0 < 0) px0 = 0;
  if (py0 < 0) py0 = 0;
  if (px1 > art::kWidth) px1 = art::kWidth;
  if (py1 > art::kHeight) py1 = art::kHeight;
  const float s = art::Scale();
  for (int y = py0; y < py1; ++y) {
    uint8_t* row = to + static_cast<size_t>(y) * art::kWidth * 4;
    for (int x = px0; x < px1; ++x) {
      float ux, uy;
      art::ToUnits(left, x + 0.5f, y + 0.5f, &ux, &uy);
      float d = 1e9f;
      for (int n = 0; n < count; ++n) {
        const float e = art::Distance(shapes[n], ux, uy);
        if (e < d) d = e;
      }
      d *= s;
      if (d > pad) continue;
      uint8_t* p = row + x * 4;
      if (wash_a > 0.0f) Over(p, wash, wash_a * Cover(d));
      Over(p, ink, ink_a * Cover(fabsf(d) - stroke * 0.5f));
    }
  }
}

// A line of ink from a to b, in pixels.
void Line(uint8_t* to, float ax, float ay, float bx, float by, float width,
          const Paint& ink, float a) {
  const art::Shape line = {art::kCapsule, ax, ay, bx, by, width * 0.5f};
  float b[4];
  art::Bounds(line, b);
  const int x0 = b[0] - 2 < 0 ? 0 : static_cast<int>(b[0] - 2);
  const int y0 = b[1] - 2 < 0 ? 0 : static_cast<int>(b[1] - 2);
  const int x1 = b[2] + 3 > art::kWidth ? art::kWidth
                                        : static_cast<int>(b[2] + 3);
  const int y1 = b[3] + 3 > art::kHeight ? art::kHeight
                                         : static_cast<int>(b[3] + 3);
  for (int y = y0; y < y1; ++y) {
    uint8_t* row = to + static_cast<size_t>(y) * art::kWidth * 4;
    for (int x = x0; x < x1; ++x) {
      Over(row + x * 4, ink, a * Cover(art::Distance(line, x + 0.5f, y + 0.5f)));
    }
  }
}

bool Fonts(int style) {
  if (g_no_fonts) return false;
  if (!kit::Start(&g_letters, art::kWidth, art::kHeight, "controls     ")) {
    g_no_fonts = true;
    return false;
  }
  if (g_font[style][0] != nullptr) return true;
  static const int kCap[3] = {17, 13, 12};
  for (int f = 0; f < 3; ++f) {
    if (style == 1 && card_scroll::Start(g_letters.dc)) {
      g_font[1][f] = f == 2 ? card_scroll::SealFont(g_letters.dc, kCap[f])
                            : card_scroll::WordsFont(g_letters.dc, kCap[f]);
    }
    if (g_font[style][f] == nullptr) {
      g_font[style][f] = CreateFontW(
          -(kCap[f] * 10 / 7), 0, 0, 0, f == 1 ? FW_NORMAL : FW_SEMIBOLD,
          FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
          L"Segoe UI");
    }
  }
  if (g_font[style][0] == nullptr || g_font[style][1] == nullptr ||
      g_font[style][2] == nullptr) {
    g_no_fonts = true;
    ModLog("controls     : GDI gave no font, so the drawing has no words");
    return false;
  }
  return true;
}

// The width of words in pixels.
int Measure(int style, int font, const wchar_t* words) {
  if (words == nullptr || words[0] == 0 || !Fonts(style)) return 0;
  SelectObject(g_letters.dc, g_font[style][font]);
  RECT size = {0, 0, art::kWidth, art::kHeight};
  DrawTextW(g_letters.dc, words, -1, &size,
            DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
  return size.right - size.left;
}

// Words in the box (x0, y0, x1, y1).
void Words(uint8_t* to, int style, int font, const wchar_t* words,
           const int box[4], UINT align, const Paint& ink, float a) {
  if (words == nullptr || words[0] == 0 || !Fonts(style)) return;
  int b[4] = {box[0], box[1], box[2], box[3]};
  if (b[0] < 0) b[0] = 0;
  if (b[1] < 0) b[1] = 0;
  if (b[2] > art::kWidth) b[2] = art::kWidth;
  if (b[3] > art::kHeight) b[3] = art::kHeight;
  if (b[2] <= b[0] || b[3] <= b[1]) return;
  HDC dc = g_letters.dc;
  GdiFlush();
  for (int y = b[1]; y < b[3]; ++y) {
    memset(g_letters.bits + static_cast<size_t>(y) * art::kWidth + b[0], 0,
           static_cast<size_t>(b[2] - b[0]) * 4);
  }
  SelectObject(dc, g_font[style][font]);
  SetTextColor(dc, RGB(255, 255, 255));
  SetBkMode(dc, TRANSPARENT);
  RECT rect = {b[0], b[1], b[2], b[3]};
  DrawTextW(dc, words, -1, &rect,
            DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER | align);
  GdiFlush();
  for (int y = b[1]; y < b[3]; ++y) {
    const uint32_t* letters =
        g_letters.bits + static_cast<size_t>(y) * art::kWidth;
    uint8_t* row = to + static_cast<size_t>(y) * art::kWidth * 4;
    for (int x = b[0]; x < b[2]; ++x) {
      const uint32_t cover = (letters[x] >> 8) & 0xFF;
      if (cover != 0) Over(row + x * 4, ink, a * (cover / 255.0f));
    }
  }
}

// The name of a control in its line of words.
const wchar_t* NameOf(int kind, bool left, int control) {
  switch (control) {
    case art::kFirst:
    case art::kSecond:
    case art::kStickClick:
    case art::kMenu:
      return map::ButtonWord(kind, left, control);
    case art::kTrigger:
      return kind == map::kSimple ? L"Select" : L"Trigger";
    case art::kGrip:
      return L"Grip";
    case art::kStick:
      return kind == map::kWand ? L"Pad" : L"Stick";
    default:
      return nullptr;
  }
}

const wchar_t* JobOf(const State& s, int side, int control) {
  if (control < map::kButtons) {
    return map::kActionWord[ActionOf(s, side, control)];
  }
  const int fixed = control == art::kTrigger ? map::kTrigger
                    : control == art::kGrip  ? map::kGrip
                                             : map::kStick;
  return map::FixedAction(s.jobs, s.kind[side], side == s.pointing, fixed);
}

// One controller and its lines of words.
void DrawSide(uint8_t* to, int style, const State& s, int side, int lit) {
  const bool left = side == 0;
  const int kind = s.kind[side];
  const art::Art a = art::ArtOf(kind);
  const Paint& ink = kInk[style];
  const Paint& gold = kGold[style];
  Shapes(to, left, a.body, a.bodies, 3.0f, ink, 1.0f, ink, 0.07f);
  // The controls. A stick click is the stick, so it is drawn one time.
  for (int c = 0; c < art::kControls; ++c) {
    if (c == art::kStickClick || !art::Shows(kind, left, c)) continue;
    const bool on = lit == side * 8 + c ||
                    (c == art::kStick && lit == side * 8 + art::kStickClick);
    const bool idle = c == art::kPad ||
                      (c < map::kButtons &&
                       ActionOf(s, side, c) == map::kNoJob);
    Shapes(to, left, &a.control[c], 1, 2.2f, ink, idle && !on ? 0.45f : 1.0f,
           on ? gold : ink, on ? 0.85f : 0.05f);
    if (c == art::kStick) {
      art::Shape inner = a.control[c];
      inner.r *= 0.5f;
      Shapes(to, left, &inner, 1, 1.6f, ink, 0.7f, ink, 0.0f);
    }
    if (c == art::kFirst || c == art::kSecond) {
      float x, y;
      art::ToPixel(left, a.control[c].x0, a.control[c].y0, &x, &y);
      const int box[4] = {static_cast<int>(x) - 12, static_cast<int>(y) - 11,
                          static_cast<int>(x) + 13, static_cast<int>(y) + 12};
      Words(to, style, 2, map::ButtonWord(kind, left, c), box, DT_CENTER, ink,
            1.0f);
    }
  }
  // The lines of words, from the front of the controller down.
  int order[art::kControls];
  float want[art::kControls], place[art::kControls], ax[art::kControls],
      ay[art::kControls];
  int n = 0;
  for (int c = 0; c < art::kControls; ++c) {
    if (c == art::kPad || !art::Shows(kind, left, c)) continue;
    float ux, uy;
    art::Anchor(a.control[c], &ux, &uy);
    art::ToPixel(left, ux, uy, &ax[n], &ay[n]);
    order[n] = c;
    want[n] = ay[n];
    ++n;
  }
  for (int i = 1; i < n; ++i) {
    for (int j = i; j > 0 && want[j] < want[j - 1]; --j) {
      float t = want[j]; want[j] = want[j - 1]; want[j - 1] = t;
      t = ax[j]; ax[j] = ax[j - 1]; ax[j - 1] = t;
      t = ay[j]; ay[j] = ay[j - 1]; ay[j - 1] = t;
      const int o = order[j]; order[j] = order[j - 1]; order[j - 1] = o;
    }
  }
  art::PlaceLines(want, n, static_cast<float>(art::kArtTop + 8),
                  static_cast<float>(art::kFootTop - 12), place);
  int cx0 = 0, cx1 = 0;
  art::WordsColumn(left, &cx0, &cx1);
  for (int i = 0; i < n; ++i) {
    const int c = order[i];
    wchar_t text[64];
    swprintf_s(text, L"%s: %s", NameOf(kind, left, c), JobOf(s, side, c));
    const int y = static_cast<int>(place[i]);
    const int box[4] = {cx0, y - 13, cx1, y + 14};
    const bool on = lit == side * 8 + c;
    if (on) {
      // The words of the lit control are on a band of gold.
      const int w = Measure(style, 1, text);
      const art::Shape band = {art::kBox,
                               left ? cx1 - w * 0.5f : cx0 + w * 0.5f,
                               static_cast<float>(y), w * 0.5f + 6.0f, 12.0f,
                               6.0f};
      for (int py = y - 13; py < y + 14; ++py) {
        if (py < 0 || py >= art::kHeight) continue;
        uint8_t* row = to + static_cast<size_t>(py) * art::kWidth * 4;
        for (int px = cx0 - 8; px < cx1 + 8; ++px) {
          if (px < 0 || px >= art::kWidth) continue;
          Over(row + px * 4, gold,
               0.55f * Cover(art::Distance(band, px + 0.5f, py + 0.5f)));
        }
      }
    }
    Words(to, style, 1, text, box, left ? DT_RIGHT : DT_LEFT, ink, 1.0f);
    const float from = left ? cx1 + 4.0f : cx0 - 4.0f;
    Line(to, from, static_cast<float>(y), ax[i], ay[i], 1.4f, ink,
         on ? 0.9f : 0.5f);
  }
  // The side and the hand under the middle of the controller, and not past
  // the middle of the picture.
  const int middle = (art::ArtLeft(left) + art::ArtRight(left)) / 2;
  const int reach = left ? art::kWidth / 2 - 2 - middle
                         : middle - art::kWidth / 2 - 2;
  const int foot[4] = {middle - reach, art::kFootTop, middle + reach,
                       art::kFootBottom};
  Words(to, style, 1,
        side == s.pointing ? (left ? L"Left: the Hand" : L"Right: the Hand")
                           : (left ? L"Left: other hand"
                                   : L"Right: other hand"),
        foot, DT_CENTER, ink, 1.0f);
}

// The drawing in `style` for the state s, with the control `lit` lit. The
// thread of the warm calls it too, with the state that the frame thread
// took.
void Draw(int style, int lit, const State& s) {
  const double start = NowMs();
  uint8_t* to = g_picture[style];
  memset(to, 0, static_cast<size_t>(art::kWidth) * art::kHeight * 4);
  const Paint& ink = kInk[style];
  const int kind = s.kind[s.pointing];
  wchar_t head[96];
  if (s.kind[0] == s.kind[1]) {
    swprintf_s(head, L"Your controllers: %s", map::kKindName[kind]);
  } else {
    swprintf_s(head, L"Left: %s. Right: %s", map::kKindName[s.kind[0]],
               map::kKindName[s.kind[1]]);
  }
  const int box[4] = {0, art::kHeadTop, art::kWidth, art::kHeadBottom};
  Words(to, style, 0, head, box, DT_CENTER, ink, 1.0f);
  DrawSide(to, style, s, 0, lit);
  DrawSide(to, style, s, 1, lit);
  const double ms = NowMs() - start;
  ++g_draws;
  g_draw_ms += ms;
  if (ms > g_draw_most_ms) g_draw_most_ms = ms;
  if (g_draws <= 4) {
    ModLog("controls     : the drawing of the %s and %s controllers took "
           "%.2f ms (%s), frame %llu, on the %s thread",
           map::kKindLog[s.kind[0]], map::kKindLog[s.kind[1]], ms,
           style != 0 ? "parchment" : "dark", ControllersFrameCount(),
           GetCurrentThreadId() == g_warm_id ? "warm" : "frame");
  }
}

// The value of the rows of the drawing: what it shows. A change draws it
// again.
int DrawingValueOf(const State& s, int lit) {
  uint32_t h = 2166136261u;
  const auto mix = [&](uint32_t v) {
    h ^= v;
    h *= 16777619u;
  };
  for (int c = 0; c < 2; ++c) {
    mix(static_cast<uint32_t>(s.kind[c]));
    for (int b = 0; b < map::kButtons; ++b) {
      mix(static_cast<uint32_t>(ActionOf(s, c, b)));
    }
    for (int f = 0; f < map::kFixed; ++f) {
      const wchar_t* w =
          map::FixedAction(s.jobs, s.kind[c], c == s.pointing, f);
      mix(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(w)));
    }
  }
  mix(static_cast<uint32_t>(s.pointing));
  return static_cast<int>(((h & 0xFFFFFu) << 6) |
                          static_cast<uint32_t>(lit + 1));
}

int DrawingValue() { return DrawingValueOf(Now(), LitOf(g_hover)); }

// --- the warm of the drawing -----------------------------------------------------

bool g_warm_tried = false;
HANDLE g_warm = nullptr;  // the thread, until the frame thread saw its end
State g_warm_state;
int g_warm_style = 1;
int g_warm_value = -1;

DWORD WINAPI WarmDrawing(void*) {
  Draw(g_warm_style, -1, g_warm_state);
  return 0;
}

// On the frame thread, before a use of the drawing or of its letters.
void WaitWarm() {
  if (g_warm == nullptr) return;
  if (WaitForSingleObject(g_warm, 0) != WAIT_OBJECT_0) {
    const double start = NowMs();
    WaitForSingleObject(g_warm, INFINITE);
    ModLog("controls     : the page waited %.1f ms on the frame thread for "
           "the warm of the drawing", NowMs() - start);
  }
  CloseHandle(g_warm);
  g_warm = nullptr;
  g_drawn_value[g_warm_style] = g_warm_value;
}

// --- the notes -------------------------------------------------------------------

// THE NOTES are short: a note of the parchment is at most half of the list
// high (kNoteMost), and a longer note loses its last line.
const wchar_t* const kButtonTip =
    L"The arrows give this button a different job of this controller. The "
    L"button that had that job takes the old job.";

const wchar_t* const kFixedTip[2][map::kFixed] = {
    // The other hand.
    {L"It chooses an item of the ring when the stick opens the ring, and it "
     L"moves the index finger of your hand.",
     L"It grabs the land: you close the hand to hold the land.",
     L"A stick pushes in a direction. A button has no direction."},
    // The Hand.
    {L"It is the click of the game.",
     L"It takes, holds and throws: you close the hand to hold a thing.",
     L"A stick pushes in a direction. A button has no direction."}};

// Words of a row that change with the state are kept here for a frame.
wchar_t g_label[kRows][24];
wchar_t g_word[kRows][24];

}  // namespace

bool ControlsPageOn() {
  Read();
  return g_on;
}

int ControlsRowCount() { return kRows; }

bool ControlsRowIn(int k) {
  Read();
  if (g_models_on) return false;
  if (k < kButtonRow || k == kDefaultsRow) return k >= 0 && k < kRows;
  const State s = Now();
  int side = 0, which = 0;
  if (ButtonRow(k, &side, &which)) {
    return map::Has(s.kind[side], side == 0, which);
  }
  if (FixedRow(k, &side, &which)) return map::HasFixed(s.kind[side], which);
  return false;
}

const wchar_t* ControlsRowLabel(int k) {
  if (k < 0 || k >= kRows || k < kButtonRow) return L"";
  if (k == kDefaultsRow) return L"Button defaults";
  const State s = Now();
  int side = 0, which = 0;
  if (ButtonRow(k, &side, &which)) {
    map::ButtonLabel(s.kind[side], side == 0, which, g_label[k], 24);
    return g_label[k];
  }
  if (FixedRow(k, &side, &which)) {
    const wchar_t* w = map::FixedLabel(s.kind[side], side == 0, which);
    return w != nullptr ? w : L"";
  }
  return L"";
}

const wchar_t* ControlsRowName(int k) { return ControlsRowLabel(k); }

const wchar_t* ControlsRowTip(int k) {
  if (k == kDefaultsRow) {
    return L"Gives each button its job as made, as in the guide of the "
           L"mod.";
  }
  int side = 0, which = 0;
  if (ButtonRow(k, &side, &which)) {
    const State s = Now();
    if (!CanStep(s, side, which)) {
      return s.kind[side] == map::kWand && which == map::kStickClick
                 ? L"The pad turns only while you click it, so the click "
                   L"stays with the pad."
                 : L"This controller has no other button for this job.";
    }
    return kButtonTip;
  }
  if (FixedRow(k, &side, &which)) {
    const State s = Now();
    return kFixedTip[side == s.pointing ? 1 : 0][which];
  }
  return L"";
}

void ControlsRowLastLine(int k, wchar_t* out, int size) {
  int side = 0, which = 0;
  if (k == kDefaultsRow) {
    swprintf_s(out, size, L"A change acts at once.");
  } else if (ButtonRow(k, &side, &which)) {
    swprintf_s(out, size, L"A change acts when each button is up.");
  } else {
    swprintf_s(out, size, L"This control stays on its job.");
  }
}

int ControlsRowValue(int k) {
  if (k < kButtonRow) return DrawingValue();
  if (k == kDefaultsRow) {
    const State s = Now();
    return map::IsAsMade(s.map[0]) && map::IsAsMade(s.map[1]) ? 1 : 0;
  }
  int side = 0, which = 0;
  if (ButtonRow(k, &side, &which)) {
    const State s = Now();
    const int action = ActionOf(s, side, which);
    if (!CanStep(s, side, which)) return kDebugCannot;
    return s.map[side][which] * 16 + action;
  }
  return kDebugCannot;
}

int ControlsRowValueCount(int k) {
  if (k < kButtonRow) return -1;
  if (k == kDefaultsRow) return 2;
  // A button row has arrows, also on a controller with two buttons.
  return 0;
}

void ControlsRowValueText(int k, int value, wchar_t* out, int size) {
  if (k == kDefaultsRow) {
    wcscpy_s(out, size, value != 0 ? L"In use" : L"Changed");
    return;
  }
  int side = 0, which = 0;
  if (ButtonRow(k, &side, &which)) {
    const State s = Now();
    wcscpy_s(out, size, map::kActionWord[ActionOf(s, side, which)]);
    return;
  }
  out[0] = 0;
}

const wchar_t* ControlsRowFixedWord(int k) {
  if (k < kButtonRow || k == kDefaultsRow || k >= kRows) return nullptr;
  const State s = Now();
  int side = 0, which = 0;
  if (ButtonRow(k, &side, &which)) {
    wcscpy_s(g_word[k], map::kActionWord[ActionOf(s, side, which)]);
    return g_word[k];
  }
  if (FixedRow(k, &side, &which)) {
    return map::FixedAction(s.jobs, s.kind[side], side == s.pointing, which);
  }
  return nullptr;
}

bool ControlsRowStep(int k, bool back) {
  State s = Now();
  const uint64_t frame = ControllersFrameCount();
  if (k == kDefaultsRow) {
    if (map::IsAsMade(s.map[0]) && map::IsAsMade(s.map[1])) return false;
    int made[map::kButtons];
    map::AsMade(made);
    bool wrote = true;
    for (int role = 0; role < 2; ++role) {
      wrote &= SettingsPanelWriteKey(kMapKey[role], map::kAsMade);
      ControllersSetButtonMap(role, made, kBy);
    }
    g_state_frame = ~0ull;
    ++g_defaults;
    ModLog("controls     : Button defaults: each button has its job as made. "
           "%s, frame %llu",
           wrote ? "button_map_hand=1234 and button_map_other=1234 are in "
                   "bwvr.ini"
                 : "Windows refused the write to bwvr.ini, so the change "
                   "lasts for this game only",
           frame);
    return true;
  }
  int side = 0, which = 0;
  if (!ButtonRow(k, &side, &which) || !CanStep(s, side, which)) return false;
  const bool left = side == 0;
  const int slot = map::StepSlot(s.kind[side], left, s.map[side][which], back);
  if (slot < 0) return false;
  const int before = ActionOf(s, side, which);
  const int other = map::ButtonOfSlot(s.map[side], slot);
  map::GiveSlot(s.map[side], which, slot);
  const int role = side == s.pointing ? 0 : 1;
  const int value = map::MapValue(s.map[side]);
  const bool wrote = SettingsPanelWriteKey(kMapKey[role], value);
  ControllersSetButtonMap(role, s.map[side], kBy);
  g_state_frame = ~0ull;
  ++g_steps;
  char label[32], from[24], to[24], moved[32];
  wchar_t w[24];
  map::ButtonLabel(s.kind[side], left, which, w, 24);
  Narrow(w, label, sizeof(label));
  Narrow(map::kActionWord[before], from, sizeof(from));
  Narrow(map::kActionWord[ActionOf(s, side, which)], to, sizeof(to));
  map::ButtonLabel(s.kind[side], left, other, w, 24);
  Narrow(w, moved, sizeof(moved));
  ModLog("controls     : %s goes from %s to %s, and %s takes %s. %s=%d %s. It "
         "acts when each button is up, frame %llu", label, from, to, moved,
         from, kMapKey[role], value,
         wrote ? "is in bwvr.ini"
               : "is for this game only: Windows refused the write to "
                 "bwvr.ini",
         frame);
  return true;
}

const uint8_t* ControlsRowPicture(int k, int value, int style) {
  if (k < 0 || k >= kPictureRows) return nullptr;
  WaitWarm();
  style = style != 0 ? 1 : 0;
  const size_t bytes = static_cast<size_t>(art::kWidth) * art::kHeight * 4;
  if (g_picture[style] == nullptr) {
    g_picture[style] = new (std::nothrow) uint8_t[bytes];
    if (g_picture[style] == nullptr) return nullptr;
    g_drawn_value[style] = -1;
  }
  if (g_drawn_value[style] != value) {
    Draw(style, static_cast<int>(value & 63) - 1, Now());
    g_drawn_value[style] = value;
  }
  return g_picture[style] +
         static_cast<size_t>(k) * art::kBandHeight * art::kWidth * 4;
}

void ControlsSetHoverRow(int k) { g_hover = k; }

void ControlsPageWarmFrame() {
  Read();
  if (g_models_on) {
    ControlsPageModelsWarmFrame();
    return;
  }
  if (g_warm_tried) return;
  if (!g_on || !g_warm_on || g_draws > 0) {
    g_warm_tried = true;
    return;
  }
  const char* forced = ControllerHintsForcedProfile();
  if ((forced == nullptr || forced[0] == 0) &&
      (!ControllersProfileKnown(0) || !ControllersProfileKnown(1))) {
    return;
  }
  g_warm_tried = true;
  g_warm_style = kit::CardStyle(g_style_ini);
  g_warm_state = Now();
  g_warm_value = DrawingValueOf(g_warm_state, -1);
  const size_t bytes = static_cast<size_t>(art::kWidth) * art::kHeight * 4;
  if (g_picture[g_warm_style] == nullptr) {
    g_picture[g_warm_style] = new (std::nothrow) uint8_t[bytes];
    if (g_picture[g_warm_style] == nullptr) return;
    g_drawn_value[g_warm_style] = -1;
  }
  HANDLE thread = CreateThread(nullptr, 0, &WarmDrawing, nullptr,
                               CREATE_SUSPENDED, &g_warm_id);
  if (thread == nullptr) {
    ModLog("controls     : panel_warm=1, but Windows gave no thread for the "
           "warm of the drawing, so the page draws it when it shows");
    return;
  }
  g_warm = thread;
  ModLog("controls     : panel_warm=1: the profiles of the controllers are "
         "known (%s and %s), so a thread of the mod draws the drawing for the "
         "page Controls, frame %llu", map::kKindLog[g_warm_state.kind[0]],
         map::kKindLog[g_warm_state.kind[1]], ControllersFrameCount());
  SetThreadPriority(thread, THREAD_PRIORITY_BELOW_NORMAL);
  ResumeThread(thread);
}

void ControlsPageReport() {
  ControlsPageModelsReport();
  if (g_steps + g_defaults + g_draws == 0) return;
  ModLog("controls     : %llu changes of a button and %llu restores of the "
         "defaults this window. %llu drawings of the controllers, %.2f ms "
         "each on average and %.2f ms at the most", g_steps, g_defaults,
         g_draws, g_draws > 0 ? g_draw_ms / static_cast<double>(g_draws) : 0.0,
         g_draw_most_ms);
  g_steps = g_defaults = g_draws = 0;
  g_draw_ms = g_draw_most_ms = 0.0;
}

// --- the page with the models ----------------------------------------------------
//
// THE PAGE (controls_page_models=1) is one picture of the window in place of
// rows (WindowPage, debug_window.h). Each controller has a half of the page
// (controls_layout_math.h): its model at the outer side, and a column of
// boxes at the inner side, one box for each control. A box has the name of
// the control and its job. The box of a button that can take a different
// job is a key with a mark that points down: a press opens a list of the
// jobs of that controller under it. A box of a control that keeps its job
// has no key. Each box has a leader of ink to its control on the model, and
// the leaders never cross (LeaderOrder). The box under the ray, its leader
// and its control on the model are gold.
//
// THE MODELS are the models of the hints (hint_model.cpp, HintModelPanelMesh)
// drawn on the CPU (panel_raster.h): a faint wash of ink for the body and a
// darker wash for the controls, with an outline of ink, in the look of the
// parchment. The picture of each side is drawn one time for its model. With
// panel_warm=1 a thread of the mod draws it when both models are loaded, so
// the first show of the page only copies it. A controller with no model has
// the drawing of controller_art_math.h in its place.

namespace {

namespace lay = controls_layout;
namespace pr = panel_raster;

// The controls of a side in the order of their boxes before the leaders
// choose their places.
const int kBoxOrder[] = {art::kTrigger,    art::kGrip,  art::kStick,
                         art::kStickClick, art::kFirst, art::kSecond,
                         art::kMenu};
const int kBoxOrderCount = sizeof(kBoxOrder) / sizeof(kBoxOrder[0]);

// A copy of the mesh of a model, so the thread of the warm never reads the
// arrays of hint_model.cpp.
struct MeshCopy {
  bool have;
  char id[64];
  int vertices, triangles;
  float* position;
  float* normal;
  uint32_t* index;
  signed char* control;
  float low[3], high[3];
};

// THE PICTURE OF A SIDE in the box of its model on the page: BGRA, the
// color multiplied by the alpha, and the control + 1 of each pixel (0 for
// none) with its share, for the light of one control.
struct SidePicture {
  bool ready;
  bool model;  // from a model, else from the drawing
  int kind;
  char id[64];
  int box[4];  // x0, y0, x1, y1 on the page
  uint8_t* bgra;
  uint8_t* part;
  uint8_t* share;
  bool has[art::kControls];
  float anchor[art::kControls][2];  // on the page
  float middle;                     // the y of the middle of the picture
  double ms;
};

MeshCopy g_mesh[2] = {};
SidePicture g_pic[2] = {};
bool g_mesh_none[2] = {false, false};
HANDLE g_model_warm = nullptr;
bool g_model_warm_tried = false;
uint64_t g_page_draws = 0;
double g_page_ms = 0.0, g_page_most_ms = 0.0;
char g_said_parts[160] = "";  // the pictures of the last log of the parts

void FreeMesh(MeshCopy* m) {
  delete[] m->position;
  delete[] m->normal;
  delete[] m->index;
  delete[] m->control;
  memset(m, 0, sizeof(*m));
}

// On the frame thread: a copy of the mesh of side `side`, when it is loaded.
bool CopyMesh(int side) {
  if (g_mesh[side].have) return true;
  HintModelMesh mesh;
  bool none = false;
  if (!HintModelPanelMesh(side, &mesh, &none)) {
    g_mesh_none[side] = none;
    return false;
  }
  MeshCopy& m = g_mesh[side];
  m.position = new (std::nothrow) float[mesh.vertices * 3];
  m.normal = new (std::nothrow) float[mesh.vertices * 3];
  m.index = new (std::nothrow) uint32_t[mesh.triangles * 3];
  m.control = new (std::nothrow) signed char[mesh.triangles];
  if (m.position == nullptr || m.normal == nullptr || m.index == nullptr ||
      m.control == nullptr) {
    FreeMesh(&m);
    g_mesh_none[side] = true;
    return false;
  }
  memcpy(m.position, mesh.position, sizeof(float) * 3 * mesh.vertices);
  memcpy(m.normal, mesh.normal, sizeof(float) * 3 * mesh.vertices);
  memcpy(m.index, mesh.index, sizeof(uint32_t) * 3 * mesh.triangles);
  memcpy(m.control, mesh.control, static_cast<size_t>(mesh.triangles));
  strncpy_s(m.id, mesh.id, _TRUNCATE);
  m.vertices = mesh.vertices;
  m.triangles = mesh.triangles;
  for (int k = 0; k < 3; ++k) {
    m.low[k] = mesh.low[k];
    m.high[k] = mesh.high[k];
  }
  m.have = true;
  return true;
}

void FreePicture(SidePicture* p) {
  delete[] p->bgra;
  delete[] p->part;
  delete[] p->share;
  memset(p, 0, sizeof(*p));
}

bool AllocPicture(SidePicture* p, int w, int h) {
  const size_t n = static_cast<size_t>(w) * h;
  p->bgra = new (std::nothrow) uint8_t[n * 4];
  p->part = new (std::nothrow) uint8_t[n];
  p->share = new (std::nothrow) uint8_t[n];
  if (p->bgra == nullptr || p->part == nullptr || p->share == nullptr) {
    FreePicture(p);
    return false;
  }
  memset(p->bgra, 0, n * 4);
  memset(p->part, 0, n);
  memset(p->share, 0, n);
  return true;
}

// The anchor of each control: the middle of its pixels on the picture. The
// stick click has the pixels of the stick, so its anchor is near the top of
// the stick, and the anchor of the stick is a little lower.
void Anchors(SidePicture* p) {
  const int w = p->box[2] - p->box[0], h = p->box[3] - p->box[1];
  double sx[art::kControls] = {}, sy[art::kControls] = {},
         sw[art::kControls] = {};
  int top_y[art::kControls], top_x[art::kControls];
  for (int c = 0; c < art::kControls; ++c) top_y[c] = top_x[c] = -1;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t at = static_cast<size_t>(y) * w + x;
      const int c = p->part[at] - 1;
      if (c < 0 || c >= art::kControls || p->share[at] < 128) continue;
      sx[c] += x + 0.5;
      sy[c] += y + 0.5;
      sw[c] += 1.0;
      if (top_y[c] < 0) {
        top_y[c] = y;
        top_x[c] = x;
      }
    }
  }
  for (int c = 0; c < art::kControls; ++c) {
    p->has[c] = sw[c] > 0.0;
    if (p->has[c]) {
      p->anchor[c][0] = static_cast<float>(p->box[0] + sx[c] / sw[c]);
      p->anchor[c][1] = static_cast<float>(p->box[1] + sy[c] / sw[c]);
    }
  }
  if (p->has[art::kStick]) {
    p->has[art::kStickClick] = true;
    const float mx = p->anchor[art::kStick][0], my = p->anchor[art::kStick][1];
    const float tx = p->box[0] + top_x[art::kStick] + 0.5f;
    const float ty = p->box[1] + top_y[art::kStick] + 0.5f;
    p->anchor[art::kStickClick][0] = 0.5f * (mx + tx);
    p->anchor[art::kStickClick][1] = 0.5f * (my + ty);
    p->anchor[art::kStick][1] = my + 0.35f * (my - ty);
  }
}

// The colors of the drawing of a model, in the ink of the cards.
const float kBodyWash[3] = {0.62f, 0.49f, 0.34f};
const float kControlWash[3] = {0.34f, 0.22f, 0.12f};
const float kBodyAlpha = 0.42f;
const float kControlAlpha = 0.80f;
const float kOutlineInk[4] = {0.19f * 0.92f, 0.11f * 0.92f, 0.05f * 0.92f,
                              0.92f};

// THE PICTURE OF A MODEL. The view of controls_layout_math.h, fit into the
// box of the model on the page, with 3 by 3 samples a pixel.
bool DrawModel(int side, const MeshCopy& m, SidePicture* out) {
  const double start = NowMs();
  const bool left = side == 0;
  float dir[3];
  lay::EyeDirection(left, dir);
  float center[3], eye[3];
  for (int k = 0; k < 3; ++k) {
    center[k] = 0.5f * (m.low[k] + m.high[k]);
    eye[k] = center[k] + dir[k] * 0.6f;
  }
  const float up[3] = {0.0f, 0.0f, -1.0f};
  pr::View v;
  if (!pr::LookAt(eye, center, up, 1.0f, 0.0f, 0, 0, &v)) return false;
  float ext[4] = {1e9f, 1e9f, -1e9f, -1e9f};
  for (int i = 0; i < m.vertices; ++i) {
    float q[3];
    if (!pr::Project(v, &m.position[i * 3], q)) continue;
    if (q[0] < ext[0]) ext[0] = q[0];
    if (q[1] < ext[1]) ext[1] = q[1];
    if (q[0] > ext[2]) ext[2] = q[0];
    if (q[1] > ext[3]) ext[3] = q[1];
  }
  if (!(ext[2] > ext[0]) || !(ext[3] > ext[1])) return false;
  int column[4];
  lay::ModelColumn(left, &column[0], &column[2]);
  column[1] = lay::kModelTop;
  column[3] = lay::kModelBottom;
  float fit[3];
  lay::Fit(ext, column, 6.0f, fit);
  int box[4] = {static_cast<int>(floorf(fit[1] + fit[0] * ext[0])) - 4,
                static_cast<int>(floorf(fit[2] + fit[0] * ext[1])) - 4,
                static_cast<int>(ceilf(fit[1] + fit[0] * ext[2])) + 4,
                static_cast<int>(ceilf(fit[2] + fit[0] * ext[3])) + 4};
  for (int k = 0; k < 2; ++k) {
    if (box[k] < column[k]) box[k] = column[k];
    if (box[k + 2] > column[k + 2]) box[k + 2] = column[k + 2];
  }
  const int w = box[2] - box[0], h = box[3] - box[1];
  const int ss = 3;
  float* color = new (std::nothrow) float[pr::ColorFloats(w, h, ss)];
  float* depth = new (std::nothrow) float[pr::Samples(w, h, ss)];
  uint8_t* id = new (std::nothrow) uint8_t[pr::Samples(w, h, ss)];
  uint8_t* scratch = new (std::nothrow) uint8_t[pr::Samples(w, h, ss)];
  SidePicture p;
  memset(&p, 0, sizeof(p));
  const bool ok = color != nullptr && depth != nullptr && id != nullptr &&
                  scratch != nullptr && AllocPicture(&p, w, h);
  if (ok) {
    const float clear[4] = {0, 0, 0, 0};
    pr::Target t;
    pr::Start(&t, w, h, ss, color, depth, id, clear);
    v.focal = fit[0] * ss;
    v.cx = (fit[1] - box[0]) * ss;
    v.cy = (fit[2] - box[1]) * ss;
    // The light comes from above the eye.
    float light[3] = {dir[0] * 0.6f, dir[1] * 0.6f + 0.8f, dir[2] * 0.6f};
    pr::Normalize(light);
    for (int tri = 0; tri < m.triangles; ++tri) {
      const int c = m.control[tri];
      const float* wash = c >= 0 ? kControlWash : kBodyWash;
      const float alpha = c >= 0 ? kControlAlpha : kBodyAlpha;
      pr::Corner corner[3];
      bool in = true;
      for (int k = 0; k < 3 && in; ++k) {
        const uint32_t vi = m.index[tri * 3 + k];
        float q[3];
        if (vi >= static_cast<uint32_t>(m.vertices) ||
            !pr::Project(v, &m.position[vi * 3], q)) {
          in = false;
          break;
        }
        corner[k].x = q[0];
        corner[k].y = q[1];
        corner[k].depth = q[2];
        const float lit = pr::Light(&m.normal[vi * 3], light, 0.3f);
        const float level = 0.62f + 0.38f * lit;
        for (int a = 0; a < 3; ++a) {
          corner[k].color[a] = wash[a] * level * alpha;
        }
        corner[k].color[3] = alpha;
        corner[k].u = corner[k].v = 0.0f;
      }
      if (!in) continue;
      pr::Triangle(&t, corner,
                   pr::Opaque(static_cast<uint8_t>(c >= 0 ? 10 + c : 1)));
    }
    pr::Outline(&t, kOutlineInk, ss, 0.004f, scratch);
    pr::Resolve(t, p.bgra, w);
    pr::ResolveParts(t, p.part, p.share);
    const size_t n = static_cast<size_t>(w) * h;
    for (size_t i = 0; i < n; ++i) {
      p.part[i] = p.part[i] >= 10 ? static_cast<uint8_t>(p.part[i] - 9) : 0;
    }
  }
  delete[] color;
  delete[] depth;
  delete[] id;
  delete[] scratch;
  if (!ok) return false;
  memcpy(p.box, box, sizeof(box));
  p.model = true;
  strncpy_s(p.id, m.id, _TRUNCATE);
  p.middle = (box[1] + box[3]) * 0.5f;
  Anchors(&p);
  p.ms = NowMs() - start;
  p.ready = true;
  FreePicture(out);
  *out = p;
  return true;
}

// THE DRAWING of a controller with no model: the shapes of
// controller_art_math.h in the box of the model, the right controller as
// drawn and the left one as its mirror.
bool DrawArt(int side, int kind, SidePicture* out) {
  const double start = NowMs();
  const bool left = side == 0;
  const art::Art a = art::ArtOf(kind);
  int column[4];
  lay::ModelColumn(left, &column[0], &column[2]);
  const float s0 = (column[2] - column[0] - 16) / art::kArtWidth;
  const float s1 = (lay::kModelBottom - lay::kModelTop - 16) / art::kArtHeight;
  const float scale = s0 < s1 ? s0 : s1;
  const int w = static_cast<int>(art::kArtWidth * scale) + 12;
  const int h = static_cast<int>(art::kArtHeight * scale) + 12;
  const int x0 = (column[0] + column[2] - w) / 2;
  const int y0 = (lay::kModelTop + lay::kModelBottom - h) / 2;
  const int box[4] = {x0, y0, x0 + w, y0 + h};
  SidePicture p;
  memset(&p, 0, sizeof(p));
  if (!AllocPicture(&p, w, h)) return false;
  const Paint ink = kInk[1];
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float mx = (x + 0.5f - 6.0f) / scale;
      const float ux = left ? art::kArtWidth - mx : mx;
      const float uy = (y + 0.5f - 6.0f) / scale;
      float body = 1e9f;
      for (int b = 0; b < a.bodies; ++b) {
        const float d = art::Distance(a.body[b], ux, uy);
        if (d < body) body = d;
      }
      body *= scale;
      uint8_t* px = p.bgra + (static_cast<size_t>(y) * w + x) * 4;
      Over(px, ink, 0.10f * Cover(body));
      Over(px, ink, Cover(fabsf(body) - 1.5f));
      for (int c = 0; c < art::kControls; ++c) {
        if (c == art::kStickClick || c == art::kPad ||
            !art::Shows(kind, left, c)) {
          continue;
        }
        const float d = art::Distance(a.control[c], ux, uy) * scale;
        if (d > 3.0f) continue;
        Over(px, ink, 0.55f * Cover(d));
        Over(px, ink, Cover(fabsf(d) - 1.1f));
        if (d < 0.0f) {
          p.part[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>(c + 1);
          p.share[static_cast<size_t>(y) * w + x] = 255;
        }
      }
    }
  }
  memcpy(p.box, box, sizeof(box));
  p.model = false;
  p.kind = kind;
  p.middle = (box[1] + box[3]) * 0.5f;
  Anchors(&p);
  p.ms = NowMs() - start;
  p.ready = true;
  FreePicture(out);
  *out = p;
  return true;
}

DWORD WINAPI WarmModels(void*) {
  for (int side = 0; side < 2; ++side) {
    if (g_mesh[side].have) DrawModel(side, g_mesh[side], &g_pic[side]);
  }
  return 0;
}

void WaitModels() {
  if (g_model_warm == nullptr) return;
  if (WaitForSingleObject(g_model_warm, 0) != WAIT_OBJECT_0) {
    const double start = NowMs();
    WaitForSingleObject(g_model_warm, INFINITE);
    ModLog("controls     : the page waited %.1f ms on the frame thread for "
           "the warm of the models", NowMs() - start);
  }
  CloseHandle(g_model_warm);
  g_model_warm = nullptr;
}

// The picture of each side for the kinds of the state: the model when the
// side has one, else the drawing.
void EnsurePictures(const State& s) {
  WaitModels();
  for (int side = 0; side < 2; ++side) {
    SidePicture& p = g_pic[side];
    if (CopyMesh(side)) {
      if (p.ready && p.model && strcmp(p.id, g_mesh[side].id) == 0) continue;
      if (DrawModel(side, g_mesh[side], &p)) {
        ModLog("controls     : the picture of the model %s of the %s "
               "controller took %.1f ms on the frame thread", p.id,
               side == 0 ? "left" : "right", p.ms);
        continue;
      }
    }
    if (p.ready && !p.model && p.kind == s.kind[side]) continue;
    DrawArt(side, s.kind[side], &p);
  }
}

// --- the boxes -------------------------------------------------------------------

struct PageBox {
  int control;
  bool can;  // a button with a list of jobs
  int rect[4];
};
struct PageLayout {
  int count[2];
  PageBox box[2][lay::kMostBoxes];
};

PageLayout Layout(const State& s) {
  PageLayout out;
  memset(&out, 0, sizeof(out));
  for (int side = 0; side < 2; ++side) {
    const bool left = side == 0;
    const int kind = s.kind[side];
    int controls[lay::kMostBoxes];
    int n = 0;
    for (int k = 0; k < kBoxOrderCount && n < lay::kMostBoxes; ++k) {
      const int c = kBoxOrder[k];
      bool shows = false;
      if (c < map::kButtons) {
        shows = map::Has(kind, left, c);
      } else {
        const int fixed = c == art::kTrigger ? map::kTrigger
                          : c == art::kGrip  ? map::kGrip
                                             : map::kStick;
        shows = map::HasFixed(kind, fixed);
      }
      if (shows) controls[n++] = c;
    }
    const SidePicture& p = g_pic[side];
    const float middle =
        p.ready ? p.middle : (lay::kModelTop + lay::kModelBottom) * 0.5f;
    float anchors[lay::kMostBoxes][2], starts[lay::kMostBoxes][2];
    for (int i = 0; i < n; ++i) {
      int r[4];
      lay::BoxRect(left, i, n, middle, r);
      lay::LeaderStart(left, r, &starts[i][0], &starts[i][1]);
      const int c = controls[i];
      if (p.ready && p.has[c]) {
        anchors[i][0] = p.anchor[c][0];
        anchors[i][1] = p.anchor[c][1];
      } else {
        // No place on the picture: the box keeps its place in kBoxOrder.
        anchors[i][0] = starts[i][0];
        anchors[i][1] = starts[i][1];
      }
    }
    int order[lay::kMostBoxes];
    lay::LeaderOrder(anchors, starts, n, order);
    out.count[side] = n;
    for (int i = 0; i < n; ++i) {
      PageBox& b = out.box[side][order[i]];
      b.control = controls[i];
      b.can = controls[i] < map::kButtons && CanStep(s, side, controls[i]);
      lay::BoxRect(left, order[i], n, middle, b.rect);
    }
  }
  return out;
}

// The words of a box: the name of its control and its job.
void BoxWords(const State& s, int side, int control, wchar_t* name,
              int name_size, const wchar_t** job) {
  const wchar_t* word = NameOf(s.kind[side], side == 0, control);
  if (word == nullptr) word = L"";
  if (word[0] != 0 && word[1] == 0) {
    swprintf_s(name, name_size, L"%s button", word);
  } else {
    wcsncpy_s(name, name_size, word, _TRUNCATE);
  }
  *job = JobOf(s, side, control);
  if (*job == nullptr) *job = L"";
}

// --- the drawing of the page ----------------------------------------------------

kit::Font g_head_font = {}, g_sub_font = {}, g_name_font = {},
          g_job_font[3] = {};

// The font of a job: the largest of g_job_font in which it fits `width`.
const kit::Font& JobFont(kit::Canvas* c, const wchar_t* job, int width) {
  for (int f = 0; f < 2; ++f) {
    if (g_job_font[f].font == nullptr) continue;
    SelectObject(c->dc, g_job_font[f].font);
    SIZE size = {};
    GetTextExtentPoint32W(c->dc, job, static_cast<int>(wcslen(job)), &size);
    if (size.cx <= width) return g_job_font[f];
  }
  return g_job_font[2];
}

// The box (x0, y0, x1, y1) of `to` goes `share` of the way back to the same
// box of `under`.
void Lighter(const card_scroll::Picture& to, const card_scroll::Picture& under,
             const int box[4], float share) {
  const int keep = static_cast<int>((1.0f - share) * 256.0f);
  for (int y = box[1]; y < box[3]; ++y) {
    uint8_t* d = to.bgra + (static_cast<size_t>(y) * to.pitch + box[0]) * 4;
    const uint8_t* u =
        under.bgra + (static_cast<size_t>(y) * under.pitch + box[0]) * 4;
    for (int x = box[0]; x < box[2]; ++x, d += 4, u += 4) {
      for (int k = 0; k < 4; ++k) {
        d[k] = static_cast<uint8_t>(u[k] + ((d[k] - u[k]) * keep) / 256);
      }
    }
  }
}
HFONT g_plain_job = nullptr, g_plain_name = nullptr;

// Premultiplied color over a BGRA pixel.
inline void OverPremul(uint8_t* d, const float rgb[3], float a) {
  if (a <= 0.0f) return;
  if (a > 1.0f) a = 1.0f;
  const float k = 1.0f - a;
  d[0] = static_cast<uint8_t>(rgb[2] * 255.0f * a + d[0] * k + 0.5f);
  d[1] = static_cast<uint8_t>(rgb[1] * 255.0f * a + d[1] * k + 0.5f);
  d[2] = static_cast<uint8_t>(rgb[0] * 255.0f * a + d[2] * k + 0.5f);
  d[3] = static_cast<uint8_t>(255.0f * a + d[3] * k + 0.5f);
}

const float kInkRgb[3] = {0.20f, 0.12f, 0.055f};
const float kGoldRgb[3] = {0.84f, 0.58f, 0.13f};
const float kLightInk[3] = {0.88f, 0.88f, 0.88f};

// A line from a to b, or a dot with a == b, of `rgb` on a picture.
void InkCapsule(const card_scroll::Picture& to, float ax, float ay, float bx,
                float by, float radius, const float rgb[3], float alpha) {
  const art::Shape line = {art::kCapsule, ax, ay, bx, by, radius};
  float b[4];
  art::Bounds(line, b);
  int x0 = static_cast<int>(b[0]) - 2, y0 = static_cast<int>(b[1]) - 2;
  int x1 = static_cast<int>(b[2]) + 3, y1 = static_cast<int>(b[3]) + 3;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > to.width) x1 = to.width;
  if (y1 > to.height) y1 = to.height;
  for (int y = y0; y < y1; ++y) {
    uint8_t* row = to.bgra + static_cast<size_t>(y) * to.pitch * 4;
    for (int x = x0; x < x1; ++x) {
      OverPremul(row + x * 4, rgb,
                 alpha * Cover(art::Distance(line, x + 0.5f, y + 0.5f)));
    }
  }
}

// The picture of a side over a picture at the origin of the page, with the
// pixels of control `lit` in gold.
void LayPicture(const card_scroll::Picture& to, int ox, int oy,
                const SidePicture& p, int lit) {
  const int w = p.box[2] - p.box[0], h = p.box[3] - p.box[1];
  for (int y = 0; y < h; ++y) {
    const int ty = oy + p.box[1] + y;
    if (ty < 0 || ty >= to.height) continue;
    uint8_t* d =
        to.bgra + (static_cast<size_t>(ty) * to.pitch + ox + p.box[0]) * 4;
    const uint8_t* sp = p.bgra + static_cast<size_t>(y) * w * 4;
    for (int x = 0; x < w; ++x, d += 4, sp += 4) {
      const int a = sp[3];
      if (a != 0) {
        for (int k = 0; k < 3; ++k) {
          d[k] = static_cast<uint8_t>(sp[k] + d[k] * (255 - a) / 255);
        }
      }
      const size_t at = static_cast<size_t>(y) * w + x;
      if (lit >= 0 && p.part[at] == lit + 1) {
        OverPremul(d, kGoldRgb, 0.78f * p.share[at] / 255.0f);
      }
    }
  }
}

// The mark of a list on a box: a small triangle that points down, with 4 by
// 4 samples a pixel.
void DownMark(const card_scroll::Picture& to, float cx, float cy,
              const float rgb[3]) {
  const float hw = 6.5f, hh = 4.5f;
  for (int y = static_cast<int>(cy - hh) - 1;
       y <= static_cast<int>(cy + hh) + 1; ++y) {
    for (int x = static_cast<int>(cx - hw) - 1;
         x <= static_cast<int>(cx + hw) + 1; ++x) {
      if (x < 0 || y < 0 || x >= to.width || y >= to.height) continue;
      int in = 0;
      for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
          const float px = x + (sx + 0.5f) / 4.0f, py = y + (sy + 0.5f) / 4.0f;
          const float t = (py - (cy - hh)) / (2.0f * hh);
          if (t < 0.0f || t > 1.0f) continue;
          if (fabsf(px - cx) <= hw * (1.0f - t)) ++in;
        }
      }
      OverPremul(to.bgra + (static_cast<size_t>(y) * to.pitch + x) * 4, rgb,
                 in / 16.0f);
    }
  }
}

int PageLookOf(int part, int hover, int open, int flash) {
  if (part == flash) return kit::kFlash;
  if (part == open) return kit::kOn;
  if (part == hover) return kit::kHover;
  return kit::kPlain;
}

void DrawPage(kit::Canvas* c, const int box[4], int style, int hover,
              int open, int flash) {
  const double start = NowMs();
  Read();
  const State s = Now();
  EnsurePictures(s);
  const PageLayout l = Layout(s);
  const bool parchment = style != 0;
  const int ox = box[0], oy = box[1];
  if (parchment && g_job_font[0].font == nullptr) {
    g_head_font = kit::WordsFont(c, 23);
    g_sub_font = kit::SealFont(c, 14);
    g_name_font = kit::SealFont(c, 13);
    g_job_font[0] = kit::WordsFont(c, 18);
    g_job_font[1] = kit::WordsFont(c, 16);
    g_job_font[2] = kit::WordsFont(c, 14);
  }
  if (!parchment && g_plain_job == nullptr) {
    const auto font = [](int em, int weight) {
      return CreateFontW(-em, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                         ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
                         L"Segoe UI");
    };
    g_plain_job = font(21, FW_NORMAL);
    g_plain_name = font(17, FW_SEMIBOLD);
  }
  const card_scroll::Picture card = kit::CardPicture(c);
  // In the dark look the GDI picture is the picture of the window.
  const card_scroll::Picture plain = {reinterpret_cast<uint8_t*>(c->bits),
                                      c->width, c->width, c->height};
  const card_scroll::Picture& dst = parchment ? card : plain;
  const float* ink = parchment ? kInkRgb : kLightInk;
  if (!parchment) GdiFlush();
  // The control that is lit: the box of the list, else the box under the
  // ray.
  int lit_side = -1, lit_box = -1;
  {
    bool left = false;
    int b = 0;
    const int part = open >= 0 ? open : hover;
    if (lay::SideOfPart(part, &left, &b) && b < l.count[left ? 0 : 1]) {
      lit_side = left ? 0 : 1;
      lit_box = b;
    }
  }
  for (int side = 0; side < 2; ++side) {
    const bool left = side == 0;
    // The header of the column of boxes.
    int cx0 = 0, cx1 = 0;
    lay::BoxColumn(left, &cx0, &cx1);
    const int head[4] = {ox + cx0, oy + lay::kHeadTop, ox + cx1,
                         oy + lay::kHeadTop + 34};
    const int sub[4] = {ox + cx0, oy + lay::kHeadTop + 36, ox + cx1,
                        oy + lay::kHeadBottom};
    const wchar_t* head_word = left ? L"Left" : L"Right";
    const wchar_t* sub_word = side == s.pointing ? L"Hand" : L"Other hand";
    if (parchment) {
      kit::InkWordsIn(c, card, g_head_font, head_word, head, 31 + side,
                      kit::kCenter);
      kit::InkWordsIn(c, card, g_sub_font, sub_word, sub, 33 + side,
                      kit::kCenter);
    } else {
      kit::PlainText(c, g_plain_job, RGB(235, 235, 235), head_word, head);
      kit::PlainText(c, g_plain_name, RGB(170, 170, 170), sub_word, sub);
      GdiFlush();
    }
    const SidePicture& p = g_pic[side];
    const int lit_control = lit_side == side ? l.box[side][lit_box].control
                                             : -1;
    // The stick click lights the stick.
    const int lit_part =
        lit_control == art::kStickClick ? art::kStick : lit_control;
    if (p.ready) LayPicture(dst, ox, oy, p, lit_part);
    // The leaders, then the boxes.
    for (int i = 0; i < l.count[side]; ++i) {
      const PageBox& b = l.box[side][i];
      if (!p.ready || !p.has[b.control]) continue;
      float sx, sy;
      lay::LeaderStart(left, b.rect, &sx, &sy);
      const bool on = side == lit_side && i == lit_box;
      const float ax = ox + p.anchor[b.control][0];
      const float ay = oy + p.anchor[b.control][1];
      if (on) {
        InkCapsule(dst, ox + sx, oy + sy, ax, ay, 1.4f, kGoldRgb, 1.0f);
        InkCapsule(dst, ox + sx, oy + sy, ax, ay, 0.5f, ink, 0.9f);
        InkCapsule(dst, ax, ay, ax, ay, 4.5f, ink, 1.0f);
        InkCapsule(dst, ax, ay, ax, ay, 3.0f, kGoldRgb, 1.0f);
      } else {
        InkCapsule(dst, ox + sx, oy + sy, ax, ay, 0.65f, ink, 0.8f);
        InkCapsule(dst, ax, ay, ax, ay, 2.8f, ink, 1.0f);
      }
    }
    for (int i = 0; i < l.count[side]; ++i) {
      const PageBox& b = l.box[side][i];
      const int part = lay::PartOf(left, i);
      const int r[4] = {ox + b.rect[0], oy + b.rect[1], ox + b.rect[2],
                        oy + b.rect[3]};
      wchar_t name[32];
      const wchar_t* job = nullptr;
      BoxWords(s, side, b.control, name, 32, &job);
      const int look = PageLookOf(part, hover, open, flash);
      const bool on = side == lit_side && i == lit_box;
      // The name over the job, at the left of the box. A box with a list
      // has the mark at its right.
      const int name_box[4] = {r[0] + 14, r[1] + 7, r[2] - 10, r[1] + 27};
      const int job_box[4] = {r[0] + 14, r[1] + 27, r[2] - (b.can ? 30 : 10),
                              r[3] - 6};
      if (parchment) {
        if (b.can) {
          kit::PageToCard(c, r);
          card_scroll::Key(card, r[0], r[1], r[2], r[3],
                           look == kit::kHover   ? card_scroll::kKeyHover
                           : look == kit::kPlain ? card_scroll::kKeyPlain
                                                 : card_scroll::kKeyFlash,
                           part * 3 + 5);
          DownMark(card, r[2] - 18.0f, (r[1] + r[3]) * 0.5f + 1.0f, kInkRgb);
        } else if (on) {
          // A control that keeps its job has no key. Under the ray it has a
          // rule of gold under its words.
          InkCapsule(card, r[0] + 14.0f, r[3] - 4.0f, r[2] - 14.0f,
                     r[3] - 4.0f, 1.2f, kGoldRgb, 1.0f);
        }
        kit::InkWordsIn(c, card, g_name_font, name, name_box, part * 3 + 6,
                        kit::kLeftAlign);
        kit::InkWordsIn(c, card, JobFont(c, job, job_box[2] - job_box[0]),
                        job, job_box, part * 3 + 7, kit::kLeftAlign);
        if (!b.can && !on) {
          // The words of a control that keeps its job are lighter.
          const int words[4] = {r[0] + 10, r[1] + 4, r[2] - 8, r[3] - 7};
          Lighter(card, kit::PagePicture(c), words, 0.42f);
        }
      } else {
        const int rect[4] = {r[0], r[1], r[2] - r[0], r[3] - r[1]};
        if (b.can) kit::PlainButton(c, rect, look, L"", g_plain_job);
        kit::PlainText(c, g_plain_name,
                       b.can ? RGB(170, 170, 170) : RGB(120, 120, 120), name,
                       name_box);
        SelectObject(c->dc, g_plain_job);
        SetTextColor(c->dc, b.can ? RGB(230, 190, 110) : RGB(140, 140, 140));
        SetBkMode(c->dc, TRANSPARENT);
        RECT jr = {job_box[0], job_box[1], job_box[2], job_box[3]};
        DrawTextW(c->dc, job, -1, &jr,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        GdiFlush();
        if (b.can) {
          DownMark(plain, r[2] - 18.0f, (r[1] + r[3]) * 0.5f + 1.0f,
                   kLightInk);
        }
      }
    }
  }
  const double ms = NowMs() - start;
  ++g_page_draws;
  g_page_ms += ms;
  if (ms > g_page_most_ms) g_page_most_ms = ms;
  char sources[160];
  _snprintf_s(sources, _TRUNCATE, "%s and %s",
              g_pic[0].model ? g_pic[0].id : "a drawing",
              g_pic[1].model ? g_pic[1].id : "a drawing");
  if (strcmp(sources, g_said_parts) != 0) {
    strcpy_s(g_said_parts, sources);
    char line[700] = "";
    for (int side = 0; side < 2; ++side) {
      for (int i = 0; i < l.count[side]; ++i) {
        wchar_t name[32];
        const wchar_t* job = nullptr;
        BoxWords(s, side, l.box[side][i].control, name, 32, &job);
        char n[32], j[32], one[96];
        Narrow(name, n, sizeof(n));
        Narrow(job, j, sizeof(j));
        _snprintf_s(one, _TRUNCATE, "%spart %d %s %s: %s%s",
                    line[0] != 0 ? ", " : "", lay::PartOf(side == 0, i),
                    side == 0 ? "left" : "right", n, j,
                    l.box[side][i].can ? " (a list)" : "");
        strncat_s(line, one, _TRUNCATE);
      }
    }
    ModLog("controls     : the page with the models (%s, %s): %s", sources,
           parchment ? "parchment" : "dark", line);
  }
  if (g_page_draws <= 4) {
    ModLog("controls     : a picture of the page took %.2f ms, frame %llu", ms,
           ControllersFrameCount());
  }
}

unsigned PageValue(int hover, int open, int flash) {
  const State s = Now();
  unsigned h = static_cast<unsigned>(DrawingValueOf(s, -1));
  const auto mix = [&](unsigned v) {
    h ^= v;
    h *= 16777619u;
  };
  mix(static_cast<unsigned>(hover + 2));
  mix(static_cast<unsigned>(open + 2));
  mix(static_cast<unsigned>(flash + 2));
  mix(g_pic[0].ready ? 1u : 0u);
  mix(g_pic[1].ready ? 1u : 0u);
  return h;
}

int PageHit(float x, float y) {
  const PageLayout l = Layout(Now());
  for (int side = 0; side < 2; ++side) {
    for (int i = 0; i < l.count[side]; ++i) {
      const int* r = l.box[side][i].rect;
      if (x >= r[0] && x < r[2] && y >= r[1] && y < r[3]) {
        return lay::PartOf(side == 0, i);
      }
    }
  }
  return -1;
}

bool PageBoxOf(int part, int out[4]) {
  bool left = false;
  int b = 0;
  if (!lay::SideOfPart(part, &left, &b)) return false;
  const PageLayout l = Layout(Now());
  if (b >= l.count[left ? 0 : 1]) return false;
  memcpy(out, l.box[left ? 0 : 1][b].rect, sizeof(int) * 4);
  return true;
}

// The slot of each item of the list that shows.
int g_list_slot[kPageListMost];

bool PageListOf(int part, PageList* out) {
  bool left = false;
  int b = 0;
  if (!lay::SideOfPart(part, &left, &b)) return false;
  const int side = left ? 0 : 1;
  const State s = Now();
  const PageLayout l = Layout(s);
  if (b >= l.count[side] || !l.box[side][b].can) return false;
  const int button = l.box[side][b].control;
  int choices[map::kButtons];
  const int n = map::SlotChoices(s.kind[side], left, choices);
  memset(out, 0, sizeof(*out));
  out->now = -1;
  for (int k = 0; k < n && out->count < kPageListMost; ++k) {
    const int action = map::SlotAction(s.jobs, s.kind[side],
                                       side == s.pointing, left, choices[k]);
    // Two slots with no job are one item: the slot of the button when it
    // has no job, else the first of them.
    int twin = -1;
    for (int i = 0; i < out->count; ++i) {
      if (wcscmp(out->word[i], map::kActionWord[action]) == 0) twin = i;
    }
    if (twin >= 0) {
      if (choices[k] == s.map[side][button]) {
        g_list_slot[twin] = choices[k];
        out->now = twin;
      }
      continue;
    }
    wcsncpy_s(out->word[out->count], map::kActionWord[action], _TRUNCATE);
    g_list_slot[out->count] = choices[k];
    if (choices[k] == s.map[side][button]) out->now = out->count;
    ++out->count;
  }
  memcpy(out->box, l.box[side][b].rect, sizeof(int) * 4);
  return out->count > 0;
}

bool PageChoose(int part, int item, wchar_t* status, int size) {
  bool left = false;
  int b = 0;
  status[0] = 0;
  if (!lay::SideOfPart(part, &left, &b) || item < 0 ||
      item >= kPageListMost) {
    return false;
  }
  const int side = left ? 0 : 1;
  State s = Now();
  const PageLayout l = Layout(s);
  if (b >= l.count[side] || !l.box[side][b].can) return false;
  const int which = l.box[side][b].control;
  const int slot = g_list_slot[item];
  if (slot == s.map[side][which]) return false;
  const int before = ActionOf(s, side, which);
  const int other = map::ButtonOfSlot(s.map[side], slot);
  map::GiveSlot(s.map[side], which, slot);
  const int role = side == s.pointing ? 0 : 1;
  const int value = map::MapValue(s.map[side]);
  const bool wrote = SettingsPanelWriteKey(kMapKey[role], value);
  ControllersSetButtonMap(role, s.map[side], kBy);
  g_state_frame = ~0ull;
  ++g_steps;
  wchar_t label[24], moved[24];
  map::ButtonLabel(s.kind[side], left, which, label, 24);
  map::ButtonLabel(s.kind[side], left, other, moved, 24);
  const int after = ActionOf(s, side, which);
  swprintf_s(status, size, L"%s: %s. %s: %s", label, map::kActionWord[after],
             moved, map::kActionWord[before]);
  char l8[32], m8[32], from[24], to[24];
  Narrow(label, l8, sizeof(l8));
  Narrow(moved, m8, sizeof(m8));
  Narrow(map::kActionWord[before], from, sizeof(from));
  Narrow(map::kActionWord[after], to, sizeof(to));
  ModLog("controls     : the list of %s: it goes from %s to %s, and %s takes "
         "%s. %s=%d %s. It acts when each button is up, frame %llu", l8, from,
         to, m8, from, kMapKey[role], value,
         wrote ? "is in bwvr.ini"
               : "is for this game only: Windows refused the write to "
                 "bwvr.ini",
         ControllersFrameCount());
  return true;
}

void PageStatus(int part, wchar_t* out, int size) {
  bool left = false;
  int b = 0;
  out[0] = 0;
  const State s = Now();
  if (!lay::SideOfPart(part, &left, &b)) {
    swprintf_s(out, size, L"Point at a button to change its job.");
    return;
  }
  const int side = left ? 0 : 1;
  const PageLayout l = Layout(s);
  if (b >= l.count[side]) return;
  const PageBox& box = l.box[side][b];
  wchar_t label[24];
  if (box.control < map::kButtons) {
    map::ButtonLabel(s.kind[side], left, box.control, label, 24);
  } else {
    const int fixed = box.control == art::kTrigger ? map::kTrigger
                      : box.control == art::kGrip  ? map::kGrip
                                                   : map::kStick;
    const wchar_t* w = map::FixedLabel(s.kind[side], left, fixed);
    wcsncpy_s(label, w != nullptr ? w : L"", _TRUNCATE);
  }
  wchar_t name[32];
  const wchar_t* job = nullptr;
  BoxWords(s, side, box.control, name, 32, &job);
  swprintf_s(out, size,
             box.can ? L"%s: %s. Pull the trigger to change it."
                     : L"%s: %s. This control always has this job.",
             label, job);
}

bool PageButtonCan() {
  const State s = Now();
  return !(map::IsAsMade(s.map[0]) && map::IsAsMade(s.map[1]));
}

void PageButtonPress(wchar_t* status, int size) {
  int made[map::kButtons];
  map::AsMade(made);
  bool wrote = true;
  for (int role = 0; role < 2; ++role) {
    wrote &= SettingsPanelWriteKey(kMapKey[role], map::kAsMade);
    ControllersSetButtonMap(role, made, kBy);
  }
  g_state_frame = ~0ull;
  ++g_defaults;
  swprintf_s(status, size, L"Each button has its default job again.");
  ModLog("controls     : Reset Defaults: each button has its job as made. %s, "
         "frame %llu",
         wrote ? "button_map_hand=1234 and button_map_other=1234 are in "
                 "bwvr.ini"
               : "Windows refused the write to bwvr.ini, so the change lasts "
                 "for this game only",
         ControllersFrameCount());
}

const WindowPage kPage = {
    DrawPage,   PageValue,  PageHit,           PageBoxOf,     PageListOf,
    PageChoose, PageStatus, L"Reset Defaults", PageButtonCan, PageButtonPress,
};

}  // namespace

const WindowPage* ControlsPage() {
  Read();
  return g_on && g_models_on ? &kPage : nullptr;
}

void ControlsPageModelsWarmFrame() {
  if (g_model_warm_tried) return;
  Read();
  if (!g_on || !g_models_on || !g_warm_on) {
    g_model_warm_tried = true;
    return;
  }
  const char* forced = ControllerHintsForcedProfile();
  if ((forced == nullptr || forced[0] == 0) &&
      (!ControllersProfileKnown(0) || !ControllersProfileKnown(1))) {
    return;
  }
  const bool a = CopyMesh(0) || g_mesh_none[0];
  const bool b = CopyMesh(1) || g_mesh_none[1];
  if (!a || !b) return;
  g_model_warm_tried = true;
  if (!g_mesh[0].have && !g_mesh[1].have) return;
  g_model_warm = CreateThread(nullptr, 0, &WarmModels, nullptr,
                              CREATE_SUSPENDED, nullptr);
  if (g_model_warm == nullptr) return;
  ModLog("controls     : panel_warm=1: the models of the controllers are "
         "loaded (%s and %s), so a thread of the mod draws their pictures "
         "for the page Controls, frame %llu",
         g_mesh[0].have ? g_mesh[0].id : "none",
         g_mesh[1].have ? g_mesh[1].id : "none", ControllersFrameCount());
  SetThreadPriority(g_model_warm, THREAD_PRIORITY_BELOW_NORMAL);
  ResumeThread(g_model_warm);
}

void ControlsPageModelsReport() {
  if (g_page_draws == 0) return;
  ModLog("controls     : %llu pictures of the page with the models, %.2f ms "
         "each on average and %.2f ms at the most. The pictures of the "
         "models took %.1f and %.1f ms", g_page_draws,
         g_page_ms / static_cast<double>(g_page_draws), g_page_most_ms,
         g_pic[0].ms, g_pic[1].ms);
  g_page_draws = 0;
  g_page_ms = g_page_most_ms = 0.0;
}
