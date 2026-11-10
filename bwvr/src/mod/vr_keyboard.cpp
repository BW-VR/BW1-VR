// The keyboard in the headset (docs/ui-layer.md, "The keyboard in the
// headset").
//
// Some boxes of the game ask for text: the name of the god in the first
// start, the name of a saved game, a name in a game for more players. Each
// one is a SetupEdit control of a SetupBox. While a SetupEdit has the focus
// of the box that is up, and the menus are on the panel in the room
// (menu_room.cpp), a keyboard card shows below the panel, nearer to the
// player and tilted to face the head. The ray of the pointing hand
// highlights the key under it, with a light pulse at each new key, and the
// trigger presses that key, with a stronger pulse.
//
// A key goes to the game as a key of a computer does: WM_KEYDOWN, WM_CHAR
// when Windows makes one for that key, and WM_KEYUP 60 ms later, to the
// window procedure of the game (GameWindowProc 0x007DBF30). The game puts
// WM_CHAR in its queue of characters (0x00E855A0, from 0x007DC199), and the
// box gives each one to the control that has the focus (SetupBox at
// 0x00408EC8, then SetupEdit::Char 0x0040B5F0). WM_KEYDOWN goes to
// LHKeyboard::ProcessKeyboard (0x007DCAF0), and the loops of the menus give
// the key to the box (fn_00408F80, then SetupEdit::KeyDown 0x0040BAF0). So
// Backspace and Enter act as the game makes them act.
//
// THE LOOK follows hint_card_style, the key of the hint cards. With 1 the
// card is a page of parchment (card_scroll.h) with a torn edge. Each key is
// a frame of ink round a face that is pressed into the paper, with its letter
// in dark ink. The key under the ray has a gold face and a gold rim, the key
// that the trigger pressed is bright yellow for 120 ms, and Shift is a red wax
// seal while it is on. With 0 the card is dark and opaque, with grey keys.
//
// THE BAKE of the parchment. The page is drawn one time. Each key in each of
// its looks is drawn one time, when it first shows, and a copy of its box is
// kept. A new picture of the card copies the box of each key whose look
// changed, so most new pictures only copy.
//
//   vr_keyboard=1            0: no keyboard in the headset
//   hint_card_style=1        1: the keyboard is a page of parchment. 0: the
//                            dark keyboard. The switch "Card look" of the
//                            debug page changes it too
//   vr_keyboard_test_save=0  N: saves the first N new pictures of the card
//                            as bwvr-keyboard-<frame>.bmp (a test key)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include <new>

#include "../vr/xr_screen.h"
#include "card_scroll.h"
#include "text_fit.h"
#include "menu_room.h"
#include "vr_keyboard_math.h"

extern void ModLog(const char* format, ...);

// controllers.cpp
void ControllersGesturePulse(float amplitude, float seconds);
// hint_card.cpp: hint_card_style now (the debug page can change it), or
// kDebugCannot (below 0) while the hint cards cannot show.
int HintCardStyleValue();
// eye_capture.cpp
void EyeCaptureSaveUi(const char* name, uint64_t frame, int style,
                      const void* pixels, int pitch, uint32_t width,
                      uint32_t height);

