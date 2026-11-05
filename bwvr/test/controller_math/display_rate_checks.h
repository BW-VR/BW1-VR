// The display rate of the headset app, a test (android/src/display_rate.h,
// and the row "Display rate" of android/src/menu_model.h). main.cpp includes
// this file after Check and Near, just before main.
#pragma once

#include <string>
#include <vector>

#include "../../android/src/display_rate.h"
#include "../../android/src/menu_model.h"
#include "../../src/common/ini_text.h"

namespace display_rate_checks {

inline bool Same(const std::vector<int>& a, const std::vector<int>& b) { return a == b; }

inline void DisplayRateChecks() {
  namespace dr = display_rate;
  using namespace menu_model;
  using bwvr_ini::IniText;
  printf("\nThe display rate of the headset app\n");

  // THE LIST OF THE ROW: the rates 72, 80, 90 and 120 that the runtime lists.
  Check(Same(dr::Offered({72.0f, 80.0f, 90.0f, 120.0f}), {72, 80, 90, 120}),
        "a runtime with 72, 80, 90 and 120 Hz: the row offers the four", 0, 0);
  Check(Same(dr::Offered({120.0f, 60.0f, 72.0f, 90.0f}), {72, 90, 120}),
        "a rate that is not 72, 80, 90 or 120 is not offered, and the order is 72, 80, 90, 120",
        0, 0);
  Check(Same(dr::Offered({72.0f, 89.9f, 119.88f}), {72, 90, 120}),
        "a rate of the runtime within 0.5 Hz counts", 0, 0);
  Check(Same(dr::Offered({72.6f, 80.6f}), {}), "a rate 0.6 Hz away does not count", 0, 0);
  Check(dr::Offered({}).empty(), "a runtime with no rates (no extension): no row", 0, 0);
  Check(dr::RuntimeRate(90, {72.0f, 89.9f}) == 89.9f,
        "the request has the float of the runtime list", dr::RuntimeRate(90, {72.0f, 89.9f}), 89.9);
  Check(dr::RuntimeRate(0, {72.0f, 90.0f}) == 0.0f, "display_hz=0 asks for no rate (0)", 0, 0);
  Check(dr::RuntimeRate(80, {72.0f, 90.0f}) == 0.0f,
        "a rate that the runtime does not list asks for no rate", 0, 0);

  // THE STEAM FRAME: the runtime lists only 144 Hz, and the settings of SteamVR set the rate.
  Check(Same(dr::Offered({144.0f}), {144}), "a runtime with 144 Hz: the row offers it", 0, 0);
  Check(Same(dr::SteamVrOffered(), {72, 90, 120, 144}), "the row with the settings of SteamVR", 0, 0);
  Check(dr::Step(120, 1, dr::SteamVrOffered()) == 144 && dr::Step(144, 1, dr::SteamVrOffered()) == 0,
        "the steps reach 144 Hz and then Headset default", 0, 0);
  Check(dr::HzOfPeriod(11111111) == 90, "a period of 11.11 ms is 90 Hz", dr::HzOfPeriod(11111111), 90);
  Check(dr::HzOfPeriod(6944444) == 144, "a period of 6.94 ms is 144 Hz", dr::HzOfPeriod(6944444), 144);
  Check(dr::HzOfPeriod(0) == 0, "no period is no rate", 0, 0);
  Check(dr::Reached(144, 6944444) && !dr::Reached(144, 11111111) && !dr::Reached(0, 11111111),
        "the rate of the row against the display period", 0, 0);
  Check(dr::RowText(144, 144) == "144 Hz", "the row at the rate of the headset", 0, 0);
  Check(dr::RowText(144, 90) == "144 Hz (now 90 Hz)", "the row with another rate of the headset", 0, 0);
  Check(dr::RowText(0, 90) == "Headset default (now 90 Hz)", "the row with no rate shows the rate now", 0, 0);
  Check(dr::RowText(0, 0) == "Headset default", "the row before the first frame", 0, 0);

  // THE FALL BACK OF THE INI VALUE.
  const std::vector<int> four = {72, 80, 90, 120};
  Check(dr::Checked(0, four) == 0, "display_hz=0 stays 0", 0, 0);
  Check(dr::Checked(90, four) == 90, "display_hz=90 of the list stays 90", 0, 0);
  Check(dr::Checked(75, four) == 0, "display_hz=75 is not a rate of the list: 0", 0, 0);
  Check(dr::Checked(-5, four) == 0, "a negative display_hz is 0", 0, 0);
  Check(dr::Checked(90, {72, 120}) == 0, "display_hz=90 with a runtime of 72 and 120 Hz: 0", 0,
        0);
  Check(dr::Checked(90, {}) == 0, "display_hz=90 with no rates of the runtime: 0", 0, 0);

  // THE STEPS OF THE ROW: Headset default, then each rate, round.
  Check(dr::Step(0, 1, four) == 72 && dr::Step(72, 1, four) == 80 && dr::Step(90, 1, four) == 120,
        "a step to the right goes to the next rate", 0, 0);
  Check(dr::Step(120, 1, four) == 0 && dr::Step(0, -1, four) == 120,
        "the steps go round through Headset default", 0, 0);
  Check(dr::Step(75, 1, four) == 72, "a value that is not in the list counts as 0", 0, 0);

  // THE ROW OF THE MENU.
  Menu menu;
  IniText ini("[bwvr]\r\ndisplay_hz=90\r\n");
  menu.Load(ini);
  Check(!menu.HasDisplayRow() && menu.DisplayHz() == 0 && menu.DisplayWanted() == 90,
        "before the rates of the runtime: no row, and the app asks for no rate", 0, 0);
  // The page "Performance": its entry on the main page.
  std::vector<Item> items = menu.Items();
  int performance = -1;
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (menu.Label(items[r]) == "Performance") performance = r;
  }
  Check(performance >= 0, "the main page has the group Performance", performance, 0);
  auto page = [&]() {
    menu.Back();
    menu.SetCursor(performance);
    menu.Select();
    return menu.Items();
  };
  auto has_row = [&](const std::vector<Item>& rows) {
    for (const Item& i : rows) {
      if (i.kind == ItemKind::kDisplayRate) return true;
    }
    return false;
  };
  Check(!has_row(page()), "with no rates the page Performance has no row Display rate", 0, 0);
  IniText no_row("[bwvr]\r\ndisplay_hz=90\r\n");
  menu.Store(&no_row);
  Check(no_row.GetInt("bwvr", "display_hz", -1) == 90,
        "with no row a save keeps display_hz as it is", 0, 0);

