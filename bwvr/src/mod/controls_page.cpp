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
//   controls_page=1    0: the settings window has no tab Controls. The keys
//                      of the map still act

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
#include "controls_page.h"
#include "debug_menu.h"
#include "panel_kit.h"

extern void ModLog(const char* format, ...);
uint64_t ControllersFrameCount();
bool SettingsPanelWriteKey(const char* key, int value);
const char* ControllerHintsForcedProfile();

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
int g_hover = -1;
uint64_t g_steps = 0, g_defaults = 0, g_draws = 0;
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
  }
  ModLog("controls     : controls_page=%d: %s", g_on ? 1 : 0,
         g_on ? "the settings window has the tab Controls"
              : "the settings window has no tab Controls");
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

void Draw(int style, int lit) {
  const double start = NowMs();
  uint8_t* to = g_picture[style];
  memset(to, 0, static_cast<size_t>(art::kWidth) * art::kHeight * 4);
  const State s = Now();
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
           "%.2f ms (%s), frame %llu", map::kKindLog[s.kind[0]],
           map::kKindLog[s.kind[1]], ms,
           style != 0 ? "parchment" : "dark", ControllersFrameCount());
  }
}

// The value of the rows of the drawing: what it shows. A change draws it
// again.
int DrawingValue() {
  const State s = Now();
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
  const int lit = LitOf(g_hover);
  return static_cast<int>(((h & 0xFFFFFu) << 6) |
                          static_cast<uint32_t>(lit + 1));
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
    {L"It is the click of the game. controller_swap_buttons=1 swaps it with "
     L"the grip.",
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
    swprintf_s(out, size, L"Keys: button_map_hand=1234 and "
                          L"button_map_other=1234. A change acts at once.");
  } else if (ButtonRow(k, &side, &which)) {
    const State s = Now();
    swprintf_s(out, size,
               L"Key: %hs. A change acts when each button is up.",
               kMapKey[side == s.pointing ? 0 : 1]);
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
  style = style != 0 ? 1 : 0;
  const size_t bytes = static_cast<size_t>(art::kWidth) * art::kHeight * 4;
  if (g_picture[style] == nullptr) {
    g_picture[style] = new (std::nothrow) uint8_t[bytes];
    if (g_picture[style] == nullptr) return nullptr;
    g_drawn_value[style] = -1;
  }
  if (g_drawn_value[style] != value) {
    Draw(style, static_cast<int>(value & 63) - 1);
    g_drawn_value[style] = value;
  }
  return g_picture[style] +
         static_cast<size_t>(k) * art::kBandHeight * art::kWidth * 4;
}

void ControlsSetHoverRow(int k) { g_hover = k; }

void ControlsPageReport() {
  if (g_steps + g_defaults + g_draws == 0) return;
  ModLog("controls     : %llu changes of a button and %llu restores of the "
         "defaults this window. %llu drawings of the controllers, %.2f ms "
         "each on average and %.2f ms at the most", g_steps, g_defaults,
         g_draws, g_draws > 0 ? g_draw_ms / static_cast<double>(g_draws) : 0.0,
         g_draw_most_ms);
  g_steps = g_defaults = g_draws = 0;
  g_draw_ms = g_draw_most_ms = 0.0;
}
