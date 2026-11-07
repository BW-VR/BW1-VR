// The view for videos (video_view). See docs/backend.md, "The view for
// videos".
//
// WHY. A capture of the headset (Quest 3) records the middle of the view.
// The player of this game looks down most of the time, with the head and
// with the eyes, so that capture cuts off what the player looks at. This view
// puts a 16:9 area of the left eye picture into the window of the game on
// the PC, so a capture tool records it there.
//
// THE AREA. Once a frame, before the eye passes (VideoViewFrame), the area
// is fov_deg wide at the middle of the picture (video_view_fov_deg), with
// the shape 16:9. Its middle is down_pct percent of the height of the
// picture below the middle of the picture (video_view_down_pct). The
// projection of the game (InfoTransform) gives the pixels of a tangent. The
// area of the frame is latched, so the marks of both eye passes and the
// window have the same area.
//
// THE SMOOTHED HEAD. A video of a head that shakes is not good to look at.
// The area moves in the eye picture against the shake: a smoothed direction
// follows the forward direction of the head with the time constant
// smooth_ms (video_view_smooth_ms), and the area is where that direction is
// in the picture. The smoothed direction stays within kBandDeg of the head,
// so the area moves at most that far, and a fast turn of the head moves the
// video with the head. Only the video is smoothed: the headset shows the
// eye pictures as they are, 1:1 with the head.
//
// THE MARKS. In each eye pass the mod draws faint corner marks around the
// area (video_view_marks), after the vignette and before the interface of
// the game. They are fixed to the head: their place is the area with no
// smoothing, grown by the largest move of the smoothing and a gap. So the
// marks never move by themselves, and the video never has a mark. The
// vertices have the format 0x1C4 of the game with rhw 1, as the vignette
// has, so one walk puts each mark at the same pixel in both eyes.
//
// THE WINDOW. In a headset session the mod does not copy the frame to the
// window (skip_desktop_present). With video_view=1 and the swapchain of the
// window (backend=vulkan, vk_window=1) the copy goes out, and vk_window.cpp
// blits the area into a 16:9 rect in the middle of the client area, with
// black around it. A picture with no eye pass (a menu) fills the 16:9 rect
// with its own shape.
//
// THE LAYERS (video_view_layers). The quad layers of the headset are not in
// the eye pictures: the hint cards, the subtitle card, the ring and the
// panels of panel_kit.cpp on it, the keyboard, the bubble of a miracle, the
// panels of the interface and the walls of the cut scene window. The
// session keeps a record of the quads of each frame, in the order of the
// runtime (XrScreen::WantVideoLayers). With no headset the record comes
// from what the mod staged (VideoViewEndPasses). VideoViewDraws takes each
// corner of a quad to the left eye picture (video_layers_math.h): with the
// pose of the left eye that the pictures are drawn from and the projection
// of the game, or for a screen in the room, to the place on the screen
// where the eye sees it. Then to the area of the window. vk_window.cpp
// draws them over the area with their alpha (vk_video.cpp). A card from the
// pixels of the CPU gives the number of its picture
// (video_view_copy_changed), so vk_video.cpp copies it only when it changes.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <math.h>
#include <stdint.h>

#include "../common/bw_addresses.h"
#include "debug_menu.h"
#include "../vr/xr_screen.h"
#include "video_layers_math.h"
#include "video_view.h"
#include "video_view_math.h"

extern void ModLog(const char* format, ...);

uint64_t ControllersFrameCount();
void ControllersGesturePulse(float amplitude, float seconds);
// stereo.cpp: the eye pass now, or -1 outside the eye passes.
int StereoPassEye();
// proxy/ddraw_proxy.cpp
bool ProxyBackendVulkan();
// vk_window.cpp: the window has a swapchain of Vulkan.
bool VkWindowWanted();

