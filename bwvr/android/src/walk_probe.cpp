#include "walk_probe.h"

#include <atomic>
#include <mutex>

#include "../../src/mod/vk_core.h"
#include "app_log.h"

namespace {

namespace wp = walk_parts;

// The command buffers of the renderer that the probe follows: each with its
// own timestamps, kMostMarks from Base(entry) in the query pool.
constexpr int kEntries = 16;
struct Entry {
  VkCommandBuffer cb = VK_NULL_HANDLE;
  wp::Recording rec;
  bool open = false;       // between vkBeginCommandBuffer and vkEndCommandBuffer
  bool recorded = false;   // ended, and not begun again
  bool submitted = false;  // a submit of a walk took it after its end
  uint64_t used = 0;       // the count of begins at its last begin
};
Entry g_entries[kEntries];
int g_count = 0;
Entry* g_last = nullptr;     // the entry of the last call, for a quick find
Entry* g_current = nullptr;  // the entry in recording for vkUpdateDescriptorSets
uint64_t g_begins = 0;

std::atomic<bool> g_on{true};
bool g_said = false;
vk::Functions g_real = {};
VkQueryPool g_pool = VK_NULL_HANDLE;
VkDevice g_pool_device = VK_NULL_HANDLE;  // the device of g_pool
double g_tick_ns = 0.0;
uint64_t g_mask = 0;

std::mutex g_lock;  // the totals: the thread that plays adds, the time log takes
wp::Totals g_totals;

Entry* Find(VkCommandBuffer cb, bool make) {
  if (g_last != nullptr && g_last->cb == cb) return g_last;
  for (int i = 0; i < g_count; ++i) {
    if (g_entries[i].cb == cb) return g_last = &g_entries[i];
  }
  if (!make) return nullptr;
  if (g_count < kEntries) {
    Entry& e = g_entries[g_count++];
    e.cb = cb;
    return g_last = &e;
  }
  // A full table: the entry that began least recently and does not record
  // now goes (a one-time command buffer of vk_core.cpp, for example). An
  // entry of a submitted walk stays: the GPU can still write its timestamps.
  Entry* old = nullptr;
  for (int i = 0; i < g_count; ++i) {
    Entry& e = g_entries[i];
    if (e.open || (e.recorded && e.submitted)) continue;
    if (old == nullptr || e.used < old->used) old = &e;
  }
  if (old == nullptr) return nullptr;
  if (g_current == old) g_current = nullptr;
  *old = Entry();
  old->cb = cb;
  return g_last = old;
}

uint32_t Base(const Entry* e) {
  return static_cast<uint32_t>(e - g_entries) * wp::kMostMarks;
}

void Stamp(VkCommandBuffer cb, const Entry* e, uint32_t mark) {
  if (g_pool == VK_NULL_HANDLE) return;
  g_real.vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, g_pool, Base(e) + mark);
}

