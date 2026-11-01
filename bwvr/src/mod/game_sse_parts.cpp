// THE SSE PATHS OF THE GAME, ONE AT A TIME (game_sse_parts, docs/backend.md,
// "The SSE paths of the game, one at a time"). The table of the parts and
// the rules of the compare are in game_sse_parts_math.h.
//
// THE PATCH. Each function of a part starts with "mov reg, [flag]" and a
// jump to its x87 code when the flag is 0. With game_sse_parts not 0 the mod
// writes the address of its own flag of the part into each such instruction,
// before each other hook of the mod. A hook of MinHook on such a function
// copies the first instructions into its trampoline, so the copy reads the
// flag of the mod too. The flags of the mod change only at the end of a
// frame (GameSsePartsFrame), so each eye pass of a frame has the same paths.
//
// THE COMPARE (game_sse_check=1, a test aid). A hook on the function of a
// part runs it two times on the same input: first with the flag 0 (the x87
// path), then with the flag 1 (the SSE path). Between the two runs the hook
// puts back each global that the first run changed. The output of the SSE
// path stays, and the log has the largest difference to the output of the
// x87 path. The land is different: the SSE path of a patch does the work of
// the whole fn_00874AA0, with its draws. So a stub at the two calls of the
// builders of the triangles in fn_007A1800 compares the vertices of Table1
// and Table0 at that moment with the vertices that land_fast_math::Grid
// makes. Grid gives the bits of the x87 loop of the game (land_fast=2 shows
// that), when the x87 unit has 24 bits and rounds to the nearest.
//
// The parts that draw, call a method of an object or change the state of
// the game cannot run two times, so they have no compare: the sprite, the
// sphere on the screen, the tiles of the textures of the land and the spans
// of a byte map.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <emmintrin.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"
#include "fast_transform_math.h"
#include "game_sse_parts.h"
#include "game_sse_parts_math.h"
#include "land_fast_math.h"

void ModLog(const char* format, ...);
bool StereoInPass();
int StereoPassEye();

namespace {

using namespace game_sse_parts_math;

static_assert(kFloatToInt == 12, "FtolCheck writes the flag at g_flags + 48");

const uintptr_t kTable0 = 0x00E3B5E0;       // LH3DP3::Table0, the clip flags
const uintptr_t kTable1 = 0x00E437E0;       // LH3DP3::Table1, the vertices
const uintptr_t kList = 0x00EA5DE0;         // g_TriangleList, 16 bit indices
const uintptr_t kListCount = 0x00EA9EA4;    // int32, the indices in the list
const uintptr_t kVertexCount = 0x00EA9EA8;  // uint16, the vertices in Table1
const uintptr_t kListFlag = 0x00EA9EB8;     // int32, the builders set it to 0
const int32_t kListMost = static_cast<int32_t>((kListCount - kList) / 2);
const uint32_t kTableVertices = 8192;  // the vertices that a compare reads at the most
const uint32_t kCopyMost = 1024;       // the new indices and vertices that a compare keeps
const uint32_t kLandMost = 17 * 17;
const int kLinesMost = 8;              // the lines of samples of a part in a run

// The calls of the builders of the triangles in fn_007A1800.
const uintptr_t kLandCallClip = 0x007A2D14;    // call fn_00875C60
const uintptr_t kLandCallNoClip = 0x007A2D23;  // call fn_00876910
const uint8_t kLandCallClipBytes[5] = {0xE8, 0x47, 0x2F, 0x0D, 0x00};
const uint8_t kLandCallNoClipBytes[5] = {0xE8, 0xE8, 0x3B, 0x0D, 0x00};

uint32_t g_mask = 0;        // game_sse_parts
int g_check = 0;            // game_sse_check
bool g_test_keys = false;   // test_keys
bool g_patched = false;
bool g_old_way = false;     // the test key: each part reads the flag of the game
bool g_key_down = false;
int g_last_eye = -1;
uint32_t g_checked = 0;     // the parts with a hook of the compare
uint64_t g_report_at = 0;
uint32_t g_frames = 0;      // the ends of frames in the window of the report
uint16_t g_last_control = 0;

// The flags that the patched readers read, one for each part. Each reader
// reads 4 bytes.
volatile int32_t g_flags[kPartCount] = {};

template <typename T>
T Read(uintptr_t address) {
  T v;
  memcpy(&v, reinterpret_cast<const void*>(address), sizeof(T));
  return v;
}

bool Expect(uintptr_t address, const uint8_t* bytes, size_t size) {
  const void* site = reinterpret_cast<const void*>(address);
  return !IsBadReadPtr(site, size) && memcmp(site, bytes, size) == 0;
}

bool WriteCode(uintptr_t address, const void* bytes, size_t size) {
  void* at = reinterpret_cast<void*>(address);
  DWORD old = 0;
  if (!VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &old)) return false;
  memcpy(at, bytes, size);
  DWORD unused = 0;
  VirtualProtect(at, size, old, &unused);
  FlushInstructionCache(GetCurrentProcess(), at, size);
  return true;
}

