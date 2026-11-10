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
// on the Hand a release before 225 ms is a click on the object. With
// creature_open_slap=0 the open hand never slaps (OpenSpeed), and a slap
// needs the grip.
//
// THE ZONE OF THE TOUCH (creature_touch_easy, EasyRule). The game slaps and
// strokes only in its Creature state, and that state starts only when the
// Creature has ended its body action, 0.1 to 2 s after the press. So the
// press must be on before the hand hits. With the easy rule the press starts
// when the Hand's point is near the body, and it stays through the swing of
// a slap and through a stroke that leaves the skin for a moment.
//
// THE PANEL THAT STAYS (creature_panel_linger). The game arms its Creature
// panel only while a press holds the Creature (the locked select). The mod
// arms it also while the Hand is at the player's Creature with no press, and
// for kLingerSeconds after the Hand left, so the panel does not go on and
// off at the edge of the body.
//
// THE PANEL OVER THE CREATURE (creature_panel_over, OverMoves). The panel
// takes its place over the head when it shows, and it stays there while the
// Creature breathes, turns and takes a step. It takes a new place only when
// the place that it would take now stays far from its place for a moment.

#pragma once

namespace creature_touch_math {

// The time in the body before the press starts.
const double kDwellSeconds = 0.2;
// The least time of a press.
const double kLeastSeconds = 0.5;

// THE RULE OF A PRESS OF THE OPEN HAND.
struct Rule {
  float near_cm = 0.0f;  // a press starts with the Hand's point this near
                         // to the body, in cm of the room. 0 is in the body
  double dwell = kDwellSeconds;  // for this time without a break
  float out_cm = 5.0f;   // a press ends with the Hand's point past this gap
  double out_seconds = 0.0;  // for this time without a break
};

// The rule of creature_touch_easy=0: in the body for 0.2 s, and out past the
// reach of the touch at once.
inline Rule OldRule(float touch_cm) {
  Rule r;
  r.out_cm = touch_cm;
  return r;
}

// The rule of creature_touch_easy=1. The start: 8 cm from the skin for 0.1 s.
// The end: 15 cm from the skin for 0.3 s, so the wind up of a slap and a
// stroke along a thin back keep the press. The reach of the touch counts
// when it is larger.
const float kEasyNearCm = 8.0f;
const double kEasyDwellSeconds = 0.1;
const float kEasyOutCm = 15.0f;
const double kEasyOutSeconds = 0.3;
inline Rule EasyRule(float touch_cm) {
  Rule r;
  r.near_cm = kEasyNearCm;
  r.dwell = kEasyDwellSeconds;
  r.out_cm = touch_cm > kEasyOutCm ? touch_cm : kEasyOutCm;
  r.out_seconds = kEasyOutSeconds;
  return r;
}
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
  double in_since = -1.0;   // the hand came near the body at this time
  double started = 0.0;     // the time of the press
  double out_since = -1.0;  // the hand went out of the zone at this time
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
//   rule       the zone and the times of the press
//   grip       the grip is closed
// A body that the mod does not know is out of the zone at once.
inline int Step(Press* p, double now, bool can_start, bool can_keep,
                bool have_gap, float gap_cm, const Rule& rule, bool grip) {
  if (p->on) {
    if (grip) {
      p->on = false;
      return kEndGrip;
    }
    if (!can_keep) {
      p->on = false;
      return kEndStop;
    }
    if (have_gap && gap_cm <= rule.out_cm) {
      p->out_since = -1.0;
      return kNothing;
    }
    if (p->out_since < 0.0) p->out_since = now;
    const bool long_out =
        !have_gap || now - p->out_since + 1e-9 >= rule.out_seconds;
    if (long_out && now - p->started >= kLeastSeconds) {
      p->on = false;
      p->out_since = -1.0;
      return kEndOut;
    }
    return kNothing;
  }
  if (!can_start || !have_gap || gap_cm > rule.near_cm || grip) {
    p->in_since = -1.0;
    return kNothing;
  }
  if (p->in_since < 0.0) p->in_since = now;
  if (now - p->in_since + 1e-9 < rule.dwell) return kNothing;
  p->on = true;
  p->started = now;
  p->in_since = -1.0;
  p->out_since = -1.0;
  return kStart;
}

// The step with the old rule and the reach out_cm.
inline int Step(Press* p, double now, bool can_start, bool can_keep,
                bool have_gap, float gap_cm, float out_cm, bool grip) {
  return Step(p, now, can_start, can_keep, have_gap, gap_cm, OldRule(out_cm),
              grip);
}

// The real speed of the hand that the game reads in a press of the open
// hand with creature_open_slap=0, in m/s: never the speed of a slap. A hand
// that moves still moves, so the game still sees a stroke.
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

// THE FIRST STROKE (creature_stroke_wait=0). HandStateCreature::Enter sets
// the timers of the state to 0, and the stroke test (0x005B2154 to
// 0x005B2193) needs all of these: the contact time +0xD4 above 1000 ms, the
// time +0xCC at (1 - total) * 1250 ms or more, the time +0xD0 at 2000 ms or
// more, and a bone that is not the bone of the last stroke +0x108. So the
// first stroke of a press comes 2 s after the state starts. The mod gives the
// timers these values at the start of the state, so the first stroke comes
// with the first motion of the hand on the body. After each stroke the game's
// own waits stay: +0xD0 goes to 1000, so a stroke comes each second at most.
struct StrokeTimers {
  int since_stroke;  // +0xCC
  int since_play;    // +0xD0
  int contact;       // +0xD4
  int bone;          // +0x108
};
inline void FirstStrokeTimers(StrokeTimers* t) {
  // The total is -1 at its lowest, so the test of +0xCC needs 2500 ms.
  if (t->since_stroke < 2500) t->since_stroke = 2500;
  if (t->since_play < 2000) t->since_play = 2000;
  if (t->contact < 1000) t->contact = 1000;
  // 9 is the value of the game for no bone (0x005B227B).
  t->bone = 9;
}
// The test of the game, for the math test.
inline bool StrokeTimersPass(const StrokeTimers& t, float total, int bone,
                             int frame_ms) {
  const int need = static_cast<int>((1.0f - total) * 0.5f * 2500.0f);
  return t.contact + frame_ms > 1000 && t.since_stroke >= need &&
         t.bone != bone && t.since_play >= 2000;
}

// THE PANEL OVER THE CREATURE STAYS. It takes a new place only when the
// place that it would take now is more than kOverMoveDegrees from its place
// as the eyes see it, or kOverMoveRatio times nearer or farther, for
// kOverMoveSeconds without a break.
const float kOverMoveDegrees = 12.0f;
const float kOverMoveRatio = 1.4f;
const double kOverMoveSeconds = 0.5;
struct OverHold {
  bool placed = false;
  double away_since = -1.0;
};
//   fresh      the panel shows in this frame and did not show before
//   angle_deg  the angle between the place now and the place that it would
//              take now, from the eyes
//   ratio      the distance of the place that it would take now over the
//              distance of the place now
// True when the panel takes the new place in this frame.
inline bool OverMoves(OverHold* h, bool fresh, float angle_deg, float ratio,
                      double now) {
  if (fresh || !h->placed) {
    h->placed = true;
    h->away_since = -1.0;
    return true;
  }
  const bool away = angle_deg > kOverMoveDegrees || ratio > kOverMoveRatio ||
                    ratio * kOverMoveRatio < 1.0f;
  if (!away) {
    h->away_since = -1.0;
    return false;
  }
  if (h->away_since < 0.0) h->away_since = now;
  if (now - h->away_since + 1e-9 < kOverMoveSeconds) return false;
  h->away_since = -1.0;
  return true;
}

}  // namespace creature_touch_math
