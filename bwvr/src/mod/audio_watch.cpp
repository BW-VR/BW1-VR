// THE WAIT IN THE AUDIO TURN (audio_turn_watch, audio_watch_math.h,
// docs/audio.md, "7.7 The watch of the audio turn").
//
// Frame thread. The audio turn is GAudio::ProcessAudioGameTurn
// (0x00427080), the part audio_turn of turn_parts.cpp. While it runs, a call
// of the following kinds has a timer:
//
//   LH:    an import of runblack.exe from LHaudiodllR.dll
//   QM:    an import of LHaudiodllR.dll from QMixer.dll
//   k32:   a wait call (Sleep, EnterCriticalSection, WaitFor...) in the import
//          table of LHaudiodllR.dll or QMixer.dll
//   winmm: the wave and timer calls of the two DLLs, and the same for the
//          imports from the audio compression manager, the log DLL of the
//          game, OLE and the window manager (waits and sends)
//   ds:    IDirectSoundBuffer::Lock, Unlock and GetCurrentPosition in the
//          hooks of audio_capture.cpp
//   mod:   the lock of the mixer of the mod, when it must wait
//
// A timer is a stub in the import slot, as in turn_parts.cpp: the stub keeps
// the registers, takes the clock, puts the address of ExitThunk in the place
// of the return address, and jumps to the function that the slot held. At the
// return ExitThunk takes the clock, closes the timer and jumps to the caller.
// No MinHook hook is made, so no thread stops. The slots change at the first
// audio turn, after the mod has patched the import of Sleep of the audio DLL
// (audio_voices.cpp), so the chain is: the DLL, the stub, the hook of the mod,
// the system. A hook of the mod that reads its return address calls
// AudioWatchCaller.
//
// Only the frame thread in an audio turn counts. A call on another thread, or
// outside the audio turn, goes through the stub with no clock read.
//
// Mixer thread. The hooks of audio_capture.cpp time the DirectSound calls and
// waveOutWrite (AudioWatchBegin and AudioWatchEnd). The sums go to the line of
// the window.
//
//   audio_turn_watch=0   no stub and no line (the default)
//   audio_turn_watch=1   the timers, each 10 s one line, and in the line of a
//                        long frame the parts of an audio turn of 5 ms or more
//
// It needs turn_parts=1 and time_log=1.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "audio_watch.h"
#include "audio_watch_math.h"
#include "trace_rec.h"
#include "thread_id.h"
#include "time_log.h"

void ModLog(const char* format, ...);

bool g_audio_watch = false;

namespace {

namespace aw = audio_watch_math;

constexpr int kStubBytes = 10;
// The ids of the timers of the hooks of the mod: kind + 1. Id 0 is the audio
// turn itself.
constexpr int kFirstImport = kAwKinds + 1;

DWORD g_thread = 0;  // the frame thread
bool g_active = false;  // an audio turn runs on the frame thread
bool g_slots_done = false;
LARGE_INTEGER g_hz = {};
double g_ms_per_tick = 0.0;

uint8_t* g_stubs = nullptr;
uint32_t g_orig[aw::kMaxTimers];
char g_name_text[aw::kMaxTimers][40];
const char* g_names[aw::kMaxTimers];
int g_count = kFirstImport;
int g_counts[8];  // timers by group, for the log

aw::Stack g_stack;
aw::Table g_turn;
aw::Frame g_frame;
aw::Window g_window;
uint64_t g_lost = 0;
uint64_t g_outside_calls = 0;
uint64_t g_other_calls = 0;

// The mixer thread. Sums in units of 100 ns.
const char* const kSpanNames[5] = {"IDirectSoundBuffer::Lock", "IDirectSoundBuffer::Unlock",
                                   "IDirectSoundBuffer::GetCurrentPosition", "waveOutWrite",
                                   "the render of the mod"};
volatile LONG g_span_calls[5];
volatile LONG g_span_sum[5];
volatile LONG g_span_most[5];

// The images, for the call sites.
struct Range {
  uintptr_t lo = 0, hi = 0;
  const char* name = "";
};
Range g_range[4];  // the game, LHaudiodllR.dll, QMixer.dll, the mod

// The bar of a watched call in the timeline (trace_rec.h). The name of a timer
// lives as long as the process.
void TraceCall(int id, int64_t begin, int64_t end) {
  static trace_events::Def defs[aw::kMaxTimers];
  if (id < 0 || id >= aw::kMaxTimers || g_names[id] == nullptr) return;
  if (defs[id].name == nullptr) {
    defs[id].cat = "audio";
    defs[id].name = g_names[id];
  }
  TraceBar(&defs[id], begin, end);
}

int64_t Now() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

void __cdecl ExitThunk();
void __cdecl NoReturn();

Range ImageOf(HMODULE m, const char* name) {
  Range r;
  r.name = name;
  const uint8_t* base = reinterpret_cast<const uint8_t*>(m);
  if (base == nullptr || IsBadReadPtr(base, 0x400)) return r;
  const IMAGE_NT_HEADERS32* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
      base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
  r.lo = reinterpret_cast<uintptr_t>(base);
  r.hi = r.lo + nt->OptionalHeader.SizeOfImage;
  return r;
}

void Site(uint32_t a, char* out, size_t size) {
  for (int i = 0; i < 4; ++i) {
    if (a >= g_range[i].lo && a < g_range[i].hi && g_range[i].lo != 0) {
      if (i == 0) {
        _snprintf_s(out, size, _TRUNCATE, "the game 0x%08X", a);
      } else if (i == 3) {
        _snprintf_s(out, size, _TRUNCATE, "the mod");
      } else {
        _snprintf_s(out, size, _TRUNCATE, "%s+0x%X", g_range[i].name,
                    static_cast<unsigned>(a - g_range[i].lo));
      }
      return;
    }
  }
  _snprintf_s(out, size, _TRUNCATE, "0x%08X", a);
}

}  // namespace

