// THE ANGLES AND THE ROTATIONS OF THE ANIMATIONS WITH NO X87 FUNCTION
// (anim_trig, anim_trig_math.h, docs/backend.md, "The long game turn").
//
// When the Creature uses an animation for the first time after the game
// mixed its look, Morphable::GetSetAnim makes the mixed animation in the game
// turn (anim_watch.cpp). For each bone and each key it turns three sets of
// angles into rotations (LHMatrix::SetYXZMatrixOnly), mixes them, and turns
// the mix back into angles (LHMatrix::GetYXZ with fn_007FA990). Other code
// of the animations uses fn_007FACC0. Each of the four functions uses FSIN,
// FCOS or FPATAN. On a PC one such animation takes 1 to 5 ms. Under FEX each
// of these instructions is a call of a function, so an animation is expected
// to take much longer there. The long game turns of the Frame (19 to 33 ms)
// can have this cause. Only a headset can show how much of them it is.
//
// With anim_trig=1 each of the four functions goes to the function of
// anim_trig_math.h with the same steps and the same stores to floats, in
// double arithmetic with no x87 instruction. The game gets the same floats to
// the precision of its x87 unit (anim_trig_check counts the differences). Each
// other caller of the four functions (the physics, the Hand) gets them too.
//
//   anim_trig=1             the functions of the mod (the default)
//   anim_trig=0             the x87 code of the game. With 0 at the start the
//                           mod makes no hook
//   anim_trig_check=1       a test: each call also runs the code of the game,
//                           and each 450 frames the log gives the calls, the
//                           calls with a float that differs, and the largest
//                           difference
//   anim_trig_test_frame=N  a test: at frame N of the controllers the test key
//                           acts one time
//
// The test key (test_keys=1): Ctrl, Shift and A switch between 1 and 0 while
// the game runs. The debug page has the same switch.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"
#include "anim_trig_math.h"
#include "debug_menu.h"

extern void ModLog(const char* format, ...);
void ControllersGesturePulse(float amplitude, float seconds);
uint64_t ControllersFrameCount();
uint64_t ControllersTestFrame(uint64_t key_frame, uint64_t frame);

