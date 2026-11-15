// The sheet of the gestures over the palm. See gesture_sheet.h and
// docs/miracles.md, "The sheet of the gestures".
//
// WHY. The plain game shows the gesture pictures of the miracles at the lower
// right of its screen. The lens of the headset cuts that corner off. A tap of
// B opens the spell book, so the palm that turns up is free for a sheet that
// shows how to draw each miracle.
//
// WHAT. While the palm of the hand that does not point is up, and the player
// draws no gesture, a page of parchment shows over that palm, at the place
// of the spell book (spell_book_math::BookCenter), facing the head. It has a
// row for each miracle that the Worship Sites of the player offer (the
// icons whose magic type is on, as the book lists them), and a row for the
// gesture "again". A row has the pictures of the chain of the miracle
// (GSpellSeedInfo+0x100: the spiral or the inverse spiral small, then the
// gesture of the miracle) and its name. The pictures are the game's own: the
// cells of data\textures\S_Gesture0.raw and S_Gesture1.raw with their alpha
// files, which the game draws at the corner of its screen
// (gesture_sheet_math::CellOf). The mod reads the files of the game folder
// once, and keeps no copy.
//
// While the draw button is down, or the game shows a picture that goes on
// from a drawn gesture, the palm shows the row of the game's live pictures
// (gesture_panel.cpp) and not the sheet.
//
// THE KEYS of bwvr.ini:
//
//   gesture_sheet=1          0: no sheet. The palm shows the row of the
//                            game's pictures, or opens the book with
//                            spell_book_palm=1
//   gesture_sheet_test_save=0  N: saves the first N pictures of the sheet as
//                            bwvr-sheet-NNNNN.bmp (a test key)
//   gesture_sheet_test_frame=0  a test key: from this frame of the scripted
//                            controller the sheet shows for 300 frames, also
//                            with the palm down

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "../vr/xr_screen.h"
#include "card_scroll.h"
#include "controllers.h"
#include "cutscene_window.h"
#include "gesture_panel.h"
#include "gesture_panel_math.h"
#include "gesture_sheet.h"
#include "gesture_sheet_math.h"
#include "radial_menu.h"
#include "spell_book.h"
#include "spell_book_math.h"
#include "temple.h"
#include "text_fit.h"

extern void ModLog(const char* format, ...);
uint64_t ControllersFrameCount();
uint64_t ControllersTestFrame(uint64_t key_frame, uint64_t frame);
bool ControllersCardPose(int which, float grip_position[3],
                         float grip_orientation[4], float head[3], bool* left);
void EyeCaptureSaveUi(const char* name, uint64_t frame, int style,
                      const void* pixels, int pitch, uint32_t width,
                      uint32_t height);

namespace {

using namespace gesture_sheet_math;

// GSpellSystemInfo: the gesture type of "again" [V].
const uintptr_t kAgainType = 0x00DA0570;
const int kGestureTypes = 24;

bool g_read = false;
bool g_on = true;
int g_save = 0;
int g_saved = 0;
uint64_t g_test_frame = 0;

// THE PICTURES OF THE GAME: the alpha of the two textures. The color files
// are white strokes, so the alpha alone is the ink.
bool g_atlas_read = false;
bool g_atlas_ok = false;
uint8_t g_alpha[kAtlases][kAtlasSide * kAtlasSide];

bool ReadFileBytes(const char* path, uint8_t* out, size_t size) {
  HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD got = 0;
  const BOOL ok = ReadFile(file, out, static_cast<DWORD>(size), &got, nullptr);
  CloseHandle(file);
  return ok && got == size;
}

void ReadAtlas() {
  if (g_atlas_read) return;
  g_atlas_read = true;
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr) {
    ModLog("gesture sheet: the folder of the game is not known, so no sheet");
    return;
  }
  slash[1] = 0;
  g_atlas_ok = true;
  for (int a = 0; a < kAtlases && g_atlas_ok; ++a) {
    char file[MAX_PATH];
    sprintf_s(file, "%sdata\\textures\\S_Gesture%da.raw", path, a);
    if (!ReadFileBytes(file, g_alpha[a], sizeof(g_alpha[a]))) {
      g_atlas_ok = false;
      ModLog("gesture sheet: the game has no file %s of %d bytes, so no sheet",
             file, kAtlasSide * kAtlasSide);
    }
  }
  if (g_atlas_ok) {
    ModLog("gesture sheet: the gesture pictures of the game come from "
           "data\\textures\\S_Gesture0a.raw and S_Gesture1a.raw");
  }
}

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
  g_on = IniKey("gesture_sheet", 1) != 0;
  g_save = IniKey("gesture_sheet_test_save", 0);
  const int test = IniKey("gesture_sheet_test_frame", 0);
  g_test_frame = test > 0 ? static_cast<uint64_t>(test) : 0;
  ModLog("gesture sheet: gesture_sheet=%d: %s", g_on ? 1 : 0,
         g_on ? "the palm of the hand that does not point shows the gesture "
                "pictures of the game for each miracle of the Worship Sites, "
                "while the player draws no gesture"
              : "no sheet of the gestures");
}

