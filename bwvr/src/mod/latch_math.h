// THE TABLE OF THE LATCHED 3D PARTS (gpu_mesh_latch, docs/software-tnl.md, "The
// latch"). Pure logic with no game, so the hook and the checks use it.
//
// A part is latched until a frame. The table is a hash of 4096 entries with a
// probe of 8. A snow land has hundreds of parts with a layer in view at the same
// time, so a short table (the first version had 64) lost a latch before the next
// layer could keep it, and the log showed tens of thousands of new latches in 10
// s. When all 8 places of a key are live, the entry with the earliest end goes.
#pragma once

#include <stdint.h>

namespace latch_math {

const uint32_t kSize = 4096;
const uint32_t kProbe = 8;

struct Entry {
  uintptr_t part;
  uint32_t until;
};

class PartTable {
 public:
  // True while the part is latched at this frame.
  bool Has(uintptr_t part, uint32_t frame) const {
    if (part == 0) return false;
    uint32_t at = Hash(part);
    for (uint32_t i = 0; i < kProbe; ++i, at = (at + 1) & (kSize - 1)) {
      if (e_[at].part == part) return frame < e_[at].until;
    }
    return false;
  }
  // Latches the part until `until`. 0: it was latched and the end moved, 1: a new
  // latch, 2: a new latch in the place of a live entry of another part.
  int Set(uintptr_t part, uint32_t frame, uint32_t until) {
    uint32_t at = Hash(part);
    uint32_t free_at = kSize;
    uint32_t oldest = kSize;
    for (uint32_t i = 0; i < kProbe; ++i, at = (at + 1) & (kSize - 1)) {
      if (e_[at].part == part) {
        const bool live = frame < e_[at].until;
        e_[at].until = until;
        return live ? 0 : 1;
      }
      if (e_[at].part == 0 || frame >= e_[at].until) {
        if (free_at == kSize) free_at = at;
      } else if (oldest == kSize || e_[at].until < e_[oldest].until) {
        oldest = at;
      }
    }
    const uint32_t place = free_at != kSize ? free_at : oldest;
    const int result = free_at != kSize ? 1 : 2;
    e_[place].part = part;
    e_[place].until = until;
    return result;
  }
  // Clears the ended entries. Gives the count of live ones.
  uint32_t Prune(uint32_t frame) {
    uint32_t live = 0;
    for (Entry& e : e_) {
      if (e.part == 0) continue;
      if (frame >= e.until) {
        e.part = 0;
      } else {
        ++live;
      }
    }
    return live;
  }

 private:
  static uint32_t Hash(uintptr_t part) {
    return (static_cast<uint32_t>(part >> 4) * 2654435761u) >> 20 & (kSize - 1);
  }
  Entry e_[kSize] = {};
};

}  // namespace latch_math
