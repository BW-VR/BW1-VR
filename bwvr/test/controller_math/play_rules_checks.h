// The rules of "Play" in the headset app, a test (android/src/play_rules.h):
// the keys of a run after a save of the menu, the registry of the game, the
// environment and the command line of the game, the input of the mod while
// the menu shows over the game, and "Quit game" in the menu. main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../../android/src/menu_model.h"
#include "../../android/src/play_rules.h"
#include "../../src/common/settings_table.h"

namespace play_rules_checks {

inline bool Retired(const char* key) {
  for (int i = 0; i < bwvr_settings::kRetiredKeyCount; ++i) {
    if (strcmp(bwvr_settings::kRetiredKeys[i], key) == 0) return true;
  }
  return false;
}

inline void RunKeyChecks() {
  printf("\nPlay: the keys of a run\n");
  menu_model::Menu menu;
  bwvr_ini::IniText ini("[bwvr]\r\nvk_stream_live=1\r\nui_panel=0\r\nkeep_me=7\r\n");
  menu.Load(ini);
  // The save deletes vk_stream_live (a retired key), so "Play" writes the
  // keys of the run after the save.
  menu.Store(&ini);
  Check(Retired("vk_stream_live") && !ini.Has("bwvr", "vk_stream_live"),
        "a save deletes vk_stream_live, a retired key", 0, 0);
  play_rules::SetRunKeys(&ini);
  Check(ini.GetInt("bwvr", "vk_stream_live", 0) == 1, "vk_stream_live=1 after the save", 0, 0);
  Check(ini.GetInt("bwvr", "xr_input", 0) == 1, "xr_input=1", 0, 0);
  Check(ini.Has("bwvr", "vk_device") && ini.GetInt("bwvr", "vk_device", 1) == 0, "vk_device=0",
        0, 0);
  Check(ini.GetInt("bwvr", "ui_panel", 0) == 1, "ui_panel=1 in place of 0", 0, 0);
  Check(ini.GetInt("bwvr", "stream_live", 0) == 1 && ini.GetInt("bwvr", "input_live", 0) == 1,
        "stream_live=1 and input_live=1 for the app", 0, 0);
  Check(ini.GetInt("bwvr", "vulkan_strict", 0) == 1 && ini.Get("bwvr", "backend", "") == "vulkan",
        "the save gives vulkan_strict=1 and backend=vulkan", 0, 0);
  Check(ini.GetInt("bwvr", "keep_me", 0) == 7, "another key stays", 0, 0);
  Check(play_rules::kRunKeyCount == 6, "six keys of a run", play_rules::kRunKeyCount, 6);
  // A second save and Play give the same lines. The save deletes
  // vk_stream_live, and Play adds it at the end of the section again.
  auto sorted = [](const std::string& text) {
    std::vector<std::string> lines;
    size_t at = 0;
    while (at < text.size()) {
      const size_t end = text.find("\r\n", at);
      lines.push_back(text.substr(at, end - at));
      at = end + 2;
    }
    std::sort(lines.begin(), lines.end());
    return lines;
  };
  const std::vector<std::string> once = sorted(ini.Text());
  menu.Store(&ini);
  play_rules::SetRunKeys(&ini);
  Check(sorted(ini.Text()) == once, "a second save and Play give the same lines of bwvr.ini",
        static_cast<double>(sorted(ini.Text()).size()), static_cast<double>(once.size()));
  // No key of a run is a row of the menu, so a save of the menu does not
  // write over it with a value of the player.
  bool row = false;
  for (int i = 0; i < bwvr_settings::kSettingCount; ++i) {
    const char* key = bwvr_settings::kSettings[i].key;
    if (key == nullptr) continue;
    for (const play_rules::RunKey& k : play_rules::kRunKeys) {
      if (strcmp(key, k.key) == 0) row = true;
    }
  }
  Check(!row, "no key of a run is a row of the menu", 0, 0);
}

inline void RegistryChecks() {
  printf("\nPlay: the registry of the game in the prefix\n");
  const std::string reg = play_rules::GameRegistry(1380, 1104);
  Check(reg.compare(0, 36, "Windows Registry Editor Version 5.00") == 0,
        "a file of reg import", 0, 0);
  Check(reg.find("[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White]\r\n"
                 "\"GameDir\"=\"G:\\\\\"\r\n") != std::string::npos,
        "GameDir is G:\\ (a backslash is two in the file)", 0, 0);
  Check(reg.find("\\Black & White\\BWSetup]\r\n\"ScreenW\"=dword:00000564\r\n"
                 "\"ScreenH\"=dword:00000450\r\n\"FullScreen\"=dword:00000000\r\n"
                 "\"ScreenD\"=dword:00000020\r\n") != std::string::npos,
        "ScreenW 1380, ScreenH 1104, FullScreen 0, ScreenD 32 as REG_DWORD", 0, 0);
  Check(reg.find("Session Manager\\KnownDLLs]\r\n\"bwvr_ddraw\"=\"ddraw.dll\"\r\n"
                 "\"bwvr_openxr_loader\"=\"openxr_loader.dll\"\r\n") != std::string::npos,
        "the files of the mod are known DLLs", 0, 0);
  Check(reg.find("Profile") == std::string::npos && reg.find("LHMultiplayer") == std::string::npos,
        "no value of a profile", 0, 0);
  Check(play_rules::ModFolder("/f/prefix") == "/f/prefix/drive_c/windows/syswow64",
        "the files of the mod go into syswow64, the 32 bit system folder", 0, 0);
  Check(play_rules::AppFolder("/f/prefix") + "/" + play_rules::kRegFile ==
            "/f/prefix/drive_c/bwvr/game.reg" &&
            strcmp(play_rules::kRegFileWine, "C:\\bwvr\\game.reg") == 0,
        "the registry file is C:\\bwvr\\game.reg", 0, 0);
}

inline void GameStartChecks() {
  printf("\nPlay: the start of the game in Wine\n");
  const std::vector<std::string> env = {"PATH=/system/bin", "WINEPREFIX=/f/prefix",
                                        "WINEDLLOVERRIDES=mscoree,mshtml="};
  const std::vector<std::string> game = play_rules::GameEnvironment(env);
  size_t overrides = 0;
  for (const std::string& e : game) {
    if (e.compare(0, 17, "WINEDLLOVERRIDES=") == 0) ++overrides;
  }
  Check(overrides == 1 && game.back() == "WINEDLLOVERRIDES=mscoree,mshtml=;ddraw=n",
        "the game has one WINEDLLOVERRIDES: no Mono, no Gecko, ddraw native", 0, 0);
  Check(game.size() == 3 && game[0] == "PATH=/system/bin" && game[1] == "WINEPREFIX=/f/prefix",
        "the other values stay", static_cast<double>(game.size()), 3);
  const std::vector<std::string> fresh = play_rules::GameArgs("/t/wine", true);
  Check(fresh.size() == 3 && fresh[0] == "/t/wine" && fresh[1] == "G:\\runblack.exe" &&
            fresh[2] == "-NEWGAME",
        "Play, new game: wine G:\\runblack.exe -NEWGAME", static_cast<double>(fresh.size()), 3);
  const std::vector<std::string> play = play_rules::GameArgs("/t/wine", false);
  Check(play.size() == 2 && play[1] == "G:\\runblack.exe", "Play in VR: no word",
        static_cast<double>(play.size()), 2);
  Near("300 frames in 10 s are 30 frames a second",
       play_rules::FramesPerSecond(100, 1000.0, 400, 11000.0), 30.0, 1e-9);
  Check(play_rules::FramesPerSecond(400, 1000.0, 100, 2000.0) == 0.0 &&
            play_rules::FramesPerSecond(1, 5.0, 2, 5.0) == 0.0,
        "no rate for a frame back or no time", 0, 0);
  Check(play_rules::Clock(9, 5, 7) == "09:05:07", "the clock of the status line", 0, 0);
}

inline void MaskChecks() {
  printf("\nPlay: the input of the mod while the game runs\n");
  namespace ic = input_channel;
  ic::SlotData slot;
  memset(&slot, 0, sizeof(slot));
  slot.flags = ic::kFocused | ic::kRunning;
  slot.eyes[0].position[0] = -0.032f;
  for (int h = 0; h < 2; ++h) {
    slot.hands[h].flags = ic::kActive | ic::kAimValid | ic::kMenu | ic::kPrimary;
    slot.hands[h].trigger = 0.8f;
    slot.hands[h].serial = 40 + h;
    slot.hands[h].aim_position[1] = 1.1f;
  }
  ic::SlotData keep = slot;
  play_rules::MaskInput(&keep, false);
  Check(memcmp(&keep, &slot, sizeof(slot)) == 0,
        "no menu: the slot as the session gave it, the menu button too", 0, 0);
  ic::SlotData menu = slot;
  play_rules::MaskInput(&menu, true);
  Check((menu.flags & ic::kFocused) == 0 && (menu.flags & ic::kRunning) != 0,
        "the menu over the game: no focus for the mod, the session runs", 0, 0);
  Check(menu.hands[0].flags == 0 && menu.hands[1].flags == 0 && menu.hands[1].trigger == 0.0f &&
            menu.hands[0].aim_orientation[3] == 1.0f,
        "the menu over the game: no active hand, no button", 0, 0);
  Check(menu.hands[0].serial == 40 && menu.hands[1].serial == 41 &&
            menu.eyes[0].position[0] == -0.032f,
        "the serials and the views stay", 0, 0);
}

inline void QuitChecks() {
  printf("\nPlay: Quit game in the menu\n");
  using namespace menu_model;
  Menu menu;
  menu.Load(bwvr_ini::IniText("[bwvr]\r\n"));
  std::vector<Item> items = menu.Items();
  Check(items[0].kind == ItemKind::kPlay && items[1].kind == ItemKind::kNewGame,
        "no game: Play in VR and Play, new game", 0, 0);
  const size_t rows = items.size();
  menu.SetCursor(3);
  menu.Select();
  Check(!menu.OnMain(), "a group page is open", 0, 0);
  menu.SetGameRunning(true);
  items = menu.Items();
  Check(menu.OnMain() && menu.Cursor() == 0, "the start of a game goes to the main page", 0, 0);
  Check(items[0].kind == ItemKind::kQuit && menu.Label(items[0]) == "Quit game" &&
            items.size() == rows - 1,
        "the game runs: Quit game in place of the two plays", static_cast<double>(items.size()),
        static_cast<double>(rows - 1));
  Check(menu.Select() == Action::kQuit, "a select on Quit game", 0, 0);
  Check(!menu.Help(items[0]).empty(), "Quit game has a help text", 0, 0);
  bool play = false;
  for (const Item& item : items) {
    if (item.kind == ItemKind::kPlay || item.kind == ItemKind::kNewGame) play = true;
  }
  Check(!play, "no Play while the game runs", 0, 0);
  menu.SetGameRunning(false);
  Check(menu.Items()[0].kind == ItemKind::kPlay, "the end of the game: Play again", 0, 0);
}

inline void PlayRulesChecks() {
  RunKeyChecks();
  RegistryChecks();
  GameStartChecks();
  MaskChecks();
  QuitChecks();
}

}  // namespace play_rules_checks
