// THE RULES OF WINE IN THE HEADSET APP (docs/android.md, "Wine in the app").
// No Android code: the math test of the PC checks each rule
// (test/controller_math/wine_rules_checks.h).
//
// The APK carries one Wine tree for each ABI as a tar file
// (assets/wine/<abi>.tar) and its build id (assets/wine/<abi>.id). The app
// unpacks the tree of its own ABI into its internal folder, makes a Wine
// prefix there, and starts Windows programs in Wine as child processes.
//
// pack_wine.sh and check_apk.ps1 read kLeftOutNames and kLeftOutPrefix from
// this file, so the list of the DLLs that stay out of the tree has one
// source.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace wine_rules {

// THE DLLS THAT STAY OUT OF THE TREE ("Only Vulkan"): the mod is the
// DirectDraw of the game, and the app draws with Vulkan alone. So Wine gets
// no graphics DLL: no DirectDraw, no Direct3D, no OpenGL, no Vulkan of Wine
// and no display driver of Android. A name is the file name with no
// extension: each file of that name (the PE DLL of each architecture and
// its Unix library) stays out.
constexpr const char* kLeftOutNames[] = {"ddraw",    "ddrawex",    "wined3d",
                                         "dxgi",     "opengl32",   "vulkan-1",
                                         "winevulkan", "wineandroid"};
// And each file whose name starts with this: d3d8, d3d9, d3d10, d3d11,
// d3d12, d3dim, d3drm, d3dx9_*, d3dcompiler_* and the others.
constexpr const char kLeftOutPrefix[] = "d3d";

inline std::string Lower(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

// The file name of a path, with no folder and no extension. "x/d3d9.dll" is
// "d3d9", "wineandroid.drv" is "wineandroid", "opengl32.so" is "opengl32".
inline std::string Stem(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  const size_t dot = name.find('.');
  if (dot != std::string::npos) name.resize(dot);
  return Lower(name);
}

// True for a file that must not be in the tree.
inline bool IsLeftOut(const std::string& path) {
  const std::string stem = Stem(path);
  if (stem.empty()) return false;
  for (const char* name : kLeftOutNames) {
    if (stem == name) return true;
  }
  return stem.compare(0, sizeof(kLeftOutPrefix) - 1, kLeftOutPrefix) == 0;
}

// The name of the architecture of Wine for an ABI of Android, "" for an ABI
// with no Wine tree.
inline const char* WineArch(const std::string& abi) {
  if (abi == "arm64-v8a") return "aarch64";
  if (abi == "x86_64") return "x86_64";
  return "";
}

// The CPU of the 32 bit x86 code in the WoW64 of Wine: the default value of
// HKLM\Software\Microsoft\Wow64\x86 that pack_wine.sh writes into wine.inf
// of the tree. On ARM64 it is wowbox64.dll of Box64, which the app puts into
// system32 of the prefix. On x86_64 it is wow64cpu.dll of Wine (the code
// runs natively), which Wine makes itself.
inline const char* CpuDll(const std::string& abi) {
  if (abi == "arm64-v8a") return "wowbox64.dll";
  if (abi == "x86_64") return "wow64cpu.dll";
  return "";
}
// True when the app copies the CPU DLL from the tree into system32 of the
// prefix (a DLL that is not a part of Wine).
inline bool CopiesCpuDll(const std::string& abi) { return abi == "arm64-v8a"; }

// The drive of the game folder in Wine.
constexpr char kGameDrive[] = "g:";
// The test program (wine_probe=1) in the game folder, and its result file.
constexpr char kProbeExe[] = "bwvr_wine_probe.exe";
constexpr char kProbeResult[] = "bwvr-wine-probe.txt";

// The tree has the layout of the install of Wine for Android: a folder for
// the ABI (<abi>/bin, <abi>/lib) beside share. Wine finds share from lib by
// the relative path of its build, and its loader is in the folder of
// ntdll.so.
struct Paths {
  std::string tree;      // the unpacked tree
  std::string id_file;   // the build id of the unpacked tree
  std::string loader;    // <abi>/lib/wine/<arch>-unix/wine
  std::string server;    // <abi>/bin/wineserver
  std::string libs;      // <abi>/lib: libfreetype.so
  std::string dlls;      // <abi>/lib/wine: a folder for each architecture
  std::string cpu_dll;   // the CPU DLL in the tree (arm64-v8a only)
  std::string prefix;    // the Wine prefix
  std::string prefix_id; // the marker of a whole prefix: the build id of its tree
  std::string user_reg;  // user.reg of the prefix
  std::string system32;  // system32 of the prefix
  std::string game_link; // dosdevices/g: of the prefix
  std::string log;       // wine.log: stdout and stderr of each program
};

// files: the internal folder of the app (getFilesDir). The tree must be
// there: Android lets an app run and map as code only files of its internal
// folder, not of the shared storage. app_folder: the folder of the app on
// the shared storage, where the logs are.
inline Paths MakePaths(const std::string& files, const std::string& app_folder,
                       const std::string& abi) {
  Paths p;
  const std::string arch = WineArch(abi);
  p.tree = files + "/wine";
  p.id_file = p.tree + "/bwvr-wine-id.txt";
  p.libs = p.tree + "/" + abi + "/lib";
  p.dlls = p.libs + "/wine";
  p.loader = p.dlls + "/" + arch + "-unix/wine";
  p.server = p.tree + "/" + abi + "/bin/wineserver";
  p.cpu_dll = CopiesCpuDll(abi) ? p.dlls + "/" + arch + "-windows/" + CpuDll(abi) : "";
  p.prefix = files + "/prefix";
  p.prefix_id = p.prefix + "/bwvr-prefix-id.txt";
  p.user_reg = p.prefix + "/user.reg";
  p.system32 = p.prefix + "/drive_c/windows/system32";
  p.game_link = p.prefix + "/dosdevices/" + kGameDrive;
  p.log = app_folder + "/wine.log";
  return p;
}

// The text with no white space at its ends.
inline std::string Trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
  return s.substr(a, b - a);
}

