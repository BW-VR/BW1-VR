// The address space of the process in the log (address_space.h).
//
// WHY. The game is a process of 32 bits, so the game, the mod, the driver of
// the GPU and the OpenXR runtime share 2 GB of address space, or 4 GB with
// the flag LARGE_ADDRESS_AWARE (large_address.h). When no block is free, an
// allocation fails: a texture of the renderer cannot get its pixels, and a
// driver can write to address 0.
// The line of each report window shows how near the process is to that.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "address_space.h"

void ModLog(const char* format, ...);

namespace {

volatile LONG g_busy = 0;  // a walk of the report runs on the pool
bool g_said_low = false;
const uint64_t kLowBlock = 128ull << 20;

void CALLBACK ReportOnPool(PTP_CALLBACK_INSTANCE, void*) {
  LARGE_INTEGER start, done, hz;
  QueryPerformanceCounter(&start);
  address_space_math::Totals t;
  uint64_t total = 0;
  const bool ok = ReadAddressSpace(&t, &total);
  QueryPerformanceCounter(&done);
  QueryPerformanceFrequency(&hz);
  if (ok) {
    const double mb = 1.0 / 1048576.0;
    ModLog("address space: %.0f MB of %.0f MB in use (%.0f MB of modules, "
           "%.0f MB of views, %.0f MB of private memory, %.0f MB committed). "
           "%.0f MB free in %u blocks, the largest %.1f MB. %u regions, the "
           "walk took %.2f ms",
           t.used * mb, total * mb, t.image * mb, t.mapped * mb, t.priv * mb,
           t.committed * mb, t.free * mb, t.free_blocks, t.largest_free * mb,
           t.regions,
           (done.QuadPart - start.QuadPart) * 1000.0 /
               static_cast<double>(hz.QuadPart));
    if (t.largest_free < kLowBlock && !g_said_low) {
      g_said_low = true;
      ModLog("address space: the largest free block is less than 128 MB. A "
             "large allocation of the game, of the mod or of a driver can "
             "fail now");
    }
  }
  InterlockedExchange(&g_busy, 0);
}

}  // namespace

bool ReadAddressSpace(address_space_math::Totals* out, uint64_t* total) {
  *out = address_space_math::Totals();
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  const uintptr_t low =
      reinterpret_cast<uintptr_t>(info.lpMinimumApplicationAddress);
  const uintptr_t high =
      reinterpret_cast<uintptr_t>(info.lpMaximumApplicationAddress);
  *total = static_cast<uint64_t>(high) + 1;
  uintptr_t at = low;
  while (at < high) {
    MEMORY_BASIC_INFORMATION m;
    if (VirtualQuery(reinterpret_cast<void*>(at), &m, sizeof(m)) == 0) break;
    address_space_math::Region r;
    r.base = reinterpret_cast<uintptr_t>(m.BaseAddress);
    r.size = m.RegionSize;
    r.state = m.State == MEM_FREE      ? address_space_math::kFree
              : m.State == MEM_RESERVE ? address_space_math::kReserved
                                       : address_space_math::kCommitted;
    r.type = m.Type == MEM_IMAGE    ? address_space_math::kImage
             : m.Type == MEM_MAPPED ? address_space_math::kMapped
             : m.Type == MEM_PRIVATE ? address_space_math::kPrivate
                                     : address_space_math::kNone;
    address_space_math::Add(out, r);
    const uintptr_t next =
        reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
    if (next <= at) break;
    at = next;
  }
  return out->regions > 0;
}

void ReserveLowAddressSpace() {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  const uintptr_t two_gb = 0x80000000u;
  if (reinterpret_cast<uintptr_t>(info.lpMaximumApplicationAddress) < two_gb) {
    ModLog("high memory  : test_high_memory=1, but runblack.exe has no flag "
           "LARGE_ADDRESS_AWARE. Nothing is reserved");
    return;
  }
  uint64_t reserved = 0;
  unsigned blocks = 0;
  uintptr_t at = reinterpret_cast<uintptr_t>(info.lpMinimumApplicationAddress);
  while (at < two_gb) {
    MEMORY_BASIC_INFORMATION m;
    if (VirtualQuery(reinterpret_cast<void*>(at), &m, sizeof(m)) == 0) break;
    const uintptr_t base = reinterpret_cast<uintptr_t>(m.BaseAddress);
    uintptr_t end = base + m.RegionSize;
    if (end <= at) break;
    if (m.State == MEM_FREE) {
      const uintptr_t start = static_cast<uintptr_t>(
          address_space_math::ReservedBytes(base));
      if (end > two_gb) end = two_gb;
      if (end > start &&
          VirtualAlloc(reinterpret_cast<void*>(start), end - start,
                       MEM_RESERVE, PAGE_NOACCESS) != nullptr) {
        reserved += end - start;
        ++blocks;
      }
    }
    at = base + m.RegionSize;
  }
  ModLog("high memory  : test_high_memory=1. %u free blocks below 2 GB are "
         "reserved, %.0f MB. Each allocation from now on gets an address "
         "above 2 GB",
         blocks, reserved / 1048576.0);
}

void AddressSpaceReport() {
  if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return;
  if (!TrySubmitThreadpoolCallback(&ReportOnPool, nullptr, nullptr)) {
    InterlockedExchange(&g_busy, 0);
  }
}
