// Tells if runblack.exe is the one build that the mod supports.
//
// Every address in bw_addresses.h is correct for one file only: runblack.exe
// from the unofficial v1.42 patch. The image base cannot tell the builds
// apart, because every Black & White executable loads at 0x00400000. So this
// reads the file from the disk and compares its SHA-1 with the known value.
//
// The mod and the launcher both use this file, so they always agree. The
// installer compares the same value (see installer/bwvr.iss).
//
// No DLL is loaded here, so the mod can call this from DllMain.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "exe_sha1.h"

namespace bwvr {

enum class ExeVerdict {
  kSupported,   // the v1.42 file, byte for byte
  kOtherBuild,  // a different file: do not install hooks
  kUnreadable,  // the file cannot be read: do not install hooks
};

struct ExeReport {
  ExeVerdict verdict = ExeVerdict::kUnreadable;
  uint64_t size = 0;
  char sha1[41] = {0};         // lower case hex, empty if unreadable
  const char* build = nullptr;  // a name from kKnownBuilds, or null
  DWORD error = 0;              // GetLastError if unreadable
};

// Reads the whole file once. About 10 to 40 ms for 8.6 MB.
inline ExeReport CheckGameExe(const wchar_t* path) {
  ExeReport report;
  HANDLE file = CreateFileW(
      path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    report.error = GetLastError();
    return report;
  }
  LARGE_INTEGER size = {};
  GetFileSizeEx(file, &size);
  report.size = static_cast<uint64_t>(size.QuadPart);

  const DWORD kChunk = 64 * 1024;
  uint8_t* buffer =
      static_cast<uint8_t*>(HeapAlloc(GetProcessHeap(), 0, kChunk));
  if (buffer == nullptr) {
    report.error = ERROR_NOT_ENOUGH_MEMORY;
    CloseHandle(file);
    return report;
  }
  detail::Sha1 sha;
  detail::Sha1Start(&sha);
  bool ok = true;
  for (;;) {
    DWORD got = 0;
    if (!ReadFile(file, buffer, kChunk, &got, nullptr)) {
      report.error = GetLastError();
      ok = false;
      break;
    }
    if (got == 0) break;
    detail::Sha1Add(&sha, buffer, got);
  }
  HeapFree(GetProcessHeap(), 0, buffer);
  CloseHandle(file);
  if (!ok) return report;

  detail::Sha1Finish(&sha, report.sha1);
  report.build = KnownBuildName(report.sha1);
  report.verdict = (report.size == kGameExeSize &&
                    strcmp(report.sha1, kGameExeSha1) == 0)
                       ? ExeVerdict::kSupported
                       : ExeVerdict::kOtherBuild;
  return report;
}

}  // namespace bwvr
