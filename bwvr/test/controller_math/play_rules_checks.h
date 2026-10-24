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
  Check(ini.GetInt("bwvr", "vk_pace", 0) == 1, "vk_pace=1: the frames wait for the app", 0, 0);
  Check(ini.GetInt("bwvr", "stream_live", 0) == 1 && ini.GetInt("bwvr", "input_live", 0) == 1,
        "stream_live=1 and input_live=1 for the app", 0, 0);
  Check(ini.GetInt("bwvr", "vulkan_strict", 0) == 1 && ini.Get("bwvr", "backend", "") == "vulkan",
        "the save gives vulkan_strict=1 and backend=vulkan", 0, 0);
  Check(ini.GetInt("bwvr", "keep_me", 0) == 7, "another key stays", 0, 0);
  Check(play_rules::kRunKeyCount == 7, "seven keys of a run", play_rules::kRunKeyCount, 7);
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
  const std::string reg = play_rules::GameRegistry(1380, 1104, true);
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
  Check(reg.find("[HKEY_CURRENT_USER\\Software\\Wine\\Drivers]\r\n\"Audio\"=\"aaudio\"\r\n") !=
            std::string::npos,
        "wine_audio=1: the audio driver of Wine is aaudio", 0, 0);
  const std::string quiet = play_rules::GameRegistry(1380, 1104, false);
  Check(quiet.find("[HKEY_CURRENT_USER\\Software\\Wine\\Drivers]\r\n\"Audio\"=-\r\n") !=
            std::string::npos,
        "wine_audio=0: reg import deletes the Audio value (no audio driver)", 0, 0);
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

// The slots of the menu button: one each 14 ms, from the time 0. Gives the
// count of the slots with the menu bit for the mod, the first and the last
// of them, and the actions.
struct MenuRun {
  int mod_slots = 0;
  int first = -1;
  int last = -1;
  int shows = 0;
  int hides = 0;
};

inline MenuRun RunMenu(play_rules::MenuButtonRule* r, int press_from, int press_to,
                       int slots, bool* menu_over, int hold_ms, bool hold_back,
                       int hand = 0) {
  MenuRun run;
  for (int i = 0; i < slots; ++i) {
    bool down[2] = {false, false};
    down[hand] = i >= press_from && i < press_to;
    const play_rules::MenuStep s =
        play_rules::StepMenuButton(r, down, i * 14.0, *menu_over, hold_ms, hold_back);
    if (s.action == play_rules::MenuAction::kShow) {
      ++run.shows;
      *menu_over = true;
    }
    if (s.action == play_rules::MenuAction::kHide) {
      ++run.hides;
      *menu_over = false;
    }
    if (s.mod_menu[0] || s.mod_menu[1]) {
      if (!s.mod_menu[hand]) run.mod_slots = -1000;  // the bit on the other hand
      ++run.mod_slots;
      if (run.first < 0) run.first = i;
      run.last = i;
    }
  }
  return run;
}

