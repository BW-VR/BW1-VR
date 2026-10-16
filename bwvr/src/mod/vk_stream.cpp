// THE RECORD OF THE WALKS FOR THE HEADSET APP (vk_stream.h).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdlib.h>

#include <set>

#include "../common/bw_addresses.h"
#include "one_walk.h"
#include "ui_panel.h"
#include "vk_prim_math.h"
#include "vk_scene.h"
#include "vk_stream.h"

void ModLog(const char* format, ...);
int StereoPassEye();
int IntroPassEye();

bool g_vk_stream_on = false;

namespace {

namespace ss = scene_stream;

int g_record_frame = 0;
int g_frames = 3;
uint64_t g_first = 0;
uint64_t g_last = 0;
bool g_done = false;
ss::StartBody g_options = {};
ss::Writer g_writer;
// The surfaces in the stream, and the ones that the game wrote since their
// pixels went in.
std::set<uint64_t> g_known;
std::set<uint64_t> g_with_pixels;
uint64_t g_draws = 0;
uint64_t g_surface_bytes = 0;

uint64_t Id(const void* p) { return reinterpret_cast<uintptr_t>(p); }

ss::StreamState State() {
  ss::StreamState s = {};
  s.pass_eye = StereoPassEye();
  s.intro_eye = IntroPassEye();
  s.one_walk = OneWalkActive() ? 1u : 0u;
  s.shift_per_rhw = OneWalkShiftPerRhw();
  s.bound_target = Id(OneWalkBoundTarget());
  s.one_walk_mode = OneWalkModeNow();
  return s;
}

// A surface goes into the stream before the first record that names it.
// pixels: also its pixels, for a texture. A surface that the game wrote goes
// in again at its next use as a texture.
void Surface(void* surface, bool pixels) {
  if (surface == nullptr) return;
  const uint64_t id = Id(surface);
  const bool known = g_known.count(id) != 0;
  const bool have = g_with_pixels.count(id) != 0;
  if (known && (!pixels || have)) return;
  ss::SurfaceBody body = {};
  body.surface = id;
  DDSCAPS2 caps = {};
  if (SUCCEEDED(static_cast<IDirectDrawSurface7*>(surface)->GetCaps(&caps))) {
    body.caps = caps.dwCaps;
  }
  uint8_t* rgba = nullptr;
  if (pixels && VkSceneReadSurfaceRgba(surface, &body.width, &body.height, &rgba)) {
    body.has_pixels = 1;
    const uint32_t bytes = body.width * body.height * 4;
    g_writer.Record(ss::kSurface, &body, sizeof(body), rgba, bytes);
    g_surface_bytes += bytes;
    free(rgba);
    g_with_pixels.insert(id);
  } else {
    g_writer.Record(ss::kSurface, &body, sizeof(body));
  }
  g_known.insert(id);
}

void Finish() {
  g_vk_stream_on = false;
  g_done = true;
  char path[MAX_PATH];
  GetModuleFileNameA(GetModuleHandleA(nullptr), path, MAX_PATH);
  char* slash = strrchr(path, '\\');
  if (slash != nullptr) slash[1] = 0;
  strcat_s(path, "bwvr-stream.bin");
  FILE* f = nullptr;
  const std::vector<uint8_t>& bytes = g_writer.Bytes();
  bool ok = false;
  if (fopen_s(&f, path, "wb") == 0 && f != nullptr) {
    ok = fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    fclose(f);
  }
  ModLog("vk stream    : %s frames %llu to %llu, %llu draws, %zu surfaces "
         "(%llu bytes of pixels), %zu bytes, to %s",
         ok ? "wrote" : "could not write", g_first, g_last, g_draws,
         g_known.size(), g_surface_bytes, bytes.size(), path);
  g_writer.Clear();
}

}  // namespace

void VkStreamSetOptions(int record_frame, int frames) {
  g_record_frame = record_frame > 0 ? record_frame : 0;
  g_frames = frames > 0 ? frames : 3;
  if (g_record_frame > 0) {
    ModLog("vk stream    : vk_stream_record=%d, vk_stream_frames=%d. The "
           "calls of the Vulkan renderer of these frames go to "
           "bwvr-stream.bin (a test aid for the headset app)",
           g_record_frame, g_frames);
  }
}

scene_stream::StartBody* VkStreamOptions() { return &g_options; }

void VkStreamFrame(uint64_t frame, uint64_t name, bool capture) {
  if (g_record_frame == 0 || g_done) return;
  if (!g_vk_stream_on && frame == static_cast<uint64_t>(g_record_frame)) {
    if (g_options.direct == 0) {
      ModLog("vk stream    : no record: it needs backend=vulkan");
      g_done = true;
      return;
    }
    g_vk_stream_on = true;
    g_first = frame;
    g_writer.Clear();
    g_writer.Record(ss::kStart, &g_options, sizeof(g_options));
  }
  if (!g_vk_stream_on) return;
  if (frame >= g_first + static_cast<uint64_t>(g_frames)) {
    g_writer.Record(ss::kFrameEnd, &g_last, sizeof(g_last));
    Finish();
    return;
  }
  if (frame != g_first) g_writer.Record(ss::kFrameEnd, &g_last, sizeof(g_last));
  g_last = frame;
  ss::FrameBody body = {frame, name, capture ? 1u : 0u, 0};
  g_writer.Record(ss::kFrame, &body, sizeof(body));
}

