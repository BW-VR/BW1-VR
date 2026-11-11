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
// mesh on the CPU in the world), for each eye: each triangle of each hand is
// cut to its part in front of the plane of the page (hand_cut_math.h), and
// that part goes onto the page along the lines from the eye, as the eye
// picture of that eye has it (the declared view of the projection layer).
// The page gets a hole there, so the eye sees the hand of its eye picture.
// The two eyes see the hand at two places of the page, so the card goes out
// as two quads in one place: kCardRing for the left eye and kCardRingRight
// for the right eye, each with its own holes. A part of the hand behind the
// page makes no hole, so the page stays over it. The text of the page stays
// sharp: it is still a quad layer. When no hand is in front of the page, the
// card goes out as one quad for both eyes, as before.
//
// ONLY WHERE THE HAND IS OPAQUE. The texture of the game fades the wrist
// and the forearm with its alpha, so the eye picture shows the land through
// them. A triangle with a vertex below kOpaque makes no hole: there the page
// stays, behind the clear part of the hand.
//
// THE COST. Each triangle fills its rows of samples by spans, so the cost
// goes with the pixels of the hand on the page. Only the box of the holes of
// the last frame is cleared and put back. The picture of the owner keeps its
// pixels: this file keeps a copy of the box that it cut, and puts it back in
// the next frame while the owner gives the same picture (the same pixels and
// the same serial). The right picture is copied whole only when the owner
// gives a new picture.
//
// The hole is 1 pixel larger than the hand, for the time between the draw
// and the display. The hands of the low poly model (hand_model=0) have no
// mesh here, so they make no hole.
//
// THE KEY of bwvr.ini:
//
//   card_hand_front=1   0: the page of the spell book and of the ring is
//                       over the hands, also where a hand is in front of it
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

// A vertex below this opacity (of 255) makes its triangles clear.
const uint8_t kOpaque = 153;

bool g_read = false;
bool g_on = true;
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

// The masks of the two eyes, one byte a pixel of the texture of the card,
// and the box of the holes of each eye in this frame. The mask is 0 outside
// that box.
const int kMostSide = 1024;
uint8_t g_mask[2][kMostSide * kMostSide];
uint8_t g_grown[kMostSide * kMostSide];
int g_box[2][4] = {};
int g_mask_pitch = 0;

// The picture of the owner that this file cut: its pixels and serial, the
// box of the cut, and the clean pixels of that box (at their own place in a
// picture of the size of the texture). The right picture differs from the
// clean picture of the owner only in g_right_box.
const uint8_t* g_last_pixels = nullptr;
uint32_t g_last_serial = 0;
bool g_left_cut = false;
int g_left_box[4] = {};
uint8_t g_clean[kMostSide * kMostSide * 4];
const uint8_t* g_right_for = nullptr;
int g_right_box[4] = {};

// The log and the report.
bool g_split_before = false;
int g_logs = 0;
const int kMostLogs = 60;
uint64_t g_split_frames = 0;
uint64_t g_most_pixels = 0;
uint64_t g_timed = 0;
double g_sum_ms = 0.0;
double g_most_ms = 0.0;
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
    g_save = GetPrivateProfileIntA("bwvr", "hand_cut_test_save", 0, path);
  }
  ModLog("hand cut     : card_hand_front=%d: %s", g_on ? 1 : 0,
         g_on ? "a hand in front of the page of the spell book or of the "
                "ring shows in front of it, in each eye, where the hand is "
                "opaque"
              : "the page of the spell book and of the ring is over the "
                "hands");
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

// Copies the rows of a box from one picture to another of the same pitch.
void CopyBox(uint8_t* to, const uint8_t* from, int pitch_px, const int b[4]) {
  if (b[0] >= b[2] || b[1] >= b[3]) return;
  for (int y = b[1]; y < b[3]; ++y) {
    const size_t at = (static_cast<size_t>(y) * pitch_px + b[0]) * 4;
    memcpy(to + at, from + at, static_cast<size_t>(b[2] - b[0]) * 4);
  }
}

