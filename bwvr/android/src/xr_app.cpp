#include "xr_app.h"

#include <string.h>

#include "app_log.h"

namespace {

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
  for (const XrExtensionProperties& e : extensions) {
    if (strcmp(e.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME) == 0) {
      vulkan2 = true;
    }
    if (strcmp(e.extensionName, XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME) == 0) {
      android = true;
    }
  }
  AppLog("xr           : the runtime has %u extensions. Vulkan 2: %s, Android: %s",
         count, vulkan2 ? "yes" : "no", android ? "yes" : "no");
  if (!vulkan2 || !android) return false;

  const char* names[] = {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME,
                         XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME};
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
  info.enabledExtensionCount = 2;
  info.enabledExtensionNames = names;
  if (!Ok(xrCreateInstance(&info, &instance_), "xrCreateInstance")) return false;

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
    eye_width_ = view[0].recommendedImageRectWidth;
    eye_height_ = view[0].recommendedImageRectHeight;
    AppLog("xr           : the headset asks for %u by %u for each eye",
           eye_width_, eye_height_);
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

  VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "Black & White VR Mod";
  app.pEngineName = "bwvr";
  app.apiVersion = VK_API_VERSION_1_1;
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
  const float priority = 1.0f;
  VkDeviceQueueCreateInfo queue = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  queue.queueFamilyIndex = gpu_.queue_family;
  queue.queueCount = 1;
  queue.pQueuePriorities = &priority;
  VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
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
  return GpuFinish(&gpu_);
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

  struct Bind {
    XrAction action;
    const char* path;
  };
  auto suggest = [&](const char* profile, const std::vector<Bind>& binds) {
    std::vector<XrActionSuggestedBinding> s;
    for (const Bind& b : binds) s.push_back({b.action, Path(instance_, b.path)});
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
           {haptic_, "/user/hand/right/output/haptic"}});
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
           {haptic_, "/user/hand/right/output/haptic"}});
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
           {haptic_, "/user/hand/right/output/haptic"}});

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
  }
  return true;
}

bool XrApp::MakeLayer(Layer* layer, uint32_t width, uint32_t height) {
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
  return MakeStaging(&gpu_, static_cast<VkDeviceSize>(width) * height * 4,
                     &layer->staging);
}

void XrApp::SetPixels(Layer* layer, const std::vector<uint8_t>& rgba) {
  const size_t bytes = static_cast<size_t>(layer->width) * layer->height * 4;
  if (rgba.size() < bytes || layer->staging.mapped == nullptr) return;
  // The GPU may still copy the buffer of the frame before.
  vkWaitForFences(gpu_.device, 1, &gpu_.fence, VK_TRUE, UINT64_MAX);
  uint8_t* out = static_cast<uint8_t*>(layer->staging.mapped);
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
  menu_written_ = true;
}

void XrApp::SetCursorPixels(const std::vector<uint8_t>& rgba) {
  SetPixels(&cursor_, rgba);
  cursor_written_ = true;
}

// The staging buffer into the next image of the swapchain. The runtime
// takes a released image in the layout of a color attachment.
void XrApp::CopyLayer(Layer* layer) {
  uint32_t index = 0;
  XrSwapchainImageAcquireInfo acquire = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  if (!Ok(xrAcquireSwapchainImage(layer->swapchain, &acquire, &index),
          "xrAcquireSwapchainImage")) {
    return;
  }
  XrSwapchainImageWaitInfo wait = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  wait.timeout = XR_INFINITE_DURATION;
  xrWaitSwapchainImage(layer->swapchain, &wait);
  const VkImage image = layer->images[index].image;
  VkCommandBuffer cmd = GpuBegin(&gpu_);
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
  GpuSubmit(&gpu_, VK_NULL_HANDLE, VK_NULL_HANDLE);
  XrSwapchainImageReleaseInfo release = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  xrReleaseSwapchainImage(layer->swapchain, &release);
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
    }
    event = {XR_TYPE_EVENT_DATA_BUFFER};
  }
}

bool XrApp::BeginFrame(XrInput* input) {
  XrFrameWaitInfo wait = {XR_TYPE_FRAME_WAIT_INFO};
  frame_ = {XR_TYPE_FRAME_STATE};
  if (!Ok(xrWaitFrame(session_, &wait, &frame_), "xrWaitFrame")) return false;
  XrFrameBeginInfo begin = {XR_TYPE_FRAME_BEGIN_INFO};
  if (!Ok(xrBeginFrame(session_, &begin), "xrBeginFrame")) return false;

  *input = XrInput();
  if (state_ != XR_SESSION_STATE_FOCUSED) return true;
  XrActiveActionSet active = {action_set_, XR_NULL_PATH};
  XrActionsSyncInfo sync = {XR_TYPE_ACTIONS_SYNC_INFO};
  sync.countActiveActionSets = 1;
  sync.activeActionSets = &active;
  if (XR_FAILED(xrSyncActions(session_, &sync))) return true;
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
  XrCompositionLayerQuad quads[2] = {{XR_TYPE_COMPOSITION_LAYER_QUAD},
                                     {XR_TYPE_COMPOSITION_LAYER_QUAD}};
  const XrCompositionLayerBaseHeader* layers[2];
  uint32_t count = 0;
  if (frame_.shouldRender && menu_written_) {
    CopyLayer(&menu_);
    XrCompositionLayerQuad& q = quads[count];
    q.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    q.space = local_;
    q.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    q.subImage.swapchain = menu_.swapchain;
    q.subImage.imageRect.extent = {static_cast<int32_t>(menu_.width),
                                   static_cast<int32_t>(menu_.height)};
    q.pose.orientation.w = 1.0f;
    q.pose.position = {menu_center[0], menu_center[1], menu_center[2]};
    q.size = {menu_size[0], menu_size[1]};
    layers[count] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&q);
    ++count;
    if (show_cursor && cursor_written_) {
      CopyLayer(&cursor_);
      XrCompositionLayerQuad& c = quads[count];
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
      layers[count] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&c);
      ++count;
    }
  }
  XrFrameEndInfo end = {XR_TYPE_FRAME_END_INFO};
  end.displayTime = frame_.predictedDisplayTime;
  end.environmentBlendMode = blend_;
  end.layerCount = count;
  end.layers = layers;
  Ok(xrEndFrame(session_, &end), "xrEndFrame");
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
  if (gpu_.device != VK_NULL_HANDLE) vkDeviceWaitIdle(gpu_.device);
  FreeStaging(&gpu_, &menu_.staging);
  FreeStaging(&gpu_, &cursor_.staging);
  if (menu_.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(menu_.swapchain);
  if (cursor_.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(cursor_.swapchain);
  menu_.swapchain = cursor_.swapchain = XR_NULL_HANDLE;
  for (XrSpace& s : aim_space_) {
    if (s != XR_NULL_HANDLE) xrDestroySpace(s);
    s = XR_NULL_HANDLE;
  }
  if (local_ != XR_NULL_HANDLE) xrDestroySpace(local_);
  local_ = XR_NULL_HANDLE;
  if (session_ != XR_NULL_HANDLE) xrDestroySession(session_);
  session_ = XR_NULL_HANDLE;
  GpuRelease(&gpu_);
  if (gpu_.device != VK_NULL_HANDLE) vkDestroyDevice(gpu_.device, nullptr);
  if (gpu_.instance != VK_NULL_HANDLE) vkDestroyInstance(gpu_.instance, nullptr);
  gpu_ = Gpu();
  if (instance_ != XR_NULL_HANDLE) xrDestroyInstance(instance_);
  instance_ = XR_NULL_HANDLE;
}