bool OwnWindowInFront() {
  HWND w = GetForegroundWindow();
  DWORD pid = 0;
  if (w != nullptr) GetWindowThreadProcessId(w, &pid);
  return pid == GetCurrentProcessId();
}

// THE RESULTS OF THE COMPARE, on the frame thread. The float to integer
// runs on more threads, so its counts can lose a step.
struct Stats {
  uint32_t calls = 0;
  uint32_t compared = 0;
  uint32_t differ = 0;    // calls with an output that differs
  uint32_t skipped = 0;   // calls with no compare
  uint32_t counts = 0;    // calls with a count of the list, of the vertices or a flag that differs
  uint32_t words = 0;     // indices of the list that differ
  uint32_t int_most = 0;  // the largest difference of an integer or of a color channel
  uint32_t outside = 0;   // float to integer: differences for a value of 2^31 or more in size
  uint32_t items = 0;     // indices and new vertices that the compare read
  VertexDiff v;
  int lines = 0;          // the lines of samples with a value that differs, in the whole run
  int flag_lines = 0;     // the lines of samples with only clip flags that differ
  // The vertex with the largest difference in the window of the report.
  bool worst_on = false;
  double worst_most = 0.0;
  uint32_t worst_index = 0;
  uint32_t worst_want[8] = {};
  uint32_t worst_got[8] = {};
  uint32_t worst_flags[2] = {};
};
Stats g_stats[kPartCount];

void LogVertex(int part, const char* what, uint32_t index, const uint32_t* want,
               const uint32_t* got, uint32_t want_flag, uint32_t got_flag) {
  Stats& s = g_stats[part];
  const double most =
      VertexMost(want, got, want_flag != 0 && want_flag == got_flag);
  if (memcmp(want, got, 32) != 0 && (!s.worst_on || WorseThan(most, s.worst_most))) {
    s.worst_on = true;
    s.worst_most = most;
    s.worst_index = index;
    memcpy(s.worst_want, want, 32);
    memcpy(s.worst_got, got, 32);
    s.worst_flags[0] = want_flag;
    s.worst_flags[1] = got_flag;
  }
  if (memcmp(want, got, 32) != 0) {
    if (s.lines >= kLinesMost) return;
    ++s.lines;
  } else {
    if (s.flag_lines >= 2) return;
    ++s.flag_lines;
  }
  ModLog("game sse     : compare of %s, %s %u: x87 %08X %08X %08X %08X %08X %08X "
         "%08X %08X flags %X, SSE %08X %08X %08X %08X %08X %08X %08X %08X flags %X",
         kParts[part].name, what, index, want[0], want[1], want[2], want[3],
         want[4], want[5], want[6], want[7], want_flag, got[0], got[1], got[2],
         got[3], got[4], got[5], got[6], got[7], got_flag);
}

// The x87 run of a compare on the frame thread: each part with a hook of
// the compare has the flag 0, and float to integer gives the result of its
// x87 path. Else 0.
volatile int32_t g_x87_run = 0;

// FLOAT TO INTEGER (fn_007A1400). It takes the value in ST0, pops it, and
// gives the integer in eax. Its x87 path changes ecx and edx, its SSE path
// changes ecx and keeps edx. The hook copies ST0, runs the x87 path on the
// copy and the SSE path on the value with the edx of the caller, and gives
// the edx of the SSE run. Each value of the hook is on the stack, because
// more threads call the function. GameSseFtolCompare uses no x87 register:
// the caller can have values on the x87 stack below ST0. It gives the
// result of the SSE path, or of the x87 path in the x87 run of a compare.
void* g_ftol_original = nullptr;
struct FtolSample {
  uint64_t value;
  uint32_t sse;
  uint32_t x87;
};
FtolSample g_ftol_samples[kLinesMost];
volatile LONG g_ftol_sample_count = 0;
int g_ftol_logged = 0;
FtolSample g_ftol_outside;  // the first difference for a value out of the range
int g_ftol_outside_count = 0;
bool g_ftol_outside_logged = false;