// True when the unpacked tree must be made again from the APK: there is no
// tree (no id), or the APK has a tree of another build.
inline bool NeedsUnpack(const std::string& installed_id, const std::string& apk_id) {
  const std::string a = Trim(installed_id), b = Trim(apk_id);
  return a.empty() || a != b;
}

// True when the app makes a new prefix: the prefix has no marker. The last
// step of a good setup of the prefix writes the marker, so a wineboot that
// stopped or failed gives a new prefix.
inline bool NeedsPrefix(const std::string& marker) { return Trim(marker).empty(); }

// True when the CPU DLL of the tree goes into system32 of a prefix that is
// there: the prefix was made with a tree of another build. A new prefix gets
// it before its wineboot.
inline bool NeedsCpuCopy(const std::string& abi, const std::string& marker,
                         const std::string& tree_id) {
  return CopiesCpuDll(abi) && Trim(marker) != Trim(tree_id);
}

// The environment of a program in Wine: the environment of the app with
// these values in place of its own. WINEDEBUG only when wine_debug has a
// value. WINEDLLOVERRIDES: no Mono and no Gecko, so wineboot asks for no
// download. WINELOADER and WINEDLLPATH are not needed: Wine finds its loader
// and its DLLs from the place of ntdll.so in the tree.
inline std::vector<std::string> Environment(const std::vector<std::string>& inherited,
                                            const Paths& p, const std::string& files,
                                            const std::string& debug) {
  static const char* const kDrop[] = {"WINE",    "LD_LIBRARY_PATH=", "LD_PRELOAD=",
                                      "HOME=",   "TMPDIR=",          "DISPLAY=",
                                      "WAYLAND_DISPLAY="};
  std::vector<std::string> env;
  for (const std::string& e : inherited) {
    bool drop = e.find('=') == std::string::npos;
    for (const char* d : kDrop) {
      if (e.compare(0, std::char_traits<char>::length(d), d) == 0) drop = true;
    }
    if (!drop) env.push_back(e);
  }
  env.push_back("WINEPREFIX=" + p.prefix);
  env.push_back("WINESERVER=" + p.server);
  env.push_back("WINEDLLOVERRIDES=mscoree,mshtml=");
  env.push_back("LD_LIBRARY_PATH=" + p.libs);
  env.push_back("HOME=" + files);
  env.push_back("TMPDIR=" + files + "/tmp");
  if (!Trim(debug).empty()) env.push_back("WINEDEBUG=" + Trim(debug));
  return env;
}

