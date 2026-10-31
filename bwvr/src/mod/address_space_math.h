// The address space of the process, for the report (address_space.cpp,
// docs/backend.md, "The address space"). Pure: no Windows. Checks in
// test/controller_math/address_space_checks.h.
//
// The process of the game has 2 GB of address space, or 4 GB on a Windows
// of 64 bits when runblack.exe has the flag LARGE_ADDRESS_AWARE
// (large_address.h). VirtualAlloc starts each block at a multiple of 64 KB
// (the granularity), so a block takes the address space up to the next
// multiple of 64 KB, and a free region is of use only from its first
// multiple of 64 KB.
#pragma once

#include <stdint.h>

namespace address_space_math {

const uint32_t kGranularity = 65536;

// The address space that VirtualAlloc takes for a block of `bytes`.
inline uint64_t ReservedBytes(uint64_t bytes) {
  return (bytes + kGranularity - 1) / kGranularity * kGranularity;
}

// The bytes of a free region from its first multiple of 64 KB to its end,
// the part where VirtualAlloc can put a block.
inline uint64_t UsableBytes(uint64_t base, uint64_t size) {
  const uint64_t start = ReservedBytes(base);
  const uint64_t end = base + size;
  return end > start ? end - start : 0;
}

// One region of VirtualQuery.
enum State { kFree, kReserved, kCommitted };
enum Type { kNone, kImage, kMapped, kPrivate };
struct Region {
  uint64_t base;
  uint64_t size;
  State state;
  Type type;
};

// The sums of a walk of the regions.
struct Totals {
  uint64_t free;       // the usable bytes of the free regions
  uint64_t used;       // reserved and committed
  uint64_t committed;
  uint64_t image;      // the modules (EXE and DLL)
  uint64_t mapped;     // the views of files and of shared memory
  uint64_t priv;       // VirtualAlloc and the heaps
  uint64_t largest_free;  // the largest block that VirtualAlloc can make
  uint32_t free_blocks;   // the free regions with a usable part
  uint32_t regions;
};

inline void Add(Totals* t, const Region& r) {
  ++t->regions;
  if (r.state == kFree) {
    const uint64_t usable = UsableBytes(r.base, r.size);
    if (usable == 0) return;
    t->free += usable;
    ++t->free_blocks;
    if (usable > t->largest_free) t->largest_free = usable;
    return;
  }
  t->used += r.size;
  if (r.state == kCommitted) t->committed += r.size;
  if (r.type == kImage) {
    t->image += r.size;
  } else if (r.type == kMapped) {
    t->mapped += r.size;
  } else {
    t->priv += r.size;
  }
}

}  // namespace address_space_math
