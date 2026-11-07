// THE SEA WITH DEPTH (sea_depth). The math is in sea_depth_math.h. See
// docs/backend.md, "The sea with depth".
//
// WHAT THE GAME DRAWS. GLandscape::Draw draws the sky, the reflection of the
// land (fn_007FF4F0, with no depth write), the light of the Hand on the
// water at night (fn_005E3F70, called at 0x005E4E6B), the sea (LH3DWater::
// Draw 0x00879930, called at 0x005E4E8C), and then the land (fn_007FF610).
// The sea has the depth test ALWAYS and no depth write, so it is a
// translucent background, and the land draws over it. The land draw
// (fn_00874AA0) puts a vertex with an altitude byte of 3 or less at the
// height 0, in the plane of the sea. In a block with water it gives a vertex
// with a byte of 0 or 1 an alpha of 0 (the alpha of the specular color, which
// the land and the waves on the coast use). So the coast is a flat band of
// land on the plane of the sea that fades into the sea.
//
// sea_depth=1 keeps that order of the draws and changes the land of the sea
// in a block with water:
//  1. A vertex of the sea goes under the plane of the sea, by the game's
//     step of a byte (sea_depth_math.h, FloorHeight). FloorStubEcx and
//     FloorStubEdi take the place of the load of 0.0 at 0x00874BB3 and at
//     0x0087508B (the loops with no fog and with fog).
//  2. The alpha of a vertex of the sea falls with its depth (FloorAlpha).
//     The test of the cut (cmp eax, 1) tests 3, and AlphaStub takes the
//     place of the cut, so it gives each byte of the sea its alpha.
// So the land goes on down under the water, and it fades into the sea that
// draws behind it, as the coast of the game does. The fade has no hard
// line, because the sea and the land have no depth test between them.
//
// The light of the Hand is a square of 120 units at the height 0, with the
// depth test ALWAYS and the depth write on. Its depth would hide the land
// under the water in that square. With sea_depth=1 it draws with no depth
// write (GlowHere). The sea draws over it, as in the game.
//
// The reflection pass draws the land through the same code. It keeps the
// game's tables: the call at 0x005E48DD goes to Reflection.
//
// The patches stay in place, and the tables decide. With sea_depth=0 the
// stubs give the game's values, and the light writes its depth. The changes
// need the x87 path of the land draw (0x00EA9EB4 at 0). The game takes its
// SSE path (fn_007A1800) only on a Pentium 4.
//
// Once a frame, before the scene walk (WaterModeApply), the mod latches the
// tables of the frame, so each eye pass and each block of a frame draws the
// same floor.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdint.h>
#include <string.h>

#include "../common/bw_addresses.h"
#include "debug_menu.h"
#include "sea_depth_math.h"

extern void ModLog(const char* format, ...);
uint64_t ControllersFrameCount();
void ControllersGesturePulse(float amplitude, float seconds);

