// The ini text, the table of the settings and the menu of the headset app, a
// test (ini_text.h, settings_table.h, android/src/menu_model.h). main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include <string>

#include "../../android/src/menu_layout.h"
#include "../../android/src/menu_model.h"
#include "../../src/common/exe_sha1.h"
#include "../../src/common/ini_text.h"
#include "../../src/common/settings_table.h"

namespace menu_checks {

inline bool Has(const std::string& text, const char* part) {
  return text.find(part) != std::string::npos;
}

inline void IniChecks() {
  using bwvr_ini::IniText;
  printf("\nThe ini text of the headset app\n");

  IniText ini("; a comment\r\n[bwvr]\r\nvr=1\r\n  Stereo = 1 \r\n\r\n"
              "[game_before]\r\nsaved=1\r\n");
  Check(ini.GetInt("bwvr", "vr", 0) == 1, "a key reads", 0, 0);
  Check(ini.GetInt("BWVR", "stereo", 0) == 1,
        "the names have no case, and the spaces do not count", 0, 0);
  Check(ini.GetInt("bwvr", "saved", 7) == 7,
        "a key of another section does not read", 0, 0);
  Check(ini.GetInt("bwvr", "missing", 7) == 7, "a missing key is the fallback",
        0, 0);

  ini.SetInt("bwvr", "stereo", 0);
  Check(ini.GetInt("bwvr", "stereo", 1) == 0, "a write replaces the line", 0,
        0);
  ini.SetInt("bwvr", "one_walk", 1);
  const std::string text = ini.Text();
  Check(Has(text, "stereo=0\r\none_walk=1\r\n\r\n[game_before]"),
        "a new key goes after the last line of its section", 0, 0);
  Check(Has(text, "; a comment\r\n"), "a comment stays", 0, 0);

  ini.Remove("bwvr", "vr");
  Check(!ini.Has("bwvr", "vr"), "a remove takes the line away", 0, 0);
  ini.Set("android", "test", "x");
  Check(Has(ini.Text(), "\r\n[android]\r\ntest=x\r\n"),
        "a new section goes at the end", 0, 0);

  IniText lf("[bwvr]\nbackend=vulkan\n");
  Check(lf.Get("bwvr", "backend", "") == "vulkan", "LF lines read too", 0, 0);
  Check(lf.Text() == "[bwvr]\r\nbackend=vulkan\r\n",
        "and the text has CRLF lines", 0, 0);

  IniText empty("");
  empty.SetInt("bwvr", "vr", 1);
  Check(empty.Text() == "[bwvr]\r\nvr=1\r\n", "an empty file gets a section",
        0, 0);
}

inline void TableChecks() {
  namespace st = bwvr_settings;
  printf("\nThe table of the settings\n");
  char label[64];
  st::ChoiceLabel("Off|30 degrees|45 degrees", 2, label, sizeof(label));
  Check(strcmp(label, "45 degrees") == 0, "the label of item 2 of a list", 0,
        0);
  st::ChoiceLabel("None=0|5%=5|10%=10", 1, label, sizeof(label));
  Check(strcmp(label, "5%") == 0, "the label of a label=value item", 0, 0);
  Check(st::ChoiceCount("a|b|c") == 3, "a list of 3",
        st::ChoiceCount("a|b|c"), 3);
  Check(st::ChoiceValue("None=0|5%=5|10%=10", 2) == 10, "the value of item 2",
        st::ChoiceValue("None=0|5%=5|10%=10", 2), 10);
  Check(st::ChoiceIndex("None=0|5%=5|10%=10", 7) == 1,
        "a value that is not in the list takes the item below it",
        st::ChoiceIndex("None=0|5%=5|10%=10", 7), 1);
  Check(st::SliderSnap("10|45|1| degrees", 50) == 45, "a slider stops at 45",
        st::SliderSnap("10|45|1| degrees", 50), 45);
  Check(st::SliderSnap("5|100|5|%", 13) == 15, "a slider goes on its step",
        st::SliderSnap("5|100|5|%", 13), 15);
  Check(strcmp(st::SliderUnit("10|45|1| degrees"), " degrees") == 0,
        "the unit of a slider", 0, 0);

  // The numbers of the launcher with the figures of a Quest 3.
  int w = 0, h = 0;
  st::ScaleToPixels(100, 2064, 2208, 0, true, 25, &w, &h);
  Check(w == 2760 && h == 2208, "100% with one walk is 2760 x 2208", w, 2760);
  st::ScaleToPixels(100, 2064, 2208, 0, false, 25, &w, &h);
  Check(w == 2208 && h == 2208, "100% with two walks is square", w, 2208);
  st::ScaleToPixels(50, 2064, 2208, 20, false, 25, &w, &h);
  Check(w == 1324 && h == 1324, "50% with a margin of 20%", w, 1324);
  st::ScaleToPixels(10, 2064, 2208, 0, false, 25, &w, &h);
  Check(w == 640, "the render is 640 at the least", w, 640);

  // Each row has a key, a label and a help text, and each heading a label.
  bool rows_ok = true;
  for (int i = 0; i < st::kSettingCount; ++i) {
    const st::Setting& s = st::kSettings[i];
    if (s.label == nullptr) rows_ok = false;
    if (s.kind != st::kGroup && (s.key == nullptr || s.help == nullptr)) {
      rows_ok = false;
    }
  }
  Check(rows_ok, "each row of the table is complete", 0, 0);
  Check(st::kSettings[0].kind == st::kGroup, "the table starts with a group",
        0, 0);

  // The SHA-1 of the file of the game: the check value of FIPS 180-4.
  bwvr::detail::Sha1 sha;
  bwvr::detail::Sha1Start(&sha);
  bwvr::detail::Sha1Add(&sha, reinterpret_cast<const uint8_t*>("abc"), 3);
  char hex[41];
  bwvr::detail::Sha1Finish(&sha, hex);
  Check(strcmp(hex, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0,
        "the SHA-1 of \"abc\"", 0, 0);
  Check(bwvr::KnownBuildName("3c7297dae357834860ff46a89ca5d1a552747100") !=
            nullptr,
        "version 1.10 has a name", 0, 0);
}

inline void MenuChecks() {
  using namespace menu_model;
  using bwvr_ini::IniText;
  printf("\nThe menu of the headset app\n");

  Menu menu;
  IniText ini("[bwvr]\r\nsnap_turn=3\r\nplay_mode=0\r\nfake_head=1\r\n"
              "gpu_bridge=1\r\nresolution_pct=500\r\nkeep_me=7\r\n"
              "vk_device=0\r\n");
  menu.Load(ini);
  Check(menu.ValueOf("snap_turn") == 3, "a choice reads its index", 0, 0);
  Check(menu.ValueOf("play_mode") == 0,
        "play mode 0 of an old ini shows the God view (item 0)",
        menu.ValueOf("play_mode"), 0);
  Check(menu.ValueOf("resolution_pct") == 200,
        "a scale stops at its high end", menu.ValueOf("resolution_pct"), 200);
  Check(menu.ValueOf("one_walk") == 1, "a missing key is its default", 0, 0);

  // The main page: the two commands, the groups and Save.
  std::vector<Item> items = menu.Items();
  Check(menu.OnMain(), "the menu starts on the main page", 0, 0);
  Check(items.front().kind == ItemKind::kPlay, "the first row is Play", 0, 0);
  Check(items.back().kind == ItemKind::kSave, "the last row is Save", 0, 0);
  int groups = 0;
  for (const Item& item : items) {
    if (item.kind == ItemKind::kGroup) ++groups;
  }
  Check(groups == 7, "7 groups of the launcher", groups, 7);

  // The group "Performance" with the renderer Vulkan: only the renderer.
  int performance = -1;
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (items[r].kind == ItemKind::kGroup &&
        strcmp(menu.Label(items[r]).c_str(), "Performance") == 0) {
      performance = r;
    }
  }
  Check(performance >= 0, "the group Performance is there", performance, 0);
  menu.SetCursor(performance);
  Check(menu.Help(menu.Items()[performance]) == "The settings: Renderer.",
        "the help of a group names the settings on its page", 0, 0);
  Check(menu.Select() == Action::kNone && !menu.OnMain(),
        "a select on a group opens its page", 0, 0);
  items = menu.Items();
  Check(items.size() == 2, "the page has the renderer and Back",
        static_cast<double>(items.size()), 2);
  Check(menu.ValueText(items[0]) == "Vulkan", "the renderer is Vulkan", 0, 0);
  menu.Step(1);
  Check(menu.ValueText(items[0]) == "Vulkan",
        "a step does not change the renderer", 0, 0);
  menu.Back();
  Check(menu.OnMain() && menu.Cursor() == performance,
        "Back goes to the entry of the group", menu.Cursor(), performance);

  // The group "Moving around": a list goes round.
  int moving = -1;
  items = menu.Items();
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (items[r].kind == ItemKind::kGroup &&
        strcmp(menu.Label(items[r]).c_str(), "Moving around") == 0) {
      moving = r;
    }
  }
  menu.SetCursor(moving);
  menu.Select();
  items = menu.Items();
  Check(menu.Label(items[0]) == "Turn in steps", "the first row of the page",
        0, 0);
  Check(menu.ValueText(items[0]) == "90 degrees", "snap_turn 3 is 90 degrees",
        0, 0);
  menu.SetCursor(0);
  menu.Step(1);
  Check(menu.ValueOf("snap_turn") == 0, "a step past the end goes round",
        menu.ValueOf("snap_turn"), 0);
  menu.Step(-1);
  Check(menu.ValueOf("snap_turn") == 3, "and a step back goes round too",
        menu.ValueOf("snap_turn"), 3);
  Check(menu.Changed(), "a step marks a change", 0, 0);
  menu.Back();

