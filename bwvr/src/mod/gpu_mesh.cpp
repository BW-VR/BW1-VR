// The meshes of the 3D parts on the GPU (docs/software-tnl.md, "The meshes on
// the GPU"). The math is in gpu_mesh_math.h, the shader in vk_mesh.vert.
//
// WHERE. A 3D part draws its meshes through five functions of the engine,
// each one mesh:
//
//   fn_00856D40  unlit, with clip flags
//   fn_0085AB10  lit by the normal of each vertex, no clip flags
//   fn_0084BA90  lit, with clip flags
//   fn_0085BA30  unlit, no clip flags
//   fn_00859530  lit, with clip flags, with an offset for each vertex (a
//                person or an animal)
//
// Each one transforms every vertex on the CPU, tests the back face of every
// triangle, clips the triangles that leave the view, runs the material of the
// mesh, and calls LH3DRender::DrawTriangle. Under FEX this is x87 code for
// each vertex and each triangle.
//
// WHAT THE HOOK DOES. A hook at the start of each function (a hook of
// MinHook). When the mesh can take the GPU path, the hook does what the
// function does after the loops, and not the loops:
//
//   1. the material (the same calls as the function: the function of the
//      material type, the tiling, the cull mode),
//   2. a look at the state of the device: no blend, a depth write, the depth
//      test and no texture on stage 1. A mesh with blend sorts by depth, so
//      it keeps the CPU path, and the function of the game runs from its
//      start (it runs the material a second time, which changes nothing),
//   3. the data of the mesh go to the renderer one time (a sum over the
//      words checks that the game did not change them),
//   4. one record for the draw: the matrix of the part and the camera, the
//      projection, the colors and the light.
//
// Else the function of the game runs as it is, and the loops of
// fast_transform.cpp and part_draw.cpp are still in it.
//
// WHICH MESH TAKES THE CPU PATH. A mesh in a group (a matrix for each group
// of vertices, the flag of the mesh group), a mesh with a texture shift
// (water, fire: DrawTriangle adds a shift to u and v), a mesh with a
// texture on stage 1, a mesh with blend or with no depth write, and a draw
// that h_DrawTriangle changes (the glow of a target, an advisor of a cut
// scene, the box of a speech bubble).
//
// THE COMPARE (gpu_mesh=2). Each mesh that could take the GPU path is drawn
// two times: the function of the game draws it in the left eye layer, and the
// GPU path draws it in the right eye layer. The renderer does not move the
// right eye in this mode (VkSceneSetMesh), so a test frame gives the
// difference of the two layers.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"
#include "debug_menu.h"
#include "gpu_mesh.h"
#include "gpu_mesh_math.h"
#include "one_walk.h"
#include "vk_ddraw.h"
#include "vk_scene.h"
#include "vk_stream.h"

void ModLog(const char* format, ...);
void ControllersGesturePulse(float amplitude, float seconds);
// draw_hooks.cpp
void DrawHooksMeshBatch(float rhw);
bool DrawHooksTriangleSpecial(uint32_t specular);