// The stub of a timer pushes its id and jumps to EnterThunk. The stack then
// has the id at [esp] and the return address of the caller at [esp+4], and
// the arguments of the function after it.
extern "C" uint32_t __cdecl AudioWatchEnter(uint32_t id, uint32_t* ret_slot) {
  const uint32_t original = g_orig[id];
  if (ThisThreadId() != g_thread) {
    ++g_other_calls;
  } else if (!g_active) {
    ++g_outside_calls;
  } else if (g_stack.Push(static_cast<int>(id), Now(), *ret_slot,
                          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ret_slot)))) {
    *ret_slot = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ExitThunk));
  }
  return original;
}

// ExitThunk comes here with the stack place after the return of the
// function. Integers only.
extern "C" uint32_t __cdecl AudioWatchExit(uint32_t esp) {
  aw::Closed closed;
  const int top = g_stack.Match(esp);
  const int64_t begin = top >= 0 ? g_stack.open[top].start : 0;
  const int64_t now = Now();
  const uint32_t ret = g_stack.Return(now, esp, &closed);
  if (ret != 0) {
    g_turn.AddCall(closed, g_ms_per_tick);
    if (g_trace) TraceCall(closed.id, begin, now);
    return ret;
  }
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&NoReturn));
}

namespace {

__declspec(naked) void EnterThunk() {
  __asm {
    pushfd
    pushad
    // [esp+36] is the id, [esp+40] the return address.
    lea eax, [esp + 40]
    push eax
    push dword ptr [esp + 40]
    call AudioWatchEnter
    add esp, 8
    // The place of the id gets the address of the function.
    mov [esp + 36], eax
    popad
    popfd
    ret
  }
}

__declspec(naked) void __cdecl ExitThunk() {
  __asm {
    // A place for the return address of the caller.
    push eax
    pushfd
    pushad
    lea eax, [esp + 40]
    push eax
    call AudioWatchExit
    add esp, 4
    mov [esp + 36], eax
    popad
    popfd
    ret
  }
}

// A return with no open call on the stack of the timers. It cannot occur when
// each call returns. The fault goes to the crash report of the mod.
__declspec(naked) void __cdecl NoReturn() {
  __asm {
    ud2
  }
}

void MakeStub(int id) {
  uint8_t* at = g_stubs + id * kStubBytes;
  at[0] = 0x68;  // push id
  const uint32_t v = static_cast<uint32_t>(id);
  memcpy(at + 1, &v, 4);
  at[5] = 0xE9;  // jmp EnterThunk
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<intptr_t>(&EnterThunk) -
                                           reinterpret_cast<intptr_t>(at + 10));
  memcpy(at + 6, &rel, 4);
}

