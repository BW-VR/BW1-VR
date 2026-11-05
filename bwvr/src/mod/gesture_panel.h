// The game's gesture pictures in a row over the palm of the hand that does
// not point, while that palm is up (gesture_panel.cpp, docs/miracles.md,
// "The gesture row over the palm").
#pragma once

// mod_main.cpp, at the start, where the real hand is in the world (play modes
// 0, 3 and 4 with the controllers). It reads gesture_panel and
// gesture_panel_cm, and hooks the draw of the pictures.
void GesturePanelInstall();
// controllers.cpp, once a frame, after SpellBookFrame.
void GesturePanelFrame();
// sky_detail.cpp: once in each eye pass, after the game's bubble of the
// spell book.
void GesturePanelDraw();
// spell_book.cpp: true while the row wants the palm that turns up: the draw
// button is down, or the game shows a picture that goes on from a gesture
// that the player drew. Then the palm does not open the book.
bool GesturePanelWantsPalm();
// controllers.cpp, once a report window.
void GesturePanelReport();
