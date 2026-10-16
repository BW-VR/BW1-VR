// THE VULKAN RENDERER OF THE MOD IN THE APP (docs/android.md, "The renderer
// in the app").
//
// The app compiles vk_core.cpp and vk_scene.cpp of the mod. On the PC the
// renderer asks other parts of the mod for the state of a walk: one walk,
// the eye pass, the panel target. In the app these functions give the state
// that the stream of the game sends with each walk (scene_host.h).

#include "scene_host.h"

#include <stdarg.h>
#include <stdio.h>

#include <string>

#include "app_log.h"

namespace {

SceneHostState g_state;
std::string g_folder = ".";

}  // namespace

void SceneHostSetState(const SceneHostState& state) { g_state = state; }
const SceneHostState& SceneHostGetState() { return g_state; }

// ---- The functions of the mod that vk_scene.cpp calls -----------------------

void ModLog(const char* format, ...) {
  char line[2048];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  AppLog("%s", line);
}

// The pulse of a test key. The app has no test keys of the game.
void ControllersGesturePulse(float, float) {}

int StereoPassEye() { return g_state.pass_eye; }
int IntroPassEye() { return g_state.intro_eye; }

bool OneWalkActive() { return g_state.one_walk; }
float OneWalkShiftPerRhw() { return g_state.shift_per_rhw; }
void* OneWalkBoundTarget() { return g_state.bound_target; }
int OneWalkModeNow() { return g_state.one_walk_mode; }

bool GpuBridgeIsPanel(const void* surface) {
  return surface != nullptr && surface == g_state.panel_target;
}

void VkPlatformSetFolder(const char* folder) { g_folder = folder; }
const char* VkPlatformFolder() { return g_folder.c_str(); }
