// WHERE THE TIME OF THE SCENE WALK GOES (profile_sample, a test key, and
// docs/backend.md, "Where the time of the scene walk goes").
//
// The scene walk is the largest part of a frame. It is code of the game, of
// dxwrapper, of the Direct3D 9 runtime, of the driver and of the mod. A timer
// on each call cannot split it: the calls of dxwrapper into Direct3D 9 have
// hooks of the mod already (draw_merge.cpp, one_walk.cpp), and a timer on
// each call adds time to each call. So this file takes samples.
//
// A thread of its own stops the frame thread for a moment each
// profile_sample microseconds while the frame thread is in a scene walk
// (SceneSampleWalk), reads the address of its next instruction and a copy of
// the top 384 bytes of its stack, and lets it go on. While the frame thread is
// stopped, the thread calls nothing that can take a lock that the stopped
// thread holds. Each 450 frames the report gives the share of each module and
// the buckets of 64 bytes with the most samples (scene_sample_math.h), and the
// time that the samples held the frame thread.
//
// An address in ntdll.dll or win32u.dll is mostly a call of the kernel: the
// driver in the kernel, or a wait. For these samples the report also gives
// the return addresses on the stack that are not in a module of the system:
// the code that waits.
//
//   profile_sample=0      off (the default)
//   profile_sample=N      a sample each N microseconds, 100 to 5000
//
// THE SAMPLES OF THE LONG FRAMES (profile_sample_slow=1, with slow_frame_ms
// above 0, docs/android.md, "The long frames"): the thread takes samples in
// the whole frame, in and outside the scene walks. At each present the
// samples of the frame that ended stay only when the frame is long
// (SceneSampleFrameEnd). The report gives the samples of the long frames
// alone. 0 (the default): the samples of each frame, as above.
//
// THE SAMPLES OF THE GAME TURN (profile_sample_turn=N, 100 to 5000, a test
// key, docs/backend.md, "The long game turn"): the thread takes a sample each
// N microseconds only while the frame thread is in the game turn
// (GGame::ProcessNetworkPackets, pass_guard.cpp), in place of the scene
// walks. At the end of each turn its samples stay only when the turn took
// profile_sample_turn_ms or more (2 by default, 0 keeps each turn). For each
// sample the thread also reads the stack from the sample up to the hook of
// the turn (at most kDeepWords), and keeps the return addresses in the code
// of runblack.exe (the chain, at most kChainWords). The report then also
// gives the calls of the game on these stacks: the share of the samples that
// have each call. The outermost calls are the parts of the turn.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "scene_sample.h"
#include "scene_sample_math.h"
#include "d3d7_thread.h"
#include "render_thread.h"
#include "time_log.h"

extern void ModLog(const char* format, ...);

