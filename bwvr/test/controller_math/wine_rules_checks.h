// The rules of Wine in the headset app, a test (android/src/wine_rules.h):
// the DLLs that stay out of the tree, the paths, the build id, the
// environment, the seed of user.reg and the tar reader. main.cpp includes
// this file after Check and Near, just before main.
#pragma once

#include <string.h>

#include <string>
#include <vector>

#include "../../android/src/wine_rules.h"

namespace wine_rules_checks {

inline bool HasEntry(const std::vector<std::string>& env, const std::string& entry) {
  for (const std::string& e : env) {
    if (e == entry) return true;
  }
  return false;
}

inline size_t CountKey(const std::vector<std::string>& env, const std::string& key) {
  size_t n = 0;
  for (const std::string& e : env) {
    if (e.compare(0, key.size() + 1, key + "=") == 0) ++n;
  }
  return n;
}

inline void Put(uint8_t* at, const char* text, size_t most) {
  const size_t n = strlen(text);
  memcpy(at, text, n < most ? n : most);
}

// A ustar header as GNU tar writes it, with a right checksum.
inline void MakeHeader(uint8_t* h, const char* name, char type, unsigned mode,
                       unsigned long long size, const char* link, const char* prefix) {
  memset(h, 0, 512);
  Put(h, name, 100);
  snprintf(reinterpret_cast<char*>(h + 100), 8, "%07o", mode);
  snprintf(reinterpret_cast<char*>(h + 108), 8, "%07o", 0u);
  snprintf(reinterpret_cast<char*>(h + 116), 8, "%07o", 0u);
  snprintf(reinterpret_cast<char*>(h + 124), 12, "%011llo", size);
  snprintf(reinterpret_cast<char*>(h + 136), 12, "%011o", 0u);
  h[156] = static_cast<uint8_t>(type);
  if (link != nullptr) Put(h + 157, link, 100);
  memcpy(h + 257, "ustar", 6);
  memcpy(h + 263, "00", 2);
  if (prefix != nullptr) Put(h + 345, prefix, 155);
  memset(h + 148, ' ', 8);
  unsigned sum = 0;
  for (int i = 0; i < 512; ++i) sum += h[i];
  snprintf(reinterpret_cast<char*>(h + 148), 8, "%06o", sum);
  h[155] = ' ';
}

inline void LeftOutChecks() {
  using wine_rules::IsLeftOut;
  printf("\nWine: the DLLs that stay out of the tree\n");
  Check(IsLeftOut("lib/wine/i386-windows/ddraw.dll"), "ddraw.dll of i386 stays out", 0, 0);
  Check(IsLeftOut("lib/wine/aarch64-windows/DDRAW.DLL"), "a name in capitals stays out too", 0, 0);
  Check(IsLeftOut("ddrawex.dll"), "ddrawex.dll stays out", 0, 0);
  Check(IsLeftOut("lib/wine/x86_64-windows/wined3d.dll"), "wined3d.dll stays out", 0, 0);
  Check(IsLeftOut("dxgi.dll"), "dxgi.dll stays out", 0, 0);
  Check(IsLeftOut("lib/wine/x86_64-unix/opengl32.so"), "the Unix library of opengl32 stays out",
        0, 0);
  Check(IsLeftOut("opengl32.dll"), "opengl32.dll stays out", 0, 0);
  Check(IsLeftOut("wineandroid.drv"), "the display driver of Android stays out", 0, 0);
  Check(IsLeftOut("lib/wine/i386-windows/vulkan-1.dll") &&
            IsLeftOut("lib/wine/aarch64-windows/vulkan-1.dll"),
        "vulkan-1.dll of each architecture stays out", 0, 0);
  Check(IsLeftOut("lib/wine/i386-windows/winevulkan.dll") &&
            IsLeftOut("lib/wine/x86_64-windows/winevulkan.dll") &&
            IsLeftOut("lib/wine/x86_64-unix/winevulkan.so"),
        "winevulkan of each architecture and its Unix library stay out", 0, 0);
  Check(!IsLeftOut("vulkan.dll") && !IsLeftOut("winevulkanx.dll"),
        "only the whole names vulkan-1 and winevulkan stay out", 0, 0);
  Check(IsLeftOut("d3d8.dll") && IsLeftOut("d3d9.dll") && IsLeftOut("d3d11.dll") &&
            IsLeftOut("d3d12core.dll") && IsLeftOut("d3dim700.dll") && IsLeftOut("d3drm.dll"),
        "each d3d DLL stays out", 0, 0);
  Check(IsLeftOut("d3dx9_43.dll") && IsLeftOut("d3dcompiler_47.dll"),
        "the d3dx and the d3dcompiler DLLs stay out", 0, 0);
  Check(!IsLeftOut("lib/wine/i386-windows/ntdll.dll"), "ntdll.dll stays in", 0, 0);
  Check(!IsLeftOut("gdi32.dll") && !IsLeftOut("user32.dll") && !IsLeftOut("win32u.so"),
        "gdi32, user32 and win32u stay in", 0, 0);
  Check(!IsLeftOut("dsound.dll") && !IsLeftOut("dinput.dll") && !IsLeftOut("winmm.dll"),
        "the sound and the input of the game stay in", 0, 0);
  Check(!IsLeftOut("wow64cpu.dll") && !IsLeftOut("wowbox64.dll"), "the CPU DLLs stay in", 0, 0);
  Check(!IsLeftOut("dxgkrnl.sys") && !IsLeftOut("dxdiagn.dll") && !IsLeftOut("dwrite.dll"),
        "a name that only starts like a left out name stays in", 0, 0);
  Check(!IsLeftOut("lib/wine/d3d9/readme"), "a folder name does not count", 0, 0);
  Check(!IsLeftOut("lib/d3d/ntdll.dll"), "only the file name counts, not its folder", 0, 0);
  Check(wine_rules::Stem("lib/wine/x86_64-windows/wineandroid.drv.so") == "wineandroid",
        "the stem ends at the first dot", 0, 0);
}

inline void PathChecks() {
  printf("\nWine: the paths and the CPU\n");
  const std::string files = "/data/user/0/org.bwvr.quest/files";
  const std::string app = "/storage/emulated/0/Android/data/org.bwvr.quest/files";
  const wine_rules::Paths a = wine_rules::MakePaths(files, app, "arm64-v8a");
  Check(a.tree == files + "/wine", "the tree is in the internal folder of the app", 0, 0);
  Check(a.loader == files + "/wine/arm64-v8a/lib/wine/aarch64-unix/wine",
        "the loader is in the folder of ntdll.so", 0, 0);
  Check(a.server == files + "/wine/arm64-v8a/bin/wineserver",
        "wineserver is in arm64-v8a/bin of the tree", 0, 0);
  Check(a.libs == files + "/wine/arm64-v8a/lib",
        "LD_LIBRARY_PATH is arm64-v8a/lib of the tree (libfreetype.so)", 0, 0);
  Check(a.dlls == files + "/wine/arm64-v8a/lib/wine", "the DLLs are in arm64-v8a/lib/wine", 0,
        0);
  Check(a.cpu_dll == files + "/wine/arm64-v8a/lib/wine/aarch64-windows/wowbox64.dll",
        "arm64: the CPU DLL is wowbox64.dll in aarch64-windows of the tree", 0, 0);
  Check(a.prefix == files + "/prefix" && a.user_reg == files + "/prefix/user.reg",
        "the prefix is in the internal folder of the app", 0, 0);
  Check(a.system32 == files + "/prefix/drive_c/windows/system32", "system32 of the prefix", 0,
        0);
  Check(a.game_link == files + "/prefix/dosdevices/g:", "the game folder is the drive G:", 0, 0);
  Check(a.log == app + "/wine.log", "wine.log is in the folder of the app beside its log", 0,
        0);
  const wine_rules::Paths x = wine_rules::MakePaths(files, app, "x86_64");
  Check(x.loader == files + "/wine/x86_64/lib/wine/x86_64-unix/wine" &&
            x.libs == files + "/wine/x86_64/lib",
        "x86_64: the loader and lib in x86_64 of the tree", 0, 0);
  Check(x.cpu_dll.empty(), "x86_64: the app copies no CPU DLL", 0, 0);
  Check(std::string(wine_rules::CpuDll("arm64-v8a")) == "wowbox64.dll" &&
            std::string(wine_rules::CpuDll("x86_64")) == "wow64cpu.dll",
        "the CPU DLL of each ABI", 0, 0);
  Check(wine_rules::CopiesCpuDll("arm64-v8a") && !wine_rules::CopiesCpuDll("x86_64"),
        "only wowbox64.dll goes into system32", 0, 0);
  Check(std::string(wine_rules::WineArch("arm64-v8a")) == "aarch64" &&
            std::string(wine_rules::WineArch("x86_64")) == "x86_64" &&
            std::string(wine_rules::WineArch("armeabi-v7a")).empty(),
        "the architecture of Wine for each ABI", 0, 0);
}

inline void IdChecks() {
  printf("\nWine: the build id of the tree\n");
  using wine_rules::NeedsUnpack;
  Check(NeedsUnpack("", "3f2a"), "no tree: unpack", 0, 0);
  Check(NeedsUnpack("1111", "3f2a"), "a tree of another build: unpack", 0, 0);
  Check(!NeedsUnpack("3f2a", "3f2a"), "the same build: no unpack", 0, 0);
  Check(!NeedsUnpack("3f2a\n", "  3f2a\r\n"), "the white space at the ends does not count", 0,
        0);
  Check(NeedsUnpack("  \n", "  \n"), "an empty id is no tree", 0, 0);

  printf("\nWine: the marker of the prefix\n");
  using wine_rules::NeedsCpuCopy;
  using wine_rules::NeedsPrefix;
  Check(NeedsPrefix("") && NeedsPrefix(" \r\n"),
        "no marker (a wineboot that stopped or failed): a new prefix", 0, 0);
  Check(!NeedsPrefix("3f2a\n"), "a marker: the prefix stays", 0, 0);
  Check(!NeedsPrefix("1111"), "a marker of another tree: the prefix stays too", 0, 0);
  Check(NeedsCpuCopy("arm64-v8a", "1111", "3f2a"),
        "arm64: a prefix of another tree gets the CPU DLL of this tree", 0, 0);
  Check(!NeedsCpuCopy("arm64-v8a", "3f2a\n", "3f2a"),
        "arm64: a prefix of this tree has its CPU DLL", 0, 0);
  Check(!NeedsCpuCopy("x86_64", "1111", "3f2a"),
        "x86_64: no CPU DLL to copy (wow64cpu.dll is a part of Wine)", 0, 0);
  const wine_rules::Paths p = wine_rules::MakePaths("/f", "/s", "arm64-v8a");
  Check(p.prefix_id == "/f/prefix/bwvr-prefix-id.txt", "the marker is in the prefix", 0, 0);
}

inline void EnvironmentChecks() {
  printf("\nWine: the environment of a program\n");
  const std::string files = "/data/user/0/org.bwvr.quest/files";
  const wine_rules::Paths p = wine_rules::MakePaths(files, "/sdcard/x", "x86_64");
  const std::vector<std::string> app = {
      "PATH=/product/bin:/apex/com.android.runtime/bin:/system/bin",
      "ANDROID_DATA=/data",
      "HOME=/",
      "WINEDEBUG=+all",
      "WINEPREFIX=/somewhere",
      "LD_PRELOAD=libsigchain.so",
      "LD_LIBRARY_PATH=/system/lib64",
      "TMPDIR=/data/local/tmp"};
  const std::vector<std::string> env = wine_rules::Environment(app, p, files, " -all ");
  Check(HasEntry(env, "PATH=/product/bin:/apex/com.android.runtime/bin:/system/bin") &&
            HasEntry(env, "ANDROID_DATA=/data"),
        "the other values of the app stay", 0, 0);
  Check(HasEntry(env, "WINEPREFIX=" + files + "/prefix") && CountKey(env, "WINEPREFIX") == 1,
        "WINEPREFIX is the prefix, one time", 0, 0);
  Check(HasEntry(env, "WINESERVER=" + files + "/wine/x86_64/bin/wineserver"), "WINESERVER", 0,
        0);
  Check(HasEntry(env, "LD_LIBRARY_PATH=" + files + "/wine/x86_64/lib") &&
            CountKey(env, "LD_LIBRARY_PATH") == 1,
        "LD_LIBRARY_PATH is lib of the tree, one time", 0, 0);
  Check(CountKey(env, "LD_PRELOAD") == 0, "no LD_PRELOAD of the app", 0, 0);
  Check(HasEntry(env, "HOME=" + files) && CountKey(env, "HOME") == 1, "HOME, one time", 0, 0);
  Check(HasEntry(env, "TMPDIR=" + files + "/tmp") && CountKey(env, "TMPDIR") == 1,
        "TMPDIR, one time", 0, 0);
  Check(HasEntry(env, "WINEDEBUG=-all") && CountKey(env, "WINEDEBUG") == 1,
        "WINEDEBUG is the trimmed wine_debug, one time", 0, 0);
  Check(HasEntry(env, "WINEDLLOVERRIDES=mscoree,mshtml="), "no Mono and no Gecko", 0, 0);
  const std::vector<std::string> plain = wine_rules::Environment(app, p, files, "");
  Check(CountKey(plain, "WINEDEBUG") == 0, "no wine_debug: no WINEDEBUG, the default of Wine",
        0, 0);
  Check(CountKey(plain, "WINELOADER") == 0 && CountKey(plain, "WINEDLLPATH") == 0,
        "no WINELOADER and no WINEDLLPATH: Wine finds them from ntdll.so", 0, 0);
  const std::string reg = wine_rules::UserRegSeed();
  Check(reg.compare(0, 24, "WINE REGISTRY Version 2\n") == 0, "user.reg starts with its head",
        0, 0);
  Check(reg.find("[Software\\\\Wine\\\\Drivers]\n\"Graphics\"=\"null\"\n") != std::string::npos,
        "user.reg: HKCU\\Software\\Wine\\Drivers, Graphics = null", 0, 0);
}

inline void TarChecks() {
  printf("\nWine: the tar reader\n");
  using wine_rules::TarEntry;
  uint8_t h[512];
  TarEntry e;
  MakeHeader(h, "bin/wine", '0', 0755, 12345, nullptr, nullptr);
  Check(wine_rules::ParseTarHeader(h, &e) && !e.end, "a header of a file reads", 0, 0);
  Check(e.name == "bin/wine" && e.type == '0' && e.mode == 0755, "its name, type and mode", 0,
        0);
  Near("its size", static_cast<double>(e.size), 12345.0, 0.0);
  Near("its data takes whole blocks", static_cast<double>(wine_rules::TarDataBytes(e)),
       12800.0, 0.0);
  MakeHeader(h, "x86_64-windows/ntdll.dll", '0', 0644, 512, nullptr, "lib/wine");
  Check(wine_rules::ParseTarHeader(h, &e) && e.name == "lib/wine/x86_64-windows/ntdll.dll",
        "a long name: the prefix and the name, joined", 0, 0);
  Near("a size of one block takes one block",
       static_cast<double>(wine_rules::TarDataBytes(e)), 512.0, 0.0);
  MakeHeader(h, "share/wine/", '5', 0755, 0, nullptr, nullptr);
  Check(wine_rules::ParseTarHeader(h, &e) && e.type == '5' && e.name == "share/wine",
        "a folder, with no slash at its end", 0, 0);
  Near("a folder has no data", static_cast<double>(wine_rules::TarDataBytes(e)), 0.0, 0.0);
  MakeHeader(h, "lib/wine/x86_64-unix/wine", '2', 0777, 0, "../../../bin/wine", nullptr);
  Check(wine_rules::ParseTarHeader(h, &e) && e.type == '2' && e.link == "../../../bin/wine",
        "a symbolic link and its target", 0, 0);
  MakeHeader(h, "bin/wine", '0', 0755, 1, nullptr, nullptr);
  h[0] ^= 1;
  Check(!wine_rules::ParseTarHeader(h, &e), "a bad checksum does not read", 0, 0);
  MakeHeader(h, "bin/wine", '0', 0755, 1, nullptr, nullptr);
  memcpy(h + 257, "xstar", 5);
  Check(!wine_rules::ParseTarHeader(h, &e), "a header that is not ustar does not read", 0, 0);
  memset(h, 0, sizeof(h));
  Check(wine_rules::ParseTarHeader(h, &e) && e.end, "a block of zeros is the end", 0, 0);

  using wine_rules::SafeName;
  Check(SafeName("bin/wine") && SafeName("lib/wine/i386-windows/ntdll.dll"),
        "a relative name is safe", 0, 0);
  Check(!SafeName("/system/bin/sh") && !SafeName("../files/x") && !SafeName("lib/../../x") &&
            !SafeName("") && !SafeName("lib//x"),
        "an absolute name, a .. and an empty part are not safe", 0, 0);
}

inline void WineRulesChecks() {
  LeftOutChecks();
  PathChecks();
  IdChecks();
  EnvironmentChecks();
  TarChecks();
}

}  // namespace wine_rules_checks
