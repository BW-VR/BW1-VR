// The Creature with no press: the pure rules. See docs/creature.md, "The
// stroke with the open hand", and docs/ui-layer.md, "The Creature panel".
// controllers.cpp, hand_direct.cpp and ui_panel.cpp use them, and
// test/controller_math checks them (creature_touch_checks.h). It has no game
// and no headset in it.
//
// THE STROKE WITH THE OPEN HAND (creature_stroke_open). The game strokes the
// Creature only in the Creature state of the Hand, and only a press of its
// Action button on the Creature starts that state (docs/creature.md, 1.2).
// So the mod presses that button for the player while the open hand stays in
// the body of the player's Creature. A hand that goes past the Creature to an
// object near it must not stroke it, because the Creature learns from each
// stroke. So the hand must stay in the body for kDwellSeconds first. A press
// lasts kLeastSeconds or more, so the game never reads it as a click: a
// short press on the Creature is the leash click (450 ms), and with the leash
// on the Hand a release before 225 ms is a click on the object. The open hand
// never slaps (OpenSpeed). A slap needs the grip.
//
// THE PANEL THAT STAYS (creature_panel_linger). The game arms its Creature
// panel only while a press holds the Creature (the locked select). The mod
// arms it also while the Hand is at the player's Creature with no press, and
// for kLingerSeconds after the Hand left, so the panel does not go on and
// off at the edge of the body.

#pragma once

namespace creature_touch_math {

// The time in the body before the press starts.
const double kDwellSeconds = 0.2;
// The least time of a press.
const double kLeastSeconds = 0.5;
// The speed of an open hand that the game reads, as a part of the speed of a
// slap.
const float kOpenSlapShare = 0.8f;
// The time that the panel stays after the Hand left the Creature.
const double kLingerSeconds = 3.0;

// What the caller does now.
enum Event {
  kNothing,
  kStart,    // press the Action button of the game
  kEndOut,   // let it go: the hand left the body
  kEndGrip,  // the grip closed: the press goes on as a press of the grip
  kEndStop,  // let it go: the hand cannot stroke now
};

// One press of the open hand.
struct Press {
  bool on = false;
  double in_since = -1.0;  // the hand went into the body at this time
  double started = 0.0;    // the time of the press
};

// One step, once a frame.
//   can_start  the hand can start a press now: the controller places the
//              Hand, the Hand is empty, the grip and the trigger are open,
//              and the target at the Hand is the player's Creature
//   can_keep   the press can go on: the controller places the Hand and the
//              trigger is open
//   have_gap   the mod knows the gap to the body of that Creature
//   gap_cm     the gap from the Hand's point to the body, in cm of the room.
//              0 or less is in the body
//   out_cm     a press ends past this gap (creature_touch_cm, the reach of
//              the touch in the Creature state)
//   grip       the grip is closed
inline int Step(Press* p, double now, bool can_start, bool can_keep,
                bool have_gap, float gap_cm, float out_cm, bool grip) {
  if (p->on) {
    if (grip) {
      p->on = false;
      return kEndGrip;
    }
    if (!can_keep) {
      p->on = false;
      return kEndStop;
    }
    const bool out = !have_gap || gap_cm > out_cm;
    if (out && now - p->started >= kLeastSeconds) {
      p->on = false;
      return kEndOut;
    }
    return kNothing;
  }
  if (!can_start || !have_gap || gap_cm > 0.0f || grip) {
    p->in_since = -1.0;
    return kNothing;
  }
  if (p->in_since < 0.0) p->in_since = now;
  if (now - p->in_since + 1e-9 < kDwellSeconds) return kNothing;
  p->on = true;
  p->started = now;
  p->in_since = -1.0;
  return kStart;
}

// The real speed of the hand that the game reads in a press of the open
// hand, in m/s: never the speed of a slap. A hand that moves still moves, so
// the game still sees a stroke.
inline float OpenSpeed(float real_mps, float slap_mps) {
  const float most = kOpenSlapShare * slap_mps;
  return real_mps < most ? real_mps : most;
}

// THE PANEL THAT STAYS.
struct Linger {
  const void* creature = nullptr;  // the Creature of the last show
  double seen = -1.0;              // the last time of a show
};

// One step, once a frame, before the game draws the panel.
//   own        the player's Creature, or null
//   at_hand    the player's Creature is at the Hand now
//   game_armed the game armed the panel in this frame
// True when the mod arms the panel in this frame.
inline bool LingerStep(Linger* l, const void* own, bool at_hand,
                       bool game_armed, double now, double seconds) {
  if (own == nullptr) {
    l->creature = nullptr;
    l->seen = -1.0;
    return false;
  }
  if (game_armed || at_hand) {
    l->creature = own;
    l->seen = now;
    return !game_armed;
  }
  return l->creature == own && l->seen >= 0.0 && now - l->seen < seconds;
}

}  // namespace creature_touch_math
