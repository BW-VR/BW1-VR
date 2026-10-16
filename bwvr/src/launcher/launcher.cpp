// The launcher for Black & White VR.
//
// It edits two things, because the settings live in two places and neither is
// obvious: the mod's own options in bwvr.ini beside the game, and the render
// resolution in the registry, where the game keeps it. Then it starts the
// game.
//
// WHY IT IS BUILT FROM A TABLE. The project keeps adding settings. A hand
// laid out dialog would mean an edit in three places for each new one: the
// layout, the reader and the writer.
// Here a setting is one row in kSettings and nothing else. The window sizes
// itself around whatever the table holds, and starts a new column when one gets
// too tall, so the table can grow without the dialog becoming unusable.
//
// It has no dependencies beyond Windows itself, so it is one file to ship and
// nothing for anyone to install.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "../common/exe_check.h"
#include "../common/settings_table.h"
#include "../common/version.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdi32.lib")

// The controls of Windows with its visual styles (version 6 of the common
// controls). Without this line Windows gives the launcher version 5, which
// draws in the old style and cannot show a picture beside the text of a
// button.
#pragma comment(linker, "\"/manifestdependency:type='win32' " \
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")

namespace {

using namespace bwvr_settings;

const char* kWindowClass = "BwvrLauncher";
const char* kProductName = BWVR_PRODUCT_NAME;
const char* kTitle = BWVR_PRODUCT_NAME " " BWVR_VERSION_TEXT;
const char* kRegistryPath =
    "SOFTWARE\\Lionhead Studios Ltd\\Black & White\\BWSetup";

// One control per setting, in the same order. The labels are kept too,
// because a slider has no text of its own and puts its value in its label.
HWND g_control[kSettingCount];
HWND g_label[kSettingCount];
HWND g_heading[kSettingCount];
// The buttons below the table.
enum {
  kButtonPlay,
  kButtonFresh,
  kButtonSave,
  kButtonPlain,
  kButtonSupport,
  kButtonCount
};
HWND g_buttons[kButtonCount];
HWND g_window = nullptr;
HWND g_status = nullptr;
// The row "Renderer" shows Vulkan (UpdateRendererRows).
bool g_renderer_vulkan = false;
HWND g_tips = nullptr;
HFONT g_font = nullptr;
HFONT g_bold = nullptr;
char g_game_dir[MAX_PATH];
char g_ini_path[MAX_PATH];
int g_dpi = 96;
// The most rows of a column. A test sets a smaller number with "-rows" to get
// a picture of a table that has more columns.
int g_max_rows = 22;

int Scaled(int at96) { return MulDiv(at96, g_dpi, 96); }

// --- where the game is -----------------------------------------------------

void TakeGameDir(const char* dir) {
  strcpy_s(g_game_dir, sizeof(g_game_dir), dir);
  _snprintf_s(g_ini_path, sizeof(g_ini_path), _TRUNCATE, "%s\\bwvr.ini", dir);
}

bool FileThere(const char* dir, const char* name) {
  char path[MAX_PATH];
  _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", dir, name);
  return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

// Where the game's own installer recorded the folder. The installer of the
// mod uses the same value, so this only matters when someone moves the
// launcher by hand.
bool GameDirFromRegistry(char* out, size_t size) {
  HKEY key;
  if (RegOpenKeyExA(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_READ, &key) !=
      ERROR_SUCCESS) {
    return false;
  }
  DWORD type = 0;
  DWORD bytes = static_cast<DWORD>(size);
  const LONG got = RegQueryValueExA(key, "GameDir", nullptr, &type,
                                    reinterpret_cast<BYTE*>(out), &bytes);
  RegCloseKey(key);
  if (got != ERROR_SUCCESS || type != REG_SZ || bytes == 0) return false;
  out[size - 1] = 0;
  return true;
}

// The launcher sits beside the game after an install. The folders above it
// are where it ends up when it is built from the repository.
bool FindGame() {
  char exe[MAX_PATH];
  GetModuleFileNameA(nullptr, exe, MAX_PATH);
  char* slash = strrchr(exe, '\\');
  if (slash != nullptr) *slash = 0;

  const char* candidates[] = {
      "", "\\Black & White Game", "\\..\\Black & White Game",
      "\\..\\..\\Black & White Game", "\\..\\..\\..\\Black & White Game",
  };
  for (int i = 0; i < 5; ++i) {
    char probe[MAX_PATH];
    _snprintf_s(probe, sizeof(probe), _TRUNCATE, "%s%s", exe, candidates[i]);
    char full[MAX_PATH];
    if (GetFullPathNameA(probe, MAX_PATH, full, nullptr) == 0) continue;
    if (FileThere(full, "runblack.exe")) {
      TakeGameDir(full);
      return true;
    }
  }

  char recorded[MAX_PATH] = {0};
  if (GameDirFromRegistry(recorded, sizeof(recorded)) &&
      FileThere(recorded, "runblack.exe")) {
    TakeGameDir(recorded);
    return true;
  }
  return false;
}

// --- the registry, where the game keeps its resolution ---------------------

int ReadRegistryInt(const char* name, int fallback) {
  HKEY key;
  if (RegOpenKeyExA(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_READ, &key) !=
      ERROR_SUCCESS) {
    return fallback;
  }
  DWORD value = 0;
  DWORD size = sizeof(value);
  DWORD type = 0;
  const LONG got =
      RegQueryValueExA(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&value),
                       &size);
  RegCloseKey(key);
  if (got != ERROR_SUCCESS || type != REG_DWORD) return fallback;
  return static_cast<int>(value);
}

void WriteRegistryInt(const char* name, int value) {
  HKEY key;
  if (RegCreateKeyExA(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0,
                      KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
    return;
  }
  const DWORD v = static_cast<DWORD>(value);
  RegSetValueExA(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v),
                 sizeof(v));
  RegCloseKey(key);
}


// --- the ini ---------------------------------------------------------------

int ReadIni(const char* key, int fallback) {
  return GetPrivateProfileIntA("bwvr", key, fallback, g_ini_path);
}

void ReadIniText(const char* key, char* out, DWORD size) {
  GetPrivateProfileStringA("bwvr", key, "", out, size, g_ini_path);
}

// True when Windows refused a write to bwvr.ini. Under Program Files an
// unelevated write fails, and the launcher must not say "Saved." then.
bool g_write_failed = false;

void WriteIniText(const char* key, const char* text) {
  if (!WritePrivateProfileStringA("bwvr", key, text, g_ini_path)) {
    g_write_failed = true;
  }
}

void WriteIni(const char* key, int value) {
  char text[32];
  _snprintf_s(text, sizeof(text), _TRUNCATE, "%d", value);
  WriteIniText(key, text);
}

// WHAT THE HEADSET ASKS FOR, per eye.
//
// The game writes this into bwvr.ini every time it starts with a headset
// connected, because only a running OpenXR session can ask. Until then these
// are a Quest 3's figures, which are a reasonable guess and are replaced by
// the truth on the first run.
int HeadsetWidth() { return ReadIni("headset_width", 2064); }
int HeadsetHeight() { return ReadIni("headset_height", 2208); }

// The size of the render for `percent` (settings_table.h, ScaleToPixels),
// with the values of bwvr.ini.
void ScaleToPixels(int percent, int* width, int* height) {
  bwvr_settings::ScaleToPixels(percent, HeadsetWidth(), HeadsetHeight(),
                               ReadIni("fov_margin_pct", 0),
                               ReadIni("one_walk", 1) != 0,
                               ReadIni("one_walk_guard_pct", 25), width,
                               height);
}

void SayScale(int index, int percent) {
  if (g_label[index] == nullptr) return;
  int w = 0, h = 0;
  ScaleToPixels(percent, &w, &h);
  char text[128];
  _snprintf_s(text, sizeof(text), _TRUNCATE, "%s: %d%% (%d x %d)",
              kSettings[index].label, percent, w, h);
  SetWindowTextA(g_label[index], text);
}

// THE SLIDERS WITH A VALUE OF THEIR OWN (kSlider). The spec is
// "low|high|step|unit". The value is on the step.
int SliderValue(int index) {
  return SliderSnap(kSettings[index].choices,
                    static_cast<int>(
                        SendMessageA(g_control[index], TBM_GETPOS, 0, 0)));
}

void SaySlider(int index, int value) {
  if (g_label[index] == nullptr) return;
  char text[128];
  _snprintf_s(text, sizeof(text), _TRUNCATE, "%s: %d%s",
              kSettings[index].label, value,
              SliderUnit(kSettings[index].choices));
  SetWindowTextA(g_label[index], text);
}

// THE RENDERER VULKAN needs a GPU with Vulkan 1.3. The launcher asks the
// Vulkan of Windows, with no device: an instance, the list of the GPUs and
// their versions.
bool g_said_no_vulkan = false;

bool HasVulkan13() {
  HMODULE library = LoadLibraryA("vulkan-1.dll");
  if (library == nullptr) return false;
  bool found = false;
  PFN_vkGetInstanceProcAddr get_proc =
      reinterpret_cast<PFN_vkGetInstanceProcAddr>(
          GetProcAddress(library, "vkGetInstanceProcAddr"));
  PFN_vkCreateInstance create =
      get_proc != nullptr
          ? reinterpret_cast<PFN_vkCreateInstance>(
                get_proc(nullptr, "vkCreateInstance"))
          : nullptr;
  VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "Black & White VR Mod launcher";
  app.apiVersion = VK_API_VERSION_1_3;
  VkInstanceCreateInfo info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  info.pApplicationInfo = &app;
  VkInstance instance = VK_NULL_HANDLE;
  if (create != nullptr &&
      create(&info, nullptr, &instance) == VK_SUCCESS) {
    PFN_vkEnumeratePhysicalDevices enumerate =
        reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
            get_proc(instance, "vkEnumeratePhysicalDevices"));
    PFN_vkGetPhysicalDeviceProperties properties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
            get_proc(instance, "vkGetPhysicalDeviceProperties"));
    PFN_vkDestroyInstance destroy = reinterpret_cast<PFN_vkDestroyInstance>(
        get_proc(instance, "vkDestroyInstance"));
    if (enumerate != nullptr && properties != nullptr) {
      VkPhysicalDevice devices[16];
      uint32_t count = 16;
      if (enumerate(instance, &count, devices) >= 0) {
        for (uint32_t i = 0; i < count && i < 16; ++i) {
          VkPhysicalDeviceProperties p;
          properties(devices[i], &p);
          if (p.apiVersion >= VK_API_VERSION_1_3) found = true;
        }
      }
    }
    if (destroy != nullptr) destroy(instance, nullptr);
  }
  FreeLibrary(library);
  return found;
}

