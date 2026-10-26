// THE PARTS OF WINDOWS AND OF DIRECT3D 7 THAT THE VULKAN RENDERER USES, for
// a build with no Windows: the headset app (docs/android.md, "The renderer
// in the app"). vk_core.cpp and vk_scene.cpp include this file in place of
// windows.h, ddraw.h and d3d.h when _WIN32 is not defined.
//
// The values of the constants are the values of the headers of the Windows
// SDK. The math test of the PC compares each one with the SDK
// (test/controller_math/vk_platform_checks.h).
//
// The interfaces of DirectDraw and Direct3D are only the methods that the
// renderer calls. In the app their objects are the records of the stream
// of the game (android/src/scene_player.cpp).
#pragma once

#ifdef _WIN32
#error "vk_platform_posix.h is for a build with no Windows"
#endif

#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// DWORD, LONG, HRESULT, and the parts of Direct3D 7 and DirectDraw 7.
#include <string>

#include "../common/ini_text.h"
#include "vk_d3d7_posix.h"

// ---- Windows ----------------------------------------------------------------

typedef int BOOL;
typedef void* HWND;
typedef void* HMODULE;
typedef void* FARPROC;

#define SUCCEEDED(hr) (static_cast<HRESULT>(hr) >= 0)
#define FAILED(hr) (static_cast<HRESULT>(hr) < 0)
#define S_OK static_cast<HRESULT>(0)
#define E_FAIL static_cast<HRESULT>(0x80004005u)
#define MAX_PATH 260
#define _TRUNCATE (static_cast<size_t>(-1))

union LARGE_INTEGER {
  struct {
    uint32_t LowPart;
    int32_t HighPart;
  };
  int64_t QuadPart;
};

inline BOOL QueryPerformanceCounter(LARGE_INTEGER* value) {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  value->QuadPart = static_cast<int64_t>(t.tv_sec) * 1000000000 + t.tv_nsec;
  return 1;
}

inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* value) {
  value->QuadPart = 1000000000;
  return 1;
}

inline void Sleep(DWORD ms) { usleep(static_cast<useconds_t>(ms) * 1000); }

// A lock with the names of the slim reader writer lock of Windows. The
// renderer takes it only as exclusive.
struct SRWLOCK {
  pthread_mutex_t mutex;
};
#define SRWLOCK_INIT {PTHREAD_MUTEX_INITIALIZER}
inline void AcquireSRWLockExclusive(SRWLOCK* lock) { pthread_mutex_lock(&lock->mutex); }
inline void ReleaseSRWLockExclusive(SRWLOCK* lock) { pthread_mutex_unlock(&lock->mutex); }

// A critical section of Windows can be entered again by the thread that
// holds it, so this one is a recursive mutex.
struct CRITICAL_SECTION {
  pthread_mutex_t mutex;
};
inline void InitializeCriticalSection(CRITICAL_SECTION* cs) {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&cs->mutex, &attr);
  pthread_mutexattr_destroy(&attr);
}
inline void EnterCriticalSection(CRITICAL_SECTION* cs) { pthread_mutex_lock(&cs->mutex); }
inline void LeaveCriticalSection(CRITICAL_SECTION* cs) { pthread_mutex_unlock(&cs->mutex); }
inline void DeleteCriticalSection(CRITICAL_SECTION* cs) { pthread_mutex_destroy(&cs->mutex); }

// VirtualAlloc gives pages with zeros, so calloc does the same here.
#define MEM_COMMIT 0x00001000u
#define MEM_RESERVE 0x00002000u
#define MEM_RELEASE 0x00008000u
#define PAGE_READWRITE 0x04u
inline void* VirtualAlloc(void*, size_t size, DWORD, DWORD) { return calloc(1, size); }
inline BOOL VirtualFree(void* p, size_t, DWORD) {
  free(p);
  return 1;
}

