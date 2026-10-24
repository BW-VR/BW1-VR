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

#include "../../src/common/input_channel.h"
#include "../../src/mod/quad_layer_math.h"
#include "../../src/mod/scene_stream.h"
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
  // The menu button (the actions of the mod, SetInputLive).
  bool menu[2] = {false, false};
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
  // A pulse of the mod (the ring of the events of the input channel):
  // amplitude 0 to 1, seconds.
  void PulseWith(int hand, float amplitude, float seconds);

  // THE INPUT OF THE MOD (input_live=1, docs/android.md, "The input
  // channel"). Call SetInputLive before Start: the session then has the
  // actions of the mod as well (the grip pose, the trigger and the squeeze
  // as values, the stick click, the face buttons and the menu button), with
  // the bindings of xr_screen.cpp of the PC. After BeginFrame, InputSlot
  // gives what the mod reads from a session of its own: the views of the
  // eyes (xrLocateViews), the hands after xrSyncActions, the display time of
  // the frame, the focus, the count of the recenters and the profiles. The
  // poses are the poses of the runtime at the display time, with no
  // smoothing and no prediction of the app.
  void SetInputLive(bool on) { input_live_ = on; }
  void InputSlot(input_channel::SlotData* out);

  // THE MENU QUAD. Shown (the default): the menu and the cursor go out over
  // each other layer. Not shown (the game runs and the player plays): no
  // menu quad and no cursor.
  void SetMenuShown(bool on) { menu_shown_ = on; }
  // The picture of the renderer goes out of the layers until the next
  // picture (the game ended).
  void ClearScreenSource() { screen_source_ = VK_NULL_HANDLE; }

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
  bool HasScreen() const { return screen_.between != VK_NULL_HANDLE; }

  // THE PROJECTION LAYER (stream_projection, docs/android.md, "The
  // projection layer"). On (1, the default): a picture of the renderer with
  // the values of the projection layer of the mod (the kView of its frame,
  // src/mod/view_layer_math.h, ShowsProjection) shows in a projection layer,
  // each eye with the pose and the angles of view that the mod gives. The
  // runtime reprojects it to the pose of the head at the display time. A
  // picture with no such values shows on the stereo screen. Off (0): each
  // picture shows on the stereo screen. Before Start.
  void SetProjection(bool on) { projection_on_ = on; }
  bool ProjectionOn() const { return projection_on_; }
  // The two swapchains of the projection layer, width by height (the size of
  // the picture of the renderer). After one fault it gives false with no new
  // try, and the pictures show on the stereo screen.
  bool MakeEyes(uint32_t width, uint32_t height);
  bool HasEyes() const { return eyes_.between != VK_NULL_HANDLE; }
  // The values of the projection layer of the newest picture (after
  // SetScreenSource or MarkScreenChanged). has: false when that picture has
  // none, and then it shows on the stereo screen.
  // view.frame is the frame of the mod of the picture also when has is false.
  void SetScreenView(bool has, const scene_stream::ViewBody& view);
  // xr_copy_on_change (1 is the default): true, a layer gets a new image of
  // its swapchain only when its picture changed, and the runtime shows the
  // last released image. false, each frame copies each layer. Before Start.
  void SetCopyOnChange(bool on) { copy_on_change_ = on; }

  // THE QUAD LAYERS (stream_layers, docs/android.md, "The quad layers"). On
  // (1, the default): the cards, the panel quads and the cut scene window of
  // the mod for the frame of the newest picture (quad_layer_math.h,
  // PictureLayers) show as the session of the mod shows them: quad layers
  // over the projection layer, in the order of XrScreen::BuildFrame, and a
  // frame of the cut scene window as a quad of each eye at the pose of the
  // window. The menu and the cursor stay over them. Off (0): no quad layer of
  // the mod, and a frame of the cut scene window shows on the stereo screen.
  // Before Start.
  void SetLayersOn(bool on) { layers_on_ = on; }
  bool LayersOn() const { return layers_on_; }
  // The layers of the newest picture (after SetScreenSource or
  // MarkScreenChanged), the pictures of the cards of the stream, and the
  // panel image of the renderer for that picture (VkScenePanelFor, a VkImage
  // of panel_width by panel_height in the layout of a color attachment), or
  // VK_NULL_HANDLE. A card gets new pixels only when its picture, its rect or
  // the size of its texture changed.
  void SetScreenLayers(const quad_layer_math::FrameLayers& layers,
                       const quad_layer_math::AppPictures& pictures, VkImage panel,
                       uint32_t panel_width, uint32_t panel_height);

  // THE SCREEN QUAD (stream_screen, docs/android.md, "The screens with no
  // scene"). On (1, the default): a frame with no scene of the mod (the
  // logos, the videos, the front end, a dialog box) shows on one quad fixed
  // in the room, with the pose and the size of its kScreen
  // (src/mod/screen_quad_math.h), as the screen of the PC. It is never a
  // projection layer. The menu and the cursor stay over it. Off (0): the
  // app shows no frame with no scene. Before Start.
  void SetScreenQuadOn(bool on) { screen_quad_on_ = on; }
  bool ScreenQuadOn() const { return screen_quad_on_; }
  // A new screen picture: layer `layer` of `image` (the picture of the last
  // walk of the renderer, image_width by image_height, in the layout of a
  // color attachment), or the pixels of the CPU (RGBA, width by height). The
  // quad shows the picture in place of the pictures of the world until
  // ClearScreenQuad.
  void SetScreenQuadImage(const scene_stream::ScreenBody& body, VkImage image,
                          uint32_t layer, uint32_t image_width, uint32_t image_height);
  void SetScreenQuadPixels(const scene_stream::ScreenBody& body,
                           const std::vector<uint8_t>& rgba, uint32_t width,
                           uint32_t height);
  // The pictures of the world come again, or the game ended.
  void ClearScreenQuad() { flat_on_ = false; }
  bool ScreenQuadShown() const { return flat_on_; }
  // The kScreen of the newest screen picture.
  const scene_stream::ScreenBody& ScreenQuadBody() const { return flat_body_; }
  // THE MENU PANEL QUAD THAT ASKS FOR THE KEYBOARD: true when the last frame
  // showed the menu panel quad of the mod with kPanelQuadText (a dialog box of
  // the game in a frame of the scene takes text), with its pose and its size.
  bool PanelTextQuad(float position[3], float orientation[4], float size[2]) const;

  // THE KEYBOARD OF THE APP (screen_keyboard, docs/android.md, "The keyboard
  // of the app"): one quad for both eyes with the picture of the keyboard
  // (RGBA, with the color multiplied by the alpha, width by height), at the
  // pose and the size of SetKeyboard, after the screen quad and before the
  // menu. The swapchain is made at the first pixels.
  void SetKeyboardPixels(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height);
  void SetKeyboard(bool shown, const float position[3], const float orientation[4],
                   const float size[2]);

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
  // THE EYE PICTURES OF THE RENDERER in two swapchains: the stereo screen (two
  // quads), or the projection layer.
  struct Screen {
    Layer layer[2];
    VkImage between = VK_NULL_HANDLE;  // the picture at the size of the swapchains
    VkDeviceMemory between_memory = VK_NULL_HANDLE;
    uint64_t copies[2] = {0, 0};  // the copies of each eye
    bool failed = false;  // the set could not be made, and the app does not try again
  };
  // False with the reason in *why, and then the set has no part. count: the
  // swapchains of the set (2 for the eyes, 1 for the panel).
  bool MakeScreenSet(Screen* screen, uint32_t width, uint32_t height,
                     const char** why, int count = 2);
  void FreeScreenSet(Screen* screen);
  // One eye of the picture of the renderer into the next image of its
  // swapchain. True when the eye released a new image in this call.
  bool CopyScreen(Screen* screen, int eye);
  // Layer `source_layer` of `source` (source_width by source_height, in the
  // layout of a color attachment) into the next image of swapchain `index`
  // of the set, with a blit to the size of the swapchain and a copy.
  bool CopyImage(Screen* screen, int index, VkImage source, uint32_t source_layer,
                 uint32_t source_width, uint32_t source_height);
  void SetPixels(Layer* layer, const std::vector<uint8_t>& rgba);

  // THE QUAD LAYERS OF THE MOD.
  struct Card {
    Layer layer;
    bool failed = false;   // the runtime gave no swapchain: the card does not show
    uint64_t picture = 0;  // the hash of the picture in the staging buffer
    int32_t rect[4] = {0, 0, 0, 0};
    bool said = false;
    uint64_t frames = 0;   // frames with this card
  };
  // The pixels of a card into its staging buffer, at its rect. True when
  // the card has the picture of `body` after the call.
  bool TakeCard(int index, const scene_stream::CardBody& body,
                const quad_layer_math::AppPictures& pictures);
  void FreeLayers();
  bool layers_on_ = true;  // stream_layers
  quad_layer_math::FrameLayers newest_layers_;
  quad_layer_math::FrameLayers shown_layers_;
  Card cards_[bwvr::kCards];
  Screen panel_;  // one swapchain of the size of the panel image
  VkImage panel_source_ = VK_NULL_HANDLE;
  uint32_t panel_width_ = 0;
  uint32_t panel_height_ = 0;
  bool panel_shown_ = false;  // the released image has the panel of shown_layers_
  Layer wall_;                // the walls of the cut scene window: black with an alpha
  bool wall_failed_ = false;
  int wall_alpha_ = -1;       // the alpha of the staging buffer of the walls
  uint64_t panel_frames_ = 0;
  uint64_t wall_frames_ = 0;
  uint64_t window_frames_ = 0;
  bool said_panel_ = false;
  // The menu panel quad of the last frame that asks for the keyboard
  // (PanelTextQuad).
  bool panel_text_ = false;
  float panel_text_position_[3] = {0.0f, 0.0f, 0.0f};
  float panel_text_orientation_[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float panel_text_size_[2] = {0.0f, 0.0f};
  bool said_walls_ = false;
  bool said_window_ = false;

  // THE KEYBOARD.
  Layer keyboard_;
  bool keyboard_failed_ = false;
  bool keyboard_written_ = false;
  bool keyboard_shown_ = false;
  float keyboard_position_[3] = {0.0f, 0.0f, 0.0f};
  float keyboard_orientation_[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float keyboard_size_[2] = {0.0f, 0.0f};
  uint64_t keyboard_frames_ = 0;

  // THE SCREEN QUAD.
  bool screen_quad_on_ = true;  // stream_screen
  bool flat_on_ = false;        // the newest picture is a screen
  bool flat_cpu_ = false;       // its source is the pixels of the CPU
  scene_stream::ScreenBody flat_body_ = {};
  Screen flat_;                 // one swapchain for the picture of the walk
  Layer flat_pixels_;           // a swapchain with staging for the pixels of the CPU
  bool flat_pixels_failed_ = false;
  VkImage flat_source_ = VK_NULL_HANDLE;
  uint32_t flat_layer_ = 0;
  uint32_t flat_width_ = 0;
  uint32_t flat_height_ = 0;
  uint64_t flat_frames_ = 0;    // frames of the session with the screen quad
  uint64_t flat_pictures_ = 0;  // new screen pictures
  // The source, the size and the quad in the log, for one line at a change.
  int said_flat_source_ = -1;
  uint32_t said_flat_width_ = 0;
  uint32_t said_flat_height_ = 0;
  float said_flat_size_[2] = {0.0f, 0.0f};
  int flat_lines_ = 0;

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
  // The actions of the mod (input_live=1).
  bool input_live_ = false;
  bool synced_ = false;  // xrSyncActions of this frame gave the input
  XrAction grip_ = XR_NULL_HANDLE;
  XrAction trigger_ = XR_NULL_HANDLE;
  XrAction squeeze_ = XR_NULL_HANDLE;
  XrAction stick_click_ = XR_NULL_HANDLE;
  XrAction primary_ = XR_NULL_HANDLE;
  XrAction secondary_ = XR_NULL_HANDLE;
  XrAction menu_button_ = XR_NULL_HANDLE;
  XrSpace grip_space_[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
  uint32_t recenters_ = 0;
  uint32_t serial_[2] = {0, 0};
  std::string profile_[2];
  bool profiles_read_ = false;
  void ReadProfiles();
  XrPath hands_[2] = {XR_NULL_PATH, XR_NULL_PATH};
  XrSpace aim_space_[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};

  XrFrameState frame_ = {XR_TYPE_FRAME_STATE};
  Gpu gpu_;
  Layer menu_;
  Layer cursor_;
  bool menu_written_ = false;
  bool cursor_written_ = false;
  bool menu_shown_ = true;

  bool copy_on_change_ = true;
  uint64_t copies_ = 0;
  uint64_t copies_skipped_ = 0;
  bool renderer13_ = false;
  bool renderer_used_ = false;
  bool shading_rate_ = false;
  float max_anisotropy_ = 0.0f;
  Screen screen_;  // the stereo screen
  Screen eyes_;    // the projection layer
  bool projection_on_ = true;  // stream_projection
  // The values of the projection layer of the newest picture, and of the
  // picture in the released image of each eye of the projection layer.
  bool view_newest_ = false;
  scene_stream::ViewBody newest_view_ = {};
  bool view_shown_[2] = {false, false};
  scene_stream::ViewBody shown_view_[2] = {};
  uint64_t projection_frames_ = 0;  // frames of the session with the projection layer
  uint64_t screen_frames_ = 0;      // frames of the session with the stereo screen
  bool said_projection_ = false;
  uint64_t said_frame_ = 0;  // the frame of the last picture of the layer in the log
  // The frame of the mod of the newest picture and of the picture of the
  // released images of the eyes, for the log of the cut scene window.
  uint64_t newest_frame_ = 0;
  uint64_t shown_frame_ = 0;
  uint64_t said_window_frame_ = 0;
  bool said_no_view_ = false;
  VkImage screen_source_ = VK_NULL_HANDLE;
  uint32_t source_width_ = 0;
  uint32_t source_height_ = 0;
};
