// THE SAVE IN THE VIRTUALSTORE (save_virtual_store, 1 is the default, 0 is
// the old behavior).
//
// The game is a 32 bit program with no manifest. When it has no
// administrator rights, Windows virtualizes its files in Program Files: a
// write that the folder refuses goes to the VirtualStore of the user
// (save_store_math.h). The profile of the player is then in the
// VirtualStore, and two calls of each save fail (backend.md, "A save in the
// VirtualStore"):
//
//   - MoveFileA of "<slot>.sav.tmp" to "<slot>.sav" fails with error 3
//     (ERROR_PATH_NOT_FOUND) when the folder of the slot is only in the
//     VirtualStore. The same for "Script_<slot>.sav.tmp".
//   - CreateFileA of "SaveGame.inf" fails with error 5 (ERROR_ACCESS_DENIED).
//     Windows does not virtualize a file with the extension .inf, so the game
//     cannot write it or read it in Program Files. The Save Game Room shows
//     a slot only when it can read "<slot>.sav" and "SaveGame.inf".
//
// LHOSFile is the class of the game for these files. The mod puts its own
// functions in four calls of LHOSFile to Windows. The instructions read the
// address of the function from the import table of the game, and the mod
// changes only the address in each instruction to a slot of its own, as
// file_read_ahead.cpp does:
//
//   0x007BC6CA  LHOSFile::Exists  mov esi, [GetFileAttributesA]
//   0x007BC79B  LHOSFile::Open    call [CreateFileA]
//   0x007BCC45  LHOSFile::Delete  call [DeleteFileA]
//   0x007BCC2A  LHOSFile::Rename  call [MoveFileA]
//
// Each function makes the call of the game first. Only when Windows refuses
// it (error 2, 3 or 5), the game process is virtualized and the file is in a
// virtualized folder, the mod makes the same call on the path of the
// VirtualStore copy of the file. That path is a usual folder of the user, so
// the call works there. The file then goes where Windows puts each other file
// of the save, and the game reads it from there. A call that Windows does
// not refuse stays as it is. A process with administrator rights is not
// virtualized, and the mod changes none of its calls.
//
// Each function calls Windows through the import table of the game, so a
// hook of another module on that table stays in the chain.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "save_store_math.h"

void ModLog(const char* format, ...);

