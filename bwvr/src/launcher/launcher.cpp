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
#include <winhttp.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "../common/exe_check.h"
#include "../common/settings_table.h"
#include "../common/update_check.h"
#include "../common/version.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winhttp.lib")

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
int g_max_rows = 26;

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
// their versions. It asks one time, because an instance takes time.
bool g_said_no_vulkan = false;

bool AskVulkan13() {
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

bool HasVulkan13() {
  static int s_answer = -1;
  if (s_answer < 0) s_answer = AskVulkan13() ? 1 : 0;
  return s_answer != 0;
}

// THE FALL BACK OF THE RENDERER VULKAN (docs/backend.md). The mod writes
// this file beside the game before it uses Vulkan, and the first picture
// deletes it. When it is there at a start, the game uses dxwrapper. A save
// deletes it, so the next start with Vulkan tries Vulkan again.
const char kVulkanPendingFile[] = "bwvr-vulkan-pending.txt";
// bwvr.ini has backend=vulkan and the file is there, so the row "Renderer"
// shows Direct3D 9 (LoadAll).
bool g_vulkan_fell_back = false;

bool VulkanPendingThere() { return FileThere(g_game_dir, kVulkanPendingFile); }

// --- the status line: the checks before a start ----------------------------

// The text is green with a tick, or red with a warning sign
// (WM_CTLCOLORSTATIC). With a newer version on GitHub the line of a ready
// PC is blue, and a click on it opens the page of that version.
bool g_status_ok = true;
bool g_status_update = false;
// The line shows the result of CheckEverything, and `before` of that call.
// When the answer of GitHub comes, the line shows the checks again with the
// same `before`. A line of another fault stays.
bool g_status_from_checks = false;
char g_status_before[256] = "";
// The tooltip of the status line: each check and its result.
char g_status_tip[2048] = "";

void SetStatusTip(const char* text) {
  strcpy_s(g_status_tip, sizeof(g_status_tip), text);
  if (g_tips == nullptr || g_status == nullptr) return;
  TOOLINFOA tip = {0};
  tip.cbSize = sizeof(tip);
  tip.hwnd = g_window;
  tip.uId = reinterpret_cast<UINT_PTR>(g_status);
  tip.lpszText = g_status_tip;
  SendMessageA(g_tips, TTM_UPDATETIPTEXTA, 0, reinterpret_cast<LPARAM>(&tip));
}

// The font of the launcher has no tick and no warning sign. Windows takes
// them from a font that it links to it (Segoe UI Symbol).
void SayStatus(bool ok, const char* text, bool update = false) {
  g_status_ok = ok;
  g_status_update = ok && update;
  wchar_t wide[1024];
  wide[0] = ok ? 0x2713 : 0x26A0;
  wide[1] = L' ';
  if (MultiByteToWideChar(CP_ACP, 0, text, -1, wide + 2, 1022) == 0) {
    wide[2] = 0;
  }
  SetWindowTextW(g_status, wide);
  InvalidateRect(g_status, nullptr, TRUE);
}

void SayStatus(const char* text) {
  g_status_from_checks = false;
  SayStatus(false, text);
}

// --- the update check ------------------------------------------------------
//
// THE UPDATE CHECK (docs/launcher.md, "The update check"). When the launcher
// opens, a thread of its own asks GitHub for the latest release, so the
// window never waits for the network. The owner marks the release for the
// players as the latest one. With no answer, or an answer that is not as
// expected, the status line says nothing about updates, and its tooltip
// gives the cause.

const char kLatestReleaseUrl[] =
    "https://api.github.com/repos/BW1-VR/BW1-VR/releases/latest";
// The thread posts this message to the window when it has the result.
const UINT kUpdateAnswer = WM_APP + 1;
// The time out of each step of the ask: the name, the connection, the send
// and the answer.
const int kUpdateTimeoutMs = 5000;
// A release of the mod is about 10 KB of JSON.
const DWORD kUpdateMostBytes = 1024 * 1024;

struct UpdateResult {
  bool newer = false;
  char version[32] = "";
  char page[512] = "";
  // The line of the tooltip of the status line.
  char note[640] = "";
};

// The ask of this start: the address, or a tag of a test that takes the
// place of the answer. The window writes them before the thread starts.
char g_update_url[512] = "";
char g_update_test_tag[128] = "";
// The thread writes g_update_thread_result, and then posts kUpdateAnswer.
// The window reads it only after that message, into g_update.
UpdateResult g_update_thread_result;
UpdateResult g_update;
HANDLE g_update_thread = nullptr;

// A GET of `url` with WinHTTP. True with the body of an answer with the
// HTTP status 200. Else `why` gives the cause.
bool FetchUrl(const char* url, char* body, DWORD size, DWORD* got, char* why,
              size_t why_size) {
  *got = 0;
  wchar_t wide[512];
  if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wide, 512) == 0) {
    strcpy_s(why, why_size, "the address is too long");
    return false;
  }
  URL_COMPONENTS parts = {sizeof(parts)};
  wchar_t host[256] = L"";
  wchar_t path[512] = L"";
  parts.lpszHostName = host;
  parts.dwHostNameLength = 256;
  parts.lpszUrlPath = path;
  parts.dwUrlPathLength = 512;
  if (!WinHttpCrackUrl(wide, 0, 0, &parts)) {
    _snprintf_s(why, why_size, _TRUNCATE,
                "the address is not a URL (WinHTTP error %lu)", GetLastError());
    return false;
  }
  wchar_t agent[64];
  _snwprintf_s(agent, 64, _TRUNCATE, L"BW1-VR-launcher/%hs",
               BWVR_VERSION_TEXT);
  HINTERNET session =
      WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  // Windows before 8.1 has no automatic proxy.
  if (session == nullptr) {
    session = WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  }
  HINTERNET connection = nullptr;
  HINTERNET request = nullptr;
  const char* step = "open";
  bool ok = false;
  DWORD status = 0;
  DWORD status_size = sizeof(status);
  if (session == nullptr) goto done;
  WinHttpSetTimeouts(session, kUpdateTimeoutMs, kUpdateTimeoutMs,
                     kUpdateTimeoutMs, kUpdateTimeoutMs);
  step = "connect";
  connection = WinHttpConnect(session, host, parts.nPort, 0);
  if (connection == nullptr) goto done;
  step = "request";
  request = WinHttpOpenRequest(
      connection, L"GET", path, nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
  if (request == nullptr) goto done;
  WinHttpAddRequestHeaders(request, L"Accept: application/vnd.github+json",
                           static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
  step = "send";
  if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    goto done;
  }
  step = "answer";
  if (!WinHttpReceiveResponse(request, nullptr)) goto done;
  if (!WinHttpQueryHeaders(
          request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
          WINHTTP_NO_HEADER_INDEX)) {
    goto done;
  }
  if (status != 200) {
    _snprintf_s(why, why_size, _TRUNCATE,
                "GitHub answered with the HTTP status %lu", status);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return false;
  }
  step = "read";
  for (;;) {
    if (*got >= size - 1) {
      _snprintf_s(why, why_size, _TRUNCATE,
                  "the answer is longer than %lu bytes", size - 1);
      WinHttpCloseHandle(request);
      WinHttpCloseHandle(connection);
      WinHttpCloseHandle(session);
      return false;
    }
    DWORD read = 0;
    if (!WinHttpReadData(request, body + *got, size - 1 - *got, &read)) {
      goto done;
    }
    if (read == 0) break;
    *got += read;
  }
  body[*got] = 0;
  ok = true;

done:
  if (!ok) {
    const DWORD error = GetLastError();
    const char* meaning = "";
    if (error == ERROR_WINHTTP_NAME_NOT_RESOLVED) {
      meaning = ": no network, or no name server";
    } else if (error == ERROR_WINHTTP_TIMEOUT) {
      meaning = ": the time out";
    } else if (error == ERROR_WINHTTP_CANNOT_CONNECT) {
      meaning = ": no connection";
    } else if (error == ERROR_WINHTTP_SECURE_FAILURE) {
      meaning = ": the secure connection failed";
    }
    _snprintf_s(why, why_size, _TRUNCATE,
                "WinHTTP error %lu at the step %s%s", error, step, meaning);
  }
  if (request != nullptr) WinHttpCloseHandle(request);
  if (connection != nullptr) WinHttpCloseHandle(connection);
  if (session != nullptr) WinHttpCloseHandle(session);
  return ok;
}

