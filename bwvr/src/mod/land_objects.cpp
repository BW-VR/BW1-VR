// The objects on the land with SSE and worker threads (docs/backend.md, "The
// objects on the land"). The math is in land_objects_math.h.
//
// WHERE. fn_005E5CD0 draws the objects on the land. The timer of the objects
// (object_timer.cpp) shows where its time goes: the draws of the 3D parts
// (fn_00815A70, fn_0080DB30), the mesh loops of fast_transform.cpp, and
// fn_0080F700 (vtable+0x144 of a 3D part), which draws its meshes with
// fn_00852C40. Two parts of this work are here with SSE:
//
// 1. THE REFLECTION MESHES. fn_00852C40 is a mesh with a reflection map: it
//    makes each vertex on the x87 unit (the matrix, the light, the place in
//    the reflection map, the clip flags and the projection), then builds the
//    triangles and draws them. A jump at the start of its vertex loop with one
//    matrix goes to a stub here. When the SSE loop did the work, the stub goes
//    on at the code after the loop. Else it goes back into the loop of the
//    game, through the trampoline of MinHook.
//
//      fn_00852C40  0x00852EEC  lea eax,[ebp-8]; mov [ebp-14h],eax
//                                                     after: 0x00853672
//
//    At the start of the loop the x87 stack is empty. The locals of
//    fn_00852C40 hold the mesh ([ebp-1Ch]) and the two rows of the reflection
//    map ([ebp-58h], [ebp-4Ch], [ebp-40h] and [ebp-54h], [ebp-48h],
//    [ebp-3Ch]). The code after the loop reads only [ebp-1Ch], and sets each
//    other register and local that it uses. The loop with a matrix for each
//    group of vertices (the bones of a mesh) stays the code of the game.
//
// 2. THE TEST OF THE VIEW of each 3D part (LH3DBoundingBox::CheckRegionOnScreen,
//    fn_00868C80). hand_direct.cpp hooks its start, so the stub of the mod is
//    at the start of its path of the x87 unit, after the test of the SSE flag
//    of the game and the sub esp of its frame:
//
//      fn_00868C80  0x00868CA2  mov ecx,[esp+3Ch]; fld [ecx+44h]
//                   return 0: 0x00868FB7, return 1: 0x00868FAC
//
//    The bounds are in eax and the part at [esp+3Ch]. The stub writes the
//    globals of the test in the order of the game (0x00EA1AD0, 0x00EA1AF0,
//    0x00EA1AF4, 0x00C37EA0), calls the entry +0xA0 of the part with the edx
//    of the game, and goes to the end of the game with its return value.
//
// THE THREADS. land_objects_threads=N starts N worker threads. At the start
// of fn_005E5CD0 (a hook of MinHook) the workers and the frame thread make the
// test of the view of each object of its list, before the draws. The test of
// one object reads only that object, its 3D part, its mesh and the camera.
// A tree (Tree::Draw) scales two entries of the matrix of its part for its
// draw, so the work before the draws does the same for a tree. At the call of
// the test in the draw the stub takes the result when the bounds, the part
// and the globals are the same bits as before the draws, else it makes the
// test itself. So the draws stay in the order of the game, and each call of
// +0xA0 stays at its place. The SSE loop of a mesh with at least 2 parts of
// kLeast vertices goes in parts to the threads too. A worker waits with
// WaitOnAddress (in Wine a futex of the process, with no call of the server).
//
// THE SAME BITS. As fast_transform.cpp: the SSE code runs only while the x87
// unit has the precision of 24 bits and rounds to the nearest, and MXCSR is
// set to round to the nearest with no flush to zero while it runs, on each
// thread. land_objects=2 runs the code of the game and compares it with the
// SSE code: the vertices after the loop, and the globals, the return value
// and the call of +0xA0 at each of the four ends of the test of the view.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <emmintrin.h>
#include <intrin.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"
#include "fast_transform_math.h"
#include "game_sse_parts.h"
#include "land_objects.h"
#include "land_objects_math.h"
#include "object_timer.h"
#include "thread_id.h"

void ModLog(const char* format, ...);

