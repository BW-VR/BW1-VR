// THE RULES OF "PLAY" IN THE HEADSET APP (docs/android.md, "Play in the
// app"). No Android code: the math test of the PC checks each rule
// (test/controller_math/play_rules_checks.h).
//
// "Play in VR" and "Play, new game" save the menu, write the keys of a run
// into bwvr.ini, write the registry of the game (with the values of a
// profile of the PC, when the game folder has a new one) into the Wine
// prefix, put the files of the mod from the APK into the 32 bit system
// folder of the prefix, and start runblack.exe in Wine. The mod then has no
// Vulkan device (vk_device=0), gives its frames to the app through the live
// stream, and takes the head and the hands from the input channel.
#pragma once

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "../../src/common/ini_text.h"
#include "../../src/common/input_channel.h"
#include "wine_rules.h"

namespace play_rules {

// THE KEYS OF A RUN. "Play" writes each key after the save of the menu, at
// each start, because a save deletes the retired keys (vk_stream_live is
// one). Only the keys whose default is not the value that a run needs are
// here. The save of the menu writes vulkan_strict=1 and backend=vulkan
// itself, and the app reads stream_projection and stream_layers with 1 as
// the default.
struct RunKey {
  const char* key;
  int value;
};
constexpr RunKey kRunKeys[] = {
    // The mod (mod_main.cpp, vk_stream.cpp, xr_input.cpp, ui_panel.cpp).
    {"vk_stream_live", 1},  // the frames of the walks into the ring (default 0)
    {"xr_input", 1},        // the head and the hands from the input channel (0)
    {"vk_device", 0},       // no Vulkan device of the mod (1)
    {"ui_panel", 1},        // the help text and the menus of the game as panel quads (0)
    {"vk_pace", 1},         // a frame of the game waits for a frame of the session (0)
    // The app (main.cpp).
    {"stream_live", 1},     // the reader of the ring (0)
    {"input_live", 1},      // the writer of the input channel (0)
};
constexpr int kRunKeyCount = static_cast<int>(sizeof(kRunKeys) / sizeof(kRunKeys[0]));

inline void SetRunKeys(bwvr_ini::IniText* ini) {
  for (const RunKey& k : kRunKeys) ini->SetInt("bwvr", k.key, k.value);
}

// THE FILES OF THE MOD in the APK (assets/mod), the build of .\dev.ps1
// build: the DirectDraw of the game, and the OpenXR loader that it imports.
// The mod needs no other file at run time: with vulkan_strict=1 it loads no
// dxwrapper, and it loads d3d11.dll and dxgi.dll only at a first call, which
// the renderer Vulkan does not make.
constexpr const char* kModFiles[] = {"ddraw.dll", "openxr_loader.dll"};
constexpr char kModAssets[] = "mod";

// The 32 bit system folder of a WoW64 prefix, where the app puts the files
// of the mod. The DLL search of Windows looks first in the folder of the
// program, so a ddraw.dll in the game folder of the player would come
// first. The values of KnownDLLs make the two files known DLLs: Wine then
// maps them from this folder, whatever the folder of the program has
// (wineboot, create_known_dlls, and ntdll, open_known_dll).
inline std::string ModFolder(const std::string& prefix) {
  return prefix + "/drive_c/windows/syswow64";
}

// The folder of the files of the app in drive C: of the prefix, and the
// registry file of a start in it, as Wine names it.
inline std::string AppFolder(const std::string& prefix) { return prefix + "/drive_c/bwvr"; }
constexpr char kRegFile[] = "game.reg";
constexpr char kRegFileWine[] = "C:\\bwvr\\game.reg";

// The drive of the game folder (wine_rules::kGameDrive) as GameDir.
constexpr char kGameDir[] = "G:\\";
constexpr char kGameExe[] = "G:\\runblack.exe";

// THE REGISTRY OF THE GAME in the prefix, as a file for reg import. Only
// what the game needs: GameDir (the game makes it its current folder after
// its start, docs/backend.md and tools/desk/README.md), and the values of
// the launcher of the PC (docs/launcher.md): the size of the render as
// ScreenW and ScreenH, FullScreen 0 and ScreenD 32. Each a REG_DWORD as the
// game writes it, GameDir a REG_SZ. Then the values of KnownDLLs for the
// files of the mod. Then the audio driver of Wine (wine_audio), so a prefix
// of an older tree gets it too: audio gives "Audio" = "aaudio", and no audio
// deletes the value (mmdevapi then finds no driver, and the game has no
// sound).
inline std::string GameRegistry(int width, int height, bool audio) {
  char dwords[256];
  snprintf(dwords, sizeof(dwords),
           "\"ScreenW\"=dword:%08x\r\n\"ScreenH\"=dword:%08x\r\n"
           "\"FullScreen\"=dword:00000000\r\n\"ScreenD\"=dword:00000020\r\n",
           static_cast<unsigned>(width), static_cast<unsigned>(height));
  std::string text = "Windows Registry Editor Version 5.00\r\n\r\n";
  text += "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White]\r\n";
  text += "\"GameDir\"=\"G:\\\\\"\r\n\r\n";
  text += "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\BWSetup]\r\n";
  text += dwords;
  text += "\r\n[HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\Session Manager\\"
          "KnownDLLs]\r\n";
  for (const char* f : kModFiles) {
    std::string name = f;
    name.resize(name.find('.'));
    text += "\"bwvr_" + name + "\"=\"" + f + "\"\r\n";
  }
  text += "\r\n[HKEY_CURRENT_USER\\Software\\Wine\\Drivers]\r\n";
  text += audio ? "\"Audio\"=\"" + std::string(wine_rules::kAudioDriver) + "\"\r\n"
                : "\"Audio\"=-\r\n";
  return text;
}

// The DLL overrides of the game: the overrides of each program
// (wine_rules::Environment), and ddraw as a native DLL alone. Wine has no
// builtin DirectDraw in the tree ("Only Vulkan").
constexpr char kGameOverrides[] = "WINEDLLOVERRIDES=mscoree,mshtml=;ddraw=n";

// THE VARIABLES OF BOX64 FOR THE GAME (docs/android.md, "The variables of
// Box64"). wowbox64.dll reads its settings from the Windows environment of
// its process (LoadEnvVariables of Box64 0.4.4, GetEnvironmentVariableA),
// and only the settings that are "available in WowBox64". It reads no
// box64rc file under Windows. Wine gives the Unix environment of the loader
// to the Windows environment of start.exe, and start.exe gives it to
// runblack.exe. reg import, wineserver -w and wineboot start with the
// environment of each program, so they do not get them. The loader of the
// game also starts the wine server of the game (the server before it ended
// at wineserver -w), so that server gets them too. It has no Box64, so they
// change nothing there.
//
//   box64_strongmem=0  no variable (the default)
//   box64_strongmem=1 to 3  BOX64_DYNAREC_STRONGMEM=<n>: memory barriers for
//                    the strong memory order of x86 (1 at writes, 2 also at
//                    SIMD instructions, 3 also more often). Box64 also has
//                    4 (a model of the whole x86 order), which only
//                    box64_env gives
//   box64_env        more variables: NAME=VALUE with spaces between them.
//                    Only names that start with BOX64_ go to the game.
constexpr char kStrongMemName[] = "BOX64_DYNAREC_STRONGMEM";
constexpr int kStrongMemMost = 3;

struct Box64Env {
  std::vector<std::string> vars;     // NAME=VALUE for the game, in order
  std::vector<std::string> refused;  // each word of box64_env that does not go, with why
};

inline bool IsBox64Name(const std::string& name) {
  if (name.compare(0, 6, "BOX64_") != 0 || name.size() == 6) return false;
  for (char c : name) {
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

inline Box64Env Box64Vars(int strongmem, const std::string& extra) {
  Box64Env out;
  std::vector<std::string> names;
  if (strongmem >= 1 && strongmem <= kStrongMemMost) {
    out.vars.push_back(std::string(kStrongMemName) + "=" + std::to_string(strongmem));
    names.push_back(kStrongMemName);
  }
  size_t at = 0;
  while (at < extra.size()) {
    while (at < extra.size() && (extra[at] == ' ' || extra[at] == '\t')) ++at;
    size_t end = at;
    while (end < extra.size() && extra[end] != ' ' && extra[end] != '\t') ++end;
    if (end == at) break;
    const std::string word = extra.substr(at, end - at);
    at = end;
    const size_t eq = word.find('=');
    const std::string name = eq == std::string::npos ? word : word.substr(0, eq);
    if (eq == std::string::npos) {
      out.refused.push_back(word + " (no =VALUE)");
    } else if (!IsBox64Name(name)) {
      out.refused.push_back(word + " (the name does not start with BOX64_)");
    } else if (std::find(names.begin(), names.end(), name) != names.end()) {
      out.refused.push_back(word + (name == kStrongMemName && strongmem >= 1
                                        ? " (box64_strongmem sets it)"
                                        : " (the name comes two times)"));
    } else {
      out.vars.push_back(word);
      names.push_back(name);
    }
  }
  return out;
}

// The environment of the game: the environment of a program in Wine with
// kGameOverrides in place of its overrides, and the variables of Box64
// (Box64Vars) in place of an inherited variable with the same name.
inline std::vector<std::string> GameEnvironment(const std::vector<std::string>& env,
                                                const std::vector<std::string>& box64 = {}) {
  std::vector<std::string> out;
  for (const std::string& e : env) {
    if (e.compare(0, 17, "WINEDLLOVERRIDES=") == 0) continue;
    bool replaced = false;
    for (const std::string& v : box64) {
      const size_t eq = v.find('=');
      if (eq != std::string::npos && e.compare(0, eq + 1, v, 0, eq + 1) == 0) replaced = true;
    }
    if (!replaced) out.push_back(e);
  }
  out.push_back(kGameOverrides);
  for (const std::string& v : box64) out.push_back(v);
  return out;
}

// The command line of the game: "Play, new game" gives -NEWGAME, as the
// launcher of the PC. "Play in VR" gives no word: the game loads slot 20 of
// the profile, or starts a new game when there is none (docs/backend.md, "A
// test save").
inline std::vector<std::string> GameArgs(const std::string& loader, bool new_game) {
  std::vector<std::string> args = {loader, kGameExe};
  if (new_game) args.push_back("-NEWGAME");
  return args;
}

// THE FRAME RATE of the game from the frames of the live stream: the frame
// numbers of the mod at two times (ms).
inline double FramesPerSecond(unsigned long long frame0, double ms0, unsigned long long frame1,
                              double ms1) {
  if (ms1 <= ms0 || frame1 < frame0) return 0.0;
  return (frame1 - frame0) * 1000.0 / (ms1 - ms0);
}

// THE INPUT OF THE MOD while the game runs. menu_over_game: the menu of the
// app shows over the game, and the player points at the menu, so the mod
// gets no focus and no active hand, as with no input focus of the session
// (XrApp::InputSlot). The views, the display time and the serials stay.
// With no menu the slot stays as the session gave it.
inline void MaskInput(input_channel::SlotData* slot, bool menu_over_game) {
  if (!menu_over_game) return;
  for (int h = 0; h < 2; ++h) {
    input_channel::Hand& hand = slot->hands[h];
    const uint32_t serial = hand.serial;
    memset(&hand, 0, sizeof(hand));
    hand.aim_orientation[3] = 1.0f;
    hand.grip_orientation[3] = 1.0f;
    hand.serial = serial;
  }
  slot->flags &= ~input_channel::kFocused;
}

// THE MENU BUTTON while the game runs. A hold of play_menu_hold_ms shows the
// menu of the app over the game, and a press while the menu shows hides it.
// With play_menu_press=1 (hold_back) the app holds the menu bit of the slot
// back while the button is down: a press shorter than the hold goes to the
// mod at its release (down for kReleaseSlots slots, then up), on the hand
// that pressed. A hold and a press that hides the menu never reach the mod.
// With play_menu_press=0 the mod gets each press at once, as the session
// gives it, and a hold opens the menu of the game too. With hold_ms=0 there
// is no menu over the game, and each press goes to the mod at once.
constexpr int kReleaseSlots = 2;

enum class MenuAction { kNone, kShow, kHide };

struct MenuButtonRule {
  bool down = false;     // the button was down in the slot before
  double down_ms = 0.0;  // the time of the press
  bool taken = false;    // the press is the app's: a hold or a hide
  int hand = 0;          // the hand of the press
  int release_slots = 0; // the slots that still give the press to the mod
};

struct MenuStep {
  MenuAction action = MenuAction::kNone;
  bool mod_menu[2] = {false, false};  // the menu bit of each hand for the mod
};

// One slot. down: the menu button of each hand in this slot.
inline MenuStep StepMenuButton(MenuButtonRule* r, const bool down[2], double now_ms,
                               bool menu_over_game, int hold_ms, bool hold_back) {
  MenuStep s;
  const bool any = down[0] || down[1];
  const bool back = hold_back && hold_ms > 0;
  if (any && !r->down) {
    r->down_ms = now_ms;
    r->hand = down[0] ? 0 : 1;
    r->taken = false;
    if (menu_over_game && hold_ms > 0) {
      s.action = MenuAction::kHide;
      r->taken = true;
    }
  }
  if (any && !r->taken && !menu_over_game && hold_ms > 0 && now_ms - r->down_ms >= hold_ms) {
    s.action = MenuAction::kShow;
    r->taken = true;
  }
  if (back) {
    if (!any && r->down && !r->taken) r->release_slots = kReleaseSlots;
    if (r->release_slots > 0) {
      s.mod_menu[r->hand] = true;
      --r->release_slots;
    }
  } else {
    s.mod_menu[0] = down[0];
    s.mod_menu[1] = down[1];
  }
  r->down = any;
  return s;
}

// The menu bits of the step into the slot of the mod.
inline void ApplyMenuBits(input_channel::SlotData* slot, const MenuStep& step) {
  for (int h = 0; h < 2; ++h) {
    if (step.mod_menu[h]) {
      slot->hands[h].flags |= input_channel::kMenu;
    } else {
      slot->hands[h].flags &= ~input_channel::kMenu;
    }
  }
}

// The time of the day as text for the status line, "14:03:22".
inline std::string Clock(int hour, int minute, int second) {
  char text[16];
  snprintf(text, sizeof(text), "%02d:%02d:%02d", hour, minute, second);
  return text;
}

// THE PROFILE OF THE PC (pc_profile=1, docs/android.md, "The profile of the
// PC"). push_profile.ps1 writes the values of a profile of the game into
// bwvr-profile.reg in the game folder of the headset, and it copies the
// folder of the profile and its Creature. The game keeps a profile in two
// places of the registry (LHMultiplayerR.dll): the value Profile of the key
// LHMultiplayer (the name of the current player, REG_BINARY UTF-16 with a
// zero at its end), and the key LHMultiplayer\Profiles\<folder> with the
// values of that player (PlayerProfile, the key bindings, LoveBuckets). The
// folder is the name with a "_" before each character (ProfileNameToKey).
// The app imports only these values, after GameRegistry: never GameDir, a
// value of BWSetup (the screen, the detail, the volumes of the PC), a value
// of another key, or kSecretValue. It imports each copy one time: the stamp
// of the file goes into kProfileDone, so the values that the game in the
// headset writes later stay until push_profile.ps1 makes a new copy (each
// copy has its own line "; copy <guid>").
constexpr char kProfileFile[] = "bwvr-profile.reg";
constexpr char kProfileDone[] = "pc-profile.done";
constexpr size_t kProfileFileMost = 256 * 1024;
// The password of the online login of the player, encrypted with a key of
// the game that each copy of the game can undo. A secret does not go to the
// headset: push_profile.ps1 leaves it out, and the filter drops it.
constexpr char kSecretValue[] = "login password";

inline const char* RegHeader() { return "Windows Registry Editor Version 5.00"; }
inline const char* MultiplayerKey() {
  return "HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\LHMultiplayer";
}
inline const char* ProfilesKey() {
  return "HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\LHMultiplayer\\"
         "Profiles\\";
}

// One value of the file: the name as it is between the quotes (with its
// escapes), and the data after "=" as the file has it.
struct ProfileValue {
  std::string name;
  std::string data;
};

struct ProfileFile {
  bool ok = false;
  std::string why;         // why the file is not right (ok false)
  std::string name;        // the name of the profile, from the value Profile
  std::string profile;     // the data of the value Profile
  std::string folder;      // the key of the profile, for example _A_n_n
  std::vector<ProfileValue> values;  // the values of the key of the profile
  int dropped = 0;         // the keys and the values that the filter dropped
};

inline char Lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

// Two names of the registry are the same name when only the case is not the
// same.
inline bool SameKey(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (Lower(a[i]) != Lower(b[i])) return false;
  }
  return true;
}

inline bool StartsKey(const std::string& text, const std::string& start) {
  return text.size() >= start.size() && SameKey(text.substr(0, start.size()), start);
}

inline bool HexDigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

inline int HexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  return Lower(c) - 'a' + 10;
}

// A quoted text of a .reg file from text[at] (the opening quote): a
// backslash comes only before a backslash or a quote. Gives the index after
// the closing quote, or npos when the form is not right. inner: the text
// between the quotes, with its escapes.
inline size_t QuotedEnd(const std::string& text, size_t at, std::string* inner) {
  if (at >= text.size() || text[at] != '"') return std::string::npos;
  for (size_t i = at + 1; i < text.size(); ++i) {
    if (text[i] == '\\') {
      if (i + 1 >= text.size() || (text[i + 1] != '\\' && text[i + 1] != '"')) {
        return std::string::npos;
      }
      ++i;
    } else if (text[i] == '"') {
      if (inner != nullptr) *inner = text.substr(at + 1, i - at - 1);
      return i + 1;
    }
  }
  return std::string::npos;
}

// The bytes of a list "aa,bb,cc" (empty for no byte). False when the form is
// not right.
inline bool HexBytes(const std::string& list, std::vector<unsigned char>* bytes) {
  bytes->clear();
  if (list.empty()) return true;
  size_t i = 0;
  while (true) {
    if (i + 2 > list.size() || !HexDigit(list[i]) || !HexDigit(list[i + 1])) return false;
    bytes->push_back(static_cast<unsigned char>(HexValue(list[i]) * 16 + HexValue(list[i + 1])));
    i += 2;
    if (i == list.size()) return true;
    if (list[i] != ',') return false;
    ++i;
  }
}

// The data of a value: dword:XXXXXXXX, hex:aa,bb, hex(N):aa,bb, or a quoted
// text. The import takes no other form.
inline bool ValueDataRight(const std::string& data) {
  std::vector<unsigned char> bytes;
  if (data.compare(0, 6, "dword:") == 0) {
    if (data.size() != 14) return false;
    for (size_t i = 6; i < data.size(); ++i) {
      if (!HexDigit(data[i])) return false;
    }
    return true;
  }
  if (data.compare(0, 4, "hex:") == 0) return HexBytes(data.substr(4), &bytes);
  if (data.compare(0, 4, "hex(") == 0) {
    const size_t close = data.find("):");
    if (close == std::string::npos || close == 4 || close > 12) return false;
    for (size_t i = 4; i < close; ++i) {
      if (!HexDigit(data[i])) return false;
    }
    return HexBytes(data.substr(close + 2), &bytes);
  }
  if (!data.empty() && data[0] == '"') return QuotedEnd(data, 0, nullptr) == data.size();
  return false;
}

// The name in a value Profile (hex:, UTF-16 LE with a zero at its end). Only
// the printable characters of ASCII, and no backslash: the name is also the
// key of the profile and a folder. False when the data is not such a name.
inline bool ProfileNameFromData(const std::string& data, std::string* name) {
  if (data.compare(0, 4, "hex:") != 0) return false;
  std::vector<unsigned char> bytes;
  if (!HexBytes(data.substr(4), &bytes) || bytes.size() % 2 != 0) return false;
  std::string text;
  size_t i = 0;
  for (; i + 1 < bytes.size(); i += 2) {
    const unsigned c = bytes[i] | (bytes[i + 1] << 8);
    if (c == 0) break;
    if (c < 0x20 || c > 0x7E || c == '\\') return false;
    text += static_cast<char>(c);
  }
  if (text.empty()) return false;
  *name = text;
  return true;
}

// The key of a profile: the name with a "_" before each character.
inline std::string ProfileFolder(const std::string& name) {
  std::string folder;
  for (char c : name) {
    folder += '_';
    folder += c;
  }
  return folder;
}

// Reads bwvr-profile.reg. The form: the line "Windows Registry Editor Version
// 5.00", then keys ("[key]") and values ("name"=data), one on each line,
// only the printable characters of ASCII. The filter keeps the value Profile
// of LHMultiplayer and each value of the key of that profile but
// kSecretValue. It drops each other key and value, kSecretValue, each delete
// ("[-key]", "name"=-) and the default value (@). Lines that start with ";"
// are comments ("; copy <guid>" of push_profile.ps1). A file with a line of
// another form, no value Profile, or no key
// of that profile is not right, and the app imports nothing from it.
inline ProfileFile ParseProfileFile(const std::string& text) {
  ProfileFile f;
  if (text.empty()) {
    f.why = "the file is empty";
    return f;
  }
  if (text.size() > kProfileFileMost) {
    f.why = "the file has more than 256 KB";
    return f;
  }
  for (char c : text) {
    const unsigned char u = static_cast<unsigned char>(c);
    if ((u < 0x20 && c != '\r' && c != '\n' && c != '\t') || u > 0x7E) {
      f.why = "the file has a character that is not printable ASCII";
      return f;
    }
  }
  enum class Key { kNone, kMultiplayer, kProfile, kOther };
  Key key = Key::kNone;
  std::string key_folder;
  std::vector<std::pair<std::string, std::vector<ProfileValue>>> profiles;
  bool header = false;
  bool have_profile = false;
  size_t at = 0;
  int line_number = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(at, end - at);
    at = end + 1;
    ++line_number;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
    if (line.empty() || line[0] == ';') continue;
    if (!header) {
      if (line != RegHeader()) {
        f.why = "the first line is not \"" + std::string(RegHeader()) + "\"";
        return f;
      }
      header = true;
      continue;
    }
    if (line[0] == '[') {
      if (line.back() != ']') {
        f.why = "line " + std::to_string(line_number) + " is not a key";
        return f;
      }
      const std::string name = line.substr(1, line.size() - 2);
      if (!name.empty() && name[0] == '-') {
        key = Key::kOther;
        ++f.dropped;
      } else if (SameKey(name, MultiplayerKey())) {
        key = Key::kMultiplayer;
      } else if (StartsKey(name, ProfilesKey()) && name.size() > strlen(ProfilesKey()) &&
                 name.find('\\', strlen(ProfilesKey())) == std::string::npos) {
        key = Key::kProfile;
        key_folder = name.substr(strlen(ProfilesKey()));
        profiles.push_back({key_folder, {}});
      } else {
        key = Key::kOther;
        ++f.dropped;
      }
      continue;
    }
    if (key == Key::kNone) {
      f.why = "line " + std::to_string(line_number) + " is a value before the first key";
      return f;
    }
    std::string value_name;
    size_t eq = 0;
    if (line[0] == '@') {
      eq = 1;
    } else {
      eq = QuotedEnd(line, 0, &value_name);
      if (eq == std::string::npos) {
        f.why = "line " + std::to_string(line_number) + " has no right name of a value";
        return f;
      }
    }
    if (eq >= line.size() || line[eq] != '=') {
      f.why = "line " + std::to_string(line_number) + " has no \"=\" after the name";
      return f;
    }
    const std::string data = line.substr(eq + 1);
    if (data == "-" || line[0] == '@') {
      ++f.dropped;
      continue;
    }
    if (!ValueDataRight(data)) {
      f.why = "line " + std::to_string(line_number) + " has data of a form that the import "
              "does not take";
      return f;
    }
    if (key == Key::kMultiplayer && SameKey(value_name, "Profile")) {
      f.profile = data;
      have_profile = true;
    } else if (key == Key::kProfile && !SameKey(value_name, kSecretValue)) {
      profiles.back().second.push_back({value_name, data});
    } else {
      ++f.dropped;
    }
  }
  if (!header) {
    f.why = "the file has no line \"" + std::string(RegHeader()) + "\"";
    return f;
  }
  if (!have_profile) {
    f.why = "the file has no value Profile of LHMultiplayer";
    return f;
  }
  if (!ProfileNameFromData(f.profile, &f.name)) {
    f.why = "the value Profile is not a name of printable ASCII characters";
    return f;
  }
  const std::string want = ProfileFolder(f.name);
  bool found = false;
  for (const auto& p : profiles) {
    if (SameKey(p.first, want) && !found) {
      found = true;
      f.folder = p.first;
      f.values = p.second;
    } else {
      f.dropped += 1 + static_cast<int>(p.second.size());
    }
  }
  if (!found) {
    f.why = "the file has no key of the profile " + f.name + " (" + want + ")";
    f.name.clear();
    return f;
  }
  if (f.values.empty()) {
    f.why = "the key of the profile " + f.name + " has no value";
    f.name.clear();
    return f;
  }
  f.ok = true;
  return f;
}

