// THE PARTS OF THE GAME TURN (turn_parts, turn_parts_math.h, docs/backend.md,
// "The long game turn").
//
// The game turn is the call of GGame::ProcessNetworkPackets that runs
// GGame::ProcessOneGameTurn: StartTurn, ProcessTurn and EndTurn. A sampler of
// the place of the frame thread does not work under a translator of the code
// (FEX), so the mod times the large functions that the turn calls with the
// clock. The ways to find the large functions are the sampler of the turn on a
// PC (profile_sample_turn) and the timers of this file with turn_parts_add.
//
// HOW A TIMER WORKS (the same way as walk_parts.cpp). Each timed function gets
// a hook of MinHook to a stub of its own. The stub keeps each register and the
// flags, takes the clock, puts the address of ExitThunk in the place of the
// return address, and jumps to the function. At the return ExitThunk takes the
// clock, closes the timer and jumps to the return address of the caller. So a
// stub does not need the arguments or the calling convention of the function.
// The exit code uses integers only: the x87 unit can hold the value that the
// function returns.
//
// The function that a stub times sees the address of ExitThunk as its return
// address. The code of the game does not read it, but a hook of the mod can
// (_ReturnAddress). So a function that the mod hooks already (the same
// address) gets its timer on the trampoline of that hook (FindTrampoline): the
// code of the game alone.
//
// ALL HOOKS AT THE START. TurnPartsInstall queues the hooks and applies them
// with one MH_ApplyQueued, from PassGuardInstall, as anim_trig.cpp does. MinHook
// stops each other thread while it writes hooks, and a stop of a thread that a
// translator of the code translates can hang the process (docs/android.md,
// "SuspendThread under FEX in Wine"). No hook comes after the start of the game.
//
// Only the frame thread in a game turn counts. A call on another thread, or
// outside the turn, goes through the stub with no clock read. A part counts
// once, at its outer call. Timer 0 is the game turn, so its exclusive time is
// the rest: the turn outside each part.
//
//   turn_parts=0    no hook and no line (the default)
//   turn_parts=1    the timers on the parts of kDefs, each 10 s one line with
//                   the parts of the long turns (1 ms or more) of the window,
//                   and with slow_frame_ms the parts of the turn in each line
//                   of a long frame
//   turn_parts_add  more addresses to time, for example
//                   turn_parts_add=0x00730760,0x0063E090. The name of such a
//                   part is its address (fn_00730760)
//
// It needs time_log=1, which writes the lines.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MinHook.h"
#include "thread_id.h"
#include "time_log.h"
#include "turn_parts.h"
#include "turn_parts_math.h"
#include "walk_parts_math.h"

void ModLog(const char* format, ...);

bool g_turn_parts = false;

