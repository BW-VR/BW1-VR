// THE PARTS OF THE SCENE WALK (walk_parts.h, docs/backend.md, "The parts of
// the scene walk").
//
// A sampler of the place of the frame thread does not work under a translator
// of the code (FEX): it gives the place of the last call of the system. So the
// mod times the parts of the walk with the clock. Each timed function gets a
// hook of MinHook to a stub of its own. The stub keeps each register and the
// flags, takes the clock, puts the address of ExitThunk in the place of the
// return address, and jumps to the function. At the return ExitThunk takes
// the clock, closes the timer and jumps to the return address of the caller.
// So a stub does not need the arguments or the calling convention of the
// function. The exit code uses integers only: the x87 unit can hold the value
// that the function returns.
//
// The function that a stub times sees the address of ExitThunk as its return
// address. The code of the game does not read it, but some hooks of the mod
// do (_ReturnAddress). So a function that the mod hooks already gets its
// timer on the trampoline of that hook (FindTrampoline): the code of the game
// alone. The time of the hook of the mod is then in the time of its caller.
// The hook of the mod on FinishFrame has a timer of its own in its code
// (WalkPartsHookBegin).
//
// Only the frame thread in a scene walk counts. A call on another thread, or
// outside a walk, goes through the stub with no clock read. The hooks come at
// the third walk, when the device of the game is made. A frame is the walks
// from one present to the next.
//
//   walk_parts=0      no hook and no line (the default)
//   walk_parts=1      each timer
//   walk_parts=2      no timer on each call of the device and of the
//                     Vulkan renderer of the mod
//   walk_parts_add    more addresses to time, for example
//                     walk_parts_add=0x007FF610,0x00879930
//   walk_parts_clock  1 (the default): the clock of the timers is the time
//                     stamp counter of the CPU, and the performance counter
//                     gives its rate over each window. 0: each timer reads
//                     QueryPerformanceCounter

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d.h>
#include <intrin.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/bw_addresses.h"
#include "MinHook.h"
#include "fast_transform.h"
#include "thread_id.h"
#include "vk_scene.h"
#include "vk_stream.h"
#include "walk_parts.h"
#include "walk_parts_math.h"

void ModLog(const char* format, ...);
// The calls of the hook of the mod on FinishFrame (mod_main.cpp), which get
// timers of their own.
float UiBarsBegin();
void UiBarsEnd(float saved);
void UiPanelBeforeFinish(bool in_stereo_pass);
void UiPanelAfterFinish(bool in_stereo_pass, uint64_t frame);
void ControllersScreenApply();
void XrInputNewFrame();
void AudioFrame();
void SetupBoxWatchFrame(uint64_t finish_frame);
void GpuBridgeResume();
void HangWatchFrame();
void StallWatchFrame();
void TabletopCountLists();