namespace {

namespace gm = gpu_mesh_math;

// The five functions.
enum Kind { kClip = 0, kLit = 1, kLitClip = 2, kFlat = 3, kOffsets = 4, kKinds = 5 };
struct Site {
  uintptr_t address;
  uint8_t bytes[5];
};
const Site kSites[kKinds] = {
    {0x00856D40, {0x83, 0xEC, 0x1C, 0xA1, 0xBC}},
    {0x0085AB10, {0x55, 0x8B, 0xEC, 0x83, 0xEC}},
    {0x0084BA90, {0x55, 0x8B, 0xEC, 0x83, 0xEC}},
    {0x0085BA30, {0xA1, 0xBC, 0x93, 0xFA, 0x00}},
    {0x00859530, {0x83, 0xEC, 0x20, 0x53, 0x55}},
};
const char* const kKindName[kKinds] = {"fn_00856D40", "fn_0085AB10", "fn_0084BA90",
                                       "fn_0085BA30", "fn_00859530"};

// The globals of the game.
const uintptr_t kDevice = 0x00ECA638;          // LH3DRender::Direct3DDevice7
const uintptr_t kCurrentMaterial = 0x00ECA64C;
const uintptr_t kMaterialTable = 0x00ECA618;   // g_set_render_mode_data
const uintptr_t kNeedTilling = 0x00ECA614;
const uintptr_t kCullCache = 0x00ECA248;
const uintptr_t kDoUpdateMaterial = 0x00C38714;
const uintptr_t kUvShift = 0x00ECA628;         // the part shifts u and v
const uintptr_t kClampOn = 0x00FA9398;         // DrawTriangle clamps the screen
const uintptr_t kMeshCounter = 0x00F05184;
const uintptr_t kCurrentMesh = 0x00F0517C;
const uintptr_t kGroup = 0x00FA93BC;           // the mesh group: flags first
const uintptr_t kPart = 0x00EA1AE8;            // the 3D part being drawn
const uintptr_t kOffsetPointer = 0x00F05180;
const uintptr_t kMatrix = 0x00EA9EA0;          // pointer to the 12 floats
const uintptr_t kNearPlane = 0x00E839E0;
const uintptr_t kHalfX = 0x00E839F0;
const uintptr_t kHalfY = 0x00E839F4;
const uintptr_t kColor = 0x00C37D8C;
const uintptr_t kSpecular = 0x00E9FE2C;
const uintptr_t kLightDirection = 0x00F03140;
const uintptr_t kAmbient = 0x00C39264;
const uintptr_t kTilingOff = 0x0082FF50;
const uintptr_t kTilingOn = 0x0082FF10;

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

int g_mode = 0;  // gpu_mesh
int g_compare_every = 0;  // gpu_mesh_compare_every: frames between two samples of the compare
uint32_t g_kinds = 31;    // gpu_mesh_kinds: bit k of the functions on the GPU path
uint32_t g_base_kinds = 31;
uint64_t g_frames = 0;
int g_sample = 0;
int g_twin_lines[kKinds + 1] = {};
bool g_test_keys = false;
bool g_in = false;
bool g_key_down = false;
bool g_said_backend = false;
uint64_t g_report_at = 0;

// For the report.
struct Counts {
  uint32_t gpu[kKinds] = {};
  uint32_t cpu[kKinds] = {};
  uint32_t why_pre = 0;     // the checks before the material
  uint32_t why_special = 0; // h_DrawTriangle would change the batch
  uint32_t why_state = 0;   // blend, no depth write, stage 1
  uint32_t why_table = 0;   // the table of meshes is full
  uint32_t why_kind = 0;    // gpu_mesh_kinds leaves the function out
  uint32_t data_sent = 0;
  uint64_t data_bytes = 0;
  uint64_t gpu_ticks = 0;   // the time of the hook for the GPU path
  uint64_t cpu_ticks = 0;   // the time of the function of the game
  uint32_t vertices = 0, triangles = 0, offsets = 0;
};
Counts g_counts;
uint64_t g_window_tsc = 0, g_window_qpc = 0;

// THE TABLE OF THE MESHES: what the renderer has, by the address of the
// mesh. A mesh that the game made again at the same address has other words
// and gets a new record of data.
struct Known {
  uintptr_t mesh;
  const void* vertices;
  const void* indices;
  uint64_t hash;
  uint32_t vertex_count, triangles;
  uint32_t epoch;
};
const uint32_t kTableSize = 1u << 15;
Known* g_table = nullptr;

Known* FindKnown(uintptr_t mesh) {
  if (g_table == nullptr) {
    g_table = static_cast<Known*>(VirtualAlloc(nullptr, sizeof(Known) * kTableSize,
                                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (g_table == nullptr) return nullptr;
  }
  uint32_t at = static_cast<uint32_t>((mesh >> 3) * 2654435761u) >> 17;
  for (uint32_t probe = 0; probe < 64; ++probe, at = (at + 1) & (kTableSize - 1)) {
    Known* k = &g_table[at];
    if (k->mesh == mesh || k->mesh == 0) return k;
  }
  return nullptr;
}

typedef void(__fastcall* MeshFunction)(void* mesh, void* edx);
typedef void(__fastcall* MaterialFunction)(void* mesh, uint32_t edx);
typedef void(__cdecl* TilingFunction)(int on);
typedef long(__stdcall* SetRenderStateFunction)(void* device, uint32_t state, uint32_t value);
typedef uint32_t(__fastcall* OffsetKindFunction)(void* part, void* edx);

MeshFunction g_back[kKinds] = {};

// The color of a lit mesh: the color of the part times the color of the mesh
// when the mesh has no color of its own (fn_007ACF70).
uint32_t MeshColor(const uint8_t* mesh, uint32_t color) {
  if (Read<int32_t>(reinterpret_cast<uintptr_t>(mesh) + 8) != 0) return color;
  auto channel = [&](int k, uint32_t value) -> uint32_t {
    return (static_cast<uint32_t>(mesh[0x0C + k]) * value) / 0xFFu;
  };
  return (channel(0, color & 0xFF) & 0xFF) | (channel(1, (color >> 8) & 0xFF) & 0xFF) << 8 |
         (channel(2, (color >> 16) & 0xFF) & 0xFF) << 16 | channel(3, color >> 24) << 24;
}

// What the function does after the loops, for a mesh that is drawn: the
// material, the tiling and the cull mode.
void RunMaterial(uint8_t* mesh) {
  if (Read<uint32_t>(kDoUpdateMaterial) == 0) return;
  Write<void*>(kCurrentMaterial, mesh);
  const uintptr_t table = Read<uintptr_t>(kMaterialTable);
  const uint32_t type = Read<uint32_t>(reinterpret_cast<uintptr_t>(mesh));
  const MaterialFunction material =
      Read<MaterialFunction>(table + static_cast<uintptr_t>(type) * 8);
  material(mesh, 0);
  if (Read<uint32_t>(kNeedTilling) == 0 && (mesh[5] & 4) == 0) {
    reinterpret_cast<TilingFunction>(kTilingOff)(0);
  } else {
    reinterpret_cast<TilingFunction>(kTilingOn)(0);
  }
  const uint32_t cull = ((~static_cast<uint32_t>(mesh[5]) & 1u) << 1) | 1u;
  if (Read<uint32_t>(kCullCache) != cull) {
    void* device = Read<void*>(kDevice);
    const uintptr_t vtable = Read<uintptr_t>(reinterpret_cast<uintptr_t>(device));
    const long result =
        Read<SetRenderStateFunction>(vtable + 0x50)(device, 0x16, cull);
    Write<uint32_t>(kCullCache, 0xFFFFFFFFu);
    if (result == 0) Write<uint32_t>(kCullCache, cull);
  }
}

// The offsets of fn_00859530 as 3 floats for each vertex.
float g_expanded[3 * gm::kMostVertices];

// The values of one draw that Prepare made.
struct Prepared {
  gm::Consts c;
  const float* offsets;   // 3 floats for each vertex, or null
  uint32_t offset_floats;
  const float* source;    // the offsets of the game, for the pointer after the draw
  uint32_t offset_step;   // floats of the game for each vertex: 1 or 3
  uint32_t vertex_count, triangles;
  float first_field;      // for DrawHooksMeshBatch
  uint32_t zfunc, cull, zwrite;  // the device now, for the log of the compare
};

// The checks, the material, the state of the device and the data of the mesh.
// True when the mesh takes the GPU path. False: nothing is drawn, and the
// function of the game must run.
bool Prepare(Kind kind, uint8_t* mesh, Prepared* out) {
  const uintptr_t m = reinterpret_cast<uintptr_t>(mesh);
  // The checks that need no material.
  const int32_t vertex_count = Read<int32_t>(m + 0x10);
  const int32_t triangles = Read<int32_t>(m + 0x18);
  const uint8_t* vertices = Read<const uint8_t*>(m + 0x14);
  const uint16_t* indices = Read<const uint16_t*>(m + 0x1C);
  const float* matrix = Read<const float*>(kMatrix);
  if (vertex_count <= 0 || vertex_count > static_cast<int32_t>(gm::kMostVertices) ||
      triangles <= 0 || triangles > static_cast<int32_t>(gm::kMostTriangles) ||
      vertices == nullptr || indices == nullptr || matrix == nullptr ||
      Read<uint32_t>(kClampOn) != 0 ||
      (Read<uint32_t>(kUvShift) != 0 && (mesh[5] & 0x10) == 0)) {
    ++g_counts.why_pre;
    return false;
  }
  if (kind != kOffsets) {
    const uintptr_t group = Read<uintptr_t>(kGroup);
    if (group == 0 || (Read<uint32_t>(group) & 1u) != 0) {
      ++g_counts.why_pre;
      return false;
    }
  }
  // The values of the draw that the loop of the game reads: they are read
  // before the material, as the loop reads them before it.
  const uint32_t specular = Read<uint32_t>(kSpecular);
  const uint32_t color = Read<uint32_t>(kColor);
  if ((g_kinds & (1u << kind)) == 0) {
    ++g_counts.why_kind;
    return false;
  }
  if (DrawHooksTriangleSpecial(specular)) {
    ++g_counts.why_special;
    return false;
  }
  const uint32_t n = static_cast<uint32_t>(vertex_count);
  float* offsets = nullptr;
  uint32_t offset_kind = 0;  // 0 none, 1 one offset for y, 2 three offsets
  if (kind == kOffsets) {
    const uintptr_t part = Read<uintptr_t>(kPart);
    offsets = Read<float*>(kOffsetPointer);
    if (part == 0 || offsets == nullptr) {
      ++g_counts.why_pre;
      return false;
    }
    const uintptr_t vtable = Read<uintptr_t>(part);
    offset_kind =
        Read<OffsetKindFunction>(vtable + 0x1B0)(reinterpret_cast<void*>(part), nullptr) != 0 ? 2u
                                                                                              : 1u;
  }
  Known* known = FindKnown(m);
  if (known == nullptr) {
    ++g_counts.why_table;
    return false;
  }

  // The material, and the state of the device that it left.
  Write<void*>(kCurrentMesh, mesh);
  RunMaterial(mesh);
  gm::DeviceState state = {};
  if (!VkDdrawMeshState(Read<void*>(kDevice), &state) || !gm::OpaqueState(state)) {
    ++g_counts.why_state;
    return false;
  }
  out->zfunc = state.z_func;
  out->cull = state.cull;
  out->zwrite = state.z_write;

  // The values of the draw.
  gm::Consts& c = out->c;
  c = gm::Consts();
  gm::SetMatrix(&c, matrix);
  c.near_plane = Read<float>(kNearPlane);
  c.half_x = Read<float>(kHalfX);
  c.half_y = Read<float>(kHalfY);
  c.specular = specular;
  const bool lit = kind == kLit || kind == kLitClip || kind == kOffsets;
  c.color = (kind == kLit || kind == kLitClip) ? MeshColor(mesh, color) : color;
  if (lit) {
    c.flags |= gm::kLit;
    if (kind == kOffsets) c.flags |= gm::kTruncate;
    memcpy(c.light, reinterpret_cast<const void*>(kLightDirection), 12);
    c.ambient = Read<int32_t>(kAmbient);
  }
  out->offsets = nullptr;
  out->offset_floats = 0;
  out->source = offsets;
  out->offset_step = offset_kind == 2 ? 3u : 1u;
  if (kind == kOffsets) {
    if (offset_kind == 1) {
      for (uint32_t i = 0; i < n; ++i) {
        g_expanded[3 * i] = 0.0f;
        g_expanded[3 * i + 1] = offsets[i];
        g_expanded[3 * i + 2] = 0.0f;
      }
      out->offsets = g_expanded;
    } else {
      out->offsets = offsets;
    }
    out->offset_floats = 3 * n;
  }
  out->vertex_count = n;
  out->triangles = static_cast<uint32_t>(triangles);

  // The data of the mesh, one time.
  const uint64_t hash = gm::MeshHash(vertices, n, indices, out->triangles);
  const uint32_t epoch = VkStreamMeshEpoch();
  if (known->mesh != m || known->hash != hash || known->epoch != epoch ||
      known->vertex_count != n || known->triangles != out->triangles ||
      known->vertices != vertices || known->indices != indices) {
    VkSceneMeshData(m, hash, vertices, n, indices, out->triangles);
    known->mesh = m;
    known->hash = hash;
    known->epoch = epoch;
    known->vertex_count = n;
    known->triangles = out->triangles;
    known->vertices = vertices;
    known->indices = indices;
    ++g_counts.data_sent;
    g_counts.data_bytes += static_cast<uint64_t>(n) * 32 + static_cast<uint64_t>(triangles) * 6;
  }

  // The first vertex of the batch, for the measure of the nearest geometry
  // (DrawHooksMeshBatch): the rhw of the vertex, or its camera z when the game
  // clips it.
  const gm::Camera cam =
      gm::CameraSpace(c, reinterpret_cast<const float*>(vertices), out->offsets);
  const bool clipped = kind != kLit && kind != kFlat &&
                       !(cam.z >= c.near_plane && cam.x <= cam.z && cam.x >= -cam.z &&
                         cam.y <= cam.z && cam.y >= -cam.z);
  out->first_field = clipped ? cam.z : (cam.z != 0.0f ? c.near_plane * (1.0f / cam.z) : 0.0f);
  return true;
}

// The draw of a mesh that Prepare made. effects: what the function of the game
// does after its draw (the counter of the meshes, the pointer of the offsets)
// and h_DrawTriangle does for the batch.
void Submit(Kind kind, uint8_t* mesh, const Prepared& p, bool effects) {
  const uintptr_t m = reinterpret_cast<uintptr_t>(mesh);
  if (effects) DrawHooksMeshBatch(p.first_field);
  VkSceneDrawMesh(m, p.c, p.offsets, p.offset_floats);
  ++g_counts.gpu[kind];
  g_counts.vertices += p.vertex_count;
  g_counts.triangles += p.triangles;
  g_counts.offsets += p.offsets != nullptr ? p.vertex_count : 0;
  if (!effects) return;
  if (kind == kClip || kind == kLitClip) {
    Write<uint32_t>(kMeshCounter, Read<uint32_t>(kMeshCounter) + 1);
  }
  if (kind == kOffsets) {
    // The game moves the pointer past the offsets of the mesh.
    Write<const float*>(kOffsetPointer,
                        p.source + static_cast<size_t>(p.offset_step) * p.vertex_count);
  }
}

void Hook(Kind kind, void* mesh_in, void* edx) {
  uint8_t* mesh = static_cast<uint8_t*>(mesh_in);
  if (g_mode == 0 || !VkDdrawOn() || !VkSceneDirect()) {
    if (g_mode != 0 && !g_said_backend) {
      g_said_backend = true;
      ModLog("gpu mesh     : gpu_mesh=%d needs backend=vulkan, so the meshes take the CPU "
             "path", g_mode);
    }
    g_back[kind](mesh, edx);
    return;
  }
  const uint64_t start = __rdtsc();
  Prepared p;
  if (!Prepare(kind, mesh, &p)) {
    const uint64_t cpu_start = __rdtsc();
    g_back[kind](mesh, edx);
    ++g_counts.cpu[kind];
    g_counts.cpu_ticks += __rdtsc() - cpu_start;
    g_counts.gpu_ticks += cpu_start - start;  // the checks and the material of this mesh
    return;
  }
  if (g_mode == 2) {
    // The compare: the function of the game draws the mesh into the left eye
    // layer, and the GPU path draws it into the right eye layer.
    if (g_twin_lines[kind] < 6) {
      ++g_twin_lines[kind];
      ModLog("gpu mesh     : compare, %s, mesh 0x%08X: type %u, flags 0x%02X, %u vertices, %u "
             "triangles, color 0x%08X, specular 0x%08X, ambient %d, flags 0x%X, camera "
             "position %.3f %.3f %.3f, first vertex rhw field %.5f, zfunc/cull/zwrite %u/%u/%u",
             kKindName[kind], static_cast<unsigned>(reinterpret_cast<uintptr_t>(mesh)),
             Read<uint32_t>(reinterpret_cast<uintptr_t>(mesh)), mesh[5], p.vertex_count,
             p.triangles, p.c.color, p.c.specular, p.c.ambient, p.c.flags, p.c.row[0][3],
             p.c.row[1][3], p.c.row[2][3], p.first_field, p.zfunc, p.cull, p.zwrite);
    }
    OneWalkSetMode(kOneWalkLeftOnly);
    g_back[kind](mesh, edx);
    OneWalkSetMode(kOneWalkRightOnly);
    Submit(kind, mesh, p, false);
    OneWalkSetMode(kOneWalkBoth);
  } else {
    Submit(kind, mesh, p, true);
  }
  g_counts.gpu_ticks += __rdtsc() - start;
}

void __fastcall Hook0(void* mesh, void* edx) { Hook(kClip, mesh, edx); }
void __fastcall Hook1(void* mesh, void* edx) { Hook(kLit, mesh, edx); }
void __fastcall Hook2(void* mesh, void* edx) { Hook(kLitClip, mesh, edx); }
void __fastcall Hook3(void* mesh, void* edx) { Hook(kFlat, mesh, edx); }
void __fastcall Hook4(void* mesh, void* edx) { Hook(kOffsets, mesh, edx); }

bool Expect(const Site& s) {
  const void* at = reinterpret_cast<const void*>(s.address);
  return !IsBadReadPtr(at, sizeof(s.bytes)) && memcmp(at, s.bytes, sizeof(s.bytes)) == 0;
}

const char* ModeText(int mode) {
  static const char* const kText[3] = {
      "The meshes take the CPU path of the game",
      "The opaque meshes with one matrix take the GPU path: the renderer transforms, lights, "
      "clips and culls them",
      "A test aid: each such mesh is drawn with the CPU path in the left eye layer and with "
      "the GPU path in the right eye layer"};
  return kText[mode < 0 || mode > 2 ? 0 : mode];
}

double TicksPerMs() {
  LARGE_INTEGER q;
  QueryPerformanceCounter(&q);
  const uint64_t tsc = __rdtsc();
  LARGE_INTEGER f;
  QueryPerformanceFrequency(&f);
  if (g_window_qpc == 0) {
    g_window_qpc = static_cast<uint64_t>(q.QuadPart);
    g_window_tsc = tsc;
    return 0.0;
  }
  const double ms = static_cast<double>(static_cast<uint64_t>(q.QuadPart) - g_window_qpc) * 1000.0 /
                    static_cast<double>(f.QuadPart);
  const double ticks = static_cast<double>(tsc - g_window_tsc);
  g_window_qpc = static_cast<uint64_t>(q.QuadPart);
  g_window_tsc = tsc;
  return ms > 0.0 ? ticks / ms : 0.0;
}

void Report() {
  const uint64_t now = GetTickCount64();
  if (g_report_at == 0) {
    g_report_at = now + 10000;
    TicksPerMs();
    return;
  }
  if (now < g_report_at) return;
  g_report_at = now + 10000;
  const double per_ms = TicksPerMs();
  VkSceneMeshCounts renderer = {};
  VkSceneTakeMeshCounts(&renderer);
  uint32_t gpu = 0, cpu = 0;
  for (int k = 0; k < kKinds; ++k) {
    gpu += g_counts.gpu[k];
    cpu += g_counts.cpu[k];
  }
  if (g_mode != 0 || gpu + cpu > 0) {
    ModLog("gpu mesh     : gpu_mesh=%d, in 10 s: %u meshes on the GPU path (%u vertices, %u "
           "triangles, %u offsets), %u on the CPU path. The GPU path by function: %s %u, %s "
           "%u, %s %u, %s %u, %s %u. The CPU path by reason: checks before the material %u, "
           "a draw that h_DrawTriangle changes %u, blend or no depth write or stage 1 %u, "
           "table full %u, left out by gpu_mesh_kinds %u, and in the function of the game %u, %u, %u, %u, %u (by function). "
           "Records of mesh data %u (%llu bytes), the renderer has %u meshes in %llu bytes "
           "and drew %llu, %llu with no data. Time on the frame thread in 10 s: the GPU path "
           "%.2f ms, the function of the game %.2f ms",
           g_mode, gpu, g_counts.vertices, g_counts.triangles, g_counts.offsets, cpu,
           kKindName[0], g_counts.gpu[0], kKindName[1], g_counts.gpu[1], kKindName[2],
           g_counts.gpu[2], kKindName[3], g_counts.gpu[3], kKindName[4], g_counts.gpu[4],
           g_counts.why_pre, g_counts.why_special, g_counts.why_state, g_counts.why_table,
           g_counts.why_kind,
           g_counts.cpu[0], g_counts.cpu[1], g_counts.cpu[2], g_counts.cpu[3], g_counts.cpu[4],
           g_counts.data_sent, static_cast<unsigned long long>(g_counts.data_bytes),
           renderer.meshes, static_cast<unsigned long long>(renderer.arena_bytes),
           static_cast<unsigned long long>(renderer.draws),
           static_cast<unsigned long long>(renderer.no_data),
           per_ms > 0.0 ? static_cast<double>(g_counts.gpu_ticks) / per_ms : 0.0,
           per_ms > 0.0 ? static_cast<double>(g_counts.cpu_ticks) / per_ms : 0.0);
  }
  g_counts = Counts();
}

}  // namespace

void GpuMeshSetOptions(int mode, bool test_keys, int compare_every, int kinds) {
  g_compare_every = compare_every;
  g_kinds = g_base_kinds = static_cast<uint32_t>(kinds) & 31u;
  g_mode = mode < 0 || mode > 2 ? 0 : mode;
  g_test_keys = test_keys;
  VkSceneSetMesh(g_mode == 2 ? 1 : 0);
  ModLog("gpu mesh     : gpu_mesh=%d, gpu_mesh_kinds=%d. %s", g_mode, kinds, ModeText(g_mode));
}

void GpuMeshInstall() {
  if (g_in || (g_mode == 0 && !g_test_keys)) return;
  for (int k = 0; k < kKinds; ++k) {
    if (!Expect(kSites[k])) {
      ModLog("gpu mesh     : the code at the start of %s is not the code of the game that the "
             "mod fits, so the meshes take the CPU path", kKindName[k]);
      return;
    }
  }
  void* const hooks[kKinds] = {reinterpret_cast<void*>(&Hook0), reinterpret_cast<void*>(&Hook1),
                               reinterpret_cast<void*>(&Hook2), reinterpret_cast<void*>(&Hook3),
                               reinterpret_cast<void*>(&Hook4)};
  bool ok = true;
  for (int k = 0; k < kKinds && ok; ++k) {
    void* target = reinterpret_cast<void*>(kSites[k].address);
    void* back = nullptr;
    ok = MH_CreateHook(target, hooks[k], &back) == MH_OK && MH_QueueEnableHook(target) == MH_OK;
    g_back[k] = reinterpret_cast<MeshFunction>(back);
  }
  const MH_STATUS applied = ok ? MH_ApplyQueued() : MH_UNKNOWN;
  if (applied != MH_OK) {
    for (int k = 0; k < kKinds; ++k) MH_RemoveHook(reinterpret_cast<void*>(kSites[k].address));
  }
  g_in = applied == MH_OK;
  ModLog("hooks        : the meshes %s at 0x%08X, 0x%08X, 0x%08X, 0x%08X and 0x%08X %s",
         "fn_00856D40, fn_0085AB10, fn_0084BA90, fn_0085BA30 and fn_00859530",
         static_cast<unsigned>(kSites[0].address), static_cast<unsigned>(kSites[1].address),
         static_cast<unsigned>(kSites[2].address), static_cast<unsigned>(kSites[3].address),
         static_cast<unsigned>(kSites[4].address), g_in ? "ok" : "FAILED");
  if (!g_in) g_mode = 0;
}

int GpuMeshValue() {
  if (!g_in) return kDebugCannot;
  return g_mode == 1 ? 1 : 0;
}

void GpuMeshToggle(const char* keys) {
  g_mode = g_mode == 1 ? 0 : 1;
  VkSceneSetMesh(0);
  ControllersGesturePulse(0.5f, g_mode == 1 ? 0.30f : 0.08f);
  ModLog("test key     : %s, gpu_mesh is %d from the next mesh", keys, g_mode);
}

// The compare samples (gpu_mesh_compare_every=N): each Nth frame the next frame
// gives its difference to the log, and the functions on the GPU path go
// through 6 sets in turn: all of them (gpu_mesh_kinds), then each function
// alone. So one run gives the difference for each function.
void Sample() {
  if (g_mode != 2 || g_compare_every <= 0) {
    g_kinds = g_base_kinds;
    return;
  }
  ++g_frames;
  if (g_frames % static_cast<uint64_t>(g_compare_every) != 0) {
    // The frame after a sample goes back to the base set.
    g_kinds = g_base_kinds;
    return;
  }
  const int step = g_sample++ % (kKinds + 1);
  g_kinds = step == 0 ? g_base_kinds : ((1u << (step - 1)) & g_base_kinds);
  if (g_kinds == 0) return;
  VkSceneCompareNext(g_kinds);
}

void GpuMeshFrame() {
  if (!g_in) return;
  Report();
  Sample();
  if (!g_test_keys) return;
  const bool ctrl_shift = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                          (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
  const bool down = ctrl_shift && (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
  if (down && !g_key_down) {
    // 1, 0, 2, 1 ...: the compare needs one_walk=1, which the renderer
    // checks for the draw.
    g_mode = g_mode == 1 ? 0 : (g_mode == 0 ? 2 : 1);
    VkSceneSetMesh(g_mode == 2 ? 1 : 0);
    ModLog("gpu mesh     : Ctrl Shift F10: gpu_mesh=%d. %s", g_mode, ModeText(g_mode));
  }
  g_key_down = down;
}
