#include "gpu.h"

#include <time.h>

#include "app_log.h"

namespace {

uint32_t FindMemory(const Gpu& gpu, uint32_t bits, VkMemoryPropertyFlags want) {
  for (uint32_t i = 0; i < gpu.memory.memoryTypeCount; ++i) {
    if ((bits & (1u << i)) != 0 &&
        (gpu.memory.memoryTypes[i].propertyFlags & want) == want) {
      return i;
    }
  }
  return UINT32_MAX;
}

double NowMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

// The two timestamps at `first` of a pool into the time of the GPU of a
// command buffer that the GPU finished.
void TakeStamps(Gpu* gpu, VkQueryPool pool, uint32_t first) {
  uint64_t stamps[4] = {0, 0, 0, 0};
  if (vkGetQueryPoolResults(gpu->device, pool, first, 2, sizeof(stamps), stamps,
                            2 * sizeof(uint64_t),
                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) ==
          VK_SUCCESS &&
      stamps[1] != 0 && stamps[3] != 0) {
    const uint64_t ticks = (stamps[2] - stamps[0]) & gpu->timestamp_mask;
    const double ms = static_cast<double>(ticks) * gpu->timestamp_ns * 1e-6;
    // A time over 100 ms is not the time of one command buffer.
    if (ms < 100.0) gpu->busy.Add(ms);
  }
}

// THE RING: the next entry, with no wait while the ring can grow.
VkCommandBuffer RingBegin(Gpu* gpu) {
  const int next = gpu->ring_order.Next();
  bool next_done = true;
  if (gpu->ring_order.Count() > 0) {
    const Gpu::RingEntry& e = gpu->ring_entries[next];
    next_done = e.serial == 0 || vkGetFenceStatus(gpu->device, e.fence) == VK_SUCCESS;
  }
  const frame_loop::CommandPick pick = gpu->ring_order.Pick(next_done);
  if (pick.grow) {
    Gpu::RingEntry e;
    VkCommandBufferAllocateInfo alloc = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = gpu->pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkFenceCreateInfo fence = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkAllocateCommandBuffers(gpu->device, &alloc, &e.cmd) != VK_SUCCESS ||
        vkCreateFence(gpu->device, &fence, nullptr, &e.fence) != VK_SUCCESS) {
      AppLog("gpu          : the ring of command buffers cannot grow (vkAllocateCommandBuffers "
             "or vkCreateFence)");
    }
    e.query = gpu->ring_queries++;
    gpu->ring_entries.insert(gpu->ring_entries.begin() + pick.index, e);
    if (gpu->ring_order.Count() > 1) ++gpu->ring_grows;
  }
  Gpu::RingEntry& e = gpu->ring_entries[pick.index];
  if (pick.wait) {
    ++gpu->ring_waits;
    const double start = gpu->timing ? NowMs() : 0.0;
    vkWaitForFences(gpu->device, 1, &e.fence, VK_TRUE, UINT64_MAX);
    if (gpu->timing) gpu->fence_wait.Add(NowMs() - start);
  }
  // The GPU finished the last submit of this entry.
  if (e.serial > gpu->done_serial) gpu->done_serial = e.serial;
  if (e.query_written) {
    e.query_written = false;
    TakeStamps(gpu, gpu->ring_query, 2 * e.query);
  }
  gpu->ring_current = pick.index;
  vkResetCommandBuffer(e.cmd, 0);
  VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(e.cmd, &begin);
  if (gpu->ring_query != VK_NULL_HANDLE) {
    vkCmdResetQueryPool(e.cmd, gpu->ring_query, 2 * e.query, 2);
    // At the bottom of the pipe, as GpuBegin with no ring.
    vkCmdWriteTimestamp(e.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu->ring_query,
                        2 * e.query);
  }
  return e.cmd;
}

}  // namespace