// The name of an import for the lines: a name with the C++ decoration gives
// the function (and the word ctor or dtor for a constructor or a destructor).
void ShortName(const char* prefix, const char* import_name, char* out, size_t size) {
  char core[40];
  const char* p = import_name;
  const char* suffix = "";
  if (p[0] == '?') {
    while (*p == '?') ++p;
    if (import_name[1] == '?' && (p[0] == '0' || p[0] == '1')) {
      suffix = p[0] == '0' ? ".ctor" : ".dtor";
      ++p;
    }
  }
  size_t n = 0;
  while (p[n] != 0 && p[n] != '@' && n + 1 < sizeof(core)) {
    core[n] = p[n];
    ++n;
  }
  core[n] = 0;
  _snprintf_s(out, size, _TRUNCATE, "%s%s%s", prefix, core, suffix);
}

bool InList(const char* name, const char* const* list) {
  for (int i = 0; list[i] != nullptr; ++i) {
    if (strcmp(name, list[i]) == 0) return true;
  }
  return false;
}

const char* const kWaits[] = {"Sleep",
                              "SleepEx",
                              "EnterCriticalSection",
                              "WaitForSingleObject",
                              "WaitForSingleObjectEx",
                              "WaitForMultipleObjects",
                              "WaitForMultipleObjectsEx",
                              "SignalObjectAndWait",
                              "MsgWaitForMultipleObjects",
                              "MsgWaitForMultipleObjectsEx",
                              "SendMessageA",
                              "SendMessageW",
                              nullptr};

// One source of timers: the imports of `module` from `dll`. With `waits` only
// the names in kWaits.
struct Source {
  HMODULE module;
  const char* dll;
  const char* prefix;
  bool waits;
  int group;
};

// A timer on the import slot. False when there is no room.
bool AddSlot(void** slot, const char* prefix, const char* import_name, int group) {
  if (g_count >= aw::kMaxTimers) return false;
  // LHaudiodllR.dll exports some variables. A slot whose target is not code
  // gets no stub.
  MEMORY_BASIC_INFORMATION mbi = {};
  if (*slot == nullptr || VirtualQuery(*slot, &mbi, sizeof(mbi)) == 0 ||
      (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                      PAGE_EXECUTE_WRITECOPY)) == 0) {
    return true;
  }
  DWORD old = 0;
  if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
  const int id = g_count;
  char name[40];
  ShortName(prefix, import_name, name, sizeof(name));
  // A name that comes twice (the overloads) gets a number.
  int same = 0;
  for (int i = kFirstImport; i < id; ++i) {
    if (strncmp(g_name_text[i], name, strlen(name)) == 0 &&
        (g_name_text[i][strlen(name)] == 0 || g_name_text[i][strlen(name)] == '#')) {
      ++same;
    }
  }
  if (same > 0) {
    _snprintf_s(g_name_text[id], sizeof(g_name_text[id]), _TRUNCATE, "%s#%d", name, same + 1);
  } else {
    _snprintf_s(g_name_text[id], sizeof(g_name_text[id]), _TRUNCATE, "%s", name);
  }
  g_names[id] = g_name_text[id];
  g_orig[id] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(*slot));
  MakeStub(id);
  *slot = g_stubs + id * kStubBytes;
  VirtualProtect(slot, sizeof(void*), old, &old);
  ++g_count;
  ++g_counts[group];
  return true;
}

// The timers on the imports of one source.
void AddSource(const Source& s) {
  const uint8_t* base = reinterpret_cast<const uint8_t*>(s.module);
  if (base == nullptr || IsBadReadPtr(base, 0x400)) return;
  const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
  const IMAGE_NT_HEADERS32* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) return;
  const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (dir.VirtualAddress == 0) return;
  const IMAGE_IMPORT_DESCRIPTOR* d =
      reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
  for (; d->Name != 0; ++d) {
    if (_stricmp(reinterpret_cast<const char*>(base + d->Name), s.dll) != 0) continue;
    const DWORD names_rva = d->OriginalFirstThunk != 0 ? d->OriginalFirstThunk : d->FirstThunk;
    const IMAGE_THUNK_DATA32* names = reinterpret_cast<const IMAGE_THUNK_DATA32*>(base + names_rva);
    void** slots = reinterpret_cast<void**>(const_cast<uint8_t*>(base) + d->FirstThunk);
    for (int i = 0; names[i].u1.AddressOfData != 0; ++i) {
      if (names[i].u1.Ordinal & IMAGE_ORDINAL_FLAG32) continue;
      const char* fn = reinterpret_cast<const char*>(
          reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names[i].u1.AddressOfData)->Name);
      if (s.waits && !InList(fn, kWaits)) continue;
      if (!AddSlot(&slots[i], s.prefix, fn, s.group)) return;
    }
  }
}

