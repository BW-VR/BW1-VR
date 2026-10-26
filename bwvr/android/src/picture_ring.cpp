#include "picture_ring.h"

#include <time.h>

#include "app_log.h"
#include "xr_app.h"

namespace {

double NowMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

}  // namespace

bool PictureRing::Start(const Gpu& gpu, VkQueue queue, VkSemaphore session_timeline,
                        bool swap_red_blue) {
  if (started_) return true;
  if (gpu.device == VK_NULL_HANDLE) return false;
  device_ = gpu.device;
  queue_ = queue;
  session_timeline_ = session_timeline;
  memory_ = gpu.memory;
  swap_red_blue_ = swap_red_blue;
  // The byte order of the swapchains, so the copy of the session keeps the
  // bytes (an sRGB swapchain too).
  format_ = swap_red_blue ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
  // The play thread records into a pool of its own: a pool is for one thread.
  VkCommandPoolCreateInfo pool = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool.queueFamilyIndex = gpu.queue_family;
  if (vkCreateCommandPool(device_, &pool, nullptr, &pool_) != VK_SUCCESS) {
    pool_ = VK_NULL_HANDLE;
    AppLog("ring         : vkCreateCommandPool failed, so the app has no ring of the pictures");
    return false;
  }
  for (Slot& s : slots_) {
    VkCommandBufferAllocateInfo alloc = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = pool_;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkFenceCreateInfo fence = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkAllocateCommandBuffers(device_, &alloc, &s.cmd) != VK_SUCCESS ||
        vkCreateFence(device_, &fence, nullptr, &s.fence) != VK_SUCCESS) {
      AppLog("ring         : vkAllocateCommandBuffers or vkCreateFence failed, so the app "
             "has no ring of the pictures");
      Stop();
      return false;
    }
  }
  // THE TIME LOG: the timestamps of the copies of each slot, as GpuBegin and
  // GpuSubmit write them.
  if (gpu.query != VK_NULL_HANDLE) {
    VkQueryPoolCreateInfo info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = 2 * frame_loop::kSlots;
    if (vkCreateQueryPool(device_, &info, nullptr, &query_) != VK_SUCCESS) {
      query_ = VK_NULL_HANDLE;
    }
    timestamp_ns_ = gpu.timestamp_ns;
    timestamp_mask_ = gpu.timestamp_mask;
  }
  started_ = true;
  AppLog("ring         : a ring of %d slots for the pictures of the game, each an image of "
         "the eye pictures (a layer for each eye) and, when the picture needs them, an image "
         "of the panel and of the screen, %s",
         frame_loop::kSlots,
         swap_red_blue ? "B8G8R8A8_UNORM (a blit puts blue first)"
                       : "R8G8B8A8_UNORM (a copy of the bytes of the renderer)");
  AppLog("ring         : the copies into the slots go to %s",
         session_timeline != VK_NULL_HANDLE
             ? "the second queue with the renderer, and a write of a slot waits on the GPU "
               "for the last copy of the session from it (a timeline semaphore)"
             : "the queue of the session, after the walks of the renderer");
  return true;
}

void PictureRing::Stop() {
  if (device_ == VK_NULL_HANDLE) return;
  XrApp::QueueLock();
  vkDeviceWaitIdle(device_);
  XrApp::QueueUnlock();
  for (Slot& s : slots_) {
    FreeImage(&s.eyes);
    FreeImage(&s.panel);
    FreeImage(&s.screen);
    if (s.fence != VK_NULL_HANDLE) vkDestroyFence(device_, s.fence, nullptr);
    s.fence = VK_NULL_HANDLE;
    s.cmd = VK_NULL_HANDLE;
  }
  if (query_ != VK_NULL_HANDLE) vkDestroyQueryPool(device_, query_, nullptr);
  query_ = VK_NULL_HANDLE;
  if (pool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, pool_, nullptr);
  pool_ = VK_NULL_HANDLE;
  if (started_) {
    AppLog("ring         : the ring of the pictures ends: %llu pictures that the play thread "
           "dropped (no free slot), %llu pictures that a newer picture replaced before a "
           "frame of the session",
           static_cast<unsigned long long>(ring_.Dropped()),
           static_cast<unsigned long long>(ring_.Replaced()));
  }
  started_ = false;
  device_ = VK_NULL_HANDLE;
}

