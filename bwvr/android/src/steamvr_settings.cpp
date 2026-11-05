// The settings of SteamVR (steamvr_settings.h). The OpenVR interfaces are C++ objects with
// a table of functions, and a name with a version fixes that table. So the app needs no
// header of OpenVR, only the places in the two tables that it calls.
#include "steamvr_settings.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_log.h"

namespace steamvr_settings {
namespace {

constexpr char kKey[] = "preferredRefreshRate";

typedef void* (*Factory)(const char* name, int* error);
// IVRClientCore_003, place 3.
typedef void* (*GenericInterface)(void* self, const char* name, int* error);
constexpr int kPlaceGeneric = 3;
// IVRSettings_003, places 3 and 7.
typedef void (*SetFloat)(void* self, const char* section, const char* key, float value, int* error);
typedef float (*GetFloat)(void* self, const char* section, const char* key, int* error);
constexpr int kPlaceSetFloat = 3;
constexpr int kPlaceGetFloat = 7;

void* g_settings = nullptr;
std::string g_key;

void* Place(void* object, int place) {
  void** table = *static_cast<void***>(object);
  return table[place];
}

// The path of vrclient.so in this process, empty with none.
std::string FindClient() {
  FILE* maps = fopen("/proc/self/maps", "r");
  if (maps == nullptr) return std::string();
  char line[1024];
  std::string path;
  while (fgets(line, sizeof(line), maps) != nullptr) {
    if (strstr(line, "/vrclient.so") == nullptr) continue;
    const char* start = strchr(line, '/');
    if (start == nullptr) continue;
    path = start;
    while (!path.empty() && (path.back() == '\n' || path.back() == ' ')) path.pop_back();
    break;
  }
  fclose(maps);
  return path;
}

}  // namespace

bool Start(const std::string& ini_key) {
  g_settings = nullptr;
  const std::string path = FindClient();
  if (path.empty()) {
    AppLog("steamvr      : no vrclient.so in this process, so the runtime is not SteamVR");
    return false;
  }
  // The library of the runtime only: a second copy would have no connection.
  void* library = dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD);
  if (library == nullptr) {
    AppLog("steamvr      : %s does not open (%s)", path.c_str(), dlerror());
    return false;
  }
  const Factory factory = reinterpret_cast<Factory>(dlsym(library, "VRClientCoreFactory"));
  if (factory == nullptr) {
    AppLog("steamvr      : %s has no VRClientCoreFactory", path.c_str());
    return false;
  }
  int error = 0;
  void* core = factory("IVRClientCore_003", &error);
  if (core == nullptr) {
    AppLog("steamvr      : no IVRClientCore_003 (error %d)", error);
    return false;
  }
  const GenericInterface generic =
      reinterpret_cast<GenericInterface>(Place(core, kPlaceGeneric));
  error = 0;
  void* settings = generic(core, "IVRSettings_003", &error);
  if (settings == nullptr || error != 0) {
    AppLog("steamvr      : no IVRSettings_003 (error %d)", error);
    return false;
  }
  g_key = ini_key;
  const char* id = getenv("SteamAppId");
  if (g_key.empty() && id != nullptr && id[0] != 0) g_key = std::string("steam.app.") + id;
  if (g_key.empty()) {
    AppLog("steamvr      : the app has no key of SteamVR: no SteamAppId in the environment and "
           "no steamvr_app_key in bwvr.ini");
    return false;
  }
  g_settings = settings;
  AppLog("steamvr      : the settings of SteamVR answer. The key of the app is %s (%s, SteamAppId "
         "%s), and its preferredRefreshRate is %.2f Hz",
         g_key.c_str(), ini_key.empty() ? "from the environment" : "steamvr_app_key",
         id != nullptr ? id : "not set", ReadRate());
  return true;
}

bool On() { return g_settings != nullptr; }

const std::string& AppKey() { return g_key; }

float ReadRate() {
  if (g_settings == nullptr) return 0.0f;
  const GetFloat get = reinterpret_cast<GetFloat>(Place(g_settings, kPlaceGetFloat));
  int error = 0;
  const float value = get(g_settings, g_key.c_str(), kKey, &error);
  return error == 0 ? value : 0.0f;
}

bool WriteRate(float hz) {
  if (g_settings == nullptr) return false;
  const SetFloat set = reinterpret_cast<SetFloat>(Place(g_settings, kPlaceSetFloat));
  int error = 0;
  set(g_settings, g_key.c_str(), kKey, hz, &error);
  if (error != 0) AppLog("steamvr      : preferredRefreshRate was not set (error %d)", error);
  return error == 0;
}

}  // namespace steamvr_settings