namespace {

bool g_on = true;

// The active box (SetupBox::GetCurrentActiveBox reads it), the control that
// has the focus of a box (SetupBox::SetFocusControl writes box + 0x70), and
// the table of the virtual functions of SetupEdit (??_7SetupEdit@@6B@).
const uintptr_t kActiveBox = 0x00C4CD20;
const int kFocusOffset = 0x70;
const uintptr_t kSetupEditTable = 0x008AB3A0;

// The window of the game, as controllers.cpp finds it.
const uintptr_t kGameWindow = 0x00E85078;
const uintptr_t kGameWindowMade = 0x00E8C0F4;

// The card.
const int kWidth = 1024;
const int kHeight = 384;
const int kMargin = 14;
const int kGap = 8;
const DWORD kKeyHoldMs = 60;
const ULONGLONG kFlashMs = 120;

// The trigger is down above 0.6 and up below 0.4.
const float kTriggerDown = 0.6f;
const float kTriggerUp = 0.4f;

HDC g_dc = nullptr;
HBITMAP g_dib = nullptr;
HFONT g_font = nullptr;
HFONT g_word_font = nullptr;
uint32_t* g_bits = nullptr;
bool g_raster_failed = false;

bool g_shown = false;
bool g_trigger_down = false;
vr_keyboard_math::Press g_press;
vr_keyboard_math::ShiftState g_shift;
int g_hover = -1;
int g_flash = -1;
ULONGLONG g_flash_until = 0;

// What the card shows now, so it draws again only when that changes.
struct Look {
  int hover;
  int flash;
  bool shift;
  int style;  // hint_card_style
};
Look g_drawn = {-2, -2, false, 0};  // the dark card in g_bits

bool SameLook(const Look& a, const Look& b) {
  return a.hover == b.hover && a.flash == b.flash && a.shift == b.shift &&
         a.style == b.style;
}

// The keys of bwvr.ini, read one time.
bool g_read = false;
int g_style_ini = 1;  // hint_card_style
int g_save = 0;       // vr_keyboard_test_save
int g_saved = 0;

// The time of each new picture of the card.
uint64_t g_draws = 0;
double g_draw_ms = 0.0;
double g_draw_most_ms = 0.0;
int g_draw_logs = 0;
const int kMaxDrawLogs = 40;

// A WM_KEYUP that waits.
HWND g_up_window = nullptr;
uint32_t g_up_vk = 0;
LPARAM g_up_param = 0;
ULONGLONG g_up_at = 0;
bool g_up_waits = false;

uint64_t g_opens = 0;
uint64_t g_keys = 0;
int g_key_logs = 0;
const int kMaxKeyLogs = 60;

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

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  g_style_ini = IniKey("hint_card_style", 1) != 0 ? 1 : 0;
  g_save = IniKey("vr_keyboard_test_save", 0);
  ModLog("keyboard     : hint_card_style=%d, %s%s", g_style_ini,
         g_style_ini != 0 ? "the keyboard is a page of parchment with keys "
                            "of ink"
                          : "the keyboard is dark with grey keys",
         g_save > 0 ? ". vr_keyboard_test_save saves the first new pictures "
                      "of the card (a test)"
                    : "");
}

// hint_card_style now: the switch of the debug page, else bwvr.ini.
int CardStyle() {
  ReadSettings();
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

HWND GameWindow() {
  HWND window = *reinterpret_cast<const HWND*>(kGameWindow);
  if (window == nullptr) window = *reinterpret_cast<const HWND*>(kGameWindowMade);
  return window != nullptr && IsWindow(window) ? window : nullptr;
}

// True while the control that has the focus of the active box is a
// SetupEdit: a text box.
bool EditHasFocus() {
  const uintptr_t box = *reinterpret_cast<const uintptr_t*>(kActiveBox);
  if (box == 0) return false;
  const uint8_t* at = reinterpret_cast<const uint8_t*>(box) + kFocusOffset;
  if (IsBadReadPtr(at, sizeof(uintptr_t))) return false;
  const uintptr_t focus = *reinterpret_cast<const uintptr_t*>(at);
  if (focus == 0 || IsBadReadPtr(reinterpret_cast<const void*>(focus),
                                 sizeof(uintptr_t))) {
    return false;
  }
  return *reinterpret_cast<const uintptr_t*>(focus) == kSetupEditTable;
}

void SendUpNow() {
  if (!g_up_waits) return;
  g_up_waits = false;
  if (g_up_window != nullptr) {
    SendMessageA(g_up_window, WM_KEYUP, g_up_vk, g_up_param);
  }
}

// One key, as a keyboard of a computer gives it to the game.
void SendKey(uint32_t vk, uint32_t ch) {
  SendUpNow();
  HWND window = GameWindow();
  if (window == nullptr) return;
  const UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
  const LPARAM down = static_cast<LPARAM>(1 | (scan << 16));
  SendMessageA(window, WM_KEYDOWN, vk, down);
  if (ch != 0) SendMessageA(window, WM_CHAR, ch, down);
  g_up_window = window;
  g_up_vk = vk;
  g_up_param = static_cast<LPARAM>(1u | (scan << 16) | (1u << 30) | (1u << 31));
  g_up_at = GetTickCount64() + kKeyHoldMs;
  g_up_waits = true;
}

const char* KeyName(int i, char* buffer, size_t size) {
  const vr_keyboard_math::Key& k = vr_keyboard_math::kKeys[i];
  switch (k.kind) {
    case vr_keyboard_math::kSpace: return "Space";
    case vr_keyboard_math::kBack: return "Backspace";
    case vr_keyboard_math::kEnter: return "Enter";
    case vr_keyboard_math::kShift: return "Shift";
    default:
      buffer[0] = k.label;
      buffer[1] = 0;
      (void)size;
      return buffer;
  }
}

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
  g_font = font(40, FW_SEMIBOLD);
  g_word_font = font(26, FW_SEMIBOLD);
  if (g_dc == nullptr || g_dib == nullptr || bits == nullptr ||
      g_font == nullptr || g_word_font == nullptr) {
    g_raster_failed = true;
    ModLog("keyboard     : GDI gave no picture for the keyboard, so the "
           "keyboard does not show");
    return false;
  }
  g_bits = static_cast<uint32_t*>(bits);
  SelectObject(g_dc, g_dib);
  SetBkMode(g_dc, TRANSPARENT);
  return true;
}

