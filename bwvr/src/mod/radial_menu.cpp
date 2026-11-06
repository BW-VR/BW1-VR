// The ring. See docs/controllers.md, "The ring", and docs/controls-audit.md.
//
// WHY. The plain game needs the keyboard for the Temple and its rooms, the
// Creature, the kinds of the leash, the bookmarks, the pause and more
// (docs/controls-audit.md, "What a player cannot do with the controllers"). A player in a headset has no keyboard.
// The ring is on the controller of the hand that does not point.
//
// WHAT. A ring of 8 items opens at that hand. It is one picture in the room
// (a quad layer, as the hint cards), so it is sharp and it is never double.
// It stays where it opened, and the hand moves over it. There are two ways
// to use it (radial_menu_math.h):
//
//   Hold the button, move the hand toward an item, let go of the button.
//   Push the stick toward an item, pull the trigger of the same hand.
//
// A release in the middle, and a stick that rests in the middle for half a
// second, choose nothing.
//
// THE LAYOUT OF THE BUTTONS (button_layout, controllers.cpp) chooses the
// face button and the job of the stick. With 1 the button is the second face
// button (Y), and the stick does not drive the ring: it moves the player
// (move_stick). A controller with no face buttons keeps the ring on its
// stick. With 0 the button is the first face button (X), and the stick
// drives the ring too.
//
// HOW AN ITEM ACTS. Each item is a key of the plain game. The mod writes the
// key into the key buffer of the game (GKeyBuffer::AddKey 0x005E1BB0), and
// the game reads it at its own point of the frame with GGame::ProcessKey
// 0x0063EF20. So each gate of the game holds: a dialog box, a cut scene, the
// Temple, the tutorial. The key needs no window in front, and it does not
// touch the key table, so a Ctrl of the ring does not start the zoom of the
// camera. For an action of the control map the mod reads the key that the
// player gave it in the Options Room.
//
// THE KEYS of bwvr.ini:
//
//   radial_menu=1              0 turns the ring off
//   radial_menu_button=1       what opens the ring with a hold: 0 nothing,
//                              1 the face button of button_layout (Y or X),
//                              2 the stick click, 3 the trigger
//   radial_menu_stick=1        1 the stick opens the ring and points, and the
//                              trigger chooses. It acts with button_layout=0,
//                              and on a controller with no face buttons
//   radial_menu_items          the 8 items of the first page, from the top
//                              and clockwise, with commas
//   radial_menu_items2         the second page. "more" changes the page
//   places_panel=1             1 the item "places" opens the panel of
//                              places and bookmarks (places_panel.cpp), and
//                              page 2 as made has it in place of the three
//                              bookmark items. 0 page 2 has mark_set,
//                              mark_go and mark_back, and the word "places"
//                              is an empty slot
//   snapshot_item=1            1 the item "snapshot" saves the eye pictures
//                              and a copy of the log for a report
//                              (snapshot.cpp). 0 the word is an empty slot
//   radial_menu_items_temple   the ring in the Temple
//   radial_menu_reach_cm=5     how far the hand moves to pick an item
//   radial_menu_size_pct=100   the size of the ring, 50 to 200
//   radial_menu_creature=1     the item "creature" in the God view: 1 one
//                              teleport to the Creature, 0 the key of the
//                              game, which starts its follow camera
//   hint_card_style=1          the key of the hint cards. 1 each item is a
//                              wax seal and the card in the middle is a
//                              parchment scroll (card_scroll.h). 0 dark
//                              round boxes with white letters
//
// With test_keys=1, Ctrl and the 1 key of the number pad go through the two
// ways while the stick can drive the ring: both, the button only, the stick
// only. radial_test_save=N (a test key) saves the first N pictures of the
// ring as bwvr-ring-NNNNN.bmp.
//
// THE DEBUG PAGE (debug_menu=1, debug_menu.cpp). The ring gets the item
// "debug", and its pages hold the switches of the test keys
// (debug_menu_math.h). A badge of a switch has its word and its value now.
// After a choice of a switch, the ring opens on that page again for 30 s, so
// a second choice for an A/B needs no walk through the pages. With
// debug_window=1 (the default) the item "debug" opens the debug window
// (debug_window.cpp) in place of the pages.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../vr/xr_screen.h"
#include "audio.h"
#include "card_scroll.h"
#include "controllers.h"
#include "cutscene_window.h"
#include "debug_menu.h"
#include "debug_menu_math.h"
#include "debug_window.h"
#include "haptics.h"
#include "hint_card_math.h"
#include "places_panel.h"
#include "radial_menu.h"
#include "radial_menu_math.h"
#include "settings_panel.h"
#include "snapshot.h"
#include "tabletop.h"

extern void ModLog(const char* format, ...);

// controllers.cpp: the frame count, and a pulse on the hands of the mask (bit
// 0 is the hand that does not point).
uint64_t ControllersFrameCount();
void ControllersGrabPulse(int hands, float amplitude, float seconds);
// eye_capture.cpp
void EyeCaptureSaveUi(const char* name, uint64_t frame, int style,
                      const void* pixels, int pitch, uint32_t width,
                      uint32_t height);
// hint_card.cpp: hint_card_style now, which the switch "Card look" of the
// debug page changes. kDebugCannot while the hint cards are off.
int HintCardStyleValue();

namespace {

using namespace radial_math;

// --- the settings ----------------------------------------------------------------

enum Button { kButtonNone, kButtonFace, kButtonStickClick, kButtonTrigger };

bool g_read = false;
bool g_on = true;
int g_button = kButtonFace;
bool g_stick = true;
int g_reach_cm = 5;
int g_size_pct = 100;
bool g_creature_teleport = true;
bool g_test_keys = false;
int g_save = 0;
bool g_style = true;  // hint_card_style of bwvr.ini: the scroll look
bool g_places = true;    // places_panel
bool g_snapshot = true;  // snapshot_item
bool g_settings_item = true;  // settings_panel
int g_items[kPages][kSlots];
int g_pages = 1;
int g_temple_items[kPages][kSlots];
Settings g_settings = DefaultSettings();
// The test key: 0 both ways, 1 the button only, 2 the stick only.
int g_way = 0;
// The rings with all their pages: the pages of the ini, then the debug
// pages. `base` is the count of the pages of the ini, and `home` the page and
// the slot of the item "debug" (-1 for none).
struct Ring {
  debug_math::Pages pages;
  int base;
  int home;
  int home_slot;
};
Ring g_ring;
Ring g_temple_ring;
bool g_debug = false;
bool g_debug_window = false;  // debug_window, with debug_menu=1
bool g_debug_failed = false;
// The controller of the ring has face buttons. One with none (a Vive wand, a
// Windows Mixed Reality controller) keeps the ring on its stick.
bool g_face_buttons = true;

bool IniPath(char path[MAX_PATH]) {
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return false;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr) return false;
  slash[1] = 0;
  if (strlen(path) + 9 >= MAX_PATH) return false;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return true;
}

int IniKey(const char* key, int fallback) {
  char path[MAX_PATH];
  if (!IniPath(path)) return fallback;
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

void IniText(const char* key, const char* fallback, char* out, DWORD size) {
  char path[MAX_PATH];
  if (!IniPath(path)) {
    strcpy_s(out, size, fallback);
    return;
  }
  GetPrivateProfileStringA("bwvr", key, fallback, out, size, path);
  if (out[0] == 0) strcpy_s(out, size, fallback);
}

void Narrow(const wchar_t* in, char* out, size_t size) {
  size_t n = 0;
  for (; n + 1 < size && in[n] != 0; ++n) {
    out[n] = (in[n] >= 32 && in[n] < 127) ? static_cast<char>(in[n]) : '?';
  }
  out[n] = 0;
}

// The word of an item for the log: the word of the ini, the ini key of a
// switch, or the name of a group.
const char* ItemId(int item, char* buffer, size_t size) {
  if (IsSwitchItem(item)) {
    const int i = item - kItemSwitchBase;
    const char* key = DebugSwitchKey(i);
    if (key[0] != 0) return key;
    Narrow(DebugSwitchLabel(i), buffer, size);
    return buffer;
  }
  if (IsGroupItem(item)) {
    Narrow(DebugGroupName(item - kItemGroupBase), buffer, size);
    return buffer;
  }
  if (item < 0 || item >= kItems) return "none";
  return kItemInfo[item].id;
}

// The words of a page, for the log.
void PageWords(const int items[kSlots], char* out, size_t size) {
  out[0] = 0;
  for (int i = 0; i < kSlots; ++i) {
    if (i > 0) strncat_s(out, size, ", ", _TRUNCATE);
    char buffer[48];
    strncat_s(out, size, ItemId(items[i], buffer, sizeof(buffer)), _TRUNCATE);
  }
}

// The name of a page of a ring, for the log.
void PageTitle(const Ring& ring, bool temple, int page, char* out,
               size_t size) {
  if (page < ring.base) {
    if (temple) {
      strcpy_s(out, size, "The ring of the Temple");
    } else {
      sprintf_s(out, size, "Page %d", page + 1);
    }
    return;
  }
  for (int s = 0; s < kSlots; ++s) {
    const int item = ring.pages.items[page][s];
    if (IsSwitchItem(item)) {
      char group[32];
      Narrow(DebugGroupName(DebugSwitchGroup(item - kItemSwitchBase)), group,
             sizeof(group));
      sprintf_s(out, size, "The debug page %s (page %d)", group, page + 1);
      return;
    }
  }
  sprintf_s(out, size, "The debug page of the groups (page %d)", page + 1);
}

// THE ITEM "debug" in the pages of the ini of a ring. With debug_menu=0 the
// word is an empty slot. With debug_menu=1 and no page with the word, the
// last page gets it (DebugSlot).
void PlaceDebugItem(Ring* ring, const char* which) {
  ring->home = -1;
  ring->home_slot = -1;
  debug_math::Pages& p = ring->pages;
  for (int page = 0; page < p.count; ++page) {
    for (int s = 0; s < kSlots; ++s) {
      if (p.items[page][s] != kItemDebug) continue;
      if (!g_debug || ring->home >= 0) {
        p.items[page][s] = kItemNone;
      } else {
        ring->home = page;
        ring->home_slot = s;
      }
    }
  }
  if (!g_debug || ring->home >= 0 || p.count < 1) return;
  const int page = p.count - 1;
  const int slot = DebugSlot(p.items[page]);
  const int before = p.items[page][slot];
  p.items[page][slot] = kItemDebug;
  ring->home = page;
  ring->home_slot = slot;
  if (before != kItemNone) {
    ModLog("ring         : debug_menu=1: the item \"debug\" takes slot %d (0 is "
           "the top) of %s %d, in place of \"%s\"", slot, which, page + 1,
           kItemInfo[before].id);
  }
}

// With debug_window=1 the item opens the debug window (debug_window.cpp).
// So it is no link, and the ring has no debug pages. The word
// "debug_window" in a page of the ini is the word "debug".
void PlaceDebug(Ring* ring, const char* which) {
  debug_math::Pages& p = ring->pages;
  for (int page = 0; page < p.count; ++page) {
    for (int s = 0; s < kSlots; ++s) {
      if (p.items[page][s] != kItemDebugWindow) continue;
      p.items[page][s] = g_debug ? kItemDebug : kItemNone;
    }
  }
  PlaceDebugItem(ring, which);
  if (!g_debug_window || ring->home < 0) return;
  p.items[ring->home][ring->home_slot] = kItemDebugWindow;
  ring->home = -1;
  ring->home_slot = -1;
}

// THE DEBUG PAGES of a ring, after the pages of the ini. A switch that
// cannot change now is not there.
void BuildDebugPages(Ring* ring) {
  debug_math::Pages& p = ring->pages;
  p.count = ring->base;
  if (!g_debug || ring->home < 0) return;
  p.links[ring->home][ring->home_slot] = -1;
  static int group_of[kMaxSwitches];
  const int count = DebugSwitchCount();
  for (int i = 0; i < count; ++i) {
    group_of[i] = DebugSwitchValue(i) == kDebugCannot ? -1
                                                      : DebugSwitchGroup(i);
  }
  int group_page[64];
  if (!debug_math::AddDebugPages(&p, ring->home, ring->home_slot, group_of,
                                 count, DebugGroupCount(), group_page) &&
      !g_debug_failed) {
    g_debug_failed = true;
    ModLog("ring         : the debug pages do not fit in %d pages, so the item "
           "\"debug\" goes to no page", kMaxPages);
  }
}

void ReadPage(const char* key, const char* fallback, int out[kSlots]) {
  char text[256];
  IniText(key, fallback, text, sizeof(text));
  char unknown[32];
  const int bad = ParseItems(text, out, unknown, sizeof(unknown));
  if (bad > 0) {
    ModLog("ring         : %s has %d word%s that the ring does not know (the "
           "first one is \"%s\"). Each such slot is empty", key, bad,
           bad == 1 ? "" : "s", unknown);
  }
  // An item whose key is 0 is an empty slot.
  for (int i = 0; i < kSlots; ++i) {
    if ((out[i] == kItemPlaces && !g_places) ||
        (out[i] == kItemSnapshot && !g_snapshot) ||
        (out[i] == kItemSettings && !g_settings_item)) {
      ModLog("ring         : %s: the item \"%s\" is an empty slot, because "
             "%s=0", key, kItemInfo[out[i]].id,
             out[i] == kItemPlaces     ? "places_panel"
             : out[i] == kItemSnapshot ? "snapshot_item"
                                       : "settings_panel");
      out[i] = kItemNone;
    }
  }
}

// The stick can drive the ring: radial_menu_stick is 1, and the layout of the
// buttons gives the stick to the ring (button_layout=0, or a controller with
// no face buttons). The trigger cannot open the ring and choose with the
// stick too.
bool StickCanDrive() {
  return g_stick && g_button != kButtonTrigger &&
         (ControllersButtonLayout() == 0 || !g_face_buttons);
}

void ApplyWay() {
  const bool stick = StickCanDrive();
  // The test key leaves one way on only while the ring has two ways.
  const int way = stick ? g_way : 0;
  g_settings.button = g_button != kButtonNone && way != 2;
  g_settings.stick = stick && way != 1;
}

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  g_on = IniKey("radial_menu", 1) != 0;
  g_button = IniKey("radial_menu_button", kButtonFace);
  if (g_button < kButtonNone || g_button > kButtonTrigger) {
    g_button = kButtonFace;
  }
  g_stick = IniKey("radial_menu_stick", 1) != 0;
  g_reach_cm = IniKey("radial_menu_reach_cm", 5);
  if (g_reach_cm < 2) g_reach_cm = 2;
  if (g_reach_cm > 15) g_reach_cm = 15;
  g_size_pct = IniKey("radial_menu_size_pct", 100);
  if (g_size_pct < 50) g_size_pct = 50;
  if (g_size_pct > 200) g_size_pct = 200;
  g_creature_teleport = IniKey("radial_menu_creature", 1) != 0;
  g_test_keys = IniKey("test_keys", 0) != 0;
  g_save = IniKey("radial_test_save", 0);
  g_style = IniKey("hint_card_style", 1) != 0;
  g_places = PlacesPanelOn();
  g_snapshot = SnapshotOn();
  g_settings_item = SettingsPanelOn();
  g_settings.hand_in = g_reach_cm * 0.01f;
  g_settings.hand_out = g_reach_cm * 0.007f;
  ApplyWay();
  if (!g_on) {
    ModLog("ring         : radial_menu=0, the ring is off");
    return;
  }
  ReadPage("radial_menu_items", kDefaultPage1, g_items[0]);
  const char* const page2 =
      g_settings_item ? (g_places ? kDefaultPage2Settings
                                  : kDefaultPage2MarksSettings)
                      : (g_places ? kDefaultPage2 : kDefaultPage2Marks);
  ReadPage("radial_menu_items2", page2, g_items[1]);
  ReadPage("radial_menu_items_temple", kDefaultTemple, g_temple_items[0]);
  for (int i = 0; i < kSlots; ++i) g_temple_items[1][i] = kItemNone;
  g_pages = PageHasItems(g_items[1]) ? 2 : 1;
  g_debug = DebugMenuOn();
  g_debug_window = DebugWindowOn();
  for (int page = 0; page < g_pages; ++page) {
    memcpy(g_ring.pages.items[page], g_items[page], sizeof(g_items[page]));
  }
  g_ring.pages.count = g_pages;
  memcpy(g_temple_ring.pages.items[0], g_temple_items[0],
         sizeof(g_temple_items[0]));
  g_temple_ring.pages.count = 1;
  PlaceDebug(&g_ring, "page");
  PlaceDebug(&g_temple_ring, "the ring of the Temple, page");
  debug_math::IniLinks(&g_ring.pages);
  debug_math::IniLinks(&g_temple_ring.pages);
  g_ring.base = g_ring.pages.count;
  g_temple_ring.base = g_temple_ring.pages.count;
  const bool new_layout = ControllersButtonLayout() != 0;
  const char* const kButtonWords[4] = {
      "no button opens it",
      new_layout ? "the second face button of the hand that does not point "
                   "opens it (Y, button_layout=1)"
                 : "the first face button of the hand that does not point "
                   "opens it (X, button_layout=0)",
      "the stick click of that hand opens it",
      "the trigger of that hand opens it"};
  char a[200], b[200], c[200];
  PageWords(g_ring.pages.items[0], a, sizeof(a));
  PageWords(g_ring.pages.items[g_pages > 1 ? 1 : 0], b, sizeof(b));
  PageWords(g_temple_ring.pages.items[0], c, sizeof(c));
  ModLog("ring         : radial_menu=1. radial_menu_button=%d: %s, the hand "
         "picks an item after %d cm, and the release chooses. "
         "radial_menu_stick=%d: %s. The size is %d percent",
         g_button, kButtonWords[g_button], g_reach_cm, g_stick ? 1 : 0,
         g_settings.stick ? "the stick of that hand opens the ring and points, "
                            "and its trigger chooses"
         : g_stick && new_layout
             ? "the stick does not open the ring with button_layout=1, but "
               "on a controller with no face buttons it does"
             : "the stick does not open the ring",
         g_size_pct);
  ModLog("ring         : hint_card_style=%d: %s", g_style ? 1 : 0,
         g_style ? "each item is a wax seal, and the card in the middle is a "
                   "parchment scroll"
                 : "each item is a dark round box with white letters");
  ModLog("ring         : places_panel=%d: %s. snapshot_item=%d: %s",
         g_places ? 1 : 0,
         g_places ? "the item \"places\" opens the panel of places and "
                    "bookmarks"
                  : "page 2 as made has the bookmark items of the ring",
         g_snapshot ? 1 : 0,
         g_snapshot ? "the item \"snapshot\" saves the eye pictures and the "
                      "log for a report"
                    : "no snapshot item");
  ModLog("ring         : page 1: %s", a);
  if (g_pages > 1) ModLog("ring         : page 2: %s", b);
  ModLog("ring         : in the Temple: %s", c);
  DebugMenuLogStart();
}

