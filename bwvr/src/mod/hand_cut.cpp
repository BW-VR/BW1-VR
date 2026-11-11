// A hand in front of a card. See hand_cut.h and docs/ui-layer.md, "A hand
// in front of a card".
//
// WHY. The spell book and the ring are the quad layer kCardRing, and the
// runtime draws a quad layer over the eye pictures. The hands of the mod are
// in the eye pictures. So the page of the book hides the hand that picks an
// item, also when the hand is in front of the page, and the ring hides the
// hand that moves over it.
//
// WHAT. Once a frame, after the hands are built (hand_model.cpp skins each
// mesh on the CPU in the world), for each eye: each triangle of each hand
// that faces the eye is cut to its part in front of the plane of the page
// (hand_cut_math.h), and that part goes onto the page along the lines from
// the eye, as the eye picture of that eye has it (the declared view of the
// projection layer). The page gets a hole there, so the eye sees the hand
// of its eye picture. The two eyes see the hand at two places of the page,
// so the card goes out as two quads in one place: kCardRing for the left
// eye and kCardRingRight for the right eye, each with its own holes. A part
// of the hand behind the page makes no hole, so the page stays over it. The
// text of the page stays sharp: it is still a quad layer. When no hand is in
// front of the page, the card goes out as one quad for both eyes.
//
// A SOLID HAND (card_hand_solid=1). A hand goes faint while the controller
// of a hint shows in it (hint_hand_opacity_pct). While a hand is in front of
// the page, it has its full opacity (HandCutSolid, overlay.cpp), so the
// player sees a solid hand over the page. The texture of the game still
// fades the wrist and the forearm with its alpha.
//
// ONLY WHERE THE HAND IS OPAQUE. A triangle with a vertex below kOpaque (the
// alpha of the texture at the vertex times the opacity of the hand, as the
// draw has it) makes no hole: there the page stays, behind the clear part.
//
// THE COST. The mask has half of the pixels of the card each way, and the
// cut scales it up with a soft edge of one pixel of the mask. Only the
// triangles that face the eye fill it, by spans of rows. Only the box of the
// holes of the last frame is cleared. The picture of the owner keeps its
// pixels: this file keeps a copy of the box that it cut, and puts it back in
// the next frame while the owner gives the same picture (the same pixels and
// the same serial). When the eyes, the page and the hands did not move more
// than kStill, the cut of the last frame stays, with no work.
//
// THE KEYS of bwvr.ini:
//
//   card_hand_front=1   0: the page of the spell book and of the ring is
//                       over the hands, also where a hand is in front of it
//   card_hand_solid=1   0: a hand in front of the page keeps the opacity of
//                       the hint
//   hand_cut_test_save=0  N: saves the pictures of both eyes of the first N
//                       frames with a cut, at most one each 30 frames, as
//                       bwvr-handcut-left-<frame>.bmp and
//                       bwvr-handcut-right-<frame>.bmp (a test key)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "controller_math.h"
#include "controllers.h"
#include "hand_cut.h"
#include "hand_cut_math.h"
#include "hand_model.h"

extern void ModLog(const char* format, ...);

uint64_t ControllersFrameCount();
// eye_capture.cpp
void EyeCaptureSaveUi(const char* name, uint64_t frame, int style,
                      const void* pixels, int pitch, uint32_t width,
                      uint32_t height);
void ControllersGrabPulse(int hands, float amplitude, float seconds);
// stereo.cpp: the view that the eye picture of an eye declares this frame.
bool StereoDeclaredView(int eye, float position[3], float orientation[4],
                        float fov[4], float size[2]);