// --- what the sheet shows ---------------------------------------------------

struct Row {
  int seed;           // the seed type, or -1 for the row of "again"
  int count;          // the gestures of the chain
  int gesture[kMostGestures];
};

struct Look {
  int rows;
  Row row[kMostRows];
  bool no_site;
};

bool SameLook(const Look& a, const Look& b) {
  return memcmp(&a, &b, sizeof(Look)) == 0;
}

void BuildLook(Look* look) {
  memset(look, 0, sizeof(*look));
  int seeds[kMostRows];
  const int found = SpellBookSiteMiracles(seeds, kMostRows - 1);
  look->no_site = found < 0;
  for (int i = 0; i < found; ++i) {
    Row& r = look->row[look->rows];
    r.seed = seeds[i];
    r.count = SpellBookChain(seeds[i], r.gesture);
    if (r.count > 0) ++look->rows;
  }
  // The gesture "again" asks for the last miracle again. The flat game
  // shows it whenever a Worship Site has a miracle.
  const int again = *reinterpret_cast<const int32_t*>(kAgainType);
  if (look->rows > 0 && again > 0 && again < kGestureTypes) {
    Row& r = look->row[look->rows++];
    r.seed = -1;
    r.count = 1;
    r.gesture[0] = again;
  }
}

// --- the picture -------------------------------------------------------------

struct Rgb {
  float r, g, b;
};
const Rgb kInk = {52.0f, 30.0f, 14.0f};       // the dark brown of the cards
const Rgb kStartInk = {120.0f, 86.0f, 52.0f};  // the start of a chain
const Rgb kNoteInk = {150.0f, 40.0f, 20.0f};
const Rgb kRule = {90.0f, 55.0f, 25.0f};
const Rgb kWhite = {255.0f, 255.0f, 255.0f};
const Rgb kPanel = {22.0f, 26.0f, 34.0f};

HDC g_dc = nullptr;
HBITMAP g_dib = nullptr;
uint32_t* g_bits = nullptr;
HFONT g_title_font = nullptr;
HFONT g_name_font = nullptr;
bool g_page = false;
bool g_raster_failed = false;
uint8_t g_pixels[kTexture * kTexture * 4];
int g_width_px = 0, g_height_px = 0;
Layout g_layout = LayoutFor(0);
Look g_drawn;
bool g_have_drawn = false;
uint32_t g_serial = 0;
uint64_t g_rasters = 0;
double g_raster_ms = 0.0;

double NowSeconds() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart) /
         static_cast<double>(frequency.QuadPart);
}

bool RasterStart() {
  if (g_dc != nullptr) return true;
  if (g_raster_failed) return false;
  g_dc = CreateCompatibleDC(nullptr);
  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = kTexture;
  info.bmiHeader.biHeight = -kTexture;  // top first
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  if (g_dc != nullptr) {
    g_dib = CreateDIBSection(g_dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  }
  // The font of the hint cards and of the book, when the cards are
  // parchment (hint_card_style=1).
  g_page = IniKey("hint_card_style", 1) != 0 && g_dc != nullptr &&
           card_scroll::Start(g_dc);
  const auto font = [](int em, int weight) {
    if (g_page) return card_scroll::WordsFont(g_dc, em * 7 / 10);
    return CreateFontW(-em, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
                       L"Segoe UI");
  };
  g_title_font = font(48, FW_SEMIBOLD);
  g_name_font = font(36, FW_BOLD);
  if (g_dc == nullptr || g_dib == nullptr || bits == nullptr ||
      g_title_font == nullptr || g_name_font == nullptr) {
    g_raster_failed = true;
    ModLog("gesture sheet: GDI gave no picture for the sheet, so it does not "
           "show");
    return false;
  }
  g_bits = static_cast<uint32_t*>(bits);
  SelectObject(g_dc, g_dib);
  SetBkMode(g_dc, TRANSPARENT);
  SetTextColor(g_dc, RGB(255, 255, 255));
  return true;
}

float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

void Over(uint8_t* out, const Rgb& color, float a) {
  if (a <= 0.0f) return;
  const float keep = 1.0f - a;
  out[0] = static_cast<uint8_t>(color.b * a + out[0] * keep + 0.5f);
  out[1] = static_cast<uint8_t>(color.g * a + out[1] * keep + 0.5f);
  out[2] = static_cast<uint8_t>(color.r * a + out[2] * keep + 0.5f);
  out[3] = static_cast<uint8_t>(255.0f * a + out[3] * keep + 0.5f);
}

// The alpha of a cell of the textures at u, v (0 to 1 across the cell), with
// a bilinear filter.
float CellAlpha(int atlas, int cx, int cy, float u, float v) {
  const float fx = u * kCellSide - 0.5f, fy = v * kCellSide - 0.5f;
  const int x0 = static_cast<int>(floorf(fx)), y0 = static_cast<int>(floorf(fy));
  const float ax = fx - x0, ay = fy - y0;
  float sum = 0.0f;
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < 2; ++i) {
      int x = x0 + i, y = y0 + j;
      if (x < 0 || y < 0 || x >= kCellSide || y >= kCellSide) continue;
      const float w = (i ? ax : 1.0f - ax) * (j ? ay : 1.0f - ay);
      sum += w * g_alpha[atlas][(cy + y) * kAtlasSide + cx + x];
    }
  }
  return sum * (1.0f / 255.0f);
}