namespace {

const int kRing = 1 << 17;          // the samples kept between two reports
const int kMaxModules = 512;
const int kTopModules = 8;
const int kTopBuckets = 16;
const int kTopCallers = 12;
// A copy of the top of the stack of each sample, for the callers of a wait in
// ntdll.dll. The ring of the stacks is smaller than the ring of the
// addresses: a report has about 20 samples in a frame.
const int kStackWords = 96;
const int kStackRing = 1 << 15;

int g_interval_us = 0;             // profile_sample
bool g_read = false;
bool g_failed = false;
HANDLE g_target = nullptr;         // the frame thread, or the render thread
// profile_sample_thread: 0 the frame thread, 1 the render thread, 2 the
// D3D7 thread.
int g_other = 0;
HANDLE g_thread = nullptr;         // the thread of the samples
volatile LONG g_in_walk = 0;
// profile_sample_outside=1: the samples come while the frame thread is NOT
// in a scene walk (the game turn, the streaming of the land, the loads, the
// waits for the headset), in place of the scene walks.
LONG g_outside = 0;
// profile_sample_slow=1: samples in the whole frame, and only the samples of
// the long frames go to the report. The samples of a long frame go from the
// ring into a ring of their own, with their stacks.
bool g_slow = false;
const int kSlowRing = 1 << 13;
uint32_t* g_slow_ring = nullptr;
uint32_t* g_slow_stacks = nullptr;
LONG g_slow_written = 0;
LONG g_slow_reported = 0;
LONG g_frame_first = 0;   // the first sample of the frame that runs now
uint64_t g_slow_frames = 0;  // the long frames since the last report
uint64_t g_all_frames = 0;   // all frames since the last report
double g_slow_ms = 0.0;      // their time
// profile_sample_turn: the samples come only in the game turn, and the
// samples of the long turns go to the ring of the long frames, with their
// chains. g_turn_base is the stack pointer of the hook of the turn: the
// stack of a sample is read up to there.
bool g_turn = false;
double g_turn_min_ms = 2.0;          // profile_sample_turn_ms
volatile LONG g_turn_base = 0;
uint64_t g_turn_all = 0;             // the turns since the last report
double g_turn_most_ms = 0.0;         // the longest turn since the last report
const int kChainWords = 32;
const int kDeepWords = 4096;         // 16 KB of the stack
uint32_t* g_deep = nullptr;          // the stack of one sample, its thread only
uint32_t* g_chains = nullptr;        // kChainWords for each sample of g_stacks
uint32_t* g_slow_chains = nullptr;   // kChainWords for each sample of g_slow_ring
uint32_t* g_chain_scratch = nullptr; // the chains of a report
const int kTopChainCalls = 28;
uint32_t g_text_low = 0;             // the code of runblack.exe
uint32_t g_text_high = 0;

// Written by the thread of the samples, read by the report on the frame
// thread. The report reads only up to the count it read first.
uint32_t* g_ring = nullptr;
uint32_t* g_stacks = nullptr;      // kStackWords for each sample, or zeros
volatile LONG g_written = 0;       // samples written in all
LONG g_reported = 0;               // samples that a report took
volatile LONG64 g_hold_ticks = 0;  // the time that the frame thread was held
volatile LONG g_lost = 0;          // stops that gave no address

uint32_t* g_scratch = nullptr;
scene_sample::Module g_modules[kMaxModules];
char g_names[kMaxModules][40];
int g_module_count = 0;
LARGE_INTEGER g_frequency = {};

int IniKey(const char* key, int fallback) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return fallback;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr) return fallback;
  slash[1] = 0;
  if (strlen(path) + 9 >= MAX_PATH) return fallback;
  strcat_s(path, "bwvr.ini");
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

// The return addresses in the code of runblack.exe in the first `words`
// words of g_deep, the innermost first, as a chain of kChainWords (zeros at
// the end). The code of the game is always there, so the bytes before a word
// are read in place.
void ScanChain(int words, uint32_t* chain) {
  uint32_t found[512];
  int count = 0;
  for (int w = 0; w < words && count < 512; ++w) {
    const uint32_t word = g_deep[w];
    if (word < g_text_low + 7 || word >= g_text_high) continue;
    if (!scene_sample::EndsWithCall(reinterpret_cast<const uint8_t*>(
            static_cast<uintptr_t>(word - 7)))) {
      continue;
    }
    found[count++] = word;
  }
  scene_sample::KeepChain(found, count, chain, kChainWords);
}

