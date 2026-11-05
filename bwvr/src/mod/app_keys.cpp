// The keys that the debug page changes for the renderer and the app
// (app_keys.h).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

#include "app_keys.h"
#include "vk_scene.h"

void ModLog(const char* format, ...);

namespace {

using namespace app_keys;

// The ini key and the default of each key (the same as the readers in
// vk_scene.cpp and in the app).
const char* const kName[kKeys] = {"vk_mres_draws",  "vk_mres_sort",      "vk_compose_light",
                                  "vk_pass_store",  "xr_compose_direct", "xr_panel_rect",
                                  "ring_slot_order", "late_camera",  "xr_release_take",
                                  "late_take", "vk_bindless", "xr_panel_keep",
                                  "vk_fast_shaders", "vk_tiny_draws", "vk_compose_fast", "vk_list_keep", "vk_cheap_draws", "late_grab", "late_prepare",
                                  "late_prepare_gap", "late_prebuild", "late_fast_cores"};
const int kDefault[kKeys] = {1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int g_value[kKeys] = {1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
bool g_read = false;

void Read() {
  if (g_read) return;
  g_read = true;
  char path[MAX_PATH] = "";
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash != nullptr && strlen(path) + 9 < MAX_PATH) {
    slash[1] = 0;
    strcat_s(path, "bwvr.ini");
    for (int i = 0; i < kKeys; ++i) {
      g_value[i] = GetPrivateProfileIntA("bwvr", kName[i], kDefault[i], path) != 0 ? 1 : 0;
    }
    // THE TINY DRAWS: the mod leaves them out before they go to the app.
    VkSceneSetTinyPixels(static_cast<float>(GetPrivateProfileIntA("bwvr", "vk_tiny_pixels", 10, path)) *
                         0.1f);
    VkSceneSetTinyDraws(g_value[kKeyTinyDraws] != 0);
  }
}

}  // namespace

uint32_t AppKeysBits() {
  Read();
  return Pack(g_value);
}

int AppKeyValue(int key) {
  Read();
  return key >= 0 && key < kKeys ? g_value[key] : 0;
}

void AppKeyChange(int key, const char* by) {
  Read();
  if (key < 0 || key >= kKeys) return;
  g_value[key] = g_value[key] != 0 ? 0 : 1;
  // A renderer in this process (a run with no headset app) takes the values
  // now. The app takes them from the next frame of the stream.
  VkSceneSetAppKeys(g_value[kKeyMresDraws], g_value[kKeyMresSort], g_value[kKeyComposeLight],
                    g_value[kKeyPassStore]);
  VkSceneSetTinyDraws(g_value[kKeyTinyDraws] != 0);
  ModLog("test key     : %s, %s is %d from the next frame of the stream (the app plays it "
         "with the frame)",
         by, kName[key], g_value[key]);
}