  // The group "Foveated rendering": a check box and a slider.
  items = menu.Items();
  int fovea = -1;
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (items[r].kind == ItemKind::kGroup &&
        strcmp(menu.Label(items[r]).c_str(), "Foveated rendering") == 0) {
      fovea = r;
    }
  }
  menu.SetCursor(fovea);
  menu.Select();
  items = menu.Items();
  Check(menu.CheckValue(items[0]) == 0, "foveation is off", 0, 0);
  menu.SetCursor(0);
  menu.Select();
  Check(menu.ValueOf("foveation") == 1, "a select on a check box turns it on",
        0, 0);
  menu.SetCursor(1);
  menu.SetSliderAt(1.0);
  Check(menu.ValueOf("foveation_inner_deg") == 45,
        "a press at the right end of the slider is the high value",
        menu.ValueOf("foveation_inner_deg"), 45);
  menu.Step(1);
  Check(menu.ValueOf("foveation_inner_deg") == 45, "a slider stops at its end",
        menu.ValueOf("foveation_inner_deg"), 45);
  menu.Step(-1);
  Check(menu.ValueOf("foveation_inner_deg") == 44, "a step of the slider",
        menu.ValueOf("foveation_inner_deg"), 44);
  Check(menu.ValueText(items[1]) == "44 degrees", "the slider with its unit",
        0, 0);
  menu.Back();

  // The scale of the resolution says the size of the render.
  menu.SetHeadsetSize(2064, 2208);
  int w = 0, h = 0;
  menu.RenderSize(&w, &h);
  Check(w == 4096 && h == 4096, "200% of a Quest 3 is the most, 4096", w,
        4096);

  // The save, by the rules of the launcher for the renderer Vulkan.
  menu.Store(&ini);
  Check(ini.Get("bwvr", "backend", "") == "vulkan", "backend=vulkan", 0, 0);
  Check(!ini.Has("bwvr", "gpu_bridge"), "the save removes gpu_bridge", 0, 0);
  Check(ini.GetInt("bwvr", "vulkan_strict", 0) == 1,
        "the save writes vulkan_strict=1: Vulkan alone", 0, 0);
  Check(!ini.Has("bwvr", "fake_head"), "the save removes a retired key", 0, 0);
  Check(ini.GetInt("bwvr", "vr", 0) == 1 && ini.GetInt("bwvr", "stereo", 0) == 1 &&
            ini.GetInt("bwvr", "head_camera", 0) == 1,
        "vr, stereo and head_camera are 1", 0, 0);
  Check(ini.GetInt("bwvr", "play_mode", 0) == 4,
        "play_mode is the value of the item, 4", ini.GetInt("bwvr", "play_mode", 0),
        4);
  Check(ini.GetInt("bwvr", "keep_me", 0) == 7,
        "a key that is not in the table stays", 0, 0);
  Check(ini.GetInt("bwvr", "vk_device", 1) == 0,
        "vk_device=0 of the headset app stays (no row, not retired)", 0, 0);
  Check(!ini.Has("bwvr", "d3d9_backend") && !ini.Has("bwvr", "xr_vulkan"),
        "the rows of Direct3D 9 are not written", 0, 0);
  Check(ini.GetInt("bwvr", "foveation", 0) == 1, "foveation=1 is saved", 0, 0);

  // A load of the saved text gives the same values.
  Menu again;
  again.Load(ini);
  Check(again.ValueOf("foveation_inner_deg") == 44 &&
            again.ValueOf("snap_turn") == 3 && again.ValueOf("play_mode") == 0,
        "a load of the save gives the same values", 0, 0);
}

