// The settings window and the welcome page: the table of the settings, the
// words of their values, the text that goes into bwvr.ini, and the layout of
// the welcome page. See settings_panel.cpp, docs/controllers.md, "The
// settings window", and docs/ui-layer.md, "The settings window". No Windows
// and no game here, so the math test compiles this file
// (settings_panel_checks.h).
//
// THE TABLE has one line for each setting. The words are the words of the
// launcher (launcher.cpp, kSettings): a player knows them from there. A line
// has the group (the tab), the ini key, the label of the row, the first line
// of the note, the values, the default of the mod, when a change acts, what
// the setting does, and whether the window shows it with settings_lean=1.
//
// THE VALUES are a list of "word=value" with "|" between them, as the rows
// kValues of the launcher. The ini gets the value, not the place in the
// list. A row with two values is a toggle: the first value is the value
// that is "off", so the knob is at the left for it. A row with more values
// has two arrows, and a step goes round the list.
//
// THE LEAN WINDOW (settings_lean=1) has only the rows that a player changes:
// the turn, the walk, the zoom, the comfort, the grab of the world and the
// trail. The other lines of the table stay for settings_lean=0, and their
// keys still act. The Hand and the hints follow the options of the game
// (hand_follow_game, hints_follow_game), and the other keys have one value
// that each player uses.

#pragma once

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

