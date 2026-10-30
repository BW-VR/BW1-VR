// THE PATH OF A FILE IN THE VIRTUALSTORE (save_store.cpp). Pure logic, so the
// checks in test/controller_math can run it.
//
// Windows virtualizes the files of a 32 bit program with no manifest that
// has no administrator rights. A write that a virtualized folder refuses
// goes to the VirtualStore of the user. The virtualized folders are Program
// Files, Program Files (x86), ProgramData and Windows. The copy of a file is
// at the path of the file with no drive, in the folder
// %LOCALAPPDATA%\VirtualStore:
//
//   C:\Program Files (x86)\Lionhead Studios Ltd\Black & White\Profiles\x
//   %LOCALAPPDATA%\VirtualStore\Program Files (x86)\Lionhead Studios Ltd\
//       Black & White\Profiles\x
#pragma once

#include <stddef.h>
#include <string.h>

namespace save_store {

// The errors of Windows after which the mod makes a call again in the
// VirtualStore: ERROR_FILE_NOT_FOUND, ERROR_PATH_NOT_FOUND and
// ERROR_ACCESS_DENIED.
inline bool RetryError(unsigned long error) {
  return error == 2 || error == 3 || error == 5;
}

inline char Lower(char c) {
  return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// True when `full` is in the folder `root`, with no difference between
// capital and small letters. A backslash at the end of `root` has no effect.
// The folder itself is not in the folder.
inline bool InFolder(const char* full, const char* root) {
  size_t n = strlen(root);
  while (n > 0 && root[n - 1] == '\\') --n;
  if (n == 0) return false;
  for (size_t i = 0; i < n; ++i) {
    if (full[i] == 0 || Lower(full[i]) != Lower(root[i])) return false;
  }
  return full[n] == '\\' && full[n + 1] != 0;
}

// The path of the VirtualStore copy of the file `full`. `full` is an
// absolute path with a drive ("C:\..."). `roots` are the virtualized
// folders, `store` is the VirtualStore folder of the user. True, with the
// path in `out`, when the file is in one of the roots and the path fits in
// `size` characters with its end zero.
inline bool StorePath(const char* full, const char* const* roots,
                      int root_count, const char* store, char* out,
                      size_t size) {
  if (full == nullptr || store == nullptr || out == nullptr || size == 0) {
    return false;
  }
  out[0] = 0;
  const bool drive = ((full[0] >= 'A' && full[0] <= 'Z') ||
                      (full[0] >= 'a' && full[0] <= 'z')) &&
                     full[1] == ':' && full[2] == '\\';
  if (!drive) return false;
  bool in = false;
  for (int i = 0; i < root_count && !in; ++i) {
    in = roots[i] != nullptr && InFolder(full, roots[i]);
  }
  if (!in) return false;
  size_t s = strlen(store);
  while (s > 0 && store[s - 1] == '\\') --s;
  if (s == 0) return false;
  const char* rest = full + 3;
  const size_t r = strlen(rest);
  if (s + 1 + r + 1 > size) return false;
  memcpy(out, store, s);
  out[s] = '\\';
  memcpy(out + s + 1, rest, r + 1);
  return true;
}

}  // namespace save_store
