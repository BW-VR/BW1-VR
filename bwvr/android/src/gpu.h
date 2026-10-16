// THE VULKAN DEVICE OF THE HEADSET APP and the helpers for the copies of the
// pictures of the CPU (docs/android.md, "The picture").
//
// In the headset the OpenXR runtime makes the instance and the device
// (xr_app.cpp). On a flat screen the app makes them (flat_view.cpp). Both
// fill a Gpu, and the rest of the app uses only the Gpu.
#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>

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
};

// The pool, the command buffer and the fence of a Gpu with a device.
bool GpuFinish(Gpu* gpu);
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