namespace {

namespace vm = video_view_math;
namespace lm = video_layers_math;

bool g_on = false;
int g_down = vm::kDownDefault;
int g_fov = vm::kFovDefault;
int g_smooth_ms = vm::kSmoothDefault;
bool g_marks = true;
bool g_layers = true;  // video_view_layers
bool g_copy_changed = true;  // video_view_copy_changed
// backend=vulkan, read once: ProxyBackendVulkan reads the ini file.
bool g_vulkan = false;

// The left eye of the frame.
float g_head[4] = {0.0f, 0.0f, 0.0f, 1.0f};
float g_eye[3] = {0.0f, 0.0f, 0.0f};
vm::Smooth g_smooth = {{0.0f, 0.0f, -1.0f}, false};
LARGE_INTEGER g_last = {};
bool g_have_last = false;
const float kFrameCap = 0.1f;

// THE AREA OF THE FRAME, latched in VideoViewFrame.
bool g_valid = false;
uint64_t g_frame = 0;
float g_width = 0.0f, g_height = 0.0f;  // the eye picture, pixels
vm::Rect g_area = {};    // with no smoothing, for the marks
vm::Rect g_moved = {};   // with the smoothing, for the window
float g_margin_x = 0.0f, g_margin_y = 0.0f;
uint64_t g_drawn_frame[2] = {0, 0};
// The tangents of the half angles of the eye picture, and the left eye of
// the frame, for the layers.
float g_tan_x = 0.0f, g_tan_y = 0.0f;
float g_frame_eye[3] = {0.0f, 0.0f, 0.0f};
float g_frame_head[4] = {0.0f, 0.0f, 0.0f, 1.0f};
// The serial of the last record that went into the window, so a record goes
// into one window only.
uint64_t g_used_serial = 0;
// For the report: the windows with layers, the quads drawn and the quads
// outside the area, and each kind once in the log.
uint64_t g_layer_windows = 0;
uint64_t g_layer_draws = 0;
uint64_t g_layer_outside = 0;
uint64_t g_layer_cards = 0, g_layer_panels = 0, g_layer_walls = 0;
uint64_t g_layer_room_panels = 0;  // the panels of panel_kit on a card
// The first draw of each card, of a panel of the interface, of a wall and of
// a panel in the room goes into the log.
const int kSaidRoomPanel = bwvr::kCards + 2;
const int kSaidCount = bwvr::kCards + 3;
bool g_said[kSaidCount] = {};
const char* const kCardNames[bwvr::kCards] = {
    "the hint card of the pointing hand", "the hint card of the other hand",
    "the subtitle card", "the ring", "the keyboard", "the bubble card",
    "the ray over the panels"};

// For the log and the report.
bool g_said_size = false;
bool g_lagging = false;
uint64_t g_lag_start = 0;
float g_lag_most = 0.0f;
float g_lag_window = 0.0f;
int g_logs = 0;
const int kMaxLogs = 40;
uint64_t g_frames = 0;
uint64_t g_mark_passes = 0;
uint64_t g_mark_failed = 0;
uint64_t g_windows = 0;
bool g_said_backend = false;

struct Vertex {
  float x, y, z, rhw;
  uint32_t diffuse, specular;
  float u, v;
};
static_assert(sizeof(Vertex) == 32, "the game's vertex is 32 bytes");
const uint32_t kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR |
                      D3DFVF_TEX1;  // 0x1C4, as the game draws
const int kVertices = vm::kMarkRects * 6;
Vertex g_vertices[kVertices];
// White, with an alpha of 0.35: faint over a bright land and over the sea.
const uint32_t kMarkColor = 0x59FFFFFFu;

// THE STATES that the draw changes: the color of the vertices over the
// picture with its alpha, with no depth test, no depth write and no fog.
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

// by names who changed a key, or is empty at the start.
void LogKeys(const char* by) {
  const char* colon = by[0] != 0 ? ": " : "";
  if (!g_on) {
    ModLog("video view   : %s%svideo_view=0. The window of the game shows the "
           "left eye picture, and a headset session does not copy it to the "
           "window", by, colon);
    return;
  }
  ModLog("video view   : %s%svideo_view=1. The window of the game shows a "
         "16:9 area of the left eye picture, %d degrees wide, with its middle "
         "%d%% of the height below the middle of the picture "
         "(video_view_down_pct). Smoothed head %d ms within %.0f degrees "
         "(video_view_smooth_ms), corner marks in the headset %s "
         "(video_view_marks=%d), the quad layers of the headset %s "
         "(video_view_layers=%d), a copy of the pixels of a card %s "
         "(video_view_copy_changed=%d)",
         by, colon, g_fov, g_down, g_smooth_ms, vm::kBandDeg,
         g_marks ? "on" : "off", g_marks ? 1 : 0,
         g_layers ? "in the video" : "not in the video", g_layers ? 1 : 0,
         g_copy_changed ? "only after a new picture" : "at each present",
         g_copy_changed ? 1 : 0);
}

// The session keeps the record of the layers only while the video wants it.
void WantLayers() {
  bwvr::XrScreen::WantVideoLayers(g_on && g_layers && g_vulkan);
}

}  // namespace