namespace {

namespace hc = hand_cut_math;

// A vertex below this opacity (of 255) makes its triangles clear: about
// half opaque.
const uint8_t kOpaque = 128;
// The cut of the last frame stays when nothing moved more than this, in
// meters (about a pixel of the card at the distance of the book).
const float kStill = 0.0003f;

bool g_read = false;
bool g_on = true;
bool g_solid = true;
int g_save = 0;
int g_saved = 0;
uint64_t g_saved_frame = 0;

struct Offer {
  uint64_t frame;
  bwvr::CardLayer layer;
  uint8_t* right;
  const char* who;
};
Offer g_offer = {};

// THE MASKS of the two eyes, at half of the pixels of the card each way,
// one byte a pixel, and the box of each in pixels of the mask. The mask is
// 0 outside its box.
const int kMostSide = 1024;
const int kHalfSide = kMostSide / 2;
uint8_t g_mask[2][kHalfSide * kHalfSide];
uint8_t g_grown[kHalfSide * kHalfSide];
int g_box[2][4] = {};
int g_mask_pitch = 0;
// The box of each cut in pixels of the card.
int g_cut_box[2][4] = {};

// The picture of the owner that this file cut: its pixels and serial, the
// box of the left cut and the clean pixels of that box (at their own place
// in a picture of the size of the texture). The right picture differs from
// the clean picture of the owner only in g_right_box.
const uint8_t* g_last_pixels = nullptr;
uint32_t g_last_serial = 0;
int g_last_pitch = 0;
bool g_left_cut = false;
int g_left_box[4] = {};
uint8_t g_clean[kMostSide * kMostSide * 4];
const uint8_t* g_right_for = nullptr;
int g_right_box[4] = {};
uint32_t g_left_serial = 0, g_right_serial = 0;

// What the last cut saw: the eyes, the page and some points of the hands.
struct Seen {
  bool valid;
  float values[40];
};
Seen g_seen = {};

// The hands that made a hole in the last cut (HandCutSolid).
bool g_hand_front[2] = {false, false};

// The log and the report.
bool g_split_before = false;
int g_logs = 0;
const int kMostLogs = 60;
uint64_t g_split_frames = 0, g_still_frames = 0;
uint64_t g_most_pixels = 0;
uint64_t g_timed = 0;
double g_sum_ms = 0.0, g_most_ms = 0.0;
double g_part_ms[4] = {0.0, 0.0, 0.0, 0.0};  // map, masks, copies, cut
bool g_said_no_view = false;
bool g_said_no_mesh = false;

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash =
      length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash != nullptr && strlen(path) + 9 < MAX_PATH) {
    slash[1] = 0;
    strcat_s(path, MAX_PATH, "bwvr.ini");
    g_on = GetPrivateProfileIntA("bwvr", "card_hand_front", 1, path) != 0;
    g_solid = GetPrivateProfileIntA("bwvr", "card_hand_solid", 1, path) != 0;
    g_save = GetPrivateProfileIntA("bwvr", "hand_cut_test_save", 0, path);
  }
  ModLog("hand cut     : card_hand_front=%d: %s. card_hand_solid=%d: %s",
         g_on ? 1 : 0,
         g_on ? "a hand in front of the page of the spell book or of the "
                "ring shows in front of it, in each eye, where the hand is "
                "opaque"
              : "the page of the spell book and of the ring is over the "
                "hands",
         g_solid ? 1 : 0,
         g_solid ? "a hand in front of the page has its full opacity"
                 : "a hand in front of the page keeps the opacity of the "
                   "hint");
}

double NowMs() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart) * 1000.0 /
         static_cast<double>(frequency.QuadPart);
}