namespace {

namespace lo = land_objects_math;

const uintptr_t kLoop = 0x00852EEC;
const uintptr_t kAfter = 0x00853672;
const uint8_t kLoopBytes[11] = {0x8D, 0x45, 0xF8, 0x89, 0x45, 0xEC,
                                0xB9, 0xE4, 0x37, 0xE4, 0x00};
const uint8_t kAfterBytes[7] = {0x8B, 0x4D, 0xE4, 0x66, 0x8B, 0x51, 0x10};

const uintptr_t kCullPath = 0x00868CA2;
const uint8_t kCullPathBytes[7] = {0x8B, 0x4C, 0x24, 0x3C, 0xD9, 0x41, 0x44};
const uintptr_t kCullZero = 0x00868FB7;  // xor eax,eax; add esp,38h; ret 4
const uintptr_t kCullOne = 0x00868FAC;   // mov eax,1; add esp,38h; ret 4
// The four ends of the test, for the compare: path 1, path 3, path 4, and
// the return 0 of the paths 0 and 2.
const uintptr_t kCullEnds[4] = {0x00868E0E, 0x00868F94, 0x00868FAC, 0x00868FB7};
const uint8_t kCullEndBytes[4][5] = {{0xB8, 0x01, 0x00, 0x00, 0x00},
                                     {0xB8, 0x01, 0x00, 0x00, 0x00},
                                     {0xB8, 0x01, 0x00, 0x00, 0x00},
                                     {0x33, 0xC0, 0x83, 0xC4, 0x38}};

const uintptr_t kDrawObjects = 0x005E5CD0;
const uint8_t kDrawObjectsBytes[6] = {0x81, 0xEC, 0x6C, 0x01, 0x00, 0x00};
const uintptr_t kListCount = 0x00D20198;
const uintptr_t kList = 0x00D1D28C;
const int kListMost = 2999;
// The code that the work before the draws knows.
const uintptr_t kPartMesh = 0x007F9E70;  // vtable+0xF8 of a part: mov eax,[ecx+7Ch]
const uintptr_t kTreeDraw = 0x0074AB00;  // vtable+0x610 of a tree
const uintptr_t kTreeScale = 0x00402520;  // vtable+0x120 of a tree: fld [ecx+50h]

const uintptr_t kTable0 = 0x00E3B5E0;  // LH3DP3::Table0, the clip flags
const uintptr_t kTable1 = 0x00E437E0;  // LH3DP3::Table1, the vertices
// Table0 has 0x8200 bytes before Table1.
const uint32_t kMaxVertices = 0x8200 / 4;
const uint32_t kLeast = 256;       // the least vertices of a part for a thread
const uint32_t kLeastObjects = 64;  // the least objects of a part for a thread
const int kMaxThreads = 7;

int g_mode = 1;     // land_objects
int g_threads = 0;  // land_objects_threads
int g_threads_on = 0;
bool g_test_keys = false;
bool g_in = false;
bool g_cull_in = false;
bool g_draw_in = false;
bool g_compare_in = false;
bool g_key_mode = false;
bool g_key_threads = false;
void* g_back = nullptr;
uintptr_t g_after = kAfter;
void* g_after_original = nullptr;
void* g_cull_back = nullptr;
uintptr_t g_cull_zero = kCullZero;
uintptr_t g_cull_one = kCullOne;
void* g_cull_end_original[4] = {};
typedef void(__cdecl* DrawObjectsFn)();
DrawObjectsFn o_DrawObjects = nullptr;

// For the report, on the frame thread.
uint64_t g_report_at = 0;
uint32_t g_meshes = 0;
uint32_t g_vertices = 0;
uint32_t g_most = 0;
uint32_t g_game_loops = 0;
uint32_t g_inexact = 0;
uint32_t g_shared = 0;  // meshes in parts on the threads
uint16_t g_last_control = 0;
uint32_t g_mxcsr_sets = 0;
uint32_t g_game_mxcsr = 0;
uint32_t g_odd = 0;     // the constants 1 and 0 of the game were not 1 and 0
int64_t g_ticks = 0;    // the clock in the SSE loops
uint32_t g_culls = 0;   // the tests of the view with SSE
uint32_t g_cull_hits = 0;  // of them, with the result of the work before the draws
uint32_t g_cull_game = 0;  // the tests of the view with the code of the game
uint32_t g_before = 0;     // the calls of the work before the draws
uint32_t g_before_objects = 0;
uint32_t g_before_faults = 0;
int64_t g_before_ticks = 0;
LARGE_INTEGER g_hz = {};

// THE COMPARE (land_objects=2).
uint8_t g_copy[kMaxVertices * lo::kOutStride];
uint32_t g_copy_flags[kMaxVertices];
uint32_t g_pending = 0;
uint32_t g_cmp_meshes = 0;
uint32_t g_cmp_vertices = 0;
uint32_t g_cmp_differ = 0;
uint32_t g_cmp_words[3] = {0, 0, 0};   // color, specular, clip flags
double g_cmp_most[8] = {};             // x, y, z, rhw, u, v, u2, v2
uint32_t g_cmp_nan = 0;
int g_cmp_lines = 0;
// The test of the view: the result of the SSE code that waits for its end.
bool g_cull_pending = false;
lo::CullOut g_cull_want;
uint32_t g_cull_radius_before = 0;
uint32_t g_cmp_culls = 0;
uint32_t g_cmp_cull_paths = 0;   // another end of the test
uint32_t g_cmp_cull_globals = 0;  // another value of a global
int g_cmp_cull_lines = 0;

template <typename T>
T Read(uintptr_t address) {
  T v;
  memcpy(&v, reinterpret_cast<const void*>(address), sizeof(T));
  return v;
}

template <typename T>
void Write(uintptr_t address, T v) {
  memcpy(reinterpret_cast<void*>(address), &v, sizeof(T));
}

int64_t Qpc() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

// The state of the units for the SSE code, as in fast_transform.cpp.
struct Units {
  uint32_t game_mxcsr = 0;
  uint32_t loop_mxcsr = 0;

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

// The values of the game for the loop. `frame` is ebp of fn_00852C40.
void Fill(const uint8_t* frame, lo::EnvConsts* c) {
  const uintptr_t matrix = Read<uint32_t>(0x00EA9EA0);
  memcpy(c->base.m, reinterpret_cast<const void*>(matrix), sizeof(c->base.m));
  c->base.near_plane = Read<float>(0x00E839E0);
  c->base.half_x = Read<float>(0x00E839F0);
  c->base.half_y = Read<float>(0x00E839F4);
  c->base.max_x = Read<float>(0x00C2AB00);
  c->base.max_y = Read<float>(0x00C2AB04);
  c->base.color = Read<uint32_t>(0x00C37D8C);
  c->base.specular = Read<uint32_t>(0x00E9FE2C);
  memcpy(c->light.direction, reinterpret_cast<const void*>(0x00F03140), 12);
  c->light.ambient = Read<int32_t>(0x00C39264);
  c->light.color = c->base.color;
  c->k255 = Read<float>(0x008AB270);
  memcpy(&c->a[0], frame - 0x58, 4);
  memcpy(&c->a[1], frame - 0x4C, 4);
  memcpy(&c->a[2], frame - 0x40, 4);
  memcpy(&c->b[0], frame - 0x54, 4);
  memcpy(&c->b[1], frame - 0x48, 4);
  memcpy(&c->b[2], frame - 0x3C, 4);
  c->ku = Read<float>(0x00C37EA4);
  c->kv = Read<float>(0x00C37EA8);
  c->half = Read<float>(0x009A3B98);
  const uintptr_t slot = Read<uint32_t>(0x00EA0284);
  c->ou = Read<float>(slot + 0x08);
  c->ov = Read<float>(slot + 0x0C);
  c->su = Read<float>(slot + 0x10);
  c->sv = Read<float>(slot + 0x14);
}

// The values of the game for the test of the view.
void FillCull(lo::CullGlobals* g) {
  memcpy(g->cam, reinterpret_cast<const void*>(0x00EA9E40), sizeof(g->cam));
  g->near_plane = Read<float>(0x00E839E0);
  g->width = Read<int32_t>(0x00E839E4);
  g->height = Read<int32_t>(0x00E839E8);
  g->half_x = Read<float>(0x00E839F0);
  g->half_y = Read<float>(0x00E839F4);
  memcpy(g->eye, reinterpret_cast<const void*>(0x00EA1DB8), sizeof(g->eye));
  g->pick[0] = Read<int32_t>(0x00EA1AC8);
  g->pick[1] = Read<int32_t>(0x00EA1ACC);
  g->k = Read<float>(0x008AA3B4);
  g->f = Read<float>(0x00C3812C);
}

void ReadCullIn(const uint8_t* bounds, const uint8_t* part, lo::CullIn* in) {
  memcpy(&in->bounds[0], bounds + 4, 12);
  memcpy(&in->bounds[3], bounds + 0x1C, 4);
  memcpy(in->part, part + 0x14, sizeof(in->part));
}

bool ConstantsOk() {
  return Read<float>(0x008AA390) == 1.0f && Read<uint32_t>(0x008AA398) == 0;
}

// THE WORKERS. A job is a function on the parts of a range.
typedef BOOL(WINAPI* WaitOnAddressFn)(volatile VOID*, PVOID, SIZE_T, DWORD);
typedef VOID(WINAPI* WakeByAddressAllFn)(PVOID);
WaitOnAddressFn g_wait = nullptr;
WakeByAddressAllFn g_wake = nullptr;

typedef void (*PartFn)(uint32_t first, uint32_t last);
struct Job {
  PartFn run = nullptr;
  uint32_t starts[kMaxThreads + 2] = {};
  int parts = 0;
  uint32_t mxcsr = 0;
};
Job g_job;
volatile LONG g_job_seq = 0;
volatile LONG g_job_left = 0;
int g_workers = 0;
// The number of the last job at the start of each worker: a new worker
// answers only the jobs after its start, which count it in g_job_left.
LONG g_seen_at_start[kMaxThreads + 1] = {};

DWORD WINAPI Worker(void* arg) {
  const int part = static_cast<int>(reinterpret_cast<intptr_t>(arg));
  LONG seen = g_seen_at_start[part];
  for (;;) {
    LONG now = g_job_seq;
    while (now == seen) {
      g_wait(&g_job_seq, &seen, sizeof(seen), INFINITE);
      now = g_job_seq;
    }
    seen = now;
    // The job is in memory before the new number (FEX can run with no TSO).
    _mm_mfence();
    if (part < g_job.parts) {
      _mm_setcsr(g_job.mxcsr);
      g_job.run(g_job.starts[part], g_job.starts[part + 1]);
    }
    // Each worker answers each job, also a job with no part for it. So the
    // frame thread starts no new job before each worker is past this one,
    // and no worker reads the fields of the next job for this one.
    InterlockedDecrement(&g_job_left);
  }
}

void StartWorkers(int n) {
  if (g_workers >= n) return;
  if (g_wait == nullptr) {
    HMODULE m = GetModuleHandleA("kernelbase.dll");
    if (m == nullptr) m = LoadLibraryA("api-ms-win-core-synch-l1-2-0.dll");
    if (m != nullptr) {
      g_wait = reinterpret_cast<WaitOnAddressFn>(GetProcAddress(m, "WaitOnAddress"));
      g_wake = reinterpret_cast<WakeByAddressAllFn>(GetProcAddress(m, "WakeByAddressAll"));
    }
    if (g_wait == nullptr || g_wake == nullptr) {
      g_wait = nullptr;
      g_threads_on = 0;
      g_threads = 0;
      ModLog("land objects : no WaitOnAddress, so each part runs on the frame thread");
      return;
    }
  }
  while (g_workers < n) {
    // Part 0 is the frame thread, so worker w makes part w + 1. No job runs
    // now: the workers start on the frame thread between the frames.
    g_seen_at_start[g_workers + 1] = g_job_seq;
    _mm_mfence();
    HANDLE h = CreateThread(nullptr, 64 * 1024, &Worker,
                            reinterpret_cast<void*>(static_cast<intptr_t>(g_workers + 1)), 0,
                            nullptr);
    if (h == nullptr) break;
    CloseHandle(h);
    ++g_workers;
  }
  ModLog("land objects : %d worker threads: the test of the view of the objects before their "
         "draws, and the SSE loop of a mesh with %u vertices or more",
         g_workers, 2 * kLeast);
}

// Runs `run` on the parts of `count` items, the frame thread and the workers,
// each part at least `least` items. Gives the count of parts.
int RunParts(PartFn run, uint32_t count, uint32_t least, uint32_t mxcsr) {
  int parts = 1;
  if (g_threads_on > 0 && g_workers > 0) {
    const int threads = (g_threads_on < g_workers ? g_threads_on : g_workers) + 1;
    parts = lo::Split(count, threads, least, g_job.starts);
  }
  if (parts <= 1) {
    run(0, count);
    return 1;
  }
  g_job.run = run;
  g_job.parts = parts;
  g_job.mxcsr = mxcsr;
  g_job_left = g_workers;
  _mm_mfence();
  InterlockedIncrement(&g_job_seq);
  g_wake(const_cast<LONG*>(&g_job_seq));
  run(g_job.starts[0], g_job.starts[1]);
  while (g_job_left > 0) _mm_pause();
  // The work of the workers is in memory before the frame thread reads it.
  _mm_mfence();
  g_job.parts = 0;
  return parts;
}

// THE MESH JOB.
const lo::EnvConsts* g_mesh_c = nullptr;
const uint8_t* g_mesh_in = nullptr;
uint8_t* g_mesh_out = nullptr;
uint32_t* g_mesh_flags = nullptr;

void MeshPart(uint32_t first, uint32_t last) {
  lo::EnvMesh(*g_mesh_c, g_mesh_in, first, last, g_mesh_out, g_mesh_flags);
}

void RunMesh(const lo::EnvConsts& c, const uint8_t* in, uint32_t count, uint8_t* out,
             uint32_t* flags, uint32_t mxcsr) {
  g_mesh_c = &c;
  g_mesh_in = in;
  g_mesh_out = out;
  g_mesh_flags = flags;
  if (RunParts(&MeshPart, count, kLeast, mxcsr) > 1) ++g_shared;
}

// THE WORK BEFORE THE DRAWS: the test of the view of each object of the list
// of fn_005E5CD0.
struct Before {
  uint32_t part = 0;  // 0: no result
  uint32_t bounds = 0;
  lo::CullIn in;
  lo::CullOut out;
};
Before g_before_list[kListMost];
const int kSlots = 8192;  // the table from a part to its entry
int16_t g_slot_entry[kSlots];
uint32_t g_slot_part[kSlots];
lo::CullGlobals g_before_globals;
bool g_before_on = false;
unsigned long g_frame_thread = 0;
volatile LONG g_faults = 0;

int SlotOf(uint32_t part) { return static_cast<int>((part * 2654435761u) >> 19) & (kSlots - 1); }

bool InTables(uint32_t a) { return ObjectTimerGameTables(a); }

void BeforeOne(int i) {
  Before& b = g_before_list[i];
  b.part = 0;
  const uint8_t* object = reinterpret_cast<const uint8_t*>(Read<uint32_t>(kList + 4 * i));
  if (object == nullptr || (object[10] & 1) != 0) return;
  const uint8_t* part = reinterpret_cast<const uint8_t*>(Read<uint32_t>(
      reinterpret_cast<uintptr_t>(object) + 0x40));
  if (part == nullptr) return;
  const uint32_t table = Read<uint32_t>(reinterpret_cast<uintptr_t>(part));
  if (!InTables(table) || Read<uint32_t>(table + 0xF8) != kPartMesh) return;
  const uint32_t mesh = Read<uint32_t>(reinterpret_cast<uintptr_t>(part) + 0x7C);
  if (mesh == 0) return;
  const uint8_t* bounds = reinterpret_cast<const uint8_t*>(mesh + 0x14);
  ReadCullIn(bounds, part, &b.in);
  // A tree scales the entries +0x20 and +0x28 of the matrix of its part for
  // its draw (when the bits 0x3C0 of +0x5C are 0): the scale of the tree
  // times a value of the two tables by the type of the tree.
  const uint32_t kind = Read<uint32_t>(reinterpret_cast<uintptr_t>(object));
  if (InTables(kind) && Read<uint32_t>(kind + 0x610) == kTreeDraw &&
      Read<uint32_t>(kind + 0x120) == kTreeScale &&
      (Read<uint16_t>(reinterpret_cast<uintptr_t>(object) + 0x5C) & 0x3C0) == 0) {
    const float scale = Read<float>(reinterpret_cast<uintptr_t>(object) + 0x50);
    const uint32_t at = object[0x5C] & 0x3C;
    b.in.part[3] = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(scale), _mm_set_ss(Read<float>(0x00DA3A0C + at))));
    b.in.part[5] = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(scale), _mm_set_ss(Read<float>(0x00DA39CC + at))));
  }
  b.out = lo::Cull(b.in, g_before_globals);
  b.bounds = mesh + 0x14;
  b.part = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(part));
}

