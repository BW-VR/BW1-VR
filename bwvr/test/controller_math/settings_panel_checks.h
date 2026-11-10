// THE SETTINGS WINDOW AND THE WELCOME PAGE (docs/ui-layer.md, "The settings
// window"): the table, the words of the values, the text for bwvr.ini, the
// ring as made, and the layout of the welcome page. main.cpp includes this
// file after Check and Near, just before main.
#pragma once

#include <math.h>
#include <string.h>
#include <wchar.h>

#include "../../src/mod/debug_window_math.h"
#include "../../src/mod/radial_menu_math.h"
#include "../../src/mod/settings_panel_math.h"

namespace settings_panel_checks {

inline void SettingsPanelChecks() {
  using namespace settings_math;
  printf("\nThe settings window and the welcome page\n");

  // The table: each line is in the order of its id, has a key, values with
  // a word and a number each, and the default of the mod among its values.
  bool order = true, keys = true, values = true, fallback = true;
  bool groups = true, labels = true, rows = true;
  int in_group[kGroups] = {};
  for (int i = 0; i < kSettings; ++i) {
    const Setting& s = kTable[i];
    if (s.id != i) order = false;
    if (s.key == nullptr || s.key[0] == 0) keys = false;
    if (s.group < 0 || s.group >= kGroups) {
      groups = false;
    } else {
      ++in_group[s.group];
    }
    const int count = ValueCount(s.values);
    if (count < 2) values = false;
    for (int k = 0; k < count; ++k) {
      wchar_t word[40];
      int v = 0;
      if (!ValueAt(s.values, k, word, 40, &v) || word[0] == 0) values = false;
      // A value word fits between the arrows: at most 13 letters.
      if (wcslen(word) > 13) values = false;
    }
    if (IndexOfValue(s.values, s.fallback) < 0) fallback = false;
    if (s.label == nullptr || wcslen(s.label) > 18) labels = false;
    for (int j = 0; j < i; ++j) {
      if (strcmp(kTable[j].key, s.key) == 0) keys = false;
    }
  }
  for (int g = 0; g < kGroups; ++g) {
    if (in_group[g] == 0 || in_group[g] > debugwin_math::kViewRows) {
      rows = false;
    }
  }
  Check(order, "each line of the table is at the place of its id", order,
        1);
  Check(keys, "each line has its own ini key", keys, 1);
  Check(groups, "each line is in a group of the window", groups, 1);
  Check(rows, "each group has 1 to 10 rows, so no tab needs a scroll bar",
        rows, 1);
  Check(values, "each line has 2 or more values, each with a word of 13 "
        "letters at most and a number", values, 1);
  Check(fallback, "the default of the mod is a value of each line",
        fallback, 1);
  Check(labels, "each label fits in the row: 18 letters at most", labels, 1);
  Check(kGroups <= debugwin_math::kMostTabs, "the groups fit in the tabs",
        kGroups, debugwin_math::kMostTabs);

  // The words of the launcher: the same keys and the same values.
  Check(IndexOfValue(kTable[kVignette].values, 2) == 2,
        "comfort_vignette 2 is Strong, the third value",
        IndexOfValue(kTable[kVignette].values, 2), 2);
  Check(IndexOfValue(kTable[kLeftHand].values, -1) == 0,
        "left_hand -1 (the option of the game) is the first value",
        IndexOfValue(kTable[kLeftHand].values, -1), 0);
  Check(IndexOfValue(kTable[kMoveStick].values, 1) == 0 &&
            IndexOfValue(kTable[kMoveStick].values, 0) == -1,
        "move_stick 1 (head) is the first value, and the window has no off",
        IndexOfValue(kTable[kMoveStick].values, 1), 0);
  Check(IndexOfValue(kTable[kSnapTurn].values, 4) == 0 &&
            IndexOfValue(kTable[kSnapTurn].values, 0) == -1,
        "snap_turn 4 (smooth) is the first value, and the window has no off",
        IndexOfValue(kTable[kSnapTurn].values, 4), 0);
  Check(ShownValue(kSnapTurn, 0) == 2 && ShownValue(kMoveStick, 0) == 1 &&
            ShownValue(kSnapTurn, 3) == 3 && ShownValue(kMoveSpeed, 0) == 0,
        "snap_turn 0 shows as 45 degrees and move_stick 0 as the head, and "
        "the other values show as they are", ShownValue(kSnapTurn, 0), 2);
  Check(IndexOfValue(kTable[kMoveType].values, 0) == 0 &&
            IndexOfValue(kTable[kMoveType].values, 1) == 1 &&
            kTable[kMoveType].fallback == 0 &&
            !IsToggle(kTable[kMoveType].values),
        "move_type: Walk 0 is the default, Fly 1, with arrows",
        kTable[kMoveType].fallback, 0);
  Check(IndexOfValue(kTable[kSnapTurn].values, 7) == -1,
        "a value that the list does not have has no place",
        IndexOfValue(kTable[kSnapTurn].values, 7), -1);
  {
    wchar_t word[40];
    int v = 0;
    const bool ok = ValueAt(kTable[kMoveSpeed].values, 3, word, 40, &v);
    Check(ok && wcscmp(word, L"Very fast") == 0 && v == 400,
          "the fourth value of the walk speed is \"Very fast\", 400 cm", v,
          400);
    Check(!ValueAt(kTable[kMoveSpeed].values, 4, word, 40, &v),
          "no fifth value of the walk speed", 0, 0);
    // A toggle: the first value is "off", so the knob is at the left.
    ValueAt(kTable[kGestureTrail].values, 0, word, 40, &v);
    Check(v == 0 && debugwin_math::KindOfRow(
                        ValueCount(kTable[kGestureTrail].values), false) ==
                        debugwin_math::kRowToggle,
          "the gesture trail is a toggle with off first", v, 0);
  }
  for (int i = 0; i < kSettings; ++i) {
    const int count = ValueCount(kTable[i].values);
    const int kind = debugwin_math::KindOfRow(count, false);
    if ((count == 2) != (kind == debugwin_math::kRowToggle)) {
      Check(false, "a row with 2 values is a toggle", count, 2);
    }
  }

  // The step goes round the list, and a value out of the list steps to an
  // end.
  Check(StepIndex(0, 4, false) == 1, "a step forward from the first value",
        StepIndex(0, 4, false), 1);
  Check(StepIndex(3, 4, false) == 0, "a step forward from the last value "
        "goes to the first", StepIndex(3, 4, false), 0);
  Check(StepIndex(0, 4, true) == 3, "a step back from the first value goes "
        "to the last", StepIndex(0, 4, true), 3);
  Check(StepIndex(-1, 4, false) == 0, "a value out of the list steps "
        "forward to the first value", StepIndex(-1, 4, false), 0);
  Check(StepIndex(-1, 4, true) == 3, "a value out of the list steps back "
        "to the last value", StepIndex(-1, 4, true), 3);
  Check(StepIndex(1, 2, false) == 0 && StepIndex(0, 2, false) == 1,
        "a toggle changes each time", StepIndex(1, 2, false), 0);

  // The text for bwvr.ini, as the launcher writes it.
  {
    char text[16];
    IniText(-1, text, sizeof(text));
    Check(strcmp(text, "-1") == 0, "left_hand -1 goes into bwvr.ini as -1",
          0, 0);
    IniText(150, text, sizeof(text));
    Check(strcmp(text, "150") == 0, "150 goes into bwvr.ini as 150", 0, 0);
  }
  Check(SnapDegrees(3) == 90 && SnapDegrees(0) == 0 && SnapDegrees(9) == 45,
        "snap_turn 3 is 90 degrees, 0 is off, and a value out of the list is "
        "45", SnapDegrees(3), 90);
  Check(SnapTurnSmooth(4) && !SnapTurnSmooth(2) && SnapDegrees(4) == 45,
        "snap_turn 4 is the smooth turn, with steps of 45 degrees where no "
        "smooth turn is", SnapDegrees(4), 45);
  {
    wchar_t word[40];
    int v = 0;
    const bool ok = ValueAt(kTable[kSnapTurn].values, 0, word, 40, &v);
    Check(ok && wcscmp(word, L"Smooth") == 0 && v == 4,
          "the first value of the turn is \"Smooth\", 4", v, 4);
    Check(IndexOfValue(kTable[kTurnSpeed].values, 90) == 1 &&
              kTable[kTurnSpeed].fallback == 90,
          "the turn speed is 90 degrees a second by default, the second value",
          IndexOfValue(kTable[kTurnSpeed].values, 90), 1);
  }
  Check(BlinkMs(2) == 200 && BlinkMs(5) == 0,
        "blink 2 is 200 ms, and a value out of the list is no blink",
        BlinkMs(2), 200);

  // The ring as made: "settings" has the slot of "names" on page 2, and the
  // other items stay where they are.
  {
    using namespace radial_math;
    int with[kSlots], without[kSlots], marks[kSlots], marks_with[kSlots];
    char unknown[32];
    ParseItems(kDefaultPage2Settings, with, unknown, sizeof(unknown));
    ParseItems(kDefaultPage2, without, unknown, sizeof(unknown));
    ParseItems(kDefaultPage2Marks, marks, unknown, sizeof(unknown));
    ParseItems(kDefaultPage2MarksSettings, marks_with, unknown,
               sizeof(unknown));
    int moved = 0, moved_marks = 0, at = -1;
    for (int i = 0; i < kSlots; ++i) {
      if (with[i] != without[i]) ++moved;
      if (marks_with[i] != marks[i]) ++moved_marks;
      if (with[i] == kItemSettings) at = i;
    }
    Check(moved == 1 && moved_marks == 1 && at >= 0 &&
              without[at] == ItemOfId("names", 5) &&
              marks_with[at] == kItemSettings,
          "page 2 as made: \"settings\" takes the slot of \"names\" only", at,
          6);
    Check(ItemOfId("settings", 8) == kItemSettings &&
              kItemInfo[kItemSettings].how == kHowSettings,
          "the word \"settings\" is the item of the settings window",
          ItemOfId("settings", 8), kItemSettings);
  }

  // The welcome page: the buttons are on the page, apart, and the controls
  // are between the words and the buttons.
  {
    int a[4], b[4];
    WelcomeButtonRect(kWelcomeSettings, a);
    WelcomeButtonRect(kWelcomePlay, b);
    Check(a[0] >= 0 && b[0] + b[2] <= kWelcomeWidth &&
              a[1] + a[3] <= kWelcomeHeight && a[0] + a[2] + 40 < b[0],
          "the two buttons are on the page, with room between them",
          b[0] - (a[0] + a[2]), 40);
    int last[4];
    WelcomeControlBox(kWelcomeControls - 1, last);
    Check(last[3] <= kWelcomeButtonTop &&
              kWelcomeControlsTop >= kWelcomeWordsBottom,
          "the controls are between the words and the buttons", last[3],
          kWelcomeButtonTop);
    float u = 0.0f, v = 0.0f;
    WelcomePartMiddle(kWelcomePlay, &u, &v);
    Check(WelcomePartAt(u, v) == kWelcomePlay,
          "the middle of Play is on Play", WelcomePartAt(u, v), kWelcomePlay);
    WelcomePartMiddle(kWelcomeSettings, &u, &v);
    Check(WelcomePartAt(u, v) == kWelcomeSettings,
          "the middle of Settings is on Settings", WelcomePartAt(u, v),
          kWelcomeSettings);
    Check(WelcomePartAt(0.5f, 0.3f) == -1 && WelcomePartAt(1.2f, 0.9f) == -1,
          "the words and a point off the page are no button", 0, 0);
    Check(fabs(kWelcomeMeters / kWelcomeMetersHigh -
               static_cast<float>(kWelcomeWidth) / kWelcomeHeight) < 1e-4,
          "the page in the room has the shape of the picture",
          kWelcomeMeters / kWelcomeMetersHigh,
          static_cast<float>(kWelcomeWidth) / kWelcomeHeight);
  }
  Check(WelcomeShows(1, 0) && !WelcomeShows(1, 1) && !WelcomeShows(0, 0),
        "the welcome page shows with welcome_page=1 and no welcome_done=1, "
        "and never with welcome_page=0", 0, 0);

  // THE LEAN WINDOW (settings_lean=1): the rows that a player changes, in
  // the tabs Moving and Hands. The group Cards has no row and no tab.
  {
    int lean_rows = 0;
    bool hidden_ok = true;
    for (int i = 0; i < kSettings; ++i) {
      if (InWindow(i, true)) ++lean_rows;
      if (!InWindow(i, false)) hidden_ok = false;
    }
    const bool gone = !InWindow(kTeleport, true) &&
                      !InWindow(kRecenterHold, true) &&
                      !InWindow(kLeftHand, true) &&
                      !InWindow(kOpenStroke, true) &&
                      !InWindow(kGestureTrail, true) &&
                      !InWindow(kHintLearn, true) &&
                      !InWindow(kCardStyle, true) &&
                      !InWindow(kSubtitleCard, true);
    Check(lean_rows == 9 && gone && hidden_ok,
          "the lean window has 9 rows, with no Teleport, Recenter hold, Hand, "
          "Open hand stroke, Gesture trail, Hints, Card look or Subtitle "
          "card, and the old window has each row", lean_rows, 9);
    Check(TabCount(true) == 2 && TabCount(false) == 3 &&
              GroupOfTab(0, true) == kGroupMoving &&
              GroupOfTab(1, true) == kGroupHands &&
              GroupOfTab(2, true) == -1 &&
              TabOfGroup(kGroupCards, true) == -1 &&
              TabOfGroup(kGroupCards, false) == 2,
          "the lean window has the tabs Moving and Hands, and the old window "
          "also has Cards", TabCount(true), 2);
  }

  // THE TOGGLES: only the rows of Off and On. Smooth zoom (In steps,
  // Smooth) is a choice of two ways, with arrows.
  Check(IsToggle(kTable[kGestureTrail].values) &&
            IsToggle(kTable[kTeleport].values) &&
            !IsToggle(kTable[kZoomSmooth].values) &&
            !IsToggle(kTable[kSnapTurn].values) &&
            !IsToggle(kTable[kCardStyle].values),
        "a toggle is a row of Off and On, and the other rows have arrows", 0,
        0);

  // THE RESTART. A change of a row that acts at the next start asks for a
  // restart until it is back at the value of the start.
  {
    int start[kSettings], now[kSettings];
    for (int i = 0; i < kSettings; ++i) start[i] = now[i] = kTable[i].fallback;
    const bool none = !RestartNeeded(start, now);
    now[kSnapTurn] = 3;
    const bool live = !RestartNeeded(start, now);
    now[kGrabHand] = 2;
    const bool later = RestartNeeded(start, now);
    now[kGrabHand] = start[kGrabHand];
    const bool back = !RestartNeeded(start, now);
    Check(none && live && later && back && wcslen(kRestartLine) < 48,
          "the bottom row asks for a restart after a change of Grab the "
          "world, not after a change that acts at once, and not when the "
          "value is back", later, 1);
  }
}

}  // namespace settings_panel_checks
