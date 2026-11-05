#include "copy_time.h"

#include <mutex>

namespace copy_time {

namespace {

std::mutex g_lock;
Window g_window;

}  // namespace

const char* Name(int kind) {
  switch (kind) {
    case kSlotEyes: return "the eye pictures into a slot of the ring";
    case kSlotDirect: return "the eye pictures straight into the swapchains (xr_eye_direct=1)";
    case kSlotCompose:
      return "the eye pictures made from the two images straight into the swapchains "
             "(xr_compose_direct=1)";
    case kSlotComposeEyes:
      return "the eye pictures made from the two images into the image of the renderer "
             "(xr_compose_direct=1, a picture that took the old way)";
    case kSlotPanel: return "the panel image into a slot";
    case kSlotScreen: return "the screen picture into a slot";
    case kSwapEyes: return "an eye picture of a slot into its swapchain";
    case kSwapCompose: return "the eye pictures made from the two images at the take (xr_compose_take=1)";
    case kSwapStereo: return "an eye picture into the stereo screen";
    case kSwapPanel: return "the panel image into its swapchain";
    case kSwapScreen: return "the screen picture into the screen quad";
    default: return "a copy";
  }
}

void Add(int kind, double ms, uint64_t bytes) {
  if (kind < 0 || kind >= kKinds) return;
  std::lock_guard<std::mutex> g(g_lock);
  g_window.ms[kind].Add(ms);
  g_window.bytes[kind] += bytes;
}

void TakeWindow(Window* out) {
  std::lock_guard<std::mutex> g(g_lock);
  *out = g_window;
  g_window = Window();
}

bool Stamps::Start(VkDevice device, double timestamp_ns, uint64_t mask, uint32_t buffers) {
  Stop();
  if (device == VK_NULL_HANDLE || buffers == 0 || timestamp_ns <= 0.0) return false;
  VkQueryPoolCreateInfo info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  info.queryCount = 2 * kPairs * buffers;
  if (vkCreateQueryPool(device, &info, nullptr, &pool_) != VK_SUCCESS) {
    pool_ = VK_NULL_HANDLE;
    return false;
  }
  device_ = device;
  ns_ = timestamp_ns;
  mask_ = mask == 0 ? ~0ull : mask;
  buffers_.assign(buffers, Buffer());
  return true;
}

// The caller makes sure that the GPU finished each command buffer of the set.
void Stamps::Stop() {
  if (pool_ != VK_NULL_HANDLE) vkDestroyQueryPool(device_, pool_, nullptr);
  pool_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
  buffers_.clear();
}

void Stamps::Begin(VkCommandBuffer cmd, uint32_t buffer) {
  if (pool_ == VK_NULL_HANDLE || buffer >= buffers_.size()) return;
  buffers_[buffer] = Buffer();
  vkCmdResetQueryPool(cmd, pool_, 2 * kPairs * buffer, 2 * kPairs);
}

void Stamps::Before(VkCommandBuffer cmd, uint32_t buffer, int kind, uint64_t bytes) {
  if (pool_ == VK_NULL_HANDLE || buffer >= buffers_.size()) return;
  Buffer& b = buffers_[buffer];
  if (b.open || b.count >= kPairs) return;
  vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool_,
                      2 * kPairs * buffer + 2 * b.count);
  b.kind[b.count] = kind;
  b.bytes[b.count] = bytes;
  b.open = true;
}

void Stamps::After(VkCommandBuffer cmd, uint32_t buffer) {
  if (pool_ == VK_NULL_HANDLE || buffer >= buffers_.size()) return;
  Buffer& b = buffers_[buffer];
  if (!b.open) return;
  vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool_,
                      2 * kPairs * buffer + 2 * b.count + 1);
  b.open = false;
  ++b.count;
}

void Stamps::Take(uint32_t buffer) {
  if (pool_ == VK_NULL_HANDLE || buffer >= buffers_.size()) return;
  Buffer& b = buffers_[buffer];
  for (uint32_t i = 0; i < b.count; ++i) {
    // Each query with its availability: a command buffer that did not go to
    // the GPU has none.
    uint64_t stamps[4] = {0, 0, 0, 0};
    if (vkGetQueryPoolResults(device_, pool_, 2 * kPairs * buffer + 2 * i, 2, sizeof(stamps),
                              stamps, 2 * sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) !=
            VK_SUCCESS ||
        stamps[1] == 0 || stamps[3] == 0) {
      continue;
    }
    const uint64_t ticks = (stamps[2] - stamps[0]) & mask_;
    const double ms = static_cast<double>(ticks) * ns_ * 1e-6;
    // A time over 100 ms is not the time of one copy.
    if (ms < 100.0) Add(b.kind[i], ms, b.bytes[i]);
  }
  b = Buffer();
}

}  // namespace copy_time
