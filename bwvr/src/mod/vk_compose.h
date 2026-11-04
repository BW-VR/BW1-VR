// THE LATE COMPOSE of the eye picture (vk_compose.cpp, docs/backend.md, "The
// compose of the eye picture"). With fovea_res=1 and fovea_res_layer=0 the
// eye picture of a walk is made from the outer image and the centre image.
// The renderer can leave that step out of the walk, and the headset app
// records it later into its own command buffer, with the swapchain image of
// each eye as the target. So the picture goes to the swapchain in one pass,
// with no eye picture of the renderer and no copy of 48 MB.
//
// The module has its own shaders, its own pipelines and its own descriptor
// sets, and it uses the functions of the device through
// vkGetDeviceProcAddr, so a table that the app wraps (WalkProbeInstall) does
// not see its commands.
#pragma once

#include <stdint.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "vk_compose_math.h"

namespace vk_compose {

// What the compose needs from a walk.
struct Input {
  vk_compose_math::Layout layout;
  VkImageView outer[2];   // a 2D view of each eye layer of the outer image
  VkImageView center[2];  // and of the centre image
  uint64_t generation;    // changes with the views
  bool tint;              // the marks of the test (foveation_show): the outer image is blue
};

// A walk left its eye picture to the compose: `eye_image` is the image of
// the walk (two layers). uncomposed false: the walk composed it itself, and
// the input is only there for the test (CompareWithTarget).
void Set(const Input& input, uint64_t eye_image, bool uncomposed);
// True when the last walk left the eye picture `eye_image` to the compose,
// and nothing composed it yet into that image.
bool Pending(uint64_t eye_image);
// The eye image of the last walk that left its picture to the compose.
uint64_t Pending();
// The walk writes this eye image now (or the images of the compose are made
// again): nothing is pending for it any more.
void Drop(uint64_t eye_image);
void DropAll();
// True when the module works: the device has the functions, the shaders, the
// layouts and the pipeline of the format of the renderer. Makes them at the
// first call. The thread that records calls it.
bool Usable();
// The walk composed into the eye image itself (Record with the layers of
// that image): the picture is not pending any more.
void MarkComposed(uint64_t eye_image);

// One target layer of the compose: a layer of an image, and the format of the
// view that the pass uses. An sRGB format gets the bytes decoded in the
// shader, so the stored bytes are the bytes of the game.
struct Target {
  VkImage image;
  uint32_t layer;
  VkFormat format;
};

// Records the compose of the pending input into the command buffer: one pass
// for each eye, into targets[0] and targets[1], whole image, no load, in the
// layout of a color attachment at the end. `slot` (0 to 7) is the place of
// the vertices of the compose: the caller keeps it free until the GPU
// finished the command buffer. False when nothing is pending or the module
// cannot work (then it records nothing).
bool Record(VkCommandBuffer cb, uint32_t slot, const Target targets[2]);

// A swapchain image goes (or an image of the renderer is made again): the
// views of the module for it go.
void Forget(VkImage image);
// The views, the pipelines and the buffers go. The device must be idle.
void Stop();

// The counts since the start.
struct Counts {
  uint64_t records = 0;
  uint64_t no_pending = 0;
};
Counts Taken();

// A TEST AID (vk_compose_test=1, desktop runs): the compose of the input of
// the last walk into images of sRGB and of UNORM format, compared byte for
// byte with the eye image that the walk made itself. One line of the log for
// each format. The walk must have ended, and `eye_image` must be in the
// layout of a color attachment.
void CompareWithTarget(VkImage eye_image, uint32_t width, uint32_t height);

}  // namespace vk_compose
