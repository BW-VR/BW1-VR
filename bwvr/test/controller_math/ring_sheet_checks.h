// THE ICONS, THE PAGES AS MADE AND THE SHEETS OF THE RING (docs/controllers.md,
// "The ring"). main.cpp includes this file after Check and Near, just before
// main.
#pragma once

#include "../../src/mod/ring_icon_math.h"
#include "../../src/mod/ring_sheet_math.h"

namespace ring_sheet_checks {

inline bool BoxesApart(const ring_sheet_math::Layout& l) {
  for (int i = 0; i < l.count; ++i) {
    for (int k = i + 1; k < l.count; ++k) {
      const int* a = l.tile[i].box;
      const int* b = l.tile[k].box;
      if (a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3]) {
        return false;
      }
    }
  }
  return true;
}

inline bool BoxesInPicture(const ring_sheet_math::Layout& l) {
  for (int i = 0; i < l.count; ++i) {
    const int* b = l.tile[i].box;
    if (b[0] < 0 || b[1] < 0 || b[2] > ring_sheet_math::kSize ||
        b[3] > ring_sheet_math::kSize) {
      return false;
    }
  }
  return true;
}

// The middle of the sheet is free for `px` pixels round it.
inline bool MiddleFree(const ring_sheet_math::Layout& l, int px) {
  const int m = static_cast<int>(ring_sheet_math::kMiddle);
  for (int i = 0; i < l.count; ++i) {
    const int* b = l.tile[i].box;
    if (b[0] < m + px && m - px < b[2] && b[1] < m + px && m - px < b[3]) {
      return false;
    }
  }
  return true;
}

