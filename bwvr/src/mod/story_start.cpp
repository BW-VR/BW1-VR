// A story starts at the head (docs/god-view.md, "A story starts at the
// head").
//
// WHY. In the God view the game camera stays 400 units from the player and
// about 300 units above the land (GodAfter in comfort_camera.cpp). A script
// takes the camera with START_CAMERA_CONTROL, and then MOVE_CAMERA_POSITION
// and MOVE_CAMERA_FOCUS ease the six Zoomers of GCamera from where they are
// (Zoomer::SetDestinationWithSpeedAndTime 0x00407D60, called by
// CameraModeScript::MoveCameraPosition 0x004616F0). So the flight of a story,
// for example the flight to a silver scroll, starts far above the player.
//
// WHAT THE MOD DOES. A hook on GScript::StartCameraControl 0x006ECCA0. When
// the call pushes CameraModeScript over a camera mode of the player, and the
// God view owned the last frame, the mod puts the six Zoomers at the head of
// that frame (Zoomer::SetPosition 0x00441AC0): the position at the head, and
// the focus one meter of the room ahead of it (story_start_math::StartPose).
// The next move of the script then starts at the head, as it starts at the
// camera of the player in the plain game. A cut of the script
// (SET_CAMERA_POSITION) still cuts.
//
// story_start_at_head=1. 0 leaves the camera where the game has it. With
// test_keys=1, Ctrl, Shift and S change it (stereo.cpp).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"
#include "story_start_math.h"

extern void ModLog(const char* format, ...);
// head_camera.cpp: the render head of the last frame in the world, with the
// right, up and forward axes as columns. False in a flat frame of the cut
// scene window, and before the first main call.
bool HeadCameraRenderBasis(float basis_out[9], float position_out[3]);
// head_camera.cpp: the camera the game asked for on the last main call.
bool HeadCameraGameCamera(float position[3], float focus[3]);
// tabletop.cpp: the God view owned the last main camera call, and its scale.
bool TabletopGodOwnsView();
float TabletopViewScale();

namespace {

const uintptr_t kGame = 0x00D0195C;            // GGame::g_game
const int kGameCamera = 0x2502C0;              // GGame + this: the GCamera
const uintptr_t kStartCameraControl = 0x006ECCA0;
const uintptr_t kZoomerSetPosition = 0x00441AC0;
// GCamera: the mode stack and the index of its top (-1 when empty).
const int kModes = 0x28;
const int kModeSlots = 12;
const int kTop = 0x58;
// Three Zoomers for the focus and three for the position, 0x30 bytes each.
// A Zoomer holds its value at +0 and its destination at +4.
const int kCameraFocus = 0x88;
const int kCameraPosition = 0x118;
const int kZoomerSize = 0x30;
// The vtables of the camera modes (script_camera.cpp has the full table).
const uintptr_t kModeScript = 0x008C7D5C;
const uintptr_t kPlayerModes[] = {0x008C7BFC, 0x008C7958, 0x008C7AB4,
                                  0x008C7A58, 0x008C7B58, 0x008C7814};
// The first bytes of GScript::StartCameraControl: mov ecx, [GGame::g_game],
// mov eax, [ecx + 0x205A28].
const uint8_t kStartBytes[12] = {0x8B, 0x0D, 0x5C, 0x19, 0xD0, 0x00,
                                 0x8B, 0x81, 0x28, 0x5A, 0x20, 0x00};
// The frames after a start that the log checks the camera of the game.
const int kCheckFrames = 3;

typedef void(__cdecl* StartFn)();
typedef void(__fastcall* ZoomerSetFn)(void* self, void* unused, float value);

StartFn o_Start = nullptr;
bool g_installed = false;
bool g_on = true;
// The pose that the last start put the camera at, and the frames that the
// log still checks.
float g_start_position[3] = {0.0f, 0.0f, 0.0f};
float g_start_focus[3] = {0.0f, 0.0f, 0.0f};
int g_check = 0;
// For the report.
uint64_t g_starts = 0;
uint64_t g_moved = 0;
int g_logs = 0;
const int kMaxLogs = 120;

bool Say() {
  if (g_logs >= kMaxLogs) return false;
  ++g_logs;
  return true;
}

uint8_t* GameCamera() {
  uint8_t* const game = *reinterpret_cast<uint8_t* const*>(kGame);
  if (game == nullptr || IsBadReadPtr(game + kGameCamera, sizeof(void*))) {
    return nullptr;
  }
  uint8_t* const camera = *reinterpret_cast<uint8_t* const*>(game + kGameCamera);
  if (camera == nullptr ||
      IsBadReadPtr(camera, kCameraPosition + 3 * kZoomerSize)) {
    return nullptr;
  }
  return camera;
}

// The vtable of the top camera mode, or 0.
uintptr_t TopMode(const uint8_t* camera) {
  if (camera == nullptr) return 0;
  const int32_t top = *reinterpret_cast<const int32_t*>(camera + kTop);
  if (top < 0 || top >= kModeSlots) return 0;
  const uintptr_t* const mode =
      *reinterpret_cast<const uintptr_t* const*>(camera + kModes + 4 * top);
  if (mode == nullptr || IsBadReadPtr(mode, sizeof(uintptr_t))) return 0;
  return *mode;
}

bool PlayerMode(uintptr_t vtable) {
  for (uintptr_t mode : kPlayerModes) {
    if (vtable == mode) return true;
  }
  return false;
}

float* Zoomer(uint8_t* camera, int first, int axis) {
  return reinterpret_cast<float*>(camera + first + axis * kZoomerSize);
}

float Distance(const float a[3], const float b[3]) {
  const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
  return sqrtf(dx * dx + dy * dy + dz * dz);
}

void __cdecl h_Start() {
  uint8_t* camera = GameCamera();
  const uintptr_t before = TopMode(camera);
  const bool god = TabletopGodOwnsView();
  const float scale = TabletopViewScale();
  o_Start();
  camera = GameCamera();
  const uintptr_t after = TopMode(camera);
  const bool taken = PlayerMode(before) && after == kModeScript;
  if (!taken) return;
  ++g_starts;
  float basis[9], head[3];
  const bool have_head = HeadCameraRenderBasis(basis, head);
  float position[3], focus[3];
  const bool pose =
      have_head && story_start_math::StartPose(head, basis, scale, position,
                                               focus);
  if (!story_start_math::Wanted(g_on, god, true, true, pose)) {
    if (Say()) {
      ModLog("story start  : a script took the camera (START_CAMERA_CONTROL). "
             "The camera of the game stays where it is (%s)",
             !g_on   ? "story_start_at_head=0"
             : !god  ? "the God view did not own the last frame"
                     : "no head");
    }
    return;
  }
  float was_position[3], was_focus[3];
  const ZoomerSetFn set = reinterpret_cast<ZoomerSetFn>(kZoomerSetPosition);
  for (int axis = 0; axis < 3; ++axis) {
    float* p = Zoomer(camera, kCameraPosition, axis);
    float* f = Zoomer(camera, kCameraFocus, axis);
    was_position[axis] = p[0];
    was_focus[axis] = f[0];
    set(p, nullptr, position[axis]);
    set(f, nullptr, focus[axis]);
    g_start_position[axis] = position[axis];
    g_start_focus[axis] = focus[axis];
  }
  ++g_moved;
  g_check = kCheckFrames;
  if (Say()) {
    ModLog("story start  : a script took the camera (START_CAMERA_CONTROL). "
           "The camera of the game was at %.1f %.1f %.1f and looked at %.1f "
           "%.1f %.1f. It is now at your head at %.1f %.1f %.1f and looks at "
           "%.1f %.1f %.1f, %.1f units from where it was, so the next move "
           "of the story starts at your head (story_start_at_head=1)",
           was_position[0], was_position[1], was_position[2], was_focus[0],
           was_focus[1], was_focus[2], position[0], position[1], position[2],
           focus[0], focus[1], focus[2], Distance(was_position, position));
  }
}

bool SameBytes(uintptr_t address, const uint8_t* want, int count) {
  const void* at = reinterpret_cast<const void*>(address);
  return !IsBadReadPtr(at, count) && memcmp(at, want, count) == 0;
}

}  // namespace

