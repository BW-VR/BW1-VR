// THE MENU OF THE HEADSET APP (docs/android.md, "The menu"). It has no
// Android code, so the math test of the PC checks it
// (test/controller_math/menu_checks.h).
//
// The menu has the form of the menu of the game: a main page with the
// commands and one entry for each group of settings, and a page for each
// group. The rows of a group page are the rows of the launcher
// (settings_table.h), with the renderer Vulkan: the rows that only the
// renderer Direct3D 9 uses are not shown. A save writes bwvr.ini by the
// rules of the launcher (SaveAll in launcher.cpp) for the renderer Vulkan.
#pragma once

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../src/common/ini_text.h"
#include "../../src/common/settings_table.h"

namespace menu_model {

using bwvr_ini::IniText;
namespace st = bwvr_settings;

enum class ItemKind {
  kPlay,     // "Play in VR"
  kNewGame,  // "Play, new game"
  kGroup,    // a group of settings: opens its page
  kSave,     // "Save Config"
  kSetting,  // a row of the table
  kBack,     // back to the main page
};

struct Item {
  ItemKind kind;
  int index;  // kGroup: the row of the heading. kSetting: the row
};

enum class Action {
  kNone,
  kPlay,
  kNewGame,
  kSave,
};

class Menu {
 public:
  Menu() { values_.assign(st::kSettingCount, 0); }

  // The values of the rows from bwvr.ini, as LoadAll of the launcher.
  void Load(const IniText& ini) {
    for (int i = 0; i < st::kSettingCount; ++i) {
      const st::Setting& s = st::kSettings[i];
      if (s.key == nullptr) continue;
      int value = ini.GetInt("bwvr", s.key, s.fallback);
      switch (s.kind) {
        case st::kBool:
          value = value != 0 ? 1 : 0;
          break;
        case st::kChoice: {
          const int count = st::ChoiceCount(s.choices);
          if (value < 0 || value >= count) value = s.fallback;
          break;
        }
        case st::kValues:
          // play_mode 0 is not a mode of the table, and it shows the God
          // view, as in the launcher.
          if (strcmp(s.key, "play_mode") == 0 && value == 0) value = 4;
          value = st::ChoiceIndex(s.choices, value);
          break;
        case st::kScale:
        case st::kSlider: {
          int low = 0, high = 0, step = 0;
          st::ScaleRange(s.choices, &low, &high, &step);
          if (value < low) value = low;
          if (value > high) value = high;
          break;
        }
        case st::kRenderer:
          // The headset app has the renderer Vulkan only.
          value = 0;
          break;
        default:
          break;
      }
      values_[i] = value;
    }
    headset_width_ = ini.GetInt("bwvr", "headset_width", 2064);
    headset_height_ = ini.GetInt("bwvr", "headset_height", 2208);
    guard_pct_ = ini.GetInt("bwvr", "one_walk_guard_pct", 25);
    changed_ = false;
    ++revision_;
  }

  // The values into bwvr.ini, as SaveAll of the launcher with the renderer
  // Vulkan, and vulkan_strict=1. The resolution of the game is not in the
  // ini: RenderSize gives it for the registry of the game.
  void Store(IniText* ini) const {
    for (int i = 0; i < st::kSettingCount; ++i) {
      const st::Setting& s = st::kSettings[i];
      if (s.key == nullptr || !Shown(i)) continue;
      switch (s.kind) {
        case st::kBool:
        case st::kInt:
        case st::kChoice:
        case st::kScale:
        case st::kSlider:
          ini->SetInt("bwvr", s.key, values_[i]);
          break;
        case st::kValues:
          ini->SetInt("bwvr", s.key, st::ChoiceValue(s.choices, values_[i]));
          break;
        case st::kRenderer:
          ini->Set("bwvr", s.key, "vulkan");
          break;
        default:
          break;
      }
    }
    // The keys with one right answer. The renderer Vulkan has no bridge of
    // Direct3D 9.
    ini->Remove("bwvr", "gpu_bridge");
    // The app draws with Vulkan alone: no fallback of the mod to another
    // DirectDraw, to Direct3D or to GDI (ddraw_proxy.cpp, "ONLY VULKAN").
    ini->SetInt("bwvr", "vulkan_strict", 1);
    ini->SetInt("bwvr", "vr", 1);
    ini->SetInt("bwvr", "stereo", 1);
    ini->SetInt("bwvr", "head_camera", 1);
    for (int i = 0; i < st::kRetiredKeyCount; ++i) {
      ini->Remove("bwvr", st::kRetiredKeys[i]);
    }
  }

