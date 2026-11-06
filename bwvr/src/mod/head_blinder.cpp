// The head blinder (head_blinder). See docs/god-view.md, "The head under the
// land".
//
// WHY. In the God view the land is at the waist, and a player who bends down
// can put the head into a hill. The game draws no land from the inside, so
// the eyes then see what is under the land. As in Half-Life: Alyx, an opaque
// overlay in a warm gold and red then fills the view of the eyes, so the
// player cannot see under the land.
//
// THE HEAD. Once a frame (stereo.cpp, before the eye passes) the head point
// is the render head of the last frame of the God view (HeadCameraTableEye),
// in the middle between the eyes. The land counts at the head point and at
// four points 6 cm of the room around it, and the highest one counts
// (blinder_math.h, Clearance), so a steep slope in front of the face counts
// too. The land is LH3DIsland::GetAltitude, with the sea and the places off
// the map at 0, as hand_land.cpp has it. Only the land: a building, a rock
// and a tree do not count.
//
// THE STRENGTH. From 5 cm over the land the overlay starts at the edge of the
// view, and at the land and under it the view is opaque (blinder_math.h,
// Strength and OpacityAt). The level goes up in 0.1 s and down in 0.25 s
// (Fade). The overlay never moves the view.
//
// THE PICTURE. The last draw of the mod in each eye pass, after the vignette
// (sky_detail.cpp): a fan and rings of triangles about the middle of the eye,
// gold in the middle and red at the edge, with the states of the vignette
// (vignette.cpp, VignetteDrawBlinder). One walk copies it into the right eye
// with no shift, so both eyes get the same picture. The head point is
// between the eyes, so both eyes have the same level.
//
// WHEN. Only while the God view owns the view and no dialog box is up. In a
// room of the Temple, in a menu, in a story and in a cut scene the overlay
// does nothing.
//
// THE KEYS of bwvr.ini:
//
//   head_blinder=1   0 no overlay. The switch "Head blinder" of the debug
//                    page changes it while the game runs

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>

#include "../common/bw_addresses.h"
#include "blinder_math.h"
#include "debug_menu.h"

extern void ModLog(const char* format, ...);

uint64_t ControllersFrameCount();
void ControllersGesturePulse(float amplitude, float seconds);
// head_camera.cpp: the render head of the last frame of the table or the God
// view, in world units.
bool HeadCameraTableEye(float out[3]);
// tabletop.cpp: the God view owns the view, and its scale in units for each
// meter.
bool TabletopGodOwnsView();
float TabletopScale();
// temple.cpp: the ray of a room of the Temple is on.
bool TempleRayOn();
// vignette.cpp: the eye of the pass that runs, and the draw.
int VignetteEyeNow();
bool VignetteDrawBlinder(const blinder_math::BlinderPoint* points, int count);

namespace {

namespace bm = blinder_math;

// LH3DIsland::GetAltitude, fastcall on the map coordinates (hand_land.cpp).
const uintptr_t kGetAltitude = 0x00803090;
const float kMapScale = 6553.6f;
const float kMapSize = 5120.0f;
typedef float(__fastcall* GetAltitudeFn)(const int32_t* coords);
// A dialog box is up: SetupBox::GetCurrentActiveBox 0x00407ED0 returns
// [0x00C4CD20] (controllers.cpp, MenuUp).
const uintptr_t kActiveBox = 0x00C4CD20;

bool g_on = true;
float g_level = 0.0f;
float g_clearance = 1000.0f;  // meters of the room, the last frame
LARGE_INTEGER g_last = {};
bool g_have_last = false;
const float kFrameCap = 0.1f;
uint64_t g_frame = 0;
uint64_t g_drawn_frame[2] = {0, 0};

// For the log and the report.
bool g_shown = false;
bool g_full = false;
double g_shown_seconds = 0.0;
float g_least = 1000.0f;
uint64_t g_episodes = 0;
uint64_t g_full_frames = 0;
uint64_t g_passes = 0;
uint64_t g_failed = 0;
int g_logs = 0;
const int kMaxLogs = 60;

bm::BlinderPoint g_points[bm::kBlinderVertices];

float Ground(float x, float z) {
  if (!(x >= 0.0f && x < kMapSize && z >= 0.0f && z < kMapSize)) return 0.0f;
  const int32_t coords[3] = {static_cast<int32_t>(x * kMapScale),
                             static_cast<int32_t>(z * kMapScale), 0};
  const float y = reinterpret_cast<GetAltitudeFn>(kGetAltitude)(coords);
  if (!(y > 0.0f && y < 5000.0f)) return 0.0f;
  return y;
}

bool DialogUp() {
  return *reinterpret_cast<const uintptr_t*>(kActiveBox) != 0;
}

// The clearance of the head now, in meters of the room, or false when the
// overlay does nothing now.
bool HeadClearance(float* clearance, float head[3]) {
  if (!TabletopGodOwnsView() || DialogUp() || TempleRayOn()) return false;
  const float scale = TabletopScale();
  if (!(scale > 0.0f) || !HeadCameraTableEye(head)) return false;
  float points[bm::kPoints][2];
  bm::AroundPoints(head[0], head[2], scale, points);
  float ground[bm::kPoints];
  for (int i = 0; i < bm::kPoints; ++i) {
    ground[i] = Ground(points[i][0], points[i][1]);
  }
  *clearance = bm::Clearance(head[1], ground, bm::kPoints, scale);
  return *clearance == *clearance;
}

void LogSetting(const char* by) {
  ModLog("head blinder : %s%shead_blinder=%d. %s", by, by[0] != 0 ? ": " : "",
         g_on ? 1 : 0,
         g_on ? "When your head goes into the land, a gold and red overlay "
                "fills the view of your eyes. It starts at the edge 5 cm over "
                "the land, and it is opaque at the land and under it"
              : "The eyes see under the land when the head goes into it");
}

}  // namespace