// It runs until the process ends. In a scene walk it spins on its core
// between two samples, so a run with profile_sample uses one more core.
// Outside the scene walks it sleeps. With profile_sample_slow it spins in the
// whole frame, in and outside the scene walks.
DWORD WINAPI SampleThread(LPVOID) {
  const LONGLONG step = g_frequency.QuadPart * g_interval_us / 1000000;
  LARGE_INTEGER next, now;
  QueryPerformanceCounter(&next);
  for (;;) {
    if (!g_slow && InterlockedCompareExchange(&g_in_walk, 0, 0) == g_outside) {
      Sleep(1);
      QueryPerformanceCounter(&next);
      continue;
    }
    next.QuadPart += step;
    // A wait of more than 2 ms sleeps. A shorter one spins on this thread,
    // because Sleep has steps of 1 ms or more.
    for (;;) {
      QueryPerformanceCounter(&now);
      const LONGLONG left = next.QuadPart - now.QuadPart;
      if (left <= 0) break;
      if (left > g_frequency.QuadPart / 500) {
        Sleep(1);
      } else {
        YieldProcessor();
      }
    }
    if (next.QuadPart < now.QuadPart - 10 * step) next = now;
    if (!g_slow && InterlockedCompareExchange(&g_in_walk, 0, 0) == g_outside) continue;
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    if (SuspendThread(g_target) == static_cast<DWORD>(-1)) continue;
    CONTEXT context;
    ZeroMemory(&context, sizeof(context));
    context.ContextFlags = CONTEXT_CONTROL;
    const BOOL got = GetThreadContext(g_target, &context);
    const LONG at = InterlockedCompareExchange(&g_written, 0, 0);
    uint32_t* stack = g_stacks + (at & (kStackRing - 1)) * kStackWords;
    // The stack is read while the thread is stopped. ReadProcessMemory takes
    // no lock of the process, and it fails, with no fault, on a page that is
    // not there.
    SIZE_T copied = 0;
    if (!got || !ReadProcessMemory(GetCurrentProcess(),
                                   reinterpret_cast<LPCVOID>(context.Esp),
                                   stack, kStackWords * sizeof(uint32_t),
                                   &copied)) {
      copied = 0;
    }
    // profile_sample_turn: the stack up to the hook of the turn, for the
    // chain. The words go to g_deep, and the scan comes after the resume.
    SIZE_T deep = 0;
    if (g_turn && got) {
      const uint32_t base = static_cast<uint32_t>(
          InterlockedCompareExchange(&g_turn_base, 0, 0));
      if (base > context.Esp) {
        SIZE_T bytes = base - context.Esp;
        if (bytes > kDeepWords * sizeof(uint32_t)) {
          bytes = kDeepWords * sizeof(uint32_t);
        }
        if (!ReadProcessMemory(GetCurrentProcess(),
                               reinterpret_cast<LPCVOID>(context.Esp), g_deep,
                               bytes, &deep)) {
          deep = 0;
        }
      }
    }
    ResumeThread(g_target);
    QueryPerformanceCounter(&b);
    InterlockedExchangeAdd64(&g_hold_ticks, b.QuadPart - a.QuadPart);
    if (copied < kStackWords * sizeof(uint32_t)) {
      ZeroMemory(stack, kStackWords * sizeof(uint32_t));
    }
    if (g_turn) {
      uint32_t* chain = g_chains + (at & (kStackRing - 1)) * kChainWords;
      ZeroMemory(chain, kChainWords * sizeof(uint32_t));
      ScanChain(static_cast<int>(deep / sizeof(uint32_t)), chain);
    }
    if (!got) {
      InterlockedIncrement(&g_lost);
      continue;
    }
    g_ring[at & (kRing - 1)] = static_cast<uint32_t>(context.Eip);
    InterlockedIncrement(&g_written);
  }
}

// The modules of the process now, sorted by base, with their names.
void ReadModules() {
  HMODULE handles[kMaxModules];
  DWORD bytes = 0;
  g_module_count = 0;
  if (!K32EnumProcessModules(GetCurrentProcess(), handles, sizeof(handles),
                             &bytes)) {
    return;
  }
  int count = static_cast<int>(bytes / sizeof(HMODULE));
  if (count > kMaxModules) count = kMaxModules;
  scene_sample::Module modules[kMaxModules];
  char names[kMaxModules][40];
  int n = 0;
  for (int i = 0; i < count; ++i) {
    MODULEINFO info;
    if (!K32GetModuleInformation(GetCurrentProcess(), handles[i], &info,
                                 sizeof(info))) {
      continue;
    }
    modules[n].base = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(info.lpBaseOfDll));
    modules[n].size = info.SizeOfImage;
    if (!K32GetModuleBaseNameA(GetCurrentProcess(), handles[i], names[n],
                               sizeof(names[n]))) {
      strcpy_s(names[n], "?");
    }
    ++n;
  }
  int order[kMaxModules];
  scene_sample::SortModules(modules, n, order);
  for (int i = 0; i < n; ++i) {
    g_modules[i] = modules[i];
    strcpy_s(g_names[i], names[order[i]]);
  }
  g_module_count = n;
}

// profile_sample_dump=1 (a test key): each report also appends its modules
// and each of its samples to bwvr-samples.txt beside runblack.exe, for a
// script that gives the share of each function. A line "M name base size"
// is a module, "W frames" starts a report, and each other line is the
// address of a sample, in hex. With profile_sample_turn the chain of the
// sample follows its address on its line, the innermost call first.
bool g_dump = false;
bool g_dumped = false;  // the file of this run is open for appends

