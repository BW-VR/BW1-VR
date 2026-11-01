#include "xr_app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/system_properties.h>
#include <time.h>

#include <mutex>
#include <string>

#include "../../src/common/ini_text.h"
#include "../../src/mod/controller_math.h"
#include "../../src/mod/screen_quad_math.h"
#include "app_log.h"
#include "eye_size.h"
#include "picture_ring.h"

// The game folder that the app gives to the renderer (scene_host.cpp).
const char* VkPlatformFolder();

namespace {

// The quads of the mod take these values of openxr.h (quad_layer_math.h).
static_assert(quad_layer_math::kEyeBoth == XR_EYE_VISIBILITY_BOTH &&
                  quad_layer_math::kEyeLeft == XR_EYE_VISIBILITY_LEFT &&
                  quad_layer_math::kEyeRight == XR_EYE_VISIBILITY_RIGHT &&
                  quad_layer_math::kBlendSourceAlpha ==
                      XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
              "the values of a quad of quad_layer_math.h");

bool Ok(XrResult result, const char* what) {
  if (XR_SUCCEEDED(result)) return true;
  AppLog("xr           : %s failed (%d)", what, static_cast<int>(result));
  return false;
}

template <typename T>
T Proc(XrInstance instance, const char* name) {
  PFN_xrVoidFunction f = nullptr;
  if (XR_FAILED(xrGetInstanceProcAddr(instance, name, &f))) return nullptr;
  return reinterpret_cast<T>(f);
}

double ClockMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

// The CPU time of this thread in ms.
double ThreadCpuMs() {
  timespec t;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

// The lock of the queue that the renderer of the mod and the app share
// (vk::External::lock). With frame_loop=1 the play thread (the renderer and
// the copies into the ring of the pictures) and the session submit from two
// threads. A thread with TimeQueueWaits(true) adds up its waits for the lock.
std::recursive_mutex g_queue_lock;
thread_local bool t_time_lock = false;
thread_local double t_lock_wait_ms = 0.0;
void LockQueue() {
  if (!t_time_lock) {
    g_queue_lock.lock();
    return;
  }
  if (g_queue_lock.try_lock()) return;
  const double start = ClockMs();
  g_queue_lock.lock();
  t_lock_wait_ms += ClockMs() - start;
}
void UnlockQueue() { g_queue_lock.unlock(); }

XrPath Path(XrInstance instance, const char* text) {
  XrPath path = XR_NULL_PATH;
  xrStringToPath(instance, text, &path);
  return path;
}

}  // namespace

bool XrApp::Start(ANativeActivity* activity, uint32_t menu_width,
                  uint32_t menu_height, uint32_t cursor_size) {
  if (!MakeInstance(activity)) return false;
  if (!MakeVulkan()) return false;
  if (!MakeSession()) return false;
  ReadDisplayRates();
  if (!MakeActions()) return false;
  if (!MakeLayer(&menu_, menu_width, menu_height)) return false;
  if (!MakeLayer(&cursor_, cursor_size, cursor_size)) return false;
  AppLog("xr           : the session is made. The menu is a quad of %u by %u",
         menu_width, menu_height);
  return true;
}

bool XrApp::MakeInstance(ANativeActivity* activity) {
  auto initialize = Proc<PFN_xrInitializeLoaderKHR>(XR_NULL_HANDLE,
                                                    "xrInitializeLoaderKHR");
  if (initialize == nullptr) {
    AppLog("xr           : the loader has no xrInitializeLoaderKHR");
    return false;
  }
  XrLoaderInitInfoAndroidKHR loader = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
  loader.applicationVM = activity->vm;
  loader.applicationContext = activity->clazz;
  if (!Ok(initialize(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loader)),
          "xrInitializeLoaderKHR")) {
    return false;
  }