// The map of this frame from the world to the LOCAL space of OpenXR, the
// inverse of ControllersLocalToWorld: local = inverse * (world - base).
bool WorldToLocalMap(float base[3], float inverse[9]) {
  const float zero[3] = {0.0f, 0.0f, 0.0f};
  float units = 0.0f;
  if (!ControllersLocalToWorld(zero, base, &units)) return false;
  float m[9];
  for (int c = 0; c < 3; ++c) {
    float axis[3] = {0.0f, 0.0f, 0.0f};
    axis[c] = 1.0f;
    float w[3];
    if (!ControllersLocalToWorld(axis, w, &units)) return false;
    for (int r = 0; r < 3; ++r) m[r * 3 + c] = w[r] - base[r];
  }
  const float det = m[0] * (m[4] * m[8] - m[5] * m[7]) -
                    m[1] * (m[3] * m[8] - m[5] * m[6]) +
                    m[2] * (m[3] * m[7] - m[4] * m[6]);
  if (!(fabsf(det) > 1e-12f)) return false;
  const float k = 1.0f / det;
  inverse[0] = (m[4] * m[8] - m[5] * m[7]) * k;
  inverse[1] = (m[2] * m[7] - m[1] * m[8]) * k;
  inverse[2] = (m[1] * m[5] - m[2] * m[4]) * k;
  inverse[3] = (m[5] * m[6] - m[3] * m[8]) * k;
  inverse[4] = (m[0] * m[8] - m[2] * m[6]) * k;
  inverse[5] = (m[2] * m[3] - m[0] * m[5]) * k;
  inverse[6] = (m[3] * m[7] - m[4] * m[6]) * k;
  inverse[7] = (m[1] * m[6] - m[0] * m[7]) * k;
  inverse[8] = (m[0] * m[4] - m[1] * m[3]) * k;
  return true;
}

// The vertices of both hands in the LOCAL space, once a frame.
const int kMostVertices = 4096;
float g_local[2][kMostVertices][3];

// The hands of this frame.
struct Hands {
  int triangles[2];
  int vertices[2];
  const uint32_t* index[2];
  const uint8_t* alpha[2];
};

// Copies the rows of a box from one picture to another of the same pitch.
void CopyBox(uint8_t* to, const uint8_t* from, int pitch_px, const int b[4]) {
  if (b[0] >= b[2] || b[1] >= b[3]) return;
  for (int y = b[1]; y < b[3]; ++y) {
    const size_t at = (static_cast<size_t>(y) * pitch_px + b[0]) * 4;
    memcpy(to + at, from + at, static_cast<size_t>(b[2] - b[0]) * 4);
  }
}

float Cross3Dot(const float a[3], const float b[3], const float c[3],
                const float eye[3]) {
  const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
  const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1],
                      e1[2] * e2[0] - e1[0] * e2[2],
                      e1[0] * e2[1] - e1[1] * e2[0]};
  return n[0] * (a[0] - eye[0]) + n[1] * (a[1] - eye[1]) +
         n[2] * (a[2] - eye[2]);
}

// The side of the triangles of a hand that face the eye: the side of the
// triangle nearest to the eye, which faces it. So the turn of the mesh and
// of the maps does not count.
float FrontSide(const Hands& hands, int hand, const float eye[3]) {
  float best = 1e30f, side = 0.0f;
  for (int t = 0; t < hands.triangles[hand]; ++t) {
    const uint32_t* i = hands.index[hand] + t * 3;
    if (i[0] >= static_cast<uint32_t>(hands.vertices[hand]) ||
        i[1] >= static_cast<uint32_t>(hands.vertices[hand]) ||
        i[2] >= static_cast<uint32_t>(hands.vertices[hand])) {
      continue;
    }
    const float* a = g_local[hand][i[0]];
    const float* b = g_local[hand][i[1]];
    const float* c = g_local[hand][i[2]];
    float d = 0.0f;
    for (int k = 0; k < 3; ++k) {
      const float m = (a[k] + b[k] + c[k]) * (1.0f / 3.0f) - eye[k];
      d += m * m;
    }
    if (d < best) {
      best = d;
      side = Cross3Dot(a, b, c, eye) < 0.0f ? -1.0f : 1.0f;
    }
  }
  return side;
}

