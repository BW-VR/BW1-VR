// The late compose of the eye picture (vk_compose.h, docs/backend.md, "The
// compose of the eye picture").

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
// The headset app (docs/android.md, "The renderer in the app").
#include "vk_platform_posix.h"
#endif
#include <stdio.h>
#include <string.h>

#include <cstddef>
#include <mutex>
#include <vector>

#include "vk_compose.h"
#include "vk_compose_frag.h"
#include "vk_compose_vert.h"
#include "vk_core.h"

void ModLog(const char* format, ...);

namespace vk_compose {

namespace {

namespace m = vk_compose_math;

// The functions of the device that the module calls. They come from
// vkGetDeviceProcAddr, so a table of the renderer that the app wraps does
// not see the commands of the compose.
struct Dev {
  PFN_vkCreateImageView CreateImageView;
  PFN_vkDestroyImageView DestroyImageView;
  PFN_vkCreateSampler CreateSampler;
  PFN_vkDestroySampler DestroySampler;
  PFN_vkCreateDescriptorSetLayout CreateSetLayout;
  PFN_vkDestroyDescriptorSetLayout DestroySetLayout;
  PFN_vkCreatePipelineLayout CreatePipelineLayout;
  PFN_vkDestroyPipelineLayout DestroyPipelineLayout;
  PFN_vkCreateShaderModule CreateShaderModule;
  PFN_vkDestroyShaderModule DestroyShaderModule;
  PFN_vkCreateGraphicsPipelines CreatePipelines;
  PFN_vkDestroyPipeline DestroyPipeline;
  PFN_vkCreateDescriptorPool CreatePool;
  PFN_vkDestroyDescriptorPool DestroyPool;
  PFN_vkResetDescriptorPool ResetPool;
  PFN_vkAllocateDescriptorSets AllocateSets;
  PFN_vkUpdateDescriptorSets UpdateSets;
  PFN_vkCmdPipelineBarrier2 CmdBarrier;
  PFN_vkCmdBeginRendering CmdBegin;
  PFN_vkCmdEndRendering CmdEnd;
  PFN_vkCmdBindPipeline CmdBindPipeline;
  PFN_vkCmdBindDescriptorSets CmdBindSets;
  PFN_vkCmdBindVertexBuffers CmdBindVertices;
  PFN_vkCmdPushConstants CmdPush;
  PFN_vkCmdSetViewport CmdViewport;
  PFN_vkCmdSetScissor CmdScissor;
  PFN_vkCmdDraw CmdDraw;
};

constexpr uint32_t kSlots = 8;
constexpr VkDeviceSize kSlotBytes = 8192;  // 150 vertices of 32 bytes

struct PushConstants {
  float scale[4];
  float tint[4];
  uint32_t flags[4];
};

struct View {
  VkImage image;
  uint32_t layer;
  VkFormat format;
  VkImageView view;
};

struct Pipe {
  VkFormat format;
  VkPipeline pipeline;
};

std::mutex g_lock;
Dev g_dev = {};
VkDevice g_device = VK_NULL_HANDLE;
bool g_ready = false;
bool g_failed = false;
VkShaderModule g_vert = VK_NULL_HANDLE;
VkShaderModule g_frag = VK_NULL_HANDLE;
VkSampler g_sampler = VK_NULL_HANDLE;
VkDescriptorSetLayout g_set_layout = VK_NULL_HANDLE;
VkPipelineLayout g_layout = VK_NULL_HANDLE;
VkDescriptorPool g_pool = VK_NULL_HANDLE;
VkDescriptorSet g_sets[m::kMostSets][2] = {};
uint64_t g_sets_generation = 0;
vk::Buffer g_vertices = {};
std::vector<View> g_views;
std::vector<Pipe> g_pipes;

// The pending input.
Input g_input = {};
uint64_t g_eye_image = 0;
bool g_have = false;
bool g_uncomposed = false;
Counts g_counts;

// THE COMPOSE AT THE TAKE: the inputs that wait for the thread of the session.
struct Ticket {
  Input input;
  uint64_t serial;
};
Ticket g_tickets[kSlots] = {};
uint64_t g_ticket_serial = 0;
uint64_t g_ticket_first = 1;  // an older ticket names images that went
std::mutex g_take_lock;

template <typename T>
bool Load(T* out, const char* name) {
  *out = reinterpret_cast<T>(vk::f.vkGetDeviceProcAddr(g_device, name));
  return *out != nullptr;
}

bool LoadDevice() {
#define BWVR_LOAD(member, name) ok = Load(&g_dev.member, name) && ok
  bool ok = true;
  BWVR_LOAD(CreateImageView, "vkCreateImageView");
  BWVR_LOAD(DestroyImageView, "vkDestroyImageView");
  BWVR_LOAD(CreateSampler, "vkCreateSampler");
  BWVR_LOAD(DestroySampler, "vkDestroySampler");
  BWVR_LOAD(CreateSetLayout, "vkCreateDescriptorSetLayout");
  BWVR_LOAD(DestroySetLayout, "vkDestroyDescriptorSetLayout");
  BWVR_LOAD(CreatePipelineLayout, "vkCreatePipelineLayout");
  BWVR_LOAD(DestroyPipelineLayout, "vkDestroyPipelineLayout");
  BWVR_LOAD(CreateShaderModule, "vkCreateShaderModule");
  BWVR_LOAD(DestroyShaderModule, "vkDestroyShaderModule");
  BWVR_LOAD(CreatePipelines, "vkCreateGraphicsPipelines");
  BWVR_LOAD(DestroyPipeline, "vkDestroyPipeline");
  BWVR_LOAD(CreatePool, "vkCreateDescriptorPool");
  BWVR_LOAD(DestroyPool, "vkDestroyDescriptorPool");
  BWVR_LOAD(ResetPool, "vkResetDescriptorPool");
  BWVR_LOAD(AllocateSets, "vkAllocateDescriptorSets");
  BWVR_LOAD(UpdateSets, "vkUpdateDescriptorSets");
  BWVR_LOAD(CmdBarrier, "vkCmdPipelineBarrier2");
  BWVR_LOAD(CmdBegin, "vkCmdBeginRendering");
  BWVR_LOAD(CmdEnd, "vkCmdEndRendering");
  BWVR_LOAD(CmdBindPipeline, "vkCmdBindPipeline");
  BWVR_LOAD(CmdBindSets, "vkCmdBindDescriptorSets");
  BWVR_LOAD(CmdBindVertices, "vkCmdBindVertexBuffers");
  BWVR_LOAD(CmdPush, "vkCmdPushConstants");
  BWVR_LOAD(CmdViewport, "vkCmdSetViewport");
  BWVR_LOAD(CmdScissor, "vkCmdSetScissor");
  BWVR_LOAD(CmdDraw, "vkCmdDraw");
#undef BWVR_LOAD
  return ok;
}

void DestroyAll() {
  if (g_device == VK_NULL_HANDLE) return;
  for (View& v : g_views) {
    if (v.view != VK_NULL_HANDLE) g_dev.DestroyImageView(g_device, v.view, nullptr);
  }
  g_views.clear();
  for (Pipe& p : g_pipes) g_dev.DestroyPipeline(g_device, p.pipeline, nullptr);
  g_pipes.clear();
  if (g_pool != VK_NULL_HANDLE) g_dev.DestroyPool(g_device, g_pool, nullptr);
  if (g_layout != VK_NULL_HANDLE) g_dev.DestroyPipelineLayout(g_device, g_layout, nullptr);
  if (g_set_layout != VK_NULL_HANDLE) g_dev.DestroySetLayout(g_device, g_set_layout, nullptr);
  if (g_sampler != VK_NULL_HANDLE) g_dev.DestroySampler(g_device, g_sampler, nullptr);
  if (g_vert != VK_NULL_HANDLE) g_dev.DestroyShaderModule(g_device, g_vert, nullptr);
  if (g_frag != VK_NULL_HANDLE) g_dev.DestroyShaderModule(g_device, g_frag, nullptr);
  vk::FreeBuffer(&g_vertices);
  g_pool = VK_NULL_HANDLE;
  g_layout = VK_NULL_HANDLE;
  g_set_layout = VK_NULL_HANDLE;
  g_sampler = VK_NULL_HANDLE;
  g_vert = g_frag = VK_NULL_HANDLE;
  memset(g_sets, 0, sizeof(g_sets));
  g_sets_generation = 0;
  g_ticket_first = g_ticket_serial + 1;
  g_ready = false;
  g_have = false;
  g_device = VK_NULL_HANDLE;
}

bool Init() {
  if (g_ready) return true;
  if (g_failed || !vk::Started()) return false;
  g_device = vk::Get().device;
  if (vk::f.vkGetDeviceProcAddr == nullptr || !LoadDevice()) {
    g_failed = true;
    g_device = VK_NULL_HANDLE;
    ModLog("vk compose   : the device has not the functions of the compose. The late "
           "compose is off");
    return false;
  }
  bool ok = true;
  VkShaderModuleCreateInfo module = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  module.codeSize = sizeof(kVkComposeVert);
  module.pCode = kVkComposeVert;
  ok = ok && g_dev.CreateShaderModule(g_device, &module, nullptr, &g_vert) == VK_SUCCESS;
  module.codeSize = sizeof(kVkComposeFrag);
  module.pCode = kVkComposeFrag;
  ok = ok && g_dev.CreateShaderModule(g_device, &module, nullptr, &g_frag) == VK_SUCCESS;
  VkSamplerCreateInfo sampler = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sampler.magFilter = VK_FILTER_LINEAR;
  sampler.minFilter = VK_FILTER_LINEAR;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler.addressModeU = sampler.addressModeV = sampler.addressModeW =
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.maxLod = 0.0f;
  ok = ok && g_dev.CreateSampler(g_device, &sampler, nullptr, &g_sampler) == VK_SUCCESS;
  VkDescriptorSetLayoutBinding bindings[2] = {};
  for (uint32_t i = 0; i < 2; ++i) {
    bindings[i].binding = i;
    bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  VkDescriptorSetLayoutCreateInfo set_layout = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  set_layout.bindingCount = 2;
  set_layout.pBindings = bindings;
  ok = ok && g_dev.CreateSetLayout(g_device, &set_layout, nullptr, &g_set_layout) == VK_SUCCESS;
  VkPushConstantRange range = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(PushConstants)};
  VkPipelineLayoutCreateInfo layout = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layout.setLayoutCount = 1;
  layout.pSetLayouts = &g_set_layout;
  layout.pushConstantRangeCount = 1;
  layout.pPushConstantRanges = &range;
  ok = ok && g_dev.CreatePipelineLayout(g_device, &layout, nullptr, &g_layout) == VK_SUCCESS;
  VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 * m::kMostSets};
  VkDescriptorPoolCreateInfo pool = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pool.maxSets = 2 * m::kMostSets;
  pool.poolSizeCount = 1;
  pool.pPoolSizes = &size;
  ok = ok && g_dev.CreatePool(g_device, &pool, nullptr, &g_pool) == VK_SUCCESS;
  ok = ok && vk::MakeBuffer(kSlotBytes * kSlots, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true,
                            &g_vertices);
  if (!ok) {
    g_failed = true;
    DestroyAll();
    ModLog("vk compose   : the shaders, the layouts or the buffer of the compose were not "
           "made. The late compose is off");
    return false;
  }
  g_ready = true;
  ModLog("vk compose   : the late compose is ready (the eye picture from the outer image and "
         "the centre image in one pass for each eye, into the image that the caller names)");
  return true;
}

VkPipeline PipelineFor(VkFormat format) {
  for (const Pipe& p : g_pipes) {
    if (p.format == format) return p.pipeline;
  }
  VkPipelineShaderStageCreateInfo stages[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = g_vert;
  stages[0].pName = "main";
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = g_frag;
  stages[1].pName = "main";
  const VkVertexInputBindingDescription binding = {0, sizeof(m::Vertex),
                                                   VK_VERTEX_INPUT_RATE_VERTEX};
  const VkVertexInputAttributeDescription attributes[4] = {
      {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
      {1, 0, VK_FORMAT_R32_SFLOAT, 8},
      {2, 0, VK_FORMAT_R32G32_SFLOAT, 16},
      {3, 0, VK_FORMAT_R32G32_SFLOAT, 24}};
  VkPipelineVertexInputStateCreateInfo input = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  input.vertexBindingDescriptionCount = 1;
  input.pVertexBindingDescriptions = &binding;
  input.vertexAttributeDescriptionCount = 4;
  input.pVertexAttributeDescriptions = attributes;
  VkPipelineInputAssemblyStateCreateInfo assembly = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
  VkPipelineColorBlendAttachmentState blend = {};
  blend.colorWriteMask = 0xF;
  VkPipelineColorBlendStateCreateInfo blend_state = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  blend_state.attachmentCount = 1;
  blend_state.pAttachments = &blend;
  const VkDynamicState dynamic[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic_state.dynamicStateCount = 2;
  dynamic_state.pDynamicStates = dynamic;
  // One layer, no eye, no depth.
  VkPipelineRenderingCreateInfo rendering = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  rendering.viewMask = 0;
  rendering.colorAttachmentCount = 1;
  rendering.pColorAttachmentFormats = &format;
  VkGraphicsPipelineCreateInfo info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  info.pNext = &rendering;
  info.stageCount = 2;
  info.pStages = stages;
  info.pVertexInputState = &input;
  info.pInputAssemblyState = &assembly;
  info.pViewportState = &viewport;
  info.pRasterizationState = &raster;
  info.pMultisampleState = &multisample;
  info.pColorBlendState = &blend_state;
  info.pDynamicState = &dynamic_state;
  info.layout = g_layout;
  VkPipeline pipeline = VK_NULL_HANDLE;
  if (g_dev.CreatePipelines(g_device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) !=
      VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }
  g_pipes.push_back({format, pipeline});
  ModLog("vk compose   : a pipeline of the compose for the format %d", static_cast<int>(format));
  return pipeline;
}

VkImageView ViewFor(const Target& t) {
  for (const View& v : g_views) {
    if (v.image == t.image && v.layer == t.layer && v.format == t.format) return v.view;
  }
  VkImageViewCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  info.image = t.image;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = t.format;
  info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, t.layer, 1};
  VkImageView view = VK_NULL_HANDLE;
  if (g_dev.CreateImageView(g_device, &info, nullptr, &view) != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }
  g_views.push_back({t.image, t.layer, t.format, view});
  return view;
}

// The two descriptor sets of the input (one for each eye): the outer image and the
// centre image of its set. They are made again when the views change.
bool EnsureSets(const Input& input) {
  VkDescriptorSet* sets = g_sets[input.set];
  if (sets[0] != VK_NULL_HANDLE && g_sets_generation == input.generation) return true;
  if (g_sets_generation != input.generation) {
    g_dev.ResetPool(g_device, g_pool, 0);
    memset(g_sets, 0, sizeof(g_sets));
    g_sets_generation = input.generation;
  }
  VkDescriptorSetLayout layouts[2] = {g_set_layout, g_set_layout};
  VkDescriptorSetAllocateInfo alloc = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  alloc.descriptorPool = g_pool;
  alloc.descriptorSetCount = 2;
  alloc.pSetLayouts = layouts;
  if (g_dev.AllocateSets(g_device, &alloc, sets) != VK_SUCCESS) {
    sets[0] = sets[1] = VK_NULL_HANDLE;
    return false;
  }
  for (int eye = 0; eye < 2; ++eye) {
    VkDescriptorImageInfo images[2] = {
        {g_sampler, input.outer[eye], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {g_sampler, input.center[eye], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
    VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    for (uint32_t i = 0; i < 2; ++i) {
      writes[i].dstSet = sets[eye];
      writes[i].dstBinding = i;
      writes[i].descriptorCount = 1;
      writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writes[i].pImageInfo = &images[i];
    }
    g_dev.UpdateSets(g_device, 2, writes, 0, nullptr);
  }
  return true;
}

bool IsSrgb(VkFormat format) {
  return format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_B8G8R8A8_SRGB;
}

bool RecordInput(VkCommandBuffer cb, uint32_t slot, const Input& input, const Target targets[2]);

}  // namespace

void Set(const Input& input, uint64_t eye_image, bool uncomposed) {
  std::lock_guard<std::mutex> lock(g_lock);
  g_input = input;
  g_eye_image = eye_image;
  g_have = true;
  g_uncomposed = uncomposed;
}

bool Pending(uint64_t eye_image) {
  std::lock_guard<std::mutex> lock(g_lock);
  return g_have && g_uncomposed && g_eye_image == eye_image;
}

uint64_t Pending() {
  std::lock_guard<std::mutex> lock(g_lock);
  return g_have && g_uncomposed ? g_eye_image : 0;
}

void Drop(uint64_t eye_image) {
  std::lock_guard<std::mutex> lock(g_lock);
  if (g_eye_image == eye_image) g_have = false;
}

void DropAll() {
  std::lock_guard<std::mutex> lock(g_lock);
  g_have = false;
  g_ticket_first = g_ticket_serial + 1;
}

bool Usable() {
  std::lock_guard<std::mutex> lock(g_lock);
  return Init() && PipelineFor(vk::Get().color_format) != VK_NULL_HANDLE;
}

void MarkComposed(uint64_t eye_image) {
  std::lock_guard<std::mutex> lock(g_lock);
  if (g_eye_image == eye_image) g_uncomposed = false;
}

bool Record(VkCommandBuffer cb, uint32_t slot, const Target targets[2]) {
  std::lock_guard<std::mutex> lock(g_lock);
  if (!g_have) {
    ++g_counts.no_pending;
    return false;
  }
  return RecordInput(cb, slot, g_input, targets);
}

uint64_t TakeTicket(uint64_t eye_image) {
  std::lock_guard<std::mutex> lock(g_lock);
  if (!g_have || !g_uncomposed || g_eye_image != eye_image) return 0;
  ++g_ticket_serial;
  Ticket& t = g_tickets[g_ticket_serial % kSlots];
  t.input = g_input;
  t.serial = g_ticket_serial;
  g_uncomposed = false;
  return g_ticket_serial;
}

bool RecordTicket(VkCommandBuffer cb, uint64_t ticket, uint32_t slot, const Target targets[2]) {
  std::lock_guard<std::mutex> lock(g_lock);
  const Ticket& t = g_tickets[ticket % kSlots];
  if (ticket < g_ticket_first || t.serial != ticket) {
    ++g_counts.no_pending;
    return false;
  }
  return RecordInput(cb, slot, t.input, targets);
}

void TakeLock() { g_take_lock.lock(); }
void TakeUnlock() { g_take_lock.unlock(); }

namespace {

// With g_lock.
bool RecordInput(VkCommandBuffer cb, uint32_t slot, const Input& input, const Target targets[2]) {
  if (!Init() || !EnsureSets(input)) return false;
  const m::Layout& l = input.layout;
  m::Vertex grid[m::kVertices];
  m::BuildGrid(l, grid);
  slot %= kSlots;
  const VkDeviceSize at = kSlotBytes * slot;
  memcpy(static_cast<uint8_t*>(g_vertices.mapped) + at, grid, sizeof(grid));
  for (int eye = 0; eye < 2; ++eye) {
    const Target& t = targets[eye];
    const VkPipeline pipeline = PipelineFor(t.format);
    const VkImageView view = ViewFor(t);
    if (pipeline == VK_NULL_HANDLE || view == VK_NULL_HANDLE) return false;
    // The old pixels go: the pass loads nothing. The wait is for every
    // earlier command of the queue, because a copy of an earlier picture can
    // still read the image.
    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = 0;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = t.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, t.layer, 1};
    VkDependencyInfo dependency = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    g_dev.CmdBarrier(cb, &dependency);

    VkRenderingAttachmentInfo color = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    color.imageView = view;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo rendering = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering.renderArea = {{0, 0}, {l.width, l.height}};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;
    g_dev.CmdBegin(cb, &rendering);
    const VkViewport viewport = {0.0f, 0.0f, static_cast<float>(l.width),
                                 static_cast<float>(l.height), 0.0f, 1.0f};
    const VkRect2D scissor = {{0, 0}, {l.width, l.height}};
    g_dev.CmdViewport(cb, 0, 1, &viewport);
    g_dev.CmdScissor(cb, 0, 1, &scissor);
    g_dev.CmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    g_dev.CmdBindSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_layout, 0, 1, &g_sets[input.set][eye],
                      0, nullptr);
    PushConstants push = {};
    push.scale[0] = 2.0f / static_cast<float>(l.width);
    push.scale[1] = 2.0f / static_cast<float>(l.height);
    // The marks of the test: the outer image has a blue colour.
    push.tint[0] = input.tint ? 0x90 / 255.0f : 1.0f;
    push.tint[1] = input.tint ? 0xB8 / 255.0f : 1.0f;
    push.tint[2] = 1.0f;
    push.tint[3] = 1.0f;
    push.flags[0] = IsSrgb(t.format) ? 1u : 0u;
    g_dev.CmdPush(cb, g_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                  sizeof(push), &push);
    g_dev.CmdBindVertices(cb, 0, 1, &g_vertices.buffer, &at);
    g_dev.CmdDraw(cb, m::kVertices, 1, 0, 0);
    g_dev.CmdEnd(cb);
  }
  ++g_counts.records;
  return true;
}

}  // namespace

void Forget(VkImage image) {
  std::lock_guard<std::mutex> lock(g_lock);
  // The eye image of a pending compose goes: nothing is pending.
  if (g_eye_image == (uint64_t)image) g_have = false;
  if (g_device == VK_NULL_HANDLE) return;
  for (size_t i = 0; i < g_views.size();) {
    if (g_views[i].image == image) {
      g_dev.DestroyImageView(g_device, g_views[i].view, nullptr);
      g_views.erase(g_views.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
}

void Stop() {
  std::lock_guard<std::mutex> lock(g_lock);
  DestroyAll();
  g_failed = false;
}

Counts Taken() {
  std::lock_guard<std::mutex> lock(g_lock);
  return g_counts;
}

namespace {

struct Diff {
  uint32_t max = 0;
  uint64_t one = 0, more = 0, pixels = 0;
  double sum = 0.0;
};

void Compare(const uint8_t* a, const uint8_t* b, uint64_t pixels, Diff* d) {
  for (uint64_t i = 0; i < pixels; ++i) {
    uint32_t worst = 0;
    for (int c = 0; c < 4; ++c) {
      const int x = a[i * 4 + c], y = b[i * 4 + c];
      const uint32_t diff = static_cast<uint32_t>(x > y ? x - y : y - x);
      if (diff > worst) worst = diff;
      d->sum += diff;
    }
    if (worst > d->max) d->max = worst;
    if (worst == 1) ++d->one;
    if (worst > 1) ++d->more;
  }
  d->pixels += pixels;
}

}  // namespace

void CompareWithTarget(VkImage eye_image, uint32_t width, uint32_t height) {
  {
    std::lock_guard<std::mutex> lock(g_lock);
    if (!g_have) return;
  }
  const uint64_t bytes = static_cast<uint64_t>(width) * height * 4;
  vk::Buffer old_buffer = {}, new_buffer = {};
  if (!vk::MakeBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, &old_buffer) ||
      !vk::MakeBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, &new_buffer)) {
    vk::FreeBuffer(&old_buffer);
    vk::FreeBuffer(&new_buffer);
    ModLog("vk compose   : the test has no buffers");
    return;
  }
  const VkFormat formats[2] = {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM};
  for (int variant = 0; variant < 2; ++variant) {
    vk::Image images[2] = {};
    bool made = true;
    for (int eye = 0; eye < 2; ++eye) {
      made = made && vk::MakeImage(width, height, 1, formats[variant],
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                   VK_IMAGE_ASPECT_COLOR_BIT, &images[eye], 1);
    }
    Diff diff[2];
    bool ran = made;
    if (made) {
      const Target targets[2] = {{images[0].image, 0, formats[variant]},
                                 {images[1].image, 0, formats[variant]}};
      for (int eye = 0; eye < 2 && ran; ++eye) {
        // The eye image of the walk: its bytes, then the compose of this
        // eye into an image of the format.
        VkCommandBuffer cb = vk::BeginOnce();
        if (cb == VK_NULL_HANDLE) {
          ran = false;
          break;
        }
        vk::Barrier(cb, eye_image, VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 1, static_cast<uint32_t>(eye));
        VkBufferImageCopy copy = {};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, static_cast<uint32_t>(eye), 1};
        copy.imageExtent = {width, height, 1};
        vk::f.vkCmdCopyImageToBuffer(cb, eye_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     old_buffer.buffer, 1, &copy);
        vk::Barrier(cb, eye_image, VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 1, static_cast<uint32_t>(eye));
        // Both eyes are composed, and the copy reads the image of this eye.
        ran = Record(cb, 0, targets);
        if (ran) {
          vk::Barrier(cb, images[eye].image, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1);
          VkBufferImageCopy copy_new = {};
          copy_new.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
          copy_new.imageExtent = {width, height, 1};
          vk::f.vkCmdCopyImageToBuffer(cb, images[eye].image,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, new_buffer.buffer,
                                       1, &copy_new);
        }
        const bool submitted = vk::EndOnce(cb);
        ran = ran && submitted;
        if (ran) {
          Compare(static_cast<const uint8_t*>(old_buffer.mapped),
                  static_cast<const uint8_t*>(new_buffer.mapped),
                  static_cast<uint64_t>(width) * height, &diff[eye]);
        }
      }
    }
    for (int eye = 0; eye < 2; ++eye) {
      Forget(images[eye].image);
      vk::FreeImage(&images[eye]);
    }
    if (!ran) {
      ModLog("vk compose   : the test of format %d did not run", static_cast<int>(formats[variant]));
      continue;
    }
    for (int eye = 0; eye < 2; ++eye) {
      const Diff& d = diff[eye];
      ModLog("vk compose   : test, %s target, eye %d: %llu pixels of %u by %u, the largest "
             "difference to the compose of the walk %u of 255, %llu pixels differ by 1, %llu by "
             "2 or more, the mean difference %.4f",
             variant == 0 ? "sRGB" : "UNORM", eye, static_cast<unsigned long long>(d.pixels),
             width, height, d.max, static_cast<unsigned long long>(d.one),
             static_cast<unsigned long long>(d.more),
             d.sum / (4.0 * static_cast<double>(d.pixels)));
    }
  }
  vk::FreeBuffer(&old_buffer);
  vk::FreeBuffer(&new_buffer);
}

}  // namespace vk_compose
