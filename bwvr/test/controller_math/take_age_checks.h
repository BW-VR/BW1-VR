// The age of a picture in the layer of the headset app, a test
// (src/mod/take_age_math.h, and the order of the free slots in
// android/src/frame_loop.h). main.cpp includes this file after Check and Near,
// just before main.
#pragma once

#include <string>

#include "../../android/src/frame_loop.h"
#include "../../src/mod/take_age_math.h"

namespace take_age_checks {

namespace ta = take_age;

inline ta::TakeFacts World(uint64_t frame, uint64_t view_frame, bool eyes = true) {
  ta::TakeFacts t;
  t.eyes = eyes;
  t.frame = frame;
  t.have_view = true;
  t.view_frame = view_frame;
  return t;
}

inline void ClassifyChecks() {
  printf("\nThe reasons of a take with an old picture\n");
  ta::Shown shown;
  Check(ta::Classify(shown, World(256, 256)) == ta::kNew,
        "the first picture is new", ta::Classify(shown, World(256, 256)), 0);
  shown = ta::After(shown, World(256, 256));
  Check(ta::Classify(shown, World(257, 257)) == ta::kNew, "the next frame is new", 0, 0);
  Check(ta::Classify(shown, World(260, 260)) == ta::kNew,
        "a frame that skips frames is new (the frames before it went)", 0, 0);
  shown = ta::After(shown, World(260, 260));

  Check((ta::Classify(shown, World(259, 259)) & ta::kOlderFrame) != 0 &&
            (ta::Classify(shown, World(259, 259)) & ta::kOlderView) != 0,
        "a frame below the shown frame is older, also in its view", 0, 0);
  Check(ta::Classify(shown, World(260, 260)) == ta::kSameFrame,
        "the same frame again is named as such", ta::Classify(shown, World(260, 260)),
        ta::kSameFrame);
  Check((ta::Classify(shown, World(262, 258)) & ta::kOlderView) != 0 &&
            (ta::Classify(shown, World(262, 258)) & ta::kOlderFrame) == 0,
        "a new frame with a view of an older frame is an older view", 0, 0);
  Check((ta::Classify(shown, World(262, 261)) & ta::kViewLags) != 0 &&
            ta::Lag(World(262, 261)) == 1 && ta::Lag(World(262, 262)) == 0,
        "a view below its own frame number lags by the difference (the newest frame of its "
        "batch had no walk)",
        static_cast<double>(ta::Lag(World(262, 261))), 1);
  Check(ta::Classify(shown, World(262, 262)) == ta::kNew && !ta::NamesLine(ta::kSameFrame),
        "a picture with the view of its own frame is new, and a repeat gets no line", 0, 0);

  const ta::TakeFacts none = World(300, 300, false);
  Check(ta::Classify(shown, none) == ta::kNoEyes && ta::NamesLine(ta::kNoEyes),
        "a picture with no eyes is named", 0, 0);
  const ta::Shown after_none = ta::After(shown, none);
  Check(after_none.frame == 260 && after_none.view_frame == 260,
        "a take with no eyes leaves the shown picture as it was",
        static_cast<double>(after_none.frame), 260);

  // A screen has frame numbers of its own: no frame of the world is older than it.
  ta::TakeFacts screen;
  screen.screen = true;
  screen.eyes = true;
  screen.frame = 1000000735;
  Check(ta::Classify(shown, screen) == ta::kNew,
        "a screen after the world compares no frame number", 0, 0);
  const ta::Shown on_screen = ta::After(shown, screen);
  Check(ta::Classify(on_screen, World(256, 256)) == ta::kNew,
        "a world picture after a screen compares no frame number (a new land)", 0, 0);
  screen.frame = 1000000734;
  Check((ta::Classify(on_screen, screen) & ta::kOlderFrame) != 0,
        "a screen below the screen before is older", 0, 0);

  ta::Window w;
  ta::Count(&w, ta::kViewLags | ta::kOlderView, 3);
  ta::Count(&w, ta::kNew, 0);
  ta::Count(&w, ta::kViewLags, 5);
  Check(w.takes == 3 && w.reasons[1] == 1 && w.reasons[3] == 2 && w.most_lag == 5,
        "the window counts each reason and the most lag", static_cast<double>(w.reasons[3]), 2);
  Check(std::string(ta::ReasonName(3)).find("no walk") != std::string::npos &&
            std::string(ta::ReasonName(ta::kReasons)).empty(),
        "each reason has a name, and a bit past the last has none", 0, 0);
}

inline void LayerChecks() {
  printf("\nThe layer of a frame of the session\n");
  ta::Layer l;
  Check(l.Check(500, 500) == ta::kLayerOk, "the first layer is right", 0, 0);
  Check(l.Check(500, 500) == ta::kLayerOk, "the same picture again is right", 0, 0);
  Check(l.Check(501, 501) == ta::kLayerOk, "the next picture is right", 0, 0);
  const uint32_t pair = l.Check(502, 501);
  Check(pair == ta::kPairDiffers && l.Last() == 502,
        "the two eyes with two frames are named, and the layer takes the higher frame",
        static_cast<double>(l.Last()), 502);
  Check(l.Check(501, 501) == ta::kLayerOlder,
        "a layer with a frame below the layer before is named", 0, 0);
  l.Reset();
  Check(l.Check(256, 256) == ta::kLayerOk, "after a reset (a new land) a low frame is right", 0, 0);
}

inline void WriteChecks() {
  printf("\nThe writes of the play thread into the swapchains of a slot\n");
  ta::Writes w;
  w.Reset();
  w.Write(2, 10, 1000.0);
  Check(w.Take().writes == 0, "a slot that no frame named has no gap", 0, 0);
  // A frame 10 names slot 1, and its display time is 1030 ms.
  w.Named(1, 10, 1030.0);
  w.Write(1, 10, 1005.0);
  w.Write(1, 11, 1016.0);
  w.Write(1, 12, 1027.0);
  w.Write(1, 20, 1100.0);
  const ta::WriteStats s = w.Take();
  Check(s.writes == 4 && s.gap[0] == 1 && s.gap[1] == 1 && s.gap[2] == 1 && s.gap[3] == 1,
        "the writes fall in the bins 0, 1, 2 and 3 or more frames",
        static_cast<double>(s.gap[3]), 1);
  Check(s.before_display == 3 && s.have_least && s.least_ms < -24.9 && s.least_ms > -25.1,
        "three writes start before the display time, the first 25 ms before it", s.least_ms, -25);
  Check(w.Take().writes == 0, "a Take starts a new window", 0, 0);
  w.Named(3, 5, -1.0);
  w.Write(3, 5, 2000.0);
  const ta::WriteStats t = w.Take();
  Check(t.writes == 1 && !t.have_least && t.before_display == 0,
        "a frame with no display time gives a gap and no time", static_cast<double>(t.writes), 1);
}

// The order of the free slots. One write and one take in each step, as in a
// game at the rate of the display: the gap is the takes between the take that
// showed a slot and the write into it.
inline int LeastGap(bool least_recent, int slots_queue) {
  namespace fl = frame_loop;
  fl::Ring r(slots_queue != 0);
  r.SetLeastRecent(least_recent);
  const bool all[fl::kSlots] = {true, true, true, true};
  int shown_take[fl::kSlots] = {0, 0, 0, 0};
  int takes = 0;
  int least = 1000;
  for (int step = 0; step < 200; ++step) {
    bool dropped = false;
    const int i = r.BeginWrite(&dropped);
    if (i < 0) break;
    if (shown_take[i] > 0 && takes - shown_take[i] < least) least = takes - shown_take[i];
    r.EndWrite(i);
    const int shown = r.Take(all);
    if (shown >= 0) shown_take[shown] = ++takes;
  }
  return least;
}

// A model of 20000 steps with a random choice of write, finished slots and
// take: with the order on, the play thread never writes the shown slot, the
// serial of the shown slot only grows, and the slot that the take picks is
// ready.
inline void RingOrderModel(bool least_recent, int* writes_into_shown, int* older_shown,
                           int* picks_not_ready) {
  namespace fl = frame_loop;
  fl::Ring r(true);
  r.SetLeastRecent(least_recent);
  uint32_t seed = 12345;
  const auto next = [&seed]() {
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
  };
  uint64_t last_serial = 0;
  *writes_into_shown = *older_shown = *picks_not_ready = 0;
  for (int step = 0; step < 20000; ++step) {
    if (next() % 3 != 0) {
      bool dropped = false;
      const int i = r.BeginWrite(&dropped);
      if (i >= 0) {
        if (i == r.Shown()) ++*writes_into_shown;
        r.EndWrite(i);
      }
    }
    bool done[fl::kSlots];
    for (int i = 0; i < fl::kSlots; ++i) done[i] = next() % 4 != 0;
    const int pick = r.Take(done);
    if (pick >= 0) {
      if (r.State(pick) != fl::Slot::kShown) ++*picks_not_ready;
      if (r.Serial(pick) < last_serial) ++*older_shown;
      last_serial = r.Serial(pick);
    }
  }
}

inline void RingOrderChecks() {
  printf("\nThe order of the free slots of the ring\n");
  Check(LeastGap(false, 1) == 1,
        "the lowest free index writes a slot one take after it showed (the old order)",
        LeastGap(false, 1), 1);
  Check(LeastGap(true, 1) == 3,
        "the slot that showed the longest ago is written three takes after it showed",
        LeastGap(true, 1), 3);
  Check(LeastGap(false, 0) == 1 && LeastGap(true, 0) == 2,
        "a ring of 3 slots: gap 1 with the old order, gap 2 with the new order",
        LeastGap(true, 0), 2);
  for (int order = 0; order < 2; ++order) {
    int into_shown = -1, older = -1, not_ready = -1;
    RingOrderModel(order != 0, &into_shown, &older, &not_ready);
    Check(into_shown == 0 && older == 0 && not_ready == 0,
          order ? "20000 random steps with the new order: no write into the shown slot, no older "
                  "picture, each pick is a shown slot"
                : "20000 random steps with the old order: no write into the shown slot, no older "
                  "picture, each pick is a shown slot",
          into_shown + older + not_ready, 0);
  }
  frame_loop::Ring r(true);
  Check(!r.LeastRecent(), "the ring starts with the old order", 0, 0);
  r.SetLeastRecent(true);
  Check(r.LeastRecent(), "the order can change while the ring runs", 0, 0);
}

inline void TakeAgeChecks() {
  ClassifyChecks();
  LayerChecks();
  WriteChecks();
  RingOrderChecks();
}

}  // namespace take_age_checks