// The mask of one eye: the opaque parts of both hands in front of the page
// that face the eye, on the page as the eye sees them, at half of the
// pixels each way, 1 pixel of the mask larger. Returns the count of pixels
// of the card that the holes cover.
uint64_t EyeMask(int eye, const float eye_at[3], const hc::Page& page,
                 const bwvr::CardLayer& layer, const Hands& hands) {
  const int hw = static_cast<int>(layer.texture_width) / 2;
  const int hh = static_cast<int>(layer.texture_height) / 2;
  uint8_t* mask = g_mask[eye];
  int* box = g_box[eye];
  if (g_mask_pitch == hw) {
    for (int y = box[1]; y < box[3]; ++y) {
      memset(mask + static_cast<size_t>(y) * hw + box[0], 0,
             static_cast<size_t>(box[2] - box[0]));
    }
  } else {
    memset(mask, 0, sizeof(g_mask[eye]));
  }
  box[0] = hw;
  box[1] = hh;
  box[2] = 0;
  box[3] = 0;
  const float ppm_x = 0.5f * layer.rect[2] / layer.width;
  const float ppm_y = 0.5f * layer.rect[3] / layer.height;
  const float mid_x = 0.5f * (layer.rect[0] + 0.5f * layer.rect[2]);
  const float mid_y = 0.5f * (layer.rect[1] + 0.5f * layer.rect[3]);
  for (int hand = 0; hand < 2; ++hand) {
    const float front = FrontSide(hands, hand, eye_at);
    for (int t = 0; t < hands.triangles[hand]; ++t) {
      const uint32_t* tri_index = hands.index[hand] + t * 3;
      bool ok = true;
      float tri[3][3];
      for (int k = 0; k < 3 && ok; ++k) {
        const uint32_t v = tri_index[k];
        ok = v < static_cast<uint32_t>(hands.vertices[hand]) &&
             hands.alpha[hand][v] >= kOpaque;
        if (ok) memcpy(tri[k], g_local[hand][v], sizeof(tri[k]));
      }
      if (!ok) continue;
      if (Cross3Dot(tri[0], tri[1], tri[2], eye_at) * front <= 0.0f) continue;
      float in_front[4][3];
      const int n = hc::ClipFront(tri, page, in_front);
      if (n == 0) continue;
      float px[4][2];
      bool on = true;
      for (int k = 0; k < n && on; ++k) {
        float x = 0.0f, y = 0.0f;
        on = hc::OntoPage(eye_at, page, in_front[k], &x, &y);
        px[k][0] = mid_x + x * ppm_x;
        px[k][1] = mid_y - y * ppm_y;
      }
      if (!on) continue;
      hc::FillConvex(px, n, mask, hw, hh, box);
      g_hand_front[hand] = true;
    }
  }
  if (box[0] >= box[2] || box[1] >= box[3]) {
    box[0] = box[1] = box[2] = box[3] = 0;
    return 0;
  }
  // The mask 1 pixel larger: the largest value of each row of 3, then of
  // each column of 3.
  int g[4] = {box[0] - 1, box[1] - 1, box[2] + 1, box[3] + 1};
  if (g[0] < 0) g[0] = 0;
  if (g[1] < 0) g[1] = 0;
  if (g[2] > hw) g[2] = hw;
  if (g[3] > hh) g[3] = hh;
  for (int y = g[1]; y < g[3]; ++y) {
    const uint8_t* in = mask + static_cast<size_t>(y) * hw;
    uint8_t* out = g_grown + static_cast<size_t>(y) * hw;
    for (int x = g[0]; x < g[2]; ++x) {
      uint8_t m = in[x];
      if (x > g[0] && in[x - 1] > m) m = in[x - 1];
      if (x + 1 < g[2] && in[x + 1] > m) m = in[x + 1];
      out[x] = m;
    }
  }
  uint64_t pixels = 0;
  for (int y = g[1]; y < g[3]; ++y) {
    uint8_t* out = mask + static_cast<size_t>(y) * hw;
    const uint8_t* mid = g_grown + static_cast<size_t>(y) * hw;
    const uint8_t* up = y > g[1] ? mid - hw : nullptr;
    const uint8_t* down = y + 1 < g[3] ? mid + hw : nullptr;
    for (int x = g[0]; x < g[2]; ++x) {
      uint8_t m = mid[x];
      if (up != nullptr && up[x] > m) m = up[x];
      if (down != nullptr && down[x] > m) m = down[x];
      out[x] = m;
      if (m > 0) ++pixels;
    }
  }
  for (int k = 0; k < 4; ++k) box[k] = g[k];
  // The box of the cut in pixels of the card, inside the rect of the card.
  int* c = g_cut_box[eye];
  c[0] = 2 * g[0];
  c[1] = 2 * g[1];
  c[2] = 2 * g[2];
  c[3] = 2 * g[3];
  if (c[0] < layer.rect[0]) c[0] = layer.rect[0];
  if (c[1] < layer.rect[1]) c[1] = layer.rect[1];
  if (c[2] > layer.rect[0] + layer.rect[2]) c[2] = layer.rect[0] + layer.rect[2];
  if (c[3] > layer.rect[1] + layer.rect[3]) c[3] = layer.rect[1] + layer.rect[3];
  if (c[0] >= c[2] || c[1] >= c[3]) return 0;
  return pixels * 4;
}

