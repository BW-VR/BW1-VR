// THE EYE CAMERA AFTER A CAMERA OF ITS OWN (docs/backend.md, "The eye camera
// after a camera of its own", camera_stale_math.h).
//
// The eye loop (stereo.cpp) gives the engine the camera of one eye: the
// camera that the game asked for, with the head, the roll and the offset of
// the eye. Then it walks the scene. The walk is not one camera. An effect of
// the game can give the engine a camera of its own for one draw
// (LH3DTech::UpdateCamera 0x00819920 with a camera at the origin) and then
// give the stored camera back (the restore at 0x0068B7D0, called at
// 0x0068BF2F). That restore builds the camera matrices again from the stored
// camera. The build has no eye offset and no roll. Every draw after it, in
// the same pass, uses a camera that is not the camera of the eye.
//
// WHAT IT LOOKS LIKE. The land, the houses and the trees are drawn in the
// walk, before the sorted list. The sorted list (LH3DZSorter, drawn from
// LH3DRender::FinishFrame through fn_0082F280) holds the draws that need an
// order from far to near: the clouds and the ground mist (an LH3DMist, drawn
// through the virtual function +0x108 of its object), the beams of light
// (GLightSheet::Draw 0x004246E0), billboards (LH3DSprite::Draw 0x00840530)
// and the effects of the particle systems. An item of the list that comes
// after a restore is drawn from a camera with no roll and no eye offset. The
// world has the roll of the head, so the item does not roll with the world.
// It seems fixed to the head. Which items come after the restore depends on
// their distance from the camera, so it shows at some times and not at
// others.
//
// WHAT THE MOD DOES. eye_after_camera=1 puts the eye camera back
// (StereoReapplyEye) at two places:
//
//   1. After a call that gives a camera back. A call in the scene walk, in an
//      eye pass, whose camera is the stored camera from before an earlier
//      call of the walk, is a restore. A stack of the stored cameras before
//      the calls finds it, because the engine stores the camera of its own
//      too.
//   2. Before each item of the sorted list, and after the last item, when
//      the camera matrix is not the matrix that the eye loop wrote. This
//      covers an effect that gives the engine a camera and does not give it
//      back.
//
// eye_after_camera=0 keeps the camera that the game leaves. The loop of the
// sorted list is then the loop of the game, item for item.
//
// THE TRACE (roll_trace=1) counts, for each report window, the camera calls
// of the walk by the address that called, the items of the sorted list by
// the address of their draw function, and how many of them saw a camera that
// was not the camera of the eye, with the reason and the first frame. The
// clouds and the sprites that the game draws outside the list are counted by
// their own hooks (sky_detail.cpp, sprite_up.cpp).
//
// THE LOOP. fn_0082F280 sets the flag of the flush (0x00ECA610), calls the
// empty function 0x008398B0, and walks the list from 0x00EE9D38. A node has
// the object at +4, the draw function at +8 (a thiscall with no arguments),
// a number at +0xC that the loop copies to 0x00EE9D30 before the call, the
// distance at +0x10 and the next node at +0x14. The hook does the same walk.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "MinHook.h"
#include "camera_stale_math.h"

extern void ModLog(const char* format, ...);
// controllers.cpp: the frame number that the scripted controller counts.
uint64_t ControllersFrameCount();
// controllers.cpp: a pulse of the controllers, for the key: long for 1 and
// short for 0.
void ControllersGesturePulse(float amplitude, float seconds);
// head_camera.cpp
bool HeadCameraInScene();
// stereo.cpp
bool StereoInPass();
bool StereoEyeCameraStale();
void StereoReapplyEye();

void EyeStaleToggle(const char* by);

