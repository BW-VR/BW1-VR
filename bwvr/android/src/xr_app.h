// THE OPENXR SESSION OF THE HEADSET APP (docs/android.md, "The session").
//
// The Khronos loader for Android finds the runtime of the headset (Meta
// Horizon OS, or another runtime that the broker of Khronos names). The
// session has Vulkan (XR_KHR_vulkan_enable2): the runtime makes the instance
// and the device. The menu is a quad of the compositor in front of the
// player, and the cursor of the controller ray is a second, small quad.
#pragma once

#include <android/native_activity.h>
#include <stdint.h>

#include <string>
#include <vector>

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_VULKAN
#include <jni.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "../../src/mod/vk_core.h"
#include "gpu.h"

// The state of the controllers of one frame. Index 0 is the left hand, 1
// the right hand.
struct XrInput {
  bool aim_valid[2] = {false, false};
  float aim_position[2][3] = {};
  float aim_orientation[2][4] = {};
  bool select[2] = {false, false};
  bool back[2] = {false, false};
  float stick[2][2] = {};
};

class XrApp {
 public:
  // False when the device has no OpenXR runtime or no headset. Then the app
  // shows the menu on the screen (flat_view.cpp).
  bool Start(ANativeActivity* activity, uint32_t menu_width,
             uint32_t menu_height, uint32_t cursor_size);
  void Stop();

  // The events of the runtime. quit: the runtime ends the session.
  void PollEvents(bool* quit);
  bool Running() const { return running_; }

  // What the headset asks for, for each eye.
  uint32_t EyeWidth() const { return eye_width_; }
  uint32_t EyeHeight() const { return eye_height_; }
  const std::string& RuntimeName() const { return runtime_name_; }

  // New pixels of the menu or of the cursor (RGBA, with the color multiplied
  // by the alpha). The next frames show them.
  void SetMenuPixels(const std::vector<uint8_t>& rgba);
  void SetCursorPixels(const std::vector<uint8_t>& rgba);

  // ONE FRAME. Begin waits for the time of the frame and reads the input at
  // the time when the frame shows. End gives the layers: the menu quad at
  // menu_center (meters in the LOCAL space) of menu_size, and the cursor at
  // cursor_point when show_cursor.
  bool BeginFrame(XrInput* input);
  void EndFrame(const float menu_center[3], const float menu_size[2],
                bool show_cursor, const float cursor_point[3],
                float cursor_size);
  // A short pulse on one hand, for example when the highlight moves.
  void Pulse(int hand);

  // THE RENDERER OF THE MOD IN THE SESSION (docs/android.md, "The picture
  // of the renderer in the headset"). True when the device of the runtime
  // has Vulkan 1.3 and the features of the renderer.
  bool Renderer13() const { return renderer13_; }
  // The instance, the device and the queue of the session for
  // vk::UseExternal, so the renderer draws on the device of the session.
  void RendererDevice(vk::External* out);
  // A STEREO SCREEN in front of the player: two quads, one for each eye,
  // width by height pixels. image is an image of the renderer with a layer
  // for each eye, in the layout of a color attachment, image_width by
  // image_height (VkSceneEyes). A frame copies it into the quads only after
  // SetScreenSource gave a new picture.
  bool MakeScreen(uint32_t width, uint32_t height);
  void SetScreenSource(VkImage image, uint32_t image_width,
                       uint32_t image_height);
  // The renderer drew a new picture into the image of SetScreenSource. The
  // next frames copy it into the quads, because a quad copies only a
  // picture of a new revision.
  void MarkScreenChanged();
  bool HasScreen() const { return between_ != VK_NULL_HANDLE; }
  // xr_copy_on_change (1 is the default): true, a layer gets a new image of
  // its swapchain only when its picture changed, and the runtime shows the
  // last released image. false, each frame copies each layer. Before Start.
  void SetCopyOnChange(bool on) { copy_on_change_ = on; }

 private:
  struct Layer {
    XrSwapchain swapchain = XR_NULL_HANDLE;
    std::vector<XrSwapchainImageVulkan2KHR> images;
    uint32_t width = 0;
    uint32_t height = 0;
    Staging staging;
    bool swap_red_blue = false;
    // The revision of the content, and the revision of the last image that
    // the app released. 0: no image is released yet.
    unsigned revision = 1;
    unsigned released = 0;
  };
  // True when the layer needs a new image in this frame. Else it counts a
  // skipped copy.
  bool LayerChanged(Layer* layer);

  bool MakeInstance(ANativeActivity* activity);
  bool MakeVulkan();
  bool MakeSession();
  bool MakeActions();
  bool MakeLayer(Layer* layer, uint32_t width, uint32_t height,
                 bool staging = true);
  void CopyLayer(Layer* layer);
  void CopyScreen(int eye);
  void SetPixels(Layer* layer, const std::vector<uint8_t>& rgba);

  XrInstance instance_ = XR_NULL_HANDLE;
  XrSystemId system_ = XR_NULL_SYSTEM_ID;
  XrSession session_ = XR_NULL_HANDLE;
  XrSpace local_ = XR_NULL_HANDLE;
  XrSessionState state_ = XR_SESSION_STATE_UNKNOWN;
  bool running_ = false;
  XrEnvironmentBlendMode blend_ = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  int64_t format_ = 0;
  uint32_t eye_width_ = 0;
  uint32_t eye_height_ = 0;
  std::string runtime_name_;

  XrActionSet action_set_ = XR_NULL_HANDLE;
  XrAction aim_ = XR_NULL_HANDLE;
  XrAction select_ = XR_NULL_HANDLE;
  XrAction back_ = XR_NULL_HANDLE;
  XrAction stick_ = XR_NULL_HANDLE;
  XrAction haptic_ = XR_NULL_HANDLE;
  XrPath hands_[2] = {XR_NULL_PATH, XR_NULL_PATH};
  XrSpace aim_space_[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};

  XrFrameState frame_ = {XR_TYPE_FRAME_STATE};
  Gpu gpu_;
  Layer menu_;
  Layer cursor_;
  bool menu_written_ = false;
  bool cursor_written_ = false;

  bool copy_on_change_ = true;
  uint64_t copies_ = 0;
  uint64_t copies_skipped_ = 0;
  uint64_t screen_copies_[2] = {0, 0};  // the copies of each eye of the stereo screen
  bool renderer13_ = false;
  bool renderer_used_ = false;
  bool shading_rate_ = false;
  float max_anisotropy_ = 0.0f;
  Layer screen_[2];
  VkImage between_ = VK_NULL_HANDLE;  // the picture at the size of the quads
  VkDeviceMemory between_memory_ = VK_NULL_HANDLE;
  VkImage screen_source_ = VK_NULL_HANDLE;
  uint32_t source_width_ = 0;
  uint32_t source_height_ = 0;
};
