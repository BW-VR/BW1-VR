// The comfort vignette (comfort_vignette). See docs/god-view.md, "The comfort
// of a move".
//
// WHY. A move of the view that the player makes with a control, and that the
// body does not feel, can make a person ill in a headset. The edges of the
// view carry most of that motion. The vignette makes the edges dark while
// such a move runs, and the middle of the view stays clear.
//
// WHICH MOVES. Each one reports itself (VignetteMove, VignetteStep): the walk
// with the stick, the smooth zoom and the grab of the world with one hand or
// two hands (tabletop.cpp) report a share of their full speed in each frame
// that they move. A snap turn, a zoom step and a
// teleport are steps of one frame (ApplyGodSteps in tabletop.cpp), and a step
// puts the level at its top at once. A move that the player did not make (a
// story, the game camera, a cut of the view) reports nothing, and a head that
// moves reports nothing.
//
// THE LEVEL. Once a frame, before the eye passes (stereo.cpp), the level goes
// to the largest share of the frame before. It goes up in 0.15 s and down in
// 0.3 s (comfort_math.h, Fade). The vignette never moves the view.
//
// THE PICTURE. In each eye pass, last in the draws of the mod after the
// world (sky_detail.cpp, h_SunGlare), the vignette draws rings of black
// triangles about the middle of the eye. The darkness of a ring follows its
// angle from the middle (comfort_math.h, Shape). The eye of a pass is
// symmetric about the forward direction of the head (docs/backend.md, "What
// the view must cover"), so the middle is the middle of the render. The
// angles go to pixels with the projection of the game (InfoTransform). The
// vertices have the game's format 0x1C4 with rhw 1, and the draw has no
// depth test and no depth write, so the land never hides it. One walk for
// both eyes (one_walk.cpp) copies a batch with rhw 1 into the right half with
// no shift, so each eye gets the same rings. The interface of the game and
// the cards of the mod draw after it, so they stay clear.
//
// THE DEVICE. As overlay.cpp does: the game's Direct3D 7 device and one
// DrawPrimitive for each eye. The draw reads each state before it changes it
// and puts back the value that it read, in the reverse order.
//
// THE KEYS of bwvr.ini:
//
//   comfort_vignette=0   0 off, 1 gentle, 2 strong. The switch "Vignette" of
//                        the debug page of the ring changes it while the
//                        game runs

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdint.h>
#include <string.h>

#include "../common/bw_addresses.h"
#include "comfort_math.h"
#include "debug_menu.h"

extern void ModLog(const char* format, ...);

uint64_t ControllersFrameCount();
// controllers.cpp: a pulse on the pointing hand.
void ControllersGesturePulse(float amplitude, float seconds);

