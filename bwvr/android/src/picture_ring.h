// THE RING OF THE PICTURES OF THE GAME (frame_loop=1, docs/android.md, "The
// frame loop"). The play thread plays a batch of frames of the live stream
// into the renderer and copies the picture of the renderer into a slot of
// the ring: the eye pictures, the panel image and the screen picture, each
// in an image of the slot, with the values of the frame (the poses of the
// views, the quad layers, the frame number). It submits the copies with the
// fence of the slot. The thread of the session takes the newest slot whose
// fence is signaled (vkGetFenceStatus, no wait), or with frame_queue=1 the
// next finished slot in the order of the pictures, and shows it. The rules
// of the slots are in frame_loop.h (Ring).
//
// The renderer draws each walk into the next of two targets, so a picture
// stays in its target only until the walk after the next one. The copy into
// the slot keeps it for the session as long as the session needs it.
#pragma once

#include <stdint.h>

#include <functional>
#include <mutex>
#include <vector>

#include <vulkan/vulkan.h>

#include "../../src/common/input_channel.h"
#include "../../src/mod/eye_copy_math.h"
#include "../../src/mod/fovea_res_math.h"
#include "../../src/mod/quad_layer_math.h"
#include "../../src/mod/scene_stream.h"
#include "../../src/mod/screen_quad_math.h"
#include "../../src/mod/time_log_math.h"
#include "copy_time.h"
#include "frame_loop.h"
#include "gpu.h"

// The images that the rings of the pictures made or freed since the start,
// any thread (frame_loop::OnceSetValid).
uint64_t PictureRingImageChanges();

// A PICTURE OF THE GAME as the session shows it (main.cpp, ShowPicture).
// In the old loop it points at the renderer and at the player of the stream.
// In a slot of the ring it points at the copies of the slot.
struct Picture {
  screen_quad_math::Last last = screen_quad_math::Last::kNothing;
  // THE SCREEN QUAD: the newest screen picture of the stream.
  uint64_t screen_pictures = 0;
  scene_stream::ScreenBody screen_body = {};
  bool screen_ok = false;  // the picture of the screen is there
  bool screen_cpu = false;  // the pixels of the CPU, else an image
  const std::vector<uint8_t>* screen_rgba = nullptr;
  VkImage screen_image = VK_NULL_HANDLE;
  uint32_t screen_layer = 0;
  uint32_t screen_width = 0;
  uint32_t screen_height = 0;
  // THE WORLD: the eye pictures (a layer for each eye), the values of the
  // projection layer and the quad layers of their frame.
  bool eyes_ok = false;
  VkImage eyes = VK_NULL_HANDLE;
  uint32_t width = 0;
  uint32_t height = 0;
  bool have_view = false;
  scene_stream::ViewBody view = {};  // view.frame also when have_view is false
  // The place of the player in the world of the frame (motion_comp_math.h).
  bool have_world = false;
  input_channel::World world = {};
  // The rects of the outer and the centre image in the eye picture
  // (fovea_res_layer=1, VkSceneEyesAtlas).
  fovea_res_math::Atlas atlas = {};
  const quad_layer_math::FrameLayers* layers = nullptr;
  const quad_layer_math::AppPictures* cards = nullptr;
  VkImage panel = VK_NULL_HANDLE;  // the panel image of the picture, or none
  // The part of the panel image that its copy into the slot took (PanelPart). The
  // session copies the same part. Empty: the whole image.
  eye_copy_math::Rect panel_part = {};
  // The images of a slot: in the layout of a transfer source, with the bytes
  // in the order of the swapchains. Else the images of the renderer.
  bool from_ring = false;
  // THE EYES STRAIGHT INTO THE SWAPCHAINS (xr_eye_direct=1): the slot whose
  // two swapchains have the eye pictures (XrApp::DirectBegin), else -1. Then
  // `eyes` is VK_NULL_HANDLE, and the session copies nothing for the eyes.
  int direct_slot = -1;
  // THE STREAM: the newest frame of the batch, and the pictures of the play
  // since frames came.
  uint64_t last_frame = 0;
  uint64_t pictures = 0;
  double play_start_ms = 0.0;  // CLOCK_MONOTONIC at the start of the play of its batch
  double arrival_ms = 0.0;     // and at the arrival of the newest frame of the batch
  double ready_ms = 0.0;       // and at the submit of its copies into a slot of the ring
  double gap_ms = 0.0;         // the newest frame of the batch: its time from the frame before (slow_frame_ms)
};