void Fill(const int r[4], COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  RECT rect = {r[0], r[1], r[0] + r[2], r[1] + r[3]};
  FillRect(g_dc, &rect, brush);
  DeleteObject(brush);
}

// The word or the letter on key i.
void Label(int i, bool shift, wchar_t label[8]) {
  const vr_keyboard_math::Key& k = vr_keyboard_math::kKeys[i];
  memset(label, 0, 8 * sizeof(wchar_t));
  switch (k.kind) {
    case vr_keyboard_math::kSpace: wcscpy_s(label, 8, L"Space"); break;
    case vr_keyboard_math::kBack: wcscpy_s(label, 8, L"Back"); break;
    case vr_keyboard_math::kEnter: wcscpy_s(label, 8, L"Enter"); break;
    case vr_keyboard_math::kShift: wcscpy_s(label, 8, L"Shift"); break;
    default: {
      char c = k.label;
      if (c >= 'A' && c <= 'Z' && !shift) c = static_cast<char>(c - 'A' + 'a');
      label[0] = static_cast<wchar_t>(c);
      break;
    }
  }
}

void Raster(const Look& look) {
  const int all[4] = {0, 0, kWidth, kHeight};
  Fill(all, RGB(22, 24, 28));
  for (int i = 0; i < vr_keyboard_math::kKeyCount; ++i) {
    const vr_keyboard_math::Key& k = vr_keyboard_math::kKeys[i];
    int r[4];
    vr_keyboard_math::KeyRect(i, kWidth, kHeight, kMargin, kGap, r);
    COLORREF face = RGB(62, 66, 76);
    COLORREF text = RGB(235, 235, 235);
    if (k.kind == vr_keyboard_math::kShift && look.shift) {
      face = RGB(90, 120, 170);
    }
    if (i == look.hover) face = RGB(205, 172, 96);
    if (i == look.flash) face = RGB(250, 225, 150);
    if (i == look.hover || i == look.flash) text = RGB(20, 20, 20);
    Fill(r, face);
    wchar_t label[8];
    Label(i, look.shift, label);
    SelectObject(g_dc, k.kind == vr_keyboard_math::kChar ? g_font : g_word_font);
    SetTextColor(g_dc, text);
    RECT rect = {r[0], r[1], r[0] + r[2], r[1] + r[3]};
    text_fit::Draw(g_dc, label, rect, DT_CENTER | DT_VCENTER, 1, "keyboard");
  }
  GdiFlush();
  // Opaque: the alpha of each pixel is 255, so the color needs no change for
  // the premultiplied alpha of the card.
  for (int i = 0; i < kWidth * kHeight; ++i) g_bits[i] |= 0xFF000000u;
  g_drawn = look;
}

// --- the parchment (hint_card_style=1) -----------------------------------------

// The capital letter of a key, and of a word on a key, in pixels.
const int kInkCap = 30;
const int kWordCap = 19;
// The wax seal of Shift is this far inside the key.
const int kWaxInsetX = 7;
const int kWaxInsetY = 8;

