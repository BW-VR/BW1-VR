// THE ANIMATIONS THAT THE GAME MAKES IN THE GAME TURN (anim_watch, a test
// key, docs/backend.md, "The long game turn").
//
// Morphable::GetSetAnim (0x00619690, thiscall: the number of the animation,
// its kind, one more value, ret 0xC) gives an animation of a Morphable (the
// Creature). The animations are in a table of the object at +0x1D4: one slot
// for each number (0x2B8 numbers) and each kind. Kind 0 is the mixed
// animation of the look of the object now: the mix of the base animation
// (kind 1) and two others by the weights at +0xA0 and +0xA8. When its slot
// is empty, the function makes it (fn_0085EF40), and to make it, it can also
// make the animations of the kinds 1 to 5 (a read of the file, fn_00861A00).
// Morphable::MorphAnims (0x00619100, a virtual function, thiscall, no
// argument) mixes the rest pose again and frees each mixed animation of the
// object (the first 0xE8 slots). So each animation that the Creature uses
// after it is made again.
//
// With anim_watch=1 the mod counts these calls and times each call of
// GetSetAnim that makes an animation (its slot was empty). Each 450 frames
// one line goes to the log. The hooks change nothing that the game does.
//
//   anim_watch=0   no hook and no line (the default)
//   anim_watch=1   the counts and the times

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"

extern void ModLog(const char* format, ...);