// --- the game --------------------------------------------------------------------

const uintptr_t kGame = 0x00D0195C;
// GGame.
const int kGameNoKeys = 0x0A;          // bit 0: the game takes no keys now
const int kGameFlags = 0x14;           // bit 2 pause, 0x20000 the names
const int kGamePlayers = 0x18;
const int kGamePlayerSize = 0xA60;
const int kGameTemple = 0x59B4;        // the Temple object
const int kGameInTemple = 0x205A28;    // 1 while the player is in the Temple
const int kGameLocalPlayer = 0x205A59;
const int kGameSpeed = 0x205B74;       // a float, 0.5 to 2
const int kGameHelp = 0x25005C;        // the HelpSystem
const int kGameKeyBuffer = 0x2502A8;   // the GKeyBuffer
const int kGameControlMap = 0x250300;  // the ControlMap
const int kPlayerCreature = 0xA4C;
// HelpSystem: both are not 0 while a cut scene plays, and GGame::DoAction
// then does nothing.
const int kHelpCinemaA = 0x45E8;
const int kHelpCinemaB = 0x45EC;
// ControlMap: one record of 0x30C bytes for each action.
const int kMapRecord = 0x30C;
const int kMapKey = 0x100;
const int kMapKeyMod = 0x184;
const int kMapMoves = 0x652C;          // 0: the tutorial has the moves off
const int kMapJumps = 0x6530;          // 0: and the keys of the Temple
// GKeyBuffer: the size and the count, as words.
const int kBufferSize = 0x0C;
const int kBufferCount = 0x0E;
// GKeyBuffer::AddKey, thiscall (key, modifiers), ret 8.
const uintptr_t kAddKey = 0x005E1BB0;
// LHKeyboard::CriticalSection: GGame::ProcessGameInputs holds it while it
// reads the buffer.
const uintptr_t kKeyboardLock = 0x00E8C118;
// GGame::MyInterface, thiscall, no arguments.
const uintptr_t kMyInterface = 0x00555850;
const int kFaceStatus = 0x39C;         // the local GInterfaceStatus
const int kStatusLeash = 0x12C;        // its GLeashStatus
const int kLeashOn = 0x14;
const int kLeashKind = 0x1C;           // 1 to 3
const int kCreatureLeashes = 0x1108;   // + 4 * kind: 1 when it has that kind
// A GameThingWithPos: its map point, in 1/6553.6 of a unit.
const int kThingX = 0x14;
const int kThingZ = 0x18;
const float kMapUnit = 0.00015258789f;
// The bookmarks: a record of 0x24 bytes for each, with bit 0 of the byte at
// +4 set while it is set. The keys act only while kBookmarksOn is not 0.
const uintptr_t kBookmarks = 0x00C58B30;
const int kBookmarkSize = 0x24;
const uintptr_t kBookmarksOn = 0x009CD384;
// The Temple object: its 7 rooms, and the room now.
const int kTempleRoomNow = 0x1C;
const char* const kRoomName[7] = {
    "the hall", "the Creature Room", "the Challenge Room",
    "the room of the multiplayer game", "the Options Room",
    "the Save Game Room", "the Library"};

typedef void*(__fastcall* MyInterfaceFn)(void* self, void* unused);
typedef int(__fastcall* AddKeyFn)(void* self, void* unused, int key,
                                  int modifiers);

struct Facts {
  uint8_t* game;
  bool temple;
  int room;            // 0 to 6, or -1
  bool paused;
  float speed;
  bool names;
  bool details;
  bool cutscene;
  bool keys_off;
  bool moves_on;
  bool jumps_on;
  void* creature;
  bool leash_known;
  bool leash_on;
  int leash_kind;
  bool leash_has[4];
  bool marks_on;
  bool mark[kMarks];
};

bool ReadFacts(Facts* f) {
  memset(f, 0, sizeof(*f));
  f->room = -1;
  void* const* slot = reinterpret_cast<void* const*>(kGame);
  if (IsBadReadPtr(slot, sizeof(void*)) || *slot == nullptr) return false;
  uint8_t* game = static_cast<uint8_t*>(*slot);
  if (IsBadReadPtr(game + kGameControlMap, 8)) return false;
  f->game = game;
  f->keys_off = (game[kGameNoKeys] & 1) != 0;
  const uint32_t flags = *reinterpret_cast<const uint32_t*>(game + kGameFlags);
  f->paused = (flags & 4) != 0;
  f->names = (flags & 0x20000) != 0;
  f->details = (flags & 0x200000) != 0;
  f->speed = *reinterpret_cast<const float*>(game + kGameSpeed);
  f->temple = *reinterpret_cast<const int32_t*>(game + kGameInTemple) == 1;
  const uint8_t* temple =
      *reinterpret_cast<uint8_t* const*>(game + kGameTemple);
  if (f->temple && temple != nullptr &&
      !IsBadReadPtr(temple, kTempleRoomNow + 4)) {
    const void* now = *reinterpret_cast<void* const*>(temple + kTempleRoomNow);
    for (int i = 0; i < 7; ++i) {
      if (now != nullptr &&
          *reinterpret_cast<void* const*>(temple + 4 * i) == now) {
        f->room = i;
      }
    }
  }
  const uint8_t* help = *reinterpret_cast<uint8_t* const*>(game + kGameHelp);
  if (help != nullptr && !IsBadReadPtr(help + kHelpCinemaA, 8)) {
    f->cutscene =
        *reinterpret_cast<const int32_t*>(help + kHelpCinemaA) != 0 &&
        *reinterpret_cast<const int32_t*>(help + kHelpCinemaB) != 0;
  }
  const uint8_t* map =
      *reinterpret_cast<uint8_t* const*>(game + kGameControlMap);
  f->moves_on = f->jumps_on = true;
  if (map != nullptr && !IsBadReadPtr(map + kMapMoves, 8)) {
    f->moves_on = *reinterpret_cast<const int32_t*>(map + kMapMoves) != 0;
    f->jumps_on = *reinterpret_cast<const int32_t*>(map + kMapJumps) != 0;
  }
  const uint8_t* player =
      game + kGamePlayers + kGamePlayerSize * game[kGameLocalPlayer];
  f->creature = *reinterpret_cast<void* const*>(player + kPlayerCreature);
  if (f->creature != nullptr &&
      IsBadReadPtr(f->creature, kCreatureLeashes + 16)) {
    f->creature = nullptr;
  }
  if (f->creature != nullptr) {
    const uint8_t* creature = static_cast<const uint8_t*>(f->creature);
    for (int kind = 1; kind <= 3; ++kind) {
      f->leash_has[kind] = *reinterpret_cast<const int32_t*>(
                               creature + kCreatureLeashes + 4 * kind) == 1;
    }
  }
  uint8_t* face = static_cast<uint8_t*>(
      reinterpret_cast<MyInterfaceFn>(kMyInterface)(game, nullptr));
  if (face != nullptr && !IsBadReadPtr(face + kFaceStatus, 4)) {
    const uint8_t* status =
        *reinterpret_cast<uint8_t* const*>(face + kFaceStatus);
    if (status != nullptr && !IsBadReadPtr(status + kStatusLeash, 4)) {
      const uint8_t* leash =
          *reinterpret_cast<uint8_t* const*>(status + kStatusLeash);
      if (leash != nullptr && !IsBadReadPtr(leash, kLeashKind + 4)) {
        f->leash_known = true;
        f->leash_on = *reinterpret_cast<const int32_t*>(leash + kLeashOn) != 0;
        f->leash_kind = *reinterpret_cast<const int32_t*>(leash + kLeashKind);
      }
    }
  }
  f->marks_on = *reinterpret_cast<const int32_t*>(kBookmarksOn) != 0;
  for (int i = 0; i < kMarks; ++i) {
    f->mark[i] = (*reinterpret_cast<const uint8_t*>(
                      kBookmarks + kBookmarkSize * i + 4) & 1) != 0;
  }
  return true;
}

