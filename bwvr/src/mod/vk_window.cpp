// The window of the game with a swapchain of Vulkan (vk_window.h).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include "vk_window.h"
#include "debug_menu.h"
#include "vk_fallback.h"
#include "vk_video.h"
#include "video_view_math.h"

void ModLog(const char* format, ...);
void ControllersGesturePulse(float amplitude, float seconds);
// vk_scene.cpp: true while the Vulkan renderer runs.
bool VkSceneOn();
// vk_scene.cpp: an image to a file bwvr-vk-<what>-<frame>.bmp.
void VkSceneWriteImage(const vk::Image& image, const char* what,
                       uint64_t frame, VkImageLayout layout);

namespace {

using vk::f;

// The functions of the surface and of the swapchain. The core does not read
// them, because a device with no VK_KHR_swapchain has none.
struct WindowFunctions {
  PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
  PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
  PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
      vkGetPhysicalDeviceSurfacePresentModesKHR;
  PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
  PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
  PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
  PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
  PFN_vkQueuePresentKHR vkQueuePresentKHR;
  PFN_vkCreateSemaphore vkCreateSemaphore;
  PFN_vkDestroySemaphore vkDestroySemaphore;
};
WindowFunctions w = {};

bool g_wanted = true;    // vk_window, the value now
bool g_loaded = false;
bool g_unusable = false;  // the window takes the way through the CPU for good

const uint32_t kMaxImages = 8;
// The presents in flight. The slot of a present is free again three presents
// later, so the frame thread does not wait for its fence.
const int kSlots = 3;
struct Slot {
  VkCommandPool pool;
  VkCommandBuffer cb;
  VkFence fence;
  VkSemaphore acquired;  // the swapchain image is free for the blit
  bool pending;          // submitted, with the fence not waited for
};
Slot g_slots[kSlots] = {};
int g_slot = 0;
bool g_slots_made = false;
// THE TIME OF A PRESENT ON THE GPU: two timestamps in the command buffer of
// each slot, read when the frame thread waits for the fence of the slot.
// VK_NULL_HANDLE when the queue has no timestamps.
VkQueryPool g_times = VK_NULL_HANDLE;

HWND g_window = nullptr;
VkSurfaceKHR g_surface = VK_NULL_HANDLE;
VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
VkFormat g_format = VK_FORMAT_UNDEFINED;
VkPresentModeKHR g_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
uint32_t g_width = 0;   // the extent of the swapchain
uint32_t g_height = 0;
uint32_t g_asked_w = 0;  // the client area that the swapchain was made for
uint32_t g_asked_h = 0;
VkImage g_images[kMaxImages] = {};
// THE LAYERS IN THE VIEW FOR VIDEOS: a view of each image, when the
// swapchain can be a color attachment.
VkImageView g_views[kMaxImages] = {};
bool g_attachment = false;
bool g_said_attachment = false;
// The blit into the image is done, and the present can show it.
VkSemaphore g_drawn[kMaxImages] = {};
uint32_t g_image_count = 0;
bool g_remake = false;  // the swapchain is out of date

// For the report.
uint64_t g_presents = 0;
uint64_t g_no_image = 0;
uint64_t g_made = 0;
uint64_t g_cpu_ticks = 0;   // the calls of VkWindowPresent that presented
uint64_t g_gpu_ns = 0;      // the presents that the GPU did, read back
uint64_t g_gpu_count = 0;
uint64_t g_part_presents = 0;  // presents of a part (the view for videos)
uint64_t g_layer_presents = 0;  // presents with quad layers over the part

void Unusable(const char* what, VkResult r) {
  g_unusable = true;
  ModLog("vk window    : %s (%d). The window gets the picture through the CPU "
         "and GDI from now on", what, static_cast<int>(r));
}

bool Load() {
  if (g_loaded) return true;
  const vk::Core& core = vk::Get();
  if (!core.window_present) {
    g_unusable = true;
    ModLog("vk window    : the Vulkan device has no VK_KHR_swapchain, or the "
           "instance no VK_KHR_win32_surface. The window gets the picture "
           "through the CPU and GDI");
    return false;
  }
#define BWVR_WINDOW_INSTANCE(name)                                       \
  w.name = reinterpret_cast<PFN_##name>(                                 \
      f.vkGetInstanceProcAddr(core.instance, #name));                    \
  if (w.name == nullptr) {                                               \
    Unusable("the instance has no " #name, VK_ERROR_EXTENSION_NOT_PRESENT); \
    return false;                                                        \
  }
#define BWVR_WINDOW_DEVICE(name)                                         \
  w.name = reinterpret_cast<PFN_##name>(                                 \
      f.vkGetDeviceProcAddr(core.device, #name));                        \
  if (w.name == nullptr) {                                               \
    Unusable("the device has no " #name, VK_ERROR_EXTENSION_NOT_PRESENT); \
    return false;                                                        \
  }
  BWVR_WINDOW_INSTANCE(vkCreateWin32SurfaceKHR)
  BWVR_WINDOW_INSTANCE(vkDestroySurfaceKHR)
  BWVR_WINDOW_INSTANCE(vkGetPhysicalDeviceSurfaceSupportKHR)
  BWVR_WINDOW_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
  BWVR_WINDOW_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR)
  BWVR_WINDOW_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR)
  BWVR_WINDOW_DEVICE(vkCreateSwapchainKHR)
  BWVR_WINDOW_DEVICE(vkDestroySwapchainKHR)
  BWVR_WINDOW_DEVICE(vkGetSwapchainImagesKHR)
  BWVR_WINDOW_DEVICE(vkAcquireNextImageKHR)
  BWVR_WINDOW_DEVICE(vkQueuePresentKHR)
  BWVR_WINDOW_DEVICE(vkCreateSemaphore)
  BWVR_WINDOW_DEVICE(vkDestroySemaphore)
#undef BWVR_WINDOW_INSTANCE
#undef BWVR_WINDOW_DEVICE
  g_loaded = true;
  return true;
}

VkSemaphore MakeSemaphore() {
  VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkSemaphore semaphore = VK_NULL_HANDLE;
  w.vkCreateSemaphore(vk::Get().device, &info, nullptr, &semaphore);
  return semaphore;
}

bool MakeSlots() {
  const vk::Core& core = vk::Get();
  for (int i = 0; i < kSlots; ++i) {
    Slot& s = g_slots[i];
    VkCommandPoolCreateInfo pool = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.queueFamilyIndex = core.queue_family;
    if (f.vkCreateCommandPool(core.device, &pool, nullptr, &s.pool) !=
        VK_SUCCESS) {
      return false;
    }
    VkCommandBufferAllocateInfo cb_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cb_info.commandPool = s.pool;
    cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_info.commandBufferCount = 1;
    if (f.vkAllocateCommandBuffers(core.device, &cb_info, &s.cb) !=
        VK_SUCCESS) {
      return false;
    }
    VkFenceCreateInfo fence = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (f.vkCreateFence(core.device, &fence, nullptr, &s.fence) !=
        VK_SUCCESS) {
      return false;
    }
    s.acquired = MakeSemaphore();
    if (s.acquired == VK_NULL_HANDLE) return false;
    s.pending = false;
  }
  if (core.timestamp_ns > 0.0f && core.timestamp_bits > 0) {
    VkQueryPoolCreateInfo times = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    times.queryType = VK_QUERY_TYPE_TIMESTAMP;
    times.queryCount = 2 * kSlots;
    if (f.vkCreateQueryPool(core.device, &times, nullptr, &g_times) !=
        VK_SUCCESS) {
      g_times = VK_NULL_HANDLE;
    }
  }
  g_slot = 0;
  g_slots_made = true;
  return true;
}

// The time of the present of slot i on the GPU, after the wait for its fence.
void ReadTimes(int i) {
  if (g_times == VK_NULL_HANDLE) return;
  uint64_t t[2] = {0, 0};
  if (f.vkGetQueryPoolResults(vk::Get().device, g_times,
                              static_cast<uint32_t>(2 * i), 2, sizeof(t), t,
                              sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) !=
      VK_SUCCESS) {
    return;
  }
  const uint32_t bits = vk::Get().timestamp_bits;
  const uint64_t mask = bits >= 64 ? ~0ull : ((1ull << bits) - 1ull);
  const uint64_t ticks = (t[1] - t[0]) & mask;
  g_gpu_ns += static_cast<uint64_t>(static_cast<double>(ticks) *
                                    vk::Get().timestamp_ns);
  ++g_gpu_count;
}

// Waits until the GPU has done each present in flight.
void WaitSlots() {
  if (!g_slots_made) return;
  const VkDevice device = vk::Get().device;
  for (int i = 0; i < kSlots; ++i) {
    if (!g_slots[i].pending) continue;
    f.vkWaitForFences(device, 1, &g_slots[i].fence, VK_TRUE, UINT64_MAX);
    g_slots[i].pending = false;
    ReadTimes(i);
  }
}

// The swapchain goes, when the GPU and the present engine have done each
// present of it.
void DropSwapchain() {
  if (g_swapchain == VK_NULL_HANDLE) return;
  const VkDevice device = vk::Get().device;
  WaitSlots();
  // The present engine can still read an image of a present, so the queue
  // becomes idle first. This comes only at a change of the window.
  vk::LockQueue();
  f.vkQueueWaitIdle(vk::Get().queue);
  vk::UnlockQueue();
  for (uint32_t i = 0; i < kMaxImages; ++i) {
    if (g_views[i] != VK_NULL_HANDLE) {
      f.vkDestroyImageView(device, g_views[i], nullptr);
      g_views[i] = VK_NULL_HANDLE;
    }
  }
  w.vkDestroySwapchainKHR(device, g_swapchain, nullptr);
  g_swapchain = VK_NULL_HANDLE;
  g_image_count = 0;
}

void DropSurface() {
  DropSwapchain();
  if (g_surface != VK_NULL_HANDLE) {
    w.vkDestroySurfaceKHR(vk::Get().instance, g_surface, nullptr);
    g_surface = VK_NULL_HANDLE;
  }
  g_window = nullptr;
}

bool MakeSurface(HWND window) {
  const vk::Core& core = vk::Get();
  VkWin32SurfaceCreateInfoKHR info = {
      VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
  info.hinstance = GetModuleHandleA(nullptr);
  info.hwnd = window;
  VkResult r =
      w.vkCreateWin32SurfaceKHR(core.instance, &info, nullptr, &g_surface);
  if (r != VK_SUCCESS) {
    g_surface = VK_NULL_HANDLE;
    Unusable("vkCreateWin32SurfaceKHR failed", r);
    return false;
  }
  VkBool32 supported = VK_FALSE;
  w.vkGetPhysicalDeviceSurfaceSupportKHR(core.physical, core.queue_family,
                                         g_surface, &supported);
  if (!supported) {
    w.vkDestroySurfaceKHR(core.instance, g_surface, nullptr);
    g_surface = VK_NULL_HANDLE;
    Unusable("the queue of the renderer cannot present to the window",
             VK_ERROR_FEATURE_NOT_PRESENT);
    return false;
  }
  g_window = window;
  return true;
}

// 1: a swapchain for the client area of `width` by `height`. 0: no swapchain
// now (a window with no area). -1: the window takes the way through the CPU.
int MakeSwapchain(uint32_t width, uint32_t height) {
  const vk::Core& core = vk::Get();
  VkSurfaceCapabilitiesKHR caps;
  VkResult r = w.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(core.physical,
                                                           g_surface, &caps);
  if (r != VK_SUCCESS) {
    Unusable("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed", r);
    return -1;
  }
  VkExtent2D extent = caps.currentExtent;
  if (extent.width == 0xFFFFFFFFu) {
    extent.width = width;
    extent.height = height;
  }
  if (extent.width == 0 || extent.height == 0) return 0;
  if ((caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
    Unusable("the swapchain of the window cannot take a blit",
             VK_ERROR_FEATURE_NOT_PRESENT);
    return -1;
  }

  // 8 bits UNORM: the picture of the game has its gamma already, and a
  // blit into a format with sRGB would change it.
  VkSurfaceFormatKHR formats[64];
  uint32_t format_count = 64;
  w.vkGetPhysicalDeviceSurfaceFormatsKHR(core.physical, g_surface,
                                         &format_count, formats);
  VkSurfaceFormatKHR chosen = {VK_FORMAT_UNDEFINED,
                               VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  const VkFormat wanted[2] = {VK_FORMAT_B8G8R8A8_UNORM,
                              VK_FORMAT_R8G8B8A8_UNORM};
  for (int k = 0; k < 2 && chosen.format == VK_FORMAT_UNDEFINED; ++k) {
    VkFormatProperties properties;
    f.vkGetPhysicalDeviceFormatProperties(core.physical, wanted[k],
                                          &properties);
    if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) ==
        0) {
      continue;
    }
    for (uint32_t i = 0; i < format_count && i < 64; ++i) {
      if (formats[i].format == wanted[k] &&
          formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        chosen = formats[i];
        break;
      }
    }
  }
  if (chosen.format == VK_FORMAT_UNDEFINED) {
    Unusable("the window has no swapchain format of 8 bits UNORM",
             VK_ERROR_FORMAT_NOT_SUPPORTED);
    return -1;
  }

  // The frame rate of the game is the rate of the headset, so a present
  // must not wait for the monitor. FIFO waits, so it is not used.
  VkPresentModeKHR modes[16];
  uint32_t mode_count = 16;
  w.vkGetPhysicalDeviceSurfacePresentModesKHR(core.physical, g_surface,
                                              &mode_count, modes);
  bool mailbox = false, immediate = false;
  for (uint32_t i = 0; i < mode_count && i < 16; ++i) {
    if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) mailbox = true;
    if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) immediate = true;
  }
  if (!mailbox && !immediate) {
    Unusable("the window has no present with no wait for the monitor "
             "(mailbox or immediate)",
             VK_ERROR_FEATURE_NOT_PRESENT);
    return -1;
  }
  const VkPresentModeKHR mode =
      mailbox ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;

  uint32_t count = caps.minImageCount + 1;
  if (count < 3) count = 3;
  if (caps.maxImageCount != 0 && count > caps.maxImageCount) {
    count = caps.maxImageCount;
  }
  if (count > kMaxImages) count = kMaxImages;
  VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if ((caps.supportedCompositeAlpha & alpha) == 0) {
    const VkCompositeAlphaFlagBitsKHR others[3] = {
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
    for (int i = 0; i < 3; ++i) {
      if ((caps.supportedCompositeAlpha & others[i]) != 0) {
        alpha = others[i];
        break;
      }
    }
  }

  DropSwapchain();
  VkSwapchainCreateInfoKHR info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  info.surface = g_surface;
  info.minImageCount = count;
  info.imageFormat = chosen.format;
  info.imageColorSpace = chosen.colorSpace;
  info.imageExtent = extent;
  info.imageArrayLayers = 1;
  // A color attachment too, for the quad layers in the view for videos.
  const bool attachment =
      (caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0;
  info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    (attachment ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0u);
  info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.preTransform = caps.currentTransform;
  info.compositeAlpha = alpha;
  info.presentMode = mode;
  info.clipped = VK_TRUE;
  r = w.vkCreateSwapchainKHR(core.device, &info, nullptr, &g_swapchain);
  if (r != VK_SUCCESS) {
    g_swapchain = VK_NULL_HANDLE;
    Unusable("vkCreateSwapchainKHR failed", r);
    return -1;
  }
  g_image_count = kMaxImages;
  w.vkGetSwapchainImagesKHR(core.device, g_swapchain, &g_image_count,
                            g_images);
  for (uint32_t i = 0; i < g_image_count; ++i) {
    if (g_drawn[i] == VK_NULL_HANDLE) g_drawn[i] = MakeSemaphore();
    if (g_drawn[i] == VK_NULL_HANDLE) {
      Unusable("vkCreateSemaphore failed", VK_ERROR_OUT_OF_HOST_MEMORY);
      return -1;
    }
  }
  g_attachment = attachment;
  for (uint32_t i = 0; i < g_image_count && attachment; ++i) {
    VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = g_images[i];
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = chosen.format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (f.vkCreateImageView(core.device, &view, nullptr, &g_views[i]) !=
        VK_SUCCESS) {
      g_views[i] = VK_NULL_HANDLE;
      g_attachment = false;
    }
  }
  if (!g_attachment && !g_said_attachment) {
    g_said_attachment = true;
    ModLog("vk window    : the swapchain of the window cannot be a color "
           "attachment, so the view for videos has no quad layers");
  }
  g_format = chosen.format;
  g_mode = mode;
  g_width = extent.width;
  g_height = extent.height;
  g_asked_w = width;
  g_asked_h = height;
  g_remake = false;
  if (g_made++ < 8) {
    ModLog("vk window    : a swapchain of Vulkan on the window of the game, "
           "%u by %u, %u images, format %d, %s",
           g_width, g_height, g_image_count, static_cast<int>(g_format),
           mode == VK_PRESENT_MODE_MAILBOX_KHR ? "mailbox" : "immediate");
  }
  return 1;
}

// The pixels of `part` of the picture (x0, y0, x1, y1 from 0 to 1), kept
// inside it. All of it with no part or with a part of no size.
void SourceRect(const vk::Image& source, const float* part, int32_t out[4]) {
  const int32_t width = static_cast<int32_t>(source.width);
  const int32_t height = static_cast<int32_t>(source.height);
  out[0] = out[1] = 0;
  out[2] = width;
  out[3] = height;
  if (part == nullptr) return;
  const float sw = static_cast<float>(source.width);
  const float sh = static_cast<float>(source.height);
  int32_t x0 = static_cast<int32_t>(part[0] * sw + 0.5f);
  int32_t y0 = static_cast<int32_t>(part[1] * sh + 0.5f);
  int32_t x1 = static_cast<int32_t>(part[2] * sw + 0.5f);
  int32_t y1 = static_cast<int32_t>(part[3] * sh + 0.5f);
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > width) x1 = width;
  if (y1 > height) y1 = height;
  if (x1 <= x0 || y1 <= y0) return;
  out[0] = x0;
  out[1] = y0;
  out[2] = x1;
  out[3] = y1;
}

}  // namespace

void VkWindowSetOptions(int on) {
  g_wanted = on != 0;
  ModLog("vk window    : vk_window=%d. %s", g_wanted ? 1 : 0,
         g_wanted ? "The window of the game gets the picture through a "
                    "swapchain of Vulkan, with no copy to the CPU"
                  : "The window of the game gets the picture through the "
                    "CPU and GDI");
}

void VkWindowToggle(const char* keys) {
  g_wanted = !g_wanted;
  ControllersGesturePulse(0.5f, g_wanted ? 0.30f : 0.08f);
  ModLog("test key     : %s, vk_window is %d from the next present: the "
         "window gets the picture %s",
         keys, g_wanted ? 1 : 0,
         g_wanted ? "through a swapchain of Vulkan" : "through the CPU and GDI");
}

// The value of vk_window for the debug page. With no renderer, or a window
// that cannot have a swapchain, the switch changes nothing.
int VkWindowValue() {
  if (g_unusable || !VkSceneOn()) return kDebugCannot;
  return g_wanted ? 1 : 0;
}

bool VkWindowWanted() { return g_wanted && !g_unusable; }

bool VkWindowPresent(HWND window, const vk::Image& source, uint32_t width,
                     uint32_t height, const float* part, float aspect,
                     const VideoDraw* draws, int draw_count) {
  if (!g_wanted || g_unusable || window == nullptr || width == 0 ||
      height == 0) {
    return false;
  }
  if (!vk::Started() || source.image == VK_NULL_HANDLE) return false;
  if (!Load()) return false;
  if (!g_slots_made && !MakeSlots()) {
    Unusable("the command buffers of the window could not be made",
             VK_ERROR_OUT_OF_DEVICE_MEMORY);
    return false;
  }
  if (window != g_window) DropSurface();
  if (g_surface == VK_NULL_HANDLE && !MakeSurface(window)) return false;
  if (g_swapchain == VK_NULL_HANDLE || g_remake || width != g_asked_w ||
      height != g_asked_h) {
    const int made = MakeSwapchain(width, height);
    if (made < 0) return false;
    if (made == 0) return true;
  }

  LARGE_INTEGER start;
  QueryPerformanceCounter(&start);
  const vk::Core& core = vk::Get();
  Slot& s = g_slots[g_slot];
  // The slot of three presents ago: the GPU has done it, or nearly.
  if (s.pending) {
    f.vkWaitForFences(core.device, 1, &s.fence, VK_TRUE, UINT64_MAX);
    s.pending = false;
    ReadTimes(g_slot);
  }
  // No wait for an image: with none free the window keeps its picture.
  uint32_t index = 0;
  VkResult r = w.vkAcquireNextImageKHR(core.device, g_swapchain, 0,
                                       s.acquired, VK_NULL_HANDLE, &index);
  if (r == VK_NOT_READY || r == VK_TIMEOUT) {
    ++g_no_image;
    return true;
  }
  if (r == VK_ERROR_OUT_OF_DATE_KHR) {
    g_remake = true;
    ++g_no_image;
    return true;
  }
  if (r == VK_ERROR_SURFACE_LOST_KHR) {
    DropSurface();
    ++g_no_image;
    return true;
  }
  if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
    Unusable("vkAcquireNextImageKHR failed", r);
    return false;
  }
  if (r == VK_SUBOPTIMAL_KHR) g_remake = true;
  if (index >= g_image_count) {
    Unusable("vkAcquireNextImageKHR gave an image that the swapchain does "
             "not have", VK_ERROR_UNKNOWN);
    return false;
  }

  f.vkResetCommandPool(core.device, s.pool, 0);
  VkCommandBuffer cb = s.cb;
  VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  f.vkBeginCommandBuffer(cb, &begin);
  const uint32_t query = static_cast<uint32_t>(2 * g_slot);
  if (g_times != VK_NULL_HANDLE) {
    f.vkCmdResetQueryPool(cb, g_times, query, 2);
    f.vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, g_times,
                           query);
  }
  vk::Barrier(cb, source.image, VK_IMAGE_ASPECT_COLOR_BIT,
              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, source.layers);
  vk::Barrier(cb, g_images[index], VK_IMAGE_ASPECT_COLOR_BIT,
              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              1);
  // The part of the picture, and its place in the window (THE VIEW FOR
  // VIDEOS). With no part and no aspect: all of it on all of the window.
  int32_t from[4];
  SourceRect(source, part, from);
  const int32_t sx0 = from[0], sy0 = from[1], sx1 = from[2], sy1 = from[3];
  video_view_math::Rect place = {0.0f, 0.0f, static_cast<float>(g_width),
                                 static_cast<float>(g_height)};
  if (aspect != 0.0f) {
    const float shape =
        aspect > 0.0f ? aspect
                      : static_cast<float>(sx1 - sx0) /
                            static_cast<float>(sy1 - sy0);
    place = video_view_math::Fit(static_cast<float>(g_width),
                                 static_cast<float>(g_height), shape);
  }
  const int32_t dx0 = static_cast<int32_t>(place.x0);
  const int32_t dy0 = static_cast<int32_t>(place.y0);
  const int32_t dx1 = static_cast<int32_t>(place.x1);
  const int32_t dy1 = static_cast<int32_t>(place.y1);
  if (dx0 > 0 || dy0 > 0 || dx1 < static_cast<int32_t>(g_width) ||
      dy1 < static_cast<int32_t>(g_height)) {
    // Black around the picture. The blit comes after the clear.
    VkClearColorValue black = {};
    black.float32[3] = 1.0f;
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    f.vkCmdClearColorImage(cb, g_images[index],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1,
                           &range);
    vk::Barrier(cb, g_images[index], VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
  }
  if (part != nullptr || aspect != 0.0f) ++g_part_presents;
  VkImageBlit blit = {};
  blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  blit.srcOffsets[0] = {sx0, sy0, 0};
  blit.srcOffsets[1] = {sx1, sy1, 1};
  blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  blit.dstOffsets[0] = {dx0, dy0, 0};
  blit.dstOffsets[1] = {dx1, dy1, 1};
  f.vkCmdBlitImage(cb, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   g_images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                   &blit, VK_FILTER_LINEAR);
  // THE LAYERS IN THE VIEW FOR VIDEOS over the area (vk_video.cpp).
  const float area[4] = {static_cast<float>(dx0), static_cast<float>(dy0),
                         static_cast<float>(dx1), static_cast<float>(dy1)};
  if (draws != nullptr && draw_count > 0 && g_attachment &&
      g_views[index] != VK_NULL_HANDLE) {
    VkVideoCompose(cb, g_slot, g_images[index], g_views[index], g_format,
                   g_width, g_height, area, draws, draw_count);
    vk::Barrier(cb, g_images[index], VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1);
    ++g_layer_presents;
  } else {
    vk::Barrier(cb, g_images[index], VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1);
  }
  vk::Barrier(cb, source.image, VK_IMAGE_ASPECT_COLOR_BIT,
              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, source.layers);
  if (g_times != VK_NULL_HANDLE) {
    f.vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, g_times,
                           query + 1);
  }
  f.vkEndCommandBuffer(cb);

