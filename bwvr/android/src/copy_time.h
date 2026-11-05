// THE GPU TIME OF EACH COPY OF A PICTURE (time_log=1, docs/android.md, "The
// time log of the app", line 3). A pair of timestamps goes around each copy
// of a picture in a command buffer of the play thread (the copies into a
// slot of the ring, or straight into the swapchains) and of the session (the
// copies from a slot into the swapchains). Each 10 s the time log gives the
// GPU time and the MB of each kind of copy, so a log of a Quest 3 and a log
// of another headset can be compared.
#pragma once

#include <stdint.h>

#include <vector>

#include <vulkan/vulkan.h>

#include "../../src/mod/time_log_math.h"

namespace copy_time {

enum Kind {
  kSlotEyes,    // the play thread: the eye pictures into a slot of the ring
  kSlotDirect,  // the play thread: the eye pictures straight into the swapchains
  kSlotCompose,  // the play thread: the eye pictures made from the two images straight in the swapchains
  kSlotComposeEyes,  // the play thread: the eye pictures made from the two images into the image of the renderer
  kSlotPanel,   // the play thread: the panel image into a slot
  kSlotScreen,  // the play thread: the screen picture into a slot
  kSwapEyes,    // the session: an eye picture into its swapchain
  kSwapCompose,  // the session: the eye pictures made from the two images at the take
  kSwapStereo,  // the session: an eye picture into the stereo screen
  kSwapPanel,   // the session: the panel image into its swapchain
  kSwapScreen,  // the session: the screen picture into the screen quad
  kKinds
};
const char* Name(int kind);

// The copies of a window of the time log: the GPU time of each copy of a
// kind, and the bytes that the copies of a kind wrote.
struct Window {
  time_log_math::Stat ms[kKinds];
  uint64_t bytes[kKinds] = {};
};
// Any thread.
void Add(int kind, double ms, uint64_t bytes);
// The copies since the last call, any thread.
void TakeWindow(Window* out);
// The GPU time of the last compose at the take in ms, for the records of the pictures.
float LastComposeMs();

// The timestamps of the copies of a set of command buffers, each with an
// index of its own (a slot of the ring, an entry of the ring of command
// buffers of the session). One thread uses one set.
class Stamps {
 public:
  static constexpr uint32_t kPairs = 6;  // the most copies in a command buffer
  // timestamp_ns: the ns of a tick. mask: the valid bits of a timestamp of
  // the queue. False when the pool cannot be made: then no call records.
  bool Start(VkDevice device, double timestamp_ns, uint64_t mask, uint32_t buffers);
  void Stop();
  bool On() const { return pool_ != VK_NULL_HANDLE; }
  // At the start of the record of command buffer `buffer`, after Take of its
  // last record.
  void Begin(VkCommandBuffer cmd, uint32_t buffer);
  // Around a copy of `kind` that writes `bytes` bytes. Each stamp is at the
  // bottom of the pipe, so the time between them is the time of the copy.
  void Before(VkCommandBuffer cmd, uint32_t buffer, int kind, uint64_t bytes);
  void After(VkCommandBuffer cmd, uint32_t buffer);
  // When the GPU finished the command buffer: its copies go into the window.
  void Take(uint32_t buffer);

 private:
  struct Buffer {
    uint32_t count = 0;
    bool open = false;
    int kind[kPairs] = {};
    uint64_t bytes[kPairs] = {};
  };
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueryPool pool_ = VK_NULL_HANDLE;
  double ns_ = 0.0;
  uint64_t mask_ = ~0ull;
  std::vector<Buffer> buffers_;
};

}  // namespace copy_time