DWORD WINAPI AskForUpdate(void*) {
  UpdateResult& result = g_update_thread_result;
  static char body[kUpdateMostBytes];
  DWORD got = 0;
  char why[256] = "";
  const char* from = "GitHub";
  bool answered = false;
  if (g_update_test_tag[0] != 0) {
    // A test: the answer of a release with this tag.
    from = "the tag of the test";
    _snprintf_s(body, sizeof(body), _TRUNCATE,
                "{\"tag_name\":\"%s\",\"html_url\":\"https://github.com/"
                "BW1-VR/BW1-VR/releases/tag/%s\"}",
                g_update_test_tag, g_update_test_tag);
    got = static_cast<DWORD>(strlen(body));
    answered = true;
  } else {
    answered =
        FetchUrl(g_update_url, body, sizeof(body), &got, why, sizeof(why));
  }
  if (!answered) {
    _snprintf_s(result.note, sizeof(result.note), _TRUNCATE,
                "Updates: no answer from %s (%s).", g_update_url, why);
  } else {
    update_check::Release release;
    const update_check::Verdict verdict = update_check::Judge(
        body, got, BWVR_VERSION_MAJOR, BWVR_VERSION_MINOR, BWVR_VERSION_PATCH,
        &release);
    switch (verdict) {
      case update_check::kNewer:
        result.newer = true;
        _snprintf_s(result.version, sizeof(result.version), _TRUNCATE,
                    "%d.%d.%d", release.major, release.minor, release.patch);
        strcpy_s(result.page, sizeof(result.page), release.page);
        _snprintf_s(result.note, sizeof(result.note), _TRUNCATE,
                    "Updates: the latest release from %s is %s (tag %s), and "
                    "this launcher is " BWVR_VERSION_TEXT ". The page: %s",
                    from, result.version, release.tag, release.page);
        break;
      case update_check::kNotNewer:
        _snprintf_s(result.note, sizeof(result.note), _TRUNCATE,
                    "Updates: the latest release from %s is %d.%d.%d (tag "
                    "%s), and this launcher is " BWVR_VERSION_TEXT
                    ". No newer version is out.",
                    from, release.major, release.minor, release.patch,
                    release.tag);
        break;
      case update_check::kNoTag:
        _snprintf_s(result.note, sizeof(result.note), _TRUNCATE,
                    "Updates: the answer from %s (%lu bytes) has no "
                    "tag_name.",
                    from, got);
        break;
      case update_check::kBadTag:
        _snprintf_s(result.note, sizeof(result.note), _TRUNCATE,
                    "Updates: the tag %s from %s has no version.", release.tag,
                    from);
        break;
      default:
        _snprintf_s(result.note, sizeof(result.note), _TRUNCATE,
                    "Updates: the answer from %s has no html_url of GitHub.",
                    from);
        break;
    }
  }
  if (g_window != nullptr) PostMessageA(g_window, kUpdateAnswer, 0, 0);
  return 0;
}

