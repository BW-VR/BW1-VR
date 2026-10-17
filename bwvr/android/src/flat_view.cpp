#include "flat_view.h"

#include <string.h>

#include <vulkan/vulkan_android.h>

#include "app_log.h"

namespace {

bool IsSrgb(VkFormat f) {
  return f == VK_FORMAT_R8G8B8A8_SRGB || f == VK_FORMAT_B8G8R8A8_SRGB;
}

}  // namespace

bool FlatView::Start(ANativeWindow* window, uint32_t menu_width,
                     uint32_t menu_height) {
  window_ = window;
  menu_width_ = menu_width;
  menu_height_ = menu_height;

  if (gpu_.device == VK_NULL_HANDLE) {
    const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME,
                                VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Black & White VR Mod";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = 2;
    info.ppEnabledExtensionNames = extensions;
    if (vkCreateInstance(&info, nullptr, &gpu_.instance) != VK_SUCCESS) {
      AppLog("flat view    : vkCreateInstance failed");
      return false;
    }
  }

  VkAndroidSurfaceCreateInfoKHR surface = {
      VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
  surface.window = window;
  if (vkCreateAndroidSurfaceKHR(gpu_.instance, &surface, nullptr, &surface_) !=
      VK_SUCCESS) {
    AppLog("flat view    : vkCreateAndroidSurfaceKHR failed");
    return false;
  }

  if (gpu_.device == VK_NULL_HANDLE) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(gpu_.instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(gpu_.instance, &count, devices.data());
    for (VkPhysicalDevice d : devices) {
      uint32_t families = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(d, &families, nullptr);
      std::vector<VkQueueFamilyProperties> family(families);
      vkGetPhysicalDeviceQueueFamilyProperties(d, &families, family.data());
      for (uint32_t i = 0; i < families; ++i) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface_, &present);
        if ((family[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && present) {
          gpu_.physical = d;
          gpu_.queue_family = i;
          break;
        }
      }
      if (gpu_.physical != VK_NULL_HANDLE) break;
    }
    if (gpu_.physical == VK_NULL_HANDLE) {
      AppLog("flat view    : no GPU can show a picture in the window");
      return false;
    }
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = gpu_.queue_family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue;
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = device_extensions;
    if (vkCreateDevice(gpu_.physical, &info, nullptr, &gpu_.device) != VK_SUCCESS) {
      AppLog("flat view    : vkCreateDevice failed");
      return false;
    }
    if (!GpuFinish(&gpu_)) return false;
    VkSemaphoreCreateInfo s = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(gpu_.device, &s, nullptr, &acquired_);
    if (!MakeStaging(&gpu_, static_cast<VkDeviceSize>(menu_width) * menu_height * 4,
                     &staging_)) {
      AppLog("flat view    : the staging buffer failed");
      return false;
    }
  }
  return MakeSwapchain();
}

bool FlatView::MakeSwapchain() {
  VkSurfaceCapabilitiesKHR caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu_.physical, surface_, &caps);
  uint32_t count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_.physical, surface_, &count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_.physical, surface_, &count,
                                       formats.data());
  VkSurfaceFormatKHR chosen = formats.empty()
                                  ? VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM,
                                                       VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                                  : formats[0];
  for (const VkSurfaceFormatKHR& f : formats) {
    if (f.format == VK_FORMAT_R8G8B8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_SRGB) {
      chosen = f;
      break;
    }
  }
  format_ = chosen.format;
  extent_ = caps.currentExtent;
  if (extent_.width == UINT32_MAX) {
    extent_.width = static_cast<uint32_t>(ANativeWindow_getWidth(window_));
    extent_.height = static_cast<uint32_t>(ANativeWindow_getHeight(window_));
  }
  VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if ((caps.supportedCompositeAlpha & alpha) == 0) {
    alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  }
  VkSwapchainCreateInfoKHR info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  info.surface = surface_;
  info.minImageCount = caps.minImageCount < 2 ? 2 : caps.minImageCount;
  if (caps.maxImageCount != 0 && info.minImageCount > caps.maxImageCount) {
    info.minImageCount = caps.maxImageCount;
  }
  info.imageFormat = chosen.format;
  info.imageColorSpace = chosen.colorSpace;
  info.imageExtent = extent_;
  info.imageArrayLayers = 1;
  info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.preTransform = caps.currentTransform;
  info.compositeAlpha = alpha;
  info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  info.clipped = VK_TRUE;
  if (vkCreateSwapchainKHR(gpu_.device, &info, nullptr, &swapchain_) != VK_SUCCESS) {
    AppLog("flat view    : vkCreateSwapchainKHR failed");
    swapchain_ = VK_NULL_HANDLE;
    return false;
  }
  vkGetSwapchainImagesKHR(gpu_.device, swapchain_, &count, nullptr);
  images_.resize(count);
  vkGetSwapchainImagesKHR(gpu_.device, swapchain_, &count, images_.data());
  while (drawn_.size() < count) {
    VkSemaphoreCreateInfo s = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore = VK_NULL_HANDLE;
    vkCreateSemaphore(gpu_.device, &s, nullptr, &semaphore);
    drawn_.push_back(semaphore);
  }

  // The image of the menu, with the same encoding as the swapchain, so the
  // blit does not change the colors.
  if (menu_image_ == VK_NULL_HANDLE) {
    VkImageCreateInfo image = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = IsSrgb(format_) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    image.extent = {menu_width_, menu_height_, 1};
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(gpu_.device, &image, nullptr, &menu_image_) != VK_SUCCESS) {
      return false;
    }
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(gpu_.device, menu_image_, &req);
    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < gpu_.memory.memoryTypeCount; ++i) {
      if ((req.memoryTypeBits & (1u << i)) != 0 &&
          (gpu_.memory.memoryTypes[i].propertyFlags &
           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
        type = i;
        break;
      }
    }
    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = type;
    if (type == UINT32_MAX ||
        vkAllocateMemory(gpu_.device, &alloc, nullptr, &menu_memory_) != VK_SUCCESS) {
      return false;
    }
    vkBindImageMemory(gpu_.device, menu_image_, menu_memory_, 0);
  }
  AppLog("flat view    : a swapchain of %u images, %u by %u, format %d, "
         "transform 0x%X",
         count, extent_.width, extent_.height, static_cast<int>(format_),
         static_cast<unsigned>(caps.currentTransform));
  return true;
}