namespace {

namespace tp = turn_parts_math;
namespace wp = walk_parts_math;

// What a part counts besides its place.
enum Filter {
  kAll = 0,           // each call
  kMadeAnimation = 1  // only a call of Morphable::GetSetAnim that makes an animation
};

struct Def {
  uint32_t address;
  const char* name;
  int filter;
};

// The parts: GGame::ProcessOneGameTurn (0x0054D620), the three phases of the
// turn, and the functions that the phases call. Every direct callee of the
// phases has a timer, so the parts add up to the turn and the rest (the turn
// outside each part) stays near 0. A part with no name has the name fn_ and its
// address. `process_turn` has a call in each real turn. The helpers that the
// phases call many times (fn_00552F80, fn_00555850, fn_007AEE98) have no timer.
const Def kDefs[] = {
    {0x0054E5C0, "process_turn", kAll},
    {0x0054E4F0, "start_turn", kAll},
    {0x0054E960, "end_turn", kAll},
    {0x0054D620, "one_game_turn", kAll},
    {0x00649A20, "players_towns", kAll},
    {0x005EC810, nullptr, kAll},
    {0x006EB6B0, "script", kAll},
    {0x00619500, "morph_texture", kAll},
    {0x0043B7E0, "building_site", kAll},
    {0x008067F0, "mesh_load", kAll},
    {0x00619690, "new_animation", kMadeAnimation},
    {0x00427080, "audio_turn", kAll},
    {0x00644FC0, "physics_turn", kAll},
    // The other callees of ProcessTurn.
    {0x00555280, nullptr, kAll}, {0x00441F50, nullptr, kAll}, {0x0054E910, nullptr, kAll},
    {0x0050BB60, nullptr, kAll}, {0x0071A650, nullptr, kAll}, {0x008356E0, nullptr, kAll},
    {0x00775140, nullptr, kAll}, {0x004380B0, nullptr, kAll}, {0x0071D1C0, nullptr, kAll},
    {0x0071B020, nullptr, kAll}, {0x005E1FE0, nullptr, kAll}, {0x00720300, nullptr, kAll},
    {0x00539D70, nullptr, kAll}, {0x0070A460, nullptr, kAll}, {0x00633EF0, nullptr, kAll},
    {0x00550DD0, nullptr, kAll}, {0x0072E310, nullptr, kAll}, {0x0070D360, nullptr, kAll},
    {0x005C8FE0, nullptr, kAll}, {0x006E3B50, nullptr, kAll}, {0x007741A0, nullptr, kAll},
    {0x0046E4E0, nullptr, kAll}, {0x006EB710, nullptr, kAll}, {0x007E1420, nullptr, kAll},
    {0x00435F30, nullptr, kAll}, {0x006F6910, nullptr, kAll}, {0x00439DD0, nullptr, kAll},
    {0x0056FB10, nullptr, kAll}, {0x00730760, nullptr, kAll}, {0x005CDB90, nullptr, kAll},
    {0x0052B7A0, nullptr, kAll}, {0x00557B60, nullptr, kAll}, {0x00771BE0, nullptr, kAll},
    {0x0067D630, nullptr, kAll}, {0x006EBBC0, nullptr, kAll}, {0x00591370, nullptr, kAll},
    {0x00441D40, nullptr, kAll}, {0x0076EAF0, nullptr, kAll}, {0x0063E090, nullptr, kAll},
    {0x00606FC0, nullptr, kAll}, {0x0070B220, nullptr, kAll}, {0x0068F590, nullptr, kAll},
    {0x0068F5B0, nullptr, kAll}, {0x00552640, nullptr, kAll}, {0x006E6890, nullptr, kAll},
    {0x005C4660, nullptr, kAll},
    // The callees of StartTurn.
    {0x00511F50, nullptr, kAll}, {0x00557BA0, nullptr, kAll},
    // The callees of EndTurn.
    {0x007DEE00, nullptr, kAll}, {0x0071E5F0, nullptr, kAll}, {0x00564050, nullptr, kAll},
    {0x00565110, nullptr, kAll}, {0x0071D6F0, nullptr, kAll}, {0x006360B0, nullptr, kAll},
    {0x006016D0, nullptr, kAll}, {0x00601850, nullptr, kAll}, {0x004286C0, nullptr, kAll},
};
constexpr int kDefCount = sizeof(kDefs) / sizeof(kDefs[0]);
// The index of process_turn in kDefs: a game turn with a call of it is a real
// turn.
constexpr int kProcessTurnDef = 0;

// The animations of a Morphable (anim_watch.cpp): a table of the object at
// +0x1D4, one slot for each number (0x2B8) and each kind. An empty slot means
// that GetSetAnim makes the animation.
const int kSlotsOffset = 0x1D4;
const int kAnimsPerKind = 0x2B8;
const int kKinds = 6;

struct Timer {
  uint32_t shown = 0;       // the address of the function
  uint32_t at = 0;          // the code that has the hook: shown, or its trampoline
  void* original = nullptr; // the trampoline that the stub jumps to
  int filter = kAll;
  char name[48] = {};
};

Timer g_timers[tp::kMaxParts];
const char* g_names[tp::kMaxParts];
int g_count = 1;  // timer 0 is the rest
int g_process_turn_id = 0;
uint8_t* g_stubs = nullptr;
const int kStubBytes = 10;

DWORD g_thread = 0;  // the frame thread
bool g_in_turn = false;
uint64_t g_seen_in_turn = 0;       // calls of a stub in a game turn on the frame thread
uint64_t g_seen_outside = 0;       // calls of a stub on the frame thread outside a game turn
uint64_t g_seen_other_thread = 0;  // calls of a stub on another thread
uint64_t g_turn_function_calls = 0;
LARGE_INTEGER g_hz = {};
double g_ms_per_tick = 0.0;
wp::Stack g_stack;
tp::Frame g_frame;
tp::Window g_window;
uint64_t g_lost = 0;  // open calls that no return closed, in all windows

int64_t Now() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

bool SlotEmpty(uint32_t self, uint32_t anim, uint32_t kind) {
  if (self == 0 || anim >= static_cast<uint32_t>(kAnimsPerKind) ||
      kind >= static_cast<uint32_t>(kKinds)) {
    return false;
  }
  const uint8_t* slot = reinterpret_cast<const uint8_t*>(self) + kSlotsOffset +
                        (anim + kind * kAnimsPerKind) * 4;
  return *reinterpret_cast<const uint32_t*>(slot) == 0;
}

void __cdecl ExitThunk();
void __cdecl NoReturn();

}  // namespace