  uint32_t count = 0;
  if (!Ok(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr),
          "xrEnumerateInstanceExtensionProperties")) {
    return false;
  }
  std::vector<XrExtensionProperties> extensions(count,
                                                {XR_TYPE_EXTENSION_PROPERTIES});
  xrEnumerateInstanceExtensionProperties(nullptr, count, &count,
                                         extensions.data());
  bool vulkan2 = false, android = false;
  // THE FOVEATION OF THE RUNTIME (docs/android.md, "The sharpness of the
  // middle"): the extensions for a log line. The app uses none of them.
  static const char* const kFoveation[] = {
      "XR_FB_foveation",       "XR_FB_foveation_configuration",
      "XR_FB_foveation_vulkan", "XR_META_foveation_eye_tracked",
      "XR_FB_swapchain_update_state", "XR_FB_composition_layer_settings"};
  bool foveation[6] = {};
  for (const XrExtensionProperties& e : extensions) {
    for (int i = 0; i < 6; ++i) {
      if (strcmp(e.extensionName, kFoveation[i]) == 0) foveation[i] = true;
    }
    if (strcmp(e.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME) == 0) {
      vulkan2 = true;
    }
    if (strcmp(e.extensionName, XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME) == 0) {
      android = true;
    }
    if (strcmp(e.extensionName, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME) == 0) {
      display_rate_ext_ = true;
    }
    if (strcmp(e.extensionName, XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME) == 0) {
      timespec_ext_ = true;
    }
  }
  AppLog("xr           : the runtime has %u extensions. Vulkan 2: %s, Android: %s, "
         "display rate: %s",
         count, vulkan2 ? "yes" : "no", android ? "yes" : "no",
         display_rate_ext_ ? "yes" : "no");
  {
    char list[256] = "";
    size_t at = 0;
    for (int i = 0; i < 6 && at < sizeof(list); ++i) {
      at += snprintf(list + at, sizeof(list) - at, "%s%s %s", i == 0 ? "" : ", ",
                     kFoveation[i], foveation[i] ? "yes" : "no");
    }
    AppLog("xr           : the extensions of the runtime for foveation and the filter of "
           "the layers: %s", list);
  }
  if (!vulkan2 || !android) return false;

  // XR_FB_display_refresh_rate only when the runtime lists it. A runtime with
  // none (for example Lepton of the Steam Frame) uses its own rate.
  // XR_KHR_convert_timespec_time only when the runtime lists it: the time
  // log takes the display time of a frame on CLOCK_MONOTONIC from it.
  const char* names[4] = {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME,
                          XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME};
  uint32_t name_count = 2;
  if (display_rate_ext_) names[name_count++] = XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME;
  if (timespec_ext_) names[name_count++] = XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME;
  XrInstanceCreateInfoAndroidKHR android_info = {
      XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
  android_info.applicationVM = activity->vm;
  android_info.applicationActivity = activity->clazz;
  XrInstanceCreateInfo info = {XR_TYPE_INSTANCE_CREATE_INFO};
  info.next = &android_info;
  strcpy(info.applicationInfo.applicationName, "Black & White VR Mod");
  info.applicationInfo.applicationVersion = 1;
  strcpy(info.applicationInfo.engineName, "bwvr");
  info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 34);
  info.enabledExtensionCount = name_count;
  info.enabledExtensionNames = names;
  if (!Ok(xrCreateInstance(&info, &instance_), "xrCreateInstance")) return false;
  if (timespec_ext_) {
    to_timespec_ = Proc<PFN_xrConvertTimeToTimespecTimeKHR>(instance_,
                                                            "xrConvertTimeToTimespecTimeKHR");
  }
  AppLog("xr           : XR_KHR_convert_timespec_time: %s",
         to_timespec_ != nullptr ? "yes. The time log has the age of the shown picture in ms"
                                 : "no. The time log has no age of the shown picture in ms");

  XrInstanceProperties properties = {XR_TYPE_INSTANCE_PROPERTIES};
  xrGetInstanceProperties(instance_, &properties);
  runtime_name_ = properties.runtimeName;
  AppLog("xr           : runtime %s %u.%u.%u", properties.runtimeName,
         XR_VERSION_MAJOR(properties.runtimeVersion),
         XR_VERSION_MINOR(properties.runtimeVersion),
         XR_VERSION_PATCH(properties.runtimeVersion));

  XrSystemGetInfo system = {XR_TYPE_SYSTEM_GET_INFO};
  system.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  if (!Ok(xrGetSystem(instance_, &system, &system_), "xrGetSystem")) return false;
  XrSystemProperties sp = {XR_TYPE_SYSTEM_PROPERTIES};
  xrGetSystemProperties(instance_, system_, &sp);
  AppLog("xr           : system %s, vendor 0x%X", sp.systemName, sp.vendorId);
  // THE SIZE OF THE RUNTIME (docs/android.md, "The size of the eye
  // pictures"): the values that can explain the size that it asks for.
  AppLog("xr           : the system: swapchains of at most %u by %u, at most %u layers, "
         "orientation tracking %s, position tracking %s",
         sp.graphicsProperties.maxSwapchainImageWidth,
         sp.graphicsProperties.maxSwapchainImageHeight, sp.graphicsProperties.maxLayerCount,
         sp.trackingProperties.orientationTracking ? "yes" : "no",
         sp.trackingProperties.positionTracking ? "yes" : "no");
  {
    uint32_t types = 0;
    xrEnumerateViewConfigurations(instance_, system_, 0, &types, nullptr);
    std::vector<XrViewConfigurationType> type(types);
    if (types > 0) xrEnumerateViewConfigurations(instance_, system_, types, &types, type.data());
    char list[160] = "";
    size_t at = 0;
    for (uint32_t i = 0; i < types && at < sizeof(list); ++i) {
      at += snprintf(list + at, sizeof(list) - at, "%s%d", i == 0 ? "" : ", ",
                     static_cast<int>(type[i]));
    }
    // The properties of Android that a tool of the headset can set for the
    // size of the eyes (empty: not set).
    static const char* const kProps[] = {"debug.oculus.textureWidth",
                                         "debug.oculus.textureHeight",
                                         "debug.oculus.refreshRate",
                                         "debug.oculus.foveation.level",
                                         "debug.oculus.gpuLevel", "debug.oculus.cpuLevel"};
    char props[512] = "";
    size_t pat = 0;
    for (int i = 0; i < 6 && pat < sizeof(props); ++i) {
      char value[PROP_VALUE_MAX] = "";
      __system_property_get(kProps[i], value);
      pat += snprintf(props + pat, sizeof(props) - pat, "%s%s=%s", i == 0 ? "" : ", ",
                      kProps[i], value[0] != 0 ? value : "(empty)");
    }
    AppLog("xr           : the view configurations of the runtime: %s. The properties of "
           "the system: %s",
           types > 0 ? list : "none", props);
  }

  uint32_t views = 0;
  xrEnumerateViewConfigurationViews(instance_, system_,
                                    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                                    &views, nullptr);
  std::vector<XrViewConfigurationView> view(views,
                                            {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  xrEnumerateViewConfigurationViews(instance_, system_,
                                    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                    views, &views, view.data());
  if (views > 0) {
    // THE CAP OF THE SIZE OF THE EYE PICTURES (eye_size_max, eye_size.h).
    const uint32_t w = view[0].recommendedImageRectWidth;
    const uint32_t h = view[0].recommendedImageRectHeight;
    const eye_size::Size size = eye_size::FromKey(eye_size_max_, w, h);
    eye_width_ = size.width;
    eye_height_ = size.height;
    char cap[200];
    if (eye_size_max_ == 0) {
      snprintf(cap, sizeof(cap), "eye_size_max=0: no cap, the app takes %u by %u",
               eye_width_, eye_height_);
    } else if (size.capped) {
      snprintf(cap, sizeof(cap),
               "eye_size_max=%d: the app takes %u by %u, the panel of a Quest 3 (%u by "
               "%u) with the aspect of the headset",
               eye_size_max_, eye_width_, eye_height_, eye_size::kPanelWidth,
               eye_size::kPanelHeight);
    } else {
      snprintf(cap, sizeof(cap),
               "eye_size_max=%d: the size fits in the panel of a Quest 3 (%u by %u), so "
               "the app takes %u by %u",
               eye_size_max_, eye_size::kPanelWidth, eye_size::kPanelHeight, eye_width_,
               eye_height_);
    }
    AppLog("xr           : the headset asks for %u by %u for each eye (at most %u by %u). "
           "%s",
           w, h, view[0].maxImageRectWidth, view[0].maxImageRectHeight, cap);
    asked_width_ = w;
    asked_height_ = h;
    for (uint32_t i = 0; i < views; ++i) {
      AppLog("xr           : view %u of the runtime: %u by %u (at most %u by %u), %u "
             "samples (at most %u)",
             i, view[i].recommendedImageRectWidth, view[i].recommendedImageRectHeight,
             view[i].maxImageRectWidth, view[i].maxImageRectHeight,
             view[i].recommendedSwapchainSampleCount, view[i].maxSwapchainSampleCount);
    }
  }

  uint32_t modes = 0;
  xrEnumerateEnvironmentBlendModes(instance_, system_,
                                   XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                                   &modes, nullptr);
  std::vector<XrEnvironmentBlendMode> mode(modes);
  xrEnumerateEnvironmentBlendModes(instance_, system_,
                                   XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                   modes, &modes, mode.data());
  if (modes > 0) blend_ = mode[0];
  return true;
}

bool XrApp::MakeVulkan() {
  auto requirements = Proc<PFN_xrGetVulkanGraphicsRequirements2KHR>(
      instance_, "xrGetVulkanGraphicsRequirements2KHR");
  auto create_instance =
      Proc<PFN_xrCreateVulkanInstanceKHR>(instance_, "xrCreateVulkanInstanceKHR");
  auto get_device = Proc<PFN_xrGetVulkanGraphicsDevice2KHR>(
      instance_, "xrGetVulkanGraphicsDevice2KHR");
  auto create_device =
      Proc<PFN_xrCreateVulkanDeviceKHR>(instance_, "xrCreateVulkanDeviceKHR");
  if (requirements == nullptr || create_instance == nullptr ||
      get_device == nullptr || create_device == nullptr) {
    AppLog("xr           : the functions of XR_KHR_vulkan_enable2 are missing");
    return false;
  }
  XrGraphicsRequirementsVulkan2KHR req = {
      XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
  if (!Ok(requirements(instance_, system_, &req),
          "xrGetVulkanGraphicsRequirements2KHR")) {
    return false;
  }

  // The instance asks for Vulkan 1.3 for the renderer of the mod, and the
  // device decides. maxApiVersionSupported is the highest version that the
  // runtime was tested with, not a limit, so the log only names it.
  AppLog("xr           : the runtime takes Vulkan %u.%u to %u.%u (the highest "
         "version that it was tested with)",
         XR_VERSION_MAJOR(req.minApiVersionSupported),
         XR_VERSION_MINOR(req.minApiVersionSupported),
         XR_VERSION_MAJOR(req.maxApiVersionSupported),
         XR_VERSION_MINOR(req.maxApiVersionSupported));
  VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "Black & White VR Mod";
  app.pEngineName = "bwvr";
  app.apiVersion = VK_API_VERSION_1_3;
  VkInstanceCreateInfo vi = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  vi.pApplicationInfo = &app;
  XrVulkanInstanceCreateInfoKHR xi = {XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
  xi.systemId = system_;
  xi.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
  xi.vulkanCreateInfo = &vi;
  VkResult vr = VK_SUCCESS;
  if (!Ok(create_instance(instance_, &xi, &gpu_.instance, &vr),
          "xrCreateVulkanInstanceKHR") ||
      vr != VK_SUCCESS) {
    AppLog("xr           : the Vulkan instance gave %d", static_cast<int>(vr));
    return false;
  }
  XrVulkanGraphicsDeviceGetInfoKHR gi = {
      XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
  gi.systemId = system_;
  gi.vulkanInstance = gpu_.instance;
  if (!Ok(get_device(instance_, &gi, &gpu_.physical),
          "xrGetVulkanGraphicsDevice2KHR")) {
    return false;
  }
  uint32_t families = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(gpu_.physical, &families, nullptr);
  std::vector<VkQueueFamilyProperties> family(families);
  vkGetPhysicalDeviceQueueFamilyProperties(gpu_.physical, &families,
                                           family.data());
  gpu_.queue_family = UINT32_MAX;
  for (uint32_t i = 0; i < families; ++i) {
    if ((family[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
      gpu_.queue_family = i;
      break;
    }
  }
  if (gpu_.queue_family == UINT32_MAX) return false;
  const uint32_t family_queues = family[gpu_.queue_family].queueCount;

  // THE FEATURES OF THE RENDERER OF THE MOD, as the session of the PC asks
  // for them (xr_screen.cpp): dynamic rendering, synchronization2,
  // multiview, the anisotropic filter, and a shading rate attachment when
  // the GPU has it.
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(gpu_.physical, &properties);
  VkPhysicalDeviceVulkan13Features have13 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  VkPhysicalDeviceVulkan11Features have11 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  have13.pNext = &have11;
  VkPhysicalDeviceFeatures2 have = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  if (properties.apiVersion >= VK_API_VERSION_1_3) {
    have.pNext = &have13;
    vkGetPhysicalDeviceFeatures2(gpu_.physical, &have);
  }
  renderer13_ = properties.apiVersion >= VK_API_VERSION_1_3 &&
                have13.dynamicRendering && have13.synchronization2 &&
                have11.multiview;
  max_anisotropy_ = have.features.samplerAnisotropy
                        ? properties.limits.maxSamplerAnisotropy
                        : 0.0f;
  uint32_t extension_count = 0;
  vkEnumerateDeviceExtensionProperties(gpu_.physical, nullptr, &extension_count,
                                       nullptr);
  std::vector<VkExtensionProperties> extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(gpu_.physical, nullptr, &extension_count,
                                       extensions.data());
  bool rate_extension = false;
  for (const VkExtensionProperties& e : extensions) {
    if (strcmp(e.extensionName, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) == 0) {
      rate_extension = true;
    }
  }
  VkPhysicalDeviceFragmentShadingRateFeaturesKHR rate = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};
  if (renderer13_ && rate_extension) {
    VkPhysicalDeviceFeatures2 query = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    query.pNext = &rate;
    vkGetPhysicalDeviceFeatures2(gpu_.physical, &query);
  }
  shading_rate_ = renderer13_ && rate_extension && rate.attachmentFragmentShadingRate;
  AppLog("xr           : the GPU of the runtime: %s, Vulkan %u.%u. The renderer "
         "of the mod: %s. The anisotropic filter: up to %.0f. A shading rate "
         "attachment: %s",
         properties.deviceName, VK_API_VERSION_MAJOR(properties.apiVersion),
         VK_API_VERSION_MINOR(properties.apiVersion),
         renderer13_ ? "yes"
                     : properties.apiVersion < VK_API_VERSION_1_3
                           ? "no (it needs Vulkan 1.3)"
                           : "no (the GPU has no dynamic rendering, "
                             "synchronization2 or multiview)",
         max_anisotropy_,
         shading_rate_ ? "yes" : "no");
  // THE FOVEATION OF THE GPU (docs/android.md, "The sharpness of the
  // middle"): a fragment density map, for a log line. The renderer uses none.
  {
    bool density = false, density2 = false, offset = false;
    for (const VkExtensionProperties& e : extensions) {
      if (strcmp(e.extensionName, VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME) == 0) density = true;
      if (strcmp(e.extensionName, VK_EXT_FRAGMENT_DENSITY_MAP_2_EXTENSION_NAME) == 0) density2 = true;
      if (strcmp(e.extensionName, "VK_QCOM_fragment_density_map_offset") == 0) offset = true;
    }
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT map = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT};
    VkPhysicalDeviceFragmentDensityMapPropertiesEXT map_size = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT};
    if (density && properties.apiVersion >= VK_API_VERSION_1_1) {
      VkPhysicalDeviceFeatures2 query = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
      query.pNext = &map;
      vkGetPhysicalDeviceFeatures2(gpu_.physical, &query);
      VkPhysicalDeviceProperties2 props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      props.pNext = &map_size;
      vkGetPhysicalDeviceProperties2(gpu_.physical, &props);
    }
    AppLog("xr           : the GPU for foveated rendering: VK_KHR_fragment_shading_rate %s, "
           "VK_EXT_fragment_density_map %s (the map %s, images with no subsamples %s, "
           "texels of %u by %u to %u by %u pixels), VK_EXT_fragment_density_map2 %s, "
           "VK_QCOM_fragment_density_map_offset %s",
           rate_extension ? "yes" : "no", density ? "yes" : "no",
           map.fragmentDensityMap ? "yes" : "no",
           map.fragmentDensityMapNonSubsampledImages ? "yes" : "no",
           map_size.minFragmentDensityTexelSize.width,
           map_size.minFragmentDensityTexelSize.height,
           map_size.maxFragmentDensityTexelSize.width,
           map_size.maxFragmentDensityTexelSize.height, density2 ? "yes" : "no",
           offset ? "yes" : "no");
  }

  // THE TWO QUEUES (frame_loop_queues=2): the session on the first with the
  // higher priority, the renderer on the second. The timeline semaphore of
  // the session lets the second queue wait on the GPU for a copy of the
  // session.
  VkPhysicalDeviceVulkan12Features have12 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  if (properties.apiVersion >= VK_API_VERSION_1_3) {
    VkPhysicalDeviceFeatures2 query = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    query.pNext = &have12;
    vkGetPhysicalDeviceFeatures2(gpu_.physical, &query);
  }
  const bool two = renderer13_ && queues_wanted_ >= 2 && family_queues >= 2 &&
                   have12.timelineSemaphore;
  AppLog("xr           : frame_loop_queues=%d. The family of the queue has %u queues%s. %s",
         queues_wanted_, family_queues,
         have12.timelineSemaphore ? "" : ", and the device has no timeline semaphores",
         two ? "The session takes queue 0 (priority 1.0), the renderer of the mod with the "
               "frame loop queue 1 (priority 0.5)"
             : "One queue for the session and the renderer");
  const float priorities[2] = {1.0f, 0.5f};
  VkDeviceQueueCreateInfo queue = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  queue.queueFamilyIndex = gpu_.queue_family;
  queue.queueCount = two ? 2 : 1;
  queue.pQueuePriorities = priorities;
  VkPhysicalDeviceVulkan13Features want13 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  want13.dynamicRendering = VK_TRUE;
  want13.synchronization2 = VK_TRUE;
  VkPhysicalDeviceVulkan11Features want11 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  want11.multiview = VK_TRUE;
  VkPhysicalDeviceVulkan12Features want12 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  want12.timelineSemaphore = VK_TRUE;
  if (two) {
    want13.pNext = &want12;
    want12.pNext = &want11;
  } else {
    want13.pNext = &want11;
  }
  VkPhysicalDeviceFragmentShadingRateFeaturesKHR want_rate = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};
  want_rate.attachmentFragmentShadingRate = VK_TRUE;
  if (shading_rate_) want11.pNext = &want_rate;
  VkPhysicalDeviceFeatures2 want = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  want.pNext = &want13;
  want.features.samplerAnisotropy = max_anisotropy_ > 0.0f ? VK_TRUE : VK_FALSE;
  const char* rate_name = VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME;
  VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  if (renderer13_) {
    di.pNext = &want;
    if (shading_rate_) {
      di.enabledExtensionCount = 1;
      di.ppEnabledExtensionNames = &rate_name;
    }
  }
  di.queueCreateInfoCount = 1;
  di.pQueueCreateInfos = &queue;
  XrVulkanDeviceCreateInfoKHR xd = {XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
  xd.systemId = system_;
  xd.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
  xd.vulkanPhysicalDevice = gpu_.physical;
  xd.vulkanCreateInfo = &di;
  if (!Ok(create_device(instance_, &xd, &gpu_.device, &vr),
          "xrCreateVulkanDeviceKHR") ||
      vr != VK_SUCCESS) {
    AppLog("xr           : the Vulkan device gave %d", static_cast<int>(vr));
    return false;
  }
  if (!GpuFinish(&gpu_)) return false;
  if (time_log_) GpuTimingStart(&gpu_);
  if (two) {
    vkGetDeviceQueue(gpu_.device, gpu_.queue_family, 1, &queue2_);
    VkSemaphoreTypeCreateInfo type = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type.initialValue = 0;
    VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    info.pNext = &type;
    if (vkCreateSemaphore(gpu_.device, &info, nullptr, &gpu_.timeline) != VK_SUCCESS) {
      gpu_.timeline = VK_NULL_HANDLE;
      queue2_ = VK_NULL_HANDLE;
      AppLog("xr           : vkCreateSemaphore of the timeline failed, so the renderer "
             "takes the queue of the session");
    }
  }
  return true;
}

bool XrApp::MakeSession() {
  XrGraphicsBindingVulkan2KHR binding = {XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
  binding.instance = gpu_.instance;
  binding.physicalDevice = gpu_.physical;
  binding.device = gpu_.device;
  binding.queueFamilyIndex = gpu_.queue_family;
  binding.queueIndex = 0;
  XrSessionCreateInfo info = {XR_TYPE_SESSION_CREATE_INFO};
  info.next = &binding;
  info.systemId = system_;
  if (!Ok(xrCreateSession(instance_, &info, &session_), "xrCreateSession")) {
    return false;
  }
  XrReferenceSpaceCreateInfo space = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  space.poseInReferenceSpace.orientation.w = 1.0f;
  if (!Ok(xrCreateReferenceSpace(session_, &space, &local_),
          "xrCreateReferenceSpace")) {
    return false;
  }

  uint32_t count = 0;
  xrEnumerateSwapchainFormats(session_, 0, &count, nullptr);
  std::vector<int64_t> formats(count);
  xrEnumerateSwapchainFormats(session_, count, &count, formats.data());
  const int64_t wanted[] = {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB,
                            VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM};
  for (int64_t w : wanted) {
    for (int64_t f : formats) {
      if (format_ == 0 && f == w) format_ = f;
    }
  }
  if (format_ == 0 && count > 0) format_ = formats[0];
  AppLog("xr           : %u formats of a swapchain, the menu takes %lld", count,
         static_cast<long long>(format_));
  return format_ != 0;
}

void XrApp::ReadDisplayRates() {
  display_rates_.clear();
  if (!display_rate_ext_) {
    AppLog("xr           : the runtime has no XR_FB_display_refresh_rate. The app asks "
           "for no display rate, and the headset sets the rate");
    return;
  }
  enumerate_rates_ = Proc<PFN_xrEnumerateDisplayRefreshRatesFB>(
      instance_, "xrEnumerateDisplayRefreshRatesFB");
  get_rate_ = Proc<PFN_xrGetDisplayRefreshRateFB>(instance_, "xrGetDisplayRefreshRateFB");
  request_rate_ = Proc<PFN_xrRequestDisplayRefreshRateFB>(instance_,
                                                         "xrRequestDisplayRefreshRateFB");
  if (enumerate_rates_ == nullptr || get_rate_ == nullptr || request_rate_ == nullptr) {
    AppLog("xr           : the functions of XR_FB_display_refresh_rate are missing. The "
           "app asks for no display rate");
    display_rate_ext_ = false;
    return;
  }
  uint32_t count = 0;
  if (!Ok(enumerate_rates_(session_, 0, &count, nullptr),
          "xrEnumerateDisplayRefreshRatesFB")) {
    return;
  }
  std::vector<float> rates(count, 0.0f);
  if (count > 0 && !Ok(enumerate_rates_(session_, count, &count, rates.data()),
                       "xrEnumerateDisplayRefreshRatesFB")) {
    return;
  }
  rates.resize(count);
  display_rates_ = rates;
  std::string list;
  for (float r : rates) {
    char part[24];
    snprintf(part, sizeof(part), "%s%.2f", list.empty() ? "" : " ", r);
    list += part;
  }
  float now = 0.0f;
  get_rate_(session_, &now);
  AppLog("xr           : the runtime gives %u display rates: %s Hz. It uses %.2f Hz now",
         count, list.c_str(), now);
}

bool XrApp::RequestDisplayRate(float hz) {
  if (!display_rate_ext_ || request_rate_ == nullptr) return false;
  const XrResult r = request_rate_(session_, hz);
  float now = 0.0f;
  if (get_rate_ != nullptr) get_rate_(session_, &now);
  if (hz > 0.0f) {
    AppLog("xr           : the app asks the runtime for %.2f Hz (%d). The runtime uses "
           "%.2f Hz now",
           hz, static_cast<int>(r), now);
  } else {
    AppLog("xr           : the app asks the runtime for no display rate (0, %d). The "
           "runtime uses %.2f Hz now",
           static_cast<int>(r), now);
  }
  return XR_SUCCEEDED(r);
}

bool XrApp::MakeActions() {
  XrActionSetCreateInfo set = {XR_TYPE_ACTION_SET_CREATE_INFO};
  strcpy(set.actionSetName, "menu");
  strcpy(set.localizedActionSetName, "Menu");
  if (!Ok(xrCreateActionSet(instance_, &set, &action_set_), "xrCreateActionSet")) {
    return false;
  }
  hands_[0] = Path(instance_, "/user/hand/left");
  hands_[1] = Path(instance_, "/user/hand/right");
  auto make = [&](const char* name, const char* label, XrActionType type,
                  XrAction* out) {
    XrActionCreateInfo a = {XR_TYPE_ACTION_CREATE_INFO};
    strcpy(a.actionName, name);
    strcpy(a.localizedActionName, label);
    a.actionType = type;
    a.countSubactionPaths = 2;
    a.subactionPaths = hands_;
    return Ok(xrCreateAction(action_set_, &a, out), name);
  };
  if (!make("aim", "Aim", XR_ACTION_TYPE_POSE_INPUT, &aim_) ||
      !make("select", "Select", XR_ACTION_TYPE_BOOLEAN_INPUT, &select_) ||
      !make("back", "Back", XR_ACTION_TYPE_BOOLEAN_INPUT, &back_) ||
      !make("stick", "Stick", XR_ACTION_TYPE_VECTOR2F_INPUT, &stick_) ||
      !make("pulse", "Pulse", XR_ACTION_TYPE_VIBRATION_OUTPUT, &haptic_)) {
    return false;
  }
  // The actions of the mod (input_live=1), as XrScreen of the PC has them. A
  // float action on a boolean input reads 0 or 1.
  if (input_live_ &&
      (!make("grip_pose", "Grip pose", XR_ACTION_TYPE_POSE_INPUT, &grip_) ||
       !make("trigger", "Trigger", XR_ACTION_TYPE_FLOAT_INPUT, &trigger_) ||
       !make("squeeze", "Squeeze", XR_ACTION_TYPE_FLOAT_INPUT, &squeeze_) ||
       !make("stick_click", "Stick click", XR_ACTION_TYPE_BOOLEAN_INPUT,
             &stick_click_) ||
       !make("primary", "Primary button", XR_ACTION_TYPE_BOOLEAN_INPUT,
             &primary_) ||
       !make("secondary", "Secondary button", XR_ACTION_TYPE_BOOLEAN_INPUT,
             &secondary_) ||
       !make("menu_button", "Menu button", XR_ACTION_TYPE_BOOLEAN_INPUT,
             &menu_button_))) {
    return false;
  }

  struct Bind {
    XrAction action;
    const char* path;
  };
  auto suggest = [&](const char* profile, const std::vector<Bind>& binds,
                     const std::vector<Bind>& live) {
    std::vector<XrActionSuggestedBinding> s;
    for (const Bind& b : binds) s.push_back({b.action, Path(instance_, b.path)});
    if (input_live_) {
      for (const Bind& b : live) s.push_back({b.action, Path(instance_, b.path)});
    }
    XrInteractionProfileSuggestedBinding info = {
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    info.interactionProfile = Path(instance_, profile);
    info.countSuggestedBindings = static_cast<uint32_t>(s.size());
    info.suggestedBindings = s.data();
    const XrResult r = xrSuggestInteractionProfileBindings(instance_, &info);
    AppLog("xr           : the bindings of %s: %d", profile, static_cast<int>(r));
  };
  suggest("/interaction_profiles/khr/simple_controller",
          {{aim_, "/user/hand/left/input/aim/pose"},
           {aim_, "/user/hand/right/input/aim/pose"},
           {select_, "/user/hand/left/input/select/click"},
           {select_, "/user/hand/right/input/select/click"},
           {back_, "/user/hand/left/input/menu/click"},
           {back_, "/user/hand/right/input/menu/click"},
           {haptic_, "/user/hand/left/output/haptic"},
           {haptic_, "/user/hand/right/output/haptic"}},
          {{grip_, "/user/hand/left/input/grip/pose"},
           {grip_, "/user/hand/right/input/grip/pose"},
           {trigger_, "/user/hand/left/input/select/click"},
           {trigger_, "/user/hand/right/input/select/click"},
           {menu_button_, "/user/hand/left/input/menu/click"},
           {menu_button_, "/user/hand/right/input/menu/click"}});
  suggest("/interaction_profiles/oculus/touch_controller",
          {{aim_, "/user/hand/left/input/aim/pose"},
           {aim_, "/user/hand/right/input/aim/pose"},
           {select_, "/user/hand/left/input/trigger/value"},
           {select_, "/user/hand/right/input/trigger/value"},
           {back_, "/user/hand/left/input/y/click"},
           {back_, "/user/hand/right/input/b/click"},
           {stick_, "/user/hand/left/input/thumbstick"},
           {stick_, "/user/hand/right/input/thumbstick"},
           {haptic_, "/user/hand/left/output/haptic"},
           {haptic_, "/user/hand/right/output/haptic"}},
          {{grip_, "/user/hand/left/input/grip/pose"},
           {grip_, "/user/hand/right/input/grip/pose"},
           {trigger_, "/user/hand/left/input/trigger/value"},
           {trigger_, "/user/hand/right/input/trigger/value"},
           {squeeze_, "/user/hand/left/input/squeeze/value"},
           {squeeze_, "/user/hand/right/input/squeeze/value"},
           {stick_click_, "/user/hand/left/input/thumbstick/click"},
           {stick_click_, "/user/hand/right/input/thumbstick/click"},
           {primary_, "/user/hand/left/input/x/click"},
           {primary_, "/user/hand/right/input/a/click"},
           {secondary_, "/user/hand/left/input/y/click"},
           {secondary_, "/user/hand/right/input/b/click"},
           {menu_button_, "/user/hand/left/input/menu/click"}});
  suggest("/interaction_profiles/valve/index_controller",
          {{aim_, "/user/hand/left/input/aim/pose"},
           {aim_, "/user/hand/right/input/aim/pose"},
           {select_, "/user/hand/left/input/trigger/click"},
           {select_, "/user/hand/right/input/trigger/click"},
           {back_, "/user/hand/left/input/b/click"},
           {back_, "/user/hand/right/input/b/click"},
           {stick_, "/user/hand/left/input/thumbstick"},
           {stick_, "/user/hand/right/input/thumbstick"},
           {haptic_, "/user/hand/left/output/haptic"},
           {haptic_, "/user/hand/right/output/haptic"}},
          {{grip_, "/user/hand/left/input/grip/pose"},
           {grip_, "/user/hand/right/input/grip/pose"},
           {trigger_, "/user/hand/left/input/trigger/value"},
           {trigger_, "/user/hand/right/input/trigger/value"},
           {squeeze_, "/user/hand/left/input/squeeze/value"},
           {squeeze_, "/user/hand/right/input/squeeze/value"},
           {stick_click_, "/user/hand/left/input/thumbstick/click"},
           {stick_click_, "/user/hand/right/input/thumbstick/click"},
           {primary_, "/user/hand/left/input/a/click"},
           {primary_, "/user/hand/right/input/a/click"},
           {secondary_, "/user/hand/left/input/b/click"},
           {secondary_, "/user/hand/right/input/b/click"}});

  XrSessionActionSetsAttachInfo attach = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach.countActionSets = 1;
  attach.actionSets = &action_set_;
  if (!Ok(xrAttachSessionActionSets(session_, &attach),
          "xrAttachSessionActionSets")) {
    return false;
  }
  for (int h = 0; h < 2; ++h) {
    XrActionSpaceCreateInfo space = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
    space.action = aim_;
    space.subactionPath = hands_[h];
    space.poseInActionSpace.orientation.w = 1.0f;
    if (!Ok(xrCreateActionSpace(session_, &space, &aim_space_[h]),
            "xrCreateActionSpace")) {
      return false;
    }
    if (!input_live_) continue;
    space.action = grip_;
    if (!Ok(xrCreateActionSpace(session_, &space, &grip_space_[h]),
            "xrCreateActionSpace(grip)")) {
      return false;
    }
  }
  if (input_live_) {
    AppLog("xr           : input_live=1. The session has the actions of the "
           "mod: the grip pose, the trigger, the squeeze, the stick click, "
           "the face buttons and the menu button");
  }
  return true;
}

bool XrApp::MakeLayer(Layer* layer, uint32_t width, uint32_t height,
                      bool staging) {
  XrSwapchainCreateInfo info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
  info.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
                    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                    XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
  info.format = format_;
  info.sampleCount = 1;
  info.width = width;
  info.height = height;
  info.faceCount = 1;
  info.arraySize = 1;
  info.mipCount = 1;
  if (!Ok(xrCreateSwapchain(session_, &info, &layer->swapchain),
          "xrCreateSwapchain")) {
    return false;
  }
  uint32_t count = 0;
  xrEnumerateSwapchainImages(layer->swapchain, 0, &count, nullptr);
  layer->images.assign(count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
  xrEnumerateSwapchainImages(
      layer->swapchain, count, &count,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(layer->images.data()));
  layer->width = width;
  layer->height = height;
  layer->swap_red_blue =
      format_ == VK_FORMAT_B8G8R8A8_SRGB || format_ == VK_FORMAT_B8G8R8A8_UNORM;
  if (!staging) return true;
  return MakeStaging(&gpu_, static_cast<VkDeviceSize>(width) * height * 4,
                     &layer->staging);
}

uint8_t* XrApp::PixelsFor(Layer* layer, size_t bytes) {
  if (layer->staging.mapped == nullptr) return nullptr;
  if (!frame_loop_) {
    // The GPU may still copy the buffer of the frame before.
    vkWaitForFences(gpu_.device, 1, &gpu_.fence, VK_TRUE, UINT64_MAX);
    return static_cast<uint8_t*>(layer->staging.mapped);
  }
  if (frame_loop::Direct(staging_direct_, layer->dirty,
                         GpuSerialDone(&gpu_, layer->staging_serial))) {
    if (time_log_) {
      ++times_.direct;
      times_.direct_bytes += bytes;
    }
    return static_cast<uint8_t*>(layer->staging.mapped);
  }
  // The GPU may still copy the staging buffer, so the pixels wait in memory
  // for the next copy (CopyLayer).
  layer->mirror.resize(static_cast<size_t>(layer->staging.size));
  layer->dirty = true;
  return layer->mirror.data();
}

void XrApp::SetPixels(Layer* layer, const std::vector<uint8_t>& rgba) {
  const size_t bytes = static_cast<size_t>(layer->width) * layer->height * 4;
  if (rgba.size() < bytes || layer->staging.size < bytes) return;
  uint8_t* out = PixelsFor(layer, bytes);
  if (out == nullptr) return;
  if (!layer->swap_red_blue) {
    memcpy(out, rgba.data(), bytes);
    return;
  }
  for (size_t i = 0; i < bytes; i += 4) {
    out[i] = rgba[i + 2];
    out[i + 1] = rgba[i + 1];
    out[i + 2] = rgba[i];
    out[i + 3] = rgba[i + 3];
  }
}

void XrApp::SetMenuPixels(const std::vector<uint8_t>& rgba) {
  SetPixels(&menu_, rgba);
  ++menu_.revision;
  menu_written_ = true;
}

void XrApp::SetCursorPixels(const std::vector<uint8_t>& rgba) {
  SetPixels(&cursor_, rgba);
  ++cursor_.revision;
  cursor_written_ = true;
}

bool XrApp::LayerChanged(Layer* layer) {
  if (copy_on_change_ && layer->released == layer->revision) {
    ++copies_skipped_;
    return false;
  }
  return true;
}

// The staging buffer into the next image of the swapchain. The runtime
// takes a released image in the layout of a color attachment.
void XrApp::CopyLayer(Layer* layer) {
  if (!LayerChanged(layer)) return;
  // With no command buffer (the ring could not make one) the frame copies
  // nothing.
  if (frame_loop_ && FrameCommands() == VK_NULL_HANDLE) return;
  // A card with xr_layer_rect=1: only the rect that its quad shows.
  const frame_loop::Rect rect =
      frame_loop::CopyRect(frame_loop_ && layer_rect_, layer->rect, layer->width, layer->height);
  if (frame_loop_ && layer->dirty) {
    // New pixels go into the staging buffer only when the GPU finished the
    // last copy of it. Until then the layer keeps its image.
    if (!GpuSerialDone(&gpu_, layer->staging_serial)) {
      ++staging_skips_;
      return;
    }
    const int before = PartTo(kPartStaging);
    uint8_t* to = static_cast<uint8_t*>(layer->staging.mapped);
    const uint8_t* from = layer->mirror.data();
    size_t bytes = 0;
    if (rect.width == layer->width && rect.height == layer->height) {
      bytes = layer->mirror.size() < layer->staging.size ? layer->mirror.size()
                                                          : static_cast<size_t>(layer->staging.size);
      memcpy(to, from, bytes);
    } else {
      const size_t row = static_cast<size_t>(rect.width) * 4;
      for (uint32_t r = 0; r < rect.height; ++r) {
        const size_t at = (static_cast<size_t>(rect.y + r) * layer->width + rect.x) * 4;
        memcpy(to + at, from + at, row);
      }
      bytes = row * rect.height;
      if (time_log_) ++times_.staged_rects;
    }
    if (time_log_) {
      ++times_.staged;
      times_.staged_bytes += bytes;
    }
    PartTo(before);
    layer->dirty = false;
  }
  uint32_t index = 0;
  if (!NextImage(layer, !frame_loop_ || image_wait_, &index)) return;
  const VkImage image = layer->images[index].image;
  if (frame_loop_) {
    // Into the command buffer of the frame, which SubmitFrame submits before
    // the release.
    VkCommandBuffer cmd = FrameCommands();
    ImageBarrier(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT);
    if (rect.width == layer->width && rect.height == layer->height) {
      CopyToImage(cmd, layer->staging, image, layer->width, layer->height);
    } else {
      VkBufferImageCopy region = {};
      region.bufferOffset = (static_cast<VkDeviceSize>(rect.y) * layer->width + rect.x) * 4;
      region.bufferRowLength = layer->width;
      region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.imageSubresource.layerCount = 1;
      region.imageOffset = {static_cast<int32_t>(rect.x), static_cast<int32_t>(rect.y), 0};
      region.imageExtent = {rect.width, rect.height, 1};
      vkCmdCopyBufferToImage(cmd, layer->staging.buffer, image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }
    ImageBarrier(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT,
                 VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    to_release_.push_back(layer->swapchain);
    layer->staging_serial = GpuNextSerial(gpu_);
    layer->released = layer->revision;
    memcpy(layer->released_rect, layer->rect, sizeof(layer->rect));
    ++copies_;
    return;
  }
  // The renderer submits to the same queue.
  LockQueue();
  const int before = PartTo(kPartBegin);
  VkCommandBuffer cmd = GpuBegin(&gpu_);
  PartTo(before);
  ImageBarrier(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
               VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT);
  CopyToImage(cmd, layer->staging, image, layer->width, layer->height);
  ImageBarrier(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  const int outer = PartTo(kPartSwapchain);
  const double submit_start = time_log_ ? ClockMs() : 0.0;
  GpuSubmit(&gpu_, VK_NULL_HANDLE, VK_NULL_HANDLE);
  const double release_start = time_log_ ? ClockMs() : 0.0;
  XrSwapchainImageReleaseInfo release = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  xrReleaseSwapchainImage(layer->swapchain, &release);
  UnlockQueue();
  if (time_log_) {
    const double now = ClockMs();
    step_ms_[kStepSubmit] += release_start - submit_start;
    step_ms_[kStepRelease] += now - release_start;
  }
  PartTo(outer);
  layer->released = layer->revision;
  ++copies_;
}

int XrApp::PartTo(int part) {
  const int before = part_;
  if (!parts_on_ || part == part_) return before;
  const double now = ClockMs();
  const double cpu = ThreadCpuMs();
  part_ms_[part_] += now - part_at_;
  part_cpu_ms_[part_] += cpu - part_cpu_at_;
  part_at_ = now;
  part_cpu_at_ = cpu;
  part_ = part;
  return before;
}

VkCommandBuffer XrApp::FrameCommands() {
  if (frame_cmd_ == VK_NULL_HANDLE) {
    const int before = PartTo(kPartBegin);
    frame_cmd_ = GpuBegin(&gpu_);
    PartTo(before);
  }
  return frame_cmd_;
}

void XrApp::SubmitFrame() {
  if (frame_cmd_ == VK_NULL_HANDLE) return;
  PartTo(kPartSwapchain);
  const double submit_start = time_log_ ? ClockMs() : 0.0;
  LockQueue();
  GpuSubmit(&gpu_, VK_NULL_HANDLE, VK_NULL_HANDLE);
  const double release_start = time_log_ ? ClockMs() : 0.0;
  XrSwapchainImageReleaseInfo release = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  for (XrSwapchain s : to_release_) xrReleaseSwapchainImage(s, &release);
  UnlockQueue();
  if (time_log_) {
    const double now = ClockMs();
    step_ms_[kStepSubmit] += release_start - submit_start;
    step_ms_[kStepRelease] += now - release_start;
  }
  to_release_.clear();
  frame_cmd_ = VK_NULL_HANDLE;
}

bool XrApp::NextImage(Layer* layer, bool block, uint32_t* index) {
  // The calls of the swapchain have steps of their own.
  struct Back {
    XrApp* app;
    int part;
    ~Back() { app->PartTo(part); }
  } back = {this, PartTo(kPartSwapchain)};
  const double start = time_log_ ? ClockMs() : 0.0;
  if (layer->pending < 0) {
    uint32_t i = 0;
    XrSwapchainImageAcquireInfo acquire = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    LockQueue();
    const bool acquired = Ok(xrAcquireSwapchainImage(layer->swapchain, &acquire, &i),
                             "xrAcquireSwapchainImage");
    UnlockQueue();
    if (time_log_) ++times_.acquires;
    if (!acquired) return false;
    layer->pending = static_cast<int>(i);
  }
  const double wait_start = time_log_ ? ClockMs() : 0.0;
  // With no block the runtime gives the image at once or says that it is
  // still busy (XR_TIMEOUT_EXPIRED). Then the image stays acquired, and the
  // next call waits for the same image again.
  XrSwapchainImageWaitInfo wait = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  wait.timeout = block ? XR_INFINITE_DURATION : 0;
  const XrResult waited = xrWaitSwapchainImage(layer->swapchain, &wait);
  if (time_log_) {
    const double now = ClockMs();
    step_ms_[kStepAcquire] += wait_start - start;
    step_ms_[kStepImageWait] += now - wait_start;
  }
  if (waited == XR_TIMEOUT_EXPIRED) {
    ++not_ready_;
    if (time_log_) ++times_.not_ready;
    return false;
  }
  if (XR_FAILED(waited)) {
    Ok(waited, "xrWaitSwapchainImage");
    return false;
  }
  *index = static_cast<uint32_t>(layer->pending);
  layer->pending = -1;
  return true;
}

double XrApp::DisplayTimeMs() const {
  if (to_timespec_ == nullptr) return -1.0;
  timespec t = {};
  if (XR_FAILED(to_timespec_(instance_, frame_.predictedDisplayTime, &t))) return -1.0;
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

void XrApp::ShownPicture(double play_start_ms, double arrival_ms, bool fresh) {
  age_pending_ = false;
  if (!time_log_ || play_start_ms <= 0.0) return;
  const double display = DisplayTimeMs();
  if (display < 0.0) return;
  times_.age_ms.Add(display - play_start_ms);
  if (!fresh) return;
  times_.latency_ms.Add(display - play_start_ms);
  if (arrival_ms > 0.0) times_.arrival_ms.Add(display - arrival_ms);
  age_times_ = frame_loop::AgeTimes();
  age_times_.arrival = arrival_ms;
  age_times_.play = play_start_ms;
  age_times_.display = display;
  age_pending_ = true;
}

void XrApp::ShownParts(double input_ms, double ready_ms, double taken_ms) {
  if (!age_pending_) return;
  age_times_.input = input_ms;
  age_times_.ready = ready_ms;
  age_times_.taken = taken_ms;
}

std::vector<XrApp::Layer*> XrApp::StagingLayers() {
  std::vector<Layer*> all = {&menu_, &cursor_, &keyboard_, &wall_, &flat_pixels_};
  for (Card& k : cards_) all.push_back(&k.layer);
  return all;
}

void XrApp::SetFrameLoop(bool on) {
  if (on == frame_loop_ || gpu_.device == VK_NULL_HANDLE) return;
  SubmitFrame();
  if (on) ReadCopyKeys();
  LockQueue();
  GpuSetRing(&gpu_, on);
  UnlockQueue();
  // The device is idle: the copies recorded one time go. The sources of the
  // ring of the pictures can change before the next frame loop.
  for (Screen* s : {&eyes_, &screen_, &panel_, &flat_}) FreeOnce(s);
  // The device is idle. Off: the newest pixels go into the staging buffers.
  // On: the copies in memory start empty, and the next pixels fill them (a
  // read of a staging buffer can be slow). A card writes only its rect, and
  // its quad shows only that rect.
  for (Layer* l : StagingLayers()) {
    if (l->staging.mapped == nullptr) continue;
    if (!on && l->dirty) {
      const size_t n = l->mirror.size() < l->staging.size ? l->mirror.size()
                                                         : static_cast<size_t>(l->staging.size);
      memcpy(l->staging.mapped, l->mirror.data(), n);
    }
    std::vector<uint8_t>().swap(l->mirror);
    l->dirty = false;
    l->staging_serial = 0;
  }
  frame_loop_ = on;
}

bool XrApp::SwapRedBlue() const {
  return format_ == VK_FORMAT_B8G8R8A8_SRGB || format_ == VK_FORMAT_B8G8R8A8_UNORM;
}

void XrApp::OldLoopSync(bool before) {
  if (!renderer_second_) return;
  if (before) {
    vkWaitForFences(gpu_.device, 1, &gpu_.fence, VK_TRUE, UINT64_MAX);
    if (gpu_.ring) {
      LockQueue();
      vkQueueWaitIdle(gpu_.queue);
      UnlockQueue();
    }
    return;
  }
  LockQueue();
  vkQueueWaitIdle(queue2_);
  UnlockQueue();
}

void XrApp::QueueLock() { LockQueue(); }
void XrApp::QueueUnlock() { UnlockQueue(); }
void XrApp::TimeQueueWaits(bool on) { t_time_lock = on; }
double XrApp::TakeQueueWaitMs() {
  const double ms = t_lock_wait_ms;
  t_lock_wait_ms = 0.0;
  return ms;
}

void XrApp::PollEvents(bool* quit) {
  XrEventDataBuffer event = {XR_TYPE_EVENT_DATA_BUFFER};
  while (xrPollEvent(instance_, &event) == XR_SUCCESS) {
    if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
      const auto* changed =
          reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
      state_ = changed->state;
      AppLog("xr           : the session state is %d", static_cast<int>(state_));
      if (state_ == XR_SESSION_STATE_READY) {
        XrSessionBeginInfo begin = {XR_TYPE_SESSION_BEGIN_INFO};
        begin.primaryViewConfigurationType =
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        running_ = Ok(xrBeginSession(session_, &begin), "xrBeginSession");
      } else if (state_ == XR_SESSION_STATE_STOPPING) {
        running_ = false;
        xrEndSession(session_);
      } else if (state_ == XR_SESSION_STATE_EXITING ||
                 state_ == XR_SESSION_STATE_LOSS_PENDING) {
        running_ = false;
        *quit = true;
      }
    } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
      running_ = false;
      *quit = true;
    } else if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
      // The headset recentered its LOCAL space, for example from its own
      // menu. The mod keeps an origin of its own, so it recenters too.
      const auto* change =
          reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(&event);
      if (change->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL) {
        ++recenters_;
        AppLog("xr           : the LOCAL space recentered (%u recenters)",
               recenters_);
      }
    } else if (event.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
      profiles_read_ = false;
    } else if (event.type == XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB) {
      // The display period of the next frames follows the rate, and the input
      // channel gives it to the mod (predictedDisplayPeriod).
      const auto* rate =
          reinterpret_cast<const XrEventDataDisplayRefreshRateChangedFB*>(&event);
      AppLog("xr           : the display rate changed from %.2f to %.2f Hz",
             rate->fromDisplayRefreshRate, rate->toDisplayRefreshRate);
    }
    event = {XR_TYPE_EVENT_DATA_BUFFER};
  }
}

bool XrApp::BeginFrame(XrInput* input) {
  XrFrameWaitInfo wait = {XR_TYPE_FRAME_WAIT_INFO};
  frame_ = {XR_TYPE_FRAME_STATE};
  const double wait_start = time_log_ ? ClockMs() : 0.0;
  wait_end_ms_ = -1.0;
  if (!Ok(xrWaitFrame(session_, &wait, &frame_), "xrWaitFrame")) return false;
  if (time_log_) {
    wait_end_ms_ = ClockMs();
    last_wait_ms_ = wait_end_ms_ - wait_start;
    times_.wait.Add(last_wait_ms_);
  }
  // THE STEPS of this frame (the time log): from here to the end of
  // xrEndFrame. The thread of the session adds up its waits for the lock.
  for (double& ms : step_ms_) ms = 0.0;
  if (time_log_) {
    TimeQueueWaits(true);
    TakeQueueWaitMs();
  }
  const double begin_start = time_log_ ? ClockMs() : 0.0;
  XrFrameBeginInfo begin = {XR_TYPE_FRAME_BEGIN_INFO};
  LockQueue();
  const bool begun = Ok(xrBeginFrame(session_, &begin), "xrBeginFrame");
  UnlockQueue();
  const double actions_start = time_log_ ? ClockMs() : 0.0;
  if (time_log_) step_ms_[kStepBegin] = actions_start - begin_start;
  if (!begun) return false;
  // The time of the actions and the poses, at each return below.
  struct ActionsTimer {
    XrApp* app;
    double start;
    ~ActionsTimer() {
      if (app->time_log_) app->step_ms_[kStepActions] += ClockMs() - start;
    }
  } actions_timer{this, actions_start};

  *input = XrInput();
  synced_ = false;
  if (state_ != XR_SESSION_STATE_FOCUSED) return true;
  XrActiveActionSet active = {action_set_, XR_NULL_PATH};
  XrActionsSyncInfo sync = {XR_TYPE_ACTIONS_SYNC_INFO};
  sync.countActiveActionSets = 1;
  sync.activeActionSets = &active;
  const XrResult synced = xrSyncActions(session_, &sync);
  if (XR_FAILED(synced)) return true;
  // XR_SESSION_NOT_FOCUSED is a success code with no input.
  synced_ = synced == XR_SUCCESS;
  for (int h = 0; h < 2; ++h) {
    XrActionStateGetInfo get = {XR_TYPE_ACTION_STATE_GET_INFO};
    get.subactionPath = hands_[h];
    XrActionStateBoolean b = {XR_TYPE_ACTION_STATE_BOOLEAN};
    get.action = select_;
    if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &get, &b))) {
      input->select[h] = b.isActive && b.currentState;
    }
    b = {XR_TYPE_ACTION_STATE_BOOLEAN};
    get.action = back_;
    if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &get, &b))) {
      input->back[h] = b.isActive && b.currentState;
    }
    if (menu_button_ != XR_NULL_HANDLE) {
      b = {XR_TYPE_ACTION_STATE_BOOLEAN};
      get.action = menu_button_;
      if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &get, &b))) {
        input->menu[h] = b.isActive && b.currentState;
      }
    }
    if (stick_click_ != XR_NULL_HANDLE) {
      b = {XR_TYPE_ACTION_STATE_BOOLEAN};
      get.action = stick_click_;
      if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &get, &b))) {
        input->stick_click[h] = b.isActive && b.currentState;
      }
    }
    XrActionStateVector2f v = {XR_TYPE_ACTION_STATE_VECTOR2F};
    get.action = stick_;
    if (XR_SUCCEEDED(xrGetActionStateVector2f(session_, &get, &v)) && v.isActive) {
      input->stick[h][0] = v.currentState.x;
      input->stick[h][1] = v.currentState.y;
    }
    XrSpaceLocation where = {XR_TYPE_SPACE_LOCATION};
    if (XR_SUCCEEDED(xrLocateSpace(aim_space_[h], local_,
                                   frame_.predictedDisplayTime, &where))) {
      const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
      if ((where.locationFlags & need) == need) {
        input->aim_valid[h] = true;
        input->aim_position[h][0] = where.pose.position.x;
        input->aim_position[h][1] = where.pose.position.y;
        input->aim_position[h][2] = where.pose.position.z;
        input->aim_orientation[h][0] = where.pose.orientation.x;
        input->aim_orientation[h][1] = where.pose.orientation.y;
        input->aim_orientation[h][2] = where.pose.orientation.z;
        input->aim_orientation[h][3] = where.pose.orientation.w;
      }
    }
  }
  return true;
}