namespace {

const uintptr_t kGetSetAnim = 0x00619690;
const uintptr_t kMorphAnims = 0x00619100;
const int kSlotsOffset = 0x1D4;
const int kAnimsPerKind = 0x2B8;
const int kMixedSlots = 0xE8;
const int kKinds = 6;  // the kinds 0 to 5 of the switch of GetSetAnim
const int kCallers = 4;

typedef int(__fastcall* GetSetAnimFn)(void* self, void* unused, int anim,
                                      int kind, int value);
typedef void(__fastcall* MorphAnimsFn)(void* self, void* unused);
GetSetAnimFn o_GetSetAnim = nullptr;
MorphAnimsFn o_MorphAnims = nullptr;

bool g_on = false;
LARGE_INTEGER g_frequency = {};
DWORD g_thread = 0;   // the thread of the first call: the frame thread
int g_depth = 0;      // GetSetAnim calls itself

uint64_t g_calls = 0;
uint64_t g_builds[kKinds] = {};
uint64_t g_build_ticks = 0;
uint64_t g_build_most = 0;
uint64_t g_morphs = 0;
uint64_t g_freed = 0;
uintptr_t g_caller[kCallers] = {};
uint64_t g_caller_count[kCallers] = {};

bool SlotEmpty(void* self, int anim, int kind) {
  if (anim < 0 || anim >= kAnimsPerKind || kind < 0 || kind >= kKinds) {
    return false;
  }
  const uint8_t* slot = reinterpret_cast<const uint8_t*>(self) +
                        kSlotsOffset + (anim + kind * kAnimsPerKind) * 4;
  return *reinterpret_cast<const uint32_t*>(slot) == 0;
}

int __fastcall h_GetSetAnim(void* self, void* unused, int anim, int kind,
                            int value) {
  if (GetCurrentThreadId() != g_thread || g_depth > 0) {
    return o_GetSetAnim(self, unused, anim, kind, value);
  }
  ++g_calls;
  if (!SlotEmpty(self, anim, kind)) {
    return o_GetSetAnim(self, unused, anim, kind, value);
  }
  LARGE_INTEGER before, after;
  QueryPerformanceCounter(&before);
  ++g_depth;
  const int made = o_GetSetAnim(self, unused, anim, kind, value);
  --g_depth;
  QueryPerformanceCounter(&after);
  const uint64_t spent =
      static_cast<uint64_t>(after.QuadPart - before.QuadPart);
  ++g_builds[kind];
  g_build_ticks += spent;
  if (spent > g_build_most) g_build_most = spent;
  return made;
}

void __fastcall h_MorphAnims(void* self, void* unused) {
  if (GetCurrentThreadId() == g_thread) {
    ++g_morphs;
    const uint32_t* slot = reinterpret_cast<const uint32_t*>(
        reinterpret_cast<const uint8_t*>(self) + kSlotsOffset);
    for (int i = 0; i < kMixedSlots; ++i) {
      if (slot[i] != 0) ++g_freed;
    }
    const uintptr_t from = reinterpret_cast<uintptr_t>(_ReturnAddress());
    for (int i = 0; i < kCallers; ++i) {
      if (g_caller[i] == from || g_caller[i] == 0) {
        g_caller[i] = from;
        ++g_caller_count[i];
        break;
      }
    }
  }
  o_MorphAnims(self, unused);
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

bool Hook(uintptr_t address, void* replacement, void** original,
          const char* name) {
  void* target = reinterpret_cast<void*>(address);
  const bool ok = MH_CreateHook(target, replacement, original) == MH_OK &&
                  MH_QueueEnableHook(target) == MH_OK;
  if (!ok) {
    ModLog("anim watch   : %s at 0x%08X FAILED to hook", name,
           static_cast<unsigned>(address));
  }
  return ok;
}

}  // namespace

// From PassGuardInstall, on the frame thread.
void AnimWatchInstall() {
  if (IniKey("anim_watch", 0) == 0) return;
  QueryPerformanceFrequency(&g_frequency);
  g_thread = GetCurrentThreadId();
  g_on = Hook(kGetSetAnim, reinterpret_cast<void*>(&h_GetSetAnim),
              reinterpret_cast<void**>(&o_GetSetAnim),
              "Morphable::GetSetAnim") &&
         Hook(kMorphAnims, reinterpret_cast<void*>(&h_MorphAnims),
              reinterpret_cast<void**>(&o_MorphAnims),
              "Morphable::MorphAnims");
  // One stop of the other threads for both hooks (anim_trig.cpp, HookAll).
  g_on = g_on && MH_ApplyQueued() == MH_OK;
  if (!g_on) {
    // No hook stays queued, so a later MH_ApplyQueued does not enable it.
    // MH_RemoveHook of a target with no hook gives an error and does nothing.
    MH_RemoveHook(reinterpret_cast<void*>(kGetSetAnim));
    MH_RemoveHook(reinterpret_cast<void*>(kMorphAnims));
  }
  ModLog("anim watch   : anim_watch=1 (a test). Each 450 frames the calls of "
         "Morphable::GetSetAnim, the animations that it makes and their time, "
         "and the calls of Morphable::MorphAnims%s",
         g_on ? "" : ". A hook failed, so no line comes");
}

// From PassGuardReport, each 450 frames.
void AnimWatchReport() {
  if (!g_on || g_frequency.QuadPart == 0) return;
  const double to_ms = 1000.0 / static_cast<double>(g_frequency.QuadPart);
  uint64_t builds = 0;
  for (int k = 0; k < kKinds; ++k) builds += g_builds[k];
  ModLog("anim watch   : %llu calls of GetSetAnim on the frame thread, %llu "
         "of them made an animation (kind 0: %llu, 1: %llu, 2 to 5: %llu). "
         "They took %.2f ms in all, the most %.2f ms in one call. %llu calls "
         "of MorphAnims, which freed %llu mixed animations. Its callers: "
         "0x%08X %llu, 0x%08X %llu, 0x%08X %llu, 0x%08X %llu",
         g_calls, builds, g_builds[0], g_builds[1],
         g_builds[2] + g_builds[3] + g_builds[4] + g_builds[5],
         static_cast<double>(g_build_ticks) * to_ms,
         static_cast<double>(g_build_most) * to_ms, g_morphs, g_freed,
         static_cast<unsigned>(g_caller[0]), g_caller_count[0],
         static_cast<unsigned>(g_caller[1]), g_caller_count[1],
         static_cast<unsigned>(g_caller[2]), g_caller_count[2],
         static_cast<unsigned>(g_caller[3]), g_caller_count[3]);
  g_calls = 0;
  for (int k = 0; k < kKinds; ++k) g_builds[k] = 0;
  g_build_ticks = 0;
  g_build_most = 0;
  g_morphs = 0;
  g_freed = 0;
  for (int i = 0; i < kCallers; ++i) {
    g_caller[i] = 0;
    g_caller_count[i] = 0;
  }
}
