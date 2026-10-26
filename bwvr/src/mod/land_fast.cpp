// The vertex loop of the land with SSE (docs/backend.md, "The vertex loop of
// the land with SSE"). The math is in land_fast_math.h.
//
// WHERE. fn_00874AA0 draws one patch of the land (from fn_007FF4F0 and from
// LH3DIsland::Draw, fn_007FF610). It makes the vertices of the patch in a
// loop on the x87 unit, then builds the triangles and draws them. A jump at
// the start of the loop goes to a stub here. The stub keeps each register,
// and calls the SSE loop. When the SSE loop did the work, the stub goes on at
// the code after the loop. Else it goes back into the loop of the game,
// through the trampoline of MinHook.
//
//   fn_00874AA0  0x00874B40  mov edx,[ebp-8]; mov eax,[edx+90Ch]
//                                                     after: 0x00875308
//
// At the start of the loop the x87 stack is empty, and the locals of the
// loop hold the patch ([ebp-8]), the step of the grid ([ebp-20h]), the cells
// to skip at the end of a row ([ebp-44h]), the step of the coordinates
// ([ebp-28h]), the step of the texture ([ebp-14h]) and the first u and v
// ([ebp-3Ch], [ebp-10h]). The code after the loop reads only [ebp-8] and
// [ebp-20h], sets each other register and local that it uses, and starts
// with an empty x87 stack. The stub also gives esi, ebx and edi the values
// that the loop of the game leaves: the ends of Table1, of Table0 and of the
// cells.
//
// THE PATHS OF THE GAME. At the start of the renderer, fn_007AC9E0 sets the
// flag 0x00EA9EB4 from the class of the CPU (fn_008A2610, CPUID 1): 1 only
// for the family 15 with SSE2. With the flag the engine draws the land, the
// clip of a triangle and the sprites with code of its own with SSE
// (fn_007A1800 for a patch), and fn_00874AA0 does not come to its loop. So
// the loop of the x87 unit runs on a CPU of the family 6 (an Intel CPU, and
// the Quest under FEX), and the SSE path on an AMD CPU of the family 15 and
// up. game_sse_path (a test key) sets the flag after fn_007AC9E0: 0 gives
// the paths of the x87 unit on each PC, so a desk run can show the loop
// here, and 1 gives the SSE paths of the game.
//
// THE SAME BITS. As fast_transform.cpp: the SSE loop runs only while the x87
// unit has the precision of 24 bits and rounds to the nearest
// (fast_transform_math::Exact), and it sets MXCSR to round to the nearest
// with no flush to zero while it runs.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <emmintrin.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"
#include "fast_transform_math.h"
#include "land_fast.h"
#include "land_fast_math.h"

void ModLog(const char* format, ...);