  VkSemaphoreSubmitInfo wait = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
  wait.semaphore = s.acquired;
  wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkSemaphoreSubmitInfo signal = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
  signal.semaphore = g_drawn[index];
  signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkCommandBufferSubmitInfo cb_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  cb_info.commandBuffer = cb;
  VkSubmitInfo2 submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submit.waitSemaphoreInfoCount = 1;
  submit.pWaitSemaphoreInfos = &wait;
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &cb_info;
  submit.signalSemaphoreInfoCount = 1;
  submit.pSignalSemaphoreInfos = &signal;
  f.vkResetFences(core.device, 1, &s.fence);

  VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &g_drawn[index];
  present.swapchainCount = 1;
  present.pSwapchains = &g_swapchain;
  present.pImageIndices = &index;
  // The runtime of the headset uses the queue too.
  vk::LockQueue();
  r = f.vkQueueSubmit2(core.queue, 1, &submit, s.fence);
  VkResult shown = VK_ERROR_UNKNOWN;
  if (r == VK_SUCCESS) shown = w.vkQueuePresentKHR(core.queue, &present);
  vk::UnlockQueue();
  if (r != VK_SUCCESS) {
    Unusable("a submit of the window failed", r);
    return false;
  }
  s.pending = true;
  g_slot = (g_slot + 1) % kSlots;
  if (shown == VK_ERROR_OUT_OF_DATE_KHR || shown == VK_SUBOPTIMAL_KHR) {
    g_remake = true;
  } else if (shown == VK_ERROR_SURFACE_LOST_KHR) {
    DropSurface();
  } else if (shown != VK_SUCCESS) {
    Unusable("vkQueuePresentKHR failed", shown);
    return true;
  }
  ++g_presents;
  LARGE_INTEGER end;
  QueryPerformanceCounter(&end);
  g_cpu_ticks += static_cast<uint64_t>(end.QuadPart - start.QuadPart);
  VkFallbackPicture("the window, through the swapchain");
  return true;
}

