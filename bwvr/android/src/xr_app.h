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

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_TIMESPEC
#include <jni.h>
#include <time.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "../../src/common/input_channel.h"
#include "../../src/mod/eye_copy_math.h"
#include "../../src/mod/fovea_res_math.h"
#include "../../src/mod/quad_layer_math.h"
#include "../../src/mod/scene_stream.h"
#include "../../src/mod/swapchain_free_math.h"
#include "../../src/mod/take_age_math.h"
#include "../../src/mod/time_log_math.h"
#include "../../src/mod/vk_core.h"
#include "frame_loop.h"
#include "gpu.h"
#include "motion_comp.h"

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
  // The click of each stick (the actions of the mod, SetInputLive).
  bool stick_click[2] = {false, false};
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

  // What the headset asks for, for each eye, after the cap of eye_size_max
  // (eye_size.h, docs/android.md, "The size of the eye pictures"). The app
  // writes it as headset_width and headset_height, and the render of the
  // game, the swapchains of the projection layer, the panel image and the
  // screen quad take their size from it.
  uint32_t EyeWidth() const { return eye_width_; }
  uint32_t EyeHeight() const { return eye_height_; }
  // eye_size_max of bwvr.ini, before Start: 1 (the default) caps the size of
  // the runtime at the panel of a Quest 3 (2064 by 2208) with the aspect of
  // the runtime. 0: the size of the runtime, with no cap.
  void SetEyeSizeMax(int key) { eye_size_max_ = key; }

  // THE TIME LOG (time_log, docs/android.md, "The time log of the app").
  // Before Start: the clock reads of the frames, the waits of the copies for
  // the GPU and the timestamps of the command buffers of the app. TakeTimes
  // gives the stats since its last call and sets them back: the wait in
  // xrWaitFrame, the time from the end of xrWaitFrame to the end of
  // xrEndFrame, the time in xrEndFrame, the waits of the app for the fence of
  // its last copy, and the time of the GPU of each command buffer of the app.
  // THE STEPS of a frame of the session from the end of xrWaitFrame to the
  // end of xrEndFrame, each the sum in ms of one frame (AddStep for the
  // steps of main.cpp). kLock is the part of the other steps that waited
  // for the lock of the queue.
  enum Step {
    kStepBegin,      // xrBeginFrame
    kStepActions,    // xrSyncActions, the states and the poses of the actions
    kStepTake,       // the picture of the ring of the pictures (frame_loop=1)
    kStepSlot,       // the slot of the input channel (xrLocateViews) and the pulses
    kStepUi,         // the menu button, the keyboard, the controllers, the paint
    kStepPlay,       // the play of a batch on this thread (the old loop)
    kStepLayers,     // the rest of EndFrame: the records of the copies, the layers
    kStepAcquire,    // xrAcquireSwapchainImage
    kStepImageWait,  // xrWaitSwapchainImage
    kStepSubmit,     // vkQueueSubmit of the copies
    kStepRelease,    // xrReleaseSwapchainImage
    kStepEnd,        // xrEndFrame
    kStepLock,       // of them, the waits for the lock of the queue
    kSteps
  };
  // THE PARTS OF THE LAYERS (kStepLayers), each the sum in ms of one frame,
  // the time of the clock and the CPU time of the thread of the session.
  // kPartSwapchain is the time of the steps of the swapchains and of the
  // submit in the layers, which the step line has as steps of their own.
  enum LayerPart {
    kPartBegin,      // the begin of the command buffer of the frame (GpuBegin)
    kPartEyes,       // the records of the copies of the eye pictures
    kPartPanel,      // the panel image
    kPartCards,      // the cards
    kPartWalls,      // the walls of the cut scene window
    kPartScreen,     // the screen quad
    kPartKeyboard,   // the keyboard of the app
    kPartCursor,     // the cursor
    kPartMenu,       // the menu
    kPartStaging,    // the copies of pixels of the CPU into a staging buffer (memcpy)
    kPartLog,        // the lines of the log of a new picture
    kPartRest,       // the rest: the order, the quads and the projection layer
    kPartSwapchain,  // not a part of kStepLayers
    kParts
  };
  struct FrameTimes {
    time_log_math::Stat wait;
    time_log_math::Stat work;
    time_log_math::Stat end;
    time_log_math::Stat fence_wait;
    time_log_math::Stat busy;
    time_log_math::Stat steps[kSteps];
    uint64_t acquires = 0;     // xrAcquireSwapchainImage calls
    uint64_t not_ready = 0;    // swapchain images that were not free yet (no wait)
    // THE AGE of the shown picture in ms, from the start of its play to the
    // display time of the frame, in each frame (age_ms) and in the frame
    // that shows it first (latency_ms), and from the arrival of its newest
    // frame in the app to the display time of that frame (arrival_ms).
    // Only with XR_KHR_convert_timespec_time.
    time_log_math::Stat age_ms;
    time_log_math::Stat latency_ms;
    time_log_math::Stat arrival_ms;
    // THE PARTS OF THE LAYERS in each frame: the clock and the CPU time of
    // the thread. The pixels of the CPU: the copies from the copy in memory
    // into a staging buffer (with their bytes and the count of copies of a
    // rect), and the writes straight into a staging buffer
    // (xr_staging_direct=1).
    time_log_math::Stat parts[kParts];
    time_log_math::Stat part_cpu[kParts];
    uint64_t staged = 0;
    uint64_t staged_bytes = 0;
    uint64_t staged_rects = 0;
    uint64_t direct = 0;
    uint64_t direct_bytes = 0;
  };
  // The time of one step of main.cpp in this frame (a clock pair).
  void AddStep(Step step, double ms) {
    if (time_log_) step_ms_[step] += ms;
  }
  // The display time of this frame on CLOCK_MONOTONIC in ms, or a negative
  // value when the runtime has no XR_KHR_convert_timespec_time.
  double DisplayTimeMs() const;
  // The shown picture of this frame started its play at play_start_ms, its
  // newest frame came at arrival_ms (CLOCK_MONOTONIC, 0: not known), and
  // fresh: this frame shows it first.
  void ShownPicture(double play_start_ms, double arrival_ms, bool fresh);
  // The other times of a new picture of this frame (frame_loop::AgeTimes),
  // after ShownPicture: the parts of its age go into the time log at the end
  // of xrEndFrame.
  void ShownParts(double input_ms, double ready_ms, double taken_ms);
  void SetTimeLog(bool on) { time_log_ = on; }
  // THE PIXELS OF THE CPU of the layers with the frame loop (docs/android.md,
  // "The frame loop"), before Start.
  // xr_layer_rect=1: a card copies only its rect into the staging buffer and
  // into the swapchain image, because its quad shows only that rect, and it
  // shows only when the released image has that rect. 0 (the default): the
  // whole image. No run has shown a copy of a rect yet.
  // xr_staging_direct=1 (the default): new pixels go straight into the
  // staging buffer when the GPU finished its last copy and no copy in memory
  // waits, so the layers step has no memcpy for them. 0: the pixels go into
  // the copy in memory, and the layers step copies them into the staging
  // buffer.
  void SetLayerRect(bool on) { layer_rect_ = on; }
  void SetStagingDirect(bool on) { staging_direct_ = on; }
  void TakeTimes(FrameTimes* out);
  // The wait in xrWaitFrame of this frame, in ms, and the clock
  // (CLOCK_MONOTONIC, ms) at its end, -1 before the first frame.
  double LastWaitMs() const { return last_wait_ms_; }
  double WaitEndMs() const { return wait_end_ms_; }
  const std::string& RuntimeName() const { return runtime_name_; }

  // THE DISPLAY RATE (XR_FB_display_refresh_rate, docs/android.md, "The
  // display rate"). The rates that the runtime gives, in Hz, after Start.
  // Empty when the runtime does not list the extension: then the app asks
  // for no rate.
  const std::vector<float>& DisplayRates() const { return display_rates_; }
  // Asks the runtime for a rate of DisplayRates, or for 0 (no preference:
  // the runtime uses its own rate). The log has the request and the rate
  // that the runtime uses after it. False with no extension or when the
  // runtime refuses the rate.
  bool RequestDisplayRate(float hz);

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
  // THE LATE CAMERA (docs/android.md, "The late camera"): the middle of the eyes
  // at a display time, in the LOCAL space. False when the runtime has no valid
  // place for that time.
  bool LocateHead(int64_t display_time, float head[3]);

  // THE GAZE OF AN EYE TRACKER (XR_EXT_eye_gaze_interaction, docs/android.md,
  // "The input channel"). Call SetEyeGaze(true) before Start: the instance
  // then asks for the extension when the runtime lists it, and the session
  // has an action for the gaze when the headset has an eye tracker. On Meta
  // Horizon OS the app asks for the permission of the eye tracking.
  // InputSlot gives the gaze in the space of the head (x right, y up, -z
  // forward) at the display time of the frame, with kGazeValid.
  void SetEyeGaze(bool on) { gaze_wanted_ = on; }

  // THE MENU QUAD. Shown (the default): the menu and the cursor go out over
  // each other layer. Not shown (the game runs and the player plays): no
  // menu quad and no cursor.
  void SetMenuShown(bool on) { menu_shown_ = on; }
  // The picture of the renderer goes out of the layers until the next
  // picture (the game ended).
  void ClearScreenSource() {
    screen_source_ = VK_NULL_HANDLE;
    direct_newest_ = -1;
  }
  // The session took a new picture of the ring: the slot of the picture
  // before is free, and the play thread can write it or replace its
  // swapchains. No layer names a slot until SetDirectSource names the slot
  // of this picture, so direct_newest_ is -1 or the shown slot.
  void ClearDirectSource() { direct_newest_ = -1; }

  // THE AGE LOG (take_age_log=1, take_age_math.h, docs/android.md, "The age of
  // a picture in the layer"). SetAgeLog: at each Play, before the play thread
  // starts. AgeTake: the session took a picture of the ring (facts), from the
  // slot `slot` (-1: none). It counts the reasons of the picture and writes a
  // line for one that is older than the picture before. AgeTick: each frame of
  // the session, the line of the window each 10 s. With the key off they do
  // nothing.
  void SetAgeLog(bool on);
  bool AgeLog() const { return age_log_; }
  void AgeTake(const take_age::TakeFacts& facts, int slot);
  void AgeTick(double now_ms);

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
  bool HasScreen() const { return Made(screen_); }

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
  // try until the next Play (NewPlay clears the flag with xr_resize_free=1),
  // and the pictures show on the stereo screen.
  bool MakeEyes(uint32_t width, uint32_t height);
  bool HasEyes() const { return Made(eyes_); }
  // The values of the projection layer of the newest picture (after
  // SetScreenSource or MarkScreenChanged). has: false when that picture has
  // none, and then it shows on the stereo screen.
  // view.frame is the frame of the mod of the picture also when has is false.
  void SetScreenView(bool has, const scene_stream::ViewBody& view);
  // THE MOTION OF THE STICK (motion_comp.h): the place of the player of the
  // newest picture (after SetScreenView), and the keys and the newest place
  // of the mod for each frame of the session.
  void SetScreenWorld(bool has, const input_channel::World& world) {
    motion_.SetNewest(has, world);
  }
  MotionComp& Motion() { return motion_; }
  // The display time of this frame of the session (ns, the clock of the
  // runtime).
  int64_t DisplayTime() const { return frame_.predictedDisplayTime; }
  int64_t DisplayPeriod() const { return frame_.predictedDisplayPeriod; }
  // THE MIDDLE AS ITS OWN LAYER (fovea_res_layer=1): the rects of the outer
  // and the centre image in the newest picture (VkSceneEyesAtlas), after
  // SetScreenView. With Atlas::on the layer of the whole view shows the
  // outer rect, and a second projection layer shows the centre rect over it
  // (fovea_res_math::CenterFov).
  void SetScreenAtlas(const fovea_res_math::Atlas& atlas) { newest_atlas_ = atlas; }
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
  // panel_part: the part of the panel image that the picture has (an empty rect:
  // the whole image). A quad whose rect is not in the part of the last copy does
  // not show.
  void SetScreenLayers(const quad_layer_math::FrameLayers& layers,
                       const quad_layer_math::AppPictures& pictures, VkImage panel,
                       uint32_t panel_width, uint32_t panel_height,
                       const eye_copy_math::Rect& panel_part);

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
  // THE FRAME LOOP: the same screen picture in the image of another slot of
  // the ring. The quad copies from the shown slot only, which the play thread
  // does not write. The picture does not change, so the quad gets no copy.
  void MoveScreenQuadImage(VkImage image) {
    if (flat_on_ && !flat_cpu_ && image != VK_NULL_HANDLE) {
      flat_source_ = image;
      flat_frozen_ = false;
    }
  }
  // THE FRAME LOOP: the session leaves the shown slot (a new slot, or the end
  // of the game). No layer copies again from an image of that slot: the
  // play thread can write it, or it can go. A layer keeps its image until a
  // new source (SetScreenSource, MarkScreenChanged, the panel of
  // SetScreenLayers, SetScreenQuadImage, MoveScreenQuadImage).
  void FreezeSlotSources() { eyes_frozen_ = panel_frozen_ = flat_frozen_ = true; }
  // The pictures of the world come again, or the game ended.
  void ClearScreenQuad() { flat_on_ = false; }
  bool ScreenQuadShown() const { return flat_on_; }
  // The kScreen of the newest screen picture.
  const scene_stream::ScreenBody& ScreenQuadBody() const { return flat_body_; }
  // THE MENU PANEL QUAD THAT ASKS FOR THE KEYBOARD: true when the last frame
  // showed the menu panel quad of the mod with kPanelQuadText (a dialog box of
  // the game in a frame of the scene takes text), with its pose and its size.
  bool PanelTextQuad(float position[3], float orientation[4], float size[2]) const;

  // THE FRAME LOOP (frame_loop, frame_loop.h, docs/android.md, "The frame
  // loop"). On (from Play to the end of a game with frame_loop=1): the
  // copies of a frame go into one command buffer of the ring of command
  // buffers (gpu.h), with no wait on the CPU for the GPU. The pixels of the
  // CPU go into a copy in memory, and into the staging buffer only when the
  // GPU finished the last copy of that buffer. The sources of the pictures
  // (SetScreenSource, the panel image, SetScreenQuadImage) are the images
  // of a slot of the ring of the pictures: in the layout of a transfer
  // source, with the bytes in the order of the swapchains. Off: the old
  // loop. Each change waits until the device is idle.
  void SetFrameLoop(bool on);
  bool FrameLoop() const { return frame_loop_; }
  // frame_loop_image_wait (0 by default): with the frame loop a swapchain
  // image that the runtime does not give free at once (xrWaitSwapchainImage
  // with a timeout of 0) stays acquired, and the next frame tries it again.
  // The layer shows its image before until then. 1: the session waits for
  // the image, as the old loop does.
  void SetImageWait(bool wait) { image_wait_ = wait; }
  // The swapchains have blue first (the slots of the ring take that order).
  bool SwapRedBlue() const;
  // xr_panel_rect (0 by default, at each Play before the play thread starts, and
  // the debug page): 1, the copy of the panel image into a slot takes only the
  // part that the panel quads show (PanelPart of picture_ring.h). Only the play
  // thread reads it.
  void SetPanelRect(bool on) { panel_rect_ = on; }
  bool PanelRect() const { return panel_rect_; }
  // THE LATE COMPOSE (xr_compose_direct=1, docs/android.md, "The compose into
  // the swapchains"): the format of the swapchains is red first with 8 bits,
  // UNORM or sRGB, so the compose pass writes the bytes of the game into them.
  uint32_t SwapFormat() const { return static_cast<uint32_t>(format_); }
  bool ComposeFormatOk() const {
    return format_ == VK_FORMAT_R8G8B8A8_SRGB || format_ == VK_FORMAT_R8G8B8A8_UNORM;
  }
  // THE COPIES OF THE EYE PICTURES (eye_copy_math.h, docs/android.md, "The
  // copies of the eye pictures").
  // xr_eye_rects (1 by default, read with the keys of the copies at the
  // first Play with the frame loop): a copy of an eye picture with an atlas
  // (fovea_res_layer=1) takes only the part that the layers show. 0: the
  // whole picture.
  bool EyeRects() const { return eye_rects_; }
  // xr_eye_direct (0 by default, before the play thread starts): 1, the play
  // thread copies a picture of the projection layer straight into two
  // swapchains of its slot of the ring, and the session copies nothing for
  // the eyes. 0: the copy into the slot and the copy of the session.
  void SetEyeDirect(bool on) { eye_direct_ = on; }
  bool EyeDirect() const { return eye_direct_; }
  // THE PLAY THREAD (xr_eye_direct=1). DirectBegin: the two swapchains of
  // `slot` (made at the first call for the slot, of the size of
  // eye_copy_math::DirectSize), and an acquired and waited image of each,
  // into targets. False when the picture does not go straight into them (no
  // key, blue first, another size, no swapchain): then it takes the old
  // way. DirectEnd: after the submit of the copies, the release of the
  // images, before the slot is ready.
  // A slot with images that no frame took (xr_release_take=1) keeps them for the
  // new picture, so DirectBegin acquires nothing then.
  bool DirectBegin(int slot, uint32_t width, uint32_t height,
                   const fovea_res_math::Atlas& atlas, VkImage targets[2]);
  void DirectEnd(int slot);
  // THE SESSION: the newest picture is in the two swapchains of `slot`, in
  // place of SetScreenSource. The values of SetScreenView and SetScreenAtlas
  // go with it.
  void SetDirectSource(int slot);
  // A NEW RENDER SIZE IN THE SAME PROCESS (xr_resize_free, 1 by default,
  // docs/android.md, "A new render size in the same process"). SetResizeFree:
  // at each Play, before the play thread starts. 1: NewPlay frees the sets
  // that have another size than the render of the game, and clears the failed
  // flags, so the next picture makes them at the new size. A slot of
  // xr_eye_direct=1 whose swapchains do not fit a picture frees them and
  // makes two at the size of the picture (DirectBegin). 0: the sets stay as
  // they are. NewPlay is a call of the session thread while the play thread does not run, and
  // it logs the count of the swapchains that the app holds.
  void SetResizeFree(bool on) { resize_free_ = on; }
  void NewPlay(uint32_t width, uint32_t height);
  // The swapchains that the app holds now, any thread.
  int Swapchains() const { return swapchains_.load(std::memory_order_relaxed); }
  // THE LOCK OF THE QUEUE that the renderer, the play thread and the session
  // share. The session holds it for each submit and for each call of
  // OpenXR that can use the queue (xrBeginFrame, xrEndFrame,
  // xrAcquireSwapchainImage, xrReleaseSwapchainImage, XR_KHR_vulkan_enable2).
  static void QueueLock();
  static void QueueUnlock();
  // THE TIME LOG of a thread: TimeQueueWaits(true) on a thread measures its
  // waits for the lock, and TakeQueueWaitMs gives their sum since its last
  // call on that thread.
  static void TimeQueueWaits(bool on);
  static double TakeQueueWaitMs();
  // The Gpu of the session, for the ring of the pictures (picture_ring.h).
  const Gpu& SessionGpu() const { return gpu_; }
  // THE TWO QUEUES (frame_loop_queues, before Start, 2 by default). 2: when
  // the queue family has two queues or more, the device gets two, the
  // session the first (with the higher priority) and the renderer of the
  // mod with the play thread the second, so a copy of the session does not
  // wait on the GPU behind a walk. 1: one queue for all.
  void SetQueues(int count) { queues_wanted_ = count; }
  bool TwoQueues() const { return queue2_ != VK_NULL_HANDLE; }
  // THE ONE PASS (xr_density_pass, docs/android.md, "The one pass with a density
  // map"). SetDensityWanted, before Start: the device gets the extension
  // VK_EXT_fragment_density_map when the GPU has it with the map and the images
  // with no subsamples. DensityMap: the device has it. SetDensityPass, at each
  // Play before the play thread starts: 1, the play thread gives the renderer a
  // swapchain image of its slot (DensityBegin) and the walks of both eyes draw
  // into it with a density map.
  void SetDensityWanted(bool on) { density_wanted_ = on; }
  bool DensityMap() const { return density_map_; }
  void SetDensityPass(bool on) { density_pass_ = on; }
  bool DensityPass() const { return density_pass_ && density_map_; }
  // THE PLAY THREAD (xr_density_pass=1). DensityBegin: the swapchain of `slot`
  // with a layer for each eye (made at the first call for the slot, of width by
  // height), and an acquired and waited image of it, into *target. False when
  // the slot cannot have it (no key, no density map, blue first, no swapchain
  // of two layers): then the pictures of the slot take the old way. DirectEnd
  // releases the image. The probe of the runtime runs at the first call.
  bool DensityBegin(int slot, uint32_t width, uint32_t height, VkImage* target);
  // The waits of DensityBegin for a swapchain image (xrWaitSwapchainImage) since
  // the last call: the count, the sum and the most in ms.
  static void TakeSwapchainWaits(uint64_t* count, double* sum_ms, double* most_ms);
  // The count of the pictures of the one pass that went into the projection layer
  // with no copy (direct), and of the ones that took a copy from the swapchain
  // image. The log at the end of the app has both.
  void CountDensity(bool direct);
  // Before RendererDevice: the renderer takes the second queue (with the
  // frame loop, when the device has it). It keeps it to the end of the
  // process.
  void SetRendererSecondQueue(bool second) { renderer_second_ = second && TwoQueues(); }
  bool RendererOnSecondQueue() const { return renderer_second_; }
  VkQueue RendererQueue() const { return renderer_second_ ? queue2_ : gpu_.queue; }
  // The serial of the last submit of the session, and its timeline (or
  // none): a submit on the second queue waits on the GPU for it.
  uint64_t SessionSerial() const { return gpu_.serial; }
  VkSemaphore SessionTimeline() const { return renderer_second_ ? gpu_.timeline : VK_NULL_HANDLE; }
  // THE OLD LOOP with the renderer on the second queue: before the play of a
  // batch the CPU waits for the last copy of the session, and after it for
  // the walks (vkQueueWaitIdle of the second queue). Nothing with one queue.
  void OldLoopSync(bool before);

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
    // THE FRAME LOOP: the pixels for the staging buffer (dirty: newer than
    // the staging buffer), and the serial of the last submit that read the
    // staging buffer.
    std::vector<uint8_t> mirror;
    bool dirty = false;
    uint64_t staging_serial = 0;
    // The rect of the pixels that the quad shows (a card, xr_layer_rect=1),
    // x, y, width and height. A width of 0 is the whole image.
    int32_t rect[4] = {0, 0, 0, 0};
    // The rect of the image that the app released last (frame_loop::RectShows).
    int32_t released_rect[4] = {0, 0, 0, 0};
    // An image that the app acquired and the runtime did not give free yet
    // (frame_loop_image_wait=0), or -1.
    int pending = -1;
    // THE ONE PASS: a swapchain with two layers. Layer 0 of the set owns it
    // (array_index 0), and layer 1 names the same swapchain (array_index 1,
    // shared: it is never destroyed or released as a swapchain of its own).
    uint32_t array_index = 0;
    bool shared = false;
  };
  // The next image of the swapchain of a layer, free for a copy, into
  // *index. block: wait for it. With no block a busy image stays acquired
  // (Layer::pending), and the result is false.
  bool NextImage(Layer* layer, bool block, uint32_t* index);
  // True when the layer needs a new image in this frame. Else it counts a
  // skipped copy.
  bool LayerChanged(Layer* layer);

  bool MakeInstance(ANativeActivity* activity);
  bool MakeVulkan();
  bool MakeSession();
  bool MakeActions();
  bool MakeLayer(Layer* layer, uint32_t width, uint32_t height,
                 bool staging = true, bool stereo = false);
  void CopyLayer(Layer* layer);
  // The pixels of a layer: straight into its staging buffer when it is free
  // (xr_staging_direct=1), else into its copy in memory. Null when the layer
  // has no staging buffer.
  uint8_t* PixelsFor(Layer* layer, size_t bytes);
  // THE PARTS OF THE LAYERS: the part that the time goes to from now on. The
  // result is the part before. Nothing with time_log=0.
  int PartTo(int part);
  // THE FRAME LOOP: the command buffer of this frame (begun at its first
  // use), the swapchain images to release after its submit, and the submit
  // with the releases before xrEndFrame.
  VkCommandBuffer FrameCommands();
  void SubmitFrame();
  // The layers with a staging buffer, for a change of the frame loop.
  std::vector<Layer*> StagingLayers();
  bool frame_loop_ = false;
  // xr_copy_once (1 by default, read at the start of the frame loop from
  // bwvr.ini of the game folder): CopySlot records each copy of the same
  // size one time and then runs it with vkCmdExecuteCommands. 0: CopySlot
  // records the barriers and the copy in each frame.
  bool copy_once_ = true;
  bool copy_keys_read_ = false;
  uint64_t once_made_ = 0;   // secondary command buffers recorded
  uint64_t once_run_ = 0;    // copies that ran one of them
  uint64_t once_frees_ = 0;  // frees of the entries of a set
  uint64_t once_full_ = 0;   // copies with no free entry, recorded in the frame
  bool image_wait_ = false;
  // THE COPIES OF THE EYE PICTURES. eye_part_: the part of the newest
  // picture that a copy into the swapchains of the eyes takes. The sets of
  // the slots of xr_eye_direct=1: each written by the play thread alone, and
  // read by the session after the slot is ready. The swapchain handles of a
  // set stay until a new Play frees the set (NewPlay), the slot replaces it
  // (DirectBegin), or Stop.
  bool eye_rects_ = true;
  eye_copy_math::Rect eye_part_ = {};
  bool eye_direct_ = false;
  bool panel_rect_ = false;
  eye_copy_math::Rect panel_part_ = {};       // the part of the panel image of the last copy
  eye_copy_math::Rect panel_part_copy_ = {};  // the part of the copy that CopySlot makes now
  int direct_newest_ = -1;       // the slot of the newest picture, or -1
  uint64_t direct_revision_ = 0;  // one more for each new picture in a slot
  uint64_t direct_latched_[2] = {0, 0};  // the revision that each eye shows
  int shown_direct_[2] = {-1, -1};  // the slot that each eye shows, or -1 (eyes_)
  uint64_t direct_frames_ = 0;      // frames of the session with a picture of a slot
  uint64_t direct_stale_ = 0;       // frames with no projection layer: see EndFrame
  bool direct_sizes_said_ = false;  // LogViewSizes at the first picture of a slot
  // The layer of the swapchain that eye `eye` shows.
  const Layer& EyeLayer(int eye) const;
  bool eyes_frozen_ = false;   // screen_source_ is in a slot that the session left
  bool panel_frozen_ = false;  // panel_source_ too
  bool flat_frozen_ = false;   // flat_source_ too
  int queues_wanted_ = 2;
  VkQueue queue2_ = VK_NULL_HANDLE;
  bool renderer_second_ = false;
  VkCommandBuffer frame_cmd_ = VK_NULL_HANDLE;
  std::vector<XrSwapchain> to_release_;
  uint64_t staging_skips_ = 0;  // copies that waited a frame for a staging buffer
  // THE EYE PICTURES OF THE RENDERER in two swapchains: the stereo screen (two
  // quads), or the projection layer.
  struct Screen {
    Layer layer[2];
    VkImage between = VK_NULL_HANDLE;  // the picture at the size of the swapchains
    VkDeviceMemory between_memory = VK_NULL_HANDLE;
    uint64_t copies[2] = {0, 0};  // the copies of each eye
    bool failed = false;  // the set could not be made, and the app does not try again
    bool between_said = false;  // the log said that the image between cannot be made
    // THE COPIES RECORDED ONE TIME (xr_copy_once=1): for each source image,
    // layer of the source and swapchain image, a secondary command buffer
    // with the copy of the same size (CopySlot). The entries are for sources
    // of one size (once_width by once_height).
    struct Once {
      VkImage source;
      uint32_t layer;
      VkImage target;
      bool second;  // with the barrier of the second queue
      eye_copy_math::Rect part;  // the rect of the copy (xr_eye_rects)
      VkCommandBuffer cmd;
    };
    std::vector<Once> once;
    uint32_t once_width = 0, once_height = 0;
    uint64_t once_changes = 0;  // PictureRingImageChanges at the record of the entries
  };
  // The swapchains of the set are made.
  static bool Made(const Screen& screen) { return screen.layer[0].swapchain != XR_NULL_HANDLE; }
  // THE TWO SWAPCHAINS OF A SLOT (xr_eye_direct=1). acquired, waited: the
  // image of each eye between DirectBegin and DirectEnd.
  // made_width, made_height: the size of the pictures that the set was made
  // for (NewPlay). replaced: the replacements of the set since the Play
  // started (swapchain_free_math::kMostReplaces).
  struct DirectSet {
    Screen set;
    bool failed = false;
    // THE ONE PASS: the set is one swapchain of two layers, and the slot could
    // not make or use one (the pictures of the slot take the old way).
    bool stereo = false;
    bool density_failed = false;
    bool acquired[2] = {false, false};
    bool waited[2] = {false, false};
    uint32_t index[2] = {0, 0};  // the image of each eye while it is acquired
    uint32_t made_width = 0;
    uint32_t made_height = 0;
    int replaced = 0;
  };
  DirectSet direct_[frame_loop::kSlots];
  // THE AGE LOG. The session and the play thread both write it, under the lock.
  bool age_log_ = false;
  std::mutex age_lock_;
  take_age::Shown age_shown_;
  take_age::Layer age_layer_;
  take_age::Writes age_writes_;
  take_age::Window age_window_;
  uint64_t age_frames_ = 0;        // frames of the session since the Play
  double age_window_ms_ = 0.0;     // the start of the window
  int age_take_lines_ = 0;   // the lines written for a take, and for a layer
  int age_layer_lines_ = 0;
  uint64_t age_kept_ = 0, age_whole_ = 0;  // VkSceneCounts at the start of the window
  // A frame of the session with a projection layer of the slot `slot` (-1: the
  // old way) and the frames of its two eyes.
  void AgeLayer(int slot, uint64_t left, uint64_t right);
  // The play thread starts a write into the swapchains of `slot`.
  void AgeWrite(int slot);
  // The play thread counts its pictures straight into the swapchains, and
  // the pictures that took the old way with the key on.
  std::atomic<uint64_t> direct_writes_{0};
  std::atomic<uint64_t> direct_misses_{0};
  // xr_resize_free, and the swapchains that the runtime made and the app did
  // not destroy yet (MakeLayer, DestroySwapchain).
  bool resize_free_ = true;
  std::atomic<int> swapchains_{0};
  void DestroySwapchain(XrSwapchain swapchain);
  // The set goes with all its parts, and it is a new set (no failed flag, no
  // released image). The caller holds the lock of the queue and has waited
  // for the idle device.
  void DropSet(Screen* screen);
  // False with the reason in *why, and then the set has no part. count: the
  // swapchains of the set (2 for the eyes, 1 for the panel). With the frame
  // loop the set has no image between: a copy of the same size needs none,
  // and MakeBetween makes it at the first copy of another size.
  bool MakeScreenSet(Screen* screen, uint32_t width, uint32_t height,
                     const char** why, int count = 2);
  bool MakeBetween(Screen* screen, const char** why);
  // A set of the one pass: one swapchain with a layer for each eye, in two
  // Layers of the set (see Layer::shared).
  bool MakeStereoSet(Screen* screen, uint32_t width, uint32_t height, const char** why);
  // xr_copy_once: the keys, the secondary command buffer of a copy (null:
  // CopySlot records it), and the free of the entries of a set.
  void ReadCopyKeys();
  VkCommandBuffer OnceCopy(Screen* screen, VkImage source, uint32_t source_layer,
                           uint32_t width, uint32_t height, VkImage target,
                           const eye_copy_math::Rect& part);
  void FreeOnce(Screen* screen);
  // THE FRAME LOOP: a copy from an image of a slot of the ring (the layout
  // of a transfer source, the bytes in the order of the swapchain) into the
  // command buffer of the frame.
  bool CopySlot(Screen* screen, int index, VkImage source, uint32_t source_layer,
                uint32_t source_width, uint32_t source_height);
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
  int eye_size_max_ = 1;
  std::string runtime_name_;
  // THE TIME LOG.
  bool time_log_ = false;
  FrameTimes times_;
  double step_ms_[kSteps] = {};
  bool parts_on_ = false;  // in EndFrame with time_log=1
  int part_ = kPartRest;
  double part_at_ = 0.0;
  double part_cpu_at_ = 0.0;
  double part_ms_[kParts] = {};
  double part_cpu_ms_[kParts] = {};
  bool layer_rect_ = false;
  bool staging_direct_ = true;
  uint64_t not_ready_ = 0;
  // XR_KHR_convert_timespec_time: the display time on CLOCK_MONOTONIC.
  PFN_xrConvertTimeToTimespecTimeKHR to_timespec_ = nullptr;
  bool timespec_ext_ = false;
  double wait_end_ms_ = -1.0;  // the clock at the end of xrWaitFrame of this frame
  double last_wait_ms_ = 0.0;

  // THE DISPLAY RATE.
  void ReadDisplayRates();
  bool display_rate_ext_ = false;  // the instance has XR_FB_display_refresh_rate
  std::vector<float> display_rates_;
  PFN_xrEnumerateDisplayRefreshRatesFB enumerate_rates_ = nullptr;
  PFN_xrGetDisplayRefreshRateFB get_rate_ = nullptr;
  PFN_xrRequestDisplayRefreshRateFB request_rate_ = nullptr;

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
  // The gaze (SetEyeGaze).
  bool gaze_wanted_ = false;
  bool gaze_extension_ = false;  // the instance has the extension
  bool gaze_supported_ = false;  // and the headset has an eye tracker
  XrAction gaze_ = XR_NULL_HANDLE;
  XrSpace gaze_space_ = XR_NULL_HANDLE;
  XrSpace view_ = XR_NULL_HANDLE;  // the VIEW space: the space of the head
  uint64_t gaze_frames_ = 0;  // the slots with a gaze, for the log
  uint64_t gaze_slots_ = 0;

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
  // THE ONE PASS: the key at the start (the device), the key of this Play, the
  // device has the extension, the texel of the map in pixels (the finest, then
  // the coarsest), and the probe of the runtime.
  bool density_wanted_ = false;
  bool density_pass_ = false;
  bool density_map_ = false;
  uint32_t density_texel_[4] = {0, 0, 0, 0};
  bool density_probed_ = false;
  // The extensions of the foveation of the runtime that the instance has on (the
  // order of kFoveation in MakeInstance), and the headset has the eye tracked
  // foveation.
  bool foveation_on_[5] = {};
  bool runtime_fov_tracked_ = false;
  std::atomic<uint64_t> density_direct_{0}, density_copied_{0};
  void DensityProbe(uint32_t width, uint32_t height);
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
  // The atlas of the newest picture (in pixels of the picture) and of the
  // picture in the released image of each eye (in pixels of the swapchain,
  // fovea_res_math::FitAtlas) (fovea_res_layer=1).
  fovea_res_math::Atlas newest_atlas_ = {};
  fovea_res_math::Atlas shown_atlas_[2] = {};
  uint64_t center_frames_ = 0;  // frames of the session with the centre layer
  bool said_center_ = false;
  bool said_sharp_ = false;     // the line of the sharpness of the middle
  void LogSharpness(const scene_stream::ViewBody& view, const fovea_res_math::Atlas& atlas);
  void LogViewSizes(const char* when);
  uint32_t asked_width_ = 0;    // the size that the runtime asks for, for LogViewSizes
  uint32_t asked_height_ = 0;
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
  // The motion of the stick, and the parts of the age of the new picture of
  // this frame (age_pending_) for the time log.
  MotionComp motion_;
  PictureAge age_;
  frame_loop::AgeTimes age_times_;
  bool age_pending_ = false;
};