// --- pre-flight, the same checks the script did ----------------------------

void SayStatus(const char* text) { SetWindowTextA(g_status, text); }

// The build of the game. The mod's addresses fit one file only, so a player
// with another build must be told, rather than shown a crash.
bool GameBuildIsRight(char* message, size_t size) {
  char exe[MAX_PATH];
  _snprintf_s(exe, sizeof(exe), _TRUNCATE, "%s\\runblack.exe", g_game_dir);
  wchar_t wide[MAX_PATH];
  if (MultiByteToWideChar(CP_ACP, 0, exe, -1, wide, MAX_PATH) == 0) return true;

  const bwvr::ExeReport report = bwvr::CheckGameExe(wide);
  if (report.verdict == bwvr::ExeVerdict::kSupported) return true;
  if (report.verdict == bwvr::ExeVerdict::kUnreadable) {
    _snprintf_s(message, size, _TRUNCATE,
                "Cannot read runblack.exe (error %lu).", report.error);
    return false;
  }
  _snprintf_s(message, size, _TRUNCATE,
              "This Black & White is %s. The mod needs the unofficial v1.42 "
              "patch.",
              report.build != nullptr ? report.build : "a build the mod does "
                                                       "not know");
  return false;
}

void CheckEverything() {
  char message[512];
  message[0] = 0;

  // Every file the mod needs at run time. A missing dxwrapper file makes the
  // mod fall back to a slow path in silence, so this check lists them.
  static const char* const kNeeded[] = {
      "ddraw.dll",
      "openxr_loader.dll",
      "dxwrapper\\ddraw.dll",
      "dxwrapper\\dxwrapper.dll",
      "dxwrapper\\dxwrapper.ini",
  };
  const char* missing = nullptr;
  for (const char* name : kNeeded) {
    if (!FileThere(g_game_dir, name)) {
      missing = name;
      break;
    }
  }
  if (missing != nullptr) {
    _snprintf_s(message, sizeof(message), _TRUNCATE,
                "%s is missing from the game folder. Install the %s again.",
                missing, kProductName);
    SayStatus(message);
    return;
  }

  if (!GameBuildIsRight(message, sizeof(message))) {
    SayStatus(message);
    return;
  }

  // The game is 32 bit, so a runtime registered only for 64 bit cannot serve
  // it. This is the single most common reason for a headset showing nothing.
  HKEY key;
  bool runtime = false;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "SOFTWARE\\WOW6432Node\\Khronos\\OpenXR\\1", 0, KEY_READ,
                    &key) == ERROR_SUCCESS) {
    char path[MAX_PATH] = {0};
    DWORD size = sizeof(path);
    DWORD type = 0;
    if (RegQueryValueExA(key, "ActiveRuntime", nullptr, &type,
                         reinterpret_cast<BYTE*>(path), &size) ==
        ERROR_SUCCESS) {
      const char* name = strrchr(path, '\\');
      _snprintf_s(message, sizeof(message), _TRUNCATE, "Ready. OpenXR runtime: %s",
                  name != nullptr ? name + 1 : path);
      runtime = true;
    }
    RegCloseKey(key);
  }
  if (!runtime) {
    strcpy_s(message, sizeof(message),
             "No 32 bit OpenXR runtime is registered. The game is 32 bit, so a "
             "64 bit only runtime cannot serve it.");
  }
  SayStatus(message);
}