const char* KeyName(int key) {
  switch (key) {
    case 0x01: return "Esc";
    case 0x0E: return "Backspace";
    case 0x0F: return "Tab";
    case 0x19: return "P";
    case 0x39: return "Space";
    case 0x3C: return "F2";
    default: break;
  }
  static char text[16];
  if (key >= 0x02 && key <= 0x0A) {
    sprintf_s(text, "%d", key - 1);
  } else if (key >= 0x3B && key <= 0x44) {
    sprintf_s(text, "F%d", key - 0x3A);
  } else {
    sprintf_s(text, "scan code 0x%02X", key);
  }
  return text;
}

const char* ModsName(int mods) {
  if (mods == kModCtrl) return "Ctrl and ";
  if (mods == kModAlt) return "Alt and ";
  if (mods == kModShift) return "Shift and ";
  if (mods == (kModCtrl | kModShift)) return "Ctrl, Shift and ";
  return "";
}

// --- the log ---------------------------------------------------------------------

int g_logs = 0;
const int kMaxLogs = 400;
bool LogRoom() {
  if (g_logs >= kMaxLogs) return false;
  ++g_logs;
  return true;
}

uint64_t g_opens = 0, g_choices = 0, g_refusals = 0, g_keys_sent = 0;
uint64_t g_blocked = 0, g_no_choice = 0;
// The cost of the picture, for the report: how often it was drawn, and the
// longest draw and the longest fade step in milliseconds.
uint64_t g_rasters = 0;
double g_raster_ms = 0.0;
double g_stage_ms = 0.0;

double NowSeconds() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart) /
         static_cast<double>(frequency.QuadPart);
}

// One line for each change of what the ring can change in the game. A log
// from a headset then says what a choice did.
bool g_have_watch = false;
Facts g_watch;

void Watch(const Facts& f) {
  const uint64_t frame = ControllersFrameCount();
  if (!g_have_watch) {
    g_have_watch = true;
    g_watch = f;
    return;
  }
  const Facts& w = g_watch;
  if (f.temple != w.temple || f.room != w.room) {
    if (f.temple) {
      ModLog("ring         : the game: the player is in the Temple, in %s, "
             "frame %llu", f.room >= 0 ? kRoomName[f.room] : "no room yet",
             frame);
    } else {
      ModLog("ring         : the game: the player is out of the Temple, frame "
             "%llu", frame);
    }
  }
  if (f.paused != w.paused) {
    ModLog("ring         : the game: the pause is %s, frame %llu",
           f.paused ? "on" : "off", frame);
  }
  if (f.speed != w.speed) {
    ModLog("ring         : the game: the speed is %.2f (it was %.2f), frame "
           "%llu", f.speed, w.speed, frame);
  }
  if (f.names != w.names) {
    ModLog("ring         : the game: the villager names are %s, frame %llu",
           f.names ? "on" : "off", frame);
  }
  if (f.details != w.details) {
    ModLog("ring         : the game: the villager details are %s, frame %llu",
           f.details ? "on" : "off", frame);
  }
  if (f.leash_known && w.leash_known &&
      (f.leash_kind != w.leash_kind || f.leash_on != w.leash_on)) {
    ModLog("ring         : the game: the leash is kind %d and %s (it was kind "
           "%d and %s), frame %llu", f.leash_kind, f.leash_on ? "on" : "off",
           w.leash_kind, w.leash_on ? "on" : "off", frame);
  }
  for (int i = 0; i < kMarks; ++i) {
    if (f.mark[i] != w.mark[i]) {
      ModLog("ring         : the game: bookmark %d is %s, frame %llu", i + 1,
             f.mark[i] ? "set" : "not set", frame);
    }
  }
  if (f.cutscene != w.cutscene && LogRoom()) {
    ModLog("ring         : the game: a cut scene %s, frame %llu",
           f.cutscene ? "starts" : "ends", frame);
  }
  g_watch = f;
}

// --- the keys --------------------------------------------------------------------

struct KeyToSend {
  int key;
  int mods;
};
const int kMaxKeys = 8;
KeyToSend g_queue[kMaxKeys];
int g_queued = 0;

void QueueKey(int key, int mods) {
  if (g_queued >= kMaxKeys) return;
  g_queue[g_queued].key = key;
  g_queue[g_queued].mods = mods;
  ++g_queued;
}

// One key into the key buffer of the game. The game takes it from there at
// its next GGame::ProcessBufferedKeys, as it takes a key of the keyboard.
void SendKey(int key, int mods) {
  const uint64_t frame = ControllersFrameCount();
  void* const* slot = reinterpret_cast<void* const*>(kGame);
  if (IsBadReadPtr(slot, sizeof(void*)) || *slot == nullptr) {
    ModLog("ring         : the key %s%s is dropped, there is no game, frame "
           "%llu", ModsName(mods), KeyName(key), frame);
    return;
  }
  uint8_t* game = static_cast<uint8_t*>(*slot);
  if ((game[kGameNoKeys] & 1) != 0) {
    ModLog("ring         : the key %s%s is dropped, the game takes no keys "
           "now, frame %llu", ModsName(mods), KeyName(key), frame);
    return;
  }
  uint8_t* buffer = game + kGameKeyBuffer;
  CRITICAL_SECTION* lock = reinterpret_cast<CRITICAL_SECTION*>(kKeyboardLock);
  EnterCriticalSection(lock);
  const int added =
      reinterpret_cast<AddKeyFn>(kAddKey)(buffer, nullptr, key, mods);
  const int count = *reinterpret_cast<const uint16_t*>(buffer + kBufferCount);
  const int size = *reinterpret_cast<const uint16_t*>(buffer + kBufferSize);
  LeaveCriticalSection(lock);
  if (added != 0) ++g_keys_sent;
  ModLog("ring         : the key %s%s %s the key buffer of the game (%d of %d "
         "keys), frame %llu", ModsName(mods), KeyName(key),
         added != 0 ? "goes into" : "does not fit in", count, size, frame);
}

// The key that the player gave an action in the Options Room. False when the
// action has no key.
bool ActionKey(const Facts& f, int action, int* key, int* mods) {
  const uint8_t* map =
      *reinterpret_cast<uint8_t* const*>(f.game + kGameControlMap);
  if (map == nullptr || action < 0 || action > 0x20) return false;
  const uint8_t* record = map + kMapRecord * action;
  if (IsBadReadPtr(record + kMapKeyMod, 4)) return false;
  *key = *reinterpret_cast<const int32_t*>(record + kMapKey);
  *mods = ModsOfKey(*reinterpret_cast<const int32_t*>(record + kMapKeyMod));
  return *key != 0;
}

// --- an item now -------------------------------------------------------------------

// What the card says under the name of an item, and whether the item can act
// now. An item that cannot act is dim on the ring.
struct ItemNow {
  bool can;
  const wchar_t* name;
  const wchar_t* note;   // empty for none
  const char* why;       // for the log, when it cannot act
  wchar_t own[48];       // the note of a switch, when own_note
  bool own_note;
};

const wchar_t* NoteOf(const ItemNow& now) {
  return now.own_note ? now.own : now.note;
}

// An item of the debug page: a switch says its ini key and its value now.
ItemNow DebugStateOf(int item) {
  ItemNow now = {true, L"", L"", "", {0}, false};
  if (IsGroupItem(item)) {
    now.name = DebugGroupName(item - kItemGroupBase);
    return now;
  }
  const int i = item - kItemSwitchBase;
  now.name = DebugSwitchName(i);
  const int value = DebugSwitchValue(i);
  if (value == kDebugCannot) {
    now.can = false;
    now.note = L"It cannot change now";
    now.why = "it cannot change now";
    return now;
  }
  wchar_t word[32];
  DebugSwitchValueText(i, value, word, 32);
  const char* key = DebugSwitchKey(i);
  if (key[0] != 0) {
    swprintf_s(now.own, L"%hs=%d (%ls)", key, value, word);
  } else {
    swprintf_s(now.own, L"It is %ls now", word);
  }
  now.own_note = true;
  return now;
}

int g_last_mark = -1;

ItemNow StateOf(int item, const Facts& f) {
  if (IsSwitchItem(item) || IsGroupItem(item)) return DebugStateOf(item);
  ItemNow now = {true, kItemInfo[item].name, L"", "", {0}, false};
  const ItemInfo& info = kItemInfo[item];
  const auto no = [&now](const wchar_t* note, const char* why) {
    now.can = false;
    now.note = note;
    now.why = why;
  };
  if (item == kItemNone) {
    now.can = false;
    return now;
  }
  if (item == kItemMore || item == kItemDebug || item == kItemBack ||
      item == kItemDebugWindow || item == kItemSettings) {
    return now;
  }
  // The snapshot needs no game: it saves what the headset shows.
  if (item == kItemSnapshot) {
    if (SnapshotBusy()) no(L"A snapshot saves now", "a snapshot saves now");
    return now;
  }
  // The recenter is no key of the game, so each gate of the game keys lets
  // it act.
  if (info.how == kHowRecenter) return now;
  if (f.game == nullptr) {
    no(L"No game runs now", "no game runs");
    return now;
  }
  if (f.keys_off) {
    no(L"The game takes no keys now", "the game takes no keys now");
    return now;
  }
  if (f.cutscene) {
    no(L"Not in a cut scene", "a cut scene plays, and the game does no "
                              "action then");
    return now;
  }
  if (info.how == kHowAction) {
    if (ActionIsOff(info.code, f.moves_on, f.jumps_on)) {
      no(L"The tutorial has this off", "the tutorial has this control off");
      return now;
    }
    int key = 0, mods = 0;
    if (!ActionKey(f, info.code, &key, &mods)) {
      no(L"No key in the game options",
         "the action has no key in the control map of the game");
      return now;
    }
  }
  switch (item) {
    case kItemCreature:
      if (f.creature == nullptr) {
        no(L"You have no Creature", "the player has no Creature");
      }
      break;
    case kItemLeashAggression:
    case kItemLeashLearning:
    case kItemLeashCompassion:
    case kItemLeashNext:
    case kItemLeashPrevious:
      if (f.creature == nullptr) {
        no(L"You have no Creature", "the player has no Creature");
      } else if (info.how == kHowLeash) {
        if (!f.leash_has[info.code]) {
          no(L"Your Creature does not have it",
             "the Creature does not have this kind of leash");
        } else if (f.leash_known && f.leash_kind == info.code) {
          now.note = f.leash_on ? L"It is on now" : L"It is the leash now";
        }
      }
      break;
    case kItemMarkSet:
    case kItemMarkGo:
    case kItemMarkBack:
    case kItemTown:
      if (!f.marks_on || !f.moves_on || !f.jumps_on) {
        no(L"The game has this off now", "the game has the bookmarks off now");
      } else if (item == kItemMarkGo) {
        bool any = false;
        for (int i = 0; i < kMarks; ++i) any = any || f.mark[i];
        if (!any) no(L"No bookmark is set", "no bookmark is set");
      }
      break;
    case kItemPlaces:
      if (f.temple) {
        no(L"Not in the Temple", "the player is in the Temple, where the "
                                 "bookmarks do nothing");
      }
      break;
    case kItemPause:
      if (f.paused) now.name = L"Go on with the game";
      break;
    case kItemSlower:
      if (f.speed <= 0.5f) no(L"The game is at its slowest",
                              "the speed is 0.5, the least");
      break;
    case kItemFaster:
      if (f.speed >= 2.0f) no(L"The game is at its fastest",
                              "the speed is 2.0, the most");
      break;
    case kItemNames:
      now.note = f.names ? L"They show now" : L"";
      break;
    case kItemDetails:
      now.note = f.details ? L"They show now" : L"";
      break;
    case kItemLeave:
      if (f.temple && f.room > 0) now.name = L"Back to the hall";
      break;
    case kItemHall:
    case kItemRoomCreature:
    case kItemRoomChallenge:
    case kItemRoomSave:
    case kItemRoomOptions:
    case kItemRoomLibrary: {
      // The room of each item (TempleRoomsEnum).
      const int room = item == kItemHall            ? 0
                       : item == kItemRoomCreature  ? 1
                       : item == kItemRoomChallenge ? 2
                       : item == kItemRoomSave      ? 5
                       : item == kItemRoomOptions   ? 4
                                                    : 6;
      if (f.temple && f.room == room) {
        no(L"You are here", "the player is in this room");
      }
      break;
    }
    default:
      break;
  }
  return now;
}