void XrApp::EndFrame(const float menu_center[3], const float menu_size[2],
                     bool show_cursor, const float cursor_point[3],
                     float cursor_size) {
  namespace ql = quad_layer_math;
  const double end_frame_start = time_log_ ? ClockMs() : 0.0;
  // The steps of the swapchains so far belong to this call too.
  const double before_steps = step_ms_[kStepAcquire] + step_ms_[kStepImageWait] +
                              step_ms_[kStepSubmit] + step_ms_[kStepRelease];
  // THE PARTS OF THE LAYERS: the clock of the parts starts here.
  parts_on_ = time_log_;
  if (parts_on_) {
    for (int k = 0; k < kParts; ++k) part_ms_[k] = part_cpu_ms_[k] = 0.0;
    part_ = kPartRest;
    part_at_ = end_frame_start;
    part_cpu_at_ = ThreadCpuMs();
  }
  // The quads: the two of the stereo screen or the quad layers of the mod
  // (ql::kMostLayers), then the keyboard, the menu and the cursor.
  constexpr int kQuads = ql::kMostLayers + 3;
  XrCompositionLayerQuad quads[kQuads];
  for (XrCompositionLayerQuad& q : quads) q = {XR_TYPE_COMPOSITION_LAYER_QUAD};
  XrCompositionLayerProjectionView views[2] = {
      {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
      {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
  XrCompositionLayerProjection projection = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  // THE CENTRE LAYER (fovea_res_layer=1): a second projection layer.
  XrCompositionLayerProjectionView center_views[2] = {
      {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
      {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
  XrCompositionLayerProjection center = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  const XrCompositionLayerBaseHeader* layers[kQuads + 2];
  uint32_t count = 0;
  int used = 0;  // the quads in use
  // THE PROJECTION LAYER of the renderer, behind the menu: each eye with the
  // pose that the mod drew it from and the angles of view that the mod
  // gives (kView). The runtime reprojects it to the pose of the head, so the
  // app adds no motion. A frame of the cut scene window (stream_layers=1)
  // shows the same eye pictures on a quad of each eye at the pose of the
  // window.
  // THE SCREEN QUAD of a frame with no scene (stream_screen=1) takes the
  // place of the pictures of the world.
  const bool flat = frame_.shouldRender && flat_on_ && screen_quad_on_;
  const bool source = frame_.shouldRender && screen_source_ != VK_NULL_HANDLE && !flat;
  const bool want_world = source && projection_on_ && view_newest_ && HasEyes();
  // With stream_projection=0 each picture shows on the stereo screen, a frame
  // of the cut scene window too, and no quad layer of the mod goes out.
  const bool want_window = source && projection_on_ && layers_on_ &&
                           ql::AsScreen(newest_layers_) && HasEyes();
  bool world = false;
  bool window = false;
  if (want_world || want_window) {
    PartTo(kPartEyes);
    for (int eye = 0; eye < 2; ++eye) {
      // The values go with the picture of the image that the eye released.
      if (CopyScreen(&eyes_, eye)) {
        shown_view_[eye] = newest_view_;
        view_shown_[eye] = view_newest_;
        motion_.TakeNewest(eye);
        // The rects in pixels of the swapchain: a picture of another size
        // went into it with a blit (fovea_res_math::FitAtlas).
        shown_atlas_[eye] = fovea_res_math::FitAtlas(newest_atlas_, eyes_.layer[eye].width,
                                                     eyes_.layer[eye].height);
        if (eye == 0) {
          shown_layers_ = newest_layers_;
          shown_frame_ = newest_frame_;
        }
      }
    }
    world = want_world && view_shown_[0] && view_shown_[1];
    window = !world && want_window && eyes_.layer[0].released > 0 &&
             eyes_.layer[1].released > 0 && ql::AsScreen(shown_layers_);
    PartTo(kPartRest);
  }
  if (world) {
    // THE POSES OF THE VIEWS: the poses of the picture of each eye, with the
    // difference of the place of the player since its frame (motion_comp).
    float pose_position[2][3], pose_orientation[2][4];
    for (int eye = 0; eye < 2; ++eye) {
      const scene_stream::ViewEye& e = shown_view_[eye].eyes[eye];
      const scene_stream::ViewEye* both = shown_view_[eye].eyes;
      const float head[3] = {(both[0].position[0] + both[1].position[0]) * 0.5f,
                             (both[0].position[1] + both[1].position[1]) * 0.5f,
                             (both[0].position[2] + both[1].position[2]) * 0.5f};
      memcpy(pose_position[eye], e.position, sizeof(pose_position[eye]));
      memcpy(pose_orientation[eye], e.orientation, sizeof(pose_orientation[eye]));
      motion_.View(eye, head, pose_position[eye], pose_orientation[eye]);
    }
    // One line for each new picture of the layer: the frame of the mod of its
    // values, and the difference of the place of the player.
    if (!said_projection_ || shown_view_[0].frame != said_frame_) {
      said_frame_ = shown_view_[0].frame;
      PartTo(kPartLog);
      char motion[320];
      motion_.Describe(motion, sizeof(motion));
      AppLog("xr           : the projection layer shows the picture of frame %llu%s",
             static_cast<unsigned long long>(said_frame_), motion);
      PartTo(kPartRest);
    }
    for (int eye = 0; eye < 2; ++eye) {
      const scene_stream::ViewEye& e = shown_view_[eye].eyes[eye];
      XrCompositionLayerProjectionView& v = views[eye];
      const float* p = pose_position[eye];
      const float* q = pose_orientation[eye];
      v.pose.position = {p[0], p[1], p[2]};
      v.pose.orientation = {q[0], q[1], q[2], q[3]};
      v.fov = {e.fov[0], e.fov[1], e.fov[2], e.fov[3]};
      v.subImage.swapchain = eyes_.layer[eye].swapchain;
      v.subImage.imageRect.offset = {0, 0};
      v.subImage.imageRect.extent = {static_cast<int32_t>(eyes_.layer[eye].width),
                                     static_cast<int32_t>(eyes_.layer[eye].height)};
      v.subImage.imageArrayIndex = 0;
      // THE MIDDLE AS ITS OWN LAYER: the layer of the whole view shows the
      // outer image, and the centre layer the centre image of the same image
      // of the swapchain, at the same pose, over the part of the view of its
      // rect (fovea_res_math::CenterFov).
      const fovea_res_math::Atlas& a = shown_atlas_[eye];
      if (a.on) {
        v.subImage.imageRect.offset = {a.outer[0], a.outer[1]};
        v.subImage.imageRect.extent = {a.outer[2], a.outer[3]};
        XrCompositionLayerProjectionView& c = center_views[eye];
        c.pose = v.pose;
        float fov[4];
        fovea_res_math::CenterFov(e.fov, a.view, fov);
        c.fov = {fov[0], fov[1], fov[2], fov[3]};
        c.subImage.swapchain = v.subImage.swapchain;
        c.subImage.imageRect.offset = {a.center[0], a.center[1]};
        c.subImage.imageRect.extent = {a.center[2], a.center[3]};
        c.subImage.imageArrayIndex = 0;
      }
    }
    projection.layerFlags = 0;
    projection.space = local_;
    projection.viewCount = 2;
    projection.views = views;
    layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);
    ++projection_frames_;
    // The centre layer only when both eyes have it. Its alpha goes to 0 at its
    // edge, and its colors have no alpha in them.
    if (shown_atlas_[0].on && shown_atlas_[1].on) {
      center.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
                          XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
      center.space = local_;
      center.viewCount = 2;
      center.views = center_views;
      layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&center);
      ++center_frames_;
      if (!said_center_) {
        said_center_ = true;
        const fovea_res_math::Atlas& a = shown_atlas_[0];
        AppLog("xr           : the centre of fovea_res goes out as a projection layer of "
               "its own over the projection layer (fovea_res_layer=1): the outer image at "
               "%d %d, %d by %d, and the centre image at %d %d, %d by %d of the swapchain, "
               "the angles of view of the centre %.4f %.4f %.4f %.4f",
               a.outer[0], a.outer[1], a.outer[2], a.outer[3], a.center[0], a.center[1],
               a.center[2], a.center[3], center_views[0].fov.angleLeft,
               center_views[0].fov.angleRight, center_views[0].fov.angleUp,
               center_views[0].fov.angleDown);
      }
    }
    if (!said_sharp_) {
      said_sharp_ = true;
      LogSharpness(shown_view_[0], shown_atlas_[0]);
    }
    if (!said_projection_) {
      said_projection_ = true;
      const scene_stream::ViewBody& b = shown_view_[0];
      AppLog("xr           : the pictures of the renderer go out in a projection "
             "layer, with the values of frame %llu of the mod: the left eye at "
             "%.4f %.4f %.4f, the right eye at %.4f %.4f %.4f, the angles of view "
             "%.4f %.4f %.4f %.4f, half angle %.2f degrees",
             static_cast<unsigned long long>(b.frame), b.eyes[0].position[0],
             b.eyes[0].position[1], b.eyes[0].position[2], b.eyes[1].position[0],
             b.eyes[1].position[1], b.eyes[1].position[2], b.eyes[0].fov[0],
             b.eyes[0].fov[1], b.eyes[0].fov[2], b.eyes[0].fov[3],
             b.half_angle * 57.29578f);
    }
  }
  panel_text_ = false;
  if ((world || window) && layers_on_) {
    // THE QUAD LAYERS OF THE MOD, in the order of XrScreen::BuildFrame. The
    // panel image goes into its swapchain for a new picture.
    const ql::FrameLayers& f = shown_layers_;
    bool panel_ok = false;
    if (world && f.panel && panel_source_ != VK_NULL_HANDLE &&
        panel_width_ == f.panel_body.width && panel_height_ == f.panel_body.height) {
      PartTo(kPartPanel);
      if (Made(panel_) &&
          (panel_.layer[0].width != panel_width_ || panel_.layer[0].height != panel_height_)) {
        // The GPU may still copy into the swapchain or the image between.
        LockQueue();
        vkDeviceWaitIdle(gpu_.device);
        UnlockQueue();
        FreeScreenSet(&panel_);
        panel_shown_ = false;
      }
      if (!Made(panel_) && !panel_.failed) {
        const char* why = "";
        if (MakeScreenSet(&panel_, panel_width_, panel_height_, &why, 1)) {
          AppLog("xr           : a swapchain of %u by %u for the panel image of the "
                 "renderer (the panel quads of the mod)", panel_width_, panel_height_);
        } else {
          AppLog("xr           : the swapchain of the panel quads of %u by %u cannot "
                 "be made: %s. The panel quads do not show", panel_width_,
                 panel_height_, why);
        }
      }
      if (Made(panel_)) {
        if (CopyImage(&panel_, 0, panel_source_, 0, panel_width_, panel_height_)) {
          panel_shown_ = true;
        }
        panel_ok = panel_shown_;
      }
      PartTo(kPartRest);
    }
    bool card_ok[bwvr::kCards];
    for (int c = 0; c < bwvr::kCards; ++c) {
      const Card& k = cards_[c];
      card_ok[c] = f.card[c] && !k.failed && k.layer.swapchain != XR_NULL_HANDLE &&
                   k.picture == f.cards[c].picture &&
                   memcmp(k.rect, f.cards[c].rect, sizeof(k.rect)) == 0;
    }
    ql::LayerRef order[ql::kMostLayers];
    const int n = ql::Order(f, world, panel_ok, card_ok, order);
    int panels = 0;
    bool walls = false;
    for (int i = 0; i < n && used < kQuads - 2; ++i) {
      const ql::LayerRef& r = order[i];
      ql::QuadSpec s;
      if (!ql::Spec(f, r, eyes_.layer[0].width, eyes_.layer[0].height, &s)) continue;
      XrSwapchain swapchain = XR_NULL_HANDLE;
      if (r.kind == ql::kLayerPanel) {
        swapchain = panel_.layer[0].swapchain;
        ++panels;
        // The menu panel quad of a dialog box that takes text: the keyboard
        // of the app shows at it (main.cpp, UpdateKeyboard).
        int32_t text_rect[4];
        if (r.index == 0 && ql::PanelText(f.panel_body, text_rect)) {
          panel_text_ = true;
          for (int k = 0; k < 3; ++k) panel_text_position_[k] = s.position[k];
          for (int k = 0; k < 4; ++k) panel_text_orientation_[k] = s.orientation[k];
          panel_text_size_[0] = s.size[0];
          panel_text_size_[1] = s.size[1];
        }
      } else if (r.kind == ql::kLayerCard) {
        Card& k = cards_[r.index];
        PartTo(kPartCards);
        CopyLayer(&k.layer);
        PartTo(kPartRest);
        if (k.layer.released == 0) continue;
        // xr_layer_rect=1: the released image must have the rect of the quad.
        if (!frame_loop::RectShows(frame_loop_ && layer_rect_, k.layer.released_rect,
                                   f.cards[r.index].rect)) {
          continue;
        }
        swapchain = k.layer.swapchain;
        ++k.frames;
        if (!k.said) {
          k.said = true;
          AppLog("xr           : %s goes out as one quad layer over the %s, %.3f by "
                 "%.3f m", ql::CardName(r.index), window ? "cut scene window" : "projection layer",
                 s.size[0], s.size[1]);
        }
      } else if (r.kind == ql::kLayerWall) {
        PartTo(kPartWalls);
        struct Rest {
          XrApp* app;
          ~Rest() { app->PartTo(kPartRest); }
        } rest = {this};
        if (wall_.swapchain == XR_NULL_HANDLE && !wall_failed_) {
          if (!MakeLayer(&wall_, ql::kWallPixels, ql::kWallPixels)) {
            wall_failed_ = true;
            if (wall_.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(wall_.swapchain);
            FreeStaging(&gpu_, &wall_.staging);
            wall_ = Layer();
            AppLog("xr           : no swapchain for the walls of the cut scene window, so "
                   "the view has the window with no walls");
          }
        }
        if (wall_failed_) continue;
        const int alpha = ql::WallAlpha(f.window_body);
        if (alpha != wall_alpha_) {
          std::vector<uint8_t> black(static_cast<size_t>(ql::kWallPixels) * ql::kWallPixels * 4, 0);
          // Black has no color to multiply, so only the alpha changes. It is
          // the last byte of a pixel in each format of the swapchain.
          for (size_t p = 3; p < black.size(); p += 4) black[p] = static_cast<uint8_t>(alpha);
          SetPixels(&wall_, black);
          ++wall_.revision;
          wall_alpha_ = alpha;
        }
        CopyLayer(&wall_);
        if (wall_.released == 0) continue;
        swapchain = wall_.swapchain;
        walls = true;
      } else if (r.kind == ql::kLayerEye) {
        swapchain = eyes_.layer[r.index].swapchain;
        // With an atlas (fovea_res_layer=1) the window shows the same columns
        // of the outer image.
        fovea_res_math::OuterRect(shown_atlas_[r.index], eyes_.layer[0].width,
                                  eyes_.layer[0].height, s.rect, s.rect);
      }
      XrCompositionLayerQuad& q = quads[used++];
      q.layerFlags = s.flags;
      q.space = local_;
      q.eyeVisibility = static_cast<XrEyeVisibility>(s.eye);
      q.subImage.swapchain = swapchain;
      q.subImage.imageRect.offset = {s.rect[0], s.rect[1]};
      q.subImage.imageRect.extent = {s.rect[2], s.rect[3]};
      q.subImage.imageArrayIndex = 0;
      q.pose.position = {s.position[0], s.position[1], s.position[2]};
      q.pose.orientation = {s.orientation[0], s.orientation[1], s.orientation[2],
                            s.orientation[3]};
      q.size = {s.size[0], s.size[1]};
      layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&q);
    }
    if (panels > 0) {
      ++panel_frames_;
      if (!said_panel_) {
        said_panel_ = true;
        AppLog("xr           : the panel quads of the mod go out as %d quad layer(s) over "
               "the projection layer, with the panel image of the renderer", panels);
      }
    }
    if (walls) {
      ++wall_frames_;
      if (!said_walls_) {
        said_walls_ = true;
        AppLog("xr           : the walls of the cut scene window go out as four quad "
               "layers over the other layers");
      }
    }
    if (window) {
      ++window_frames_;
      // One line for each new picture of the window: the frame of the mod.
      if (said_window_frame_ != shown_frame_) {
        said_window_frame_ = shown_frame_;
        PartTo(kPartLog);
        AppLog("xr           : the cut scene window shows the picture of frame %llu",
               static_cast<unsigned long long>(shown_frame_));
        PartTo(kPartRest);
      }
      if (!said_window_) {
        said_window_ = true;
        int32_t x = 0, shown = 0;
        ql::WindowColumns(f.window_body, eyes_.layer[0].width, 0, &x, &shown);
        AppLog("xr           : the cut scene window goes out as two quad layers, %.2f "
               "by %.2f m at %.2f %.2f %.2f m. Each eye shows %d of the %u columns of "
               "its picture", f.window_body.width, f.window_body.height,
               f.window_body.position[0], f.window_body.position[1],
               f.window_body.position[2], shown, eyes_.layer[0].width);
      }
    }
  }
  if (!world && !window && source) {
    // THE STEREO SCREEN of the renderer, behind the menu: a quad for each
    // eye, 2.6 m wide, 2.6 m in front of the head. Only when the picture
    // went out in no projection layer and in no cut scene window.
    if (projection_on_ && !view_newest_ && !said_no_view_) {
      said_no_view_ = true;
      AppLog("xr           : stream_projection=1, but the mod gave no values of "
             "the projection layer for the picture of the renderer. It shows on "
             "the stereo screen");
    }
    const float width = 2.6f;
    const float height = width * screen_.layer[0].height / screen_.layer[0].width;
    for (int eye = 0; eye < 2; ++eye) {
      PartTo(kPartEyes);
      CopyScreen(&screen_, eye);
      PartTo(kPartRest);
      XrCompositionLayerQuad& q = quads[used++];
      q.space = local_;
      q.eyeVisibility = eye == 0 ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
      q.subImage.swapchain = screen_.layer[eye].swapchain;
      q.subImage.imageRect.extent = {static_cast<int32_t>(screen_.layer[eye].width),
                                     static_cast<int32_t>(screen_.layer[eye].height)};
      q.pose.orientation.w = 1.0f;
      q.pose.position = {0.0f, 0.1f, -2.6f};
      q.size = {width, height};
      layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&q);
    }
    ++screen_frames_;
  }
  if (flat) {
    // One quad for both eyes, fixed in the room where the mod put it, with
    // the newest screen picture. A swapchain gets a new image only for a new
    // picture (xr_copy_on_change).
    XrSwapchain swapchain = XR_NULL_HANDLE;
    uint32_t w = 0, h = 0;
    PartTo(kPartScreen);
    if (flat_cpu_) {
      CopyLayer(&flat_pixels_);
      if (flat_pixels_.released > 0) {
        swapchain = flat_pixels_.swapchain;
        w = flat_pixels_.width;
        h = flat_pixels_.height;
      }
    } else if (Made(flat_) && flat_source_ != VK_NULL_HANDLE) {
      CopyImage(&flat_, 0, flat_source_, flat_layer_, flat_width_, flat_height_);
      if (flat_.layer[0].released > 0) {
        swapchain = flat_.layer[0].swapchain;
        w = flat_.layer[0].width;
        h = flat_.layer[0].height;
      }
    }
    PartTo(kPartRest);
    quad_layer_math::QuadSpec s;
    if (swapchain != XR_NULL_HANDLE && screen_quad_math::Spec(flat_body_, w, h, &s)) {
      XrCompositionLayerQuad& q = quads[used++];
      q.layerFlags = s.flags;
      q.space = local_;
      q.eyeVisibility = static_cast<XrEyeVisibility>(s.eye);
      q.subImage.swapchain = swapchain;
      q.subImage.imageRect.offset = {s.rect[0], s.rect[1]};
      q.subImage.imageRect.extent = {s.rect[2], s.rect[3]};
      q.subImage.imageArrayIndex = 0;
      q.pose.position = {s.position[0], s.position[1], s.position[2]};
      q.pose.orientation = {s.orientation[0], s.orientation[1], s.orientation[2],
                            s.orientation[3]};
      q.size = {s.size[0], s.size[1]};
      layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&q);
      ++flat_frames_;
      // One line for the first quad and for each change of the source, of the
      // size of the picture or of the quad.
      const int kind = flat_cpu_ ? 1 : 0;
      if (flat_lines_ < 40 &&
          (kind != said_flat_source_ || w != said_flat_width_ || h != said_flat_height_ ||
           s.size[0] != said_flat_size_[0] || s.size[1] != said_flat_size_[1])) {
        ++flat_lines_;
        said_flat_source_ = kind;
        said_flat_width_ = w;
        said_flat_height_ = h;
        said_flat_size_[0] = s.size[0];
        said_flat_size_[1] = s.size[1];
        AppLog("xr           : the screen quad shows screen %llu of the mod (%s, %u by %u) "
               "as one quad layer, %.2f by %.2f m at %.2f %.2f %.2f m",
               static_cast<unsigned long long>(flat_body_.count),
               flat_cpu_ ? "the pixels of the CPU" : "the picture of the last walk", w, h,
               s.size[0], s.size[1], s.position[0], s.position[1], s.position[2]);
      }
    }
  }
  // THE KEYBOARD OF THE APP under the screen quad.
  if (frame_.shouldRender && keyboard_shown_ && keyboard_written_ && !keyboard_failed_) {
    PartTo(kPartKeyboard);
    CopyLayer(&keyboard_);
    PartTo(kPartRest);
    XrCompositionLayerQuad& q = quads[used++];
    q.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    q.space = local_;
    q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    q.subImage.swapchain = keyboard_.swapchain;
    q.subImage.imageRect.extent = {static_cast<int32_t>(keyboard_.width),
                                   static_cast<int32_t>(keyboard_.height)};
    q.pose.position = {keyboard_position_[0], keyboard_position_[1], keyboard_position_[2]};
    q.pose.orientation = {keyboard_orientation_[0], keyboard_orientation_[1],
                          keyboard_orientation_[2], keyboard_orientation_[3]};
    q.size = {keyboard_size_[0], keyboard_size_[1]};
    layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&q);
    ++keyboard_frames_;
  }
  // THE MENU AND THE CURSOR, over each layer of the mod. The cursor also
  // shows on the keyboard with no menu: with the turn of the keyboard, 4 mm
  // in front of it along its normal.
  if (frame_.shouldRender && show_cursor && cursor_written_ && !menu_shown_) {
    PartTo(kPartCursor);
    CopyLayer(&cursor_);
    PartTo(kPartRest);
    XrCompositionLayerQuad& c = quads[used++];
    c.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    c.space = local_;
    c.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    c.subImage.swapchain = cursor_.swapchain;
    c.subImage.imageRect.extent = {static_cast<int32_t>(cursor_.width),
                                   static_cast<int32_t>(cursor_.height)};
    const float* q = keyboard_orientation_;
    const float front[3] = {0.0f, 0.0f, 0.004f};
    float ahead[3];
    controller_math::QuatRotate(q, front, ahead);
    c.pose.orientation = {q[0], q[1], q[2], q[3]};
    c.pose.position = {cursor_point[0] + ahead[0], cursor_point[1] + ahead[1],
                       cursor_point[2] + ahead[2]};
    c.size = {cursor_size, cursor_size};
    layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&c);
  }
  if (frame_.shouldRender && menu_written_ && menu_shown_) {
    PartTo(kPartMenu);
    CopyLayer(&menu_);
    PartTo(kPartRest);
    XrCompositionLayerQuad& q = quads[used++];
    q.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    q.space = local_;
    q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    q.subImage.swapchain = menu_.swapchain;
    q.subImage.imageRect.extent = {static_cast<int32_t>(menu_.width),
                                   static_cast<int32_t>(menu_.height)};
    q.pose.orientation.w = 1.0f;
    q.pose.position = {menu_center[0], menu_center[1], menu_center[2]};
    q.size = {menu_size[0], menu_size[1]};
    layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&q);
    if (show_cursor && cursor_written_) {
      PartTo(kPartCursor);
      CopyLayer(&cursor_);
      PartTo(kPartRest);
      XrCompositionLayerQuad& c = quads[used++];
      c.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
      c.space = local_;
      c.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
      c.subImage.swapchain = cursor_.swapchain;
      c.subImage.imageRect.extent = {static_cast<int32_t>(cursor_.width),
                                     static_cast<int32_t>(cursor_.height)};
      c.pose.orientation.w = 1.0f;
      // A little in front of the menu, so the menu does not hide it.
      c.pose.position = {cursor_point[0], cursor_point[1], cursor_point[2] + 0.004f};
      c.size = {cursor_size, cursor_size};
      layers[count++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&c);
    }
  }
  XrFrameEndInfo end = {XR_TYPE_FRAME_END_INFO};
  end.displayTime = frame_.predictedDisplayTime;
  end.environmentBlendMode = blend_;
  end.layerCount = count;
  end.layers = layers;
  // THE FRAME LOOP: the copies of this frame go to the GPU in one submit,
  // and their swapchain images go back to the runtime, before xrEndFrame.
  SubmitFrame();
  PartTo(kPartSwapchain);
  const double end_start = time_log_ ? ClockMs() : 0.0;
  if (parts_on_) {
    parts_on_ = false;
    for (int k = 0; k < kParts; ++k) {
      times_.parts[k].Add(part_ms_[k]);
      times_.part_cpu[k].Add(part_cpu_ms_[k]);
    }
  }
  LockQueue();
  Ok(xrEndFrame(session_, &end), "xrEndFrame");
  UnlockQueue();
  // THE PARTS OF THE AGE of a new picture of this frame, and the lines of
  // the motion of the stick and of the parts each 10 s.
  if (time_log_) {
    const double now = ClockMs();
    if (age_pending_) {
      age_pending_ = false;
      age_times_.ended = now;
      age_.Add(age_times_);
    }
    age_.Tick(now, true);
    motion_.Tick(now, true);
  }
  if (time_log_ && wait_end_ms_ >= 0.0) {
    const double now = ClockMs();
    times_.end.Add(now - end_start);
    times_.work.Add(now - wait_end_ms_);
    step_ms_[kStepEnd] = now - end_start;
    const double swapchain_steps = step_ms_[kStepAcquire] + step_ms_[kStepImageWait] +
                                   step_ms_[kStepSubmit] + step_ms_[kStepRelease] -
                                   before_steps;
    step_ms_[kStepLayers] = (end_start - end_frame_start) - swapchain_steps;
    step_ms_[kStepLock] = TakeQueueWaitMs();
    for (int k = 0; k < kSteps; ++k) times_.steps[k].Add(step_ms_[k]);
  }
}

