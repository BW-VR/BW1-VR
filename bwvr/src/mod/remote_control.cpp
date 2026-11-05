// A TEST AID: the remote control of the switches of the debug page
// (remote_control of bwvr.ini, 0 by default, docs/android.md, "The test run"). A thread
// reads bwvr-control.txt next to the game 5 times a second. The game thread sets the
// switches at its next frame, because the functions of the switches run there when
// the player uses the debug page. With mark=1 in the file a click of a stick writes the
// scale of the table into bwvr-marks.csv, so a test can ask the player where a picture
// starts to disturb.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "controllers.h"
#include "debug_menu.h"
#include "remote_math.h"

void ModLog(const char* format, ...);
float TabletopViewScale();
float DetailDistanceFactor();

namespace {

namespace rm = remote_math;

char g_path[MAX_PATH] = "";
CRITICAL_SECTION g_lock;
char g_text[rm::kMostBytes];
int g_size = 0;
volatile LONG g_new = 0;
bool g_on = false;
char g_marks_path[MAX_PATH] = "";
bool g_mark_on = false;
bool g_click_before = false;
ULONGLONG g_mark_at = 0;
int g_serial_now = -1;

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

// One line for a click: the serial of the step, the scale, and the factor of the distances.
void WriteMark() {
  const float units = TabletopViewScale();
  const float factor = DetailDistanceFactor();
  char line[96];
  const int size = sprintf_s(line, "%d,%.1f,%.2f\r\n", g_serial_now, units, factor);
  const HANDLE file = CreateFileA(g_marks_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file != INVALID_HANDLE_VALUE) {
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(size), &written, nullptr);
    CloseHandle(file);
  }
  ModLog("remote       : the player marked %.1f units a meter with the factor %.2f", units, factor);
}

void MarkFrame() {
  if (!g_mark_on) return;
  const bool click = ControllersAnyStickClick();
  const ULONGLONG now = GetTickCount64();
  // One click is one mark, also when the switch of the stick bounces.
  if (click && !g_click_before && now - g_mark_at >= 1000) {
    g_mark_at = now;
    WriteMark();
  }
  g_click_before = click;
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
  strcpy_s(g_marks_path, path);
  strcat_s(g_marks_path, "bwvr-marks.csv");
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
  if (!g_on) return;
  MarkFrame();
  if (InterlockedExchange(&g_new, 0) == 0) return;
  char text[rm::kMostBytes];
  EnterCriticalSection(&g_lock);
  const int size = g_size;
  memcpy(text, g_text, static_cast<size_t>(size));
  LeaveCriticalSection(&g_lock);
  g_serial_now = rm::SerialOf(text, size);
  int at = 0;
  rm::Line line;
  while (rm::NextLine(text, size, &at, &line)) {
    if (rm::IsAppKey(line)) continue;
    char key[64];
    int value = 0;
    if (rm::KeyIs(line, "mark")) {
      if (rm::IntOf(line, &value)) g_mark_on = value != 0;
      continue;
    }
    if (line.key_size >= static_cast<int>(sizeof(key)) || !rm::IntOf(line, &value)) {
      ModLog("remote       : a line of %d bytes has no key and number", line.key_size);
      continue;
    }
    memcpy(key, line.key, static_cast<size_t>(line.key_size));
    key[line.key_size] = 0;
    DebugSwitchSetKey(key, value);
  }
}