// --- reading and writing the whole table -----------------------------------

void LayOutControls(bool first);

// The rows that only the renderer Direct3D 9 uses are not in the table with
// the renderer Vulkan (RowShown). `first`: the window goes to the middle of
// the work area.
void UpdateRendererRows(bool first) {
  bool vulkan = false;
  for (int i = 0; i < kSettingCount; ++i) {
    if (kSettings[i].kind == kRenderer && g_control[i] != nullptr) {
      vulkan = SendMessageA(g_control[i], CB_GETCURSEL, 0, 0) == 0;
    }
  }
  if (vulkan == g_renderer_vulkan && !first) return;
  g_renderer_vulkan = vulkan;
  LayOutControls(first);
}

void LoadAll() {
  for (int i = 0; i < kSettingCount; ++i) {
    const Setting& s = kSettings[i];
    if (g_control[i] == nullptr) continue;
    switch (s.kind) {
      case kBool:
        SendMessageA(g_control[i], BM_SETCHECK,
                     ReadIni(s.key, s.fallback) != 0 ? BST_CHECKED
                                                     : BST_UNCHECKED,
                     0);
        break;
      case kInt: {
        char text[32];
        _snprintf_s(text, sizeof(text), _TRUNCATE, "%d",
                    ReadIni(s.key, s.fallback));
        SetWindowTextA(g_control[i], text);
        break;
      }
      case kChoice:
        SendMessageA(g_control[i], CB_SETCURSEL, ReadIni(s.key, s.fallback), 0);
        break;
      case kValues: {
        int value = ReadIni(s.key, s.fallback);
        // The God view replaced the immersive mode, play mode 0. An older
        // bwvr.ini can still hold 0, and it shows the God view.
        if (strcmp(s.key, "play_mode") == 0 && value == 0) value = 4;
        SendMessageA(g_control[i], CB_SETCURSEL, ChoiceIndex(s.choices, value),
                     0);
        break;
      }
      case kSlider: {
        int low = 0, high = 0, step = 0;
        ScaleRange(s.choices, &low, &high, &step);
        int value = ReadIni(s.key, s.fallback);
        if (value < low) value = low;
        if (value > high) value = high;
        SendMessageA(g_control[i], TBM_SETPOS, TRUE, value);
        SaySlider(i, value);
        break;
      }
      case kRenderer: {
        char text[MAX_PATH] = {0};
        ReadIniText(s.key, text, sizeof(text));
        SendMessageA(g_control[i], CB_SETCURSEL,
                     _stricmp(text, "vulkan") == 0 ? 0 : 1, 0);
        break;
      }
      case kScale: {
        // The percentage is kept in the ini and the registry holds whatever it
        // came out as. Reading the percentage back is what makes the slider
        // land where the player left it, whatever the headset now asks for.
        int low = 0, high = 0, step = 0;
        ScaleRange(s.choices, &low, &high, &step);
        int percent = ReadIni(s.key, s.fallback);
        if (percent < low) percent = low;
        if (percent > high) percent = high;
        SendMessageA(g_control[i], TBM_SETPOS, TRUE, percent);
        SayScale(i, percent);
        break;
      }
      default:
        break;
    }
  }
}

// dxwrapper sets its own anisotropic filtering on the Direct3D 9 device, from
// dxwrapper.ini. Left at 16, it would overrule "Off" in this launcher. So the
// launcher writes the same level there. dxwrapper reads 0 as "leave it to the
// game".
void WriteBackendAnisotropy(int level) {
  char path[MAX_PATH];
  _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\dxwrapper\\dxwrapper.ini",
              g_game_dir);
  if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return;
  char text[32];
  _snprintf_s(text, sizeof(text), _TRUNCATE, "%d", level > 1 ? level : 0);
  WritePrivateProfileStringA("d3d9", "AnisotropicFiltering", text, path);
}

// The game's own display settings, as they were before the mod first
// changed them. The mod runs the game in a window at a square size, which
// the plain game would then open too. So the uninstaller puts these back.
// They are written once, in their own section, which the retired key sweep
// never touches. -1 means the game had no value of its own.
void SaveGameDisplayOnce() {
  if (GetPrivateProfileIntA("game_before", "saved", 0, g_ini_path) != 0) return;
  static const char* const kNames[] = {"ScreenW", "ScreenH", "FullScreen",
                                       "ScreenD"};
  for (const char* name : kNames) {
    char text[32];
    _snprintf_s(text, sizeof(text), _TRUNCATE, "%d",
                ReadRegistryInt(name, -1));
    WritePrivateProfileStringA("game_before", name, text, g_ini_path);
  }
  WritePrivateProfileStringA("game_before", "saved", "1", g_ini_path);
}

