// The spell book (docs/miracles.md, "The spell book"): a tap of B on the
// pointing hand, or a press of the first face button of the hand that does
// not point, opens a book over the hand that does not point.
// It holds the miracles that the player can ask for now, each with the orb
// of the game (or the pictures of its gestures). The pointing hand picks one,
// and its grip takes the miracle: the request goes to the game in its own
// way, as at the end of a chain of gestures. The debug switch "Unlock all" gives each Worship
// Site of the player every miracle of the game, with no cost in prayer
// power. The switch "All miracles in the book" (debug_all_book=1) shows each
// miracle and power level of the game on pages of 6, with no change to the
// town.

#pragma once

#include "miracle.h"

// controllers.cpp, once a frame, after RadialMenuFrame.
void SpellBookFrame();

// controllers.cpp, once a pump: the grip of the pointing hand (the game's
// right button). True while the book holds this press of the grip, so the
// game and the grab of the world do not get it.
bool SpellBookPumpGrip(bool grip_down);

// hand_direct.cpp, before the interface step of the game (fn_005D1120): the
// request of a miracle that the player took from the book goes to the game
// here, where the game sends the request of a chain of gestures.
void SpellBookInterfaceStep(void* face);

// True while the book is open.
bool SpellBookOpen();

// True when the palm of the hand that does not point can open the book
// (spell_book_palm=1). gesture_panel.cpp then gives that palm to the book,
// except while the row of the gestures wants it.
bool SpellBookPalmOn();

// For the hint words: spell_book is on, and the first face button of the
// hand that does not point opens the book (button_layout=1, a controller
// with face buttons that does not send Esc with that button).
bool SpellBookButtonOn();

// For the hint card: a press of the button can open the book now.
bool SpellBookCanOpen();

// THE TAP OF B (spell_book_b=1). controllers.cpp, at the release of B (the
// second face button of the pointing hand) that closed no window and stopped
// no teleport: the length of the press and the most motion of the hand in
// meters. A short press with a still hand (miracle_cancel_ms, 5 cm) opens or
// closes the book in the next SpellBookFrame, while no miracle is in the
// Hand.
void SpellBookTapB(float seconds, float moved_m);
// spell_book_b is on.
bool SpellBookTapOn();
// For the hint card: a tap of B can open the book now.
bool SpellBookTapCanOpen();

// For the sheet of the gestures (gesture_sheet.cpp): the seed types of the
// miracles that the Worship Sites of the player offer (icons whose magic type
// is on), at most `most`, in the order of the icons. -1 when the player has no
// Worship Site.
int SpellBookSiteMiracles(int out[], int most);
// The gesture types of the chain of a seed type (GSpellSeedInfo+0x100): the
// start of the chain first. The count, 0 to 3.
int SpellBookChain(int seed, int out[3]);
// The short name of a seed type, as the book has it.
const wchar_t* SpellBookSeedName(int seed);

// hint_card.cpp: the lines of the pointing card while the book is open, or
// for a moment after the game refused a miracle of the book. The return
// value is the count of lines, 0 with no book.
int SpellBookHint(MiracleLine lines[kMiracleLines]);

// controllers.cpp, for the word "book" of the scripted controller: the
// middle of the next item of the open book, in the LOCAL space of OpenXR, in
// meters. False when the book is closed or empty.
bool SpellBookTestItem(float out[3]);

// The switches of the debug page (debug_menu.cpp): spell_book,
// spell_book_orbs, debug_all_miracles and debug_all_book.
int SpellBookValue();
void SpellBookChange(const char* by);
int SpellBookOrbsValue();
void SpellBookOrbsChange(const char* by);
int SpellBookAllValue();
void SpellBookAllChange(const char* by);
int SpellBookAllBookValue();
void SpellBookAllBookChange(const char* by);
// spell_book_b, spell_book_palm and spell_book_ring.
int SpellBookRingValue();
void SpellBookRingChange(const char* by);
int SpellBookTapValue();
void SpellBookTapChange(const char* by);
int SpellBookPalmValue();
void SpellBookPalmChange(const char* by);
// spell_book_look.
int SpellBookLookValue();
void SpellBookLookChange(const char* by);
// A job of the thread of the warm of the fonts (card_scroll::WarmAdd): with
// spell_book_look=1 it draws the scroll of a ring one time, so the first
// open of the book does not draw it on the frame thread.
void SpellBookWarm();

// miracle.cpp: the game cast the miracle in the Hand (refused false), or
// refused the cast with the reason. power_up is the power up of the seed
// (SpellSeed+0x6C): -1 for the plain miracle. The tally of the book of all
// miracles counts it when it is the miracle of the last take of that book.
void SpellBookNoteCast(int type, int power_up, bool refused, const char* why);

// hand_glow.cpp: true while the book is open with spell_book_point=1. Then
// the glow at the Hand's point does not show: the page shows the point of
// the pick.
bool SpellBookHidesGlow();
int SpellBookPointValue();
void SpellBookPointChange(const char* by);

// controllers.cpp, once a report window.
void SpellBookReport();