// The timers on the import slots, at the first audio turn.
void InstallSlots() {
  g_slots_done = true;
  HMODULE game = GetModuleHandleA(nullptr);
  HMODULE lh = AudioAudioDll();
  HMODULE qm = AudioMixerDll();
  if (lh == nullptr || qm == nullptr) {
    ModLog("audio watch  : LHaudiodllR.dll or QMixer.dll is not loaded, so the watch has no "
           "timer on a call of the DLLs. The DirectSound calls and the lock of the mod have one");
    return;
  }
  HMODULE self = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCSTR>(&InstallSlots), &self);
  g_range[0] = ImageOf(game, "the game");
  g_range[1] = ImageOf(lh, "LH");
  g_range[2] = ImageOf(qm, "QM");
  g_range[3] = ImageOf(self, "the mod");
  g_stubs = static_cast<uint8_t*>(VirtualAlloc(nullptr, aw::kMaxTimers * kStubBytes,
                                               MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
  if (g_stubs == nullptr) {
    ModLog("audio watch  : no memory for the stubs, so the watch has no timer on a call of the "
           "DLLs");
    return;
  }
  // Group 0: the calls of the game into the audio DLL. 1: the calls of that
  // DLL into QMixer. 2: waits. 3: the rest of the imports.
  const Source sources[] = {
      {game, "LHaudiodllR.dll", "LH:", false, 0},
      {lh, "QMIXER.dll", "QM:", false, 1},
      {lh, "KERNEL32.dll", "k32:", true, 2},
      {qm, "KERNEL32.dll", "k32:", true, 2},
      {lh, "USER32.dll", "user:", true, 2},
      {qm, "USER32.dll", "user:", true, 2},
      {lh, "WINMM.dll", "winmm:", false, 3},
      {qm, "WINMM.dll", "winmm:", false, 3},
      {lh, "MSACM32.dll", "acm:", false, 3},
      {lh, "LHLogR.dll", "log:", false, 3},
      {lh, "ole32.dll", "ole:", false, 3},
      {qm, "ole32.dll", "ole:", false, 3},
  };
  for (const Source& s : sources) AddSource(s);
  ModLog("audio watch  : %d timers on import slots: %d calls of the game into LHaudiodllR.dll, "
         "%d calls of that DLL into QMixer.dll, %d wait calls of the two DLLs, and %d other "
         "calls of the two DLLs (wave and timer, the audio compression, the log, OLE)",
         g_count - kFirstImport, g_counts[0], g_counts[1], g_counts[2], g_counts[3]);
}

// Appends the span of the mixer thread of a kind to the window, and clears it.
aw::Span TakeSpan(int i) {
  aw::Span s;
  s.calls = static_cast<uint64_t>(InterlockedExchange(&g_span_calls[i], 0));
  s.sum_ms = static_cast<double>(InterlockedExchange(&g_span_sum[i], 0)) / 1.0e4;
  s.most_ms = static_cast<double>(InterlockedExchange(&g_span_most[i], 0)) / 1.0e4;
  return s;
}

}  // namespace

// From TurnPartsInstall, on the frame thread.
void AudioWatchInit() {
  if (AudioIni("audio_turn_watch", 0) == 0) return;
  QueryPerformanceFrequency(&g_hz);
  if (g_hz.QuadPart == 0) {
    ModLog("audio watch  : no clock. No watch");
    return;
  }
  g_ms_per_tick = 1000.0 / static_cast<double>(g_hz.QuadPart);
  g_thread = ThisThreadId();
  g_names[0] = "audio_turn";
  g_names[kAwLock + 1] = "ds:Lock";
  g_names[kAwUnlock + 1] = "ds:Unlock";
  g_names[kAwPosition + 1] = "ds:GetCurrentPosition";
  g_names[kAwWrite + 1] = "winmm:waveOutWrite";
  g_names[kAwRender + 1] = "mod:render";
  g_names[kAwModLock + 1] = "mod:lock";
  g_audio_watch = true;
  ModLog("audio watch  : audio_turn_watch=1. The frame thread in an audio turn: the time and the "
         "calls of each call into LHaudiodllR.dll and QMixer.dll and of each wait call of the "
         "two DLLs, exclusive, with the call site. The mixer thread: the DirectSound calls and "
         "waveOutWrite. Each 10 s one line, and in the line of a long frame the parts of an "
         "audio turn of %.0f ms or more. The import slots change at the first audio turn",
         aw::kLongAudioMs);
}