void XrApp::TakeTimes(FrameTimes* out) {
  *out = times_;
  out->fence_wait = gpu_.fence_wait;
  out->busy = gpu_.busy;
  times_ = FrameTimes();
  gpu_.fence_wait.Reset();
  gpu_.busy.Reset();
}

void XrApp::SetKeyboardPixels(const std::vector<uint8_t>& rgba, uint32_t width,
                              uint32_t height) {
  if (keyboard_failed_) return;
  if (keyboard_.swapchain == XR_NULL_HANDLE) {
    if (!MakeLayer(&keyboard_, width, height)) {
      keyboard_failed_ = true;
      AppLog("xr           : the runtime gave no swapchain for the keyboard of %u by %u. "
             "The keyboard does not show", width, height);
      return;
    }
    AppLog("xr           : the keyboard is a quad of %u by %u", width, height);
  }
  if (width != keyboard_.width || height != keyboard_.height) return;
  SetPixels(&keyboard_, rgba);
  ++keyboard_.revision;
  keyboard_written_ = true;
}

bool XrApp::PanelTextQuad(float position[3], float orientation[4], float size[2]) const {
  if (!panel_text_) return false;
  for (int i = 0; i < 3; ++i) position[i] = panel_text_position_[i];
  for (int i = 0; i < 4; ++i) orientation[i] = panel_text_orientation_[i];
  size[0] = panel_text_size_[0];
  size[1] = panel_text_size_[1];
  return true;
}

