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
//   vr_keyboard=1  0: no keyboard in the headset

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../vr/xr_screen.h"
#include "menu_room.h"
#include "vr_keyboard_math.h"

extern void ModLog(const char* format, ...);

// controllers.cpp
void ControllersGesturePulse(float amplitude, float seconds);

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
};
Look g_drawn = {-2, -2, false};

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
    wchar_t label[8] = {0};
    switch (k.kind) {
      case vr_keyboard_math::kSpace: wcscpy_s(label, L"Space"); break;
      case vr_keyboard_math::kBack: wcscpy_s(label, L"Back"); break;
      case vr_keyboard_math::kEnter: wcscpy_s(label, L"Enter"); break;
      case vr_keyboard_math::kShift: wcscpy_s(label, L"Shift"); break;
      default: {
        char c = k.label;
        if (c >= 'A' && c <= 'Z' && !look.shift) c = static_cast<char>(c - 'A' + 'a');
        label[0] = static_cast<wchar_t>(c);
        break;
      }
    }
    SelectObject(g_dc, k.kind == vr_keyboard_math::kChar ? g_font : g_word_font);
    SetTextColor(g_dc, text);
    RECT rect = {r[0], r[1], r[0] + r[2], r[1] + r[3]};
    DrawTextW(g_dc, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }
  GdiFlush();
  // Opaque: the alpha of each pixel is 255, so the color needs no change for
  // the premultiplied alpha of the card.
  for (int i = 0; i < kWidth * kHeight; ++i) g_bits[i] |= 0xFF000000u;
  g_drawn = look;
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
  const Look look = {g_hover, g_flash, g_shift.on};
  if (look.hover != g_drawn.hover || look.flash != g_drawn.flash ||
      look.shift != g_drawn.shift) {
    Raster(look);
  }
  bwvr::CardLayer layer = {};
  layer.visible = true;
  layer.pixels = reinterpret_cast<const uint8_t*>(g_bits);
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
  if (g_opens + g_keys == 0) return;
  ModLog("keyboard     : the keyboard showed %llu times, and %llu keys were "
         "pressed this window", g_opens, g_keys);
  g_opens = 0;
  g_keys = 0;
}
