// A TEST RUNTIME OF OPENXR for the Android emulator (docs/android.md, "Test
// the APK", step 3). It is not in the APK.
//
// The emulator has no OpenXR runtime. With this library as the active
// runtime (/system/etc/openxr/1/active_runtime.json), the Khronos loader of
// the app loads it, and the app runs its session as in a headset. The
// runtime is a headset with two Touch controllers that follow a script:
//
//   <frame> point <hand> <layer> <x> <y>   the aim of the hand goes to the
//                                          pixel x, y of quad <layer> of the
//                                          last frame. <layer> menu: the last
//                                          quad of 1600 by 1200 pixels (the
//                                          menu of the app). keyboard: the
//                                          last quad of 1600 by 640 pixels
//                                          (the keyboard of the app).
//                                          screen: the first opaque quad (the
//                                          screen quad of a frame with no
//                                          scene)
//   <frame> point <hand> key <word>        the aim of the hand goes to the
//                                          middle of a key of the keyboard of
//                                          the app (keyboard_layout.h): a
//                                          letter or a digit, space, back or
//                                          enter
//   <frame> trigger <hand> <0|1>           the trigger of the hand
//   <frame> button <hand> <0|1>            B (right) or Y (left)
//   <frame> menu <hand> <0|1>              the menu button (left on a Touch
//                                          controller)
//   <frame> hold <name>                    the script waits at this frame
//                                          until the file <name> is in the
//                                          folder of the runtime (the test
//                                          puts it there). The frames of
//                                          the session go on
//   <frame> stick <hand> <x> <y>           the thumbstick
//   <frame> capture <name>                 the images of the quads of this
//                                          frame go to <name>-<layer>.bmp
//   <frame> quads <name>                   the images of the quads of this
//                                          frame but the menu and the cursor
//                                          of the app (1600 by 1200 and 64 by
//                                          64) go to <name>-<layer>.bmp
//   <frame> eyes <name>                    the images of the eyes of the
//                                          projection layer of this frame go
//                                          to <name>-p<eye>.bmp
//   <frame> recenter                       the LOCAL space recenters (an
//                                          event of a change of the space)
//   <frame> gaze <x> <y>                   the eye tracker looks to the
//                                          tangents x (right) and y (up) in
//                                          the space of the head. Before the
//                                          first gaze the eye tracker gives
//                                          no pose
//   <frame> exit                           the runtime ends the session
//
// The head stands still at the origin of the LOCAL space: each eye is 32 mm
// to its side, with the angles of view below (xrLocateViews). The grip pose
// of a hand is its aim pose. The headset has an eye tracker
// (XR_EXT_eye_gaze_interaction) when the app asks for the extension.
//
// <frame> is a frame of the script. The script counts the frames of the
// session, but it stops at a hold, so after a hold the frames of the script
// are behind the frames of the session. A line of the report has the frame
// of the session.
//
// <hand> is left or right. The script is script.txt in the folder
// /data/local/tmp/bwvr-xr, and the runtime writes report.txt and the
// pictures there. Each line of report.txt is one fact that the test reads:
// the calls of the app, the layers of the frames, the pulses.
//
// The runtime checks what a runtime of a headset needs: the types of the
// structs, the order of the calls of a swapchain image, the layouts of the
// layers, and the bindings for /interaction_profiles/oculus/touch_controller.
// A fault goes into the report as a line "fault ...". For a projection layer
// the report has the pose, the angles of view and the rect of each eye, one
// line for each change ("frame N projection ..."). For the quads the report
// has the pose, the size, the rect, the eye and the flags of each quad of a
// frame, one line for each change ("frame N quads ..."), and each picture
// that it reads back has the frame ("frame N capture ...").

#include <android/log.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_VULKAN
#include <jni.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>
#include <openxr/openxr_platform.h>

#include "../src/keyboard_layout.h"