// The stub of a timer pushes its id and jumps to EnterThunk. The stack then has
// the id at [esp] and the return address of the caller at [esp+4], and the
// arguments of the function after it. `self` is ecx at the call.
extern "C" uint32_t __cdecl TurnPartsEnter(uint32_t id, uint32_t* ret_slot, uint32_t self) {
  const Timer& t = g_timers[id];
  // The counts of the other threads are not exact (no lock). They are a hint.
  if (ThisThreadId() != g_thread) {
    ++g_seen_other_thread;
  } else if (!g_in_turn) {
    ++g_seen_outside;
  } else {
    ++g_seen_in_turn;
  }
  if (g_in_turn && ThisThreadId() == g_thread &&
      tp::Timed(true, true, g_stack.nest[id], g_stack.depth)) {
    const bool take = t.filter != kMadeAnimation || SlotEmpty(self, ret_slot[1], ret_slot[2]);
    if (take &&
        g_stack.Push(static_cast<int>(id), Now(), *ret_slot,
                     static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ret_slot)))) {
      *ret_slot = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ExitThunk));
    }
  }
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t.original));
}

// ExitThunk comes here with the stack place after the return of the function.
// Integers only.
extern "C" uint32_t __cdecl TurnPartsExit(uint32_t esp) {
  const uint32_t ret = g_stack.Return(Now(), esp);
  if (ret != 0) return ret;
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&NoReturn));
}

namespace {

__declspec(naked) void EnterThunk() {
  __asm {
    pushfd
    pushad
    // [esp+36] is the id, [esp+40] the return address. pushad keeps ecx at
    // [esp+24].
    mov eax, [esp + 24]
    push eax
    lea eax, [esp + 44]
    push eax
    push dword ptr [esp + 44]
    call TurnPartsEnter
    add esp, 12
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
    call TurnPartsExit
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

bool Known(uint32_t address) {
  for (int i = 1; i < g_count; ++i) {
    if (g_timers[i].shown == address || g_timers[i].at == address) return true;
  }
  return false;
}

// True when the code at `t` is a trampoline of MinHook for `target`: memory of
// no module, with a jump back into `target` 5 to 32 bytes after its start in
// its first 32 bytes.
bool IsTrampolineOf(uint32_t t, uint32_t target) {
  MEMORY_BASIC_INFORMATION mbi = {};
  if (VirtualQuery(reinterpret_cast<const void*>(t), &mbi, sizeof(mbi)) == 0 ||
      mbi.Type != MEM_PRIVATE || mbi.State != MEM_COMMIT ||
      (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0 ||
      (mbi.Protect & PAGE_GUARD) != 0) {
    return false;
  }
  // A trampoline can be the last one of a block of MinHook, so the bytes after
  // it can be past the end of the memory.
  const uint32_t end = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mbi.BaseAddress)) +
                       static_cast<uint32_t>(mbi.RegionSize);
  const int room = static_cast<int>(end - t);
  const uint8_t* b = reinterpret_cast<const uint8_t*>(t);
  for (int j = 0; j < 32 && j + 5 <= room; ++j) {
    if (b[j] != 0xE9) continue;
    int32_t rel = 0;
    memcpy(&rel, b + j + 1, 4);
    const uint32_t dest = t + j + 5 + static_cast<uint32_t>(rel);
    if (dest >= target + 5 && dest <= target + 32) return true;
  }
  return false;
}

// THE CODE OF THE GAME UNDER A HOOK OF THE MOD. A stub on the hook of the mod
// itself would change the return address that the hook reads, so the timer goes
// on the trampoline of MinHook, which the hook calls for the code of the game.
// The hook reads the address of the trampoline from a variable of the mod (o_
// and the name of the function). The search reads each 4 bytes of the first 2
// KiB of the code of the hook as an address in the image of the mod, and takes
// the value there when it is a trampoline of `target`. 0 when there is none.
uint32_t FindTrampoline(uint32_t hook, uint32_t target) {
  HMODULE mod = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(hook), &mod)) {
    return 0;
  }
  const uint8_t* base = reinterpret_cast<const uint8_t*>(mod);
  const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
      base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
  const uint32_t lo = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base));
  const uint32_t hi = lo + nt->OptionalHeader.SizeOfImage;
  const uint8_t* code = reinterpret_cast<const uint8_t*>(hook);
  for (int i = 0; i < 2048; ++i) {
    if (IsBadReadPtr(code + i, 4)) break;
    uint32_t v = 0;
    memcpy(&v, code + i, 4);
    if (v < lo || v + 4 > hi || (v & 3) != 0) continue;
    if (IsBadReadPtr(reinterpret_cast<const void*>(v), 4)) continue;
    const uint32_t t = *reinterpret_cast<const uint32_t*>(v);
    if (t != 0 && IsTrampolineOf(t, target)) return t;
  }
  return 0;
}