void BeforePart(uint32_t first, uint32_t last) {
  for (uint32_t i = first; i < last; ++i) {
    __try {
      BeforeOne(static_cast<int>(i));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      g_before_list[i].part = 0;
      InterlockedIncrement(&g_faults);
    }
  }
}

void WorkBefore() {
  g_before_on = false;
  // With the SSE path of the test of a sphere (the flag 0x00EA9EB4 of a CPU
  // of the family 15 or of game_sse_path=1, or the bit of game_sse_parts)
  // the test of the view does not come to the stub, so a result before the
  // draws has no use.
  if (g_mode != 1 || !g_cull_in || g_threads_on <= 0 || g_workers <= 0 || !ConstantsOk() ||
      GameSseSphereFlag() != 0) {
    return;
  }
  int n = Read<int32_t>(kListCount);
  if (n <= 0) return;
  if (n > kListMost) n = kListMost;
  Units units;
  if (!units.Begin()) return;
  const int64_t t0 = Qpc();
  FillCull(&g_before_globals);
  RunParts(&BeforePart, static_cast<uint32_t>(n), kLeastObjects, units.loop_mxcsr);
  units.End();
  for (int s = 0; s < kSlots; ++s) g_slot_entry[s] = -1;
  for (int i = 0; i < n; ++i) {
    const uint32_t part = g_before_list[i].part;
    if (part == 0) continue;
    int s = SlotOf(part);
    while (g_slot_entry[s] >= 0 && g_slot_part[s] != part) s = (s + 1) & (kSlots - 1);
    g_slot_entry[s] = static_cast<int16_t>(i);
    g_slot_part[s] = part;
  }
  g_before_ticks += Qpc() - t0;
  ++g_before;
  g_before_objects += static_cast<uint32_t>(n);
  g_frame_thread = ThisThreadId();
  g_before_on = true;
}

