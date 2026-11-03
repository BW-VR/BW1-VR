// The draw of the meshes of a 3D part with SSE (docs/backend.md, "The parts
// with SSE"). The math is in part_draw_math.h.
//
// WHERE. After the vertex loops of fast_transform.cpp a mesh still runs
// two pieces of x87 code. Under FEX x87 code is slow, and these pieces run
// for each vertex or each triangle of each mesh of each part:
//
//   The triangles  The loop after the vertices of fn_00856D40, fn_0085AB10,
//                  fn_0084BA90 and fn_00859530: the test of the back face of
//                  each triangle and the list of the triangles.
//   The offsets    The vertex loops of fn_00859530, the draw of the mesh of a
//                  part with an offset for each vertex (a person or an
//                  animal). Its two loops differ in the offsets: three floats
//                  for each vertex, or one float for y.
//
// A jump at the start of each piece goes to a stub here. The stub keeps each
// register and calls the SSE code. When that did the work, the stub goes on
// at the code after the piece. Else it goes back into the code of the game,
// through the trampoline of MinHook.
//
//   offsets 1  0x00859582  mov eax,[0C37D8C]                 after: 0x00859AC4
//   offsets 2  0x00859831  mov eax,[0C37D8C]                 after: 0x00859AC4
//   triangles  0x0085715F  fn_00856D40  mov [0EA9EB8],edx     after: 0x00857340
//              0x0085B078  fn_0085AB10  mov [0EA9EB8],eax     after: 0x0085B195
//              0x0084C10B  fn_0084BA90  mov [0EA9EB8],ecx     after: 0x0084C25E
//              0x00859AE7  fn_00859530  mov [0EA9EB8],edx     after: 0x00859C43
//
// At the hook of a triangle loop the code of the game has set the list size
// (g_TriangleListSize) to 0 and g_NumClipVerts. The stub stores g_NoBackfaceCull
// (the instruction that it replaces), builds the list, and goes on at the
// code that tests the size, with the size in ecx (fn_00856D40, fn_0085AB10)
// or in eax (fn_0084BA90, fn_00859530), and the mesh in esi (fn_00856D40,
// fn_00859530). A triangle that the game must clip goes to the clip function
// of the game (fn_0081A760), as in the loop of the game.
//
// THE SAME BITS. The SSE code runs only while the x87 unit has the
// precision of 24 bits and rounds to the nearest, and MXCSR rounds to the
// nearest. The triangles only subtract, multiply and compare screen values,
// so the flush to zero of MXCSR changes nothing in them, and the mod leaves
// MXCSR as it is. The offsets loop divides and projects, so it sets MXCSR as
// fast_transform.cpp does while it runs. The
// light of the offsets loop needs the SSE2 path of __ftol of the game
// (0x00E83A20 is not 0).
//
// THE COMPARE (part_draw=2). The code of the game runs. Before it, the stub
// makes the result with SSE into a copy of its own, and a hook after the
// piece compares the copy with what the game made.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <emmintrin.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"
#include "debug_menu.h"
#include "part_draw.h"
#include "part_draw_math.h"

void ModLog(const char* format, ...);
// controllers.cpp
void ControllersGesturePulse(float amplitude, float seconds);

