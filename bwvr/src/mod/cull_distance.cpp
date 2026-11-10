// THE MODEL CULL DISTANCE (cull_distance, cull_fog). The math is in
// cull_math.h. See docs/backend.md, "Models are drawn further out".
//
// WHAT THE GAME DOES. GLandscape fills its list of map objects from the land
// blocks in view, near to far, and stops at the first block that is not
// nearer than [0x00C37200]: 700 units (0x005E5DE6). The same distance ends
// the objects at 0x005E4A99 and the fade of the shadows (fn_00874600). The
// fog is computed on the CPU, linear in the depth from a start to an end. The
// sky sets the two once a frame through fn_007FEAA0 (cdecl, the start and
// the end, plain ret), and its only caller is at 0x00869F59: 400 to 900 units
// in clear weather, and down to 15 to 350 in rain. The function also keeps
// the end less the start at 0x00E9B6DC, which the fog of the objects and of
// the land divides by.
//
// WHAT THE MOD DOES. cull_distance multiplies the 700: tabletop.cpp writes it
// once a frame, with the gain of the God view on top (CullDistanceFactor).
// With cull_fog=1 a hook on fn_007FEAA0 multiplies the start and the end of
// the sky by the same factor, before the game keeps them. So the far edge of
// the objects is at the same share of the fog as in the game, in each
// weather, and the far land is not in fog. Above a gain of 3 the God view
// has the fog off, and the factor of the fog has no effect.
//
// Only with play_mode=3 and 4, where tabletop.cpp writes the distance. Each
// frame latches the values (CullDistanceFrame), so both eye passes of a
// frame have the same distances.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"
#include "cull_math.h"
#include "debug_menu.h"
#include "tabletop.h"

extern void ModLog(const char* format, ...);
uint64_t ControllersFrameCount();
void ControllersGesturePulse(float amplitude, float seconds);

namespace {

// The fog set of the sky, and its first bytes:
//   fld dword ptr [esp+8] / mov eax, dword ptr [esp+4]
const uintptr_t kSetFog = 0x007FEAA0;
const uint8_t kSetFogBytes[8] = {0xD9, 0x44, 0x24, 0x08,
                                 0x8B, 0x44, 0x24, 0x04};
// The map object distance (tabletop.cpp writes it).
const uintptr_t kVanish = 0x00C37200;

typedef void(__cdecl* SetFogFn)(float start, float end);
SetFogFn o_SetFog = nullptr;

// The settings. The debug window changes them.
bool g_loaded = false;
bool g_installed = false;
int g_index = cull_math::kCullDefault;  // cull_distance
int g_fog = 1;                          // cull_fog
// A test for a desktop run: from this frame of the controllers on, each
// g_test_period frames, the cull distance steps as the debug page steps it.
uint64_t g_test_frame = 0;   // cull_test_frame, 0 is never
uint64_t g_test_period = 700;  // cull_test_period
uint64_t g_test_next = 0;

// The values of this frame.
float g_frame_factor = 1.0f;
float g_frame_fog = 1.0f;

// The counts of a report window.
uint64_t g_fog_sets = 0;
uint64_t g_fog_scaled = 0;
float g_sky_start = 0.0f;
float g_sky_end = 0.0f;
float g_used_start = 0.0f;
float g_used_end = 0.0f;

void Load() {
  if (g_loaded) return;
  g_loaded = true;
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash =
      length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  g_index = cull_math::CullIndex(static_cast<int>(GetPrivateProfileIntA(
      "bwvr", "cull_distance", cull_math::kCullDefault, path)));
  g_fog = GetPrivateProfileIntA("bwvr", "cull_fog", 1, path) != 0 ? 1 : 0;
  g_test_frame = GetPrivateProfileIntA("bwvr", "cull_test_frame", 0, path);
  g_test_period = GetPrivateProfileIntA("bwvr", "cull_test_period", 700, path);
  if (g_test_period < 60) g_test_period = 60;
  g_test_next = g_test_frame;
}

const char* FactorWords(int index) {
  static const char* const kWords[cull_math::kCullCount] = {
      "the game's own", "2 times", "3 times", "5 times", "10 times"};
  return kWords[cull_math::CullIndex(index)];
}

const char* FogWords(int fog) {
  return fog != 0 ? "the fog moves out by the same factor"
                  : "the fog of the game stays";
}

// The sky asks for its fog. The fog of the frame goes to the game.
void __cdecl h_SetFog(float start, float end) {
  ++g_fog_sets;
  g_sky_start = start;
  g_sky_end = end;
  float used_start = start, used_end = end;
  if (g_frame_fog > 1.0f) {
    cull_math::ScaleFog(start, end, g_frame_fog, &used_start, &used_end);
    ++g_fog_scaled;
  }
  g_used_start = used_start;
  g_used_end = used_end;
  o_SetFog(used_start, used_end);
}

void Step(const char* by) {
  g_index = cull_math::CullNext(g_index);
  ControllersGesturePulse(0.5f, g_index != 0 ? 0.30f : 0.08f);
  ModLog("cull         : %s: cull_distance is %d now: map objects go %s as "
         "far out, from the next frame, frame %llu", by, g_index,
         FactorWords(g_index), ControllersFrameCount());
}

}  // namespace

