// THE MODULE LIST AT A STALL (stall_modules, stall_addrs) AND THE TEST SPIN
// (test_stall_ms), docs/backend.md, "The module list at a stall".
//
// In the headset app the game can stop: no frame ends, and the main thread
// runs. The log of Wine can name an address (the critical section of a
// "RtlpWaitForCriticalSection" line), and only the map of the process says
// what holds it. With stall_modules=1 a small thread of the mod looks at the
// count of the frames each 500 ms. When the count stays the same for 5 s,
// the thread writes one time the modules of this process (the name, the
// base and the size, sorted by base) and the result of VirtualQuery for each
// address of stall_addrs, with the module that holds it. It does nothing
// else: it does not stop or read a thread. The frame thread only adds 1 to
// the count at the end of each frame.
//
// The log can need a lock that the stopped thread holds. Then the list does
// not come, and the game is as it is with no watch.
//
//   stall_modules=1  the watch (the default)
//   stall_modules=0  no thread and no count
//   test_stall_ms=N  a test aid: the frame thread spins N ms, one time, 20 s
//                    after the first scene walk (0, the default: no spin)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "scene_sample_math.h"
#include "stall_math.h"

extern void ModLog(const char* format, ...);

namespace {

const int kMaxModules = 512;
// The stalls that get the whole list. The next ones get one line, so a
// game that stops again and again does not fill the log.
const int kMaxLists = 10;

bool g_on = false;
volatile LONG g_frames = 0;
uint32_t g_addrs[stall_math::kMaxAddrs];
int g_addr_count = 0;
stall_math::Spin g_spin;
LARGE_INTEGER g_hz = {};

const char* StateName(DWORD state) {
  switch (state) {
    case MEM_COMMIT: return "MEM_COMMIT";
    case MEM_RESERVE: return "MEM_RESERVE";
    case MEM_FREE: return "MEM_FREE";
    default: return "?";
  }
}

const char* TypeName(DWORD type) {
  switch (type) {
    case MEM_IMAGE: return "MEM_IMAGE";
    case MEM_MAPPED: return "MEM_MAPPED";
    case MEM_PRIVATE: return "MEM_PRIVATE";
    default: return "none";
  }
}

void Report(int stall, LONG frames) {
  static HMODULE handles[kMaxModules];
  static scene_sample::Module modules[kMaxModules];
  static char names[kMaxModules][MAX_PATH];
  static char sorted[kMaxModules][MAX_PATH];
  DWORD bytes = 0;
  int count = 0;
  if (K32EnumProcessModules(GetCurrentProcess(), handles, sizeof(handles), &bytes)) {
    count = static_cast<int>(bytes / sizeof(HMODULE));
    if (count > kMaxModules) count = kMaxModules;
  }
  int n = 0;
  for (int i = 0; i < count; ++i) {
    MODULEINFO info;
    if (!K32GetModuleInformation(GetCurrentProcess(), handles[i], &info, sizeof(info))) {
      continue;
    }
    modules[n].base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(info.lpBaseOfDll));
    modules[n].size = info.SizeOfImage;
    if (GetModuleFileNameA(handles[i], names[n], MAX_PATH) == 0) strcpy_s(names[n], "?");
    ++n;
  }
  int order[kMaxModules];
  scene_sample::SortModules(modules, n, order);
  for (int i = 0; i < n; ++i) strcpy_s(sorted[i], names[order[i]]);
  ModLog("stall        : stall %d: no frame of the game ended for %u s (after %ld frames). "
         "The %d modules of the process, sorted by base, and %d addresses of stall_addrs",
         stall, stall_math::kStallMs / 1000, frames, n, g_addr_count);
  for (int i = 0; i < n; ++i) {
    ModLog("stall        : module 0x%08X to 0x%08X (0x%X bytes) %s", modules[i].base,
           modules[i].base + modules[i].size, modules[i].size, sorted[i]);
  }
  for (int a = 0; a < g_addr_count; ++a) {
    const uint32_t address = g_addrs[a];
    const int m = scene_sample::ModuleOf(address, modules, n);
    char in[MAX_PATH + 64];
    if (m >= 0) {
      sprintf_s(in, "It is in the module %s+0x%X", sorted[m], address - modules[m].base);
    } else {
      strcpy_s(in, "It is in no module");
    }
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(static_cast<uintptr_t>(address)), &mbi,
                     sizeof(mbi)) == 0) {
      ModLog("stall        : the address 0x%08X: VirtualQuery fails (error %lu). %s", address,
             GetLastError(), in);
      continue;
    }
    const uint32_t base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mbi.BaseAddress));
    const uint32_t alloc = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mbi.AllocationBase));
    ModLog("stall        : the address 0x%08X: VirtualQuery gives the region 0x%08X to 0x%08X "
           "(0x%X bytes), the allocation base 0x%08X, the state %s, the protection 0x%lX, "
           "the type %s. %s",
           address, base, base + static_cast<uint32_t>(mbi.RegionSize),
           static_cast<uint32_t>(mbi.RegionSize), alloc, StateName(mbi.State), mbi.Protect,
           TypeName(mbi.Type), in);
  }
}