void AudioWatchTurnBegin(int64_t now) {
  if (!g_audio_watch) return;
  if (!g_slots_done) InstallSlots();
  g_stack = aw::Stack();
  g_turn.Clear();
  g_stack.Push(0, now, 0, 0);
  g_active = true;
}

void AudioWatchTurnEnd(int64_t now) {
  if (!g_audio_watch || !g_active) return;
  g_active = false;
  while (g_stack.depth > 1) g_stack.DropTop();
  if (g_stack.depth != 1) return;
  const double ms = static_cast<double>(now - g_stack.open[0].start) * g_ms_per_tick;
  g_turn.AddCall(g_stack.CloseTop(now), g_ms_per_tick);
  g_lost += g_stack.lost;
  g_window.AddTurn(g_turn, ms);
  if (g_time_log_slow) g_frame.AddTurn(g_turn, ms);
  g_stack = aw::Stack();
}

void AudioWatchGameTurnEnd() {
  if (!g_audio_watch || !g_active) return;
  // The audio turn did not end: its return did not come.
  g_active = false;
  g_lost += static_cast<uint64_t>(g_stack.depth);
  g_stack = aw::Stack();
}

int AudioWatchFrameText(char* out, size_t size) {
  if (size > 0) out[0] = 0;
  if (!g_audio_watch || g_frame.audio_ms < aw::kLongAudioMs) return 0;
  return aw::FrameText(out, size, g_frame, g_names, &Site);
}

void AudioWatchFrameEnd() {
  if (g_audio_watch) g_frame.Clear();
}

bool AudioWatchWindowLine(double seconds) {
  if (!g_audio_watch) return false;
  static char line[8192];
  aw::Span spans[5];
  for (int i = 0; i < 5; ++i) spans[i] = TakeSpan(i);
  g_window.outside_calls = g_outside_calls;
  g_window.other_thread_calls = g_other_calls;
  g_outside_calls = g_other_calls = 0;
  aw::WindowLine(line, sizeof(line), g_window, g_names, &Site, kSpanNames, spans, 5, seconds);
  ModLog("audio watch  : %s%s", line,
         g_lost > 0 ? ". An audio turn or a call that no return closed was lost (a fault)" : "");
  g_window.Reset();
  return true;
}

int64_t AudioWatchBegin(int kind, uintptr_t caller) {
  if (!g_audio_watch || kind < 0 || kind >= kAwKinds) return 0;
  const bool frame = ThisThreadId() == g_thread;
  if (frame) {
    if (!g_active || !g_stack.Push(kind + 1, Now(), static_cast<uint32_t>(caller), 0)) return 0;
    return 1;
  }
  // The mixer thread: the lock of the mod is not timed there.
  if (kind == kAwModLock) return 0;
  return Now();
}

void AudioWatchEnd(int kind, int64_t token) {
  if (token == 0 || kind < 0 || kind >= kAwKinds) return;
  if (ThisThreadId() == g_thread) {
    if (!g_active || g_stack.depth < 2 || g_stack.open[g_stack.depth - 1].id != kind + 1) return;
    const int64_t begin = g_stack.open[g_stack.depth - 1].start;
    const int64_t now = Now();
    g_turn.AddCall(g_stack.CloseTop(now), g_ms_per_tick);
    if (g_trace) TraceCall(kind + 1, begin, now);
    return;
  }
  if (g_trace) TraceCall(kind + 1, token, Now());
  if (kind >= 5) return;
  const double us100 = static_cast<double>(Now() - token) * 1.0e7 / static_cast<double>(g_hz.QuadPart);
  const LONG v = us100 < 0.0 ? 0 : (us100 > 2.0e9 ? 2000000000 : static_cast<LONG>(us100));
  InterlockedIncrement(&g_span_calls[kind]);
  InterlockedExchangeAdd(&g_span_sum[kind], v);
  // One writer for each kind in practice. A race loses only a little of the most.
  if (v > g_span_most[kind]) InterlockedExchange(&g_span_most[kind], v);
}

uintptr_t AudioWatchCallerSlow(uintptr_t ret) {
  if (ret != reinterpret_cast<uintptr_t>(&ExitThunk) || ThisThreadId() != g_thread) return ret;
  return g_stack.TopCaller(static_cast<uint32_t>(ret));
}