// mod_main.cpp, after TabletopInstall, with play_mode=3 and 4.
void CullDistanceInstall() {
  Load();
  if (g_installed) return;
  if (!TabletopInstalled()) {
    ModLog("cull         : the table and the God view are off, so the game "
           "keeps its distances (cull_distance=%d has no effect)", g_index);
    return;
  }
  bool hooked = false;
  if (memcmp(reinterpret_cast<const void*>(kSetFog), kSetFogBytes,
             sizeof(kSetFogBytes)) != 0) {
    ModLog("cull         : the fog set at 0x%08X is not the code that this was "
           "read from, so the fog of the game stays",
           static_cast<unsigned>(kSetFog));
  } else {
    void* target = reinterpret_cast<void*>(kSetFog);
    MH_STATUS status =
        MH_CreateHook(target, reinterpret_cast<void*>(&h_SetFog),
                      reinterpret_cast<void**>(&o_SetFog));
    if (status == MH_OK) status = MH_EnableHook(target);
    ModLog("hooks        : the fog set of the sky at 0x%08X %s",
           static_cast<unsigned>(kSetFog), status == MH_OK ? "ok" : "FAILED");
    hooked = status == MH_OK;
  }
  if (!hooked) g_fog = 0;
  g_installed = true;
  ModLog("cull         : cull_distance=%d: map objects go %s as far out as in "
         "the game. cull_fog=%d: %s", g_index, FactorWords(g_index), g_fog,
         FogWords(g_fog));
  if (g_test_frame != 0) {
    ModLog("cull         : cull_test_frame=%llu: from that frame on, the cull "
           "distance steps each %llu frames (cull_test_period)", g_test_frame,
           g_test_period);
  }
}

// tabletop.cpp, TabletopApplyFrame: once a frame, before the eye passes.
void CullDistanceFrame() {
  if (!g_installed) return;
  if (g_test_frame != 0 && ControllersFrameCount() >= g_test_next) {
    g_test_next = ControllersFrameCount() + g_test_period;
    Step("the test cull_test_frame");
  }
  g_frame_factor = cull_math::CullFactor(g_index);
  g_frame_fog = cull_math::FogFactor(g_fog != 0, g_frame_factor);
}

// The factor of the map object distance in this frame, 1 for the game's own.
float CullDistanceFactor() { return g_installed ? g_frame_factor : 1.0f; }

// tabletop.cpp, TabletopReport: once a report window.
void CullDistanceReport() {
  if (!g_installed) return;
  if (g_fog_sets > 0) {
    ModLog("cull         : cull_distance=%d, map objects stop at %.0f units. "
           "cull_fog=%d: the sky asked for the fog from %.0f to %.0f units, "
           "the game got %.0f to %.0f. %llu of %llu fog sets scaled", g_index,
           *reinterpret_cast<const float*>(kVanish), g_fog, g_sky_start,
           g_sky_end, g_used_start, g_used_end, g_fog_scaled, g_fog_sets);
  } else {
    ModLog("cull         : cull_distance=%d, map objects stop at %.0f units. "
           "The sky set no fog in this window", g_index,
           *reinterpret_cast<const float*>(kVanish));
  }
  g_fog_sets = 0;
  g_fog_scaled = 0;
}

// The debug window (debug_menu.cpp).
int CullDistanceValue() {
  Load();
  if (!g_installed) return kDebugCannot;
  return g_index;
}

void CullDistanceChange(const char* by) {
  Load();
  if (!g_installed) return;
  Step(by);
}

int CullFogValue() {
  Load();
  if (!g_installed || o_SetFog == nullptr) return kDebugCannot;
  return g_fog;
}

void CullFogChange(const char* by) {
  Load();
  if (!g_installed || o_SetFog == nullptr) return;
  g_fog = g_fog != 0 ? 0 : 1;
  ControllersGesturePulse(0.5f, g_fog != 0 ? 0.30f : 0.08f);
  ModLog("cull         : %s: cull_fog is %d now: %s, from the next fog set of "
         "the sky, frame %llu", by, g_fog, FogWords(g_fog),
         ControllersFrameCount());
}