namespace {

const uintptr_t kLoop = 0x00874B40;
const uintptr_t kAfter = 0x00875308;
const uint8_t kLoopBytes[9] = {0x8B, 0x55, 0xF8, 0x8B, 0x82, 0x0C, 0x09, 0x00, 0x00};
const uint8_t kAfterBytes[5] = {0xB8, 0x10, 0x00, 0x00, 0x00};

const uintptr_t kSetPath = 0x007AC9E0;
const uintptr_t kPathFlag = 0x00EA9EB4;
const uint8_t kSetPathBytes[18] = {0x56, 0xE8, 0x1A, 0x00, 0x00, 0x00, 0x0F, 0xB6, 0x05,
                                   0x30, 0x3A, 0xE8, 0x00, 0xA3, 0xB4, 0x9E, 0xEA, 0x00};

const uintptr_t kTable0 = 0x00E3B5E0;  // LH3DP3::Table0, the clip flags
const uintptr_t kTable1 = 0x00E437E0;  // LH3DP3::Table1, the vertices
const uint32_t kMaxVertices = 17 * 17;

int g_mode = 1;  // land_fast
int g_sse_path = -1;  // game_sse_path
bool g_in = false;
bool g_path_in = false;
typedef int(__cdecl* SetPathFn)();
SetPathFn g_set_path = nullptr;

// The targets of the stub, and the registers that the loop of the game
// leaves at its end.
void* g_back = nullptr;
uintptr_t g_after = kAfter;
uint32_t g_esi_end = 0;
uint32_t g_ebx_end = 0;
uint32_t g_edi_end = 0;

// For the report, on the frame thread.
uint64_t g_report_at = 0;
uint32_t g_patches = 0;
uint32_t g_vertices = 0;
uint32_t g_game_loops = 0;
uint32_t g_inexact = 0;
uint16_t g_last_control = 0;
uint32_t g_mxcsr_sets = 0;
uint32_t g_game_mxcsr = 0;

// THE COMPARE (land_fast=2). The SSE loop makes a copy, and the loop of the
// game runs. A hook at the start of the code after the loop compares the
// copy with what the game wrote, before other code writes the tables again.
uint8_t g_copy[kMaxVertices * 32];
uint32_t g_copy_flags[kMaxVertices];
uint32_t g_pending = 0;       // the vertices of the pending copy
bool g_pending_clip = false;
uint32_t g_cmp_patches = 0;
uint32_t g_cmp_vertices = 0;
uint32_t g_cmp_differ = 0;    // vertices with a bit that differs
uint32_t g_cmp_words[3] = {0, 0, 0};  // diffuse, specular, clip flags
double g_cmp_most[6] = {0, 0, 0, 0, 0, 0};  // x, y, z, rhw, u, v
uint32_t g_cmp_nan = 0;       // floats that differ with a NaN in one
int g_cmp_lines = 0;
void* g_after_original = nullptr;

template <typename T>
T Read(uintptr_t address) {
  T v;
  memcpy(&v, reinterpret_cast<const void*>(address), sizeof(T));
  return v;
}

// The state of the units for the SSE loop, as in fast_transform.cpp.
struct Units {
  uint32_t game_mxcsr;
  uint32_t loop_mxcsr;

  bool Begin() {
    uint16_t control = 0;
    __asm fnstcw control
    game_mxcsr = _mm_getcsr();
    loop_mxcsr = game_mxcsr & ~0xE040u;
    if (!fast_transform_math::Exact(control, loop_mxcsr)) {
      ++g_inexact;
      g_last_control = control;
      return false;
    }
    if (loop_mxcsr != game_mxcsr) {
      _mm_setcsr(loop_mxcsr);
      ++g_mxcsr_sets;
      g_game_mxcsr = game_mxcsr;
    }
    return true;
  }