// THE PART OF THE PANEL IMAGE THAT A COPY TAKES (xr_panel_rect): with `on`, the
// smallest rect that holds the rect of each panel quad of the frame
// (quad_layer_math::PanelUnion), else the whole image. The play thread (the
// copy into the slot) and the session (the copy into the swapchain) both
// call it with the layers of the same picture, so they agree.
inline eye_copy_math::Rect PanelPart(bool on, const quad_layer_math::FrameLayers& layers,
                                     uint32_t width, uint32_t height) {
  int32_t r[4];
  if (on && layers.panel && layers.panel_body.width == width &&
      layers.panel_body.height == height && quad_layer_math::PanelUnion(layers.panel_body, r)) {
    return {r[0], r[1], r[2], r[3]};
  }
  return eye_copy_math::Whole(width, height);
}

class PictureRing {
 public:
  // An image of a slot, in the byte order of the swapchains.
  struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t layers = 0;
  };
  struct Slot {
    Picture picture;
    // The copies that the picture points at.
    quad_layer_math::FrameLayers layers;
    quad_layer_math::AppPictures cards;
    std::vector<uint8_t> screen_rgba;
    // The screen picture that the slot holds (its count in the stream, 0:
    // none), so a slot copies a screen picture only when it is new for it.
    uint64_t screen_held = 0;
    scene_stream::ScreenBody screen_body = {};
    bool screen_ok = false;
    bool screen_cpu = false;
    uint32_t screen_width = 0;
    uint32_t screen_height = 0;
    Image eyes;
    Image panel;
    Image screen;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    // The serial of the last submit of the session that can read the slot
    // (the second queue waits on the GPU for it before a new write).
    uint64_t read_serial = 0;
    bool submitted = false;
    bool query_written = false;
  };
  // The stats of the play thread for the time log, since the last
  // TakeStats.
  struct Stats {
    uint64_t dropped = 0;   // pictures that the play thread dropped (no free slot)
    uint64_t replaced = 0;  // pictures that a newer picture replaced before a frame of the session
    time_log_math::Stat slot_wait;  // waits of the play thread for the fence of a slot
    time_log_math::Stat busy;       // the GPU time of the copies into the slots
  };

  // After the start of the session: the device of the session (gpu), the
  // queue of the renderer (the queue of the session, or the second queue of
  // frame_loop_queues=2), the timeline of the session when the renderer has
  // the second queue (else VK_NULL_HANDLE), and the byte order of the
  // swapchains. False with a line of the log when it cannot make its
  // command buffers.
  bool Start(const Gpu& gpu, VkQueue queue, VkSemaphore session_timeline, bool swap_red_blue);
  bool Started() const { return started_; }
  // At the end of the app, after the play thread ended.
  void Stop();

  // THE PLAY THREAD. BeginWrite gives the slot of the next picture with its
  // command buffer open, and the ms that it waited (*waited). It waits for
  // the fence of the slot only when the slot holds a dropped picture whose
  // copies the GPU did not finish.
  int BeginWrite(double* waited);
  Slot& At(int i) { return slots_[i]; }
  // The copies from images of the renderer, in the layout of a color
  // attachment, into the images of slot i. They make an image of the slot
  // at the first use and at a new size. part: the rect of the eye pictures
  // that the copy takes (xr_eye_rects, eye_copy_math::CopyPart).
  bool CopyEyes(int i, VkImage source, uint32_t width, uint32_t height,
                const eye_copy_math::Rect& part);
  // THE EYES STRAIGHT INTO THE SWAPCHAINS (xr_eye_direct=1): the rect `part`
  // of each layer of the source into the swapchain image of its eye
  // (targets, acquired and waited by XrApp::DirectBegin), at the same place.
  // The swapchain images go to the layout of a color attachment.
  void CopyDirect(int i, VkImage source, const eye_copy_math::Rect& part,
                  const VkImage targets[2]);
  // THE LATE COMPOSE (xr_compose_direct=1, VkSceneComposeRecord): the pass of
  // the compose into the swapchain image of each eye (targets, as for
  // CopyDirect), of the format `format`. part: the rect that it writes, for the
  // time log. False when the renderer cannot record it, then nothing is written.
  bool ComposeDirect(int i, const VkImage targets[2], uint32_t format,
                     const eye_copy_math::Rect& part);
  // The compose into the image of the renderer, the layers of the eye picture
  // that a copy takes after it (CopyEyes, CopyDirect, CopyScreen).
  bool ComposeEyes(int i, const eye_copy_math::Rect& part);
  // The image of the eye pictures of slot i goes, when the slot has one (a
  // picture of the old way before). It waits for the idle device one time.
  void FreeEyes(int i);
  bool CopyPanel(int i, VkImage source, uint32_t width, uint32_t height,
                 const eye_copy_math::Rect& part);
  bool CopyScreen(int i, VkImage source, uint32_t layer, uint32_t width, uint32_t height);
  // The submit of the copies with the fence of the slot. Then after_submit
  // (the release of the swapchain images of xr_eye_direct=1), and then the
  // slot is ready, so no frame of the session shows it before the release.
  void EndWrite(int i, const std::function<void()>& after_submit = nullptr);
  // THE QUEUE AT THE GPU (frame_loop_gpu_depth): true when fewer than `depth`
  // slots have copies that the GPU did not finish. The fence of a slot comes
  // after the walks of its picture on the queue, so a finished slot has a
  // finished picture. Else it waits up to timeout_ns for one of them, and
  // gives the result after the wait. *waited: true when the call found
  // `depth` unfinished slots or more (it waited). The play thread alone
  // calls it.
  bool WaitGpu(int depth, uint64_t timeout_ns, bool* waited);

  // THE QUEUE OF THE PICTURES (frame_queue, frame_loop.h): at each Play,
  // before the start of the play thread. The ring then uses 4 slots (on) or
  // 3 (off, the old way).
  void SetQueue(bool on);

  // The facts of one take for the line "late picture" (long_frames.h).
  struct TakeFacts {
    int delay = 0;          // the takes after the submit of the shown picture (Ring::ShownDelay)
    uint64_t replaced = 0;  // pictures that this take freed before they showed
    uint64_t dropped = 0;   // pictures that the play thread dropped since the take before
  };
  // THE SESSION, once in each frame of the session: the picture that the
  // ring picks (frame_loop::Ring::Take), or null when no new picture is
  // finished. The picture stays valid until the next Take or Clear.
  // session_serial: the serial of the last submit of the session, which
  // has each copy from the slot shown before. facts: when not null, the
  // facts of this take.
  const Picture* Take(uint64_t session_serial, TakeFacts* facts = nullptr);
  // The game ended: no slot shows.
  void Clear(uint64_t session_serial);
  void TakeStats(Stats* out);

 private:
  bool MakeImage(Image* image, uint32_t width, uint32_t height, uint32_t layers);
  void FreeImage(Image* image);
  // A copy of `layers` layers from `first` of a source of the renderer into
  // an image of the slot, of the rect `part` (the whole image with no part).
  void Copy(int i, VkImage source, uint32_t first, uint32_t layers, const Image& to,
            uint32_t width, uint32_t height, const eye_copy_math::Rect* part, int kind);

  bool started_ = false;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkSemaphore session_timeline_ = VK_NULL_HANDLE;
  VkPhysicalDeviceMemoryProperties memory_ = {};
  VkCommandPool pool_ = VK_NULL_HANDLE;
  VkQueryPool query_ = VK_NULL_HANDLE;
  copy_time::Stamps stamps_;  // the GPU time of each copy (time_log=1)
  double timestamp_ns_ = 0.0;
  uint64_t timestamp_mask_ = 0;
  VkFormat format_ = VK_FORMAT_R8G8B8A8_UNORM;
  bool swap_red_blue_ = false;
  Slot slots_[frame_loop::kSlots];

  // The rules of the slots, the stats and the fences of ready slots: the
  // session and the play thread take the lock for them, never for a wait.
  std::mutex lock_;
  frame_loop::Ring ring_;
  Stats stats_;
  uint64_t dropped_at_take_ = 0;  // ring_.Dropped() at the take before
};