// The holes of one eye into its picture: the mask scaled up to the pixels
// of the card (each pixel of the card takes 3 quarters of its own pixel of
// the mask and 1 quarter of the next, each way), and each channel of the
// pixel times what is left.
void Cut(uint8_t* pixels, int eye, int w) {
  const int* c = g_cut_box[eye];
  const int* b = g_box[eye];
  const int hw = g_mask_pitch;
  const uint8_t* mask = g_mask[eye];
  static int row_value[kMostSide];
  for (int y = c[1]; y < c[3]; ++y) {
    // The two rows of the mask for this row of the card, and their weights
    // of 3 and 1.
    const int j = y >> 1;
    const int j2 = (y & 1) ? j + 1 : j - 1;
    const uint8_t* near_row = j >= b[1] && j < b[3]
                                  ? mask + static_cast<size_t>(j) * hw
                                  : nullptr;
    const uint8_t* far_row = j2 >= b[1] && j2 < b[3]
                                 ? mask + static_cast<size_t>(j2) * hw
                                 : nullptr;
    if (near_row == nullptr && far_row == nullptr) continue;
    // Each pixel of the mask row: 3 times the near row and 1 time the far
    // row, 0 to 1020.
    for (int i = b[0]; i < b[2]; ++i) {
      row_value[i] = (near_row != nullptr ? 3 * near_row[i] : 0) +
                     (far_row != nullptr ? far_row[i] : 0);
    }
    uint32_t* out = reinterpret_cast<uint32_t*>(pixels) +
                    static_cast<size_t>(y) * w;
    for (int x = c[0]; x < c[2]; ++x) {
      const int i = x >> 1;
      const int i2 = (x & 1) ? i + 1 : i - 1;
      const int v_near = i >= b[0] && i < b[2] ? row_value[i] : 0;
      const int v_far = i2 >= b[0] && i2 < b[2] ? row_value[i2] : 0;
      const int m = (3 * v_near + v_far + 8) >> 4;  // 0 to 255
      if (m == 0) continue;
      if (m >= 255) {
        out[x] = 0;
        continue;
      }
      const uint32_t keep = static_cast<uint32_t>(256 - m);
      const uint32_t p = out[x];
      const uint32_t rb = (((p & 0x00FF00FFu) * keep) >> 8) & 0x00FF00FFu;
      const uint32_t ag = (((p >> 8) & 0x00FF00FFu) * keep) & 0xFF00FF00u;
      out[x] = rb | ag;
    }
  }
}

// The values that the last cut saw, to find a frame in which nothing moved.
void SeenNow(const float eyes[2][3], const bwvr::CardLayer& layer,
             const Hands& hands, Seen* out) {
  int n = 0;
  for (int e = 0; e < 2; ++e) {
    for (int k = 0; k < 3; ++k) out->values[n++] = eyes[e][k];
  }
  for (int k = 0; k < 3; ++k) out->values[n++] = layer.position[k];
  for (int k = 0; k < 4; ++k) out->values[n++] = layer.orientation[k];
  for (int hand = 0; hand < 2; ++hand) {
    const int count = hands.triangles[hand] > 0 ? hands.vertices[hand] : 0;
    const int picks[4] = {0, count / 3, 2 * count / 3, count - 1};
    for (int p = 0; p < 4; ++p) {
      for (int k = 0; k < 3; ++k) {
        out->values[n++] = count > 0 ? g_local[hand][picks[p]][k] : 0.0f;
      }
    }
  }
  while (n < 40) out->values[n++] = 0.0f;
  out->valid = true;
}