void StoryStartSetOptions(bool on) { g_on = on; }

void StoryStartInstall() {
  if (g_installed) return;
  if (!SameBytes(kStartCameraControl, kStartBytes, sizeof(kStartBytes))) {
    g_on = false;
    ModLog("hooks        : GScript::StartCameraControl at 0x%08X does not "
           "have the expected bytes, so story_start_at_head is 0",
           kStartCameraControl);
    return;
  }
  void* target = reinterpret_cast<void*>(kStartCameraControl);
  MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(&h_Start),
                                   reinterpret_cast<void**>(&o_Start));
  if (status == MH_OK) status = MH_EnableHook(target);
  g_installed = status == MH_OK;
  ModLog("hooks        : GScript::StartCameraControl at 0x%08X %s",
         kStartCameraControl, g_installed ? "ok" : "FAILED");
  if (!g_installed) return;
  ModLog("story start  : story_start_at_head=%d. %s", g_on ? 1 : 0,
         g_on ? "When a script takes the camera in the God view, the camera "
                "of the game goes to your head, so the move of the story "
                "starts there"
              : "When a script takes the camera, the camera of the game "
                "stays where it is");
}

// Once a frame, after the frame (stereo.cpp). The log gives the camera of
// the game in the first frames after a start, as the proof that the move of
// the story starts at the head.
void StoryStartFrame(uint64_t frame) {
  if (g_check <= 0) return;
  --g_check;
  float position[3], focus[3];
  if (!HeadCameraGameCamera(position, focus) || !Say()) return;
  ModLog("story start  : frame %llu: the camera of the game is at %.1f %.1f "
         "%.1f, %.1f units from the head of the start, and looks at %.1f "
         "%.1f %.1f", frame, position[0], position[1], position[2],
         Distance(position, g_start_position), focus[0], focus[1], focus[2]);
}

void StoryStartToggle(const char* keys) {
  g_on = !g_on;
  ModLog("test key     : %s: story_start_at_head=%d. %s", keys, g_on ? 1 : 0,
         g_on ? "The move of a story starts at your head"
              : "The move of a story starts at the camera of the game");
}

void StoryStartReport() {
  if (g_starts == 0) return;
  ModLog("story start  : scripts took the camera %llu times, and the camera "
         "went to your head %llu times (story_start_at_head=%d)", g_starts,
         g_moved, g_on ? 1 : 0);
  g_starts = 0;
  g_moved = 0;
}