void Dump(LONG take, uint64_t frames, LONG first) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr) return;
  slash[1] = 0;
  if (strlen(path) + 18 >= MAX_PATH) return;
  strcat_s(path, "bwvr-samples.txt");
  FILE* f = nullptr;
  if (fopen_s(&f, path, g_dumped ? "a" : "w") != 0 || f == nullptr) return;
  g_dumped = true;
  fprintf(f, "W %llu\n", frames);
  for (int m = 0; m < g_module_count; ++m) {
    fprintf(f, "M %s %08X %08X\n", g_names[m], g_modules[m].base,
            g_modules[m].size);
  }
  for (LONG i = 0; i < take; ++i) {
    fprintf(f, "%08X", g_scratch[i]);
    if (g_turn) {
      const uint32_t* chain =
          g_slow_chains + ((first + i) & (kSlowRing - 1)) * kChainWords;
      for (int w = 0; w < kChainWords && chain[w] != 0; ++w) {
        fprintf(f, " %08X", chain[w]);
      }
    }
    fprintf(f, "\n");
  }
  fclose(f);
}

// The modules of the system that a wait goes through.
bool IsSystemModule(int m) {
  if (m < 0) return false;
  return _stricmp(g_names[m], "ntdll.dll") == 0 ||
         _stricmp(g_names[m], "KERNEL32.DLL") == 0 ||
         _stricmp(g_names[m], "KERNELBASE.dll") == 0 ||
         _stricmp(g_names[m], "win32u.dll") == 0;
}

// True when the word is in a module and a call is just before it.
bool IsReturnAddress(uint32_t word, int m) {
  if (m < 0 || word - g_modules[m].base < 7) return false;
  uint8_t before[7];
  SIZE_T got = 0;
  if (!ReadProcessMemory(GetCurrentProcess(),
                         reinterpret_cast<LPCVOID>(word - 7), before,
                         sizeof(before), &got) ||
      got != sizeof(before)) {
    return false;
  }
  return scene_sample::EndsWithCall(before);
}

uint32_t g_caller1[kStackRing];
uint32_t g_caller2[kStackRing];
int g_own_module = -1;  // the module of the mod

// For each sample in ntdll.dll of the newest kStackRing samples: the first
// return address on its stack outside the modules of the system, and the
// first one that is also outside d3d9.dll and the mod (0 when there is
// none). Gives the count. stacks: the ring of the stacks of the samples, with
// ring samples.
int FindCallers(LONG first, LONG take, const uint32_t* stacks, int ring) {
  g_own_module = scene_sample::ModuleOf(
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&FindCallers)),
      g_modules, g_module_count);
  int count = 0;
  const LONG from = take > ring ? take - ring : 0;
  for (LONG i = from; i < take; ++i) {
    const int m = scene_sample::ModuleOf(g_scratch[i], g_modules,
                                         g_module_count);
    if (m < 0 || _stricmp(g_names[m], "ntdll.dll") != 0) continue;
    const uint32_t* stack = stacks + ((first + i) & (ring - 1)) * kStackWords;
    uint32_t c1 = 0, c2 = 0;
    for (int w = 0; w < kStackWords && c2 == 0; ++w) {
      const int wm =
          scene_sample::ModuleOf(stack[w], g_modules, g_module_count);
      if (wm < 0 || IsSystemModule(wm)) continue;
      if (!IsReturnAddress(stack[w], wm)) continue;
      if (c1 == 0) c1 = stack[w];
      if (wm != g_own_module && _stricmp(g_names[wm], "d3d9.dll") != 0) {
        c2 = stack[w];
      }
    }
    g_caller1[count] = c1;
    g_caller2[count] = c2;
    ++count;
  }
  return count;
}

// The return addresses with the most samples, as text. `addresses` is sorted
// here.
void CallerList(uint32_t* addresses, int count, LONG take, char* line,
                size_t size) {
  scene_sample::Bucket top[kTopCallers];
  const int got =
      scene_sample::TopBuckets(addresses, count, top, kTopCallers, 0);
  int used = 0;
  line[0] = 0;
  for (int k = 0; k < got; ++k) {
    const int m =
        scene_sample::ModuleOf(top[k].first, g_modules, g_module_count);
    if (top[k].first == 0) {
      used += sprintf_s(line + used, size - used, "%snone %.1f%%",
                        k == 0 ? "" : ", ", 100.0 * top[k].count / take);
    } else {
      used += sprintf_s(line + used, size - used, "%s%s+0x%X %.1f%%",
                        k == 0 ? "" : ", ", m >= 0 ? g_names[m] : "?",
                        m >= 0 ? top[k].first - g_modules[m].base
                               : top[k].first,
                        100.0 * top[k].count / take);
    }
    if (used >= static_cast<int>(size) - 80) break;
  }
}