extern "C" uint32_t __cdecl GameSseFtolCompare(const uint64_t* value, uint32_t sse,
                                               uint32_t x87) {
  const uint32_t give = g_x87_run != 0 ? x87 : sse;
  Stats& s = g_stats[kFloatToInt];
  ++s.calls;
  ++s.compared;
  if (sse == x87) return give;
  ++s.differ;
  if (!FtolInRange(*value)) {
    ++s.outside;
    if (g_ftol_outside_count == 0) {
      g_ftol_outside.value = *value;
      g_ftol_outside.sse = sse;
      g_ftol_outside.x87 = x87;
      g_ftol_outside_count = 1;
    }
    return give;
  }
  const uint32_t d = sse - x87;
  const uint32_t ad = static_cast<int32_t>(d) < 0 ? 0u - d : d;
  if (ad > s.int_most) s.int_most = ad;
  const LONG n = InterlockedIncrement(&g_ftol_sample_count) - 1;
  if (n < kLinesMost) {
    g_ftol_samples[n].value = *value;
    g_ftol_samples[n].sse = sse;
    g_ftol_samples[n].x87 = x87;
  }
  return give;
}

__declspec(naked) void FtolCheck() {
  __asm {
    push edx                           // [esp]: the edx of the caller
    sub esp, 8
    fst qword ptr [esp]                // [esp]: the value, for the log
    fld st(0)
    mov dword ptr [g_flags + 48], 0
    call dword ptr [g_ftol_original]   // the x87 path on the copy
    push eax                           // [esp]: x87, [esp + 12]: the edx
    mov edx, dword ptr [esp + 12]
    mov dword ptr [g_flags + 48], 1
    call dword ptr [g_ftol_original]   // the SSE path on the value
    push edx                           // the edx of the SSE run
    push eax                           // sse, edx, x87, the value
    push dword ptr [esp + 8]           // x87
    push dword ptr [esp + 4]           // sse
    lea ecx, [esp + 20]                // the value
    push ecx
    call GameSseFtolCompare            // eax: the result to give
    add esp, 12
    pop ecx
    pop edx
    add esp, 16
    ret
  }
}

// A compare runs: a part that it calls runs one time. Each run of a compare
// sets the flag of each part with a hook of the compare (0 for the x87 run,
// 1 for the SSE run), so a part that it calls takes the path of that run.
bool g_busy = false;
uint32_t g_checked_parts = 0;  // the parts with a hook of the compare

void RunFlags(int32_t value) {
  for (int part = 0; part < kPartCount; ++part) {
    if (((g_checked_parts >> part) & 1u) != 0) g_flags[part] = value;
  }
  g_x87_run = value == 0 ? 1 : 0;
}

// THE LIGHT OF THE LAND (fn_00801C90 at a point, fn_00802120 at a cell):
// cdecl (the place, the first color out, the second color out).
typedef void(__cdecl* LightFn)(const void* at, uint32_t* a, uint32_t* b);
LightFn g_light_point = nullptr;
LightFn g_light_cell = nullptr;

void LightCompare(int part, LightFn fn, const void* at, uint32_t* a, uint32_t* b) {
  if (g_busy || a == nullptr || b == nullptr || a == b) {
    fn(at, a, b);
    return;
  }
  g_busy = true;
  uint32_t want_a = *a;
  uint32_t want_b = *b;
  RunFlags(0);
  fn(at, &want_a, &want_b);
  RunFlags(1);
  fn(at, a, b);
  g_busy = false;
  Stats& s = g_stats[part];
  ++s.calls;
  ++s.compared;
  if (want_a == *a && want_b == *b) return;
  ++s.differ;
  const uint32_t d = ColorDiff(want_a, *a) > ColorDiff(want_b, *b)
                         ? ColorDiff(want_a, *a)
                         : ColorDiff(want_b, *b);
  if (d > s.int_most) s.int_most = d;
  if (s.lines < kLinesMost) {
    ++s.lines;
    ModLog("game sse     : compare of %s: x87 %08X %08X, SSE %08X %08X",
           kParts[part].name, want_a, want_b, *a, *b);
  }
}

void __cdecl LightPointCheck(const void* at, uint32_t* a, uint32_t* b) {
  LightCompare(kLightPoint, g_light_point, at, a, b);
}

