// THE WAITS OF THE FRAME THREAD IN THE TIMELINE (trace, trace_rec.h). The game
// calls Sleep, SleepEx, WaitForSingleObject, WaitForSingleObjectEx,
// WaitForMultipleObjects, EnterCriticalSection, ReadFile and CreateFileA
// through the import table of runblack.exe (a slot that the exe does not use
// is not changed). With trace=1 the mod puts a function of its own in each of
// these import slots. The function times
// the call and gives a bar when the calling thread is the frame thread and the
// call took trace_wait_us or more. It then calls the function that the slot
// had before (the system, or a hook of another patch).
//
// An import slot is one pointer that the loader wrote, so no code is changed
// and no thread stops (docs/android.md, "SuspendThread under FEX in Wine"). A
// call from the mod or from a DLL of the game does not pass the table of the
// game, so it has no bar. The slots change once, at the start, before the
// game runs its frame loop.
//
// EnterCriticalSection first tries TryEnterCriticalSection and only a failed
// try is timed, so a lock that nobody holds costs one more call and no clock.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "thread_id.h"
#include "trace_rec.h"

void ModLog(const char* format, ...);

namespace te = trace_events;

namespace {

uint32_t g_frame_tid = 0;

const te::Def kSleepDef = {"Sleep", "wait", {"ms", nullptr, nullptr, nullptr}};
const te::Def kWaitDef = {"WaitForSingleObject", "wait", {"ms", nullptr, nullptr, nullptr}};
const te::Def kWaitManyDef = {"WaitForMultipleObjects", "wait", {"ms", nullptr, nullptr, nullptr}};
const te::Def kEnterDef = {"EnterCriticalSection", "wait", {nullptr, nullptr, nullptr, nullptr}};
const te::Def kReadDef = {"ReadFile", "io", {"bytes", nullptr, nullptr, nullptr}};
const te::Def kOpenDef = {"CreateFileA", "io", {nullptr, nullptr, nullptr, nullptr}};

typedef VOID(WINAPI* SleepFn)(DWORD);
typedef DWORD(WINAPI* SleepExFn)(DWORD, BOOL);
typedef DWORD(WINAPI* WaitFn)(HANDLE, DWORD);
typedef DWORD(WINAPI* WaitExFn)(HANDLE, DWORD, BOOL);
typedef DWORD(WINAPI* WaitManyFn)(DWORD, const HANDLE*, BOOL, DWORD);
typedef VOID(WINAPI* EnterFn)(LPCRITICAL_SECTION);
typedef BOOL(WINAPI* TryEnterFn)(LPCRITICAL_SECTION);
typedef BOOL(WINAPI* ReadFn)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef HANDLE(WINAPI* OpenFn)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

SleepFn o_Sleep = nullptr;
SleepExFn o_SleepEx = nullptr;
WaitFn o_Wait = nullptr;
WaitExFn o_WaitEx = nullptr;
WaitManyFn o_WaitMany = nullptr;
EnterFn o_Enter = nullptr;
TryEnterFn o_TryEnter = nullptr;
ReadFn o_Read = nullptr;
OpenFn o_Open = nullptr;

// The calls of the frame thread through the slots. Only the frame thread
// writes it.
uint32_t g_calls = 0;

inline bool OnFrameThread() {
  if (ThisThreadId() != g_frame_tid) return false;
  ++g_calls;
  return true;
}

VOID WINAPI w_Sleep(DWORD ms) {
  if (!OnFrameThread()) {
    o_Sleep(ms);
    return;
  }
  const int64_t b = TraceNow();
  o_Sleep(ms);
  TraceWait(&kSleepDef, b, TraceNow(), static_cast<float>(ms));
}

DWORD WINAPI w_SleepEx(DWORD ms, BOOL alertable) {
  if (!OnFrameThread()) return o_SleepEx(ms, alertable);
  const int64_t b = TraceNow();
  const DWORD r = o_SleepEx(ms, alertable);
  TraceWait(&kSleepDef, b, TraceNow(), static_cast<float>(ms));
  return r;
}

DWORD WINAPI w_Wait(HANDLE h, DWORD ms) {
  if (!OnFrameThread()) return o_Wait(h, ms);
  const int64_t b = TraceNow();
  const DWORD r = o_Wait(h, ms);
  TraceWait(&kWaitDef, b, TraceNow(), static_cast<float>(ms));
  return r;
}

DWORD WINAPI w_WaitEx(HANDLE h, DWORD ms, BOOL alertable) {
  if (!OnFrameThread()) return o_WaitEx(h, ms, alertable);
  const int64_t b = TraceNow();
  const DWORD r = o_WaitEx(h, ms, alertable);
  TraceWait(&kWaitDef, b, TraceNow(), static_cast<float>(ms));
  return r;
}

DWORD WINAPI w_WaitMany(DWORD n, const HANDLE* h, BOOL all, DWORD ms) {
  if (!OnFrameThread()) return o_WaitMany(n, h, all, ms);
  const int64_t b = TraceNow();
  const DWORD r = o_WaitMany(n, h, all, ms);
  TraceWait(&kWaitManyDef, b, TraceNow(), static_cast<float>(ms));
  return r;
}

VOID WINAPI w_Enter(LPCRITICAL_SECTION cs) {
  if (!OnFrameThread() || o_TryEnter == nullptr) {
    o_Enter(cs);
    return;
  }
  if (o_TryEnter(cs)) return;
  const int64_t b = TraceNow();
  o_Enter(cs);
  TraceWait(&kEnterDef, b, TraceNow());
}

BOOL WINAPI w_Read(HANDLE f, LPVOID out, DWORD bytes, LPDWORD got, LPOVERLAPPED ov) {
  if (!OnFrameThread()) return o_Read(f, out, bytes, got, ov);
  const int64_t b = TraceNow();
  const BOOL r = o_Read(f, out, bytes, got, ov);
  TraceWait(&kReadDef, b, TraceNow(), static_cast<float>(bytes));
  return r;
}

HANDLE WINAPI w_Open(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa,
                     DWORD creation, DWORD flags, HANDLE model) {
  if (!OnFrameThread()) return o_Open(name, access, share, sa, creation, flags, model);
  const int64_t b = TraceNow();
  const HANDLE h = o_Open(name, access, share, sa, creation, flags, model);
  TraceWait(&kOpenDef, b, TraceNow());
  return h;
}

struct Patch {
  const char* name;
  void* wrapper;
  void** original;
};

const Patch kPatches[] = {
    {"Sleep", reinterpret_cast<void*>(&w_Sleep), reinterpret_cast<void**>(&o_Sleep)},
    {"SleepEx", reinterpret_cast<void*>(&w_SleepEx), reinterpret_cast<void**>(&o_SleepEx)},
    {"WaitForSingleObject", reinterpret_cast<void*>(&w_Wait), reinterpret_cast<void**>(&o_Wait)},
    {"WaitForSingleObjectEx", reinterpret_cast<void*>(&w_WaitEx),
     reinterpret_cast<void**>(&o_WaitEx)},
    {"WaitForMultipleObjects", reinterpret_cast<void*>(&w_WaitMany),
     reinterpret_cast<void**>(&o_WaitMany)},
    {"EnterCriticalSection", reinterpret_cast<void*>(&w_Enter), reinterpret_cast<void**>(&o_Enter)},
    {"ReadFile", reinterpret_cast<void*>(&w_Read), reinterpret_cast<void**>(&o_Read)},
    {"CreateFileA", reinterpret_cast<void*>(&w_Open), reinterpret_cast<void**>(&o_Open)},
};

// The slots of the import table of the exe that name one of kPatches. Gives
// the count of the slots that changed.
int PatchImports(HMODULE exe, int* by_patch) {
  const uint8_t* base = reinterpret_cast<const uint8_t*>(exe);
  const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (dir.VirtualAddress == 0) return 0;
  int changed = 0;
  const IMAGE_IMPORT_DESCRIPTOR* d =
      reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
  for (; d->Name != 0; ++d) {
    const char* dll = reinterpret_cast<const char*>(base + d->Name);
    if (_strnicmp(dll, "kernel32", 8) != 0) continue;
    const IMAGE_THUNK_DATA* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(
        base + (d->OriginalFirstThunk != 0 ? d->OriginalFirstThunk : d->FirstThunk));
    IMAGE_THUNK_DATA* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(
        const_cast<uint8_t*>(base) + d->FirstThunk);
    for (int i = 0; names[i].u1.AddressOfData != 0; ++i) {
      if (IMAGE_SNAP_BY_ORDINAL32(names[i].u1.Ordinal)) continue;
      const IMAGE_IMPORT_BY_NAME* by_name =
          reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names[i].u1.AddressOfData);
      for (int k = 0; k < static_cast<int>(sizeof(kPatches) / sizeof(kPatches[0])); ++k) {
        if (strcmp(reinterpret_cast<const char*>(by_name->Name), kPatches[k].name) != 0) continue;
        DWORD old = 0;
        if (!VirtualProtect(&slots[i].u1.Function, sizeof(void*), PAGE_READWRITE, &old)) continue;
        // The first slot of a name gives the original. A second slot of the same
        // name (a second import table) keeps the same original.
        if (*kPatches[k].original == nullptr) {
          *kPatches[k].original = reinterpret_cast<void*>(slots[i].u1.Function);
        }
        slots[i].u1.Function = reinterpret_cast<DWORD>(kPatches[k].wrapper);
        VirtualProtect(&slots[i].u1.Function, sizeof(void*), old, &old);
        ++changed;
        ++by_patch[k];
      }
    }
  }
  return changed;
}

}  // namespace

