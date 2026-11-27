// The layer guard and the blame list (layer_guard_math.h). main.cpp includes
// this file after Check and Near, just before main.
#pragma once

#include "../../src/vr/layer_guard_math.h"

namespace layer_guard_checks {

inline void LayerGuardChecks() {
  using namespace layer_guard_math;
  printf("\nThe layer guard and the blame list\n");

  // The values of the ini.
  Check(ClampMode(0) == kGuardOff, "0 is no guard", ClampMode(0), kGuardOff);
  Check(ClampMode(1) == kGuardWine, "1 is the guard under Wine", ClampMode(1),
        kGuardWine);
  Check(ClampMode(2) == kGuardAlways, "2 is the guard always", ClampMode(2),
        kGuardAlways);
  Check(ClampMode(7) == kGuardWine, "a value above 2 is the default",
        ClampMode(7), kGuardWine);
  Check(ClampMode(-1) == kGuardWine, "a value below 0 is the default",
        ClampMode(-1), kGuardWine);

  // The guard of the session.
  Check(!GuardOn(kGuardOff, false), "0 on Windows has no guard", 0, 0);
  Check(!GuardOn(kGuardOff, true), "0 under Wine has no guard", 0, 0);
  Check(!GuardOn(kGuardWine, false), "1 on Windows has no guard", 0, 0);
  Check(GuardOn(kGuardWine, true), "1 under Wine has the guard", 1, 1);
  Check(GuardOn(kGuardAlways, false), "2 on Windows has the guard", 1, 1);
  Check(GuardOn(kGuardAlways, true), "2 under Wine has the guard", 1, 1);
  Check(GuardOn(9, true) && !GuardOn(9, false),
        "a wrong value acts as 1", 1, 1);

  // The guard of a frame: only before a projection layer.
  Check(GuardFrame(kGuardWine, true, kFirstProjection),
        "under Wine a frame with the projection first gets the guard", 1, 1);
  Check(!GuardFrame(kGuardWine, true, kFirstQuad),
        "a frame with a quad first needs no guard", 0, 0);
  Check(!GuardFrame(kGuardWine, true, kFirstNone),
        "a frame with no layer gets no guard", 0, 0);
  Check(!GuardFrame(kGuardWine, false, kFirstProjection),
        "on Windows with 1 the projection frame gets no guard", 0, 0);
  Check(GuardFrame(kGuardAlways, false, kFirstProjection),
        "with 2 the projection frame gets the guard on Windows", 1, 1);
  Check(!GuardFrame(kGuardOff, true, kFirstProjection),
        "with 0 no frame gets the guard", 0, 0);

  // Each card is in one entry of the blame list, and only one.
  uint32_t all = 0;
  int overlaps = 0;
  for (int entry = 0; entry < kBlameCards; ++entry) {
    const uint32_t cards = BlameCards(entry);
    if ((all & cards) != 0) ++overlaps;
    all |= cards;
  }
  const uint32_t every = (1u << bwvr::kCards) - 1u;
  Check(all == every, "the blame list has each kind of card", all, every);
  Check(overlaps == 0, "no card is in two entries", overlaps, 0);
  Check(BlameCards(kBlamePanel) == 0, "the panel entry has no card",
        BlameCards(kBlamePanel), 0);
  const uint32_t six = CardBit(bwvr::kCardHintPointing) |
                       CardBit(bwvr::kCardHintOther) |
                       CardBit(bwvr::kCardHintButton0) |
                       CardBit(bwvr::kCardHintButton0 + 1) |
                       CardBit(bwvr::kCardHintButton0 + 2) |
                       CardBit(bwvr::kCardHintButton0 + 3);
  Check(HintCards() == six, "the hint entry has the six cards of hints",
        HintCards(), six);

  // The order: the newest kind stops first.
  const uint32_t ray = CardBit(bwvr::kCardRay);
  const uint32_t tip = CardBit(bwvr::kCardRayTip);
  Check(BlameNext(ray | tip, true, false) == 3,
        "the ray and its end: the end of the ray stops first",
        BlameNext(ray | tip, true, false), 3);
  Check(BlameNext(ray, true, false) == 4,
        "then the ray, before the panel", BlameNext(ray, true, false), 4);
  Check(BlameNext(0, true, false) == kBlamePanel,
        "then the panel", BlameNext(0, true, false), kBlamePanel);
  Check(BlameNext(0, true, true) == kBlameNone,
        "a panel that stopped before does not stop again",
        BlameNext(0, true, true), kBlameNone);
  Check(BlameNext(0, false, false) == kBlameNone,
        "a frame with no card and no panel stops nothing",
        BlameNext(0, false, false), kBlameNone);
  const uint32_t keyboard = CardBit(bwvr::kCardKeyboard);
  const uint32_t ring = CardBit(bwvr::kCardRing);
  const uint32_t subtitles = CardBit(bwvr::kCardSubtitles);
  const uint32_t hint = CardBit(bwvr::kCardHintButton0 + 2);
  Check(BlameNext(keyboard | ring | subtitles | hint, true, false) == 6,
        "the keyboard stops before the ring",
        BlameNext(keyboard | ring | subtitles | hint, true, false), 6);
  Check(BlameNext(ring | subtitles | hint, true, false) == 7,
        "the ring stops before the subtitles",
        BlameNext(ring | subtitles | hint, true, false), 7);
  Check(BlameNext(subtitles | hint, true, false) == 8,
        "the subtitles stop before the hints",
        BlameNext(subtitles | hint, true, false), 8);
  Check(BlameNext(hint, true, false) == 9,
        "a card of a button stops all the hints",
        BlameNext(hint, true, false), 9);
  const uint32_t sheet = CardBit(bwvr::kCardSheet);
  Check(BlameNext(sheet | ring | tip, false, false) == 0,
        "the sheet of the gestures is the newest kind",
        BlameNext(sheet | ring | tip, false, false), 0);
  Check(BlameNext(CardBit(bwvr::kCardRingRight) | ring, true, false) == 1,
        "the ring of the right eye stops before the ring",
        BlameNext(CardBit(bwvr::kCardRingRight) | ring, true, false), 1);
  Check(BlameNext(CardBit(bwvr::kCardCreature) | tip, true, false) == 2,
        "the Creature card stops before the end of the ray",
        BlameNext(CardBit(bwvr::kCardCreature) | tip, true, false), 2);
  Check(BlameNext(CardBit(bwvr::kCardBubble) | keyboard, true, false) == 5,
        "the bubble stops before the keyboard",
        BlameNext(CardBit(bwvr::kCardBubble) | keyboard, true, false), 5);

  // A frame with all the cards loses one entry for each refused frame, and
  // the panel goes last.
  uint32_t left = every;
  bool panel_refused = false;
  int steps = 0;
  int last = kBlameNone;
  for (;;) {
    const int entry = BlameNext(left, true, panel_refused);
    if (entry == kBlameNone) break;
    if (entry == kBlamePanel) panel_refused = true;
    left &= ~BlameCards(entry);
    last = entry;
    ++steps;
  }
  Check(steps == kBlameCards + 1,
        "all the cards and the panel stop in 11 refused frames", steps,
        kBlameCards + 1);
  Check(last == kBlamePanel && left == 0, "the panel stops last", last,
        kBlamePanel);
}

}  // namespace layer_guard_checks