namespace {

const char* kFolder = "/data/local/tmp/bwvr-xr";

// THE REPORT.
FILE* g_report = nullptr;
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

void Report(const char* format, ...) {
  char line[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  __android_log_write(ANDROID_LOG_INFO, "bwvr-xr-test", line);
  pthread_mutex_lock(&g_lock);
  if (g_report == nullptr) {
    char path[256];
    snprintf(path, sizeof(path), "%s/report.txt", kFolder);
    g_report = fopen(path, "w");
  }
  if (g_report != nullptr) {
    fprintf(g_report, "%s\n", line);
    fflush(g_report);
  }
  pthread_mutex_unlock(&g_lock);
}

int64_t NowNs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return static_cast<int64_t>(t.tv_sec) * 1000000000 + t.tv_nsec;
}

// THE SCRIPT.
struct Step {
  long frame;
  std::string what;
  int hand;  // 0 left, 1 right
  int layer;
  float a, b;
  std::string name;
};
std::vector<Step> g_script;

void LoadScript() {
  g_script.clear();
  char path[256];
  snprintf(path, sizeof(path), "%s/script.txt", kFolder);
  FILE* f = fopen(path, "r");
  if (f == nullptr) {
    Report("note no script at %s", path);
    return;
  }
  char line[256];
  while (fgets(line, sizeof(line), f) != nullptr) {
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
    Step s = {};
    char what[32] = {0}, hand[16] = {0}, name[64] = {0};
    if (sscanf(line, "%ld %31s", &s.frame, what) < 2) continue;
    s.what = what;
    const char* rest = strstr(line, what) + strlen(what);
    if (s.what == "point") {
      char layer[16] = {0};
      sscanf(rest, "%15s %15s %f %f", hand, layer, &s.a, &s.b);
      s.layer = strcmp(layer, "menu") == 0       ? -1
                : strcmp(layer, "keyboard") == 0 ? -2
                : strcmp(layer, "screen") == 0   ? -3
                                                 : atoi(layer);
      if (strcmp(layer, "key") == 0) {
        // The middle of the key on the keyboard of the app.
        char word[16] = {0};
        sscanf(rest, "%15s %15s %15s", hand, layer, word);
        const uint32_t code = strcmp(word, "space") == 0   ? ' '
                              : strcmp(word, "back") == 0  ? input_channel::kKeyBack
                              : strcmp(word, "enter") == 0 ? input_channel::kKeyEnter
                                                           : static_cast<uint8_t>(word[0]);
        s.layer = -2;
        s.a = s.b = -1.0f;
        for (int i = 0; i < keyboard_layout::KeyCount(); ++i) {
          keyboard_layout::Key key;
          keyboard_layout::Rect r;
          if (keyboard_layout::KeyAt(i, &key, &r) && key.code == code) {
            s.a = (r.l + r.r) * 0.5f;
            s.b = (r.t + r.b) * 0.5f;
          }
        }
        if (s.a < 0.0f) Report("fault the script names a key that the keyboard has not: %s", word);
      }
    } else if (s.what == "trigger" || s.what == "button" || s.what == "menu") {
      sscanf(rest, "%15s %f", hand, &s.a);
    } else if (s.what == "stick") {
      sscanf(rest, "%15s %f %f", hand, &s.a, &s.b);
    } else if (s.what == "gaze") {
      sscanf(rest, "%f %f", &s.a, &s.b);
    } else if (s.what == "capture" || s.what == "eyes" || s.what == "quads" ||
               s.what == "hold") {
      sscanf(rest, "%63s", name);
      s.name = name;
    }
    s.hand = strcmp(hand, "left") == 0 ? 0 : 1;
    g_script.push_back(s);
  }
  fclose(f);
  Report("note the script has %zu steps", g_script.size());
}

// THE OBJECTS. Each handle is a pointer to one of these.
struct Instance;
struct Session;

struct ActionSet {
  std::string name;
};

struct Action {
  ActionSet* set;
  std::string name;
  XrActionType type;
  std::vector<XrPath> subaction;
  std::vector<XrPath> bound;  // the paths of the touch profile
  bool gaze = false;  // bound to the gaze of the eye tracker
};

struct Space {
  bool action = false;
  Action* of = nullptr;
  XrPath subaction = XR_NULL_PATH;
  XrPosef pose;
};

struct Swapchain {
  Session* session;
  uint32_t width, height;
  int64_t format;
  std::vector<VkImage> images;
  std::vector<VkDeviceMemory> memory;
  int acquired = -1;
  bool waited = false;
  int released = -1;
};

struct Hand {
  XrVector3f position;
  XrQuaternionf orientation;
  bool trigger = false;
  bool button = false;
  bool menu = false;
  float stick[2] = {0, 0};
};

struct Instance {
  std::map<std::string, XrPath> paths;
  std::vector<std::string> names;
  std::vector<std::unique_ptr<Action>> actions;
  std::vector<std::unique_ptr<ActionSet>> sets;
  bool touch_bound = false;
  PFN_vkGetInstanceProcAddr vk_get = nullptr;
  VkInstance vk_instance = VK_NULL_HANDLE;
};

struct Session {
  Instance* instance;
  VkInstance vk_instance;
  VkPhysicalDevice physical;
  VkDevice device;
  uint32_t family;
  uint32_t queue_index;
  XrSessionState state = XR_SESSION_STATE_IDLE;
  bool begun = false;
  bool attached = false;
  long frame = 0;
  // The frame of the script, and true in the frame of the session in which
  // it came to that frame (a hold keeps it).
  long script = 0;
  bool script_new = false;
  std::string held;  // the name of the hold that waits, or empty
  int64_t last_wait = 0;
  bool in_frame = false;
  bool waited = false;
  Hand hands[2];
  std::vector<XrCompositionLayerQuad> last_quads;
  // The swapchains of the eyes of the projection layer of the last frame.
  std::vector<XrSwapchain> last_eyes;
  long haptics = 0;
  std::vector<std::unique_ptr<Swapchain>> swapchains;
  std::vector<std::unique_ptr<Space>> spaces;
  bool exit_asked = false;
};

std::vector<XrSessionState> g_events;
Instance* g_instance = nullptr;
// The one session of a test run.
Session* g_session = nullptr;
long g_faults = 0;
// The recenters of the script that the app did not poll yet.
int g_recenters_due = 0;
// The gaze of the script: the orientation of the pose of the eye tracker in
// the space of the head, and true after the first word gaze.
bool g_gaze_set = false;
XrQuaternionf g_gaze = {0.0f, 0.0f, 0.0f, 1.0f};
// The views of the head: the angles left, right, up and down in radians.
const float kFov[4] = {-0.9425f, 0.8203f, 0.8901f, -0.9599f};
const float kEyeX = 0.032f;
bool g_views_said = false;

void Fault(const char* format, ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  ++g_faults;
  Report("fault %s", line);
}

XrPath PathOf(Instance* in, const std::string& s) {
  auto it = in->paths.find(s);
  if (it != in->paths.end()) return it->second;
  in->names.push_back(s);
  const XrPath p = static_cast<XrPath>(in->names.size());
  in->paths[s] = p;
  return p;
}

std::string NameOf(Instance* in, XrPath p) {
  if (p == XR_NULL_PATH || p > in->names.size()) return "";
  return in->names[p - 1];
}

void Push(Session* s, XrSessionState state) {
  s->state = state;
  g_events.push_back(state);
}

// THE VULKAN OF THE APP, for the images of the swapchains.
struct Vk {
  PFN_vkGetDeviceProcAddr get_device = nullptr;
  PFN_vkCreateImage create_image = nullptr;
  PFN_vkDestroyImage destroy_image = nullptr;
  PFN_vkGetImageMemoryRequirements requirements = nullptr;
  PFN_vkAllocateMemory allocate = nullptr;
  PFN_vkFreeMemory free = nullptr;
  PFN_vkBindImageMemory bind = nullptr;
  PFN_vkGetPhysicalDeviceMemoryProperties memory_properties = nullptr;
  PFN_vkCreateCommandPool create_pool = nullptr;
  PFN_vkAllocateCommandBuffers allocate_cmd = nullptr;
  PFN_vkBeginCommandBuffer begin = nullptr;
  PFN_vkEndCommandBuffer end = nullptr;
  PFN_vkCmdPipelineBarrier barrier = nullptr;
  PFN_vkCmdCopyImageToBuffer copy = nullptr;
  PFN_vkCreateBuffer create_buffer = nullptr;
  PFN_vkGetBufferMemoryRequirements buffer_requirements = nullptr;
  PFN_vkBindBufferMemory bind_buffer = nullptr;
  PFN_vkMapMemory map = nullptr;
  PFN_vkGetDeviceQueue get_queue = nullptr;
  PFN_vkQueueSubmit submit = nullptr;
  PFN_vkQueueWaitIdle wait_idle = nullptr;
  PFN_vkResetCommandBuffer reset = nullptr;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
  void* mapped = nullptr;
  VkDeviceSize buffer_size = 0;
  VkPhysicalDeviceMemoryProperties props = {};
} g_vk;

bool LoadVk(Session* s) {
  Instance* in = s->instance;
  auto gi = [&](const char* n) { return in->vk_get(s->vk_instance, n); };
  g_vk.get_device = reinterpret_cast<PFN_vkGetDeviceProcAddr>(gi("vkGetDeviceProcAddr"));
  g_vk.memory_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
      gi("vkGetPhysicalDeviceMemoryProperties"));
  if (g_vk.get_device == nullptr || g_vk.memory_properties == nullptr) return false;
  auto gd = [&](const char* n) { return g_vk.get_device(s->device, n); };
#define BWVR_LOAD(field, type, name) \
  g_vk.field = reinterpret_cast<type>(gd(name)); \
  if (g_vk.field == nullptr) return false;
  BWVR_LOAD(create_image, PFN_vkCreateImage, "vkCreateImage")
  BWVR_LOAD(destroy_image, PFN_vkDestroyImage, "vkDestroyImage")
  BWVR_LOAD(requirements, PFN_vkGetImageMemoryRequirements, "vkGetImageMemoryRequirements")
  BWVR_LOAD(allocate, PFN_vkAllocateMemory, "vkAllocateMemory")
  BWVR_LOAD(free, PFN_vkFreeMemory, "vkFreeMemory")
  BWVR_LOAD(bind, PFN_vkBindImageMemory, "vkBindImageMemory")
  BWVR_LOAD(create_pool, PFN_vkCreateCommandPool, "vkCreateCommandPool")
  BWVR_LOAD(allocate_cmd, PFN_vkAllocateCommandBuffers, "vkAllocateCommandBuffers")
  BWVR_LOAD(begin, PFN_vkBeginCommandBuffer, "vkBeginCommandBuffer")
  BWVR_LOAD(end, PFN_vkEndCommandBuffer, "vkEndCommandBuffer")
  BWVR_LOAD(barrier, PFN_vkCmdPipelineBarrier, "vkCmdPipelineBarrier")
  BWVR_LOAD(copy, PFN_vkCmdCopyImageToBuffer, "vkCmdCopyImageToBuffer")
  BWVR_LOAD(create_buffer, PFN_vkCreateBuffer, "vkCreateBuffer")
  BWVR_LOAD(buffer_requirements, PFN_vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements")
  BWVR_LOAD(bind_buffer, PFN_vkBindBufferMemory, "vkBindBufferMemory")
  BWVR_LOAD(map, PFN_vkMapMemory, "vkMapMemory")
  BWVR_LOAD(get_queue, PFN_vkGetDeviceQueue, "vkGetDeviceQueue")
  BWVR_LOAD(submit, PFN_vkQueueSubmit, "vkQueueSubmit")
  BWVR_LOAD(wait_idle, PFN_vkQueueWaitIdle, "vkQueueWaitIdle")
  BWVR_LOAD(reset, PFN_vkResetCommandBuffer, "vkResetCommandBuffer")
#undef BWVR_LOAD
  g_vk.memory_properties(s->physical, &g_vk.props);
  VkCommandPoolCreateInfo pool = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool.queueFamilyIndex = s->family;
  if (g_vk.create_pool(s->device, &pool, nullptr, &g_vk.pool) != VK_SUCCESS) return false;
  VkCommandBufferAllocateInfo alloc = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  alloc.commandPool = g_vk.pool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1;
  return g_vk.allocate_cmd(s->device, &alloc, &g_vk.cmd) == VK_SUCCESS;
}

uint32_t MemoryType(uint32_t bits, VkMemoryPropertyFlags want) {
  for (uint32_t i = 0; i < g_vk.props.memoryTypeCount; ++i) {
    if ((bits & (1u << i)) && (g_vk.props.memoryTypes[i].propertyFlags & want) == want) return i;
  }
  return UINT32_MAX;
}

void Barrier(VkImage image, VkImageLayout from, VkImageLayout to) {
  VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
  b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  b.oldLayout = from;
  b.newLayout = to;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = image;
  b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  g_vk.barrier(g_vk.cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
}

// The released image of a swapchain into a BMP. The app gives a released
// image in the layout of a color attachment.
void Capture(Session* s, Swapchain* sc, const std::string& path) {
  if (sc->released < 0) {
    Fault("capture of a swapchain with no released image");
    return;
  }
  const VkDeviceSize bytes = static_cast<VkDeviceSize>(sc->width) * sc->height * 4;
  if (g_vk.buffer_size < bytes) {
    VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = bytes;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    g_vk.create_buffer(s->device, &info, nullptr, &g_vk.buffer);
    VkMemoryRequirements req;
    g_vk.buffer_requirements(s->device, g_vk.buffer, &req);
    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = MemoryType(req.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    g_vk.allocate(s->device, &alloc, nullptr, &g_vk.buffer_memory);
    g_vk.bind_buffer(s->device, g_vk.buffer, g_vk.buffer_memory, 0);
    g_vk.map(s->device, g_vk.buffer_memory, 0, bytes, 0, &g_vk.mapped);
    g_vk.buffer_size = bytes;
  }
  VkQueue queue = VK_NULL_HANDLE;
  g_vk.get_queue(s->device, s->family, s->queue_index, &queue);
  g_vk.wait_idle(queue);
  g_vk.reset(g_vk.cmd, 0);
  VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  g_vk.begin(g_vk.cmd, &begin);
  const VkImage image = sc->images[sc->released];
  Barrier(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  VkBufferImageCopy region = {};
  region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  region.imageExtent = {sc->width, sc->height, 1};
  g_vk.copy(g_vk.cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, g_vk.buffer, 1, &region);
  Barrier(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  g_vk.end(g_vk.cmd);
  VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &g_vk.cmd;
  g_vk.submit(queue, 1, &submit, VK_NULL_HANDLE);
  g_vk.wait_idle(queue);

  const bool bgra = sc->format == VK_FORMAT_B8G8R8A8_SRGB || sc->format == VK_FORMAT_B8G8R8A8_UNORM;
  FILE* f = fopen(path.c_str(), "wb");
  if (f == nullptr) {
    Fault("cannot write %s", path.c_str());
    return;
  }
  const uint32_t w = sc->width, h = sc->height, data = w * h * 4;
  uint8_t header[54] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) {
    for (int i = 0; i < 4; ++i) header[at + i] = static_cast<uint8_t>(v >> (8 * i));
  };
  put32(2, 54 + data);
  put32(10, 54);
  put32(14, 40);
  put32(18, w);
  put32(22, h);
  header[26] = 1;
  header[28] = 32;
  put32(34, data);
  fwrite(header, 1, sizeof(header), f);
  const uint8_t* px = static_cast<const uint8_t*>(g_vk.mapped);
  std::vector<uint8_t> row(w * 4);
  long lit = 0;
  for (int y = static_cast<int>(h) - 1; y >= 0; --y) {
    const uint8_t* src = px + static_cast<size_t>(y) * w * 4;
    for (uint32_t x = 0; x < w; ++x) {
      const uint8_t r = bgra ? src[x * 4 + 2] : src[x * 4];
      const uint8_t g = src[x * 4 + 1];
      const uint8_t b = bgra ? src[x * 4] : src[x * 4 + 2];
      row[x * 4] = b;
      row[x * 4 + 1] = g;
      row[x * 4 + 2] = r;
      row[x * 4 + 3] = src[x * 4 + 3];
      if (r + g + b > 120) ++lit;
    }
    fwrite(row.data(), 1, row.size(), f);
  }
  fclose(f);
  Report("frame %ld capture %s %ux%u lit %ld", s->frame, path.c_str(), w, h, lit);
}

// THE AIM: from the hand at its place to a pixel of a quad of the last frame.
void PointAt(Session* s, int hand, int layer, float px, float py) {
  // The menu and the keyboard: the last quad of the size of their picture.
  // The screen: the first opaque quad.
  const int asked = layer;
  if (layer == -1 || layer == -2) {
    const int32_t want_h = layer == -1 ? 1200 : 640;
    for (int i = static_cast<int>(s->last_quads.size()) - 1; i >= 0; --i) {
      const XrCompositionLayerQuad& m = s->last_quads[i];
      if (m.subImage.imageRect.extent.width == 1600 && m.subImage.imageRect.extent.height == want_h) {
        layer = i;
        break;
      }
    }
  } else if (layer == -3) {
    for (int i = 0; i < static_cast<int>(s->last_quads.size()); ++i) {
      if (s->last_quads[i].layerFlags == 0) {
        layer = i;
        break;
      }
    }
  }
  if (layer < 0 && asked < 0) {
    Fault("point at the %s, the last frame had no such quad",
          asked == -1 ? "menu" : asked == -2 ? "keyboard" : "screen");
    return;
  }
  if (layer < 0 || layer >= static_cast<int>(s->last_quads.size())) {
    Fault("point at layer %d, the last frame had %zu quads", layer, s->last_quads.size());
    return;
  }
  const XrCompositionLayerQuad& q = s->last_quads[layer];
  const uint32_t iw = static_cast<uint32_t>(q.subImage.imageRect.extent.width);
  const uint32_t ih = static_cast<uint32_t>(q.subImage.imageRect.extent.height);
  // The point in the frame of the quad, turned by its pose.
  const float lx = (px / iw - 0.5f) * q.size.width;
  const float ly = (0.5f - py / ih) * q.size.height;
  const XrQuaternionf& o = q.pose.orientation;
  // v + w * t + cross(o.xyz, t), t = 2 * cross(o.xyz, v), with v = (lx, ly, 0).
  const float cx = 2.0f * (o.y * 0.0f - o.z * ly);
  const float cy2 = 2.0f * (o.z * lx - o.x * 0.0f);
  const float cz = 2.0f * (o.x * ly - o.y * lx);
  const float tx = q.pose.position.x + lx + o.w * cx + (o.y * cz - o.z * cy2);
  const float ty = q.pose.position.y + ly + o.w * cy2 + (o.z * cx - o.x * cz);
  const float tz = q.pose.position.z + 0.0f + o.w * cz + (o.x * cy2 - o.y * cx);
  Hand& hd = s->hands[hand];
  float fx = tx - hd.position.x, fy = ty - hd.position.y, fz = tz - hd.position.z;
  const float len = sqrtf(fx * fx + fy * fy + fz * fz);
  fx /= len;
  fy /= len;
  fz /= len;
  const float pitch = asinf(fy);
  const float yaw = atan2f(-fx, -fz);
  // q = yaw about y, then pitch about x.
  const float cy = cosf(yaw * 0.5f), sy = sinf(yaw * 0.5f);
  const float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
  hd.orientation = {cy * sp, sy * cp, -sy * sp, cy * cp};
}

// The script goes to its next frame, except at a hold whose file is not
// there yet.
void StepScript(Session* s) {
  if (!s->held.empty()) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", kFolder, s->held.c_str());
    if (access(path, F_OK) != 0) {
      s->script_new = false;
      return;
    }
    Report("frame %ld go %s script %ld", s->frame, s->held.c_str(), s->script);
    s->held.clear();
  }
  ++s->script;
  s->script_new = true;
}

void RunScript(Session* s) {
  if (!s->script_new) return;
  for (const Step& st : g_script) {
    if (st.frame != s->script) continue;
    Hand& h = s->hands[st.hand];
    if (st.what == "point") {
      PointAt(s, st.hand, st.layer, st.a, st.b);
      const Hand& hp = s->hands[st.hand];
      Report("frame %ld point %d %d %.0f %.0f pose %.4f %.4f %.4f %.4f %.4f %.4f %.4f", s->frame,
             st.hand, st.layer, st.a, st.b, hp.position.x, hp.position.y, hp.position.z,
             hp.orientation.x, hp.orientation.y, hp.orientation.z, hp.orientation.w);
    } else if (st.what == "trigger") {
      h.trigger = st.a != 0.0f;
      Report("frame %ld trigger %d %d", s->frame, st.hand, h.trigger ? 1 : 0);
    } else if (st.what == "button") {
      h.button = st.a != 0.0f;
      Report("frame %ld button %d %d", s->frame, st.hand, h.button ? 1 : 0);
    } else if (st.what == "menu") {
      h.menu = st.a != 0.0f;
      Report("frame %ld menu %d %d", s->frame, st.hand, h.menu ? 1 : 0);
    } else if (st.what == "hold") {
      s->held = st.name;
      Report("frame %ld hold %s script %ld", s->frame, st.name.c_str(), s->script);
    } else if (st.what == "stick") {
      h.stick[0] = st.a;
      h.stick[1] = st.b;
    } else if (st.what == "recenter") {
      ++g_recenters_due;
      Report("frame %ld recenter", s->frame);
    } else if (st.what == "gaze") {
      // The turn from -z to the direction (a, b, -1): a yaw about y, then a
      // pitch about x.
      const float yaw = -atanf(st.a);
      const float pitch = atanf(st.b / sqrtf(1.0f + st.a * st.a));
      const float cy = cosf(yaw * 0.5f), sy = sinf(yaw * 0.5f);
      const float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
      g_gaze = {cy * sp, sy * cp, -sy * sp, cy * cp};
      g_gaze_set = true;
      Report("frame %ld gaze %.3f %.3f", s->frame, st.a, st.b);
    } else if (st.what == "exit" && !s->exit_asked) {
      s->exit_asked = true;
      Report("frame %ld exit", s->frame);
      Push(s, XR_SESSION_STATE_VISIBLE);
      Push(s, XR_SESSION_STATE_SYNCHRONIZED);
      Push(s, XR_SESSION_STATE_STOPPING);
    }
  }
}

// THE INPUT of an action from the touch profile.
bool BoundTo(Action* a, Instance* in, int hand, const char* input) {
  const std::string side = hand == 0 ? "/user/hand/left" : "/user/hand/right";
  for (XrPath p : a->bound) {
    if (NameOf(in, p) == side + input) return true;
  }
  return false;
}

int HandOf(Instance* in, XrPath subaction) {
  const std::string n = NameOf(in, subaction);
  if (n == "/user/hand/left") return 0;
  if (n == "/user/hand/right") return 1;
  return -1;
}

// ---- The functions of OpenXR -------------------------------------------

XrResult XRAPI_CALL GetInstanceProcAddr(XrInstance instance, const char* name,
                                        PFN_xrVoidFunction* function);

XrResult XRAPI_CALL EnumerateInstanceExtensionProperties(
    const char* layer, uint32_t capacity, uint32_t* count, XrExtensionProperties* out) {
  static const char* kNames[] = {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME,
                                 XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
                                 XR_KHR_LOADER_INIT_EXTENSION_NAME,
                                 XR_KHR_LOADER_INIT_ANDROID_EXTENSION_NAME,
                                 XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME};
  *count = 5;
  if (capacity == 0) return XR_SUCCESS;
  if (capacity < 5) return XR_ERROR_SIZE_INSUFFICIENT;
  for (uint32_t i = 0; i < 5; ++i) {
    strcpy(out[i].extensionName, kNames[i]);
    out[i].extensionVersion = 1;
  }
  return XR_SUCCESS;
}

XrResult XRAPI_CALL InitializeLoader(const XrLoaderInitInfoBaseHeaderKHR* info) {
  Report("call xrInitializeLoaderKHR type %d", static_cast<int>(info->type));
  return XR_SUCCESS;
}

XrResult XRAPI_CALL CreateInstance(const XrInstanceCreateInfo* info, XrInstance* out) {
  if (info->type != XR_TYPE_INSTANCE_CREATE_INFO) Fault("xrCreateInstance: a wrong type");
  bool android = false;
  for (const XrBaseInStructure* n = static_cast<const XrBaseInStructure*>(info->next); n != nullptr; n = n->next) {
    if (n->type == XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR) android = true;
  }
  if (!android) Fault("xrCreateInstance with no XrInstanceCreateInfoAndroidKHR");
  std::string ext;
  for (uint32_t i = 0; i < info->enabledExtensionCount; ++i) {
    ext += std::string(" ") + info->enabledExtensionNames[i];
  }
  Report("call xrCreateInstance app %s api %u.%u extensions%s", info->applicationInfo.applicationName,
         XR_VERSION_MAJOR(info->applicationInfo.apiVersion),
         XR_VERSION_MINOR(info->applicationInfo.apiVersion), ext.c_str());
  LoadScript();
  g_instance = new Instance();
  *out = reinterpret_cast<XrInstance>(g_instance);
  return XR_SUCCESS;
}

XrResult XRAPI_CALL DestroyInstance(XrInstance instance) {
  Report("call xrDestroyInstance faults %ld", g_faults);
  delete reinterpret_cast<Instance*>(instance);
  g_instance = nullptr;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetInstanceProperties(XrInstance, XrInstanceProperties* p) {
  p->runtimeVersion = XR_MAKE_VERSION(1, 0, 0);
  strcpy(p->runtimeName, "bwvr test runtime");
  return XR_SUCCESS;
}

XrResult XRAPI_CALL StringToPath(XrInstance instance, const char* s, XrPath* out) {
  *out = PathOf(reinterpret_cast<Instance*>(instance), s);
  return XR_SUCCESS;
}

XrResult XRAPI_CALL PathToString(XrInstance instance, XrPath p, uint32_t capacity,
                                 uint32_t* count, char* out) {
  const std::string n = NameOf(reinterpret_cast<Instance*>(instance), p);
  *count = static_cast<uint32_t>(n.size() + 1);
  if (capacity == 0) return XR_SUCCESS;
  if (capacity < *count) return XR_ERROR_SIZE_INSUFFICIENT;
  memcpy(out, n.c_str(), n.size() + 1);
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetSystem(XrInstance, const XrSystemGetInfo* info, XrSystemId* out) {
  if (info->formFactor != XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY) return XR_ERROR_FORM_FACTOR_UNSUPPORTED;
  *out = 1;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetSystemProperties(XrInstance, XrSystemId, XrSystemProperties* p) {
  strcpy(p->systemName, "bwvr test headset");
  p->vendorId = 0xB3;
  p->graphicsProperties.maxSwapchainImageWidth = 4096;
  p->graphicsProperties.maxSwapchainImageHeight = 4096;
  p->graphicsProperties.maxLayerCount = 16;
  p->trackingProperties.orientationTracking = XR_TRUE;
  p->trackingProperties.positionTracking = XR_TRUE;
  for (XrBaseOutStructure* n = static_cast<XrBaseOutStructure*>(p->next); n != nullptr; n = n->next) {
    if (n->type == XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT) {
      reinterpret_cast<XrSystemEyeGazeInteractionPropertiesEXT*>(n)->supportsEyeGazeInteraction = XR_TRUE;
      Report("call xrGetSystemProperties eye gaze");
    }
  }
  return XR_SUCCESS;
}

XrResult XRAPI_CALL EnumerateViewConfigurations(XrInstance, XrSystemId, uint32_t capacity,
                                                uint32_t* count, XrViewConfigurationType* out) {
  *count = 1;
  if (capacity >= 1) out[0] = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL EnumerateViewConfigurationViews(XrInstance, XrSystemId, XrViewConfigurationType,
                                                    uint32_t capacity, uint32_t* count,
                                                    XrViewConfigurationView* out) {
  *count = 2;
  if (capacity == 0) return XR_SUCCESS;
  for (uint32_t i = 0; i < 2 && i < capacity; ++i) {
    // The numbers of a Quest 3.
    out[i].recommendedImageRectWidth = 2064;
    out[i].recommendedImageRectHeight = 2208;
    out[i].maxImageRectWidth = 4096;
    out[i].maxImageRectHeight = 4096;
    out[i].recommendedSwapchainSampleCount = 1;
    out[i].maxSwapchainSampleCount = 1;
  }
  return XR_SUCCESS;
}

XrResult XRAPI_CALL EnumerateEnvironmentBlendModes(XrInstance, XrSystemId, XrViewConfigurationType,
                                                   uint32_t capacity, uint32_t* count,
                                                   XrEnvironmentBlendMode* out) {
  *count = 1;
  if (capacity >= 1) out[0] = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetVulkanGraphicsRequirements2(XrInstance, XrSystemId,
                                                   XrGraphicsRequirementsVulkanKHR* r) {
  if (r->type != XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR) Fault("the graphics requirements have a wrong type");
  r->minApiVersionSupported = XR_MAKE_VERSION(1, 0, 0);
  r->maxApiVersionSupported = XR_MAKE_VERSION(1, 3, 0);
  Report("call xrGetVulkanGraphicsRequirements2KHR");
  return XR_SUCCESS;
}

XrResult XRAPI_CALL CreateVulkanInstance(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* info,
                                         VkInstance* out, VkResult* vr) {
  Instance* in = reinterpret_cast<Instance*>(instance);
  in->vk_get = info->pfnGetInstanceProcAddr;
  auto create = reinterpret_cast<PFN_vkCreateInstance>(in->vk_get(VK_NULL_HANDLE, "vkCreateInstance"));
  *vr = create(info->vulkanCreateInfo, info->vulkanAllocator, out);
  in->vk_instance = *out;
  Report("call xrCreateVulkanInstanceKHR %d", static_cast<int>(*vr));
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetVulkanGraphicsDevice2(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* info,
                                             VkPhysicalDevice* out) {
  Instance* in = reinterpret_cast<Instance*>(instance);
  auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
      in->vk_get(info->vulkanInstance, "vkEnumeratePhysicalDevices"));
  uint32_t count = 1;
  VkPhysicalDevice first = VK_NULL_HANDLE;
  enumerate(info->vulkanInstance, &count, &first);
  *out = first;
  Report("call xrGetVulkanGraphicsDevice2KHR %s", first != VK_NULL_HANDLE ? "a device" : "no device");
  return first != VK_NULL_HANDLE ? XR_SUCCESS : XR_ERROR_RUNTIME_FAILURE;
}

XrResult XRAPI_CALL CreateVulkanDevice(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* info,
                                       VkDevice* out, VkResult* vr) {
  Instance* in = reinterpret_cast<Instance*>(instance);
  auto create = reinterpret_cast<PFN_vkCreateDevice>(in->vk_get(in->vk_instance, "vkCreateDevice"));
  *vr = create(info->vulkanPhysicalDevice, info->vulkanCreateInfo, info->vulkanAllocator, out);
  Report("call xrCreateVulkanDeviceKHR %d", static_cast<int>(*vr));
  return XR_SUCCESS;
}

XrResult XRAPI_CALL CreateSession(XrInstance instance, const XrSessionCreateInfo* info, XrSession* out) {
  Instance* in = reinterpret_cast<Instance*>(instance);
  const auto* binding = static_cast<const XrGraphicsBindingVulkanKHR*>(info->next);
  if (binding == nullptr || binding->type != XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR) {
    Fault("xrCreateSession with no binding of Vulkan");
    return XR_ERROR_GRAPHICS_DEVICE_INVALID;
  }
  Session* s = new Session();
  s->instance = in;
  s->vk_instance = binding->instance;
  s->physical = binding->physicalDevice;
  s->device = binding->device;
  s->family = binding->queueFamilyIndex;
  s->queue_index = binding->queueIndex;
  for (int h = 0; h < 2; ++h) {
    s->hands[h].position = {h == 0 ? -0.2f : 0.2f, -0.35f, -0.3f};
    s->hands[h].orientation = {0, 0, 0, 1};
  }
  if (!LoadVk(s)) Fault("the functions of Vulkan of the app are missing");
  g_session = s;
  Report("call xrCreateSession family %u queue %u", s->family, s->queue_index);
  Push(s, XR_SESSION_STATE_IDLE);
  Push(s, XR_SESSION_STATE_READY);
  *out = reinterpret_cast<XrSession>(s);
  return XR_SUCCESS;
}

XrResult XRAPI_CALL DestroySession(XrSession session) {
  Session* s = reinterpret_cast<Session*>(session);
  Report("call xrDestroySession frames %ld haptics %ld", s->frame, s->haptics);
  delete s;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL PollEvent(XrInstance, XrEventDataBuffer* event) {
  if (g_recenters_due > 0 && g_session != nullptr) {
    --g_recenters_due;
    auto* e = reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(event);
    *e = {XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING};
    e->session = reinterpret_cast<XrSession>(g_session);
    e->referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    e->changeTime = NowNs();
    e->poseValid = XR_TRUE;
    e->poseInPreviousSpace.orientation.w = 1.0f;
    return XR_SUCCESS;
  }
  if (g_events.empty()) return XR_EVENT_UNAVAILABLE;
  auto* e = reinterpret_cast<XrEventDataSessionStateChanged*>(event);
  e->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
  e->next = nullptr;
  e->session = reinterpret_cast<XrSession>(g_session);
  e->state = g_events.front();
  e->time = NowNs();
  g_events.erase(g_events.begin());
  return XR_SUCCESS;
}

XrResult XRAPI_CALL BeginSession(XrSession session, const XrSessionBeginInfo* info) {
  Session* s = reinterpret_cast<Session*>(session);
  if (s->state != XR_SESSION_STATE_READY) Fault("xrBeginSession in the state %d", s->state);
  if (info->primaryViewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) Fault("not stereo");
  s->begun = true;
  Report("call xrBeginSession");
  Push(s, XR_SESSION_STATE_SYNCHRONIZED);
  Push(s, XR_SESSION_STATE_VISIBLE);
  Push(s, XR_SESSION_STATE_FOCUSED);
  return XR_SUCCESS;
}

XrResult XRAPI_CALL EndSession(XrSession session) {
  Session* s = reinterpret_cast<Session*>(session);
  if (s->state != XR_SESSION_STATE_STOPPING) Fault("xrEndSession in the state %d", s->state);
  s->begun = false;
  Report("call xrEndSession");
  Push(s, XR_SESSION_STATE_IDLE);
  Push(s, XR_SESSION_STATE_EXITING);
  return XR_SUCCESS;
}

XrResult XRAPI_CALL RequestExitSession(XrSession session) {
  Session* s = reinterpret_cast<Session*>(session);
  Push(s, XR_SESSION_STATE_STOPPING);
  return XR_SUCCESS;
}

XrResult XRAPI_CALL CreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* info,
                                         XrSpace* out) {
  Session* s = reinterpret_cast<Session*>(session);
  auto space = std::make_unique<Space>();
  space->pose = info->poseInReferenceSpace;
  Report("call xrCreateReferenceSpace %d", static_cast<int>(info->referenceSpaceType));
  *out = reinterpret_cast<XrSpace>(space.get());
  s->spaces.push_back(std::move(space));
  return XR_SUCCESS;
}

XrResult XRAPI_CALL CreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* info, XrSpace* out) {
  Session* s = reinterpret_cast<Session*>(session);
  auto space = std::make_unique<Space>();
  space->action = true;
  space->of = reinterpret_cast<Action*>(info->action);
  space->subaction = info->subactionPath;
  space->pose = info->poseInActionSpace;
  *out = reinterpret_cast<XrSpace>(space.get());
  s->spaces.push_back(std::move(space));
  return XR_SUCCESS;
}

XrResult XRAPI_CALL DestroySpace(XrSpace) { return XR_SUCCESS; }

XrResult XRAPI_CALL LocateSpace(XrSpace space, XrSpace, XrTime, XrSpaceLocation* where) {
  Space* sp = reinterpret_cast<Space*>(space);
  where->locationFlags = 0;
  if (!sp->action) {
    where->pose = sp->pose;
    where->locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
                           XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    return XR_SUCCESS;
  }
  Session* s = g_session;
  if (s == nullptr) return XR_SUCCESS;
  if (sp->of != nullptr && sp->of->gaze) {
    if (!g_gaze_set) return XR_SUCCESS;
    where->pose.position = {0.0f, 0.0f, 0.0f};
    where->pose.orientation = g_gaze;
    where->locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
                           XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    return XR_SUCCESS;
  }
  const int hand = HandOf(g_instance, sp->subaction);
  if (hand < 0 || (!BoundTo(sp->of, g_instance, hand, "/input/aim/pose") &&
                   !BoundTo(sp->of, g_instance, hand, "/input/grip/pose"))) {
    return XR_SUCCESS;
  }
  where->pose.position = s->hands[hand].position;
  where->pose.orientation = s->hands[hand].orientation;
  where->locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
                         XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL EnumerateSwapchainFormats(XrSession, uint32_t capacity, uint32_t* count, int64_t* out) {
  static const int64_t kFormats[] = {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM};
  *count = 3;
  if (capacity == 0) return XR_SUCCESS;
  for (uint32_t i = 0; i < 3 && i < capacity; ++i) out[i] = kFormats[i];
  return XR_SUCCESS;
}

XrResult XRAPI_CALL CreateSwapchain(XrSession session, const XrSwapchainCreateInfo* info, XrSwapchain* out) {
  Session* s = reinterpret_cast<Session*>(session);
  auto sc = std::make_unique<Swapchain>();
  sc->session = s;
  sc->width = info->width;
  sc->height = info->height;
  sc->format = info->format;
  VkImageUsageFlags usage = 0;
  if (info->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (info->usageFlags & XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT) usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (info->usageFlags & XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if (info->usageFlags & XR_SWAPCHAIN_USAGE_SAMPLED_BIT) usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  // The compositor reads each image, as the runtime of a headset does.
  usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  for (int i = 0; i < 3; ++i) {
    VkImageCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = static_cast<VkFormat>(info->format);
    ci.extent = {info->width, info->height, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = info->arraySize;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    VkImage image = VK_NULL_HANDLE;
    if (g_vk.create_image(s->device, &ci, nullptr, &image) != VK_SUCCESS) {
      Fault("vkCreateImage for a swapchain failed");
      return XR_ERROR_RUNTIME_FAILURE;
    }
    VkMemoryRequirements req;
    g_vk.requirements(s->device, image, &req);
    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = MemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory memory = VK_NULL_HANDLE;
    g_vk.allocate(s->device, &alloc, nullptr, &memory);
    g_vk.bind(s->device, image, memory, 0);
    sc->images.push_back(image);
    sc->memory.push_back(memory);
  }
  Report("call xrCreateSwapchain %ux%u format %lld usage 0x%llX", info->width, info->height,
         static_cast<long long>(info->format), static_cast<unsigned long long>(info->usageFlags));
  *out = reinterpret_cast<XrSwapchain>(sc.get());
  s->swapchains.push_back(std::move(sc));
  return XR_SUCCESS;
}

XrResult XRAPI_CALL DestroySwapchain(XrSwapchain) { return XR_SUCCESS; }

XrResult XRAPI_CALL EnumerateSwapchainImages(XrSwapchain swapchain, uint32_t capacity, uint32_t* count,
                                             XrSwapchainImageBaseHeader* out) {
  Swapchain* sc = reinterpret_cast<Swapchain*>(swapchain);
  *count = static_cast<uint32_t>(sc->images.size());
  if (capacity == 0) return XR_SUCCESS;
  auto* images = reinterpret_cast<XrSwapchainImageVulkanKHR*>(out);
  for (uint32_t i = 0; i < *count && i < capacity; ++i) {
    if (images[i].type != XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR) Fault("a swapchain image of a wrong type");
    images[i].image = sc->images[i];
  }
  return XR_SUCCESS;
}

XrResult XRAPI_CALL AcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo*,
                                          uint32_t* index) {
  Swapchain* sc = reinterpret_cast<Swapchain*>(swapchain);
  if (sc->acquired >= 0) Fault("xrAcquireSwapchainImage with an image that is not released");
  sc->acquired = (sc->released + 1) % static_cast<int>(sc->images.size());
  sc->waited = false;
  *index = static_cast<uint32_t>(sc->acquired);
  return XR_SUCCESS;
}

XrResult XRAPI_CALL WaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo*) {
  Swapchain* sc = reinterpret_cast<Swapchain*>(swapchain);
  if (sc->acquired < 0) Fault("xrWaitSwapchainImage with no acquired image");
  sc->waited = true;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL ReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo*) {
  Swapchain* sc = reinterpret_cast<Swapchain*>(swapchain);
  if (sc->acquired < 0 || !sc->waited) Fault("xrReleaseSwapchainImage with no waited image");
  sc->released = sc->acquired;
  sc->acquired = -1;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL CreateActionSet(XrInstance instance, const XrActionSetCreateInfo* info, XrActionSet* out) {
  Instance* in = reinterpret_cast<Instance*>(instance);
  auto set = std::make_unique<ActionSet>();
  set->name = info->actionSetName;
  *out = reinterpret_cast<XrActionSet>(set.get());
  in->sets.push_back(std::move(set));
  return XR_SUCCESS;
}

XrResult XRAPI_CALL DestroyActionSet(XrActionSet) { return XR_SUCCESS; }

XrResult XRAPI_CALL CreateAction(XrActionSet set, const XrActionCreateInfo* info, XrAction* out) {
  auto a = std::make_unique<Action>();
  a->set = reinterpret_cast<ActionSet*>(set);
  a->name = info->actionName;
  a->type = info->actionType;
  for (uint32_t i = 0; i < info->countSubactionPaths; ++i) a->subaction.push_back(info->subactionPaths[i]);
  *out = reinterpret_cast<XrAction>(a.get());
  g_instance->actions.push_back(std::move(a));
  return XR_SUCCESS;
}

XrResult XRAPI_CALL DestroyAction(XrAction) { return XR_SUCCESS; }

XrResult XRAPI_CALL SuggestBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* info) {
  Instance* in = reinterpret_cast<Instance*>(instance);
  const std::string profile = NameOf(in, info->interactionProfile);
  Report("call xrSuggestInteractionProfileBindings %s %u", profile.c_str(), info->countSuggestedBindings);
  if (profile == "/interaction_profiles/ext/eye_gaze_interaction") {
    for (uint32_t i = 0; i < info->countSuggestedBindings; ++i) {
      const std::string p = NameOf(in, info->suggestedBindings[i].binding);
      if (p != "/user/eyes_ext/input/gaze_ext/pose") {
        Fault("the eye gaze profile has no path %s", p.c_str());
        return XR_ERROR_PATH_UNSUPPORTED;
      }
      reinterpret_cast<Action*>(info->suggestedBindings[i].action)->gaze = true;
    }
    return XR_SUCCESS;
  }
  if (profile != "/interaction_profiles/oculus/touch_controller") return XR_SUCCESS;
  // The inputs of a Touch controller. A path that the profile has not is a
  // fault of the app.
  static const char* kInputs[] = {"/input/aim/pose", "/input/grip/pose", "/input/trigger/value",
                                  "/input/trigger/touch", "/input/squeeze/value", "/input/thumbstick",
                                  "/input/thumbstick/click", "/output/haptic"};
  for (uint32_t i = 0; i < info->countSuggestedBindings; ++i) {
    const std::string p = NameOf(in, info->suggestedBindings[i].binding);
    bool known = p == "/user/hand/right/input/a/click" || p == "/user/hand/right/input/b/click" ||
                 p == "/user/hand/left/input/x/click" || p == "/user/hand/left/input/y/click" ||
                 p == "/user/hand/left/input/menu/click";
    for (const char* k : kInputs) {
      if (p == std::string("/user/hand/left") + k || p == std::string("/user/hand/right") + k) known = true;
    }
    if (!known) {
      Fault("the touch profile has no path %s", p.c_str());
      return XR_ERROR_PATH_UNSUPPORTED;
    }
  }
  for (uint32_t i = 0; i < info->countSuggestedBindings; ++i) {
    Action* a = reinterpret_cast<Action*>(info->suggestedBindings[i].action);
    a->bound.push_back(info->suggestedBindings[i].binding);
  }
  in->touch_bound = true;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL AttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* info) {
  Session* s = reinterpret_cast<Session*>(session);
  s->attached = true;
  g_session = s;
  Report("call xrAttachSessionActionSets %u touch %d", info->countActionSets, s->instance->touch_bound ? 1 : 0);
  return XR_SUCCESS;
}

XrResult XRAPI_CALL SyncActions(XrSession session, const XrActionsSyncInfo*) {
  Session* s = reinterpret_cast<Session*>(session);
  if (!s->attached) Fault("xrSyncActions before xrAttachSessionActionSets");
  if (s->state != XR_SESSION_STATE_FOCUSED) return XR_SESSION_NOT_FOCUSED;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetActionStateBoolean(XrSession session, const XrActionStateGetInfo* info,
                                          XrActionStateBoolean* state) {
  Session* s = reinterpret_cast<Session*>(session);
  Action* a = reinterpret_cast<Action*>(info->action);
  const int hand = HandOf(s->instance, info->subactionPath);
  state->isActive = XR_FALSE;
  state->currentState = XR_FALSE;
  if (a->type != XR_ACTION_TYPE_BOOLEAN_INPUT) Fault("xrGetActionStateBoolean of a %d action", a->type);
  if (hand < 0) return XR_SUCCESS;
  const Hand& h = s->hands[hand];
  bool value = false, active = false;
  if (BoundTo(a, s->instance, hand, "/input/trigger/value")) {
    active = true;
    value = value || h.trigger;
  }
  if (BoundTo(a, s->instance, hand, hand == 1 ? "/input/b/click" : "/input/y/click")) {
    active = true;
    value = value || h.button;
  }
  if (BoundTo(a, s->instance, hand, "/input/menu/click")) {
    active = true;
    value = value || h.menu;
  }
  state->isActive = active ? XR_TRUE : XR_FALSE;
  state->currentState = value ? XR_TRUE : XR_FALSE;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetActionStateFloat(XrSession session, const XrActionStateGetInfo* info,
                                        XrActionStateFloat* state) {
  Session* s = reinterpret_cast<Session*>(session);
  Action* a = reinterpret_cast<Action*>(info->action);
  const int hand = HandOf(s->instance, info->subactionPath);
  state->isActive = XR_FALSE;
  state->currentState = 0.0f;
  if (a->type != XR_ACTION_TYPE_FLOAT_INPUT) Fault("xrGetActionStateFloat of a %d action", a->type);
  if (hand < 0) return XR_SUCCESS;
  if (BoundTo(a, s->instance, hand, "/input/trigger/value")) {
    state->isActive = XR_TRUE;
    state->currentState = s->hands[hand].trigger ? 1.0f : 0.0f;
  } else if (BoundTo(a, s->instance, hand, "/input/squeeze/value")) {
    state->isActive = XR_TRUE;
  }
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetCurrentInteractionProfile(XrSession session, XrPath hand,
                                                 XrInteractionProfileState* state) {
  Session* s = reinterpret_cast<Session*>(session);
  state->interactionProfile = XR_NULL_PATH;
  if (s->instance->touch_bound && HandOf(s->instance, hand) >= 0) {
    state->interactionProfile = PathOf(s->instance, "/interaction_profiles/oculus/touch_controller");
  }
  return XR_SUCCESS;
}

XrResult XRAPI_CALL LocateViews(XrSession session, const XrViewLocateInfo* info, XrViewState* state,
                                uint32_t capacity, uint32_t* count, XrView* views) {
  Session* s = reinterpret_cast<Session*>(session);
  if (info->type != XR_TYPE_VIEW_LOCATE_INFO) Fault("xrLocateViews: a wrong type");
  if (info->viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) Fault("xrLocateViews not stereo");
  if (!s->begun) Fault("xrLocateViews before xrBeginSession");
  *count = 2;
  if (capacity == 0) return XR_SUCCESS;
  if (capacity < 2) return XR_ERROR_SIZE_INSUFFICIENT;
  state->viewStateFlags = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT |
                          XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
  for (int e = 0; e < 2; ++e) {
    if (views[e].type != XR_TYPE_VIEW) Fault("xrLocateViews: a view of a wrong type");
    views[e].pose = {};
    views[e].pose.orientation.w = 1.0f;
    views[e].pose.position.x = e == 0 ? -kEyeX : kEyeX;
    views[e].fov = {kFov[0], kFov[1], kFov[2], kFov[3]};
  }
  if (!g_views_said) {
    g_views_said = true;
    Report("call xrLocateViews eyes %.4f %.4f fov %.4f %.4f %.4f %.4f", -kEyeX, kEyeX, kFov[0], kFov[1],
           kFov[2], kFov[3]);
  }
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetActionStateVector2f(XrSession session, const XrActionStateGetInfo* info,
                                           XrActionStateVector2f* state) {
  Session* s = reinterpret_cast<Session*>(session);
  Action* a = reinterpret_cast<Action*>(info->action);
  const int hand = HandOf(s->instance, info->subactionPath);
  state->isActive = XR_FALSE;
  state->currentState = {0, 0};
  if (hand < 0 || !BoundTo(a, s->instance, hand, "/input/thumbstick")) return XR_SUCCESS;
  state->isActive = XR_TRUE;
  state->currentState = {s->hands[hand].stick[0], s->hands[hand].stick[1]};
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetActionStatePose(XrSession, const XrActionStateGetInfo*, XrActionStatePose* state) {
  state->isActive = XR_TRUE;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL ApplyHapticFeedback(XrSession session, const XrHapticActionInfo* info,
                                        const XrHapticBaseHeader* haptic) {
  Session* s = reinterpret_cast<Session*>(session);
  if (haptic->type != XR_TYPE_HAPTIC_VIBRATION) Fault("a pulse of a wrong type");
  ++s->haptics;
  const auto* v = reinterpret_cast<const XrHapticVibration*>(haptic);
  Report("frame %ld pulse %s amplitude %.2f duration %lld", s->frame,
         NameOf(s->instance, info->subactionPath).c_str(), v->amplitude,
         static_cast<long long>(v->duration));
  return XR_SUCCESS;
}

XrResult XRAPI_CALL WaitFrame(XrSession session, const XrFrameWaitInfo*, XrFrameState* state) {
  Session* s = reinterpret_cast<Session*>(session);
  if (!s->begun) Fault("xrWaitFrame before xrBeginSession");
  // 72 frames a second, as a Quest 3 at the lowest rate.
  const int64_t period = 1000000000 / 72;
  const int64_t now = NowNs();
  if (s->last_wait != 0 && now - s->last_wait < period) usleep(static_cast<useconds_t>((period - (now - s->last_wait)) / 1000));
  s->last_wait = NowNs();
  state->predictedDisplayTime = s->last_wait + period;
  state->predictedDisplayPeriod = period;
  state->shouldRender = s->state == XR_SESSION_STATE_VISIBLE || s->state == XR_SESSION_STATE_FOCUSED;
  s->waited = true;
  ++s->frame;
  StepScript(s);
  RunScript(s);
  return XR_SUCCESS;
}

XrResult XRAPI_CALL BeginFrame(XrSession session, const XrFrameBeginInfo*) {
  Session* s = reinterpret_cast<Session*>(session);
  if (!s->waited) Fault("xrBeginFrame with no xrWaitFrame");
  s->waited = false;
  s->in_frame = true;
  return XR_SUCCESS;
}

XrResult XRAPI_CALL EndFrame(XrSession session, const XrFrameEndInfo* info) {
  Session* s = reinterpret_cast<Session*>(session);
  if (!s->in_frame) Fault("xrEndFrame with no xrBeginFrame");
  s->in_frame = false;
  s->last_quads.clear();
  s->last_eyes.clear();
  std::string what;
  std::string quads;  // each quad in full, for the line "quads"
  static std::string s_last_projection;
  for (uint32_t i = 0; i < info->layerCount; ++i) {
    const XrCompositionLayerBaseHeader* l = info->layers[i];
    if (l->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
      // THE PROJECTION LAYER: two views, each of a released image of its
      // swapchain, with a rect in that swapchain.
      const auto* p = reinterpret_cast<const XrCompositionLayerProjection*>(l);
      what += " projection";
      if (p->viewCount != 2 || p->views == nullptr) {
        Fault("frame %ld: a projection layer with %u views", s->frame, p->viewCount);
        continue;
      }
      if (reinterpret_cast<Space*>(p->space) == nullptr) Fault("frame %ld: a projection layer with no space", s->frame);
      std::string text;
      for (uint32_t e = 0; e < 2; ++e) {
        const XrCompositionLayerProjectionView& v = p->views[e];
        if (v.type != XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW) Fault("frame %ld: a view of a projection layer of a wrong type", s->frame);
        Swapchain* sc = reinterpret_cast<Swapchain*>(v.subImage.swapchain);
        if (sc == nullptr || sc->released < 0) Fault("frame %ld: a view of a projection layer with no released image", s->frame);
        if (sc != nullptr && sc->acquired >= 0) Fault("frame %ld: a view of a projection layer of a swapchain with an acquired image", s->frame);
        if (sc != nullptr && (v.subImage.imageRect.offset.x < 0 || v.subImage.imageRect.offset.y < 0 ||
                              v.subImage.imageRect.offset.x + v.subImage.imageRect.extent.width > static_cast<int32_t>(sc->width) ||
                              v.subImage.imageRect.offset.y + v.subImage.imageRect.extent.height > static_cast<int32_t>(sc->height))) {
          Fault("frame %ld: the rect of a view of a projection layer is not in its swapchain", s->frame);
        }
        if (v.subImage.imageArrayIndex != 0) Fault("frame %ld: a view of a projection layer of array index %u", s->frame, v.subImage.imageArrayIndex);
        if (!(v.fov.angleLeft < v.fov.angleRight) || !(v.fov.angleDown < v.fov.angleUp)) Fault("frame %ld: the angles of a view of a projection layer are empty", s->frame);
        s->last_eyes.push_back(v.subImage.swapchain);
        char part[256];
        snprintf(part, sizeof(part), " eye %u pose %.4f %.4f %.4f %.4f %.4f %.4f %.4f fov %.4f %.4f %.4f %.4f rect %d %d %dx%d",
                 e, v.pose.position.x, v.pose.position.y, v.pose.position.z, v.pose.orientation.x,
                 v.pose.orientation.y, v.pose.orientation.z, v.pose.orientation.w, v.fov.angleLeft,
                 v.fov.angleRight, v.fov.angleUp, v.fov.angleDown, v.subImage.imageRect.offset.x,
                 v.subImage.imageRect.offset.y, v.subImage.imageRect.extent.width,
                 v.subImage.imageRect.extent.height);
        text += part;
      }
      if (text != s_last_projection || s->frame % 72 == 0) Report("frame %ld projection%s", s->frame, text.c_str());
      s_last_projection = text;
      continue;
    }
    if (l->type != XR_TYPE_COMPOSITION_LAYER_QUAD) {
      what += " other";
      continue;
    }
    const auto* q = reinterpret_cast<const XrCompositionLayerQuad*>(l);
    Swapchain* sc = reinterpret_cast<Swapchain*>(q->subImage.swapchain);
    if (sc == nullptr || sc->released < 0) Fault("frame %ld: a quad with no released image", s->frame);
    if (sc != nullptr && sc->acquired >= 0) Fault("frame %ld: a quad of a swapchain with an acquired image", s->frame);
    if (sc != nullptr && (q->subImage.imageRect.extent.width > static_cast<int32_t>(sc->width) ||
                          q->subImage.imageRect.extent.height > static_cast<int32_t>(sc->height))) {
      Fault("frame %ld: the rect of a quad is larger than its swapchain", s->frame);
    }
    s->last_quads.push_back(*q);
    if (sc != nullptr && (q->subImage.imageRect.offset.x < 0 || q->subImage.imageRect.offset.y < 0 ||
                          q->subImage.imageRect.offset.x + q->subImage.imageRect.extent.width > static_cast<int32_t>(sc->width) ||
                          q->subImage.imageRect.offset.y + q->subImage.imageRect.extent.height > static_cast<int32_t>(sc->height))) {
      Fault("frame %ld: the rect of a quad is not in its swapchain", s->frame);
    }
    {
      char full[320];
      snprintf(full, sizeof(full), " quad %.4f %.4f %.4f turn %.4f %.4f %.4f %.4f size %.4f %.4f rect %d %d %dx%d eye %d flags %u",
               q->pose.position.x, q->pose.position.y, q->pose.position.z, q->pose.orientation.x,
               q->pose.orientation.y, q->pose.orientation.z, q->pose.orientation.w, q->size.width,
               q->size.height, q->subImage.imageRect.offset.x, q->subImage.imageRect.offset.y,
               q->subImage.imageRect.extent.width, q->subImage.imageRect.extent.height,
               static_cast<int>(q->eyeVisibility), static_cast<unsigned>(q->layerFlags));
      quads += full;
    }
    char part[160];
    snprintf(part, sizeof(part), " quad %.3f %.3f %.3f size %.3f %.3f eye %d", q->pose.position.x,
             q->pose.position.y, q->pose.position.z, q->size.width, q->size.height,
             static_cast<int>(q->eyeVisibility));
    what += part;
  }
  // One line for each change of the layers, not one for each frame.
  static std::string s_last;
  char head[64];
  snprintf(head, sizeof(head), "layers %u", info->layerCount);
  const std::string line = head + what;
  if (line.substr(0, 9) != s_last.substr(0, 9) || s->frame % 72 == 0) {
    Report("frame %ld %s", s->frame, line.c_str());
  }
  s_last = line;
  // One line for each change of the quads.
  static std::string s_last_quads;
  if (quads != s_last_quads) Report("frame %ld quads%s", s->frame, quads.c_str());
  s_last_quads = quads;
  for (const Step& st : g_script) {
    if (!s->script_new || st.frame != s->script) continue;
    if (st.what == "capture" || st.what == "quads") {
      for (uint32_t i = 0; i < s->last_quads.size(); ++i) {
        const XrExtent2Di e = s->last_quads[i].subImage.imageRect.extent;
        if (st.what == "quads" && ((e.width == 1600 && e.height == 1200) || (e.width == 64 && e.height == 64))) continue;
        char path[256];
        snprintf(path, sizeof(path), "%s/%s-%u.bmp", kFolder, st.name.c_str(), i);
        Capture(s, reinterpret_cast<Swapchain*>(s->last_quads[i].subImage.swapchain), path);
      }
    }
    if (st.what != "eyes") continue;
    if (s->last_eyes.empty()) Fault("frame %ld: eyes of a frame with no projection layer", s->frame);
    for (uint32_t e = 0; e < s->last_eyes.size(); ++e) {
      char path[256];
      snprintf(path, sizeof(path), "%s/%s-p%u.bmp", kFolder, st.name.c_str(), e);
      Capture(s, reinterpret_cast<Swapchain*>(s->last_eyes[e]), path);
    }
  }
  return XR_SUCCESS;
}

XrResult XRAPI_CALL ResultToString(XrInstance, XrResult result, char buffer[XR_MAX_RESULT_STRING_SIZE]) {
  snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_RESULT_%d", static_cast<int>(result));
  return XR_SUCCESS;
}

XrResult XRAPI_CALL GetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
  struct Entry {
    const char* name;
    PFN_xrVoidFunction f;
  };
#define BWVR_XR(n, f) {n, reinterpret_cast<PFN_xrVoidFunction>(f)}
  static const Entry kTable[] = {
      BWVR_XR("xrGetInstanceProcAddr", GetInstanceProcAddr),
      BWVR_XR("xrEnumerateInstanceExtensionProperties", EnumerateInstanceExtensionProperties),
      BWVR_XR("xrInitializeLoaderKHR", InitializeLoader),
      BWVR_XR("xrCreateInstance", CreateInstance),
      BWVR_XR("xrDestroyInstance", DestroyInstance),
      BWVR_XR("xrGetInstanceProperties", GetInstanceProperties),
      BWVR_XR("xrResultToString", ResultToString),
      BWVR_XR("xrStringToPath", StringToPath),
      BWVR_XR("xrPathToString", PathToString),
      BWVR_XR("xrGetSystem", GetSystem),
      BWVR_XR("xrGetSystemProperties", GetSystemProperties),
      BWVR_XR("xrEnumerateViewConfigurations", EnumerateViewConfigurations),
      BWVR_XR("xrEnumerateViewConfigurationViews", EnumerateViewConfigurationViews),
      BWVR_XR("xrEnumerateEnvironmentBlendModes", EnumerateEnvironmentBlendModes),
      BWVR_XR("xrGetVulkanGraphicsRequirements2KHR", GetVulkanGraphicsRequirements2),
      BWVR_XR("xrCreateVulkanInstanceKHR", CreateVulkanInstance),
      BWVR_XR("xrGetVulkanGraphicsDevice2KHR", GetVulkanGraphicsDevice2),
      BWVR_XR("xrCreateVulkanDeviceKHR", CreateVulkanDevice),
      BWVR_XR("xrCreateSession", CreateSession),
      BWVR_XR("xrDestroySession", DestroySession),
      BWVR_XR("xrPollEvent", PollEvent),
      BWVR_XR("xrBeginSession", BeginSession),
      BWVR_XR("xrEndSession", EndSession),
      BWVR_XR("xrRequestExitSession", RequestExitSession),
      BWVR_XR("xrCreateReferenceSpace", CreateReferenceSpace),
      BWVR_XR("xrCreateActionSpace", CreateActionSpace),
      BWVR_XR("xrDestroySpace", DestroySpace),
      BWVR_XR("xrLocateSpace", LocateSpace),
      BWVR_XR("xrEnumerateSwapchainFormats", EnumerateSwapchainFormats),
      BWVR_XR("xrCreateSwapchain", CreateSwapchain),
      BWVR_XR("xrDestroySwapchain", DestroySwapchain),
      BWVR_XR("xrEnumerateSwapchainImages", EnumerateSwapchainImages),
      BWVR_XR("xrAcquireSwapchainImage", AcquireSwapchainImage),
      BWVR_XR("xrWaitSwapchainImage", WaitSwapchainImage),
      BWVR_XR("xrReleaseSwapchainImage", ReleaseSwapchainImage),
      BWVR_XR("xrCreateActionSet", CreateActionSet),
      BWVR_XR("xrDestroyActionSet", DestroyActionSet),
      BWVR_XR("xrCreateAction", CreateAction),
      BWVR_XR("xrDestroyAction", DestroyAction),
      BWVR_XR("xrSuggestInteractionProfileBindings", SuggestBindings),
      BWVR_XR("xrAttachSessionActionSets", AttachSessionActionSets),
      BWVR_XR("xrSyncActions", SyncActions),
      BWVR_XR("xrGetActionStateBoolean", GetActionStateBoolean),
      BWVR_XR("xrGetActionStateFloat", GetActionStateFloat),
      BWVR_XR("xrGetActionStateVector2f", GetActionStateVector2f),
      BWVR_XR("xrGetActionStatePose", GetActionStatePose),
      BWVR_XR("xrGetCurrentInteractionProfile", GetCurrentInteractionProfile),
      BWVR_XR("xrLocateViews", LocateViews),
      BWVR_XR("xrApplyHapticFeedback", ApplyHapticFeedback),
      BWVR_XR("xrWaitFrame", WaitFrame),
      BWVR_XR("xrBeginFrame", BeginFrame),
      BWVR_XR("xrEndFrame", EndFrame),
  };
#undef BWVR_XR
  for (const Entry& e : kTable) {
    if (strcmp(e.name, name) == 0) {
      *function = e.f;
      return XR_SUCCESS;
    }
  }
  // The loader asks for each function of the core. The test does not need
  // the others.
  *function = nullptr;
  return XR_ERROR_FUNCTION_UNSUPPORTED;
}

}  // namespace

extern "C" __attribute__((visibility("default"))) XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loader, XrNegotiateRuntimeRequest* request) {
  if (loader == nullptr || request == nullptr ||
      loader->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
      request->structType != XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST) {
    return XR_ERROR_INITIALIZATION_FAILED;
  }
  request->runtimeInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
  request->runtimeApiVersion = XR_CURRENT_API_VERSION;
  request->getInstanceProcAddr = GetInstanceProcAddr;
  Report("call xrNegotiateLoaderRuntimeInterface");
  return XR_SUCCESS;
}
