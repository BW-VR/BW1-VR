// The spell book (docs/miracles.md, "The spell book"): a press of the first
// face button of the hand that does not point opens a book over that hand.
// It holds the miracles that the player can ask for now, each with the
// pictures of its gestures. The pointing hand picks one, and its grip takes
// the miracle: the request goes to the game in its own way, as at the end of
// a chain of gestures. The debug switch "All miracles" gives each Worship
// Site of the player every miracle of the game, with no cost in prayer
// power.

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

// hint_card.cpp: the lines of the pointing card while the book is open, or
// for a moment after the game refused a miracle of the book. The return
// value is the count of lines, 0 with no book.
int SpellBookHint(MiracleLine lines[kMiracleLines]);

// controllers.cpp, for the word "book" of the scripted controller: the
// middle of the next item of the open book, in the LOCAL space of OpenXR, in
// meters. False when the book is closed or empty.
bool SpellBookTestItem(float out[3]);

// The switches of the debug page (debug_menu.cpp): spell_book and
// debug_all_miracles.
int SpellBookValue();
void SpellBookChange(const char* by);
int SpellBookAllValue();
void SpellBookAllChange(const char* by);

// controllers.cpp, once a report window.
void SpellBookReport();