void Start() {
  if (g_read) return;
  g_read = true;
  g_interval_us = IniKey("profile_sample", 0);
  g_dump = IniKey("profile_sample_dump", 0) != 0;
  // profile_sample_outside=N (100 to 5000) takes samples each N microseconds
  // outside the scene walks. It needs no profile_sample, so a save of the
  // launcher, which removes profile_sample, keeps it.
  const int outside = IniKey("profile_sample_outside", 0);
  g_outside = outside > 0 ? 1 : 0;
  if (g_outside != 0 && g_interval_us <= 0) g_interval_us = outside;
  // profile_sample_slow=N (100 to 5000) takes samples each N microseconds in
  // the whole frame and keeps the samples of the long frames. It needs
  // slow_frame_ms above 0 with time_log=1 (time_log.cpp), which says which
  // frame is long.
  const int slow = IniKey("profile_sample_slow", 0);
  // profile_sample_turn=N (100 to 5000) takes samples each N microseconds in
  // the game turn alone, and keeps the samples of the turns that took
  // profile_sample_turn_ms or more. It goes before profile_sample_slow and
  // profile_sample_outside.
  const int turn = IniKey("profile_sample_turn", 0);
  if (turn > 0) {
    g_turn = true;
    g_outside = 0;
    g_interval_us = turn;
    g_turn_min_ms = IniKey("profile_sample_turn_ms", 2);
    if (g_turn_min_ms < 0.0) g_turn_min_ms = 0.0;
    // The code of runblack.exe, from its headers.
    const uint8_t* image =
        reinterpret_cast<const uint8_t*>(GetModuleHandleA(nullptr));
    const IMAGE_DOS_HEADER* dos =
        reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const IMAGE_NT_HEADERS32* nt =
        reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
    g_text_low = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(image)) +
                 nt->OptionalHeader.BaseOfCode;
    g_text_high = g_text_low + nt->OptionalHeader.SizeOfCode;
    if (slow > 0) {
      ModLog("scene sample : profile_sample_turn=%d goes before "
             "profile_sample_slow=%d. No sample of the long frames",
             turn, slow);
    }
  } else if (slow > 0) {
    if (!g_time_log_slow) {
      ModLog("scene sample : profile_sample_slow=%d needs slow_frame_ms above 0 and "
             "time_log=1. No sample of the long frames", slow);
    } else {
      g_slow = true;
      g_outside = 0;
      if (g_interval_us <= 0) g_interval_us = slow;
    }
  }
  if (g_interval_us <= 0) {
    g_interval_us = 0;
    return;
  }
  if (g_interval_us < 100) g_interval_us = 100;
  if (g_interval_us > 5000) g_interval_us = 5000;
  QueryPerformanceFrequency(&g_frequency);
  g_ring = new uint32_t[kRing];
  g_scratch = new uint32_t[kRing];
  g_stacks = new uint32_t[kStackRing * kStackWords];
  ZeroMemory(g_stacks, kStackRing * kStackWords * sizeof(uint32_t));
  if (g_slow || g_turn) {
    g_slow_ring = new uint32_t[kSlowRing];
    g_slow_stacks = new uint32_t[kSlowRing * kStackWords];
    ZeroMemory(g_slow_stacks, kSlowRing * kStackWords * sizeof(uint32_t));
  }
  if (g_turn) {
    g_deep = new uint32_t[kDeepWords];
    g_chains = new uint32_t[kStackRing * kChainWords];
    ZeroMemory(g_chains, kStackRing * kChainWords * sizeof(uint32_t));
    g_slow_chains = new uint32_t[kSlowRing * kChainWords];
    ZeroMemory(g_slow_chains, kSlowRing * kChainWords * sizeof(uint32_t));
    g_chain_scratch = new uint32_t[kSlowRing * kChainWords];
  }
  // profile_sample_thread=1 takes the samples of the render thread, and 2
  // of the D3D7 thread (d3d7_thread.cpp), in place of the frame thread. That
  // thread starts with the first walk that has it, so SceneSampleWalk opens
  // it then.
  g_other = IniKey("profile_sample_thread", 0);
  if (g_other != 1 && g_other != 2) g_other = 0;
  // The game turn runs on the frame thread.
  if (g_turn) g_other = 0;
  if (g_other == 0) {
    g_target = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                              THREAD_QUERY_INFORMATION,
                          FALSE, GetCurrentThreadId());
  }
  if (g_target != nullptr || g_other != 0) {
    g_thread = CreateThread(nullptr, 0, SampleThread, nullptr, 0, nullptr);
  }
  if ((g_target == nullptr && g_other == 0) || g_thread == nullptr) {
    g_failed = true;
    ModLog("scene sample : profile_sample=%d, but the thread of the samples "
           "could not start (error %lu)", g_interval_us, GetLastError());
    return;
  }
  SetThreadPriority(g_thread, THREAD_PRIORITY_TIME_CRITICAL);
  ModLog("scene sample : profile_sample=%d. A thread stops the %s thread "
         "for a moment each %d microseconds %s and reads the "
         "address of its code. Each 450 frames the log has the share of each "
         "module", g_interval_us,
         g_other == 1 ? "render" : g_other == 2 ? "D3D7" : "frame",
         g_interval_us,
         g_turn ? "in the game turn alone, and keeps the samples of the turns "
                  "that take profile_sample_turn_ms or more (profile_sample_turn)"
         : g_slow ? "in the whole frame, and keeps the samples of the long frames "
                  "(profile_sample_slow)"
         : g_outside != 0 ? "outside the scene walks (profile_sample_outside=1)"
                          : "in each scene walk");
}

}  // namespace

