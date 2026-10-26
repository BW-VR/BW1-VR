// THE WAY OF A DRAW FROM THE GAME TO THE HEADSET APP (render_chain.h,
// docs/backend.md, "The way of a draw to the headset app").
//
// THE KEYS of bwvr.ini, read at the install of the backend of the mod on
// Vulkan (backend=vulkan):
//
//   vk_state_skip=1            a render state, a stage state or a texture
//                              with the value of the device does not go to
//                              the renderer and the stream. 0: each call goes
//   vk_stream_worker=1         a frame of the stream goes into the ring on a
//                              thread of the stream. 0: on the frame thread
//   vk_stream_surface_cache=1  the stream finds a surface that it has with
//                              its pixels in a table. 0: in its two sets
//   render_chain_time=0        1: the times of the steps of the mod with the
//                              clock of the CPU, in the report of each 450
//                              frames. 0: no clock read
//
// The test keys (test_keys=1) and the debug page (debug_menu=1) change the
// first three while the game runs: Ctrl Shift 8, Ctrl Shift 9 and Ctrl
// Shift 0.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <string.h>

#include "debug_menu.h"
#include "render_chain.h"

void ModLog(const char* format, ...);
void ControllersGesturePulse(float amplitude, float seconds);

namespace rc = render_chain_math;

bool g_chain_time = false;
bool g_chain_state_skip = true;
bool g_chain_worker = true;
bool g_chain_surface_cache = true;
rc::Window g_chain = {};
ChainStream g_chain_stream = {};
int g_chain_device_depth = 0;

namespace {

bool g_installed = false;
bool g_test_keys = false;
uint64_t g_walk_start = 0;
// The clock of the timers at the start of the window, and the performance
// counter at that time.
uint64_t g_window_tsc = 0;
int64_t g_window_qpc = 0;
LARGE_INTEGER g_qpc_hz = {};
double g_pair_ns = 0.0;  // the cost of two clock reads
volatile uint64_t g_sink = 0;

int64_t Qpc() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

// The cost of two reads of the clock: the least of 5 rounds of 2000 pairs.
double PairNs() {
  double best = 0.0;
  for (int round = 0; round < 5; ++round) {
    const int64_t q0 = Qpc();
    for (int i = 0; i < 2000; ++i) {
      const uint64_t a = __rdtsc();
      g_sink = g_sink + (__rdtsc() - a);
    }
    const int64_t q1 = Qpc();
    const double ns = static_cast<double>(q1 - q0) * 1e9 /
                      static_cast<double>(g_qpc_hz.QuadPart) / 2000.0;
    if (round == 0 || ns < best) best = ns;
  }
  return best;
}

// A key in another program is not for the game: the keys count only while a
// window of this process is in front.
bool GameInFront() {
  HWND window = GetForegroundWindow();
  if (window == nullptr) return false;
  DWORD process = 0;
  GetWindowThreadProcessId(window, &process);
  return process == GetCurrentProcessId();
}

void Pulse(bool on) { ControllersGesturePulse(0.5f, on ? 0.30f : 0.08f); }

void Toggle(bool* key, const char* name, const char* by, const char* when) {
  *key = !*key;
  Pulse(*key);
  ModLog("test key     : %s, %s is %d %s", by, name, *key ? 1 : 0, when);
}

}  // namespace

