// The scroll look of the cards. See card_scroll.h.

#include "card_scroll.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

extern void ModLog(const char* format, ...);

namespace card_scroll {
namespace {

// --- the noise tile ----------------------------------------------------------

// Value noise that repeats each kTile pixels, so a lookup is one read with no
// edge. Each channel is in 0 to 1. The smallest cell is 4 pixels.
const int kTile = 256;
const int kTileMask = kTile - 1;
float g_blotch[kTile * kTile];  // cells of 64 to 8 px: the light and dark areas
float g_grain[kTile * kTile];   // fibers of 16 by 4 px and dots of 4 px, about 0
float g_mid[kTile * kTile];     // cells of 16 and 8 px
float g_fine[kTile * kTile];    // cells of 4 px

float Hash(int x, int y, int seed) {
  uint32_t h = static_cast<uint32_t>(x) * 374761393u +
               static_cast<uint32_t>(y) * 668265263u +
               static_cast<uint32_t>(seed) * 1442695041u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return static_cast<float>(h & 0xFFFFFF) / 16777215.0f;
}

// Value noise at a cell size of cell_x by cell_y pixels, which repeats each
// kTile pixels.
float Value(int x, int y, int cell_x, int cell_y, int seed) {
  const int period_x = kTile / cell_x;
  const int period_y = kTile / cell_y;
  const int xi = x / cell_x;
  const int yi = y / cell_y;
  float fx = static_cast<float>(x % cell_x) / cell_x;
  float fy = static_cast<float>(y % cell_y) / cell_y;
  fx = fx * fx * (3.0f - 2.0f * fx);
  fy = fy * fy * (3.0f - 2.0f * fy);
  const int x0 = xi % period_x, x1 = (xi + 1) % period_x;
  const int y0 = yi % period_y, y1 = (yi + 1) % period_y;
  const float a = Hash(x0, y0, seed), b = Hash(x1, y0, seed);
  const float c = Hash(x0, y1, seed), d = Hash(x1, y1, seed);
  return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

void MakeTile() {
  for (int y = 0; y < kTile; ++y) {
    for (int x = 0; x < kTile; ++x) {
      const int i = y * kTile + x;
      g_blotch[i] = (Value(x, y, 64, 64, 11) + 0.5f * Value(x, y, 32, 32, 12) +
                     0.25f * Value(x, y, 16, 16, 13) +
                     0.125f * Value(x, y, 8, 8, 14)) /
                    1.875f;
      g_grain[i] = (Value(x, y, 16, 4, 21) - 0.5f) * 0.07f +
                   (Value(x, y, 4, 4, 22) - 0.5f) * 0.05f;
      g_mid[i] = (2.0f * Value(x, y, 16, 16, 31) + Value(x, y, 8, 8, 32)) /
                 3.0f;
      g_fine[i] = Value(x, y, 4, 4, 41);
    }
  }
}

inline float Tile(const float* tile, int x, int y) {
  return tile[(y & kTileMask) * kTile + (x & kTileMask)];
}

// --- small helpers -----------------------------------------------------------

struct Rgb {
  float r, g, b;
};

inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

inline float Smooth(float a, float b, float x) {
  const float t = Clamp01((x - a) / (b - a));
  return t * t * (3.0f - 2.0f * t);
}

inline float Gauss(float x, float s) { return expf(-(x * x) / (2.0f * s * s)); }

// The signed distance to a rect with round corners: below 0 is inside.
inline float RoundRect(float px, float py, float x0, float y0, float x1,
                       float y1, float radius) {
  const float qx = fabsf(px - 0.5f * (x0 + x1)) - (0.5f * (x1 - x0) - radius);
  const float qy = fabsf(py - 0.5f * (y0 + y1)) - (0.5f * (y1 - y0) - radius);
  const float ox = qx > 0.0f ? qx : 0.0f;
  const float oy = qy > 0.0f ? qy : 0.0f;
  const float most = qx > qy ? qx : qy;
  return sqrtf(ox * ox + oy * oy) + (most < 0.0f ? most : 0.0f) - radius;
}

// One color over a pixel, at the alpha a. The pixel has its color multiplied
// by its alpha.
inline void Over(uint8_t* out, const Rgb& c, float a) {
  if (a <= 0.0f) return;
  if (a > 1.0f) a = 1.0f;
  const float k = 1.0f - a;
  const float r = c.r < 0.0f ? 0.0f : (c.r > 255.0f ? 255.0f : c.r);
  const float g = c.g < 0.0f ? 0.0f : (c.g > 255.0f ? 255.0f : c.g);
  const float b = c.b < 0.0f ? 0.0f : (c.b > 255.0f ? 255.0f : c.b);
  out[0] = static_cast<uint8_t>(b * a + out[0] * k + 0.5f);
  out[1] = static_cast<uint8_t>(g * a + out[1] * k + 0.5f);
  out[2] = static_cast<uint8_t>(r * a + out[2] * k + 0.5f);
  out[3] = static_cast<uint8_t>(255.0f * a + out[3] * k + 0.5f);
}

inline Rgb Mix(const Rgb& a, const Rgb& b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

inline Rgb Scale(const Rgb& a, float k) { return {a.r * k, a.g * k, a.b * k}; }

const Rgb kPaper = {240.0f, 218.0f, 170.0f};
const Rgb kBurnt = {112.0f, 62.0f, 26.0f};
const Rgb kRoll = {222.0f, 190.0f, 134.0f};
const Rgb kRollHollow = {80.0f, 46.0f, 18.0f};
const Rgb kInk = {52.0f, 30.0f, 14.0f};
const Rgb kInkBleed = {96.0f, 58.0f, 28.0f};
const Rgb kBlack = {0.0f, 0.0f, 0.0f};
const Rgb kSheen = {255.0f, 250.0f, 235.0f};
const Rgb kWax[kWaxTones] = {
    {160.0f, 30.0f, 26.0f},   // no tone: red
    {36.0f, 124.0f, 60.0f},   // the trigger: green
    {204.0f, 128.0f, 22.0f},  // the grip: amber
    {160.0f, 30.0f, 26.0f},   // any other button: red
};
const Rgb kWaxInk[kWaxTones] = {
    {255.0f, 236.0f, 205.0f},
    {255.0f, 244.0f, 214.0f},
    {70.0f, 36.0f, 6.0f},
    {255.0f, 236.0f, 205.0f},
};

// The burnt edge as a function of the depth into the paper, in steps of
// 0.25 px, so a pixel needs no exp.
const int kBurnSteps = 256;
float g_burn_near[kBurnSteps];  // the dark rim of the paper
float g_burn_far[kBurnSteps];   // the wide brown cloud, times the noise
float g_burn_roll[kBurnSteps];  // the dark rim of a roll

void MakeBurn() {
  for (int i = 0; i < kBurnSteps; ++i) {
    const float depth = i * 0.25f;
    g_burn_near[i] = expf(-depth / 7.0f) * 0.75f;
    g_burn_far[i] = expf(-depth / 22.0f) * 0.18f;
    g_burn_roll[i] = expf(-depth / 3.0f) * 0.8f;
  }
}

inline int BurnStep(float depth) {
  if (depth <= 0.0f) return 0;
  const int i = static_cast<int>(depth * 4.0f);
  return i < kBurnSteps ? i : kBurnSteps - 1;
}

// A pixel this deep in the paper, or deeper, has the last step of the burn.
const float kDeep = 64.0f;

// --- the bake ----------------------------------------------------------------

// Start bakes the paper of each pixel of the tile, so a redraw does not mix
// the noise again. g_paper is kPaper times the blotch and the grain. g_deep
// is the opaque BGRA pixel at kDeep or more from each edge, where the burn
// is at its last step: one read gives the pixel that the full mix gives,
// bit for bit.
Rgb g_paper[kTile * kTile];
uint32_t g_deep[kTile * kTile];

inline int TileIndex(int x, int y) {
  return (y & kTileMask) * kTile + (x & kTileMask);
}

// An opaque pixel of the color, as Paper and Page write it.
inline void Opaque(uint8_t* out, const Rgb& c) {
  out[0] = static_cast<uint8_t>(c.b > 255.0f ? 255.0f : c.b);
  out[1] = static_cast<uint8_t>(c.g > 255.0f ? 255.0f : c.g);
  out[2] = static_cast<uint8_t>(c.r > 255.0f ? 255.0f : c.r);
  out[3] = 255;
}

void MakeBake() {
  const int last = kBurnSteps - 1;
  for (int i = 0; i < kTile * kTile; ++i) {
    const float k = (0.84f + 0.28f * g_blotch[i]) * (1.0f + g_grain[i]);
    g_paper[i] = Scale(kPaper, k);
    const float burn = g_burn_near[last] + g_burn_far[last] * g_mid[i];
    uint8_t pixel[4];
    Opaque(pixel, Mix(g_paper[i], kBurnt, Clamp01(burn)));
    memcpy(&g_deep[i], pixel, 4);
  }
}

// --- the fonts ---------------------------------------------------------------

// Start runs on the frame thread or on the thread of the warm, so one lock
// keeps the second call out until the first one is done.
SRWLOCK g_start_lock = SRWLOCK_INIT;
// The thread of the warm of the fonts (WarmStart), and whether it runs.
DWORD g_warm_thread = 0;
volatile LONG g_warm_started = 0;
// The jobs of the thread after the fonts (WarmAdd), and the event that the
// thread sets at its end. g_warm_over is 1 after a WarmWait saw that end.
const int kMostJobs = 4;
WarmJob g_jobs[kMostJobs] = {};
int g_job_count = 0;
HANDLE g_warm_done = nullptr;
volatile LONG g_warm_over = 0;
bool g_started = false;
bool g_ok = false;
double g_start_ms = 0.0;
wchar_t g_words_face[LF_FACESIZE] = L"";
wchar_t g_seal_face[LF_FACESIZE] = L"";
char g_note[256] = "";

// The font files, in the folder bwvr-fonts beside runblack.exe.
const char* const kFontFiles[] = {"Alegreya[wght].ttf", "AlegreyaSC-Bold.ttf"};
const int kFontCount = 2;

bool FontFolderFile(const char* dir, const char* name, char* out,
                    size_t size) {
  if (dir != nullptr && dir[0] != 0) {
    const size_t length = strlen(dir);
    const bool slash = dir[length - 1] == '\\' || dir[length - 1] == '/';
    return _snprintf_s(out, size, _TRUNCATE, "%s%s%s", dir, slash ? "" : "\\",
                       name) > 0;
  }
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return false;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr) return false;
  slash[1] = 0;
  return _snprintf_s(out, size, _TRUNCATE, "%sbwvr-fonts\\%s", path, name) > 0;
}

// True when GDI gives the face itself, not a font in its place.
bool HaveFace(HDC dc, const wchar_t* face) {
  HFONT font = CreateFontW(-40, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
  if (font == nullptr) return false;
  HGDIOBJ old = SelectObject(dc, font);
  wchar_t got[LF_FACESIZE] = L"";
  GetTextFaceW(dc, LF_FACESIZE, got);
  SelectObject(dc, old);
  DeleteObject(font);
  return _wcsicmp(got, face) == 0;
}

void Narrow(const wchar_t* in, char* out, size_t size) {
  size_t i = 0;
  for (; in[i] != 0 && i + 1 < size; ++i) {
    out[i] = in[i] < 128 ? static_cast<char>(in[i]) : '?';
  }
  out[i] = 0;
}

// A bold font of the face with a capital letter of cap pixels. The height of
// the capital H comes from its outline at a large size, so each face has the
// same letter height.
HFONT FontForCap(HDC dc, const wchar_t* face, int cap) {
  const int probe_em = 200;
  HFONT probe = CreateFontW(-probe_em, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                            ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
  int cap_units = 0;
  if (probe != nullptr) {
    HGDIOBJ old = SelectObject(dc, probe);
    const MAT2 identity = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
    GLYPHMETRICS metrics;
    if (GetGlyphOutlineW(dc, L'H', GGO_METRICS, &metrics, 0, nullptr,
                         &identity) != GDI_ERROR) {
      cap_units = static_cast<int>(metrics.gmBlackBoxY);
    }
    SelectObject(dc, old);
    DeleteObject(probe);
  }
  const int em = cap_units > 0 ? (probe_em * cap + cap_units / 2) / cap_units
                               : cap * 10 / 7;
  return CreateFontW(-em, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                     ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
}

void LoadFonts(HDC dc, const char* dir) {
  int missing = 0;
  char first_missing[MAX_PATH] = "";
  for (int i = 0; i < kFontCount; ++i) {
    char path[MAX_PATH];
    if (!FontFolderFile(dir, kFontFiles[i], path, sizeof(path))) continue;
    wchar_t wide[MAX_PATH];
    if (MultiByteToWideChar(CP_ACP, 0, path, -1, wide, MAX_PATH) == 0) continue;
    if (AddFontResourceExW(wide, FR_PRIVATE, nullptr) == 0) {
      if (missing++ == 0) strcpy_s(first_missing, path);
    }
  }
  const wchar_t* const words_faces[] = {L"Alegreya", L"Georgia", L"Segoe UI"};
  for (const wchar_t* face : words_faces) {
    if (HaveFace(dc, face)) {
      wcscpy_s(g_words_face, face);
      break;
    }
  }
  if (HaveFace(dc, L"Alegreya SC")) {
    wcscpy_s(g_seal_face, L"Alegreya SC");
  } else {
    wcscpy_s(g_seal_face, g_words_face);
  }
  char words[LF_FACESIZE], seal[LF_FACESIZE];
  Narrow(g_words_face, words, sizeof(words));
  Narrow(g_seal_face, seal, sizeof(seal));
  if (missing > 0) {
    _snprintf_s(g_note, _TRUNCATE,
                "the words use %s and the seals use %s. %d font file(s) did "
                "not load, the first is %s",
                words[0] != 0 ? words : "no font", seal[0] != 0 ? seal : "no font",
                missing, first_missing);
  } else {
    _snprintf_s(g_note, _TRUNCATE,
                "the words use %s and the seals use %s, from the folder %s",
                words, seal,
                dir != nullptr && dir[0] != 0 ? dir : "bwvr-fonts");
  }
}

}  // namespace

namespace {
bool StartLocked(HDC dc);
}  // namespace

bool Start(HDC dc) {
  AcquireSRWLockExclusive(&g_start_lock);
  const bool ok = StartLocked(dc);
  ReleaseSRWLockExclusive(&g_start_lock);
  return ok;
}

namespace {

bool StartLocked(HDC dc) {
  if (g_started) return g_ok;
  g_started = true;
  LARGE_INTEGER t0, t1, f;
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&t0);
  MakeTile();
  MakeBurn();
  MakeBake();
  char font_dir[MAX_PATH] = "";
  char ini[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, ini, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(ini, '\\') : nullptr;
  if (slash != nullptr && strlen(ini) + 9 < MAX_PATH) {
    strcpy_s(slash + 1, MAX_PATH - (slash + 1 - ini), "bwvr.ini");
    GetPrivateProfileStringA("bwvr", "hint_card_fonts_test_dir", "", font_dir,
                             sizeof(font_dir), ini);
  }
  LoadFonts(dc, font_dir);
  QueryPerformanceCounter(&t1);
  g_start_ms = (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart;
  g_ok = g_words_face[0] != 0;
  ModLog("card scroll  : %s. The noise tile and the fonts took %.1f ms on "
         "the %s thread",
         g_ok ? g_note : "GDI gave no font for the words", g_start_ms,
         g_warm_thread == GetCurrentThreadId() ? "warm" : "frame");
  return g_ok;
}

// --- the warm of the fonts ---------------------------------------------------

// The fonts of the cards, and the capital letter of each in pixels. Each cap
// comes from a constant of the card that uses it:
//   hint_card.cpp     words 31 (kScrollTextCap), seals 23 (kScrollSealCap)
//   radial_menu.cpp   seals 22 (kSealCap), 19 (kSealSwitchCap), words 17
//                     (kSealValueCap), 27 (kScrollNameCap), 19
//                     (kScrollLongCap), 20 (kScrollNoteCap)
//   vr_keyboard.cpp   words 30 (kInkCap), seals 19 (kWordCap), digits 30
//   spell_book.cpp    words 37, 25, 21, 17 and 14 (7/10 of the em 54, 36,
//                     30, 25 and 21)
//   places_panel.cpp  words 38, 26 and 22, seals 22, digits 30
//   debug_window.cpp  words 36, 24, 22, 21 and 19, seals 21
//   controls_page.cpp words 17 and 13, seals 12 (kCap of Fonts)
// A card that asks for a font that is not here gets a line in the log, so a
// change of a constant shows.
enum { kWarmWords = 0, kWarmSeal = 1, kWarmDigits = 2 };
const int kWarmWordCaps[] = {13, 14, 17, 19, 20, 21, 22, 24,
                             25, 26, 27, 30, 31, 36, 37, 38};
const int kWarmSealCaps[] = {12, 19, 21, 22, 23};
const int kWarmDigitCaps[] = {30};

// The letters of the cards: the printable ASCII, the letters of Latin-1, and
// the quotes, the dashes and the dots of the game's text.
const wchar_t* const kWarmText[] = {
    L" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNO",
    L"PQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
    L"\u00A0\u00A1\u00A2\u00A3\u00A9\u00AB\u00AE\u00B0\u00B4\u00B7\u00BB"
    L"\u00BF\u00C0\u00C1\u00C2\u00C4\u00C7\u00C8\u00C9\u00CA\u00CB\u00CD"
    L"\u00CE\u00D1\u00D3\u00D4\u00D6\u00D9\u00DA\u00DC\u00DF",
    L"\u00E0\u00E1\u00E2\u00E4\u00E7\u00E8\u00E9\u00EA\u00EB\u00EC\u00ED"
    L"\u00EE\u00EF\u00F1\u00F2\u00F3\u00F4\u00F6\u00F9\u00FA\u00FB\u00FC"
    L"\u2013\u2014\u2018\u2019\u201C\u201D\u2026"};

double g_warm_ms = 0.0;

bool Warmed(int kind, int cap) {
  const int* caps = kind == kWarmWords   ? kWarmWordCaps
                    : kind == kWarmSeal  ? kWarmSealCaps
                                         : kWarmDigitCaps;
  const int count =
      kind == kWarmWords  ? static_cast<int>(sizeof(kWarmWordCaps) / sizeof(int))
      : kind == kWarmSeal ? static_cast<int>(sizeof(kWarmSealCaps) / sizeof(int))
                          : static_cast<int>(sizeof(kWarmDigitCaps) / sizeof(int));
  for (int i = 0; i < count; ++i) {
    if (caps[i] == cap) return true;
  }
  return false;
}

// One line for each font that a card asks for and the warm did not draw.
void NoteCold(int kind, int cap) {
  static uint64_t said[3] = {0, 0, 0};
  if (InterlockedCompareExchange(&g_warm_started, 0, 0) == 0 || cap < 0 ||
      cap >= 64 || Warmed(kind, cap) || (said[kind] >> cap) & 1) {
    return;
  }
  said[kind] |= 1ull << cap;
  static const char* const kKinds[] = {"words", "seals", "digits"};
  ModLog("card scroll  : a card asks for the font of the %s with a capital "
         "of %d px, and the warm did not draw it. Its first draw makes each "
         "letter on the frame thread", kKinds[kind], cap);
}

// Draws each letter of the font once into the picture of dc, with DrawTextW
// as each card draws its words. A draw with TextOutW makes the letters, but
// the first DrawTextW of a font still costs 7 to 10 ms after it: DrawTextW
// goes the way of the text layout of Windows, and that way keeps its own
// data for each font and size.
void WarmFont(HDC dc, HFONT font) {
  if (font == nullptr) return;
  HGDIOBJ old = SelectObject(dc, font);
  for (const wchar_t* text : kWarmText) {
    RECT box = {2, 2, 2046, 126};
    DrawTextW(dc, text, -1, &box, DT_SINGLELINE | DT_NOPREFIX | DT_LEFT);
  }
  GdiFlush();
  SelectObject(dc, old);
  DeleteObject(font);
}

DWORD WINAPI WarmThread(void*) {
  LARGE_INTEGER t0, t1, f;
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&t0);
  HDC dc = CreateCompatibleDC(nullptr);
  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = 2048;
  info.bmiHeader.biHeight = -128;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib = dc != nullptr ? CreateDIBSection(dc, &info, DIB_RGB_COLORS,
                                                 &bits, nullptr, 0)
                              : nullptr;
  int fonts = 0;
  if (dib != nullptr) {
    HGDIOBJ old_bitmap = SelectObject(dc, dib);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    if (Start(dc)) {
      for (int cap : kWarmWordCaps) {
        WarmFont(dc, FontForCap(dc, g_words_face, cap));
        ++fonts;
      }
      for (int cap : kWarmSealCaps) {
        WarmFont(dc, FontForCap(dc, g_seal_face, cap));
        ++fonts;
      }
      for (int cap : kWarmDigitCaps) {
        WarmFont(dc, DigitsFont(dc, cap, nullptr));
        ++fonts;
      }
    }
    SelectObject(dc, old_bitmap);
    DeleteObject(dib);
  }
  if (dc != nullptr) DeleteDC(dc);
  QueryPerformanceCounter(&t1);
  g_warm_ms = (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart;
  ModLog("card scroll  : font_warm=1: a thread of the mod drew each letter of "
         "%d fonts of the cards one time in %.1f ms, with the noise tile and "
         "the load of the fonts. So the first draw of a card does not make "
         "the letters on the frame thread",
         fonts, g_warm_ms);
  // The jobs come after the fonts, so their letters are warm too.
  for (int i = 0; i < g_job_count; ++i) g_jobs[i]();
  SetEvent(g_warm_done);
  return 0;
}

}  // namespace

HFONT WordsFont(HDC dc, int cap) {
  NoteCold(kWarmWords, cap);
  return FontForCap(dc, g_words_face, cap);
}

HFONT SealFont(HDC dc, int cap) {
  NoteCold(kWarmSeal, cap);
  return FontForCap(dc, g_seal_face, cap);
}

void WarmStart() {
  if (InterlockedExchange(&g_warm_started, 1) != 0) return;
  char ini[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, ini, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(ini, '\\') : nullptr;
  int warm = 1, style = 1;
  if (slash != nullptr && strlen(ini) + 9 < MAX_PATH) {
    strcpy_s(slash + 1, MAX_PATH - (slash + 1 - ini), "bwvr.ini");
    warm = static_cast<int>(GetPrivateProfileIntA("bwvr", "font_warm", 1, ini));
    style = static_cast<int>(
        GetPrivateProfileIntA("bwvr", "hint_card_style", 1, ini));
  }
  if (warm == 0 || style == 0) {
    // With no warm, NoteCold says nothing.
    InterlockedExchange(&g_warm_started, 0);
    ModLog("card scroll  : font_warm=%d, hint_card_style=%d: no thread draws "
           "the letters of the fonts at the start. The first draw of each "
           "font of a card makes its letters on the frame thread",
           warm, style);
    return;
  }
  // The event is set before the thread exists, so a WarmWait of the frame
  // thread always finds it.
  g_warm_done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  HANDLE thread =
      g_warm_done != nullptr
          ? CreateThread(nullptr, 0, &WarmThread, nullptr, CREATE_SUSPENDED,
                         &g_warm_thread)
          : nullptr;
  if (thread == nullptr) {
    if (g_warm_done != nullptr) CloseHandle(g_warm_done);
    g_warm_done = nullptr;
    InterlockedExchange(&g_warm_started, 0);
    ModLog("card scroll  : font_warm=1, but Windows gave no thread for the "
           "warm of the fonts");
    return;
  }
  SetThreadPriority(thread, THREAD_PRIORITY_BELOW_NORMAL);
  ResumeThread(thread);
  CloseHandle(thread);
}

void WarmAdd(WarmJob job) {
  if (job == nullptr || g_job_count >= kMostJobs ||
      InterlockedCompareExchange(&g_warm_started, 0, 0) != 0) {
    return;
  }
  g_jobs[g_job_count++] = job;
}

double WarmWait() {
  if (InterlockedCompareExchange(&g_warm_over, 0, 0) != 0) return 0.0;
  if (g_warm_done == nullptr || GetCurrentThreadId() == g_warm_thread) {
    return 0.0;
  }
  if (WaitForSingleObject(g_warm_done, 0) == WAIT_OBJECT_0) {
    InterlockedExchange(&g_warm_over, 1);
    return 0.0;
  }
  LARGE_INTEGER t0, t1, f;
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&t0);
  WaitForSingleObject(g_warm_done, INFINITE);
  QueryPerformanceCounter(&t1);
  InterlockedExchange(&g_warm_over, 1);
  const double ms = (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart;
  ModLog("card scroll  : a panel waited %.1f ms on the frame thread for the "
         "end of the warm", ms);
  return ms;
}

HFONT DigitsFont(HDC dc, int cap, const char** face) {
  if (GetCurrentThreadId() != g_warm_thread) NoteCold(kWarmDigits, cap);
  // Alegreya and Alegreya SC have old style digits only, and GDI cannot ask
  // for the lining digits of a font. These faces of Windows have lining
  // digits, and their zero is narrower than their capital O.
  const wchar_t* const faces[] = {L"Palatino Linotype", L"Book Antiqua",
                                  L"Cambria"};
  const char* const names[] = {"Palatino Linotype", "Book Antiqua",
                               "Cambria"};
  for (int i = 0; i < 3; ++i) {
    if (HaveFace(dc, faces[i])) {
      if (face != nullptr) *face = names[i];
      return FontForCap(dc, faces[i], cap);
    }
  }
  if (face != nullptr) *face = "the font of the words";
  return FontForCap(dc, g_words_face, cap);
}

const char* FontNote() { return g_note; }

double StartMs() { return g_start_ms; }

void Paper(const Picture& picture, int x0, int y0, int x1, int y1, int seed) {
  const int w = picture.width;
  const int h = picture.height;
  for (int y = 0; y < h; ++y) {
    memset(picture.bgra + static_cast<size_t>(y) * picture.pitch * 4, 0,
           static_cast<size_t>(w) * 4);
  }
  const int kMaxWidth = 2048;
  if (w > kMaxWidth || w <= 0 || h <= 0) return;
  const int ox = seed * 71, oy = seed * 53;
  const float rw = static_cast<float>(kRollWidth);
  const float l0 = static_cast<float>(x0), l1 = l0 + rw;
  const float r1 = static_cast<float>(x1), r0 = r1 - rw;
  const float body_l = l0 + 0.5f * rw, body_r = r1 - 0.5f * rw;
  const float top = static_cast<float>(y0), bottom = static_cast<float>(y1);
  const float roll_top = top - kRollOver, roll_bottom = bottom + kRollOver;
  const float roll_radius = 0.5f * rw;

  // For each column: the torn top and bottom edges, the shade of the paper
  // near a roll, and the shade of the cylinder of a roll.
  static float edge_top[kMaxWidth], edge_bottom[kMaxWidth];
  static float side[kMaxWidth], roll_shade[kMaxWidth];
  static bool side_none[kMaxWidth];  // the shade near a roll is 1 exactly
  for (int x = 0; x < w; ++x) {
    const float px = x + 0.5f;
    edge_top[x] = top + 5.2f * (Tile(g_mid, x + ox, 7 + oy) - 0.5f) +
                  1.4f * (Tile(g_fine, x + ox, 29 + oy) - 0.5f);
    edge_bottom[x] = bottom + 5.2f * (Tile(g_mid, x + ox, 101 + oy) - 0.5f) +
                     1.4f * (Tile(g_fine, x + ox, 131 + oy) - 0.5f);
    float e = px - l1 < r0 - px ? px - l1 : r0 - px;
    if (e < 0.0f) e = 0.0f;
    side[x] = 1.0f - 0.38f * expf(-e / 6.0f);
    side_none[x] = side[x] == 1.0f;
    const bool left = px < 0.5f * (l0 + r1);
    const float t = Clamp01(left ? (px - l0) / rw : (px - r0) / rw);
    const float cylinder = 0.5f + 0.5f * sinf(3.14159265f * t);
    const float wrap = left ? t - 0.78f : t - 0.22f;
    roll_shade[x] = (0.42f + 0.62f * cylinder + 0.22f * Gauss(t - 0.33f, 0.09f)) *
                    (1.0f - 0.35f * Gauss(wrap, 0.035f));
  }

  // The drop shadow: the outline of the shape, moved, with a soft edge of
  // 5 px each way. It shows only where the paper does not cover.
  const float shadow_dx = 3.0f, shadow_dy = 5.0f, shadow_soft = 5.0f;
  const float shadow_alpha = 0.5f;

  for (int y = 0; y < h; ++y) {
    const float py = y + 0.5f;
    uint8_t* out = picture.bgra + static_cast<size_t>(y) * picture.pitch * 4;
    for (int x = 0; x < w; ++x, out += 4) {
      const float px = x + 0.5f;
      float cover_body = 0.0f;
      float body_d = 1.0f;
      if (px > body_l - 1.0f && px < body_r + 1.0f &&
          py > edge_top[x] - 1.0f && py < edge_bottom[x] + 1.0f) {
        const float dv = edge_top[x] - py > py - edge_bottom[x]
                             ? edge_top[x] - py
                             : py - edge_bottom[x];
        const float dh = body_l - px > px - body_r ? body_l - px : px - body_r;
        body_d = dv > dh ? dv : dh;
        cover_body = Clamp01(0.5f - body_d);
        // Deep in the body, away from the rolls and their shade: the baked
        // pixel.
        if (body_d <= -kDeep && side_none[x]) {
          memcpy(out, &g_deep[TileIndex(x + ox, y + oy)], 4);
          continue;
        }
      }
      float roll_d = 1e6f;
      const bool in_left = px < l1 + 1.0f;
      const bool in_right = px > r0 - 1.0f;
      if ((in_left || in_right) && py > roll_top - 1.0f &&
          py < roll_bottom + 1.0f) {
        roll_d = in_left ? RoundRect(px, py, l0, roll_top, l1, roll_bottom,
                                     roll_radius)
                         : RoundRect(px, py, r0, roll_top, r1, roll_bottom,
                                     roll_radius);
      }
      const float cover_roll = Clamp01(0.5f - roll_d);
      if (cover_body < 1.0f && cover_roll < 1.0f) {
        const float sx = px - shadow_dx, sy = py - shadow_dy;
        float d = RoundRect(sx, sy, body_l, top, body_r, bottom, 2.0f);
        const float dl = RoundRect(sx, sy, l0, roll_top, l1, roll_bottom,
                                   roll_radius);
        const float dr = RoundRect(sx, sy, r0, roll_top, r1, roll_bottom,
                                   roll_radius);
        if (dl < d) d = dl;
        if (dr < d) d = dr;
        if (d < shadow_soft) {
          Over(out, kBlack,
               shadow_alpha * Smooth(-shadow_soft, shadow_soft, -d));
        }
      }
      if (cover_body > 0.0f) {
        const int t = TileIndex(x + ox, y + oy);
        const float depth = -body_d;
        const int step = BurnStep(depth);
        const float burn = g_burn_near[step] + g_burn_far[step] * g_mid[t];
        Rgb color = Mix(g_paper[t], kBurnt, Clamp01(burn));
        color = Scale(color, side[x]);
        if (cover_body >= 1.0f) {
          // Most pixels: opaque paper, so no blend.
          Opaque(out, color);
        } else {
          Over(out, color, cover_body);
        }
      }
      if (cover_roll > 0.0f) {
        const float grain =
            (Tile(g_fine, x + ox, (y + oy) / 4) - 0.5f) * 0.12f;
        Rgb color = Scale(kRoll, roll_shade[x] * (1.0f + grain));
        color = Mix(color, kBurnt, Clamp01(g_burn_roll[BurnStep(-roll_d)]));
        // The hollow ends of the roll.
        const float cx = in_left ? 0.5f * (l0 + l1) : 0.5f * (r0 + r1);
        const float ex = (px - cx) / (rw * 0.30f);
        const float ends[2] = {roll_top + 6.0f, roll_bottom - 6.0f};
        for (float ey : ends) {
          const float ez = (py - ey) / 3.2f;
          const float r = sqrtf(ex * ex + ez * ez);
          if (r < 1.4f) color = Mix(color, kRollHollow, Clamp01(1.4f - r) * 0.9f);
        }
        Over(out, color, cover_roll);
      }
    }
  }
}

void Page(const Picture& picture, int x0, int y0, int x1, int y1, int seed) {
  const int w = picture.width;
  const int h = picture.height;
  for (int y = 0; y < h; ++y) {
    memset(picture.bgra + static_cast<size_t>(y) * picture.pitch * 4, 0,
           static_cast<size_t>(w) * 4);
  }
  const int kMaxSide = 2048;
  if (w > kMaxSide || h > kMaxSide || w <= 0 || h <= 0) return;
  const int ox = seed * 71, oy = seed * 53;
  // The torn edges: the top and the bottom for each column, the left and
  // the right for each row.
  static float edge_top[kMaxSide], edge_bottom[kMaxSide];
  static float edge_left[kMaxSide], edge_right[kMaxSide];
  for (int x = 0; x < w; ++x) {
    edge_top[x] = y0 + 5.2f * (Tile(g_mid, x + ox, 7 + oy) - 0.5f) +
                  1.4f * (Tile(g_fine, x + ox, 29 + oy) - 0.5f);
    edge_bottom[x] = y1 + 5.2f * (Tile(g_mid, x + ox, 101 + oy) - 0.5f) +
                     1.4f * (Tile(g_fine, x + ox, 131 + oy) - 0.5f);
  }
  for (int y = 0; y < h; ++y) {
    edge_left[y] = x0 + 5.2f * (Tile(g_mid, 61 + ox, y + oy) - 0.5f) +
                   1.4f * (Tile(g_fine, 83 + ox, y + oy) - 0.5f);
    edge_right[y] = x1 + 5.2f * (Tile(g_mid, 157 + ox, y + oy) - 0.5f) +
                    1.4f * (Tile(g_fine, 179 + ox, y + oy) - 0.5f);
  }
  for (int y = 0; y < h; ++y) {
    const float py = y + 0.5f;
    uint8_t* out = picture.bgra + static_cast<size_t>(y) * picture.pitch * 4;
    for (int x = 0; x < w; ++x, out += 4) {
      const float px = x + 0.5f;
      float d = edge_top[x] - py;
      if (py - edge_bottom[x] > d) d = py - edge_bottom[x];
      if (edge_left[y] - px > d) d = edge_left[y] - px;
      if (px - edge_right[y] > d) d = px - edge_right[y];
      const int t = TileIndex(x + ox, y + oy);
      if (d <= -kDeep) {
        memcpy(out, &g_deep[t], 4);
        continue;
      }
      const float cover = Clamp01(0.5f - d);
      if (cover <= 0.0f) continue;
      const int step = BurnStep(-d);
      const float burn = g_burn_near[step] + g_burn_far[step] * g_mid[t];
      const Rgb color = Mix(g_paper[t], kBurnt, Clamp01(burn));
      if (cover >= 1.0f) {
        Opaque(out, color);
      } else {
        Over(out, color, cover);
      }
    }
  }
}

void Seal(const Picture& picture, int x0, int y0, int x1, int y1, int tone,
          int seed) {
  // The distance to the uneven edge of the wax, for each pixel of the box and
  // a border, so the shade and the shadow read their neighbors from it.
  const int kBorder = 9;
  const int kMaxW = 800, kMaxH = 120;
  const int gx0 = x0 - kBorder, gy0 = y0 - kBorder;
  const int gw = (x1 - x0) + 2 * kBorder, gh = (y1 - y0) + 2 * kBorder;
  if (gw <= 0 || gh <= 0 || gw > kMaxW || gh > kMaxH) return;
  static float grid[kMaxW * kMaxH];
  const float fx0 = static_cast<float>(x0), fy0 = static_cast<float>(y0);
  const float fx1 = static_cast<float>(x1), fy1 = static_cast<float>(y1);
  const float radius = 0.5f * (fy1 - fy0);
  const int ox = seed * 37 + 91, oy = seed * 29 + 17;
  for (int j = 0; j < gh; ++j) {
    for (int i = 0; i < gw; ++i) {
      const int x = gx0 + i, y = gy0 + j;
      grid[j * gw + i] =
          RoundRect(x + 0.5f, y + 0.5f, fx0, fy0, fx1, fy1, radius) +
          6.8f * (Tile(g_mid, x + ox, y + oy) - 0.5f) +
          2.4f * (Tile(g_fine, x + ox, y + oy) - 0.5f);
    }
  }
  const auto at = [&](int i, int j) {
    if (i < 0) i = 0;
    if (j < 0) j = 0;
    if (i >= gw) i = gw - 1;
    if (j >= gh) j = gh - 1;
    return grid[j * gw + i];
  };
  const Rgb& wax = kWax[tone & 3];
  const float light_x = -0.6f, light_y = -0.8f;
  for (int j = 1; j < gh - 1; ++j) {
    const int y = gy0 + j;
    if (y < 0 || y >= picture.height) continue;
    for (int i = 1; i < gw - 1; ++i) {
      const int x = gx0 + i;
      if (x < 0 || x >= picture.width) continue;
      uint8_t* out =
          picture.bgra + (static_cast<size_t>(y) * picture.pitch + x) * 4;
      // The small shadow of the seal: its outline 2 px right and 3 px down.
      const float ds = at(i - 2, j - 3);
      if (ds < 2.5f) Over(out, kBlack, 0.55f * Smooth(-2.5f, 2.5f, -ds));
      const float d = grid[j * gw + i];
      const float cover = Clamp01(0.5f - d);
      if (cover <= 0.0f) continue;
      float nx = at(i + 1, j) - at(i - 1, j);
      float ny = at(i, j + 1) - at(i, j - 1);
      const float length = sqrtf(nx * nx + ny * ny) + 1e-4f;
      nx /= length;
      ny /= length;
      const float t = -d;
      const float lit = nx * light_x + ny * light_y;
      float shade = 0.92f + 0.5f * lit * (1.0f - Smooth(0.0f, 7.0f, t)) -
                    0.25f * (1.0f - Smooth(0.0f, 2.5f, t));
      if (fabsf(t - 8.5f) < 4.5f) shade -= 0.45f * Gauss(t - 8.5f, 1.1f) * lit;
      shade += 0.06f * (Tile(g_fine, x + ox + 50, y + oy) - 0.5f);
      Rgb color = Scale(wax, shade);
      const float hx = (x + 0.5f - (fx0 + radius * 0.9f)) / (radius * 0.7f);
      const float hy = (y + 0.5f - (fy0 + radius * 0.55f)) / (radius * 0.28f);
      const float sheen = Clamp01(1.0f - sqrtf(hx * hx + hy * hy));
      color = Mix(color, kSheen, sheen * 0.32f);
      Over(out, color, cover);
    }
  }
}

void Ink(const Picture& picture, const uint32_t* letters, int x0, int y0,
         int x1, int y1, int seed) {
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > picture.width) x1 = picture.width;
  if (y1 > picture.height) y1 = picture.height;
  const int pitch = picture.pitch;
  const int kMaxRows = 1024, kMaxColumns = 2048;
  if (x1 - x0 > kMaxColumns - 2 || y1 > kMaxRows || x1 <= x0 || y1 <= y0) {
    return;
  }
  // A row with no letter, and with no letter above or below it, has no ink.
  static bool has[kMaxRows + 1];
  for (int y = y0 > 0 ? y0 - 1 : 0; y <= y1 && y < picture.height; ++y) {
    const uint32_t* row = letters + y * pitch;
    bool any = false;
    for (int x = x0; x < x1 && !any; ++x) any = (row[x] & 0xFF00) != 0;
    has[y] = any;
  }
  const auto row_has = [&](int y) {
    return y >= 0 && y < picture.height && y <= y1 && has[y];
  };
  // The ink bleeds 1 px into the paper: the mean of the 3 by 3 pixels, from
  // the sum of each column of 3 pixels.
  static int column[kMaxColumns];
  const int ox = seed * 43, oy = seed * 61;
  for (int y = y0; y < y1; ++y) {
    if (!row_has(y - 1) && !row_has(y) && !row_has(y + 1)) continue;
    const int left = x0 > 0 ? x0 - 1 : x0;
    const int right = x1 < picture.width ? x1 + 1 : x1;
    for (int x = left; x < right; ++x) {
      int sum = 0;
      for (int j = y - 1; j <= y + 1; ++j) {
        if (row_has(j)) sum += (letters[j * pitch + x] >> 8) & 0xFF;
      }
      column[x - left] = sum;
    }
    for (int x = x0; x < x1; ++x) {
      const int c = x - left;
      const int sum = column[c] + (c > 0 ? column[c - 1] : 0) +
                      (x + 1 < right ? column[c + 1] : 0);
      if (sum == 0) continue;
      uint8_t* out = picture.bgra + (static_cast<size_t>(y) * pitch + x) * 4;
      const float bleed = sum * (1.0f / (9.0f * 255.0f));
      const float letter =
          ((letters[y * pitch + x] >> 8) & 0xFF) * (1.0f / 255.0f);
      const float strength = 0.9f + 0.1f * Tile(g_mid, x + ox, y + oy);
      Over(out, kInkBleed, bleed * 0.22f);
      Over(out, kInk, letter * strength);
    }
  }
}

void SealInk(const Picture& picture, const uint32_t* letters, int x0, int y0,
             int x1, int y1, int tone) {
  if (x0 < 1) x0 = 1;
  if (y0 < 1) y0 = 1;
  if (x1 > picture.width) x1 = picture.width;
  if (y1 > picture.height) y1 = picture.height;
  const int pitch = picture.pitch;
  // The amber wax has dark letters with a light edge under them. The other
  // waxes have light letters with a dark edge.
  const bool dark_letters = (tone & 3) == 2;
  const Rgb edge = dark_letters ? Rgb{255.0f, 230.0f, 160.0f}
                                : Rgb{30.0f, 10.0f, 5.0f};
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const float letter =
          ((letters[y * pitch + x] >> 8) & 0xFF) * (1.0f / 255.0f);
      const float under =
          ((letters[(y - 1) * pitch + x - 1] >> 8) & 0xFF) * (1.0f / 255.0f);
      if (letter <= 0.0f && under <= 0.0f) continue;
      uint8_t* out = picture.bgra + (static_cast<size_t>(y) * pitch + x) * 4;
      Over(out, edge, under * 0.55f);
      Over(out, kWaxInk[tone & 3], letter);
    }
  }
}

void Key(const Picture& picture, int x0, int y0, int x1, int y1, int look,
         int seed) {
  // The face, the frame of ink and the rim for each look. The rim of the
  // plain key is the bleed of its ink.
  const Rgb kFace[kKeyLooks] = {
      {0.0f, 0.0f, 0.0f}, {224.0f, 146.0f, 28.0f}, {255.0f, 228.0f, 96.0f}};
  const float kFaceAlpha[kKeyLooks] = {0.07f, 0.72f, 0.92f};
  const Rgb kFrame[kKeyLooks] = {
      kInk, {150.0f, 88.0f, 10.0f}, {196.0f, 128.0f, 24.0f}};
  const float kFrameAlpha[kKeyLooks] = {0.85f, 0.95f, 0.95f};
  const Rgb kRim[kKeyLooks] = {
      kInkBleed, {255.0f, 204.0f, 72.0f}, {255.0f, 248.0f, 200.0f}};
  const float kRimAlpha[kKeyLooks] = {0.2f, 0.95f, 1.0f};
  if (look < 0 || look >= kKeyLooks) look = kKeyPlain;
  const float fx0 = static_cast<float>(x0), fy0 = static_cast<float>(y0);
  const float fx1 = static_cast<float>(x1), fy1 = static_cast<float>(y1);
  const float cx = 0.5f * (fx0 + fx1), cy = 0.5f * (fy0 + fy1);
  const float radius = 9.0f;
  const float half_x = 0.5f * (fx1 - fx0) - radius;
  const float half_y = 0.5f * (fy1 - fy0) - radius;
  const float light_x = -0.6f, light_y = -0.8f;
  const float frame = 3.0f;  // the width of the frame of ink
  const float rim = look == kKeyPlain ? 1.5f : static_cast<float>(kKeyRim);
  const int ox = seed * 37 + 11, oy = seed * 23 + 5;
  for (int y = y0 - kKeyRim; y < y1 + kKeyRim; ++y) {
    if (y < 0 || y >= picture.height) continue;
    const float py = y + 0.5f;
    for (int x = x0 - kKeyRim; x < x1 + kKeyRim; ++x) {
      if (x < 0 || x >= picture.width) continue;
      const float px = x + 0.5f;
      // The distance to the edge of the key, and the direction out of the
      // key at the nearest point of the edge.
      const float sx = px < cx ? -1.0f : 1.0f;
      const float sy = py < cy ? -1.0f : 1.0f;
      const float qx = fabsf(px - cx) - half_x;
      const float qy = fabsf(py - cy) - half_y;
      float d, nx, ny;
      if (qx > 0.0f && qy > 0.0f) {
        const float length = sqrtf(qx * qx + qy * qy);
        d = length - radius;
        nx = sx * qx / length;
        ny = sy * qy / length;
      } else if (qx > qy) {
        d = qx - radius;
        nx = sx;
        ny = 0.0f;
      } else {
        d = qy - radius;
        nx = 0.0f;
        ny = sy;
      }
      // The edge of the ink is not straight.
      d += 1.0f * (Tile(g_fine, x + ox, y + oy) - 0.5f);
      uint8_t* out =
          picture.bgra + (static_cast<size_t>(y) * picture.pitch + x) * 4;
      if (d < 0.5f) {
        // The face, then its wall: the face is lower than the paper, so the
        // wall at the top and the left is in shade and the wall at the
        // bottom and the right has the light.
        const float inside = Clamp01(0.5f - d);
        Over(out, kFace[look], kFaceAlpha[look] * inside);
        const float wall = 1.0f - Smooth(0.0f, 6.0f, -d);
        const float lit = -(nx * light_x + ny * light_y);
        if (lit < 0.0f) {
          Over(out, kBlack, 0.32f * wall * -lit * inside);
        } else {
          Over(out, kSheen, 0.28f * wall * lit * inside);
        }
      }
      if (look != kKeyPlain || d > -0.5f) {
        const float glow = Clamp01(rim + 0.3f - d) * Clamp01(d + frame);
        Over(out, kRim[look], kRimAlpha[look] * glow);
      }
      const float ink = Clamp01(0.5f - d) * Clamp01(d + frame + 0.5f);
      const float strength = 0.9f + 0.1f * Tile(g_mid, x + ox, y + oy);
      Over(out, kFrame[look], kFrameAlpha[look] * ink * strength);
    }
  }
}

}  // namespace card_scroll
