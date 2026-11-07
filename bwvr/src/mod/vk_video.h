// THE LAYERS IN THE VIEW FOR VIDEOS (vk_video.cpp, docs/backend.md, "The
// view for videos"). vk_window.cpp blits the area of the left eye into the
// window, and then this file draws the quad layers of the headset over it
// (video_view.h, VideoViewDraws): one draw of a textured quad for each, with
// the blend of a color that has the alpha multiplied in (ONE,
// ONE_MINUS_SRC_ALPHA), as the runtime of the headset composes them.
//
// THE TEXTURES. A card in a session with Vulkan comes from its copy on the
// GPU (XrScreen::WantVideoLayers). A card with no such copy (a desktop run,
// a session with the relay of Direct3D 11) comes from the pixels of the
// CPU: this file copies the part that shows into an image of its own for
// each card, through a buffer of each slot. A panel of the interface is the
// panel image of the Vulkan renderer (VkScenePanelView). A wall is black
// with its alpha, from an image of one white pixel.

#pragma once

#include "video_view.h"
#include "vk_core.h"

// The slots of the draws. A slot is free again when the GPU has done the
// command buffer that used it last. vk_window.cpp uses 0 to 2 for its three
// presents in turn, and 3 for a picture to a file, which waits.
const int kVkVideoSlots = 4;

// Draws `draws` over the rect `place` (x0, y0, x1, y1 in pixels) of
// `target`, an image of width by height in `format`, in the layout
// TRANSFER_DST_OPTIMAL. `view` is a view of it for a color attachment. The
// call leaves the target in COLOR_ATTACHMENT_OPTIMAL. False when the
// pipeline cannot be made: then the target has only the barrier.
bool VkVideoCompose(VkCommandBuffer cb, int slot, VkImage target,
                    VkImageView view, VkFormat format, uint32_t width,
                    uint32_t height, const float place[4],
                    const VideoDraw* draws, int count);

// For the report of each 450 frames.
void VkVideoReport();
