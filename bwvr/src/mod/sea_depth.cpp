// THE SEA WITH DEPTH (sea_depth). The math is in sea_depth_math.h. See
// docs/backend.md, "The sea with depth".
//
// WHAT THE GAME DRAWS. GLandscape::Draw draws the sky, the reflection of the
// land (fn_007FF4F0, with no depth write), the plain sea (LH3DWater::Draw
// 0x00879930, called at 0x005E4E8C, which goes on to fn_0087A090), and then
// the land (fn_007FF610, called at 0x005E4E96). The plain sea has the depth
// test ALWAYS and no depth write, so it is only a background, and the land
// draws over it. The land draw (fn_00874AA0) puts a vertex with an altitude
// byte of 3 or less at the height 0, in the plane of the sea, and in a block
// with water it gives a vertex with a byte of 0 or 1 an alpha of 0. A second
// pass of a block with water draws the waves on the coast (white foam with
// an alpha that moves across the land). So the coast is a flat band of land
// on the plane of the sea that ends in a fade and in foam, and the sea never
// covers the land.
//
// sea_depth=1 makes four changes:
//  1. The land draw puts a vertex of the sea under the plane of the sea, by
//     the game's step of a byte (sea_depth_math.h, FloorHeight). FloorStub
//     takes the place of the load of 0.0 at 0x00874BB3 and at 0x0087508B
//     (the loops with no fog and with fog).
//  2. The plain sea draws after the land. The call of the sea at 0x005E4E8C
//     keeps the material, and the call of the land at 0x005E4E96 draws the
//     land and then the sea.
//  3. In that draw the plain sea has the depth test LESSEQUAL (SeaZFunc, in
//     place of the code at 0x0087A25D). It still writes no depth.
//  4. The waves on the coast (the DrawTriangle at 0x00875B98) draw after the
//     sea. h_DrawTriangle gives each batch of the waves to SeaDepthTakeBatch,
//     which keeps a copy of its vertices and indices. After the sea,
//     ReplayWaves sets the material of the waves (0x00E9CD64) as the land
//     draw does and draws the copies. The copies have the depth of the land,
//     so they pass the depth test LESSEQUAL where the land is, and the sea
//     writes no depth.
// So the land over the water hides the sea, and the sea covers the land
// under the water, in each eye at the real water line. The foam of the waves
// stays over the water line as in the game, and makes the edge soft. A
// thing that draws after the sea (a villager, the Creature, the hands)
// draws as before.
//
// The reflection pass draws the land through the same code. It keeps the
// game's heights: the call at 0x005E48DD goes to Reflection, which gives the
// stub the table of the game for that call.
//
// The patches stay in place, and the table and the flags decide. With
// sea_depth=0 the stub gives the game's height, and the sea and the waves
// draw at their own places. The changes need the plain sea (the tiling
// float 0x00C38228 at 0, water_simple=1) and the x87 path of the land draw
// (0x00EA9EB4 at 0). The game takes its SSE path of the land (fn_007A1800)
// only on a Pentium 4.
//
// Once a frame, before the scene walk (WaterModeApply), the mod latches the
// state of the frame, so each eye pass and each block of a frame draws the
// same sea.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdint.h>
#include <string.h>

#include <vector>

#include "../common/bw_addresses.h"
#include "debug_menu.h"
#include "sea_depth_math.h"

extern void ModLog(const char* format, ...);
uint64_t ControllersFrameCount();
void ControllersGesturePulse(float amplitude, float seconds);