bool SameSeen(const Seen& a, const Seen& b) {
  if (!a.valid || !b.valid) return false;
  for (int i = 0; i < 40; ++i) {
    if (fabsf(a.values[i] - b.values[i]) > kStill) return false;
  }
  return true;
}

void StageBoth(const bwvr::CardLayer& layer) {
  bwvr::CardLayer l = layer;
  l.eye = 1;
  l.serial = g_left_serial;
  bwvr::XrScreen::StageCard(bwvr::kCardRing, l);
  bwvr::CardLayer r = layer;
  r.pixels = g_offer.right;
  r.eye = 2;
  r.serial = g_right_serial;
  bwvr::XrScreen::StageCard(bwvr::kCardRingRight, r);
}

void Count(double start, const double part[4]) {
  ++g_timed;
  const double ms = NowMs() - start;
  g_sum_ms += ms;
  if (ms > g_most_ms) g_most_ms = ms;
  for (int k = 0; k < 4; ++k) g_part_ms[k] += part[k];
}

}  // namespace

void HandCutOffer(const bwvr::CardLayer& layer, uint8_t* right,
                  const char* who) {
  ReadSettings();
  if (!g_on || right == nullptr || layer.pixels == nullptr) return;
  g_offer.frame = ControllersFrameCount();
  g_offer.layer = layer;
  g_offer.right = right;
  g_offer.who = who;
}

bool HandCutSolid(int hand) {
  return g_on && g_solid && hand >= 0 && hand <= 1 && g_hand_front[hand];
}

