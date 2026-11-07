// The layers in the view for videos (vk_video.h).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

#include "video_layers_math.h"
#include "vk_video.h"
#include "vk_video_frag.h"
#include "vk_video_vert.h"

void ModLog(const char* format, ...);
// vk_scene.cpp: the view of the panel image `image` (a VkImage), or
// VK_NULL_HANDLE when it is not a panel image of the renderer.
VkImageView VkScenePanelView(uint64_t image);

namespace {

using vk::f;

// The push constants of one quad (vk_video.vert).
struct Quad {
  float corner[4][4];
  float uv[4];
  float color[4];
};
static_assert(sizeof(Quad) == 96, "the push constants are 96 bytes");

bool g_made = false;
bool g_failed = false;
VkDescriptorSetLayout g_set_layout = VK_NULL_HANDLE;
VkPipelineLayout g_layout = VK_NULL_HANDLE;
VkShaderModule g_vert = VK_NULL_HANDLE;
VkShaderModule g_frag = VK_NULL_HANDLE;
VkSampler g_sampler = VK_NULL_HANDLE;
VkDescriptorPool g_pool = VK_NULL_HANDLE;
VkDescriptorSet g_sets[kVkVideoSlots][kVideoDraws] = {};
// A pipeline for each format of a target (BGRA and RGBA).
const int kFormats = 2;
VkFormat g_pipeline_format[kFormats] = {VK_FORMAT_UNDEFINED,
                                        VK_FORMAT_UNDEFINED};
VkPipeline g_pipelines[kFormats] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
// One white pixel, for a quad with a color only.
vk::Image g_white = {};
bool g_white_ready = false;
// THE PIXELS OF THE CPU: an image for each card, and a buffer for each
// slot. Each image keeps what its last copy came from: the pixels, the
// number of the picture and the part. A draw of the same picture uses the
// image with no copy.
const int kCards = 8;
vk::Image g_cards[kCards] = {};
bool g_card_written[kCards] = {};
const uint8_t* g_card_pixels[kCards] = {};
uint32_t g_card_serial[kCards] = {};
int32_t g_card_rect[kCards][4] = {};
vk::Buffer g_stage[kVkVideoSlots] = {};

// For the report.
uint64_t g_composes = 0;
uint64_t g_draws = 0;
uint64_t g_uploads = 0;
uint64_t g_upload_bytes = 0;
uint64_t g_kept = 0;  // draws of a picture that the image of the card had
uint64_t g_ticks = 0;  // the time of VkVideoCompose on the frame thread
uint64_t g_skipped = 0;  // draws with no texture

void Fail(const char* what) {
  g_failed = true;
  ModLog("vk video     : %s failed. The view for videos shows the eye "
         "picture with no quad layers", what);
}

VkShaderModule Module(const uint32_t* code, size_t bytes) {
  VkShaderModuleCreateInfo info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  info.codeSize = bytes;
  info.pCode = code;
  VkShaderModule m = VK_NULL_HANDLE;
  f.vkCreateShaderModule(vk::Get().device, &info, nullptr, &m);
  return m;
}

bool Make() {
  if (g_made) return true;
  if (g_failed) return false;
  const VkDevice device = vk::Get().device;
  VkDescriptorSetLayoutBinding binding = {};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo set_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  set_info.bindingCount = 1;
  set_info.pBindings = &binding;
  if (f.vkCreateDescriptorSetLayout(device, &set_info, nullptr,
                                    &g_set_layout) != VK_SUCCESS) {
    Fail("the layout of the descriptor sets");
    return false;
  }
  VkPushConstantRange range = {};
  range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  range.size = sizeof(Quad);
  VkPipelineLayoutCreateInfo layout_info = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layout_info.setLayoutCount = 1;
  layout_info.pSetLayouts = &g_set_layout;
  layout_info.pushConstantRangeCount = 1;
  layout_info.pPushConstantRanges = &range;
  if (f.vkCreatePipelineLayout(device, &layout_info, nullptr, &g_layout) !=
      VK_SUCCESS) {
    Fail("the layout of the pipeline");
    return false;
  }
  g_vert = Module(kVkVideoVert, sizeof(kVkVideoVert));
  g_frag = Module(kVkVideoFrag, sizeof(kVkVideoFrag));
  if (g_vert == VK_NULL_HANDLE || g_frag == VK_NULL_HANDLE) {
    Fail("the shaders");
    return false;
  }
  VkSamplerCreateInfo sampler = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sampler.magFilter = VK_FILTER_LINEAR;
  sampler.minFilter = VK_FILTER_LINEAR;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.maxLod = 0.25f;
  if (f.vkCreateSampler(device, &sampler, nullptr, &g_sampler) != VK_SUCCESS) {
    Fail("the sampler");
    return false;
  }
  VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               kVkVideoSlots * kVideoDraws};
  VkDescriptorPoolCreateInfo pool = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pool.maxSets = kVkVideoSlots * kVideoDraws;
  pool.poolSizeCount = 1;
  pool.pPoolSizes = &size;
  if (f.vkCreateDescriptorPool(device, &pool, nullptr, &g_pool) !=
      VK_SUCCESS) {
    Fail("the descriptor pool");
    return false;
  }
  VkDescriptorSetLayout layouts[kVideoDraws];
  for (int i = 0; i < kVideoDraws; ++i) layouts[i] = g_set_layout;
  for (int s = 0; s < kVkVideoSlots; ++s) {
    VkDescriptorSetAllocateInfo allocate = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = g_pool;
    allocate.descriptorSetCount = kVideoDraws;
    allocate.pSetLayouts = layouts;
    if (f.vkAllocateDescriptorSets(device, &allocate, g_sets[s]) !=
        VK_SUCCESS) {
      Fail("the descriptor sets");
      return false;
    }
  }
  if (!vk::MakeImage(1, 1, 1, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, &g_white)) {
    Fail("the white image");
    return false;
  }
  g_made = true;
  ModLog("vk video     : the pipeline of the quad layers in the view for "
         "videos is ready");
  return true;
}