namespace {

namespace pd = part_draw_math;

// The hooks.
struct Site {
  uintptr_t address;
  uint8_t bytes[10];
  int length;  // the bytes to check
};
const Site kOffset1Site = {0x00859582, {0xA1, 0x8C, 0x7D, 0xC3, 0x00}, 5};
const Site kOffset2Site = {0x00859831, {0xA1, 0x8C, 0x7D, 0xC3, 0x00}, 5};
const Site kOffsetAfterSite = {0x00859AC4, {0x66, 0x8B, 0x4E, 0x10, 0x33, 0xC0}, 6};
const Site kTriSites[4] = {
    {0x0085715F, {0x89, 0x15, 0xB8, 0x9E, 0xEA, 0x00}, 6},
    {0x0085B078, {0xA3, 0xB8, 0x9E, 0xEA, 0x00}, 5},
    {0x0084C10B, {0x89, 0x0D, 0xB8, 0x9E, 0xEA, 0x00}, 6},
    {0x00859AE7, {0x89, 0x15, 0xB8, 0x9E, 0xEA, 0x00}, 6},
};
// Where the code of the game tests the size of the list: the hooks of the
// compare.
const Site kTriCompareSites[4] = {
    {0x00857348, {0xA1, 0x14, 0x87, 0xC3, 0x00}, 5},
    {0x0085B19D, {0xA1, 0x14, 0x87, 0xC3, 0x00}, 5},
    {0x0084C266, {0x8B, 0x0D, 0x14, 0x87, 0xC3, 0x00}, 6},
    {0x00859C4B, {0x8B, 0x0D, 0x14, 0x87, 0xC3, 0x00}, 6},
};
const uintptr_t kTriAfter[4] = {0x00857340, 0x0085B195, 0x0084C25E, 0x00859C43};
// The register with the size in the compare hook: 0 eax, 1 ecx.
const int kTriCompareRegister[4] = {1, 1, 0, 0};

const uintptr_t kTable0 = 0x00E3B5E0;      // LH3DP3::Table0, the clip flags
const uintptr_t kTable1 = 0x00E437E0;      // LH3DP3::Table1, the vertices
const uintptr_t kList = 0x00EA5DE0;        // g_TriangleList
const uintptr_t kListSize = 0x00EA9EA4;    // g_TriangleListSize
const uintptr_t kNoCull = 0x00EA9EB8;      // g_NoBackfaceCull
const uintptr_t kOffsetPointer = 0x00F05180;
const uintptr_t kHasSse2 = 0x00E83A20;     // LH3DP3::HasSSE2, for __ftol
const uintptr_t kClipTriangle = 0x0081A760;
const int32_t kMaxVertices = (kTable1 - kTable0) / 4;

const uintptr_t kMatrix = 0x00EA9EA0;
const uintptr_t kNearPlane = 0x00E839E0;
const uintptr_t kHalfX = 0x00E839F0;
const uintptr_t kHalfY = 0x00E839F4;
const uintptr_t kMaxScreen = 0x00C2AB00;
const uintptr_t kColor = 0x00C37D8C;
const uintptr_t kSpecular = 0x00E9FE2C;
const uintptr_t kLightDirection = 0x00F03140;
const uintptr_t kAmbient = 0x00C39264;

int g_mode = 0;           // part_draw
bool g_test_keys = false;
bool g_in = false;        // the hooks of the SSE code are on
bool g_compare_in = false;  // the hooks of the compare are on
bool g_key_down = false;
uint64_t g_report_at = 0;

// For the report, on the frame thread.
struct Counts {
  uint32_t tri_calls[3] = {0, 0, 0};  // with flags and the cull test, with flags and no cull test, with no flags
  uint32_t tri_triangles = 0, tri_clips = 0, tri_game = 0;
  uint32_t off_calls[3] = {0, 0, 0};  // [1] three offsets, [2] one offset
  uint32_t off_vertices = 0, off_game = 0;
  uint32_t inexact = 0;
  uint32_t mxcsr_sets = 0;
  uint16_t last_control = 0;
  uint32_t game_mxcsr = 0;
};
Counts g_counts;

// The compare, for the report.
struct Compare {
  uint32_t tri_meshes = 0, tri_clipped = 0, tri_differ = 0, tri_missing = 0;
  uint32_t off_meshes = 0, off_vertices = 0, off_differ = 0, off_flags = 0;
  double off_most[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  uint32_t off_words[2] = {0, 0};  // color, specular
  uint32_t nan = 0;
  int lines = 0;
};
Compare g_cmp;

template <typename T>
T Read(uintptr_t address) {
  T v;
  memcpy(&v, reinterpret_cast<const void*>(address), sizeof(T));
  return v;
}

bool Expect(const Site& s) {
  const void* site = reinterpret_cast<const void*>(s.address);
  return !IsBadReadPtr(site, static_cast<UINT_PTR>(s.length)) &&
         memcmp(site, s.bytes, static_cast<size_t>(s.length)) == 0;
}

// The state of the units for SSE code. `full`: MXCSR also gets no flush to
// zero and no denormals as zero while the code runs (End puts it back).
struct Units {
  uint32_t game_mxcsr = 0;
  uint32_t loop_mxcsr = 0;