// The values of a right file as keys of a file for reg import, to go after
// GameRegistry. Empty for a file that is not right.
inline std::string ProfileRegistry(const ProfileFile& f) {
  if (!f.ok) return "";
  std::string text = "\r\n[" + std::string(MultiplayerKey()) + "]\r\n";
  text += "\"Profile\"=" + f.profile + "\r\n";
  text += "\r\n[" + std::string(ProfilesKey()) + f.folder + "]\r\n";
  for (const ProfileValue& v : f.values) text += "\"" + v.name + "\"=" + v.data + "\r\n";
  return text;
}

// The stamp of a file: FNV-1a with 64 bits of its bytes, and its size, as
// text. A new file of push_profile.ps1 has a new stamp.
inline std::string ProfileStamp(const std::string& text) {
  unsigned long long h = 14695981039346656037ULL;
  for (char c : text) {
    h ^= static_cast<unsigned char>(c);
    h *= 1099511628211ULL;
  }
  char out[48];
  snprintf(out, sizeof(out), "%016llx %zu", h, text.size());
  return out;
}

// True when "Play" imports the file: a right file whose stamp is not the
// stamp in kProfileDone (the text of that file, empty when it is not there).
inline bool NeedsProfileImport(const ProfileFile& f, const std::string& stamp,
                               const std::string& done) {
  if (!f.ok) return false;
  std::string last = done;
  while (!last.empty() && (last.back() == '\n' || last.back() == '\r' || last.back() == ' ')) {
    last.pop_back();
  }
  return last != stamp;
}

