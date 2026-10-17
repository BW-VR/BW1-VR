// THE MENU ON A FLAT SCREEN (docs/android.md, "With no headset"). With no
// OpenXR runtime, for example in the Android emulator, the app shows the
// menu in its window with a swapchain of Vulkan. The keys of a keyboard or
// a game pad and a touch on the screen drive the menu.
#pragma once

#include <android/native_window.h>
#include <stdint.h>

#include <vector>

#include "gpu.h"

class FlatView {
 public:
  bool Start(ANativeWindow* window, uint32_t menu_width, uint32_t menu_height);
  void Stop();
  bool Ready() const { return swapchain_ != VK_NULL_HANDLE; }

  // New pixels of the menu (RGBA, with the color multiplied by the alpha).
  void SetMenuPixels(const std::vector<uint8_t>& rgba);
  // Shows the menu in the middle of the window at the largest size that
  // fits. False when the swapchain is out of date.
  bool Present();

  // The part of the window that the bars of the system do not cover
  // (android_app::contentRect). An empty rect is the whole window.
  void SetContentRect(int left, int top, int right, int bottom);

  // The rect of the window that shows the menu, for a touch.
  void MenuRect(float* x, float* y, float* width, float* height) const;

 private:
  bool MakeSwapchain();
  void DropSwapchain();

  Gpu gpu_;
  ANativeWindow* window_ = nullptr;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D extent_ = {0, 0};
  std::vector<VkImage> images_;
  VkSemaphore acquired_ = VK_NULL_HANDLE;
  // One for each image: a present can still wait on the semaphore of its
  // image when the next present comes.
  std::vector<VkSemaphore> drawn_;

  uint32_t menu_width_ = 0;
  uint32_t menu_height_ = 0;
  VkImage menu_image_ = VK_NULL_HANDLE;
  VkDeviceMemory menu_memory_ = VK_NULL_HANDLE;
  bool menu_new_ = false;
  bool menu_ready_ = false;
  Staging staging_;
  int content_[4] = {0, 0, 0, 0};
};