inline void RingSheetChecks() {
  using namespace radial_math;
  using namespace ring_icon_math;
  printf("\nThe icons, the pages as made and the sheets of the ring\n");

  // THE ICONS. Each one covers a fair part of its box, nothing at the edge
  // of the box, and each item of the ring has one.
  {
    bool cover_ok = true, edge_ok = true;
    int worst = -1;
    for (int icon = 1; icon < kIcons; ++icon) {
      int in = 0, edge = 0;
      for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
          const float c = IconCover(icon, 0.0f, 0.0f, 100.0f, x + 0.5f,
                                    y + 0.5f);
          if (c > 0.5f) ++in;
          if ((x < 1 || y < 1 || x > 98 || y > 98) && c > 0.0f) ++edge;
        }
      }
      if (in < 600 || in > 7000) {
        cover_ok = false;
        worst = icon;
      }
      if (edge > 0) {
        edge_ok = false;
        worst = icon;
      }
    }
    Check(cover_ok, "each icon covers 6 to 70 percent of its box", worst, -1);
    Check(edge_ok, "no icon touches the edge of its box", worst, -1);
    Check(IconCover(kIconNone, 0.0f, 0.0f, 56.0f, 28.0f, 28.0f) == 0.0f,
          "no icon covers nothing", 0, 0);
    bool all = true;
    for (int item = 1; item < kItems; ++item) {
      if (ItemIcon(item) == kIconNone) {
        all = false;
        worst = item;
      }
    }
    Check(all, "each item of the ring has an icon", worst, -1);
    Check(ItemIcon(kItemSwitchBase) == kIconNone &&
              ItemIcon(kItemGroupBase) == kIconNone,
          "a switch and a group of the debug page have their word only", 0,
          0);
    // A pixel at the middle of a stroke of the plus is covered, and one far
    // from it is not.
    Check(IconCover(kIconPlus, 0.0f, 0.0f, 56.0f, 28.0f, 10.0f) > 0.99f &&
              IconCover(kIconPlus, 0.0f, 0.0f, 56.0f, 10.0f, 10.0f) == 0.0f,
          "the plus covers its strokes and not its corners", 0, 0);
  }

  // THE PAGES AS MADE (ring_pages=1).
  {
    int one[kSlots], two[kSlots];
    char unknown[16];
    const int bad = ParseItems(kDefaultRing1, one, unknown, sizeof(unknown)) +
                    ParseItems(kDefaultRing2, two, unknown, sizeof(unknown));
    Check(bad == 0, "each word of the pages as made is known", bad, 0);
    Check(one[0] == kItemLeash && one[4] == kItemMore && two[4] == kItemMore,
          "page 1 has the leashes at the top, and each page has More at the "
          "bottom", one[0], kItemLeash);
    Check(PageHasItem(one, kItemPlaces) && PageHasItem(one, kItemSave) &&
              PageHasItem(one, kItemSettings) &&
              PageHasItem(one, kItemRecenter) &&
              PageHasItem(one, kItemNames) && PageHasItem(one, kItemPause),
          "page 1 has the places, quick save, the VR Settings, the recenter, "
          "the names and the pause", 0, 0);
    bool none = true;
    const int gone[] = {kItemTemple, kItemEnter, kItemTown, kItemCreature,
                        kItemLeashAggression, kItemLeashLearning,
                        kItemLeashCompassion};
    for (int g : gone) {
      none = none && !PageHasItem(one, g) && !PageHasItem(two, g);
    }
    Check(none, "no page as made has a place or one of the three leashes",
          0, 0);
    Check(two[6] == kItemNone && DebugSlot(two) == 6,
          "page 2 has an empty slot for the item debug", DebugSlot(two), 6);
    Check(IsLinkItem(kItemLeash) && IsLinkItem(kItemPlacesPage) &&
              !IsLinkItem(kItemPlaces) &&
              SheetOfItem(kItemLeash) == kSheetLeash &&
              SheetOfItem(kItemPlacesPage) == kSheetPlaces &&
              SheetOfItem(kItemMore) == kSheetNone,
          "the leash and the places go to their sheets", 0, 0);
  }

  // THE LAYOUT OF THE SHEETS.
  ring_sheet_math::Layout places, all, empty, leash;
  {
    const bool set[kMarks] = {true, false, true, true, false, false, false};
    const bool full[kMarks] = {true, true, true, true, true, true, true};
    const bool no[kMarks] = {false, false, false, false, false, false, false};
    ring_sheet_math::PlacesLayout(set, &places);
    ring_sheet_math::PlacesLayout(full, &all);
    ring_sheet_math::PlacesLayout(no, &empty);
    ring_sheet_math::LeashLayout(&leash);
    Check(places.count == 13 && all.count == 21 && empty.count == 7 &&
              all.count <= kMaxTiles,
          "the places: 6 places, a seal and a bin for each bookmark, and "
          "the plus", places.count, 13);
    Check(places.tile[6].kind == ring_sheet_math::kTileMark &&
              places.tile[6].arg == 0 &&
              places.tile[7].kind == ring_sheet_math::kTileRemove &&
              places.tile[8].arg == 2 &&
              places.tile[12].kind == ring_sheet_math::kTileAdd,
          "bookmarks 1, 3 and 4 in order, each with its bin, and the plus last",
          places.tile[8].arg, 2);
    Check(BoxesApart(places) && BoxesApart(all) && BoxesApart(empty) &&
              BoxesApart(leash),
          "no two tiles of a sheet have boxes that overlap", 0, 0);
    Check(BoxesInPicture(places) && BoxesInPicture(all) &&
              BoxesInPicture(leash),
          "each box is in the picture", 0, 0);
    // 3 cm round the middle at 32 px to 1 cm.
    Check(MiddleFree(places, 96) && MiddleFree(all, 96) &&
              MiddleFree(leash, 64),
          "the middle of each sheet has no tile, where the hand starts", 0, 0);
    Check(leash.count == 3 && leash.tile[0].arg == 3 &&
              leash.tile[1].arg == 2 && leash.tile[2].arg == 1,
          "the leashes: Compassion, Learning, Aggression", leash.count, 3);
    int key = 0, mods = 0;
    const bool go = ring_sheet_math::MarkKey(places.tile[8], &key, &mods);
    Check(go && key == 4 && mods == 0, "the seal of bookmark 3 sends the key 3",
          key, 4);
    const bool remove = ring_sheet_math::MarkKey(places.tile[9], &key, &mods);
    Check(remove && key == 4 && mods == (kModCtrl | kModShift),
          "its bin sends Ctrl, Shift and 3", mods, kModCtrl | kModShift);
    Check(!ring_sheet_math::MarkKey(places.tile[12], &key, &mods) &&
              !ring_sheet_math::MarkKey(places.tile[0], &key, &mods),
          "the plus and a place send no key of a bookmark", 0, 0);
    Check(ring_sheet_math::FreeMark(set) == 1 &&
              ring_sheet_math::FreeMark(full) == -1,
          "the plus takes the first bookmark that is not set", 0, 0);
  }

  // THE PICK ON A SHEET.
  const float per_pixel = 1.0f / 3200.0f;
  Sheet sheets[kSheets];
  memset(sheets, 0, sizeof(sheets));
  ring_sheet_math::SheetOf(places, per_pixel, &sheets[kSheetPlaces]);
  ring_sheet_math::SheetOf(leash, per_pixel, &sheets[kSheetLeash]);
  {
    bool each = true;
    for (int i = 0; i < places.count; ++i) {
      float x = 0.0f, y = 0.0f;
      ring_sheet_math::TileMiddle(places, i, per_pixel, &x, &y);
      if (SheetPick(sheets[kSheetPlaces], x, y, -1, 0.01f) != i) each = false;
    }
    Check(each, "the middle of each box picks its tile", 0, 0);
    Check(SheetPick(sheets[kSheetPlaces], 0.0f, 0.0f, -1, 0.01f) == -1,
          "the middle of the sheet picks no tile", 0, 0);
    float x = 0.0f, y = 0.0f;
    ring_sheet_math::TileMiddle(places, 0, per_pixel, &x, &y);
    // 1 cm below the box of the Creature: no tile, but the Creature stays.
    const float below = sheets[kSheetPlaces].box[0][1] - 0.005f;
    Check(SheetPick(sheets[kSheetPlaces], x, below, -1, 0.01f) == -1 &&
              SheetPick(sheets[kSheetPlaces], x, below, 0, 0.01f) == 0,
          "a tile stays for a hand that shakes just out of its box", 0, 0);
    Check(sheets[kSheetPlaces].box[0][1] > 0.06f,
          "the boxes of the places start more than 6 cm above the middle",
          sheets[kSheetPlaces].box[0][1], 0.06);
  }

  // THE RING TO A SHEET, WITH THE BUTTON AND THE HAND.
  int pages[kPages][kSlots];
  char unknown[16];
  ParseItems(kDefaultRing1, pages[0], unknown, sizeof(unknown));
  ParseItems(kDefaultRing2, pages[1], unknown, sizeof(unknown));
  pages[0][1] = kItemPlacesPage;  // radial_menu.cpp, with places_sheet=1
  const Settings set = DefaultSettings();
  {
    State st;
    Reset(&st);
    Events ev;
    Input in;
    memset(&in, 0, sizeof(in));
    in.can_open = true;
    in.have_hand = true;
    in.sheets = sheets;
    in.button = true;
    in.time = 1.0;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    // Up and to the right: the places.
    in.hand[0] = 0.05f;
    in.hand[1] = 0.05f;
    in.time = 1.1;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(st.slot == 1 && st.sheet == kSheetNone, "the hand picks the places",
          st.slot, 1);
    in.time = 1.3;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(!ev.sheeted && st.sheet == kSheetNone,
          "the sheet waits for the dwell", st.sheet, kSheetNone);
    in.time = 1.6;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(ev.sheeted && st.sheet == kSheetPlaces && st.slot == -1,
          "after the dwell the ring goes to the sheet of the places",
          st.sheet, kSheetPlaces);
    Near("the middle of the sheet is the hand", st.sheet_middle[0], 0.05,
         1e-6);
    // The hand goes to the Creature: its box from the middle of the sheet.
    float x = 0.0f, y = 0.0f;
    ring_sheet_math::TileMiddle(places, 0, per_pixel, &x, &y);
    in.hand[0] = 0.05f + x;
    in.hand[1] = 0.05f + y;
    in.time = 1.7;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(ev.moved && st.slot == 0, "the hand over the Creature picks it",
          st.slot, 0);
    in.time = 3.0;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(st.sheet == kSheetPlaces && st.slot == 0 && !ev.paged,
          "a tile is no link: the sheet stays", st.slot, 0);
    in.button = false;
    in.time = 3.1;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(ev.closed && ev.chosen == 0 && ev.chosen_sheet == kSheetPlaces,
          "the release chooses the tile of the sheet", ev.chosen, 0);
    Check(st.open == kClosed && st.sheet == kSheetNone,
          "and the ring closes, with no sheet", st.sheet, kSheetNone);

    // The next open starts on page 1, and a release in the middle of the
    // sheet chooses nothing.
    in.button = true;
    in.hand[0] = in.hand[1] = 0.0f;
    in.time = 5.0;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(ev.opened && st.sheet == kSheetNone && st.page == 0,
          "the ring opens on page 1", st.page, 0);
    in.hand[1] = 0.07f;  // up: the leashes
    in.time = 5.1;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    in.time = 5.6;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(st.sheet == kSheetLeash, "the hand up goes to the leashes",
          st.sheet, kSheetLeash);
    in.hand[1] = 0.075f;
    in.time = 5.7;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(st.slot == -1, "a hand that stays near the middle picks nothing",
          st.slot, -1);
    in.button = false;
    in.time = 5.8;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(ev.closed && ev.chosen == -1, "and its release chooses nothing",
          ev.chosen, -1);
  }

  // THE RING TO A SHEET, WITH THE STICK AND THE TRIGGER.
  {
    State st;
    Reset(&st);
    Events ev;
    Input in;
    memset(&in, 0, sizeof(in));
    in.can_open = true;
    in.sheets = sheets;
    in.stick[1] = 0.9f;  // up: the leashes
    in.time = 1.0;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(ev.opened && st.open == kOpenStick && st.slot == 0,
          "the stick opens the ring and points at the leashes", st.slot, 0);
    in.trigger = true;
    in.time = 1.1;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(ev.sheeted && st.sheet == kSheetLeash,
          "the trigger on the leashes goes to their sheet", st.sheet,
          kSheetLeash);
    // The stick at the top points at the tile of Learning: not live until
    // the stick leaves it.
    in.trigger = false;
    in.time = 1.2;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(st.slot == -1 && st.dead_slot == 1,
          "the tile under the stick at the change is not live", st.dead_slot,
          1);
    // To the right and up: Aggression.
    in.stick[0] = 0.85f;
    in.stick[1] = 0.75f;
    in.time = 1.3;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(st.slot == 2, "the stick right and up picks Aggression", st.slot,
          2);
    in.trigger = true;
    in.time = 1.4;
    Step(&st, set, in, pages, nullptr, 2, &ev);
    Check(ev.chosen == 2 && ev.chosen_sheet == kSheetLeash && st.open == kClosed,
          "the trigger chooses it", ev.chosen, 2);
  }
}

}  // namespace ring_sheet_checks