  bool Begin(bool full) {
    uint16_t control = 0;
    __asm fnstcw control
    game_mxcsr = _mm_getcsr();
    loop_mxcsr = full ? (game_mxcsr & ~0xE040u) : game_mxcsr;
    const uint32_t seen = full ? loop_mxcsr : (game_mxcsr & ~0x8040u);
    if (!fast_transform_math::Exact(control, seen)) {
      ++g_counts.inexact;
      g_counts.last_control = control;
      return false;
    }
    if (loop_mxcsr != game_mxcsr) {
      _mm_setcsr(loop_mxcsr);
      ++g_counts.mxcsr_sets;
      g_counts.game_mxcsr = game_mxcsr;
    }
    return true;
  }

  void End() const {
    if (loop_mxcsr != game_mxcsr) _mm_setcsr(game_mxcsr);
  }
};

bool Fill(fast_transform_math::Consts* c) {
  const float* m = Read<const float*>(kMatrix);
  if (m == nullptr) return false;
  memcpy(c->m, m, sizeof(c->m));
  c->near_plane = Read<float>(kNearPlane);
  c->half_x = Read<float>(kHalfX);
  c->half_y = Read<float>(kHalfY);
  c->max_x = Read<float>(kMaxScreen);
  c->max_y = Read<float>(kMaxScreen + 4);
  c->color = Read<uint32_t>(kColor);
  c->specular = Read<uint32_t>(kSpecular);
  return true;
}

void CompareFloat(double* most, uint32_t want, uint32_t got) {
  if (want == got) return;
  float a, b;
  memcpy(&a, &want, 4);
  memcpy(&b, &got, 4);
  if (a != a || b != b) {
    ++g_cmp.nan;
    return;
  }
  const double d = a > b ? static_cast<double>(a) - b : static_cast<double>(b) - a;
  if (d > *most) *most = d;
}

// ---------------------------------------------------------------------------
// The triangles

struct DryList {
  bool pending = false;
  bool clipped = false;
  uint32_t size = 0;
  uint16_t list[pd::kListCapacity + 8];
};
DryList g_dry;

void* g_tri_back0 = nullptr;
void* g_tri_back1 = nullptr;
void* g_tri_back2 = nullptr;
void* g_tri_back3 = nullptr;
void** const g_tri_back[4] = {&g_tri_back0, &g_tri_back1, &g_tri_back2, &g_tri_back3};
void* g_tri_compare_original[4] = {nullptr, nullptr, nullptr, nullptr};

typedef void(__fastcall* ClipFunction)(uint32_t i0, uint32_t i1, uint32_t i2,
                                       uint32_t flag);

// The loop after the vertices. `kind` 1: the mesh has clip flags, 0: it has
// none. Gives the size of the list, or -1 when the code of the game must run.
int __cdecl PartDrawTriangles(const uint8_t* mesh, int kind) {
  if (g_dry.pending && g_dry.size != 0 && !g_dry.clipped) {
    // The code of the game made no list for the last mesh, and the SSE code did.
    ++g_cmp.tri_missing;
  }
  g_dry.pending = false;
  if (g_mode == 0) {
    ++g_counts.tri_game;
    return -1;
  }
  const int32_t triangles = Read<int32_t>(reinterpret_cast<uintptr_t>(mesh) + 0x18);
  const uint16_t* indices =
      Read<const uint16_t*>(reinterpret_cast<uintptr_t>(mesh) + 0x1C);
  const bool no_cull = (mesh[5] & 1) != 0;
  if (triangles > 0 &&
      (indices == nullptr ||
       static_cast<uint32_t>(triangles) * 3 + 64 > pd::kListCapacity)) {
    ++g_counts.tri_game;
    return -1;
  }
  Units units;
  if (!units.Begin(false)) {
    ++g_counts.tri_game;
    return -1;
  }
  const uint32_t* flags = kind != 0 ? reinterpret_cast<const uint32_t*>(kTable0) : nullptr;
  const uint8_t* table1 = reinterpret_cast<const uint8_t*>(kTable1);
  if (g_mode == 2) {
    g_dry.clipped = false;
    g_dry.size = pd::BuildTriangles(
        indices, triangles, no_cull, flags, table1, g_dry.list, 0,
        [](uint32_t, uint32_t, uint32_t, uint32_t size) {
          g_dry.clipped = true;
          return size;
        });
    g_dry.pending = true;
    return -1;
  }
  *reinterpret_cast<volatile uint32_t*>(kNoCull) = no_cull ? 1u : 0u;
  uint16_t* list = reinterpret_cast<uint16_t*>(kList);
  uint32_t clips = 0;
  const uint32_t size = pd::BuildTriangles(
      indices, triangles, no_cull, flags, table1, list, 0,
      [&](uint32_t i0, uint32_t i1, uint32_t i2, uint32_t size_now) {
        *reinterpret_cast<volatile uint32_t*>(kListSize) = size_now;
        reinterpret_cast<ClipFunction>(kClipTriangle)(i0, i1, i2, 0x20);
        ++clips;
        return Read<uint32_t>(kListSize);
      });
  *reinterpret_cast<volatile uint32_t*>(kListSize) = size;
  ++g_counts.tri_calls[flags == nullptr ? 2 : (no_cull ? 1 : 0)];
  g_counts.tri_triangles += triangles > 0 ? static_cast<uint32_t>(triangles) : 0;
  g_counts.tri_clips += clips;
  return static_cast<int>(size);
}

// At the test of the size in the code of the game: compare the list.
void __cdecl PartDrawTrianglesCompare(uint32_t game_size) {
  if (!g_dry.pending) return;
  g_dry.pending = false;
  ++g_cmp.tri_meshes;
  if (g_dry.clipped) {
    ++g_cmp.tri_clipped;
    return;
  }
  const uint16_t* list = reinterpret_cast<const uint16_t*>(kList);
  if (game_size == g_dry.size &&
      memcmp(list, g_dry.list, static_cast<size_t>(game_size) * 2) == 0) {
    return;
  }
  ++g_cmp.tri_differ;
  if (g_cmp.lines < 12) {
    ++g_cmp.lines;
    ModLog("part draw    : compare of the triangles: the game has %u words, SSE has %u words",
           game_size, g_dry.size);
  }
}

// ---------------------------------------------------------------------------
// The offsets

float* g_off_copy_vertices = nullptr;  // kMaxVertices vertices, then the flags
uint8_t* g_off_copy = nullptr;
uint32_t* g_off_copy_flags = nullptr;
uint32_t g_off_count = 0;  // the pending compare

void* g_offset1_back = nullptr;
void* g_offset2_back = nullptr;
uintptr_t g_offset_after = 0x00859AC4;
void* g_offset_after_original = nullptr;

// The loops of fn_00859530. `mesh` is the object. Gives 1 when it did the work.
int __cdecl PartDrawOffset(const uint8_t* mesh, int kind) {
  g_off_count = 0;
  if (g_mode == 0) {
    ++g_counts.off_game;
    return 0;
  }
  const int32_t count = Read<int32_t>(reinterpret_cast<uintptr_t>(mesh) + 0x10);
  const uint8_t* in = Read<const uint8_t*>(reinterpret_cast<uintptr_t>(mesh) + 0x14);
  const float* offsets = Read<const float*>(kOffsetPointer);
  fast_transform_math::Consts c;
  Units units;
  if (count <= 0 || count > kMaxVertices || in == nullptr || offsets == nullptr ||
      Read<uint32_t>(kHasSse2) == 0 || !Fill(&c) || !units.Begin(true)) {
    ++g_counts.off_game;
    return 0;
  }
  fast_transform_math::Light l;
  memcpy(l.direction, reinterpret_cast<const void*>(kLightDirection), sizeof(l.direction));
  l.ambient = Read<int32_t>(kAmbient);
  l.color = Read<uint32_t>(kColor);
  const pd::OffsetKind offset_kind = kind == 1 ? pd::kXyz : pd::kY;
  const uint32_t n = static_cast<uint32_t>(count);
  if (g_mode == 2) {
    if (g_off_copy == nullptr) {
      g_off_copy = static_cast<uint8_t*>(VirtualAlloc(
          nullptr, kMaxVertices * 32 + kMaxVertices * 4, MEM_COMMIT | MEM_RESERVE,
          PAGE_READWRITE));
      if (g_off_copy == nullptr) {
        units.End();
        return 0;
      }
      g_off_copy_flags = reinterpret_cast<uint32_t*>(g_off_copy + kMaxVertices * 32);
    }
    // The bytes that the loop does not write are as in the table now.
    memcpy(g_off_copy, reinterpret_cast<const void*>(kTable1), n * 32);
    pd::TransformOffsets(c, l, offset_kind, in, offsets, n, g_off_copy, g_off_copy_flags);
    g_off_count = n;
    units.End();
    return 0;
  }
  pd::TransformOffsets(c, l, offset_kind, in, offsets, n,
                       reinterpret_cast<uint8_t*>(kTable1),
                       reinterpret_cast<uint32_t*>(kTable0));
  units.End();
  // The game moves the pointer past the offsets of the mesh.
  *reinterpret_cast<const float**>(kOffsetPointer) =
      offsets + (kind == 1 ? 3 : 1) * static_cast<size_t>(n);
  ++g_counts.off_calls[kind == 1 ? 1 : 2];
  g_counts.off_vertices += n;
  return 1;
}

void __cdecl PartDrawOffsetCompare() {
  if (g_off_count == 0) return;
  const uint32_t n = g_off_count;
  g_off_count = 0;
  ++g_cmp.off_meshes;
  g_cmp.off_vertices += n;
  const uint8_t* table1 = reinterpret_cast<const uint8_t*>(kTable1);
  const uint32_t* table0 = reinterpret_cast<const uint32_t*>(kTable0);
  // The float fields by word: x, y, z, rhw, -, -, u, v.
  static const int kField[8] = {0, 1, 2, 3, -1, -1, 4, 5};
  for (uint32_t i = 0; i < n; ++i) {
    uint32_t a[8], b[8];
    memcpy(a, table1 + static_cast<size_t>(i) * 32, 32);
    memcpy(b, g_off_copy + static_cast<size_t>(i) * 32, 32);
    const bool flags_differ = table0[i] != g_off_copy_flags[i];
    if (memcmp(a, b, 32) == 0 && !flags_differ) continue;
    ++g_cmp.off_differ;
    for (int w = 0; w < 8; ++w) {
      if (kField[w] >= 0) CompareFloat(&g_cmp.off_most[kField[w]], a[w], b[w]);
    }
    if (a[4] != b[4]) ++g_cmp.off_words[0];
    if (a[5] != b[5]) ++g_cmp.off_words[1];
    if (flags_differ) ++g_cmp.off_flags;
    if (g_cmp.lines < 12) {
      ++g_cmp.lines;
      ModLog("part draw    : compare of the offsets, vertex %u of %u: the game %08X %08X %08X "
             "%08X %08X %08X %08X %08X flags %X, SSE %08X %08X %08X %08X %08X %08X %08X %08X "
             "flags %X",
             i, n, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], table0[i], b[0], b[1], b[2],
             b[3], b[4], b[5], b[6], b[7], g_off_copy_flags[i]);
    }
  }
}

__declspec(naked) void Offset1Stub() {
  __asm {
    pushad
    push 1
    push esi
    call PartDrawOffset
    add esp, 8
    test eax, eax
    popad
    jz offset1_game
    jmp dword ptr [g_offset_after]
  offset1_game:
    jmp dword ptr [g_offset1_back]
  }
}

__declspec(naked) void Offset2Stub() {
  __asm {
    pushad
    push 2
    push esi
    call PartDrawOffset
    add esp, 8
    test eax, eax
    popad
    jz offset2_game
    jmp dword ptr [g_offset_after]
  offset2_game:
    jmp dword ptr [g_offset2_back]
  }
}

// The stubs of the triangle loops. After pushad: edi [esp], esi [esp+4], ebp
// [esp+8], ebx [esp+16], edx [esp+20], ecx [esp+24], eax [esp+28].
uintptr_t g_tri_after_0 = kTriAfter[0];
uintptr_t g_tri_after_1 = kTriAfter[1];
uintptr_t g_tri_after_2 = kTriAfter[2];
uintptr_t g_tri_after_3 = kTriAfter[3];

// fn_00856D40: the mesh is in ebp. After: ecx the size, esi the mesh.
__declspec(naked) void TriStub0() {
  __asm {
    pushad
    push 1
    push ebp
    call PartDrawTriangles
    add esp, 8
    cmp eax, -1
    je tri0_game
    mov dword ptr [esp + 24], eax
    mov edx, dword ptr [esp + 8]
    mov dword ptr [esp + 4], edx
    popad
    jmp dword ptr [g_tri_after_0]
  tri0_game:
    popad
    jmp dword ptr [g_tri_back0]
  }
}

// fn_0085AB10: the mesh is in ebx, and it has no clip flags. After: ecx the
// size.
__declspec(naked) void TriStub1() {
  __asm {
    pushad
    push 0
    push ebx
    call PartDrawTriangles
    add esp, 8
    cmp eax, -1
    je tri1_game
    mov dword ptr [esp + 24], eax
    popad
    jmp dword ptr [g_tri_after_1]
  tri1_game:
    popad
    jmp dword ptr [g_tri_back1]
  }
}

// fn_0084BA90: the mesh is in esi. After: eax the size.
__declspec(naked) void TriStub2() {
  __asm {
    pushad
    push 1
    push esi
    call PartDrawTriangles
    add esp, 8
    cmp eax, -1
    je tri2_game
    mov dword ptr [esp + 28], eax
    popad
    jmp dword ptr [g_tri_after_2]
  tri2_game:
    popad
    jmp dword ptr [g_tri_back2]
  }
}

// fn_00859530: the mesh is in esi. After: eax the size, esi the mesh.
__declspec(naked) void TriStub3() {
  __asm {
    pushad
    push 1
    push esi
    call PartDrawTriangles
    add esp, 8
    cmp eax, -1
    je tri3_game
    mov dword ptr [esp + 28], eax
    popad
    jmp dword ptr [g_tri_after_3]
  tri3_game:
    popad
    jmp dword ptr [g_tri_back3]
  }
}

// A stub for a hook of the compare: pushad; [push reg;] call fn; [add esp,4;]
// popad; jmp dword ptr [original]. `reg` < 0: no argument, 0 eax, 1 ecx.
uint8_t* MakeCompareStub(void* fn, int reg, void** original) {
  uint8_t* at = static_cast<uint8_t*>(
      VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
  if (at == nullptr) return nullptr;
  int i = 0;
  at[i++] = 0x60;  // pushad
  if (reg >= 0) at[i++] = static_cast<uint8_t>(0x50 + reg);  // push eax or ecx
  at[i++] = 0xE8;  // call fn
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<intptr_t>(fn) -
                                           reinterpret_cast<intptr_t>(at + i + 4));
  memcpy(at + i, &rel, 4);
  i += 4;
  if (reg >= 0) {
    at[i++] = 0x83;  // add esp, 4
    at[i++] = 0xC4;
    at[i++] = 0x04;
  }
  at[i++] = 0x61;  // popad
  at[i++] = 0xFF;  // jmp dword ptr [original]
  at[i++] = 0x25;
  const uint32_t o = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(original));
  memcpy(at + i, &o, 4);
  FlushInstructionCache(GetCurrentProcess(), at, 32);
  return at;
}