namespace {

typedef DWORD(WINAPI* GetFileAttributesAFn)(LPCSTR);
typedef HANDLE(WINAPI* CreateFileAFn)(LPCSTR, DWORD, DWORD,
                                      LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                      HANDLE);
typedef BOOL(WINAPI* DeleteFileAFn)(LPCSTR);
typedef BOOL(WINAPI* MoveFileAFn)(LPCSTR, LPCSTR);

// The import table of runblack.exe. Each function reads its entry at each
// call.
GetFileAttributesAFn* g_attributes =
    reinterpret_cast<GetFileAttributesAFn*>(0x008A91C0);
CreateFileAFn* g_create = reinterpret_cast<CreateFileAFn*>(0x008A921C);
DeleteFileAFn* g_delete = reinterpret_cast<DeleteFileAFn*>(0x008A9228);
MoveFileAFn* g_move = reinterpret_cast<MoveFileAFn*>(0x008A915C);

bool g_on = false;
// Windows virtualizes the files of this process (TokenVirtualizationEnabled).
bool g_virtualized = false;
// The VirtualStore of the user and the virtualized folders.
char g_store[MAX_PATH] = {};
const int kMaxRoots = 5;
char g_roots[kMaxRoots][MAX_PATH] = {};
int g_root_count = 0;

// The log gets one line for each call that the mod makes again, to a limit.
// A save has three such calls, and the Save Game Room reads one
// "SaveGame.inf" for each slot.
LONG g_lines = 0;
const LONG kMaxLines = 60;

bool Room() {
  const LONG n = InterlockedIncrement(&g_lines);
  if (n == kMaxLines + 1) {
    ModLog("save store   : the log has %ld lines about calls in the "
           "VirtualStore, and it gets no more. The mod makes the calls all the "
           "same", kMaxLines);
  }
  return n <= kMaxLines;
}

// The path of the VirtualStore copy of the file `name`. False when the mod
// must not make the call again: the key is 0, the process is not
// virtualized, or the file is not in a virtualized folder.
bool Store(const char* name, char* out) {
  if (!g_on || !g_virtualized || name == nullptr) return false;
  char full[MAX_PATH];
  const DWORD n = GetFullPathNameA(name, MAX_PATH, full, nullptr);
  if (n == 0 || n >= MAX_PATH) return false;
  const char* roots[kMaxRoots];
  for (int i = 0; i < g_root_count; ++i) roots[i] = g_roots[i];
  return save_store::StorePath(full, roots, g_root_count, g_store, out,
                               MAX_PATH);
}

// LHOSFile::Exists asks for the attributes of a file. The game reads a
// "SaveGame.inf" only when Exists finds it.
DWORD WINAPI h_GetFileAttributesA(LPCSTR name) {
  const DWORD attributes = (*g_attributes)(name);
  if (attributes != INVALID_FILE_ATTRIBUTES) return attributes;
  const DWORD error = GetLastError();
  char store[MAX_PATH];
  if (!save_store::RetryError(error) || !Store(name, store)) {
    SetLastError(error);
    return attributes;
  }
  const DWORD again = (*g_attributes)(store);
  if (again == INVALID_FILE_ATTRIBUTES) {
    SetLastError(error);
    return attributes;
  }
  return again;
}

const char* Creation(DWORD creation) {
  switch (creation) {
    case CREATE_NEW: return "CREATE_NEW";
    case CREATE_ALWAYS: return "CREATE_ALWAYS";
    case OPEN_EXISTING: return "OPEN_EXISTING";
    case OPEN_ALWAYS: return "OPEN_ALWAYS";
    case TRUNCATE_EXISTING: return "TRUNCATE_EXISTING";
  }
  return "another mode";
}

// LHOSFile::Open: the game writes a file with CREATE_ALWAYS or
// TRUNCATE_EXISTING, and reads one with OPEN_ALWAYS or OPEN_EXISTING.
HANDLE WINAPI h_CreateFileA(LPCSTR name, DWORD access, DWORD share,
                            LPSECURITY_ATTRIBUTES security, DWORD creation,
                            DWORD flags, HANDLE model) {
  const HANDLE file =
      (*g_create)(name, access, share, security, creation, flags, model);
  if (file != INVALID_HANDLE_VALUE) return file;
  const DWORD error = GetLastError();
  char store[MAX_PATH];
  if (!save_store::RetryError(error) || !Store(name, store)) {
    SetLastError(error);
    return file;
  }
  const HANDLE again =
      (*g_create)(store, access, share, security, creation, flags, model);
  const DWORD error_again = GetLastError();
  const bool worked = again != INVALID_HANDLE_VALUE;
  if (Room()) {
    ModLog("save store   : Windows refused CreateFileA(%s, %s) with error "
           "%lu. The call on the VirtualStore copy %s", name,
           Creation(creation), error,
           worked ? "worked" : "failed too");
    if (!worked) {
      ModLog("save store   : the VirtualStore copy is %s, error %lu", store,
             error_again);
    }
  }
  if (worked) return again;
  SetLastError(error);
  return file;
}

// LHOSFile::Delete: a save deletes the old "<slot>.sav" before the rename.
BOOL WINAPI h_DeleteFileA(LPCSTR name) {
  const BOOL deleted = (*g_delete)(name);
  if (deleted) return deleted;
  const DWORD error = GetLastError();
  char store[MAX_PATH];
  if (!save_store::RetryError(error) || !Store(name, store) ||
      (*g_attributes)(store) == INVALID_FILE_ATTRIBUTES) {
    SetLastError(error);
    return deleted;
  }
  const BOOL again = (*g_delete)(store);
  const DWORD error_again = GetLastError();
  if (Room()) {
    ModLog("save store   : Windows refused DeleteFileA(%s) with error %lu. "
           "The delete of the VirtualStore copy %s (error %lu)", name, error,
           again ? "worked" : "failed too", again ? 0ul : error_again);
  }
  if (again) return again;
  SetLastError(error);
  return deleted;
}

// LHOSFile::Rename: a save renames "<slot>.sav.tmp" to "<slot>.sav", and
// "Script_<slot>.sav.tmp" to "Script_<slot>.sav".
BOOL WINAPI h_MoveFileA(LPCSTR from, LPCSTR to) {
  const BOOL moved = (*g_move)(from, to);
  if (moved) return moved;
  const DWORD error = GetLastError();
  char store_from[MAX_PATH];
  char store_to[MAX_PATH];
  if (!save_store::RetryError(error) || !Store(from, store_from) ||
      !Store(to, store_to)) {
    SetLastError(error);
    return moved;
  }
  const BOOL again = (*g_move)(store_from, store_to);
  const DWORD error_again = GetLastError();
  if (Room()) {
    ModLog("save store   : Windows refused MoveFileA(%s, %s) with error %lu. "
           "The rename in the VirtualStore %s (error %lu)", from, to, error,
           again ? "worked" : "failed too", again ? 0ul : error_again);
  }
  if (again) return again;
  SetLastError(error);
  return moved;
}

struct Site {
  uintptr_t at;       // the instruction
  uint8_t code[2];    // its opcode and its ModRM byte
  uintptr_t import;   // the address that it reads
  void** slot;        // the slot of the mod that it reads after the change
};

void* g_slot_attributes = reinterpret_cast<void*>(&h_GetFileAttributesA);
void* g_slot_create = reinterpret_cast<void*>(&h_CreateFileA);
void* g_slot_delete = reinterpret_cast<void*>(&h_DeleteFileA);
void* g_slot_move = reinterpret_cast<void*>(&h_MoveFileA);

const Site kSites[] = {
    {0x007BC6CA, {0x8B, 0x35}, 0x008A91C0, &g_slot_attributes},
    {0x007BC79B, {0xFF, 0x15}, 0x008A921C, &g_slot_create},
    {0x007BCC45, {0xFF, 0x15}, 0x008A9228, &g_slot_delete},
    {0x007BCC2A, {0xFF, 0x15}, 0x008A915C, &g_slot_move},
};
const int kSiteCount = sizeof(kSites) / sizeof(kSites[0]);

// True when the instruction reads the address `import`.
bool SiteIs(const Site& s) {
  const uint8_t* code = reinterpret_cast<const uint8_t*>(s.at);
  uint32_t address = 0;
  memcpy(&address, code + 2, 4);
  return code[0] == s.code[0] && code[1] == s.code[1] && address == s.import;
}

void WriteAddress(const Site& s, uintptr_t address) {
  uint8_t* code = reinterpret_cast<uint8_t*>(s.at);
  DWORD old = 0;
  VirtualProtect(code + 2, 4, PAGE_EXECUTE_READWRITE, &old);
  const uint32_t value = static_cast<uint32_t>(address);
  memcpy(code + 2, &value, 4);
  VirtualProtect(code + 2, 4, old, &old);
  FlushInstructionCache(GetCurrentProcess(), code, 6);
}

// The VirtualStore folder of the user, the virtualized folders, and whether
// Windows virtualizes this process.
void Prepare() {
  HANDLE token = nullptr;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    DWORD virtualized = 0;
    DWORD got = 0;
    if (GetTokenInformation(token, TokenVirtualizationEnabled, &virtualized,
                            sizeof(virtualized), &got)) {
      g_virtualized = virtualized != 0;
    }
    CloseHandle(token);
  }
  char local[MAX_PATH];
  const DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", local, MAX_PATH);
  if (n > 0 && n + 14 < MAX_PATH) {
    lstrcpynA(g_store, local, MAX_PATH);
    lstrcatA(g_store, "\\VirtualStore");
  } else {
    g_virtualized = false;
  }
  // In a 32 bit process ProgramFiles is the folder Program Files (x86), and
  // ProgramW6432 is the folder Program Files.
  const char* names[kMaxRoots] = {"ProgramFiles", "ProgramW6432",
                                  "ProgramFiles(x86)", "ProgramData",
                                  "SystemRoot"};
  g_root_count = 0;
  for (int i = 0; i < kMaxRoots; ++i) {
    const DWORD len =
        GetEnvironmentVariableA(names[i], g_roots[g_root_count], MAX_PATH);
    if (len > 0 && len < MAX_PATH) ++g_root_count;
  }
}

}  // namespace

