// THE CHOICE OF THE DIRECTDRAW OF THE PROXY (ddraw_proxy.cpp, docs/backend.md,
// "Only Vulkan"). No Windows code, so the math test checks it
// (test/controller_math/proxy_choice_checks.h).
#pragma once

namespace proxy_choice {

enum class Path {
  kModVulkan,     // the DirectDraw of the mod (backend=vulkan, vk_ddraw.cpp)
  kNoDirectDraw,  // vulkan_strict=1 and a game that is not v1.42: none
  kSystem,        // the DirectDraw of the system
  kDll,           // a DLL that the key backend names, or dxwrapper
};

// True when the two texts are the same with no case (ASCII only).
inline bool SameText(const char* a, const char* b) {
  for (;; ++a, ++b) {
    char x = *a, y = *b;
    if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
    if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
    if (x != y) return false;
    if (x == 0) return true;
  }
}

// strict: vulkan_strict=1. wrong_game: runblack.exe is not the v1.42 file
// (ProxyForceSystemDDraw). backend: the text of the key backend, empty when
// the key is not there. dxwrapper_there: dxwrapper\ddraw.dll is beside the
// game. vulkan_fails: backend=vulkan cannot start in this process
// (vulkan_fallback: the file of the fall back is there, or there is no
// vulkan-1.dll).
inline Path Choose(bool strict, bool wrong_game, const char* backend,
                   bool dxwrapper_there, bool vulkan_fails) {
  // ONLY VULKAN: the DirectDraw of the mod, or none. The key backend does
  // not count.
  if (strict) return wrong_game ? Path::kNoDirectDraw : Path::kModVulkan;
  if (wrong_game) return Path::kSystem;
  // With no key the answer is dxwrapper when it is installed: that is what
  // the installer puts there. System DirectDraw cannot go far above 720p.
  if (backend == nullptr || backend[0] == 0) {
    return dxwrapper_there ? Path::kDll : Path::kSystem;
  }
  if (SameText(backend, "system")) return Path::kSystem;
  if (SameText(backend, "vulkan")) {
    // The fall back: dxwrapper as with no key.
    if (!vulkan_fails) return Path::kModVulkan;
    return dxwrapper_there ? Path::kDll : Path::kSystem;
  }
  return Path::kDll;
}

}  // namespace proxy_choice