  // The size of the render for the registry of the game (ScreenW, ScreenH),
  // from the resolution scale and the values of the ini.
  void RenderSize(int* width, int* height) const {
    int percent = 100, margin = 0;
    bool one_walk = true;
    for (int i = 0; i < st::kSettingCount; ++i) {
      const char* key = st::kSettings[i].key;
      if (key == nullptr) continue;
      if (strcmp(key, "resolution_pct") == 0) percent = values_[i];
      if (strcmp(key, "fov_margin_pct") == 0) {
        margin = st::ChoiceValue(st::kSettings[i].choices, values_[i]);
      }
      if (strcmp(key, "one_walk") == 0) one_walk = values_[i] != 0;
    }
    st::ScaleToPixels(percent, headset_width_, headset_height_, margin,
                      one_walk, guard_pct_, width, height);
  }

  // What the headset asks for, for each eye. The app writes it to the ini
  // too (headset_width, headset_height), as the mod does on the PC.
  void SetHeadsetSize(int width, int height) {
    headset_width_ = width;
    headset_height_ = height;
    ++revision_;
  }

  // THE ROWS OF THE PAGE.
  bool OnMain() const { return group_ < 0; }
  int Group() const { return group_; }
  int Cursor() const { return cursor_; }
  bool Changed() const { return changed_; }
  void MarkSaved() {
    changed_ = false;
    ++revision_;
  }
  // A count that goes up with each change of what the menu shows, so the app
  // paints the menu again only after a change.
  unsigned Revision() const { return revision_; }

  std::vector<Item> Items() const {
    std::vector<Item> items;
    if (OnMain()) {
      items.push_back({ItemKind::kPlay, -1});
      items.push_back({ItemKind::kNewGame, -1});
      for (int i = 0; i < st::kSettingCount; ++i) {
        if (st::kSettings[i].kind == st::kGroup && GroupHasRows(i)) {
          items.push_back({ItemKind::kGroup, i});
        }
      }
      items.push_back({ItemKind::kSave, -1});
      return items;
    }
    for (int i = group_ + 1; i < st::kSettingCount; ++i) {
      if (st::kSettings[i].kind == st::kGroup) break;
      if (Shown(i)) items.push_back({ItemKind::kSetting, i});
    }
    items.push_back({ItemKind::kBack, -1});
    return items;
  }

  // The page title: the name of the mod on the main page, else the group.
  std::string Title() const {
    if (OnMain()) return "Black & White VR Mod";
    return st::kSettings[group_].label;
  }

  std::string Label(const Item& item) const {
    switch (item.kind) {
      case ItemKind::kPlay:
        return "Play in VR";
      case ItemKind::kNewGame:
        return "Play, new game";
      case ItemKind::kSave:
        return changed_ ? "Save Config *" : "Save Config";
      case ItemKind::kBack:
        return "Back";
      case ItemKind::kGroup:
      case ItemKind::kSetting:
        return st::kSettings[item.index].label;
    }
    return "";
  }

