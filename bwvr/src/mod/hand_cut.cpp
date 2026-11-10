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

bool g_read = false;
bool g_on = true;
int g_save = 0;
int g_saved = 0;
uint64_t g_saved_frame = 0;

struct Offer {
  uint64_t frame;
  bwvr::CardLayer layer;
  uint8_t* right;
  bool* dirtied;
  const char* who;
};
Offer g_offer = {};

// The masks of the two eyes, one byte a pixel of the texture of the card.
const int kMostSide = 1024;
uint8_t g_mask[2][kMostSide * kMostSide];
int g_box[2][4];

// The log and the report.
bool g_split_before = false;
int g_logs = 0;
const int kMostLogs = 60;
uint64_t g_split_frames = 0;
uint64_t g_most_pixels = 0;
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
                "ring shows in front of it, in each eye"
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

void ToLocal(const float base[3], const float inverse[9], const float w[3],
             float out[3]) {
  const float d[3] = {w[0] - base[0], w[1] - base[1], w[2] - base[2]};
  for (int r = 0; r < 3; ++r) {
    out[r] = inverse[r * 3 + 0] * d[0] + inverse[r * 3 + 1] * d[1] +
             inverse[r * 3 + 2] * d[2];
  }
}

// The mask of one eye: the parts of both hands in front of the page, on the
// page as the eye sees them, 1 pixel larger. Returns the count of pixels.
uint64_t EyeMask(int eye, const float eye_at[3], const hc::Page& page,
                 const bwvr::CardLayer& layer, const float base[3],
                 const float inverse[9]) {
  const int w = static_cast<int>(layer.texture_width);
  const int h = static_cast<int>(layer.texture_height);
  uint8_t* mask = g_mask[eye];
  int* box = g_box[eye];
  box[0] = w;
  box[1] = h;
  box[2] = 0;
  box[3] = 0;
  const float ppm_x = layer.rect[2] / layer.width;
  const float ppm_y = layer.rect[3] / layer.height;
  const float mid_x = layer.rect[0] + 0.5f * layer.rect[2];
  const float mid_y = layer.rect[1] + 0.5f * layer.rect[3];
  // A box of the mask that is clear before the fill: the rect of the card.
  for (int y = layer.rect[1]; y < layer.rect[1] + layer.rect[3]; ++y) {
    memset(mask + static_cast<size_t>(y) * w + layer.rect[0], 0,
           static_cast<size_t>(layer.rect[2]));
  }
  for (int hand = 0; hand < 2; ++hand) {
    const float* points = nullptr;
    const uint32_t* index = nullptr;
    int vertices = 0;
    const int triangles =
        HandModelWorldMesh(hand, &points, &index, &vertices);
    for (int t = 0; t < triangles; ++t) {
      float tri[3][3];
      bool ok = true;
      for (int k = 0; k < 3 && ok; ++k) {
        const uint32_t v = index[t * 3 + k];
        if (v >= static_cast<uint32_t>(vertices)) {
          ok = false;
          break;
        }
        ToLocal(base, inverse, points + v * 3, tri[k]);
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
  if (box[0] >= box[2] || box[1] >= box[3]) return 0;
  // Keep the mask inside the rect of the card, then make it 1 pixel larger.
  if (box[0] < layer.rect[0]) box[0] = layer.rect[0];
  if (box[1] < layer.rect[1]) box[1] = layer.rect[1];
  if (box[2] > layer.rect[0] + layer.rect[2]) box[2] = layer.rect[0] + layer.rect[2];
  if (box[3] > layer.rect[1] + layer.rect[3]) box[3] = layer.rect[1] + layer.rect[3];
  if (box[0] >= box[2] || box[1] >= box[3]) return 0;
  uint64_t pixels = 0;
  const int gx0 = box[0] > layer.rect[0] ? box[0] - 1 : box[0];
  const int gy0 = box[1] > layer.rect[1] ? box[1] - 1 : box[1];
  const int gx1 = box[2] < layer.rect[0] + layer.rect[2] ? box[2] + 1 : box[2];
  const int gy1 = box[3] < layer.rect[1] + layer.rect[3] ? box[3] + 1 : box[3];
  // The larger mask: each pixel takes the largest value of its 8 neighbors,
  // into a second mask, which then goes back into the mask.
  static uint8_t grown[kMostSide * kMostSide];
  for (int y = gy0; y < gy1; ++y) {
    for (int x = gx0; x < gx1; ++x) {
      uint8_t most = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        const int yy = y + dy;
        if (yy < box[1] || yy >= box[3]) continue;
        for (int dx = -1; dx <= 1; ++dx) {
          const int xx = x + dx;
          if (xx < box[0] || xx >= box[2]) continue;
          const uint8_t v = mask[static_cast<size_t>(yy) * w + xx];
          if (v > most) most = v;
        }
      }
      grown[static_cast<size_t>(y) * w + x] = most;
      if (most > 0) ++pixels;
    }
  }
  for (int y = gy0; y < gy1; ++y) {
    memcpy(mask + static_cast<size_t>(y) * w + gx0,
           grown + static_cast<size_t>(y) * w + gx0,
           static_cast<size_t>(gx1 - gx0));
  }
  box[0] = gx0;
  box[1] = gy0;
  box[2] = gx1;
  box[3] = gy1;
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

void HandCutOffer(const bwvr::CardLayer& layer, uint8_t* right, bool* dirtied,
                  const char* who) {
  ReadSettings();
  if (!g_on || right == nullptr || layer.pixels == nullptr) return;
  g_offer.frame = ControllersFrameCount();
  g_offer.layer = layer;
  g_offer.right = right;
  g_offer.dirtied = dirtied;
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
  if (!HandModelBuilt()) {
    if (!g_said_no_mesh) {
      g_said_no_mesh = true;
      ModLog("hand cut     : no mesh of a hand was built (the low poly hands), "
             "so no hand cuts the %s, frame %llu", g_offer.who, frame);
    }
    return;
  }
  const double start = NowMs();
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

  uint64_t count[2];
  for (int e = 0; e < 2; ++e) {
    count[e] = EyeMask(e, eyes[e], page, layer, base, inverse);
  }
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
  if (!split) return;

  // The right eye starts from the picture of the layer, then each eye gets
  // its own holes.
  uint8_t* left = const_cast<uint8_t*>(layer.pixels);
  const int w = static_cast<int>(layer.texture_width);
  for (int row = layer.rect[1]; row < layer.rect[1] + layer.rect[3]; ++row) {
    memcpy(g_offer.right + (static_cast<size_t>(row) * w + layer.rect[0]) * 4,
           left + (static_cast<size_t>(row) * w + layer.rect[0]) * 4,
           static_cast<size_t>(layer.rect[2]) * 4);
  }
  if (count[0] > 0) Cut(left, 0, w);
  if (count[1] > 0) Cut(g_offer.right, 1, w);
  if (g_offer.dirtied != nullptr) *g_offer.dirtied = true;
  bwvr::CardLayer l = layer;
  l.eye = 1;
  l.serial = bwvr::XrScreen::NewCardSerial();
  bwvr::XrScreen::StageCard(bwvr::kCardRing, l);
  bwvr::CardLayer r = layer;
  r.pixels = g_offer.right;
  r.eye = 2;
  r.serial = bwvr::XrScreen::NewCardSerial();
  bwvr::XrScreen::StageCard(bwvr::kCardRingRight, r);
  if (g_saved < g_save && (g_saved == 0 || frame >= g_saved_frame + 30)) {
    ++g_saved;
    g_saved_frame = frame;
    const size_t at = (static_cast<size_t>(layer.rect[1]) * w + layer.rect[0]) * 4;
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
  ++g_split_frames;
  if (count[0] > g_most_pixels) g_most_pixels = count[0];
  if (count[1] > g_most_pixels) g_most_pixels = count[1];
  const double ms = NowMs() - start;
  if (ms > g_most_ms) g_most_ms = ms;
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
  if (g_split_frames == 0) return;
  ModLog("hand cut     : this window a hand was in front of a page in %llu "
         "frames: the card went out as one quad for each eye. The largest "
         "hole of an eye had %llu px, and the longest cut took %.2f ms",
         g_split_frames, g_most_pixels, g_most_ms);
  g_split_frames = 0;
  g_most_pixels = 0;
  g_most_ms = 0.0;
}
