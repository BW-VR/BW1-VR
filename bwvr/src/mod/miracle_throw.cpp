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
//   of the first atom of the collection (+0x40), and moves it with that atom
//   on each update. The atom has its place at +0x80, +0x84 and +0x88.
//
// WHAT THE MOD DOES (miracle_throw=1). Before the cast reads them, the mod
// writes the velocity of the throw of an object to CHand+0x48C8: the top
// speed of the grip of the real hand in the 120 ms before the opening of the
// grip, in world units a second (s v). And it writes no turn to CHand+0x48D4.
// So the miracle leaves in the direction and with the speed of the real hand,
// and it flies straight, as a thrown object does. miracle_throw=2 keeps the
// game's turn, and 0 keeps the game's velocity and turn. The log has a line
// for each throw (the real hand, the game's filter, the turn, and what the
// cast got), and the path of the Fireball after it: the hook on
// ModifyAtomCollection only reads the place of the atom.

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

typedef bool(__fastcall* ModifyFn)(void* self, void* unused, void* collection);
ModifyFn o_Modify = nullptr;

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
};
Cast g_next = {};
Cast g_cast = {};
const double kCastWait = 1.0;

// The flight of a Fireball: the place of its atom over time.
struct Track {
  bool active;
  void* atom;
  Cast cast;
  double t0;
  float start[3];
  float last[3];
  double last_time;
  int marks;
};
const int kTracks = 4;
Track g_tracks[kTracks] = {};
// The times after the start of a flight at which the log has its place.
const double kMarks[] = {0.25, 0.5, 1.0, 2.0, 4.0};
const int kMarkCount = sizeof(kMarks) / sizeof(kMarks[0]);
// A flight ends in the log when its atom has no update for this time, or
// after the longest time.
const double kGone = 0.5;
const double kLongest = 10.0;

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
  ModLog("miracle throw: the %s of frame %llu %s %.1f %.1f %.1f, %.2f s after "
         "it started, %.1f units (%.2f m of the room) across the land from "
         "its start and %.1f units up. Its path goes %.1f degrees to the %s "
         "of the velocity that the cast got, and %.1f degrees to the %s of "
         "the game's filter of the Hand",
         t.cast.name, t.cast.frame, what, at[0], at[1], at[2], seconds,
         across, across / units, path[1], fabsf(to_sent), SideWord(to_sent),
         fabsf(to_game), SideWord(to_game));
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
  return oldest;
}

void Follow(void* atom, const float at[3]) {
  const double now = Now();
  Track* t = TrackOf(atom);
  if (t == nullptr) {
    if (!g_cast.valid || now - g_cast.time > kCastWait) return;
    t = FreeTrack();
    memset(t, 0, sizeof(*t));
    t->active = true;
    t->atom = atom;
    t->cast = g_cast;
    g_cast.valid = false;
    t->t0 = now;
    for (int i = 0; i < 3; ++i) t->start[i] = t->last[i] = at[i];
    t->last_time = now;
    if (g_path_logs < kMaxPathLogs) {
      ++g_path_logs;
      const float units = t->cast.units > 0.0f ? t->cast.units : 1.0f;
      const float d[3] = {at[0] - t->cast.hand[0], at[1] - t->cast.hand[1],
                          at[2] - t->cast.hand[2]};
      ModLog("miracle throw: the %s of frame %llu starts its flight at %.1f "
             "%.1f %.1f, %.0f ms after the cast and %.1f cm of the room from "
             "the Hand's point at the cast",
             t->cast.name, t->cast.frame, at[0], at[1], at[2],
             (now - t->cast.time) * 1000.0, Speed(d) / units * 100.0f);
    }
    return;
  }
  for (int i = 0; i < 3; ++i) t->last[i] = at[i];
  t->last_time = now;
  while (t->marks < kMarkCount && now - t->t0 >= kMarks[t->marks]) {
    LogPlace(*t, at, now - t->t0, "is at");
    ++t->marks;
  }
}

bool __fastcall h_Modify(void* self, void* unused, void* collection) {
  if (g_installed && collection != nullptr &&
      !IsBadReadPtr(static_cast<uint8_t*>(collection) + kCollectionAtom,
                    sizeof(void*))) {
    uint8_t* atom = *reinterpret_cast<uint8_t* const*>(
        static_cast<uint8_t*>(collection) + kCollectionAtom);
    if (atom != nullptr && !IsBadReadPtr(atom + kAtomPlace, 3 * sizeof(float))) {
      const float* p = reinterpret_cast<const float*>(atom + kAtomPlace);
      const float at[3] = {p[0], p[1], p[2]};
      Follow(atom, at);
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

}  // namespace

void MiracleThrowInstall() {
  if (g_installed) return;
  g_mode = IniInt("miracle_throw", kThrowHand);
  if (g_mode < kThrowGame || g_mode > kThrowHandCurve) g_mode = kThrowHand;
  void* target = reinterpret_cast<void*>(kModifyFireBall);
  MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(&h_Modify),
                                   reinterpret_cast<void**>(&o_Modify));
  if (status == MH_OK) status = MH_EnableHook(target);
  ModLog("hooks        : the Fireball on its atom "
         "AttatchFireBallToAtom::ModifyAtomCollection at 0x%08X %s",
         static_cast<unsigned>(kModifyFireBall),
         status == MH_OK ? "ok" : "FAILED");
  g_installed = true;
  LogKey("the start");
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
    LogPlace(t, t.last, t.last_time - t.t0,
             gone ? "has no update now. Its last place is"
                  : "flies for the longest time of the log. It is at");
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