  // The value of a row as text, for example "On", "45 degrees", "100% (2580
  // x 2064)". Empty for a command.
  std::string ValueText(const Item& item) const {
    if (item.kind != ItemKind::kSetting) return "";
    const int i = item.index;
    const st::Setting& s = st::kSettings[i];
    char text[160];
    text[0] = 0;
    switch (s.kind) {
      case st::kBool:
        snprintf(text, sizeof(text), "%s", values_[i] != 0 ? "On" : "Off");
        break;
      case st::kInt:
        snprintf(text, sizeof(text), "%d", values_[i]);
        break;
      case st::kChoice:
      case st::kValues:
      case st::kRenderer:
        st::ChoiceLabel(s.choices, values_[i], text, sizeof(text));
        break;
      case st::kSlider:
        snprintf(text, sizeof(text), "%d%s", values_[i],
                 st::SliderUnit(s.choices));
        break;
      case st::kScale: {
        int w = 0, h = 0;
        RenderSize(&w, &h);
        snprintf(text, sizeof(text), "%d%% (%d x %d)", values_[i], w, h);
        break;
      }
      default:
        break;
    }
    return text;
  }

  // The help text of a row. A group says the settings on its page. Empty
  // when the row has no help.
  std::string Help(const Item& item) const {
    if (item.kind == ItemKind::kSetting) {
      const char* help = st::kSettings[item.index].help;
      return help != nullptr ? help : "";
    }
    if (item.kind == ItemKind::kGroup) {
      std::string text = "The settings: ";
      bool first = true;
      for (int i = item.index + 1; i < st::kSettingCount; ++i) {
        if (st::kSettings[i].kind == st::kGroup) break;
        if (!Shown(i)) continue;
        if (!first) text += ", ";
        text += st::kSettings[i].label;
        first = false;
      }
      return text + ".";
    }
    switch (item.kind) {
      case ItemKind::kPlay:
        return "Saves the settings and starts the game in the headset.";
      case ItemKind::kNewGame:
        return "Saves the settings and starts a new game on the first "
               "island, with no menu.";
      case ItemKind::kSave:
        return "Writes the settings to bwvr.ini in the game folder and does "
               "not start the game.";
      case ItemKind::kBack:
        return "Goes back to the main page.";
      default:
        return "";
    }
  }

  // A row with a slider: the place of the value on the bar, 0 to 1. -1 for
  // a row with no slider.
  double SliderFraction(const Item& item) const {
    if (item.kind != ItemKind::kSetting) return -1.0;
    const st::Setting& s = st::kSettings[item.index];
    if (s.kind != st::kSlider && s.kind != st::kScale) return -1.0;
    int low = 0, high = 0, step = 0;
    st::ScaleRange(s.choices, &low, &high, &step);
    if (high <= low) return 0.0;
    return static_cast<double>(values_[item.index] - low) / (high - low);
  }

  // A row with a check box: 0 or 1. -1 for a row with no check box.
  int CheckValue(const Item& item) const {
    if (item.kind != ItemKind::kSetting) return -1;
    if (st::kSettings[item.index].kind != st::kBool) return -1;
    return values_[item.index];
  }

  // THE INPUT. The cursor is the row with the highlight.
  void SetCursor(int row) {
    const int count = static_cast<int>(Items().size());
    if (row < 0) row = 0;
    if (row >= count) row = count - 1;
    if (row != cursor_) ++revision_;
    cursor_ = row;
  }
  void Up() { SetCursor(cursor_ > 0 ? cursor_ - 1 : 0); }
  void Down() { SetCursor(cursor_ + 1); }

