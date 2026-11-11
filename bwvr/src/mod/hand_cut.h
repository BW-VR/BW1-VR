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
// owner keeps its picture. A new picture of the owner needs a new serial.
// `who` names the card in the log.
void HandCutOffer(const bwvr::CardLayer& layer, uint8_t* right,
                  const char* who);

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

// Once a report window.
void HandCutReport();
