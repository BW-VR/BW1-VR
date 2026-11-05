// A TEST AID: the remote control of the switches of the debug page
// (remote_control of bwvr.ini, 0 by default, docs/android.md, "The test run"). A thread
// reads bwvr-control.txt next to the game 5 times a second. The game thread sets the
// switches at its next frame, because the functions of the switches run there when
// the player uses the debug page.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

#include "debug_menu.h"
#include "remote_math.h"

void ModLog(const char* format, ...);

namespace {

namespace rm = remote_math;

char g_path[MAX_PATH] = "";
CRITICAL_SECTION g_lock;
char g_text[rm::kMostBytes];
int g_size = 0;
volatile LONG g_new = 0;
bool g_on = false;

DWORD WINAPI ReadThread(void*) {
  int serial = -1;
  char text[rm::kMostBytes];
  for (;;) {
    Sleep(200);
    const HANDLE file =
        CreateFileA(g_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) continue;
    DWORD size = 0;
    const BOOL read = ReadFile(file, text, sizeof(text), &size, nullptr);
    CloseHandle(file);
    if (!read) continue;
    const int now = rm::SerialOf(text, static_cast<int>(size));
    if (now < 0 || now == serial) continue;
    serial = now;
    EnterCriticalSection(&g_lock);
    memcpy(g_text, text, size);
    g_size = static_cast<int>(size);
    LeaveCriticalSection(&g_lock);
    InterlockedExchange(&g_new, 1);
  }
}

}  // namespace

void RemoteControlInstall() {
  char path[MAX_PATH] = "";
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 20 >= MAX_PATH) return;
  slash[1] = 0;
  strcpy_s(g_path, path);
  strcat_s(g_path, "bwvr-control.txt");
  strcat_s(path, "bwvr.ini");
  if (GetPrivateProfileIntA("bwvr", "remote_control", 0, path) == 0) return;
  InitializeCriticalSection(&g_lock);
  const HANDLE thread = CreateThread(nullptr, 0, &ReadThread, nullptr, 0, nullptr);
  if (thread == nullptr) return;
  CloseHandle(thread);
  g_on = true;
  ModLog("remote       : remote_control=1. The switches of the debug page follow %s", g_path);
}

void RemoteControlFrame() {
  if (!g_on || InterlockedExchange(&g_new, 0) == 0) return;
  char text[rm::kMostBytes];
  EnterCriticalSection(&g_lock);
  const int size = g_size;
  memcpy(text, g_text, static_cast<size_t>(size));
  LeaveCriticalSection(&g_lock);
  int at = 0;
  rm::Line line;
  while (rm::NextLine(text, size, &at, &line)) {
    if (rm::IsAppKey(line)) continue;
    char key[64];
    int value = 0;
    if (line.key_size >= static_cast<int>(sizeof(key)) || !rm::IntOf(line, &value)) {
      ModLog("remote       : a line of %d bytes has no key and number", line.key_size);
      continue;
    }
    memcpy(key, line.key, static_cast<size_t>(line.key_size));
    key[line.key_size] = 0;
    DebugSwitchSetKey(key, value);
  }
}