void XrApp::SetKeyboard(bool shown, const float position[3], const float orientation[4],
                        const float size[2]) {
  keyboard_shown_ = shown;
  if (!shown) return;
  for (int i = 0; i < 3; ++i) keyboard_position_[i] = position[i];
  for (int i = 0; i < 4; ++i) keyboard_orientation_[i] = orientation[i];
  keyboard_size_[0] = size[0];
  keyboard_size_[1] = size[1];
}

void XrApp::SetScreenLayers(const quad_layer_math::FrameLayers& layers,
                            const quad_layer_math::AppPictures& pictures, VkImage panel,
                            uint32_t panel_width, uint32_t panel_height) {
  newest_layers_ = layers;
  for (int c = 0; c < bwvr::kCards; ++c) {
    if (layers.card[c]) TakeCard(c, layers.cards[c], pictures);
  }
  panel_source_ = layers.panel ? panel : VK_NULL_HANDLE;
  panel_frozen_ = false;
  panel_width_ = panel_width;
  panel_height_ = panel_height;
  // A new picture has a new panel image.
  if (panel_source_ != VK_NULL_HANDLE) ++panel_.layer[0].revision;
}

void XrApp::SetScreenQuadImage(const scene_stream::ScreenBody& body, VkImage image,
                               uint32_t layer, uint32_t image_width, uint32_t image_height) {
  if (!screen_quad_on_ || image == VK_NULL_HANDLE || image_width == 0 || image_height == 0) {
    return;
  }
  // A picture of a new size needs a new swapchain.
  if (Made(flat_) &&
      (flat_.layer[0].width != image_width || flat_.layer[0].height != image_height)) {
    // The GPU may still copy into the swapchain or the image between.
    LockQueue();
    vkDeviceWaitIdle(gpu_.device);
    UnlockQueue();
    FreeScreenSet(&flat_);
  }
  if (!Made(flat_)) {
    if (flat_.failed) return;
    const char* why = "";
    if (!MakeScreenSet(&flat_, image_width, image_height, &why, 1)) {
      AppLog("xr           : the swapchain of the screen quad of %u by %u cannot be made: "
             "%s. The frames with no scene do not show", image_width, image_height, why);
      return;
    }
    AppLog("xr           : a swapchain of %u by %u for the screen quad (the frames with no "
           "scene of the mod, stream_screen=1)", image_width, image_height);
  }
  flat_body_ = body;
  flat_cpu_ = false;
  flat_source_ = image;
  flat_frozen_ = false;
  flat_layer_ = layer;
  flat_width_ = image_width;
  flat_height_ = image_height;
  ++flat_.layer[0].revision;
  ++flat_pictures_;
  flat_on_ = true;
}