void __cdecl h_DrawObjects() {
  WorkBefore();
  o_DrawObjects();
  g_before_on = false;
}

// The result of the work before the draws for this call, or null.
const lo::CullOut* FindBefore(const uint8_t* bounds, const uint8_t* part, const lo::CullIn& in,
                              const lo::CullGlobals& g) {
  if (!g_before_on || ThisThreadId() != g_frame_thread) return nullptr;
  const uint32_t p = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(part));
  int s = SlotOf(p);
  while (g_slot_entry[s] >= 0) {
    if (g_slot_part[s] == p) {
      const Before& b = g_before_list[g_slot_entry[s]];
      if (b.part != p || b.bounds != static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bounds)) ||
          memcmp(&b.in, &in, sizeof(in)) != 0 ||
          memcmp(&g_before_globals, &g, sizeof(g)) != 0) {
        return nullptr;
      }
      return &b.out;
    }
    s = (s + 1) & (kSlots - 1);
  }
  return nullptr;
}

typedef void(__fastcall* PartCallFn)(void* part, int edx);

// The test of the view. Gives 0 for the code of the game, 1 when the stub
// returns 0 from fn_00868C80, 2 when it returns 1.
extern "C" int __cdecl LandObjectsCull(const uint8_t* bounds, uint8_t* part) {
  if (g_mode == 0 || part == nullptr || !ConstantsOk()) {
    ++g_cull_game;
    return 0;
  }
  lo::CullIn in;
  ReadCullIn(bounds, part, &in);
  lo::CullGlobals g;
  FillCull(&g);
  lo::CullOut local;
  // A result of the work before the draws is used only when the x87 unit
  // has the precision and the rounding of the SSE code now too.
  uint16_t control = 0;
  __asm fnstcw control
  const bool exact = fast_transform_math::Exact(control, _mm_getcsr() & ~0xE040u);
  const lo::CullOut* out =
      g_mode == 1 && exact ? FindBefore(bounds, part, in, g) : nullptr;
  if (out != nullptr) {
    ++g_cull_hits;
  } else {
    Units units;
    if (!units.Begin()) {
      ++g_cull_game;
      return 0;
    }
    local = lo::Cull(in, g);
    units.End();
    out = &local;
  }
  if (g_mode == 2) {
    g_cull_want = *out;
    g_cull_radius_before = Read<uint32_t>(0x00C37EA0);
    g_cull_pending = true;
    ++g_cull_game;
    return 0;
  }
  ++g_culls;
  // The writes of the game, in its order.
  Write<int32_t>(0x00EA1AD0, 0);
  Write<int32_t>(0x00EA1AF0, 0);
  Write<float>(0x00EA1AF4, out->depth);
  const lo::CullOut result = *out;
  if (result.path == 2 || result.path >= 3) Write<float>(0x00C37EA0, result.radius);
  if (result.path >= 3) {
    Write<int32_t>(0x00EA1AF0, 1);
    Write<int32_t>(0x00EA1AD0, result.under);
  }
  const int edx = lo::CullEdx(result.path);
  if (edx >= 0) {
    const uint32_t table = Read<uint32_t>(reinterpret_cast<uintptr_t>(part));
    const PartCallFn call = reinterpret_cast<PartCallFn>(Read<uint32_t>(table + 0xA0));
    call(part, edx);
  }
  if (result.path == 1) {
    Write<int32_t>(0x00EA1AF0, 1);
    Write<int32_t>(0x00EA1AD0, 1);
    Write<float>(0x00C37EA0, result.radius);
  }
  return lo::CullReturn(result.path) != 0 ? 2 : 1;
}