void SaveAll() {
  for (int i = 0; i < kSettingCount; ++i) {
    const Setting& s = kSettings[i];
    if (g_control[i] == nullptr) continue;
    switch (s.kind) {
      case kBool:
        WriteIni(s.key, SendMessageA(g_control[i], BM_GETCHECK, 0, 0) ==
                                BST_CHECKED
                            ? 1
                            : 0);
        break;
      case kInt: {
        char text[32] = {0};
        GetWindowTextA(g_control[i], text, sizeof(text));
        WriteIni(s.key, atoi(text));
        break;
      }
      case kChoice:
        WriteIni(s.key,
                 static_cast<int>(SendMessageA(g_control[i], CB_GETCURSEL, 0, 0)));
        break;
      case kValues:
        WriteIni(s.key,
                 ChoiceValue(s.choices, static_cast<int>(SendMessageA(
                                            g_control[i], CB_GETCURSEL, 0, 0))));
        break;
      case kScale: {
        const int percent =
            static_cast<int>(SendMessageA(g_control[i], TBM_GETPOS, 0, 0));
        WriteIni(s.key, percent);
        // The game reads its own resolution from the registry, not from the
        // ini, so the percentage has to be turned into pixels here.
        int w = 0, h = 0;
        ScaleToPixels(percent, &w, &h);
        WriteRegistryInt("ScreenW", w);
        WriteRegistryInt("ScreenH", h);
        break;
      }
      case kSlider:
        WriteIni(s.key, SliderValue(i));
        break;
      case kRenderer: {
        // Vulkan only with a GPU that has Vulkan 1.3: else the game would
        // have no picture.
        bool vulkan =
            SendMessageA(g_control[i], CB_GETCURSEL, 0, 0) == 0;
        if (vulkan && !HasVulkan13()) {
          vulkan = false;
          SendMessageA(g_control[i], CB_SETCURSEL, 1, 0);
          g_said_no_vulkan = true;
        }
        WriteIniText(s.key, vulkan ? "vulkan" : "dxwrapper\\ddraw.dll");
        break;
      }
      default:
        break;
    }
  }
  // A PC with no Vulkan 1.3 has the renderer Direct3D 9 now, and its rows.
  UpdateRendererRows(false);
  // These are not offered as choices, because getting them wrong stops the mod
  // working at all and there is only one right answer. The renderer Vulkan
  // has no bridge of Direct3D 9.
  if (g_renderer_vulkan) {
    WritePrivateProfileStringA("bwvr", "gpu_bridge", nullptr, g_ini_path);
  } else {
    WriteIni("gpu_bridge", 1);
  }
  WriteIni("vr", 1);
  WriteIni("stereo", 1);
  WriteIni("head_camera", 1);
  // Settings that have one right answer and are not offered. Removing them
  // means an old ini cannot keep a wrong value. The mod's own default is the
  // right answer for each, and the keys still work for testing.
  for (const char* key : kRetiredKeys) {
    WritePrivateProfileStringA("bwvr", key, nullptr, g_ini_path);
  }
  // The renderer Vulkan takes the anisotropy of the game, so dxwrapper.ini
  // matters only with Direct3D 9.
  if (!g_renderer_vulkan) WriteBackendAnisotropy(ReadIni("anisotropy", 16));
  SaveGameDisplayOnce();
  // The game reads these from the registry. If they are missing, it starts
  // full screen at the size of the desktop, in 16 bit color. Another install
  // of the game can remove them. The mod needs a window at the size above.
  WriteRegistryInt("FullScreen", 0);
  WriteRegistryInt("ScreenD", 32);
}

// Starts runblack.exe. With `process`, the caller gets the handle of the game
// and closes it.
bool StartGame(const char* parameters, HANDLE* process) {
  char exe[MAX_PATH];
  _snprintf_s(exe, sizeof(exe), _TRUNCATE, "%s\\runblack.exe", g_game_dir);
  SHELLEXECUTEINFOA info = {sizeof(info)};
  info.lpFile = exe;
  info.lpDirectory = g_game_dir;
  info.lpParameters = parameters;
  info.nShow = SW_SHOWNORMAL;
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  if (!ShellExecuteExA(&info)) return false;
  if (process != nullptr) {
    *process = info.hProcess;
  } else if (info.hProcess != nullptr) {
    CloseHandle(info.hProcess);
  }
  return true;
}

void Play(bool new_game) {
  SaveAll();
  if (StartGame(new_game ? "-NEWGAME" : nullptr, nullptr)) {
    PostMessageA(g_window, WM_CLOSE, 0, 0);
  } else {
    SayStatus("Could not start runblack.exe.");
  }
}

// THE GAME WITHOUT VR.
//
// "Play, no VR" starts the game with vr=0, so the mod does nothing, full
// screen at the size of the main monitor. These values are for one game only:
// a start of runblack.exe without the launcher must find the values for the
// headset. So the launcher stays, with no window, until the game ends, and
// then it writes the values for the headset again (WinMain). It writes them
// after the end, because the game can write its display values when it ends.
HANDLE g_plain_game = nullptr;
int g_headset_screen_w = 0;
int g_headset_screen_h = 0;

// The renderer of the headset: the plain game has dxwrapper.
char g_headset_backend[MAX_PATH] = {0};

void WriteHeadsetStart() {
  WriteIni("vr", 1);
  if (g_headset_backend[0] != 0) WriteIniText("backend", g_headset_backend);
  WriteRegistryInt("ScreenW", g_headset_screen_w);
  WriteRegistryInt("ScreenH", g_headset_screen_h);
  WriteRegistryInt("FullScreen", 0);
}

void PlayNoVr() {
  SaveAll();
  ReadIniText("backend", g_headset_backend, sizeof(g_headset_backend));
  WriteIniText("backend", "dxwrapper\\ddraw.dll");
  g_headset_screen_w = ReadRegistryInt("ScreenW", 0);
  g_headset_screen_h = ReadRegistryInt("ScreenH", 0);
  WriteIni("vr", 0);
  WriteRegistryInt("ScreenW", GetSystemMetrics(SM_CXSCREEN));
  WriteRegistryInt("ScreenH", GetSystemMetrics(SM_CYSCREEN));
  WriteRegistryInt("FullScreen", 1);
  if (StartGame(nullptr, &g_plain_game)) {
    PostMessageA(g_window, WM_CLOSE, 0, 0);
  } else {
    WriteHeadsetStart();
    SayStatus("Could not start runblack.exe.");
  }
}

