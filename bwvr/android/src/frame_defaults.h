// THE VALUES OF THE STEAM FRAME (frame_defaults of bwvr.ini, docs/android.md, "The values of
// the Steam Frame"). The Steam Frame and a Quest 3 have one APK, and these values have run
// only on the Steam Frame. So the defaults of the keys stay for each other runtime, and with
// SteamVR the app puts each value that bwvr.ini does not have into the file. A value that the
// file has stays, also a 0. It has no Android code, so the math test of the PC checks it
// (test/controller_math/remote_checks.h).
#pragma once

#include <string>

#include "../../src/common/ini_text.h"

namespace frame_defaults {

struct Value {
  const char* key;
  const char* text;
};

constexpr Value kValues[] = {
    // The outer image and the centre image, composed at the take of a picture.
    {"fovea_res", "1"},
    {"fovea_res_outer_pct", "25"},
    {"fovea_res_center_pct", "100"},
    {"fovea_res_center_deg", "20"},
    {"fovea_res_center_down_deg", "10"},
    {"xr_compose_direct", "1"},
    {"xr_compose_take", "1"},
    {"xr_eye_mutable", "1"},
    {"vk_compose_fast", "1"},
    {"vk_compose_rate", "2"},
    // The draws of a walk.
    {"tu_debug", "sysmem"},
    {"vk_bindless", "1"},
    {"vk_fast_shaders", "1"},
    {"vk_list_keep", "1"},
    {"vk_cheap_draws", "1"},
    {"vk_tiny_draws", "1"},
    {"vk_tiny_pixels", "30"},
    {"detail_by_scale", "1"},
    // The panel, and the motion between the frames of the game.
    {"xr_panel_keep", "1"},
    {"move_stick_cap_ms", "250"},
    {"late_grab", "1"},
    {"late_prepare", "1"},
    {"late_prepare_gap", "1"},
};

inline bool IsFrameRuntime(const std::string& runtime) {
  return runtime.find("SteamVR") != std::string::npos;
}

// Puts each value that the text does not have, and gives their count.
inline int Seed(bwvr_ini::IniText* ini) {
  int added = 0;
  for (const Value& v : kValues) {
    if (ini->Has("bwvr", v.key)) continue;
    ini->Set("bwvr", v.key, v.text);
    ++added;
  }
  return added;
}

}  // namespace frame_defaults
