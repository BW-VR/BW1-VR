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
//   1. the checks of the mesh and the reads of the inputs (the color, the
//      matrix, the light), as the function does before its loops,
//   2. the material (the same calls as the function: the function of the
//      material type, the tiling, the cull mode),
//   3. the data of the mesh go to the renderer one time (a cheap test at each
//      draw, a full sum over the words at a slow rate),
//   4. one record for the draw: the matrix of the part and the camera, the
//      projection, the colors, the light and the shift of the texture. The
//      renderer reads the blend, the depth states and the textures of both
//      stages from the device, as for each other draw.
//
// Else the function of the game runs as it is, and the loops of
// fast_transform.cpp and part_draw.cpp are still in it.
//
// THE ORDER. The five functions draw a mesh at once through
// LH3DRender::DrawTriangle, in the order of their calls. The z sorter of the
// engine (LH3DZSorter::NewZObject) holds whole objects with a callback, and
// the callback calls the function of the mesh later, in the order of the
// depth. So a mesh with blend or with no depth write needs no sort of its own:
// the record of the GPU path comes at the same place as the draw of the CPU
// path, with the same states, and the triangles of a mesh go in the order of
// its indices in both paths.
//
// WHICH MESH TAKES THE CPU PATH. A draw with the clamp of the screen
// (0x00FA9398, the icons of the spells), a mesh that h_DrawTriangle changes (the
// glow of a target, an advisor of a cut scene, the box of a speech bubble), a
// mesh with more than 20000 vertices or 40000 triangles, a mesh with groups
// whose counts are not its vertex count, and a mesh of a function that
// gpu_mesh_kinds leaves out. The keys gpu_mesh_blend, gpu_mesh_stage1,
// gpu_mesh_uvshift and gpu_mesh_group put that kind of mesh on the CPU path.
// The log counts each reason apart.
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
#include <stdlib.h>
#include <string.h>

#include "MinHook.h"
#include "debug_menu.h"
#include "gpu_mesh.h"
#include "gpu_mesh_math.h"
#include "latch_math.h"
#include "one_walk.h"
#include "vk_ddraw.h"
#include "vk_scene.h"
#include "vk_stream.h"

void ModLog(const char* format, ...);
int StereoPassEye();
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
const uintptr_t kUvShiftU = 0x00ECA62C;
const uintptr_t kUvShiftV = 0x00ECA630;
const uintptr_t kClampOn = 0x00FA9398;         // DrawTriangle clamps the screen
const uintptr_t kMeshCounter = 0x00F05184;
const uintptr_t kCurrentMesh = 0x00F0517C;
const uintptr_t kGroup = 0x00FA93BC;           // the mesh group: flags first
const uintptr_t kPart = 0x00EA1AE8;            // the 3D part being drawn
const uintptr_t kOffsetPointer = 0x00F05180;
const uintptr_t kMatrix = 0x00EA9EA0;          // pointer to the 12 floats
const uintptr_t kGroupMatrices = 0x00E9FE48;   // pointer to the matrices of the groups
const uintptr_t kLightPosition = 0x00EA9E80;   // 3 floats: the light in camera space
const uintptr_t kRootTable = 0x00EEA394;       // pointer to the table of fn_00841170
const uintptr_t kSetInverse = 0x007FB290;      // LHMatrix::SetInverse, fastcall (out, in)
const uintptr_t kInverseRoot = 0x00841170;     // cdecl float (float)
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
uint32_t g_parts_keys = gm::kPartsAll;  // gpu_mesh_blend and the other keys
uint32_t g_parts = gm::kPartsAll;       // the parts now (the test key turns them off)
uint32_t g_check_frames = 30;           // gpu_mesh_check_frames
bool g_timer = false;                   // gpu_mesh_timer
uint64_t g_frames = 0;
int g_sample = 0;
int g_twin_lines[kKinds + 1] = {};
bool g_test_keys = false;
bool g_in = false;
bool g_key_down = false;
bool g_parts_key_down = false;
bool g_depth_key_down = false;
bool g_vertices_key_down = false;
bool g_latch_key_down = false;
bool g_equal_key_down = false;
bool g_said_backend = false;
// Latched for each frame (GpuMeshFrame): the mode, and a renderer that draws
// the meshes.
bool g_active = false;
uint32_t g_frame = 0;  // the frame number of the checks of the data
uint64_t g_report_at = 0;
uintptr_t g_last_mesh = 0;  // the mesh of the last draw on the GPU path
// The last draw on the GPU path, for the log of the pairs of layers.
struct LastDraw {
  uintptr_t mesh;
  uint32_t type, kind, vertices, zfunc, zwrite, blend, grouped;
  const void* data;
  float rows[12];
};
LastDraw g_last = {};
bool g_last_layer = false;   // the last draw on the GPU path was a layer
bool g_after_gpu = false;    // the draw before was a mesh on the GPU path
uint32_t g_pairs_said = 0;
struct Caller {
  uint32_t address;
  uint32_t count;
  uint32_t via;
};
Caller g_callers[8] = {};
// The callers of the batches of the game with the depth test EQUAL.
Caller g_equal_callers[16] = {};
// THE LATCH (gpu_mesh_latch, docs/software-tnl.md, "The layers"). The functions
// that draw a mesh class of their own (fn_00851870, fn_00859D90, fn_0084E200,
// fn_0085A6F0, fn_00850FC0, fn_0085C7F0) draw layers (blend or no depth write,
// depth test EQUAL or LESSEQUAL) over the meshes of a 3D part on the CPU path. When
// such a layer comes right after a mesh of the five functions, the meshes of that
// 3D part take the CPU path for the next frames, so the layer and the mesh under
// it have the same path.
latch_math::PartTable g_latched;
bool g_latch_key = true;     // gpu_mesh_latch
bool g_latch_on = true;      // the key now (Ctrl Shift 3)
bool g_latch_active = false; // some part is latched now
uint32_t g_latch_frames = 90;
bool g_in_cpu_mesh = false;  // the function of the game for a mesh runs now
bool g_last_valid = false;   // the draw before was a mesh of the five functions
struct LastMesh {
  uintptr_t part, mesh;
  uint32_t type, vertices, kind;
  bool gpu;
};
LastMesh g_last_mesh_info = {};
uint32_t g_latch_said = 0;
bool g_material_skip = false;  // gpu_mesh_material_skip
uintptr_t g_mat_mesh = 0;
uint32_t g_mat_serial = 0;
// THE TRACE (gpu_mesh_trace=1, a test aid that reaches the Frame). For the callers
// of the five functions (the return address of the call): the meshes on the GPU
// path, those with blend, those drawn outside an eye pass, and those drawn when
// the device had no stage 0 texture. And the frames that took long (the menu
// pauses the game for about 200 ms).
bool g_trace = false;
struct TraceCaller {
  uint32_t address, gpu, blend, outside;
};
TraceCaller g_trace_callers[16] = {};
uint64_t g_trace_frame_qpc = 0;
uint32_t g_trace_long = 0, g_trace_longest_ms = 0, g_trace_meshes_in_frame = 0;
uint32_t g_trace_meshes_before_long = 0;
uint32_t g_epoch = 0;       // VkStreamMeshEpoch, read one time in a frame (GpuMeshFrame)
uint32_t g_changes_said_sample = 0, g_changes_said_full = 0;  // lines of the log for a change

