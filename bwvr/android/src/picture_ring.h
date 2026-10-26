// THE RING OF THE PICTURES OF THE GAME (frame_loop=1, docs/android.md, "The
// frame loop"). The play thread plays a batch of frames of the live stream
// into the renderer and copies the picture of the renderer into a slot of
// the ring: the eye pictures, the panel image and the screen picture, each
// in an image of the slot, with the values of the frame (the poses of the
// views, the quad layers, the frame number). It submits the copies with the
// fence of the slot. The thread of the session takes the newest slot whose
// fence is signaled (vkGetFenceStatus, no wait) and shows it. The rules of
// the slots are in frame_loop.h (Ring).
//
// The renderer draws each walk into the next of two targets, so a picture
// stays in its target only until the walk after the next one. The copy into
// the slot keeps it for the session as long as the session needs it.
#pragma once

#include <stdint.h>

#include <mutex>
#include <vector>

#include <vulkan/vulkan.h>

#include "../../src/mod/quad_layer_math.h"
#include "../../src/mod/scene_stream.h"
#include "../../src/mod/screen_quad_math.h"
#include "../../src/mod/time_log_math.h"
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
  const quad_layer_math::FrameLayers* layers = nullptr;
  const quad_layer_math::AppPictures* cards = nullptr;
  VkImage panel = VK_NULL_HANDLE;  // the panel image of the picture, or none
  // The images of a slot: in the layout of a transfer source, with the bytes
  // in the order of the swapchains. Else the images of the renderer.
  bool from_ring = false;
  // THE STREAM: the newest frame of the batch, and the pictures of the play
  // since frames came.
  uint64_t last_frame = 0;
  uint64_t pictures = 0;
  double play_start_ms = 0.0;  // CLOCK_MONOTONIC at the start of the play of its batch
  double arrival_ms = 0.0;     // and at the arrival of the newest frame of the batch
};

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
  // at the first use and at a new size.
  bool CopyEyes(int i, VkImage source, uint32_t width, uint32_t height);
  bool CopyPanel(int i, VkImage source, uint32_t width, uint32_t height);
  bool CopyScreen(int i, VkImage source, uint32_t layer, uint32_t width, uint32_t height);
  // The submit of the copies with the fence of the slot. The slot is ready.
  void EndWrite(int i);
  // THE QUEUE AT THE GPU (frame_loop_gpu_depth): true when fewer than `depth`
  // slots have copies that the GPU did not finish. The fence of a slot comes
  // after the walks of its picture on the queue, so a finished slot has a
  // finished picture. Else it waits up to timeout_ns for one of them, and
  // gives the result after the wait. *waited: true when the call found
  // `depth` unfinished slots or more (it waited). The play thread alone
  // calls it.
  bool WaitGpu(int depth, uint64_t timeout_ns, bool* waited);

  // THE SESSION, once in each frame of the session: the newest picture that
  // the GPU finished, or null when no new picture is finished. The picture
  // stays valid until the next Take or Clear.
  // session_serial: the serial of the last submit of the session, which
  // has each copy from the slot shown before.
  const Picture* Take(uint64_t session_serial);
  // The game ended: no slot shows.
  void Clear(uint64_t session_serial);
  void TakeStats(Stats* out);

 private:
  bool MakeImage(Image* image, uint32_t width, uint32_t height, uint32_t layers);
  void FreeImage(Image* image);
  // A copy of `layers` layers from `first` of a source of the renderer into
  // an image of the slot.
  void Copy(int i, VkImage source, uint32_t first, uint32_t layers, const Image& to,
            uint32_t width, uint32_t height);

  bool started_ = false;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkSemaphore session_timeline_ = VK_NULL_HANDLE;
  VkPhysicalDeviceMemoryProperties memory_ = {};
  VkCommandPool pool_ = VK_NULL_HANDLE;
  VkQueryPool query_ = VK_NULL_HANDLE;
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
};