// The end `end` of the test of the game (0: path 1, 1: path 3, 2: path 4, 3:
// return 0). Compares the result of the SSE code with the game.
extern "C" void __cdecl LandObjectsCullEnd(int end) {
  if (!g_cull_pending) return;
  g_cull_pending = false;
  ++g_cmp_culls;
  const lo::CullOut& w = g_cull_want;
  const int want_end = w.path == 1 ? 0 : w.path == 3 ? 1 : w.path == 4 ? 2 : 3;
  bool differ = false;
  if (want_end != end) {
    ++g_cmp_cull_paths;
    differ = true;
  }
  const uint32_t depth = Read<uint32_t>(0x00EA1AF4);
  const uint32_t radius = Read<uint32_t>(0x00C37EA0);
  uint32_t want_depth, want_radius;
  memcpy(&want_depth, &w.depth, 4);
  memcpy(&want_radius, &w.radius, 4);
  if (w.path == 0) want_radius = g_cull_radius_before;
  // Each end is after the last write of the game (path 1 writes
  // 0x00EA1AF0, 0x00EA1AD0 and 0x00C37EA0 at 0x00868DF4 to 0x00868E08).
  if (depth != want_depth || radius != want_radius ||
      Read<int32_t>(0x00EA1AD0) != w.under || Read<int32_t>(0x00EA1AF0) != w.in_view) {
    ++g_cmp_cull_globals;
    differ = true;
  }
  if (differ && g_cmp_cull_lines < 12) {
    ++g_cmp_cull_lines;
    ModLog("land objects : compare of the test of the view: the game end %d, 0x00EA1AF4 %08X, "
           "0x00C37EA0 %08X, 0x00EA1AD0 %d, 0x00EA1AF0 %d. SSE path %d (end %d), %08X, %08X, "
           "%d, %d",
           end, depth, radius, Read<int32_t>(0x00EA1AD0), Read<int32_t>(0x00EA1AF0), w.path,
           want_end, want_depth, want_radius, w.under, w.in_view);
  }
}