void SceneSampleWalk(bool in) {
  if (!g_read) {
    if (!in) return;
    Start();
  }
  if (g_interval_us == 0 || g_failed) return;
  if (g_target == nullptr) {
    const DWORD id = g_other == 1   ? RenderThreadWorkerId()
                     : g_other == 2 ? D3D7ThreadWorkerId()
                                    : 0;
    if (id == 0) return;
    g_target = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                              THREAD_QUERY_INFORMATION,
                          FALSE, id);
    if (g_target == nullptr) return;
  }
  // With profile_sample_turn the flag is the game turn (SceneSampleTurn).
  if (g_turn) return;
  InterlockedExchange(&g_in_walk, in ? 1 : 0);
}

bool SceneSampleTurnOn() { return g_turn && !g_failed && g_interval_us > 0; }

void SceneSampleTurnBegin(const void* stack) {
  if (!SceneSampleTurnOn()) return;
  g_frame_first = InterlockedCompareExchange(&g_written, 0, 0);
  InterlockedExchange(&g_turn_base,
                      static_cast<LONG>(reinterpret_cast<uintptr_t>(stack)));
  InterlockedExchange(&g_in_walk, 1);
}

void SceneSampleTurnEnd(double turn_ms) {
  if (!SceneSampleTurnOn()) return;
  InterlockedExchange(&g_in_walk, 0);
  const LONG written = InterlockedCompareExchange(&g_written, 0, 0);
  ++g_turn_all;
  if (turn_ms > g_turn_most_ms) g_turn_most_ms = turn_ms;
  if (turn_ms >= g_turn_min_ms) {
    LONG first = g_frame_first;
    if (written - first > kSlowRing) first = written - kSlowRing;
    for (LONG i = first; i < written; ++i) {
      const LONG to = g_slow_written & (kSlowRing - 1);
      g_slow_ring[to] = g_ring[i & (kRing - 1)];
      memcpy(g_slow_stacks + to * kStackWords,
             g_stacks + (i & (kStackRing - 1)) * kStackWords,
             kStackWords * sizeof(uint32_t));
      memcpy(g_slow_chains + to * kChainWords,
             g_chains + (i & (kStackRing - 1)) * kChainWords,
             kChainWords * sizeof(uint32_t));
      ++g_slow_written;
    }
    ++g_slow_frames;
    g_slow_ms += turn_ms;
  }
  g_frame_first = written;
}