// For the report.
struct Counts {
  uint32_t gpu[kKinds] = {};
  uint32_t cpu[kKinds] = {};
  // The reasons for the CPU path. A mesh has the first that applies.
  uint32_t r_data = 0;      // no vertices, no indices, or too many
  uint32_t r_clamp = 0;     // DrawTriangle clamps the screen
  uint32_t r_uv = 0;        // the shift of the texture (gpu_mesh_uvshift=0)
  uint32_t r_group = 0;     // a matrix for each group of vertices
  uint32_t r_offsets = 0;   // fn_00859530 without its offsets
  uint32_t r_special = 0;   // h_DrawTriangle would change the batch
  uint32_t r_kind = 0;      // gpu_mesh_kinds leaves the function out
  uint32_t r_table = 0;     // the table of meshes is full
  uint32_t r_blend = 0;     // blend (gpu_mesh_blend=0)
  uint32_t r_nozwrite = 0;  // no depth write (gpu_mesh_blend=0)
  uint32_t r_noztest = 0;   // no depth test (gpu_mesh_blend=0)
  uint32_t r_stage1 = 0;    // a texture on stage 1 (gpu_mesh_stage1=0)
  uint32_t r_cached = 0;    // the state test failed in the last frames of this mesh
  // fn_0081C090 (gpu_mesh_vertices): batches of vertices that come with the call.
  uint32_t v_gpu = 0, v_cpu = 0, v_vertices = 0, v_triangles = 0;
  uint32_t v_clamp = 0, v_special = 0, v_data = 0, v_index = 0, v_state = 0;
  uint64_t v_ticks = 0;
  // What the meshes on the GPU path have.
  uint32_t f_blend = 0, f_nozwrite = 0, f_noztest = 0, f_stage1 = 0, f_uv = 0;
  uint32_t data_sent = 0;
  uint32_t data_changed = 0;  // a mesh whose words changed at the same address
  uint32_t changed_sample = 0;  // of them found by the sample at a draw
  uint32_t changed_full = 0;    // of them found only by the full sum
  uint32_t full_age_max = 0;    // frames between the last full sum and one that found a change
  uint32_t changes_said = 0;    // lines of the log for a change
  uint32_t data_full = 0;     // full sums
  uint32_t data_retry = 0;    // data that the renderer dropped, given again
  uint32_t f_group = 0;       // meshes on the GPU path with groups
  uint32_t f_layer = 0;       // blend or no depth write with a depth test of EQUAL or LESSEQUAL
  uint32_t f_layer_same = 0;  // of them the mesh that the draw before drew too
  uint32_t f_layer_pairs = 0; // a layer right after a layer on the GPU path
  // CPU batches (DrawTriangle, DrawAndClip) with blend or no depth write and a
  // depth test of EQUAL or LESSEQUAL right after a draw of a mesh on the GPU
  // path: a layer on the CPU path over a mesh on the GPU path.
  uint32_t cpu_layer[2] = {};
  uint32_t cpu_after_gpu = 0;  // all CPU batches right after a mesh on the GPU path
  // The latch (gpu_mesh_latch): a layer from a mesh class function right after a
  // mesh of the five functions sends the meshes of that 3D part to the CPU path.
  uint32_t latch_new = 0;       // parts latched for the first time
  uint32_t latch_refresh = 0;   // layers that kept a part latched
  uint32_t r_latch = 0;         // meshes on the CPU path because their part is latched
  uint32_t latch_by[6] = {};    // new latches by the function of the layer
  uint32_t latch_full = 0;      // new latches that took the place of a live latch
  uint32_t trace_outside = 0;   // gpu_mesh_trace: meshes on the GPU path outside an eye pass
  uint32_t trace_long = 0;      // frames of more than 120 ms between two frames of the hook
  uint32_t trace_longest_ms = 0;
  uint32_t trace_meshes_before_long = 0;
  uint32_t latch_live = 0;      // latched parts now
  // The draws with the depth test EQUAL (vk_equal_depth): meshes on the GPU path,
  // and the batches of the game on the CPU path (DrawTriangle, DrawAndClip).
  uint32_t equal_gpu = 0;
  uint32_t equal_gpu_kind[5] = {};
  uint32_t equal_cpu[2] = {};
  // The material (RunMaterial), with gpu_mesh_timer=1: runs, runs that changed a
  // state of the device, runs of the same mesh with no change since the last run,
  // runs skipped (gpu_mesh_material_skip), and the time of the parts.
  uint32_t mat_runs = 0, mat_changed = 0, mat_repeat = 0, mat_skipped = 0;
  uint64_t mat_ticks[3] = {};   // the function of the type, the tiling, the cull mode
  uint32_t groups = 0;        // the groups of those meshes
  uint32_t lights = 0;        // lights of groups checked against the functions of the game
  uint32_t light_diff = 0;    // of them with other bits
  uint64_t data_bytes = 0;
  uint64_t gpu_ticks = 0;     // the hook for the meshes on the GPU path
  uint64_t cpu_ticks = 0;     // the function of the game
  uint64_t check_ticks = 0;   // the checks (and the material) of a mesh that went to the CPU
  uint64_t part_ticks[6] = {};
  // gpu_mesh_timer=1: the meshes by material type (the word at +0 of the mesh).
  uint32_t t_gpu[32] = {}, t_cpu[32] = {}, t_blend[32] = {}, t_nowrite[32] = {};
  uint32_t t_stage1[32] = {}, t_uv[32] = {}, t_group[32] = {};
  uint32_t vertices = 0, triangles = 0, offsets = 0;
};
Counts g_counts;
uint64_t g_window_tsc = 0, g_window_qpc = 0;
const char* const kPartName[6] = {"checks", "material", "state", "consts", "data", "record"};

// THE TABLE OF THE MESHES: what the renderer has, by the address of the
// mesh. A mesh that the game made again at the same address has other words
// and gets a new record of data.
struct Known {
  uintptr_t mesh;
  const void* vertices;
  const void* indices;
  uint64_t hash;
  uint32_t sample;
  uint32_t vertex_count, triangles;
  uint32_t epoch;
  uint32_t group_count;  // the groups that the renderer has for the mesh (0: none)
  uint32_t checked;    // the frame of the last test of the data
  uint32_t full_at;    // the frame of the last full sum
  uint32_t* blocks;    // the sums of the 32 byte blocks of the data, to find a change
  uint32_t block_count;
  uint32_t next_full;  // the frame of the next full sum
  uint32_t cpu_until;  // the frame until which the mesh goes to the CPU path (the state test failed)
  uint32_t retry_at;   // the frame of the next try to give the renderer its data, or 0
  uint32_t flags;
};
const uint32_t kKnownChanged = 1u;  // the words changed in place: a full sum at each draw
const uint32_t kKnownBadGroups = 2u;  // the groups of the mesh do not add up to its vertices
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

// What the function does after the loops for a mesh that is drawn: the
// material, the tiling and the cull mode. Each call of the game here is a
// state that its own cache drops when it has the value, so a run that finds
// the device as it left it changes nothing. With gpu_mesh_material_skip=1 the
// run is left out for the mesh of the run before when the device had no change
// since (VkDdrawStateSerial) and the mesh has no dynamic texture (the word at
// +0x138). With gpu_mesh_timer=1 the log counts the runs, the runs that changed
// the device, and the time of the parts.
void RunMaterial(uint8_t* mesh) {
  if (Read<uint32_t>(kDoUpdateMaterial) == 0) return;
  Write<void*>(kCurrentMaterial, mesh);
  const uintptr_t m = reinterpret_cast<uintptr_t>(mesh);
  const bool measure = g_timer || g_material_skip;
  const uint32_t serial = measure ? VkDdrawStateSerial() : 0;
  if (g_material_skip && m == g_mat_mesh && serial == g_mat_serial &&
      Read<uint32_t>(m + 0x138) == 0) {
    ++g_counts.mat_skipped;
    return;
  }
  uint64_t t0 = g_timer ? __rdtsc() : 0;
  const uintptr_t table = Read<uintptr_t>(kMaterialTable);
  const uint32_t type = Read<uint32_t>(m);
  const MaterialFunction material =
      Read<MaterialFunction>(table + static_cast<uintptr_t>(type) * 8);
  material(mesh, 0);
  uint64_t t1 = g_timer ? __rdtsc() : 0;
  if (Read<uint32_t>(kNeedTilling) == 0 && (mesh[5] & 4) == 0) {
    reinterpret_cast<TilingFunction>(kTilingOff)(0);
  } else {
    reinterpret_cast<TilingFunction>(kTilingOn)(0);
  }
  uint64_t t2 = g_timer ? __rdtsc() : 0;
  const uint32_t cull = ((~static_cast<uint32_t>(mesh[5]) & 1u) << 1) | 1u;
  if (Read<uint32_t>(kCullCache) != cull) {
    void* device = Read<void*>(kDevice);
    const uintptr_t vtable = Read<uintptr_t>(reinterpret_cast<uintptr_t>(device));
    const long result =
        Read<SetRenderStateFunction>(vtable + 0x50)(device, 0x16, cull);
    Write<uint32_t>(kCullCache, 0xFFFFFFFFu);
    if (result == 0) Write<uint32_t>(kCullCache, cull);
  }
  if (measure) {
    const uint32_t after = VkDdrawStateSerial();
    ++g_counts.mat_runs;
    if (after != serial) {
      ++g_counts.mat_changed;
    } else if (m == g_mat_mesh && serial == g_mat_serial) {
      ++g_counts.mat_repeat;
    }
    g_mat_mesh = m;
    g_mat_serial = after;
    if (g_timer) {
      const uint64_t t3 = __rdtsc();
      g_counts.mat_ticks[0] += t1 - t0;
      g_counts.mat_ticks[1] += t2 - t1;
      g_counts.mat_ticks[2] += t3 - t2;
    }
  }
}

// The offsets of fn_00859530 as 3 floats for each vertex.
float g_expanded[3 * gm::kMostVertices];
// The groups of a draw: 12 floats for each group (the matrix in the rows of
// the shader), then with a light 4 floats for each group. And the ordinal of
// the group of each vertex for the data of a mesh.
float g_group_floats[gm::kMostGroups * 16];
uint16_t g_ordinals[gm::kMostVertices];

// The values of one draw that Prepare made.
struct Prepared {
  gm::Consts c;
  const float* offsets;   // 3 floats for each vertex, or null
  uint32_t offset_floats;
  const float* source;    // the offsets of the game, for the pointer after the draw
  uint32_t offset_step;   // floats of the game for each vertex: 1 or 3
  uint32_t vertex_count, triangles;
  const float* groups;    // the matrices and lights of the groups, or null
  uint32_t group_count;
  float first_field;      // for DrawHooksMeshBatch
  uint32_t zfunc, cull, zwrite;  // the device now, for the log of the compare
  uint32_t blend;                // the blend state of the device
};

// The clock of the parts of the hook (gpu_mesh_timer=1).
struct PartClock {
  uint64_t last = 0;
  void Start() {
    if (g_timer) last = __rdtsc();
  }
  void Lap(int part) {
    if (!g_timer) return;
    const uint64_t now = __rdtsc();
    g_counts.part_ticks[part] += now - last;
    last = now;
  }
};

// The data of the mesh for the renderer, one time. The full sum over the words
// is the proof that the data are the data that the renderer has. A mesh of the
// game is an asset (its vertices and indices do not change in place), so a
// draw tests the pointers, the counts and a sample of a few words, and the
// full sum comes at a slow rate (gpu_mesh_check_frames), and at each draw for a
// mesh whose words changed once. The offsets of a person or an animal are not
// part of the data: each draw sends them.
// Keeps the sums of the blocks of the data of a mesh, to say where a change is.
void KeepBlocks(Known* known, const uint8_t* vertices, uint32_t n, const uint16_t* indices,
                uint32_t triangles) {
  const uint32_t count = gm::BlockCount(n, triangles);
  if (known->block_count != count) {
    free(known->blocks);
    known->blocks = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * count));
    known->block_count = known->blocks != nullptr ? count : 0;
  }
  if (known->blocks != nullptr) gm::BlockSums(vertices, n, indices, triangles, known->blocks);
}

