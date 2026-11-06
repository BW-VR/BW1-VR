// The window of the game with a swapchain of Vulkan (vk_window.cpp,
// docs/backend.md, "The window with Vulkan").
//
// The present of the DirectDraw of the mod (vk_ddraw.cpp) puts the picture
// of the last walk into the window of the game. With vk_window=1 a swapchain
// of Vulkan on that window takes it: a blit with a linear filter on the GPU
// to the size of the client area, and a present in the queue of the
// renderer. The picture does not go through the CPU, and the frame thread
// does not wait for the GPU. With vk_window=0, or when the window cannot
// have a swapchain, the picture goes through the CPU and GDI
// (VkScenePresentPixels).
//
// The swapchain needs VK_KHR_surface and VK_KHR_win32_surface on the
// instance and VK_KHR_swapchain on the device. vk_core.cpp and the OpenXR
// session (xr_screen.cpp) turn them on when the GPU has them
// (vk::Core::window_present).

#pragma once

#include <windows.h>

#include "video_view.h"
#include "vk_core.h"

// vk_window of bwvr.ini: 1 (the default) a swapchain of Vulkan. 0: the way
// through the CPU and GDI. Shift D (test_keys=1) changes it from the next
// present.
void VkWindowSetOptions(int on);
void VkWindowToggle(const char* keys);
bool VkWindowWanted();
// The value of vk_window for the debug page, or kDebugCannot.
int VkWindowValue();

// Blits layer 0 of `source` (in the layout of a color attachment, which it
// keeps) to the client area of `window`, `width` by `height` pixels, and
// presents it. False when the picture did not go to the window: then the
// caller takes the way through the CPU. True also for a present that the
// swapchain had no image for: the window keeps the picture before.
//
// THE VIEW FOR VIDEOS (video_view.h). `part`: null for the whole picture,
// else the part of it (x0, y0, x1, y1 from 0 to 1). `aspect`: 0 for the
// whole client area. Above 0: the largest rect of that shape (width over
// height) in the middle of the client area, with black around it. Below 0:
// the same with the shape of the part. `draws`: the quad layers of the
// headset over the part (video_view.h, VideoViewDraws), drawn with
// vk_video.cpp when the swapchain can be a color attachment.
bool VkWindowPresent(HWND window, const vk::Image& source, uint32_t width,
                     uint32_t height, const float* part = nullptr,
                     float aspect = 0.0f, const VideoDraw* draws = nullptr,
                     int draw_count = 0);

// THE PICTURE OF THE VIDEO: the part of layer 0 of `source` at the pixels of
// the source, with the draws over it, goes to the file
// bwvr-vk-video-<frame>.bmp beside the game. It waits for the GPU, so it is
// for a frame that saves pictures only.
void VkWindowPicture(const vk::Image& source, const float part[4],
                     const VideoDraw* draws, int draw_count, uint64_t frame);

// The swapchain and the surface go, and GDI can draw into the window again.
// A later present makes them again.
void VkWindowStop();

// For the report of each 450 frames.
void VkWindowReport();