// A timer on the code at `address`, queued. When the mod hooks that code
// already, the timer goes on the trampoline of that hook. Gives false and a
// line when there is no timer.
bool Add(uint32_t address, const char* name, int filter) {
  if (g_count >= tp::kMaxParts || address == 0) return false;
  if (Known(address)) {
    ModLog("turn parts   : %s at 0x%08X has a timer already. No second timer", name,
           static_cast<unsigned>(address));
    return false;
  }
  uint32_t at = address;
  bool under_hook = false;
  void* original = nullptr;
  MH_STATUS st =
      MH_CreateHook(reinterpret_cast<void*>(at), g_stubs + g_count * kStubBytes, &original);
  if (st == MH_ERROR_ALREADY_CREATED) {
    // The hook of the mod is a jump at the start of the code.
    const uint8_t* code = reinterpret_cast<const uint8_t*>(address);
    uint32_t trampoline = 0;
    int32_t rel = 0;
    if (!IsBadReadPtr(code, 5) && code[0] == 0xE9) {
      memcpy(&rel, code + 1, 4);
      trampoline = FindTrampoline(address + 5 + static_cast<uint32_t>(rel), address);
    }
    if (trampoline == 0 || Known(trampoline)) {
      ModLog("turn parts   : no timer on %s at 0x%08X: the mod hooks it, and the trampoline of "
             "that hook is not found in the hook at 0x%08X",
             name, static_cast<unsigned>(address),
             static_cast<unsigned>(address + 5 + static_cast<uint32_t>(rel)));
      return false;
    }
    at = trampoline;
    under_hook = true;
    st = MH_CreateHook(reinterpret_cast<void*>(at), g_stubs + g_count * kStubBytes, &original);
  }
  if (st != MH_OK) {
    ModLog("turn parts   : no timer on %s at 0x%08X (%s)", name, static_cast<unsigned>(address),
           MH_StatusToString(st));
    return false;
  }
  if (MH_QueueEnableHook(reinterpret_cast<void*>(at)) != MH_OK) {
    MH_RemoveHook(reinterpret_cast<void*>(at));
    ModLog("turn parts   : no timer on %s at 0x%08X (the hook did not queue)", name,
           static_cast<unsigned>(address));
    return false;
  }
  Timer& t = g_timers[g_count];
  t.shown = address;
  t.at = at;
  t.original = original;
  t.filter = filter;
  _snprintf_s(t.name, sizeof(t.name), _TRUNCATE, "%s", name);
  g_names[g_count] = t.name;
  MakeStub(g_count);
  if (under_hook) {
    ModLog("turn parts   : %s at 0x%08X is under a hook of the mod, so its timer is on the code "
           "of the game alone",
           name, static_cast<unsigned>(address));
  }
  ++g_count;
  return true;
}

int IniKey(const char* key, int fallback) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return fallback;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

void IniText(const char* key, char* out, DWORD size) {
  out[0] = 0;
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  GetPrivateProfileStringA("bwvr", key, "", out, size, path);
}

}  // namespace