void HandCutFrame() {
  ReadSettings();
  const uint64_t frame = ControllersFrameCount();
  const bool offered = g_on && g_offer.frame == frame && g_offer.frame != 0;
  if (!offered) {
    g_hand_front[0] = g_hand_front[1] = false;
    g_seen.valid = false;
    return;
  }
  g_offer.frame = 0;
  const bwvr::CardLayer& layer = g_offer.layer;
  if (bwvr::XrScreen::CardFailed(bwvr::kCardRingRight) ||
      layer.texture_width > static_cast<uint32_t>(kMostSide) ||
      layer.texture_height > static_cast<uint32_t>(kMostSide) ||
      !(layer.width > 0.0f) || !(layer.height > 0.0f)) {
    return;
  }
  const double start = NowMs();
  double part[4] = {0.0, 0.0, 0.0, 0.0};
  uint8_t* left = const_cast<uint8_t*>(layer.pixels);
  const int w = static_cast<int>(layer.texture_width);
  // The cut of a picture of the other owner (the book after the ring, or
  // the ring after the book) goes back first: that picture stays alive.
  if (g_left_cut && layer.pixels != g_last_pixels && g_last_pixels != nullptr) {
    CopyBox(const_cast<uint8_t*>(g_last_pixels), g_clean, g_last_pitch,
            g_left_box);
    g_left_cut = false;
  }
  g_last_pitch = w;
  const bool same = layer.pixels == g_last_pixels &&
                    layer.serial == g_last_serial;
  g_last_pixels = layer.pixels;
  g_last_serial = layer.serial;
  // A new picture of the owner is clean.
  if (!same) g_left_cut = false;

  if (!HandModelBuilt()) {
    if (!g_said_no_mesh) {
      g_said_no_mesh = true;
      ModLog("hand cut     : no mesh of a hand was built (the low poly hands), "
             "so no hand cuts the %s, frame %llu", g_offer.who, frame);
    }
    if (g_left_cut) CopyBox(left, g_clean, w, g_left_box);
    g_left_cut = false;
    return;
  }
  float eyes[2][3];
  for (int e = 0; e < 2; ++e) {
    float orientation[4], fov[4], size[2];
    if (!StereoDeclaredView(e, eyes[e], orientation, fov, size)) {
      if (!g_said_no_view) {
        g_said_no_view = true;
        ModLog("hand cut     : the eye pictures declare no view now, so no "
               "hand cuts the %s, frame %llu", g_offer.who, frame);
      }
      if (g_left_cut) CopyBox(left, g_clean, w, g_left_box);
      g_left_cut = false;
      return;
    }
  }
  float base[3], inverse[9];
  if (!WorldToLocalMap(base, inverse)) return;
  hc::Page page;
  for (int i = 0; i < 3; ++i) page.center[i] = layer.position[i];
  const float x[3] = {1.0f, 0.0f, 0.0f};
  const float y[3] = {0.0f, 1.0f, 0.0f};
  const float z[3] = {0.0f, 0.0f, 1.0f};
  controller_math::QuatRotate(layer.orientation, x, page.right);
  controller_math::QuatRotate(layer.orientation, y, page.up);
  controller_math::QuatRotate(layer.orientation, z, page.normal);

  // Both hands into the LOCAL space, one time for both eyes.
  Hands hands = {};
  for (int hand = 0; hand < 2; ++hand) {
    const float* points = nullptr;
    hands.triangles[hand] =
        HandModelWorldMesh(hand, &points, &hands.index[hand],
                           &hands.alpha[hand], &hands.vertices[hand]);
    if (hands.vertices[hand] > kMostVertices) hands.triangles[hand] = 0;
    for (int v = 0; v < hands.vertices[hand] && hands.triangles[hand] > 0;
         ++v) {
      const float* p = points + v * 3;
      const float d[3] = {p[0] - base[0], p[1] - base[1], p[2] - base[2]};
      for (int r = 0; r < 3; ++r) {
        g_local[hand][v][r] = inverse[r * 3 + 0] * d[0] +
                              inverse[r * 3 + 1] * d[1] +
                              inverse[r * 3 + 2] * d[2];
      }
    }
  }
  part[0] = NowMs() - start;

  // Nothing moved, and the owner gives the same picture: the cut of the last
  // frame stays.
  Seen seen = {};
  SeenNow(eyes, layer, hands, &seen);
  if (same && SameSeen(seen, g_seen)) {
    if (g_split_before) {
      StageBoth(layer);
      ++g_split_frames;
    }
    ++g_still_frames;
    Count(start, part);
    return;
  }
  g_seen = seen;
  // Put back the clean pixels of the box that the last frame cut.
  if (g_left_cut) CopyBox(left, g_clean, w, g_left_box);
  g_left_cut = false;

  double t = NowMs();
  g_hand_front[0] = g_hand_front[1] = false;
  uint64_t count[2];
  for (int e = 0; e < 2; ++e) {
    count[e] = EyeMask(e, eyes[e], page, layer, hands);
  }
  g_mask_pitch = w / 2;
  part[1] = NowMs() - t;
  const bool split = count[0] + count[1] > 0;
  if (split != g_split_before && g_logs < kMostLogs) {
    ++g_logs;
    if (split) {
      ModLog("hand cut     : frame %llu, a hand is in front of the page of "
             "the %s: %llu px of holes for the left eye and %llu px for the "
             "right eye. The card goes out as one quad for each eye",
             frame, g_offer.who, count[0], count[1]);
    } else {
      ModLog("hand cut     : frame %llu, no hand is in front of the page of "
             "the %s now. The card goes out as one quad for both eyes",
             frame, g_offer.who);
    }
  }
  g_split_before = split;
  if (!split) {
    Count(start, part);
    return;
  }

  // The right picture: the clean picture of the owner with the holes of the
  // right eye. Copied whole for a new picture, and else only where the
  // holes of the last frame were.
  t = NowMs();
  if (!same || g_right_for != g_offer.right) {
    const int all[4] = {layer.rect[0], layer.rect[1],
                        layer.rect[0] + layer.rect[2],
                        layer.rect[1] + layer.rect[3]};
    CopyBox(g_offer.right, left, w, all);
    g_right_for = g_offer.right;
  } else {
    CopyBox(g_offer.right, left, w, g_right_box);
  }
  for (int k = 0; k < 4; ++k) g_left_box[k] = g_cut_box[0][k];
  CopyBox(g_clean, left, w, g_left_box);
  part[2] = NowMs() - t;
  t = NowMs();
  if (count[0] > 0) {
    Cut(left, 0, w);
    g_left_cut = true;
  }
  for (int k = 0; k < 4; ++k) g_right_box[k] = g_cut_box[1][k];
  if (count[1] > 0) Cut(g_offer.right, 1, w);
  part[3] = NowMs() - t;
  g_left_serial = bwvr::XrScreen::NewCardSerial();
  g_right_serial = bwvr::XrScreen::NewCardSerial();
  StageBoth(layer);
  ++g_split_frames;
  Count(start, part);
  if (count[0] > g_most_pixels) g_most_pixels = count[0];
  if (count[1] > g_most_pixels) g_most_pixels = count[1];
  // The test pictures, after the time of the cut.
  if (g_saved < g_save && (g_saved == 0 || frame >= g_saved_frame + 30)) {
    ++g_saved;
    g_saved_frame = frame;
    const size_t at =
        (static_cast<size_t>(layer.rect[1]) * w + layer.rect[0]) * 4;
    EyeCaptureSaveUi("handcut-left", frame, 1, left + at, w * 4,
                     static_cast<uint32_t>(layer.rect[2]),
                     static_cast<uint32_t>(layer.rect[3]));
    EyeCaptureSaveUi("handcut-right", frame, 1, g_offer.right + at, w * 4,
                     static_cast<uint32_t>(layer.rect[2]),
                     static_cast<uint32_t>(layer.rect[3]));
    ModLog("hand cut     : frame %llu, the pictures of the %s with the holes "
           "of the hand are saved: %llu px for the left eye and %llu px for "
           "the right eye", frame, g_offer.who, count[0], count[1]);
  }
}