void Report() {
  const uint64_t now = GetTickCount64();
  if (g_report_at == 0) g_report_at = now + 10000;
  if (now < g_report_at) return;
  g_report_at = now + 10000;
  const double to_ms = g_hz.QuadPart > 0 ? 1000.0 / static_cast<double>(g_hz.QuadPart) : 0.0;
  if (g_mode == 2) {
    ModLog("land objects : land_objects=2, the compare in 10 s: %u meshes and %u vertices. "
           "Vertices with a bit that differs: %u. The largest difference of x %.6g, y %.6g, "
           "z %.6g, rhw %.6g, u %.6g, v %.6g, u2 %.6g, v2 %.6g. Words that differ: color %u, "
           "specular %u, clip flags %u. Floats that differ with a NaN: %u",
           g_cmp_meshes, g_cmp_vertices, g_cmp_differ, g_cmp_most[0], g_cmp_most[1],
           g_cmp_most[2], g_cmp_most[3], g_cmp_most[4], g_cmp_most[5], g_cmp_most[6],
           g_cmp_most[7], g_cmp_words[0], g_cmp_words[1], g_cmp_words[2], g_cmp_nan);
    ModLog("land objects : land_objects=2, the compare of the test of the view in 10 s: %u "
           "tests, %u with another end, %u with another value of a global",
           g_cmp_culls, g_cmp_cull_paths, g_cmp_cull_globals);
    g_cmp_meshes = g_cmp_vertices = g_cmp_differ = g_cmp_nan = 0;
    g_cmp_culls = g_cmp_cull_paths = g_cmp_cull_globals = 0;
    memset(g_cmp_words, 0, sizeof(g_cmp_words));
    memset(g_cmp_most, 0, sizeof(g_cmp_most));
  }
  ModLog("land objects : land_objects=%d, land_objects_threads=%d, in 10 s: %u meshes and %u "
         "vertices (most %u in a mesh) with SSE, %u of them in parts on the threads, %.2f ms "
         "in the SSE loops, %u meshes with the loop of the game. %u tests of the view with "
         "SSE (%u with the result of the work before the draws), %u with the code of the "
         "game. The work before the draws: %u times, %u objects, %.2f ms on the frame thread, "
         "%ld faults",
         g_mode, g_threads_on, g_meshes, g_vertices, g_most, g_shared,
         static_cast<double>(g_ticks) * to_ms, g_game_loops, g_culls, g_cull_hits, g_cull_game,
         g_before, g_before_objects,
         static_cast<double>(g_before_ticks) * to_ms, g_faults);
  if (g_inexact > 0) {
    ModLog("land objects : %u times the code of the game ran, because the x87 control word "
           "was 0x%04X (the SSE code needs a precision of 24 bits and rounding to the nearest)",
           g_inexact, g_last_control);
  }
  if (g_odd > 0) {
    ModLog("land objects : %u meshes went to the loop of the game, because the constants 1 "
           "and 0 of the game at 0x008AA390 and 0x008AA398 were not 1 and 0",
           g_odd);
  }
  if (g_mxcsr_sets > 0) {
    ModLog("land objects : MXCSR of the game was 0x%08X. For %u runs of SSE code the mod set "
           "it to round to the nearest with no flush to zero, and put it back after each run",
           g_game_mxcsr, g_mxcsr_sets);
  }
  g_meshes = g_vertices = g_most = g_game_loops = g_inexact = g_mxcsr_sets = g_shared = 0;
  g_odd = g_culls = g_cull_hits = g_cull_game = g_before = g_before_objects = 0;
  g_ticks = g_before_ticks = 0;
  g_faults = 0;
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
  ++g_cmp_meshes;
  g_cmp_vertices += n;
  const uint8_t* table1 = reinterpret_cast<const uint8_t*>(kTable1);
  const uint32_t* table0 = reinterpret_cast<const uint32_t*>(kTable0);
  // The float fields by word: x, y, z, rhw, -, -, u, v, u2, v2.
  static const int kField[10] = {0, 1, 2, 3, -1, -1, 4, 5, 6, 7};
  for (uint32_t i = 0; i < n; ++i) {
    uint32_t a[10], b[10];
    memcpy(a, table1 + i * lo::kOutStride, lo::kOutStride);
    memcpy(b, g_copy + i * lo::kOutStride, lo::kOutStride);
    const bool flags_differ = table0[i] != g_copy_flags[i];
    if (memcmp(a, b, lo::kOutStride) == 0 && !flags_differ) continue;
    ++g_cmp_differ;
    for (int w = 0; w < 10; ++w) {
      if (kField[w] >= 0) CompareFloat(kField[w], a[w], b[w]);
    }
    if (a[4] != b[4]) ++g_cmp_words[0];
    if (a[5] != b[5]) ++g_cmp_words[1];
    if (flags_differ) ++g_cmp_words[2];
    if (g_cmp_lines < 12) {
      ++g_cmp_lines;
      ModLog("land objects : compare, vertex %u of %u: the game %08X %08X %08X %08X %08X "
             "%08X %08X %08X %08X %08X flags %X, SSE %08X %08X %08X %08X %08X %08X %08X %08X "
             "%08X %08X flags %X",
             i, n, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], table0[i], b[0],
             b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], g_copy_flags[i]);
    }
  }
}

extern "C" void __cdecl LandObjectsCompare() { ComparePending(); }

