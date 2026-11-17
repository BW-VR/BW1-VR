// A THROWN MIRACLE. See docs/throw.md, "A thrown miracle".
//
// WHAT THE GAME DOES [V, runblack.exe 1.42].
// - With a miracle in the Hand the Grain state (HandStateGrain_vfunc3
//   0x005B3310) keeps a filter of the speed of the Hand at CHand+0x48C8. Each
//   update moves it toward the velocity of the Hand's point CHand+0x78 in
//   that update, by 1 - exp(-16.09 dt): a time constant of 62 ms
//   (miracle_throw_math.h).
// - The release of a miracle that is cast at the release (FUN_005D48D0)
//   calls the cast at the land point FUN_005D3340. It copies CHand+0x48C8,
//   with no cap, into the first vector of packet 0x4D (0x005D351C), and a
//   Fireball and a flock leave the Hand with that velocity.
// - The same packet has the turn of the Hand at CHand+0x48D4 (0x005D354E).
//   The Grain state writes it as (0, y, 0) at 0x005B36C6: y is a filter of
//   the turn of the direction of the path of the Hand across the land
//   (__CIacos at 0x005B363F), over its speed. The cast copies y (the packet
//   at GInterfaceStatus+0x54) into the cast info at +0x34
//   (SpellSeed::DoPreCastThings 0x007294D5), and a Fireball turns in its
//   flight by it [I for the use, desktop runs show the turn].
// - The particle system of a Fireball carries a MagicFireBall:
//   AttatchFireBallToAtom::ModifyAtomCollection (0x00682FD0, thiscall, the
//   AtomCollection, ret 4) makes it with MagicFireBall::Create at the place
//   of each atom of the collection (the list starts at +0x40, the next atom
//   is at +0x18), and moves it with that atom on each update. The atom has
//   its place at +0x80, +0x84 and +0x88 and its velocity at +0x34.
// - The plain Fireball has one ball. Increase has 3 and Extreme 8, in a fan
//   about the direction of the cast (miracle_throw_math.h, "The balls of a
//   Fireball"). Ball 0 flies along the cast. The particle system puts each
//   new atom at the head of its list, so the first atom of the list is the
//   last ball, which for Increase flies to the right of the cast.
//
// WHAT THE MOD DOES (miracle_throw=1). Before the cast reads them, the mod
// writes the velocity of the throw of an object to CHand+0x48C8: the top
// speed of the grip of the real hand in the 120 ms before the opening of the
// grip, in world units a second (s v). And it writes no turn to CHand+0x48D4.
// So the miracle leaves in the direction and with the speed of the real hand,
// and it flies straight, as a thrown object does. miracle_throw=2 keeps the
// game's turn, and 0 keeps the game's velocity and turn. The log has a line
// for each throw (the real hand, the game's filter, the turn, and what the
// cast got), the balls and their turn from the cast, the path of ball 0 and
// the last place of each ball. Without the arc (below) the hook on
// ModifyAtomCollection only reads the atoms.
//
// THE FLIGHT OF A BALL [V]. The rule of group 0 of the particle system,
// UpdateRuleGravityWithFloor::ModifyAtomCollection (0x006A1880, thiscall, the
// AtomCollection, ret 4), moves each ball one step in each game turn, with
// the time dt of the float at 0x00D4E0EC (0.1 s, miracle_throw_math.h, "The
// flight of a ball"). It has a drag (Damping at +0x28), a gravity (Gravity at
// +0x24 times GravityScale of the atom at +0x11C), less gravity near
// -MaxSpeed (+0x20), the contact with the land and the deflection by a
// Physical Shield. The game draws the atom on the line between its last two
// places when the collection has InterpolateDrawPos (bit 2 of +0x38).
//
// THE ARC (miracle_flight=1, with miracle_throw 1 or 2 and throw_law=2). At
// the first sight of the balls the mod puts each ball on the real arc of the
// throw: from the Hand's point at the cast, with the velocity of the cast
// over k and the gravity s g / k^2 (miracle_throw_math::BallFlightPlan). Ball
// 0 has the velocity of the cast, and each other ball keeps the turn and the
// speed of the game's fan about ball 0. Then the hook on the rule runs the
// rule of the game for the collection in more steps of a shorter dt, with no
// drag, no cap and the gravity of the arc in GravityScale. With the start
// velocity of RuleStartVelocity each step is on the arc. The rule of the game
// still makes the contact, the bounce, the impact sound and the deflection.
// At the first contact of a ball the mod gives it back its GravityScale, and
// the game's rule moves it as in the plain game.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "MinHook.h"
#include "debug_menu.h"
#include "miracle_throw.h"
#include "miracle_throw_math.h"
#include "physics_switch.h"
#include "throw_physics.h"

extern void ModLog(const char* format, ...);

// hand_direct.cpp
bool HandDirectSwing(float peak_room[3], float room[9], float* life,
                     float* units_per_meter, double* peak_ms,
                     double* opened_ms);