void GpuSetRing(Gpu* gpu, bool on) {
  if (on == gpu->ring || gpu->device == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(gpu->device);
  if (on) {
    gpu->ring = true;
    gpu->ring_current = -1;
    if (gpu->query != VK_NULL_HANDLE && gpu->ring_query == VK_NULL_HANDLE) {
      VkQueryPoolCreateInfo info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
      info.queryType = VK_QUERY_TYPE_TIMESTAMP;
      info.queryCount = 2 * static_cast<uint32_t>(gpu->ring_order.Most());
      if (vkCreateQueryPool(gpu->device, &info, nullptr, &gpu->ring_query) != VK_SUCCESS) {
        gpu->ring_query = VK_NULL_HANDLE;
      }
    }
    AppLog("gpu          : the ring of command buffers is on (up to %d). A copy of the "
           "session does not wait on the CPU for the GPU",
           gpu->ring_order.Most());
    return;
  }
  // The GPU is idle, so each submit of the ring is finished.
  for (Gpu::RingEntry& e : gpu->ring_entries) {
    if (e.query_written) TakeStamps(gpu, gpu->ring_query, 2 * e.query);
    e.query_written = false;
  }
  gpu->done_serial = gpu->serial;
  gpu->ring = false;
  gpu->ring_current = -1;
  AppLog("gpu          : the ring of command buffers is off: %zu entries, %llu new entries "
         "because the GPU was not done, %llu waits of a full ring",
         gpu->ring_entries.size(), static_cast<unsigned long long>(gpu->ring_grows),
         static_cast<unsigned long long>(gpu->ring_waits));
}

bool GpuSerialDone(Gpu* gpu, uint64_t serial) {
  if (frame_loop::SerialDone(serial, gpu->done_serial)) return true;
  for (const Gpu::RingEntry& e : gpu->ring_entries) {
    if (e.serial != serial) continue;
    if (vkGetFenceStatus(gpu->device, e.fence) != VK_SUCCESS) return false;
    gpu->done_serial = serial;
    return true;
  }
  // No entry has the serial any more: a later begin used its entry again
  // after its fence, so the GPU finished it.
  return true;
}

bool GpuTimingStart(Gpu* gpu) {
  gpu->timing = true;
  VkPhysicalDeviceProperties p;
  vkGetPhysicalDeviceProperties(gpu->physical, &p);
  uint32_t families = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(gpu->physical, &families, nullptr);
  uint32_t bits = 0;
  if (gpu->queue_family < families) {
    VkQueueFamilyProperties family[32];
    uint32_t n = families < 32 ? families : 32;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu->physical, &n, family);
    if (gpu->queue_family < n) bits = family[gpu->queue_family].timestampValidBits;
  }
  if (bits == 0 || p.limits.timestampPeriod <= 0.0f) {
    AppLog("gpu          : time_log=1, but the queue has no timestamps (%u valid bits, "
           "%.3f ns a tick). The log has no time of the GPU of the app",
           bits, p.limits.timestampPeriod);
    return false;
  }
  VkQueryPoolCreateInfo info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  info.queryCount = 2;
  if (vkCreateQueryPool(gpu->device, &info, nullptr, &gpu->query) != VK_SUCCESS) {
    gpu->query = VK_NULL_HANDLE;
    AppLog("gpu          : time_log=1, but vkCreateQueryPool failed. The log has no "
           "time of the GPU of the app");
    return false;
  }
  gpu->timestamp_ns = p.limits.timestampPeriod;
  gpu->timestamp_mask = bits >= 64 ? ~0ull : (1ull << bits) - 1;
  AppLog("gpu          : time_log=1. Each command buffer of the app has a timestamp at "
         "its start and at its end (%u valid bits, %.3f ns a tick)",
         bits, p.limits.timestampPeriod);
  return true;
}