// The picture of a gesture type in a box of side px at x, y (its top left),
// in pixels of the sheet. Each pixel of the box takes the mean of 2 by 2
// samples of the cell, so a small box keeps its strokes smooth.
void DrawGesture(int gesture, float x, float y, float px, const Rgb& ink) {
  int atlas = 0, cx = 0, cy = 0;
  if (!CellOf(gesture, &atlas, &cx, &cy)) return;
  const int x0 = static_cast<int>(floorf(x)), y0 = static_cast<int>(floorf(y));
  const int x1 = static_cast<int>(ceilf(x + px)),
            y1 = static_cast<int>(ceilf(y + px));
  for (int yy = y0 < 0 ? 0 : y0; yy < y1 && yy < g_height_px; ++yy) {
    uint8_t* row = g_pixels + static_cast<size_t>(yy) * kTexture * 4;
    for (int xx = x0 < 0 ? 0 : x0; xx < x1 && xx < g_width_px; ++xx) {
      float a = 0.0f;
      for (int s = 0; s < 4; ++s) {
        const float sx = xx + 0.25f + 0.5f * (s & 1);
        const float sy = yy + 0.25f + 0.5f * (s >> 1);
        const float u = (sx - x) / px, v = (sy - y) / px;
        if (u < 0.0f || v < 0.0f || u >= 1.0f || v >= 1.0f) continue;
        a += 0.25f * CellAlpha(atlas, cx, cy, u, v);
      }
      Over(row + xx * 4, ink, a);
    }
  }
}