namespace {

namespace wp = walk_parts_math;

// The groups of timers. kEach is a timer on each call of the device or of the
// renderer: walk_parts=2 leaves them out.
enum Group { kGame = 0, kEach = 1 };

struct Fn {
  uint32_t address;
  const char* name;
  int group;
};

// The functions of the game, from the call tree of GGame::Process3dEngine
// (0x0054DA80). The names are the names of bw1-decomp, or a short own name
// with the address.
const Fn kGameFns[] = {
    {0x0082F0E0, "LH3DRender::StartFrame", kGame},
    {0x00644FC0, "PhysicsObject::GameTurnUpdate", kGame},
    {0x008357A0, "LH3DAtmos::Update3D", kGame},
    {0x00825190, "TemporaryShadow::UpdateAll", kGame},
    {0x005E2160, "GLandAlignement::DrawSky", kGame},
    {0x00794370, "Temple::Draw", kGame},
    {0x005E3F60, "GLandscape::PreDraw", kGame},
    {0x006CA6E0, "PSysLightMaps::AddDrawing", kGame},
    {0x00871F00, "LH3DLandscape::TextureUpdateThread", kGame},
    {0x005E42E0, "GLandscape::Draw", kGame},
    {0x0048DD70, "LH3DCreature::DrawFightSparkles", kGame},
    {0x005D9310, "GInterface::DrawAllLeashes", kGame},
    {0x00646DE0, "PhysicsObject::DrawAll", kGame},
    {0x0046E490, "CHand::UpdateHeldObject", kGame},
    {0x0046D100, "CHand::AddDrawing", kGame},
    {0x00518640, "GInterface::Draw", kGame},
    {0x00845C50, "DrawLiquidParticles", kGame},
    {0x005533B0, "GGame::Draw", kGame},
    {0x0068F5E0, "PSysGlobal::DrawLoop", kGame},
    {0x0052B790, "FireFly::DrawAll", kGame},
    {0x007203F0, "Spell::DrawSpells", kGame},
    {0x0063E0D0, "GParticleContainer::DrawParticleContainers", kGame},
    {0x0064C140, "GPlayer::DrawPlayers", kGame},
    {0x007447F0, "TownCentre::DrawAll", kGame},
    {0x00836250, "LH3DAtmos::Render3D", kGame},
    {0x005C59A0, "HelpSystem::Draw3D", kGame},
    {0x00826C90, "InfluenceCircle::Draw", kGame},
    {0x0082F460, "LH3DRender::FinishFrame", kGame},
    {0x008366A0, "LH3DAtmos::Render2D", kGame},
    // In GLandscape::Draw.
    {0x007FF610, "the land, fn_007FF610 (LH3DIsland::Draw)", kGame},
    {0x00879930, "the sea, fn_00879930 (LH3DWater)", kGame},
    {0x00646FE0, "fn_00646FE0, in GLandscape::Draw", kGame},
    {0x005E5830, "fn_005E5830, in GLandscape::Draw", kGame},
    {0x0074A7C0, "Tree::PreDraw", kGame},
    {0x004059F0, "Windmill::PreDraw", kGame},
    {0x00802550, "fn_00802550, in GLandscape::Draw", kGame},
    {0x008296D0, "fn_008296D0, in GLandscape::Draw", kGame},
    {0x007FF4F0, "fn_007FF4F0, in GLandscape::Draw", kGame},
    {0x00874850, "fn_00874850, in GLandscape::Draw", kGame},
    {0x005E5100, "fn_005E5100, in GLandscape::Draw", kGame},
    {0x005E5CD0, "fn_005E5CD0, in GLandscape::Draw", kGame},
    {0x0081F820, "fn_0081F820, in GLandscape::Draw", kGame},
    {0x00824B90, "fn_00824B90, in GLandscape::Draw", kGame},
    {0x005DFCE0, "fn_005DFCE0, in GLandscape::Draw", kGame},
    // In the land.
    {0x007FF2D0, "LH3DIsland::PreDraw", kGame},
    {0x00877D20, "fn_00877D20, in the land", kGame},
    {0x00874AA0, "fn_00874AA0, in the land", kGame},
    {0x00878350, "fn_00878350, in the land", kGame},
    {0x00876910, "fn_00876910, in fn_00874AA0", kGame},
    {0x00875C60, "fn_00875C60, in fn_00874AA0", kGame},
    {0x0081E590, "fn_0081E590, in FinishFrame and GGame::Draw", kGame},
    // In LH3DRender::FinishFrame: the draws of the z sorter, and two more.
    {0x0082F280, "the sorted draws, fn_0082F280", kGame},
    {0x0086BB60, "fn_0086BB60, in FinishFrame", kGame},
    {0x0086FEE0, "fn_0086FEE0, in FinishFrame", kGame},
    {0x00836200, "fn_00836200, in FinishFrame", kGame},
    // The meshes and the sprites.
    {0x00856D40, "a mesh with clip, fn_00856D40", kGame},
    {0x0085AB10, "a lit mesh, fn_0085AB10", kGame},
    {0x0084BA90, "a lit mesh with clip, fn_0084BA90", kGame},
    {0x00840530, "LH3DSprite::Draw", kGame},
    // The draw calls of the engine.
    {0x0082F810, "LH3DRender::DrawTriangle", kGame},
    {0x0082A500, "DrawAndClip", kGame},
    {0x00412940, "LH3DRender::SetRenderState", kEach},
};

// The methods of the device and of the surfaces of the backend (dxwrapper, or
// the device of the mod with backend=vulkan) that a walk calls, by their slot
// in the table of methods.
struct Slot {
  int slot;
  const char* name;
};
const Slot kDeviceSlots[] = {
    {5, "IDirect3DDevice7::BeginScene"},
    {6, "IDirect3DDevice7::EndScene"},
    {10, "IDirect3DDevice7::Clear"},
    {11, "IDirect3DDevice7::SetTransform"},
    {18, "IDirect3DDevice7::SetLight"},
    {20, "IDirect3DDevice7::SetRenderState"},
    {25, "IDirect3DDevice7::DrawPrimitive"},
    {26, "IDirect3DDevice7::DrawIndexedPrimitive"},
    {35, "IDirect3DDevice7::SetTexture"},
    {37, "IDirect3DDevice7::SetTextureStageState"},
};
const Slot kSurfaceSlots[] = {
    {5, "IDirectDrawSurface7::Blt"},
    {25, "IDirectDrawSurface7::Lock"},
    {32, "IDirectDrawSurface7::Unlock"},
};

struct Timer {
  uint32_t address = 0;  // the code that the hook is on
  uint32_t shown = 0;    // the address in the line
  char name[96] = {};
  void* original = nullptr;
};

int g_mode = 0;
char g_add[512] = {};
bool g_installed = false;
bool g_failed = false;
bool g_in_walk = false;
bool g_calibrate = false;
bool g_frame_has_walk = false;
unsigned long g_thread = 0;
uint64_t g_walks = 0;
uint32_t g_presents = 0;
int g_clock = 1;             // walk_parts_clock
int64_t g_window_start = 0;  // the clock of the timers at the start of the window
int64_t g_window_qpc = 0;    // the performance counter at the start of the window
double g_pair_ns = 0.0;
double g_tick_mhz = 0.0;  // the rate of the clock of the timers, from the measure
LARGE_INTEGER g_hz = {};

Timer g_timers[wp::kMaxTimers];
// Timer 0 is the scene walk, timer 1 the hook of the mod on FinishFrame.
const int kHookTimer = 1;
int g_count = 2;
int g_hook_open = -1;  // the place of the open call of timer 1 on the stack, or -1
wp::Stack g_stack;
wp::Window g_window;
uint8_t* g_stubs = nullptr;
const int kStubBytes = 10;

int64_t Qpc() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

// The clock of the timers. walk_parts_clock=1: the time stamp counter of the
// CPU (RDTSC, under FEX the counter of the ARM timer), with no call of the
// system. QueryPerformanceCounter of Wine can be a call of the system, and a
// timer pair then costs more than most functions that it times. 0: the
// performance counter.
int64_t Now() {
  if (g_clock == 1) return static_cast<int64_t>(__rdtsc());
  return Qpc();
}

// The ms of the performance counter from `q0` to `q1`.
double QpcMs(int64_t q0, int64_t q1) {
  return static_cast<double>(q1 - q0) * 1000.0 / static_cast<double>(g_hz.QuadPart);
}

void __cdecl ExitThunk();
void __cdecl NoReturn();

}  // namespace

