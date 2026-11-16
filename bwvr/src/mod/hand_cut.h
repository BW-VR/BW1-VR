// A hand in front of a card: the hands of the eye pictures show in front of
// the page of the spell book and of the ring where they are nearer to the
// eye than the page. See hand_cut.cpp and docs/ui-layer.md, "A hand in front
// of a card".
#pragma once

#include <stdint.h>

#include "../vr/xr_screen.h"

// The owner of kCardRing (spell_book.cpp, radial_menu.cpp) calls this after
// it staged the card in this frame. `right` is a second picture of the size
// of the texture of the layer, for the right eye. When a hand is in front of
// the page, HandCutFrame cuts holes into the pixels of the layer and into
// `right`. While the owner gives the same pixels with the same serial,
// HandCutFrame puts the clean pixels of its last cut back first, so the
// owner keeps its picture. A new picture of the owner needs a new serial,
// and it must be clean: no hole of the cut in it. `who` names the card in
// the log. `changed` is the box (x0, y0, x1, y1, in pixels of the texture)
// out of which the picture of the serial `changed_from` and this picture
// have the same pixels, or null. With it, the right picture gets only that
// box again, not the whole picture.
void HandCutOffer(const bwvr::CardLayer& layer, uint8_t* right,
                  const char* who, const int changed[4] = nullptr,
                  uint32_t changed_from = 0);

// spell_book.cpp: the box of the holes that the last cut made in `pixels`,
// the picture of an owner. False when that picture has no hole now. An
// owner that changes only a part of its picture also puts its clean pixels
// back in this box, so the new picture is clean.
bool HandCutHoles(const uint8_t* pixels, int box[4]);

// spell_book.cpp, in each frame that the game's bubble of a pull shows: its
// middle in the LOCAL space and its radius, in meters. With card_ball_cut=1
// the cut makes a hole for each eye where that eye sees the bubble in front
// of the page, the size of the bubble, so the bubble shows over the page and
// the page has no hole beside it. True from HandCutBallOn when the cut does
// it: then the book makes no hole of its own.
void HandCutBall(const float center[3], float radius_m);
bool HandCutBallOn();

// stereo.cpp: once a frame, after OverlayFrame built the hands and before
// the eye passes, with the eye views of this frame in place.
void HandCutFrame();

// overlay.cpp: true when the hand (0 left, 1 right) made a hole in the page
// in the last cut, with card_hand_solid=1. Then the hand has its full
// opacity, and not the faint opacity of a hint.
bool HandCutSolid(int hand);

// The switches of the debug page (debug_menu.cpp): card_hand_front and
// card_hand_solid.
int HandCutValue();
void HandCutChange(const char* by);
int HandCutSolidValue();
void HandCutSolidChange(const char* by);
// card_hand_both_sides and card_ball_cut.
int HandCutSidesValue();
void HandCutSidesChange(const char* by);
int HandCutBallValue();
void HandCutBallChange(const char* by);

// Once a report window.
void HandCutReport();