const size_t kPictureBytes = static_cast<size_t>(kWidth) * kHeight * 4;
uint8_t* g_page = nullptr;  // the page with no keys
uint8_t* g_card = nullptr;  // the picture that the layer shows
// The box of each key in each look, for Shift off [0] and on [1]. Only the
// letters and Shift change with Shift: the other keys use [0].
uint8_t* g_tiles[2][card_scroll::kKeyLooks][vr_keyboard_math::kKeyCount] = {};
Look g_card_look = {-2, -2, false, -1};  // style -1: g_card has no picture
bool g_parchment_failed = false;
HFONT g_ink_font = nullptr;
HFONT g_ink_word_font = nullptr;
HFONT g_ink_digit_font = nullptr;
int g_ink_ascent = 0, g_ink_height = 0;
int g_word_ascent = 0, g_word_height = 0;
int g_digit_ascent = 0, g_digit_height = 0;

bool ParchmentStart() {
  if (g_card != nullptr) return true;
  if (g_parchment_failed) return false;
  g_parchment_failed = true;
  if (!card_scroll::Start(g_dc)) {
    ModLog("keyboard     : GDI gave no font for the parchment, so the keyboard "
           "is dark");
    return false;
  }
  g_ink_font = card_scroll::WordsFont(g_dc, kInkCap);
  g_ink_word_font = card_scroll::SealFont(g_dc, kWordCap);
  const char* digit_face = "";
  g_ink_digit_font = card_scroll::DigitsFont(g_dc, kInkCap, &digit_face);
  g_page = new (std::nothrow) uint8_t[kPictureBytes];
  g_card = new (std::nothrow) uint8_t[kPictureBytes];
  if (g_ink_font == nullptr || g_ink_word_font == nullptr ||
      g_ink_digit_font == nullptr || g_page == nullptr || g_card == nullptr) {
    delete[] g_page;
    delete[] g_card;
    g_page = g_card = nullptr;
    ModLog("keyboard     : no font or no memory for the parchment, so the "
           "keyboard is dark");
    return false;
  }
  TEXTMETRICW metrics;
  SelectObject(g_dc, g_ink_font);
  GetTextMetricsW(g_dc, &metrics);
  g_ink_ascent = metrics.tmAscent;
  g_ink_height = metrics.tmHeight;
  SelectObject(g_dc, g_ink_word_font);
  GetTextMetricsW(g_dc, &metrics);
  g_word_ascent = metrics.tmAscent;
  g_word_height = metrics.tmHeight;
  SelectObject(g_dc, g_ink_digit_font);
  GetTextMetricsW(g_dc, &metrics);
  g_digit_ascent = metrics.tmAscent;
  g_digit_height = metrics.tmHeight;
  const double start = NowMs();
  const card_scroll::Picture page = {g_page, kWidth, kWidth, kHeight};
  card_scroll::Page(page, 4, 4, kWidth - 4, kHeight - 4, 9);
  g_parchment_failed = false;
  ModLog("keyboard     : the page of the parchment keyboard took %.2f ms (%s). "
         "The digits use %s, so that they are as high as a capital letter",
         NowMs() - start, card_scroll::FontNote(), digit_face);
  return true;
}

int LookOf(int i, const Look& look) {
  if (i == look.flash) return card_scroll::kKeyFlash;
  if (i == look.hover) return card_scroll::kKeyHover;
  return card_scroll::kKeyPlain;
}

bool ShiftChanges(int i) {
  const vr_keyboard_math::Key& k = vr_keyboard_math::kKeys[i];
  return k.kind == vr_keyboard_math::kShift ||
         (k.kind == vr_keyboard_math::kChar && k.label >= 'A' &&
          k.label <= 'Z');
}

void Box(int i, int box[4]) {
  vr_keyboard_math::KeyBox(i, kWidth, kHeight, kMargin, kGap,
                           card_scroll::kKeyRim, box);
}

// A copy of the box of key i from the card to its tile, and back.
void CardToTile(int i, uint8_t* tile) {
  int box[4];
  Box(i, box);
  const size_t row = static_cast<size_t>(box[2] - box[0]) * 4;
  for (int y = box[1]; y < box[3]; ++y) {
    memcpy(tile + static_cast<size_t>(y - box[1]) * row,
           g_card + (static_cast<size_t>(y) * kWidth + box[0]) * 4, row);
  }
}

