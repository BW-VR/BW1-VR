// THE VALUES OF A HEADSET (frame_defaults and quest_defaults of bwvr.ini, docs/android.md, "The
// values of the Steam Frame" and "The values of a Quest 3"). The Steam Frame and a Quest 3 have one
// APK, and each list has only values that have run on its headset. So the defaults of the keys stay
// for each other headset, and on these two the app puts each value of the list that bwvr.ini does
// not have into the file. A value that the file has stays, also a 0. It has no Android code, so the
// math test of the PC checks it (test/controller_math/remote_checks.h).
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
    {"late_prebuild", "1"},
    {"late_fast_cores", "1"},
    {"late_top_game", "1"},
};

// THE VALUES OF A QUEST 3. No key of the driver of the Steam Frame (tu_debug), and no binding of
// threads to cores (late_fast_cores, late_top_game), because the cores of a Quest 3 are different.
// The family of a Quest 3 has more than one queue, and the compose at the take needs one queue for
// the session and the renderer.
constexpr Value kQuestValues[] = {
    // The outer image and the centre image, composed at the take of a picture.
    {"fovea_res", "1"},
    {"fovea_res_outer_pct", "20"},
    {"fovea_res_center_pct", "100"},
    {"fovea_res_center_deg", "17"},
    {"fovea_res_center_down_deg", "10"},
    {"frame_loop_queues", "1"},
    {"xr_compose_direct", "1"},
    {"xr_compose_take", "1"},
    {"xr_eye_mutable", "1"},
    {"vk_compose_fast", "1"},
    {"vk_compose_rate", "2"},
    // The draws of a walk.
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
    {"late_prebuild", "1"},
};

inline bool IsFrameRuntime(const std::string& runtime) {
  return runtime.find("SteamVR") != std::string::npos;
}

// The runtime of Meta on a headset with the model "Quest 3" or "Quest 3S".
inline bool IsQuest3(const std::string& runtime, const std::string& model) {
  return runtime.find("Oculus") != std::string::npos && model.find("Quest 3") != std::string::npos;
}

// Puts each value of the list that the text does not have, and gives their count.
template <int N>
inline int Seed(bwvr_ini::IniText* ini, const Value (&values)[N]) {
  int added = 0;
  for (const Value& v : values) {
    if (ini->Has("bwvr", v.key)) continue;
    ini->Set("bwvr", v.key, v.text);
    ++added;
  }
  return added;
}

}  // namespace frame_defaults