namespace {

using camera_stale_math::Census;
using camera_stale_math::Tally;

const uintptr_t kSortedFlush = 0x0082F280;
const uintptr_t kFlushFlag = 0x00ECA610;
const uintptr_t kFlushStub = 0x008398B0;
const uintptr_t kListHead = 0x00EE9D38;
const uintptr_t kListCurrent = 0x00EE9D30;
// The stored camera: the position at 0x00EA1DB8 and the focus at 0x00EA1DC4,
// 6 floats in a row.
const uintptr_t kStoredCamera = 0x00EA1DB8;
// The return address of the call of InnerCamera::PreDraw (head_camera.cpp).
// The camera of a room of the Temple is not a camera of its own.
const uintptr_t kRoomPreDrawReturn = 0x007969D2;

// Draw functions of the sorted list that have a name.
struct KnownDraw {
  uintptr_t address;
  const char* name;
};
const KnownDraw kKnown[] = {
    {0x004246E0, "a beam of light, GLightSheet"},
    {0x007FA980, "an object, virtual draw"},
    {0x00840530, "a sprite, LH3DSprite::Draw"},
    {0x0068B480, "an effect with a camera of its own"},
};

typedef void(__cdecl* FlushFn)();
typedef void(__fastcall* ItemFn)(void* self, void* unused);
FlushFn o_Flush = nullptr;

bool g_hooked = false;
// eye_after_camera
bool g_on = true;
// roll_test_frame: at this frame of the scripted controller the key above
// changes, for one run with each value. 0 is no change.
uint64_t g_test_frame = 0;
bool g_test_done = false;
// roll_trace
bool g_trace = false;
bool g_test_keys = false;

// THE CAMERA CALLS OF THE WALK. The stored cameras from before the open
// calls.
camera_stale_math::CameraStack g_stack;
// The calls since the matrix was last the matrix of the eye, and whether the
// last of them gave a camera back.
uint64_t g_calls_since = 0;
bool g_last_restore = false;

struct CallerTally {
  uintptr_t caller;
  uint64_t calls;
  uint64_t restores;
};
const int kCallers = 12;
CallerTally g_callers[kCallers];
int g_caller_count = 0;

uint64_t g_scene_calls = 0;
uint64_t g_restores = 0;
uint64_t g_reapplied_restore = 0;
uint64_t g_reapplied_item = 0;
uint64_t g_reapplied_end = 0;
uint64_t g_flushes = 0;
uint64_t g_items = 0;

// The trace of the items.
const int kCensusSlots = 24;
Census<kCensusSlots> g_census;
Tally g_items_all;
uint64_t g_reason_items[camera_stale_math::kReasonCount];
// The clouds and the sprites that the game draws outside the list.
const int kOutsideKinds = 2;
const char* const kOutsideName[kOutsideKinds] = {"clouds", "sprites"};
Tally g_outside[kOutsideKinds];
uint64_t g_outside_reason[kOutsideKinds][camera_stale_math::kReasonCount];

const char* NameOf(uintptr_t address) {
  for (const KnownDraw& known : kKnown) {
    if (known.address == address) return known.name;
  }
  return "";
}

CallerTally* CallerSlot(uintptr_t caller) {
  for (int i = 0; i < g_caller_count; ++i) {
    if (g_callers[i].caller == caller) return &g_callers[i];
  }
  if (g_caller_count == kCallers) return &g_callers[kCallers - 1];
  CallerTally* slot = &g_callers[g_caller_count++];
  slot->caller = caller;
  slot->calls = 0;
  slot->restores = 0;
  return slot;
}

camera_stale_math::Reason ReasonNow(bool stale) {
  return camera_stale_math::Classify(!stale, g_calls_since, g_last_restore);
}

// Ctrl, Shift and R change eye_after_camera between 1 and 0 (test_keys=1). The
// key is read once in each flush of the sorted list, and only while a window
// of the game is in front.
void PollKey() {
  static bool was = false;
  if (!g_test_keys) return;
  const bool down = (GetAsyncKeyState('R') & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool pressed = down && !was;
  was = down;
  if (!pressed) return;
  DWORD process = 0;
  const HWND window = GetForegroundWindow();
  if (window != nullptr) GetWindowThreadProcessId(window, &process);
  if (process != GetCurrentProcessId()) return;
  EyeStaleToggle("Ctrl, Shift and R");
}

void TestFrameCheck() {
  if (g_test_frame == 0 || g_test_done) return;
  const uint64_t frame = ControllersFrameCount();
  if (frame < g_test_frame) return;
  g_test_done = true;
  g_on = !g_on;
  ModLog("eye restore  : roll_test_frame at frame %llu: eye_after_camera=%d "
         "now", frame, g_on ? 1 : 0);
}

// The eye camera back, when the engine has another one. True when it did.
bool PutEyeBack() {
  if (!StereoEyeCameraStale()) return false;
  StereoReapplyEye();
  g_calls_since = 0;
  g_last_restore = false;
  return true;
}

// The walk of the sorted list, as fn_0082F280 does it, with a look at the
// camera before each item.
void __cdecl h_SortedFlush() {
  TestFrameCheck();
  PollKey();
  if (!g_on && !g_trace) {
    o_Flush();
    return;
  }
  ++g_flushes;
  *reinterpret_cast<uint32_t*>(kFlushFlag) = 1;
  reinterpret_cast<void(__cdecl*)()>(kFlushStub)();
  const uint64_t frame = g_trace ? ControllersFrameCount() : 0;
  uint32_t* node = *reinterpret_cast<uint32_t**>(kListHead);
  // The intro and the menus draw with a camera of their own and no scene
  // walk. Only the scene walk of an eye pass has an eye camera to keep.
  const bool pass = StereoInPass() && HeadCameraInScene();
  while (node != nullptr) {
    const bool stale = pass && StereoEyeCameraStale();
    if (g_trace && pass) {
      const camera_stale_math::Reason reason = ReasonNow(stale);
      ++g_items;
      g_items_all.Add(frame, stale);
      ++g_reason_items[reason];
      Tally* tally = g_census.Find(node[2]);
      if (tally != nullptr) tally->Add(frame, stale);
    }
    if (stale && g_on && PutEyeBack()) ++g_reapplied_item;
    *reinterpret_cast<uint32_t*>(kListCurrent) = node[3];
    reinterpret_cast<ItemFn>(node[2])(reinterpret_cast<void*>(node[1]),
                                      nullptr);
    node = reinterpret_cast<uint32_t*>(node[5]);
  }
  if (g_on && pass && PutEyeBack()) ++g_reapplied_end;
}

bool Hook(uintptr_t address, void* replacement, void** original,
          const char* name) {
  void* target = reinterpret_cast<void*>(address);
  MH_STATUS status = MH_CreateHook(target, replacement, original);
  if (status == MH_OK) status = MH_EnableHook(target);
  ModLog("hooks        : %s at 0x%08X %s", name, address,
         status == MH_OK ? "ok" : "FAILED");
  return status == MH_OK;
}

}  // namespace

// eye_after_camera, roll_trace, roll_test_frame and test_keys.
void EyeStaleSet(bool on, bool trace, int test_frame, bool test_keys) {
  g_on = on;
  g_trace = trace;
  g_test_keys = test_keys;
  g_test_frame = test_frame > 0 ? static_cast<uint64_t>(test_frame) : 0;
  ModLog("eye restore  : eye_after_camera=%d. After a camera of its own, "
         "the draws of the same eye pass use %s", on ? 1 : 0,
         on ? "the camera of the eye, so they roll with the world"
            : "the camera that the game leaves, which has no roll and no eye "
              "offset");
  if (g_test_frame != 0) {
    ModLog("eye restore  : roll_test_frame=%d changes the key at that frame",
           test_frame);
  }
  if (trace) ModLog("eye restore  : roll_trace=1");
}

void EyeStaleInstall() {
  if (g_hooked) return;
  g_hooked = true;
  Hook(kSortedFlush, reinterpret_cast<void*>(&h_SortedFlush),
       reinterpret_cast<void**>(&o_Flush), "the sorted list of draws");
}

int EyeStaleValue() { return g_on ? 1 : 0; }

void EyeStaleToggle(const char* by) {
  g_on = !g_on;
  ControllersGesturePulse(0.5f, g_on ? 0.24f : 0.08f);
  ModLog("eye restore  : eye_after_camera=%d now (%s), frame %llu",
         g_on ? 1 : 0, by != nullptr ? by : "", ControllersFrameCount());
}

// A new eye pass. The stored camera of the pass is the camera that the game
// asked for, and no call is open.
void EyeStalePassBegin() {
  g_stack.Clear();
  g_calls_since = 0;
  g_last_restore = false;
}

// Before a camera call of the scene walk in an eye pass. True when the call
// gives a camera back: its camera is the stored camera from before an open
// call. `caller` is the return address of the call.
bool EyeStaleCallBegin(uintptr_t caller, const float* position,
                       const float* focus) {
  if (position == nullptr || focus == nullptr) return false;
  if (IsBadReadPtr(position, sizeof(float) * 3) ||
      IsBadReadPtr(focus, sizeof(float) * 3)) {
    return false;
  }
  ++g_scene_calls;
  float args[6];
  memcpy(args, position, sizeof(float) * 3);
  memcpy(args + 3, focus, sizeof(float) * 3);
  bool restore = false;
  if (caller != kRoomPreDrawReturn) {
    restore = g_stack.Call(
        args, reinterpret_cast<const float*>(kStoredCamera));
  }
  if (!StereoEyeCameraStale()) g_calls_since = 0;
  ++g_calls_since;
  g_last_restore = restore;
  if (restore) ++g_restores;
  if (g_trace) {
    CallerTally* slot = CallerSlot(caller);
    ++slot->calls;
    if (restore) ++slot->restores;
  }
  return restore;
}

// After the call. A restore leaves a camera with no eye offset and no roll.
void EyeStaleCallEnd(bool restore) {
  if (!restore || !g_on) return;
  if (PutEyeBack()) ++g_reapplied_restore;
}

// A draw outside the sorted list, for the trace. kind: 0 a cloud, 1 a sprite.
void EyeStaleDraw(int kind) {
  if (!g_trace || kind < 0 || kind >= kOutsideKinds || !StereoInPass() ||
      !HeadCameraInScene()) {
    return;
  }
  const bool stale = StereoEyeCameraStale();
  g_outside[kind].Add(ControllersFrameCount(), stale);
  ++g_outside_reason[kind][ReasonNow(stale)];
}

// Once per report window.
void EyeStaleReport() {
  if (!g_hooked) return;
  if (g_scene_calls > 0 || g_reapplied_item > 0 || g_reapplied_end > 0) {
    ModLog("eye restore  : %llu camera calls in the walk, %llu gave a camera "
           "back. The eye camera was put back %llu times after such a call, "
           "%llu times before an item of the sorted list and %llu times "
           "after the last item (%llu flushes)",
           g_scene_calls, g_restores, g_reapplied_restore, g_reapplied_item,
           g_reapplied_end, g_flushes);
  }
  if (g_trace) {
    char callers[320] = "";
    int at = 0;
    for (int i = 0; i < g_caller_count && at < 280; ++i) {
      at += _snprintf_s(callers + at, sizeof(callers) - at, _TRUNCATE,
                        "%s0x%08X x%llu (%llu gave a camera back)",
                        i == 0 ? "" : ", ", g_callers[i].caller,
                        g_callers[i].calls, g_callers[i].restores);
    }
    if (g_caller_count > 0) {
      ModLog("roll trace   : camera calls in the walk by the address that "
             "called: %s", callers);
    }
    if (g_items > 0) {
      ModLog("roll trace   : %llu items of the sorted list. %llu of them saw a "
             "camera that is not the camera of the eye, in %llu frames, the "
             "first at frame %llu. By reason: %s %llu, %s %llu, %s %llu",
             g_items, g_items_all.stale, g_items_all.stale_frames,
             g_items_all.first_frame,
             camera_stale_math::ReasonText(camera_stale_math::kInsideCamera),
             g_reason_items[camera_stale_math::kInsideCamera],
             camera_stale_math::ReasonText(camera_stale_math::kAfterRestore),
             g_reason_items[camera_stale_math::kAfterRestore],
             camera_stale_math::ReasonText(camera_stale_math::kOtherChange),
             g_reason_items[camera_stale_math::kOtherChange]);
      // The draw functions, the ones with the most items that saw another
      // camera first.
      bool listed[kCensusSlots] = {};
      for (int rank = 0; rank < 8; ++rank) {
        int best = -1;
        for (int i = 0; i < g_census.used; ++i) {
          if (listed[i]) continue;
          if (best < 0 ||
              g_census.tally[i].stale > g_census.tally[best].stale ||
              (g_census.tally[i].stale == g_census.tally[best].stale &&
               g_census.tally[i].draws > g_census.tally[best].draws)) {
            best = i;
          }
        }
        if (best < 0) break;
        listed[best] = true;
        const Tally& t = g_census.tally[best];
        const char* name = NameOf(g_census.key[best]);
        ModLog("roll trace   : draw function 0x%08X%s%s: %llu items, %llu saw "
               "another camera, in %llu frames, the first at frame %llu",
               static_cast<unsigned>(g_census.key[best]),
               name[0] != '\0' ? " " : "", name, t.draws, t.stale,
               t.stale_frames, t.first_frame);
      }
      if (g_census.overflow > 0) {
        ModLog("roll trace   : %llu items of draw functions beyond the table",
               g_census.overflow);
      }
    }
    for (int kind = 0; kind < kOutsideKinds; ++kind) {
      const Tally& t = g_outside[kind];
      if (t.draws == 0) continue;
      ModLog("roll trace   : %llu %s drawn in the walk. %llu saw a camera that "
             "is not the camera of the eye (inside a camera of its own %llu, "
             "after the stored camera came back %llu, other %llu), in %llu "
             "frames, the first at frame %llu",
             t.draws, kOutsideName[kind], t.stale,
             g_outside_reason[kind][camera_stale_math::kInsideCamera],
             g_outside_reason[kind][camera_stale_math::kAfterRestore],
             g_outside_reason[kind][camera_stale_math::kOtherChange],
             t.stale_frames, t.first_frame);
    }
  }
  g_scene_calls = 0;
  g_restores = 0;
  g_reapplied_restore = 0;
  g_reapplied_item = 0;
  g_reapplied_end = 0;
  g_flushes = 0;
  g_items = 0;
  g_caller_count = 0;
  g_census.Clear();
  g_items_all.Clear();
  memset(g_reason_items, 0, sizeof(g_reason_items));
  for (int kind = 0; kind < kOutsideKinds; ++kind) g_outside[kind].Clear();
  memset(g_outside_reason, 0, sizeof(g_outside_reason));
}