namespace {

// The call sites in GLandscape::Draw, and the five bytes of each call.
const uintptr_t kReflectionCall = 0x005E48DD;  // call fn_007FF4F0
const uintptr_t kGlowCall = 0x005E4E6B;        // call fn_005E3F70
const uint8_t kReflectionBytes[5] = {0xE8, 0x0E, 0xAC, 0x21, 0x00};
const uint8_t kGlowBytes[5] = {0xE8, 0x00, 0xF1, 0xFF, 0xFF};
const uintptr_t kReflectionDraw = 0x007FF4F0;
const uintptr_t kGlowDraw = 0x005E3F70;

// The land draw fn_00874AA0. In each of its two vertex loops:
//   cmp eax, 3 / ... / jg (land)        eax is the altitude byte
//   mov reg, [block +0x928]             a block with water: ecx, then edi
//   fld dword ptr [0x008AA398]          the height 0.0 of the sea
//   test reg, reg / je                  (ecx: the test is before the fld)
//   cmp eax, 1 / jg                     the bytes 0 and 1
//   and dword ptr [esi+14h], 0FFFFFFh   the alpha 0
const uintptr_t kFloorSites[2] = {0x00874BB3, 0x0087508B};
const uint8_t kFloorBytes[6] = {0xD9, 0x05, 0x98, 0xA3, 0x8A, 0x00};
const uintptr_t kCutTestSites[2] = {0x00874BBB, 0x00875095};
const uint8_t kCutTestBytes[3] = {0x83, 0xF8, 0x01};
const uintptr_t kCutSites[2] = {0x00874BC0, 0x0087509A};
const uint8_t kCutBytes[7] = {0x81, 0x66, 0x14, 0xFF, 0xFF, 0xFF, 0x00};

// The cache of the depth write of the game (D3DRENDERSTATE_ZWRITEENABLE).
const uintptr_t kZWriteCache = 0x00ECA228;

// The flag of the SSE path of the land draw (LH3DP3::IsPentium4).
const uintptr_t kSsePath = 0x00EA9EB4;

typedef void(__cdecl* DrawFn)();
typedef void(__cdecl* GlowFn)(float x, uint32_t y, float z, uint32_t color);

// The setting. The debug window changes it.
bool g_loaded = false;
bool g_installed = false;
int g_depth = 1;

// The state of this frame.
bool g_frame_depth = false;

// The tables of the stubs. The stubs read the rows that the pointers give:
// the rows of this frame, or the rows of the game in the reflection pass.
float g_heights[sea_depth::kSeaBytes] = {};
uint8_t g_alphas[sea_depth::kSeaBytes] = {0, 0, 255, 255};
const float g_game_heights[sea_depth::kSeaBytes] = {};
const uint8_t g_game_alphas[sea_depth::kSeaBytes] = {0, 0, 255, 255};
const float* g_height_row = g_game_heights;
const uint8_t* g_alpha_row = g_game_alphas;
const float g_zero = 0.0f;

// The counts of a report window.
uint64_t g_frames = 0;
uint64_t g_frames_depth = 0;
uint64_t g_glows = 0;
uint64_t g_glows_flat = 0;

IDirect3DDevice7* Device() {
  return *reinterpret_cast<IDirect3DDevice7* const*>(
      bw::render::kDirect3DDevice7);
}

// The height of a vertex of the sea: in st0, as the fld of the game puts it.
// eax holds the altitude byte, 0 to 3, and ecx the flag of a block with
// water. The code after the call reads the flags of the test before it, so
// no instruction here changes the flags (jecxz does not).
__declspec(naked) void FloorStubEcx() {
  __asm {
    jecxz game
    push ecx
    mov ecx, dword ptr [g_height_row]
    fld dword ptr [ecx + eax * 4]
    pop ecx
    ret
  game:
    fld dword ptr [g_zero]
    ret
  }
}

// The same in the loop with fog, where edi holds the flag. Here the test of
// the flag comes after the call, so the flags can change.
__declspec(naked) void FloorStubEdi() {
  __asm {
    test edi, edi
    jz game
    push ecx
    mov ecx, dword ptr [g_height_row]
    fld dword ptr [ecx + eax * 4]
    pop ecx
    ret
  game:
    fld dword ptr [g_zero]
    ret
  }
}

// The alpha of a vertex of the sea in a block with water: the byte 3 of the
// specular color at [esi+14h]. eax holds the altitude byte, 0 to 3.
__declspec(naked) void AlphaStub() {
  __asm {
    push ecx
    push edx
    mov edx, dword ptr [g_alpha_row]
    mov cl, byte ptr [edx + eax]
    mov byte ptr [esi + 0x17], cl
    pop edx
    pop ecx
    ret
  }
}

// The depth write through the cache of the game, as GLandscape::Draw sets it
// round the reflection: the cache gets the value, or -1 when the device
// refuses it.
void SetZWrite(int32_t on) {
  volatile int32_t* cache = reinterpret_cast<volatile int32_t*>(kZWriteCache);
  if (*cache == on) return;
  IDirect3DDevice7* device = Device();
  const HRESULT hr =
      device != nullptr
          ? device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, on)
          : E_FAIL;
  *cache = hr == D3D_OK ? on : -1;
}