void __cdecl LightCellCheck(const void* at, uint32_t* a, uint32_t* b) {
  LightCompare(kLightCell, g_light_cell, at, a, b);
}

uint32_t* VertexAt(uint32_t i) { return reinterpret_cast<uint32_t*>(kTable1 + i * 32); }
uint32_t* FlagAt(uint32_t i) { return reinterpret_cast<uint32_t*>(kTable0 + i * 4); }

// THE NEW VERTEX OF A CLIP (fn_0081DD90): fastcall (the plane, the vertex
// inside, the vertex outside, the new vertex). Only the new vertex and its
// clip flags change.
typedef void(__fastcall* ClipVertexFn)(uint32_t plane, uint32_t in, uint32_t out,
                                       uint32_t made);
ClipVertexFn g_clip_vertex = nullptr;

void __fastcall ClipVertexCheck(uint32_t plane, uint32_t in, uint32_t out, uint32_t made) {
  const uint32_t m = made & 0xFFFFu;
  if (g_busy || m >= kTableVertices) {
    if (!g_busy) ++g_stats[kClipVertex].skipped;
    g_clip_vertex(plane, in, out, made);
    return;
  }
  g_busy = true;
  uint32_t before[8], want[8];
  memcpy(before, VertexAt(m), 32);
  const uint32_t before_flag = *FlagAt(m);
  RunFlags(0);
  g_clip_vertex(plane, in, out, made);
  memcpy(want, VertexAt(m), 32);
  const uint32_t want_flag = *FlagAt(m);
  memcpy(VertexAt(m), before, 32);
  *FlagAt(m) = before_flag;
  RunFlags(1);
  g_clip_vertex(plane, in, out, made);
  g_busy = false;
  Stats& s = g_stats[kClipVertex];
  ++s.calls;
  ++s.compared;
  ++s.items;
  if (s.v.Add(want, VertexAt(m), true, want_flag, *FlagAt(m))) {
    ++s.differ;
    LogVertex(kClipVertex, "vertex", m, want, VertexAt(m), want_flag, *FlagAt(m));
  }
}

// THE LIST OF THE TRIANGLES. The builders of a patch (fn_00876910 and
// fn_00875C60) and the clip of a triangle (fn_0081A760) add indices to the
// list and count them at 0x00EA9EA4. A clip adds new vertices at the end of
// Table1 and counts them at 0x00EA9EA8.
struct ListState {
  int32_t count;
  uint16_t vertices;
  int32_t flag;
};

ListState ReadList() {
  ListState s;
  s.count = Read<int32_t>(kListCount);
  s.vertices = Read<uint16_t>(kVertexCount);
  s.flag = Read<int32_t>(kListFlag);
  return s;
}

void WriteList(const ListState& s) {
  memcpy(reinterpret_cast<void*>(kListCount), &s.count, 4);
  memcpy(reinterpret_cast<void*>(kVertexCount), &s.vertices, 2);
  memcpy(reinterpret_cast<void*>(kListFlag), &s.flag, 4);
}

uint16_t g_want_list[kCopyMost];
uint32_t g_want_vertices[kCopyMost * 8];
uint32_t g_want_flags[kCopyMost];