namespace settings_math {

// --- the table -------------------------------------------------------------------

enum Group { kGroupMoving, kGroupHands, kGroupCards, kGroups };

const wchar_t* const kGroupName[kGroups] = {L"Moving", L"Hands", L"Cards"};

// When a change acts.
enum When {
  kNow = 0,        // at once: the mod has a change function for it
  kNextStart = 1,  // at the next start of the game
};

// The settings, in the order of the table.
enum Id {
  kSnapTurn,
  kTurnSpeed,
  kTeleport,
  kMoveStick,
  kMoveSpeed,
  kZoomSmooth,
  kVignette,
  kBlink,
  kRecenterHold,
  kLeftHand,
  kGrabHand,
  kGestureTrail,
  kOpenStroke,
  kHintLearn,
  kCardStyle,
  kSubtitleCard,
  kSettings
};

struct Setting {
  int id;
  int group;
  const char* key;
  const wchar_t* label;   // the row, at most 18 letters
  const wchar_t* name;    // the first line of the note: the launcher label
  const wchar_t* values;  // "word=value|word=value"
  int fallback;           // the default of the mod
  int when;
  const wchar_t* tip;
  bool lean;  // a row of the lean window
};

const Setting kTable[kSettings] = {
    {kSnapTurn, kGroupMoving, "snap_turn", L"Turn", L"Turn",
     L"Off=0|30 degrees=1|45 degrees=2|90 degrees=3|Smooth=4", 2, kNow,
     L"Push the stick of your Hand left or right to turn. You stay where you "
     L"are. 30, 45 or 90 degrees: one push turns you in one step. Smooth: "
     L"you turn while you hold the stick, and a larger push turns faster. "
     L"Off: the stick does not turn you.", true},
    {kTurnSpeed, kGroupMoving, "smooth_turn_speed", L"Turn speed",
     L"Speed of the smooth turn", L"Slow=45|Normal=90|Fast=135|Very fast=180",
     90, kNow,
     L"How fast the smooth turn turns you at a full push of the stick. "
     L"Normal is a quarter turn each second. It acts when Turn is Smooth.", true},
    {kTeleport, kGroupMoving, "teleport", L"Teleport", L"Teleport",
     L"Off=0|On=1", 1, kNow,
     L"Hold A to aim an arc. Let go of A to go to the ring at its end. B "
     L"stops the aim. Off: A does a jump, as in the game.", false},
    {kMoveStick, kGroupMoving, "move_stick", L"Stick walk",
     L"Move with the other stick", L"Off=0|Head=1|Controller=2", 1, kNow,
     L"Push the stick of your other hand to move over the land. Head: "
     L"forward is where your head faces. Controller: forward is where that "
     L"controller points. Off: the stick does not move you.", true},
    {kMoveSpeed, kGroupMoving, "move_stick_speed_cm", L"Walk speed",
     L"Speed of the stick walk",
     L"Slow=75|Normal=150|Fast=250|Very fast=400", 150, kNow,
     L"How fast the stick of your other hand moves you at a full push. "
     L"Normal is 1.5 m of the room each second.", true},
    {kZoomSmooth, kGroupMoving, "god_zoom_smooth", L"Smooth zoom",
     L"Zoom with the stick", L"In steps=0|Smooth=1", 0, kNow,
     L"In steps: one push of the stick of your Hand forward or back is one "
     L"zoom step. Smooth: the zoom changes while you hold the stick.", true},
    {kVignette, kGroupMoving, "comfort_vignette", L"Comfort vignette",
     L"Comfort vignette", L"Off=0|Gentle=1|Strong=2", 0, kNow,
     L"The edges of your view go dark while a control moves you. The middle "
     L"of the view stays clear. Some people feel less ill with it.", true},
    {kBlink, kGroupMoving, "blink", L"Blink", L"Blink when you jump or turn",
     L"Off=0|Short=1|Long=2", 0, kNow,
     L"Shows black for a moment when you jump or turn. Short is 0.1 seconds "
     L"and long is 0.2 seconds.", true},
    {kRecenterHold, kGroupMoving, "recenter_hold", L"Recenter hold",
     L"Recenter with a hold of the stick", L"Off=0|Other hand=1|Hand=2", 1,
     kNow,
     L"Hold the stick click for 1 second, with the stick at rest, to "
     L"recenter the view. Other hand: the stick of your other hand. Hand: "
     L"the stick of your Hand. Off: no hold recenters.", false},
    {kLeftHand, kGroupHands, "left_hand", L"Hand", L"Hand",
     L"Game=-1|Right=0|Left=1", -1, kNow,
     L"Which controller is the Hand. Game: the left handed option of the "
     L"game chooses it. The Hand changes when you let go of each button.", false},
    {kGrabHand, kGroupHands, "grab_hand", L"Grab the world",
     L"Grab the world", L"Off=0|Other grip=1|Either grip=2", 1, kNextStart,
     L"Hold the grip of your other hand and move that hand to pull the "
     L"world. Either grip: the grip of your Hand also grabs the world when "
     L"the Hand is empty. Off: the grips do not move the world.", true},
    {kGestureTrail, kGroupHands, "gesture_trail", L"Gesture trail",
     L"Trail for gestures", L"Off=0|On=1", 1, kNow,
     L"While you draw a gesture, a gold ribbon follows the tip of your index "
     L"finger. It shows what you draw.", true},
    {kOpenStroke, kGroupHands, "creature_stroke_open", L"Open hand stroke",
     L"Stroke with the open hand", L"Off=0|On=1", 1, kNow,
     L"Put your open hand on your Creature for a moment, then move it slowly "
     L"to stroke the Creature. Off: only the grip strokes the Creature.", false},
    {kHintLearn, kGroupCards, "hint_learn", L"Hints", L"Hints",
     L"Always=0|Until learned=1", 1, kNow,
     L"The cards beside your wrists show what each button does. Until "
     L"learned: a line goes away after you do its action 3 times. Always: "
     L"each line always shows.", false},
    {kCardStyle, kGroupCards, "hint_card_style", L"Card look", L"Card look",
     L"Plain=0|Scroll=1", 1, kNow,
     L"The look of the cards, the ring, the spell book and these windows. "
     L"Scroll: parchment with dark letters. Plain: dark cards with white "
     L"letters.", false},
    {kSubtitleCard, kGroupCards, "subtitle_card", L"Subtitle card",
     L"Subtitles on a card", L"Off=0|On=1", 1, kNextStart,
     L"The subtitles and the help text of the game show on a card in front "
     L"of you. Off: they show in the picture of the game.", false},
};

// --- the tabs --------------------------------------------------------------------

// A row of the table shows in the window: each row with settings_lean=0,
// and the rows of the lean window with 1.
inline bool InWindow(int id, bool lean) {
  return id >= 0 && id < kSettings && (!lean || kTable[id].lean);
}

// THE TABS of the rows: the groups that have a row in the window, in their
// order. With settings_lean=1 the group Cards has no row, so it has no tab.
inline bool GroupShows(int group, bool lean) {
  for (int i = 0; i < kSettings; ++i) {
    if (kTable[i].group == group && InWindow(i, lean)) return true;
  }
  return false;
}
inline int TabCount(bool lean) {
  int n = 0;
  for (int g = 0; g < kGroups; ++g) n += GroupShows(g, lean) ? 1 : 0;
  return n;
}
// The group of a tab, or -1.
inline int GroupOfTab(int tab, bool lean) {
  int n = 0;
  for (int g = 0; g < kGroups; ++g) {
    if (!GroupShows(g, lean)) continue;
    if (n == tab) return g;
    ++n;
  }
  return -1;
}
// The tab of a group, or -1 when the group has no tab.
inline int TabOfGroup(int group, bool lean) {
  if (group < 0 || group >= kGroups || !GroupShows(group, lean)) return -1;
  int n = 0;
  for (int g = 0; g < group; ++g) n += GroupShows(g, lean) ? 1 : 0;
  return n;
}

// --- the restart ------------------------------------------------------------------

// THE RESTART. A row that acts at the next start (kNextStart) has no words
// of its own. While its value is not the value of the start of this game,
// the bottom row of the window says kRestartLine at its left.
const wchar_t* const kRestartLine = L"Restart the game to use the changes.";
inline bool RestartNeeded(const int at_start[kSettings],
                          const int now[kSettings]) {
  for (int i = 0; i < kSettings; ++i) {
    if (kTable[i].when == kNextStart && at_start[i] != now[i]) return true;
  }
  return false;
}

// --- the values ------------------------------------------------------------------

// The count of the values of a list: "Off=0|On=1" is 2.
inline int ValueCount(const wchar_t* values) {
  if (values == nullptr || values[0] == 0) return 0;
  int n = 1;
  for (const wchar_t* p = values; *p != 0; ++p) {
    if (*p == L'|') ++n;
  }
  return n;
}

// Value `index` of the list: its word into `word` and its number into
// `value`. False when the list has no such value.
inline bool ValueAt(const wchar_t* values, int index, wchar_t* word,
                    size_t size, int* value) {
  if (values == nullptr || index < 0) return false;
  const wchar_t* p = values;
  for (int i = 0; i < index; ++i) {
    p = wcschr(p, L'|');
    if (p == nullptr) return false;
    ++p;
  }
  const wchar_t* end = wcschr(p, L'|');
  if (end == nullptr) end = p + wcslen(p);
  const wchar_t* equals = p;
  while (equals < end && *equals != L'=') ++equals;
  if (equals >= end) return false;
  if (word != nullptr && size > 0) {
    size_t n = static_cast<size_t>(equals - p);
    if (n >= size) n = size - 1;
    wmemcpy(word, p, n);
    word[n] = 0;
  }
  int number = 0;
  bool negative = false;
  const wchar_t* q = equals + 1;
  if (q < end && *q == L'-') {
    negative = true;
    ++q;
  }
  if (q >= end) return false;
  for (; q < end; ++q) {
    if (*q < L'0' || *q > L'9') return false;
    number = number * 10 + (*q - L'0');
  }
  if (value != nullptr) *value = negative ? -number : number;
  return true;
}

// The place in the list of a value, or -1 when the list does not have it.
inline int IndexOfValue(const wchar_t* values, int value) {
  const int count = ValueCount(values);
  for (int i = 0; i < count; ++i) {
    int v = 0;
    if (ValueAt(values, i, nullptr, 0, &v) && v == value) return i;
  }
  return -1;
}

// THE STEP of a row: the next value of the list, or the value before with
// `back`, round the list. A value that the list does not have (a value that
// a person wrote in bwvr.ini) steps to the first value forward, and to the
// last value back.
inline int StepIndex(int index, int count, bool back) {
  if (count <= 0) return -1;
  if (index < 0 || index >= count) return back ? count - 1 : 0;
  return back ? (index + count - 1) % count : (index + 1) % count;
}

// The text that goes into bwvr.ini for a value, as the launcher writes it.
inline void IniText(int value, char* out, size_t size) {
  snprintf(out, size, "%d", value);
}

// THE VALUES OF TWO KEYS are a place in a list of the mod (mod_main.cpp):
// snap_turn 0 to 3 is 0, 30, 45 or 90 degrees, and blink 0 to 2 is 0, 100 or
// 200 ms. A value out of the list gives the default of the mod. snap_turn 4
// is the smooth turn of the God view (SnapTurnSmooth). The turns that have
// no smooth turn (the middle mouse, a view that is not the God view, a room
// of the Temple) then turn in steps of 45 degrees.
inline int SnapDegrees(int snap_turn) {
  static const int kSteps[] = {0, 30, 45, 90, 45};
  return snap_turn >= 0 && snap_turn < 5 ? kSteps[snap_turn] : 45;
}
inline bool SnapTurnSmooth(int snap_turn) { return snap_turn == 4; }
inline int BlinkMs(int blink) {
  static const int kSteps[] = {0, 100, 200};
  return blink >= 0 && blink < 3 ? kSteps[blink] : 0;
}

// --- the welcome page ------------------------------------------------------------
//
// THE PICTURE is 1024 by 640 pixels:
//
//   the title      "Welcome" at the top
//   the words      two lines that say what the mod is
//   the controls   three lines, each a control and what it does
//   the buttons    Settings at the left and Play at the right
//
// The page is 56 by 35 cm, 1 m in front of the head, 12 degrees below eye
// level, at the place of the settings window.

const int kWelcomeWidth = 1024;
const int kWelcomeHeight = 640;
const float kWelcomeMeters = 0.56f;
const float kWelcomeMetersHigh = 0.35f;

const int kWelcomeLeft = 64;
const int kWelcomeRight = 960;
const int kWelcomeTitleTop = 28;
const int kWelcomeTitleBottom = 100;
const int kWelcomeWordsTop = 120;
const int kWelcomeWordsBottom = 230;
const int kWelcomeControlsTop = 250;
const int kWelcomeControlPitch = 64;
const int kWelcomeControls = 3;
const int kWelcomeButtonTop = 530;
const int kWelcomeButtonHeight = 76;
const int kWelcomeButtonWidth = 260;

enum WelcomePart { kWelcomeSettings = 0, kWelcomePlay = 1, kWelcomeParts };

// The rect (x, y, w, h) of a button.
inline void WelcomeButtonRect(int part, int out[4]) {
  out[0] = part == kWelcomeSettings ? kWelcomeLeft + 60
                                    : kWelcomeRight - 60 - kWelcomeButtonWidth;
  out[1] = kWelcomeButtonTop;
  out[2] = kWelcomeButtonWidth;
  out[3] = kWelcomeButtonHeight;
}

// The box (x0, y0, x1, y1) of control line `line`.
inline void WelcomeControlBox(int line, int out[4]) {
  out[0] = kWelcomeLeft;
  out[1] = kWelcomeControlsTop + line * kWelcomeControlPitch;
  out[2] = kWelcomeRight;
  out[3] = out[1] + kWelcomeControlPitch - 8;
}

// The button at u, v of the page (0 to 1 across and down), or -1.
inline int WelcomePartAt(float u, float v) {
  if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) return -1;
  const float x = u * kWelcomeWidth;
  const float y = v * kWelcomeHeight;
  for (int part = 0; part < kWelcomeParts; ++part) {
    int r[4];
    WelcomeButtonRect(part, r);
    if (x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3]) {
      return part;
    }
  }
  return -1;
}

// The middle of a button in u, v, for the scripted controller.
inline void WelcomePartMiddle(int part, float* u, float* v) {
  int r[4];
  WelcomeButtonRect(part, r);
  *u = (r[0] + r[2] * 0.5f) / kWelcomeWidth;
  *v = (r[1] + r[3] * 0.5f) / kWelcomeHeight;
}

// THE FIRST START. The page shows when welcome_page is 1 and bwvr.ini has no
// welcome_done=1.
inline bool WelcomeShows(int welcome_page, int welcome_done) {
  return welcome_page != 0 && welcome_done != 1;
}

// The page opens after this many seconds in which the controllers can act,
// with no dialog box, no cut scene and no other panel. So it does not open
// over the first picture of a load.
const double kWelcomeWaitSeconds = 3.0;

}  // namespace settings_math