void SceneSampleFrameEnd(bool slow, double frame_ms) {
  if (!g_slow || g_turn || g_slow_ring == nullptr) return;
  const LONG written = InterlockedCompareExchange(&g_written, 0, 0);
  if (slow) {
    // The ring of the long frames holds kSlowRing samples, the fewest of the
    // three rings (kRing samples, kStackRing stacks), so a frame gives at
    // most its newest kSlowRing samples.
    LONG first = g_frame_first;
    if (written - first > kSlowRing) first = written - kSlowRing;
    for (LONG i = first; i < written; ++i) {
      const LONG to = g_slow_written & (kSlowRing - 1);
      g_slow_ring[to] = g_ring[i & (kRing - 1)];
      memcpy(g_slow_stacks + to * kStackWords,
             g_stacks + (i & (kStackRing - 1)) * kStackWords,
             kStackWords * sizeof(uint32_t));
      ++g_slow_written;
    }
    ++g_slow_frames;
    g_slow_ms += frame_ms;
  }
  ++g_all_frames;
  g_frame_first = written;
}

void SceneSampleReport(uint64_t frames, double scene_ms) {
  if (g_interval_us == 0 || g_failed || g_ring == nullptr) return;
  // profile_sample_slow: the samples of the long frames since the last
  // report, in place of the samples of all frames.
  // profile_sample_turn: the samples of the long turns, in the same ring.
  const bool kept = g_slow || g_turn;
  const uint32_t* ring = kept ? g_slow_ring : g_ring;
  const uint32_t* stacks = kept ? g_slow_stacks : g_stacks;
  const int ring_size = kept ? kSlowRing : kRing;
  const int stack_size = kept ? kSlowRing : kStackRing;
  LONG& reported = kept ? g_slow_reported : g_reported;
  const LONG written = kept ? g_slow_written : InterlockedCompareExchange(&g_written, 0, 0);
  // The samples held the frame thread in each frame, also with
  // profile_sample_slow, so the hold is of all frames. With
  // profile_sample_turn it is of all turns.
  uint64_t held_frames = frames;
  const uint64_t turns = g_turn_all;
  const double turn_most = g_turn_most_ms;
  if (g_turn) {
    held_frames = g_turn_all;
    g_turn_all = 0;
    g_turn_most_ms = 0.0;
  }
  if (kept) {
    if (g_slow) held_frames = g_all_frames;
    g_all_frames = 0;
    frames = g_slow_frames;
    scene_ms = g_slow_frames > 0 ? g_slow_ms / static_cast<double>(g_slow_frames) : 0.0;
    g_slow_frames = 0;
    g_slow_ms = 0.0;
  }
  LONG take = written - reported;
  // The ring holds the newest ring_size samples. The report is between two
  // frames, outside each scene walk, so no sample comes while it reads.
  const LONG first = take > ring_size ? written - ring_size : reported;
  if (take > ring_size) take = ring_size;
  reported = written;
  const LONG lost = InterlockedExchange(&g_lost, 0);
  const LONG64 held = InterlockedExchange64(&g_hold_ticks, 0);
  if (g_turn && (take <= 0 || frames == 0)) {
    ModLog("scene sample : no game turn of %.0f ms or more in %llu turns. The "
           "longest turn took %.2f ms", g_turn_min_ms, turns, turn_most);
    return;
  }
  if (take <= 0 || frames == 0) return;
  for (LONG i = 0; i < take; ++i) {
    g_scratch[i] = ring[(first + i) & (ring_size - 1)];
  }

  ReadModules();
  const int callers = FindCallers(first, take, stacks, stack_size);
  if (g_dump) Dump(take, frames, first);
  // The samples of each module, and of no module.
  uint32_t per_module[kMaxModules + 1];
  ZeroMemory(per_module, sizeof(per_module));
  for (LONG i = 0; i < take; ++i) {
    const int m =
        scene_sample::ModuleOf(g_scratch[i], g_modules, g_module_count);
    ++per_module[m >= 0 ? m : kMaxModules];
  }
  const double share = 100.0 / static_cast<double>(take);
  char line[1024];
  int used = 0;
  bool shown[kMaxModules + 1];
  ZeroMemory(shown, sizeof(shown));
  for (int k = 0; k < kTopModules; ++k) {
    int best = -1;
    for (int m = 0; m <= kMaxModules; ++m) {
      if (m < kMaxModules && m >= g_module_count) continue;
      if (shown[m] || per_module[m] == 0) continue;
      if (best < 0 || per_module[m] > per_module[best]) best = m;
    }
    if (best < 0) break;
    shown[best] = true;
    used += sprintf_s(line + used, sizeof(line) - used,
                      "%s%s %.1f%% (%.2f ms)", k == 0 ? "" : ", ",
                      best == kMaxModules ? "no module" : g_names[best],
                      per_module[best] * share,
                      scene_ms * per_module[best] / take);
    if (used >= static_cast<int>(sizeof(line)) - 80) break;
  }
  const double held_ms =
      held_frames > 0 ? 1000.0 * static_cast<double>(held) /
                            static_cast<double>(g_frequency.QuadPart) /
                            static_cast<double>(held_frames)
                      : 0.0;
  if (g_turn) {
    ModLog("scene sample : %ld samples in the game turns of %.0f ms or more: "
           "%llu turns of %llu, %.0f samples in a turn. The samples held the "
           "frame thread %.3f ms in a turn (of %llu turns), and %ld stops gave "
           "no address. These turns took %.2f ms on average, the longest turn "
           "%.2f ms. By module: %s",
           take, g_turn_min_ms, frames, turns,
           static_cast<double>(take) / frames, held_ms, held_frames, lost,
           scene_ms, turn_most, line);
  } else {
    ModLog("scene sample : %ld samples %s of %llu frames, %.0f "
           "in a frame. The samples held the frame thread %.3f ms in a frame (of %llu frames), "
           "and %ld stops gave no address. %s %.2f ms in a "
           "frame. By module: %s",
           take,
           g_slow ? "in the whole frame"
           : g_outside != 0 ? "outside the scene walks" : "in the scene walks",
           frames, static_cast<double>(take) / frames, held_ms, held_frames, lost,
           g_slow ? "These long frames were" : "The scene walks were", scene_ms, line);
  }

  // The buckets with the most samples.
  scene_sample::Bucket top[kTopBuckets];
  const int got = scene_sample::TopBuckets(g_scratch, take, top, kTopBuckets);
  used = 0;
  for (int k = 0; k < got; ++k) {
    const int m =
        scene_sample::ModuleOf(top[k].first, g_modules, g_module_count);
    used += sprintf_s(line + used, sizeof(line) - used, "%s%s+0x%X %.1f%%",
                      k == 0 ? "" : ", ", m >= 0 ? g_names[m] : "?",
                      m >= 0 ? top[k].first - g_modules[m].base : top[k].first,
                      top[k].count * share);
    if (used >= static_cast<int>(sizeof(line)) - 80) break;
  }
  ModLog("scene sample : the code with the most samples, in pieces of 64 "
         "bytes: %s", line);

  if (callers > 0) {
    char first_line[1024];
    char second_line[1024];
    CallerList(g_caller1, callers, take, first_line, sizeof(first_line));
    CallerList(g_caller2, callers, take, second_line, sizeof(second_line));
    ModLog("scene sample : %d samples in ntdll.dll. The first return address "
           "on the stack outside ntdll.dll, KERNEL32.DLL, KERNELBASE.dll and "
           "win32u.dll, in %% of all samples: %s",
           callers, first_line);
    ModLog("scene sample : the first return address that is also outside "
           "d3d9.dll and the mod: %s", second_line);
  }

  // profile_sample_turn: the calls of the game on the stacks of the samples.
  // Each call counts one time in a sample, so its share is the share of the
  // samples in which the call is on the stack: the time below that call.
  if (g_turn && g_chain_scratch != nullptr) {
    int words = 0;
    for (LONG i = 0; i < take; ++i) {
      uint32_t* to = g_chain_scratch + words;
      memcpy(to, g_slow_chains + ((first + i) & (kSlowRing - 1)) * kChainWords,
             kChainWords * sizeof(uint32_t));
      words += scene_sample::UniqueWords(to, kChainWords);
    }
    scene_sample::Bucket calls[kTopChainCalls];
    const int have = scene_sample::TopBuckets(g_chain_scratch, words, calls,
                                              kTopChainCalls, 0);
    used = 0;
    for (int k = 0; k < have; ++k) {
      used += sprintf_s(line + used, sizeof(line) - used,
                        "%s0x%08X %.1f%%", k == 0 ? "" : ", ", calls[k].first,
                        calls[k].count * share);
      if (used >= static_cast<int>(sizeof(line)) - 40) break;
    }
    ModLog("scene sample : the return addresses in runblack.exe on the stacks "
           "of these samples, in %% of the samples that have each one: %s",
           line);
  }
}