template <typename Call>
void ListCompare(int part, Call call) {
  if (g_busy) {
    call();
    return;
  }
  Stats& s = g_stats[part];
  const ListState before = ReadList();
  if (before.count < 0 || before.count > kListMost || before.vertices >= kTableVertices) {
    ++s.skipped;
    call();
    return;
  }
  g_busy = true;
  RunFlags(0);
  call();
  const ListState want = ReadList();
  uint32_t words = 0;
  if (want.count > before.count && want.count <= kListMost) {
    words = static_cast<uint32_t>(want.count - before.count);
  }
  if (words > kCopyMost) words = kCopyMost;
  memcpy(g_want_list, reinterpret_cast<const uint16_t*>(kList) + before.count, words * 2);
  uint32_t made = 0;
  if (want.vertices > before.vertices && want.vertices <= kTableVertices) {
    made = static_cast<uint32_t>(want.vertices - before.vertices);
  }
  if (made > kCopyMost) made = kCopyMost;
  memcpy(g_want_vertices, VertexAt(before.vertices), made * 32);
  memcpy(g_want_flags, FlagAt(before.vertices), made * 4);
  WriteList(before);
  RunFlags(1);
  call();
  const ListState got = ReadList();
  g_busy = false;
  ++s.calls;
  ++s.compared;
  bool differ = false;
  if (got.count != want.count || got.vertices != want.vertices || got.flag != want.flag) {
    ++s.counts;
    differ = true;
    if (s.lines < kLinesMost) {
      ++s.lines;
      ModLog("game sse     : compare of %s: from %d indices and %u vertices, x87 "
             "%d indices, %u vertices, flag %d, SSE %d indices, %u vertices, flag %d",
             kParts[part].name, before.count, before.vertices, want.count,
             want.vertices, want.flag, got.count, got.vertices, got.flag);
    }
  }
  const uint16_t* list = reinterpret_cast<const uint16_t*>(kList) + before.count;
  for (uint32_t i = 0; i < words && static_cast<int32_t>(before.count + i) < got.count; ++i) {
    ++s.items;
    if (list[i] != g_want_list[i]) {
      ++s.words;
      differ = true;
    }
  }
  const uint32_t made_got =
      got.vertices > before.vertices ? static_cast<uint32_t>(got.vertices - before.vertices) : 0u;
  for (uint32_t i = 0; i < made && i < made_got; ++i) {
    const uint32_t at = before.vertices + i;
    ++s.items;
    if (s.v.Add(g_want_vertices + i * 8, VertexAt(at), true, g_want_flags[i], *FlagAt(at))) {
      differ = true;
      LogVertex(part, "new vertex", at, g_want_vertices + i * 8, VertexAt(at),
                g_want_flags[i], *FlagAt(at));
    }
  }
  if (differ) ++s.differ;
}

// fn_0081A760: fastcall (three vertices, the planes to test).
typedef void(__fastcall* ClipTriangleFn)(uint32_t a, uint32_t b, uint32_t c, uint32_t planes);
ClipTriangleFn g_clip_triangle = nullptr;

void __fastcall ClipTriangleCheck(uint32_t a, uint32_t b, uint32_t c, uint32_t planes) {
  ListCompare(kClipTriangle, [&]() { g_clip_triangle(a, b, c, planes); });
}

// fn_00876910 and fn_00875C60: fastcall (the patch, [patch+0x930]).
typedef void(__fastcall* TrianglesFn)(void* patch, uint32_t shift);
TrianglesFn g_triangles = nullptr;
TrianglesFn g_triangles_clip = nullptr;

void __fastcall TrianglesCheck(void* patch, uint32_t shift) {
  ListCompare(kTriangles, [&]() { g_triangles(patch, shift); });
}

void __fastcall TrianglesClipCheck(void* patch, uint32_t shift) {
  ListCompare(kTrianglesClip, [&]() { g_triangles_clip(patch, shift); });
}

// THE LAND. The values of the game that the loop of the land reads, as in
// land_fast.cpp.
void FillLand(land_fast_math::Consts* c) {
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

uint8_t g_land_copy[kLandMost * 32];
uint32_t g_land_flags[kLandMost];
uintptr_t g_land_clip_target = 0x00875C60;
uintptr_t g_land_target = 0x00876910;

// Called by the stubs with the patch, before fn_007A1800 calls the builder
// of the triangles. The vertices of the SSE path are in Table1 and Table0.
extern "C" void __cdecl GameSseLandCompare(const uint8_t* patch) {
  Stats& s = g_stats[kLand];
  ++s.calls;
  uint16_t control = 0;
  __asm fnstcw control
  const uint32_t game_mxcsr = _mm_getcsr();
  const uint32_t loop_mxcsr = game_mxcsr & ~0xE040u;
  if (!fast_transform_math::Exact(control, loop_mxcsr)) {
    ++s.skipped;
    g_last_control = control;
    return;
  }
  const uintptr_t at = reinterpret_cast<uintptr_t>(patch);
  _mm_setcsr(loop_mxcsr);
  LandLocals l;
  const bool ok = MakeLandLocals(Read<int32_t>(at + 0x930), Read<int32_t>(at + 0x938),
                                 Read<float>(at + 0x9C4), Read<float>(at + 0x9C8),
                                 Read<float>(0x008AB414), Read<float>(0x008AB3D8),
                                 Read<float>(0x009A3C18), Read<float>(0x009A3C14), &l);
  land_fast_math::Patch p;
  p.cells = patch;
  p.step = l.step;
  p.row_skip = l.row_skip;
  p.d = l.d;
  p.du = l.du;
  p.u0 = l.u0;
  p.v0 = l.v0;
  memcpy(&p.r0, patch + 0x90C, 4);
  memcpy(&p.c0, patch + 0x910, 4);
  memcpy(&p.clip, patch + 0x91C, 4);
  memcpy(&p.clear_low, patch + 0x928, 4);
  memcpy(&p.light, patch + 0x940, 4);
  land_fast_math::Consts c;
  FillLand(&c);
  const bool fog_ok = p.light == 0 || p.light == 2 || land_fast_math::FogExact(c.fog_dim);
  const uint32_t side = ok ? static_cast<uint32_t>(land_fast_math::GridSide(p.step)) : 0u;
  const uint32_t count = side * side;
  if (!ok || !fog_ok || count == 0 || count > kLandMost) {
    _mm_setcsr(game_mxcsr);
    ++s.skipped;
    return;
  }
  // The bytes that the loop does not write are as in the table now.
  memcpy(g_land_copy, reinterpret_cast<const void*>(kTable1), count * 32);
  land_fast_math::Grid(c, p, g_land_copy, g_land_flags, nullptr);
  _mm_setcsr(game_mxcsr);
  ++s.compared;
  bool differ = false;
  if (Read<uint16_t>(kVertexCount) != count) {
    ++s.counts;
    differ = true;
  }
  s.items += count;
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t* want = reinterpret_cast<const uint32_t*>(g_land_copy + i * 32);
    if (s.v.Add(want, VertexAt(i), p.clip != 0, g_land_flags[i], *FlagAt(i))) {
      differ = true;
      LogVertex(kLand, "vertex", i, want, VertexAt(i), p.clip != 0 ? g_land_flags[i] : 0,
                p.clip != 0 ? *FlagAt(i) : 0);
    }
  }
  if (differ) ++s.differ;
}