namespace {

using comfort_math::VignettePoint;

int g_mode = comfort_math::kVignetteOff;

// The reports of the moves since the last frame.
float g_share = 0.0f;
const char* g_share_by = "";
bool g_step = false;
const char* g_step_by = "";

// The level of now, and the time of the last frame.
float g_level = 0.0f;
LARGE_INTEGER g_last = {};
bool g_have_last = false;
const float kFrameCap = 0.1f;

// The frame and the eye pass, so the draw runs once in each eye pass.
uint64_t g_frame = 0;
int g_eye = -1;
uint64_t g_drawn_frame[2] = {0, 0};

// For the log and the report.
bool g_dark = false;          // the level is above 0 now
double g_dark_seconds = 0.0;  // how long this time
float g_dark_most = 0.0f;
uint64_t g_episodes = 0;
uint64_t g_passes = 0;
uint64_t g_failed = 0;
uint64_t g_steps = 0;
int g_logs = 0;
const int kMaxLogs = 60;

VignettePoint g_points[comfort_math::kVignetteVertices];

struct Vertex {
  float x, y, z, rhw;
  uint32_t diffuse, specular;
  float u, v;
};
static_assert(sizeof(Vertex) == 32, "the game's vertex is 32 bytes");
const uint32_t kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR |
                      D3DFVF_TEX1;  // 0x1C4, as the game draws
Vertex g_vertices[comfort_math::kVignetteVertices];

// THE STATES that the draw changes: black with the alpha of each corner over
// the picture, with no depth test, no depth write and no fog.
struct RenderSet {
  D3DRENDERSTATETYPE state;
  DWORD value;
};
const RenderSet kRender[] = {
    {D3DRENDERSTATE_ZENABLE, D3DZB_FALSE},
    {D3DRENDERSTATE_ZWRITEENABLE, FALSE},
    {D3DRENDERSTATE_ALPHABLENDENABLE, TRUE},
    {D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA},
    {D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA},
    {D3DRENDERSTATE_ALPHATESTENABLE, FALSE},
    {D3DRENDERSTATE_FOGENABLE, FALSE},
    {D3DRENDERSTATE_CULLMODE, D3DCULL_NONE},
    {D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD},
    {D3DRENDERSTATE_FILLMODE, D3DFILL_SOLID},
    {D3DRENDERSTATE_STENCILENABLE, FALSE},
    {D3DRENDERSTATE_COLORKEYENABLE, FALSE},
};
const int kRenderCount = sizeof(kRender) / sizeof(kRender[0]);

struct StageSet {
  DWORD stage;
  D3DTEXTURESTAGESTATETYPE state;
  DWORD value;
};
// Stage 0 takes the color and the alpha of the vertices only, and stage 1 off
// turns off each stage after stage 0. The texture stays bound.
const StageSet kStage[] = {
    {0, D3DTSS_COLOROP, D3DTOP_SELECTARG1},
    {0, D3DTSS_COLORARG1, D3DTA_DIFFUSE},
    {0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1},
    {0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE},
    {1, D3DTSS_COLOROP, D3DTOP_DISABLE},
};
const int kStageCount = sizeof(kStage) / sizeof(kStage[0]);

DWORD g_render_was[kRenderCount];
bool g_render_changed[kRenderCount];
DWORD g_stage_was[kStageCount];
bool g_stage_changed[kStageCount];

void SetStates(IDirect3DDevice7* device) {
  for (int i = 0; i < kRenderCount; ++i) {
    g_render_changed[i] = false;
    DWORD was = 0;
    if (FAILED(device->GetRenderState(kRender[i].state, &was))) continue;
    if (was == kRender[i].value) continue;
    if (SUCCEEDED(device->SetRenderState(kRender[i].state, kRender[i].value))) {
      g_render_was[i] = was;
      g_render_changed[i] = true;
    }
  }
  for (int i = 0; i < kStageCount; ++i) {
    g_stage_changed[i] = false;
    DWORD was = 0;
    if (FAILED(device->GetTextureStageState(kStage[i].stage, kStage[i].state,
                                            &was))) {
      continue;
    }
    if (was == kStage[i].value) continue;
    if (SUCCEEDED(device->SetTextureStageState(
            kStage[i].stage, kStage[i].state, kStage[i].value))) {
      g_stage_was[i] = was;
      g_stage_changed[i] = true;
    }
  }
}

// In the reverse order.
void PutStatesBack(IDirect3DDevice7* device) {
  for (int i = kStageCount - 1; i >= 0; --i) {
    if (!g_stage_changed[i]) continue;
    device->SetTextureStageState(kStage[i].stage, kStage[i].state,
                                 g_stage_was[i]);
    g_stage_changed[i] = false;
  }
  for (int i = kRenderCount - 1; i >= 0; --i) {
    if (!g_render_changed[i]) continue;
    device->SetRenderState(kRender[i].state, g_render_was[i]);
    g_render_changed[i] = false;
  }
}

const char* ModeName(int mode) {
  return mode == comfort_math::kVignetteStrong   ? "strong"
         : mode == comfort_math::kVignetteGentle ? "gentle"
                                                 : "off";
}

// by names who changed the key, or is empty at the start.
void LogMode(const char* by) {
  const comfort_math::VignetteShape shape = comfort_math::Shape(g_mode);
  const char* colon = by[0] != 0 ? ": " : "";
  if (g_mode == comfort_math::kVignetteOff) {
    ModLog("vignette     : %s%scomfort_vignette=0. The edges of the view stay "
           "clear in a move", by, colon);
    return;
  }
  ModLog("vignette     : %s%scomfort_vignette=%d (%s). In a move that you "
         "make with a control, the edges of the view go dark: clear to %.0f "
         "degrees from the middle, and %.0f%% dark from %.0f degrees at full "
         "speed. It comes in %.2f s and goes in %.2f s", by, colon, g_mode,
         ModeName(g_mode),
         shape.clear_deg, shape.darkest * 100.0f, shape.dark_deg,
         comfort_math::kFadeInSeconds, comfort_math::kFadeOutSeconds);
}

}  // namespace

// mod_main.cpp: comfort_vignette.
void VignetteSetMode(int mode) {
  g_mode = comfort_math::VignetteMode(mode);
  LogMode("");
}

// A move of the view that the player makes now, with share from 0 to 1 of
// the full speed of its control. The largest share of a frame counts.
void VignetteMove(float share, const char* by) {
  if (g_mode == comfort_math::kVignetteOff) return;
  if (!(share > g_share)) return;
  g_share = share > 1.0f ? 1.0f : share;
  g_share_by = by;
}

// A step of the view in one frame: a snap turn, a zoom step or a teleport.
void VignetteStep(const char* by) {
  if (g_mode == comfort_math::kVignetteOff) return;
  g_step = true;
  g_step_by = by;
}