void VideoViewSetOptions(int on, int down_pct, int fov_deg, int smooth_ms,
                         int marks, int layers, int copy_changed) {
  g_on = on != 0;
  g_copy_changed = copy_changed != 0;
  g_down = vm::ClampDown(down_pct);
  g_fov = vm::ClampFov(fov_deg);
  g_smooth_ms = vm::ClampSmooth(smooth_ms);
  g_marks = marks != 0;
  g_layers = layers != 0;
  g_vulkan = ProxyBackendVulkan();
  WantLayers();
  LogKeys("");
}

void VideoViewHead(const float position[3], const float orientation[4]) {
  for (int i = 0; i < 3; ++i) g_eye[i] = position[i];
  for (int i = 0; i < 4; ++i) g_head[i] = orientation[i];
}

void VideoViewFrame() {
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
  if (!g_on) {
    g_valid = false;
    g_smooth.have = false;
    return;
  }
  const bw::tech::InfoTransform* info =
      reinterpret_cast<const bw::tech::InfoTransform*>(
          bw::tech::kInfoTransform);
  const float hx = info->half_res_x, hy = info->half_res_y;
  const float fx = hx * info->inv_half_tan_fov_x;
  const float fy = hy * info->inv_half_tan_fov_y;
  if (!(hx > 0.0f) || !(hy > 0.0f) || !(fx > 0.0f) || !(fy > 0.0f)) {
    g_valid = false;
    return;
  }
  // The smoothed direction, and where it is in the picture of this head.
  float forward[3];
  vm::Forward(g_head, forward);
  const float band_rad = vm::kBandDeg * 3.14159265f / 180.0f;
  vm::SmoothStep(&g_smooth, forward, seconds,
                 static_cast<float>(g_smooth_ms) * 0.001f, band_rad);
  float tx = 0.0f, ty = 0.0f;
  if (!vm::Tangents(g_head, g_smooth.f, &tx, &ty)) tx = ty = 0.0f;
  const float band = tanf(band_rad);
  g_width = 2.0f * hx;
  g_height = 2.0f * hy;
  g_tan_x = hx / fx;
  g_tan_y = hy / fy;
  for (int i = 0; i < 3; ++i) g_frame_eye[i] = g_eye[i];
  for (int i = 0; i < 4; ++i) g_frame_head[i] = g_head[i];
  g_margin_x = band * fx;
  g_margin_y = band * fy;
  g_area = vm::Area(g_width, g_height, fx, g_fov, g_down, g_margin_x,
                    g_margin_y);
  g_moved = vm::Moved(g_area, tx * fx, -ty * fy, g_width, g_height);
  g_valid = true;
  ++g_frames;
  if (!g_said_size) {
    g_said_size = true;
    ModLog("video view   : the eye picture is %.0f by %.0f, the area is "
           "%.0f by %.0f at %.0f, %.0f (its middle %.0f px below the middle "
           "of the picture), the smoothed head moves it at most %.0f by %.0f "
           "px",
           g_width, g_height, g_area.x1 - g_area.x0, g_area.y1 - g_area.y0,
           g_area.x0, g_area.y0,
           0.5f * (g_area.y0 + g_area.y1) - 0.5f * g_height, g_margin_x,
           g_margin_y);
  }
  // THE LOG of the smoothing: a head that turns faster than the smoothing
  // can follow puts the area at its band, and the log has the start and the
  // end of each such time.
  const float lag = vm::LagDeg(g_smooth, forward);
  if (lag > g_lag_window) g_lag_window = lag;
  const bool lagging = lag > vm::kBandDeg - 0.05f;
  if (lagging && !g_lagging) {
    g_lag_start = ControllersFrameCount();
    g_lag_most = lag;
    if (g_logs < kMaxLogs) {
      ++g_logs;
      ModLog("video view   : the head turns faster than the smoothing, the "
             "area is %.2f degrees behind the head, frame %llu", lag,
             g_lag_start);
    }
  }
  if (lag > g_lag_most) g_lag_most = lag;
  if (g_lagging && lag < 0.1f && g_logs < kMaxLogs) {
    ++g_logs;
    ModLog("video view   : the area is on the head again (%.2f degrees) "
           "after %llu frames, frame %llu", lag,
           ControllersFrameCount() - g_lag_start, ControllersFrameCount());
  }
  if (lagging) g_lagging = true;
  if (lag < 0.1f) g_lagging = false;
}

