// THE LAYER GUARD AND THE BLAME LIST (layer_guard, xr_screen.cpp, and
// docs/backend.md, "The layer guard"). Pure math: no Windows, no OpenXR. The
// checks are in test/controller_math/layer_guard_checks.h.
//
// THE GUARD. Under Wine and Proton, wineopenxr copies the layers of a frame
// into slots that each have the size of the first layer. A quad is larger
// than a projection layer, so with the projection layer first, each quad
// after it goes into a slot that is too small, and the next quad writes over
// its pose and its size. The guard puts one tiny clear quad first, so each
// slot has the size of a quad. GuardFrame says when a frame gets it.
//
// THE BLAME LIST. A frame that the runtime refuses shows nothing. Then one
// kind of card stops, so that the next frame can go through. BlameNext says
// which kind: the newest kind in the frame first, so a fault of one card
// takes the fewest cards away. The interface panel is the last entry.

#pragma once

#include <stdint.h>

#include "xr_screen.h"

namespace layer_guard_math {

// The values of layer_guard.
const int kGuardOff = 0;     // no guard
const int kGuardWine = 1;    // the guard only under Wine or Proton
const int kGuardAlways = 2;  // the guard on each runtime, for a test

// A value out of the range is the default, kGuardWine.
inline int ClampMode(int value) {
  return value < kGuardOff || value > kGuardAlways ? kGuardWine : value;
}

// True when the frames of the session get the guard.
inline bool GuardOn(int mode, bool wine) {
  mode = ClampMode(mode);
  return mode == kGuardAlways || (mode == kGuardWine && wine);
}

// The kind of the first layer of a frame, before the guard.
const int kFirstNone = 0;        // a frame with no layer
const int kFirstProjection = 1;  // the projection layer of the world
const int kFirstQuad = 2;        // a quad: a screen or the cut scene window

// True when the frame gets the guard as its first layer. A frame whose first
// layer is a quad has slots of the size of a quad already, and a frame with
// no layer has nothing to protect.
inline bool GuardFrame(int mode, bool wine, int first) {
  return first == kFirstProjection && GuardOn(mode, wine);
}

// The bit of one card in a set of cards.
inline uint32_t CardBit(int card) {
  return card >= 0 && card < bwvr::kCards ? 1u << card : 0u;
}

// The cards that hold hints (bwvr::IsHintCard): one entry of the list.
inline uint32_t HintCards() {
  uint32_t set = 0;
  for (int card = 0; card < bwvr::kCards; ++card) {
    if (bwvr::IsHintCard(card)) set |= CardBit(card);
  }
  return set;
}

// The entries of the blame list, in the order that they stop. Each entry
// is one kind of card. The entry after the last card is the panel.
const int kBlameCards = 10;
const int kBlamePanel = kBlameCards;
const int kBlameNone = -1;

// The cards of entry `entry`, or 0 for the panel and for a wrong entry.
inline uint32_t BlameCards(int entry) {
  switch (entry) {
    case 0: return CardBit(bwvr::kCardSheet);
    case 1: return CardBit(bwvr::kCardRingRight);
    case 2: return CardBit(bwvr::kCardCreature);
    case 3: return CardBit(bwvr::kCardRayTip);
    case 4: return CardBit(bwvr::kCardRay);
    case 5: return CardBit(bwvr::kCardBubble);
    case 6: return CardBit(bwvr::kCardKeyboard);
    case 7: return CardBit(bwvr::kCardRing);
    case 8: return CardBit(bwvr::kCardSubtitles);
    case 9: return HintCards();
    default: return 0u;
  }
}

// The entry that stops after a refused frame. `cards` is the set of the
// cards in that frame. With no card of the list in it, the panel stops when
// the frame had a quad of the panel and the panel did not stop before.
// kBlameNone when nothing in the frame can stop.
inline int BlameNext(uint32_t cards, bool with_panel, bool panel_refused) {
  for (int entry = 0; entry < kBlameCards; ++entry) {
    if ((cards & BlameCards(entry)) != 0) return entry;
  }
  return with_panel && !panel_refused ? kBlamePanel : kBlameNone;
}

}  // namespace layer_guard_math
