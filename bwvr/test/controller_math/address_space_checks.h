// The address space of the process, a test (address_space_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/address_space_math.h"

namespace address_space_checks {

inline void AddressSpaceChecks() {
  using namespace address_space_math;
  printf("\nThe address space of the process\n");

  // A block of VirtualAlloc takes a whole number of 64 KB.
  Check(ReservedBytes(0) == 0, "0 bytes take no address space",
        static_cast<double>(ReservedBytes(0)), 0);
  Check(ReservedBytes(2) == 65536,
        "a mip level of 1 by 1 pixels takes 64 KB",
        static_cast<double>(ReservedBytes(2)), 65536);
  Check(ReservedBytes(65536) == 65536, "64 KB take 64 KB",
        static_cast<double>(ReservedBytes(65536)), 65536);
  Check(ReservedBytes(65537) == 131072, "64 KB and 1 byte take 128 KB",
        static_cast<double>(ReservedBytes(65537)), 131072);
  Check(ReservedBytes(131072) == 131072,
        "a level of 256 by 256 pixels of 16 bits takes 128 KB",
        static_cast<double>(ReservedBytes(131072)), 131072);

  // A free region is of use from its first multiple of 64 KB.
  Check(UsableBytes(0x10000, 0x30000) == 0x30000,
        "a free region on a multiple of 64 KB is all of use",
        static_cast<double>(UsableBytes(0x10000, 0x30000)), 0x30000);
  Check(UsableBytes(0x11000, 0x2F000) == 0x20000,
        "a free region after a block of 4 KB is of use from the next 64 KB",
        static_cast<double>(UsableBytes(0x11000, 0x2F000)), 0x20000);
  Check(UsableBytes(0x11000, 0xF000) == 0,
        "the rest of a 64 KB after a block of 4 KB is of no use",
        static_cast<double>(UsableBytes(0x11000, 0xF000)), 0);

  // The sums of a walk.
  Totals t = {};
  const Region regions[] = {
      {0x10000, 0x10000, kCommitted, kPrivate},
      {0x20000, 0x1000, kCommitted, kPrivate},
      {0x21000, 0xF000, kFree, kNone},       // of no use
      {0x30000, 0x50000, kFree, kNone},      // 320 KB of use
      {0x80000, 0x20000, kReserved, kMapped},
      {0xA0000, 0x40000, kCommitted, kImage},
      {0xE0000, 0x22000, kFree, kNone},      // 136 KB of use
  };
  for (const Region& r : regions) Add(&t, r);
  Check(t.regions == 7, "the walk counts each region", t.regions, 7);
  Check(t.free == 0x50000 + 0x22000, "the free bytes are the usable parts",
        static_cast<double>(t.free), 0x50000 + 0x22000);
  Check(t.free_blocks == 2, "a free region of no use is no free block",
        t.free_blocks, 2);
  Check(t.largest_free == 0x50000, "the largest free block is 320 KB",
        static_cast<double>(t.largest_free), 0x50000);
  Check(t.used == 0x10000 + 0x1000 + 0x20000 + 0x40000,
        "the used bytes are the reserved and the committed regions",
        static_cast<double>(t.used), 0x10000 + 0x1000 + 0x20000 + 0x40000);
  Check(t.committed == 0x10000 + 0x1000 + 0x40000,
        "the committed bytes", static_cast<double>(t.committed),
        0x10000 + 0x1000 + 0x40000);
  Check(t.image == 0x40000 && t.mapped == 0x20000 && t.priv == 0x11000,
        "the modules, the views and the private memory", 1, 1);
}

}  // namespace address_space_checks
