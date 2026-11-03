// THE WAY OF A DRAW FROM THE GAME TO THE HEADSET APP (render_chain.cpp,
// docs/backend.md, "The way of a draw to the headset app").
//
// With backend=vulkan a draw of the game goes on the frame thread through
// the device of the mod (vk_ddraw.cpp), the renderer of the mod
// (vk_scene.cpp) and the record of the stream (vk_stream.cpp), and each
// frame of records goes into the ring for the headset app. This file has the
// keys of the changes to that way, the counts of the calls, and the times of
// each step with render_chain_time=1.
#pragma once

#include <intrin.h>
#include <stdint.h>

#include "bar_split_math.h"
#include "render_chain_math.h"

// The keys of bwvr.ini, read at the install of the backend. The test keys
// and the debug page change them while the game runs.
extern bool g_chain_time;           // render_chain_time=1
extern bool g_chain_state_skip;     // vk_state_skip=1
extern bool g_chain_worker;         // vk_stream_worker=1
extern bool g_chain_surface_cache;  // vk_stream_surface_cache=1

// The counts and the times of the window of the report. The frame thread
// alone writes them.
extern render_chain_math::Window g_chain;

// The counts of the stream in the window of the report (vk_stream.cpp).
struct ChainStream {
  uint64_t record_bytes;  // the bytes of the frames that went to the ring
  uint64_t vertex_bytes;  // the vertices in the records of the draws
  uint64_t index_bytes;   // the indices in the records of the draws
  uint64_t frames;        // the frames that went to the ring
  uint64_t worker_frames; // of them on the thread of the stream
  uint64_t waits;         // the waits of the frame thread for that thread
  uint64_t wait_ticks;    // the ticks of those waits (render_chain_time=1)
  uint64_t put_ticks;     // the ticks of the copies on that thread
  uint64_t finds;         // the finds of a surface (vk_stream_surface_cache=1)
  uint64_t cache_hits;    // the finds that the cache answered
};
extern ChainStream g_chain_stream;

// The running sum of the calls of the device and their ticks, for the split of
// the bars of the timeline into the game and the device (bar_split_math.h,
// trace_device=1). It never goes back and the frame thread alone writes it.
// The ticks count only while g_chain_acc_on is true.
extern bar_split_math::Acc g_chain_acc;
extern bool g_chain_acc_on;

// The time of a step, from its start to the end of the scope. The time of a
// scope of the device counts only when no scope of the device is open (a
// state block calls the methods of the device). Each call counts.
extern int g_chain_device_depth;

class ChainScope {
 public:
  ChainScope(int step, int call)
      : step_(step), call_(call), start_(g_chain_time ? __rdtsc() : 0) {}
  ~ChainScope() {
    if (start_ != 0) g_chain.ticks[step_][call_] += render_chain_math::Elapsed(start_, __rdtsc());
  }

 private:
  int step_;
  int call_;
  uint64_t start_;
};

class ChainDeviceScope {
 public:
  explicit ChainDeviceScope(int call)
      : call_(call),
        start_((g_chain_time || g_chain_acc_on) && g_chain_device_depth == 0 ? __rdtsc() : 0) {
    ++g_chain_device_depth;
    ++g_chain.calls[call];
    if (g_chain_acc_on) ++g_chain_acc.calls[call];
  }
  ~ChainDeviceScope() {
    --g_chain_device_depth;
    if (start_ != 0) {
      const uint64_t elapsed = render_chain_math::Elapsed(start_, __rdtsc());
      if (g_chain_time) g_chain.ticks[render_chain_math::kDevice][call_] += elapsed;
      if (g_chain_acc_on) g_chain_acc.ticks[call_] += elapsed;
    }
  }

 private:
  int call_;
  uint64_t start_;
};

// The install with the backend of the mod on Vulkan: reads the keys.
void RenderChainInstall();
// One time a frame on the frame thread (the present of vk_ddraw.cpp): the
// test keys.
void RenderChainFrame();
// The start and the end of a scene walk (render_chain_time=1).
void RenderChainWalkBegin();
void RenderChainWalkEnd();
// The lines of the report of each 450 frames.
void RenderChainReport(unsigned long long frames);

// The switches of the debug page and of the test keys.
int RenderChainStateSkipValue();
void RenderChainStateSkipToggle(const char* by);
int RenderChainWorkerValue();
void RenderChainWorkerToggle(const char* by);
int RenderChainSurfaceCacheValue();
void RenderChainSurfaceCacheToggle(const char* by);