__declspec(naked) void LandClipStub() {
  __asm {
    pushad
    push ecx
    call GameSseLandCompare
    add esp, 4
    popad
    jmp dword ptr [g_land_clip_target]
  }
}

__declspec(naked) void LandStub() {
  __asm {
    pushad
    push ecx
    call GameSseLandCompare
    add esp, 4
    popad
    jmp dword ptr [g_land_target]
  }
}

bool PatchCall(uintptr_t site, const uint8_t* expect, void* stub) {
  if (!Expect(site, expect, 5)) return false;
  uint8_t bytes[5] = {0xE8, 0, 0, 0, 0};
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<intptr_t>(stub) -
                                           static_cast<intptr_t>(site + 5));
  memcpy(bytes + 1, &rel, 4);
  return WriteCode(site, bytes, 5);
}

bool HookPart(int part, void* detour, void** original) {
  void* at = reinterpret_cast<void*>(kParts[part].function);
  MH_STATUS st = MH_CreateHook(at, detour, original);
  if (st == MH_OK) st = MH_EnableHook(at);
  ModLog("hooks        : the compare of %s at 0x%08X %s", kParts[part].name,
         static_cast<unsigned>(kParts[part].function),
         st == MH_OK ? "ok" : MH_StatusToString(st));
  return st == MH_OK;
}

void InstallChecks() {
  for (int part = 0; part < kPartCount; ++part) {
    if (((g_mask >> part) & 1u) == 0) continue;
    if (!kParts[part].compare) {
      ModLog("game sse     : %s (bit 0x%04X) has no compare: it draws or changes the "
             "state of the game, so it cannot run two times",
             kParts[part].name, 1u << part);
      continue;
    }
    bool ok = false;
    switch (part) {
      case kLand:
        ok = PatchCall(kLandCallClip, kLandCallClipBytes,
                       reinterpret_cast<void*>(&LandClipStub)) &&
             PatchCall(kLandCallNoClip, kLandCallNoClipBytes, reinterpret_cast<void*>(&LandStub));
        ModLog("hooks        : the compare of %s at 0x%08X and 0x%08X %s", kParts[part].name,
               static_cast<unsigned>(kLandCallClip), static_cast<unsigned>(kLandCallNoClip),
               ok ? "ok" : "FAILED (not the code of the calls)");
        break;
      case kTriangles:
        ok = HookPart(part, reinterpret_cast<void*>(&TrianglesCheck),
                      reinterpret_cast<void**>(&g_triangles));
        break;
      case kTrianglesClip:
        ok = HookPart(part, reinterpret_cast<void*>(&TrianglesClipCheck),
                      reinterpret_cast<void**>(&g_triangles_clip));
        break;
      case kClipTriangle:
        ok = HookPart(part, reinterpret_cast<void*>(&ClipTriangleCheck),
                      reinterpret_cast<void**>(&g_clip_triangle));
        break;
      case kClipVertex:
        ok = HookPart(part, reinterpret_cast<void*>(&ClipVertexCheck),
                      reinterpret_cast<void**>(&g_clip_vertex));
        break;
      case kLightPoint:
        ok = HookPart(part, reinterpret_cast<void*>(&LightPointCheck),
                      reinterpret_cast<void**>(&g_light_point));
        break;
      case kLightCell:
        ok = HookPart(part, reinterpret_cast<void*>(&LightCellCheck),
                      reinterpret_cast<void**>(&g_light_cell));
        break;
      case kFloatToInt:
        ok = HookPart(part, reinterpret_cast<void*>(&FtolCheck), &g_ftol_original);
        break;
      default:
        break;
    }
    if (ok) g_checked |= 1u << part;
  }
  g_checked_parts = g_checked;
}