// The call of the light of the Hand at 0x005E4E6B. The caller removes the
// arguments.
void __cdecl GlowHere(float x, uint32_t y, float z, uint32_t color) {
  ++g_glows;
  const GlowFn draw = reinterpret_cast<GlowFn>(kGlowDraw);
  if (!g_frame_depth) {
    draw(x, y, z, color);
    return;
  }
  ++g_glows_flat;
  SetZWrite(0);
  draw(x, y, z, color);
  SetZWrite(1);
}

// The call of the reflection at 0x005E48DD, with the tables of the game.
void Reflection() {
  g_height_row = g_game_heights;
  g_alpha_row = g_game_alphas;
  reinterpret_cast<DrawFn>(kReflectionDraw)();
  g_height_row = g_heights;
  g_alpha_row = g_alphas;
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
  return on != 0 ? "the land of the sea goes on down under the water and "
                   "fades into the sea by its depth"
                 : "the flat coast of the game";
}

}  // namespace

// water_mode.cpp, WaterModeSetSimple, one time at the start.
void SeaDepthInstall() {
  Load();
  if (g_installed) return;
  bool same = SameBytes(kReflectionCall, kReflectionBytes, 5) &&
              SameBytes(kGlowCall, kGlowBytes, 5);
  for (int i = 0; i < 2; ++i) {
    same = same && SameBytes(kFloorSites[i], kFloorBytes, 6) &&
           SameBytes(kCutTestSites[i], kCutTestBytes, 3) &&
           SameBytes(kCutSites[i], kCutBytes, 7);
  }
  if (!same) {
    ModLog("sea depth    : the code of the land is not the code that this "
           "was read from, so nothing changes (sea_depth=%d has no effect)",
           g_depth);
    return;
  }
  bool ok = WriteCall(kFloorSites[0], &FloorStubEcx, 1);
  ok = WriteCall(kFloorSites[1], &FloorStubEdi, 1) && ok;
  for (int i = 0; i < 2; ++i) {
    const uint8_t test[3] = {0x83, 0xF8, 0x03};  // cmp eax, 3
    ok = Write(kCutTestSites[i], test, 3) && ok;
    ok = WriteCall(kCutSites[i], &AlphaStub, 2) && ok;
  }
  ok = WriteCall(kReflectionCall, reinterpret_cast<const void*>(&Reflection),
                 0) && ok;
  ok = WriteCall(kGlowCall, reinterpret_cast<const void*>(&GlowHere), 0) &&
       ok;
  g_installed = ok;
  ModLog("sea depth    : sea_depth=%d: %s%s", g_depth, DepthWords(g_depth),
         ok ? "" : ". A write to the code FAILED, so the state of the code is "
                   "not known");
}

// water_mode.cpp, WaterModeApply: once a frame, before the scene walk.
void SeaDepthFrame() {
  if (!g_installed) return;
  ++g_frames;
  const bool x87 = *reinterpret_cast<const int32_t*>(kSsePath) == 0;
  g_frame_depth = g_depth != 0 && x87;
  if (g_frame_depth) ++g_frames_depth;
  sea_depth::FillTables(g_frame_depth, g_heights, g_alphas);
  g_height_row = g_heights;
  g_alpha_row = g_alphas;
}

// Once a report window.
void SeaDepthReport() {
  if (!g_installed || g_frames == 0) return;
  const bool x87 = *reinterpret_cast<const int32_t*>(kSsePath) == 0;
  ModLog("sea depth    : %llu frames, %llu with the floor under the water, "
         "%llu draws of the light of the Hand on the water, %llu of them with "
         "no depth write%s", g_frames, g_frames_depth, g_glows, g_glows_flat,
         g_depth != 0 && !x87 ? ". The coast is flat, because the land draw "
                                "takes the SSE path of the game"
                              : "");
  g_frames = g_frames_depth = g_glows = g_glows_flat = 0;
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