namespace {

// The call sites in GLandscape::Draw, and the five bytes of each call.
const uintptr_t kReflectionCall = 0x005E48DD;  // call fn_007FF4F0
const uintptr_t kSeaCall = 0x005E4E8C;         // call LH3DWater::Draw
const uintptr_t kLandCall = 0x005E4E96;        // call fn_007FF610
const uint8_t kReflectionBytes[5] = {0xE8, 0x0E, 0xAC, 0x21, 0x00};
const uint8_t kSeaBytes[5] = {0xE8, 0x9F, 0x4A, 0x29, 0x00};
const uint8_t kLandBytes[5] = {0xE8, 0x75, 0xA7, 0x21, 0x00};
const uintptr_t kReflectionDraw = 0x007FF4F0;
const uintptr_t kLandDraw = 0x007FF610;

// The land draw fn_00874AA0. In each of its two vertex loops:
//   cmp eax, 3 / ... / jg (land)     eax is the altitude byte
//   ... fld dword ptr [0x008AA398]   the height 0.0 of the sea
//   test (block +0x928) / je         a block with water
const uintptr_t kLandBegin = 0x00874AA0;
const uintptr_t kLandEnd = 0x00875C60;
const uintptr_t kFloorSites[2] = {0x00874BB3, 0x0087508B};
const uint8_t kFloorBytes[6] = {0xD9, 0x05, 0x98, 0xA3, 0x8A, 0x00};

// The return of the DrawTriangle of the waves on the coast, the pass of the
// material 0x00E9CD64 over the land of a block with water. Before it the
// land draw sets that material and calls fn_0082FEB0(0) and
// SetD3DTillingOn(0). After it, SetD3DTillingOff(0) and fn_0082FED0(0).
const uintptr_t kWavesReturn = 0x00875B9D;
const uintptr_t kWavesMaterial = 0x00E9CD64;  // LH3DMaterial*
const uintptr_t kWavesBefore = 0x0082FEB0;
const uintptr_t kWavesAfter = 0x0082FED0;
const uintptr_t kTillingOn = 0x0082FF10;
const uintptr_t kTillingOff = 0x0082FF50;
const uintptr_t kDrawTriangle = 0x0082F810;
// How the land draw sets a material (0x008753C1 and after).
const uintptr_t kDoUpdateMaterial = 0x00C38714;  // int
const uintptr_t kCurrentMaterial = 0x00ECA64C;   // LH3DMaterial*
const uintptr_t kRenderModeTable = 0x00ECA618;   // pointer, 8 bytes a mode
const uintptr_t kNeedTilling = 0x00ECA614;       // int
const uintptr_t kCullCache = 0x00ECA248;

// The plain sea fn_0087A090 sets the depth test ALWAYS with the cache of the
// state at 0x00ECA24C, from 0x0087A25D to 0x0087A284. SeaZFunc does the same
// with the depth test of the frame.
const uintptr_t kZFuncSite = 0x0087A25D;
const uintptr_t kZFuncAfter = 0x0087A284;
const uint8_t kZFuncBytes[39] = {
    0x83, 0x3D, 0x4C, 0xA2, 0xEC, 0x00, 0x08,  // cmp [0xECA24C], 8
    0x74, 0x1E,                                // je 0x0087A284
    0xA1, 0x38, 0xA6, 0xEC, 0x00,              // mov eax, [device]
    0x8B, 0x10,                                // mov edx, [eax]
    0x6A, 0x08, 0x6A, 0x17, 0x50,              // push 8, push 0x17, push eax
    0xFF, 0x52, 0x50,                          // call SetRenderState
    0xF7, 0xD8, 0x1B, 0xC0,                    // neg eax, sbb eax, eax
    0x83, 0xE0, 0xF7, 0x83, 0xC0, 0x08,        // and eax, -9, add eax, 8
    0xA3, 0x4C, 0xA2, 0xEC, 0x00};             // mov [0xECA24C], eax
const uintptr_t kZFuncCache = 0x00ECA24C;

// The flag of the SSE path of the land draw (LH3DP3::IsPentium4).
const uintptr_t kSsePath = 0x00EA9EB4;

typedef void(__fastcall* SeaDrawFn)(void* material);
typedef void(__cdecl* DrawFn)();
typedef void(__cdecl* ZeroFn)(int zero);
typedef void(__fastcall* RenderModeFn)(void* material, int zero);
typedef void(__fastcall* DrawTriangleFn)(void* verts, uint32_t count,
                                          uint16_t* idx, uint32_t n);

// The setting. The debug window changes it.
bool g_loaded = false;
bool g_installed = false;
int g_depth = 1;

// The state of this frame.
bool g_frame_depth = false;
const char* g_why_plain = nullptr;  // why the sea draws at its place

// The table of the stub. The stub reads the row that the pointer gives: the
// row of this frame, or the row of the game in the reflection pass.
float g_heights[sea_depth::kSeaBytes] = {};
const float g_game_heights[sea_depth::kSeaBytes] = {};
const float* g_height_row = g_game_heights;

// The sea of the walk, kept until the land has drawn.
void* g_sea_material = nullptr;
bool g_sea_waits = false;
bool g_sea_tested = false;  // the depth test of SeaZFunc in this draw
bool g_in_sea = false;      // a draw of the plain sea runs

// The batches of the waves of one land draw, kept until the sea has drawn.
struct WaveBatch {
  size_t vertex_at;  // the first byte in g_wave_vertices
  size_t index_at;   // the first index in g_wave_indices
  uint32_t count;    // vertices
  uint32_t n;        // indices
};
const size_t kVertexBytes = 32;  // FVF 0x1C4
bool g_take_waves = false;
std::vector<uint8_t> g_wave_vertices;
std::vector<uint16_t> g_wave_indices;
std::vector<WaveBatch> g_waves;

// The counts of a report window.
uint64_t g_frames = 0;
uint64_t g_frames_depth = 0;
uint64_t g_seas_after = 0;
uint64_t g_seas_at_place = 0;
uint64_t g_wave_batches = 0;

// The states of the device in the first batch of each kind (the land, the
// waves on the coast, the sea), for one line of the log each.
enum { kKindLand, kKindWaves, kKindSea, kKinds };
const char* const kKindName[kKinds] = {"the land", "the waves on the coast",
                                       "the plain sea"};
bool g_kind_seen[kKinds] = {};
bool g_kind_logged[kKinds] = {};
DWORD g_kind_state[kKinds][9] = {};
const D3DRENDERSTATETYPE kCensusStates[9] = {
    D3DRENDERSTATE_ZENABLE,         D3DRENDERSTATE_ZWRITEENABLE,
    D3DRENDERSTATE_ZFUNC,           D3DRENDERSTATE_ALPHABLENDENABLE,
    D3DRENDERSTATE_SRCBLEND,        D3DRENDERSTATE_DESTBLEND,
    D3DRENDERSTATE_ALPHATESTENABLE, D3DRENDERSTATE_ALPHAREF,
    D3DRENDERSTATE_ALPHAFUNC};

IDirect3DDevice7* Device() {
  return *reinterpret_cast<IDirect3DDevice7* const*>(
      bw::render::kDirect3DDevice7);
}

// The height of a vertex of the sea: in st0, as the fld of the game puts it.
// eax holds the altitude byte, 0 to 3. The code after the call reads the
// flags of a test before it, so no instruction here changes the flags.
__declspec(naked) void FloorStub() {
  __asm {
    push ecx
    mov ecx, dword ptr [g_height_row]
    fld dword ptr [ecx + eax * 4]
    pop ecx
    ret
  }
}

// In place of the depth test ALWAYS of the plain sea. The cache of the game
// gets the value, or -1 when the device refuses it, as the code of the game
// does.
void SeaZFunc() {
  const DWORD want = g_sea_tested ? D3DCMP_LESSEQUAL : D3DCMP_ALWAYS;
  volatile int32_t* cache = reinterpret_cast<volatile int32_t*>(kZFuncCache);
  if (*cache == static_cast<int32_t>(want)) return;
  IDirect3DDevice7* device = Device();
  if (device == nullptr) {
    *cache = -1;
    return;
  }
  const HRESULT hr = device->SetRenderState(D3DRENDERSTATE_ZFUNC, want);
  *cache = hr != D3D_OK ? -1 : static_cast<int32_t>(want);
}

void DrawSea(void* material) {
  g_in_sea = true;
  reinterpret_cast<SeaDrawFn>(bw::tech::kWaterDraw)(material);
  g_in_sea = false;
}

// The material of the waves, as the land draw sets it before their pass.
void SetWavesMaterial() {
  void* material = *reinterpret_cast<void* const*>(kWavesMaterial);
  if (*reinterpret_cast<const int*>(kDoUpdateMaterial) == 0 ||
      material == nullptr) {
    return;
  }
  *reinterpret_cast<void**>(kCurrentMaterial) = material;
  const uintptr_t table =
      *reinterpret_cast<const uintptr_t*>(kRenderModeTable);
  const int mode = *reinterpret_cast<const int*>(material);
  reinterpret_cast<RenderModeFn>(
      *reinterpret_cast<const uintptr_t*>(table + mode * 8))(material, 0);
  const uint8_t flags = reinterpret_cast<const uint8_t*>(material)[5];
  if (*reinterpret_cast<const int*>(kNeedTilling) == 0 && (flags & 4) == 0) {
    reinterpret_cast<ZeroFn>(kTillingOff)(0);
  } else {
    reinterpret_cast<ZeroFn>(kTillingOn)(0);
  }
  const DWORD cull = ((~flags & 1u) << 1) | 1u;
  volatile DWORD* cache = reinterpret_cast<volatile DWORD*>(kCullCache);
  if (*cache == cull) return;
  IDirect3DDevice7* device = Device();
  const HRESULT hr =
      device != nullptr ? device->SetRenderState(D3DRENDERSTATE_CULLMODE, cull)
                        : E_FAIL;
  *cache = hr == D3D_OK ? cull : 0xFFFFFFFFu;
}

// The waves of the land draw, after the sea, with the steps of the land draw
// round their DrawTriangle.
void ReplayWaves() {
  if (g_waves.empty()) return;
  SetWavesMaterial();
  reinterpret_cast<ZeroFn>(kWavesBefore)(0);
  reinterpret_cast<ZeroFn>(kTillingOn)(0);
  const DrawTriangleFn draw = reinterpret_cast<DrawTriangleFn>(kDrawTriangle);
  for (size_t i = 0; i < g_waves.size(); ++i) {
    const WaveBatch& w = g_waves[i];
    draw(&g_wave_vertices[w.vertex_at], w.count, &g_wave_indices[w.index_at],
         w.n);
  }
  reinterpret_cast<ZeroFn>(kTillingOff)(0);
  reinterpret_cast<ZeroFn>(kWavesAfter)(0);
  g_wave_batches += g_waves.size();
  g_waves.clear();
}

// The call of the sea at 0x005E4E8C. ecx has the material.
void __fastcall SeaHere(void* material) {
  if (g_frame_depth) {
    g_sea_material = material;
    g_sea_waits = true;
    return;
  }
  ++g_seas_at_place;
  DrawSea(material);
}

// The call of the land at 0x005E4E96: the land, then the sea that waits,
// then the waves of the land.
void LandThenSea() {
  g_waves.clear();
  g_wave_vertices.clear();
  g_wave_indices.clear();
  g_take_waves = g_sea_waits;
  reinterpret_cast<DrawFn>(kLandDraw)();
  g_take_waves = false;
  if (!g_sea_waits) return;
  g_sea_waits = false;
  ++g_seas_after;
  g_sea_tested = true;
  DrawSea(g_sea_material);
  g_sea_tested = false;
  ReplayWaves();
}

// The call of the reflection at 0x005E48DD, with the table of the game.
void Reflection() {
  g_height_row = g_game_heights;
  reinterpret_cast<DrawFn>(kReflectionDraw)();
  g_height_row = g_heights;
}

bool SameBytes(uintptr_t site, const uint8_t* bytes, size_t n) {
  const void* p = reinterpret_cast<const void*>(site);
  return !IsBadReadPtr(p, n) && memcmp(p, bytes, n) == 0;
}

bool Write(uintptr_t site, const uint8_t* bytes, size_t n) {
  void* p = reinterpret_cast<void*>(site);
  DWORD old = 0;
  if (!VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &old)) return false;
  memcpy(p, bytes, n);
  VirtualProtect(p, n, old, &old);
  FlushInstructionCache(GetCurrentProcess(), p, n);
  return true;
}

