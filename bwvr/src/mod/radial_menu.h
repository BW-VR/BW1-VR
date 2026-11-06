// The ring: a menu around the hand that does not point, for the functions of
// the plain game that have no controller button. See radial_menu.cpp,
// docs/controllers.md, "The ring", and docs/controls-audit.md.

#pragma once

// controllers.cpp, once a frame, after the hands of the frame are known.
void RadialMenuFrame();

// controllers.cpp, once for each pass of the message pump of the game. The
// keys of a choice go into the key buffer of the game here.
void RadialMenuPump(unsigned long long now_ms);

// True while the ring is open. Then a grip does not start a grab of the
// world, and the hint card of that hand does not show.
bool RadialMenuOpen();

// True while the picture of the ring shows: the ring is open, it fades out,
// or it shows a notice, or the panel of places is open (places_panel.cpp).
// The spell book (spell_book.cpp) and the panel share the quad layer of the
// ring, and the book does not open while the ring shows.
bool RadialMenuShows();

// THE PANEL OF PLACES (places_panel.cpp) uses the items of the ring and its
// keys. An item is a radial_math::Item.
//
// Whether the item can act now, and the note of the card when it cannot.
bool RadialMenuItemCan(int item, const wchar_t** note);
// The item acts as a choice on the ring does. True when it acted.
bool RadialMenuItemAct(int item);
// A key into the key buffer of the game at the next pass of the pump.
void RadialMenuQueueKey(int key, int mods);
// Which of the 7 bookmarks of the game are set, and whether the game takes
// the bookmark keys now. note gets the reason when it does not.
bool RadialMenuMarks(bool set[7], const wchar_t** note);

// For the hints: 0 the ring is off, 1 its button opens it, 2 only the stick
// opens it.
int RadialMenuHintMode();

// True while the stick of the hand that does not point can open the ring and
// point in it. The stick then does not move the player (move_stick).
bool RadialMenuStickDrives();

// controllers.cpp: the layout of the buttons changed (button_layout). A ring
// that is open closes with no choice, and a button that is down must go up
// before it opens the ring.
void RadialMenuLayoutChanged();

// The switch of the test key Ctrl and Numpad 1, for the debug page
// (debug_menu.cpp): the ways that open the ring. The value is 0 both ways, 1
// the button only, 2 the stick only, or kDebugCannot while the stick does not
// drive the ring.
int RadialMenuWayValue();
void RadialMenuWayChange(const char* by);

// Once a report window.
void RadialMenuReport();