DWORD WINAPI WatchThread(LPVOID) {
  stall_math::Watch watch;
  int stalls = 0;
  for (;;) {
    Sleep(500);
    const LONG frames = InterlockedCompareExchange(&g_frames, 0, 0);
    if (!watch.Step(static_cast<uint32_t>(frames), GetTickCount64())) continue;
    ++stalls;
    if (stalls <= kMaxLists) {
      Report(stalls, frames);
    } else {
      ModLog("stall        : stall %d: no frame of the game ended for %u s (after %ld "
             "frames). The log has the module list of the first %d stalls only",
             stalls, stall_math::kStallMs / 1000, frames, kMaxLists);
    }
  }
}

double NowMs() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return static_cast<double>(now.QuadPart) * 1000.0 / static_cast<double>(g_hz.QuadPart);
}

}  // namespace

// mod_main.cpp, at the start: stall_modules, the text of stall_addrs and
// test_stall_ms of bwvr.ini.
void StallWatchStart(bool on, const char* addrs, int test_stall_ms) {
  int bad = 0;
  g_addr_count = stall_math::ParseAddrs(addrs, g_addrs, stall_math::kMaxAddrs, &bad);
  if (bad > 0) {
    ModLog("stall        : stall_addrs has %d words that are not an address of 32 bits in "
           "hexadecimal, or more than %d addresses. The mod does not take them",
           bad, stall_math::kMaxAddrs);
  }
  g_on = on;
  if (on) {
    HANDLE thread = CreateThread(nullptr, 0, WatchThread, nullptr, 0, nullptr);
    if (thread == nullptr) {
      g_on = false;
      ModLog("stall        : stall_modules=1, but the thread of the watch could not start "
             "(error %lu)", GetLastError());
    } else {
      CloseHandle(thread);
    }
  }
  if (g_on) {
    ModLog("stall        : stall_modules=1. When no frame of the game ends for %u s, the log "
           "gets the modules of the process and %d addresses of stall_addrs, one time for "
           "each stall",
           stall_math::kStallMs / 1000, g_addr_count);
  } else {
    ModLog("stall        : stall_modules=0. No module list at a stall");
  }
  QueryPerformanceFrequency(&g_hz);
  g_spin.ms = g_hz.QuadPart != 0 && test_stall_ms > 0 ? test_stall_ms : 0;
  if (g_spin.ms > 0) {
    ModLog("stall        : test_stall_ms=%d (a test aid). The frame thread spins %d ms, one "
           "time, %u s after the first scene walk",
           g_spin.ms, g_spin.ms, stall_math::kSpinAfterMs / 1000);
  }
}

// h_FinishFrame, on the frame thread, at the end of each frame.
void StallWatchFrame() {
  if (g_on) InterlockedIncrement(&g_frames);
}

// stereo.cpp, on the frame thread, at the end of each scene walk.
void StallTestWalk() {
  if (g_spin.ms <= 0 || g_spin.done) return;
  const double start = NowMs();
  if (!g_spin.Due(start)) return;
  ModLog("stall        : test_stall_ms=%d: the frame thread spins now", g_spin.ms);
  // A spin, not a sleep: the thread stays in the state R.
  volatile uint32_t turns = 0;
  while (NowMs() - start < g_spin.ms) ++turns;
  ModLog("stall        : test_stall_ms=%d: the spin ended after %.0f ms", g_spin.ms,
         NowMs() - start);
}