// --- building the window ---------------------------------------------------

void AddTip(HWND control, const char* text) {
  if (g_tips == nullptr || text == nullptr) return;
  TOOLINFOA tip = {0};
  tip.cbSize = sizeof(tip);
  tip.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  tip.hwnd = g_window;
  tip.uId = reinterpret_cast<UINT_PTR>(control);
  tip.lpszText = const_cast<char*>(text);
  SendMessageA(g_tips, TTM_ADDTOOLA, 0, reinterpret_cast<LPARAM>(&tip));
}

void FillChoices(HWND combo, const char* choices) {
  char buffer[512];
  strcpy_s(buffer, sizeof(buffer), choices);
  char* start = buffer;
  for (char* p = buffer;; ++p) {
    if (*p == '|' || *p == 0) {
      const char end = *p;
      *p = 0;
      // A kValues item is "label=value". Only the label is shown.
      char* value = strchr(start, '=');
      if (value != nullptr) *value = 0;
      SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(start));
      start = p + 1;
      if (end == 0) break;
    }
  }
}

// How many rows a group takes: its heading and every setting under it.
// The rows that the renderer Vulkan does not use. With Vulkan they are not
// in the table.
bool RowShown(int index) {
  const char* key = kSettings[index].key;
  return !g_renderer_vulkan || !OnlyDirect3D9(key);
}

int GroupRows(int group) {
  int rows = 1;
  for (int i = group + 1; i < kSettingCount; ++i) {
    if (kSettings[i].kind == kGroup) break;
    if (RowShown(i)) ++rows;
  }
  return rows;
}

// The heart on the "Support" button: a red heart with soft edges, as an icon
// of `size` pixels. The form is the curve (x^2 + y^2 - 1)^3 = x^2 y^3, and
// each pixel gets its alpha from 16 samples.
HICON MakeHeartIcon(int size) {
  if (size < 8) size = 8;
  if (size > 64) size = 64;
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = size;
  info.bmiHeader.biHeight = -size;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP color =
      CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (color == nullptr) return nullptr;
  DWORD* pixel = static_cast<DWORD*>(bits);
  const int sub = 4;
  for (int py = 0; py < size; ++py) {
    for (int px = 0; px < size; ++px) {
      int inside = 0;
      for (int sy = 0; sy < sub; ++sy) {
        for (int sx = 0; sx < sub; ++sx) {
          // The curve is 2.3 wide and goes from -1.0 to 1.24 in y, so the
          // picture is 2.5 across and its middle is at y = 0.12.
          const double x = ((px + (sx + 0.5) / sub) / size - 0.5) * 2.5;
          const double y = (0.5 - (py + (sy + 0.5) / sub) / size) * 2.5 + 0.12;
          const double a = x * x + y * y - 1.0;
          if (a * a * a - x * x * y * y * y <= 0.0) ++inside;
        }
      }
      const DWORD alpha = static_cast<DWORD>(inside * 255 / (sub * sub));
      *pixel++ = (alpha << 24) | (224u << 16) | (49u << 8) | 72u;
    }
  }
  // An icon needs a mask. All of its bits are 0, so the alpha decides.
  static const unsigned char kNoMask[8 * 64] = {0};
  HBITMAP mask = CreateBitmap(size, size, 1, 1, kNoMask);
  ICONINFO parts = {TRUE, 0, 0, mask, color};
  HICON icon = CreateIconIndirect(&parts);
  DeleteObject(mask);
  DeleteObject(color);
  return icon;
}

// The sizes of the table.
struct TableSizes {
  int pad, row, label_w, field_w, col_w;
};

TableSizes Sizes() {
  TableSizes t;
  t.pad = Scaled(12);
  t.row = Scaled(26);
  t.label_w = Scaled(210);
  // Wide enough for the longest choice, "Grip of the other hand".
  t.field_w = Scaled(160);
  t.col_w = t.label_w + t.field_w + t.pad;
  return t;
}