  void End() const {
    if (loop_mxcsr != game_mxcsr) _mm_setcsr(game_mxcsr);
  }
};

void Fill(land_fast_math::Consts* c) {
  memcpy(c->m, reinterpret_cast<const void*>(0x00EA9E40), sizeof(c->m));
  c->one = Read<float>(0x008AA390);
  c->zero = Read<float>(0x008AA398);
  c->height_scale = Read<float>(0x00C3720C);
  c->near_plane = Read<float>(0x00E839E0);
  c->half_x = Read<float>(0x00E839F0);
  c->half_y = Read<float>(0x00E839F4);
  c->max_x = Read<float>(0x00C2AB00);
  c->max_y = Read<float>(0x00C2AB04);
  c->fog_near = Read<float>(0x00C37220);
  c->fog_far = Read<float>(0x00C37224);
  c->fog_range = Read<float>(0x00E9B6DC);
  c->fog_rgb[0] = Read<float>(0x00C37214);
  c->fog_rgb[1] = Read<float>(0x00C37218);
  c->fog_rgb[2] = Read<float>(0x00C3721C);
  c->fog_dim = Read<int32_t>(0x00C37228);
  c->fog_color = Read<uint32_t>(0x00E9B6D8);
  c->colors = reinterpret_cast<const uint32_t*>(0x00EDD90C);
}

void Report() {
  const uint64_t now = GetTickCount64();
  if (g_report_at == 0) g_report_at = now + 10000;
  if (now < g_report_at) return;
  g_report_at = now + 10000;
  if (g_mode == 2) {
    ModLog("land fast    : land_fast=2, the compare in 10 s: %u patches and %u "
           "vertices. Vertices with a bit that differs: %u. The largest "
           "difference of x %.6g, y %.6g, z %.6g, rhw %.6g, u %.6g, v %.6g. "
           "Words that differ: diffuse %u, specular %u, clip flags %u. Floats "
           "that differ with a NaN: %u",
           g_cmp_patches, g_cmp_vertices, g_cmp_differ, g_cmp_most[0],
           g_cmp_most[1], g_cmp_most[2], g_cmp_most[3], g_cmp_most[4],
           g_cmp_most[5], g_cmp_words[0], g_cmp_words[1], g_cmp_words[2],
           g_cmp_nan);
    g_cmp_patches = g_cmp_vertices = g_cmp_differ = g_cmp_nan = 0;
    memset(g_cmp_words, 0, sizeof(g_cmp_words));
    memset(g_cmp_most, 0, sizeof(g_cmp_most));
  } else {
    ModLog("land fast    : land_fast=%d, in 10 s: %u patches and %u vertices "
           "with SSE, %u patches with the loop of the game",
           g_mode, g_patches, g_vertices, g_game_loops);
  }
  if (g_inexact > 0) {
    ModLog("land fast    : %u patches went to the loop of the game, because "
           "the x87 control word was 0x%04X (the SSE loop needs a precision of "
           "24 bits and rounding to the nearest)",
           g_inexact, g_last_control);
  }
  if (g_mxcsr_sets > 0) {
    ModLog("land fast    : MXCSR of the game was 0x%08X. For %u SSE loops the "
           "mod set it to round to the nearest with no flush to zero, and put "
           "it back after each loop",
           g_game_mxcsr, g_mxcsr_sets);
  }
  g_patches = g_vertices = g_game_loops = g_inexact = g_mxcsr_sets = 0;
}

void CompareFloat(int field, uint32_t want, uint32_t got) {
  if (want == got) return;
  float a, b;
  memcpy(&a, &want, 4);
  memcpy(&b, &got, 4);
  if (a != a || b != b) {
    ++g_cmp_nan;
    return;
  }
  const double d = fabs(static_cast<double>(a) - static_cast<double>(b));
  if (d > g_cmp_most[field]) g_cmp_most[field] = d;
}

// Compares the pending copy with what the game wrote.
void ComparePending() {
  if (g_pending == 0) return;
  const uint32_t n = g_pending;
  g_pending = 0;
  ++g_cmp_patches;
  g_cmp_vertices += n;
  const uint8_t* table1 = reinterpret_cast<const uint8_t*>(kTable1);
  const uint32_t* table0 = reinterpret_cast<const uint32_t*>(kTable0);
  static const int kFloatField[8] = {0, 1, 2, 3, -1, -1, 4, 5};
  for (uint32_t i = 0; i < n; ++i) {
    uint32_t a[8], b[8];
    memcpy(a, table1 + i * 32, 32);
    memcpy(b, g_copy + i * 32, 32);
    const bool flags_differ = g_pending_clip && table0[i] != g_copy_flags[i];
    if (memcmp(a, b, 32) == 0 && !flags_differ) continue;
    ++g_cmp_differ;
    for (int w = 0; w < 8; ++w) {
      if (kFloatField[w] >= 0) CompareFloat(kFloatField[w], a[w], b[w]);
    }
    if (a[4] != b[4]) ++g_cmp_words[0];
    if (a[5] != b[5]) ++g_cmp_words[1];
    if (flags_differ) ++g_cmp_words[2];
    if (g_cmp_lines < 12) {
      ++g_cmp_lines;
      ModLog("land fast    : compare, vertex %u of %u: the game %08X %08X %08X "
             "%08X %08X %08X %08X %08X flags %X, SSE %08X %08X %08X %08X %08X "
             "%08X %08X %08X flags %X",
             i, n, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7],
             g_pending_clip ? table0[i] : 0, b[0], b[1], b[2], b[3], b[4],
             b[5], b[6], b[7], g_pending_clip ? g_copy_flags[i] : 0);
    }
  }
}

extern "C" void __cdecl LandFastCompare() { ComparePending(); }

