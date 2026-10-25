// The translator of the game on ARM64, a test: the choice of the CPU DLL
// (wow64_cpu, android/src/wine_rules.h), its value in the registry of the
// game and the filter of fex_env (android/src/play_rules.h), and the row
// "Translator of the game (Box64 or FEX)" in the menu of the headset app
// (android/src/menu_model.h). main.cpp includes this file after Check and
// Near, just before main.
#pragma once

#include <string>
#include <vector>

#include "../../android/src/menu_model.h"
#include "../../android/src/play_rules.h"
#include "../../android/src/wine_rules.h"

namespace wow64_cpu_checks {

inline void ChoiceChecks() {
  using wine_rules::Cpu;
  printf("\nThe translator of the game: the choice of the CPU DLL (wow64_cpu)\n");
  Check(wine_rules::ParseCpu("fex") == Cpu::kFex && wine_rules::ParseCpu(" FEX\r\n") == Cpu::kFex,
        "fex, also with capitals and white space: FEX", 0, 0);
  Check(wine_rules::ParseCpu("") == Cpu::kBox64 && wine_rules::ParseCpu("box64") == Cpu::kBox64 &&
            wine_rules::ParseCpu("fexx") == Cpu::kBox64 &&
            wine_rules::ParseCpu("libwow64fex.dll") == Cpu::kBox64,
        "no value, box64 and each other value: Box64 (the default)", 0, 0);
  Check(std::string(wine_rules::CpuKey(Cpu::kFex)) == "fex" &&
            std::string(wine_rules::CpuKey(Cpu::kBox64)) == "box64",
        "the values of the key: box64 and fex", 0, 0);
  Check(wine_rules::HasCpuChoice("arm64-v8a") && !wine_rules::HasCpuChoice("x86_64"),
        "the key chooses only on arm64-v8a", 0, 0);
  Check(std::string(wine_rules::GameCpuDll("arm64-v8a", Cpu::kBox64)) == "wowbox64.dll" &&
            std::string(wine_rules::GameCpuDll("arm64-v8a", Cpu::kFex)) == "libwow64fex.dll",
        "arm64-v8a: box64 gives wowbox64.dll, fex gives libwow64fex.dll", 0, 0);
  Check(std::string(wine_rules::GameCpuDll("x86_64", Cpu::kBox64)) == "wow64cpu.dll" &&
            std::string(wine_rules::GameCpuDll("x86_64", Cpu::kFex)) == "wow64cpu.dll",
        "x86_64: wow64cpu.dll with each value", 0, 0);
  Check(wine_rules::GameCpu("arm64-v8a", Cpu::kFex) == Cpu::kFex &&
            wine_rules::GameCpu("x86_64", Cpu::kFex) == Cpu::kBox64,
        "the CPU of the game: the choice on arm64-v8a, Box64 on x86_64 (the variables as before)",
        0, 0);
  const std::vector<std::string> arm = wine_rules::CopiedCpuDlls("arm64-v8a");
  Check(arm.size() == 2 && arm[0] == "wowbox64.dll" && arm[1] == "libwow64fex.dll",
        "arm64-v8a: both CPU DLLs go into system32", static_cast<double>(arm.size()), 2);
  Check(wine_rules::CopiedCpuDlls("x86_64").empty(), "x86_64: no CPU DLL goes into system32", 0,
        0);
  const std::string files = "/data/user/0/org.bwvr.quest/files";
  const wine_rules::Paths a = wine_rules::MakePaths(files, "/s", "arm64-v8a");
  Check(a.cpu_dir == files + "/wine/arm64-v8a/lib/wine/aarch64-windows" &&
            a.cpu_dll == a.cpu_dir + "/wowbox64.dll",
        "arm64-v8a: the CPU DLLs are in aarch64-windows of the tree", 0, 0);
  Check(wine_rules::MakePaths(files, "/s", "x86_64").cpu_dir.empty(),
        "x86_64: no folder of CPU DLLs", 0, 0);
  Check(!wine_rules::IsLeftOut("libwow64fex.dll"), "libwow64fex.dll stays in the tree", 0, 0);

  printf("\nThe translator of the game: the registry of the game\n");
  const std::string key = "[HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Wow64\\x86]\r\n";
  const std::string fex = play_rules::GameRegistry(1380, 1104, true, "libwow64fex.dll");
  Check(fex.find(key + "@=\"libwow64fex.dll\"\r\n") != std::string::npos,
        "fex: game.reg makes libwow64fex.dll the default value of Wow64\\x86", 0, 0);
  const std::string box = play_rules::GameRegistry(1380, 1104, true, "wowbox64.dll");
  Check(box.find(key + "@=\"wowbox64.dll\"\r\n") != std::string::npos,
        "box64: game.reg puts wowbox64.dll back (the value of wine.inf)", 0, 0);
  const std::string none = play_rules::GameRegistry(1380, 1104, true);
  Check(none.find("Wow64") == std::string::npos &&
            none == play_rules::GameRegistry(1380, 1104, true, ""),
        "no CPU DLL (x86_64): game.reg has no key Wow64, as before", 0, 0);
  Check(fex.find("KnownDLLs]") < fex.find(key) &&
            fex.find(key) < fex.find("[HKEY_CURRENT_USER\\Software\\Wine\\Drivers]"),
        "the key is between the known DLLs and the audio driver", 0, 0);
  const size_t added = std::string("\r\n").size() + key.size() +
                       std::string("@=\"libwow64fex.dll\"\r\n").size();
  Check(fex.size() == none.size() + added, "the key is all that changes",
        static_cast<double>(fex.size() - none.size()), static_cast<double>(added));
}

inline void FexVarChecks() {
  namespace pr = play_rules;
  using wine_rules::Cpu;
  printf("\nThe variables of FEX for the game (fex_env)\n");
  pr::EnvVars e = pr::FexVars("");
  Check(e.vars.empty() && e.refused.empty(), "no fex_env: no variable", 0, 0);
  e = pr::FexVars("FEX_X87REDUCEDPRECISION=1 FEX_TSOENABLED=0");
  Check(e.vars.size() == 2 && e.vars[0] == "FEX_X87REDUCEDPRECISION=1" &&
            e.vars[1] == "FEX_TSOENABLED=0" && e.refused.empty(),
        "two names of FEX go, in order", static_cast<double>(e.vars.size()), 2);
  e = pr::FexVars("\tFEX_MULTIBLOCK=0  BOX64_LOG=1 fex_silentlog=0 FEX_=1 FEX_MAXINST FEX_MULTIBLOCK=1 "
                  "WINEDEBUG=+all ");
  Check(e.vars.size() == 1 && e.vars[0] == "FEX_MULTIBLOCK=0" && e.refused.size() == 6,
        "only FEX_ and capitals, a name with =VALUE, one time. Tabs and spaces go between them",
        static_cast<double>(e.refused.size()), 6);
  Check(e.refused.size() == 6 && e.refused[0] == "BOX64_LOG=1 (the name does not start with FEX_)" &&
            e.refused[1] == "fex_silentlog=0 (the name does not start with FEX_)" &&
            e.refused[2] == "FEX_=1 (the name does not start with FEX_)" &&
            e.refused[3] == "FEX_MAXINST (no =VALUE)" &&
            e.refused[4] == "FEX_MULTIBLOCK=1 (the name comes two times)" &&
            e.refused[5] == "WINEDEBUG=+all (the name does not start with FEX_)",
        "each word that does not go has one line with why", 0, 0);

  printf("\nThe variables of the translator for the game (CpuVars)\n");
  e = pr::CpuVars(Cpu::kFex, 2, "BOX64_DYNAREC_BIGBLOCK=0", "FEX_SMCCHECKS=full");
  Check(e.vars.size() == 1 && e.vars[0] == "FEX_SMCCHECKS=full",
        "fex: only fex_env, no box64_strongmem and no box64_env",
        static_cast<double>(e.vars.size()), 1);
  e = pr::CpuVars(Cpu::kBox64, 2, "BOX64_DYNAREC_BIGBLOCK=0", "FEX_SMCCHECKS=full");
  Check(e.vars.size() == 2 && e.vars[0] == "BOX64_DYNAREC_STRONGMEM=2" &&
            e.vars[1] == "BOX64_DYNAREC_BIGBLOCK=0",
        "box64: box64_strongmem and box64_env as before, no fex_env",
        static_cast<double>(e.vars.size()), 2);
  const pr::Box64Env old = pr::Box64Vars(3, "BOX64_DYNAREC_STRONGMEM=1 X=1");
  Check(old.vars.size() == 1 && old.refused.size() == 2 &&
            old.refused[0] == "BOX64_DYNAREC_STRONGMEM=1 (box64_strongmem sets it)" &&
            old.refused[1] == "X=1 (the name does not start with BOX64_)",
        "the lines of box64_env are as before", static_cast<double>(old.refused.size()), 2);
  const std::vector<std::string> env = {"PATH=/system/bin", "FEX_SILENTLOG=1",
                                        "WINEDLLOVERRIDES=mscoree,mshtml="};
  const std::vector<std::string> game =
      pr::GameEnvironment(env, pr::FexVars("FEX_SILENTLOG=0 FEX_X87REDUCEDPRECISION=1").vars);
  Check(game.size() == 4 && game[0] == "PATH=/system/bin" &&
            game[1] == "WINEDLLOVERRIDES=mscoree,mshtml=;ddraw=n" &&
            game[2] == "FEX_SILENTLOG=0" && game[3] == "FEX_X87REDUCEDPRECISION=1",
        "the environment of the game: the variables of FEX at the end, in place of an inherited "
        "variable with the same name",
        static_cast<double>(game.size()), 4);
}

inline void MenuRowChecks() {
  using wine_rules::Cpu;
  printf("\nThe row of the translator in the menu\n");
  menu_model::Menu menu;
  menu.Load(bwvr_ini::IniText(""));
  Check(menu.Wow64Cpu() == Cpu::kBox64, "no key: Box64", 0, 0);
  menu.Load(bwvr_ini::IniText("[bwvr]\r\nwow64_cpu=fex\r\n"));
  Check(menu.Wow64Cpu() == Cpu::kFex, "wow64_cpu=fex loads", 0, 0);
  menu.Load(bwvr_ini::IniText("[bwvr]\r\nwow64_cpu=qemu\r\n"));
  Check(menu.Wow64Cpu() == Cpu::kBox64, "another value: Box64", 0, 0);
  int row = -1;
  std::vector<menu_model::Item> items = menu.Items();
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (items[r].kind == menu_model::ItemKind::kGroup &&
        menu.Label(items[r]) == std::string("Testing")) {
      row = r;
    }
  }
  Check(row >= 0, "the main page has the group Testing", row, 0);
  Check(menu.Help(items[row]).find("Translator of the game (Box64 or FEX)") != std::string::npos,
        "the help of the group names the row", 0, 0);
  menu.SetCursor(row);
  menu.Select();
  items = menu.Items();
  int at = -1, strong = -1;
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (items[r].kind == menu_model::ItemKind::kWow64Cpu) at = r;
    if (items[r].kind == menu_model::ItemKind::kStrongMem) strong = r;
  }
  Check(at >= 0 && strong == at + 1 && strong + 1 == static_cast<int>(items.size()) - 1,
        "the page Testing has the row above the row of the memory order of Box64 and Back", at, 0);
  const menu_model::Item item = items[at];
  Check(menu.Label(item) == "Translator of the game (Box64 or FEX)" &&
            menu.ValueText(item) == "Box64",
        "its label and its value", 0, 0);
  const std::string help = menu.Help(item);
  Check(help.find("Box64 (wowbox64.dll) is the default") != std::string::npos &&
            help.find("FEX (libwow64fex.dll)") != std::string::npos &&
            help.find("Steam Frame") != std::string::npos &&
            help.find("does nothing here") == std::string::npos,
        "its help says what each one is", 0, 0);
  menu.SetCursor(at);
  menu.Step(1);
  Check(menu.Wow64Cpu() == Cpu::kFex && menu.ValueText(item) == "FEX" && menu.Changed(),
        "a step: FEX", 0, 0);
  bwvr_ini::IniText out("");
  menu.Store(&out);
  Check(out.Get("bwvr", "wow64_cpu", "") == "fex", "the save writes wow64_cpu=fex", 0, 0);
  menu.Select();
  Check(menu.Wow64Cpu() == Cpu::kBox64, "the trigger gives Box64 again", 0, 0);
  menu.Step(-1);
  menu.Step(-1);
  Check(menu.Wow64Cpu() == Cpu::kBox64, "two steps left: Box64", 0, 0);
  menu.Store(&out);
  Check(out.Get("bwvr", "wow64_cpu", "") == "box64", "the save writes wow64_cpu=box64", 0, 0);

  printf("\nThe row of the translator on x86_64\n");
  menu.SetCpuChoiceCounts(false);
  menu.Step(1);
  Check(menu.Wow64Cpu() == Cpu::kFex && menu.ValueText(item) == "FEX (no effect here)",
        "the value says that it has no effect", 0, 0);
  Check(menu.Help(item).find("so this row does nothing here") != std::string::npos,
        "the help says that the row does nothing on this device", 0, 0);
  menu.Store(&out);
  Check(out.Get("bwvr", "wow64_cpu", "") == "fex", "the save keeps the value", 0, 0);
}

inline void Wow64CpuChecks() {
  ChoiceChecks();
  FexVarChecks();
  MenuRowChecks();
}

}  // namespace wow64_cpu_checks