// The name of the current profile in user.reg of the prefix: the value
// Profile of the key LHMultiplayer, as Wine writes it ("[Software\\...] time",
// "Profile"=hex:..., a long value on more lines with a "\" at the end).
// Empty when the prefix has no profile.
inline std::string PrefixProfileName(const std::string& user_reg) {
  const std::string section = "[Software\\\\Lionhead Studios Ltd\\\\Black & White\\\\LHMultiplayer]";
  bool in = false;
  size_t at = 0;
  while (at < user_reg.size()) {
    size_t end = user_reg.find('\n', at);
    if (end == std::string::npos) end = user_reg.size();
    std::string line = user_reg.substr(at, end - at);
    at = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty() && line[0] == '[') {
      in = StartsKey(line, section) &&
           (line.size() == section.size() || line[section.size()] == ' ');
      continue;
    }
    if (!in || !StartsKey(line, "\"Profile\"=")) continue;
    std::string data = line.substr(10);
    // A long value goes on: a "\" at the end, then a line with spaces first.
    while (!data.empty() && data.back() == '\\' && at < user_reg.size()) {
      data.pop_back();
      end = user_reg.find('\n', at);
      if (end == std::string::npos) end = user_reg.size();
      std::string next = user_reg.substr(at, end - at);
      at = end + 1;
      if (!next.empty() && next.back() == '\r') next.pop_back();
      size_t s = 0;
      while (s < next.size() && next[s] == ' ') ++s;
      data += next.substr(s);
    }
    std::string name;
    return ProfileNameFromData(data, &name) ? name : "";
  }
  return "";
}

// The text of the help of "Play" about the profile of the game in the
// headset. pc_profile: the key. file_found: the game folder has
// bwvr-profile.reg. imported: its stamp is the stamp of the last import.
// prefix_name: the profile in the registry of the prefix (empty: none).
inline std::string ProfileHelp(bool pc_profile, bool file_found, const ProfileFile& file,
                               bool imported, const std::string& prefix_name) {
  if (pc_profile && file_found && file.ok) {
    return "The profile of the game: " + file.name +
           (imported ? ", from the PC." : ", from the PC. Play puts it into the game.");
  }
  std::string text;
  if (pc_profile && file_found) text = "bwvr-profile.reg is not right: " + file.why + ". ";
  if (!prefix_name.empty()) return text + "The profile of the game: " + prefix_name + ".";
  return text + "No profile: the game asks for a new one.";
}

}  // namespace play_rules