bool HookCompare(uintptr_t address, void* fn, int reg, void** original) {
  uint8_t* stub = MakeCompareStub(fn, reg, original);
  void* target = reinterpret_cast<void*>(address);
  return stub != nullptr && MH_CreateHook(target, stub, original) == MH_OK &&
         MH_QueueEnableHook(target) == MH_OK;
}

bool HookSite(const Site& site, void* stub, void** back) {
  void* target = reinterpret_cast<void*>(site.address);
  return MH_CreateHook(target, stub, back) == MH_OK && MH_QueueEnableHook(target) == MH_OK;
}

const char* ModeText(int mode) {
  static const char* const kText[3] = {
      "The triangles and the offsets of the meshes run on the x87 unit",
      "The triangles and the offsets of the meshes run with SSE, with the bits of the game",
      "A test aid: the code of the game runs, and the mod compares the SSE code with it"};
  return kText[mode < 0 || mode > 2 ? 0 : mode];
}

void Report() {
  const uint64_t now = GetTickCount64();
  if (g_report_at == 0) g_report_at = now + 10000;
  if (now < g_report_at) return;
  g_report_at = now + 10000;
  if (g_mode == 2) {
    ModLog("part draw    : part_draw=2, the compare in 10 s: the triangles %u meshes, %u with a "
           "clip (not compared), %u differ, %u with no list in the game. The offsets %u "
           "meshes and %u vertices, %u vertices differ (the largest difference of x %.6g, "
           "y %.6g, z %.6g, rhw %.6g, u %.6g, v %.6g), words that differ: color %u, "
           "specular %u, clip flags %u. Floats that differ with a NaN: %u",
           g_cmp.tri_meshes, g_cmp.tri_clipped, g_cmp.tri_differ, g_cmp.tri_missing, g_cmp.off_meshes,
           g_cmp.off_vertices, g_cmp.off_differ, g_cmp.off_most[0], g_cmp.off_most[1],
           g_cmp.off_most[2], g_cmp.off_most[3], g_cmp.off_most[4], g_cmp.off_most[5],
           g_cmp.off_words[0], g_cmp.off_words[1], g_cmp.off_flags, g_cmp.nan);
    const int lines = g_cmp.lines;
    g_cmp = Compare();
    g_cmp.lines = lines;
  }
  ModLog("part draw    : part_draw=%d, in 10 s with SSE: the triangles of %u meshes with no cull "
         "test, %u with it, %u with no clip flags, %u triangles, %u of them clipped by the "
         "game; the offsets %u meshes with three offsets, %u with one, %u vertices. With the "
         "code of the game: %u triangle loops, %u offset loops",
         g_mode, g_counts.tri_calls[1], g_counts.tri_calls[0], g_counts.tri_calls[2],
         g_counts.tri_triangles, g_counts.tri_clips, g_counts.off_calls[1],
         g_counts.off_calls[2], g_counts.off_vertices, g_counts.tri_game, g_counts.off_game);
  if (g_counts.inexact > 0) {
    ModLog("part draw    : %u times the code of the game ran, because the x87 control word was "
           "0x%04X (the SSE code needs a precision of 24 bits and rounding to the nearest)",
           g_counts.inexact, g_counts.last_control);
  }
  if (g_counts.mxcsr_sets > 0) {
    ModLog("part draw    : MXCSR of the game was 0x%08X. For %u runs of the offsets the mod set "
           "it to round to the nearest with no flush to zero, and put it back after each run",
           g_counts.game_mxcsr, g_counts.mxcsr_sets);
  }
  g_counts = Counts();
}

}  // namespace