// The mask of one eye: the opaque parts of both hands in front of the page,
// on the page as the eye sees them, 1 pixel larger. Returns the count of
// pixels.
uint64_t EyeMask(int eye, const float eye_at[3], const hc::Page& page,
                 const bwvr::CardLayer& layer, const int* triangles,
                 const uint32_t* const* index, const uint8_t* const* alpha,
                 const int* vertices) {
  const int w = static_cast<int>(layer.texture_width);
  const int h = static_cast<int>(layer.texture_height);
  uint8_t* mask = g_mask[eye];
  int* box = g_box[eye];
  // Clear the box of the last frame.
  if (g_mask_pitch == w) {
    for (int y = box[1]; y < box[3]; ++y) {
      memset(mask + static_cast<size_t>(y) * w + box[0], 0,
             static_cast<size_t>(box[2] - box[0]));
    }
  } else {
    memset(mask, 0, sizeof(g_mask[eye]));
  }
  box[0] = w;
  box[1] = h;
  box[2] = 0;
  box[3] = 0;
  const float ppm_x = layer.rect[2] / layer.width;
  const float ppm_y = layer.rect[3] / layer.height;
  const float mid_x = layer.rect[0] + 0.5f * layer.rect[2];
  const float mid_y = layer.rect[1] + 0.5f * layer.rect[3];
  for (int hand = 0; hand < 2; ++hand) {
    for (int t = 0; t < triangles[hand]; ++t) {
      const uint32_t* tri_index = index[hand] + t * 3;
      bool ok = true;
      float tri[3][3];
      for (int k = 0; k < 3 && ok; ++k) {
        const uint32_t v = tri_index[k];
        ok = v < static_cast<uint32_t>(vertices[hand]) &&
             alpha[hand][v] >= kOpaque;
        if (ok) memcpy(tri[k], g_local[hand][v], sizeof(tri[k]));
      }
      if (!ok) continue;
      float front[4][3];
      const int n = hc::ClipFront(tri, page, front);
      if (n == 0) continue;
      float px[4][2];
      bool on = true;
      for (int k = 0; k < n && on; ++k) {
        float x = 0.0f, y = 0.0f;
        on = hc::OntoPage(eye_at, page, front[k], &x, &y);
        px[k][0] = mid_x + x * ppm_x;
        px[k][1] = mid_y - y * ppm_y;
      }
      if (on) hc::FillConvex(px, n, mask, w, h, box);
    }
  }
  if (box[0] >= box[2] || box[1] >= box[3]) {
    box[0] = box[1] = box[2] = box[3] = 0;
    return 0;
  }
  // The mask 1 pixel larger: the largest value of each row of 3, then of
  // each column of 3, inside the rect of the card.
  const int rx0 = layer.rect[0], ry0 = layer.rect[1];
  const int rx1 = layer.rect[0] + layer.rect[2];
  const int ry1 = layer.rect[1] + layer.rect[3];
  int g[4] = {box[0] - 1, box[1] - 1, box[2] + 1, box[3] + 1};
  if (g[0] < rx0) g[0] = rx0;
  if (g[1] < ry0) g[1] = ry0;
  if (g[2] > rx1) g[2] = rx1;
  if (g[3] > ry1) g[3] = ry1;
  if (g[0] >= g[2] || g[1] >= g[3]) {
    for (int y = box[1]; y < box[3]; ++y) {
      memset(mask + static_cast<size_t>(y) * w + box[0], 0,
             static_cast<size_t>(box[2] - box[0]));
    }
    box[0] = box[1] = box[2] = box[3] = 0;
    return 0;
  }
  for (int y = g[1]; y < g[3]; ++y) {
    const uint8_t* in = mask + static_cast<size_t>(y) * w;
    uint8_t* out = g_grown + static_cast<size_t>(y) * w;
    for (int x = g[0]; x < g[2]; ++x) {
      uint8_t m = in[x];
      if (x > 0 && in[x - 1] > m) m = in[x - 1];
      if (x + 1 < w && in[x + 1] > m) m = in[x + 1];
      out[x] = m;
    }
  }
  uint64_t pixels = 0;
  for (int y = g[1]; y < g[3]; ++y) {
    uint8_t* out = mask + static_cast<size_t>(y) * w;
    const uint8_t* up = y > g[1] ? g_grown + static_cast<size_t>(y - 1) * w
                                 : nullptr;
    const uint8_t* mid = g_grown + static_cast<size_t>(y) * w;
    const uint8_t* down = y + 1 < g[3]
                              ? g_grown + static_cast<size_t>(y + 1) * w
                              : nullptr;
    for (int x = g[0]; x < g[2]; ++x) {
      uint8_t m = mid[x];
      if (up != nullptr && up[x] > m) m = up[x];
      if (down != nullptr && down[x] > m) m = down[x];
      out[x] = m;
      if (m > 0) ++pixels;
    }
  }
  // The mask outside the rect of the card stays 0: the box was clipped.
  for (int y = box[1]; y < box[3]; ++y) {
    if (y >= g[1] && y < g[3]) {
      uint8_t* row = mask + static_cast<size_t>(y) * w;
      for (int x = box[0]; x < box[2]; ++x) {
        if (x < g[0] || x >= g[2]) row[x] = 0;
      }
    } else {
      memset(mask + static_cast<size_t>(y) * w + box[0], 0,
             static_cast<size_t>(box[2] - box[0]));
    }
  }
  for (int k = 0; k < 4; ++k) box[k] = g[k];
  return pixels;
}

