// The LARGE_ADDRESS_AWARE flag of runblack.exe.
//
// runblack.exe has no LARGE_ADDRESS_AWARE flag in its PE header, so Windows
// gives the game 2 GB of address space. The game, the mod, the driver of the
// GPU and the OpenXR runtime share that space, and a large render can fill
// it. With the flag, a Windows of 64 bits gives the process 4 GB. The flag is
// bit 0x0020 of the Characteristics field of the file header. The checksum
// field of the file is 0, so the flag is the only byte that changes.
//
// The launcher sets the flag before it starts the game (large_address=1, the
// default) and clears it with large_address=0. Before the first change it
// keeps a copy of the plain file as runblack.exe.before-bwvr. After each
// change it reads the file again and compares the SHA-1 (exe_check.h). If the
// file is then not one of the two known files, it puts the copy back.
//
// The byte functions are pure. Checks in
// test/controller_math/large_address_checks.h.
#pragma once

#include "exe_check.h"

namespace bwvr {

constexpr uint16_t kLargeAddressAware = 0x0020;  // IMAGE_FILE_LARGE_ADDRESS_AWARE

// The name of the copy of the plain runblack.exe, in the game folder.
constexpr const wchar_t* kGameExeBackupName = L"runblack.exe.before-bwvr";

// The offset of the Characteristics field in the first `n` bytes of a file,
// or 0 when these bytes are not the start of a PE image for x86.
inline uint32_t CharacteristicsOffset(const uint8_t* head, uint32_t n) {
  if (n < 0x40 || head[0] != 'M' || head[1] != 'Z') return 0;
  const uint32_t pe = static_cast<uint32_t>(head[0x3C]) |
                      (static_cast<uint32_t>(head[0x3D]) << 8) |
                      (static_cast<uint32_t>(head[0x3E]) << 16) |
                      (static_cast<uint32_t>(head[0x3F]) << 24);
  if (pe < 0x40 || pe > n || n - pe < 24) return 0;
  if (head[pe] != 'P' || head[pe + 1] != 'E' || head[pe + 2] != 0 ||
      head[pe + 3] != 0) {
    return 0;
  }
  const uint32_t machine = head[pe + 4] | (head[pe + 5] << 8);
  if (machine != 0x014C) return 0;  // IMAGE_FILE_MACHINE_I386
  return pe + 22;
}

// Sets or clears the flag in the first bytes of a file. False when these
// bytes are not the start of a PE image for x86.
inline bool SetLargeAddressInHead(uint8_t* head, uint32_t n, bool on) {
  const uint32_t at = CharacteristicsOffset(head, n);
  if (at == 0) return false;
  uint16_t value = static_cast<uint16_t>(head[at] | (head[at + 1] << 8));
  value = on ? static_cast<uint16_t>(value | kLargeAddressAware)
             : static_cast<uint16_t>(value & ~kLargeAddressAware);
  head[at] = static_cast<uint8_t>(value & 0xFF);
  head[at + 1] = static_cast<uint8_t>(value >> 8);
  return true;
}

// Sets or clears the flag in the file at `path`, in place. It writes only the
// two bytes of the field. A running game holds its file, so then the open
// fails with ERROR_SHARING_VIOLATION.
inline bool WriteLargeAddressFlag(const wchar_t* path, bool on, DWORD* error) {
  *error = 0;
  HANDLE file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    *error = GetLastError();
    return false;
  }
  uint8_t head[4096];
  DWORD got = 0;
  bool ok = ReadFile(file, head, sizeof(head), &got, nullptr) != 0;
  const uint32_t at = ok ? CharacteristicsOffset(head, got) : 0;
  if (at == 0 || !SetLargeAddressInHead(head, got, on)) {
    *error = ok ? ERROR_BAD_EXE_FORMAT : GetLastError();
    CloseHandle(file);
    return false;
  }
  LARGE_INTEGER offset = {};
  offset.QuadPart = at;
  DWORD put = 0;
  ok = SetFilePointerEx(file, offset, nullptr, FILE_BEGIN) &&
       WriteFile(file, head + at, 2, &put, nullptr) && put == 2 &&
       FlushFileBuffers(file);
  if (!ok) *error = GetLastError();
  CloseHandle(file);
  return ok;
}

enum class LargeAddressResult {
  kAlready,   // the file was as asked
  kChanged,   // the flag changed, and the SHA-1 is right
  kNotV142,   // not the v1.42 file: nothing changed
  kNoRights,  // Windows refused the write
  kInUse,     // the game runs and holds the file
  kFailed,    // a different fault. The copy went back if it was needed
};

struct LargeAddressReport {
  LargeAddressResult result = LargeAddressResult::kFailed;
  bool on_now = false;  // the flag of the file after the call
  DWORD error = 0;
};

// Makes runblack.exe at `exe` have the flag (`want`) or not. `backup` is the
// path of the copy of the plain file.
inline LargeAddressReport ApplyLargeAddress(const wchar_t* exe,
                                            const wchar_t* backup,
                                            bool want) {
  LargeAddressReport out;
  const ExeReport before = CheckGameExe(exe);
  out.on_now = before.large_address;
  if (before.verdict != ExeVerdict::kSupported) {
    out.result = LargeAddressResult::kNotV142;
    out.error = before.error;
    return out;
  }
  if (before.large_address == want) {
    out.result = LargeAddressResult::kAlready;
    return out;
  }

  // The copy of the plain file, one time. The file is plain now if the flag
  // is wanted.
  const ExeReport saved = CheckGameExe(backup);
  const bool backup_ok =
      saved.verdict == ExeVerdict::kSupported && !saved.large_address;
  if (!backup_ok && want && !CopyFileW(exe, backup, FALSE)) {
    out.error = GetLastError();
    out.result = out.error == ERROR_ACCESS_DENIED ? LargeAddressResult::kNoRights
                                                  : LargeAddressResult::kFailed;
    return out;
  }

  DWORD error = 0;
  if (!WriteLargeAddressFlag(exe, want, &error)) {
    out.error = error;
    out.result = error == ERROR_ACCESS_DENIED ? LargeAddressResult::kNoRights
                 : error == ERROR_SHARING_VIOLATION ? LargeAddressResult::kInUse
                                                    : LargeAddressResult::kFailed;
    return out;
  }

  const ExeReport after = CheckGameExe(exe);
  if (after.verdict == ExeVerdict::kSupported && after.large_address == want) {
    out.on_now = want;
    out.result = LargeAddressResult::kChanged;
    return out;
  }
  // The file is not a known file now. The copy of the plain file goes back.
  out.result = LargeAddressResult::kFailed;
  const ExeReport saved_now = CheckGameExe(backup);
  if (saved_now.verdict == ExeVerdict::kSupported &&
      !saved_now.large_address && CopyFileW(backup, exe, FALSE)) {
    out.on_now = false;
  } else {
    out.error = GetLastError();
  }
  return out;
}

}  // namespace bwvr