// A mesh whose words changed at the same address: the first lines of the log
// say how the change was found, the mesh and the first changed block.
void SayChange(Known* known, uintptr_t m, const uint8_t* vertices, uint32_t n,
               const uint16_t* indices, uint32_t triangles, bool by_sample) {
  if (by_sample) {
    ++g_counts.changed_sample;
  } else {
    ++g_counts.changed_full;
    const uint32_t age = g_frame - known->full_at;
    if (age > g_counts.full_age_max) g_counts.full_age_max = age;
  }
  // The first 12 changes that the sample found, and the first 12 that only the
  // full sum found, since the start.
  uint32_t& said = by_sample ? g_changes_said_sample : g_changes_said_full;
  if (said >= 12) return;
  ++said;
  ++g_counts.changes_said;
  int block = -1;
  if (known->blocks != nullptr && known->block_count == gm::BlockCount(n, triangles)) {
    static uint32_t now[(gm::kMostVertices * 32 + gm::kMostTriangles * 6) / 32 + 2];
    gm::BlockSums(vertices, n, indices, triangles, now);
    block = gm::FirstChangedBlock(known->blocks, now, known->block_count);
  }
  const uint32_t type = Read<uint32_t>(m);
  if (block < 0) {
    ModLog("gpu mesh     : a mesh changed in place, found by the %s: mesh 0x%08X, type %u, %u "
           "vertices, %u triangles, the first changed place is not known",
           by_sample ? "sample" : "full sum", static_cast<unsigned>(m), type, n, triangles);
  } else if (static_cast<uint32_t>(block) < n) {
    const float* v = reinterpret_cast<const float*>(vertices + static_cast<size_t>(block) * 32);
    ModLog("gpu mesh     : a mesh changed in place, found by the %s: mesh 0x%08X, type %u, %u "
           "vertices, %u triangles, the first changed place is vertex %d (now %.3f %.3f %.3f, uv "
           "%.3f %.3f, normal %.3f %.3f %.3f), frames since the last full sum %u",
           by_sample ? "sample" : "full sum", static_cast<unsigned>(m), type, n, triangles, block,
           v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], g_frame - known->full_at);
  } else {
    ModLog("gpu mesh     : a mesh changed in place, found by the %s: mesh 0x%08X, type %u, %u "
           "vertices, %u triangles, the first changed place is in the indices, about triangle %u, "
           "frames since the last full sum %u",
           by_sample ? "sample" : "full sum", static_cast<unsigned>(m), type, n, triangles,
           (static_cast<uint32_t>(block) - n) * 32 / 6, g_frame - known->full_at);
  }
}

void GiveData(Known* known, uintptr_t m, const uint8_t* vertices, uint32_t n,
              const uint16_t* indices, uint32_t triangles, const gm::MeshGroup* groups,
              uint32_t group_count, bool want_groups) {
  const uint32_t epoch = g_epoch;
  const bool same_place = known->mesh == m && known->vertices == vertices &&
                          known->indices == indices && known->vertex_count == n &&
                          known->triangles == triangles;
  // A mesh drawn without its groups before, and now with them, gets its data
  // again with the groups.
  const bool groups_missing =
      want_groups && known->group_count == 0 && (known->flags & kKnownBadGroups) == 0;
  uint64_t hash = 0;
  bool have_hash = false;
  if (same_place && known->epoch == epoch && !groups_missing) {
    if (known->retry_at != 0 && g_frame >= known->retry_at) {
      ++g_counts.data_retry;
      const bool have_groups = known->group_count != 0 && groups != nullptr &&
                               gm::GroupOrdinals(groups, group_count, n, g_ordinals);
      known->retry_at = VkSceneMeshRetry(m, known->hash, vertices, n, indices, triangles,
                                         have_groups ? g_ordinals : nullptr,
                                         have_groups ? group_count : 0)
                            ? 0u
                            : g_frame + 30u;
    }
    // A mesh is tested once in a frame: the draws of an instance in the same
    // frame see the same words.
    if (known->checked == g_frame) return;
    known->checked = g_frame;
    const bool full = g_check_frames == 0 || (known->flags & kKnownChanged) != 0 ||
                      static_cast<int32_t>(g_frame - known->next_full) >= 0;
    const uint32_t sample = gm::MeshSample(vertices, n, indices, triangles);
    if (sample == known->sample && !full) return;
    ++g_counts.data_full;
    hash = gm::MeshHash(vertices, n, indices, triangles, groups, group_count);
    const bool by_sample = sample != known->sample;
    known->next_full =
        g_frame + g_check_frames + static_cast<uint32_t>((m >> 4) % (g_check_frames / 2 + 1));
    if (hash == known->hash) {
      known->sample = sample;
      known->full_at = g_frame;
      return;
    }
    have_hash = true;
    known->flags |= kKnownChanged;
    ++g_counts.data_changed;
    SayChange(known, m, vertices, n, indices, triangles, by_sample);
    known->full_at = g_frame;
  }
  if (!have_hash) hash = gm::MeshHash(vertices, n, indices, triangles, groups, group_count);
  // The ordinals of the groups go with the data when the mesh has valid
  // groups. A retry of the renderer has them from g_ordinals only while the
  // mesh is the last one that was given, so it keeps a copy: see below.
  uint32_t sent_groups = 0;
  known->flags &= ~kKnownBadGroups;
  if (groups != nullptr) {
    if (gm::GroupOrdinals(groups, group_count, n, g_ordinals)) {
      sent_groups = group_count;
    } else {
      known->flags |= kKnownBadGroups;
    }
  }
  const bool taken = VkSceneMeshData(m, hash, vertices, n, indices, triangles,
                                     sent_groups != 0 ? g_ordinals : nullptr, sent_groups);
  known->mesh = m;
  known->hash = hash;
  known->sample = gm::MeshSample(vertices, n, indices, triangles);
  known->epoch = epoch;
  known->vertex_count = n;
  known->triangles = triangles;
  known->vertices = vertices;
  known->indices = indices;
  known->group_count = sent_groups;
  known->checked = g_frame;
  known->full_at = g_frame;
  KeepBlocks(known, vertices, n, indices, triangles);
  known->next_full =
        g_frame + g_check_frames + static_cast<uint32_t>((m >> 4) % (g_check_frames / 2 + 1));
  // A renderer in this process that dropped the data (no room, not ready)
  // gets them again from the data of this draw, in a few frames.
  known->retry_at = taken ? 0u : g_frame + 30u;
  ++g_counts.data_sent;
  g_counts.data_bytes += static_cast<uint64_t>(n) * 32 + static_cast<uint64_t>(triangles) * 6 +
                         (sent_groups != 0 ? static_cast<uint64_t>(n) * 2 : 0);
}