// A call to `target` at `site`, and `pad` bytes of nop after it.
bool WriteCall(uintptr_t site, const void* target, size_t pad) {
  uint8_t code[16];
  code[0] = 0xE8;
  const int32_t rel = static_cast<int32_t>(
      reinterpret_cast<uintptr_t>(target) - (site + 5));
  memcpy(code + 1, &rel, sizeof(rel));
  for (size_t i = 0; i < pad; ++i) code[5 + i] = 0x90;
  return Write(site, code, 5 + pad);
}

void Load() {
  if (g_loaded) return;
  g_loaded = true;
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash =
      length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash != nullptr && strlen(path) + 9 < MAX_PATH) {
    slash[1] = 0;
    strcat_s(path, MAX_PATH, "bwvr.ini");
    g_depth = GetPrivateProfileIntA("bwvr", "sea_depth", 1, path) != 0 ? 1 : 0;
  }
}

const char* DepthWords(int on) {
  return on != 0 ? "the sea draws after the land with the depth test, the "
                   "land goes on down under the water, and the waves on the "
                   "coast draw after the sea"
                 : "the plain sea of the game";
}

}  // namespace

// water_mode.cpp, WaterModeSetSimple, one time at the start.
void SeaDepthInstall() {
  Load();
  if (g_installed) return;
  bool same = SameBytes(kReflectionCall, kReflectionBytes, 5) &&
              SameBytes(kSeaCall, kSeaBytes, 5) &&
              SameBytes(kLandCall, kLandBytes, 5) &&
              SameBytes(kZFuncSite, kZFuncBytes, sizeof(kZFuncBytes));
  for (int i = 0; i < 2; ++i) {
    same = same && SameBytes(kFloorSites[i], kFloorBytes, 6);
  }
  if (!same) {
    ModLog("sea depth    : the code of the sea and of the land is not the "
           "code that this was read from, so nothing changes (sea_depth=%d "
           "has no effect)", g_depth);
    return;
  }
  bool ok = true;
  for (int i = 0; i < 2; ++i) {
    ok = WriteCall(kFloorSites[i], &FloorStub, 1) && ok;
  }
  // call SeaZFunc, then a jump over the rest of the old code.
  {
    uint8_t code[sizeof(kZFuncBytes)];
    memset(code, 0x90, sizeof(code));
    code[0] = 0xE8;
    const int32_t rel = static_cast<int32_t>(
        reinterpret_cast<uintptr_t>(&SeaZFunc) - (kZFuncSite + 5));
    memcpy(code + 1, &rel, sizeof(rel));
    code[5] = 0xEB;  // jmp short
    code[6] = static_cast<uint8_t>(kZFuncAfter - (kZFuncSite + 7));
    ok = Write(kZFuncSite, code, sizeof(code)) && ok;
  }
  ok = WriteCall(kReflectionCall, reinterpret_cast<const void*>(&Reflection),
                 0) && ok;
  ok = WriteCall(kSeaCall, reinterpret_cast<const void*>(&SeaHere), 0) && ok;
  ok = WriteCall(kLandCall, reinterpret_cast<const void*>(&LandThenSea), 0) &&
       ok;
  g_installed = ok;
  ModLog("sea depth    : sea_depth=%d: %s%s", g_depth, DepthWords(g_depth),
         ok ? "" : ". A write to the code FAILED, so the state of the code is "
                   "not known");
}