// The stub of a timer pushes its id and jumps to EnterThunk. The stack then
// has the id at [esp] and the return address of the caller at [esp+4].
extern "C" uint32_t __cdecl WalkPartsEnter(uint32_t id, uint32_t* ret_slot) {
  if ((g_in_walk || g_calibrate) && ThisThreadId() == g_thread) {
    const int64_t now = Now();
    if (g_stack.Push(static_cast<int>(id), now, *ret_slot,
                     static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ret_slot)))) {
      *ret_slot = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ExitThunk));
    }
  }
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_timers[id].original));
}

// ExitThunk comes here with the stack place after the return of the
// function. Integers only.
extern "C" uint32_t __cdecl WalkPartsExit(uint32_t esp) {
  const uint32_t ret = g_stack.Return(Now(), esp);
  if (ret != 0) return ret;
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
    call WalkPartsEnter
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
    call WalkPartsExit
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

// The function of the measure of the cost of a timer.
volatile int g_sink = 0;
__declspec(noinline) int __cdecl CostProbe(int x) {
  g_sink = g_sink + x;
  return g_sink;
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
  for (int i = 2; i < g_count; ++i) {
    if (g_timers[i].address == address || g_timers[i].shown == address) return true;
  }
  return false;
}

// True when the code at `t` is a trampoline of MinHook for `target`: memory
// of no module, with a jump back into `target` 5 to 32 bytes after its start
// in its first 32 bytes.
bool IsTrampolineOf(uint32_t t, uint32_t target) {
  MEMORY_BASIC_INFORMATION mbi = {};
  if (VirtualQuery(reinterpret_cast<const void*>(t), &mbi, sizeof(mbi)) == 0 ||
      mbi.Type != MEM_PRIVATE || mbi.State != MEM_COMMIT ||
      (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0 ||
      (mbi.Protect & PAGE_GUARD) != 0) {
    return false;
  }
  // A trampoline can be the last one of a block of MinHook, so the bytes
  // after it can be past the end of the memory.
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
// itself would change the return address that the hook reads, and some hooks
// of the mod act on it (_ReturnAddress). So the timer goes on the trampoline
// of MinHook, which the hook calls for the code of the game. The hook reads
// the address of the trampoline from a variable of the mod (o_ and the name of
// the function). The search reads each 4 bytes of the first 2 KiB of the code
// of the hook as an address in the image of the mod, and takes the value
// there when it is a trampoline of `target`. 0 when there is none.
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

// A timer on the code at `address`. When the mod hooks that code already, the
// timer goes on the trampoline of that hook (the code of the game alone).
// Gives false and a line when there is no timer.
bool Add(uint32_t address, const char* name, int group, bool quiet = false) {
  if (group == kEach && g_mode == 2) return false;
  if (g_count >= wp::kMaxTimers - 1) return false;
  if (address == 0) return false;
  if (Known(address)) {
    if (quiet) return false;
    ModLog("walk parts   : %s at 0x%08X has a timer already. No timer", name,
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
      ModLog("walk parts   : no timer on %s at 0x%08X: the mod hooks it, and the trampoline "
             "of that hook is not found in the hook at 0x%08X",
             name, static_cast<unsigned>(address),
             static_cast<unsigned>(address + 5 + static_cast<uint32_t>(rel)));
      return false;
    }
    at = trampoline;
    under_hook = true;
    st = MH_CreateHook(reinterpret_cast<void*>(at), g_stubs + g_count * kStubBytes, &original);
  }
  if (st != MH_OK) {
    ModLog("walk parts   : no timer on %s at 0x%08X (%s)", name, static_cast<unsigned>(address),
           MH_StatusToString(st));
    return false;
  }
  Timer& t = g_timers[g_count];
  t.address = at;
  t.shown = address;
  t.original = original;
  _snprintf_s(t.name, sizeof(t.name), _TRUNCATE, "%s%s", name,
              under_hook ? " (the code of the game under a hook of the mod)" : "");
  MakeStub(g_count);
  ++g_count;
  return true;
}

// THE LISTS OF THE ENGINE. The z sorter (LH3DZSorter::NewZObject, 0x0083F310)
// keeps the draws of a frame in a list of 24 bytes for each entry at
// 0x00EDDD30, with the function at +8 and the count at 0x00EE9D34 (at most
// 0x800). fn_0082F280 calls them in FinishFrame, and StartFrame sets the
// count to 0. So before the walk the list holds the draws of the frame
// before. FinishFrame also calls the callbacks of a table at 0x00EC8170 (16
// entries at most, the count at 0x00ECA600), and the method at +0x108 of the
// table of methods of each object of a list at 0x00EA1AEC (the next object
// at +0xB0). Each new function of the three lists gets a timer, at the start
// and with each report.
const uint32_t kZList = 0x00EDDD30;
const uint32_t kZCount = 0x00EE9D34;
const uint32_t kCallbacks = 0x00EC8170;
const uint32_t kCallbackCount = 0x00ECA600;
const uint32_t kObjects = 0x00EA1AEC;

bool InGame(uint32_t a) { return a >= 0x00401000 && a < 0x008A0000; }

int ScanLists() {
  const int before = g_count;
  int32_t n = *reinterpret_cast<const int32_t*>(kZCount);
  if (n > 0x800) n = 0x800;
  char name[64];
  for (int i = 0; i < n; ++i) {
    const uint32_t fn = *reinterpret_cast<const uint32_t*>(kZList + 24 * i + 8);
    if (!InGame(fn)) continue;
    _snprintf_s(name, sizeof(name), _TRUNCATE, "a sorted draw, fn_%08X",
                static_cast<unsigned>(fn));
    Add(fn, name, kGame, true);
  }
  int32_t c = *reinterpret_cast<const int32_t*>(kCallbackCount);
  if (c > 16) c = 16;
  for (int i = 0; i < c; ++i) {
    const uint32_t fn = *reinterpret_cast<const uint32_t*>(kCallbacks + 4 * i);
    if (!InGame(fn)) continue;
    _snprintf_s(name, sizeof(name), _TRUNCATE, "a callback of FinishFrame, fn_%08X",
                static_cast<unsigned>(fn));
    Add(fn, name, kGame, true);
  }
  uint32_t obj = *reinterpret_cast<const uint32_t*>(kObjects);
  for (int i = 0; obj != 0 && i < 256; ++i) {
    if (IsBadReadPtr(reinterpret_cast<const void*>(obj), 0xB4)) break;
    const uint32_t table = *reinterpret_cast<const uint32_t*>(obj);
    if (!IsBadReadPtr(reinterpret_cast<const void*>(table + 0x108), 4)) {
      const uint32_t fn = *reinterpret_cast<const uint32_t*>(table + 0x108);
      if (InGame(fn)) {
        _snprintf_s(name, sizeof(name), _TRUNCATE, "an object of FinishFrame, fn_%08X",
                    static_cast<unsigned>(fn));
        Add(fn, name, kGame, true);
      }
    }
    obj = *reinterpret_cast<const uint32_t*>(obj + 0xB0);
  }
  return g_count - before;
}

// The hooks of the timers from `first` on go on. Gives the count.
int EnableFrom(int first) {
  int enabled = 0;
  for (int i = first; i < g_count; ++i) {
    if (MH_EnableHook(reinterpret_cast<void*>(g_timers[i].address)) == MH_OK) ++enabled;
  }
  return enabled;
}

void AddTable(void* object, const Slot* slots, int count) {
  if (object == nullptr) return;
  void** table = *reinterpret_cast<void***>(object);
  for (int i = 0; i < count; ++i) {
    const uint32_t fn = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(table[slots[i].slot]));
    Add(fn, slots[i].name, kEach);
  }
}

// The cost of one timed call: the time of calls of CostProbe with a timer,
// less the time of the same calls through the trampoline, with no timer. The
// least time of 5 rounds of each counts, so a stop of the thread by the
// system in one round does not count.
double MeasureCost() {
  const int kCalls = 20000;
  const int kRounds = 5;
  const int id = g_count;
  void* original = nullptr;
  if (MH_CreateHook(reinterpret_cast<void*>(&CostProbe), g_stubs + id * kStubBytes,
                    &original) != MH_OK) {
    return 0.0;
  }
  g_timers[id].original = original;
  MakeStub(id);
  if (MH_EnableHook(reinterpret_cast<void*>(&CostProbe)) != MH_OK) {
    MH_RemoveHook(reinterpret_cast<void*>(&CostProbe));
    return 0.0;
  }
  int (*volatile probe)(int) = &CostProbe;
  int (*volatile bare_probe)(int) = reinterpret_cast<int (*)(int)>(original);
  const int64_t q_start = Qpc();
  const int64_t n_start = Now();
  int64_t bare = INT64_MAX;
  int64_t timed = INT64_MAX;
  for (int r = 0; r < kRounds; ++r) {
    int64_t t0 = Now();
    for (int i = 0; i < kCalls; ++i) bare_probe(i);
    int64_t t = Now() - t0;
    if (t < bare) bare = t;
    g_calibrate = true;
    t0 = Now();
    for (int i = 0; i < kCalls; ++i) probe(i);
    t = Now() - t0;
    g_calibrate = false;
    if (t < timed) timed = t;
  }
  const int64_t n_end = Now();
  const int64_t q_end = Qpc();
  MH_DisableHook(reinterpret_cast<void*>(&CostProbe));
  MH_RemoveHook(reinterpret_cast<void*>(&CostProbe));
  g_timers[id] = Timer();
  g_stack.NewFrame();
  // The rate of the clock of the timers over the measure.
  const double span_ms = QpcMs(q_start, q_end);
  if (n_end > n_start && span_ms > 0.0) {
    g_tick_mhz = static_cast<double>(n_end - n_start) / span_ms / 1000.0;
  }
  if (g_tick_mhz <= 0.0) return 0.0;
  const double ns = static_cast<double>(timed - bare) * 1000.0 / g_tick_mhz / kCalls;
  return ns > 0.0 ? ns : 0.0;
}
void Install() {
  g_installed = true;
  g_stubs = static_cast<uint8_t*>(VirtualAlloc(nullptr, wp::kMaxTimers * kStubBytes,
                                               MEM_COMMIT | MEM_RESERVE,
                                               PAGE_EXECUTE_READWRITE));
  if (g_stubs == nullptr || g_hz.QuadPart == 0) {
    g_failed = true;
    ModLog("walk parts   : no memory for the stubs, or no clock. No timer");
    return;
  }
  g_timers[0].shown = static_cast<uint32_t>(bw::game::kProcess3dEngine);
  _snprintf_s(g_timers[0].name, sizeof(g_timers[0].name), _TRUNCATE,
              "the scene walk, outside each timer");
  g_timers[kHookTimer].shown = 0x0082F460;
  _snprintf_s(g_timers[kHookTimer].name, sizeof(g_timers[kHookTimer].name), _TRUNCATE,
              "h_FinishFrame, the hook of the mod on LH3DRender::FinishFrame");
  for (const Fn& f : kGameFns) Add(f.address, f.name, f.group);
  // The calls of the hook of the mod on FinishFrame.
  struct ModFn {
    void* fn;
    const char* name;
  };
  const ModFn kHookFns[] = {
      {reinterpret_cast<void*>(&TabletopCountLists), "TabletopCountLists, in h_FinishFrame"},
      {reinterpret_cast<void*>(&UiBarsBegin), "UiBarsBegin, in h_FinishFrame"},
      {reinterpret_cast<void*>(&UiBarsEnd), "UiBarsEnd, in h_FinishFrame"},
      {reinterpret_cast<void*>(&UiPanelBeforeFinish), "UiPanelBeforeFinish, in h_FinishFrame"},
      {reinterpret_cast<void*>(&UiPanelAfterFinish), "UiPanelAfterFinish, in h_FinishFrame"},
      {reinterpret_cast<void*>(&ControllersScreenApply),
       "ControllersScreenApply, in h_FinishFrame"},
      {reinterpret_cast<void*>(&XrInputNewFrame), "XrInputNewFrame, in h_FinishFrame"},
      {reinterpret_cast<void*>(&AudioFrame), "AudioFrame, in h_FinishFrame"},
      {reinterpret_cast<void*>(&SetupBoxWatchFrame), "SetupBoxWatchFrame, in h_FinishFrame"},
      {reinterpret_cast<void*>(&GpuBridgeResume), "GpuBridgeResume, in h_FinishFrame"},
      {reinterpret_cast<void*>(&HangWatchFrame), "HangWatchFrame, in h_FinishFrame"},
      {reinterpret_cast<void*>(&StallWatchFrame), "StallWatchFrame, in h_FinishFrame"},
      {reinterpret_cast<void*>(&VkStreamScreen), "VkStreamScreen, in h_FinishFrame"},
  };
  for (const ModFn& m : kHookFns) {
    Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(m.fn)), m.name, kGame);
  }
  // The Vulkan renderer of the mod and the stream of the walks.
  Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&VkSceneDraw)),
      "VkSceneDraw, the Vulkan renderer of the mod", kEach);
  Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&VkSceneRenderState)),
      "VkSceneRenderState", kEach);
  Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&VkSceneStageState)),
      "VkSceneStageState", kEach);
  Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&VkSceneTexture)), "VkSceneTexture",
      kEach);
  Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&VkSceneSurfaceWritten)),
      "VkSceneSurfaceWritten", kEach);
  Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&VkStreamDraw)),
      "VkStreamDraw, the record of the stream", kEach);
  Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&VkStreamRenderState)),
      "VkStreamRenderState", kEach);
  Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&VkStreamStageState)),
      "VkStreamStageState", kEach);
  Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&VkStreamTexture)), "VkStreamTexture",
      kEach);
  Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&VkStreamWritten)), "VkStreamWritten",
      kEach);
  for (int i = 0; i < 4; ++i) {
    const char* name = nullptr;
    void* fn = FastTransformLoop(i, &name);
    Add(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(fn)), name, kGame);
  }
  // The device of the game and its render target.
  void** slot = reinterpret_cast<void**>(bw::render::kDirect3DDevice7);
  IDirect3DDevice7* device = nullptr;
  if (!IsBadReadPtr(slot, sizeof(void*))) device = static_cast<IDirect3DDevice7*>(*slot);
  if (device != nullptr) {
    AddTable(device, kDeviceSlots, static_cast<int>(sizeof(kDeviceSlots) / sizeof(Slot)));
    IDirectDrawSurface7* target = nullptr;
    if (SUCCEEDED(device->GetRenderTarget(&target)) && target != nullptr) {
      AddTable(target, kSurfaceSlots, static_cast<int>(sizeof(kSurfaceSlots) / sizeof(Slot)));
      target->Release();
    }
  } else {
    ModLog("walk parts   : no device of the game. No timer on the device");
  }
  // walk_parts_add.
  char list[sizeof(g_add)];
  memcpy(list, g_add, sizeof(list));
  char* next = nullptr;
  for (char* tok = strtok_s(list, ", ", &next); tok != nullptr;
       tok = strtok_s(nullptr, ", ", &next)) {
    const uint32_t a = static_cast<uint32_t>(strtoul(tok, nullptr, 16));
    char name[40];
    _snprintf_s(name, sizeof(name), _TRUNCATE, "fn_%08X (walk_parts_add)",
                static_cast<unsigned>(a));
    Add(a, name, kGame);
  }
  const int listed = ScanLists();
  const int enabled = EnableFrom(2);
  g_pair_ns = MeasureCost();
  ModLog("walk parts   : walk_parts=%d. %d timers in the scene walks of the frame thread (%d "
         "of them from the lists of the engine), %d of them on. One timed call costs %.0f ns "
         "(the least of 5 rounds of 20000 calls of a probe with and without a timer). The "
         "clock of the timers runs at %.3f MHz. Each 10 s the lines of the parts of the walk",
         g_mode, g_count - 1, listed, enabled, g_pair_ns, g_tick_mhz);
}

