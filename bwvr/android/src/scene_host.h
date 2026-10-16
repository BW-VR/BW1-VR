// The state of a walk for the Vulkan renderer of the mod in the app
// (scene_host.cpp). On the PC other parts of the mod give these values. In
// the app the stream of the game gives them with each walk and each draw.
#pragma once

struct SceneHostState {
  // stereo.cpp, StereoPassEye: the eye pass now (0 left, 1 right), or -1
  // outside the eye passes.
  int pass_eye = -1;
  // intro_vr.cpp, IntroPassEye: the eye pass of the intro in stereo, or -1.
  int intro_eye = -1;
  // one_walk.cpp: one walk for both eyes, its shift of the right eye, the
  // render target that it binds, and the mode of the batch now.
  bool one_walk = false;
  float shift_per_rhw = 0.0f;
  void* bound_target = nullptr;
  int one_walk_mode = 0;
  // ui_panel.h, GpuBridgeIsPanel: the surface of the panel target.
  const void* panel_target = nullptr;
};

void SceneHostSetState(const SceneHostState& state);
const SceneHostState& SceneHostGetState();

// The folder for the pictures of a test of the renderer.
void VkPlatformSetFolder(const char* folder);