void FlatView::DropSwapchain() {
  if (gpu_.device == VK_NULL_HANDLE) return;
  vkDeviceWaitIdle(gpu_.device);
  if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(gpu_.device, swapchain_, nullptr);
  swapchain_ = VK_NULL_HANDLE;
  images_.clear();
}

void FlatView::Stop() {
  DropSwapchain();
  if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(gpu_.instance, surface_, nullptr);
  surface_ = VK_NULL_HANDLE;
  window_ = nullptr;
}

void FlatView::SetMenuPixels(const std::vector<uint8_t>& rgba) {
  if (staging_.mapped == nullptr) return;
  const size_t bytes = static_cast<size_t>(menu_width_) * menu_height_ * 4;
  if (rgba.size() < bytes) return;
  vkWaitForFences(gpu_.device, 1, &gpu_.fence, VK_TRUE, UINT64_MAX);
  memcpy(staging_.mapped, rgba.data(), bytes);
  menu_new_ = true;
}

void FlatView::SetContentRect(int left, int top, int right, int bottom) {
  content_[0] = left;
  content_[1] = top;
  content_[2] = right;
  content_[3] = bottom;
}

void FlatView::MenuRect(float* x, float* y, float* width, float* height) const {
  float l = 0.0f, t = 0.0f;
  float r = static_cast<float>(extent_.width), b = static_cast<float>(extent_.height);
  if (content_[2] > content_[0] && content_[3] > content_[1]) {
    l = static_cast<float>(content_[0]);
    t = static_cast<float>(content_[1]);
    if (content_[2] < r) r = static_cast<float>(content_[2]);
    if (content_[3] < b) b = static_cast<float>(content_[3]);
  }
  const float sx = (r - l) / menu_width_;
  const float sy = (b - t) / menu_height_;
  const float s = sx < sy ? sx : sy;
  *width = menu_width_ * s;
  *height = menu_height_ * s;
  *x = l + (r - l - *width) * 0.5f;
  *y = t + (b - t - *height) * 0.5f;
}

bool FlatView::Present() {
  if (swapchain_ == VK_NULL_HANDLE) return false;
  uint32_t index = 0;
  VkResult r = vkAcquireNextImageKHR(gpu_.device, swapchain_, UINT64_MAX,
                                     acquired_, VK_NULL_HANDLE, &index);
  if (r == VK_ERROR_OUT_OF_DATE_KHR) {
    DropSwapchain();
    MakeSwapchain();
    return false;
  }
  if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) return false;

  VkCommandBuffer cmd = GpuBegin(&gpu_);
  if (menu_new_) {
    ImageBarrier(cmd, menu_image_, VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT);
    CopyToImage(cmd, staging_, menu_image_, menu_width_, menu_height_);
    ImageBarrier(cmd, menu_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    menu_new_ = false;
    menu_ready_ = true;
  }
  const VkImage target = images_[index];
  ImageBarrier(cmd, target, VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
               VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkClearColorValue black = {};
  black.float32[3] = 1.0f;
  VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdClearColorImage(cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       &black, 1, &range);
  if (menu_ready_) {
    ImageBarrier(cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    float x, y, w, h;
    MenuRect(&x, &y, &w, &h);
    VkImageBlit blit = {};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[1] = {static_cast<int32_t>(menu_width_),
                          static_cast<int32_t>(menu_height_), 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstOffsets[0] = {static_cast<int32_t>(x), static_cast<int32_t>(y), 0};
    blit.dstOffsets[1] = {static_cast<int32_t>(x + w), static_cast<int32_t>(y + h), 1};
    vkCmdBlitImage(cmd, menu_image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);
  }
  ImageBarrier(cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_WRITE_BIT, 0,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
  if (!GpuSubmit(&gpu_, acquired_, drawn_[index])) return false;

  VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &drawn_[index];
  present.swapchainCount = 1;
  present.pSwapchains = &swapchain_;
  present.pImageIndices = &index;
  r = vkQueuePresentKHR(gpu_.queue, &present);
  if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
    DropSwapchain();
    MakeSwapchain();
  }
  return r == VK_SUCCESS;
}