// The renderer begins a command buffer again only after the GPU finished it
// (it waits for the fence of its slot first), so the timestamps of the last
// submit are there. A command buffer that no submit took adds nothing.
void Harvest(Entry* e) {
  if (!e->recorded || !e->submitted) return;
  const uint32_t marks = e->rec.Marks();
  double ms[wp::kMostMarks];
  bool timed = false;
  if (g_pool != VK_NULL_HANDLE && marks >= 2) {
    uint64_t v[2 * wp::kMostMarks];
    const VkResult r = g_real.vkGetQueryPoolResults(
        vk::Get().device, g_pool, Base(e), marks, sizeof(v), v, 2 * sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    timed = r == VK_SUCCESS;
    for (uint32_t i = 0; timed && i < marks; ++i) timed = v[2 * i + 1] != 0;
    for (uint32_t i = 0; timed && i + 1 < marks; ++i) {
      const uint64_t ticks = (v[2 * (i + 1)] - v[2 * i]) & g_mask;
      ms[i] = static_cast<double>(ticks) * g_tick_ns * 1e-6;
      // A part over 100 ms is not a part of one walk.
      if (ms[i] > 100.0) timed = false;
    }
  }
  std::lock_guard<std::mutex> g(g_lock);
  g_totals.Add(e->rec, timed ? ms : nullptr);
}

VKAPI_ATTR VkResult VKAPI_CALL Begin(VkCommandBuffer cb, const VkCommandBufferBeginInfo* info) {
  Entry* e = Find(cb, true);
  if (e != nullptr) {
    Harvest(e);
    e->recorded = e->submitted = e->open = false;
  }
  const VkResult r = g_real.vkBeginCommandBuffer(cb, info);
  if (e != nullptr && r == VK_SUCCESS) {
    if (g_pool != VK_NULL_HANDLE) g_real.vkCmdResetQueryPool(cb, g_pool, Base(e), wp::kMostMarks);
    uint32_t mark = 0;
    if (e->rec.Begin(&mark)) Stamp(cb, e, mark);
    e->open = true;
    e->used = ++g_begins;
    g_current = e;
  }
  return r;
}

VKAPI_ATTR VkResult VKAPI_CALL End(VkCommandBuffer cb) {
  Entry* e = Find(cb, false);
  if (e != nullptr && e->open) {
    uint32_t mark = 0;
    if (e->rec.End(&mark)) Stamp(cb, e, mark);
    e->open = false;
    e->recorded = true;
    if (g_current == e) g_current = nullptr;
  }
  return g_real.vkEndCommandBuffer(cb);
}

VKAPI_ATTR VkResult VKAPI_CALL Submit(VkQueue queue, uint32_t count, const VkSubmitInfo2* submits,
                                      VkFence fence) {
  // Only the submit of a walk (one command buffer with a pass, and the
  // command buffer of its uploads) gives parts. A one-time command buffer
  // and a read back alone give none.
  for (uint32_t i = 0; i < count; ++i) {
    bool walk = false;
    for (uint32_t j = 0; j < submits[i].commandBufferInfoCount; ++j) {
      const Entry* e = Find(submits[i].pCommandBufferInfos[j].commandBuffer, false);
      if (e != nullptr && e->recorded && e->rec.CountsOf().passes > 0) walk = true;
    }
    for (uint32_t j = 0; walk && j < submits[i].commandBufferInfoCount; ++j) {
      Entry* e = Find(submits[i].pCommandBufferInfos[j].commandBuffer, false);
      if (e != nullptr && e->recorded) e->submitted = true;
    }
  }
  return g_real.vkQueueSubmit2(queue, count, submits, fence);
}

VKAPI_ATTR void VKAPI_CALL BeginRendering(VkCommandBuffer cb, const VkRenderingInfo* info) {
  Entry* e = Find(cb, false);
  if (e != nullptr && e->open) {
    wp::PassKey key;
    key.views = wp::ViewCount(info->viewMask);
    key.width = info->renderArea.extent.width;
    key.height = info->renderArea.extent.height;
    key.clear = info->colorAttachmentCount > 0 && info->pColorAttachments != nullptr &&
                info->pColorAttachments[0].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR;
    for (const VkBaseInStructure* n = static_cast<const VkBaseInStructure*>(info->pNext);
         n != nullptr; n = n->pNext) {
      if (n->sType == VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR &&
          reinterpret_cast<const VkRenderingFragmentShadingRateAttachmentInfoKHR*>(n)->imageView !=
              VK_NULL_HANDLE) {
        key.rate = true;
      }
      if (n->sType == VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT &&
          reinterpret_cast<const VkRenderingFragmentDensityMapAttachmentInfoEXT*>(n)->imageView !=
              VK_NULL_HANDLE) {
        key.density = true;
      }
    }
    uint32_t mark = 0;
    if (e->rec.PassBegin(key, &mark)) Stamp(cb, e, mark);
  }
  g_real.vkCmdBeginRendering(cb, info);
}

VKAPI_ATTR void VKAPI_CALL EndRendering(VkCommandBuffer cb) {
  g_real.vkCmdEndRendering(cb);
  Entry* e = Find(cb, false);
  if (e != nullptr && e->open) {
    uint32_t mark = 0;
    if (e->rec.PassEnd(&mark)) Stamp(cb, e, mark);
  }
}

VKAPI_ATTR void VKAPI_CALL Draw(VkCommandBuffer cb, uint32_t vertices, uint32_t instances,
                                uint32_t first, uint32_t first_instance) {
  Entry* e = Find(cb, false);
  if (e != nullptr && e->open) e->rec.Draw();
  g_real.vkCmdDraw(cb, vertices, instances, first, first_instance);
}

VKAPI_ATTR void VKAPI_CALL DrawIndexed(VkCommandBuffer cb, uint32_t indices, uint32_t instances,
                                       uint32_t first, int32_t offset, uint32_t first_instance) {
  Entry* e = Find(cb, false);
  if (e != nullptr && e->open) e->rec.Draw();
  g_real.vkCmdDrawIndexed(cb, indices, instances, first, offset, first_instance);
}

VKAPI_ATTR void VKAPI_CALL BindPipeline(VkCommandBuffer cb, VkPipelineBindPoint point,
                                        VkPipeline pipeline) {
  Entry* e = Find(cb, false);
  if (e != nullptr && e->open) e->rec.Pipeline();
  g_real.vkCmdBindPipeline(cb, point, pipeline);
}

VKAPI_ATTR void VKAPI_CALL BindSets(VkCommandBuffer cb, VkPipelineBindPoint point,
                                    VkPipelineLayout layout, uint32_t first, uint32_t count,
                                    const VkDescriptorSet* sets, uint32_t dynamic_count,
                                    const uint32_t* dynamic) {
  Entry* e = Find(cb, false);
  if (e != nullptr && e->open) e->rec.Sets();
  g_real.vkCmdBindDescriptorSets(cb, point, layout, first, count, sets, dynamic_count, dynamic);
}

VKAPI_ATTR void VKAPI_CALL UpdateSets(VkDevice device, uint32_t writes,
                                      const VkWriteDescriptorSet* write, uint32_t copies,
                                      const VkCopyDescriptorSet* copy) {
  if (g_current != nullptr && g_current->open) g_current->rec.Writes(writes);
  g_real.vkUpdateDescriptorSets(device, writes, write, copies, copy);
}

VKAPI_ATTR void VKAPI_CALL CopyBufferToImage(VkCommandBuffer cb, VkBuffer buffer, VkImage image,
                                             VkImageLayout layout, uint32_t count,
                                             const VkBufferImageCopy* regions) {
  Entry* e = Find(cb, false);
  if (e != nullptr && e->open) {
    uint64_t texels = 0;
    for (uint32_t i = 0; i < count; ++i) {
      const VkExtent3D& x = regions[i].imageExtent;
      texels += static_cast<uint64_t>(x.width) * x.height * x.depth *
                regions[i].imageSubresource.layerCount;
    }
    e->rec.Upload(count, texels);
  }
  g_real.vkCmdCopyBufferToImage(cb, buffer, image, layout, count, regions);
}

VKAPI_ATTR void VKAPI_CALL BlitImage(VkCommandBuffer cb, VkImage from, VkImageLayout from_layout,
                                     VkImage to, VkImageLayout to_layout, uint32_t count,
                                     const VkImageBlit* regions, VkFilter filter) {
  Entry* e = Find(cb, false);
  if (e != nullptr && e->open) e->rec.Copy();
  g_real.vkCmdBlitImage(cb, from, from_layout, to, to_layout, count, regions, filter);
}

VKAPI_ATTR void VKAPI_CALL CopyImageToBuffer(VkCommandBuffer cb, VkImage image,
                                             VkImageLayout layout, VkBuffer buffer,
                                             uint32_t count, const VkBufferImageCopy* regions) {
  Entry* e = Find(cb, false);
  if (e != nullptr && e->open) e->rec.ReadBack();
  g_real.vkCmdCopyImageToBuffer(cb, image, layout, buffer, count, regions);
}

}  // namespace