// Starts the ask when bwvr.ini has update_check=1. `test_tag` takes the
// place of the answer, and `test_url` the place of the address. A picture
// for a test (`picture`) asks only with one of them, so that the picture
// does not change with the releases on GitHub.
void StartUpdateCheck(bool picture, const char* test_tag,
                      const char* test_url) {
  if (GetPrivateProfileIntA("bwvr", "update_check", 1, g_ini_path) == 0) {
    strcpy_s(g_update.note, sizeof(g_update.note),
             "Updates: off (update_check=0 in bwvr.ini).");
    return;
  }
  if (picture && test_tag == nullptr && test_url == nullptr) {
    strcpy_s(g_update.note, sizeof(g_update.note),
             "Updates: no ask for a picture of a test.");
    return;
  }
  strcpy_s(g_update_url, sizeof(g_update_url),
           test_url != nullptr ? test_url : kLatestReleaseUrl);
  if (test_tag != nullptr) {
    strcpy_s(g_update_test_tag, sizeof(g_update_test_tag), test_tag);
  }
  strcpy_s(g_update.note, sizeof(g_update.note),
           "Updates: the launcher asks GitHub for the latest version.");
  g_update_thread =
      CreateThread(nullptr, 0, AskForUpdate, nullptr, 0, nullptr);
  if (g_update_thread == nullptr) {
    strcpy_s(g_update.note, sizeof(g_update.note),
             "Updates: the launcher could not start the thread of the ask.");
  }
}