void TileToCard(int i, const uint8_t* tile) {
  int box[4];
  Box(i, box);
  const size_t row = static_cast<size_t>(box[2] - box[0]) * 4;
  for (int y = box[1]; y < box[3]; ++y) {
    memcpy(g_card + (static_cast<size_t>(y) * kWidth + box[0]) * 4,
           tile + static_cast<size_t>(y - box[1]) * row, row);
  }
}

// Draws key i in the look into the card, over the page.
void DrawKey(int i, bool shift, int look) {
  int box[4];
  Box(i, box);
  const size_t row = static_cast<size_t>(box[2] - box[0]) * 4;
  for (int y = box[1]; y < box[3]; ++y) {
    const size_t at = (static_cast<size_t>(y) * kWidth + box[0]) * 4;
    memcpy(g_card + at, g_page + at, row);
  }
  const card_scroll::Picture picture = {g_card, kWidth, kWidth, kHeight};
  int r[4];
  vr_keyboard_math::KeyRect(i, kWidth, kHeight, kMargin, kGap, r);
  const int x0 = r[0], y0 = r[1], x1 = r[0] + r[2], y1 = r[1] + r[3];
  const vr_keyboard_math::Key& k = vr_keyboard_math::kKeys[i];
  card_scroll::Key(picture, x0, y0, x1, y1, look, i * 7 + 3);
  const bool wax = k.kind == vr_keyboard_math::kShift && shift;
  if (wax) {
    card_scroll::Seal(picture, x0 + kWaxInsetX, y0 + kWaxInsetY,
                      x1 - kWaxInsetX, y1 - kWaxInsetY, 0, i * 7 + 5);
  }
  // The letter, white on black, with its capital in the middle of the key.
  // Then the ink.
  for (int y = box[1]; y < box[3]; ++y) {
    memset(g_bits + static_cast<size_t>(y) * kWidth + box[0], 0, row);
  }
  wchar_t label[8];
  Label(i, shift, label);
  const bool word = k.kind != vr_keyboard_math::kChar;
  const bool digit = !word && k.label >= '0' && k.label <= '9';
  const int cap = word ? kWordCap : kInkCap;
  const int ascent =
      word ? g_word_ascent : (digit ? g_digit_ascent : g_ink_ascent);
  const int height =
      word ? g_word_height : (digit ? g_digit_height : g_ink_height);
  const int baseline = y0 + (r[3] + cap) / 2;
  SelectObject(g_dc, word ? g_ink_word_font
                          : (digit ? g_ink_digit_font : g_ink_font));
  SetTextColor(g_dc, RGB(255, 255, 255));
  RECT rect = {x0, baseline - ascent, x1, baseline - ascent + height};
  text_fit::Draw(g_dc, label, rect, DT_CENTER | DT_TOP | DT_NOCLIP, 1,
                 "keyboard");
  GdiFlush();
  if (wax) {
    card_scroll::SealInk(picture, g_bits, x0 + kWaxInsetX, y0 + kWaxInsetY,
                         x1 - kWaxInsetX, y1 - kWaxInsetY, 0);
  } else {
    card_scroll::Ink(picture, g_bits, x0, y0, x1, y1, i * 7 + 3);
  }
}

// Puts key i in the look into the card: a copy of its tile, or a draw that
// makes the tile. False for a draw.
bool ShowKey(int i, bool shift, int look) {
  const int s = shift && ShiftChanges(i) ? 1 : 0;
  uint8_t*& tile = g_tiles[s][look][i];
  if (tile != nullptr) {
    TileToCard(i, tile);
    return true;
  }
  DrawKey(i, shift, look);
  int box[4];
  Box(i, box);
  tile = new (std::nothrow)
      uint8_t[static_cast<size_t>(box[2] - box[0]) * (box[3] - box[1]) * 4];
  if (tile != nullptr) CardToTile(i, tile);
  return false;
}