inline void LayoutChecks() {
  namespace ml = menu_layout;
  printf("\nThe menu quad and the controller ray\n");
  const ml::Quad q = ml::MenuQuad();
  Near("the quad has the aspect of the picture", q.width / q.height,
       static_cast<double>(ml::kWidth) / ml::kHeight, 1e-6);

  // A ray from the head straight ahead hits the middle of the quad, a little
  // above its center, because the quad is 0.15 m below the head.
  const float head[3] = {0.0f, 0.0f, 0.0f};
  const float ahead[3] = {0.0f, 0.0f, -1.0f};
  double px = 0.0, py = 0.0;
  Check(ml::RayToPixel(head, ahead, q, &px, &py), "the ray ahead hits", 0, 0);
  Near("x is the middle", px, ml::kWidth * 0.5, 1e-3);
  Near("y is 0.15 m above the middle",
       py, ml::kHeight * 0.5 - 0.15 / q.height * ml::kHeight, 1e-3);

  // The corner: a ray to the top left corner of the quad.
  const float corner[3] = {q.center[0] - q.width * 0.5f + 0.001f,
                           q.center[1] + q.height * 0.5f - 0.001f, q.center[2]};
  const float to_corner[3] = {corner[0], corner[1], corner[2]};
  Check(ml::RayToPixel(head, to_corner, q, &px, &py) && px < 2.0 && py < 2.0,
        "a ray to the top left corner hits pixel 0, 0", px, 0);
  const float away[3] = {0.0f, 0.0f, 1.0f};
  Check(!ml::RayToPixel(head, away, q, &px, &py), "a ray away misses", 0, 0);
  const float wide[3] = {1.0f, 0.0f, -0.2f};
  Check(!ml::RayToPixel(head, wide, q, &px, &py), "a ray to the side misses", 0,
        0);

  // The point of a pixel is on the ray of that pixel.
  float point[3];
  ml::PixelToPoint(q, 400.0, 900.0, point);
  Check(ml::RayToPixel(head, point, q, &px, &py), "the ray to a point hits", 0,
        0);
  Near("and it hits its pixel x", px, 400.0, 1e-2);
  Near("and its pixel y", py, 900.0, 1e-2);

  // The forward of a pose: no turn is -z, a turn of 90 degrees to the left
  // about y is -x.
  const float none[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float f[3];
  ml::Forward(none, f);
  Check(fabs(f[0]) < 1e-6 && fabs(f[1]) < 1e-6 && fabs(f[2] + 1.0) < 1e-6,
        "no turn points along -z", f[2], -1);
  const float s = static_cast<float>(sqrt(0.5));
  const float left[4] = {0.0f, s, 0.0f, s};
  ml::Forward(left, f);
  Check(fabs(f[0] + 1.0) < 1e-5 && fabs(f[2]) < 1e-5,
        "a turn of 90 degrees to the left points along -x", f[0], -1);
  const float down[4] = {-0.258819f, 0.0f, 0.0f, 0.965926f};  // 30 degrees
  ml::Forward(down, f);
  Near("a pitch of 30 degrees down", f[1], -0.5, 1e-5);

  // The rows and the bar.
  Check(ml::RowAt(ml::kRowTop - 1, 10) == -1, "above the rows is no row", 0, 0);
  Check(ml::RowAt(ml::RowTop(3) + 5, 10) == 3, "a pixel in row 3", 0, 0);
  Check(ml::RowAt(ml::RowTop(3) + 5, 3) == -1, "a page of 3 rows has no row 3",
        0, 0);
  Near("the left end of the bar", ml::BarFraction(ml::kBarLeft), 0.0, 1e-9);
  Near("the right end of the bar", ml::BarFraction(ml::kBarRight), 1.0, 1e-9);
  Near("past the end of the bar", ml::BarFraction(ml::kBarRight + 100), 1.0,
       1e-9);
  Check(ml::RowTop(ml::kMaxRows) <= ml::kHelpTop,
        "the last row ends above the help", ml::RowTop(ml::kMaxRows),
        ml::kHelpTop);

  // Each page of the menu fits the rows of the picture.
  menu_model::Menu menu;
  bwvr_ini::IniText none_ini("");
  menu.Load(none_ini);
  bool fits = static_cast<int>(menu.Items().size()) <= ml::kMaxRows;
  const std::vector<menu_model::Item> main = menu.Items();
  for (int r = 0; r < static_cast<int>(main.size()); ++r) {
    if (main[r].kind != menu_model::ItemKind::kGroup) continue;
    menu.SetCursor(r);
    menu.Select();
    if (static_cast<int>(menu.Items().size()) > ml::kMaxRows) fits = false;
    menu.Back();
  }
  Check(fits, "each page of the menu has 10 rows at most", 0, 0);
}

inline void AllMenuChecks() {
  IniChecks();
  TableChecks();
  MenuChecks();
  LayoutChecks();
}

}  // namespace menu_checks