// user.reg of a new prefix, before the first wineboot: no display driver
// of Wine (the app draws, and the game shows no window). The wine server
// loads it into HKEY_CURRENT_USER, and wineboot adds the rest.
inline std::string UserRegSeed() {
  return "WINE REGISTRY Version 2\n"
         ";; All keys relative to \\\\User\\\\S-1-5-21-0-0-0-1000\n"
         "\n"
         "[Software\\\\Wine\\\\Drivers]\n"
         "\"Graphics\"=\"null\"\n";
}

// THE TAR FILE OF A TREE (ustar, as GNU tar --format=ustar writes it).
struct TarEntry {
  std::string name;  // the prefix and the name, joined
  char type = 0;     // '0' a file, '5' a folder, '2' a symbolic link
  uint32_t mode = 0;
  uint64_t size = 0;
  std::string link;  // the target of a link
  bool end = false;  // a block of zeros: the end of the archive
};

inline uint64_t TarNumber(const uint8_t* p, size_t n) {
  uint64_t v = 0;
  for (size_t i = 0; i < n && p[i] != 0 && p[i] != ' '; ++i) {
    if (p[i] < '0' || p[i] > '7') return ~0ull;
    v = v * 8 + (p[i] - '0');
  }
  return v;
}

inline std::string TarText(const uint8_t* p, size_t n) {
  size_t len = 0;
  while (len < n && p[len] != 0) ++len;
  return std::string(reinterpret_cast<const char*>(p), len);
}

// Reads one header block of 512 bytes. False for a block that is not a
// ustar header (a bad checksum, a bad number).
inline bool ParseTarHeader(const uint8_t* h, TarEntry* e) {
  *e = TarEntry();
  bool zero = true;
  for (int i = 0; i < 512; ++i) {
    if (h[i] != 0) {
      zero = false;
      break;
    }
  }
  if (zero) {
    e->end = true;
    return true;
  }
  uint64_t sum = 0;
  for (int i = 0; i < 512; ++i) sum += (i >= 148 && i < 156) ? ' ' : h[i];
  if (TarNumber(h + 148, 8) != sum) return false;
  if (TarText(h + 257, 6) != "ustar") return false;
  const uint64_t mode = TarNumber(h + 100, 8);
  const uint64_t size = TarNumber(h + 124, 12);
  if (mode == ~0ull || size == ~0ull) return false;
  e->mode = static_cast<uint32_t>(mode & 07777);
  e->size = size;
  e->type = h[156] == 0 ? '0' : static_cast<char>(h[156]);
  const std::string prefix = TarText(h + 345, 155);
  e->name = TarText(h, 100);
  if (!prefix.empty()) e->name = prefix + "/" + e->name;
  while (!e->name.empty() && e->name.back() == '/') e->name.pop_back();
  e->link = TarText(h + 157, 100);
  return true;
}

// The bytes of the data of an entry in the archive: its size up to a whole
// block.
inline uint64_t TarDataBytes(const TarEntry& e) {
  return e.type == '0' ? (e.size + 511) / 512 * 512 : 0;
}

// True for a name that stays in the tree: relative, no "..", no empty part.
// "./" at the start is allowed. The app takes only files and folders from
// the archive, no links.
inline bool SafeName(const std::string& name) {
  if (name.empty() || name[0] == '/') return false;
  size_t at = 0;
  while (at <= name.size()) {
    size_t end = name.find('/', at);
    if (end == std::string::npos) end = name.size();
    const std::string part = name.substr(at, end - at);
    if (part.empty() || part == "..") return false;
    at = end + 1;
  }
  return true;
}

}  // namespace wine_rules