// The hook of the compare at the code after the loop: pushad; call
// LandFastCompare; popad; jmp dword ptr [original].
bool InstallCompare() {
  uint8_t* at = static_cast<uint8_t*>(
      VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
  if (at == nullptr) return false;
  at[0] = 0x60;
  at[1] = 0xE8;
  const int32_t rel = static_cast<int32_t>(
      reinterpret_cast<intptr_t>(&LandFastCompare) - reinterpret_cast<intptr_t>(at + 6));
  memcpy(at + 2, &rel, 4);
  at[6] = 0x61;
  at[7] = 0xFF;
  at[8] = 0x25;
  const uint32_t o =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_after_original));
  memcpy(at + 9, &o, 4);
  FlushInstructionCache(GetCurrentProcess(), at, 16);
  void* target = reinterpret_cast<void*>(kAfter);
  return MH_CreateHook(target, at, &g_after_original) == MH_OK &&
         MH_QueueEnableHook(target) == MH_OK;
}

// The loop of fn_00874AA0. `frame` is ebp of fn_00874AA0. Gives 1 when it
// did the work, and then g_esi_end, g_ebx_end and g_edi_end are the
// registers that the loop of the game leaves.
extern "C" int __cdecl LandFastLoop(uint8_t* frame) {
  Report();
  const uint8_t* patch = *reinterpret_cast<uint8_t**>(frame - 8);
  land_fast_math::Patch p;
  p.cells = patch;
  p.step = *reinterpret_cast<const int32_t*>(frame - 0x20);
  p.row_skip = *reinterpret_cast<const int32_t*>(frame - 0x44);
  p.d = *reinterpret_cast<const float*>(frame - 0x28);
  p.du = *reinterpret_cast<const float*>(frame - 0x14);
  p.u0 = *reinterpret_cast<const float*>(frame - 0x3C);
  p.v0 = *reinterpret_cast<const float*>(frame - 0x10);
  memcpy(&p.r0, patch + 0x90C, 4);
  memcpy(&p.c0, patch + 0x910, 4);
  memcpy(&p.clip, patch + 0x91C, 4);
  memcpy(&p.clear_low, patch + 0x928, 4);
  memcpy(&p.light, patch + 0x940, 4);
  land_fast_math::Consts c;
  Fill(&c);
  const bool fog_ok = p.light == 0 || p.light == 2 || land_fast_math::FogExact(c.fog_dim);
  Units units;
  if (g_mode == 0 || p.step <= 0 || !fog_ok || !units.Begin()) {
    ++g_game_loops;
    return 0;
  }
  const uint32_t side = static_cast<uint32_t>(land_fast_math::GridSide(p.step));
  const uint32_t count = side * side;
  if (count == 0 || count > kMaxVertices) {
    units.End();
    ++g_game_loops;
    return 0;
  }
  uint8_t* table1 = reinterpret_cast<uint8_t*>(kTable1);
  uint32_t* table0 = reinterpret_cast<uint32_t*>(kTable0);
  if (g_mode == 2) {
    // A copy that the hook after the loop did not compare is old.
    g_pending = 0;
    // The bytes that the loop does not write are as in the table now.
    memcpy(g_copy, table1, count * 32);
    g_pending = land_fast_math::Grid(c, p, g_copy, g_copy_flags, nullptr);
    g_pending_clip = p.clip != 0;
    units.End();
    ++g_game_loops;
    return 0;
  }
  const uint8_t* cells_end = nullptr;
  const uint32_t n = land_fast_math::Grid(c, p, table1, table0, &cells_end);
  units.End();
  g_esi_end = static_cast<uint32_t>(kTable1 + n * 32);
  g_ebx_end = static_cast<uint32_t>(kTable0 + n * 4);
  g_edi_end = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cells_end));
  ++g_patches;
  g_vertices += n;
  return 1;
}

// The stub. popad does not change the flags, so the test of the result
// decides the jump after it.
__declspec(naked) void LandStub() {
  __asm {
    pushad
    push ebp
    call LandFastLoop
    add esp, 4
    test eax, eax
    popad
    jz land_game
    mov esi, dword ptr [g_esi_end]
    mov ebx, dword ptr [g_ebx_end]
    mov edi, dword ptr [g_edi_end]
    jmp dword ptr [g_after]
  land_game:
    jmp dword ptr [g_back]
  }
}