// The holes of one eye into its picture: each channel times what is left.
void Cut(uint8_t* pixels, int eye, int w) {
  const int* box = g_box[eye];
  const uint8_t* mask = g_mask[eye];
  for (int y = box[1]; y < box[3]; ++y) {
    uint8_t* row = pixels + (static_cast<size_t>(y) * w + box[0]) * 4;
    const uint8_t* m = mask + static_cast<size_t>(y) * w + box[0];
    for (int x = box[0]; x < box[2]; ++x, row += 4, ++m) {
      if (*m == 0) continue;
      const int keep = 255 - *m;
      for (int k = 0; k < 4; ++k) {
        row[k] = static_cast<uint8_t>((row[k] * keep + 127) / 255);
      }
    }
  }
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

void HandCutFrame() {
  ReadSettings();
  const uint64_t frame = ControllersFrameCount();
  if (!g_on || g_offer.frame != frame || g_offer.frame == 0) return;
  g_offer.frame = 0;
  const bwvr::CardLayer& layer = g_offer.layer;
  if (bwvr::XrScreen::CardFailed(bwvr::kCardRingRight) ||
      layer.texture_width > static_cast<uint32_t>(kMostSide) ||
      layer.texture_height > static_cast<uint32_t>(kMostSide) ||
      !(layer.width > 0.0f) || !(layer.height > 0.0f)) {
    return;
  }
  const double start = NowMs();
  uint8_t* left = const_cast<uint8_t*>(layer.pixels);
  const int w = static_cast<int>(layer.texture_width);
  // The same picture of the owner as in the last frame: put back the clean
  // pixels of the box that the last frame cut. A new picture is clean.
  const bool same = layer.pixels == g_last_pixels &&
                    layer.serial == g_last_serial;
  if (same && g_left_cut) CopyBox(left, g_clean, w, g_left_box);
  g_left_cut = false;
  g_last_pixels = layer.pixels;
  g_last_serial = layer.serial;

  if (!HandModelBuilt()) {
    if (!g_said_no_mesh) {
      g_said_no_mesh = true;
      ModLog("hand cut     : no mesh of a hand was built (the low poly hands), "
             "so no hand cuts the %s, frame %llu", g_offer.who, frame);
    }
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
  int triangles[2] = {0, 0}, vertices[2] = {0, 0};
  const uint32_t* index[2] = {nullptr, nullptr};
  const uint8_t* alpha[2] = {nullptr, nullptr};
  for (int hand = 0; hand < 2; ++hand) {
    const float* points = nullptr;
    triangles[hand] = HandModelWorldMesh(hand, &points, &index[hand],
                                         &alpha[hand], &vertices[hand]);
    if (vertices[hand] > kMostVertices) triangles[hand] = 0;
    for (int v = 0; v < vertices[hand] && triangles[hand] > 0; ++v) {
      const float* p = points + v * 3;
      const float d[3] = {p[0] - base[0], p[1] - base[1], p[2] - base[2]};
      for (int r = 0; r < 3; ++r) {
        g_local[hand][v][r] = inverse[r * 3 + 0] * d[0] +
                              inverse[r * 3 + 1] * d[1] +
                              inverse[r * 3 + 2] * d[2];
      }
    }
  }
  uint64_t count[2];
  for (int e = 0; e < 2; ++e) {
    count[e] = EyeMask(e, eyes[e], page, layer, triangles, index, alpha,
                       vertices);
  }
  g_mask_pitch = w;
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
    ++g_timed;
    const double ms = NowMs() - start;
    g_sum_ms += ms;
    if (ms > g_most_ms) g_most_ms = ms;
    return;
  }

  // The right picture: the clean picture of the owner with the holes of the
  // right eye. Copied whole for a new picture, and else only where the
  // holes of the last frame were.
  if (!same || g_right_for != g_offer.right) {
    const int all[4] = {layer.rect[0], layer.rect[1],
                        layer.rect[0] + layer.rect[2],
                        layer.rect[1] + layer.rect[3]};
    CopyBox(g_offer.right, left, w, all);
    g_right_for = g_offer.right;
  } else {
    CopyBox(g_offer.right, left, w, g_right_box);
  }
  // Keep the clean pixels of the box of the left holes, then cut both.
  for (int k = 0; k < 4; ++k) g_left_box[k] = g_box[0][k];
  CopyBox(g_clean, left, w, g_left_box);
  if (count[0] > 0) {
    Cut(left, 0, w);
    g_left_cut = true;
  }
  for (int k = 0; k < 4; ++k) g_right_box[k] = g_box[1][k];
  if (count[1] > 0) Cut(g_offer.right, 1, w);
  bwvr::CardLayer l = layer;
  l.eye = 1;
  l.serial = bwvr::XrScreen::NewCardSerial();
  bwvr::XrScreen::StageCard(bwvr::kCardRing, l);
  bwvr::CardLayer r = layer;
  r.pixels = g_offer.right;
  r.eye = 2;
  r.serial = bwvr::XrScreen::NewCardSerial();
  bwvr::XrScreen::StageCard(bwvr::kCardRingRight, r);
  ++g_split_frames;
  ++g_timed;
  const double ms = NowMs() - start;
  g_sum_ms += ms;
  if (ms > g_most_ms) g_most_ms = ms;
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

void HandCutReport() {
  if (g_timed == 0) return;
  ModLog("hand cut     : this window the cut ran in %llu frames, %.3f ms on "
         "average and %.3f ms at the most (with no save of a test picture). "
         "A hand was in front of a page in %llu frames: the card went out as "
         "one quad for each eye. The largest hole of an eye had %llu px",
         g_timed, g_sum_ms / static_cast<double>(g_timed), g_most_ms,
         g_split_frames, g_most_pixels);
  g_timed = 0;
  g_sum_ms = 0.0;
  g_most_ms = 0.0;
  g_split_frames = 0;
  g_most_pixels = 0;
}