void Report(int64_t now, int64_t qpc) {
  const double ms = QpcMs(g_window_qpc, qpc);
  const double seconds = ms / 1000.0;
  // The rate of the clock of the timers over the window. The rate of the
  // performance counter is its frequency.
  if (g_clock == 1 && now > g_window_start) {
    g_window.ms_per_tick = ms / static_cast<double>(now - g_window_start);
  }
  char line[512];
  wp::HeadLine(line, sizeof(line), g_window, g_count, seconds, g_pair_ns);
  ModLog("walk parts   : %s", line);
  int ids[wp::kMaxTimers];
  const int n = wp::Order(g_window, g_count, ids);
  for (int r = 0; r < n; ++r) {
    const Timer& t = g_timers[ids[r]];
    wp::TimerLine(line, sizeof(line), g_window, ids[r], r + 1, t.shown, t.name);
    ModLog("walk parts   : %s", line);
  }
  if (g_stack.lost > 0) {
    ModLog("walk parts   : %llu timed calls had no return since the start",
           static_cast<unsigned long long>(g_stack.lost));
  }
  g_window.Reset();
  const int first = g_count;
  const int more = ScanLists();
  if (more > 0) {
    const int on = EnableFrom(first);
    ModLog("walk parts   : %d more timers from the lists of the engine, %d of them on", more, on);
  }
  g_window_start = Now();
  g_window_qpc = Qpc();
}

}  // namespace