inline LONG InterlockedIncrement(volatile LONG* p) {
  return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
}
inline LONG InterlockedDecrement(volatile LONG* p) {
  return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST);
}
inline LONG InterlockedExchange(volatile LONG* p, LONG value) {
  return __atomic_exchange_n(p, value, __ATOMIC_SEQ_CST);
}
inline LONG InterlockedCompareExchange(volatile LONG* p, LONG exchange,
                                       LONG comparand) {
  __atomic_compare_exchange_n(p, &comparand, exchange, false, __ATOMIC_SEQ_CST,
                              __ATOMIC_SEQ_CST);
  return comparand;
}

// The secure string functions of the C library of Microsoft, as the
// renderer calls them. A text that does not fit is cut.
inline int _snprintf_s(char* buffer, size_t size, size_t, const char* format, ...) {
  va_list args;
  va_start(args, format);
  const int n = vsnprintf(buffer, size, format, args);
  va_end(args);
  return n < 0 || static_cast<size_t>(n) >= size ? -1 : n;
}
template <size_t N>
inline int sprintf_s(char (&buffer)[N], const char* format, ...) {
  va_list args;
  va_start(args, format);
  const int n = vsnprintf(buffer, N, format, args);
  va_end(args);
  return n;
}
template <size_t N>
inline int strcat_s(char (&buffer)[N], const char* text) {
  const size_t have = strlen(buffer);
  snprintf(buffer + have, N - have, "%s", text);
  return 0;
}
template <size_t N>
inline int strncpy_s(char (&buffer)[N], const char* text, size_t) {
  snprintf(buffer, N, "%s", text);
  return 0;
}
// The renderer joins a folder and a name with a backslash, as Windows does.
// Android takes a backslash as a part of the name, so each one becomes a
// slash.
inline int fopen_s(FILE** file, const char* path, const char* mode) {
  char posix[1024];
  snprintf(posix, sizeof(posix), "%s", path);
  for (char* p = posix; *p != 0; ++p) {
    if (*p == '\\') *p = '/';
  }
  *file = fopen(posix, mode);
  return *file != nullptr ? 0 : 1;
}
inline void* _aligned_malloc(size_t size, size_t alignment) {
  void* p = nullptr;
  return posix_memalign(&p, alignment, size) == 0 ? p : nullptr;
}
inline void _aligned_free(void* p) { free(p); }

// The folder for the pictures of a test (vk_scene_test_frame, F12). On the PC
// it is the folder of runblack.exe. The app gives its game folder.
void VkPlatformSetFolder(const char* folder);
const char* VkPlatformFolder();
// The angles of the camera of the game of the walk now
// (bw::tech::InfoTransform on the PC). The app gives the values of the
// stream.
void VkPlatformTransform(float* inv_half_tan_fov_x, int32_t* res_x,
                         int32_t* res_y);
inline HMODULE GetModuleHandleA(const char*) { return nullptr; }
inline DWORD GetModuleFileNameA(HMODULE, char* path, DWORD size) {
  // The renderer cuts the name after the last backslash.
  snprintf(path, size, "%s\\bwvr", VkPlatformFolder());
  return static_cast<DWORD>(strlen(path));
}
// A key of the ini file at path (the renderer reads bwvr.ini of the game
// folder for its own keys). fallback when the file or the key is not there.
inline unsigned GetPrivateProfileIntA(const char* section, const char* key,
                                      int fallback, const char* path) {
  FILE* file = nullptr;
  if (fopen_s(&file, path, "rb") != 0) return static_cast<unsigned>(fallback);
  std::string text;
  char part[4096];
  size_t n = 0;
  while ((n = fread(part, 1, sizeof(part), file)) > 0) text.append(part, n);
  fclose(file);
  bwvr_ini::IniText ini;
  ini.Parse(text);
  return static_cast<unsigned>(ini.GetInt(section, key, fallback));
}

// The loader of Vulkan. The renderer asks for vulkan-1.dll, the name of
// Windows. Android has libvulkan.so.
inline HMODULE LoadLibraryA(const char*) {
  return dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
}
inline FARPROC GetProcAddress(HMODULE module, const char* name) {
  return dlsym(module, name);
}
inline BOOL FreeLibrary(HMODULE module) { return dlclose(module) == 0; }