void Raster(const Look& look) {
  if (!RasterStart()) return;
  const double start = NowSeconds();
  const Layout l = LayoutFor(look.rows);
  g_layout = l;
  const float ppm = l.pixels_per_meter;
  g_width_px = PixelsOf(l.width_m, ppm);
  g_height_px = PixelsOf(l.height_m, ppm);
  const int w = g_width_px, h = g_height_px;
  // The words first, white on black: the green of a pixel is its cover.
  for (int y = 0; y < h; ++y) {
    memset(g_bits + static_cast<size_t>(y) * kTexture, 0,
           static_cast<size_t>(w) * 4);
  }
  const int title_px = static_cast<int>(kTitleM * ppm);
  SelectObject(g_dc, g_title_font);
  if (look.rows == 0) {
    RECT band = {16, 2, w - 16, h / 2 + 4};
    text_fit::Draw(g_dc, L"No miracle to cast now", band,
                   DT_CENTER | DT_VCENTER, 2, "gesture sheet title");
    RECT note = {16, h / 2, w - 16, h - 6};
    text_fit::Draw(g_dc,
                   look.no_site ? L"You have no Worship Site"
                                : L"Your Worship Site has no miracle",
                   note, DT_CENTER | DT_VCENTER, 2, "gesture sheet note");
  } else {
    RECT band = {16, 2, w - 16, title_px};
    text_fit::Draw(g_dc, L"Gestures of the miracles", band,
                   DT_CENTER | DT_VCENTER, 1, "gesture sheet title");
  }
  SelectObject(g_dc, g_name_font);
  for (int r = 0; r < look.rows; ++r) {
    float left = 0.0f, middle = 0.0f, name_left = 0.0f, name_w = 0.0f;
    RowAt(l, r, &left, &middle);
    NameAt(&name_left, &name_w);
    RECT box = {static_cast<LONG>((left + name_left) * ppm),
                static_cast<LONG>((middle - 0.5f * kRowM) * ppm),
                static_cast<LONG>((left + name_left + name_w) * ppm),
                static_cast<LONG>((middle + 0.5f * kRowM) * ppm)};
    const wchar_t* name = look.row[r].seed >= 0
                              ? SpellBookSeedName(look.row[r].seed)
                              : L"Again: the last miracle";
    text_fit::Draw(g_dc, name, box, DT_LEFT | DT_VCENTER, 2,
                   "gesture sheet name");
  }
  GdiFlush();

  // The page, then the pictures and the words over it.
  if (g_page) {
    const card_scroll::Picture page = {g_pixels, kTexture, w, h};
    card_scroll::Page(page, 4, 4, w - 4, h - 4, 7);
  } else {
    for (int y = 0; y < h; ++y) {
      uint8_t* row = g_pixels + static_cast<size_t>(y) * kTexture * 4;
      memset(row, 0, static_cast<size_t>(w) * 4);
      for (int x = 0; x < w; ++x) Over(row + x * 4, kPanel, 0.86f);
    }
  }
  const Rgb& ink = g_page ? kInk : kWhite;
  const Rgb& start_ink = g_page ? kStartInk : kWhite;
  for (int r = 0; r < look.rows; ++r) {
    float left = 0.0f, middle = 0.0f;
    RowAt(l, r, &left, &middle);
    const Row& row = look.row[r];
    for (int k = 0; k < row.count; ++k) {
      // The row of "again" has its one picture where the gestures start.
      const int slot = row.seed >= 0 ? k : 1;
      float mx = 0.0f, side = 0.0f;
      PictureAt(slot, &mx, &side);
      DrawGesture(row.gesture[k], (left + mx - 0.5f * side) * ppm,
                  (middle - 0.5f * side) * ppm, side * ppm,
                  slot == 0 ? start_ink : ink);
    }
    // A thin rule under each row but the last of a column.
    const bool last = (r + 1) % l.per_column == 0 || r + 1 == look.rows;
    if (!last) {
      const int y = static_cast<int>((middle + 0.5f * kRowM) * ppm);
      const int x0 = static_cast<int>((left + 0.004f) * ppm);
      const int x1 = static_cast<int>((left + l.column_m - 0.004f) * ppm);
      uint8_t* line = g_pixels + static_cast<size_t>(y) * kTexture * 4;
      for (int x = x0; x < x1 && x < w; ++x) {
        Over(line + x * 4, g_page ? kRule : kWhite, 0.25f);
      }
    }
  }
  for (int y = 0; y < h; ++y) {
    uint8_t* row = g_pixels + static_cast<size_t>(y) * kTexture * 4;
    const uint32_t* letters = g_bits + static_cast<size_t>(y) * kTexture;
    const bool note = look.rows == 0 && y >= h / 2;
    for (int x = 0; x < w; ++x) {
      const float cover =
          static_cast<float>((letters[x] >> 8) & 0xFF) * (1.0f / 255.0f);
      if (cover <= 0.0f) continue;
      Over(row + x * 4, note ? kNoteInk : ink, cover);
    }
  }
  g_drawn = look;
  g_have_drawn = true;
  g_serial = bwvr::XrScreen::NewCardSerial();
  ++g_rasters;
  const double ms = (NowSeconds() - start) * 1000.0;
  if (ms > g_raster_ms) g_raster_ms = ms;
  ModLog("gesture sheet: a new picture of the sheet, %d by %d px for %.1f by "
         "%.1f cm, %d rows in %d columns, took %.2f ms", w, h,
         l.width_m * 100.0f, l.height_m * 100.0f, look.rows, l.columns, ms);
  if (g_saved < g_save) {
    ++g_saved;
    EyeCaptureSaveUi("sheet", ControllersFrameCount(), 1, g_pixels,
                     kTexture * 4, static_cast<uint32_t>(w),
                     static_cast<uint32_t>(h));
  }
}

// --- the frame -----------------------------------------------------------------

bool g_palm_up = false;
bool g_showing = false;
const char* g_hide_why = "";
uint64_t g_shown_frames = 0;
int g_logs = 0;
const int kMaxLogs = 200;

bool LogRoom() {
  if (g_logs >= kMaxLogs) return false;
  ++g_logs;
  return true;
}

}  // namespace