void WalkPartsSetOptions(int mode, int clock, const char* add) {
  g_mode = (mode == 1 || mode == 2) ? mode : 0;
  g_clock = clock == 0 ? 0 : 1;
  g_add[0] = 0;
  if (add != nullptr) strncpy_s(g_add, sizeof(g_add), add, _TRUNCATE);
  QueryPerformanceFrequency(&g_hz);
  g_window.ms_per_tick = g_hz.QuadPart > 0 ? 1000.0 / static_cast<double>(g_hz.QuadPart) : 0.0;
  if (g_mode != 0) {
    ModLog("walk parts   : walk_parts=%d, walk_parts_clock=%d (%s), walk_parts_add=%s. The "
           "hooks come at the third scene walk",
           g_mode, g_clock,
           g_clock == 1 ? "the time stamp counter of the CPU" : "QueryPerformanceCounter",
           g_add[0] != 0 ? g_add : "(none)");
  }
}

bool WalkPartsOn() { return g_mode != 0 && g_installed && !g_failed; }

void WalkPartsWalk(bool begin) {
  if (g_mode == 0) return;
  if (begin) {
    if (!g_installed) {
      if (++g_walks < 3) return;
      g_thread = ThisThreadId();
      Install();
      if (g_tick_mhz > 0.0) g_window.ms_per_tick = 1.0 / (g_tick_mhz * 1000.0);
      g_window_start = Now();
      g_window_qpc = Qpc();
    }
    if (g_failed || ThisThreadId() != g_thread) return;
    const int64_t now = Now();
    if (g_presents > 0 && g_frame_has_walk) {
      g_window.AddFrame(g_stack, g_count);
      g_stack.NewFrame();
      g_presents = 0;
      g_frame_has_walk = false;
      const int64_t qpc = Qpc();
      if (QpcMs(g_window_qpc, qpc) >= time_log_math::kWindowMs && g_window.frames > 0) {
        Report(now, qpc);
      }
    }
    g_stack.Push(0, Now(), 0, 0);
    g_in_walk = true;
    return;
  }
  if (!g_in_walk || ThisThreadId() != g_thread) return;
  const int64_t now = Now();
  while (g_stack.depth > 1) g_stack.DropTop();
  if (g_stack.depth == 1) g_stack.CloseTop(now);
  g_in_walk = false;
  g_frame_has_walk = true;
}

void WalkPartsHookBegin() {
  g_hook_open = -1;
  if (!g_in_walk || ThisThreadId() != g_thread) return;
  if (g_stack.Push(kHookTimer, Now(), 0, 0)) g_hook_open = g_stack.depth - 1;
}

void WalkPartsHookEnd() {
  if (g_hook_open < 0) return;
  if (g_in_walk && g_stack.depth > g_hook_open &&
      g_stack.open[g_hook_open].id == kHookTimer) {
    const int64_t now = Now();
    while (g_stack.depth - 1 > g_hook_open) g_stack.DropTop();
    g_stack.CloseTop(now);
  }
  g_hook_open = -1;
}

void WalkPartsPresent() {
  if (g_installed && !g_failed) ++g_presents;
}
