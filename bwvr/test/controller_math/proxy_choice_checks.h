// The choice of the DirectDraw of the proxy, a test
// (src/proxy/proxy_choice_math.h). main.cpp includes this file after Check
// and Near, just before main.
#pragma once

#include "../../src/proxy/proxy_choice_math.h"

namespace proxy_choice_checks {

inline void ProxyChoiceChecks() {
  using proxy_choice::Choose;
  using proxy_choice::Path;
  printf("\nThe choice of the DirectDraw of the proxy\n");

  // vulkan_strict=0: the ways of the key backend.
  Check(Choose(false, false, "vulkan", true, false) == Path::kModVulkan,
        "backend=vulkan is the DirectDraw of the mod", 0, 0);
  Check(Choose(false, false, "VULKAN", false, false) == Path::kModVulkan,
        "the name has no case", 0, 0);
  Check(Choose(false, false, "", true, false) == Path::kDll,
        "no key and dxwrapper there: dxwrapper", 0, 0);
  Check(Choose(false, false, "", false, false) == Path::kSystem,
        "no key and no dxwrapper: the DirectDraw of the system", 0, 0);
  Check(Choose(false, false, "system", true, false) == Path::kSystem,
        "backend=system is the DirectDraw of the system", 0, 0);
  Check(Choose(false, false, "dxwrapper\\ddraw.dll", true, false) == Path::kDll,
        "backend names a DLL", 0, 0);
  Check(Choose(false, true, "vulkan", true, false) == Path::kSystem,
        "a game that is not v1.42 gets the DirectDraw of the system", 0, 0);

  // vulkan_strict=1: the DirectDraw of the mod or none, whatever backend is.
  const char* const kBackends[] = {"", "vulkan", "system",
                                   "dxwrapper\\ddraw.dll", "d7vk\\ddraw.dll"};
  bool mod_each = true, none_each = true;
  for (const char* b : kBackends) {
    for (int dx = 0; dx < 2; ++dx) {
      if (Choose(true, false, b, dx != 0, false) != Path::kModVulkan) mod_each = false;
      if (Choose(true, true, b, dx != 0, false) != Path::kNoDirectDraw) none_each = false;
    }
  }
  Check(mod_each, "vulkan_strict=1: the DirectDraw of the mod for each backend",
        0, 0);
  Check(none_each,
        "vulkan_strict=1 and a game that is not v1.42: no DirectDraw at all",
        0, 0);
  Check(Choose(true, false, nullptr, false, false) == Path::kModVulkan,
        "vulkan_strict=1 with no text of backend", 0, 0);

  // vulkan_fallback: backend=vulkan that cannot start uses dxwrapper as with
  // no key, and vulkan_strict=1 has no fall back.
  Check(Choose(false, false, "vulkan", true, true) == Path::kDll,
        "the fall back of backend=vulkan is dxwrapper", 0, 0);
  Check(Choose(false, false, "vulkan", false, true) == Path::kSystem,
        "the fall back with no dxwrapper is the DirectDraw of the system", 0,
        0);
  Check(Choose(true, false, "vulkan", true, true) == Path::kModVulkan,
        "vulkan_strict=1: no fall back", 0, 0);
  Check(Choose(false, false, "dxwrapper\\ddraw.dll", true, true) ==
            Path::kDll,
        "the fall back does not change a key that names a DLL", 0, 0);
}

}  // namespace proxy_choice_checks