namespace {

const uintptr_t kAngle = 0x007FA990;
const uintptr_t kGetYXZ = 0x007FAB30;
const uintptr_t kSetYXZMatrixOnly = 0x007FAC10;
const uintptr_t kEulerToMatrix34 = 0x007FACC0;
const uintptr_t kPlusQuarter = 0x008C7B48;   // double
const uintptr_t kHalf = 0x008D45D0;          // double
const uintptr_t kMinusQuarter = 0x009361E8;  // double
const uintptr_t kZero = 0x008AA398;          // float

typedef double(__cdecl* AngleFn)(float a, float b);
typedef void(__fastcall* ToEulerFn)(const float* src, void* unused,
                                    float* out_a, float* out_b, float* out_c);
typedef void(__fastcall* ToMatrixFn)(float* m, void* unused, float a, float b,
                                     float c);
AngleFn o_Angle = nullptr;
ToEulerFn o_GetYXZ = nullptr;
ToMatrixFn o_SetYXZMatrixOnly = nullptr;
ToMatrixFn o_EulerToMatrix34 = nullptr;

anim_trig::Constants g_k = {};
bool g_in = false;     // the hooks are in
bool g_on = false;     // anim_trig now
bool g_check = false;  // anim_trig_check
int g_test_frame = 0;
uint64_t g_test_seen = 0;

// The counts of anim_trig_check, for each function: the calls, the calls
// with a float that differs in a bit, the calls with a difference of more
// than 1e-6, and the largest difference. The rotations and the angles are
// near 1, so a difference is absolute: a value near 0 is many units of the
// last place from a value near 0 of the other sign.
enum { kFnAngle = 0, kFnGetYXZ, kFnSetYXZ, kFnToMatrix34, kFns };
const char* const kFnName[kFns] = {"fn_007FA990", "LHMatrix::GetYXZ",
                                   "LHMatrix::SetYXZMatrixOnly",
                                   "fn_007FACC0"};
uint64_t g_calls[kFns] = {};
uint64_t g_differ[kFns] = {};
uint64_t g_large[kFns] = {};
double g_most[kFns] = {};
DWORD g_thread = 0;  // the counts are of the frame thread only
// The thread that runs the code of the game for a compare now.
// LHMatrix::GetYXZ calls fn_007FA990, and that call must be the code of the
// game too.
volatile DWORD g_game_in = 0;

void Count(int fn, const float* mod, const float* game, int count) {
  if (GetCurrentThreadId() != g_thread) return;
  ++g_calls[fn];
  bool differ = false;
  double most = 0.0;
  for (int i = 0; i < count; ++i) {
    if (anim_trig::Ulps(mod[i], game[i]) != 0) differ = true;
    const double d = fabs(static_cast<double>(mod[i]) - game[i]);
    if (d > most || d != d) most = d != d ? 1e30 : d;
  }
  if (differ) ++g_differ[fn];
  if (most > 1e-6) ++g_large[fn];
  if (most > g_most[fn]) g_most[fn] = most;
}

double __cdecl h_Angle(float a, float b) {
  if (!g_on || g_game_in == GetCurrentThreadId()) return o_Angle(a, b);
  const double r = anim_trig::Angle(a, b, g_k);
  if (g_check) {
    const float mod = static_cast<float>(r);
    const float game = static_cast<float>(o_Angle(a, b));
    Count(kFnAngle, &mod, &game, 1);
  }
  return r;
}

void __fastcall h_GetYXZ(const float* src, void* unused, float* out_a,
                                float* out_b, float* out_c) {
  if (!g_on) {
    o_GetYXZ(src, unused, out_a, out_b, out_c);
    return;
  }
  if (g_check) {
    float game[3];
    g_game_in = GetCurrentThreadId();
    o_GetYXZ(src, unused, &game[0], &game[1], &game[2]);
    g_game_in = 0;
    float mod[3];
    anim_trig::GetYXZ(src, &mod[0], &mod[1], &mod[2], g_k);
    Count(kFnGetYXZ, mod, game, 3);
  }
  anim_trig::GetYXZ(src, out_a, out_b, out_c, g_k);
}

void __fastcall h_SetYXZMatrixOnly(float* m, void* unused, float a, float b,
                                 float c) {
  if (!g_on) {
    o_SetYXZMatrixOnly(m, unused, a, b, c);
    return;
  }
  if (g_check) {
    float game[9];
    o_SetYXZMatrixOnly(game, unused, a, b, c);
    anim_trig::SetYXZMatrixOnly(m, a, b, c);
    Count(kFnSetYXZ, m, game, 9);
    return;
  }
  anim_trig::SetYXZMatrixOnly(m, a, b, c);
}

void __fastcall h_EulerToMatrix34(float* m, void* unused, float a, float b,
                                  float c) {
  if (!g_on) {
    o_EulerToMatrix34(m, unused, a, b, c);
    return;
  }
  if (g_check) {
    float game[12];
    o_EulerToMatrix34(game, unused, a, b, c);
    anim_trig::EulerToMatrix34(m, a, b, c, g_k.zero);
    Count(kFnToMatrix34, m, game, 12);
    return;
  }
  anim_trig::EulerToMatrix34(m, a, b, c, g_k.zero);
}

int IniKey(const char* key, int fallback) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return fallback;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

// The four hooks go in with one stop of the other threads: MinHook stops
// each other thread for each MH_EnableHook, and under FEX a stop while a
// thread translates code can hang the process (docs/android.md, "The work
// of a frame on more cores"). So the hooks are queued and applied in one
// call. Gives false, and removes each hook, when one of them fails.
bool HookAll() {
  struct Target {
    uintptr_t address;
    void* replacement;
    void** original;
  };
  const Target targets[4] = {
      {kAngle, reinterpret_cast<void*>(&h_Angle),
       reinterpret_cast<void**>(&o_Angle)},
      {kGetYXZ, reinterpret_cast<void*>(&h_GetYXZ),
       reinterpret_cast<void**>(&o_GetYXZ)},
      {kSetYXZMatrixOnly, reinterpret_cast<void*>(&h_SetYXZMatrixOnly),
       reinterpret_cast<void**>(&o_SetYXZMatrixOnly)},
      {kEulerToMatrix34, reinterpret_cast<void*>(&h_EulerToMatrix34),
       reinterpret_cast<void**>(&o_EulerToMatrix34)}};
  int made = 0;
  bool ok = true;
  for (; made < 4; ++made) {
    void* target = reinterpret_cast<void*>(targets[made].address);
    if (MH_CreateHook(target, targets[made].replacement,
                      targets[made].original) != MH_OK ||
        MH_QueueEnableHook(target) != MH_OK) {
      ok = false;
      break;
    }
  }
  if (ok && MH_ApplyQueued() == MH_OK) return true;
  for (int i = 0; i <= made && i < 4; ++i) {
    MH_RemoveHook(reinterpret_cast<void*>(targets[i].address));
  }
  return false;
}

}  // namespace