// water_mode.cpp, WaterModeApply: once a frame, before the scene walk, after
// the tiling float of this frame.
void SeaDepthFrame() {
  if (!g_installed) return;
  ++g_frames;
  const bool plain =
      *reinterpret_cast<const float*>(bw::tech::kWaterTiling) == 0.0f;
  const bool x87 = *reinterpret_cast<const int32_t*>(kSsePath) == 0;
  g_why_plain = !plain ? "the scanline sea draws (water_simple=0)"
                : !x87 ? "the land draw takes the SSE path of the game"
                       : nullptr;
  g_frame_depth = g_depth != 0 && plain && x87;
  if (g_frame_depth) ++g_frames_depth;
  sea_depth::FillHeights(g_frame_depth, g_heights);
  g_height_row = g_heights;
  g_sea_waits = false;
}

// draw_hooks.cpp, h_DrawTriangle: each batch of the walk, with the return
// address of its caller. Reads the states of the first batch of each kind.
void SeaDepthBatch(uint32_t caller) {
  if (!g_installed) return;
  int kind = -1;
  if (g_in_sea) {
    kind = kKindSea;
  } else if (caller >= kLandBegin && caller < kLandEnd) {
    kind = caller == kWavesReturn ? kKindWaves : kKindLand;
  }
  if (kind < 0 || g_kind_seen[kind]) return;
  g_kind_seen[kind] = true;
  IDirect3DDevice7* device = Device();
  for (int i = 0; i < 9; ++i) {
    g_kind_state[kind][i] = 99;
    if (device != nullptr) {
      device->GetRenderState(kCensusStates[i], &g_kind_state[kind][i]);
    }
  }
}