// Puts the functions of the mod in the four calls of LHOSFile. on is the
// value of save_virtual_store. Call it before the game opens a file, from
// the start of the DLL.
void SaveStoreInstall(bool on) {
  if (!on) {
    ModLog("save store   : save_virtual_store=0. Each call of LHOSFile goes "
           "to Windows as the game makes it");
    return;
  }
  for (int i = 0; i < kSiteCount; ++i) {
    if (!SiteIs(kSites[i])) {
      ModLog("save store   : the code of LHOSFile at 0x%08X is not the code "
             "that the mod knows. Each call of LHOSFile goes to Windows as "
             "the game makes it", static_cast<unsigned>(kSites[i].at));
      return;
    }
  }
  Prepare();
  g_on = true;
  for (int i = 0; i < kSiteCount; ++i) {
    WriteAddress(kSites[i], reinterpret_cast<uintptr_t>(kSites[i].slot));
  }
  if (g_virtualized) {
    ModLog("save store   : save_virtual_store=1. Windows virtualizes the "
           "files of the game process. When Windows refuses a call of "
           "LHOSFile to a file in Program Files, ProgramData or Windows, the "
           "mod makes the call again on the copy of the file in %s",
           g_store);
  } else {
    ModLog("save store   : save_virtual_store=1. Windows does not virtualize "
           "the files of the game process (it has administrator rights), so "
           "the mod makes no call again in the VirtualStore");
  }
}