// Puts each shown row, the buttons and the status line in place, and sizes
// the window to them. The table goes in columns, and a new column starts when
// the current gets tall. This is what lets the table grow without the window
// growing past the screen. The first call puts the window in the middle of
// the work area, and a later call keeps its place.
void LayOutControls(bool first) {
  const TableSizes t = Sizes();
  const int pad = t.pad;
  const int row = t.row;
  const int label_w = t.label_w;
  const int field_w = t.field_w;
  const int col_w = t.col_w;
  // The tallest a column may get. Rows and pixels are interchangeable
  // everywhere below, so this number also caps the height of the table.
  const int max_rows = g_max_rows;
  const UINT move = SWP_NOZORDER | SWP_NOACTIVATE;

  int column = 0;
  int used = 0;
  int tallest = 0;

  for (int i = 0; i < kSettingCount; ++i) {
    const Setting& s = kSettings[i];
    if (!RowShown(i)) {
      if (g_label[i] != nullptr) ShowWindow(g_label[i], SW_HIDE);
      if (g_control[i] != nullptr) ShowWindow(g_control[i], SW_HIDE);
      continue;
    }
    // Break between groups, so a heading always keeps all of its settings. A
    // group taller than a column is the one case that still breaks inside it.
    if (s.kind == kGroup) {
      if (used > 0 && used + GroupRows(i) > max_rows) {
        ++column;
        used = 0;
      }
    } else if (used >= max_rows) {
      ++column;
      used = 0;
    }
    const int x = pad + column * (col_w + pad);
    const int y = pad + used * row;
    used += 1;
    if (used > tallest) tallest = used;

    if (s.kind == kGroup) {
      SetWindowPos(g_heading[i], nullptr, x, y + Scaled(6), col_w,
                   row - Scaled(4), move);
      continue;
    }
    if (g_label[i] != nullptr) {
      // A slider says its value in its own label, so it needs the whole width
      // rather than the label column.
      const int width =
          s.kind == kScale || s.kind == kSlider ? col_w : label_w;
      SetWindowPos(g_label[i], nullptr, x, y + Scaled(4), width,
                   row - Scaled(6), move);
      ShowWindow(g_label[i], SW_SHOW);
    }
    HWND control = g_control[i];
    if (control == nullptr) continue;
    switch (s.kind) {
      case kBool:
        SetWindowPos(control, nullptr, x, y + Scaled(2), col_w,
                     row - Scaled(4), move);
        break;
      case kInt:
      case kScale:
      case kSlider:
        SetWindowPos(control, nullptr, x + label_w, y, field_w,
                     row - Scaled(4), move);
        break;
      case kChoice:
      case kValues:
      case kRenderer:
        SetWindowPos(control, nullptr, x + label_w, y, field_w, row * 8,
                     move);
        break;
      default:
        break;
    }
    ShowWindow(control, SW_SHOW);
  }

  // The buttons are a block below the last column, as wide as its fields.
  // The two ways to play in VR are in the first row, "Save Config" and "Play,
  // no VR" in the second, and "Support" has the third row to itself. When a
  // column before the last is taller, the block is at the bottom of the
  // window.
  const int button_h = Scaled(28);
  const int button_gap = Scaled(8);
  const int columns = column + 1;
  const int width = columns * (col_w + pad);
  const int block_w = label_w + field_w;
  const int block_h = button_h * 3 + button_gap * 2;
  const int half_w = (block_w - button_gap) / 2;
  const int left_x = pad + column * (col_w + pad);
  const int right_x = left_x + block_w - half_w;
  const int table_bottom = pad + tallest * row;
  int row1_y = pad + used * row + Scaled(10);
  if (row1_y < table_bottom - block_h) row1_y = table_bottom - block_h;
  const int row2_y = row1_y + button_h + button_gap;
  const int row3_y = row2_y + button_h + button_gap;
  SetWindowPos(g_buttons[kButtonPlay], nullptr, left_x, row1_y, half_w,
               button_h, move);
  SetWindowPos(g_buttons[kButtonFresh], nullptr, right_x, row1_y, half_w,
               button_h, move);
  SetWindowPos(g_buttons[kButtonSave], nullptr, left_x, row2_y, half_w,
               button_h, move);
  SetWindowPos(g_buttons[kButtonPlain], nullptr, right_x, row2_y, half_w,
               button_h, move);
  SetWindowPos(g_buttons[kButtonSupport], nullptr, left_x, row3_y, block_w,
               button_h, move);
  // The status line is below the table and the buttons, over the whole
  // window, and has room for two lines of text.
  const int block_bottom = row3_y + button_h;
  const int status_y =
      (block_bottom > table_bottom ? block_bottom : table_bottom) + Scaled(8);
  const int status_h = Scaled(32);
  const int height = status_y + status_h + Scaled(6);
  SetWindowPos(g_status, nullptr, pad, status_y, width - pad * 2, status_h,
               move);

  RECT want = {0, 0, width, height};
  AdjustWindowRect(&want, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME, FALSE);
  const int frame_w = want.right - want.left;
  const int frame_h = want.bottom - want.top;
  if (first) {
    RECT work = {0, 0, 0, 0};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    SetWindowPos(g_window, nullptr,
                 work.left + ((work.right - work.left) - frame_w) / 2,
                 work.top + ((work.bottom - work.top) - frame_h) / 2, frame_w,
                 frame_h, SWP_NOZORDER);
  } else {
    SetWindowPos(g_window, nullptr, 0, 0, frame_w, frame_h,
                 SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
    InvalidateRect(g_window, nullptr, TRUE);
  }
}

// Makes a control for each row, the buttons and the status line, and lays
// them out (LayOutControls).
void BuildControls() {
  const TableSizes t = Sizes();
  const int row = t.row;
  int id = 100;

  for (int i = 0; i < kSettingCount; ++i) {
    const Setting& s = kSettings[i];
    g_heading[i] = nullptr;
    g_label[i] = nullptr;
    g_control[i] = nullptr;
    if (s.kind == kGroup) {
      HWND heading = CreateWindowExA(0, "STATIC", s.label,
                                     WS_CHILD | WS_VISIBLE, 0, 0, t.col_w,
                                     row - Scaled(4), g_window, nullptr,
                                     nullptr, nullptr);
      SendMessageA(heading, WM_SETFONT, reinterpret_cast<WPARAM>(g_bold), TRUE);
      g_heading[i] = heading;
      continue;
    }

    if (s.kind != kBool) {
      HWND label = CreateWindowExA(0, "STATIC", s.label,
                                   WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0,
                                   t.label_w, row - Scaled(6), g_window,
                                   nullptr, nullptr, nullptr);
      SendMessageA(label, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
      AddTip(label, s.help);
      g_label[i] = label;
    }

    HWND control = nullptr;
    switch (s.kind) {
      case kBool:
        control = CreateWindowExA(
            0, "BUTTON", s.label, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0,
            0, t.col_w, row - Scaled(4), g_window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id++)), nullptr,
            nullptr);
        break;
      case kInt:
        control = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_RIGHT, 0, 0, t.field_w,
            row - Scaled(4), g_window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id++)), nullptr,
            nullptr);
        break;
      case kScale:
      case kSlider: {
        // TBS_NOTICKS keeps the slider inside one ordinary row, which matters
        // because the whole layout counts rows rather than pixels. A taller
        // control would mean the column break and the window height stopped
        // agreeing with each other.
        int low = 0, high = 0, step = 0;
        ScaleRange(s.choices, &low, &high, &step);
        control = CreateWindowExA(
            0, TRACKBAR_CLASS, "",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 0, 0, t.field_w,
            row - Scaled(4), g_window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id++)), nullptr,
            nullptr);
        SendMessageA(control, TBM_SETRANGE, TRUE, MAKELONG(low, high));
        SendMessageA(control, TBM_SETPAGESIZE, 0, step);
        SendMessageA(control, TBM_SETLINESIZE, 0, step);
        break;
      }
      case kChoice:
      case kValues:
      case kRenderer:
        control = CreateWindowExA(
            0, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0,
            t.field_w, row * 8, g_window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id++)), nullptr,
            nullptr);
        FillChoices(control, s.choices);
        break;
      default:
        break;
    }
    if (control != nullptr) {
      SendMessageA(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
      AddTip(control, s.help);
    }
    g_control[i] = control;
  }

  // The buttons are made in the order of the Tab key. LayOutControls gives
  // them their places and sizes.
  g_buttons[kButtonPlay] = CreateWindowExA(
      0, "BUTTON", "Play in VR", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0,
      0, 0, 0, g_window, reinterpret_cast<HMENU>(1), nullptr, nullptr);
  g_buttons[kButtonFresh] = CreateWindowExA(
      0, "BUTTON", "Play, new game", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
      g_window, reinterpret_cast<HMENU>(2), nullptr, nullptr);
  g_buttons[kButtonSave] = CreateWindowExA(
      0, "BUTTON", "Save Config", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, g_window,
      reinterpret_cast<HMENU>(3), nullptr, nullptr);
  g_buttons[kButtonPlain] = CreateWindowExA(
      0, "BUTTON", "Play, no VR", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, g_window,
      reinterpret_cast<HMENU>(5), nullptr, nullptr);
  // The space at the start of the text keeps the text off the heart.
  g_buttons[kButtonSupport] = CreateWindowExA(
      0, "BUTTON", " Support", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, g_window,
      reinterpret_cast<HMENU>(4), nullptr, nullptr);
  // The heart shows to the left of the text, at the height of a capital.
  HICON heart = MakeHeartIcon(Scaled(14));
  if (heart != nullptr) {
    SendMessageA(g_buttons[kButtonSupport], BM_SETIMAGE, IMAGE_ICON,
                 reinterpret_cast<LPARAM>(heart));
  }
  // SS_NOPREFIX, because the product name holds an ampersand and a static
  // control would otherwise read it as an access key.
  g_status = CreateWindowExA(0, "STATIC", "",
                             WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0,
                             0, 0, 0, g_window, nullptr, nullptr, nullptr);
  for (HWND button : g_buttons) {
    SendMessageA(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
  }
  SendMessageA(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
  AddTip(g_buttons[kButtonFresh],
         "Start a new game on the first island, skipping the menu.");
  AddTip(g_buttons[kButtonSave],
         "Write the settings without starting the game.");
  AddTip(g_buttons[kButtonPlain],
         "Start the game on the monitor, without the headset. The game "
         "is full screen, at the size of the desktop, and the mod does "
         "nothing in it.");
  AddTip(g_buttons[kButtonSupport],
         "Opens " BWVR_SUPPORT_URL " in your browser. The mod is "
         "free, and it stays free.");
  LayOutControls(true);
}

LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM w, LPARAM l) {
  switch (message) {
    case WM_COMMAND:
      if (HIWORD(w) == CBN_SELCHANGE) UpdateRendererRows(false);
      switch (LOWORD(w)) {
        case 1:
          Play(false);
          return 0;
        case 2:
          Play(true);
          return 0;
        case 3:
          SaveAll();
          if (g_write_failed) {
            SayStatus("Windows did not let the launcher write bwvr.ini in the "
                      "game folder.");
          } else if (g_said_no_vulkan) {
            SayStatus("Saved. Your GPU has no Vulkan 1.3, so the renderer "
                      "stays Direct3D 9.");
          } else {
            SayStatus("Saved.");
          }
          return 0;
        case 4:
          // The support page. It opens in the player's own browser.
          ShellExecuteA(nullptr, "open", BWVR_SUPPORT_URL, nullptr, nullptr,
                        SW_SHOWNORMAL);
          return 0;
        case 5:
          PlayNoVr();
          return 0;
        default:
          break;
      }
      break;
    // A trackbar reports itself here, not through WM_COMMAND, so without this
    // the value beside the slider would never move.
    case WM_HSCROLL: {
      HWND from = reinterpret_cast<HWND>(l);
      for (int i = 0; i < kSettingCount; ++i) {
        if (g_control[i] != from) continue;
        if (kSettings[i].kind == kScale) {
          SayScale(i, static_cast<int>(SendMessageA(from, TBM_GETPOS, 0, 0)));
        } else if (kSettings[i].kind == kSlider) {
          SaySlider(i, SliderValue(i));
        }
        break;
      }
      return 0;
    }
    case WM_CTLCOLORSTATIC:
      SetBkMode(reinterpret_cast<HDC>(w), TRANSPARENT);
      return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcA(window, message, w, l);
}

void MakeFonts() {
  NONCLIENTMETRICSA metrics = {sizeof(metrics)};
  if (SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics,
                            0)) {
    g_font = CreateFontIndirectA(&metrics.lfMessageFont);
    metrics.lfMessageFont.lfWeight = FW_SEMIBOLD;
    g_bold = CreateFontIndirectA(&metrics.lfMessageFont);
  }
  if (g_font == nullptr) {
    g_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    g_bold = g_font;
  }
}