// The player chose an item. True when its keys are queued.
bool Choose(int item, const Facts& f, const ItemNow& now) {
  const uint64_t frame = ControllersFrameCount();
  if (IsSwitchItem(item)) {
    if (DebugSwitchChoose(item - kItemSwitchBase)) {
      ++g_choices;
      return true;
    }
    ++g_refusals;
    return false;
  }
  const ItemInfo& info = kItemInfo[item];
  if (!now.can) {
    ++g_refusals;
    ModLog("ring         : the player chose \"%s\", and it does not act: %s, "
           "frame %llu", info.id, now.why, frame);
    return false;
  }
  int key = 0, mods = 0;
  switch (info.how) {
    case kHowAction:
      // THE CREATURE IN THE GOD VIEW. The key of the game starts its follow
      // camera, and the God view stands aside for that camera. One teleport
      // to the Creature keeps the God view, and nothing moves after it.
      if (item == kItemCreature && g_creature_teleport &&
          TabletopGodOwnsView() && f.creature != nullptr) {
        const uint8_t* creature = static_cast<const uint8_t*>(f.creature);
        const float target[3] = {
            *reinterpret_cast<const int32_t*>(creature + kThingX) * kMapUnit,
            0.0f,
            *reinterpret_cast<const int32_t*>(creature + kThingZ) * kMapUnit};
        if (TabletopGodTeleport(target)) {
          ++g_choices;
          ModLog("ring         : the player chose \"creature\": one teleport "
                 "of the God view to the Creature at %.1f %.1f "
                 "(radial_menu_creature=1), frame %llu", target[0], target[2],
                 frame);
          return true;
        }
      }
      ActionKey(f, info.code, &key, &mods);
      break;
    case kHowKey:
      key = info.code;
      mods = info.mods;
      break;
    case kHowLeash: {
      const int action = LeashAction(f.leash_known ? f.leash_kind : 0,
                                     info.code);
      if (action < 0) {
        ++g_choices;
        ModLog("ring         : the player chose \"%s\": the leash has this "
               "kind already (kind %d), so no key goes out, frame %llu",
               info.id, f.leash_kind, frame);
        return true;
      }
      if (!ActionKey(f, action, &key, &mods)) {
        ++g_refusals;
        ModLog("ring         : the player chose \"%s\", and it does not act: "
               "action 0x%02X has no key in the control map of the game, "
               "frame %llu", info.id, action, frame);
        return false;
      }
      break;
    }
    case kHowRecenter:
      ++g_choices;
      ControllersRecenter("the item \"recenter\" of the ring");
      return true;
    case kHowMarkSet: {
      const int index = MarkToSet(f.mark, g_last_mark);
      g_last_mark = index;
      key = 2 + index;
      mods = kModCtrl;
      break;
    }
    case kHowPanel:
      if (!PlacesPanelOpenFromRing()) {
        ++g_refusals;
        ModLog("ring         : the player chose \"places\", and the panel "
               "does not open: the head has no pose yet, frame %llu", frame);
        return false;
      }
      ++g_choices;
      ModLog("ring         : the player chose \"places\": the panel of places "
             "opens, frame %llu", frame);
      return true;
    case kHowWindow:
      if (!DebugWindowOpenFromRing()) {
        ++g_refusals;
        ModLog("ring         : the player chose \"debug\", and the debug "
               "window does not open: the head has no pose yet, frame %llu",
               frame);
        return false;
      }
      ++g_choices;
      ModLog("ring         : the player chose \"debug\": the debug window "
             "opens, frame %llu", frame);
      return true;
    case kHowSettings:
      if (!SettingsPanelOpenFromRing()) {
        ++g_refusals;
        ModLog("ring         : the player chose \"settings\", and the "
               "settings window does not open: the head has no pose yet, "
               "frame %llu", frame);
        return false;
      }
      ++g_choices;
      ModLog("ring         : the player chose \"settings\": the settings "
             "window opens, frame %llu", frame);
      return true;
    case kHowSnapshot:
      if (!SnapshotStart(frame)) {
        ++g_refusals;
        return false;
      }
      ++g_choices;
      ModLog("ring         : the player chose \"snapshot\", frame %llu", frame);
      return true;
    default:
      return false;
  }
  ++g_choices;
  const int presses = info.presses > 1 ? info.presses : 1;
  for (int i = 0; i < presses; ++i) QueueKey(key, mods);
  ModLog("ring         : the player chose \"%s\": the key %s%s, %d time%s, "
         "frame %llu", info.id, ModsName(mods), KeyName(key), presses,
         presses == 1 ? "" : "s", frame);
  return true;
}

// --- the picture -----------------------------------------------------------------

// 32 pixels are 1 cm at 100 percent, so the ring is 32 cm across, the items
// are 11 cm from its middle, and a capital letter is about 8 mm high.
const int kSize = 1024;
const float kPixelsPerMeter = 3200.0f;
const float kMiddle = 512.0f;
const float kSlotRadius = 352.0f;
const int kBadgeHeight = 64;
const int kBadgePad = 22;
const int kBadgeLeast = 150;
const float kBadgeRadius = 22.0f;
const int kBadgeEm = 34;
// A badge of a switch has two lines: its word, and its value under it.
const int kSwitchHeight = 92;
const int kSwitchEm = 28;
const int kValueEm = 26;
// A badge left or right of the middle ends at the edge of the picture.
const int kBadgeMost = 320;
const int kNameEm = 38;
const int kNoteEm = 28;
const int kCardPad = 26;
const int kCardMost = 470;
const int kCardWide = 500;
const float kCardRadius = 24.0f;
const float kFadeIn = 0.10f;
const float kFadeOut = 0.20f;
// A choice that the game does not take shows its reason for this long.
const double kNoticeSeconds = 1.8;

struct Rgb {
  float r, g, b;
};
const Rgb kCardColor = {18.0f, 22.0f, 30.0f};
const Rgb kWhite = {255.0f, 255.0f, 255.0f};
const Rgb kLit = {236.0f, 240.0f, 246.0f};
const Rgb kLitInk = {16.0f, 18.0f, 22.0f};
const Rgb kDimInk = {150.0f, 156.0f, 166.0f};
const Rgb kDimLit = {120.0f, 126.0f, 136.0f};
const Rgb kNoteInk = {255.0f, 196.0f, 60.0f};

// What the picture shows. The picture is drawn again only when this changes.
struct Look {
  int item[kSlots];
  int value[kSlots];       // the value now of a switch, else 0
  bool dim[kSlots];
  int slot;                // the choice, or -1
  wchar_t name[48];        // the card in the middle: empty for none
  wchar_t note[48];
  int scroll;              // 1 the scroll look (hint_card_style), 0 plain
};

HDC g_dc = nullptr;
HBITMAP g_dib = nullptr;
uint32_t* g_bits = nullptr;
HFONT g_badge_font = nullptr;
HFONT g_switch_font = nullptr;
HFONT g_value_font = nullptr;
HFONT g_name_font = nullptr;
HFONT g_note_font = nullptr;
bool g_raster_failed = false;
uint8_t g_base[kSize * kSize * 4];
uint8_t g_staged[kSize * kSize * 4];
Look g_drawn;
bool g_have_drawn = false;
float g_staged_fade = -1.0f;
int g_saved = 0;