// From PassGuardInstall, on the frame thread.
void AnimTrigInstall() {
  g_on = IniKey("anim_trig", 1) != 0;
  g_check = IniKey("anim_trig_check", 0) != 0;
  g_test_frame = IniKey("anim_trig_test_frame", 0);
  g_thread = GetCurrentThreadId();
  if (!g_on && g_test_frame <= 0 && !g_check) {
    ModLog("anim trig    : anim_trig=0. The angles and the rotations of the "
           "animations are the x87 code of the game (FSIN, FCOS, FPATAN). No "
           "hook");
    return;
  }
  // The constants of fn_007FA990, from the game.
  const void* at[4] = {reinterpret_cast<const void*>(kPlusQuarter),
                       reinterpret_cast<const void*>(kHalf),
                       reinterpret_cast<const void*>(kMinusQuarter),
                       reinterpret_cast<const void*>(kZero)};
  for (int i = 0; i < 4; ++i) {
    if (IsBadReadPtr(at[i], 8)) {
      g_on = false;
      ModLog("anim trig    : the constants of the game cannot be read. The "
             "x87 code of the game stays");
      return;
    }
  }
  g_k.plus_quarter = *reinterpret_cast<const double*>(kPlusQuarter);
  g_k.half = *reinterpret_cast<const double*>(kHalf);
  g_k.minus_quarter = *reinterpret_cast<const double*>(kMinusQuarter);
  g_k.zero = *reinterpret_cast<const float*>(kZero);
  if (!HookAll()) {
    g_on = false;
    ModLog("anim trig    : a hook of the four functions FAILED. The x87 code "
           "of the game stays");
    return;
  }
  g_in = true;
  ModLog("anim trig    : anim_trig=%d. The angles and the rotations of the "
         "animations (fn_007FA990, LHMatrix::GetYXZ, "
         "LHMatrix::SetYXZMatrixOnly, fn_007FACC0) %s. "
         "The constants of the game: %.17g, %.17g, %.17g and %g. "
         "anim_trig_check=%d%s",
         g_on ? 1 : 0,
         g_on ? "are the functions of the mod, in doubles with no x87 "
                "instruction"
              : "are the x87 code of the game",
         g_k.plus_quarter, g_k.half, g_k.minus_quarter, g_k.zero,
         g_check ? 1 : 0,
         g_test_frame > 0 ? ". The test key acts at anim_trig_test_frame" : "");
}

// The test key (Ctrl, Shift and A, and the debug page).
void AnimTrigToggle(const char* by) {
  if (!g_in) {
    ModLog("anim trig    : %s, but the mod has no hook of the four functions "
           "(anim_trig=0 at the start, or a failed hook)", by);
    return;
  }
  g_on = !g_on;
  ControllersGesturePulse(0.5f, g_on ? 0.30f : 0.08f);
  ModLog("anim trig    : %s, anim_trig is %d now: %s, frame %llu", by,
         g_on ? 1 : 0,
         g_on ? "the functions of the mod" : "the x87 code of the game",
         ControllersFrameCount());
}

int AnimTrigValue() {
  if (!g_in) return kDebugCannot;
  return g_on ? 1 : 0;
}

// Each frame, from the keys of stereo.cpp: anim_trig_test_frame.
void AnimTrigFrame() {
  if (g_test_frame <= 0) return;
  const uint64_t frame = ControllersFrameCount();
  if (frame == g_test_seen) return;
  g_test_seen = frame;
  const uint64_t at = static_cast<uint64_t>(g_test_frame);
  if (ControllersTestFrame(at, frame) == at) {
    AnimTrigToggle("test key anim_trig_test_frame");
  }
}

// From PassGuardReport, each 450 frames.
void AnimTrigReport() {
  if (!g_in || !g_check) return;
  // The precision control of the x87 unit of the frame thread: the code of
  // the game rounds each step in its registers to it.
  const unsigned control = _control87(0, 0) & _MCW_PC;
  const int bits = control == _PC_24 ? 24 : control == _PC_53 ? 53 : 64;
  ModLog("anim trig    : check (anim_trig=%d, the x87 unit at %d bits): calls "
         "of the frame thread, the calls with a float that differs from the "
         "code of the game in a bit, the calls with a difference of more than "
         "1e-6, the largest difference: %s %llu %llu %llu %.3g, %s %llu %llu "
         "%llu %.3g, %s %llu %llu %llu %.3g, %s %llu %llu %llu %.3g",
         g_on ? 1 : 0, bits, kFnName[0], g_calls[0], g_differ[0], g_large[0],
         g_most[0], kFnName[1], g_calls[1], g_differ[1], g_large[1], g_most[1],
         kFnName[2], g_calls[2], g_differ[2], g_large[2], g_most[2],
         kFnName[3], g_calls[3], g_differ[3], g_large[3], g_most[3]);
  for (int i = 0; i < kFns; ++i) {
    g_calls[i] = 0;
    g_differ[i] = 0;
    g_large[i] = 0;
    g_most[i] = 0.0;
  }
}
