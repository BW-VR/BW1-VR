// THE CREATURE WITH NO PRESS (docs/creature.md, "The stroke with the open
// hand", and docs/ui-layer.md, "The Creature panel").
// main.cpp includes this file after Check, just before main.
#pragma once

#include "../../src/mod/creature_touch_math.h"

namespace creature_touch_checks {

// A hand at 90 frames a second. gap(t) gives the gap to the body in cm at the
// time t in seconds, and grip(t) the grip. The answers: the time of the start
// and of the end in seconds (-1 for none), the event of the end and the count
// of the starts.
struct Run {
  double start = -1.0;
  double end = -1.0;
  int end_event = creature_touch_math::kNothing;
  int starts = 0;
};

template <typename Gap, typename Grip>
Run Hand(double seconds, Gap gap, Grip grip, bool can_start = true,
         double keep_until = 1e9) {
  using namespace creature_touch_math;
  Run run;
  Press press;
  const double dt = 1.0 / 90.0;
  for (int i = 0; i * dt <= seconds; ++i) {
    const double t = i * dt;
    const int e = Step(&press, t, can_start, t < keep_until, true,
                       static_cast<float>(gap(t)), 5.0f, grip(t));
    if (e == creature_touch_math::kStart) {
      ++run.starts;
      if (run.start < 0.0) run.start = t;
    } else if (e != creature_touch_math::kNothing && run.end < 0.0) {
      run.end = t;
      run.end_event = e;
    }
  }
  return run;
}

inline void CreatureTouchChecks() {
  using namespace creature_touch_math;
  printf("\nThe Creature with no press\n");
  const auto open = [](double) { return false; };

  // THE DWELL. A hand in the body from time 0 starts the press at 0.2 s, and
  // not before.
  {
    const Run r = Hand(1.0, [](double) { return -1.0; }, open);
    Check(r.starts == 1 && r.start >= 0.19 && r.start <= 0.22,
          "a hand that stays in the body presses after 0.2 s", r.start, 0.2);
    Check(r.end < 0.0, "the press goes on while the hand stays in the body",
          r.end, -1.0);
  }
  // A hand that goes through the body in 0.15 s, as a hand that reaches for
  // an object near the Creature does, starts no press.
  {
    const Run r = Hand(1.0, [](double t) { return t > 0.3 && t < 0.45 ? -2.0 : 8.0; },
                       open);
    Check(r.starts == 0, "a hand that goes through the body in 0.15 s starts "
          "no press", r.starts, 0);
  }
  // A hand at the edge of the body, in and out each 0.1 s, starts no press.
  {
    const Run r = Hand(2.0,
                       [](double t) {
                         return static_cast<int>(t * 10.0) % 2 == 0 ? -0.5 : 0.5;
                       },
                       open);
    Check(r.starts == 0, "a hand in and out of the body each 0.1 s starts no "
          "press", r.starts, 0);
  }
  // A closed grip, or a hand that cannot start, starts no press.
  {
    const Run g = Hand(1.0, [](double) { return -1.0; },
                       [](double) { return true; });
    Check(g.starts == 0, "a closed grip in the body starts no press of the "
          "open hand", g.starts, 0);
    const Run c = Hand(1.0, [](double) { return -1.0; }, open, false);
    Check(c.starts == 0, "a hand that cannot start presses nothing", c.starts,
          0);
  }
  // THE END. The hand leaves at 1.0 s: 4 cm out stays in the touch, 6 cm out
  // ends the press at once.
  {
    const Run stays = Hand(2.0, [](double t) { return t < 1.0 ? -1.0 : 4.0; },
                          open);
    Check(stays.end < 0.0, "a hand 4 cm out of the body keeps the press "
          "(creature_touch_cm 5)", stays.end, -1.0);
    const Run gone = Hand(2.0, [](double t) { return t < 1.0 ? -1.0 : 6.0; },
                         open);
    Check(gone.end >= 1.0 && gone.end < 1.02 && gone.end_event == creature_touch_math::kEndOut,
          "a hand 6 cm out of the body ends the press at once", gone.end, 1.0);
  }
  // A hand that leaves soon after the start keeps the press until it is 0.5 s
  // old: the game never reads a click.
  {
    const Run r = Hand(2.0, [](double t) { return t < 0.25 ? -1.0 : 20.0; },
                       open);
    Check(r.start >= 0.19 && r.start <= 0.22, "the short visit presses at 0.2 s",
          r.start, 0.2);
    Check(r.end - r.start >= creature_touch_math::kLeastSeconds - 1e-6 &&
              r.end - r.start < creature_touch_math::kLeastSeconds + 0.02,
          "a press that the hand leaves at once lasts 0.5 s", r.end - r.start,
          creature_touch_math::kLeastSeconds);
  }
  // The grip closes at 0.6 s: the press of the open hand ends, and the press
  // goes on as a press of the grip.
  {
    const Run r = Hand(1.0, [](double) { return -1.0; },
                       [](double t) { return t >= 0.6; });
    Check(r.end >= 0.6 && r.end < 0.62 && r.end_event == creature_touch_math::kEndGrip,
          "a grip that closes in the press takes it", r.end, 0.6);
  }
  // A hand that cannot go on (the trigger, a lost hand) ends the press at
  // once, also before 0.5 s.
  {
    const Run r = Hand(1.0, [](double) { return -1.0; }, open, true, 0.3);
    Check(r.end >= 0.3 && r.end < 0.32 && r.end_event == creature_touch_math::kEndStop,
          "a hand that cannot go on ends the press at once", r.end, 0.3);
  }
  // A body that the mod does not know ends the press after 0.5 s.
  {
    Press p;
    Step(&p, 0.0, true, true, true, -1.0f, 5.0f, false);
    const int start = Step(&p, 0.25, true, true, true, -1.0f, 5.0f, false);
    const int early = Step(&p, 0.5, true, true, false, 0.0f, 5.0f, false);
    const int late = Step(&p, 0.76, true, true, false, 0.0f, 5.0f, false);
    Check(start == creature_touch_math::kStart && early == creature_touch_math::kNothing && late == creature_touch_math::kEndOut,
          "a body that is not known ends the press after 0.5 s", late, creature_touch_math::kEndOut);
  }
  // After an end, a hand that stays in the body presses again after a new
  // dwell, not at once.
  {
    const Run r = Hand(2.0, [](double) { return -1.0; },
                       [](double t) { return t >= 0.6 && t < 0.8; });
    Check(r.starts == 2, "a hand in the body after a grip presses again",
          r.starts, 2);
  }

  // THE SPEED. The open hand never reaches the speed of a slap, at each limit
  // of creature_slap_mps, and a slow stroke keeps its speed.
  {
    bool never = true;
    for (float limit = 0.2f; limit <= 10.0f; limit += 0.1f) {
      for (float v = 0.0f; v <= 20.0f; v += 0.25f) {
        if (!(OpenSpeed(v, limit) < limit)) never = false;
      }
    }
    Check(never, "the open hand is under the speed of a slap at each limit",
          never, 1);
    Near("a stroke of 0.3 m/s keeps its speed", OpenSpeed(0.3f, 1.0f), 0.3,
         1e-6);
    Near("a swing of 2 m/s reads as 0.8 m/s at a limit of 1 m/s",
         OpenSpeed(2.0f, 1.0f), 0.8, 1e-6);
  }

  // THE PANEL THAT STAYS.
  {
    int creature = 0, other = 0;
    const void* own = &creature;
    Linger l;
    // The Hand at the Creature: the mod arms it.
    Check(LingerStep(&l, own, true, false, 10.0, 3.0),
          "the panel shows while the Hand is at the Creature", 1, 1);
    // The game armed it (a press): the mod does not arm it a second time.
    Check(!LingerStep(&l, own, true, true, 10.5, 3.0),
          "the mod does not arm the panel that the game armed", 0, 0);
    // The Hand leaves at 10.5 s: the panel stays to 13.5 s.
    Check(LingerStep(&l, own, false, false, 13.4, 3.0),
          "the panel stays 2.9 s after the Hand left", 1, 1);
    Check(!LingerStep(&l, own, false, false, 13.5, 3.0),
          "the panel goes 3 s after the Hand left", 0, 0);
    // A different Creature of the player (a new game) does not get the time
    // of the last one.
    Linger m;
    LingerStep(&m, own, true, false, 1.0, 3.0);
    Check(!LingerStep(&m, &other, false, false, 1.5, 3.0),
          "the panel does not stay for a different Creature", 0, 0);
    // No Creature: nothing, and the time starts again.
    Check(!LingerStep(&m, nullptr, false, false, 1.6, 3.0),
          "no Creature shows no panel", 0, 0);
    Check(!LingerStep(&m, own, false, false, 1.7, 3.0),
          "after a frame with no Creature the panel does not come back", 0, 0);
    // A linger of 0 s is the old way: the panel only while it is armed.
    Linger z;
    LingerStep(&z, own, true, false, 5.0, 0.0);
    Check(!LingerStep(&z, own, false, false, 5.01, 0.0),
          "a linger of 0 s does not keep the panel", 0, 0);
  }
}

}  // namespace creature_touch_checks