bool RasterStart() {
  if (g_dc != nullptr) return true;
  if (g_raster_failed) return false;
  g_dc = CreateCompatibleDC(nullptr);
  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = kSize;
  info.bmiHeader.biHeight = -kSize;  // top first
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  if (g_dc != nullptr) {
    g_dib = CreateDIBSection(g_dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  }
  const auto font = [](int em, int weight) {
    return CreateFontW(-em, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
                       L"Segoe UI");
  };
  g_badge_font = font(kBadgeEm, FW_BOLD);
  g_switch_font = font(kSwitchEm, FW_BOLD);
  g_value_font = font(kValueEm, FW_SEMIBOLD);
  g_name_font = font(kNameEm, FW_SEMIBOLD);
  g_note_font = font(kNoteEm, FW_SEMIBOLD);
  if (g_dc == nullptr || g_dib == nullptr || bits == nullptr ||
      g_badge_font == nullptr || g_switch_font == nullptr ||
      g_value_font == nullptr || g_name_font == nullptr ||
      g_note_font == nullptr) {
    g_raster_failed = true;
    ModLog("ring         : GDI gave no picture for the ring, so the ring does "
           "not show. Its items still act");
    return false;
  }
  g_bits = static_cast<uint32_t*>(bits);
  SelectObject(g_dc, g_dib);
  SetBkMode(g_dc, TRANSPARENT);
  SetTextColor(g_dc, RGB(255, 255, 255));
  return true;
}

int TextWidth(HFONT font, const wchar_t* text) {
  SelectObject(g_dc, font);
  SIZE size = {0, 0};
  GetTextExtentPoint32W(g_dc, text, static_cast<int>(wcslen(text)), &size);
  return size.cx;
}

// How much of the pixel at px, py is inside a rect with round corners.
float RoundCover(float px, float py, float x0, float y0, float x1, float y1,
                 float radius) {
  const float cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
  const float qx = fabsf(px - cx) - (0.5f * (x1 - x0) - radius);
  const float qy = fabsf(py - cy) - (0.5f * (y1 - y0) - radius);
  const float ox = qx > 0.0f ? qx : 0.0f;
  const float oy = qy > 0.0f ? qy : 0.0f;
  const float inside = (qx > qy ? qx : qy) < 0.0f ? (qx > qy ? qx : qy) : 0.0f;
  const float d = sqrtf(ox * ox + oy * oy) + inside - radius;
  const float cover = 0.5f - d;
  return cover < 0.0f ? 0.0f : (cover > 1.0f ? 1.0f : cover);
}

// One round box of the picture: its fill, and the letters that GDI drew in
// it. Above split the letters have the color ink, and from split on ink2.
void Box(int x0, int y0, int x1, int y1, float radius, const Rgb& fill,
         float alpha, const Rgb& ink, int split, const Rgb& ink2) {
  const int left = x0 < 0 ? 0 : x0;
  const int top = y0 < 0 ? 0 : y0;
  const int right = x1 > kSize ? kSize : x1;
  const int bottom = y1 > kSize ? kSize : y1;
  for (int y = top; y < bottom; ++y) {
    const float py = static_cast<float>(y) + 0.5f;
    uint8_t* out = g_base + (static_cast<size_t>(y) * kSize + left) * 4;
    const uint32_t* letters = g_bits + static_cast<size_t>(y) * kSize;
    const Rgb& letter_ink = y >= split ? ink2 : ink;
    for (int x = left; x < right; ++x, out += 4) {
      const float cover = RoundCover(
          static_cast<float>(x) + 0.5f, py, static_cast<float>(x0),
          static_cast<float>(y0), static_cast<float>(x1),
          static_cast<float>(y1), radius);
      if (cover <= 0.0f) continue;
      Rgb color = fill;
      float a = alpha;
      const float letter =
          static_cast<float>((letters[x] >> 8) & 0xFF) * (1.0f / 255.0f);
      if (letter > 0.0f) {
        color.r += (letter_ink.r - color.r) * letter;
        color.g += (letter_ink.g - color.g) * letter;
        color.b += (letter_ink.b - color.b) * letter;
        a += (1.0f - a) * letter;
      }
      a *= cover;
      out[0] = static_cast<uint8_t>(color.b * a + 0.5f);
      out[1] = static_cast<uint8_t>(color.g * a + 0.5f);
      out[2] = static_cast<uint8_t>(color.r * a + 0.5f);
      out[3] = static_cast<uint8_t>(255.0f * a + 0.5f);
    }
  }
}

bool SameLook(const Look& a, const Look& b) {
  return memcmp(&a, &b, sizeof(Look)) == 0;
}

// The plain look (hint_card_style=0): dark round boxes with white letters.
void RasterPlain(const Look& look) {
  memset(g_bits, 0, static_cast<size_t>(kSize) * kSize * 4);
  memset(g_base, 0, sizeof(g_base));
  // The letters first, white on black. The green of a pixel is its cover.
  RECT badge[kSlots];
  int badge_split[kSlots];
  for (int i = 0; i < kSlots; ++i) {
    SetRectEmpty(&badge[i]);
    badge_split[i] = kSize;
    const int item = look.item[i];
    const bool two = IsSwitchItem(item);
    const wchar_t* word = L"";
    wchar_t value[32] = L"";
    if (two) {
      word = DebugSwitchLabel(item - kItemSwitchBase);
      DebugSwitchValueText(item - kItemSwitchBase, look.value[i], value, 32);
    } else if (IsGroupItem(item)) {
      word = DebugGroupName(item - kItemGroupBase);
    } else if (item > kItemNone && item < kItems) {
      word = kItemInfo[item].word;
    }
    if (word[0] == 0) continue;
    int w = TextWidth(two ? g_switch_font : g_badge_font, word);
    if (two) {
      const int value_w = TextWidth(g_value_font, value);
      if (value_w > w) w = value_w;
    }
    w += 2 * kBadgePad;
    if (w < kBadgeLeast) w = kBadgeLeast;
    if (w > kBadgeMost) w = kBadgeMost;
    const int h = two ? kSwitchHeight : kBadgeHeight;
    float cx = 0.0f, cy = 0.0f;
    SlotPixel(i, kMiddle, kSlotRadius, &cx, &cy);
    badge[i].left = static_cast<LONG>(cx) - w / 2;
    badge[i].right = badge[i].left + w;
    badge[i].top = static_cast<LONG>(cy) - h / 2;
    badge[i].bottom = badge[i].top + h;
    const UINT flags = DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_VCENTER |
                       DT_END_ELLIPSIS;
    RECT r = badge[i];
    r.left += kBadgePad / 2;
    r.right -= kBadgePad / 2;
    if (two) {
      r.top += 6;
      r.bottom = r.top + 44;
      badge_split[i] = r.bottom;
      SelectObject(g_dc, g_switch_font);
      DrawTextW(g_dc, word, -1, &r, flags);
      RECT v = r;
      v.top = badge_split[i] - 4;
      v.bottom = badge[i].bottom - 8;
      SelectObject(g_dc, g_value_font);
      DrawTextW(g_dc, value, -1, &v, flags);
    } else {
      SelectObject(g_dc, g_badge_font);
      DrawTextW(g_dc, word, -1, &r, flags);
    }
  }
  RECT card;
  SetRectEmpty(&card);
  int split = kSize;
  if (look.name[0] != 0) {
    const bool two = look.note[0] != 0;
    // A long name (a switch of the debug page) takes the font of the value
    // of a badge, and the card can be wider. It still ends left of the
    // badges at the left and the right.
    HFONT name_font = g_name_font;
    int most = kCardMost;
    if (TextWidth(name_font, look.name) + 2 * kCardPad > kCardMost) {
      name_font = g_value_font;
      most = kCardWide;
    }
    int w = TextWidth(name_font, look.name);
    if (two) {
      const int note_w = TextWidth(g_note_font, look.note);
      if (note_w > w) w = note_w;
    }
    w += 2 * kCardPad;
    if (w > most) w = most;
    const int h = two ? 124 : 80;
    card.left = static_cast<LONG>(kMiddle) - w / 2;
    card.right = card.left + w;
    card.top = static_cast<LONG>(kMiddle) - h / 2;
    card.bottom = card.top + h;
    RECT name = card;
    name.left += kCardPad - 8;
    name.right -= kCardPad - 8;
    if (two) {
      name.top += 14;
      name.bottom = name.top + 50;
      split = name.bottom;
    }
    SelectObject(g_dc, name_font);
    DrawTextW(g_dc, look.name, -1, &name,
              DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_VCENTER |
                  DT_END_ELLIPSIS);
    if (two) {
      RECT note = card;
      note.left += kCardPad - 8;
      note.right -= kCardPad - 8;
      note.top = split;
      note.bottom = card.bottom - 14;
      SelectObject(g_dc, g_note_font);
      DrawTextW(g_dc, look.note, -1, &note,
                DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_VCENTER |
                    DT_END_ELLIPSIS);
    }
  }
  GdiFlush();

  for (int i = 0; i < kSlots; ++i) {
    if (IsRectEmpty(&badge[i])) continue;
    const bool lit = i == look.slot;
    const bool dim = look.dim[i];
    const Rgb& fill = lit ? (dim ? kDimLit : kLit) : kCardColor;
    const float alpha = lit ? 1.0f : (dim ? 0.50f : 0.88f);
    const Rgb& ink = lit ? kLitInk : (dim ? kDimInk : kWhite);
    // The value of a switch has the color of a note.
    const Rgb& ink2 = lit || dim ? ink : kNoteInk;
    Box(badge[i].left, badge[i].top, badge[i].right, badge[i].bottom,
        kBadgeRadius, fill, alpha, ink, badge_split[i] - 4, ink2);
  }
  if (!IsRectEmpty(&card)) {
    Box(card.left, card.top, card.right, card.bottom, kCardRadius, kCardColor,
        0.92f, kWhite, split, kNoteInk);
  } else {
    // A small dot marks the middle, where a release chooses nothing.
    const int m = static_cast<int>(kMiddle);
    Box(m - 8, m - 8, m + 8, m + 8, 8.0f, kWhite, 0.55f, kWhite, kSize,
        kWhite);
  }
}

// --- the scroll look -------------------------------------------------------------
//
// With hint_card_style=1 each item is a wax seal, and the card in the middle
// is a parchment scroll with a roll at each side, as a hint card
// (card_scroll.h). The layout is that of the plain look.
//
// THE WAX. On a hint card the wax of a seal has a meaning: green the trigger,
// amber the grip, red each other button. An item of the ring is not a
// button, so a plain item has the red wax. The chosen item has gold wax with
// dark letters, a gold rim and a soft glow, and it is 12 px wider and 8 px
// higher. Its gold is lighter and more yellow than the amber of the grip, and
// only one item at a time has it. An item that cannot act now has a pale
// brown wax at an alpha of 0.6. The value of a switch is gold on the red wax
// and dark red on the gold wax.
//
// THE COST. The ring draws its picture again at each change of the choice.
// So each seal and each card is drawn one time for its slot and its state
// into a cache, and a new picture is a clear, a copy of the parts from the
// cache, and the thin ring of ink and gold. Only a part that changed is drawn
// again. While the ring is open, each frame with no new picture puts one
// more part into the cache (ScrollBake). card_scroll::Seal gives the red wax. The gold wax and the pale wax
// come from it: each pixel keeps its brightness against the middle
// brightness of the red wax.

const int kSealCap = 22;          // the capital letter of a word on a seal
const int kSealSwitchCap = 19;    // the word of a switch
const int kSealValueCap = 17;     // the value of a switch, in the words font
const int kSealLineGap = 12;      // from the word of a switch to its value
const int kScrollNameCap = 27;    // the name on the card
const int kScrollLongCap = 19;    // a long name (a switch of the debug page)
const int kScrollNoteCap = 20;
const int kScrollLineGap = 16;    // from the name to the note
const int kSealPad = 30;          // from the end of a seal to its word
const int kSwitchSealPad = 40;
const int kSealInside = 14;       // the words stay this far inside a seal
const int kChosenWider = 12;
const int kChosenHigher = 8;
const int kSealEdge = 22;         // round a seal: its shadow, its rim, its glow
const int kScrollCardMore = 40;   // the rolls make a card this much wider
const int kScrollBodyOne = 64;    // the paper of a card with one line
const int kScrollBodyTwo = 100;   // and with two lines
const int kDotSeal = 36;          // the seal in the middle with no name
// GDI draws the letters of one part into the work picture, and the part is
// drawn in a picture with the same pitch.
const int kWorkWidth = 640;
const int kWorkHeight = 200;
const int kRasterLogs = 40;

const Rgb kGoldWax = {226.0f, 174.0f, 60.0f};
const Rgb kGoldLight = {255.0f, 246.0f, 210.0f};
const Rgb kPaleGold = {196.0f, 174.0f, 128.0f};
const Rgb kPaleWax = {136.0f, 112.0f, 98.0f};
const Rgb kPaleLight = {214.0f, 200.0f, 184.0f};
const Rgb kRimGold = {255.0f, 220.0f, 120.0f};
const Rgb kGlowGold = {255.0f, 196.0f, 80.0f};
const Rgb kValueGold = {255.0f, 210.0f, 104.0f};
const Rgb kValueRed = {128.0f, 26.0f, 16.0f};
const Rgb kEdgeDark = {30.0f, 10.0f, 5.0f};
const Rgb kEdgeLight = {255.0f, 232.0f, 168.0f};
const Rgb kScrollNoteInk = {150.0f, 40.0f, 20.0f};
const Rgb kLineInk = {70.0f, 40.0f, 18.0f};
const Rgb kLineGold = {214.0f, 170.0f, 78.0f};

// One part of the picture in the cache, and what it shows. The pixels are
// BGRA with the color times the alpha, w by h, at x, y in the picture.
struct Part {
  uint8_t* px;
  size_t room;
  int x, y, w, h;
  bool ok;
  int item;
  int value;
  bool dim;
  wchar_t name[48];
  wchar_t note[48];
};

struct ScrollFont {
  HFONT font;
  int ascent;
  int height;
};

bool g_scroll_ready = false;
bool g_scroll_failed = false;
HDC g_work_dc = nullptr;
HBITMAP g_work_dib = nullptr;
uint32_t* g_letters = nullptr;
uint8_t g_work[kWorkWidth * kWorkHeight * 4];
ScrollFont g_seal_word, g_seal_switch, g_seal_value;
ScrollFont g_card_name, g_card_long, g_card_note;
// Two seals for each slot (not chosen, chosen), a card for each slot and one
// for a card with no slot, and the seal in the middle.
Part g_seal_parts[kSlots][2];
Part g_card_parts[kSlots + 1];
Part g_dot_part;
int g_scroll_new = 0;  // the parts that the picture now drew new
int g_raster_logs = 0;

// hint_card_style now. The switch "Card look" of the debug page changes it
// for all the cards. While the hint cards are off, the key of bwvr.ini.
int ScrollLookNow() {
  const int style = HintCardStyleValue();
  if (style == kDebugCannot) return g_style ? 1 : 0;
  return style != 0 ? 1 : 0;
}

ScrollFont MakeScrollFont(HFONT font) {
  ScrollFont f = {font, 0, 0};
  if (font == nullptr) return f;
  TEXTMETRICW metrics;
  SelectObject(g_work_dc, font);
  if (GetTextMetricsW(g_work_dc, &metrics)) {
    f.ascent = metrics.tmAscent;
    f.height = metrics.tmHeight;
  }
  return f;
}

// The work picture and the fonts of the scroll, one time. False keeps the
// plain look.
bool ScrollStart() {
  if (g_scroll_ready) return true;
  if (g_scroll_failed) return false;
  g_scroll_failed = true;
  g_work_dc = CreateCompatibleDC(nullptr);
  if (g_work_dc == nullptr || !card_scroll::Start(g_work_dc)) {
    ModLog("ring         : the scroll look has no font, so the ring has the "
           "plain look");
    return false;
  }
  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = kWorkWidth;
  info.bmiHeader.biHeight = -kWorkHeight;  // top first
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  g_work_dib =
      CreateDIBSection(g_work_dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (g_work_dib == nullptr || bits == nullptr) {
    ModLog("ring         : GDI gave no picture for the scroll look, so the "
           "ring has the plain look");
    return false;
  }
  g_letters = static_cast<uint32_t*>(bits);
  SelectObject(g_work_dc, g_work_dib);
  SetBkMode(g_work_dc, TRANSPARENT);
  SetTextColor(g_work_dc, RGB(255, 255, 255));
  g_seal_word = MakeScrollFont(card_scroll::SealFont(g_work_dc, kSealCap));
  g_seal_switch =
      MakeScrollFont(card_scroll::SealFont(g_work_dc, kSealSwitchCap));
  g_seal_value =
      MakeScrollFont(card_scroll::WordsFont(g_work_dc, kSealValueCap));
  g_card_name =
      MakeScrollFont(card_scroll::WordsFont(g_work_dc, kScrollNameCap));
  g_card_long =
      MakeScrollFont(card_scroll::WordsFont(g_work_dc, kScrollLongCap));
  g_card_note =
      MakeScrollFont(card_scroll::WordsFont(g_work_dc, kScrollNoteCap));
  if (g_seal_word.font == nullptr || g_seal_switch.font == nullptr ||
      g_seal_value.font == nullptr || g_card_name.font == nullptr ||
      g_card_long.font == nullptr || g_card_note.font == nullptr) {
    ModLog("ring         : GDI gave no font for the scroll look, so the ring "
           "has the plain look");
    return false;
  }
  g_scroll_failed = false;
  g_scroll_ready = true;
  return true;
}

int WorkWidth(const ScrollFont& f, const wchar_t* text) {
  SelectObject(g_work_dc, f.font);
  SIZE size = {0, 0};
  GetTextExtentPoint32W(g_work_dc, text, static_cast<int>(wcslen(text)), &size);
  return size.cx;
}

// The letters of one line, white on black, centered from left to right, on
// the baseline.
void WorkLetters(const ScrollFont& f, const wchar_t* text, int left, int right,
                 int baseline) {
  SelectObject(g_work_dc, f.font);
  RECT r = {left, baseline - f.ascent, right, baseline - f.ascent + f.height};
  DrawTextW(g_work_dc, text, -1, &r,
            DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_TOP |
                DT_END_ELLIPSIS | DT_NOCLIP);
}

// Clears w by h pixels of the work picture and of its letters.
void WorkClear(int w, int h) {
  for (int y = 0; y < h; ++y) {
    memset(g_work + static_cast<size_t>(y) * kWorkWidth * 4, 0,
           static_cast<size_t>(w) * 4);
    memset(g_letters + static_cast<size_t>(y) * kWorkWidth, 0,
           static_cast<size_t>(w) * 4);
  }
}

// Keeps w by h pixels of the work picture in the part, at x, y.
void KeepPart(Part* part, int x, int y, int w, int h) {
  const size_t bytes = static_cast<size_t>(w) * h * 4;
  if (part->room < bytes) {
    uint8_t* px = static_cast<uint8_t*>(realloc(part->px, bytes));
    if (px == nullptr) {
      part->w = part->h = 0;
      return;
    }
    part->px = px;
    part->room = bytes;
  }
  for (int row = 0; row < h; ++row) {
    memcpy(part->px + static_cast<size_t>(row) * w * 4,
           g_work + static_cast<size_t>(row) * kWorkWidth * 4,
           static_cast<size_t>(w) * 4);
  }
  part->x = x;
  part->y = y;
  part->w = w;
  part->h = h;
}

// One part over the picture of the ring.
void PutPart(const Part& part) {
  for (int row = 0; row < part.h; ++row) {
    const int y = part.y + row;
    if (y < 0 || y >= kSize) continue;
    const uint8_t* in = part.px + static_cast<size_t>(row) * part.w * 4;
    uint8_t* out = g_base + static_cast<size_t>(y) * kSize * 4;
    for (int col = 0; col < part.w; ++col, in += 4) {
      const int x = part.x + col;
      const int a = in[3];
      if (a == 0 || x < 0 || x >= kSize) continue;
      uint8_t* o = out + x * 4;
      if (a == 255) {
        memcpy(o, in, 4);
        continue;
      }
      const int k = 255 - a;
      for (int c = 0; c < 4; ++c) {
        o[c] = static_cast<uint8_t>(in[c] + (o[c] * k + 127) / 255);
      }
    }
  }
}

// One color over a pixel with its color times its alpha.
void OverPixel(uint8_t* out, const Rgb& c, float a) {
  if (a <= 0.0f) return;
  if (a > 1.0f) a = 1.0f;
  const float k = 1.0f - a;
  out[0] = static_cast<uint8_t>(c.b * a + out[0] * k + 0.5f);
  out[1] = static_cast<uint8_t>(c.g * a + out[1] * k + 0.5f);
  out[2] = static_cast<uint8_t>(c.r * a + out[2] * k + 0.5f);
  out[3] = static_cast<uint8_t>(255.0f * a + out[3] * k + 0.5f);
}

float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// The signed distance to a box with round ends (as wide as high, or wider):
// below 0 is inside.
float PillDistance(float px, float py, float x0, float y0, float x1,
                   float y1) {
  const float r = 0.5f * (y1 - y0);
  const float qx =
      px < x0 + r ? x0 + r - px : (px > x1 - r ? px - (x1 - r) : 0.0f);
  const float qy = py - 0.5f * (y0 + y1);
  return sqrtf(qx * qx + qy * qy) - r;
}

// Gives the red wax of the seal in the picture the color wax. Each pixel
// keeps its brightness against the middle brightness of the opaque wax, and
// a brighter pixel (the lit side, the sheen) goes toward light.
void Rewax(const card_scroll::Picture& p, const Rgb& wax, const Rgb& light) {
  int count[256];
  memset(count, 0, sizeof(count));
  int total = 0;
  for (int y = 0; y < p.height; ++y) {
    const uint8_t* in = p.bgra + static_cast<size_t>(y) * p.pitch * 4;
    for (int x = 0; x < p.width; ++x, in += 4) {
      if (in[3] != 255) continue;
      ++count[(in[2] * 77 + in[1] * 150 + in[0] * 29) >> 8];
      ++total;
    }
  }
  if (total == 0) return;
  int middle = 0;
  for (int sum = 0; middle < 255; ++middle) {
    sum += count[middle];
    if (2 * sum >= total) break;
  }
  if (middle < 8) middle = 8;
  const float per_middle = 1.0f / (256.0f * middle);
  for (int y = 0; y < p.height; ++y) {
    uint8_t* out = p.bgra + static_cast<size_t>(y) * p.pitch * 4;
    for (int x = 0; x < p.width; ++x, out += 4) {
      const int a = out[3];
      if (a == 0) continue;
      const float alpha = a * (1.0f / 255.0f);
      // The brightness of the pixel with no alpha, against the middle.
      const float k = (out[2] * 77 + out[1] * 150 + out[0] * 29) *
                      per_middle / alpha;
      Rgb c;
      if (k <= 1.0f) {
        c = {wax.r * k, wax.g * k, wax.b * k};
      } else {
        const float t = Clamp01(k - 1.0f);
        c = {wax.r + (light.r - wax.r) * t, wax.g + (light.g - wax.g) * t,
             wax.b + (light.b - wax.b) * t};
      }
      out[0] = static_cast<uint8_t>(c.b * alpha + 0.5f);
      out[1] = static_cast<uint8_t>(c.g * alpha + 0.5f);
      out[2] = static_cast<uint8_t>(c.r * alpha + 0.5f);
    }
  }
}

// The gold rim and the glow of the chosen seal, under the seal: a band of
// 4 px at 5 px out from the box, and a glow that ends 20 px out.
void Rim(const card_scroll::Picture& p, int x0, int y0, int x1, int y1,
         float strength) {
  for (int y = 0; y < p.height; ++y) {
    uint8_t* out = p.bgra + static_cast<size_t>(y) * p.pitch * 4;
    for (int x = 0; x < p.width; ++x, out += 4) {
      const float d = PillDistance(x + 0.5f, y + 0.5f, static_cast<float>(x0),
                                   static_cast<float>(y0),
                                   static_cast<float>(x1),
                                   static_cast<float>(y1));
      if (d >= 20.0f || out[3] == 255) continue;
      const float g = d < 9.0f ? 1.0f : 1.0f - (d - 9.0f) / 11.0f;
      uint8_t src[4] = {0, 0, 0, 0};
      OverPixel(src, kGlowGold, 0.42f * strength * g * g);
      OverPixel(src, kRimGold,
                0.95f * strength * Clamp01(d - 4.5f) * Clamp01(9.5f - d));
      // Under the seal: the rim shows only where the seal does not cover.
      const int k = 255 - out[3];
      for (int c = 0; c < 4; ++c) {
        out[c] = static_cast<uint8_t>(out[c] + (src[c] * k + 127) / 255);
      }
    }
  }
}

// Letters of the work picture in one color, with an edge under them, in the
// box. The edge is the letter 1 px up and 1 px left, as on a seal.
void LineInk(const card_scroll::Picture& p, int x0, int y0, int x1, int y1,
             const Rgb& ink, const Rgb& edge, float edge_alpha) {
  if (x0 < 1) x0 = 1;
  if (y0 < 1) y0 = 1;
  if (x1 > p.width) x1 = p.width;
  if (y1 > p.height) y1 = p.height;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const float letter =
          ((g_letters[y * kWorkWidth + x] >> 8) & 0xFF) * (1.0f / 255.0f);
      const float under =
          ((g_letters[(y - 1) * kWorkWidth + x - 1] >> 8) & 0xFF) *
          (1.0f / 255.0f);
      if (letter <= 0.0f && under <= 0.0f) continue;
      uint8_t* out = p.bgra + (static_cast<size_t>(y) * p.pitch + x) * 4;
      OverPixel(out, edge, under * edge_alpha);
      OverPixel(out, ink, letter);
    }
  }
}

