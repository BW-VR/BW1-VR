// The rules of "Play" in the headset app, a test (android/src/play_rules.h):
// the keys of a run after a save of the menu, the registry of the game, the
// environment and the command line of the game, the input of the mod while
// the menu shows over the game, "Quit game" in the menu, and the filter and
// the form of the file of the profile of the PC. main.cpp
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

// The two sticks (play_menu_sticks): slots of 14 ms. left and right give
// the slots in which each stick is down, as [from, to).
inline int RunChord(play_rules::StickChordRule* r, int l_from, int l_to, int r_from, int r_to,
                    int slots, bool* over, int hold_ms, int* hides) {
  int shows = 0;
  for (int i = 0; i < slots; ++i) {
    const bool click[2] = {i >= l_from && i < l_to, i >= r_from && i < r_to};
    const play_rules::MenuAction a =
        play_rules::StepStickChord(r, click, i * 14.0, *over, hold_ms);
    if (a == play_rules::MenuAction::kShow) { ++shows; *over = true; }
    if (a == play_rules::MenuAction::kHide) { ++*hides; *over = false; }
  }
  return shows;
}

inline void StickChordChecks() {
  printf("\nPlay: the clicks of both sticks while the game runs\n");
  using play_rules::StickChordRule;
  {
    StickChordRule r;
    bool over = false;
    int hides = 0;
    const int shows = RunChord(&r, 10, 100, 12, 100, 120, &over, 1000, &hides);
    Check(shows == 1 && over, "both sticks held for 1232 ms show the menu one time", shows, 1);
  }
  {
    StickChordRule r;
    bool over = false;
    int hides = 0;
    const int shows = RunChord(&r, 10, 60, 12, 60, 120, &over, 1000, &hides);
    Check(shows == 0 && !over, "both sticks held for 672 ms show no menu", shows, 0);
  }
  {
    StickChordRule r;
    bool over = false;
    int hides = 0;
    const int shows = RunChord(&r, 10, 200, 300, 300, 320, &over, 1000, &hides);
    Check(shows == 0, "one stick held for 2660 ms shows no menu", shows, 0);
  }
  {
    // The time counts from the second stick: left at slot 0, right at slot
    // 50 (700 ms later), both up at slot 100 (700 ms with both down).
    StickChordRule r;
    bool over = false;
    int hides = 0;
    const int shows = RunChord(&r, 0, 100, 50, 100, 120, &over, 1000, &hides);
    Check(shows == 0, "the hold counts from the second stick", shows, 0);
  }
  {
    // The menu shows. The next press of both hides it at once.
    StickChordRule r;
    bool over = true;
    int hides = 0;
    const int shows = RunChord(&r, 5, 8, 5, 8, 20, &over, 1000, &hides);
    Check(hides == 1 && shows == 0 && !over, "a press of both sticks hides the menu", hides, 1);
  }
  {
    StickChordRule r;
    bool over = false;
    int hides = 0;
    const int shows = RunChord(&r, 10, 200, 10, 200, 220, &over, 0, &hides);
    Check(shows == 0 && hides == 0, "play_menu_hold_ms=0: the sticks show no menu", shows, 0);
  }
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

// A file of push_profile.ps1 for the profile "Ann", with keys and values
// that the filter must drop.
inline std::string ProfileText() {
  return "Windows Registry Editor Version 5.00\r\n"
         "\r\n"
         "; a comment\r\n"
         "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White]\r\n"
         "\"GameDir\"=\"C:\\\\Program Files (x86)\\\\Black & White\"\r\n"
         "\r\n"
         "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\BWSetup]\r\n"
         "\"ScreenW\"=dword:00000ff0\r\n"
         "\r\n"
         "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\LHMultiplayer]\r\n"
         "\"Profile\"=hex:41,00,6e,00,6e,00,00,00\r\n"
         "\"Other\"=dword:00000001\r\n"
         "\r\n"
         "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\LHMultiplayer\\"
         "Profiles\\_A_n_n]\r\n"
         "\"file\"=\"Cbf0dfe1f.erc\"\r\n"
         "\"creature name\"=hex:41,00,6e,00,6e,00,00,00\r\n"
         "\"login name\"=hex:00,00\r\n"
         "\"login password\"=hex:c8,8b\r\n"
         "\"stuff\"=dword:00000000\r\n"
         "\"Key 0 Primary\"=dword:0000003b\r\n"
         "\"READ_SPEED\"=hex:00,00,00,00,00,00,e0,3f\r\n"
         "\"a \\\"quoted\\\" name\"=\"a \\\\ text\"\r\n"
         "\"gone\"=-\r\n"
         "@=\"default\"\r\n"
         "\r\n"
         "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\LHMultiplayer\\"
         "Profiles\\_B_o_b]\r\n"
         "\"stuff\"=dword:00000001\r\n"
         "\r\n"
         "[HKEY_LOCAL_MACHINE\\Software\\Other]\r\n"
         "\"x\"=dword:00000001\r\n"
         "[-HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\LHMultiplayer\\"
         "Profiles\\_A_n_n\\Sub]\r\n";
}

// The text with one part replaced.
inline std::string Swap(std::string text, const std::string& from, const std::string& to) {
  const size_t at = text.find(from);
  if (at != std::string::npos) text.replace(at, from.size(), to);
  return text;
}

inline void ProfileChecks() {
  printf("\nPlay: the profile of the PC (bwvr-profile.reg)\n");
  using play_rules::ParseProfileFile;
  const std::string text = ProfileText();
  const play_rules::ProfileFile f = ParseProfileFile(text);
  Check(f.ok && f.name == "Ann" && f.folder == "_A_n_n", "a right file: the profile Ann, key _A_n_n",
        f.ok ? 1 : 0, 1);
  Check(f.values.size() == 7, "the seven values of the key of the profile",
        static_cast<double>(f.values.size()), 7);
  // The key of the game with GameDir, BWSetup with ScreenW, Other, login
  // password, the delete of a value, the default value, the profile Bob with
  // its value, the key of HKEY_LOCAL_MACHINE with its value, the delete of a
  // key.
  Check(f.dropped == 13, "the filter drops each other key and value, and each delete", f.dropped,
        13);
  bool secret = false;
  for (const play_rules::ProfileValue& v : f.values) {
    if (v.name == "login password") secret = true;
  }
  Check(!secret && play_rules::ProfileRegistry(f).find("login password") == std::string::npos,
        "the filter drops login password (a secret of the player)", 0, 0);
  const play_rules::ProfileFile upper = ParseProfileFile(
      Swap(text, "\"login password\"=hex:c8,8b", "\"LOGIN PASSWORD\"=hex:c8,8b"));
  Check(upper.ok && upper.values.size() == 7 && upper.dropped == 13,
        "login password in another case is dropped too", static_cast<double>(upper.values.size()),
        7);
  Check(f.values[0].name == "file" && f.values[0].data == "\"Cbf0dfe1f.erc\"" &&
            f.values[5].name == "READ_SPEED" &&
            f.values[6].name == "a \\\"quoted\\\" name" && f.values[6].data == "\"a \\\\ text\"",
        "the names and the data stay as the file has them, with their escapes", 0, 0);
  const std::string reg = play_rules::ProfileRegistry(f);
  Check(reg.find("[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\"
                 "LHMultiplayer]\r\n\"Profile\"=hex:41,00,6e,00,6e,00,00,00\r\n") !=
            std::string::npos,
        "the import has the value Profile of LHMultiplayer", 0, 0);
  Check(reg.find("\\LHMultiplayer\\Profiles\\_A_n_n]\r\n\"file\"=\"Cbf0dfe1f.erc\"\r\n") !=
                std::string::npos &&
            reg.find("\"Key 0 Primary\"=dword:0000003b\r\n") != std::string::npos,
        "the import has the values of the key of the profile", 0, 0);
  Check(reg.find("GameDir") == std::string::npos && reg.find("BWSetup") == std::string::npos &&
            reg.find("Screen") == std::string::npos && reg.find("Other") == std::string::npos &&
            reg.find("_B_o_b") == std::string::npos &&
            reg.find("HKEY_LOCAL_MACHINE") == std::string::npos &&
            reg.find("=-") == std::string::npos && reg.find("@=") == std::string::npos &&
            reg.find("[-") == std::string::npos,
        "the import has no GameDir, no value of BWSetup, no other key, and no delete", 0, 0);
  const std::string all = play_rules::GameRegistry(1380, 1104, true) + reg;
  size_t game_dirs = 0;
  for (size_t at = all.find("\"GameDir\""); at != std::string::npos;
       at = all.find("\"GameDir\"", at + 1)) {
    ++game_dirs;
  }
  Check(game_dirs == 1 && all.find("\"GameDir\"=\"G:\\\\\"") != std::string::npos &&
            all.find("\"ScreenW\"=dword:00000564") != std::string::npos &&
            all.find("dword:00000ff0") == std::string::npos,
        "after GameRegistry: one GameDir (G:\\), and the screen of the menu", 0, 0);
  Check(play_rules::ProfileRegistry(ParseProfileFile("")).empty(),
        "a file that is not right gives no import", 0, 0);

  // The keys of the registry have no case.
  const play_rules::ProfileFile lower = ParseProfileFile(
      Swap(Swap(text, "Lionhead Studios Ltd\\Black & White\\LHMultiplayer]",
                "lionhead studios ltd\\black & white\\lhmultiplayer]"),
           "Profiles\\_A_n_n]", "PROFILES\\_a_N_N]"));
  Check(lower.ok && lower.name == "Ann" && lower.folder == "_a_N_N" && lower.values.size() == 7,
        "a key in another case is the same key", lower.ok ? 1 : 0, 1);
  const play_rules::ProfileFile lf = ParseProfileFile(Swap(text, "\r\n", "\n"));
  Check(lf.ok && lf.values.size() == 7, "a file with LF lines", 0, 0);

  // The files that are not right.
  struct Bad {
    std::string text;
    const char* what;
  };
  const Bad bad[] = {
      {"", "an empty file"},
      {Swap(text, "Windows Registry Editor Version 5.00", "REGEDIT4"), "another first line"},
      {"\xEF\xBB\xBF" + text, "a UTF-8 BOM (not ASCII)"},
      {Swap(text, "\"file\"=\"Cbf0dfe1f.erc\"", "\"file\"=\"C\xFC.erc\""),
       "a character outside ASCII"},
      {Swap(text, "dword:0000003b", "dword:3b"), "a dword with no 8 digits"},
      {Swap(text, "hex:00,00\r\n", "hex:00,0\r\n"), "a byte with one digit"},
      {Swap(text, "hex:00,00\r\n", "hex:00,00,\\\r\n  00,00\r\n"),
       "a value on two lines (the export of regedit)"},
      {Swap(text, "\"stuff\"=dword:00000000", "\"stuff\"=qword:0"), "a form of data that is not known"},
      {Swap(text, "\"stuff\"=dword:00000000", "\"stuff\"dword:00000000"), "no = after the name"},
      {Swap(text, "\"stuff\"=dword:00000000", "\"stu\\ff\"=dword:00000000"),
       "a backslash before a letter in a name"},
      {Swap(text, "\"Profile\"=hex:41,00,6e,00,6e,00,00,00", "\"Profil\"=hex:41,00"),
       "no value Profile"},
      {Swap(text, "\"Profile\"=hex:41,00,6e,00,6e,00,00,00", "\"Profile\"=\"Ann\""),
       "a value Profile that is a text"},
      {Swap(text, "\"Profile\"=hex:41,00,6e,00,6e,00,00,00", "\"Profile\"=hex:5c,00,00,00"),
       "a name with a backslash"},
      {Swap(text, "\"Profile\"=hex:41,00,6e,00,6e,00,00,00", "\"Profile\"=hex:41,00,6e"),
       "an odd count of bytes"},
      {Swap(text, "\"Profile\"=hex:41,00,6e,00,6e,00,00,00", "\"Profile\"=hex:00,00"),
       "an empty name"},
      {Swap(text, "Profiles\\_A_n_n]", "Profiles\\_A_n]"), "no key of the profile"},
      {Swap(text, "Profiles\\_A_n_n]", "Profiles\\_A_n_n\\Deeper]"),
       "the values in a key under the key of the profile"},
      {"Windows Registry Editor Version 5.00\r\n\"x\"=dword:00000001\r\n",
       "a value before the first key"},
      {Swap(text, "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White]",
            "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White"),
       "a key with no ]"},
      {std::string("Windows Registry Editor Version 5.00\r\n") +
           "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\LHMultiplayer]\r\n"
           "\"Profile\"=hex:41,00,00,00\r\n"
           "[HKEY_CURRENT_USER\\Software\\Lionhead Studios Ltd\\Black & White\\LHMultiplayer\\"
           "Profiles\\_A]\r\n\"gone\"=-\r\n",
       "a key of the profile with no value"},
      {std::string(play_rules::kProfileFileMost + 1, ';'), "a file of more than 256 KB"},
  };
  for (const Bad& b : bad) {
    const play_rules::ProfileFile p = ParseProfileFile(b.text);
    std::string what = std::string("not right: ") + b.what;
    if (!p.ok) what += " (" + p.why + ")";
    Check(!p.ok && !p.why.empty() && p.name.empty(), what.c_str(), p.ok ? 1 : 0, 0);
  }

  // The name in the value Profile and the key of a profile.
  std::string name;
  Check(play_rules::ProfileNameFromData("hex:4b,00,69,00,6d,00,00,00", &name) && name == "Kim",
        "the value Profile hex:4b,00,69,00,6d,00,00,00 gives Kim", 0, 0);
  Check(play_rules::ProfileNameFromData("hex:54,00,69,00", &name) && name == "Ti",
        "a name with no zero at its end", 0, 0);
  Check(!play_rules::ProfileNameFromData("hex:fc,00,00,00", &name) &&
            !play_rules::ProfileNameFromData("hex:41,01,00,00", &name),
        "no name with a character outside ASCII", 0, 0);
  Check(play_rules::ProfileFolder("Kim") == "_K_i_m" &&
            play_rules::ProfileFolder("bwvrtest") == "_b_w_v_r_t_e_s_t",
        "the key of a profile: a _ before each character", 0, 0);

  // One import for each new file.
  const std::string stamp = play_rules::ProfileStamp(text);
  Check(stamp == play_rules::ProfileStamp(ProfileText()) &&
            stamp != play_rules::ProfileStamp(Swap(text, "dword:0000003b", "dword:0000003c")),
        "the stamp: the same file the same stamp, a changed byte a new one", 0, 0);
  // Each copy of push_profile.ps1 has its own line "; copy <guid>": a second
  // copy with the same values is a new file, and Play imports it again.
  const std::string header = "Windows Registry Editor Version 5.00\r\n";
  const std::string copy1 =
      Swap(text, header, header + "; copy 3f2a7c1e-0d4b-4a8e-9b6f-1c2d3e4f5a6b\r\n");
  const std::string copy2 =
      Swap(text, header, header + "; copy 8e9d0c1b-2a3f-4e5d-8c7b-6a5f4e3d2c1b\r\n");
  const play_rules::ProfileFile c1 = ParseProfileFile(copy1);
  const play_rules::ProfileFile c2 = ParseProfileFile(copy2);
  Check(c1.ok && c2.ok && c1.values.size() == 7 && c1.dropped == f.dropped,
        "the line \"; copy <guid>\" is a comment: the same values", 0, 0);
  Check(play_rules::ProfileStamp(copy1) != play_rules::ProfileStamp(copy2) &&
            play_rules::NeedsProfileImport(c2, play_rules::ProfileStamp(copy2),
                                           play_rules::ProfileStamp(copy1) + "\n"),
        "a second copy with the same values has a new stamp: Play imports it again", 0, 0);
  Check(play_rules::NeedsProfileImport(f, stamp, ""), "a file with no import before: Play imports it",
        0, 0);
  Check(!play_rules::NeedsProfileImport(f, stamp, stamp + "\n"),
        "the stamp of the last import: Play imports nothing", 0, 0);
  Check(play_rules::NeedsProfileImport(f, stamp, "0000000000000000 1\n"),
        "a new file: Play imports it again", 0, 0);
  Check(!play_rules::NeedsProfileImport(ParseProfileFile("x"), stamp, ""),
        "a file that is not right: no import", 0, 0);

  // The profile in user.reg of the prefix, as Wine writes it.
  const std::string user_reg =
      "WINE REGISTRY Version 2\n"
      ";; All keys relative to \\\\User\\\\S-1-5-21-0-0-0-1000\n\n"
      "[Software\\\\Lionhead Studios Ltd\\\\Black & White\\\\LHMultiplayer\\\\Profiles\\\\_x] "
      "1729760000\n"
      "\"Profile\"=hex:78,00,00,00\n\n"
      "[Software\\\\Lionhead Studios Ltd\\\\Black & White\\\\LHMultiplayer] 1729760001\n"
      "#time=1db2a6f3c2b1e40\n"
      "\"Profile\"=hex:62,00,77,00,76,00,72,00,74,00,65,00,73,00,74,00,00,00\n\n";
  Check(play_rules::PrefixProfileName(user_reg) == "bwvrtest",
        "user.reg of the prefix: the profile bwvrtest", 0, 0);
  const std::string long_reg = Swap(user_reg, "hex:62,00,77,00,76,00,72,00,74,00,65,00,73,00,",
                                    "hex:62,00,77,00,76,00,72,00,\\\n  74,00,65,00,73,00,");
  Check(play_rules::PrefixProfileName(long_reg) == "bwvrtest",
        "user.reg of the prefix: a value on two lines", 0, 0);
  Check(play_rules::PrefixProfileName("WINE REGISTRY Version 2\n").empty() &&
            play_rules::PrefixProfileName(Swap(user_reg, "\\\\LHMultiplayer] 1729760001",
                                               "\\\\LHMultiplayerX] 1729760001"))
                .empty(),
        "no profile in a prefix with no key LHMultiplayer, and not from a key under it", 0, 0);

  // The help of Play.
  const play_rules::ProfileFile none = ParseProfileFile("");
  Check(play_rules::ProfileHelp(true, true, f, false, "") ==
            "The profile of the game: Ann, from the PC. Play puts it into the game.",
        "help: a new file of the PC", 0, 0);
  Check(play_rules::ProfileHelp(true, true, f, true, "Ann") ==
            "The profile of the game: Ann, from the PC.",
        "help: the file of the PC after its import", 0, 0);
  Check(play_rules::ProfileHelp(false, true, f, false, "bwvrtest") ==
            "The profile of the game: bwvrtest.",
        "help with pc_profile=0: the profile of the prefix", 0, 0);
  Check(play_rules::ProfileHelp(true, false, none, false, "") ==
            "No profile: the game asks for a new one.",
        "help: no file and no profile in the prefix", 0, 0);
  const std::string wrong =
      play_rules::ProfileHelp(true, true, ParseProfileFile("x"), false, "bwvrtest");
  Check(wrong.compare(0, 31, "bwvr-profile.reg is not right: ") == 0 &&
            wrong.find("The profile of the game: bwvrtest.") != std::string::npos,
        "help: a file that is not right, and the profile of the prefix", 0, 0);

  menu_model::Menu menu;
  menu.Load(bwvr_ini::IniText("[bwvr]\r\n"));
  const std::vector<menu_model::Item> items = menu.Items();
  const std::string plain = menu.Help(items[0]);
  const unsigned revision = menu.Revision();
  menu.SetProfile("The profile of the game: Ann, from the PC.");
  Check(menu.Help(items[0]) == plain + " The profile of the game: Ann, from the PC." &&
            menu.Help(items[1]).find("Ann, from the PC.") != std::string::npos &&
            menu.Revision() != revision,
        "the help of Play in VR and of Play, new game names the profile", 0, 0);
}

inline void PlayRulesChecks() {
  RunKeyChecks();
  RegistryChecks();
  GameStartChecks();
  MaskChecks();
  MenuButtonChecks();
  StickChordChecks();
  QuitChecks();
  ProfileChecks();
}

}  // namespace play_rules_checks