// Makes the card show the look: the keys whose look changed, and all the
// keys for the first picture.
void Compose(const Look& look, uint64_t frame) {
  const double start = NowMs();
  const bool first = g_card_look.style < 0;
  if (first) memcpy(g_card, g_page, kPictureBytes);
  int drawn = 0, copied = 0;
  for (int i = 0; i < vr_keyboard_math::kKeyCount; ++i) {
    const int now = LookOf(i, look);
    if (!first && now == LookOf(i, g_card_look) &&
        (look.shift == g_card_look.shift || !ShiftChanges(i))) {
      continue;
    }
    if (ShowKey(i, look.shift, now)) {
      ++copied;
    } else {
      ++drawn;
    }
  }
  g_card_look = look;
  // The letters of the parchment are in g_bits now, so the dark card must
  // draw again.
  g_drawn.hover = -2;
  const double ms = NowMs() - start;
  ++g_draws;
  g_draw_ms += ms;
  if (ms > g_draw_most_ms) g_draw_most_ms = ms;
  if (g_draw_logs < kMaxDrawLogs) {
    ++g_draw_logs;
    ModLog("keyboard     : frame %llu, a new picture of the parchment "
           "keyboard took %.2f ms: %d keys drawn and %d keys copied", frame, ms,
           drawn, copied);
  }
}

void Close(const char* why) {
  SendUpNow();
  if (!g_shown) return;
  g_shown = false;
  g_hover = -1;
  ModLog("keyboard     : the keyboard goes away: %s", why);
}

}  // namespace

void VrKeyboardSetOptions(bool on) { g_on = on; }

bool VrKeyboardShown() { return g_shown; }

void VrKeyboardHide() { Close("no menu in the room"); }

void VrKeyboardFrame(uint64_t frame) {
  const ULONGLONG now = GetTickCount64();
  if (g_up_waits && now >= g_up_at) SendUpNow();
  if (!g_on) return;
  if (!EditHasFocus()) {
    Close("no text box has the focus");
    return;
  }
  if (bwvr::XrScreen::CardFailed(bwvr::kCardKeyboard)) {
    Close("the runtime refused the layer of the keyboard");
    return;
  }
  float position[3], orientation[4], width = 0.0f, height = 0.0f;
  if (!MenuRoomKeyboardPlace(position, orientation, &width, &height)) return;
  if (!g_shown) {
    g_shown = true;
    ++g_opens;
    vr_keyboard_math::ShiftOpen(&g_shift);
    // A trigger that is already down is not a press on a key.
    g_press.down = true;
    g_trigger_down = true;
    ModLog("keyboard     : frame %llu, a text box of the game has the focus, "
           "so the keyboard shows below the menu panel, %.2f by %.2f m at "
           "%.2f %.2f %.2f m", frame, width, height, position[0], position[1],
           position[2]);
  }

  // The key under the ray, and a light pulse at each new key.
  float u = 0.0f, v = 0.0f;
  int key = -1;
  if (MenuRoomKeyboardRay(&u, &v)) {
    key = vr_keyboard_math::KeyAt(u, v, kWidth, kHeight, kMargin, kGap);
  }
  if (key != g_hover && key >= 0) ControllersGesturePulse(0.1f, 0.008f);
  g_hover = key;

  // The trigger, with a band between down and up.
  const float trigger = MenuRoomTrigger();
  if (trigger > kTriggerDown) g_trigger_down = true;
  if (trigger < kTriggerUp) g_trigger_down = false;
  const int pressed = vr_keyboard_math::PressStep(&g_press, g_trigger_down, key);
  if (pressed >= 0) {
    ControllersGesturePulse(0.3f, 0.02f);
    g_flash = pressed;
    g_flash_until = now + kFlashMs;
    ++g_keys;
    char name[4];
    if (vr_keyboard_math::kKeys[pressed].kind == vr_keyboard_math::kShift) {
      vr_keyboard_math::ShiftPress(&g_shift);
      if (g_key_logs < kMaxKeyLogs) {
        ++g_key_logs;
        ModLog("keyboard     : frame %llu, Shift: the next letter is %s",
               frame, g_shift.on ? "a capital" : "small");
      }
    } else {
      uint32_t vk = 0, ch = 0;
      if (vr_keyboard_math::Output(pressed, g_shift.on, &vk, &ch)) {
        SendKey(vk, ch);
        if (g_key_logs < kMaxKeyLogs) {
          ++g_key_logs;
          ModLog("keyboard     : frame %llu, the key %s goes to the game "
                 "(virtual key 0x%02X, character 0x%02X)", frame,
                 KeyName(pressed, name, sizeof(name)), vk, ch);
        }
      }
      vr_keyboard_math::ShiftAfter(&g_shift, pressed);
    }
  }
  if (g_flash >= 0 && now >= g_flash_until) g_flash = -1;

  if (!RasterStart()) return;
  const Look look = {g_hover, g_flash, g_shift.on, CardStyle()};
  const uint8_t* pixels = reinterpret_cast<const uint8_t*>(g_bits);
  bool fresh = false;
  if (look.style != 0 && ParchmentStart()) {
    if (!SameLook(look, g_card_look)) {
      Compose(look, frame);
      fresh = true;
    }
    pixels = g_card;
  } else if (!SameLook(look, g_drawn)) {
    const double start = NowMs();
    Raster(look);
    const double ms = NowMs() - start;
    ++g_draws;
    g_draw_ms += ms;
    if (ms > g_draw_most_ms) g_draw_most_ms = ms;
    if (g_draw_logs < kMaxDrawLogs) {
      ++g_draw_logs;
      ModLog("keyboard     : frame %llu, a new picture of the dark keyboard "
             "took %.2f ms", frame, ms);
    }
    fresh = true;
  }
  if (fresh && g_saved < g_save) {
    ++g_saved;
    char name[8];
    ModLog("keyboard     : frame %llu, the saved picture has the ray on %s, "
           "the flash on %s, and Shift %s", frame,
           look.hover >= 0 ? KeyName(look.hover, name, sizeof(name)) : "no key",
           look.flash >= 0 ? KeyName(look.flash, name + 4, 4) : "no key",
           look.shift ? "on" : "off");
    EyeCaptureSaveUi("keyboard", frame, 1, pixels, kWidth * 4, kWidth, kHeight);
  }
  // THE NUMBER OF THE PICTURE (CardLayer::serial): new at each new picture
  // and at each change between the dark card and the parchment, so the view
  // for videos copies the 1.5 MB of the keyboard only after a change.
  static uint32_t serial = 0;
  static const uint8_t* serial_pixels = nullptr;
  if (fresh || serial == 0 || pixels != serial_pixels) {
    serial = bwvr::XrScreen::NewCardSerial();
    serial_pixels = pixels;
  }
  bwvr::CardLayer layer = {};
  layer.visible = true;
  layer.pixels = pixels;
  layer.serial = serial;
  layer.texture_width = kWidth;
  layer.texture_height = kHeight;
  layer.rect[0] = 0;
  layer.rect[1] = 0;
  layer.rect[2] = kWidth;
  layer.rect[3] = kHeight;
  for (int i = 0; i < 3; ++i) layer.position[i] = position[i];
  for (int i = 0; i < 4; ++i) layer.orientation[i] = orientation[i];
  layer.width = width;
  layer.height = height;
  bwvr::XrScreen::StageCard(bwvr::kCardKeyboard, layer);
}