// The seal of a slot in its part of the cache.
void DrawSeal(Part* part, int slot, int item, int value, bool dim, bool lit) {
  part->ok = true;
  part->item = item;
  part->value = value;
  part->dim = dim;
  part->w = part->h = 0;
  const bool two = IsSwitchItem(item);
  const wchar_t* word = L"";
  wchar_t value_text[32] = L"";
  if (two) {
    word = DebugSwitchLabel(item - kItemSwitchBase);
    DebugSwitchValueText(item - kItemSwitchBase, value, value_text, 32);
  } else if (IsGroupItem(item)) {
    word = DebugGroupName(item - kItemGroupBase);
  } else if (item > kItemNone && item < kItems) {
    word = kItemInfo[item].word;
  }
  if (word[0] == 0) return;
  const ScrollFont& word_font = two ? g_seal_switch : g_seal_word;
  int w = WorkWidth(word_font, word);
  if (two) {
    const int value_w = WorkWidth(g_seal_value, value_text);
    if (value_w > w) w = value_w;
  }
  w += 2 * (two ? kSwitchSealPad : kSealPad);
  if (w < kBadgeLeast) w = kBadgeLeast;
  if (w > kBadgeMost) w = kBadgeMost;
  int h = two ? kSwitchHeight : kBadgeHeight;
  if (lit) {
    w += kChosenWider;
    h += kChosenHigher;
  }
  const int pw = w + 2 * kSealEdge;
  const int ph = h + 2 * kSealEdge;
  if (pw > kWorkWidth || ph > kWorkHeight) return;
  WorkClear(pw, ph);
  const card_scroll::Picture picture = {g_work, kWorkWidth, pw, ph};
  const int x0 = kSealEdge, y0 = kSealEdge, x1 = x0 + w, y1 = y0 + h;
  card_scroll::Seal(picture, x0, y0, x1, y1, 0, 11 + slot * 13);
  if (lit) {
    Rewax(picture, dim ? kPaleGold : kGoldWax, kGoldLight);
    Rim(picture, x0, y0, x1, y1, dim ? 0.5f : 1.0f);
  } else if (dim) {
    Rewax(picture, kPaleWax, kPaleLight);
  }

  // The letters: one line in the middle, or the word and its value.
  const int left = x0 + kSealInside, right = x1 - kSealInside;
  int split = y1;
  if (two) {
    const int top = y0 + (h - kSealSwitchCap - kSealLineGap - kSealValueCap) / 2;
    const int word_line = top + kSealSwitchCap;
    WorkLetters(g_seal_switch, word, left, right, word_line);
    WorkLetters(g_seal_value, value_text, left, right,
                word_line + kSealLineGap + kSealValueCap);
    split = word_line + kSealLineGap / 2;
  } else {
    WorkLetters(g_seal_word, word, left, right, y0 + (h + kSealCap) / 2);
  }
  GdiFlush();
  // The gold wax has dark letters, as the amber wax of card_scroll.
  const int tone = lit ? 2 : 0;
  card_scroll::SealInk(picture, g_letters, x0, y0, x1, split, tone);
  if (two) {
    if (dim) {
      card_scroll::SealInk(picture, g_letters, x0, split, x1, y1, tone);
    } else if (lit) {
      LineInk(picture, x0, split, x1, y1, kValueRed, kEdgeLight, 0.55f);
    } else {
      LineInk(picture, x0, split, x1, y1, kValueGold, kEdgeDark, 0.55f);
    }
  }
  if (dim) {
    // A dim seal is faded: all of it at a lower alpha.
    const int scale = lit ? 218 : 154;
    for (int y = 0; y < ph; ++y) {
      uint8_t* out = g_work + static_cast<size_t>(y) * kWorkWidth * 4;
      for (int i = 0; i < pw * 4; ++i) {
        out[i] = static_cast<uint8_t>((out[i] * scale) >> 8);
      }
    }
  }
  float cx = 0.0f, cy = 0.0f;
  SlotPixel(slot, kMiddle, kSlotRadius, &cx, &cy);
  KeepPart(part, static_cast<int>(cx) - w / 2 - kSealEdge,
           static_cast<int>(cy) - h / 2 - kSealEdge, pw, ph);
}

// The card in the middle, a small scroll, in its part of the cache.
void DrawCard(Part* part, const wchar_t* name, const wchar_t* note) {
  part->ok = true;
  wcsncpy_s(part->name, name, _TRUNCATE);
  wcsncpy_s(part->note, note, _TRUNCATE);
  part->w = part->h = 0;
  const bool two = note[0] != 0;
  // A long name (a switch of the debug page) takes a smaller font, and the
  // card can be wider, as in the plain look.
  const ScrollFont* name_font = &g_card_name;
  int name_cap = kScrollNameCap;
  int most = kCardMost;
  if (WorkWidth(g_card_name, name) + 2 * kCardPad > kCardMost) {
    name_font = &g_card_long;
    name_cap = kScrollLongCap;
    most = kCardWide;
  }
  int text = WorkWidth(*name_font, name);
  if (two) {
    const int note_w = WorkWidth(g_card_note, note);
    if (note_w > text) text = note_w;
  }
  int w = text + card_scroll::kWordsLeft + card_scroll::kWordsRight;
  if (w > most + kScrollCardMore) w = most + kScrollCardMore;
  const int body = two ? kScrollBodyTwo : kScrollBodyOne;
  const int h = card_scroll::kPaperTop + body + card_scroll::kPaperBottom;
  if (w > kWorkWidth || h > kWorkHeight) return;
  WorkClear(w, h);
  const card_scroll::Picture picture = {g_work, kWorkWidth, w, h};
  const int paper_x1 = w - card_scroll::kEdgeRight;
  card_scroll::Paper(picture, card_scroll::kEdgeLeft, card_scroll::kPaperTop,
                     paper_x1, card_scroll::kPaperTop + body, 29);
  const int left = card_scroll::kWordsLeft;
  const int right = w - card_scroll::kWordsRight;
  int split = h;
  if (two) {
    const int block = name_cap + kScrollLineGap + kScrollNoteCap;
    const int name_line = card_scroll::kPaperTop + (body - block) / 2 + name_cap;
    const int note_line = name_line + kScrollLineGap + kScrollNoteCap;
    WorkLetters(*name_font, name, left, right, name_line);
    WorkLetters(g_card_note, note, left, right, note_line);
    split = name_line + kScrollLineGap / 2 + 2;
  } else {
    WorkLetters(*name_font, name, left, right,
                card_scroll::kPaperTop + (body + name_cap) / 2);
  }
  GdiFlush();
  card_scroll::Ink(picture, g_letters, 0, 0, w, split, 29);
  if (two) {
    LineInk(picture, 0, split, w, h, kScrollNoteInk, kScrollNoteInk, 0.0f);
  }
  // The middle of the paper is the middle of the ring.
  const int paper_cx = (card_scroll::kEdgeLeft + paper_x1) / 2;
  const int paper_cy = card_scroll::kPaperTop + body / 2;
  KeepPart(part, static_cast<int>(kMiddle) - paper_cx,
           static_cast<int>(kMiddle) - paper_cy, w, h);
}