void OpenUpdatePage() {
  if (!g_update.newer || !update_check::IsGitHubPage(g_update.page)) return;
  ShellExecuteA(nullptr, "open", g_update.page, nullptr, nullptr,
                SW_SHOWNORMAL);
}

// Adds one line to the text of a tooltip.
void TipLine(char* tip, size_t size, const char* line) {
  if (tip[0] != 0) strcat_s(tip, size, "\n");
  strcat_s(tip, size, line);
}

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

const char* FileName(const char* path) {
  const char* slash = strrchr(path, '\\');
  const char* other = strrchr(path, '/');
  if (other != nullptr && (slash == nullptr || other > slash)) slash = other;
  return slash != nullptr ? slash + 1 : path;
}

// The active OpenXR runtime of one view of the registry (KEY_WOW64_32KEY or
// KEY_WOW64_64KEY), with its variables expanded. It only reads.
bool ActiveRuntime(REGSAM view, char* out, size_t size) {
  out[0] = 0;
  HKEY key;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Khronos\\OpenXR\\1", 0,
                    KEY_READ | view, &key) != ERROR_SUCCESS) {
    return false;
  }
  char raw[MAX_PATH] = {0};
  DWORD bytes = sizeof(raw) - 1;
  DWORD type = 0;
  const LONG got = RegQueryValueExA(key, "ActiveRuntime", nullptr, &type,
                                    reinterpret_cast<BYTE*>(raw), &bytes);
  RegCloseKey(key);
  if (got != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) ||
      raw[0] == 0) {
    return false;
  }
  if (ExpandEnvironmentStringsA(raw, out, static_cast<DWORD>(size)) == 0) {
    strcpy_s(out, size, raw);
  }
  return true;
}

// The machine of a DLL from its PE header, or 0 when it cannot be read.
WORD DllMachine(const char* path) {
  FILE* f = nullptr;
  if (fopen_s(&f, path, "rb") != 0 || f == nullptr) return 0;
  unsigned char head[1024] = {0};
  const size_t got = fread(head, 1, sizeof(head), f);
  fclose(f);
  if (got < 64 || head[0] != 'M' || head[1] != 'Z') return 0;
  DWORD pe = 0;
  memcpy(&pe, head + 0x3C, sizeof(pe));
  if (pe + 6 > got || memcmp(head + pe, "PE\0\0", 4) != 0) return 0;
  WORD machine = 0;
  memcpy(&machine, head + pe + 4, sizeof(machine));
  return machine;
}

// The value of "library_path" in the manifest of an API layer, as a full
// path. False when the manifest has none, or names a DLL with no folder
// (Windows then looks for it in its own folders).
bool LayerLibrary(const char* manifest, char* out, size_t size) {
  FILE* f = nullptr;
  if (fopen_s(&f, manifest, "rb") != 0 || f == nullptr) return false;
  static char text[65536];
  const size_t got = fread(text, 1, sizeof(text) - 1, f);
  fclose(f);
  text[got] = 0;
  const char* at = strstr(text, "\"library_path\"");
  if (at == nullptr) return false;
  at = strchr(at + 14, ':');
  if (at == nullptr) return false;
  at = strchr(at, '"');
  if (at == nullptr) return false;
  ++at;
  char path[MAX_PATH];
  size_t n = 0;
  for (; *at != 0 && *at != '"' && n + 1 < sizeof(path); ++at) {
    if (*at == '\\' && (at[1] == '\\' || at[1] == '/' || at[1] == '"')) ++at;
    path[n++] = *at == '/' ? '\\' : *at;
  }
  path[n] = 0;
  if (strchr(path, '\\') == nullptr) return false;
  const bool full = (path[0] != 0 && path[1] == ':') ||
                    (path[0] == '\\' && path[1] == '\\');
  if (full) {
    strcpy_s(out, size, path);
  } else {
    char folder[MAX_PATH];
    strcpy_s(folder, sizeof(folder), manifest);
    folder[FileName(folder) - folder] = 0;
    _snprintf_s(out, size, _TRUNCATE, "%s%s", folder, path);
  }
  return true;
}