void VideoViewDrawMarks() {
  if (!g_on || !g_marks || !g_valid || !g_vulkan) return;
  const int eye = StereoPassEye();
  if (eye < 0 || eye > 1 || g_drawn_frame[eye] == g_frame) return;
  g_drawn_frame[eye] = g_frame;
  IDirect3DDevice7* device = *reinterpret_cast<IDirect3DDevice7* const*>(
      bw::render::kDirect3DDevice7);
  if (device == nullptr) return;
  // Thin: 2 px, and 1 px more for each 1000 px of height.
  const float thick = floorf(2.0f + g_height / 1000.0f);
  const float length = 0.07f * (g_area.y1 - g_area.y0);
  vm::Rect bars[vm::kMarkRects];
  const int n =
      vm::Marks(g_area, g_margin_x, g_margin_y, thick, length, bars);
  int v = 0;
  for (int i = 0; i < n && v + 6 <= kVertices; ++i) {
    const float x[4] = {bars[i].x0, bars[i].x1, bars[i].x1, bars[i].x0};
    const float y[4] = {bars[i].y0, bars[i].y0, bars[i].y1, bars[i].y1};
    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; ++k) {
      Vertex& d = g_vertices[v++];
      d.x = x[order[k]];
      d.y = y[order[k]];
      d.z = 0.0f;
      d.rhw = 1.0f;
      d.diffuse = kMarkColor;
      d.specular = 0xFF000000u;  // no fog, if the fog were on
      d.u = 0.0f;
      d.v = 0.0f;
    }
  }
  if (v == 0) return;
  SetStates(device);
  const HRESULT result = device->DrawPrimitive(
      D3DPT_TRIANGLELIST, kFvf, g_vertices, static_cast<DWORD>(v), 0);
  PutStatesBack(device);
  if (FAILED(result)) {
    ++g_mark_failed;
    static bool said = false;
    if (!said) {
      said = true;
      ModLog("video view   : the draw of the marks failed (0x%08X)",
             static_cast<unsigned>(result));
    }
    return;
  }
  ++g_mark_passes;
}

bool VideoViewPresentWanted() {
  if (!g_on) return false;
  if (!g_vulkan) {
    if (!g_said_backend) {
      g_said_backend = true;
      ModLog("video view   : video_view=1 needs backend=vulkan. With "
             "dxwrapper a headset session does not copy the frame to the "
             "window");
    }
    return false;
  }
  return VkWindowWanted();
}

bool VideoViewWindow(bool from_eye, float uv[4], float* aspect) {
  if (!g_on) return false;
  *aspect = vm::kAspect;
  if (!from_eye || !g_valid || !(g_width > 0.0f) || !(g_height > 0.0f)) {
    // A picture with no eye pass: all of it, in its own shape.
    uv[0] = 0.0f;
    uv[1] = 0.0f;
    uv[2] = 1.0f;
    uv[3] = 1.0f;
    *aspect = -1.0f;
    return true;
  }
  uv[0] = g_moved.x0 / g_width;
  uv[1] = g_moved.y0 / g_height;
  uv[2] = g_moved.x1 / g_width;
  uv[3] = g_moved.y1 / g_height;
  ++g_windows;
  return true;
}

void VideoViewEndPasses(bool to_headset) {
  if (to_headset || !g_on || !g_layers || !g_vulkan || !g_valid) return;
  bwvr::XrScreen::RecordVideoLayers(g_frame_eye,
                                    static_cast<uint32_t>(g_width),
                                    static_cast<uint32_t>(g_height));
}