void VkStreamOnlyFrame(bool can) {
  const uint32_t v = can ? 1u : 0u;
  g_writer.Record(ss::kOnlyFrame, &v, sizeof(v));
}

void VkStreamGaze(bool valid, float gx, float gy) {
  ss::GazeBody body = {valid ? 1u : 0u, gx, gy};
  g_writer.Record(ss::kGaze, &body, sizeof(body));
}

void VkStreamWalkBegin(void* device7) {
  if (device7 == nullptr) return;
  IDirect3DDevice7* d = static_cast<IDirect3DDevice7*>(device7);
  ss::WalkBeginBody body = {};
  body.device = Id(device7);
  body.state = State();
  for (int i = 1; i < ss::kStates; ++i) {
    DWORD v = 0;
    if (SUCCEEDED(d->GetRenderState(static_cast<D3DRENDERSTATETYPE>(i), &v))) body.rs[i] = v;
  }
  for (int stage = 0; stage < 2; ++stage) {
    for (int i = 1; i < ss::kStageStates; ++i) {
      DWORD v = 0;
      if (SUCCEEDED(d->GetTextureStageState(stage, static_cast<D3DTEXTURESTAGESTATETYPE>(i), &v))) {
        body.ts[stage][i] = v;
      }
    }
    IDirectDrawSurface7* texture = nullptr;
    if (SUCCEEDED(d->GetTexture(stage, &texture)) && texture != nullptr) {
      Surface(texture, true);
      body.texture[stage] = Id(texture);
      texture->Release();
    }
  }
  D3DVIEWPORT7 v = {};
  d->GetViewport(&v);
  body.viewport = {v.dwX, v.dwY, v.dwWidth, v.dwHeight, v.dvMinZ, v.dvMaxZ};
  const bw::tech::InfoTransform* info =
      reinterpret_cast<const bw::tech::InfoTransform*>(bw::tech::kInfoTransform);
  body.transform = {info->inv_half_tan_fov_x, info->res_x, info->res_y, 0};
  if (body.state.bound_target != 0) Surface(OneWalkBoundTarget(), false);
  g_writer.Record(ss::kWalkBegin, &body, sizeof(body));
}

void VkStreamWalkEnd() {
  const ss::StreamState state = State();
  g_writer.Record(ss::kWalkEnd, &state, sizeof(state));
}

void VkStreamRenderState(uint32_t state, uint32_t value) {
  const uint32_t body[2] = {state, value};
  g_writer.Record(ss::kRenderState, body, sizeof(body));
}

void VkStreamStageState(uint32_t stage, uint32_t type, uint32_t value) {
  const uint32_t body[3] = {stage, type, value};
  g_writer.Record(ss::kStageState, body, sizeof(body));
}

void VkStreamTexture(uint32_t stage, void* surface) {
  Surface(surface, true);
  struct {
    uint32_t stage;
    uint32_t pad;
    uint64_t surface;
  } body = {stage, 0, Id(surface)};
  g_writer.Record(ss::kTexture, &body, sizeof(body));
}

void VkStreamDraw(uint32_t type, uint32_t fvf, const void* vertices,
                  uint32_t vertex_count, const uint16_t* indices,
                  uint32_t index_count) {
  ss::DrawBody body = {};
  body.state = State();
  body.type = type;
  body.fvf = fvf;
  body.vertex_count = vertex_count;
  body.index_count = indices != nullptr ? index_count : 0;
  body.vertex_bytes = vk_prim_math::FvfStride(fvf) * vertex_count;
  if (vertices == nullptr) body.vertex_bytes = 0;
  g_writer.Record(ss::kDraw, &body, sizeof(body), vertices, body.vertex_bytes,
                  indices, body.index_count * 2);
  ++g_draws;
}

void VkStreamTarget(void* surface) {
  Surface(surface, false);
  ss::TargetBody body = {};
  body.state = State();
  body.state.panel_target = GpuBridgeIsPanel(surface) ? Id(surface) : 0;
  body.surface = Id(surface);
  g_writer.Record(ss::kTarget, &body, sizeof(body));
}

void VkStreamViewport(const void* viewport7) {
  if (viewport7 == nullptr) return;
  const D3DVIEWPORT7* v = static_cast<const D3DVIEWPORT7*>(viewport7);
  ss::ViewportBody body = {v->dwX, v->dwY, v->dwWidth, v->dwHeight, v->dvMinZ, v->dvMaxZ};
  g_writer.Record(ss::kViewport, &body, sizeof(body));
}

void VkStreamClear(uint32_t count, const void* rects, uint32_t flags,
                   uint32_t color, float z) {
  ss::ClearBody body = {rects != nullptr ? count : 0, flags, color, z};
  g_writer.Record(ss::kClear, &body, sizeof(body), rects,
                  body.count * static_cast<uint32_t>(sizeof(D3DRECT)));
}

void VkStreamPanelBind(bool panel) {
  const uint32_t v = panel ? 1u : 0u;
  g_writer.Record(ss::kPanelBind, &v, sizeof(v));
}

void VkStreamWritten(void* surface) {
  if (surface == nullptr) return;
  Surface(surface, false);
  g_with_pixels.erase(Id(surface));
  const uint64_t id = Id(surface);
  g_writer.Record(ss::kWritten, &id, sizeof(id));
}