float HandDirectRoomUnits();
// controllers.cpp
uint64_t ControllersFrameCount();
void ControllersGesturePulse(float amplitude, float seconds);

namespace {

using miracle_throw_math::FlatLength;
using miracle_throw_math::SideDegrees;
using miracle_throw_math::UpDegrees;

const int kHandThrow = 0x48C8;   // CHand: the throw velocity, units a second
const int kHandTurn = 0x48D4;    // CHand: the turn of the Hand, three floats
const int kHandPlace = 0x78;     // CHand: the Hand's point
const uintptr_t kModifyFireBall = 0x00682FD0;
const int kCollectionAtom = 0x40;
const int kAtomPlace = 0x80;
const int kAtomNext = 0x18;      // AtomCore: the next atom of the list
const int kAtomVelocity = 0x34;  // AtomCore: units a second
const int kAtomGravityScale = 0x11C;  // AtomCore: GravityScale
const int kCollectionFlags = 0x38;    // AtomCollection: bit 2 InterpolateDrawPos
const uintptr_t kGravityRule = 0x006A1880;
const int kRuleMaxSpeed = 0x20;  // UpdateRuleGravityWithFloor
const int kRuleGravity = 0x24;
const int kRuleDamping = 0x28;
const uintptr_t kParticleTime = 0x00D4E0EC;  // dt of a particle step, s
const uintptr_t kParticleRate = 0x00D4E0F0;  // 1 / dt
const uintptr_t kGetAltitude = 0x00803090;
const float kMapScale = 6553.6f;  // a MapCoords int is the world unit times this
typedef float(__fastcall* GetAltitudeFn)(const int32_t* coords);

typedef bool(__fastcall* ModifyFn)(void* self, void* unused, void* collection);
ModifyFn o_Modify = nullptr;
ModifyFn o_Gravity = nullptr;
bool g_gravity_hooked = false;
// miracle_flight: 1 the real arc, 0 the game's flight.
int g_flight = 1;
bool g_rule_logged = false;

bool g_installed = false;
// miracle_throw: 0 the game's velocity and turn, 1 the velocity of the real
// hand and no turn, 2 the velocity of the real hand and the game's turn.
const int kThrowGame = 0;
const int kThrowHand = 1;
const int kThrowHandCurve = 2;
int g_mode = kThrowHand;
int g_throw_logs = 0;
int g_path_logs = 0;
const int kMaxThrowLogs = 60;
const int kMaxPathLogs = 400;

// A cast that the game sends next, and a cast that it sent. A Fireball whose
// atom comes in the second after a sent cast is the Fireball of that cast.
struct Cast {
  bool valid;
  double time;
  uint64_t frame;
  char name[32];
  float sent[3];   // what the cast got, units a second
  float game[3];   // the game's filter of the Hand
  float turn;      // the game's turn of the Hand, CHand+0x48D8
  float turn_sent; // the turn that the cast got
  float hand[3];   // the Hand's point at the cast
  float units;     // world units in a meter of the room
  bool real;       // the cast got the velocity of the real hand
  bool arc;        // the balls fly the real arc (miracle_flight=1)
  miracle_throw_math::BallFlight plan;  // the arc of ball 0, when real
  float slow_asked;  // the k of throw_slow_x10
};
Cast g_next = {};
Cast g_cast = {};
const double kCastWait = 1.0;

// The flight of a ball of a Fireball: the place of its atom over time. The
// plain Fireball has one ball, and its power levels have 3 and 8
// (miracle_throw_math.h, "The balls of a Fireball").
struct Track {
  bool active;
  void* atom;
  Cast cast;
  int ball;    // 0 is the ball on the cast, made first
  int balls;   // the balls of the cast
  double t0;
  float start[3];
  float last[3];
  double last_time;
  int marks;
  // The real arc of this ball: from the Hand's point at the cast, t seconds
  // after the cast. Valid when the cast was real.
  bool has_arc;
  float v0[3];
  bool land_found;   // where the arc meets the land
  double land_t;
  float land_at[3];
  // The mod flies the ball on the arc (miracle_flight=1) until its first
  // contact. arc_t is the time of the arc of the place of the atom now.
  void* collection;
  bool flying;
  double arc_t;
  float scale_game;  // GravityScale of the atom before the mod
  int turns;         // game turns of the rule on the arc
  int steps;         // steps of the rule on the arc
  bool contact;
};
const int kTracks = 16;
// The tracks that the mod flies on the arc now.
int g_flying = 0;
// The longest flight on the arc, in seconds of the arc. Then the game's
// gravity is back.
const double kArcLongest = 30.0;
Track g_tracks[kTracks] = {};
// The most balls that the log follows in one collection.
const int kMostBalls = 8;
// The times after the start of a flight at which the log has the place of
// ball 0.
const double kMarks[] = {0.25, 0.5, 1.0, 2.0, 4.0};
const int kMarkCount = sizeof(kMarks) / sizeof(kMarks[0]);
// A flight ends in the log when its atom has no update for this time, or
// after the longest time.
const double kGone = 0.5;
const double kLongest = 10.0;
// A ball on its arc with no update for this time is gone for the mod.
const double kGoneFlying = 2.0;

double Now() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart) /
         static_cast<double>(frequency.QuadPart);
}