  menu.SetDisplayRates(dr::Offered({72.0f, 80.0f, 90.0f, 120.0f}));
  Check(menu.HasDisplayRow() && menu.DisplayHz() == 90,
        "display_hz=90 and a runtime with 90 Hz: the row has 90", menu.DisplayHz(), 90);
  items = page();
  int row = -1;
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (items[r].kind == ItemKind::kDisplayRate) row = r;
  }
  Check(row >= 0 && items.back().kind == ItemKind::kBack,
        "the page Performance has the row Display rate, before Back", row, 0);
  if (row < 0) return;
  Check(menu.Label(items[row]) == "Display rate" && menu.ValueText(items[row]) == "90 Hz",
        "the row is a list row with the value \"90 Hz\"", 0, 0);
  const std::string help = menu.Help(items[row]);
  Check(help.find("smoother head motion") != std::string::npos &&
            help.find("power and heat") != std::string::npos &&
            help.find("fewer frames than the rate") != std::string::npos,
        "the help of the row names the motion, the power and heat, and the frames of the game",
        0, 0);
  Check(menu.SliderFraction(items[row]) < 0.0 && menu.CheckValue(items[row]) < 0,
        "the row has no slider and no check box (the look of a list row)", 0, 0);
  menu.SetCursor(row);
  menu.Select();
  Check(menu.DisplayHz() == 120 && menu.Changed(), "the trigger on the row gives the next rate",
        menu.DisplayHz(), 120);
  menu.Step(1);
  Check(menu.DisplayHz() == 0 && menu.ValueText(items[row]) == "Headset default",
        "after 120 Hz comes Headset default", menu.DisplayHz(), 0);
  menu.Step(-1);
  menu.Step(-1);
  Check(menu.DisplayHz() == 90, "a step to the left goes back", menu.DisplayHz(), 90);
  IniText saved("[bwvr]\r\n");
  menu.Store(&saved);
  Check(saved.GetInt("bwvr", "display_hz", -1) == 90, "a save writes display_hz=90", 0, 0);
  menu.Step(1);
  menu.Step(1);
  menu.Store(&saved);
  Check(saved.GetInt("bwvr", "display_hz", -1) == 0,
        "a save of Headset default writes display_hz=0", 0, 0);
  menu.Back();
  items = menu.Items();
  Check(performance < static_cast<int>(items.size()) &&
            menu.Help(items[performance]) == "The settings: Renderer, Display rate.",
        "the help of the group names the row", 0, 0);

  // The fall back of an ini value that the runtime does not give.
  Menu other;
  IniText odd("[bwvr]\r\ndisplay_hz=80\r\n");
  other.Load(odd);
  other.SetDisplayRates(dr::Offered({72.0f, 90.0f, 120.0f}));
  Check(other.DisplayHz() == 0 && other.DisplayWanted() == 80,
        "display_hz=80 and a runtime with no 80 Hz: the row has Headset default", 0, 0);
  IniText odd_saved("[bwvr]\r\ndisplay_hz=80\r\n");
  other.Store(&odd_saved);
  Check(odd_saved.GetInt("bwvr", "display_hz", -1) == 0, "and a save writes display_hz=0", 0,
        0);
}

}  // namespace display_rate_checks