// Once a frame, before the eye passes (stereo.cpp).
void VignetteFrame() {
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
  const float share = g_share;
  const char* by = g_step ? g_step_by : g_share_by;
  const bool step = g_step;
  g_share = 0.0f;
  g_share_by = "";
  g_step = false;
  if (g_mode == comfort_math::kVignetteOff) {
    g_level = 0.0f;
  } else if (step) {
    g_level = 1.0f;
    ++g_steps;
  } else {
    g_level = comfort_math::Fade(g_level, share, seconds);
  }
  const bool dark = g_level > 0.0f;
  if (dark && !g_dark) {
    ++g_episodes;
    g_dark_seconds = 0.0;
    g_dark_most = 0.0f;
    if (g_logs < kMaxLogs) {
      ++g_logs;
      ModLog("vignette     : the edges of the view go dark (%s), %s, frame "
             "%llu", ModeName(g_mode), by[0] != 0 ? by : "a move",
             ControllersFrameCount());
    }
  }
  if (dark) {
    g_dark_seconds += seconds;
    if (g_level > g_dark_most) g_dark_most = g_level;
  }
  if (!dark && g_dark && g_logs < kMaxLogs) {
    ++g_logs;
    ModLog("vignette     : the edges of the view are clear again after "
           "%.2f s, with a top level of %.2f, frame %llu", g_dark_seconds,
           g_dark_most, ControllersFrameCount());
  }
  g_dark = dark;
}

// stereo.cpp: the eye of the pass that runs, and -1 after it.
void VignettePassEye(int eye) { g_eye = eye; }

// The level of now, from 0 to 1, for a test and the log.
float VignetteLevel() { return g_level; }

// sky_detail.cpp, last in the draws of the mod in each eye pass.
void VignetteDraw() {
  if (!(g_level > 0.0f) || g_mode == comfort_math::kVignetteOff) return;
  const int eye = g_eye;
  if (eye < 0 || eye > 1 || g_drawn_frame[eye] == g_frame) return;
  g_drawn_frame[eye] = g_frame;
  IDirect3DDevice7* device = *reinterpret_cast<IDirect3DDevice7* const*>(
      bw::render::kDirect3DDevice7);
  if (device == nullptr) return;
  const bw::tech::InfoTransform* info =
      reinterpret_cast<const bw::tech::InfoTransform*>(
          bw::tech::kInfoTransform);
  const float hx = info->half_res_x, hy = info->half_res_y;
  const float fx = hx * info->inv_half_tan_fov_x;
  const float fy = hy * info->inv_half_tan_fov_y;
  if (!(hx > 0.0f) || !(hy > 0.0f)) return;
  const float far_px = 4.0f * (hx > hy ? hx : hy);
  const int n = comfort_math::BuildVignette(comfort_math::Shape(g_mode),
                                            g_level, hx, hy, fx, fy, far_px,
                                            g_points);
  if (n <= 0) return;
  for (int i = 0; i < n; ++i) {
    Vertex& d = g_vertices[i];
    d.x = g_points[i].x;
    d.y = g_points[i].y;
    d.z = 0.0f;
    d.rhw = 1.0f;
    float a = g_points[i].alpha * 255.0f + 0.5f;
    if (a < 0.0f) a = 0.0f;
    if (a > 255.0f) a = 255.0f;
    d.diffuse = static_cast<uint32_t>(a) << 24;  // black
    d.specular = 0xFF000000u;  // no fog, if the fog were on
    d.u = 0.0f;
    d.v = 0.0f;
  }
  SetStates(device);
  const HRESULT result = device->DrawPrimitive(
      D3DPT_TRIANGLELIST, kFvf, g_vertices, static_cast<DWORD>(n), 0);
  PutStatesBack(device);
  if (FAILED(result)) {
    ++g_failed;
    static bool said = false;
    if (!said) {
      said = true;
      ModLog("vignette     : the draw failed (0x%08X)",
             static_cast<unsigned>(result));
    }
    return;
  }
  ++g_passes;
}

// Once a report window.
void VignetteReport() {
  if (g_episodes == 0 && g_passes == 0 && g_failed == 0) return;
  ModLog("vignette     : %llu times dark, %llu of them from a step, drawn in "
         "%llu eye passes, %llu failed", g_episodes, g_steps, g_passes,
         g_failed);
  g_episodes = g_steps = g_passes = g_failed = 0;
}

// THE SWITCH "Vignette" of the debug page: off, gentle, strong. The pointing
// hand gives a short pulse for off, a medium pulse for gentle and a long
// pulse for strong.
// The settings window (settings_panel.cpp): comfort_vignette at once.
void VignetteSet(int mode, const char* by) {
  if (mode < comfort_math::kVignetteOff || mode > comfort_math::kVignetteStrong) {
    mode = comfort_math::kVignetteOff;
  }
  g_mode = mode;
  if (g_mode == comfort_math::kVignetteOff) g_level = 0.0f;
  LogMode(by);
}

int VignetteValue() {
  return TestSwitchesOn() ? g_mode : kDebugCannot;
}

void VignetteChange(const char* by) {
  g_mode = comfort_math::NextVignetteMode(g_mode);
  if (g_mode == comfort_math::kVignetteOff) g_level = 0.0f;
  ControllersGesturePulse(0.5f, g_mode == comfort_math::kVignetteStrong ? 0.40f
                                : g_mode == comfort_math::kVignetteGentle
                                    ? 0.24f
                                    : 0.08f);
  LogMode(by);
}