// The first implicit OpenXR API layer for 32 bit programs that is on and
// broken: its manifest or its DLL is missing, or its DLL is for 64 bit
// programs only. The OpenXR loader of the game then stops each runtime
// (XR_ERROR_FILE_ACCESS_ERROR, xr_screen.cpp). It only reads the registry.
// `name`, `why` and `where` get the parts of the message.
bool BrokenApiLayer(char* name, char* why, char* where, size_t size,
                    int* on_count) {
  struct Place {
    HKEY root;
    const char* name;
  };
  const Place places[] = {{HKEY_LOCAL_MACHINE, "HKLM"},
                          {HKEY_CURRENT_USER, "HKCU"}};
  for (const Place& place : places) {
    HKEY key;
    if (RegOpenKeyExA(place.root,
                      "SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit", 0,
                      KEY_READ | KEY_WOW64_32KEY, &key) != ERROR_SUCCESS) {
      continue;
    }
    for (DWORD i = 0;; ++i) {
      char manifest[MAX_PATH];
      DWORD name_size = sizeof(manifest);
      DWORD type = 0;
      DWORD value = 1;
      DWORD value_size = sizeof(value);
      const LONG got =
          RegEnumValueA(key, i, manifest, &name_size, nullptr, &type,
                        reinterpret_cast<BYTE*>(&value), &value_size);
      if (got == ERROR_NO_MORE_ITEMS) break;
      if (got != ERROR_SUCCESS || type != REG_DWORD || value != 0) continue;
      ++*on_count;
      const char* fault = nullptr;
      char library[MAX_PATH];
      if (GetFileAttributesA(manifest) == INVALID_FILE_ATTRIBUTES) {
        fault = "its file is missing";
      } else if (LayerLibrary(manifest, library, sizeof(library))) {
        if (GetFileAttributesA(library) == INVALID_FILE_ATTRIBUTES) {
          fault = "its DLL is missing";
        } else if (DllMachine(library) == IMAGE_FILE_MACHINE_AMD64) {
          fault = "its DLL is for 64 bit programs only";
        }
      }
      if (fault != nullptr) {
        strcpy_s(name, size, FileName(manifest));
        strcpy_s(why, size, fault);
        _snprintf_s(where, size, _TRUNCATE, "%s, %s", place.name, manifest);
        RegCloseKey(key);
        return true;
      }
    }
    RegCloseKey(key);
  }
  return false;
}