void XrApp::SetScreenQuadPixels(const scene_stream::ScreenBody& body,
                                const std::vector<uint8_t>& rgba, uint32_t width,
                                uint32_t height) {
  if (!screen_quad_on_ || width == 0 || height == 0 ||
      rgba.size() < static_cast<size_t>(width) * height * 4) {
    return;
  }
  if (flat_pixels_.swapchain != XR_NULL_HANDLE &&
      (flat_pixels_.width != width || flat_pixels_.height != height)) {
    LockQueue();
    vkDeviceWaitIdle(gpu_.device);
    UnlockQueue();
    FreeStaging(&gpu_, &flat_pixels_.staging);
    xrDestroySwapchain(flat_pixels_.swapchain);
    flat_pixels_ = Layer();
  }
  if (flat_pixels_.swapchain == XR_NULL_HANDLE) {
    if (flat_pixels_failed_) return;
    if (!MakeLayer(&flat_pixels_, width, height)) {
      if (flat_pixels_.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(flat_pixels_.swapchain);
      FreeStaging(&gpu_, &flat_pixels_.staging);
      flat_pixels_ = Layer();
      flat_pixels_failed_ = true;
      AppLog("xr           : no swapchain of %u by %u for the pixels of the CPU of the "
             "screen quad, so those frames with no scene do not show", width, height);
      return;
    }
    AppLog("xr           : a swapchain of %u by %u for the pixels of the CPU of the screen "
           "quad", width, height);
  }
  SetPixels(&flat_pixels_, rgba);
  ++flat_pixels_.revision;
  flat_body_ = body;
  flat_cpu_ = true;
  ++flat_pictures_;
  flat_on_ = true;
}

// The swapchain of a card has the size of its texture, as XrScreen::SendCard
// makes it. Its staging buffer holds the newest picture at its rect, and a
// frame copies it into a new image only after a change.
bool XrApp::TakeCard(int index, const scene_stream::CardBody& body,
                     const quad_layer_math::AppPictures& pictures) {
  namespace ql = quad_layer_math;
  Card& k = cards_[index];
  if (k.failed || !ql::CardShows(body) || !pictures.Has(index, body.picture)) return false;
  const int32_t x = body.rect[0], y = body.rect[1], w = body.rect[2], h = body.rect[3];
  if (pictures.Width(index) != static_cast<uint32_t>(w) ||
      pictures.Height(index) != static_cast<uint32_t>(h)) {
    return false;
  }
  // A picture of a new size needs a new swapchain.
  if (k.layer.swapchain != XR_NULL_HANDLE &&
      (k.layer.width != body.texture_width || k.layer.height != body.texture_height)) {
    LockQueue();
    vkDeviceWaitIdle(gpu_.device);
    UnlockQueue();
    FreeStaging(&gpu_, &k.layer.staging);
    xrDestroySwapchain(k.layer.swapchain);
    k.layer = Layer();
    k.picture = 0;
  }
  if (k.layer.swapchain == XR_NULL_HANDLE) {
    if (!MakeLayer(&k.layer, body.texture_width, body.texture_height)) {
      if (k.layer.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(k.layer.swapchain);
      FreeStaging(&gpu_, &k.layer.staging);
      k.layer = Layer();
      k.failed = true;
      AppLog("xr           : no swapchain for %s, so it does not show",
             ql::CardName(index));
      return false;
    }
    AppLog("xr           : %s has its own swapchain, %u by %u", ql::CardName(index),
           body.texture_width, body.texture_height);
  }
  if (k.picture == body.picture && memcmp(k.rect, body.rect, sizeof(k.rect)) == 0) return true;
  // The picture goes straight into the staging buffer when it is free, else
  // into the copy in memory, and CopyLayer puts it into the staging buffer
  // (PixelsFor). Its quad shows only its rect (xr_layer_rect=1).
  uint8_t* out = PixelsFor(&k.layer, static_cast<size_t>(w) * h * 4);
  if (out == nullptr) return false;
  memcpy(k.layer.rect, body.rect, sizeof(k.layer.rect));
  const std::vector<uint8_t>& in = pictures.Pixels(index);
  const size_t row = static_cast<size_t>(w) * 4;
  for (int32_t r = 0; r < h; ++r) {
    const uint8_t* from = in.data() + static_cast<size_t>(r) * row;
    uint8_t* to = out + (static_cast<size_t>(y + r) * k.layer.width + static_cast<size_t>(x)) * 4;
    // The card is BGRA. A swapchain with red first needs the two swapped.
    if (k.layer.swap_red_blue) {
      memcpy(to, from, row);
    } else {
      for (size_t i = 0; i < row; i += 4) {
        to[i] = from[i + 2];
        to[i + 1] = from[i + 1];
        to[i + 2] = from[i];
        to[i + 3] = from[i + 3];
      }
    }
  }
  k.picture = body.picture;
  memcpy(k.rect, body.rect, sizeof(k.rect));
  ++k.layer.revision;
  // The first new pictures of each card, with the hash of the stream, so a
  // test can find each picture. A fade gives a new picture in each frame.
  if (k.layer.revision <= 21) {
    AppLog("xr           : %s gets a new picture %016llx, %d by %d",
           ql::CardName(index), static_cast<unsigned long long>(body.picture), w, h);
  }
  return true;
}

void XrApp::FreeLayers() {
  for (Card& k : cards_) {
    FreeStaging(&gpu_, &k.layer.staging);
    if (k.layer.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(k.layer.swapchain);
    k.layer = Layer();
  }
  FreeScreenSet(&panel_);
  FreeStaging(&gpu_, &wall_.staging);
  if (wall_.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(wall_.swapchain);
  wall_ = Layer();
}

void XrApp::RendererDevice(vk::External* out) {
  renderer_used_ = true;
  *out = vk::External();
  out->get_instance_proc_addr = &vkGetInstanceProcAddr;
  out->instance = gpu_.instance;
  out->physical = gpu_.physical;
  out->device = gpu_.device;
  out->queue_family = gpu_.queue_family;
  out->queue_index = renderer_second_ ? 1 : 0;
  out->max_anisotropy = max_anisotropy_;
  // The renderer writes the bytes of the game, which are sRGB, into an image
  // of UNORM, as on the PC. The copy into the quads keeps the bytes.
  out->color_format = VK_FORMAT_R8G8B8A8_UNORM;
  out->shading_rate = shading_rate_;
  out->window_present = false;
  out->lock = &LockQueue;
  out->unlock = &UnlockQueue;
}

bool XrApp::MakeScreen(uint32_t width, uint32_t height) {
  if (screen_.failed) return false;
  const char* why = "";
  if (!MakeScreenSet(&screen_, width, height, &why)) {
    AppLog("xr           : the stereo screen of %u by %u cannot be made: %s. The "
           "session shows no picture of the renderer", width, height, why);
    return false;
  }
  AppLog("xr           : a stereo screen of two quads of %u by %u for the "
         "pictures of the renderer", width, height);
  return true;
}

bool XrApp::MakeEyes(uint32_t width, uint32_t height) {
  if (eyes_.failed) return false;
  const char* why = "";
  if (!MakeScreenSet(&eyes_, width, height, &why)) {
    AppLog("xr           : the projection layer of %u by %u cannot be made: %s. "
           "The pictures of the renderer show on the stereo screen",
           width, height, why);
    return false;
  }
  AppLog("xr           : a projection layer of two swapchains of %u by %u for "
         "the pictures of the renderer (stream_projection=1)", width, height);
  LogViewSizes("at the first picture of the projection layer");
  return true;
}

// The size that the runtime asks for now, in the session, against the size
// at the start of the app, and the display rate now: the runtime can give
// another size at another start (docs/android.md, "The size of the eye
// pictures").
void XrApp::LogViewSizes(const char* when) {
  uint32_t views = 0;
  xrEnumerateViewConfigurationViews(instance_, system_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                    0, &views, nullptr);
  std::vector<XrViewConfigurationView> view(views, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  if (views == 0 ||
      XR_FAILED(xrEnumerateViewConfigurationViews(instance_, system_,
                                                  XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                  views, &views, view.data()))) {
    return;
  }
  float hz = 0.0f;
  if (get_rate_ != nullptr && session_ != XR_NULL_HANDLE) get_rate_(session_, &hz);
  const uint32_t w = view[0].recommendedImageRectWidth;
  const uint32_t h = view[0].recommendedImageRectHeight;
  AppLog("xr           : %s the headset asks for %u by %u for each eye (at most %u by %u), "
         "%s the size at the start of the app (%u by %u). The display rate is %.2f Hz",
         when, w, h, view[0].maxImageRectWidth, view[0].maxImageRectHeight,
         w == asked_width_ && h == asked_height_ ? "the same as" : "not",
         asked_width_, asked_height_, hz);
}

// THE SHARPNESS OF THE MIDDLE (docs/android.md, "The sharpness of the
// middle"): one line at the first picture of the projection layer. For each
// step from the render to the panel, the pixels for each unit of tangent
// across, and how many pixels of the panel one pixel covers in the middle of
// the lens (eye_size.h). The view of the left eye of the runtime gives the
// span of the panel.
void XrApp::LogSharpness(const scene_stream::ViewBody& b, const fovea_res_math::Atlas& atlas) {
  namespace es = eye_size;
  XrViewLocateInfo locate = {XR_TYPE_VIEW_LOCATE_INFO};
  locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  locate.displayTime = frame_.predictedDisplayTime;
  locate.space = local_;
  XrViewState state = {XR_TYPE_VIEW_STATE};
  XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
  uint32_t found = 0;
  if (frame_.predictedDisplayTime == 0 ||
      XR_FAILED(xrLocateViews(session_, &locate, &state, 2, &found, views)) || found != 2) {
    return;
  }
  const float panel = es::PanelMiddle(es::kPanelWidth, tanf(views[0].fov.angleLeft),
                                      tanf(views[0].fov.angleRight));
  const float mean = es::PixelsPerTangent(static_cast<float>(es::kPanelWidth),
                                          tanf(views[0].fov.angleLeft),
                                          tanf(views[0].fov.angleRight));
  const float picture = es::PixelsPerTangent(static_cast<float>(b.width),
                                             tanf(b.eyes[0].fov[0]), tanf(b.eyes[0].fov[1]));
  char steps[512];
  if (atlas.mres) {
    const float outer = picture * atlas.outer_scale, center = picture * atlas.center_scale;
    snprintf(steps, sizeof(steps),
             "the outer image %.0f (%.2f), the centre image %.0f (%.2f), %s",
             outer, es::PanelPerPixel(panel, outer), center, es::PanelPerPixel(panel, center),
             atlas.on ? "each image a layer of its own in the swapchain, copied pixel for pixel "
                        "(fovea_res_layer=1)"
                      : "the eye picture from the two with a linear filter, at the size of the "
                        "render (fovea_res_layer=0)");
  } else {
    snprintf(steps, sizeof(steps), "no foveated resolution");
  }
  AppLog("xr           : the sharpness of the middle, in pixels for a unit of tangent across "
         "(and the pixels of the panel that one pixel covers in the middle of the lens): the "
         "panel %.0f in the middle (the mean %.0f, the view of the left eye %.3f to %.3f "
         "radians), the render and the eye picture %u by %u %.0f (%.2f), %s. The swapchains of "
         "%u by %u take a copy of the eye picture with no filter. The compositor samples them. "
         "The textures: an anisotropic filter of up to %.0f, the mip level from the pixels of "
         "each image",
         panel, mean, views[0].fov.angleLeft, views[0].fov.angleRight, b.width, b.height,
         picture, es::PanelPerPixel(panel, picture), steps, eyes_.layer[0].width,
         eyes_.layer[0].height, max_anisotropy_);
}

// A set that cannot be made frees each part that it made, and the app does
// not try it again (Screen::failed).
bool XrApp::MakeScreenSet(Screen* screen, uint32_t width, uint32_t height,
                          const char** why, int count) {
  auto fail = [&](const char* reason) {
    FreeScreenSet(screen);
    screen->failed = true;
    *why = reason;
    return false;
  };
  for (int i = 0; i < count && i < 2; ++i) {
    if (!MakeLayer(&screen->layer[i], width, height, false)) {
      return fail("the runtime gave no swapchain (xrCreateSwapchain)");
    }
  }
  // The frame loop copies a slot of the same size with no image between.
  if (frame_loop_) return true;
  if (!MakeBetween(screen, why)) return fail(*why);
  return true;
}

bool XrApp::MakeBetween(Screen* screen, const char** why) {
  const uint32_t width = screen->layer[0].width;
  const uint32_t height = screen->layer[0].height;
  // The picture of the renderer goes into an image of its own format at the
  // size of the swapchains first (a blit), then into the swapchain with a
  // copy, which keeps the bytes also when the swapchain has an sRGB format.
  VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  info.imageType = VK_IMAGE_TYPE_2D;
  // The order of the bytes of the swapchain: the blit puts the channels of
  // the renderer (RGBA) into that order.
  info.format = screen->layer[0].swap_red_blue ? VK_FORMAT_B8G8R8A8_UNORM
                                               : VK_FORMAT_R8G8B8A8_UNORM;
  info.extent = {width, height, 1};
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  auto fail = [&](const char* reason) {
    if (screen->between != VK_NULL_HANDLE) vkDestroyImage(gpu_.device, screen->between, nullptr);
    screen->between = VK_NULL_HANDLE;
    *why = reason;
    return false;
  };
  if (vkCreateImage(gpu_.device, &info, nullptr, &screen->between) != VK_SUCCESS) {
    screen->between = VK_NULL_HANDLE;
    return fail("the image between the renderer and the swapchains (vkCreateImage)");
  }
  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(gpu_.device, screen->between, &req);
  uint32_t type = UINT32_MAX;
  for (uint32_t i = 0; i < gpu_.memory.memoryTypeCount; ++i) {
    if ((req.memoryTypeBits & (1u << i)) != 0 &&
        (gpu_.memory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
      type = i;
      break;
    }
  }
  VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  alloc.allocationSize = req.size;
  alloc.memoryTypeIndex = type;
  if (type == UINT32_MAX) {
    return fail("no memory of the device for the image between the renderer and "
                "the swapchains");
  }
  if (vkAllocateMemory(gpu_.device, &alloc, nullptr, &screen->between_memory) !=
      VK_SUCCESS) {
    screen->between_memory = VK_NULL_HANDLE;
    return fail("the memory of the image between the renderer and the swapchains "
                "(vkAllocateMemory)");
  }
  vkBindImageMemory(gpu_.device, screen->between, screen->between_memory, 0);
  return true;
}

void XrApp::FreeScreenSet(Screen* screen) {
  FreeOnce(screen);
  for (Layer& l : screen->layer) {
    FreeStaging(&gpu_, &l.staging);
    if (l.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(l.swapchain);
    l.swapchain = XR_NULL_HANDLE;
    l.pending = -1;
  }
  if (screen->between != VK_NULL_HANDLE) {
    vkDestroyImage(gpu_.device, screen->between, nullptr);
  }
  if (screen->between_memory != VK_NULL_HANDLE) {
    vkFreeMemory(gpu_.device, screen->between_memory, nullptr);
  }
  screen->between = VK_NULL_HANDLE;
  screen->between_memory = VK_NULL_HANDLE;
}

void XrApp::SetScreenView(bool has, const scene_stream::ViewBody& view) {
  view_newest_ = has;
  if (has) newest_view_ = view;
  newest_frame_ = view.frame;
}

void XrApp::SetScreenSource(VkImage image, uint32_t image_width,
                            uint32_t image_height) {
  for (Layer& l : screen_.layer) ++l.revision;
  for (Layer& l : eyes_.layer) ++l.revision;
  screen_source_ = image;
  source_width_ = image_width;
  source_height_ = image_height;
  eyes_frozen_ = false;
}

void XrApp::MarkScreenChanged() {
  for (Layer& l : screen_.layer) ++l.revision;
  for (Layer& l : eyes_.layer) ++l.revision;
  eyes_frozen_ = false;
}

// One eye of the picture of the renderer into the next image of its
// swapchain.
bool XrApp::CopyScreen(Screen* screen, int eye) {
  // The stereo screen shows the whole view: with an atlas (fovea_res_layer=1)
  // the outer image at the top left of the picture.
  if (screen == &screen_ && newest_atlas_.on) {
    return CopyImage(screen, eye, screen_source_, static_cast<uint32_t>(eye),
                     static_cast<uint32_t>(newest_atlas_.outer[2]),
                     static_cast<uint32_t>(newest_atlas_.outer[3]));
  }
  return CopyImage(screen, eye, screen_source_, static_cast<uint32_t>(eye),
                   source_width_, source_height_);
}

bool XrApp::CopyImage(Screen* screen, int index, VkImage source,
                      uint32_t source_layer, uint32_t source_width,
                      uint32_t source_height) {
  // A source in a slot that the session left gives no copy (FreezeSlotSources).
  const bool frozen = (screen == &panel_) ? panel_frozen_
                      : (screen == &flat_) ? flat_frozen_
                                           : eyes_frozen_;
  if (frozen) return false;
  if (frame_loop_) {
    return CopySlot(screen, index, source, source_layer, source_width, source_height);
  }
  Layer* layer = &screen->layer[index];
  if (!LayerChanged(layer)) return false;
  if (screen->between == VK_NULL_HANDLE) {
    // A set that the frame loop made has no image between.
    const char* why = "";
    if (!MakeBetween(screen, &why)) {
      if (!screen->between_said) {
        screen->between_said = true;
        AppLog("xr           : no image between for a copy of %u by %u: %s", layer->width,
               layer->height, why);
      }
      return false;
    }
  }
  VkImage between = screen->between;
  uint32_t image = 0;
  if (!NextImage(layer, true, &image)) return false;
  const VkImage target = layer->images[image].image;
  LockQueue();
  const int before = PartTo(kPartBegin);
  VkCommandBuffer cmd = GpuBegin(&gpu_);
  PartTo(before);
  VkImageMemoryBarrier b[2] = {{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER},
                               {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}};
  // The layer of the image of the renderer: from a color attachment to a
  // source, and the image between to a target.
  b[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  b[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  b[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  b[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  b[0].srcQueueFamilyIndex = b[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b[0].image = source;
  b[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, source_layer, 1};
  b[1].srcAccessMask = 0;
  b[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  b[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  b[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  b[1].srcQueueFamilyIndex = b[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b[1].image = between;
  b[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, b);
  VkImageBlit blit = {};
  blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, source_layer, 1};
  blit.srcOffsets[1] = {static_cast<int32_t>(source_width),
                        static_cast<int32_t>(source_height), 1};
  blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  blit.dstOffsets[1] = {static_cast<int32_t>(layer->width),
                        static_cast<int32_t>(layer->height), 1};
  // A blit of the same size takes each pixel as it is.
  const bool same = source_width == layer->width && source_height == layer->height;
  vkCmdBlitImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, between,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                 same ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
  // Back to a color attachment for the renderer. The image between to a
  // source, and the swapchain image to a target.
  b[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  b[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  b[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  b[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  b[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  b[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  b[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  b[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT |
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       0, 0, nullptr, 0, nullptr, 2, b);
  ImageBarrier(cmd, target, VK_IMAGE_LAYOUT_UNDEFINED,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkImageCopy copy = {};
  copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.extent = {layer->width, layer->height, 1};
  vkCmdCopyImage(cmd, between, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  ImageBarrier(cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  const int outer = PartTo(kPartSwapchain);
  const double submit_start = time_log_ ? ClockMs() : 0.0;
  GpuSubmit(&gpu_, VK_NULL_HANDLE, VK_NULL_HANDLE);
  const double release_start = time_log_ ? ClockMs() : 0.0;
  XrSwapchainImageReleaseInfo release = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  xrReleaseSwapchainImage(layer->swapchain, &release);
  UnlockQueue();
  if (time_log_) {
    const double now = ClockMs();
    step_ms_[kStepSubmit] += release_start - submit_start;
    step_ms_[kStepRelease] += now - release_start;
  }
  PartTo(outer);
  layer->released = layer->revision;
  ++copies_;
  ++screen->copies[index];
  return true;
}

// ONE COPY FOR EACH EYE. The play thread put the picture into the slot in the
// byte order of the swapchain, at the size of the picture. A swapchain of
// that size takes it with one copy, which keeps the bytes also when the
// swapchain has an sRGB format. Only a swapchain of another size (the stereo
// screen of half the size) needs a blit into the image between first.
bool XrApp::CopySlot(Screen* screen, int index, VkImage source, uint32_t source_layer,
                     uint32_t source_width, uint32_t source_height) {
  Layer* layer = &screen->layer[index];
  if (!LayerChanged(layer)) return false;
  const bool same = source_width == layer->width && source_height == layer->height;
  if (!same && screen->between == VK_NULL_HANDLE) {
    const char* why = "";
    if (!MakeBetween(screen, &why)) {
      if (!screen->between_said) {
        screen->between_said = true;
        AppLog("xr           : no image between for a copy of %u by %u into %u by %u: %s",
               source_width, source_height, layer->width, layer->height, why);
      }
      return false;
    }
  }
  // With no command buffer (the ring could not make one) the frame copies
  // nothing.
  VkCommandBuffer cmd = FrameCommands();
  if (cmd == VK_NULL_HANDLE) return false;
  uint32_t image = 0;
  if (!NextImage(layer, image_wait_, &image)) return false;
  const VkImage target = layer->images[image].image;
  // xr_copy_once=1: the copy of the same size that CopySlot records below,
  // recorded one time.
  if (same && copy_once_) {
    const VkCommandBuffer once =
        OnceCopy(screen, source, source_layer, source_width, source_height, target);
    if (once != VK_NULL_HANDLE) {
      vkCmdExecuteCommands(cmd, 1, &once);
      ++once_run_;
      to_release_.push_back(layer->swapchain);
      layer->released = layer->revision;
      ++copies_;
      ++screen->copies[index];
      return true;
    }
  }
  // The slot is a transfer source already: the last barrier of the play
  // thread made its bytes visible to each later transfer of its queue. On
  // the second queue the session saw the fence of the slot, and this
  // barrier makes the bytes visible to its copy.
  if (renderer_second_) {
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = source;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, source_layer, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
  }
  ImageBarrier(cmd, target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
               VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkImageCopy copy = {};
  copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.extent = {layer->width, layer->height, 1};
  if (same) {
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, source_layer, 1};
    vkCmdCopyImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  } else {
    // A copy of this frame or of a frame before may still read the image
    // between, so the barrier waits for the transfers before it.
    ImageBarrier(cmd, screen->between, VK_IMAGE_LAYOUT_UNDEFINED,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkImageBlit blit = {};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, source_layer, 1};
    blit.srcOffsets[1] = {static_cast<int32_t>(source_width),
                          static_cast<int32_t>(source_height), 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstOffsets[1] = {static_cast<int32_t>(layer->width),
                          static_cast<int32_t>(layer->height), 1};
    vkCmdBlitImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, screen->between,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
    ImageBarrier(cmd, screen->between, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                 VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT);
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    vkCmdCopyImage(cmd, screen->between, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  }
  ImageBarrier(cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  to_release_.push_back(layer->swapchain);
  layer->released = layer->revision;
  ++copies_;
  ++screen->copies[index];
  return true;
}

// The keys of the copies of the session in bwvr.ini of the game folder (the
// folder that the app gives to the renderer).
void XrApp::ReadCopyKeys() {
  if (copy_keys_read_) return;
  copy_keys_read_ = true;
  std::string text;
  const std::string path = std::string(VkPlatformFolder()) + "/bwvr.ini";
  if (FILE* file = fopen(path.c_str(), "rb")) {
    char part[4096];
    size_t n = 0;
    while ((n = fread(part, 1, sizeof(part), file)) > 0) text.append(part, n);
    fclose(file);
  }
  bwvr_ini::IniText ini;
  ini.Parse(text);
  copy_once_ = ini.GetInt("bwvr", "xr_copy_once", 1) != 0;
  gpu_.pool_reset = ini.GetInt("bwvr", "xr_pool_reset", 1) != 0;
  AppLog("xr           : xr_copy_once=%d, xr_pool_reset=%d. %s, and %s",
         copy_once_ ? 1 : 0, gpu_.pool_reset ? 1 : 0,
         copy_once_ ? "Each copy of the same size from a slot of the ring into a "
                      "swapchain image is recorded one time"
                    : "The copies from the slots of the ring are recorded in each frame",
         gpu_.pool_reset ? "each command buffer of the ring of the session has a command "
                           "pool of its own (vkResetCommandPool)"
                         : "the command buffers of the ring of the session are of one "
                           "pool (vkResetCommandBuffer)");
}

// The secondary command buffer with the copy of `source` (layer
// source_layer, width by height, a transfer source) into the swapchain image
// `target` of the same size, with the barriers of CopySlot. Null when the set
// has no free entry or the command buffer cannot be made: then CopySlot
// records the copy in the frame.
VkCommandBuffer XrApp::OnceCopy(Screen* screen, VkImage source, uint32_t source_layer,
                                uint32_t width, uint32_t height, VkImage target) {
  // The ring of the pictures makes the images of a slot again at a new size,
  // also for a slot that the session never took. So the entries go when the
  // source has another size or the ring made or freed an image since their
  // record, and no entry can name an image that the ring freed.
  const uint64_t changes = PictureRingImageChanges();
  if (!frame_loop::OnceSetValid(screen->once_width, screen->once_height, screen->once_changes,
                                width, height, changes)) {
    FreeOnce(screen);
    screen->once_width = width;
    screen->once_height = height;
    screen->once_changes = changes;
  }
  for (const Screen::Once& o : screen->once) {
    if (o.source == source && o.layer == source_layer && o.target == target &&
        o.second == renderer_second_) {
      return o.cmd;
    }
  }
  // 3 slots, 2 layers and up to 8 swapchain images.
  constexpr size_t kMostOnce = 48;
  if (screen->once.size() >= kMostOnce) {
    ++once_full_;
    return VK_NULL_HANDLE;
  }
  VkCommandBufferAllocateInfo alloc = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  alloc.commandPool = gpu_.pool;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  alloc.commandBufferCount = 1;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(gpu_.device, &alloc, &cmd) != VK_SUCCESS) {
    ++once_full_;
    return VK_NULL_HANDLE;
  }
  VkCommandBufferInheritanceInfo inherit = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
  VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  // The command buffers of the ring that run it can be on the GPU at the
  // same time.
  begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  begin.pInheritanceInfo = &inherit;
  vkBeginCommandBuffer(cmd, &begin);
  if (renderer_second_) {
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = source;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, source_layer, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
  }
  ImageBarrier(cmd, target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
               VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT);
  VkImageCopy copy = {};
  copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, source_layer, 1};
  copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.extent = {width, height, 1};
  vkCmdCopyImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  ImageBarrier(cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    vkFreeCommandBuffers(gpu_.device, gpu_.pool, 1, &cmd);
    ++once_full_;
    return VK_NULL_HANDLE;
  }
  screen->once.push_back({source, source_layer, target, renderer_second_, cmd});
  ++once_made_;
  return cmd;
}

// The entries of a set go. The GPU can still run them, so the queue of the
// session is idle first. A command buffer of the frame that is open has no
// entry of this set: a set frees its entries only before its own copy.
void XrApp::FreeOnce(Screen* screen) {
  if (screen->once.empty()) return;
  LockQueue();
  vkQueueWaitIdle(gpu_.queue);
  UnlockQueue();
  for (const Screen::Once& o : screen->once) {
    vkFreeCommandBuffers(gpu_.device, gpu_.pool, 1, &o.cmd);
  }
  screen->once.clear();
  ++once_frees_;
}

void XrApp::PulseWith(int hand, float amplitude, float seconds) {
  if (haptic_ == XR_NULL_HANDLE || state_ != XR_SESSION_STATE_FOCUSED) return;
  XrHapticVibration v = {XR_TYPE_HAPTIC_VIBRATION};
  v.amplitude = amplitude < 0.0f ? 0.0f : (amplitude > 1.0f ? 1.0f : amplitude);
  v.duration = seconds > 0.0f ? static_cast<XrDuration>(seconds * 1e9f)
                              : XR_MIN_HAPTIC_DURATION;
  v.frequency = XR_FREQUENCY_UNSPECIFIED;
  XrHapticActionInfo info = {XR_TYPE_HAPTIC_ACTION_INFO};
  info.action = haptic_;
  info.subactionPath = hands_[hand ? 1 : 0];
  xrApplyHapticFeedback(session_, &info,
                        reinterpret_cast<const XrHapticBaseHeader*>(&v));
}

// The profile that the runtime chose for each hand, after
// /interaction_profiles/, as XrScreen of the PC gives it.
void XrApp::ReadProfiles() {
  profiles_read_ = true;
  static const char kPrefix[] = "/interaction_profiles/";
  for (int h = 0; h < 2; ++h) {
    profile_[h].clear();
    XrInteractionProfileState now = {XR_TYPE_INTERACTION_PROFILE_STATE};
    if (XR_SUCCEEDED(xrGetCurrentInteractionProfile(session_, hands_[h], &now)) &&
        now.interactionProfile != XR_NULL_PATH) {
      char full[XR_MAX_PATH_LENGTH] = {0};
      uint32_t length = 0;
      if (XR_SUCCEEDED(xrPathToString(instance_, now.interactionProfile,
                                      sizeof(full), &length, full))) {
        const size_t n = sizeof(kPrefix) - 1;
        profile_[h] = strncmp(full, kPrefix, n) == 0 ? full + n : full;
      }
    }
    AppLog("xr           : the %s hand has the profile \"%s\"",
           h == 0 ? "left" : "right", profile_[h].c_str());
  }
}

void XrApp::InputSlot(input_channel::SlotData* out) {
  namespace ic = input_channel;
  memset(out, 0, sizeof(*out));
  out->display_time = frame_.predictedDisplayTime;
  out->display_period = frame_.predictedDisplayPeriod;
  out->recenters = recenters_;
  out->recommended_width = eye_width_;
  out->recommended_height = eye_height_;
  out->gaze[2] = -1.0f;
  if (running_) out->flags |= ic::kRunning;
  if (synced_) out->flags |= ic::kFocused;
  for (int h = 0; h < 2; ++h) {
    out->hands[h].aim_orientation[3] = 1.0f;
    out->hands[h].grip_orientation[3] = 1.0f;
    out->hands[h].serial = ++serial_[h];
  }

  // THE VIEWS at the display time of the frame, in the LOCAL space.
  XrViewLocateInfo locate = {XR_TYPE_VIEW_LOCATE_INFO};
  locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  locate.displayTime = frame_.predictedDisplayTime;
  locate.space = local_;
  XrViewState view_state = {XR_TYPE_VIEW_STATE};
  XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
  uint32_t found = 0;
  if (frame_.predictedDisplayTime != 0 &&
      XR_SUCCEEDED(xrLocateViews(session_, &locate, &view_state, 2, &found,
                                 views)) &&
      found == 2) {
    const XrViewStateFlags need = XR_VIEW_STATE_POSITION_VALID_BIT |
                                  XR_VIEW_STATE_ORIENTATION_VALID_BIT;
    const bool valid = (view_state.viewStateFlags & need) == need;
    for (int e = 0; e < 2; ++e) {
      ic::Eye& eye = out->eyes[e];
      const XrPosef& p = views[e].pose;
      eye.position[0] = p.position.x;
      eye.position[1] = p.position.y;
      eye.position[2] = p.position.z;
      eye.orientation[0] = p.orientation.x;
      eye.orientation[1] = p.orientation.y;
      eye.orientation[2] = p.orientation.z;
      eye.orientation[3] = p.orientation.w;
      eye.fov[0] = views[e].fov.angleLeft;
      eye.fov[1] = views[e].fov.angleRight;
      eye.fov[2] = views[e].fov.angleUp;
      eye.fov[3] = views[e].fov.angleDown;
      eye.valid = valid ? 1u : 0u;
    }
  }

  // THE HANDS, as XrScreen::SyncInput of the PC reads them. With no input
  // focus each hand is not active and each button is up.
  if (!synced_ || !input_live_) return;
  if (!profiles_read_) ReadProfiles();
  for (int h = 0; h < 2; ++h) {
    ic::Hand& hand = out->hands[h];
    XrActionStateGetInfo get = {XR_TYPE_ACTION_STATE_GET_INFO};
    get.subactionPath = hands_[h];
    XrActionStatePose aim_state = {XR_TYPE_ACTION_STATE_POSE};
    XrActionStatePose grip_state = {XR_TYPE_ACTION_STATE_POSE};
    get.action = aim_;
    xrGetActionStatePose(session_, &get, &aim_state);
    get.action = grip_;
    xrGetActionStatePose(session_, &get, &grip_state);
    if (!aim_state.isActive && !grip_state.isActive) continue;
    hand.flags |= ic::kActive;
    const size_t n = profile_[h].size() < ic::kProfileChars - 1
                         ? profile_[h].size()
                         : ic::kProfileChars - 1;
    memcpy(hand.profile, profile_[h].data(), n);
    hand.profile[n] = 0;
    auto value = [&](XrAction action) {
      XrActionStateFloat v = {XR_TYPE_ACTION_STATE_FLOAT};
      get.action = action;
      if (XR_SUCCEEDED(xrGetActionStateFloat(session_, &get, &v)) && v.isActive) {
        return v.currentState < 0.0f ? 0.0f
                                     : (v.currentState > 1.0f ? 1.0f : v.currentState);
      }
      return 0.0f;
    };
    hand.trigger = value(trigger_);
    hand.squeeze = value(squeeze_);
    XrActionStateVector2f stick = {XR_TYPE_ACTION_STATE_VECTOR2F};
    get.action = stick_;
    if (XR_SUCCEEDED(xrGetActionStateVector2f(session_, &get, &stick)) &&
        stick.isActive) {
      hand.stick[0] = stick.currentState.x < -1.0f ? -1.0f
                      : (stick.currentState.x > 1.0f ? 1.0f : stick.currentState.x);
      hand.stick[1] = stick.currentState.y < -1.0f ? -1.0f
                      : (stick.currentState.y > 1.0f ? 1.0f : stick.currentState.y);
    }
    const XrAction buttons[4] = {stick_click_, primary_, secondary_, menu_button_};
    const uint32_t bits[4] = {ic::kStickClick, ic::kPrimary, ic::kSecondary, ic::kMenu};
    for (int i = 0; i < 4; ++i) {
      XrActionStateBoolean b = {XR_TYPE_ACTION_STATE_BOOLEAN};
      get.action = buttons[i];
      if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &get, &b)) &&
          b.isActive && b.currentState) {
        hand.flags |= bits[i];
      }
    }
    // Both poses at the display time of the eyes, so the hands and the head
    // share one moment.
    const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                      XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    const XrSpaceLocationFlags tracked = XR_SPACE_LOCATION_POSITION_TRACKED_BIT |
                                         XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    for (int which = 0; which < 2; ++which) {
      const bool active = which == 0 ? aim_state.isActive : grip_state.isActive;
      XrSpace space = which == 0 ? aim_space_[h] : grip_space_[h];
      XrSpaceLocation where = {XR_TYPE_SPACE_LOCATION};
      if (!active || space == XR_NULL_HANDLE ||
          XR_FAILED(xrLocateSpace(space, local_, frame_.predictedDisplayTime,
                                  &where))) {
        continue;
      }
      float* position = which == 0 ? hand.aim_position : hand.grip_position;
      float* orientation = which == 0 ? hand.aim_orientation : hand.grip_orientation;
      position[0] = where.pose.position.x;
      position[1] = where.pose.position.y;
      position[2] = where.pose.position.z;
      orientation[0] = where.pose.orientation.x;
      orientation[1] = where.pose.orientation.y;
      orientation[2] = where.pose.orientation.z;
      orientation[3] = where.pose.orientation.w;
      if ((where.locationFlags & need) == need) {
        hand.flags |= which == 0 ? ic::kAimValid : ic::kGripValid;
      }
      if ((where.locationFlags & tracked) == tracked) {
        hand.flags |= which == 0 ? ic::kAimTracked : ic::kGripTracked;
      }
    }
  }
}

void XrApp::Pulse(int hand) {
  if (haptic_ == XR_NULL_HANDLE || state_ != XR_SESSION_STATE_FOCUSED) return;
  XrHapticVibration v = {XR_TYPE_HAPTIC_VIBRATION};
  v.amplitude = 0.25f;
  v.duration = 12 * 1000 * 1000;  // 12 ms
  v.frequency = XR_FREQUENCY_UNSPECIFIED;
  XrHapticActionInfo info = {XR_TYPE_HAPTIC_ACTION_INFO};
  info.action = haptic_;
  info.subactionPath = hands_[hand];
  xrApplyHapticFeedback(session_, &info,
                        reinterpret_cast<const XrHapticBaseHeader*>(&v));
}

void XrApp::Stop() {
  SubmitFrame();
  if (not_ready_ > 0) {
    AppLog("xr           : frame_loop=1: %llu times a swapchain image was not free at once, "
           "and the layer showed its image before (frame_loop_image_wait=0)",
           static_cast<unsigned long long>(not_ready_));
  }
  if (staging_skips_ > 0) {
    AppLog("xr           : frame_loop=1: %llu copies of pixels of the CPU waited a frame or "
           "more, because the GPU did not finish the copy of their staging buffer before",
           static_cast<unsigned long long>(staging_skips_));
  }
  if (copies_ + copies_skipped_ > 0) {
    AppLog("xr           : %llu copies into the layers, %llu skipped because "
           "the picture did not change (xr_copy_on_change=%d)",
           static_cast<unsigned long long>(copies_),
           static_cast<unsigned long long>(copies_skipped_),
           copy_on_change_ ? 1 : 0);
  }
  if (screen_.copies[0] + screen_.copies[1] > 0) {
    AppLog("xr           : %llu copies of the stereo screen for eye 0 and %llu "
           "for eye 1",
           static_cast<unsigned long long>(screen_.copies[0]),
           static_cast<unsigned long long>(screen_.copies[1]));
  }
  if (projection_frames_ + screen_frames_ + window_frames_ > 0) {
    AppLog("xr           : %llu copies of the projection layer for eye 0 and %llu "
           "for eye 1. %llu frames with the projection layer, %llu frames with "
           "the stereo screen, %llu frames with the cut scene window "
           "(stream_projection=%d)",
           static_cast<unsigned long long>(eyes_.copies[0]),
           static_cast<unsigned long long>(eyes_.copies[1]),
           static_cast<unsigned long long>(projection_frames_),
           static_cast<unsigned long long>(screen_frames_),
           static_cast<unsigned long long>(window_frames_), projection_on_ ? 1 : 0);
  }
  if (keyboard_frames_ > 0) {
    AppLog("xr           : %llu frames with the keyboard",
           static_cast<unsigned long long>(keyboard_frames_));
  }
  {
    if (cards_[0].frames + cards_[1].frames + cards_[2].frames + cards_[3].frames +
            panel_frames_ + wall_frames_ + window_frames_ > 0) {
      AppLog("xr           : the quad layers of the mod (stream_layers=%d): %llu frames "
             "with %s, %llu with %s, %llu with %s, %llu with %s, %llu with the panel "
             "quads, %llu with the walls and %llu with the cut scene window as a screen",
             layers_on_ ? 1 : 0, static_cast<unsigned long long>(cards_[0].frames),
             quad_layer_math::CardName(0), static_cast<unsigned long long>(cards_[1].frames),
             quad_layer_math::CardName(1), static_cast<unsigned long long>(cards_[2].frames),
             quad_layer_math::CardName(2), static_cast<unsigned long long>(cards_[3].frames),
             quad_layer_math::CardName(3), static_cast<unsigned long long>(panel_frames_),
             static_cast<unsigned long long>(wall_frames_),
             static_cast<unsigned long long>(window_frames_));
      AppLog("xr           : the cards got new pixels %llu, %llu, %llu and %llu times, "
             "and the panel %llu copies",
             static_cast<unsigned long long>(cards_[0].layer.revision - 1),
             static_cast<unsigned long long>(cards_[1].layer.revision - 1),
             static_cast<unsigned long long>(cards_[2].layer.revision - 1),
             static_cast<unsigned long long>(cards_[3].layer.revision - 1),
             static_cast<unsigned long long>(panel_.copies[0]));
    }
  }
  if (flat_pictures_ + flat_frames_ > 0) {
    AppLog("xr           : the screen quad (stream_screen=%d): %llu screen pictures of the "
           "mod, %llu frames of the session with the screen quad, %llu copies of the "
           "picture of a walk, %llu pictures of the pixels of the CPU",
           screen_quad_on_ ? 1 : 0, static_cast<unsigned long long>(flat_pictures_),
           static_cast<unsigned long long>(flat_frames_),
           static_cast<unsigned long long>(flat_.copies[0]),
           static_cast<unsigned long long>(flat_pixels_.revision - 1));
  }
  if (once_made_ + once_full_ > 0) {
    AppLog("xr           : the copies recorded one time (xr_copy_once=%d): %llu secondary "
           "command buffers recorded, %llu copies ran one of them, %llu frees of the entries "
           "of a set, %llu copies recorded in their frame because a set had no free entry",
           copy_once_ ? 1 : 0, static_cast<unsigned long long>(once_made_),
           static_cast<unsigned long long>(once_run_),
           static_cast<unsigned long long>(once_frees_),
           static_cast<unsigned long long>(once_full_));
  }
  if (gpu_.device != VK_NULL_HANDLE) vkDeviceWaitIdle(gpu_.device);
  FreeLayers();
  FreeScreenSet(&flat_);
  FreeStaging(&gpu_, &flat_pixels_.staging);
  if (flat_pixels_.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(flat_pixels_.swapchain);
  flat_pixels_ = Layer();
  flat_source_ = VK_NULL_HANDLE;
  flat_on_ = false;
  FreeStaging(&gpu_, &menu_.staging);
  FreeStaging(&gpu_, &cursor_.staging);
  FreeStaging(&gpu_, &keyboard_.staging);
  if (keyboard_.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(keyboard_.swapchain);
  keyboard_.swapchain = XR_NULL_HANDLE;
  FreeScreenSet(&screen_);
  FreeScreenSet(&eyes_);
  screen_source_ = VK_NULL_HANDLE;
  if (menu_.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(menu_.swapchain);
  if (cursor_.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(cursor_.swapchain);
  menu_.swapchain = cursor_.swapchain = XR_NULL_HANDLE;
  for (XrSpace& s : aim_space_) {
    if (s != XR_NULL_HANDLE) xrDestroySpace(s);
    s = XR_NULL_HANDLE;
  }
  for (XrSpace& s : grip_space_) {
    if (s != XR_NULL_HANDLE) xrDestroySpace(s);
    s = XR_NULL_HANDLE;
  }
  if (local_ != XR_NULL_HANDLE) xrDestroySpace(local_);
  local_ = XR_NULL_HANDLE;
  if (session_ != XR_NULL_HANDLE) xrDestroySession(session_);
  session_ = XR_NULL_HANDLE;
  GpuRelease(&gpu_);
  if (renderer_used_) {
    // The renderer of the mod has its images and pipelines on the device and
    // has no stop of its own, so the device and the instance stay to the end
    // of the process. vk::Stop waits until the GPU is idle.
    vk::Stop();
    AppLog("xr           : the renderer of the mod used the device of the "
           "session, so the device stays to the end of the process");
  } else {
    if (gpu_.device != VK_NULL_HANDLE) vkDestroyDevice(gpu_.device, nullptr);
    if (gpu_.instance != VK_NULL_HANDLE) vkDestroyInstance(gpu_.instance, nullptr);
  }
  gpu_ = Gpu();
  if (instance_ != XR_NULL_HANDLE) xrDestroyInstance(instance_);
  instance_ = XR_NULL_HANDLE;
}