void VkWindowPicture(const vk::Image& source, const float part[4],
                     const VideoDraw* draws, int draw_count, uint64_t frame) {
  if (!vk::Started() || source.image == VK_NULL_HANDLE) return;
  int32_t s[4];
  SourceRect(source, part, s);
  const uint32_t pw = static_cast<uint32_t>(s[2] - s[0]);
  const uint32_t ph = static_cast<uint32_t>(s[3] - s[1]);
  // The format of the window, so the draws use the same pipeline.
  const VkFormat format =
      g_format != VK_FORMAT_UNDEFINED ? g_format : vk::Get().color_format;
  vk::Image image = {};
  if (!vk::MakeImage(pw, ph, 1, format,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, &image)) {
    return;
  }
  VkCommandBuffer cb = vk::BeginOnce();
  vk::Barrier(cb, source.image, VK_IMAGE_ASPECT_COLOR_BIT,
              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, source.layers);
  vk::Barrier(cb, image.image, VK_IMAGE_ASPECT_COLOR_BIT,
              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              1);
  VkImageBlit blit = {};
  blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  blit.srcOffsets[0] = {s[0], s[1], 0};
  blit.srcOffsets[1] = {s[2], s[3], 1};
  blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  blit.dstOffsets[0] = {0, 0, 0};
  blit.dstOffsets[1] = {static_cast<int32_t>(pw), static_cast<int32_t>(ph), 1};
  f.vkCmdBlitImage(cb, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_NEAREST);
  vk::Barrier(cb, source.image, VK_IMAGE_ASPECT_COLOR_BIT,
              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, source.layers);
  const float area[4] = {0.0f, 0.0f, static_cast<float>(pw),
                         static_cast<float>(ph)};
  if (draws != nullptr && draw_count > 0) {
    VkVideoCompose(cb, kVkVideoSlots - 1, image.image, image.view, format,
                   pw, ph, area, draws, draw_count);
  } else {
    vk::Barrier(cb, image.image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);
  }
  if (vk::EndOnce(cb)) {
    ModLog("vk window    : the picture of the video of frame %llu has %d "
           "quad layers", frame, draw_count);
    VkSceneWriteImage(image, "video", frame,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }
  vk::FreeImage(&image);
}

void VkWindowStop() {
  if (!g_loaded) return;
  DropSurface();
}

void VkWindowReport() {
  if (g_presents == 0 && g_no_image == 0) return;
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  const double cpu_ms =
      g_presents > 0 && frequency.QuadPart > 0
          ? 1000.0 * static_cast<double>(g_cpu_ticks) /
                static_cast<double>(frequency.QuadPart) /
                static_cast<double>(g_presents)
          : 0.0;
  const double gpu_ms =
      g_gpu_count > 0 ? static_cast<double>(g_gpu_ns) * 1e-6 /
                            static_cast<double>(g_gpu_count)
                      : -1.0;
  ModLog("vk window    : %llu presents through the swapchain of the window "
         "(%llu of a part, for the view for videos, %llu with quad layers), "
         "%llu with no free image, %llu swapchains made so far. A present "
         "takes %.3f ms on the frame thread and %.3f ms on the GPU (-1: no "
         "timestamps)",
         g_presents, g_part_presents, g_layer_presents, g_no_image, g_made,
         cpu_ms, gpu_ms);
  VkVideoReport();
  g_presents = 0;
  g_no_image = 0;
  g_part_presents = 0;
  g_layer_presents = 0;
  g_cpu_ticks = 0;
  g_gpu_ns = 0;
  g_gpu_count = 0;
}
