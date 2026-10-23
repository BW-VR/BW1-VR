// THE RULES OF "PLAY" IN THE HEADSET APP (docs/android.md, "Play in the
// app"). No Android code: the math test of the PC checks each rule
// (test/controller_math/play_rules_checks.h).
//
// "Play in VR" and "Play, new game" save the menu, write the keys of a run
// into bwvr.ini, write the registry of the game into the Wine prefix, put
// the files of the mod from the APK into the 32 bit system folder of the
// prefix, and start runblack.exe in Wine. The mod then has no Vulkan device
// (vk_device=0), gives its frames to the app through the live stream, and
// takes the head and the hands from the input channel.
#pragma once

#include <stdio.h>
#include <string.h>

#include <string>
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

// The environment of the game: the environment of a program in Wine with
// kGameOverrides in place of its overrides.
inline std::vector<std::string> GameEnvironment(const std::vector<std::string>& env) {
  std::vector<std::string> out;
  for (const std::string& e : env) {
    if (e.compare(0, 17, "WINEDLLOVERRIDES=") != 0) out.push_back(e);
  }
  out.push_back(kGameOverrides);
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

}  // namespace play_rules