bool GpuFinish(Gpu* gpu) {
  vkGetPhysicalDeviceMemoryProperties(gpu->physical, &gpu->memory);
  vkGetDeviceQueue(gpu->device, gpu->queue_family, 0, &gpu->queue);
  VkCommandPoolCreateInfo pool = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool.queueFamilyIndex = gpu->queue_family;
  if (vkCreateCommandPool(gpu->device, &pool, nullptr, &gpu->pool) != VK_SUCCESS) {
    AppLog("gpu          : vkCreateCommandPool failed");
    return false;
  }
  VkCommandBufferAllocateInfo alloc = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  alloc.commandPool = gpu->pool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1;
  if (vkAllocateCommandBuffers(gpu->device, &alloc, &gpu->cmd) != VK_SUCCESS) {
    AppLog("gpu          : vkAllocateCommandBuffers failed");
    return false;
  }
  VkFenceCreateInfo fence = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  if (vkCreateFence(gpu->device, &fence, nullptr, &gpu->fence) != VK_SUCCESS) {
    AppLog("gpu          : vkCreateFence failed");
    return false;
  }
  VkPhysicalDeviceProperties p;
  vkGetPhysicalDeviceProperties(gpu->physical, &p);
  AppLog("gpu          : %s, Vulkan %u.%u.%u, driver 0x%X", p.deviceName,
         VK_API_VERSION_MAJOR(p.apiVersion), VK_API_VERSION_MINOR(p.apiVersion),
         VK_API_VERSION_PATCH(p.apiVersion), p.driverVersion);
  return true;
}