int VideoViewDraws(const float uv[4], VideoDraw out[kVideoDraws]) {
  if (!g_on || !g_layers || !g_vulkan) return 0;
  // Static: the record is about 2 KB.
  static bwvr::VideoLayers record;
  if (!bwvr::XrScreen::VideoLayersOf(&record) ||
      record.serial == g_used_serial) {
    return 0;
  }
  g_used_serial = record.serial;
  lm::Map map;
  if (record.screen) {
    if (!lm::ScreenMap(record.eye_position, record.screen_position,
                       record.screen_orientation, record.screen_width,
                       record.screen_height, record.screen_part, &map)) {
      return 0;
    }
  } else {
    if (!g_valid || !(g_tan_x > 0.0f) || !(g_tan_y > 0.0f)) return 0;
    map = lm::EyeMap(g_frame_eye, g_frame_head, g_tan_x, g_tan_y);
  }
  int n = 0;
  for (int i = 0; i < record.count && n < kVideoDraws; ++i) {
    const bwvr::VideoQuad& q = record.quads[i];
    VideoDraw& d = out[n];
    d = {};
    float corners[4][3];
    lm::Corners(q.position, q.orientation, q.width, q.height, corners);
    for (int c = 0; c < 4; ++c) {
      float uvw[3];
      lm::Apply(map, corners[c], uvw);
      lm::ToArea(uvw, uv, map.near_w, d.corner[c]);
    }
    if (!lm::InArea(d.corner)) {
      ++g_layer_outside;
      continue;
    }
    for (int k = 0; k < 4; ++k) d.color[k] = 1.0f;
    int said = -1;
    if (q.kind == bwvr::kVideoWall) {
      // Black with the alpha: the color times the alpha is 0.
      d.source = kVideoWhite;
      d.color[0] = d.color[1] = d.color[2] = 0.0f;
      d.color[3] = q.alpha;
      d.uv[0] = d.uv[1] = 0.0f;
      d.uv[2] = d.uv[3] = 1.0f;
      ++g_layer_walls;
      said = bwvr::kCards + 1;
    } else if (q.kind == bwvr::kVideoPanel) {
      if (q.panel_image == 0 || q.texture_width == 0 ||
          q.texture_height == 0) {
        continue;
      }
      d.source = kVideoPanel;
      d.handle = q.panel_image;
      lm::TextureRect(q.rect, static_cast<float>(q.texture_width),
                      static_cast<float>(q.texture_height), d.uv);
      ++g_layer_panels;
      said = bwvr::kCards;
    } else {
      if (q.texture_width == 0 || q.texture_height == 0) continue;
      if (q.view != 0) {
        d.source = kVideoView;
        d.handle = q.view;
        d.format = q.view_format;
      } else if (q.pixels != nullptr) {
        d.source = kVideoPixels;
        d.pixels = q.pixels;
        d.texture_width = q.texture_width;
        d.texture_height = q.texture_height;
        for (int k = 0; k < 4; ++k) d.rect[k] = q.rect[k];
        d.serial = g_copy_changed ? q.serial : 0;
      } else {
        continue;
      }
      d.card = q.index;
      lm::TextureRect(q.rect, static_cast<float>(q.texture_width),
                      static_cast<float>(q.texture_height), d.uv);
      if (q.room_panel) {
        ++g_layer_room_panels;
        said = kSaidRoomPanel;
      } else {
        ++g_layer_cards;
        if (q.index >= 0 && q.index < bwvr::kCards) said = q.index;
      }
    }
    ++n;
    if (said >= 0 && !g_said[said]) {
      g_said[said] = true;
      // The place in the area of the corners in front of the eye.
      float x0 = 2.0f, y0 = 2.0f, x1 = -1.0f, y1 = -1.0f;
      for (int c = 0; c < 4; ++c) {
        if (!(d.corner[c][3] > 0.0f)) continue;
        const float x = d.corner[c][0] / d.corner[c][3];
        const float y = d.corner[c][1] / d.corner[c][3];
        x0 = x < x0 ? x : x0;
        y0 = y < y0 ? y : y0;
        x1 = x > x1 ? x : x1;
        y1 = y > y1 ? y : y1;
      }
      const char* what =
          said < bwvr::kCards       ? kCardNames[said]
          : said == bwvr::kCards    ? "a panel of the interface"
          : said == kSaidRoomPanel  ? "a panel in the room (the places, the "
                                      "settings or the debug window)"
                                    : "a wall of the cut scene window";
      ModLog("video view   : %s is in the video for the first time, frame "
             "%llu, from %.2f, %.2f to %.2f, %.2f of the area (0 to 1 across "
             "and down), %s, from %s",
             what, ControllersFrameCount(), x0, y0, x1, y1,
             record.screen ? "over the screen in the room" : "over the world",
             d.source == kVideoView     ? "its copy on the GPU"
             : d.source == kVideoPixels ? "the pixels of the CPU"
             : d.source == kVideoPanel  ? "the image of the panel"
                                        : "a color");
    }
  }
  if (n > 0) {
    ++g_layer_windows;
    g_layer_draws += static_cast<uint64_t>(n);
  }
  return n;
}