// A PICTURE OF THE WINDOW FOR A TEST.
//
// "-shot <file>" at the end of the command line makes the launcher write its
// window to a BMP file and end. A person who uses the PC does not see the
// window: it is outside the screen, it has no button on the taskbar, and it
// does not take the focus. It must show there, because a list draws its text
// only in a window that shows. The picture is the client area, which each
// control draws into a bitmap (WM_PRINT). The launcher reads the settings
// for it and writes nothing but the file.
bool WriteShot(const char* path) {
  SetWindowLongA(g_window, GWL_EXSTYLE,
                 GetWindowLongA(g_window, GWL_EXSTYLE) | WS_EX_NOACTIVATE);
  SetWindowPos(g_window, nullptr, -32000, -32000, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  ShowWindow(g_window, SW_SHOWNOACTIVATE);
  UpdateWindow(g_window);

  RECT client = {0, 0, 0, 0};
  GetClientRect(g_window, &client);
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = client.right;
  info.bmiHeader.biHeight = client.bottom;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC dc = CreateCompatibleDC(nullptr);
  HBITMAP bitmap =
      CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bitmap == nullptr) {
    DeleteDC(dc);
    return false;
  }
  HGDIOBJ before = SelectObject(dc, bitmap);
  FillRect(dc, &client, GetSysColorBrush(COLOR_BTNFACE));
  // WM_PRINT draws from the corner of the frame of the window. Move the
  // origin, so that the corner of the client area is the corner of the
  // picture.
  RECT frame = {0, 0, 0, 0};
  GetWindowRect(g_window, &frame);
  POINT corner = {0, 0};
  ClientToScreen(g_window, &corner);
  SetViewportOrgEx(dc, frame.left - corner.x, frame.top - corner.y, nullptr);
  SendMessageA(g_window, WM_PRINT, reinterpret_cast<WPARAM>(dc),
               PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
  GdiFlush();

  const DWORD size =
      static_cast<DWORD>(client.right) * static_cast<DWORD>(client.bottom) * 4;
  BITMAPFILEHEADER header = {};
  header.bfType = 0x4D42;
  header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader);
  header.bfSize = header.bfOffBits + size;
  bool written = false;
  FILE* file = nullptr;
  if (fopen_s(&file, path, "wb") == 0 && file != nullptr) {
    written = fwrite(&header, sizeof(header), 1, file) == 1 &&
              fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, file) == 1 &&
              fwrite(bits, 1, size, file) == size;
    fclose(file);
  }
  SelectObject(dc, before);
  DeleteObject(bitmap);
  DeleteDC(dc);
  return written;
}

}  // namespace

