// THE VULKAN DEVICE OF THE HEADSET APP and the helpers for the copies of the
// pictures of the CPU (docs/android.md, "The picture").
//
// In the headset the OpenXR runtime makes the instance and the device
// (xr_app.cpp). On a flat screen the app makes them (flat_view.cpp). Both
// fill a Gpu, and the rest of the app uses only the Gpu.
#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

#include <vector>

#include "copy_time.h"
#include "frame_loop.h"
#include "../../src/mod/time_log_math.h"

struct Gpu {
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  uint32_t queue_family = 0;
  VkQueue queue = VK_NULL_HANDLE;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  VkPhysicalDeviceMemoryProperties memory = {};
  // THE TIME LOG (time_log=1, GpuTimingStart): the wait of GpuBegin for the
  // fence of the last submit, and the time of the GPU of each command buffer
  // (a timestamp at its start and at its end). The caller takes the stats and
  // sets them back.
  VkQueryPool query = VK_NULL_HANDLE;
  bool query_written = false;
  double timestamp_ns = 0.0;  // the ns of a tick of a timestamp
  uint64_t timestamp_mask = 0;  // the valid bits of a timestamp of the queue
  bool timing = false;
  time_log_math::Stat fence_wait;  // ms of the waits of GpuBegin
  time_log_math::Stat busy;        // ms of the GPU of each command buffer

  // THE RING OF COMMAND BUFFERS (frame_loop=1, GpuRingStart, frame_loop.h,
  // CommandRing). Each GpuBegin takes the least recently used entry when
  // its fence says that the GPU finished it, else a new entry. So GpuBegin
  // does not wait on the CPU. Each submit has a serial, and GpuSerialDone
  // says if the GPU finished it.
  struct RingEntry {
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    // xr_pool_reset=1: the command pool of this entry alone, with no reset of
    // one command buffer. Null: cmd is of `pool`.
    VkCommandPool pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    uint32_t query = 0;      // its two timestamps in ring_query
    uint64_t serial = 0;     // the serial of its last submit, 0: none
    bool query_written = false;
  };
  bool ring = false;
  // xr_pool_reset (1 by default): a new entry of the ring gets a command pool
  // of its own, and its begin resets that pool (vkResetCommandPool). 0: the
  // command buffer of an entry is of `pool`, and its begin resets the
  // command buffer (vkResetCommandBuffer).
  bool pool_reset = true;
  frame_loop::CommandRing ring_order{16};
  std::vector<RingEntry> ring_entries;
  int ring_current = -1;   // the entry of the open command buffer
  uint64_t serial = 0;     // the serial of the last submit
  uint64_t done_serial = 0;  // the GPU finished each submit up to this serial
  VkQueryPool ring_query = VK_NULL_HANDLE;
  // The timestamps of each copy in an entry (time_log=1, copy_time.h), by
  // the index of the queries of the entry.
  copy_time::Stamps copy_stamps;
  uint32_t ring_queries = 0;  // the entries that have their timestamps
  uint64_t ring_grows = 0;    // new entries because the GPU was not done
  uint64_t ring_waits = 0;    // waits of a full ring
  // THE TWO QUEUES (frame_loop_queues=2): a timeline semaphore that each
  // submit of the ring signals with its serial, so a submit of another queue
  // can wait on the GPU for a copy of the session. VK_NULL_HANDLE: none.
  VkSemaphore timeline = VK_NULL_HANDLE;
};

// The pool, the command buffer and the fence of a Gpu with a device.
bool GpuFinish(Gpu* gpu);
// THE TIME LOG: after GpuFinish, the timestamps of each command buffer and
// the time of the waits of GpuBegin. False (with a line of the log) when the
// queue has no timestamps: then only the waits count.
bool GpuTimingStart(Gpu* gpu);
// Destroys what GpuFinish made. The device and the instance stay.
void GpuRelease(Gpu* gpu);
// THE RING (frame_loop=1). On: GpuBegin and GpuSubmit use the ring of
// command buffers. Off: the one command buffer and its fence again. Each
// change waits until the device is idle. The caller holds the lock of the
// queue.
void GpuSetRing(Gpu* gpu, bool on);
// True when the GPU finished the submit of this serial. It reads the fence
// of the entry of the serial, and never waits.
bool GpuSerialDone(Gpu* gpu, uint64_t serial);
// The serial that the next GpuSubmit gives.
inline uint64_t GpuNextSerial(const Gpu& gpu) { return gpu.serial + 1; }
// THE TIME OF EACH COPY: the index of the open command buffer of the ring in
// Gpu::copy_stamps, or UINT32_MAX with no open command buffer of the ring.
inline uint32_t GpuStampIndex(const Gpu& gpu) {
  return gpu.ring && gpu.ring_current >= 0 ? gpu.ring_entries[gpu.ring_current].query
                                           : UINT32_MAX;
}

// A buffer that the CPU writes and the GPU reads.
struct Staging {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  void* mapped = nullptr;
  VkDeviceSize size = 0;
};
bool MakeStaging(Gpu* gpu, VkDeviceSize size, Staging* out);
void FreeStaging(Gpu* gpu, Staging* staging);

// A begin of the command buffer of the Gpu. Each recording waits first for
// the last submit, so the app has one frame on the GPU at most. With the
// ring: the next entry of the ring, with no wait.
VkCommandBuffer GpuBegin(Gpu* gpu);
// The end and the submit of the command buffer. wait and signal are
// semaphores of a swapchain of a window, or null.
bool GpuSubmit(Gpu* gpu, VkSemaphore wait, VkSemaphore signal);

// A barrier of a whole image of one layer and one mip level.
void ImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from,
                  VkImageLayout to, VkAccessFlags src_access,
                  VkAccessFlags dst_access, VkPipelineStageFlags src_stage,
                  VkPipelineStageFlags dst_stage);
// The copy of a whole staging buffer of width * height * 4 bytes into an
// image in the layout TRANSFER_DST_OPTIMAL.
void CopyToImage(VkCommandBuffer cmd, const Staging& staging, VkImage image,
                 uint32_t width, uint32_t height);
