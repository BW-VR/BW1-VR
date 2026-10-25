// THE VULKAN DEVICE OF THE HEADSET APP and the helpers for the copies of the
// pictures of the CPU (docs/android.md, "The picture").
//
// In the headset the OpenXR runtime makes the instance and the device
// (xr_app.cpp). On a flat screen the app makes them (flat_view.cpp). Both
// fill a Gpu, and the rest of the app uses only the Gpu.
#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

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
};

// The pool, the command buffer and the fence of a Gpu with a device.
bool GpuFinish(Gpu* gpu);
// THE TIME LOG: after GpuFinish, the timestamps of each command buffer and
// the time of the waits of GpuBegin. False (with a line of the log) when the
// queue has no timestamps: then only the waits count.
bool GpuTimingStart(Gpu* gpu);
// Destroys what GpuFinish made. The device and the instance stay.
void GpuRelease(Gpu* gpu);

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
// the last submit, so the app has one frame on the GPU at most.
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