// From PassGuardInstall, on the frame thread.
void TurnPartsInstall() {
  if (IniKey("turn_parts", 0) == 0) return;
  if (IniKey("time_log", 1) == 0) {
    ModLog("turn parts   : turn_parts=1 needs time_log=1, which writes the lines. No hook");
    return;
  }
  QueryPerformanceFrequency(&g_hz);
  g_thread = ThisThreadId();
  g_stubs = static_cast<uint8_t*>(VirtualAlloc(nullptr, tp::kMaxParts * kStubBytes,
                                               MEM_COMMIT | MEM_RESERVE,
                                               PAGE_EXECUTE_READWRITE));
  if (g_stubs == nullptr || g_hz.QuadPart == 0) {
    ModLog("turn parts   : no memory for the stubs, or no clock. No hook");
    return;
  }
  g_ms_per_tick = 1000.0 / static_cast<double>(g_hz.QuadPart);
  g_names[0] = "outside_parts";
  _snprintf_s(g_timers[0].name, sizeof(g_timers[0].name), _TRUNCATE, "outside_parts");
  g_timers[0].shown = 0x0054CC30;  // GGame::ProcessNetworkPackets, the turn itself
  int in = 0;
  int defs_in = 0;
  for (int i = 0; i < kDefCount; ++i) {
    char auto_name[32];
    _snprintf_s(auto_name, sizeof(auto_name), _TRUNCATE, "fn_%08X",
                static_cast<unsigned>(kDefs[i].address));
    const bool ok = Add(kDefs[i].address, kDefs[i].name != nullptr ? kDefs[i].name : auto_name,
                        kDefs[i].filter);
    if (i == kProcessTurnDef) g_process_turn_id = ok ? g_count - 1 : 0;
    in += ok ? 1 : 0;
    defs_in += ok ? 1 : 0;
  }
  char list[1536];
  IniText("turn_parts_add", list, sizeof(list));
  int more = 0;
  for (char* p = list; *p != 0;) {
    char* end = nullptr;
    const unsigned long a = strtoul(p, &end, 0);
    if (end == p) break;
    char name[32];
    _snprintf_s(name, sizeof(name), _TRUNCATE, "fn_%08lX", a);
    if (Add(static_cast<uint32_t>(a), name, kAll)) {
      ++in;
      ++more;
    }
    p = end;
    while (*p == ',' || *p == ' ') ++p;
  }
  // One stop of the other threads for all hooks (anim_trig.cpp, HookAll).
  if (in == 0 || g_process_turn_id == 0 || MH_ApplyQueued() != MH_OK) {
    for (int i = 1; i < g_count; ++i) MH_RemoveHook(reinterpret_cast<void*>(g_timers[i].at));
    ModLog("turn parts   : %s. No hook, no line",
           g_process_turn_id == 0 ? "the timer on ProcessTurn failed, so no turn counts"
                                  : "the hooks did not apply");
    g_count = 1;
    return;
  }
  g_turn_parts = true;
  ModLog("turn parts   : turn_parts=1. %d timers on the functions of the game turn (%d of %d "
         "parts and %d more by turn_parts_add). Each 10 s one line with the parts of the turns "
         "of %.0f ms or more, and with slow_frame_ms the parts of the turn in each line of a "
         "long frame. The time of a part is exclusive: less the parts that it calls",
         in, defs_in, kDefCount, more, tp::kLongTurnMs);
}

// h_ProcessNetworkPackets: the call starts. The stack starts with the turn as
// timer 0.
void TurnPartsTurnBegin(int64_t begin) {
  ++g_turn_function_calls;
  if (g_stack.depth != 0) g_stack = wp::Stack();
  g_stack.Push(0, begin, 0, 0);
  g_in_turn = true;
}

// h_ProcessNetworkPackets: the call ends. A call with no game turn (the game
// asks whether one is due) has no call of process_turn and is not a turn.
void TurnPartsTurnEnd(int64_t end) {
  g_in_turn = false;
  while (g_stack.depth > 1) g_stack.DropTop();
  if (g_stack.depth == 1) g_stack.CloseTop(end);
  g_lost += g_stack.lost;
  if (g_stack.calls[g_process_turn_id] > 0) {
    static tp::Turn turn;
    turn.FromStack(g_stack, g_count, g_ms_per_tick);
    g_window.AddTurn(turn, g_count);
    if (g_time_log_slow) g_frame.AddTurn(turn, g_count);
  }
  g_stack = wp::Stack();
}

int TurnPartsFrameText(char* out, size_t size) {
  if (!g_turn_parts) {
    if (size > 0) out[0] = 0;
    return 0;
  }
  return tp::FrameText(out, size, g_frame, g_count, g_names);
}

void TurnPartsFrameEnd() {
  if (g_turn_parts) g_frame.Clear();
}

bool TurnPartsWindowLine(double seconds) {
  if (!g_turn_parts) return false;
  static char line[8192];
  g_window.turn_function_calls = g_turn_function_calls;
  g_window.stub_in_turn = g_seen_in_turn;
  g_window.stub_outside = g_seen_outside;
  g_window.stub_other_thread = g_seen_other_thread;
  g_turn_function_calls = g_seen_in_turn = g_seen_outside = g_seen_other_thread = 0;
  tp::WindowLine(line, sizeof(line), g_window, g_count, g_names, seconds);
  ModLog("turn parts   : %s%s", line,
         g_lost > 0 ? ". A call that no return closed was lost (a fault)" : "");
  g_window.Reset();
  return true;
}