bool IniPath(char path[MAX_PATH]) {
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash =
      length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return false;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return true;
}

int IniInt(const char* key, int fallback) {
  char path[MAX_PATH];
  if (!IniPath(path)) return fallback;
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

const char* SideWord(float degrees) {
  return degrees >= 0.0f ? "right" : "left";
}

float Speed(const float v[3]) {
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

bool ReadVector(const uint8_t* base, int offset, float out[3]) {
  if (base == nullptr || IsBadReadPtr(base + offset, 3 * sizeof(float))) {
    return false;
  }
  const float* p = reinterpret_cast<const float*>(base + offset);
  for (int i = 0; i < 3; ++i) out[i] = p[i];
  return true;
}

// The place of a flight at a mark, with the direction of its path from the
// start against what the cast got and against the game's filter.
void LogPlace(const Track& t, const float at[3], double seconds,
              const char* what) {
  if (g_path_logs >= kMaxPathLogs) return;
  ++g_path_logs;
  const float path[3] = {at[0] - t.start[0], at[1] - t.start[1],
                         at[2] - t.start[2]};
  const float across = FlatLength(path);
  const float units = t.cast.units > 0.0f ? t.cast.units : 1.0f;
  const float to_sent = SideDegrees(t.cast.sent, path);
  const float to_game = SideDegrees(t.cast.game, path);
  // The distance from the real arc: at the time of the arc of the place of
  // the atom while the mod flies it, and at the time since the cast for the
  // game's flight.
  char arc[200] = "";
  if (t.has_arc && !t.contact) {
    const double when = t.flying ? t.arc_t : t.last_time - t.cast.time;
    float p[3], v[3];
    miracle_throw_math::BallArcAt(t.cast.hand, t.v0, t.cast.plan.gravity,
                                  when, p, v);
    const float d[3] = {at[0] - p[0], at[1] - p[1], at[2] - p[2]};
    sprintf_s(arc, sizeof(arc),
              ". It is %.1f cm of the room from the real arc at %.2f s after "
              "the cast (%s)",
              Speed(d) / units * 100.0f, when,
              t.flying ? "the mod flies it on the arc" : "the game's flight");
  } else if (t.contact) {
    strcpy_s(arc, sizeof(arc), ". It is past its first contact");
  }
  ModLog("miracle throw: ball %d of %d of the %s of frame %llu %s %.1f %.1f "
         "%.1f, %.2f s after it started, %.1f units (%.2f m of the room) "
         "across the land from its start and %.1f units up. Its path goes "
         "%.1f degrees to the %s of the velocity that the cast got, and %.1f "
         "degrees to the %s of the game's filter of the Hand%s",
         t.ball, t.balls, t.cast.name, t.cast.frame, what, at[0], at[1], at[2],
         seconds, across, across / units, path[1], fabsf(to_sent),
         SideWord(to_sent), fabsf(to_game), SideWord(to_game), arc);
}

float LandAt(float x, float z) {
  const int32_t coords[3] = {static_cast<int32_t>(x * kMapScale),
                             static_cast<int32_t>(z * kMapScale), 0};
  return reinterpret_cast<GetAltitudeFn>(kGetAltitude)(coords);
}

// A track stops the flight on the arc. With the atom still in its
// collection, the atom gets its GravityScale back.
void StopArc(Track* t, uint8_t* atom) {
  if (!t->flying) return;
  t->flying = false;
  if (g_flying > 0) --g_flying;
  if (atom != nullptr &&
      !IsBadWritePtr(atom + kAtomGravityScale, sizeof(float))) {
    *reinterpret_cast<float*>(atom + kAtomGravityScale) = t->scale_game;
  }
}

Track* TrackOf(void* atom) {
  for (int i = 0; i < kTracks; ++i) {
    if (g_tracks[i].active && g_tracks[i].atom == atom) return &g_tracks[i];
  }
  return nullptr;
}

Track* FreeTrack() {
  Track* oldest = &g_tracks[0];
  for (int i = 0; i < kTracks; ++i) {
    if (!g_tracks[i].active) return &g_tracks[i];
    if (g_tracks[i].t0 < oldest->t0) oldest = &g_tracks[i];
  }
  // The atom of the oldest track can be gone: the mod does not write it.
  StopArc(oldest, nullptr);
  return oldest;
}

// The first sight of the balls of a cast. The particle system puts each new
// atom at the head of its list, so the last atom of the list is ball 0, the
// ball on the cast. One line gives the start of ball 0 and the turn of each
// ball from the cast, with the turn of the game's fan for that speed.
// The mod puts a ball on its arc at the time since the cast, with the start
// velocity of the steps of the rule.
void StartArc(Track* t, uint8_t* atom, uint8_t* collection, double now) {
  const miracle_throw_math::BallFlight& plan = t->cast.plan;
  if (IsBadWritePtr(atom + kAtomVelocity, 3 * sizeof(float)) ||
      IsBadWritePtr(atom + kAtomPlace, 3 * sizeof(float)) ||
      IsBadWritePtr(atom + kAtomGravityScale, sizeof(float))) {
    return;
  }
  const double t0 = now - t->cast.time;
  float p[3], v[3], start[3];
  miracle_throw_math::BallArcAt(t->cast.hand, t->v0, plan.gravity, t0, p, v);
  miracle_throw_math::RuleStartVelocity(
      v, plan.gravity,
      miracle_throw_math::kBallTurn / static_cast<float>(plan.steps), start);
  float* place = reinterpret_cast<float*>(atom + kAtomPlace);
  float* velocity = reinterpret_cast<float*>(atom + kAtomVelocity);
  for (int i = 0; i < 3; ++i) {
    place[i] = p[i];
    velocity[i] = start[i];
    t->start[i] = t->last[i] = p[i];
  }
  t->scale_game = *reinterpret_cast<const float*>(atom + kAtomGravityScale);
  t->collection = collection;
  t->arc_t = t0;
  t->flying = true;
  ++g_flying;
}

void StartBalls(uint8_t* const* atoms, int count, double now,
                uint8_t* collection) {
  const Cast cast = g_cast;
  g_cast.valid = false;
  char turns[kMostBalls * 12] = {};
  size_t used = 0;
  float start0[3] = {0.0f, 0.0f, 0.0f};
  float velocity0[3] = {0.0f, 0.0f, 0.0f};
  // The game's velocity of ball 0, the last atom of the list: the fan of the
  // other balls is about it.
  float game0[3] = {0.0f, 0.0f, 0.0f};
  ReadVector(atoms[count - 1], kAtomVelocity, game0);
  Track* track0 = nullptr;
  for (int k = 0; k < count; ++k) {
    uint8_t* atom = atoms[k];
    float at[3], velocity[3] = {0.0f, 0.0f, 0.0f};
    if (!ReadVector(atom, kAtomPlace, at)) continue;
    ReadVector(atom, kAtomVelocity, velocity);
    Track* t = FreeTrack();
    memset(t, 0, sizeof(*t));
    t->active = true;
    t->atom = atom;
    t->cast = cast;
    t->ball = count - 1 - k;
    t->balls = count;
    t->t0 = now;
    for (int i = 0; i < 3; ++i) t->start[i] = t->last[i] = at[i];
    t->last_time = now;
    if (cast.real) {
      t->has_arc = true;
      if (t->ball == 0) {
        for (int i = 0; i < 3; ++i) t->v0[i] = cast.plan.v0[i];
      } else {
        miracle_throw_math::BallFanVelocity(game0, velocity, cast.plan.v0,
                                            t->v0);
      }
      t->land_found = miracle_throw_math::BallArcMeetsLand(
          cast.hand, t->v0, cast.plan.gravity, 0.0, 0.01, kArcLongest, LandAt,
          &t->land_t, t->land_at);
      if (cast.arc) StartArc(t, atom, collection, now);
    }
    if (t->ball == 0) {
      track0 = t;
      for (int i = 0; i < 3; ++i) {
        start0[i] = at[i];
        velocity0[i] = velocity[i];
      }
    } else if (used + 12 < sizeof(turns)) {
      const float side = SideDegrees(cast.sent, velocity);
      const int wrote = sprintf_s(turns + used, sizeof(turns) - used,
                                  "%s%d %.2f", used == 0 ? "" : ", ", t->ball,
                                  side);
      if (wrote > 0) used += static_cast<size_t>(wrote);
    }
  }
  // The picture between the steps: a ball that the mod flies moves on each
  // frame, on the line between its last two places.
  bool blends = false;
  if (collection != nullptr &&
      !IsBadWritePtr(collection + kCollectionFlags, 1)) {
    blends = (collection[kCollectionFlags] & 2) != 0;
    if (!blends && track0 != nullptr && track0->flying) {
      collection[kCollectionFlags] |= 2;
    }
  }
  if (g_path_logs >= kMaxPathLogs) return;
  ++g_path_logs;
  const float units = cast.units > 0.0f ? cast.units : 1.0f;
  const float d[3] = {start0[0] - cast.hand[0], start0[1] - cast.hand[1],
                      start0[2] - cast.hand[2]};
  const float side0 = SideDegrees(cast.sent, velocity0);
  const float fan = miracle_throw_math::FireballFanRadians(
                        miracle_throw_math::FireballSpeed(Speed(cast.sent))) *
                    57.29578f;
  ModLog("miracle throw: the %s of frame %llu has %d balls. Ball 0 starts its "
         "flight at %.1f %.1f %.1f, %.0f ms after the cast and %.1f cm of the "
         "room from the Hand's point at the cast. It moves %.2f degrees to the "
         "%s of the velocity that the cast got and %.1f degrees up. The other "
         "balls move these degrees to the right of the cast: %s. The game's "
         "fan for this speed is %.2f degrees",
         cast.name, cast.frame, count, start0[0], start0[1], start0[2],
         (now - cast.time) * 1000.0, Speed(d) / units * 100.0f, fabsf(side0),
         SideWord(side0), UpDegrees(velocity0), used == 0 ? "none" : turns,
         fan);
  if (track0 == nullptr || !track0->has_arc || g_path_logs >= kMaxPathLogs) {
    return;
  }
  ++g_path_logs;
  const Track& t = *track0;
  const miracle_throw_math::BallFlight& plan = cast.plan;
  char land[200] = "The arc stays over the land for 30 s";
  if (t.land_found) {
    const float flat[3] = {t.land_at[0] - cast.hand[0], 0.0f,
                           t.land_at[2] - cast.hand[2]};
    sprintf_s(land, sizeof(land),
              "The arc meets the land %.2f s after the cast, at %.1f %.1f "
              "%.1f, %.2f m of the room across the land from the Hand's "
              "point",
              t.land_t, t.land_at[0], t.land_at[1], t.land_at[2],
              Speed(flat) / units);
  }
  ModLog("miracle throw: the real arc of ball 0 of the %s of frame %llu: k "
         "%.2f (throw_slow_x10 asks %.2f), %.1f units a second (%.2f m/s of "
         "the room) under %.1f units a second squared, from the Hand's point "
         "at the cast. %s. miracle_flight=%d, throw_law=%d: %s. %s",
         cast.name, cast.frame, plan.slow, cast.slow_asked, Speed(plan.v0),
         Speed(plan.v0) / units, plan.gravity, land, g_flight,
         ThrowPhysicsLaw(),
         t.flying ? "the mod flies the balls on their arcs until the first "
                    "contact"
                  : "the game flies the balls",
         !t.flying ? ""
         : blends  ? "The game draws the balls between their last two places"
                   : "The mod sets InterpolateDrawPos of the collection, so "
                     "the game draws the balls between their last two "
                     "places");
  if (t.flying) {
    ModLog("miracle throw: the arc has %d steps of the rule in a game turn "
           "(%.1f ms each)",
           plan.steps, 100.0f / static_cast<float>(plan.steps));
  }
}

// THE STEPS ON THE ARC. The hook on the rule of group 0 runs the rule for a
// collection with a ball on its arc in plan.steps steps of dt / steps, with
// no drag and no cap, and the gravity of the arc in GravityScale. After each
// step a ball that did not move freely had its first contact.
bool __fastcall h_Gravity(void* self, void* unused, void* collection) {
  uint8_t* rule = static_cast<uint8_t*>(self);
  uint8_t* c = static_cast<uint8_t*>(collection);
  if (g_flying <= 0 || c == nullptr || rule == nullptr ||
      IsBadReadPtr(c + kCollectionAtom, sizeof(void*)) ||
      IsBadWritePtr(rule + kRuleMaxSpeed, 3 * sizeof(float))) {
    return o_Gravity(self, unused, collection);
  }
  // The balls of this collection that fly on the arc.
  const int kMost = 32;
  uint8_t* atoms[kMostBalls];
  Track* tracks[kMostBalls];
  int count = 0;
  int seen = 0;
  uint8_t* atom = *reinterpret_cast<uint8_t* const*>(c + kCollectionAtom);
  bool here[kTracks] = {};
  while (atom != nullptr && seen < kMost &&
         !IsBadReadPtr(atom + kAtomNext, sizeof(void*))) {
    ++seen;
    for (int i = 0; i < kTracks; ++i) {
      Track& t = g_tracks[i];
      if (!t.active || !t.flying || t.atom != atom || t.collection != c) {
        continue;
      }
      here[i] = true;
      if (count < kMostBalls) {
        atoms[count] = atom;
        tracks[count] = &t;
        ++count;
      }
    }
    atom = *reinterpret_cast<uint8_t* const*>(atom + kAtomNext);
  }
  // A ball that left its collection flies no more.
  for (int i = 0; i < kTracks; ++i) {
    if (g_tracks[i].active && g_tracks[i].flying &&
        g_tracks[i].collection == c && !here[i]) {
      StopArc(&g_tracks[i], nullptr);
    }
  }
  float* dt_global = reinterpret_cast<float*>(kParticleTime);
  float* rate_global = reinterpret_cast<float*>(kParticleRate);
  const float dt = *dt_global;
  const float rate = *rate_global;
  float* rule_f = reinterpret_cast<float*>(rule + kRuleMaxSpeed);
  const float max_speed = rule_f[0];
  const float gravity = rule_f[1];
  const float damping = rule_f[2];
  if (count == 0 || !(dt > 0.0f) || !(gravity > 0.0f)) {
    return o_Gravity(self, unused, collection);
  }
  if (!g_rule_logged) {
    g_rule_logged = true;
    ModLog("miracle throw: the rule of the balls of a Fireball "
           "(UpdateRuleGravityWithFloor) has MaxSpeed %.1f, Gravity %.1f and "
           "Damping %.3f. A step of the particle system is %.3f s",
           max_speed, gravity, damping, dt);
  }
  const int steps = tracks[0]->cast.plan.steps;
  const float sub = dt / static_cast<float>(steps);
  bool result = true;
  for (int k = 0; k < steps; ++k) {
    // The balls that still fly in this step.
    float before[kMostBalls][3];
    bool flies[kMostBalls];
    int flying = 0;
    for (int b = 0; b < count; ++b) {
      Track& t = *tracks[b];
      flies[b] = t.flying && t.arc_t < kArcLongest;
      if (t.flying && !flies[b]) StopArc(&t, atoms[b]);
      if (!flies[b]) continue;
      ++flying;
      float* scale = reinterpret_cast<float*>(atoms[b] + kAtomGravityScale);
      *scale = t.cast.plan.gravity / gravity;
      ReadVector(atoms[b], kAtomVelocity, before[b]);
    }
    if (flying == 0) {
      // No ball flies on the arc now: the rest of the turn is one step of
      // the game's rule.
      *dt_global = sub * static_cast<float>(steps - k);
      *rate_global = 1.0f / *dt_global;
      result = o_Gravity(self, unused, collection);
      break;
    }
    rule_f[0] = 1e9f;
    rule_f[2] = 0.0f;
    *dt_global = sub;
    *rate_global = 1.0f / sub;
    result = o_Gravity(self, unused, collection);
    rule_f[0] = max_speed;
    rule_f[2] = damping;
    for (int b = 0; b < count; ++b) {
      if (!flies[b]) continue;
      Track& t = *tracks[b];
      float after[3], at[3];
      ReadVector(atoms[b], kAtomVelocity, after);
      ReadVector(atoms[b], kAtomPlace, at);
      ++t.steps;
      if (miracle_throw_math::RuleStepWasFree(before[b], after,
                                              t.cast.plan.gravity, sub)) {
        t.arc_t += sub;
        continue;
      }
      // THE FIRST CONTACT: the rule of the game put the ball on the land or
      // a shield deflected it.
      t.contact = true;
      StopArc(&t, atoms[b]);
      if (g_path_logs >= kMaxPathLogs) continue;
      ++g_path_logs;
      const float units = t.cast.units > 0.0f ? t.cast.units : 1.0f;
      const float land = LandAt(at[0], at[2]);
      const bool on_land = fabsf(at[1] - land) < 2.0f;
      char where[160] = "";
      if (t.land_found) {
        const float d[3] = {at[0] - t.land_at[0], at[1] - t.land_at[1],
                            at[2] - t.land_at[2]};
        sprintf_s(where, sizeof(where),
                  ", %.1f cm of the room from the place where its arc meets "
                  "the land (%.2f s after the cast)",
                  Speed(d) / units * 100.0f, t.land_t);
      }
      ModLog("miracle throw: ball %d of %d of the %s of frame %llu has its "
             "first contact (%s) %.3f s after the cast on its arc, at %.1f "
             "%.1f %.1f, %.1f units over the land there%s. It flew %d game "
             "turns in %.2f s and %d steps of the rule. The game's rule moves "
             "it from now",
             t.ball, t.balls, t.cast.name, t.cast.frame,
             on_land ? "the land" : "a deflection, for example a Physical "
                                    "Shield",
             t.arc_t + sub, at[0], at[1], at[2], at[1] - land, where, t.turns,
             Now() - t.cast.time, t.steps);
    }
  }
  for (int b = 0; b < count; ++b) ++tracks[b]->turns;
  *dt_global = dt;
  *rate_global = rate;
  return result;
}

void Follow(uint8_t* atom, double now) {
  Track* t = TrackOf(atom);
  float at[3];
  if (t == nullptr || !ReadVector(atom, kAtomPlace, at)) return;
  for (int i = 0; i < 3; ++i) t->last[i] = at[i];
  t->last_time = now;
  if (t->ball != 0) return;
  while (t->marks < kMarkCount && now - t->t0 >= kMarks[t->marks]) {
    LogPlace(*t, at, now - t->t0, "is at");
    ++t->marks;
  }
}

bool __fastcall h_Modify(void* self, void* unused, void* collection) {
  uint8_t* c = static_cast<uint8_t*>(collection);
  if (g_installed && c != nullptr &&
      !IsBadReadPtr(c + kCollectionAtom, sizeof(void*))) {
    // The atoms of the collection, from the head of its list.
    uint8_t* atoms[kMostBalls];
    int count = 0;
    uint8_t* atom = *reinterpret_cast<uint8_t* const*>(c + kCollectionAtom);
    while (atom != nullptr && count < kMostBalls &&
           !IsBadReadPtr(atom + kAtomNext, sizeof(void*))) {
      atoms[count++] = atom;
      atom = *reinterpret_cast<uint8_t* const*>(atom + kAtomNext);
    }
    const double now = Now();
    if (count > 0 && TrackOf(atoms[0]) == nullptr && g_cast.valid &&
        now - g_cast.time <= kCastWait) {
      StartBalls(atoms, count, now, c);
    } else {
      for (int k = 0; k < count; ++k) Follow(atoms[k], now);
    }
  }
  return o_Modify(self, unused, collection);
}

void LogKey(const char* from) {
  ModLog("miracle throw: %s: miracle_throw=%d. %s", from, g_mode,
         g_mode == kThrowHand
             ? "A Fireball and a flock leave the Hand with the top speed of "
               "the grip of the real hand in the 120 ms before the opening of "
               "the grip, as a thrown object does, and with no turn"
         : g_mode == kThrowHandCurve
             ? "A Fireball and a flock leave the Hand with the top speed of "
               "the grip of the real hand, and with the game's turn of the "
               "path of the Hand"
             : "A Fireball and a flock leave the Hand with the game's filter "
               "of the speed of the Hand (62 ms) and its turn of the path of "
               "the Hand");
}

void LogFlight(const char* from) {
  ModLog("miracle throw: %s: miracle_flight=%d. %s", from, g_flight,
         g_flight != 0
             ? "A thrown Fireball flies the real arc of the throw, as a thrown "
               "object does, until its first contact. It needs miracle_throw "
               "1 or 2 and throw_law=2"
             : "A thrown Fireball has the game's flight");
}

}  // namespace

void MiracleThrowInstall() {
  if (g_installed) return;
  g_mode = IniInt("miracle_throw", kThrowHand);
  if (g_mode < kThrowGame || g_mode > kThrowHandCurve) g_mode = kThrowHand;
  // physics=0 gives miracle_flight the value 0 of the plain game when
  // bwvr.ini does not set it (physics_switch.cpp).
  g_flight =
      IniInt("miracle_flight", PhysicsSwitchDefault("miracle_flight", 1)) != 0
          ? 1
          : 0;
  void* target = reinterpret_cast<void*>(kModifyFireBall);
  MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(&h_Modify),
                                   reinterpret_cast<void**>(&o_Modify));
  if (status == MH_OK) status = MH_EnableHook(target);
  ModLog("hooks        : the Fireball on its atom "
         "AttatchFireBallToAtom::ModifyAtomCollection at 0x%08X %s",
         static_cast<unsigned>(kModifyFireBall),
         status == MH_OK ? "ok" : "FAILED");
  void* rule = reinterpret_cast<void*>(kGravityRule);
  MH_STATUS rule_status =
      MH_CreateHook(rule, reinterpret_cast<void*>(&h_Gravity),
                    reinterpret_cast<void**>(&o_Gravity));
  if (rule_status == MH_OK) rule_status = MH_EnableHook(rule);
  g_gravity_hooked = rule_status == MH_OK;
  ModLog("hooks        : the flight of a ball "
         "UpdateRuleGravityWithFloor::ModifyAtomCollection at 0x%08X %s",
         static_cast<unsigned>(kGravityRule),
         g_gravity_hooked ? "ok" : "FAILED");
  g_installed = true;
  LogKey("the start");
  LogFlight("the start");
}