// The checks, the material, the state of the device and the data of the mesh.
// True when the mesh takes the GPU path. False: nothing is drawn, and the
// function of the game must run.
bool Prepare(Kind kind, uint8_t* mesh, Prepared* out, PartClock* clock) {
  const uintptr_t m = reinterpret_cast<uintptr_t>(mesh);
  // The checks that need no material.
  const int32_t vertex_count = Read<int32_t>(m + 0x10);
  const int32_t triangles = Read<int32_t>(m + 0x18);
  const uint8_t* vertices = Read<const uint8_t*>(m + 0x14);
  const uint16_t* indices = Read<const uint16_t*>(m + 0x1C);
  if (vertex_count <= 0 || vertex_count > static_cast<int32_t>(gm::kMostVertices) ||
      triangles <= 0 || triangles > static_cast<int32_t>(gm::kMostTriangles) ||
      vertices == nullptr || indices == nullptr) {
    ++g_counts.r_data;
    return false;
  }
  if (Read<uint32_t>(kClampOn) != 0) {
    ++g_counts.r_clamp;
    return false;
  }
  // The shift of the texture: DrawTriangle adds it for the material mesh, which
  // is this mesh once the material ran.
  const uintptr_t shift_mesh =
      Read<uint32_t>(kDoUpdateMaterial) != 0 ? m : Read<uintptr_t>(kCurrentMaterial);
  const bool shifted = Read<uint32_t>(kUvShift) != 0 && shift_mesh != 0 &&
                       (Read<uint8_t>(shift_mesh + 5) & 0x10) == 0;
  if (shifted && (g_parts & gm::kPartUvShift) == 0) {
    ++g_counts.r_uv;
    return false;
  }
  // A mesh with a matrix for each group of vertices: the flag of the mesh group.
  // The list of groups of the mesh (a count at +0x20, a pointer at +0x24) goes
  // with the data of the mesh when it is valid.
  bool grouped = false;
  const gm::MeshGroup* groups = nullptr;
  uint32_t group_count = 0;
  if (kind != kOffsets) {
    const uintptr_t group = Read<uintptr_t>(kGroup);
    if (group == 0) {
      ++g_counts.r_group;
      return false;
    }
    grouped = (Read<uint32_t>(group) & 1u) != 0;
  }
  {
    const int32_t count = Read<int32_t>(m + 0x20);
    const gm::MeshGroup* list = Read<const gm::MeshGroup*>(m + 0x24);
    if (count > 0 && count <= static_cast<int32_t>(gm::kMostGroups) && list != nullptr) {
      groups = list;
      group_count = static_cast<uint32_t>(count);
    }
  }
  if (grouped) {
    const float* list = Read<const float*>(kGroupMatrices);
    if ((g_parts & gm::kPartGroup) == 0 || groups == nullptr || list == nullptr ||
        Read<uintptr_t>(kRootTable) == 0) {
      ++g_counts.r_group;
      return false;
    }
  }
  if ((g_kinds & (1u << kind)) == 0) {
    ++g_counts.r_kind;
    return false;
  }
  if (g_latch_active && g_latch_on) {
    if (g_latched.Has(Read<uintptr_t>(kPart), g_frame)) {
      ++g_counts.r_latch;
      return false;
    }
  }
  // The inputs of the loops of the game. The function reads them before its
  // material, so the hook does too.
  const float* matrix = Read<const float*>(kMatrix);
  if (matrix == nullptr && !grouped) {
    ++g_counts.r_data;
    return false;
  }
  const uint32_t specular = Read<uint32_t>(kSpecular);
  const uint32_t color = Read<uint32_t>(kColor);
  const float near_plane = Read<float>(kNearPlane);
  const float half_x = Read<float>(kHalfX);
  const float half_y = Read<float>(kHalfY);
  if (DrawHooksTriangleSpecial(specular)) {
    ++g_counts.r_special;
    return false;
  }
  const uint32_t n = static_cast<uint32_t>(vertex_count);
  float* offsets = nullptr;
  uint32_t offset_kind = 0;  // 0 none, 1 one offset for y, 2 three offsets
  if (kind == kOffsets) {
    const uintptr_t part = Read<uintptr_t>(kPart);
    offsets = Read<float*>(kOffsetPointer);
    if (part == 0 || offsets == nullptr) {
      ++g_counts.r_offsets;
      return false;
    }
    const uintptr_t vtable = Read<uintptr_t>(part);
    offset_kind =
        Read<OffsetKindFunction>(vtable + 0x1B0)(reinterpret_cast<void*>(part), nullptr) != 0 ? 2u
                                                                                              : 1u;
  }
  Known* known = FindKnown(m);
  if (known == nullptr) {
    ++g_counts.r_table;
    return false;
  }
  // A mesh whose state the parts of the path did not take: the function of
  // the game, with no second run of the material, for a few frames.
  if (known->cpu_until != 0 && g_frame < known->cpu_until) {
    ++g_counts.r_cached;
    return false;
  }
  // The values of the draw that the loops of the game read before the material
  // (the color, the matrix, the light, the ambient value, the position of the
  // light and the matrices of the groups): the hook reads them here too, in
  // case the material changes one of them.
  gm::Consts& c = out->c;
  c = gm::Consts();
  if (!grouped) gm::SetMatrix(&c, matrix);
  if ((g_parts & gm::kPartDepth) != 0) {
    // The position of the game, with the order of the product of y of this function.
    c.flags |= gm::kDepthGame | (kind == kLitClip ? gm::kYOrderB : 0u) |
               (kind == kClip ? gm::kYOrderC : 0u);
  }
  c.near_plane = near_plane;
  c.half_x = half_x;
  c.half_y = half_y;
  c.specular = specular;
  const bool lit = kind == kLit || kind == kLitClip || kind == kOffsets;
  c.color = (kind == kLit || kind == kLitClip) ? MeshColor(mesh, color) : color;
  if (lit) {
    c.flags |= gm::kLit;
    if (kind == kOffsets) c.flags |= gm::kTruncate;
    memcpy(c.light, reinterpret_cast<const void*>(kLightDirection), 12);
    c.ambient = Read<int32_t>(kAmbient);
  }
  out->groups = nullptr;
  out->group_count = 0;
  gm::Consts first_group = c;
  if (grouped) {
    const float* list = Read<const float*>(kGroupMatrices);
    const uint8_t* table = Read<const uint8_t*>(kRootTable);
    float light_position[3];
    memcpy(light_position, reinterpret_cast<const void*>(kLightPosition), 12);
    float* lights = g_group_floats + group_count * 12;
    for (uint32_t g = 0; g < group_count; ++g) {
      const float* source = list + static_cast<size_t>(groups[g].matrix) * 12;
      gm::Consts one = {};
      gm::SetMatrix(&one, source);
      memcpy(g_group_floats + g * 12, one.row, 48);
      if (lit) {
        gm::GroupLight(source, light_position, table, lights + g * 4);
        lights[g * 4 + 3] = 0.0f;
        if (g_mode == 2 && g_counts.lights < 4000) {
          // The compare checks the light of a group against the functions of the game.
          float inverse[12];
          reinterpret_cast<void(__fastcall*)(float*, const float*)>(kSetInverse)(inverse, source);
          const float lx = inverse[0] * light_position[0] + inverse[3] * light_position[1] +
                           inverse[6] * light_position[2] + inverse[9];
          const float ly = inverse[1] * light_position[0] + inverse[4] * light_position[1] +
                           inverse[7] * light_position[2] + inverse[10];
          const float lz = inverse[2] * light_position[0] + inverse[5] * light_position[1] +
                           inverse[8] * light_position[2] + inverse[11];
          const float k = reinterpret_cast<float(__cdecl*)(float)>(kInverseRoot)(
              lx * lx + lz * lz + ly * ly);
          const float want[3] = {lx * k, ly * k, lz * k};
          ++g_counts.lights;
          if (memcmp(want, lights + g * 4, 12) != 0) ++g_counts.light_diff;
        }
      }
    }
    first_group = c;
    memcpy(first_group.row, g_group_floats, 48);
  }
  clock->Lap(0);

  // The material, and the state of the device that it left.
  Write<void*>(kCurrentMesh, mesh);
  RunMaterial(mesh);
  clock->Lap(1);
  gm::DeviceState state = {};
  if (!VkDdrawMeshState(Read<void*>(kDevice), &state)) {
    ++g_counts.r_data;
    return false;
  }
  const uint32_t reasons = gm::StateReasons(state);
  if (!gm::StateAllowed(reasons, g_parts)) {
    // The first reason that is not on the GPU path.
    if ((reasons & gm::kStateBlend) != 0 && (g_parts & gm::kPartBlend) == 0) {
      ++g_counts.r_blend;
    } else if ((reasons & gm::kStateNoZWrite) != 0 && (g_parts & gm::kPartBlend) == 0) {
      ++g_counts.r_nozwrite;
    } else if ((reasons & gm::kStateNoZTest) != 0 && (g_parts & gm::kPartBlend) == 0) {
      ++g_counts.r_noztest;
    } else {
      ++g_counts.r_stage1;
    }
    known->cpu_until = g_frame + 30u;
    return false;
  }
  out->zfunc = state.z_func;
  out->cull = state.cull;
  out->zwrite = state.z_write;
  out->blend = state.alpha_blend;
  clock->Lap(2);
  if (shifted) {
    c.uv_shift_u = Read<float>(kUvShiftU);
    c.uv_shift_v = Read<float>(kUvShiftV);
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
  if (g_timer) {
    const uint32_t type = Read<uint32_t>(m) & 31u;
    ++g_counts.t_gpu[type];
    if ((reasons & gm::kStateBlend) != 0) ++g_counts.t_blend[type];
    if ((reasons & (gm::kStateNoZWrite | gm::kStateNoZTest)) != 0) ++g_counts.t_nowrite[type];
    if ((reasons & gm::kStateStage1) != 0) ++g_counts.t_stage1[type];
    if (shifted) ++g_counts.t_uv[type];
    if (grouped) ++g_counts.t_group[type];
  }
  // A layer over a surface: a draw with blend or with no depth write that passes
  // at the depth of what is there (D3DCMP_EQUAL 3, D3DCMP_LESSEQUAL 4). The snow
  // on a roof is one. Two layers on the GPU path need the same depth math.
  const bool is_layer = (reasons & (gm::kStateBlend | gm::kStateNoZWrite)) != 0 &&
                        (state.z_func == 3 || state.z_func == 4);
  LastDraw now = {};
  now.mesh = m;
  now.type = Read<uint32_t>(m);
  now.kind = kind;
  now.vertices = n;
  now.zfunc = state.z_func;
  now.zwrite = state.z_write;
  now.blend = state.alpha_blend;
  now.grouped = grouped ? 1u : 0u;
  now.data = vertices;
  memcpy(now.rows, grouped ? g_group_floats : &c.row[0][0], sizeof(now.rows));
  if (is_layer) {
    ++g_counts.f_layer;
    if (m == g_last_mesh) ++g_counts.f_layer_same;
    if (g_last_layer) {
      ++g_counts.f_layer_pairs;
      if (g_pairs_said < 16) {
        ++g_pairs_said;
        ModLog("gpu mesh     : layer pair %u: mesh 0x%08X (type %u, function %s, %u vertices, vertices "
               "at 0x%08X, blend %u, z write %u, zfunc %u, groups %u) then mesh 0x%08X (type %u, "
               "function %s, %u vertices, vertices at 0x%08X, blend %u, z write %u, zfunc %u, "
               "groups %u). The same mesh %d, the same vertices %d, the same matrix %d, the same "
               "function %d",
               g_pairs_said, static_cast<unsigned>(g_last.mesh), g_last.type,
               kKindName[g_last.kind], g_last.vertices, static_cast<unsigned>(
                   reinterpret_cast<uintptr_t>(g_last.data)),
               g_last.blend, g_last.zwrite, g_last.zfunc, g_last.grouped,
               static_cast<unsigned>(now.mesh), now.type, kKindName[now.kind], now.vertices,
               static_cast<unsigned>(reinterpret_cast<uintptr_t>(now.data)), now.blend, now.zwrite,
               now.zfunc, now.grouped, g_last.mesh == now.mesh ? 1 : 0,
               g_last.data == now.data ? 1 : 0,
               memcmp(g_last.rows, now.rows, sizeof(now.rows)) == 0 ? 1 : 0,
               g_last.kind == now.kind ? 1 : 0);
      }
    }
  }
  g_last = now;
  g_last_layer = is_layer;
  g_last_mesh = m;
  if (reasons != 0 || shifted) {
    if ((reasons & gm::kStateBlend) != 0) ++g_counts.f_blend;
    if ((reasons & gm::kStateNoZWrite) != 0) ++g_counts.f_nozwrite;
    if ((reasons & gm::kStateNoZTest) != 0) ++g_counts.f_noztest;
    if ((reasons & gm::kStateStage1) != 0) ++g_counts.f_stage1;
    if (shifted) ++g_counts.f_uv;
  }

  clock->Lap(3);

  // The data of the mesh, one time.
  GiveData(known, m, vertices, n, indices, out->triangles, groups, group_count, grouped);
  clock->Lap(4);

  if (grouped) {
    if (known->group_count != group_count) {
      // The counts of the groups are not the count of the vertices: the loop of
      // the game leaves vertices as they were.
      ++g_counts.r_group;
      known->cpu_until = g_frame + 30u;
      return false;
    }
    out->groups = g_group_floats;
    out->group_count = group_count;
    c.flags |= gm::kGroup;
    ++g_counts.f_group;
    g_counts.groups += group_count;
    memcpy(first_group.row, g_group_floats, 48);
    first_group.flags = c.flags;
  }

  // The first vertex of the batch, for the measure of the nearest geometry
  // (DrawHooksMeshBatch): the rhw of the vertex, or its camera z when the game
  // clips it.
  const gm::Camera cam =
      gm::CameraSpace(first_group, reinterpret_cast<const float*>(vertices), out->offsets);
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
  VkSceneDrawMesh(m, p.c, p.offsets, p.offset_floats, p.groups, p.group_count);
  g_after_gpu = true;
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

// The mesh of the five functions that was drawn last: its 3D part, for the latch.
void NoteMesh(const uint8_t* mesh, Kind kind, bool gpu) {
  const uintptr_t m = reinterpret_cast<uintptr_t>(mesh);
  g_last_mesh_info.part = Read<uintptr_t>(kPart);
  g_last_mesh_info.mesh = m;
  g_last_mesh_info.type = Read<uint32_t>(m);
  g_last_mesh_info.vertices = Read<uint32_t>(m + 0x10);
  g_last_mesh_info.kind = kind;
  g_last_mesh_info.gpu = gpu;
  g_last_valid = true;
}

void TraceMesh(uint32_t caller, const Prepared& p) {
  const int eye = StereoPassEye();
  ++g_trace_meshes_in_frame;
  if (eye < 0) ++g_counts.trace_outside;
  TraceCaller* slot = nullptr;
  for (TraceCaller& c : g_trace_callers) {
    if (c.address == caller) {
      slot = &c;
      break;
    }
    if (slot == nullptr && c.address == 0) slot = &c;
  }
  if (slot == nullptr) return;
  slot->address = caller;
  ++slot->gpu;
  if (p.blend != 0) ++slot->blend;
  if (eye < 0) ++slot->outside;
}

void Hook(Kind kind, void* mesh_in, void* edx, uint32_t ret) {
  uint8_t* mesh = static_cast<uint8_t*>(mesh_in);
  if (!g_active) {
    if (g_mode != 0 && !g_said_backend && !VkDdrawOn()) {
      g_said_backend = true;
      ModLog("gpu mesh     : gpu_mesh=%d needs backend=vulkan, so the meshes take the CPU "
             "path", g_mode);
    }
    g_back[kind](mesh, edx);
    return;
  }
  const uint64_t start = __rdtsc();
  Prepared p;
  PartClock clock;
  clock.Start();
  if (!Prepare(kind, mesh, &p, &clock)) {
    const uint64_t cpu_start = __rdtsc();
    g_in_cpu_mesh = true;
    g_back[kind](mesh, edx);
    g_in_cpu_mesh = false;
    NoteMesh(mesh, kind, false);
    ++g_counts.cpu[kind];
    if (g_timer) ++g_counts.t_cpu[Read<uint32_t>(reinterpret_cast<uintptr_t>(mesh)) & 31u];
    g_counts.cpu_ticks += __rdtsc() - cpu_start;
    g_counts.check_ticks += cpu_start - start;  // the checks (and the material) of this mesh
    return;
  }
  uint64_t back_ticks = 0;
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
    const uint64_t back_start = __rdtsc();
    g_in_cpu_mesh = true;
    g_back[kind](mesh, edx);
    g_in_cpu_mesh = false;
    back_ticks = __rdtsc() - back_start;
    clock.Start();  // the run of the function of the game is not part of the record
    OneWalkSetMode(kOneWalkRightOnly);
    Submit(kind, mesh, p, false);
    OneWalkSetMode(kOneWalkBoth);
  } else {
    Submit(kind, mesh, p, true);
  }
  clock.Lap(5);
  if (p.zfunc == 3) {
    ++g_counts.equal_gpu;
    ++g_counts.equal_gpu_kind[kind];
  }
  if (g_trace) TraceMesh(ret, p);
  NoteMesh(mesh, kind, true);
  g_counts.gpu_ticks += __rdtsc() - start - back_ticks;
}

void __fastcall Hook0(void* mesh, void* edx) {
  Hook(kClip, mesh, edx, reinterpret_cast<uint32_t>(_ReturnAddress()));
}
void __fastcall Hook1(void* mesh, void* edx) {
  Hook(kLit, mesh, edx, reinterpret_cast<uint32_t>(_ReturnAddress()));
}
void __fastcall Hook2(void* mesh, void* edx) {
  Hook(kLitClip, mesh, edx, reinterpret_cast<uint32_t>(_ReturnAddress()));
}
void __fastcall Hook3(void* mesh, void* edx) {
  Hook(kFlat, mesh, edx, reinterpret_cast<uint32_t>(_ReturnAddress()));
}
void __fastcall Hook4(void* mesh, void* edx) {
  Hook(kOffsets, mesh, edx, reinterpret_cast<uint32_t>(_ReturnAddress()));
}

// fn_0081C090 (docs/software-tnl.md, "The batches of vertices"): a draw of a
// batch of vertices that the caller gives: positions (3 floats), a color for
// each vertex (a dword), texture coordinates (2 floats), the triangles as 6 words
// each (an index every other word), a material mesh (or none) and a flag for the
// camera matrix. It has 18 callers (text, icons, effects). It transforms the
// positions with the matrix at 0x00EA9EA0, clips, tests the back face, runs the
// material and calls DrawTriangle. The vertices are new in each call, so they go
// with each draw.
const uintptr_t kVertexSite = 0x0081C090;
const uint8_t kVertexBytes[5] = {0x51, 0x8B, 0x44, 0x24, 0x1C};
const uintptr_t kMatrixCopy = 0x00EA9E10;   // the copy of the camera matrix (12 floats)
const uintptr_t kCameraMatrix = 0x00EA9E40;
typedef void(__fastcall* VertexFunction)(uint32_t count, float* positions, uint32_t* colors,
                                         float* uvs, int triangles, uint16_t* indices,
                                         uint8_t* material, uint32_t use_matrix);
VertexFunction g_back_vertices = nullptr;
bool g_vertices_key = false;  // gpu_mesh_vertices
bool g_vertices_on = false;   // the key now (Ctrl Shift 4)
bool g_vertex_hook_in = false;
float g_vertex_pack[gm::kMostVertices * 8];
uint16_t g_index_pack[gm::kMostTriangles * 3];

void __fastcall HookVertices(uint32_t count_in, float* positions, uint32_t* colors, float* uvs,
                             int triangles, uint16_t* indices, uint8_t* material,
                             uint32_t use_matrix) {
  const uint32_t count = count_in & 0xFFFFu;
  if (!g_active || !g_vertices_on) {
    g_back_vertices(count_in, positions, colors, uvs, triangles, indices, material, use_matrix);
    return;
  }
  const uint64_t start = __rdtsc();
  auto cpu = [&](uint32_t* reason) {
    if (reason != nullptr) ++*reason;
    ++g_counts.v_cpu;
    g_back_vertices(count_in, positions, colors, uvs, triangles, indices, material, use_matrix);
  };
  if (count == 0 || count > gm::kMostVertices || triangles <= 0 ||
      triangles > static_cast<int>(gm::kMostTriangles) || positions == nullptr ||
      colors == nullptr || uvs == nullptr || indices == nullptr) {
    cpu(&g_counts.v_data);
    return;
  }
  if (Read<uint32_t>(kClampOn) != 0) {
    cpu(&g_counts.v_clamp);
    return;
  }
  if (DrawHooksTriangleSpecial(0)) {
    cpu(&g_counts.v_special);
    return;
  }
  // The indices of the triangles: an index every other word, all inside the batch.
  const uint32_t tris = static_cast<uint32_t>(triangles);
  for (uint32_t t = 0; t < tris; ++t) {
    const uint16_t a = indices[6 * t], b = indices[6 * t + 2], c = indices[6 * t + 4];
    if (a >= count || b >= count || c >= count) {
      cpu(&g_counts.v_index);
      return;
    }
    g_index_pack[3 * t] = a;
    g_index_pack[3 * t + 1] = b;
    g_index_pack[3 * t + 2] = c;
  }
  // The camera matrix: with the flag 0 the function takes a copy of the matrix
  // of the camera, and the pointer goes to the copy.
  if (use_matrix == 0) {
    memcpy(reinterpret_cast<void*>(kMatrixCopy), reinterpret_cast<const void*>(kCameraMatrix), 48);
    Write<uintptr_t>(kMatrix, kMatrixCopy);
  }
  const float* matrix = Read<const float*>(kMatrix);
  if (matrix == nullptr) {
    cpu(&g_counts.v_data);
    return;
  }
  gm::Consts c = {};
  gm::SetMatrix(&c, matrix);
  c.near_plane = Read<float>(kNearPlane);
  c.half_x = Read<float>(kHalfX);
  c.half_y = Read<float>(kHalfY);
  c.specular = 0;  // the function gives 0 as the specular color
  c.flags = gm::kVertexOrder | gm::kVertexColor |
            ((g_parts & gm::kPartDepth) != 0 ? gm::kDepthGame : 0u);
  // The shift of the texture: for the material mesh of the draw.
  const uintptr_t shift_mesh = material != nullptr && Read<uint32_t>(kDoUpdateMaterial) != 0
                                   ? reinterpret_cast<uintptr_t>(material)
                                   : Read<uintptr_t>(kCurrentMaterial);
  const bool shifted = Read<uint32_t>(kUvShift) != 0 && shift_mesh != 0 &&
                       (Read<uint8_t>(shift_mesh + 5) & 0x10) == 0;
  if (shifted && (g_parts & gm::kPartUvShift) == 0) {
    cpu(&g_counts.v_state);
    return;
  }
  if (material != nullptr) {
    Write<void*>(kCurrentMesh, material);
    RunMaterial(material);
  }
  gm::DeviceState state = {};
  if (!VkDdrawMeshState(Read<void*>(kDevice), &state) ||
      !gm::StateAllowed(gm::StateReasons(state), g_parts)) {
    cpu(&g_counts.v_state);
    return;
  }
  if (shifted) {
    c.uv_shift_u = Read<float>(kUvShiftU);
    c.uv_shift_v = Read<float>(kUvShiftV);
  }
  for (uint32_t i = 0; i < count; ++i) {
    float* v = g_vertex_pack + 8 * i;
    memcpy(v, positions + 3 * i, 12);
    memcpy(v + 3, uvs + 2 * i, 8);
    memcpy(v + 5, colors + i, 4);
    v[6] = 0.0f;
    v[7] = 0.0f;
  }
  // The first vertex, for the measure of the nearest geometry.
  const gm::Camera cam = gm::CameraSpace(c, positions, nullptr);
  const bool clipped = !(cam.z >= c.near_plane && cam.x <= cam.z && cam.x >= -cam.z &&
                         cam.y <= cam.z && cam.y >= -cam.z);
  const float first_field =
      clipped ? cam.z : (cam.z != 0.0f ? c.near_plane * (1.0f / cam.z) : 0.0f);
  const uint64_t back_ticks_start = __rdtsc();
  uint64_t back_ticks = 0;
  if (g_mode == 2) {
    OneWalkSetMode(kOneWalkLeftOnly);
    g_back_vertices(count_in, positions, colors, uvs, triangles, indices, material, 1);
    back_ticks = __rdtsc() - back_ticks_start;
    OneWalkSetMode(kOneWalkRightOnly);
    VkSceneDrawVertices(c, g_vertex_pack, count, g_index_pack, tris);
    OneWalkSetMode(kOneWalkBoth);
  } else {
    DrawHooksMeshBatch(first_field);
    VkSceneDrawVertices(c, g_vertex_pack, count, g_index_pack, tris);
  }
  g_after_gpu = true;
  ++g_counts.v_gpu;
  g_counts.v_vertices += count;
  g_counts.v_triangles += tris;
  g_counts.v_ticks += __rdtsc() - start - back_ticks;
}

bool Expect(const Site& s) {
  const void* at = reinterpret_cast<const void*>(s.address);
  return !IsBadReadPtr(at, sizeof(s.bytes)) && memcmp(at, s.bytes, sizeof(s.bytes)) == 0;
}

const char* ModeText(int mode) {
  static const char* const kText[3] = {
      "The meshes take the CPU path of the game",
      "The meshes take the GPU path: the renderer transforms, lights, clips and culls them",
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
  const Counts& c = g_counts;
  if (g_mode != 0 || gpu + cpu > 0) {
    const double total = static_cast<double>(gpu) + static_cast<double>(cpu);
    const double gpu_ms = per_ms > 0.0 ? static_cast<double>(c.gpu_ticks) / per_ms : 0.0;
    const double cpu_ms = per_ms > 0.0 ? static_cast<double>(c.cpu_ticks) / per_ms : 0.0;
    const double check_ms = per_ms > 0.0 ? static_cast<double>(c.check_ticks) / per_ms : 0.0;
    ModLog("gpu mesh     : gpu_mesh=%d, parts 0x%X, in 10 s: %u meshes on the GPU path (%.1f "
           "percent; %u vertices, %u triangles, %u offsets), %u on the CPU path. The GPU path by "
           "function: %s %u, %s %u, %s %u, %s %u, %s %u. The GPU path has: blend %u, no depth "
           "write %u, no depth test %u, stage 1 %u, shift of the texture %u. The CPU path by "
           "reason: no data or too large %u, clamp of the screen %u, shift of the texture %u, "
           "group %u, no offsets %u, h_DrawTriangle changes the draw %u, function left out by "
           "gpu_mesh_kinds %u, table full %u, blend %u, no depth write %u, no depth test %u, "
           "stage 1 %u, state of the last frames %u. The CPU path by function: %u, %u, %u, %u, "
           "%u. Meshes with groups on the GPU path %u (%u groups; %u lights of groups checked against "
           "the functions of the game, %u with other bits). Layers (blend or no depth write, depth "
           "test EQUAL or LESSEQUAL) on the GPU path %u, %u of them over the mesh of the draw "
           "before. Records of mesh data %u (%llu bytes), "
           "full sums %u, meshes whose words changed %u (found by the sample %u, found only by the "
           "full sum %u, the longest time since the last full sum %u frames), data given again %u. "
           "CPU batches (DrawTriangle %u, DrawAndClip %u) with blend or no depth write and a depth "
           "test of EQUAL or LESSEQUAL right after a mesh on the GPU path (of %u CPU batches "
           "after a mesh on the GPU path), pairs of layers on the GPU path %u. The renderer has %u meshes in %llu bytes and drew %llu, %llu "
           "with no data, %llu with no room. Time on the frame thread in 10 s: the GPU path %.2f "
           "ms (%.3f us a mesh), the function of the game %.2f ms (%.3f us a mesh), the checks "
           "of the meshes that went to the CPU path %.2f ms",
           g_mode, g_parts, gpu, total > 0.0 ? 100.0 * gpu / total : 0.0, c.vertices, c.triangles,
           c.offsets, cpu, kKindName[0], c.gpu[0], kKindName[1], c.gpu[1], kKindName[2], c.gpu[2],
           kKindName[3], c.gpu[3], kKindName[4], c.gpu[4], c.f_blend, c.f_nozwrite, c.f_noztest,
           c.f_stage1, c.f_uv, c.r_data, c.r_clamp, c.r_uv, c.r_group, c.r_offsets, c.r_special,
           c.r_kind, c.r_table, c.r_blend, c.r_nozwrite, c.r_noztest, c.r_stage1, c.r_cached,
           c.cpu[0], c.cpu[1], c.cpu[2], c.cpu[3], c.cpu[4], c.f_group, c.groups, c.lights,
           c.light_diff, c.f_layer, c.f_layer_same, c.data_sent,
           static_cast<unsigned long long>(c.data_bytes), c.data_full, c.data_changed,
           c.changed_sample, c.changed_full, c.full_age_max, c.data_retry, c.cpu_layer[0],
           c.cpu_layer[1], c.cpu_after_gpu, c.f_layer_pairs, renderer.meshes, static_cast<unsigned long long>(renderer.arena_bytes),
           static_cast<unsigned long long>(renderer.draws),
           static_cast<unsigned long long>(renderer.no_data),
           static_cast<unsigned long long>(renderer.no_room), gpu_ms,
           gpu > 0 ? gpu_ms * 1000.0 / gpu : 0.0, cpu_ms, cpu > 0 ? cpu_ms * 1000.0 / cpu : 0.0,
           check_ms);
    if (g_vertices_on || c.v_gpu + c.v_cpu > 0) {
      ModLog("gpu mesh     : batches of vertices (fn_0081C090, gpu_mesh_vertices=%d) in 10 s: %u on the "
             "GPU path (%u vertices, %u triangles), %u on the CPU path (clamp of the screen %u, "
             "h_DrawTriangle changes the draw %u, no data %u, an index out of the batch %u, state "
             "%u), %.3f us a batch on the GPU path",
             g_vertices_on ? 1 : 0, c.v_gpu, c.v_vertices, c.v_triangles, c.v_cpu, c.v_clamp,
             c.v_special, c.v_data, c.v_index, c.v_state,
             c.v_gpu > 0 && per_ms > 0.0 ? static_cast<double>(c.v_ticks) / per_ms * 1000.0 / c.v_gpu
                                         : 0.0);
    }
    ModLog("gpu mesh     : the renderer in 10 s: %llu draws with groups stopped because the draw has "
           "fewer matrices than the data have groups, %llu draws with groups left out because the "
           "buffers of the groups failed, %u meshes wait for a range (%llu bytes), %u free ranges "
           "in the arena, the buffer of the groups used %llu bytes of 8388608 in the largest walk, "
           "%u pipelines of 64",
           static_cast<unsigned long long>(renderer.bad_groups),
           static_cast<unsigned long long>(renderer.groups_off), renderer.wait_count,
           static_cast<unsigned long long>(renderer.wait_bytes), renderer.free_ranges,
           static_cast<unsigned long long>(renderer.group_peak), renderer.pipelines);
    if (g_trace) {
      ModLog("gpu mesh     : trace (gpu_mesh_trace=1) in 10 s: %u meshes on the GPU path outside an eye "
             "pass, %u frames of more than 120 ms between two frames of the hook (the longest %u ms, "
             "with %u meshes on the GPU path in the frame before it)",
             c.trace_outside, c.trace_long, c.trace_longest_ms, c.trace_meshes_before_long);
      for (const TraceCaller& k : g_trace_callers) {
        if (k.address == 0) continue;
        ModLog("gpu mesh     : trace: the five functions called from 0x%08X: %u meshes on the GPU "
               "path, %u with blend, %u outside an eye pass",
               k.address, k.gpu, k.blend, k.outside);
      }
    }
    if (g_latch_on || c.latch_new + c.r_latch > 0) {
      ModLog("gpu mesh     : latch (gpu_mesh_latch=%d) in 10 s: %u 3D parts latched for the first "
             "time (by fn_00851870 %u, fn_00859D90 %u, fn_0084E200 %u, fn_0085A6F0 %u, "
             "fn_00850FC0 %u, fn_0085C7F0 %u), %u layers kept a part latched, %u new latches took "
             "the place of a live one (the table has 4096 places), %u parts latched now, %u meshes "
             "took the CPU path because their part is latched",
             g_latch_on ? 1 : 0, c.latch_new, c.latch_by[0], c.latch_by[1], c.latch_by[2],
             c.latch_by[3], c.latch_by[4], c.latch_by[5], c.latch_refresh, c.latch_full,
             c.latch_live, c.r_latch);
    }
    if (g_timer && c.mat_runs > 0 && per_ms > 0.0) {
      ModLog("gpu mesh     : the material in 10 s: %u runs, %u changed a state of the device, %u "
             "ran for the mesh of the run before with no change of the device since, %u skipped "
             "(gpu_mesh_material_skip=%d). Time of a run in us: the function of the type %.3f, the "
             "tiling %.3f, the cull mode %.3f",
             c.mat_runs, c.mat_changed, c.mat_repeat, c.mat_skipped, g_material_skip ? 1 : 0,
             c.mat_ticks[0] / per_ms * 1000.0 / c.mat_runs,
             c.mat_ticks[1] / per_ms * 1000.0 / c.mat_runs,
             c.mat_ticks[2] / per_ms * 1000.0 / c.mat_runs);
    }
    if (c.equal_gpu + c.equal_cpu[0] + c.equal_cpu[1] > 0) {
      ModLog("gpu mesh     : draws with the depth test EQUAL in 10 s (the renderer draws them as "
             "LESS_OR_EQUAL with a small depth bias with vk_equal_depth=1): %u meshes on the GPU "
             "path (fn_00856D40 %u, fn_0085AB10 %u, fn_0084BA90 %u, fn_0085BA30 %u, fn_00859530 "
             "%u), %u batches of the game on the CPU path by DrawTriangle and %u by DrawAndClip",
             c.equal_gpu, c.equal_gpu_kind[0], c.equal_gpu_kind[1], c.equal_gpu_kind[2],
             c.equal_gpu_kind[3], c.equal_gpu_kind[4], c.equal_cpu[0], c.equal_cpu[1]);
      for (const Caller& k : g_equal_callers) {
        if (k.count == 0) continue;
        ModLog("gpu mesh     : a batch of the game with the depth test EQUAL (%s) from 0x%08X, "
               "%u times in 10 s", k.via == 0 ? "DrawTriangle" : "DrawAndClip", k.address, k.count);
      }
    }
    if (c.cpu_layer[0] + c.cpu_layer[1] > 0) {
      for (const Caller& k : g_callers) {
        if (k.count == 0) continue;
        ModLog("gpu mesh     : a layer on the CPU path (%s) right after a mesh on the GPU path, from "
               "0x%08X, %u times in 10 s", k.via == 0 ? "DrawTriangle" : "DrawAndClip", k.address,
               k.count);
      }
    }
    if (g_timer && gpu > 0 && per_ms > 0.0) {
      // The parts of the hook for a mesh on the GPU path, in us for a mesh.
      // "record" has the stream write and the draw of the renderer.
      ModLog("gpu mesh     : gpu_mesh_timer=1, in 10 s for a mesh on the GPU path in us: %s %.3f, "
             "%s %.3f, %s %.3f, %s %.3f, %s %.3f, %s %.3f. The checks of a mesh that went to the "
             "CPU path are in the first part too",
             kPartName[0], c.part_ticks[0] / per_ms * 1000.0 / gpu, kPartName[1],
             c.part_ticks[1] / per_ms * 1000.0 / gpu, kPartName[2],
             c.part_ticks[2] / per_ms * 1000.0 / gpu, kPartName[3],
             c.part_ticks[3] / per_ms * 1000.0 / gpu, kPartName[4],
             c.part_ticks[4] / per_ms * 1000.0 / gpu, kPartName[5],
             c.part_ticks[5] / per_ms * 1000.0 / gpu);
      // The meshes by material type: on the GPU path (all, with blend, with no
      // depth write or test, with stage 1, with a shift of the texture, with
      // groups) and on the CPU path.
      char types[1024];
      int at = 0;
      for (int t = 0; t < 32 && at < 900; ++t) {
        if (c.t_gpu[t] + c.t_cpu[t] == 0) continue;
        at += _snprintf_s(types + at, sizeof(types) - at, _TRUNCATE,
                          "%s%d: %u (%u, %u, %u, %u, %u) and %u", at > 0 ? ", " : "", t, c.t_gpu[t],
                          c.t_blend[t], c.t_nowrite[t], c.t_stage1[t], c.t_uv[t], c.t_group[t],
                          c.t_cpu[t]);
      }
      ModLog("gpu mesh     : by material type (type: GPU path (blend, no depth write or test, "
             "stage 1, shift of the texture, group) and CPU path): %s", types);
    }
  }
  g_counts = Counts();
  memset(g_callers, 0, sizeof(g_callers));
  memset(g_equal_callers, 0, sizeof(g_equal_callers));
  memset(g_trace_callers, 0, sizeof(g_trace_callers));
}

bool InstallVertexHook() {
  if (g_vertex_hook_in) return true;
  if (IsBadReadPtr(reinterpret_cast<const void*>(kVertexSite), 5) ||
      memcmp(reinterpret_cast<const void*>(kVertexSite), kVertexBytes, 5) != 0) {
    ModLog("gpu mesh     : the code at the start of fn_0081C090 is not the code of the game that "
           "the mod fits, so its batches take the CPU path");
    return false;
  }
  void* target = reinterpret_cast<void*>(kVertexSite);
  void* back = nullptr;
  g_vertex_hook_in = MH_CreateHook(target, reinterpret_cast<void*>(&HookVertices), &back) == MH_OK &&
                     MH_EnableHook(target) == MH_OK;
  g_back_vertices = reinterpret_cast<VertexFunction>(back);
  ModLog("hooks        : the batches of vertices fn_0081C090 at 0x%08X %s",
         static_cast<unsigned>(kVertexSite), g_vertex_hook_in ? "ok" : "FAILED");
  return g_vertex_hook_in;
}

bool InstallHooks() {
  if (g_in) return true;
  for (int k = 0; k < kKinds; ++k) {
    if (!Expect(kSites[k])) {
      ModLog("gpu mesh     : the code at the start of %s is not the code of the game that the "
             "mod fits, so the meshes take the CPU path", kKindName[k]);
      return false;
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
  if (g_in && g_vertices_key) g_vertices_on = InstallVertexHook();
  return g_in;
}

}  // namespace

void GpuMeshSetOptions(const GpuMeshOptions& o) {
  g_compare_every = o.compare_every;
  g_kinds = g_base_kinds = static_cast<uint32_t>(o.kinds) & 31u;
  g_mode = o.mode < 0 || o.mode > 2 ? 0 : o.mode;
  g_test_keys = o.test_keys;
  g_parts_keys = (o.blend != 0 ? gm::kPartBlend : 0u) | (o.stage1 != 0 ? gm::kPartStage1 : 0u) |
                 (o.uvshift != 0 ? gm::kPartUvShift : 0u) | (o.group != 0 ? gm::kPartGroup : 0u) |
                 (o.depth != 0 ? gm::kPartDepth : 0u);
  g_parts = g_parts_keys;
  g_check_frames = o.check_frames < 0 ? 0u : static_cast<uint32_t>(o.check_frames);
  g_timer = o.timer != 0;
  g_vertices_key = o.vertices != 0;
  g_trace = o.trace != 0;
  g_latch_key = g_latch_on = o.latch != 0;
  g_material_skip = o.material_skip != 0;
  VkSceneSetMesh(g_mode == 2 ? 1 : 0);
  if (g_mode == 0 && !g_test_keys) return;
  ModLog("gpu mesh     : gpu_mesh=%d, gpu_mesh_kinds=%d, parts 0x%X (blend %d, stage1 %d, uvshift "
         "%d, group %d, depth %d), gpu_mesh_check_frames=%d, gpu_mesh_timer=%d, gpu_mesh_vertices=%d, gpu_mesh_latch=%d, "
         "gpu_mesh_material_skip=%d. %s",
         g_mode, o.kinds, g_parts, o.blend, o.stage1, o.uvshift, o.group, o.depth, o.check_frames,
         o.timer, o.vertices, o.latch, o.material_skip, ModeText(g_mode));
}

void GpuMeshInstall() {
  // With gpu_mesh=0 the game runs as it did before the hooks: the test key
  // puts them in at its first press.
  if (g_in || g_mode == 0) return;
  if (!InstallHooks()) g_mode = 0;
}

// h_DrawTriangle (via 0) and h_DrawAndClip (via 1), at their start. A batch with
// blend or with no depth write, and a depth test of EQUAL or LESSEQUAL, right
// after a mesh on the GPU path is a layer on the CPU path over a mesh on the
// GPU path. Such a pair has the depth of two paths. The log counts them and
// keeps the first 8 return addresses.
void GpuMeshCpuBatch(uint32_t caller, int via) {
  // The batches of the game with the depth test EQUAL, by caller.
  if (g_in) {
    gm::DeviceState equal = {};
    if (VkDdrawMeshState(Read<void*>(kDevice), &equal) && equal.z_enable != 0 && equal.z_func == 3) {
      ++g_counts.equal_cpu[via & 1];
      Caller* slot = nullptr;
      for (Caller& c : g_equal_callers) {
        if (c.count != 0 && c.address == caller && c.via == static_cast<uint32_t>(via)) {
          slot = &c;
          break;
        }
        if (slot == nullptr && c.count == 0) slot = &c;
      }
      if (slot != nullptr) {
        slot->address = caller;
        slot->via = static_cast<uint32_t>(via);
        ++slot->count;
      }
    }
  }
  // The draw of the function of the game for a mesh of the five functions is not
  // a batch of another caller.
  if (g_in_cpu_mesh) return;
  const bool after_mesh = g_last_valid;
  const bool after_gpu = g_after_gpu;
  g_last_valid = false;
  g_after_gpu = false;
  if (!after_mesh) return;
  if (after_gpu) ++g_counts.cpu_after_gpu;
  gm::DeviceState state = {};
  if (!VkDdrawMeshState(Read<void*>(kDevice), &state)) return;
  const uint32_t reasons = gm::StateReasons(state);
  if ((reasons & (gm::kStateBlend | gm::kStateNoZWrite)) == 0 ||
      (state.z_func != 3 && state.z_func != 4)) {
    return;
  }
  if (after_gpu) {
    ++g_counts.cpu_layer[via & 1];
    bool found = false;
    for (Caller& c : g_callers) {
      if (c.count != 0 && c.address == caller && c.via == static_cast<uint32_t>(via)) {
        ++c.count;
        found = true;
        break;
      }
    }
    for (Caller& c : g_callers) {
      if (found) break;
      if (c.count == 0) {
        c.address = caller;
        c.via = static_cast<uint32_t>(via);
        c.count = 1;
        break;
      }
    }
  }
  // The latch: a layer from a function of a mesh class right after a mesh of the
  // five functions (GPU or CPU path).
  if (!g_latch_on || via != 0 || g_last_mesh_info.part == 0) return;
  const int function = gm::LayerFunctionOf(caller);
  if (function < 0) return;
  const int set = g_latched.Set(g_last_mesh_info.part, g_frame, g_frame + g_latch_frames);
  if (set == 0) {
    ++g_counts.latch_refresh;
    return;
  }
  if (set == 2) ++g_counts.latch_full;
  g_latch_active = true;
  ++g_counts.latch_new;
  ++g_counts.latch_by[function];
  if (g_latch_said < 12) {
    ++g_latch_said;
    ModLog("gpu mesh     : latch %u: the 3D part 0x%08X goes to the CPU path for %u frames, because a "
           "layer from %s (0x%08X, blend %u, z write %u, zfunc %u) follows its mesh 0x%08X (type %u, "
           "%u vertices, %s, on the %s path)",
           g_latch_said, static_cast<unsigned>(g_last_mesh_info.part), g_latch_frames,
           gm::kLayerFunctions[function].name, caller, state.alpha_blend, state.z_write, state.z_func,
           static_cast<unsigned>(g_last_mesh_info.mesh), g_last_mesh_info.type,
           g_last_mesh_info.vertices, kKindName[g_last_mesh_info.kind],
           g_last_mesh_info.gpu ? "GPU" : "CPU");
  }
}

int GpuMeshValue() {
  if (g_mode == 0 && !g_in && !g_test_keys) return kDebugCannot;
  return g_mode == 1 ? 1 : 0;
}

void GpuMeshToggle(const char* keys) {
  if (g_mode == 0 && !g_in && !InstallHooks()) return;
  g_mode = g_mode == 1 ? 0 : 1;
  VkSceneSetMesh(0);
  g_active = g_mode != 0 && VkDdrawOn() && VkSceneDirect();
  ControllersGesturePulse(0.5f, g_mode == 1 ? 0.30f : 0.08f);
  ModLog("test key     : %s, gpu_mesh is %d from the next mesh", keys, g_mode);
}

int GpuMeshPartsValue() {
  if (!g_in) return kDebugCannot;
  return g_parts == g_parts_keys ? 1 : 0;
}

void GpuMeshPartsToggle(const char* keys) {
  g_parts = g_parts == g_parts_keys ? 0u : g_parts_keys;
  ControllersGesturePulse(0.5f, g_parts == g_parts_keys ? 0.30f : 0.08f);
  ModLog("test key     : %s, the parts of the GPU path are 0x%X from the next mesh", keys, g_parts);
}

// The compare samples (gpu_mesh_compare_every=N): each Nth frame the next frame
// gives its difference to the log, and the functions on the GPU path go
// through 6 sets in turn: all of them (gpu_mesh_kinds), then each function
// alone. So one run gives the difference for each function.
namespace {
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
}  // namespace

// True while a window of the game process is in front: the test keys act only
// then.
static bool GameWindowInFront() {
  const HWND window = GetForegroundWindow();
  if (window == nullptr) return false;
  DWORD process = 0;
  GetWindowThreadProcessId(window, &process);
  return process == GetCurrentProcessId();
}

void GpuMeshFrame() {
  // Ctrl Shift 7, 6, 5, 4 and 3 (the digits of the top row): no other test key
  // uses them.
  if (g_test_keys) {
    const bool ctrl_shift = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                            (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool down = ctrl_shift && (GetAsyncKeyState('7') & 0x8000) != 0;
    if (down && !g_key_down && GameWindowInFront()) {
      if (g_mode == 2) {
        g_mode = 0;
        VkSceneSetMesh(0);
        ModLog("test key     : Ctrl Shift 7, gpu_mesh is 0 from the next mesh");
      } else {
        GpuMeshToggle("Ctrl Shift 7");
      }
    }
    g_key_down = down;
    const bool parts_down = ctrl_shift && (GetAsyncKeyState('6') & 0x8000) != 0;
    if (parts_down && !g_parts_key_down && g_in && GameWindowInFront()) {
      GpuMeshPartsToggle("Ctrl Shift 6");
    }
    g_parts_key_down = parts_down;
    // Ctrl Shift 5: the depth of the game alone (gpu_mesh_depth), with the other
    // parts as they are. Off, the renderer uses the vertex shader with the
    // linear depth and no invariant (the LEGACY variants).
    const bool depth_down = ctrl_shift && (GetAsyncKeyState('5') & 0x8000) != 0;
    if (depth_down && !g_depth_key_down && g_in && GameWindowInFront()) {
      g_parts ^= gm::kPartDepth;
      ControllersGesturePulse(0.5f, (g_parts & gm::kPartDepth) != 0 ? 0.30f : 0.08f);
      ModLog("test key     : Ctrl Shift 5, gpu_mesh_depth is %d from the next mesh (the parts are "
             "0x%X)", (g_parts & gm::kPartDepth) != 0 ? 1 : 0, g_parts);
    }
    g_depth_key_down = depth_down;
    // Ctrl Shift 4: the batches of vertices of fn_0081C090 on the GPU path or on
    // the CPU path (gpu_mesh_vertices). The hook goes in at the first press.
    const bool vertices_down = ctrl_shift && (GetAsyncKeyState('4') & 0x8000) != 0;
    if (vertices_down && !g_vertices_key_down && g_in && GameWindowInFront()) {
      if (!g_vertices_on && !InstallVertexHook()) {
        // no hook: the batches stay on the CPU path
      } else {
        g_vertices_on = !g_vertices_on;
        ControllersGesturePulse(0.5f, g_vertices_on ? 0.30f : 0.08f);
        ModLog("test key     : Ctrl Shift 4, gpu_mesh_vertices is %d from the next batch",
               g_vertices_on ? 1 : 0);
      }
    }
    g_vertices_key_down = vertices_down;
    // Ctrl Shift 3: the latch (gpu_mesh_latch) on or off.
    const bool latch_down = ctrl_shift && (GetAsyncKeyState('3') & 0x8000) != 0;
    if (latch_down && !g_latch_key_down && g_in && GameWindowInFront()) {
      g_latch_on = !g_latch_on;
      ControllersGesturePulse(0.5f, g_latch_on ? 0.30f : 0.08f);
      ModLog("test key     : Ctrl Shift 3, gpu_mesh_latch is %d from the next mesh", g_latch_on ? 1 : 0);
    }
    g_latch_key_down = latch_down;
    // Ctrl Shift 2: the depth test EQUAL with a small bias (vk_equal_depth) on or
    // off. It acts on the draws that follow, on the PC and in the app.
    const bool equal_down = ctrl_shift && (GetAsyncKeyState('2') & 0x8000) != 0;
    if (equal_down && !g_equal_key_down && GameWindowInFront()) {
      VkSceneEqualDepthToggle("Ctrl Shift 2");
    }
    g_equal_key_down = equal_down;
  }
  if (!g_in) return;
  ++g_frame;
  g_epoch = VkStreamMeshEpoch();
  if (g_trace) {
    LARGE_INTEGER q, f;
    QueryPerformanceCounter(&q);
    QueryPerformanceFrequency(&f);
    const uint64_t now = static_cast<uint64_t>(q.QuadPart);
    if (g_trace_frame_qpc != 0) {
      const uint32_t ms = static_cast<uint32_t>((now - g_trace_frame_qpc) * 1000 /
                                                static_cast<uint64_t>(f.QuadPart));
      if (ms > 120) {
        ++g_counts.trace_long;
        if (ms > g_counts.trace_longest_ms) {
          g_counts.trace_longest_ms = ms;
          g_counts.trace_meshes_before_long = g_trace_meshes_in_frame;
        }
      }
    }
    g_trace_frame_qpc = now;
    g_trace_meshes_in_frame = 0;
  }
  if (g_latch_active) {
    g_counts.latch_live = g_latched.Prune(g_frame);
    g_latch_active = g_counts.latch_live != 0;
  }
  g_active = g_mode != 0 && VkDdrawOn() && VkSceneDirect();
  Report();
  Sample();
}