bool PictureRing::MakeImage(Image* image, uint32_t width, uint32_t height, uint32_t layers) {
  VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = format_;
  info.extent = {width, height, 1};
  info.mipLevels = 1;
  info.arrayLayers = layers;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if (vkCreateImage(device_, &info, nullptr, &image->image) != VK_SUCCESS) {
    image->image = VK_NULL_HANDLE;
    return false;
  }
  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(device_, image->image, &req);
  uint32_t type = UINT32_MAX;
  for (uint32_t i = 0; i < memory_.memoryTypeCount; ++i) {
    if ((req.memoryTypeBits & (1u << i)) != 0 &&
        (memory_.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
      type = i;
      break;
    }
  }
  VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  alloc.allocationSize = req.size;
  alloc.memoryTypeIndex = type;
  if (type == UINT32_MAX ||
      vkAllocateMemory(device_, &alloc, nullptr, &image->memory) != VK_SUCCESS) {
    image->memory = VK_NULL_HANDLE;
    FreeImage(image);
    return false;
  }
  vkBindImageMemory(device_, image->image, image->memory, 0);
  image->width = width;
  image->height = height;
  image->layers = layers;
  return true;
}

void PictureRing::FreeImage(Image* image) {
  if (image->image != VK_NULL_HANDLE) vkDestroyImage(device_, image->image, nullptr);
  if (image->memory != VK_NULL_HANDLE) vkFreeMemory(device_, image->memory, nullptr);
  *image = Image();
}

int PictureRing::BeginWrite(double* waited) {
  *waited = 0.0;
  bool dropped = false;
  int i = -1;
  {
    std::lock_guard<std::mutex> g(lock_);
    i = ring_.BeginWrite(&dropped);
    if (dropped) ++stats_.dropped;
  }
  if (i < 0) return -1;
  Slot& s = slots_[i];
  // A slot that was shown is finished. A dropped slot can still have its
  // copies on the GPU: the play thread waits for them, never the session.
  if (s.submitted && vkGetFenceStatus(device_, s.fence) != VK_SUCCESS) {
    const double start = NowMs();
    vkWaitForFences(device_, 1, &s.fence, VK_TRUE, UINT64_MAX);
    *waited = NowMs() - start;
    std::lock_guard<std::mutex> g(lock_);
    stats_.slot_wait.Add(*waited);
  }
  if (s.query_written) {
    s.query_written = false;
    uint64_t stamps[4] = {0, 0, 0, 0};
    if (vkGetQueryPoolResults(device_, query_, 2 * i, 2, sizeof(stamps), stamps,
                              2 * sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) ==
            VK_SUCCESS &&
        stamps[1] != 0 && stamps[3] != 0) {
      const uint64_t ticks = (stamps[2] - stamps[0]) & timestamp_mask_;
      const double ms = static_cast<double>(ticks) * timestamp_ns_ * 1e-6;
      std::lock_guard<std::mutex> g(lock_);
      // A time over 100 ms is not the time of one command buffer.
      if (ms < 100.0) stats_.busy.Add(ms);
    }
  }
  vkResetFences(device_, 1, &s.fence);
  s.submitted = false;
  vkResetCommandBuffer(s.cmd, 0);
  VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(s.cmd, &begin);
  if (query_ != VK_NULL_HANDLE) {
    vkCmdResetQueryPool(s.cmd, query_, 2 * i, 2);
    // At the bottom of the pipe: after the walk before it on the queue.
    vkCmdWriteTimestamp(s.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_, 2 * i);
  }
  return i;
}

void PictureRing::Copy(int i, VkImage source, uint32_t first, uint32_t layers,
                       const Image& to, uint32_t width, uint32_t height) {
  VkCommandBuffer cmd = slots_[i].cmd;
  VkImageMemoryBarrier b[2] = {{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                               {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}};
  // The image of the renderer: from a color attachment to a source. The
  // image of the slot to a target: the stage of the transfers waits for a
  // copy of the session that still reads the picture before in it.
  b[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  b[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  b[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  b[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  b[0].srcQueueFamilyIndex = b[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b[0].image = source;
  b[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, first, layers};
  b[1].srcAccessMask = 0;
  b[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  b[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  b[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  b[1].srcQueueFamilyIndex = b[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b[1].image = to.image;
  b[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layers};
  vkCmdPipelineBarrier(cmd,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, b);
  if (!swap_red_blue_) {
    // The same format: a copy of the bytes.
    VkImageCopy copy = {};
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, first, layers};
    copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, layers};
    copy.extent = {width, height, 1};
    vkCmdCopyImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, to.image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  } else {
    // Blue first: a blit of the same size puts the channels in that order
    // and takes each pixel as it is.
    VkImageBlit blit = {};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, first, layers};
    blit.srcOffsets[1] = {static_cast<int32_t>(width), static_cast<int32_t>(height), 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, layers};
    blit.dstOffsets[1] = {static_cast<int32_t>(width), static_cast<int32_t>(height), 1};
    vkCmdBlitImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, to.image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
  }
  // The image of the renderer back to a color attachment for its next walk,
  // and the image of the slot to a source for the copy of the session.
  b[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  b[0].dstAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  b[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  b[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  b[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  b[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  b[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  b[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT |
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       0, 0, nullptr, 0, nullptr, 2, b);
}

// An image of a slot is made at its first use and again at a new size. The
// GPU may still copy from the old image, so the device must be idle before
// it goes.
bool PictureRing::CopyEyes(int i, VkImage source, uint32_t width, uint32_t height) {
  Slot& s = slots_[i];
  if (source == VK_NULL_HANDLE || width == 0 || height == 0) return false;
  if (s.eyes.width != width || s.eyes.height != height || s.eyes.layers != 2) {
    if (s.eyes.image != VK_NULL_HANDLE) {
      XrApp::QueueLock();
      vkDeviceWaitIdle(device_);
      XrApp::QueueUnlock();
      FreeImage(&s.eyes);
    }
    if (!MakeImage(&s.eyes, width, height, 2)) {
      AppLog("ring         : slot %d: no image of %u by %u for the eye pictures", i, width,
             height);
      return false;
    }
    AppLog("ring         : slot %d: an image of %u by %u with 2 layers for the eye pictures "
           "(%.1f MB)", i, width, height, width * height * 8.0 / (1024.0 * 1024.0));
  }
  Copy(i, source, 0, 2, s.eyes, width, height);
  return true;
}

bool PictureRing::CopyPanel(int i, VkImage source, uint32_t width, uint32_t height) {
  Slot& s = slots_[i];
  if (source == VK_NULL_HANDLE || width == 0 || height == 0) return false;
  if (s.panel.width != width || s.panel.height != height) {
    if (s.panel.image != VK_NULL_HANDLE) {
      XrApp::QueueLock();
      vkDeviceWaitIdle(device_);
      XrApp::QueueUnlock();
      FreeImage(&s.panel);
    }
    if (!MakeImage(&s.panel, width, height, 1)) {
      AppLog("ring         : slot %d: no image of %u by %u for the panel", i, width, height);
      return false;
    }
    AppLog("ring         : slot %d: an image of %u by %u for the panel image (%.1f MB)", i,
           width, height, width * height * 4.0 / (1024.0 * 1024.0));
  }
  Copy(i, source, 0, 1, s.panel, width, height);
  return true;
}

bool PictureRing::CopyScreen(int i, VkImage source, uint32_t layer, uint32_t width,
                             uint32_t height) {
  Slot& s = slots_[i];
  if (source == VK_NULL_HANDLE || width == 0 || height == 0) return false;
  if (s.screen.width != width || s.screen.height != height) {
    if (s.screen.image != VK_NULL_HANDLE) {
      XrApp::QueueLock();
      vkDeviceWaitIdle(device_);
      XrApp::QueueUnlock();
      FreeImage(&s.screen);
    }
    if (!MakeImage(&s.screen, width, height, 1)) {
      AppLog("ring         : slot %d: no image of %u by %u for the screen picture", i, width,
             height);
      return false;
    }
    AppLog("ring         : slot %d: an image of %u by %u for the screen picture (%.1f MB)", i,
           width, height, width * height * 4.0 / (1024.0 * 1024.0));
  }
  Copy(i, source, layer, 1, s.screen, width, height);
  return true;
}

void PictureRing::EndWrite(int i) {
  Slot& s = slots_[i];
  if (query_ != VK_NULL_HANDLE) {
    vkCmdWriteTimestamp(s.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_, 2 * i + 1);
  }
  vkEndCommandBuffer(s.cmd);
  VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &s.cmd;
  // THE SECOND QUEUE: the session may still copy the picture before from
  // this slot on its own queue, so the write waits on the GPU for that copy.
  // One queue keeps the order by itself.
  const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkTimelineSemaphoreSubmitInfo values = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
  if (session_timeline_ != VK_NULL_HANDLE && s.read_serial > 0) {
    values.waitSemaphoreValueCount = 1;
    values.pWaitSemaphoreValues = &s.read_serial;
    submit.pNext = &values;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &session_timeline_;
    submit.pWaitDstStageMask = &stage;
  }
  XrApp::QueueLock();
  const bool ok = vkQueueSubmit(queue_, 1, &submit, s.fence) == VK_SUCCESS;
  XrApp::QueueUnlock();
  s.submitted = ok;
  s.query_written = ok && query_ != VK_NULL_HANDLE;
  std::lock_guard<std::mutex> g(lock_);
  if (ok) {
    ring_.EndWrite(i);
  } else {
    ring_.CancelWrite(i);
  }
}

bool PictureRing::WaitGpu(int depth, uint64_t timeout_ns) {
  // Only the play thread resets a fence and sets `submitted`.
  const auto busy = [&](VkFence* fences) {
    int n = 0;
    for (Slot& s : slots_) {
      if (s.submitted && vkGetFenceStatus(device_, s.fence) != VK_SUCCESS) fences[n++] = s.fence;
    }
    return n;
  };
  VkFence fences[frame_loop::kSlots];
  const int n = busy(fences);
  if (frame_loop::MayPlay(depth, n)) return true;
  vkWaitForFences(device_, static_cast<uint32_t>(n), fences, VK_FALSE, timeout_ns);
  return frame_loop::MayPlay(depth, busy(fences));
}

const Picture* PictureRing::Take(uint64_t session_serial) {
  std::lock_guard<std::mutex> g(lock_);
  const int before = ring_.Shown();
  // The fences of the ready slots, with no wait. The play thread resets a
  // fence only for a slot in writing, which is not ready.
  bool done[frame_loop::kSlots];
  for (int i = 0; i < frame_loop::kSlots; ++i) {
    done[i] = ring_.State(i) == frame_loop::Slot::kReady &&
              vkGetFenceStatus(device_, slots_[i].fence) == VK_SUCCESS;
  }
  const uint64_t replaced = ring_.Replaced();
  const int i = ring_.Take(done);
  stats_.replaced += ring_.Replaced() - replaced;
  // Each copy from the slot shown before is in a submit up to this serial.
  if (i >= 0 && before >= 0 && before != i) slots_[before].read_serial = session_serial;
  return i >= 0 ? &slots_[i].picture : nullptr;
}

void PictureRing::Clear(uint64_t session_serial) {
  std::lock_guard<std::mutex> g(lock_);
  if (ring_.Shown() >= 0) slots_[ring_.Shown()].read_serial = session_serial;
  ring_.Clear();
}

void PictureRing::TakeStats(Stats* out) {
  std::lock_guard<std::mutex> g(lock_);
  *out = stats_;
  stats_ = Stats();
}