void PartDrawSetOptions(int mode, bool test_keys) {
  g_mode = mode < 0 || mode > 2 ? 0 : mode;
  g_test_keys = test_keys;
  ModLog("part draw    : part_draw=%d. %s", g_mode, ModeText(g_mode));
}

void PartDrawInstall() {
  if (g_in || (g_mode == 0 && !g_test_keys)) return;
  bool sites = Expect(kOffset1Site) && Expect(kOffset2Site) && Expect(kOffsetAfterSite);
  for (int i = 0; i < 4; ++i) sites = sites && Expect(kTriSites[i]) && Expect(kTriCompareSites[i]);
  if (!sites) {
    ModLog("part draw    : the code at a place of the triangles or the offsets is "
           "not the code of the game that the mod fits, so the code of the game runs");
    return;
  }
  const bool compare = g_mode == 2 || g_test_keys;
  void* const tri_stubs[4] = {reinterpret_cast<void*>(&TriStub0), reinterpret_cast<void*>(&TriStub1),
                              reinterpret_cast<void*>(&TriStub2), reinterpret_cast<void*>(&TriStub3)};
  bool ok = HookSite(kOffset1Site, reinterpret_cast<void*>(&Offset1Stub), &g_offset1_back) &&
            HookSite(kOffset2Site, reinterpret_cast<void*>(&Offset2Stub), &g_offset2_back);
  for (int i = 0; i < 4 && ok; ++i) ok = HookSite(kTriSites[i], tri_stubs[i], g_tri_back[i]);
  if (ok && compare) {
    ok = HookCompare(kOffsetAfterSite.address, reinterpret_cast<void*>(&PartDrawOffsetCompare),
                     -1, &g_offset_after_original);
    for (int i = 0; i < 4 && ok; ++i) {
      ok = HookCompare(kTriCompareSites[i].address,
                       reinterpret_cast<void*>(&PartDrawTrianglesCompare),
                       kTriCompareRegister[i], &g_tri_compare_original[i]);
    }
  }
  MH_STATUS applied = ok ? MH_ApplyQueued() : MH_UNKNOWN;
  if (applied != MH_OK) {
    MH_RemoveHook(reinterpret_cast<void*>(kOffset1Site.address));
    MH_RemoveHook(reinterpret_cast<void*>(kOffset2Site.address));
    MH_RemoveHook(reinterpret_cast<void*>(kOffsetAfterSite.address));
    for (int i = 0; i < 4; ++i) {
      MH_RemoveHook(reinterpret_cast<void*>(kTriSites[i].address));
      MH_RemoveHook(reinterpret_cast<void*>(kTriCompareSites[i].address));
    }
  }
  g_in = applied == MH_OK;
  g_compare_in = g_in && compare;
  ModLog("hooks        : the offsets of fn_00859530 at 0x%08X and 0x%08X, the triangle loops at "
         "0x%08X, 0x%08X, 0x%08X and 0x%08X %s%s",
         static_cast<unsigned>(kOffset1Site.address), static_cast<unsigned>(kOffset2Site.address),
         static_cast<unsigned>(kTriSites[0].address), static_cast<unsigned>(kTriSites[1].address),
         static_cast<unsigned>(kTriSites[2].address), static_cast<unsigned>(kTriSites[3].address),
         g_in ? "ok" : "FAILED", g_compare_in ? ", with the hooks of the compare" : "");
}

int PartDrawValue() {
  if (!g_in) return kDebugCannot;
  return g_mode == 1 ? 1 : 0;
}

void PartDrawToggle(const char* keys) {
  g_mode = g_mode == 1 ? 0 : 1;
  g_dry.pending = false;
  g_off_count = 0;
  ControllersGesturePulse(0.5f, g_mode == 1 ? 0.30f : 0.08f);
  ModLog("test key     : %s, part_draw is %d from the next mesh", keys, g_mode);
}

void PartDrawFrame() {
  if (!g_in) return;
  Report();
  if (!g_test_keys) return;
  const bool ctrl_shift = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                          (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
  const bool down = ctrl_shift && (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
  if (down && !g_key_down) {
    // 1, 0, 2, 1 ...: 2 only with the hooks of the compare.
    int next = g_mode == 1 ? 0 : (g_mode == 0 ? 2 : 1);
    if (next == 2 && !g_compare_in) next = 1;
    g_mode = next;
    g_dry.pending = false;
      g_off_count = 0;
    ModLog("part draw    : Ctrl Shift F4: part_draw=%d. %s", g_mode, ModeText(g_mode));
  }
  g_key_down = down;
}