void VrKeyboardReport() {
  if (g_opens + g_keys + g_draws == 0) return;
  ModLog("keyboard     : the keyboard showed %llu times, and %llu keys were "
         "pressed this window", g_opens, g_keys);
  if (g_draws > 0) {
    ModLog("keyboard     : %llu new pictures of the keyboard, %.2f ms each on "
           "average and %.2f ms at the most", g_draws,
           g_draw_ms / static_cast<double>(g_draws), g_draw_most_ms);
  }
  g_opens = 0;
  g_keys = 0;
  g_draws = 0;
  g_draw_ms = g_draw_most_ms = 0.0;
}

// THE CHECK of the words (text_fit_test=1): the letter or the word of each
// key, with and with no shift, in the face of its key. See text_fit.h.
void VrKeyboardTextFitTest() {
  if (!RasterStart()) return;
  for (int i = 0; i < vr_keyboard_math::kKeyCount; ++i) {
    const vr_keyboard_math::Key& k = vr_keyboard_math::kKeys[i];
    int r[4];
    vr_keyboard_math::KeyRect(i, kWidth, kHeight, kMargin, kGap, r);
    for (int shift = 0; shift < 2; ++shift) {
      wchar_t label[8];
      Label(i, shift != 0, label);
      text_fit::Check(g_dc,
                      k.kind == vr_keyboard_math::kChar ? g_font : g_word_font,
                      label, r[2], r[3], 1, "keyboard");
    }
  }
}