uint32_t TraceWaitsCalls() { return g_calls; }

// From TraceInstall, on the frame thread.
void TraceWaitsInstall() {
  g_frame_tid = ThisThreadId();
  // The slot of TryEnterCriticalSection is not changed: the call comes from
  // kernel32 here.
  HMODULE k32 = GetModuleHandleA("kernel32.dll");
  o_TryEnter = k32 != nullptr
                   ? reinterpret_cast<TryEnterFn>(GetProcAddress(k32, "TryEnterCriticalSection"))
                   : nullptr;
  int by_patch[sizeof(kPatches) / sizeof(kPatches[0])] = {};
  const int changed = PatchImports(GetModuleHandleA(nullptr), by_patch);
  // A wrapper whose original did not come from a slot calls nothing: it never
  // runs, because it is in no slot.
  char names[256] = {};
  for (int k = 0; k < static_cast<int>(sizeof(kPatches) / sizeof(kPatches[0])); ++k) {
    if (by_patch[k] == 0) continue;
    lstrcatA(names, names[0] != 0 ? ", " : "");
    lstrcatA(names, kPatches[k].name);
  }
  ModLog("trace        : %d import slots of the game now time the waits of the frame thread "
         "(%s). A wait of trace_wait_us or more gets a bar",
         changed, names[0] != 0 ? names : "none");
}