void GpuRelease(Gpu* gpu) {
  if (gpu->device == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(gpu->device);
  if (gpu->fence != VK_NULL_HANDLE) vkDestroyFence(gpu->device, gpu->fence, nullptr);
  if (gpu->query != VK_NULL_HANDLE) vkDestroyQueryPool(gpu->device, gpu->query, nullptr);
  gpu->query = VK_NULL_HANDLE;
  for (Gpu::RingEntry& e : gpu->ring_entries) {
    if (e.fence != VK_NULL_HANDLE) vkDestroyFence(gpu->device, e.fence, nullptr);
  }
  gpu->ring_entries.clear();
  if (gpu->ring_query != VK_NULL_HANDLE) {
    vkDestroyQueryPool(gpu->device, gpu->ring_query, nullptr);
  }
  gpu->ring_query = VK_NULL_HANDLE;
  gpu->ring = false;
  if (gpu->timeline != VK_NULL_HANDLE) vkDestroySemaphore(gpu->device, gpu->timeline, nullptr);
  gpu->timeline = VK_NULL_HANDLE;
  gpu->query_written = false;
  if (gpu->pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(gpu->device, gpu->pool, nullptr);
  }
  gpu->fence = VK_NULL_HANDLE;
  gpu->pool = VK_NULL_HANDLE;
  gpu->cmd = VK_NULL_HANDLE;
}

bool MakeStaging(Gpu* gpu, VkDeviceSize size, Staging* out) {
  VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  info.size = size;
  info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(gpu->device, &info, nullptr, &out->buffer) != VK_SUCCESS) {
    return false;
  }
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(gpu->device, out->buffer, &req);
  const uint32_t type = FindMemory(*gpu, req.memoryTypeBits,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (type == UINT32_MAX) return false;
  VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  alloc.allocationSize = req.size;
  alloc.memoryTypeIndex = type;
  if (vkAllocateMemory(gpu->device, &alloc, nullptr, &out->memory) != VK_SUCCESS) {
    return false;
  }
  vkBindBufferMemory(gpu->device, out->buffer, out->memory, 0);
  if (vkMapMemory(gpu->device, out->memory, 0, size, 0, &out->mapped) !=
      VK_SUCCESS) {
    return false;
  }
  out->size = size;
  return true;
}

void FreeStaging(Gpu* gpu, Staging* s) {
  if (gpu->device == VK_NULL_HANDLE) return;
  if (s->memory != VK_NULL_HANDLE) {
    vkUnmapMemory(gpu->device, s->memory);
    vkFreeMemory(gpu->device, s->memory, nullptr);
  }
  if (s->buffer != VK_NULL_HANDLE) vkDestroyBuffer(gpu->device, s->buffer, nullptr);
  *s = Staging();
}

VkCommandBuffer GpuBegin(Gpu* gpu) {
  if (gpu->ring) return RingBegin(gpu);
  if (gpu->timing) {
    const double start = NowMs();
    vkWaitForFences(gpu->device, 1, &gpu->fence, VK_TRUE, UINT64_MAX);
    gpu->fence_wait.Add(NowMs() - start);
  } else {
    vkWaitForFences(gpu->device, 1, &gpu->fence, VK_TRUE, UINT64_MAX);
  }
  // The timestamps of the last submit: the fence says that the GPU is done.
  if (gpu->query_written) {
    gpu->query_written = false;
    TakeStamps(gpu, gpu->query, 0);
  }
  vkResetCommandBuffer(gpu->cmd, 0);
  VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(gpu->cmd, &begin);
  if (gpu->query != VK_NULL_HANDLE) {
    vkCmdResetQueryPool(gpu->cmd, gpu->query, 0, 2);
    // At the bottom of the pipe: the stamp comes when the work before it on
    // the queue (the walk of the renderer) is done, so the time of the walk
    // is not in the time of this command buffer too.
    vkCmdWriteTimestamp(gpu->cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu->query, 0);
  }
  return gpu->cmd;
}

bool GpuSubmit(Gpu* gpu, VkSemaphore wait, VkSemaphore signal) {
  if (gpu->ring) {
    // The ring is only for the session of the headset: no semaphore of a
    // window.
    if (gpu->ring_current < 0) return false;
    Gpu::RingEntry& e = gpu->ring_entries[gpu->ring_current];
    gpu->ring_current = -1;
    if (gpu->ring_query != VK_NULL_HANDLE) {
      vkCmdWriteTimestamp(e.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu->ring_query,
                          2 * e.query + 1);
      e.query_written = true;
    }
    vkEndCommandBuffer(e.cmd);
    vkResetFences(gpu->device, 1, &e.fence);
    e.serial = ++gpu->serial;
    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &e.cmd;
    // The timeline of the session gets the serial (frame_loop_queues=2).
    VkTimelineSemaphoreSubmitInfo values = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    if (gpu->timeline != VK_NULL_HANDLE) {
      values.signalSemaphoreValueCount = 1;
      values.pSignalSemaphoreValues = &e.serial;
      submit.pNext = &values;
      submit.signalSemaphoreCount = 1;
      submit.pSignalSemaphores = &gpu->timeline;
    }
    return vkQueueSubmit(gpu->queue, 1, &submit, e.fence) == VK_SUCCESS;
  }
  if (gpu->query != VK_NULL_HANDLE) {
    vkCmdWriteTimestamp(gpu->cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, gpu->query, 1);
    gpu->query_written = true;
  }
  vkEndCommandBuffer(gpu->cmd);
  vkResetFences(gpu->device, 1, &gpu->fence);
  const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &gpu->cmd;
  if (wait != VK_NULL_HANDLE) {
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &wait;
    submit.pWaitDstStageMask = &stage;
  }
  if (signal != VK_NULL_HANDLE) {
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &signal;
  }
  return vkQueueSubmit(gpu->queue, 1, &submit, gpu->fence) == VK_SUCCESS;
}

void ImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from,
                  VkImageLayout to, VkAccessFlags src_access,
                  VkAccessFlags dst_access, VkPipelineStageFlags src_stage,
                  VkPipelineStageFlags dst_stage) {
  VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  b.srcAccessMask = src_access;
  b.dstAccessMask = dst_access;
  b.oldLayout = from;
  b.newLayout = to;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = image;
  b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  b.subresourceRange.levelCount = 1;
  b.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1,
                       &b);
}

void CopyToImage(VkCommandBuffer cmd, const Staging& staging, VkImage image,
                 uint32_t width, uint32_t height) {
  VkBufferImageCopy region = {};
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {width, height, 1};
  vkCmdCopyBufferToImage(cmd, staging.buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}