// A TEST AID: "--picture <file>" saves a picture of the window as a BMP
// file and ends. "--game <folder>" names the folder of the game when the
// launcher is not in it. The window is outside the screen, at the bottom of the
// order of the windows, and is never active. Nothing is written to the ini.
bool SavePicture(const char* path) {
  RECT r;
  GetWindowRect(g_window, &r);
  const int w = r.right - r.left, h = r.bottom - r.top;
  HDC screen = GetDC(nullptr);
  HDC dc = CreateCompatibleDC(screen);
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = w;
  info.bmiHeader.biHeight = h;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP bitmap =
      CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, screen);
  if (bitmap == nullptr) {
    DeleteDC(dc);
    return false;
  }
  HGDIOBJ old = SelectObject(dc, bitmap);
  const BOOL printed = PrintWindow(g_window, dc, 2);  // PW_RENDERFULLCONTENT
  SelectObject(dc, old);
  bool ok = false;
  FILE* file = nullptr;
  if (printed && fopen_s(&file, path, "wb") == 0 && file != nullptr) {
    BITMAPFILEHEADER head = {};
    head.bfType = 0x4D42;
    head.bfOffBits = sizeof(head) + sizeof(info.bmiHeader);
    head.bfSize = head.bfOffBits + static_cast<DWORD>(w) * h * 4;
    fwrite(&head, sizeof(head), 1, file);
    fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, file);
    fwrite(bits, static_cast<size_t>(w) * h * 4, 1, file);
    fclose(file);
    ok = true;
  }
  DeleteObject(bitmap);
  DeleteDC(dc);
  return ok;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR command, int) {
  // The words of a test: "-rows <n>", then "-shot <file>" to the end of the
  // line.
  const char* shot = strstr(command, "-shot ");
  if (shot != nullptr) shot += 6;
  const char* rows = strstr(command, "-rows ");
  if (rows != nullptr && atoi(rows + 6) >= 4) g_max_rows = atoi(rows + 6);

  SetProcessDPIAware();
  HDC screen = GetDC(nullptr);
  if (screen != nullptr) {
    g_dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);
  }

  INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES |
                                                         ICC_BAR_CLASSES};
  InitCommonControlsEx(&controls);

  const char* picture = nullptr;
  const char* game = nullptr;
  for (int i = 1; i + 1 < __argc; ++i) {
    if (strcmp(__argv[i], "--picture") == 0) picture = __argv[i + 1];
    if (strcmp(__argv[i], "--game") == 0) game = __argv[i + 1];
  }
  bool found = FindGame();
  if (!found && picture != nullptr && game != nullptr &&
      FileThere(game, "runblack.exe")) {
    TakeGameDir(game);
    found = true;
  }
  if (!found) {
    if (picture != nullptr) return 1;
    if (shot == nullptr) {
      MessageBoxA(nullptr,
                  "Cannot find runblack.exe. Install the " BWVR_PRODUCT_NAME
                  " in the folder that holds your Black & White game.",
                  kProductName, MB_ICONERROR | MB_OK);
      return 1;
    }
    // A picture of -shot needs no game. The table then shows its defaults,
    // and the status line names a file that is missing.
    TakeGameDir(".");
  }

  MakeFonts();

  WNDCLASSA wc = {0};
  wc.lpfnWndProc = Proc;
  wc.hInstance = instance;
  wc.lpszClassName = kWindowClass;
  wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconA(instance, MAKEINTRESOURCEA(1));
  if (wc.hIcon == nullptr) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  RegisterClassA(&wc);

  g_window = CreateWindowExA(
      picture != nullptr ? WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW : 0,
      kWindowClass, kTitle, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME &
                                   ~WS_MAXIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, Scaled(700), Scaled(560), nullptr, nullptr,
      instance, nullptr);
  if (g_window == nullptr) return 1;

  g_tips = CreateWindowExA(0, TOOLTIPS_CLASSA, nullptr,
                           WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0,
                           g_window, nullptr, instance, nullptr);
  SendMessageA(g_tips, TTM_SETMAXTIPWIDTH, 0, Scaled(380));

  BuildControls();
  LoadAll();
  UpdateRendererRows(true);
  CheckEverything();
  if (picture != nullptr) {
    SetWindowPos(g_window, HWND_BOTTOM, -30000, -30000, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(g_window, SW_SHOWNOACTIVATE);
    // Each control paints itself now, and the messages of the paint go out
    // before the picture.
    RedrawWindow(g_window, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    const DWORD until = GetTickCount() + 300;
    MSG pending;
    while (GetTickCount() < until) {
      while (PeekMessageA(&pending, nullptr, 0, 0, PM_REMOVE)) {
        DispatchMessageA(&pending);
      }
      Sleep(10);
    }
    const bool saved = SavePicture(picture);
    DestroyWindow(g_window);
    return saved ? 0 : 1;
  }
  if (shot != nullptr) return WriteShot(shot) ? 0 : 1;
  ShowWindow(g_window, SW_SHOW);
  UpdateWindow(g_window);

  MSG message;
  while (GetMessageA(&message, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageA(g_window, &message)) {
      TranslateMessage(&message);
      DispatchMessageA(&message);
    }
  }
  // "Play, no VR" started the game. When that game ends, the values for the
  // headset come back.
  if (g_plain_game != nullptr) {
    WaitForSingleObject(g_plain_game, INFINITE);
    CloseHandle(g_plain_game);
    WriteHeadsetStart();
  }
  return 0;
}