// The flags of the mod from the mask, or from the flags of the game. A part
// with a hook of the compare keeps the flag 1: its hook sets the flag for
// each of its two runs.
void Latch() {
  for (int part = 0; part < kPartCount; ++part) {
    if (((g_checked >> part) & 1u) != 0) continue;
    const int32_t game = Read<int32_t>(GameFlag(part));
    const int32_t want = PartFlag(g_mask, part, g_old_way, game);
    if (g_flags[part] != want) g_flags[part] = want;
  }
}

void Report() {
  const uint64_t now = GetTickCount64();
  if (g_report_at == 0) g_report_at = now + 10000;
  if (now < g_report_at) return;
  g_report_at = now + 10000;
  const uint32_t frames = g_frames;
  g_frames = 0;
  if (g_checked == 0) return;
  for (int part = 0; part < kPartCount; ++part) {
    if (((g_checked >> part) & 1u) == 0) continue;
    Stats& s = g_stats[part];
    const VertexDiff& v = s.v;
    uint32_t nan = 0;
    for (int f = 0; f < 6; ++f) nan += v.field[f].nan;
    ModLog("game sse     : compare of %s (bit 0x%04X) in 10 s: %u calls in %u frames, "
           "%u compared, %u skipped, %u differ. %u indices and vertices read. Vertices "
           "that differ %u, with a value that differs %u, the largest difference of "
           "x %.6g, y %.6g, z %.6g, rhw %.6g, u %.6g, v %.6g, floats with a NaN %u, "
           "colors that differ %u (the largest step of a channel %u), clip flags %u, "
           "z of clipped vertices %u. Counts that differ %u, indices %u. The largest "
           "difference of an integer or of a color %u, differences for a value of "
           "2^31 or more %u",
           kParts[part].name, 1u << part, s.calls, frames, s.compared, s.skipped,
           s.differ, s.items, v.vertices, v.values, v.field[0].most, v.field[1].most,
           v.field[2].most, v.field[3].most, v.field[4].most, v.field[5].most, nan,
           v.colors, v.color_most, v.flags, v.clipped_z, s.counts, s.words, s.int_most,
           s.outside);
    if (s.worst_on) {
      const uint32_t* w = s.worst_want;
      const uint32_t* g = s.worst_got;
      ModLog("game sse     : compare of %s, the vertex with the largest difference in "
             "10 s (%.6g, -1 is a NaN), %u: x87 %08X %08X %08X %08X %08X %08X %08X "
             "%08X flags %X, SSE %08X %08X %08X %08X %08X %08X %08X %08X flags %X",
             kParts[part].name, s.worst_most, s.worst_index, w[0], w[1], w[2], w[3],
             w[4], w[5], w[6], w[7], s.worst_flags[0], g[0], g[1], g[2], g[3], g[4],
             g[5], g[6], g[7], s.worst_flags[1]);
    }
    if (part == kLand && s.skipped > 0 && g_last_control != 0) {
      ModLog("game sse     : the compare of the land skipped patches with the x87 "
             "control word 0x%04X (Grid needs a precision of 24 bits and rounding to "
             "the nearest)",
             g_last_control);
    }
    const int lines = s.lines;
    const int flag_lines = s.flag_lines;
    s = Stats();
    s.lines = lines;
    s.flag_lines = flag_lines;
  }
  const LONG samples = g_ftol_sample_count;
  while (g_ftol_logged < kLinesMost && g_ftol_logged < samples) {
    const FtolSample& f = g_ftol_samples[g_ftol_logged++];
    double value;
    memcpy(&value, &f.value, 8);
    ModLog("game sse     : compare of float to integer: the value %.17g, x87 %d, SSE %d",
           value, static_cast<int32_t>(f.x87), static_cast<int32_t>(f.sse));
  }
  if (g_ftol_outside_count != 0 && !g_ftol_outside_logged) {
    g_ftol_outside_logged = true;
    double value;
    memcpy(&value, &g_ftol_outside.value, 8);
    ModLog("game sse     : compare of float to integer, the first difference for a value "
           "out of the range: the value %.17g, x87 %d, SSE %d",
           value, static_cast<int32_t>(g_ftol_outside.x87),
           static_cast<int32_t>(g_ftol_outside.sse));
  }
}

}  // namespace