void MiracleThrowSetFlight(int flight) { g_flight = flight != 0 ? 1 : 0; }

int MiracleFlightValue() {
  return g_installed && g_gravity_hooked ? g_flight : kDebugCannot;
}

void MiracleFlightChange(const char* by) {
  if (!g_installed || !g_gravity_hooked) return;
  g_flight = g_flight != 0 ? 0 : 1;
  ControllersGesturePulse(0.5f, g_flight != 0 ? 0.30f : 0.06f);
  LogFlight(by);
}

void MiracleThrowBeforeCast(uint8_t* hand, const char* name) {
  g_next.valid = false;
  if (!g_installed || hand == nullptr ||
      IsBadReadPtr(hand + kHandThrow, 3 * sizeof(float)) ||
      IsBadReadPtr(hand + kHandTurn, 3 * sizeof(float))) {
    return;
  }
  float* v = reinterpret_cast<float*>(hand + kHandThrow);
  float* turn = reinterpret_cast<float*>(hand + kHandTurn);
  float peak_room[3], room[9], life = 0.0f, s = 0.0f;
  double peak_ms = 0.0, opened_ms = 0.0;
  const bool swing =
      HandDirectSwing(peak_room, room, &life, &s, &peak_ms, &opened_ms);
  float real[3] = {0.0f, 0.0f, 0.0f};
  if (swing) miracle_throw_math::SwingWorld(peak_room, room, life, s, real);
  Cast& c = g_next;
  memset(&c, 0, sizeof(c));
  c.valid = true;
  c.time = Now();
  c.frame = ControllersFrameCount();
  strcpy_s(c.name, sizeof(c.name), name != nullptr ? name : "miracle");
  c.units = swing ? s : HandDirectRoomUnits();
  const float* place = reinterpret_cast<const float*>(hand + kHandPlace);
  for (int i = 0; i < 3; ++i) {
    c.game[i] = v[i];
    c.hand[i] = place[i];
  }
  c.turn = turn[1];
  // The velocity of the real hand needs a swing. No turn is the same with a
  // swing and with none.
  if (g_mode != kThrowGame && swing) {
    for (int i = 0; i < 3; ++i) v[i] = real[i];
  }
  if (g_mode == kThrowHand) {
    for (int i = 0; i < 3; ++i) turn[i] = 0.0f;
  }
  for (int i = 0; i < 3; ++i) c.sent[i] = v[i];
  c.turn_sent = turn[1];
  // THE ARC (miracle_flight=1): only for the velocity of the real hand and
  // with the real flight of a thrown object (throw_law=2).
  c.real = swing && g_mode != kThrowGame && s > 0.0f;
  c.slow_asked = ThrowPhysicsSlow();
  if (c.real) {
    miracle_throw_math::BallFlightPlan(c.sent, s, c.slow_asked, &c.plan);
  }
  c.arc = c.real && g_flight == 1 && g_gravity_hooked &&
          ThrowPhysicsLaw() == 2;
  if (g_throw_logs >= kMaxThrowLogs) return;
  ++g_throw_logs;
  const float units = c.units > 0.0f ? c.units : 1.0f;
  if (!swing) {
    ModLog("miracle throw: %s leaves the Hand, frame %llu. The real hand has "
           "no swing now (no motion, or no scale of the God view or the "
           "table), so the cast gets the game's filter of the Hand: %.1f %.1f "
           "%.1f units a second (%.2f m/s of the room). The game's turn of the "
           "Hand is %.4f, and the cast gets %.4f (miracle_throw=%d)",
           c.name, c.frame, v[0], v[1], v[2], Speed(v) / units, c.turn,
           c.turn_sent, g_mode);
    return;
  }
  const float side = SideDegrees(real, c.game);
  ModLog("miracle throw: %s leaves the Hand, frame %llu. The real hand: a top "
         "speed of %.2f m/s, %.0f ms before the cast (the grip opened %.0f ms "
         "before it), %.0f degrees up. The game's filter of the Hand: %.2f "
         "m/s, %.1f degrees to the %s of the real hand and %.0f degrees up. "
         "The game's turn of the Hand is %.4f. miracle_throw=%d: the cast "
         "gets %.1f %.1f %.1f units a second (%.2f m/s of the room at %.1f "
         "units per meter) and the turn %.4f",
         c.name, c.frame, Speed(real) / units, peak_ms, opened_ms,
         UpDegrees(real), Speed(c.game) / units, fabsf(side), SideWord(side),
         UpDegrees(c.game), c.turn, g_mode, c.sent[0], c.sent[1], c.sent[2],
         Speed(c.sent) / units, units, c.turn_sent);
}