void RenderChainInstall() {
  if (g_installed) return;
  g_installed = true;
  char path[MAX_PATH];
  GetModuleFileNameA(GetModuleHandleA(nullptr), path, MAX_PATH);
  char* slash = strrchr(path, '\\');
  if (slash != nullptr) slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  g_chain_state_skip = GetPrivateProfileIntA("bwvr", "vk_state_skip", 1, path) != 0;
  g_chain_worker = GetPrivateProfileIntA("bwvr", "vk_stream_worker", 1, path) != 0;
  g_chain_surface_cache =
      GetPrivateProfileIntA("bwvr", "vk_stream_surface_cache", 1, path) != 0;
  g_chain_time = GetPrivateProfileIntA("bwvr", "render_chain_time", 0, path) != 0;
  g_test_keys = GetPrivateProfileIntA("bwvr", "test_keys", 0, path) != 0;
  QueryPerformanceFrequency(&g_qpc_hz);
  if (g_chain_time) g_pair_ns = PairNs();
  g_window_tsc = __rdtsc();
  g_window_qpc = Qpc();
  ModLog("render chain : vk_state_skip=%d. %s", g_chain_state_skip ? 1 : 0,
         g_chain_state_skip
             ? "A render state, a stage state or a texture that has the value "
               "of the device of the mod does not go to the renderer and the "
               "stream"
             : "Each render state, stage state and texture goes to the "
               "renderer and the stream");
  ModLog("render chain : vk_stream_worker=%d. %s", g_chain_worker ? 1 : 0,
         g_chain_worker
             ? "A frame of the live stream goes into the ring on a thread of "
               "the stream, and the frame thread goes on with the game"
             : "A frame of the live stream goes into the ring on the frame thread");
  ModLog("render chain : vk_stream_surface_cache=%d. %s",
         g_chain_surface_cache ? 1 : 0,
         g_chain_surface_cache
             ? "The stream finds a surface that it has with its pixels in a "
               "table of 512 places"
             : "The stream finds each surface in its two sets");
  ModLog("render chain : render_chain_time=%d. %s", g_chain_time ? 1 : 0,
         g_chain_time ? "The report of each 450 frames has the times of the "
                        "steps of the mod on the frame thread"
                      : "No time of the steps");
  if (g_chain_time) {
    ModLog("render chain : two reads of the clock of the CPU cost %.1f ns",
           g_pair_ns);
  }
}

void RenderChainFrame() {
  if (!g_test_keys) return;
  // Ctrl Shift 8, 9 and 0. The game has no key with 8, 9 or 0
  // (docs/controls-audit.md, section 3).
  static bool down[3] = {false, false, false};
  const int keys[3] = {'8', '9', '0'};
  const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
  for (int i = 0; i < 3; ++i) {
    const bool now = (GetAsyncKeyState(keys[i]) & 0x8000) != 0;
    if (now && !down[i] && ctrl && shift && GameInFront()) {
      if (i == 0) RenderChainStateSkipToggle("Ctrl, Shift and 8");
      if (i == 1) RenderChainWorkerToggle("Ctrl, Shift and 9");
      if (i == 2) RenderChainSurfaceCacheToggle("Ctrl, Shift and 0");
    }
    down[i] = now;
  }
}

void RenderChainWalkBegin() { g_walk_start = g_chain_time ? __rdtsc() : 0; }

void RenderChainWalkEnd() {
  if (g_walk_start != 0) {
    g_chain.ticks[rc::kWalk][rc::kOther] += rc::Elapsed(g_walk_start, __rdtsc());
  }
  g_walk_start = 0;
}