// A small round seal marks the middle, where a release chooses nothing.
void DrawDot(Part* part) {
  part->ok = true;
  part->w = part->h = 0;
  const int side = kDotSeal + 2 * kSealEdge;
  WorkClear(side, side);
  const card_scroll::Picture picture = {g_work, kWorkWidth, side, side};
  card_scroll::Seal(picture, kSealEdge, kSealEdge, kSealEdge + kDotSeal,
                    kSealEdge + kDotSeal, 0, 7);
  const int m = static_cast<int>(kMiddle);
  KeepPart(part, m - side / 2, m - side / 2, side, side);
}

// The thin ring of ink with a gold line in it, on the circle of the items,
// so the ring reads as one object. It goes under the seals, after them: a
// pixel that a seal covers at an alpha of 0.5 or more has no line, so the
// line does not show through a faded seal. The line of gold is 5 px and the
// ink 11 px wide, so the headset shows no line of 1 px.
void RingLine() {
  const float r = kSlotRadius;
  const float half = 6.0f;
  const float outer = r + half, inner = r - half;
  const int y_first = static_cast<int>(kMiddle - outer) - 1;
  const int y_last = static_cast<int>(kMiddle + outer) + 1;
  for (int y = y_first; y <= y_last; ++y) {
    if (y < 0 || y >= kSize) continue;
    const float dy = y + 0.5f - kMiddle;
    if (outer * outer - dy * dy <= 0.0f) continue;
    const float reach = sqrtf(outer * outer - dy * dy) + 1.0f;
    const float hole =
        inner * inner - dy * dy > 0.0f ? sqrtf(inner * inner - dy * dy) - 1.0f
                                       : 0.0f;
    // The left span and the right span of the row, not the inside.
    const int left_end = static_cast<int>(kMiddle - hole);
    int right_start = static_cast<int>(kMiddle + hole);
    if (right_start <= left_end) right_start = left_end + 1;
    const int spans[2][2] = {{static_cast<int>(kMiddle - reach), left_end},
                             {right_start, static_cast<int>(kMiddle + reach)}};
    uint8_t* row = g_base + static_cast<size_t>(y) * kSize * 4;
    for (const auto& span : spans) {
      const int x_first = span[0] < 0 ? 0 : span[0];
      const int x_last = span[1] > kSize - 1 ? kSize - 1 : span[1];
      for (int x = x_first; x <= x_last; ++x) {
        const float dx = x + 0.5f - kMiddle;
        const float d = fabsf(sqrtf(dx * dx + dy * dy) - r);
        if (d >= half) continue;
        uint8_t* out = row + x * 4;
        if (out[3] >= 128) continue;
        uint8_t line[4] = {0, 0, 0, 0};
        OverPixel(line, kLineInk, 0.5f * Clamp01(5.5f - d));
        OverPixel(line, kLineGold, 0.9f * Clamp01(2.5f - d));
        const int k = (255 - out[3]) * (128 - out[3]) / 128;
        for (int c = 0; c < 4; ++c) {
          out[c] = static_cast<uint8_t>(out[c] + (line[c] * k + 127) / 255);
        }
      }
    }
  }
}

// The scroll look: the parts from the cache, and each part that changed.
void RasterScroll(const Look& look) {
  memset(g_base, 0, sizeof(g_base));
  for (int i = 0; i < kSlots; ++i) {
    const bool lit = i == look.slot;
    Part& part = g_seal_parts[i][lit ? 1 : 0];
    if (!part.ok || part.item != look.item[i] ||
        part.value != look.value[i] || part.dim != look.dim[i]) {
      DrawSeal(&part, i, look.item[i], look.value[i], look.dim[i], lit);
      ++g_scroll_new;
    }
    if (part.w > 0) PutPart(part);
  }
  if (look.name[0] != 0) {
    Part& card =
        g_card_parts[look.slot >= 0 && look.slot < kSlots ? look.slot : kSlots];
    if (!card.ok || wcscmp(card.name, look.name) != 0 ||
        wcscmp(card.note, look.note) != 0) {
      DrawCard(&card, look.name, look.note);
      ++g_scroll_new;
    }
    if (card.w > 0) PutPart(card);
  } else {
    if (!g_dot_part.ok) {
      DrawDot(&g_dot_part);
      ++g_scroll_new;
    }
    if (g_dot_part.w > 0) PutPart(g_dot_part);
  }
  RingLine();
}

// While the ring is open and its picture does not change in a frame, one
// part that a next choice can need goes into the cache in that frame: the
// chosen seal and the card of a slot. So some frames after the ring opens, a
// change of the choice is a copy of parts only.
uint64_t g_baked = 0;
double g_bake_ms = 0.0;

void ScrollBake(const Look& look, const int (*items)[kSlots], int page,
                const Facts& f) {
  const double start = NowSeconds();
  bool drew = false;
  for (int i = 0; i < kSlots && !drew; ++i) {
    if (i == look.slot) continue;
    Part& seal = g_seal_parts[i][1];
    if (!seal.ok || seal.item != look.item[i] ||
        seal.value != look.value[i] || seal.dim != look.dim[i]) {
      DrawSeal(&seal, i, look.item[i], look.value[i], look.dim[i], true);
      drew = true;
      break;
    }
    if (look.item[i] == kItemNone) continue;
    const ItemNow now = StateOf(items[page][i], f);
    wchar_t name[48], note[48];
    wcsncpy_s(name, now.name, _TRUNCATE);
    wcsncpy_s(note, NoteOf(now), _TRUNCATE);
    if (name[0] == 0) continue;
    Part& card = g_card_parts[i];
    if (!card.ok || wcscmp(card.name, name) != 0 ||
        wcscmp(card.note, note) != 0) {
      DrawCard(&card, name, note);
      drew = true;
    }
  }
  if (!drew) return;
  ++g_baked;
  const double ms = (NowSeconds() - start) * 1000.0;
  if (ms > g_bake_ms) g_bake_ms = ms;
}

bool Raster(const Look& look) {
  if (!RasterStart()) return false;
  const double start = NowSeconds();
  g_scroll_new = 0;
  const bool scroll = look.scroll != 0 && ScrollStart();
  if (scroll) {
    RasterScroll(look);
  } else {
    RasterPlain(look);
  }
  g_drawn = look;
  g_have_drawn = true;
  g_staged_fade = -1.0f;
  ++g_rasters;
  const double ms = (NowSeconds() - start) * 1000.0;
  if (ms > g_raster_ms) g_raster_ms = ms;
  if (g_raster_logs < kRasterLogs) {
    ++g_raster_logs;
    if (scroll) {
      ModLog("ring         : frame %llu, a new picture of the ring in the "
             "scroll look took %.2f ms. It drew %d parts new, and the others "
             "came from the cache", ControllersFrameCount(), ms,
             g_scroll_new);
    } else {
      ModLog("ring         : frame %llu, a new picture of the ring in the "
             "plain look took %.2f ms", ControllersFrameCount(), ms);
    }
  }
  if (g_saved < g_save) {
    ++g_saved;
    EyeCaptureSaveUi("ring", ControllersFrameCount(), 1, g_base, kSize * 4,
                     kSize, kSize);
  }
  return true;
}

// The staged picture is the base picture at the fade of this frame.
void Stage(float fade) {
  if (g_staged_fade == fade) return;
  g_staged_fade = fade;
  const double start = NowSeconds();
  const int scale = static_cast<int>(fade * 256.0f + 0.5f);
  if (scale >= 256) {
    memcpy(g_staged, g_base, sizeof(g_staged));
  } else {
    const size_t count = sizeof(g_staged);
    for (size_t i = 0; i < count; ++i) {
      g_staged[i] = static_cast<uint8_t>((g_base[i] * scale) >> 8);
    }
  }
  const double ms = (NowSeconds() - start) * 1000.0;
  if (ms > g_stage_ms) g_stage_ms = ms;
}

// --- the frame -------------------------------------------------------------------

State g_state;
bool g_state_ready = false;
Plane g_plane;
bool g_have_plane = false;
bool g_left = true;
float g_fade = 0.0f;
double g_show_until = 0.0;       // a notice stays until then
bool g_notice = false;
Look g_notice_look;
// The notice of the snapshot: it stays while the snapshot saves, and then it
// says how the save went.
bool g_snapshot_notice = false;
bool g_was_temple = false;
// A button that is down when the hand wakes is not a press.
bool g_button_seen_up = false;
bool g_trigger_down = false;
bool g_key_down = false;         // the test key
// After a choice of a switch the ring opens on its debug page again until
// then (seconds of NowSeconds), in the same place (the Temple or not).
const double kReturnSeconds = 30.0;
int g_return_page = 0;
double g_return_until = 0.0;
bool g_return_temple = false;

const char* SlotWord(const int (*items)[kSlots], const State& st) {
  if (st.slot < 0) return "the middle";
  static char buffer[48];
  return ItemId(items[st.page][st.slot], buffer, sizeof(buffer));
}

// The test key: Ctrl and the 1 key of the number pad.
void PollTestKey() {
  if (!g_test_keys) return;
  const bool down = (GetAsyncKeyState(VK_NUMPAD1) & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool pressed = down && !g_key_down;
  g_key_down = down;
  if (!pressed) return;
  HWND window = GetForegroundWindow();
  DWORD process = 0;
  if (window != nullptr) GetWindowThreadProcessId(window, &process);
  if (process != GetCurrentProcessId()) return;
  RadialMenuWayChange("Ctrl and numpad 1");
}

void BuildLook(const int (*items)[kSlots], int page, int slot, const Facts& f,
               Look* look) {
  memset(look, 0, sizeof(*look));
  look->slot = slot;
  look->scroll = ScrollLookNow();
  for (int i = 0; i < kSlots; ++i) {
    const int item = items[page][i];
    look->item[i] = item;
    look->value[i] =
        IsSwitchItem(item) ? DebugSwitchValue(item - kItemSwitchBase) : 0;
    look->dim[i] = item != kItemNone && !StateOf(item, f).can;
  }
  if (slot >= 0) {
    const ItemNow now = StateOf(items[page][slot], f);
    wcsncpy_s(look->name, now.name, _TRUNCATE);
    wcsncpy_s(look->note, NoteOf(now), _TRUNCATE);
  }
}

}  // namespace

// The switch of the test key Ctrl and Numpad 1 (debug_menu.cpp): the ways
// that open the ring, while the stick can drive the ring. A longer pulse for
// each next way.
int RadialMenuWayValue() {
  ReadSettings();
  if (!g_on || !StickCanDrive()) return kDebugCannot;
  return g_way;
}

void RadialMenuWayChange(const char* by) {
  ReadSettings();
  if (!g_on) return;
  if (!StickCanDrive()) {
    ModLog("ring         : %s: the stick does not drive the ring now "
           "(button_layout=%d), so the ring has one way, frame %llu", by,
           ControllersButtonLayout(), ControllersFrameCount());
    return;
  }
  g_way = (g_way + 1) % 3;
  ApplyWay();
  static const char* const kWays[3] = {
      "both ways open the ring: the button with the hand, and the stick with "
      "the trigger",
      "only the button opens the ring, and the hand picks",
      "only the stick opens the ring, and the trigger chooses"};
  ControllersGrabPulse(1, 0.5f, 0.08f + 0.11f * g_way);
  ModLog("ring         : %s: %s, frame %llu", by, kWays[g_way],
         ControllersFrameCount());
}

bool RadialMenuOpen() { return g_state_ready && g_state.open != kClosed; }

bool RadialMenuShows() {
  return RadialMenuOpen() || g_fade > 0.0f || PlacesPanelOpen() ||
         DebugWindowOpen() || WelcomePageOpen();
}

bool RadialMenuItemCan(int item, const wchar_t** note) {
  ReadSettings();
  Facts f;
  ReadFacts(&f);
  const ItemNow now = StateOf(item, f);
  if (note != nullptr) *note = now.note;
  return now.can;
}

bool RadialMenuItemAct(int item) {
  ReadSettings();
  Facts f;
  ReadFacts(&f);
  const ItemNow now = StateOf(item, f);
  return Choose(item, f, now);
}

void RadialMenuQueueKey(int key, int mods) {
  QueueKey(key, mods);
  ModLog("ring         : the panel of places: the key %s%s, frame %llu",
         ModsName(mods), KeyName(key), ControllersFrameCount());
}

bool RadialMenuMarks(bool set[kMarks], const wchar_t** note) {
  Facts f;
  const bool have = ReadFacts(&f);
  for (int i = 0; i < kMarks; ++i) set[i] = have && f.mark[i];
  const wchar_t* why = nullptr;
  if (!have) {
    why = L"No game runs now";
  } else if (f.keys_off) {
    why = L"The game takes no keys now";
  } else if (f.cutscene) {
    why = L"Not in a cut scene";
  } else if (f.temple) {
    why = L"Not in the Temple";
  } else if (!f.marks_on || !f.moves_on || !f.jumps_on) {
    why = L"The game has this off now";
  }
  if (note != nullptr) *note = why != nullptr ? why : L"";
  return why == nullptr;
}