bool Expect(uintptr_t address, const uint8_t* bytes, size_t size) {
  const void* site = reinterpret_cast<const void*>(address);
  return !IsBadReadPtr(site, size) && memcmp(site, bytes, size) == 0;
}

// fn_007AC9E0, then the flag of game_sse_path. Gives the flag in eax, as the
// function of the game does.
int __cdecl SetPathHook() {
  g_set_path();
  int32_t* flag = reinterpret_cast<int32_t*>(kPathFlag);
  if (g_sse_path >= 0) *flag = g_sse_path;
  ModLog("land fast    : the class of the CPU gives the flag of the SSE paths "
         "0x%08X = %d%s",
         static_cast<unsigned>(kPathFlag), *flag,
         g_sse_path >= 0 ? " (game_sse_path)" : "");
  return *flag;
}

void InstallPath() {
  if (g_sse_path < 0 || g_path_in) return;
  if (!Expect(kSetPath, kSetPathBytes, sizeof(kSetPathBytes))) {
    ModLog("land fast    : the code at 0x%08X is not the code that sets the flag "
           "of the SSE paths, so game_sse_path does nothing",
           static_cast<unsigned>(kSetPath));
    return;
  }
  void* at = reinterpret_cast<void*>(kSetPath);
  MH_STATUS status = MH_CreateHook(at, reinterpret_cast<void*>(&SetPathHook),
                                   reinterpret_cast<void**>(&g_set_path));
  if (status == MH_OK) status = MH_EnableHook(at);
  g_path_in = status == MH_OK;
  // The renderer can be there already: then the flag is set now.
  int32_t* flag = reinterpret_cast<int32_t*>(kPathFlag);
  const int32_t before = *flag;
  *flag = g_sse_path;
  ModLog("hooks        : the flag of the SSE paths at 0x%08X %s. game_sse_path=%d, "
         "the flag was %d",
         static_cast<unsigned>(kSetPath), g_path_in ? "ok" : "FAILED", g_sse_path,
         before);
}

}  // namespace

void LandFastSetOptions(int mode, int sse_path) {
  g_mode = mode < 0 || mode > 2 ? 1 : mode;
  g_sse_path = sse_path < -1 || sse_path > 1 ? -1 : sse_path;
  static const char* const kText[3] = {
      "The vertex loop of the land runs on the x87 unit",
      "The vertex loop of the land runs with SSE, four vertices at a time, "
      "with the bits of the game",
      "A test aid: the vertex loop of the land runs on the x87 unit, and the "
      "mod compares a copy that the SSE loop makes with it"};
  ModLog("land fast    : land_fast=%d. %s", g_mode, kText[g_mode]);
}

void LandFastInstall() {
  InstallPath();
  if (g_in || g_mode == 0) return;
  if (!Expect(kLoop, kLoopBytes, sizeof(kLoopBytes)) ||
      !Expect(kAfter, kAfterBytes, sizeof(kAfterBytes))) {
    ModLog("land fast    : the code at 0x%08X or 0x%08X is not the code of the "
           "loop of the land, so the loop of the game runs",
           static_cast<unsigned>(kLoop), static_cast<unsigned>(kAfter));
    return;
  }
  void* loop = reinterpret_cast<void*>(kLoop);
  MH_STATUS status = MH_CreateHook(loop, reinterpret_cast<void*>(&LandStub), &g_back);
  if (status == MH_OK) status = MH_QueueEnableHook(loop);
  if (status == MH_OK && g_mode == 2 && !InstallCompare()) status = MH_UNKNOWN;
  if (status == MH_OK) status = MH_ApplyQueued();
  if (status != MH_OK) {
    MH_RemoveHook(loop);
    MH_RemoveHook(reinterpret_cast<void*>(kAfter));
  }
  g_in = status == MH_OK;
  ModLog("hooks        : the vertex loop of the land at 0x%08X %s. The SSE loop "
         "is LandFastLoop at 0x%08X",
         static_cast<unsigned>(kLoop), g_in ? "ok" : "FAILED",
         static_cast<unsigned>(reinterpret_cast<uintptr_t>(&LandFastLoop)));
}