// draw_hooks.cpp, at the start of h_DrawTriangle: true when the batch is a
// batch of the waves of a land draw whose sea waits. Then a copy of it draws
// after the sea, and the batch does not draw now.
bool SeaDepthTakeBatch(uint32_t caller, void* verts, uint32_t count,
                       uint16_t* idx, uint32_t n) {
  if (!g_take_waves || caller != kWavesReturn || verts == nullptr ||
      idx == nullptr || count == 0 || n == 0) {
    return false;
  }
  SeaDepthBatch(caller);
  WaveBatch w;
  w.vertex_at = g_wave_vertices.size();
  w.index_at = g_wave_indices.size();
  w.count = count;
  w.n = n;
  const uint8_t* v = static_cast<const uint8_t*>(verts);
  g_wave_vertices.insert(g_wave_vertices.end(), v, v + count * kVertexBytes);
  g_wave_indices.insert(g_wave_indices.end(), idx, idx + n);
  g_waves.push_back(w);
  return true;
}

// Once a report window.
void SeaDepthReport() {
  if (!g_installed) return;
  for (int k = 0; k < kKinds; ++k) {
    if (!g_kind_seen[k] || g_kind_logged[k]) continue;
    g_kind_logged[k] = true;
    const DWORD* s = g_kind_state[k];
    ModLog("sea depth    : the first batch of %s: depth test %lu, depth write "
           "%lu, depth function %lu, blend %lu (source %lu, destination %lu), "
           "alpha test %lu (reference %lu, function %lu)", kKindName[k], s[0],
           s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8]);
  }
  if (g_frames == 0) return;
  ModLog("sea depth    : %llu frames, %llu with the sea after the land: %llu "
         "draws of the sea after the land and %llu at its own place, %llu "
         "batches of the waves after the sea%s%s", g_frames, g_frames_depth,
         g_seas_after, g_seas_at_place, g_wave_batches,
         g_why_plain != nullptr && g_depth != 0 ? ". The sea draws at its own "
                                                  "place, because "
                                                : "",
         g_why_plain != nullptr && g_depth != 0 ? g_why_plain : "");
  g_frames = g_frames_depth = g_seas_after = g_seas_at_place = 0;
  g_wave_batches = 0;
}

// The debug window (debug_menu.cpp).
int SeaDepthValue() {
  Load();
  if (!g_installed) return kDebugCannot;
  return g_depth;
}

void SeaDepthChange(const char* by) {
  Load();
  if (!g_installed) return;
  g_depth = g_depth != 0 ? 0 : 1;
  ControllersGesturePulse(0.5f, g_depth != 0 ? 0.30f : 0.08f);
  ModLog("sea depth    : %s: sea_depth is %d now: %s, from the next frame, "
         "frame %llu", by, g_depth, DepthWords(g_depth),
         ControllersFrameCount());
}