inline void MenuButtonChecks() {
  printf("\nPlay: the menu button while the game runs\n");
  using play_rules::MenuButtonRule;
  {
    // A short press (slots 10 to 19, 140 ms): nothing while it is down, then
    // the mod gets it at its release for two slots.
    MenuButtonRule r;
    bool over = false;
    const MenuRun run = RunMenu(&r, 10, 20, 40, &over, 1000, true);
    Check(run.mod_slots == 2 && run.first == 20 && run.last == 21,
          "a short press goes to the mod at its release, down for two slots",
          run.first, 20);
    Check(run.shows == 0 && !over, "a short press shows no menu", run.shows, 0);
  }
  {
    // The right hand presses: the bit goes to the right hand.
    MenuButtonRule r;
    bool over = false;
    const MenuRun run = RunMenu(&r, 5, 8, 20, &over, 1000, true, 1);
    Check(run.mod_slots == 2 && run.first == 8, "the press goes to the hand that pressed",
          run.mod_slots, 2);
  }
  {
    // A hold of 90 slots (1260 ms): the menu shows at 1000 ms, and the mod
    // gets nothing, also not at the release.
    MenuButtonRule r;
    bool over = false;
    const MenuRun run = RunMenu(&r, 10, 100, 130, &over, 1000, true);
    Check(run.shows == 1 && over, "a hold of 1260 ms shows the menu", run.shows, 1);
    Check(run.mod_slots == 0, "a hold never reaches the mod", run.mod_slots, 0);
    // Then a press while the menu shows: it hides the menu, and the mod gets
    // nothing.
    const MenuRun hide = RunMenu(&r, 5, 12, 40, &over, 1000, true);
    Check(hide.hides == 1 && !over, "a press while the menu shows hides it", hide.hides, 1);
    Check(hide.mod_slots == 0, "the press that hides the menu never reaches the mod",
          hide.mod_slots, 0);
    // The next short press goes to the mod again.
    const MenuRun again = RunMenu(&r, 5, 10, 30, &over, 1000, true);
    Check(again.mod_slots == 2 && again.first == 10, "then a short press goes to the mod again",
          again.first, 10);
  }
  {
    // A press of 999 ms is short, one of 1000 ms is a hold.
    MenuButtonRule r;
    bool over = false;
    MenuRun run = RunMenu(&r, 0, 71, 80, &over, 1000, true);  // 70 * 14 = 980 ms
    Check(run.shows == 0 && run.mod_slots == 2, "a press of 980 ms is short", run.shows, 0);
    MenuButtonRule r2;
    run = RunMenu(&r2, 0, 73, 80, &over, 1000, true);  // 72 * 14 = 1008 ms
    Check(run.shows == 1 && run.mod_slots == 0, "a press of 1008 ms is a hold", run.shows, 1);
  }
  {
    // play_menu_press=0 (the old way): each press goes to the mod at once,
    // and a hold shows the menu too.
    MenuButtonRule r;
    bool over = false;
    const MenuRun run = RunMenu(&r, 10, 100, 130, &over, 1000, false);
    Check(run.mod_slots == 90 && run.first == 10 && run.shows == 1,
          "play_menu_press=0: the mod gets each slot of the press at once", run.mod_slots, 90);
  }
  {
    // play_menu_hold_ms=0: no menu over the game, and each press at once.
    MenuButtonRule r;
    bool over = false;
    const MenuRun run = RunMenu(&r, 10, 100, 130, &over, 0, true);
    Check(run.shows == 0 && run.mod_slots == 90 && run.first == 10,
          "play_menu_hold_ms=0: no menu, each press at once", run.mod_slots, 90);
  }
  {
    namespace ic = input_channel;
    ic::SlotData slot;
    memset(&slot, 0, sizeof(slot));
    slot.hands[0].flags = ic::kActive | ic::kMenu | ic::kPrimary;
    slot.hands[1].flags = ic::kActive;
    play_rules::MenuStep held;
    play_rules::ApplyMenuBits(&slot, held);
    Check(slot.hands[0].flags == (ic::kActive | ic::kPrimary),
          "the app holds the menu bit back, the other bits stay", 0, 0);
    play_rules::MenuStep give;
    give.mod_menu[1] = true;
    play_rules::ApplyMenuBits(&slot, give);
    Check((slot.hands[1].flags & ic::kMenu) != 0 && (slot.hands[0].flags & ic::kMenu) == 0,
          "the press goes into the slot of its hand", 0, 0);
  }
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
  const size_t quit = items.size() - 2;
  Check(items[quit].kind == ItemKind::kQuit && menu.Label(items[quit]) == "Quit game" &&
            items.back().kind == ItemKind::kExit && items.size() == rows - 1,
        "the game runs: no plays, Quit game above Exit at the bottom",
        static_cast<double>(items.size()), static_cast<double>(rows - 1));
  menu.SetCursor(static_cast<int>(quit));
  Check(menu.Select() == Action::kQuit, "a select on Quit game", 0, 0);
  Check(!menu.Help(items[quit]).empty(), "Quit game has a help text", 0, 0);
  bool play = false;
  for (const Item& item : items) {
    if (item.kind == ItemKind::kPlay || item.kind == ItemKind::kNewGame) play = true;
  }
  Check(!play, "no Play while the game runs", 0, 0);
  menu.SetCursor(static_cast<int>(items.size()) - 1);
  Check(menu.Select() == Action::kExit, "a select on Exit while the game runs", 0, 0);
  menu.SetGameRunning(false);
  items = menu.Items();
  Check(items[0].kind == ItemKind::kPlay, "the end of the game: Play again", 0, 0);
  Check(items.back().kind == ItemKind::kExit && menu.Label(items.back()) == "Exit" &&
            !menu.Help(items.back()).empty(),
        "no game: Exit is the last row, with a help text", 0, 0);
  menu.SetCursor(static_cast<int>(items.size()) - 1);
  Check(menu.Select() == Action::kExit, "a select on Exit", 0, 0);
}

inline void PlayRulesChecks() {
  RunKeyChecks();
  RegistryChecks();
  GameStartChecks();
  MaskChecks();
  MenuButtonChecks();
  QuitChecks();
}

}  // namespace play_rules_checks