bool GestureSheetOn() {
  ReadSettings();
  if (!g_on) return false;
  ReadAtlas();
  return g_atlas_ok && !g_raster_failed;
}

int GestureSheetValue() {
  ReadSettings();
  return g_on ? 1 : 0;
}

void GestureSheetChange(const char* by) {
  ReadSettings();
  g_on = !g_on;
  ModLog("gesture sheet: %s: gesture_sheet=%d", by, g_on ? 1 : 0);
}

void GestureSheetFrame() {
  ReadSettings();
  const uint64_t frame = ControllersFrameCount();
  float grip[3], turn[4], head[3];
  bool left = true;
  const bool have_pose = ControllersCardPose(1, grip, turn, head, &left);
  g_palm_up = have_pose &&
              gesture_panel_math::PalmUpStep(
                  g_palm_up, gesture_panel_math::PalmUp(turn, left),
                  gesture_panel_math::kUpOnDeg, gesture_panel_math::kUpOffDeg);
  const uint64_t test =
      g_test_frame != 0 ? ControllersTestFrame(g_test_frame, frame) : 0;
  const bool test_now =
      g_test_frame != 0 && test >= g_test_frame && test < g_test_frame + 300;
  RingHand hand;
  const bool have_hand = ControllersRingHand(&hand);
  const char* why = "";
  if (!GestureSheetOn()) {
    why = g_on ? "the pictures of the game are not there" : "gesture_sheet=0";
  } else if (!have_pose || !have_hand || !hand.active) {
    why = "the hand has no pose";
  } else if (!g_palm_up && !test_now) {
    why = "the palm is not up";
  } else if (SpellBookOpen()) {
    why = "the spell book is open";
  } else if (SpellBookPalmOn() && !GesturePanelWantsPalm()) {
    why = "the palm opens the spell book (spell_book_palm=1)";
  } else if (GesturePanelWantsPalm()) {
    why = "the player draws a gesture, so the palm shows the row of the game";
  } else if (!hand.usable || hand.dialog) {
    why = "the controllers cannot act, or a dialog box is up";
  } else if (RadialMenuShows()) {
    why = "the ring shows";
  } else if (CutsceneWindowFlat()) {
    why = "the cut scene window is up";
  } else if (TempleInside()) {
    why = "the player is in the Temple";
  }
  bool show = why[0] == 0;
  if (show) {
    Look look;
    BuildLook(&look);
    if (!g_have_drawn || !SameLook(look, g_drawn)) Raster(look);
    show = g_have_drawn && !bwvr::XrScreen::CardFailed(bwvr::kCardSheet);
    if (!show) why = "the picture or the card failed";
  }
  if (show) {
    float center[3];
    spell_book_math::BookCenter(grip, turn, left, center);
    spell_book_math::Plane plane;
    spell_book_math::PlaneAt(center, head, &plane);
    bwvr::CardLayer layer = {};
    layer.visible = true;
    layer.pixels = g_pixels;
    layer.texture_width = kTexture;
    layer.texture_height = kTexture;
    layer.rect[0] = 0;
    layer.rect[1] = 0;
    layer.rect[2] = g_width_px;
    layer.rect[3] = g_height_px;
    for (int i = 0; i < 3; ++i) layer.position[i] = plane.center[i];
    for (int i = 0; i < 4; ++i) layer.orientation[i] = plane.orientation[i];
    layer.width = g_width_px / g_layout.pixels_per_meter;
    layer.height = g_height_px / g_layout.pixels_per_meter;
    layer.serial = g_serial;
    bwvr::XrScreen::StageCard(bwvr::kCardSheet, layer);
    ++g_shown_frames;
  }
  if (show != g_showing && LogRoom()) {
    if (show) {
      ModLog("gesture sheet: the sheet of %d rows shows over the %s palm, "
             "%.1f by %.1f cm, frame %llu", g_drawn.rows,
             left ? "left" : "right", g_layout.width_m * 100.0f,
             g_layout.height_m * 100.0f, frame);
    } else {
      ModLog("gesture sheet: the sheet does not show: %s, frame %llu", why,
             frame);
    }
  }
  g_showing = show;
  g_hide_why = why;
}

void GestureSheetReport() {
  if (g_shown_frames == 0 && g_rasters == 0) return;
  ModLog("gesture sheet: the sheet showed in %llu frames this window. %llu "
         "new pictures, the longest took %.2f ms", g_shown_frames, g_rasters,
         g_raster_ms);
  g_shown_frames = 0;
  g_rasters = 0;
  g_raster_ms = 0.0;
}