int HandCutValue() {
  ReadSettings();
  return g_on ? 1 : 0;
}

void HandCutChange(const char* by) {
  ReadSettings();
  g_on = !g_on;
  ControllersGrabPulse(1, 0.5f, g_on ? 0.40f : 0.08f);
  ModLog("hand cut     : %s: card_hand_front=%d. %s, frame %llu", by,
         g_on ? 1 : 0,
         g_on ? "A hand in front of the page shows in front of it"
              : "The page is over the hands",
         ControllersFrameCount());
}

int HandCutSolidValue() {
  ReadSettings();
  return g_solid ? 1 : 0;
}

void HandCutSolidChange(const char* by) {
  ReadSettings();
  g_solid = !g_solid;
  ControllersGrabPulse(1, 0.5f, g_solid ? 0.40f : 0.08f);
  ModLog("hand cut     : %s: card_hand_solid=%d. %s, frame %llu", by,
         g_solid ? 1 : 0,
         g_solid ? "A hand in front of the page has its full opacity"
                 : "A hand in front of the page keeps the opacity of the hint",
         ControllersFrameCount());
}

void HandCutReport() {
  if (g_timed == 0) return;
  const double n = static_cast<double>(g_timed);
  ModLog("hand cut     : this window the cut ran in %llu frames, %.3f ms on "
         "average and %.3f ms at the most (with no save of a test picture). "
         "On average: the hands into the room %.3f ms, the masks %.3f ms, "
         "the copies %.3f ms, the holes %.3f ms. In %llu frames nothing "
         "moved and the cut of the frame before stayed. A hand was in front "
         "of a page in %llu frames: the card went out as one quad for each "
         "eye. The largest hole of an eye had %llu px",
         g_timed, g_sum_ms / n, g_most_ms, g_part_ms[0] / n,
         g_part_ms[1] / n, g_part_ms[2] / n, g_part_ms[3] / n,
         g_still_frames, g_split_frames, g_most_pixels);
  g_timed = 0;
  g_sum_ms = 0.0;
  g_most_ms = 0.0;
  for (int k = 0; k < 4; ++k) g_part_ms[k] = 0.0;
  g_still_frames = 0;
  g_split_frames = 0;
  g_most_pixels = 0;
}