// The checks, in order: the files, the game, OpenXR and the renderer. The
// status line shows the first that fails, or "Ready". The tooltip shows
// each check that ran. `before` comes first in the line, for example
// "Saved.".
void CheckEverything(const char* before = nullptr) {
  char line[768] = "";
  char tip[2048] = "";
  char detail[1024];
  bool ok = false;
  bool update = false;
  g_status_from_checks = true;
  strcpy_s(g_status_before, sizeof(g_status_before),
           before != nullptr ? before : "");
  if (before != nullptr && before[0] != 0) {
    strcpy_s(line, sizeof(line), before);
    strcat_s(line, sizeof(line), " ");
  }
  char runtime[MAX_PATH] = "";
  char layer[MAX_PATH] = "", fault[MAX_PATH] = "", where[MAX_PATH] = "";
  int layers_on = 0;
  const char* missing = nullptr;
  char from_env[MAX_PATH] = "";
  DWORD env = 0;
  const char* source = "the 32 bit registry";

  // Every file the mod needs at run time. A missing dxwrapper file makes the
  // mod fall back to a slow path in silence, so this check lists them.
  static const char* const kNeeded[] = {
      "ddraw.dll",
      "openxr_loader.dll",
      "dxwrapper\\ddraw.dll",
      "dxwrapper\\dxwrapper.dll",
      "dxwrapper\\dxwrapper.ini",
  };
  for (const char* name : kNeeded) {
    if (!FileThere(g_game_dir, name)) {
      missing = name;
      break;
    }
  }
  if (missing != nullptr) {
    _snprintf_s(detail, sizeof(detail), _TRUNCATE,
                "%s is missing from the game folder. Install the %s again.",
                missing, kProductName);
    strcat_s(line, sizeof(line), detail);
    _snprintf_s(detail, sizeof(detail), _TRUNCATE,
                "The files of the mod: %s is missing in %s.", missing,
                g_game_dir);
    TipLine(tip, sizeof(tip), detail);
    goto done;
  }
  TipLine(tip, sizeof(tip),
          "The files of the mod: ddraw.dll, openxr_loader.dll and the "
          "dxwrapper folder are there.");

  if (!GameBuildIsRight(detail, sizeof(detail))) {
    strcat_s(line, sizeof(line), detail);
    TipLine(tip, sizeof(tip), "The game: runblack.exe is not v1.42.");
    goto done;
  }
  TipLine(tip, sizeof(tip), "The game: runblack.exe is v1.42.");

  // OPENXR. The game is 32 bit, so its OpenXR loader reads the variable
  // XR_RUNTIME_JSON, or else the 32 bit view of the registry. A runtime that
  // is registered only for 64 bit cannot serve it. This is the most common
  // reason for a headset that shows nothing.
  env = GetEnvironmentVariableA("XR_RUNTIME_JSON", from_env, sizeof(from_env));
  if (env > 0 && env < sizeof(from_env)) {
    strcpy_s(runtime, sizeof(runtime), from_env);
    source = "the variable XR_RUNTIME_JSON";
  } else if (!ActiveRuntime(KEY_WOW64_32KEY, runtime, sizeof(runtime))) {
    char only64[MAX_PATH] = "";
    if (ActiveRuntime(KEY_WOW64_64KEY, only64, sizeof(only64))) {
      strcat_s(line, sizeof(line),
               "The OpenXR runtime is set for 64 bit programs only. The "
               "game is 32 bit. Use a runtime with 32 bit support.");
      _snprintf_s(detail, sizeof(detail), _TRUNCATE,
                  "OpenXR: no runtime for 32 bit programs. The runtime for "
                  "64 bit programs is %s. Virtual Desktop has a runtime for "
                  "32 bit programs, and SteamVR has one from version 2.17.",
                  only64);
    } else {
      strcat_s(line, sizeof(line),
               "No OpenXR runtime is set. Start the VR app of your headset, "
               "and make it the OpenXR runtime.");
      strcpy_s(detail, sizeof(detail),
               "OpenXR: no active runtime in the registry "
               "(HKLM\\SOFTWARE\\Khronos\\OpenXR\\1, ActiveRuntime).");
    }
    TipLine(tip, sizeof(tip), detail);
    goto done;
  }
  if (GetFileAttributesA(runtime) == INVALID_FILE_ATTRIBUTES) {
    _snprintf_s(detail, sizeof(detail), _TRUNCATE,
                "The file of the OpenXR runtime is missing: %s. Install the "
                "VR app of your headset again.",
                FileName(runtime));
    strcat_s(line, sizeof(line), detail);
    _snprintf_s(detail, sizeof(detail), _TRUNCATE,
                "OpenXR: the runtime from %s is %s, and this file is not "
                "there.",
                source, runtime);
    TipLine(tip, sizeof(tip), detail);
    goto done;
  }
  _snprintf_s(detail, sizeof(detail), _TRUNCATE,
              "OpenXR: the runtime for 32 bit programs is %s (from %s).",
              runtime, source);
  TipLine(tip, sizeof(tip), detail);

  if (BrokenApiLayer(layer, fault, where, sizeof(layer), &layers_on)) {
    _snprintf_s(detail, sizeof(detail), _TRUNCATE,
                "The OpenXR API layer %s is broken: %s. Turn it off with the "
                "tool OpenXR API Layers GUI.",
                layer, fault);
    strcat_s(line, sizeof(line), detail);
    _snprintf_s(detail, sizeof(detail), _TRUNCATE,
                "OpenXR API layers for 32 bit programs: %s is on, and %s. "
                "It stops each runtime.",
                where, fault);
    TipLine(tip, sizeof(tip), detail);
    goto done;
  }
  _snprintf_s(detail, sizeof(detail), _TRUNCATE,
              "OpenXR API layers for 32 bit programs: %d on, and none is "
              "broken.",
              layers_on);
  TipLine(tip, sizeof(tip), detail);

  // THE RENDERER, as the row shows it.
  if (g_vulkan_fell_back || (g_renderer_vulkan && VulkanPendingThere())) {
    strcat_s(line, sizeof(line),
             "The last start with Vulkan failed, so the game uses Direct3D "
             "9. To try Vulkan again, set Renderer to Vulkan and save.");
    _snprintf_s(detail, sizeof(detail), _TRUNCATE,
                "Renderer: the file %s is in the game folder. The mod wrote "
                "it, because the last start with Vulkan stopped before its "
                "first picture, or Vulkan failed. bwvr.log of that start "
                "gives the cause.",
                kVulkanPendingFile);
    TipLine(tip, sizeof(tip), detail);
    goto done;
  }
  if (g_renderer_vulkan) {
    if (!HasVulkan13()) {
      strcat_s(line, sizeof(line),
               "Your GPU has no Vulkan 1.3. Set Renderer to Direct3D 9.");
      TipLine(tip, sizeof(tip),
              "Renderer: Vulkan, but no GPU has Vulkan 1.3.");
      goto done;
    }
    TipLine(tip, sizeof(tip), "Renderer: Vulkan. A GPU has Vulkan 1.3.");
  } else {
    TipLine(tip, sizeof(tip), "Renderer: Direct3D 9.");
  }

  _snprintf_s(detail, sizeof(detail), _TRUNCATE, "Ready. OpenXR runtime: %s",
              FileName(runtime));
  strcat_s(line, sizeof(line), detail);
  ok = true;
  // A newer version shows only on the line of a ready PC. A fault comes
  // first.
  if (g_update.newer) {
    _snprintf_s(detail, sizeof(detail), _TRUNCATE,
                ". A new version, %s, is out. Click here to get it.",
                g_update.version);
    strcat_s(line, sizeof(line), detail);
    update = true;
  }

done:
  if (g_update.note[0] != 0) TipLine(tip, sizeof(tip), g_update.note);
  SayStatus(ok, line, update);
  SetStatusTip(tip);
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
  // The check of the renderer follows the row.
  if (!first) CheckEverything();
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
        // No key: Vulkan, when the GPU has Vulkan 1.3. The save writes it.
        // With the file of the fall back the game uses Direct3D 9, and the
        // row shows it. A save with Vulkan tries Vulkan again.
        char text[MAX_PATH] = {0};
        ReadIniText(s.key, text, sizeof(text));
        bool vulkan = text[0] == 0 ? HasVulkan13()
                                   : _stricmp(text, "vulkan") == 0;
        g_vulkan_fell_back =
            _stricmp(text, "vulkan") == 0 && VulkanPendingThere();
        if (g_vulkan_fell_back) vulkan = false;
        SendMessageA(g_control[i], CB_SETCURSEL, vulkan ? 0 : 1, 0);
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
        // The renderer is the choice of the player now. With Vulkan the
        // next start tries Vulkan again, and with Direct3D 9 the file does
        // nothing.
        char pending[MAX_PATH];
        _snprintf_s(pending, sizeof(pending), _TRUNCATE, "%s\\%s",
                    g_game_dir, kVulkanPendingFile);
        DeleteFileA(pending);
        g_vulkan_fell_back = false;
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
  // control would otherwise read it as an access key. A window of Unicode,
  // for the tick and the warning sign (SayStatus). SS_NOTIFY gives the
  // mouse to the tooltip.
  g_status = CreateWindowExW(
      0, L"STATIC", L"",
      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_NOTIFY, 0, 0, 0, 0,
      g_window, reinterpret_cast<HMENU>(6), nullptr, nullptr);
  for (HWND button : g_buttons) {
    SendMessageA(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
  }
  SendMessageA(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
  AddTip(g_status, g_status_tip);
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
            CheckEverything("Saved. Your GPU has no Vulkan 1.3, so the "
                            "renderer stays Direct3D 9.");
          } else {
            CheckEverything("Saved.");
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
        case 6:
          // A click on the status line with a newer version.
          if (HIWORD(w) == STN_CLICKED) OpenUpdatePage();
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
    case kUpdateAnswer:
      // The thread of the ask has ended (AskForUpdate).
      g_update = g_update_thread_result;
      if (g_status_from_checks) {
        char before[sizeof(g_status_before)];
        strcpy_s(before, sizeof(before), g_status_before);
        CheckEverything(before);
      }
      return 0;
    case WM_SETCURSOR:
      // The hand of a link over the line of a newer version.
      if (reinterpret_cast<HWND>(w) == g_status && g_status_update) {
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
      }
      break;
    case WM_CTLCOLORSTATIC:
      SetBkMode(reinterpret_cast<HDC>(w), TRANSPARENT);
      // The status line: green when each check passes, blue with a newer
      // version, else red.
      if (reinterpret_cast<HWND>(l) == g_status) {
        COLORREF color = RGB(200, 0, 0);
        if (g_status_update) {
          color = RGB(0, 90, 200);
        } else if (g_status_ok) {
          color = RGB(0, 128, 0);
        }
        SetTextColor(reinterpret_cast<HDC>(w), color);
      }
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
// for it and writes nothing but the file, and the tooltip of the status line
// in a text file beside it (`<file>.tip.txt`), because a picture shows no
// tooltip.
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
  char tip_path[MAX_PATH];
  strcpy_s(tip_path, sizeof(tip_path), path);
  // The rest of the command line can end with a space.
  size_t end = strlen(tip_path);
  while (end > 0 && tip_path[end - 1] == ' ') tip_path[--end] = 0;
  strcat_s(tip_path, sizeof(tip_path), ".tip.txt");
  if (fopen_s(&file, tip_path, "wb") == 0 && file != nullptr) {
    fputs(g_status_tip, file);
    fputs("\n", file);
    fclose(file);
  }
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
  // The words of a test of the update check: "-update-tag <tag>" takes the
  // place of the answer of GitHub, "-update-url <url>" the place of its
  // address.
  const char* update_tag = nullptr;
  const char* update_url = nullptr;
  for (int i = 1; i + 1 < __argc; ++i) {
    if (strcmp(__argv[i], "--picture") == 0) picture = __argv[i + 1];
    if (strcmp(__argv[i], "--game") == 0) game = __argv[i + 1];
    if (strcmp(__argv[i], "-update-tag") == 0) update_tag = __argv[i + 1];
    if (strcmp(__argv[i], "-update-url") == 0) update_url = __argv[i + 1];
  }
  bool found = FindGame();
  if (!found && (picture != nullptr || shot != nullptr) && game != nullptr &&
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
    // and the status line names a file that is missing. "--game <folder>"
    // gives it a game.
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
  // The ask starts before the checks, so that the tooltip of the status
  // line says that it runs.
  const bool test_picture = picture != nullptr || shot != nullptr;
  StartUpdateCheck(test_picture, update_tag, update_url);
  CheckEverything();
  // A picture for a test waits for the answer, so that it shows the line of
  // the update.
  if (test_picture && g_update_thread != nullptr) {
    WaitForSingleObject(g_update_thread, 4 * kUpdateTimeoutMs);
    MSG answer;
    while (PeekMessageA(&answer, nullptr, 0, 0, PM_REMOVE)) {
      DispatchMessageA(&answer);
    }
  }
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