void GameSsePartsSetOptions(int parts, int check, bool test_keys) {
  g_mask = Mask(parts);
  g_check = g_mask != 0 && check != 0 ? 1 : 0;
  g_test_keys = test_keys;
  if (g_mask == 0) {
    ModLog("game sse     : game_sse_parts=0. Each part of the game reads the flag of "
           "the game (the class of the CPU, and game_sse_path)");
    return;
  }
  ModLog("game sse     : game_sse_parts=%u (0x%04X). Each part with its bit takes the "
         "SSE path of the game, each other part the x87 path. game_sse_check=%d%s",
         g_mask, g_mask, g_check,
         g_check != 0 ? ": each part with its bit runs on both paths, and the log "
                        "has the differences"
                      : "");
}

void GameSsePartsInstall() {
  if (g_mask == 0 || g_patched) return;
  for (int i = 0; i < kReaderCount; ++i) {
    const Reader& r = kReaders[i];
    if (!Expect(r.address, r.bytes, r.size)) {
      ModLog("game sse     : the code at 0x%08X is not the reader of the flag of %s, so "
             "game_sse_parts does nothing",
             static_cast<unsigned>(r.address), kParts[r.part].name);
      return;
    }
  }
  Latch();
  int written = 0;
  for (int i = 0; i < kReaderCount; ++i) {
    const Reader& r = kReaders[i];
    uint8_t bytes[6];
    memcpy(bytes, r.bytes, r.size);
    const uint32_t flag =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_flags[r.part]));
    memcpy(bytes + r.size - 4, &flag, 4);
    if (WriteCode(r.address, bytes, r.size)) ++written;
  }
  g_patched = written == kReaderCount;
  ModLog("hooks        : the readers of the flags of the SSE paths: %d of %d %s",
         written, kReaderCount, g_patched ? "ok" : "FAILED");
  for (int part = 0; part < kPartCount; ++part) {
    ModLog("game sse     : bit 0x%04X %s, fn_%08X: the %s path", 1u << part,
           kParts[part].name, static_cast<unsigned>(kParts[part].function),
           g_flags[part] != 0 ? "SSE" : "x87");
  }
  if (g_patched && g_check != 0) InstallChecks();
}

int GameSseSphereFlag() {
  if (g_patched) return g_flags[kSphere];
  return Read<int32_t>(GameFlag(kSphere));
}

void GameSsePartsFrame() {
  if (!g_patched) return;
  const bool in_pass = StereoInPass();
  const int eye = in_pass ? StereoPassEye() : -1;
  // The end of a frame: no eye pass, the pass of the right eye, or a second
  // pass of the left eye in a row (one walk for both eyes).
  const bool frame_end = !in_pass || eye != 0 || g_last_eye == 0;
  g_last_eye = eye;
  if (g_test_keys && g_check == 0) {
    const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool down = (GetAsyncKeyState('S') & 0x8000) != 0 && ctrl && shift;
    if (down && !g_key_down && OwnWindowInFront()) {
      g_old_way = !g_old_way;
      if (g_old_way) {
        ModLog("test key     : Ctrl, Shift and S: each part reads the "
               "flag of the game from the end of this frame (0x00EA9EB4 = %d, "
               "0x00E83A20 = %d)",
               Read<int32_t>(kPathFlag), Read<int32_t>(kFloatFlag));
      } else {
        ModLog("test key     : Ctrl, Shift and S: game_sse_parts=%u "
               "(0x%04X) from the end of this frame",
               g_mask, g_mask);
      }
    }
    g_key_down = down;
  }
  if (!frame_end) return;
  ++g_frames;
  Latch();
  Report();
}
