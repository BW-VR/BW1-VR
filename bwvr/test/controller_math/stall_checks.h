// The module list at a stall and the variables of Box64, a test: the parse
// of stall_addrs, the watch of a stall and the test spin of the mod
// (src/mod/stall_math.h), the filter of box64_env and the environment of the
// game (android/src/play_rules.h), and the row of the memory order of Box64
// in the menu of the headset app (android/src/menu_model.h). main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../../android/src/menu_model.h"
#include "../../android/src/play_rules.h"
#include "../../src/mod/stall_math.h"

namespace stall_checks {

inline void AddrChecks() {
  namespace sm = stall_math;
  printf("\nThe module list at a stall: stall_addrs\n");
  uint32_t a[sm::kMaxAddrs];
  int bad = -1;
  int n = sm::ParseAddrs("", a, sm::kMaxAddrs, &bad);
  Check(n == 0 && bad == 0, "an empty stall_addrs: no address", n, 0);
  n = sm::ParseAddrs("000000007AF441E8", a, sm::kMaxAddrs, &bad);
  Check(n == 1 && a[0] == 0x7AF441E8u && bad == 0,
        "16 digits as Wine writes them: 0x7AF441E8", n, 1);
  n = sm::ParseAddrs(" 0x7B001000, 7af441e8  0X10 ", a, sm::kMaxAddrs, &bad);
  Check(n == 3 && a[0] == 0x7B001000u && a[1] == 0x7AF441E8u && a[2] == 0x10u && bad == 0,
        "spaces and commas, 0x and 0X, small letters", n, 3);
  n = sm::ParseAddrs("0x1G zz 100000000 0x 7B000000", a, sm::kMaxAddrs, &bad);
  Check(n == 1 && a[0] == 0x7B000000u && bad == 4,
        "a word with no hexadecimal digits, more than 32 bits or no digits after 0x does "
        "not go",
        bad, 4);
  std::string many;
  for (int i = 0; i < sm::kMaxAddrs + 3; ++i) many += "1000 ";
  n = sm::ParseAddrs(many.c_str(), a, sm::kMaxAddrs, &bad);
  Check(n == sm::kMaxAddrs && bad == 3, "at most 16 addresses: the rest is bad", bad, 3);
}

inline void WatchChecks() {
  namespace sm = stall_math;
  printf("\nThe module list at a stall: the watch\n");
  sm::Watch w;
  Check(!w.Step(0, 1000) && !w.Step(0, 9000), "no frame yet: no stall, also after 8 s", 0, 0);
  Check(!w.Step(5, 10000), "the first frames: the watch starts", 0, 0);
  Check(!w.Step(5, 14900), "4.9 s with no new frame: no stall", 0, 0);
  Check(w.Step(5, 15000), "5.0 s with no new frame: a stall", 0, 0);
  Check(!w.Step(5, 16000) && !w.Step(5, 60000), "one time for each stall", 0, 0);
  Check(!w.Step(6, 61000), "a new frame ends the stall", 0, 0);
  Check(!w.Step(6, 65000) && w.Step(6, 66000), "the next stall comes 5 s after its last frame",
        0, 0);
  int count = 0;
  sm::Watch run;
  for (uint32_t t = 1; t <= 600; ++t) {
    // A frame each 500 ms for 300 s: no stall.
    if (run.Step(t, 500ull * t)) ++count;
  }
  Check(count == 0, "frames each 500 ms for 300 s: no stall", count, 0);

  printf("\nThe test spin (test_stall_ms)\n");
  sm::Spin off;
  Check(!off.Due(0.0) && !off.Due(1e6), "test_stall_ms=0: no spin", 0, 0);
  sm::Spin s;
  s.ms = 8000;
  Check(!s.Due(100.0), "the first scene walk: no spin, the time starts", 0, 0);
  Check(!s.Due(20099.0), "19.999 s after it: no spin", 0, 0);
  Check(s.Due(20100.0), "20 s after the first scene walk: the spin", 0, 0);
  Check(!s.Due(20200.0) && !s.Due(90000.0), "one time", 0, 0);
}

inline void Box64Checks() {
  namespace pr = play_rules;
  printf("\nThe variables of Box64 for the game (box64_strongmem, box64_env)\n");
  pr::Box64Env e = pr::Box64Vars(0, "");
  Check(e.vars.empty() && e.refused.empty(), "box64_strongmem=0 and no box64_env: no variable",
        static_cast<double>(e.vars.size()), 0);
  for (int n = 1; n <= 3; ++n) {
    e = pr::Box64Vars(n, "");
    Check(e.vars.size() == 1 && e.vars[0] == "BOX64_DYNAREC_STRONGMEM=" + std::to_string(n),
          "box64_strongmem=1 to 3: BOX64_DYNAREC_STRONGMEM=<n>", n, n);
  }
  e = pr::Box64Vars(4, "");
  Check(e.vars.empty(), "box64_strongmem=4 is not a value of the key: no variable", 0, 0);
  e = pr::Box64Vars(2, "BOX64_DYNAREC_BIGBLOCK=0 FOO=1");
  Check(e.vars.size() == 2 && e.vars[0] == "BOX64_DYNAREC_STRONGMEM=2" &&
            e.vars[1] == "BOX64_DYNAREC_BIGBLOCK=0",
        "the test of the brief: STRONGMEM=2 and BIGBLOCK=0 go", static_cast<double>(e.vars.size()),
        2);
  Check(e.refused.size() == 1 && e.refused[0].compare(0, 5, "FOO=1") == 0,
        "FOO=1 does not go, with one line", static_cast<double>(e.refused.size()), 1);
  e = pr::Box64Vars(0, "\tBOX64_LOG=1  box64_log=1 BOX64_=1 BOX64_X BOX64_LOG=2 WINEDEBUG=+all ");
  Check(e.vars.size() == 1 && e.vars[0] == "BOX64_LOG=1" && e.refused.size() == 5,
        "only BOX64_ and capitals, a name with =VALUE, one time. Tabs and spaces go "
        "between the words",
        static_cast<double>(e.refused.size()), 5);
  e = pr::Box64Vars(0, "BOX64_DYNAREC_STRONGMEM=1");
  Check(e.vars.size() == 1 && e.vars[0] == "BOX64_DYNAREC_STRONGMEM=1",
        "box64_strongmem=0: box64_env can set BOX64_DYNAREC_STRONGMEM", 0, 0);
  e = pr::Box64Vars(3, "BOX64_DYNAREC_STRONGMEM=1");
  Check(e.vars.size() == 1 && e.vars[0] == "BOX64_DYNAREC_STRONGMEM=3" && e.refused.size() == 1,
        "box64_strongmem=3 sets it, and box64_env does not", 0, 0);

  const std::vector<std::string> env = {"PATH=/system/bin", "BOX64_LOG=0",
                                        "WINEDLLOVERRIDES=mscoree,mshtml="};
  const std::vector<std::string> game =
      pr::GameEnvironment(env, {"BOX64_DYNAREC_STRONGMEM=2", "BOX64_LOG=1"});
  Check(game.size() == 4 && game[0] == "PATH=/system/bin" &&
            game[1] == "WINEDLLOVERRIDES=mscoree,mshtml=;ddraw=n" &&
            game[2] == "BOX64_DYNAREC_STRONGMEM=2" && game[3] == "BOX64_LOG=1",
        "the environment of the game: the variables of Box64 at the end, in place of an "
        "inherited variable with the same name",
        static_cast<double>(game.size()), 4);
  const std::vector<std::string> plain = pr::GameEnvironment(env);
  Check(plain.size() == 3 && plain[1] == "BOX64_LOG=0",
        "no variables of Box64: the environment is as before", static_cast<double>(plain.size()),
        3);

  printf("\nThe row of the memory order of Box64 in the menu\n");
  menu_model::Menu menu;
  menu.Load(bwvr_ini::IniText(""));
  Check(menu.StrongMem() == 0, "no key: Off", menu.StrongMem(), 0);
  menu.Load(bwvr_ini::IniText("[bwvr]\r\nbox64_strongmem=9\r\n"));
  Check(menu.StrongMem() == 0, "a value out of 0 to 3: Off", menu.StrongMem(), 0);
  menu.Load(bwvr_ini::IniText("[bwvr]\r\nbox64_strongmem=2\r\n"));
  Check(menu.StrongMem() == 2, "box64_strongmem=2 loads", menu.StrongMem(), 2);
  // The page "Testing".
  int row = -1;
  std::vector<menu_model::Item> items = menu.Items();
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (items[r].kind == menu_model::ItemKind::kGroup &&
        menu.Label(items[r]) == std::string("Testing")) {
      row = r;
    }
  }
  Check(row >= 0, "the main page has the group Testing", row, 0);
  menu.SetCursor(row);
  Check(menu.Help(menu.Items()[row]).find("Strict memory order of the game") !=
            std::string::npos,
        "the help of the group names the row", 0, 0);
  menu.Select();
  items = menu.Items();
  int at = -1;
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (items[r].kind == menu_model::ItemKind::kStrongMem) at = r;
  }
  Check(at >= 0 && at + 1 == static_cast<int>(items.size()) - 1,
        "the page Testing has the row, above Back", at, 0);
  const menu_model::Item item = items[at];
  Check(menu.Label(item) == "Strict memory order of the game (Box64, slower)" &&
            menu.ValueText(item) == "2: also SIMD",
        "its label and its value", 0, 0);
  Check(menu.Help(item).find("stop the game from hanging") != std::string::npos,
        "its help says that it can stop the game from hanging", 0, 0);
  menu.SetCursor(at);
  menu.Step(1);
  Check(menu.StrongMem() == 3 && menu.ValueText(item) == "3: most", "a step right: 3",
        menu.StrongMem(), 3);
  menu.Select();
  Check(menu.StrongMem() == 0 && menu.ValueText(item) == "Off" && menu.Changed(),
        "the trigger goes round to Off", menu.StrongMem(), 0);
  menu.Step(-1);
  Check(menu.StrongMem() == 3, "a step left from Off: 3", menu.StrongMem(), 3);
  bwvr_ini::IniText out("");
  menu.Store(&out);
  Check(out.GetInt("bwvr", "box64_strongmem", -1) == 3, "the save writes box64_strongmem=3",
        out.GetInt("bwvr", "box64_strongmem", -1), 3);
  menu.Step(1);
  menu.Store(&out);
  Check(out.GetInt("bwvr", "box64_strongmem", -1) == 0, "the save writes box64_strongmem=0",
        out.GetInt("bwvr", "box64_strongmem", -1), 0);
}

inline void StallChecks() {
  AddrChecks();
  WatchChecks();
  Box64Checks();
}

}  // namespace stall_checks
