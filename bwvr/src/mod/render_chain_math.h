// THE WAY OF A DRAW FROM THE GAME TO THE HEADSET APP: the pure rules of
// render_chain.cpp, vk_ddraw.cpp and vk_stream.cpp (docs/backend.md, "The
// way of a draw to the headset app"). No Windows code, so the math tests
// check it.
#pragma once

#include <stdint.h>
#include <string.h>

namespace render_chain_math {

// THE CALLS THAT CHANGE NOTHING (vk_state_skip=1). The device of the mod
// keeps each state of Direct3D 7, and a walk of the renderer and of the
// stream starts with the whole state of the device. So a call that sets the
// value that the device has already does not go to the renderer and to the
// stream: their state is that value.

// A render state: the device has the value.
inline bool SkipRenderState(uint32_t now, uint32_t value) { return now == value; }

// The D3DTSS_ADDRESS (12) of Direct3D 7 also sets D3DTSS_ADDRESSU (13) and
// D3DTSS_ADDRESSV (14). It changes nothing only when all three have the
// value. Each other stage state changes nothing when it has the value.
const uint32_t kAddress = 12;
const uint32_t kAddressU = 13;
const uint32_t kAddressV = 14;
inline bool SkipStageState(const uint32_t* stage_states, uint32_t count,
                           uint32_t type, uint32_t value) {
  if (stage_states == nullptr || type >= count) return false;
  if (type == kAddress) {
    if (kAddressV >= count) return false;
    return stage_states[kAddress] == value && stage_states[kAddressU] == value &&
           stage_states[kAddressV] == value;
  }
  return stage_states[type] == value;
}

// A texture: the stage has that surface, and no surface got a write of the
// game since the renderer and the stream got the texture of the stage. A
// write makes the stream send the pixels again at the next use of the
// surface as a texture, so a texture after a write always goes.
inline bool SkipTexture(const void* now, const void* surface,
                        uint32_t writes_now, uint32_t writes_at_set) {
  return now == surface && writes_now == writes_at_set;
}

// THE SURFACES THAT THE STREAM HAS WITH THEIR PIXELS
// (vk_stream_surface_cache=1). A table with one place for each hash of the
// id of a surface. A place holds an id whose surface the stream has with
// its pixels. A write of the game into the surface removes it, and a frame
// with the whole state clears the table. So a find in the table is true
// only when the sets of the stream (g_known, g_with_pixels) have the id.
class SurfaceCache {
 public:
  static const uint32_t kPlaceBits = 9;
  static const uint32_t kPlaces = 1u << kPlaceBits;
  SurfaceCache() { Clear(); }
  void Clear() { memset(ids_, 0, sizeof(ids_)); }
  // True when the stream has the surface with its pixels.
  bool Has(uint64_t id) const { return id != 0 && ids_[Place(id)] == id; }
  // The stream has the surface with its pixels now. The place of another id
  // with the same hash gets this id: a later find of that id is false, and
  // the sets of the stream answer.
  void Put(uint64_t id) {
    if (id != 0) ids_[Place(id)] = id;
  }
  // The game wrote the surface: the stream sends its pixels again.
  void Drop(uint64_t id) {
    if (id != 0 && ids_[Place(id)] == id) ids_[Place(id)] = 0;
  }
  static uint32_t Place(uint64_t id) {
    // The surfaces are objects of the heap: the low 4 bits are the same.
    // The high bits of the product with the golden ratio mix each bit of
    // the id.
    const uint64_t h = (id >> 4) * 0x9E3779B97F4A7C15ull;
    return static_cast<uint32_t>(h >> (64 - kPlaceBits));
  }

 private:
  uint64_t ids_[kPlaces];
};

// THE TIMES OF THE STEPS (render_chain_time=1). The ticks of the clock of
// the timers, inclusive: the device includes the renderer, and the renderer
// includes the stream.
enum Step {
  kDevice = 0,    // the methods of the device of the mod (vk_ddraw.cpp)
  kRenderer = 1,  // the calls of the device to the renderer (vk_scene.cpp)
  kStream = 2,    // the record of the stream (vk_stream.cpp)
  kHandover = 3,  // the copy into the ring, or the wait for its thread
  kWalk = 4,      // the scene walk, from BeginScene to EndScene
  kSteps = 5
};

// The kinds of calls.
enum Call {
  kDraw = 0,
  kRenderState = 1,
  kStageState = 2,
  kTexture = 3,
  kOther = 4,  // BeginScene, EndScene, Clear, SetViewport, the state blocks
  kCalls = 5
};

struct Window {
  uint64_t ticks[kSteps][kCalls];
  uint64_t calls[kCalls];       // the calls of the device of each kind
  uint64_t same[kCalls];        // with the value of the device (they change nothing)
  uint64_t skipped[kCalls];     // not given to the renderer (vk_state_skip=1)
  // The part of kHandover in a method of the device: the end of a walk
  // gives the frame to the ring in the call of the device to the renderer.
  uint64_t handover_in_device;
  void Clear() { memset(this, 0, sizeof(*this)); }
  uint64_t StepTicks(int step) const {
    uint64_t t = 0;
    for (int c = 0; c < kCalls; ++c) t += ticks[step][c];
    return t;
  }
};

// The ticks from `start` to `now`. 0 when the clock of this core is behind
// the clock of the core of the start: a thread can move to another core, and
// the counters of the cores of a virtual machine can differ.
inline uint64_t Elapsed(uint64_t start, uint64_t now) {
  return now > start ? now - start : 0;
}

// The own time of a step: its inclusive time less the time of the step in
// it. Never below 0.
inline uint64_t Own(uint64_t inclusive, uint64_t inner) {
  return inclusive > inner ? inclusive - inner : 0;
}

// The ms of a frame: `ticks` of a clock of `mhz` over `frames` frames.
inline double MsPerFrame(uint64_t ticks, double mhz, uint64_t frames) {
  if (mhz <= 0.0 || frames == 0) return 0.0;
  return static_cast<double>(ticks) / (mhz * 1000.0) / static_cast<double>(frames);
}

// The rate of the clock of the timers in MHz: `ticks` of it in `qpc` counts
// of a performance counter of `qpc_hz`.
inline double ClockMhz(uint64_t ticks, int64_t qpc, int64_t qpc_hz) {
  if (qpc <= 0 || qpc_hz <= 0) return 0.0;
  return static_cast<double>(ticks) * static_cast<double>(qpc_hz) /
         static_cast<double>(qpc) / 1e6;
}

// A share in percent, 0 for no whole.
inline double Percent(double part, double whole) {
  return whole > 0.0 ? part * 100.0 / whole : 0.0;
}

}  // namespace render_chain_math