// A stub for a hook of the compare: pushad; [push end;] call fn; [add esp,4;]
// popad; jmp dword ptr [original]. `end` < 0: no argument.
uint8_t* MakeCompareStub(void* fn, int end, void** original) {
  uint8_t* at = static_cast<uint8_t*>(
      VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
  if (at == nullptr) return nullptr;
  int i = 0;
  at[i++] = 0x60;  // pushad
  if (end >= 0) {
    at[i++] = 0x6A;  // push end
    at[i++] = static_cast<uint8_t>(end);
  }
  at[i++] = 0xE8;  // call fn
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<intptr_t>(fn) -
                                           reinterpret_cast<intptr_t>(at + i + 4));
  memcpy(at + i, &rel, 4);
  i += 4;
  if (end >= 0) {
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

bool HookCompare(uintptr_t address, void* fn, int end, void** original) {
  uint8_t* stub = MakeCompareStub(fn, end, original);
  void* target = reinterpret_cast<void*>(address);
  return stub != nullptr && MH_CreateHook(target, stub, original) == MH_OK &&
         MH_QueueEnableHook(target) == MH_OK;
}

// The vertex loop of fn_00852C40. `frame` is ebp of fn_00852C40. Gives 1
// when it did the work.
extern "C" int __cdecl LandObjectsLoop(uint8_t* frame) {
  // A copy that the hook after the loop did not compare is old.
  g_pending = 0;
  if (g_mode == 0) {
    ++g_game_loops;
    return 0;
  }
  const uintptr_t mesh = Read<uint32_t>(reinterpret_cast<uintptr_t>(frame - 0x1C));
  const int32_t count = Read<int32_t>(mesh + 0x10);
  const uint8_t* in = reinterpret_cast<const uint8_t*>(Read<uint32_t>(mesh + 0x14));
  if (count <= 0 || static_cast<uint32_t>(count) > kMaxVertices || in == nullptr) {
    ++g_game_loops;
    return 0;
  }
  if (!ConstantsOk()) {
    ++g_odd;
    ++g_game_loops;
    return 0;
  }
  Units units;
  if (!units.Begin()) {
    ++g_game_loops;
    return 0;
  }
  const int64_t t0 = Qpc();
  lo::EnvConsts c;
  Fill(frame, &c);
  const uint32_t n = static_cast<uint32_t>(count);
  if (g_mode == 2) {
    // The bytes that the loop does not write are as in the table now.
    memcpy(g_copy, reinterpret_cast<const void*>(kTable1), n * lo::kOutStride);
    RunMesh(c, in, n, g_copy, g_copy_flags, units.loop_mxcsr);
    g_pending = n;
    units.End();
    g_ticks += Qpc() - t0;
    ++g_game_loops;
    return 0;
  }
  RunMesh(c, in, n, reinterpret_cast<uint8_t*>(kTable1), reinterpret_cast<uint32_t*>(kTable0),
          units.loop_mxcsr);
  units.End();
  g_ticks += Qpc() - t0;
  ++g_meshes;
  g_vertices += n;
  if (n > g_most) g_most = n;
  return 1;
}

// The stub of the loop. popad does not change the flags, so the test of the
// result decides the jump after it.
__declspec(naked) void LoopStub() {
  __asm {
    pushad
    push ebp
    call LandObjectsLoop
    add esp, 4
    test eax, eax
    popad
    jz loop_game
    jmp dword ptr [g_after]
  loop_game:
    jmp dword ptr [g_back]
  }
}

// The stub of the test of the view: the bounds in eax, the part at
// [esp+3Ch]. After pushad the part is at [esp+5Ch].
__declspec(naked) void CullStub() {
  __asm {
    pushad
    mov ecx, [esp + 0x5C]
    push ecx
    push eax
    call LandObjectsCull
    add esp, 8
    cmp eax, 1
    popad
    jb cull_game
    je cull_zero
    jmp dword ptr [g_cull_one]
  cull_zero:
    jmp dword ptr [g_cull_zero]
  cull_game:
    jmp dword ptr [g_cull_back]
  }
}

bool Expect(uintptr_t address, const uint8_t* bytes, size_t size) {
  const void* site = reinterpret_cast<const void*>(address);
  return !IsBadReadPtr(site, size) && memcmp(site, bytes, size) == 0;
}

const char* ModeText(int mode) {
  static const char* const kText[3] = {
      "The vertex loop of the reflection meshes (fn_00852C40) and the test of the view of "
      "the 3D parts (fn_00868C80) run on the x87 unit",
      "The vertex loop of the reflection meshes (fn_00852C40) and the test of the view of "
      "the 3D parts (fn_00868C80) run with SSE, with the bits of the game",
      "A test aid: the vertex loop of the reflection meshes and the test of the view run on "
      "the x87 unit, and the mod compares the SSE code with them"};
  return kText[mode < 0 || mode > 2 ? 0 : mode];
}

}  // namespace

void LandObjectsSetOptions(int mode, int threads, bool test_keys) {
  g_mode = mode < 0 || mode > 2 ? 1 : mode;
  g_threads = threads < 0 ? 0 : (threads > kMaxThreads ? kMaxThreads : threads);
  g_threads_on = g_threads;
  g_test_keys = test_keys;
  QueryPerformanceFrequency(&g_hz);
  ModLog("land objects : land_objects=%d. %s. land_objects_threads=%d%s", g_mode,
         ModeText(g_mode), g_threads,
         g_threads == 0 ? ": one thread" : " worker threads");
}

void LandObjectsInstall() {
  if (g_in || g_cull_in || (g_mode == 0 && !g_test_keys)) return;
  ObjectTimerReadSections();
  const bool compare = g_mode == 2 || g_test_keys;
  // Each hook is queued, and one MH_ApplyQueued writes them, with one stop
  // of the other threads.
  // The vertex loop of fn_00852C40.
  bool loop = false;
  if (Expect(kLoop, kLoopBytes, sizeof(kLoopBytes)) &&
      Expect(kAfter, kAfterBytes, sizeof(kAfterBytes))) {
    void* at = reinterpret_cast<void*>(kLoop);
    loop = MH_CreateHook(at, reinterpret_cast<void*>(&LoopStub), &g_back) == MH_OK &&
           MH_QueueEnableHook(at) == MH_OK;
    if (loop && compare) {
      loop = HookCompare(kAfter, reinterpret_cast<void*>(&LandObjectsCompare), -1,
                         &g_after_original);
    }
    if (!loop) {
      MH_RemoveHook(at);
      MH_RemoveHook(reinterpret_cast<void*>(kAfter));
    }
  } else {
    ModLog("land objects : the code at 0x%08X or 0x%08X is not the code of the vertex loop of "
           "fn_00852C40, so the loop of the game runs",
           static_cast<unsigned>(kLoop), static_cast<unsigned>(kAfter));
  }
  // The test of the view, fn_00868C80.
  bool cull = false;
  bool ends_ok = true;
  for (int e = 0; e < 4; ++e) ends_ok = ends_ok && Expect(kCullEnds[e], kCullEndBytes[e], 5);
  if (Expect(kCullPath, kCullPathBytes, sizeof(kCullPathBytes)) && ends_ok) {
    void* at = reinterpret_cast<void*>(kCullPath);
    cull = MH_CreateHook(at, reinterpret_cast<void*>(&CullStub), &g_cull_back) == MH_OK &&
           MH_QueueEnableHook(at) == MH_OK;
    for (int e = 0; e < 4 && compare && cull; ++e) {
      cull = HookCompare(kCullEnds[e], reinterpret_cast<void*>(&LandObjectsCullEnd), e,
                         &g_cull_end_original[e]);
    }
    if (!cull) {
      MH_RemoveHook(at);
      for (int e = 0; e < 4; ++e) MH_RemoveHook(reinterpret_cast<void*>(kCullEnds[e]));
    }
  } else {
    ModLog("land objects : the code at 0x%08X is not the code of the test of the view of "
           "fn_00868C80, so the test of the game runs",
           static_cast<unsigned>(kCullPath));
  }
  // The work before the draws, at the start of fn_005E5CD0.
  bool draw = false;
  if (cull && (g_threads > 0 || g_test_keys)) {
    void* at = reinterpret_cast<void*>(kDrawObjects);
    if (Expect(kDrawObjects, kDrawObjectsBytes, sizeof(kDrawObjectsBytes))) {
      draw = MH_CreateHook(at, reinterpret_cast<void*>(&h_DrawObjects),
                           reinterpret_cast<void**>(&o_DrawObjects)) == MH_OK &&
             MH_QueueEnableHook(at) == MH_OK;
      if (!draw) MH_RemoveHook(at);
    }
  }
  const MH_STATUS applied = MH_ApplyQueued();
  g_in = loop && applied == MH_OK;
  g_cull_in = cull && applied == MH_OK;
  g_draw_in = draw && applied == MH_OK;
  // With the hooks of the ends of fn_00868C80 the stub of the test goes to
  // their trampolines, so that a test with SSE does not count as a compare.
  if (g_cull_in && compare) {
    g_cull_one = reinterpret_cast<uintptr_t>(g_cull_end_original[2]);
    g_cull_zero = reinterpret_cast<uintptr_t>(g_cull_end_original[3]);
  }
  g_compare_in = compare && (g_in || g_cull_in);
  ModLog("hooks        : the vertex loop of the reflection meshes at 0x%08X %s. The SSE loop "
         "is LandObjectsLoop at 0x%08X",
         static_cast<unsigned>(kLoop), g_in ? "ok" : "FAILED",
         static_cast<unsigned>(reinterpret_cast<uintptr_t>(&LandObjectsLoop)));
  ModLog("hooks        : the test of the view of the 3D parts at 0x%08X %s. The SSE test is "
         "LandObjectsCull at 0x%08X",
         static_cast<unsigned>(kCullPath), g_cull_in ? "ok" : "FAILED",
         static_cast<unsigned>(reinterpret_cast<uintptr_t>(&LandObjectsCull)));
  if (cull && (g_threads > 0 || g_test_keys)) {
    ModLog("hooks        : the start of fn_005E5CD0 (the work before the draws of the "
           "objects) at 0x%08X %s",
           static_cast<unsigned>(kDrawObjects), g_draw_in ? "ok" : "FAILED");
  }
}

void LandObjectsFrame() {
  if (!g_in && !g_cull_in) return;
  Report();
  // The workers start on the frame thread, in the play.
  if (g_threads_on > 0 && g_workers < g_threads_on) StartWorkers(g_threads_on);
  if (!g_test_keys) return;
  const bool ctrl_shift = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                          (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
  const bool mode_down = ctrl_shift && (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
  if (mode_down && !g_key_mode) {
    // 1, 0, 2, 1 ...
    g_pending = 0;
    g_cull_pending = false;
    g_mode = g_mode == 1 ? 0 : (g_mode == 0 ? 2 : 1);
    ModLog("land objects : Ctrl Shift F8: land_objects=%d. %s", g_mode, ModeText(g_mode));
  }
  g_key_mode = mode_down;
  const bool threads_down = ctrl_shift && (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
  if (threads_down && !g_key_threads) {
    const int base = g_threads > 0 ? g_threads : 2;
    g_threads_on = g_threads_on > 0 ? 0 : base;
    ModLog("land objects : Ctrl Shift F9: land_objects_threads=%d", g_threads_on);
  }
  g_key_threads = threads_down;
}
