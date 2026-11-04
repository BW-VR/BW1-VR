// The rules of the draws of the walk for the Vulkan renderer (vk_scene.cpp,
// docs/backend.md, "Fewer state changes and fewer draws"): the bound state
// of a command buffer (vk_bind_skip), the draws that the sort may move
// (vk_draw_sort), and the draws that one draw can replace (vk_draw_merge).
// Pure: no Windows, no Vulkan. Checks in
// test/controller_math/vk_draw_checks.h.
#pragma once

#include <stdint.h>
#include <string.h>

#include <algorithm>

namespace vk_draw_math {

// THE BOUND STATE. Each kind of state that a command buffer keeps has a
// slot. Need gives true when the value differs from the bound value or
// when no value is bound, and then it keeps the value as the bound value.
// Else the bind is not necessary, and the slot counts a skip.
enum StateSlot {
  kPipeline,
  kSets,
  kVertices,
  kIndices,
  kCullMode,
  kFrontFace,
  kDepthTest,
  kDepthWrite,
  kDepthCompare,
  kDepthBias,
  kTopology,
  kViewport,
  kScissor,
  kPush,
  kStateSlots
};

const size_t kMostStateBytes = 128;

class BoundState {
 public:
  // on false: Need always gives true, as with no bound state (the old way).
  void SetOn(bool on) { on_ = on; }
  bool On() const { return on_; }
  // A new command buffer, or a bind that went past this state.
  void Reset() {
    for (bool& k : known_) k = false;
  }
  void Forget(StateSlot slot) { known_[slot] = false; }
  bool Need(StateSlot slot, const void* value, size_t bytes) {
    if (bytes > kMostStateBytes) {
      known_[slot] = false;
      ++made_[slot];
      return true;
    }
    if (on_ && known_[slot] && sizes_[slot] == bytes &&
        memcmp(values_[slot], value, bytes) == 0) {
      ++skipped_[slot];
      return false;
    }
    memcpy(values_[slot], value, bytes);
    sizes_[slot] = bytes;
    known_[slot] = true;
    ++made_[slot];
    return true;
  }
  uint64_t Skipped(StateSlot slot) const { return skipped_[slot]; }
  uint64_t Made(StateSlot slot) const { return made_[slot]; }
  uint64_t SkippedAll() const {
    uint64_t n = 0;
    for (uint64_t s : skipped_) n += s;
    return n;
  }

 private:
  bool on_ = true;
  bool known_[kStateSlots] = {};
  size_t sizes_[kStateSlots] = {};
  uint8_t values_[kStateSlots][kMostStateBytes] = {};
  uint64_t skipped_[kStateSlots] = {};
  uint64_t made_[kStateSlots] = {};
};

// The values of Direct3D 7 that the rules read.
const uint32_t kLess = 2;       // D3DCMP_LESS
const uint32_t kLessEqual = 4;  // D3DCMP_LESSEQUAL
const uint32_t kTriangleList = 4;  // D3DPT_TRIANGLELIST
const uint32_t kVertexBytes = 32;  // the format 0x1C4 in the ring

// A draw of the walk as the rules see it. The handles are the bits of the
// Vulkan handles.
struct Draw {
  uint64_t pipeline;
  uint64_t set, set1;  // set1 0: no texture stage 1
  uint32_t blend;      // the blend of the pipeline, 0 with no blend
  uint32_t cull, zenable, zwrite, zfunc, type;  // D3D values
  const void* push;    // the push constants
  uint32_t push_bytes;
  uint64_t vertex_at, uv1_at, index_at;  // bytes in the ring
  uint32_t vertex_count, index_count;    // index_count 0: no indices
};

// THE SORT. A draw may move when its order does not change the picture: no
// blend, a depth test that keeps the nearer pixel, and a depth write. Then
// each pixel shows the nearest draw, in each order. Only two draws at the
// same depth in the same pixel can show another draw after the sort.
inline bool MayMove(const Draw& d) {
  return d.blend == 0 && d.zenable != 0 && d.zwrite != 0 &&
         (d.zfunc == kLess || d.zfunc == kLessEqual);
}

inline bool SortBefore(const Draw& a, const Draw& b) {
  if (a.pipeline != b.pipeline) return a.pipeline < b.pipeline;
  if (a.set != b.set) return a.set < b.set;
  return a.set1 < b.set1;
}

// The order of the draws d[0] to d[n - 1] into order[0] to order[n - 1]
// (indices of d). A row of draws that may move gets a stable sort by
// pipeline and descriptor sets. A draw that may not move keeps its place,
// and no draw moves past it. With sort false the order stays. Gives the
// count of draws that the sort gave another place.
inline uint32_t SortDraws(const Draw* d, uint32_t n, bool sort,
                          uint32_t* order) {
  for (uint32_t i = 0; i < n; ++i) order[i] = i;
  if (!sort) return 0;
  uint32_t moved = 0;
  uint32_t i = 0;
  while (i < n) {
    if (!MayMove(d[order[i]])) {
      ++i;
      continue;
    }
    uint32_t end = i + 1;
    while (end < n && MayMove(d[order[end]])) ++end;
    std::stable_sort(order + i, order + end, [d](uint32_t a, uint32_t b) {
      return SortBefore(d[a], d[b]);
    });
    for (uint32_t k = i; k < end; ++k) {
      if (order[k] != k) ++moved;
    }
    i = end;
  }
  return moved;
}

// THE MERGE. The triangles of a draw: a list draws full triangles only, as
// Direct3D draws them.
inline uint32_t ListIndices(const Draw& d) {
  const uint32_t n = d.index_count > 0 ? d.index_count : d.vertex_count;
  return n - n % 3;
}

// True when one draw with the vertices of `first` as its base can draw
// `next` after the draws that it has. The two draws have the same state and
// the same push constants, both are triangle lists with no texture stage 1,
// and the vertices of next are after the base in the ring at a full
// vertex. Then the GPU draws the triangles of the one draw in the same
// order as the two draws, so the picture is the same.
inline bool CanJoin(const Draw& first, const Draw& next) {
  return first.pipeline == next.pipeline && first.set == next.set &&
         first.set1 == 0 && next.set1 == 0 && first.blend == next.blend &&
         first.cull == next.cull && first.zenable == next.zenable &&
         first.zwrite == next.zwrite && first.zfunc == next.zfunc &&
         first.type == kTriangleList && next.type == kTriangleList &&
         first.push_bytes == next.push_bytes &&
         memcmp(first.push, next.push, first.push_bytes) == 0 &&
         next.vertex_at >= first.vertex_at &&
         (next.vertex_at - first.vertex_at) % kVertexBytes == 0 &&
         (next.vertex_at - first.vertex_at) / kVertexBytes < 0x40000000ull &&
         ListIndices(next) > 0;
}

// The indices of draw d in the one draw with the base `first` (32 bit), in
// out[0] to out[ListIndices(d) - 1]. indices: the 16 bit indices of d, or
// null when d has none.
inline void JoinIndices(const Draw& first, const Draw& d,
                        const uint16_t* indices, uint32_t* out) {
  const uint32_t base =
      static_cast<uint32_t>((d.vertex_at - first.vertex_at) / kVertexBytes);
  const uint32_t n = ListIndices(d);
  for (uint32_t i = 0; i < n; ++i) {
    out[i] = base + (indices != nullptr ? indices[i] : i);
  }
}

}  // namespace vk_draw_math