void MiracleThrowAfterCast(bool sent) {
  if (!g_next.valid) return;
  g_next.valid = false;
  if (!sent) return;
  g_cast = g_next;
  g_cast.valid = true;
  g_cast.time = Now();
}

void MiracleThrowFrame() {
  if (!g_installed) return;
  const double now = Now();
  for (int i = 0; i < kTracks; ++i) {
    Track& t = g_tracks[i];
    if (!t.active) continue;
    const bool gone = now - t.last_time > kGone;
    if (!gone && now - t.t0 < kLongest) continue;
    // The hook of the rule needs the track of a ball on its arc, up to
    // kGoneFlying with no update.
    if (t.flying && now - t.last_time <= kGoneFlying) continue;
    LogPlace(t, t.last, t.last_time - t.t0,
             gone ? "has no update now. Its last place is"
                  : "flies for the longest time of the log. It is at");
    // A ball with no update can be gone: the mod does not write its atom.
    if (gone) StopArc(&t, nullptr);
    t.active = false;
  }
}

int MiracleThrowValue() { return g_installed ? g_mode : kDebugCannot; }

// The row goes 0, 1, 2 and back to 0. A long pulse means the default 1.
void MiracleThrowChange(const char* by) {
  if (!g_installed) return;
  g_mode = (g_mode + 1) % (kThrowHandCurve + 1);
  ControllersGesturePulse(0.5f, g_mode == kThrowHand ? 0.30f : 0.06f);
  LogKey(by);
}