void VideoViewReport() {
  if (!g_on || g_frames == 0) return;
  ModLog("video view   : %llu frames with the area, %llu to the window, the "
         "area %.0f by %.0f at %.0f, %.0f now, the head %.2f degrees ahead "
         "of the area at the most, marks in %llu eye passes, %llu failed",
         g_frames, g_windows, g_moved.x1 - g_moved.x0, g_moved.y1 - g_moved.y0,
         g_moved.x0, g_moved.y0, g_lag_window, g_mark_passes, g_mark_failed);
  if (g_layers && g_vulkan) {
    ModLog("video view   : %llu windows with quad layers, %llu quads drawn "
           "(%llu cards, %llu panels: %llu of the interface and %llu in the "
           "room, %llu walls), %llu quads outside the area",
           g_layer_windows, g_layer_draws, g_layer_cards,
           g_layer_panels + g_layer_room_panels, g_layer_panels,
           g_layer_room_panels, g_layer_walls, g_layer_outside);
  }
  g_frames = g_windows = g_mark_passes = g_mark_failed = 0;
  g_layer_windows = g_layer_draws = g_layer_outside = 0;
  g_layer_cards = g_layer_panels = g_layer_walls = g_layer_room_panels = 0;
  g_lag_window = 0.0f;
}

// THE SWITCHES of the debug page. Each change gives the pointing hand a
// pulse: long for on or a lower area, short for off or a higher area.
int VideoViewValue() {
  if (!g_vulkan) return kDebugCannot;
  return g_on ? 1 : 0;
}

void VideoViewChange(const char* by) {
  g_on = !g_on;
  if (!g_on) g_valid = false;
  WantLayers();
  ControllersGesturePulse(0.5f, g_on ? 0.30f : 0.08f);
  LogKeys(by);
}

int VideoViewMarksValue() {
  if (!g_vulkan || !g_on) return kDebugCannot;
  return g_marks ? 1 : 0;
}

void VideoViewMarksChange(const char* by) {
  if (!g_on) return;
  g_marks = !g_marks;
  ControllersGesturePulse(0.5f, g_marks ? 0.30f : 0.08f);
  LogKeys(by);
}

int VideoViewLayersValue() {
  if (!g_vulkan || !g_on) return kDebugCannot;
  return g_layers ? 1 : 0;
}

void VideoViewLayersChange(const char* by) {
  if (!g_on) return;
  g_layers = !g_layers;
  WantLayers();
  ControllersGesturePulse(0.5f, g_layers ? 0.30f : 0.08f);
  LogKeys(by);
}

int VideoViewDownValue() {
  if (!g_vulkan || !g_on) return kDebugCannot;
  return g_down;
}

static void StepHeight(bool lower, const char* by) {
  if (!g_on) return;
  g_down = vm::StepDown(g_down, lower);
  g_said_size = false;
  ControllersGesturePulse(0.5f, lower ? 0.24f : 0.08f);
  LogKeys(by);
}

void VideoViewLowerChange(const char* by) { StepHeight(true, by); }
void VideoViewHigherChange(const char* by) { StepHeight(false, by); }