// mod_main.cpp: head_blinder.
void HeadBlinderSetOn(bool on) {
  g_on = on;
  LogSetting("");
}

// Once a frame, before the eye passes (stereo.cpp).
void HeadBlinderFrame() {
  ++g_frame;
  LARGE_INTEGER now, frequency;
  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&frequency);
  float seconds = 0.0f;
  if (g_have_last && frequency.QuadPart > 0) {
    seconds = static_cast<float>(now.QuadPart - g_last.QuadPart) /
              static_cast<float>(frequency.QuadPart);
    if (seconds > kFrameCap) seconds = kFrameCap;
  }
  g_last = now;
  g_have_last = true;
  float target = 0.0f;
  float head[3] = {0.0f, 0.0f, 0.0f};
  bool live = false;
  if (g_on) {
    float clearance = 1000.0f;
    live = HeadClearance(&clearance, head);
    if (live) {
      g_clearance = clearance;
      target = bm::Strength(clearance);
    }
  }
  // Out of the God view (a room of the Temple, a menu, a story) the overlay
  // goes at once: the view there is not the land of the God view.
  g_level = live ? bm::Fade(g_level, target, seconds) : 0.0f;
  const bool shown = g_level > 0.0f;
  const bool full = g_level >= 1.0f;
  if (shown && !g_shown) {
    ++g_episodes;
    g_shown_seconds = 0.0;
    g_least = g_clearance;
    if (g_logs < kMaxLogs) {
      ++g_logs;
      ModLog("head blinder : the overlay starts, your head is %.1f cm over the "
             "land at %.1f %.1f, frame %llu", g_clearance * 100.0f, head[0],
             head[2], ControllersFrameCount());
    }
  }
  if (shown) {
    g_shown_seconds += seconds;
    if (g_clearance < g_least) g_least = g_clearance;
  }
  if (full) ++g_full_frames;
  if (full && !g_full && g_logs < kMaxLogs) {
    ++g_logs;
    ModLog("head blinder : the view is opaque, your head is %.1f cm %s the "
           "land, frame %llu", fabsf(g_clearance) * 100.0f,
           g_clearance < 0.0f ? "under" : "over", ControllersFrameCount());
  }
  if (!shown && g_shown && g_logs < kMaxLogs) {
    ++g_logs;
    ModLog("head blinder : the overlay is gone after %.2f s. Your head went "
           "down to %.1f cm %s the land, frame %llu", g_shown_seconds,
           fabsf(g_least) * 100.0f, g_least < 0.0f ? "under" : "over",
           ControllersFrameCount());
  }
  g_shown = shown;
  g_full = full;
}

// The level of now, from 0 to 1.
float HeadBlinderLevel() { return g_level; }

// sky_detail.cpp, after VignetteDraw: the last draw of the eye pass.
void HeadBlinderDraw() {
  if (!(g_level > 0.0f)) return;
  const int eye = VignetteEyeNow();
  if (eye < 0 || eye > 1 || g_drawn_frame[eye] == g_frame) return;
  g_drawn_frame[eye] = g_frame;
  const bw::tech::InfoTransform* info =
      reinterpret_cast<const bw::tech::InfoTransform*>(
          bw::tech::kInfoTransform);
  const float hx = info->half_res_x, hy = info->half_res_y;
  if (!(hx > 0.0f) || !(hy > 0.0f)) return;
  const float fx = hx * info->inv_half_tan_fov_x;
  const float fy = hy * info->inv_half_tan_fov_y;
  const float far_px = 4.0f * (hx > hy ? hx : hy);
  const int n = bm::BuildBlinder(g_level, hx, hy, fx, fy, far_px, g_points);
  if (n <= 0) return;
  if (VignetteDrawBlinder(g_points, n)) {
    ++g_passes;
  } else {
    ++g_failed;
  }
}

// Once a report window.
void HeadBlinderReport() {
  if (g_episodes == 0 && g_passes == 0 && g_failed == 0) return;
  ModLog("head blinder : %llu times shown, %llu frames opaque, drawn in %llu "
         "eye passes, %llu failed", g_episodes, g_full_frames, g_passes,
         g_failed);
  g_episodes = g_full_frames = g_passes = g_failed = 0;
}

// THE SWITCH "Head blinder" of the debug page: off, on. The pointing hand
// gives a long pulse for on and a short pulse for off.
int HeadBlinderValue() {
  return TestSwitchesOn() ? (g_on ? 1 : 0) : kDebugCannot;
}

void HeadBlinderChange(const char* by) {
  g_on = !g_on;
  if (!g_on) g_level = 0.0f;
  ControllersGesturePulse(0.5f, g_on ? 0.30f : 0.08f);
  LogSetting(by);
}