int RadialMenuHintMode() {
  ReadSettings();
  if (!g_on) return 0;
  if (g_settings.button && g_button == kButtonFace) return 1;
  if (g_settings.stick) return 2;
  return 0;
}

bool RadialMenuStickDrives() {
  ReadSettings();
  return g_on && g_settings.stick;
}

void RadialMenuLayoutChanged() {
  ReadSettings();
  g_button_seen_up = false;
  g_trigger_down = false;
  if (g_state_ready && g_state.open != kClosed) {
    Reset(&g_state);
    ++g_no_choice;
    if (LogRoom()) {
      ModLog("ring         : the ring closes with no choice: the layout of "
             "the buttons changed, frame %llu", ControllersFrameCount());
    }
  }
  // A stick that is pushed now must go back to the middle before it opens
  // the ring.
  if (g_state_ready) g_state.stick_back = true;
  ApplyWay();
}

void RadialMenuPump(unsigned long long) {
  if (g_queued == 0) return;
  const int count = g_queued;
  g_queued = 0;
  for (int i = 0; i < count; ++i) SendKey(g_queue[i].key, g_queue[i].mods);
}

void RadialMenuFrame() {
  ReadSettings();
  if (!g_on) return;
  if (!g_state_ready) {
    g_state_ready = true;
    Reset(&g_state);
  }
  PollTestKey();
  const uint64_t frame = ControllersFrameCount();
  const double now = NowSeconds();
  static double last_time = 0.0;
  float dt = static_cast<float>(now - last_time);
  if (last_time == 0.0 || dt > 0.25f) dt = 0.25f;
  last_time = now;

  RingHand hand;
  const bool have_hand = ControllersRingHand(&hand);
  // A controller with no face buttons keeps the ring on its stick.
  if (have_hand && hand.active && hand.face_buttons != g_face_buttons) {
    g_face_buttons = hand.face_buttons;
    ApplyWay();
    if (LogRoom()) {
      ModLog("ring         : the controller of the ring has %s, so the stick "
             "%s the ring, frame %llu",
             g_face_buttons ? "face buttons" : "no face buttons",
             g_settings.stick ? "drives" : "does not drive", frame);
    }
  }
  Facts facts;
  const bool have_game = ReadFacts(&facts);
  if (have_game) Watch(facts);

  // The ring of the Temple has one page of the ini. A ring that is open when
  // the player goes in or out closes, because its items are not the items of
  // the place.
  const bool temple = have_game && facts.temple;
  Ring& ring = temple ? g_temple_ring : g_ring;
  const bool place_changed = temple != g_was_temple;
  g_was_temple = temple;
  // The debug pages hold the switches that can change now, so they are made
  // again while the ring is closed. An open ring keeps its pages.
  if (g_state.open == kClosed) {
    BuildDebugPages(&ring);
    g_state.open_page = g_debug && now < g_return_until &&
                                g_return_temple == temple &&
                                g_return_page < ring.pages.count
                            ? g_return_page
                            : 0;
  }
  const int (*items)[kSlots] = ring.pages.items;
  const int pages = ring.pages.count;

  Input in;
  memset(&in, 0, sizeof(in));
  in.time = now;
  const char* why = "";
  const bool open_before = g_state.open != kClosed;
  if (!have_hand || !hand.active) {
    why = "the hand has no pose";
  } else if (!hand.usable) {
    why = "the controllers cannot act now";
  } else if (hand.dialog) {
    why = "a dialog box is up";
  } else if (!have_game) {
    why = "no game runs";
  } else if (place_changed) {
    why = "the player went into the Temple or out of it";
  } else if (facts.cutscene || CutsceneWindowFlat()) {
    // The game takes no action in a cut scene, and in the Temple it takes no
    // key at all then (GGame::ProcessKey). The cut scene plays on a screen,
    // and a frame of that screen has no ring. The menu button is still Esc.
    why = "a cut scene plays";
  } else if (!open_before && hand.grab_held) {
    why = "a grip holds the world";
  } else {
    in.can_open = true;
  }
  if (have_hand && hand.active) {
    bool raw = false;
    if (g_button == kButtonFace) raw = hand.button;
    if (g_button == kButtonStickClick) raw = hand.stick_click;
    if (g_button == kButtonTrigger) raw = hand.trigger > 0.55f;
    if (!raw) g_button_seen_up = true;
    in.button = raw && g_button_seen_up;
    g_trigger_down = g_trigger_down ? hand.trigger > 0.35f
                                    : hand.trigger > 0.55f;
    in.trigger = g_button != kButtonTrigger && g_trigger_down;
    if (hand.stick_live) {
      in.stick[0] = hand.stick[0];
      in.stick[1] = hand.stick[1];
    }
    if (open_before && g_have_plane) {
      OnPlane(g_plane, hand.position, in.hand);
      in.have_hand = true;
    }
  } else {
    g_button_seen_up = false;
    g_trigger_down = false;
  }

  Events ev;
  Step(&g_state, g_settings, in, items, ring.pages.links, pages, &ev);

  if (ev.blocked) {
    ++g_blocked;
    if (LogRoom()) {
      ModLog("ring         : the ring does not open: %s, frame %llu", why,
             frame);
    }
    HapticsPulseHands(1, haptics_math::kRefused);
  }
  if (ev.opened) {
    ++g_opens;
    g_left = hand.left;
    PlaneAt(hand.position, hand.head, &g_plane);
    g_have_plane = true;
    g_notice = false;
    g_show_until = 0.0;
    if (LogRoom()) {
      char words[300], title[80];
      PageWords(items[g_state.page], words, sizeof(words));
      PageTitle(ring, temple, g_state.page, title, sizeof(title));
      ModLog("ring         : the ring opens at the %s hand (%s), at %.3f %.3f "
             "%.3f m, %.0f cm from the head. %s: %s, frame %llu",
             hand.left ? "left" : "right",
             g_state.open == kOpenButton ? "the button, and the hand picks"
                                         : "the stick, and the trigger chooses",
             g_plane.center[0], g_plane.center[1], g_plane.center[2],
             controller_math::Distance(g_plane.center, hand.head) * 100.0f,
             title, words, frame);
    }
    HapticsPulseHands(1, haptics_math::kTick);
  }
  if (ev.paged && LogRoom()) {
    char words[300], title[80];
    PageWords(items[g_state.page], words, sizeof(words));
    PageTitle(ring, temple, g_state.page, title, sizeof(title));
    ModLog("ring         : %s: %s, frame %llu", title, words, frame);
  }
  if (ev.moved && g_state.open != kClosed) {
    if (g_state.slot >= 0) {
      // A light pulse and a soft sound when the choice moves to an item.
      HapticsPulseHands(1, haptics_math::kTick);
      AudioCue(kAudioCueRingMove, g_left ? -0.4f : 0.4f);
    }
    if (LogRoom()) {
      if (g_state.open == kOpenButton) {
        ModLog("ring         : the choice is %s, with the hand %.1f cm to the "
               "right and %.1f cm up from the middle, frame %llu",
               SlotWord(items, g_state), in.hand[0] * 100.0f,
               in.hand[1] * 100.0f, frame);
      } else {
        ModLog("ring         : the choice is %s, with the stick at %.2f %.2f, "
               "frame %llu", SlotWord(items, g_state), in.stick[0],
               in.stick[1], frame);
      }
    }
  }
  if (ev.chosen >= 0) {
    const int item = items[ev.chosen_page][ev.chosen];
    const ItemNow item_now = StateOf(item, facts);
    if (Choose(item, facts, item_now)) {
      // A switch gives the pulse of its test key, so the pulse of the ring
      // does not hide it.
      if (!IsSwitchItem(item)) HapticsPulseHands(1, haptics_math::kActivate);
      AudioCue(kAudioCueRingChoose, g_left ? -0.4f : 0.4f);
      // After a switch the ring opens on its page again for a while.
      g_return_until = IsSwitchItem(item) ? now + kReturnSeconds : 0.0;
      g_return_page = ev.chosen_page;
      g_return_temple = temple;
      // THE SNAPSHOT. The ring stays, and its card says that the pictures
      // save. SnapshotFrame below changes the note when the save is over.
      if (item == kItemSnapshot) {
        g_notice = true;
        g_snapshot_notice = true;
        g_show_until = now + kNoticeSeconds;
        BuildLook(items, ev.chosen_page, ev.chosen, facts, &g_notice_look);
        for (int i = 0; i < kSlots; ++i) g_notice_look.dim[i] = true;
        wcsncpy_s(g_notice_look.name, L"Snapshot", _TRUNCATE);
        wcsncpy_s(g_notice_look.note, L"It saves the pictures", _TRUNCATE);
      }
    } else {
      // The ring stays for a moment and says why.
      HapticsPulseHands(1, haptics_math::kRefused);
      g_notice = true;
      g_show_until = now + kNoticeSeconds;
      BuildLook(items, ev.chosen_page, ev.chosen, facts, &g_notice_look);
      for (int i = 0; i < kSlots; ++i) g_notice_look.dim[i] = true;
      wcsncpy_s(g_notice_look.name, item_now.name, _TRUNCATE);
      wcsncpy_s(g_notice_look.note,
                NoteOf(item_now)[0] != 0 ? NoteOf(item_now) : L"Not now",
                _TRUNCATE);
    }
  } else if (ev.closed) {
    ++g_no_choice;
    if (LogRoom()) {
      ModLog("ring         : the ring closes with no choice%s%s, frame %llu",
             in.can_open ? "" : ": ", in.can_open ? "" : why, frame);
    }
  }

  // THE SNAPSHOT. A pulse when its files are saved, and the note on the
  // card. A ring that opens over the notice takes the place of the notice.
  const int saved = SnapshotFrame();
  if (saved != 0) {
    HapticsPulseHands(1, saved > 0 ? haptics_math::kActivate
                                   : haptics_math::kRefused);
    if (g_snapshot_notice && g_notice && g_state.open == kClosed) {
      g_show_until = now + kNoticeSeconds;
      wcsncpy_s(g_notice_look.note, SnapshotNote(), _TRUNCATE);
    }
    g_snapshot_notice = false;
  } else if (g_snapshot_notice && g_notice && SnapshotBusy()) {
    g_show_until = now + kNoticeSeconds;
  }
  if (g_state.open != kClosed) g_snapshot_notice = false;

  // The picture of this frame.
  const bool open = g_state.open != kClosed;
  const bool show = open || (g_notice && now < g_show_until);
  if (g_notice && !show) g_notice = false;
  if (open) {
    Look look;
    BuildLook(items, g_state.page, g_state.slot, facts, &look);
    if (!g_have_drawn || !SameLook(look, g_drawn)) {
      Raster(look);
    } else if (look.scroll != 0 && g_scroll_ready) {
      ScrollBake(look, items, g_state.page, facts);
    }
  } else if (g_notice && (!g_have_drawn || !SameLook(g_notice_look, g_drawn))) {
    Raster(g_notice_look);
  }
  g_fade = hint_card_math::FadeStep(g_fade, show, dt,
                                    show ? kFadeIn : kFadeOut);
  if (g_fade <= 0.0f || !g_have_plane || !g_have_drawn || g_raster_failed ||
      bwvr::XrScreen::CardFailed(bwvr::kCardRing)) {
    return;
  }
  Stage(g_fade);
  const float size = kSize / kPixelsPerMeter * (g_size_pct * 0.01f);
  bwvr::CardLayer layer = {};
  layer.visible = true;
  layer.pixels = g_staged;
  layer.texture_width = kSize;
  layer.texture_height = kSize;
  layer.rect[0] = 0;
  layer.rect[1] = 0;
  layer.rect[2] = kSize;
  layer.rect[3] = kSize;
  for (int i = 0; i < 3; ++i) layer.position[i] = g_plane.center[i];
  for (int i = 0; i < 4; ++i) layer.orientation[i] = g_plane.orientation[i];
  layer.width = size;
  layer.height = size;
  bwvr::XrScreen::StageCard(bwvr::kCardRing, layer);
}

void RadialMenuReport() {
  if (g_opens + g_blocked == 0) return;
  ModLog("ring         : the ring opened %llu times this window: %llu "
         "choices, %llu choices that did not act, %llu times closed with no "
         "choice, %llu keys into the key buffer of the game. %llu times it "
         "did not open",
         g_opens, g_choices, g_refusals, g_no_choice, g_keys_sent, g_blocked);
  if (g_rasters > 0) {
    ModLog("ring         : the picture of the ring was drawn %llu times. The "
           "longest draw took %.2f ms, and the longest step of the fade %.2f "
           "ms", g_rasters, g_raster_ms, g_stage_ms);
  }
  if (g_baked > 0) {
    ModLog("ring         : the scroll look put %llu parts into the cache "
           "between the pictures. The longest took %.2f ms", g_baked,
           g_bake_ms);
  }
  g_opens = g_choices = g_refusals = g_no_choice = g_keys_sent = 0;
  g_blocked = 0;
  g_rasters = 0;
  g_raster_ms = g_stage_ms = 0.0;
  g_baked = 0;
  g_bake_ms = 0.0;
}