void WalkProbeSetOn(bool on) { g_on.store(on); }
bool WalkProbeOn() { return g_on.load(); }

void WalkProbeInstall() {
  if (!g_on.load() || !vk::Started()) return;
  vk::Functions& f = vk::f;
  if (f.vkCmdBeginRendering == &BeginRendering) return;
  // The functions of the driver first, then the entries of the app, so a
  // call on another thread finds a valid function in each entry.
  g_real = f;
  const vk::Core& core = vk::Get();
  // A new device of the renderer: the pool and the entries of the old one
  // are gone with it.
  if (g_pool != VK_NULL_HANDLE && g_pool_device != core.device) {
    g_pool = VK_NULL_HANDLE;
    g_count = 0;
    g_last = g_current = nullptr;
  }
  if (g_pool == VK_NULL_HANDLE && core.timestamp_ns > 0.0f) {
    g_pool_device = core.device;
    VkQueryPoolCreateInfo info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = kEntries * wp::kMostMarks;
    if (g_real.vkCreateQueryPool(core.device, &info, nullptr, &g_pool) != VK_SUCCESS) {
      g_pool = VK_NULL_HANDLE;
    }
    g_tick_ns = core.timestamp_ns;
    g_mask = core.timestamp_bits >= 64 ? ~0ull : (1ull << core.timestamp_bits) - 1;
  }
  f.vkBeginCommandBuffer = &Begin;
  f.vkEndCommandBuffer = &End;
  f.vkQueueSubmit2 = &Submit;
  f.vkCmdBeginRendering = &BeginRendering;
  f.vkCmdEndRendering = &EndRendering;
  f.vkCmdDraw = &Draw;
  f.vkCmdDrawIndexed = &DrawIndexed;
  f.vkCmdBindPipeline = &BindPipeline;
  f.vkCmdBindDescriptorSets = &BindSets;
  f.vkUpdateDescriptorSets = &UpdateSets;
  f.vkCmdCopyBufferToImage = &CopyBufferToImage;
  f.vkCmdBlitImage = &BlitImage;
  f.vkCmdCopyImageToBuffer = &CopyImageToBuffer;
  if (!g_said) {
    g_said = true;
    AppLog("gpu parts    : gpu_parts=1. The app counts the draws, the pipelines, the descriptor "
           "writes and the uploads of each command buffer of the renderer%s",
           g_pool != VK_NULL_HANDLE
               ? ", with a timestamp at each bound of a pass (up to 48 in a command buffer)"
               : ". The queue has no timestamps, so the parts have no time");
  }
}

void WalkProbeTake(walk_parts::Totals* out) {
  std::lock_guard<std::mutex> g(g_lock);
  *out = g_totals;
  g_totals.Reset();
}