void RenderChainReport(unsigned long long frames) {
  if (!g_installed || frames == 0) return;
  const double f = static_cast<double>(frames);
  const rc::Window& w = g_chain;
  const ChainStream& s = g_chain_stream;
  ModLog("render chain : vk_state_skip=%d. A frame: %.1f draws, %.1f render "
         "states (%.1f with the value of the device), %.1f stage states (%.1f), "
         "%.1f textures (%.1f), %.1f other calls of the device. %.1f calls did "
         "not go to the renderer",
         g_chain_state_skip ? 1 : 0, w.calls[rc::kDraw] / f,
         w.calls[rc::kRenderState] / f, w.same[rc::kRenderState] / f,
         w.calls[rc::kStageState] / f, w.same[rc::kStageState] / f,
         w.calls[rc::kTexture] / f, w.same[rc::kTexture] / f,
         w.calls[rc::kOther] / f,
         (w.skipped[rc::kRenderState] + w.skipped[rc::kStageState] +
          w.skipped[rc::kTexture]) / f);
  ModLog("render chain : the stream, a frame: %.1f KB of records into the ring "
         "(%.1f KB of vertices, %.1f KB of indices), %.2f frames. "
         "vk_stream_worker=%d: %llu frames on the thread of the stream, and the "
         "frame thread waited for it %llu times. vk_stream_surface_cache=%d: "
         "%llu of %llu finds of a surface in the table",
         s.record_bytes / f / 1024.0, s.vertex_bytes / f / 1024.0,
         s.index_bytes / f / 1024.0, s.frames / f, g_chain_worker ? 1 : 0,
         s.worker_frames, s.waits, g_chain_surface_cache ? 1 : 0, s.cache_hits,
         s.finds);
  if (g_chain_time) {
    const uint64_t tsc = __rdtsc();
    const int64_t qpc = Qpc();
    const double mhz = rc::ClockMhz(tsc - g_window_tsc, qpc - g_window_qpc,
                                    g_qpc_hz.QuadPart);
    const double walk = rc::MsPerFrame(w.StepTicks(rc::kWalk), mhz, frames);
    const uint64_t device = w.StepTicks(rc::kDevice);
    const uint64_t renderer = w.StepTicks(rc::kRenderer);
    const uint64_t stream = w.StepTicks(rc::kStream);
    const double device_ms = rc::MsPerFrame(rc::Own(device, renderer), mhz, frames);
    // The copy into the ring at the end of a walk is in the call of the
    // device to the renderer, so the renderer has its own time without it.
    const double renderer_ms = rc::MsPerFrame(
        rc::Own(renderer, stream + w.handover_in_device), mhz, frames);
    const double stream_ms = rc::MsPerFrame(stream, mhz, frames);
    const double handover_ms = rc::MsPerFrame(w.StepTicks(rc::kHandover), mhz, frames);
    const double put_ms = rc::MsPerFrame(s.put_ticks, mhz, frames);
    const double mod_ms = device_ms + renderer_ms + stream_ms + handover_ms;
    ModLog("render chain : the frame thread, ms a frame: the scene walk %.3f, "
           "the device of the mod %.3f (its own time), the renderer of the mod "
           "%.3f (its own time), the record of the stream %.3f, the copy into "
           "the ring %.3f (the copy on the thread of the stream %.3f). The mod "
           "%.3f (%.1f percent of the walk). The clock %.1f MHz",
           walk, device_ms, renderer_ms, stream_ms, handover_ms, put_ms, mod_ms,
           rc::Percent(mod_ms, walk), mhz);
    // The inclusive time of the device for each kind of call.
    ModLog("render chain : the device of the mod with the renderer and the "
           "stream in it, ms a frame: the draws %.3f, the render states %.3f, the "
           "stage states %.3f, the textures %.3f, the other calls %.3f. The "
           "stream in it: the draws %.3f, the render states %.3f, the stage "
           "states %.3f, the textures %.3f",
           rc::MsPerFrame(w.ticks[rc::kDevice][rc::kDraw], mhz, frames),
           rc::MsPerFrame(w.ticks[rc::kDevice][rc::kRenderState], mhz, frames),
           rc::MsPerFrame(w.ticks[rc::kDevice][rc::kStageState], mhz, frames),
           rc::MsPerFrame(w.ticks[rc::kDevice][rc::kTexture], mhz, frames),
           rc::MsPerFrame(w.ticks[rc::kDevice][rc::kOther], mhz, frames),
           rc::MsPerFrame(w.ticks[rc::kStream][rc::kDraw], mhz, frames),
           rc::MsPerFrame(w.ticks[rc::kStream][rc::kRenderState], mhz, frames),
           rc::MsPerFrame(w.ticks[rc::kStream][rc::kStageState], mhz, frames),
           rc::MsPerFrame(w.ticks[rc::kStream][rc::kTexture], mhz, frames));
    g_window_tsc = tsc;
    g_window_qpc = qpc;
  }
  g_chain.Clear();
  g_chain_stream = ChainStream();
}

int RenderChainStateSkipValue() { return g_installed ? (g_chain_state_skip ? 1 : 0) : kDebugCannot; }
void RenderChainStateSkipToggle(const char* by) {
  Toggle(&g_chain_state_skip, "vk_state_skip", by, "from the next call");
}
int RenderChainWorkerValue() { return g_installed ? (g_chain_worker ? 1 : 0) : kDebugCannot; }
void RenderChainWorkerToggle(const char* by) {
  Toggle(&g_chain_worker, "vk_stream_worker", by, "from the next frame of the stream");
}
int RenderChainSurfaceCacheValue() {
  return g_installed ? (g_chain_surface_cache ? 1 : 0) : kDebugCannot;
}
void RenderChainSurfaceCacheToggle(const char* by) {
  Toggle(&g_chain_surface_cache, "vk_stream_surface_cache", by, "from the next find");
}