VkPipeline PipelineFor(VkFormat format) {
  for (int i = 0; i < kFormats; ++i) {
    if (g_pipelines[i] != VK_NULL_HANDLE && g_pipeline_format[i] == format) {
      return g_pipelines[i];
    }
  }
  int slot = -1;
  for (int i = 0; i < kFormats && slot < 0; ++i) {
    if (g_pipelines[i] == VK_NULL_HANDLE) slot = i;
  }
  if (slot < 0) return VK_NULL_HANDLE;
  VkPipelineShaderStageCreateInfo stages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = g_vert;
  stages[0].pName = "main";
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = g_frag;
  stages[1].pName = "main";
  VkPipelineVertexInputStateCreateInfo input = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo assembly = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  VkPipelineViewportStateCreateInfo viewport = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
  raster.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo multisample = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo depth = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  // The color holds the alpha already (THE TEXTURES, vk_video.h).
  VkPipelineColorBlendAttachmentState blend = {};
  blend.blendEnable = VK_TRUE;
  blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend.colorBlendOp = VK_BLEND_OP_ADD;
  blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend.alphaBlendOp = VK_BLEND_OP_ADD;
  blend.colorWriteMask = 0xF;
  VkPipelineColorBlendStateCreateInfo blend_state = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  blend_state.attachmentCount = 1;
  blend_state.pAttachments = &blend;
  const VkDynamicState dynamic[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                    VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic_state.dynamicStateCount = 2;
  dynamic_state.pDynamicStates = dynamic;
  VkPipelineRenderingCreateInfo rendering = {
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachmentFormats = &format;
  VkGraphicsPipelineCreateInfo info = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  info.pNext = &rendering;
  info.stageCount = 2;
  info.pStages = stages;
  info.pVertexInputState = &input;
  info.pInputAssemblyState = &assembly;
  info.pViewportState = &viewport;
  info.pRasterizationState = &raster;
  info.pMultisampleState = &multisample;
  info.pDepthStencilState = &depth;
  info.pColorBlendState = &blend_state;
  info.pDynamicState = &dynamic_state;
  info.layout = g_layout;
  VkPipeline pipeline = VK_NULL_HANDLE;
  if (f.vkCreateGraphicsPipelines(vk::Get().device, VK_NULL_HANDLE, 1, &info,
                                  nullptr, &pipeline) != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }
  g_pipelines[slot] = pipeline;
  g_pipeline_format[slot] = format;
  return pipeline;
}

// The image of a card for the pixels of the CPU, of the size of its picture.
// A card of a new size gets a new image after the queue is idle: the image
// before can still be read in the queue.
vk::Image* CardImage(int card, uint32_t width, uint32_t height) {
  if (card < 0 || card >= kCards) return nullptr;
  vk::Image& image = g_cards[card];
  if (image.image != VK_NULL_HANDLE && image.width == width &&
      image.height == height) {
    return &image;
  }
  if (image.image != VK_NULL_HANDLE) {
    vk::LockQueue();
    f.vkQueueWaitIdle(vk::Get().queue);
    vk::UnlockQueue();
    vk::FreeImage(&image);
  }
  g_card_written[card] = false;
  if (!vk::MakeImage(width, height, 1, VK_FORMAT_B8G8R8A8_UNORM,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, &image)) {
    return nullptr;
  }
  return &image;
}

// True when the image of the card holds the picture of the draw: the draw
// gives the number of its picture (VideoDraw::serial), and the last copy
// came from the same pixels, number, part and size.
bool Kept(const VideoDraw& d) {
  if (d.serial == 0 || d.card < 0 || d.card >= kCards) return false;
  const vk::Image& image = g_cards[d.card];
  return g_card_written[d.card] && image.image != VK_NULL_HANDLE &&
         image.width == d.texture_width && image.height == d.texture_height &&
         g_card_pixels[d.card] == d.pixels &&
         g_card_serial[d.card] == d.serial &&
         memcmp(g_card_rect[d.card], d.rect, sizeof(d.rect)) == 0;
}

// THE PIXELS OF THE CPU of each such draw go into the buffer of the slot,
// and from there into the image of the card. A draw whose picture the image
// has already (Kept) uses the image with no copy. Writes the view of each
// draw into views (VK_NULL_HANDLE for a draw that cannot have its texture).
void Upload(VkCommandBuffer cb, int slot, const VideoDraw* draws, int count,
            VkImageView views[kVideoDraws]) {
  VkDeviceSize bytes = 0;
  for (int i = 0; i < count; ++i) {
    if (draws[i].source != kVideoPixels || draws[i].pixels == nullptr) {
      continue;
    }
    if (Kept(draws[i])) {
      views[i] = g_cards[draws[i].card].view;
      ++g_kept;
      continue;
    }
    bytes += static_cast<VkDeviceSize>(draws[i].rect[2]) * draws[i].rect[3] * 4;
  }
  if (bytes == 0) return;
  vk::Buffer& stage = g_stage[slot];
  if (stage.buffer == VK_NULL_HANDLE || stage.size < bytes) {
    // The GPU has done the last use of this slot.
    vk::FreeBuffer(&stage);
    if (!vk::MakeBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true,
                        &stage)) {
      return;
    }
  }
  VkDeviceSize offset = 0;
  for (int i = 0; i < count; ++i) {
    const VideoDraw& d = draws[i];
    if (d.source != kVideoPixels || d.pixels == nullptr ||
        views[i] != VK_NULL_HANDLE) {
      continue;
    }
    vk::Image* image = CardImage(d.card, d.texture_width, d.texture_height);
    if (image == nullptr) continue;
    const int32_t x = d.rect[0], y = d.rect[1], w = d.rect[2], h = d.rect[3];
    const size_t row = static_cast<size_t>(w) * 4;
    const size_t pitch = static_cast<size_t>(d.texture_width) * 4;
    uint8_t* out = static_cast<uint8_t*>(stage.mapped) + offset;
    const uint8_t* in = d.pixels + static_cast<size_t>(y) * pitch +
                        static_cast<size_t>(x) * 4;
    for (int32_t r = 0; r < h; ++r) {
      memcpy(out + r * row, in + r * pitch, row);
    }
    vk::Barrier(cb, image->image, VK_IMAGE_ASPECT_COLOR_BIT,
                g_card_written[d.card]
                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    : VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
    VkBufferImageCopy copy = {};
    copy.bufferOffset = offset;
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageOffset = {x, y, 0};
    copy.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                        1};
    f.vkCmdCopyBufferToImage(cb, stage.buffer, image->image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    vk::Barrier(cb, image->image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    g_card_written[d.card] = true;
    g_card_pixels[d.card] = d.pixels;
    g_card_serial[d.card] = d.serial;
    memcpy(g_card_rect[d.card], d.rect, sizeof(d.rect));
    views[i] = image->view;
    offset += static_cast<VkDeviceSize>(row) * h;
    ++g_uploads;
    g_upload_bytes += static_cast<uint64_t>(row) * h;
  }
}

}  // namespace

bool VkVideoCompose(VkCommandBuffer cb, int slot, VkImage target,
                    VkImageView view, VkFormat format, uint32_t width,
                    uint32_t height, const float place[4],
                    const VideoDraw* draws, int count) {
  LARGE_INTEGER start, end;
  QueryPerformanceCounter(&start);
  const VkPipeline pipeline =
      count > 0 && view != VK_NULL_HANDLE && slot >= 0 &&
              slot < kVkVideoSlots && Make()
          ? PipelineFor(format)
          : VK_NULL_HANDLE;
  if (count > 0 && g_made && pipeline == VK_NULL_HANDLE && !g_failed) {
    Fail("the pipeline for the format of the window");
  }
  if (pipeline == VK_NULL_HANDLE) {
    vk::Barrier(cb, target, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);
    return false;
  }
  if (count > kVideoDraws) count = kVideoDraws;
  if (!g_white_ready) {
    vk::Barrier(cb, g_white.image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
    VkClearColorValue white = {};
    for (int i = 0; i < 4; ++i) white.float32[i] = 1.0f;
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    f.vkCmdClearColorImage(cb, g_white.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &white, 1,
                           &range);
    vk::Barrier(cb, g_white.image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    g_white_ready = true;
  }
  // The texture of each draw.
  VkImageView views[kVideoDraws] = {};
  Upload(cb, slot, draws, count, views);
  VkImage panel = VK_NULL_HANDLE;
  VkImageView panel_view = VK_NULL_HANDLE;
  for (int i = 0; i < count; ++i) {
    const VideoDraw& d = draws[i];
    if (d.source == kVideoWhite) {
      views[i] = g_white.view;
    } else if (d.source == kVideoView) {
      views[i] = (VkImageView)d.handle;  // a handle of 64 bits
    } else if (d.source == kVideoPanel) {
      // The quads of the panel are parts of one image.
      if (panel == VK_NULL_HANDLE) {
        panel_view = VkScenePanelView(d.handle);
        if (panel_view != VK_NULL_HANDLE) panel = (VkImage)d.handle;
      }
      if ((VkImage)d.handle == panel) views[i] = panel_view;
    }
  }
  // The panel image is a color attachment of the renderer between walks.
  if (panel != VK_NULL_HANDLE) {
    vk::Barrier(cb, panel, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
  }
  vk::Barrier(cb, target, VK_IMAGE_ASPECT_COLOR_BIT,
              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);

  VkRenderingAttachmentInfo color = {
      VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  color.imageView = view;
  color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  VkRenderingInfo rendering = {VK_STRUCTURE_TYPE_RENDERING_INFO};
  rendering.renderArea = {{0, 0}, {width, height}};
  rendering.layerCount = 1;
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachments = &color;
  f.vkCmdBeginRendering(cb, &rendering);
  f.vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  VkViewport viewport = {0.0f, 0.0f, static_cast<float>(width),
                         static_cast<float>(height), 0.0f, 1.0f};
  f.vkCmdSetViewport(cb, 0, 1, &viewport);
  // Only the area: a quad that goes past it is cut at its edge.
  VkRect2D scissor;
  scissor.offset.x = static_cast<int32_t>(place[0]);
  scissor.offset.y = static_cast<int32_t>(place[1]);
  scissor.extent.width =
      static_cast<uint32_t>(static_cast<int32_t>(place[2]) - scissor.offset.x);
  scissor.extent.height =
      static_cast<uint32_t>(static_cast<int32_t>(place[3]) - scissor.offset.y);
  f.vkCmdSetScissor(cb, 0, 1, &scissor);
  const float win_w = static_cast<float>(width);
  const float win_h = static_cast<float>(height);
  for (int i = 0; i < count; ++i) {
    if (views[i] == VK_NULL_HANDLE) {
      ++g_skipped;
      continue;
    }
    const VideoDraw& d = draws[i];
    VkDescriptorImageInfo image = {};
    image.sampler = g_sampler;
    image.imageView = views[i];
    image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = g_sets[slot][i];
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image;
    f.vkUpdateDescriptorSets(vk::Get().device, 1, &write, 0, nullptr);
    f.vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_layout,
                              0, 1, &g_sets[slot][i], 0, nullptr);
    Quad q;
    for (int c = 0; c < 4; ++c) {
      video_layers_math::AreaToClip(d.corner[c], place, win_w, win_h,
                                    q.corner[c]);
    }
    for (int k = 0; k < 4; ++k) {
      q.uv[k] = d.uv[k];
      q.color[k] = d.color[k];
    }
    f.vkCmdPushConstants(
        cb, g_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(q), &q);
    f.vkCmdDraw(cb, 4, 1, 0, 0);
    ++g_draws;
  }
  f.vkCmdEndRendering(cb);
  if (panel != VK_NULL_HANDLE) {
    vk::Barrier(cb, panel, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);
  }
  ++g_composes;
  QueryPerformanceCounter(&end);
  g_ticks += static_cast<uint64_t>(end.QuadPart - start.QuadPart);
  return true;
}

void VkVideoReport() {
  if (g_composes == 0) return;
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  const double ms = frequency.QuadPart > 0
                        ? 1000.0 * static_cast<double>(g_ticks) /
                              static_cast<double>(frequency.QuadPart) /
                              static_cast<double>(g_composes)
                        : 0.0;
  ModLog("vk video     : %llu windows with quad layers, %llu quads drawn, "
         "%llu with no texture, %llu copies of the pixels of the CPU "
         "(%.1f KB each), %llu draws with no copy because the picture did "
         "not change, %.3f ms on the frame thread for each window",
         g_composes, g_draws, g_skipped, g_uploads,
         g_uploads > 0 ? static_cast<double>(g_upload_bytes) / 1024.0 /
                             static_cast<double>(g_uploads)
                       : 0.0,
         g_kept, ms);
  g_composes = g_draws = g_skipped = g_uploads = g_upload_bytes = 0;
  g_kept = 0;
  g_ticks = 0;
}