  // A step of the value of the row at the cursor: -1 to the left, +1 to the
  // right. A list goes round, a slider stops at its end.
  void Step(int direction) {
    const std::vector<Item> items = Items();
    if (cursor_ < 0 || cursor_ >= static_cast<int>(items.size())) return;
    const Item& item = items[cursor_];
    if (item.kind != ItemKind::kSetting) return;
    const int i = item.index;
    const st::Setting& s = st::kSettings[i];
    int value = values_[i];
    switch (s.kind) {
      case st::kBool:
        value = value != 0 ? 0 : 1;
        break;
      case st::kInt:
        value += direction;
        break;
      case st::kChoice:
      case st::kValues: {
        const int count = st::ChoiceCount(s.choices);
        if (count <= 0) return;
        value = (value + direction + count) % count;
        break;
      }
      case st::kScale:
      case st::kSlider: {
        int low = 0, high = 0, step = 0;
        st::ScaleRange(s.choices, &low, &high, &step);
        value = st::SliderSnap(s.choices, value + direction * (step > 0 ? step : 1));
        break;
      }
      default:
        return;
    }
    if (value != values_[i]) {
      values_[i] = value;
      changed_ = true;
      ++revision_;
    }
  }

  // A press on the slider of the row at the cursor, at `fraction` of the bar.
  void SetSliderAt(double fraction) {
    const std::vector<Item> items = Items();
    if (cursor_ < 0 || cursor_ >= static_cast<int>(items.size())) return;
    const Item& item = items[cursor_];
    if (SliderFraction(item) < 0.0) return;
    const st::Setting& s = st::kSettings[item.index];
    int low = 0, high = 0, step = 0;
    st::ScaleRange(s.choices, &low, &high, &step);
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    const int value = st::SliderSnap(
        s.choices, low + static_cast<int>(fraction * (high - low) + 0.5));
    if (value != values_[item.index]) {
      values_[item.index] = value;
      changed_ = true;
      ++revision_;
    }
  }

  // A select (the trigger, Enter) on the row at the cursor.
  Action Select() {
    const std::vector<Item> items = Items();
    if (cursor_ < 0 || cursor_ >= static_cast<int>(items.size())) {
      return Action::kNone;
    }
    const Item item = items[cursor_];
    switch (item.kind) {
      case ItemKind::kPlay:
        return Action::kPlay;
      case ItemKind::kNewGame:
        return Action::kNewGame;
      case ItemKind::kSave:
        return Action::kSave;
      case ItemKind::kGroup:
        group_ = item.index;
        cursor_ = 0;
        ++revision_;
        return Action::kNone;
      case ItemKind::kBack:
        Back();
        return Action::kNone;
      case ItemKind::kSetting:
        Step(1);
        return Action::kNone;
    }
    return Action::kNone;
  }

  // Back (B, the Back key): from a group page to the main page, on the entry
  // of that group.
  void Back() {
    if (OnMain()) return;
    const int was = group_;
    group_ = -1;
    cursor_ = 0;
    ++revision_;
    const std::vector<Item> items = Items();
    for (int r = 0; r < static_cast<int>(items.size()); ++r) {
      if (items[r].kind == ItemKind::kGroup && items[r].index == was) {
        cursor_ = r;
      }
    }
  }

  // The value of a row by its key, for a test and for the log.
  int ValueOf(const char* key) const {
    for (int i = 0; i < st::kSettingCount; ++i) {
      if (st::kSettings[i].key != nullptr &&
          strcmp(st::kSettings[i].key, key) == 0) {
        return values_[i];
      }
    }
    return -1;
  }

 private:
  // A row is on its page when the renderer Vulkan uses it and the launcher
  // of the PC is not the only place for it.
  static bool Shown(int i) {
    return !st::OnlyDirect3D9(st::kSettings[i].key) &&
           !st::OnlyLauncher(st::kSettings[i].key);
  }

  static bool GroupHasRows(int group) {
    for (int i = group + 1; i < st::kSettingCount; ++i) {
      if (st::kSettings[i].kind == st::kGroup) return false;
      if (Shown(i)) return true;
    }
    return false;
  }

  std::vector<int> values_;
  int group_ = -1;
  int cursor_ = 0;
  bool changed_ = false;
  unsigned revision_ = 0;
  int headset_width_ = 2064;
  int headset_height_ = 2208;
  int guard_pct_ = 25;
};

}  // namespace menu_model
