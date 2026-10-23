// THE TEST PROGRAM OF WINE IN THE HEADSET APP (docs/android.md, "Wine in
// the app"). A 32 bit console program with no game code. The app starts it
// in Wine with wine_probe=1. It writes what it sees into
// bwvr-wine-probe.txt beside itself, one "key=value" in each line:
//
//   windows      the Windows version of RtlGetVersion
//   wine         the version of Wine (wine_get_version of ntdll)
//   process      the machine of the process (IsWow64Process2): i386
//   native       the machine of the system: x86_64 or arm64
//   cpu_dll      HKLM\Software\Microsoft\Wow64\x86, the 64 bit view
//   graphics     HKCU\Software\Wine\Drivers, Graphics
//   loop_ms      the time of a fixed loop of 100000000 steps
//   drive        the drive of this program, and if a file of 64 KB on it
//                writes, reads back with the same bytes and deletes
//   system_<dll> what LoadLibraryEx finds in the system folder for ddraw,
//                d3d9, wined3d, dxgi, opengl32 and vulkan-1: "none" with
//                the error, or "loaded"
//   font_*       the face, the extent of a text and the bytes of the
//                outline of a glyph of the font "Tahoma" (FreeType)
//   result       "ok" at the end of the file
//
// The exit code is 0 when the file is written, else 1.
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {

char g_text[8192];
size_t g_used = 0;

void Line(const char* format, ...) {
  va_list args;
  va_start(args, format);
  const int n = _vsnprintf_s(g_text + g_used, sizeof(g_text) - g_used, _TRUNCATE,
                             format, args);
  va_end(args);
  if (n > 0) g_used += static_cast<size_t>(n);
  if (g_used + 2 < sizeof(g_text)) {
    g_text[g_used++] = '\r';
    g_text[g_used++] = '\n';
    g_text[g_used] = 0;
  }
}

const char* MachineName(USHORT m) {
  switch (m) {
    case IMAGE_FILE_MACHINE_UNKNOWN: return "unknown";
    case IMAGE_FILE_MACHINE_I386: return "i386";
    case IMAGE_FILE_MACHINE_AMD64: return "x86_64";
    case 0xAA64: return "arm64";
    case IMAGE_FILE_MACHINE_ARMNT: return "arm";
    default: return "other";
  }
}

void Version() {
  typedef LONG(WINAPI * GetVersionFn)(OSVERSIONINFOW*);
  typedef const char*(CDECL * WineVersionFn)(void);
  HMODULE ntdll = GetModuleHandleA("ntdll.dll");
  GetVersionFn get = reinterpret_cast<GetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
  OSVERSIONINFOW v = {};
  v.dwOSVersionInfoSize = sizeof(v);
  if (get != nullptr && get(&v) == 0) {
    Line("windows=%lu.%lu.%lu", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
  } else {
    Line("windows=unknown");
  }
  WineVersionFn wine =
      reinterpret_cast<WineVersionFn>(GetProcAddress(ntdll, "wine_get_version"));
  Line("wine=%s", wine != nullptr ? wine() : "none");
}

void Machines() {
  typedef BOOL(WINAPI * Wow64Fn)(HANDLE, USHORT*, USHORT*);
  Wow64Fn fn = reinterpret_cast<Wow64Fn>(
      GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsWow64Process2"));
  USHORT process = 0, native = 0;
  if (fn == nullptr || !fn(GetCurrentProcess(), &process, &native)) {
    Line("process=unknown");
    Line("native=unknown");
    return;
  }
  // IsWow64Process2 gives IMAGE_FILE_MACHINE_UNKNOWN for a process that is
  // not in WoW64.
  Line("process=%s", process == IMAGE_FILE_MACHINE_UNKNOWN ? MachineName(native)
                                                         : MachineName(process));
  Line("native=%s", MachineName(native));
}

void RegText(HKEY root, const char* key, const char* name, const char* out) {
  char value[260] = {};
  DWORD size = sizeof(value) - 1, type = 0;
  HKEY k = nullptr;
  if (RegOpenKeyExA(root, key, 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) {
    Line("%s=none", out);
    return;
  }
  const LONG r = RegQueryValueExA(k, name, nullptr, &type, reinterpret_cast<BYTE*>(value), &size);
  RegCloseKey(k);
  Line("%s=%s", out, r == ERROR_SUCCESS && type == REG_SZ ? value : "none");
}

void Loop() {
  LARGE_INTEGER f, a, b;
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&a);
  volatile unsigned int x = 1;
  unsigned int y = x;
  for (int i = 0; i < 100000000; ++i) y = y * 1664525u + 1013904223u;
  x = y;
  QueryPerformanceCounter(&b);
  Line("loop_ms=%.1f", (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart);
  Line("loop_value=%08x", static_cast<unsigned int>(x));
}

void Drive(const char* folder) {
  Line("drive=%c:", folder[0]);
  char path[MAX_PATH];
  sprintf_s(path, "%sbwvr-wine-probe.tmp", folder);
  static unsigned char out[65536], in[65536];
  for (size_t i = 0; i < sizeof(out); ++i) out[i] = static_cast<unsigned char>(i * 7 + 3);
  bool wrote = false, read = false, same = false, deleted = false;
  HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (h != INVALID_HANDLE_VALUE) {
    DWORD n = 0;
    wrote = WriteFile(h, out, sizeof(out), &n, nullptr) && n == sizeof(out);
    CloseHandle(h);
  }
  h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (h != INVALID_HANDLE_VALUE) {
    DWORD n = 0;
    read = ReadFile(h, in, sizeof(in), &n, nullptr) && n == sizeof(in);
    CloseHandle(h);
    same = read && memcmp(in, out, sizeof(in)) == 0;
  }
  deleted = DeleteFileA(path) != 0;
  Line("drive_write=%d", wrote ? 1 : 0);
  Line("drive_read=%d", same ? 1 : 0);
  Line("drive_delete=%d", deleted ? 1 : 0);
}

void SystemDlls() {
  static const char* const kNames[] = {"ddraw", "d3d9",     "wined3d",
                                       "dxgi",  "opengl32", "vulkan-1"};
  for (const char* name : kNames) {
    char file[64];
    sprintf_s(file, "%s.dll", name);
    // Only the system folder: the folder of this program can hold the
    // ddraw.dll of the mod.
    HMODULE m = LoadLibraryExA(file, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (m != nullptr) {
      Line("system_%s=loaded", name);
      FreeLibrary(m);
    } else {
      Line("system_%s=none %lu", name, GetLastError());
    }
  }
}

void Font() {
  HDC dc = CreateCompatibleDC(nullptr);
  if (dc == nullptr) {
    Line("font_face=no dc");
    return;
  }
  HFONT font = CreateFontA(-32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                           DEFAULT_PITCH, "Tahoma");
  HGDIOBJ old = SelectObject(dc, font);
  char face[LF_FACESIZE] = {};
  GetTextFaceA(dc, sizeof(face), face);
  Line("font_face=%s", face);
  SIZE size = {};
  const char text[] = "Black & White";
  if (GetTextExtentPoint32A(dc, text, static_cast<int>(strlen(text)), &size)) {
    Line("font_extent=%ldx%ld", size.cx, size.cy);
  } else {
    Line("font_extent=none");
  }
  TEXTMETRICA tm = {};
  GetTextMetricsA(dc, &tm);
  Line("font_truetype=%d", (tm.tmPitchAndFamily & TMPF_TRUETYPE) ? 1 : 0);
  GLYPHMETRICS gm = {};
  const MAT2 identity = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
  const DWORD bytes = GetGlyphOutlineA(dc, 'A', GGO_NATIVE, &gm, 0, nullptr, &identity);
  Line("font_glyph_bytes=%ld", bytes == GDI_ERROR ? -1L : static_cast<long>(bytes));
  SelectObject(dc, old);
  DeleteObject(font);
  DeleteDC(dc);
}

}  // namespace

int main() {
  char exe[MAX_PATH] = {};
  GetModuleFileNameA(nullptr, exe, MAX_PATH);
  char folder[MAX_PATH];
  strcpy_s(folder, exe);
  char* slash = strrchr(folder, '\\');
  if (slash != nullptr) slash[1] = 0;
  Line("program=%s", exe);
  Version();
  Machines();
  RegText(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Wow64\\x86", "", "cpu_dll");
  RegText(HKEY_CURRENT_USER, "Software\\Wine\\Drivers", "Graphics", "graphics");
  Loop();
  Drive(folder);
  SystemDlls();
  Font();
  Line("result=ok");

  fputs(g_text, stdout);
  fflush(stdout);
  char path[MAX_PATH];
  sprintf_s(path, "%sbwvr-wine-probe.txt", folder);
  HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return 1;
  DWORD n = 0;
  const BOOL ok = WriteFile(h, g_text, static_cast<DWORD>(g_used), &n, nullptr);
  CloseHandle(h);
  return ok && n == g_used ? 0 : 1;
}
