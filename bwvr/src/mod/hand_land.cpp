// The Hand stops at the land and at the solid objects. See
// docs/hand-model.md, "The Hand stops at the land", hand_land_math.h and
// solid_math.h for the rules and the math.
//
// WHAT. Over the land and out of the solids the drawn hand is the real hand,
// one to one. When the real hand goes into the land, the drawn hand stays on
// the land and slides along it with the real hand. When the real hand goes
// into a house, a rock or a tree trunk, the drawn hand stays on its surface
// and slides along it. The drawn hand comes back to the real hand when the
// real hand comes out. The land and the solids hide no part of the hand, and
// no faint copy of the real hand shows in them.
//
// WHO USES THE OFFSET. overlay.cpp draws each hand at its offset, and hand
// feel gets the drawn place. hand_direct.cpp lifts the Hand's point of the
// pointing hand by the lift over the land, so the glow, the pick, a take and
// a click act at the hand that the player sees on the land. The solids do not
// move the Hand's point: it goes into a body to choose it, so a take of a
// rock acts at the real hand. The grab of the land, the grab with two hands,
// the far grab and the throw keep the real hand: a grip that holds the land
// with the hand in it moves the land as before.
//
// THE LAND. LH3DIsland::GetAltitude at each point of the hand (at most 44 a
// hand), with the sea and the places off the map at 0, as hand feel has it.
//
// THE SOLIDS (hand_object_stop). The solids near each hand that the first eye
// pass of the last frame kept (hand_direct.cpp, LatchSolids): the box of the
// mesh of a house and of a rock, and the trunk of a tree. Each point of the
// hand stops at a solid, also with the fingers that bend on the land: hand
// feel bends a finger only when it is in a body. The push comes first, and
// then the lift over the land of the pushed hand. A hand that is longer than
// kSolidsMaxLength world units (the table) has no solids: a finger is wider
// than a house there.
//
// THE FINGERS ON THE LAND. With hand feel on the land (hand_feel_land=1) the
// fingers bend on the land, and only the palm and the knuckles stop the hand
// at the land. With no such fingers each point of each finger stops it too
// (hand_land_math.h, HandPoints).
//
// ONCE A FRAME. The first call of a frame (hand_direct.cpp, before the
// overlay) makes the offset of both hands from the grip poses of this frame
// and the shape of the fingers of the last frame. Each later call of the
// frame gets the same numbers.
//
// WHEN. Only while the God view or the table owns the view and the
// controller owns the pointer, as hand feel. In a menu, in a room of the
// Temple and in a cut scene the hands are the real hands.
//
// THE PULSE. One faint pulse (the kind surface) when a hand first touches the
// land. With hand_feel=1 hand feel sends it, by its rule for a finger that
// meets the land, so a finger and the palm that touch at one time give one
// pulse. With hand_feel=0 this file sends it by the same rule. A solid has
// its own pulse by the same rule, from this file: the stop keeps the fingers
// out of the solid, so hand feel sees no finger in it.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "controllers.h"
#include "debug_menu.h"
#include "hand_feel.h"
#include "hand_land.h"
#include "hand_land_math.h"
#include "hand_model.h"
#include "haptics.h"
#include "overlay_math.h"
#include "solid_math.h"

extern void ModLog(const char* format, ...);

// controllers.cpp
uint64_t ControllersFrameCount();
int ControllersHandScalePct();
int ControllersHandCm();
void ControllersGesturePulse(float amplitude, float seconds);
bool ControllersOwnPointer();
// tabletop.cpp: the God view or the table owns the view.
bool TabletopOwnsView();
// overlay.cpp: the shape of the fingers of a hand as it was built last, and
// the tip of the index finger at the rest curl in the hand frame.
bool OverlayLastShape(int hand, float curls[overlay_math::kFingers],
                      float outer[overlay_math::kFingers],
                      float spread[overlay_math::kFingers], float* thumb_turn,
                      bool* mesh);
void OverlayIndexTipLocal(float out[3]);
// hand_feel.cpp: kDebugCannot with hand_feel=0, and true while the fingers
// bend on the land.
int HandFeelValue();
bool HandFeelBendsOnLand();
// hand_direct.cpp: the solids that the first eye pass of the last frame
// kept, the class of one for the log, and the counts for the report.
int HandDirectSolids(void* objects[], solid_math::Solid out[], int max);
const char* HandDirectSolidClass(void* object);
void HandDirectSolidCounts(uint64_t* latches, uint64_t* kept,
                           uint64_t* offers);

namespace {

namespace hl = hand_land;
namespace sm = solid_math;

const int kHands = 2;

// LH3DIsland::GetAltitude, fastcall on the map coordinates.
const uintptr_t kGetAltitude = 0x00803090;
const float kMapScale = 6553.6f;
const float kMapSize = 5120.0f;
typedef float(__fastcall* GetAltitudeFn)(const int32_t* coords);

// The gap of a corner of the skin over the land: the clearance of the Hand's
// point (hand_direct.cpp, kLandClearanceM).
const float kSkinGapM = 0.005f;
// The Hand's point is on the drawn hand when it is this near the tip of the
// index finger of the pointing hand.
const float kPointOnHandM = 0.02f;
// The middle of the palm in the hand frame (hand_feel.cpp, kPalm).
const float kPalm[3] = {overlay_math::kPalmX,
                        overlay_math::kKnuckleY + 0.20f, 0.0f};
// THE SOLIDS NEAR A HAND: a solid within this many hand lengths of the
// middle of the palm, and kSolidSlackM of the room more for the motion of one
// frame. Each point of the hand is within one hand length of the palm.
const float kSolidReach = 1.0f;
const float kSolidSlackM = 0.10f;
// Past this length of the hand, in world units, a finger is wider than a
// house (the table, hand_feel.cpp, kBodiesMaxLength). No solid counts.
const float kSolidsMaxLength = 60.0f;
// A touch of a solid starts only with a push of this many meters of the room
// or less. A real hand that a teleport or a zoom puts deeper into a solid is
// drawn as the real hand until it comes out (solid_math.h, Push).
const float kStartMostM = 0.10f;
// The log has a line when the push of a touch that holds changes by this
// many meters of the room.
const float kPushLogM = 0.01f;

bool g_loaded = false;
bool g_on = true;           // hand_land_stop
bool g_live = true;         // its switch of the debug page
bool g_objects_on = true;   // hand_object_stop
bool g_objects_live = true; // its switch of the debug page

// The touch of one solid, by the object, from frame to frame.
struct SolidTouch {
  void* object;
  sm::Touch touch;
};

struct HandState {
  bool have = false;
  float lift = 0.0f;         // the lift of the real hand over the land
  float offset[3] = {0, 0, 0};  // the drawn hand from the real hand
  float tip[3] = {0, 0, 0};  // the tip of the index finger of the real hand
  bool held = false;         // the land holds the hand up
  float quiet = 10.0f;       // seconds with no touch, for the pulse
  uint64_t held_frames = 0;
  float most_lift = 0.0f;
  float real_y = 0.0f;
  // The solids.
  int touches = 0;
  SolidTouch touch[sm::kMostSolids];
  bool solid_held = false;
  float solid_quiet = 10.0f;
  uint64_t solid_frames = 0;
  float most_push = 0.0f;
  float logged_push = 0.0f;
  // The middle of the palm of the real hand, and its bound for the solids.
  bool have_palm = false;
  float palm[3] = {0, 0, 0};
  float bound = 0.0f;
};
HandState g_hand[kHands];
uint64_t g_frame = 0;  // the frame of the latch
int g_pointing = 1;
float g_units_per_meter = 0.0f;
int64_t g_last_ticks = 0;

// For the log and the report.
int g_logs = 0;
const int kMaxLogs = 80;
int g_solid_logs = 0;
const int kMaxSolidLogs = 80;
uint64_t g_frames_held = 0;
uint64_t g_pulses = 0;
uint64_t g_point_frames = 0;
uint64_t g_land_calls = 0;
uint64_t g_solid_frames = 0;
uint64_t g_solid_pulses = 0;
uint64_t g_solids_near = 0;
bool g_said_point = false;

const char* const kPointName[] = {"a corner of the palm",
                                  "a joint of a finger",
                                  "the middle of a segment of a finger",
                                  "the tip of a finger", "the Hand's point"};
// The faces of a box, in the axes of the box (the turn of the object).
const char* const kFaceName[] = {"+1", "-1", "+2 (the top)", "-2", "+3",
                                 "-3"};

void LoadSettings() {
  if (g_loaded) return;
  g_loaded = true;
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash != nullptr && (slash - path) + 10 < MAX_PATH) {
    slash[1] = 0;
    strcat_s(path, "bwvr.ini");
    g_on = GetPrivateProfileIntA("bwvr", "hand_land_stop", 1, path) != 0;
    g_objects_on =
        GetPrivateProfileIntA("bwvr", "hand_object_stop", 1, path) != 0;
  }
  g_live = g_on;
  g_objects_live = g_objects_on;
  ModLog("hand land    : hand_land_stop=%d. %s", g_on ? 1 : 0,
         g_on ? "A drawn hand that the real hand pushes into the land stays "
                "on the land and slides along it, and the Hand's point goes "
                "with it. The grab of the land and the far grab keep the real "
                "hand"
              : "The drawn hands are the real hands also in the land, and "
                "only the Hand's point stops 5 mm over the land");
  ModLog("hand land    : hand_object_stop=%d. %s", g_objects_on ? 1 : 0,
         g_objects_on
             ? "A drawn hand that the real hand pushes into a house, a rock "
               "or a tree trunk stays on its surface and slides along it. "
               "The Hand's point keeps the real hand, so a take acts there"
             : "The drawn hands go into a house, a rock and a tree trunk");
}

// The height of the land, with the sea and the places off the map at 0.
float Ground(float x, float z) {
  ++g_land_calls;
  if (!(x >= 0.0f && x < kMapSize && z >= 0.0f && z < kMapSize)) return 0.0f;
  const int32_t coords[3] = {static_cast<int32_t>(x * kMapScale),
                             static_cast<int32_t>(z * kMapScale), 0};
  const float y = reinterpret_cast<GetAltitudeFn>(kGetAltitude)(coords);
  if (!(y > 0.0f && y < 5000.0f)) return 0.0f;
  return y;
}

// The points of one hand in the world, with their gaps: for the land (with
// fingers that bend, the palm and the knuckles only) and for the solids
// (each point), and the kind of each point of the land for the log. Gives the
// two counts.
void MakeSamples(int h, const OverlayHand& hand, float length, float units,
                 bool fingers_bend, hl::Sample* land, int* land_count,
                 int* kind, hl::Sample* solid, int* solid_count) {
  const bool left = h == 0;
  float curls[overlay_math::kFingers], outer[overlay_math::kFingers],
      spread[overlay_math::kFingers];
  float thumb_turn = 0.0f;
  bool mesh = false;
  if (!OverlayLastShape(h, curls, outer, spread, &thumb_turn, &mesh)) {
    for (int f = 0; f < overlay_math::kFingers; ++f) {
      curls[f] = overlay_math::kRestCurl;
      outer[f] = spread[f] = 0.0f;
    }
    mesh = false;
  }
  float joints[overlay_math::kFingers][overlay_math::kJoints + 1][3];
  for (int f = 0; f < overlay_math::kFingers; ++f) {
    if (!mesh || !HandModelFingerJointsPose(f, curls[f], outer[f], spread[f],
                                            thumb_turn, joints[f])) {
      overlay_math::FingerJoints(f, curls[f], joints[f]);
    }
  }
  // The Hand's point: the tip of the index finger at the rest curl, which
  // hand_direct.cpp keeps 5 mm over the land.
  float tip[3];
  OverlayIndexTipLocal(tip);
  overlay_math::PlacePoint(hand.position, hand.rotation, length, left, tip,
                           g_hand[h].tip);
  float local[hl::kMostSamples][3], gap[hl::kMostSamples];
  int all_kind[hl::kMostSamples];
  const float skin = kSkinGapM * units / length;
  const int n =
      hl::HandPoints(joints, fingers_bend, skin, tip, local, gap, kind);
  for (int i = 0; i < n; ++i) {
    overlay_math::PlacePoint(hand.position, hand.rotation, length, left,
                             local[i], land[i].p);
    land[i].gap = gap[i] * length;
  }
  *land_count = n;
  *solid_count = 0;
  if (solid == nullptr) return;
  const int m = hl::HandPoints(joints, false, skin, tip, local, gap, all_kind);
  for (int i = 0; i < m; ++i) {
    overlay_math::PlacePoint(hand.position, hand.rotation, length, left,
                             local[i], solid[i].p);
    solid[i].gap = gap[i] * length;
  }
  *solid_count = m;
}

double Seconds(int64_t ticks) {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  return frequency.QuadPart > 0
             ? static_cast<double>(ticks) / static_cast<double>(frequency.QuadPart)
             : 0.0;
}

// THE PUSH OUT OF THE SOLIDS of one hand: the touches of the last frame go
// to the solids of this frame by the object, and the touches of this frame
// stay for the next one. Gives the index of the first solid that holds the
// hand, or -1.
int PushOut(HandState* s, const sm::Solid* solids, void* const* objects,
            int count, const hl::Sample* points, int point_count,
            const float palm[3], float start_most, float offset[3]) {
  sm::Touch touches[sm::kMostSolids];
  for (int i = 0; i < count; ++i) {
    touches[i].on = false;
    for (int t = 0; t < s->touches; ++t) {
      if (s->touch[t].object == objects[i]) touches[i] = s->touch[t].touch;
    }
  }
  bool held[sm::kMostSolids];
  sm::Offset(solids, count, points, point_count, palm, start_most, touches,
             offset, held);
  s->touches = 0;
  int first = -1;
  for (int i = 0; i < count; ++i) {
    if (!held[i]) continue;
    if (first < 0) first = i;
    s->touch[s->touches].object = objects[i];
    s->touch[s->touches].touch = touches[i];
    ++s->touches;
  }
  return first;
}

void Latch() {
  LoadSettings();
  const uint64_t frame = ControllersFrameCount();
  if (frame == g_frame && frame != 0) return;
  g_frame = frame;
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  float dt = g_last_ticks != 0
                 ? static_cast<float>(Seconds(now.QuadPart - g_last_ticks))
                 : 0.0f;
  g_last_ticks = now.QuadPart;
  if (!(dt > 0.0f)) dt = 0.0f;
  if (dt > 0.05f) dt = 0.05f;
  bool valid[kHands] = {false, false};
  float lift[kHands] = {0.0f, 0.0f};
  float offset[kHands][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
  int lowest_kind[kHands] = {-1, -1};
  int first_solid[kHands] = {-1, -1};
  float solid_push[kHands] = {0.0f, 0.0f};
  OverlayHands hands = {};
  const bool acts = (g_live || g_objects_live) && TabletopOwnsView() &&
                    ControllersOwnPointer() &&
                    ControllersOverlayHands(&hands) &&
                    hands.units_per_meter > 0.0f;
  // Fingers that bend on the land (hand feel) leave the stop at the land to
  // the palm and the knuckles.
  const bool fingers_bend = HandFeelBendsOnLand();
  static void* objects[sm::kMostSolids];
  static sm::Solid solids[sm::kMostSolids];
  int solid_count = 0;
  float length = 0.0f;
  if (acts) {
    g_pointing = hands.pointing;
    g_units_per_meter = hands.units_per_meter;
    length = ControllersHandCm() * 0.01f * ControllersHandScalePct() * 0.01f *
             hands.units_per_meter;
    if (g_objects_live && length > 0.0f && length <= kSolidsMaxLength) {
      solid_count = HandDirectSolids(objects, solids, sm::kMostSolids);
    }
    g_solids_near += static_cast<uint64_t>(solid_count);
    for (int h = 0; h < kHands && length > 0.0f; ++h) {
      const OverlayHand& hand = hands.hand[h];
      HandState& s = g_hand[h];
      s.have_palm = false;
      if (!hand.valid) continue;
      hl::Sample samples[hl::kMostSamples], all[hl::kMostSamples];
      int kind[hl::kMostSamples];
      int n = 0, m = 0;
      MakeSamples(h, hand, length, hands.units_per_meter, fingers_bend,
                  samples, &n, kind, g_objects_live ? all : nullptr, &m);
      overlay_math::PlacePoint(hand.position, hand.rotation, length, h == 0,
                               kPalm, s.palm);
      s.bound = kSolidReach * length + kSolidSlackM * hands.units_per_meter;
      s.have_palm = g_objects_live && length <= kSolidsMaxLength;
      if (solid_count > 0) {
        first_solid[h] = PushOut(&s, solids, objects, solid_count, all, m,
                                 s.palm, kStartMostM * hands.units_per_meter,
                                 offset[h]);
        const float* o = offset[h];
        solid_push[h] = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
      } else {
        s.touches = 0;
      }
      if (g_live) {
        int lowest = -1;
        lift[h] = hl::Lift(samples, n, Ground, &lowest);
        if (lowest >= 0) lowest_kind[h] = kind[lowest];
        // The lift of the pushed hand. With no push it is the lift of the
        // real hand.
        if (first_solid[h] >= 0) {
          for (int i = 0; i < n; ++i) {
            for (int k = 0; k < 3; ++k) samples[i].p[k] += offset[h][k];
          }
          const float pushed = hl::Lift(samples, n, Ground);
          offset[h][1] += pushed;
        } else {
          offset[h][1] += lift[h];
        }
      }
      valid[h] = true;
      s.real_y = hand.position[1];
    }
  } else {
    for (int h = 0; h < kHands; ++h) {
      g_hand[h].touches = 0;
      g_hand[h].have_palm = false;
    }
  }
  const bool feel_pulses = HandFeelValue() != kDebugCannot;
  for (int h = 0; h < kHands; ++h) {
    HandState& s = g_hand[h];
    s.have = valid[h];
    s.lift = valid[h] ? lift[h] : 0.0f;
    for (int k = 0; k < 3; ++k) s.offset[k] = valid[h] ? offset[h][k] : 0.0f;
    const bool held = s.lift > 0.0f;
    const float cm =
        g_units_per_meter > 0.0f ? s.lift / g_units_per_meter * 100.0f : 0.0f;
    if (held) {
      ++s.held_frames;
      ++g_frames_held;
      if (s.lift > s.most_lift) s.most_lift = s.lift;
    }
    if (held != s.held && g_logs < kMaxLogs) {
      ++g_logs;
      if (held) {
        ModLog("hand land    : the %s hand touches the land, frame %llu. The "
               "land holds the drawn hand %.2f cm of the room over the real "
               "hand, at %s",
               h == 0 ? "left" : "right", g_frame, cm,
               lowest_kind[h] >= 0 ? kPointName[lowest_kind[h]] : "no point");
      } else {
        ModLog("hand land    : the %s hand is free of the land again, frame "
               "%llu, after %llu frames. The land held it up by %.2f cm of "
               "the room at the most, and the drawn hand is the real hand "
               "again",
               h == 0 ? "left" : "right", g_frame, s.held_frames,
               g_units_per_meter > 0.0f
                   ? s.most_lift / g_units_per_meter * 100.0f
                   : 0.0f);
      }
    }
    if (held && !s.held) {
      s.held_frames = 0;
      s.most_lift = 0.0f;
    }
    s.held = held;
    // The pulse of hand_feel=0. With hand feel on, hand feel sends it.
    if (hl::TouchPulse(held, dt, &s.quiet) && !feel_pulses) {
      ++g_pulses;
      const int mask = h == g_pointing ? 2 : 1;
      HapticsPulseHandsClear(mask, haptics_math::kSurface);
    }
    // THE SOLIDS: the log of a touch and of its end, and the pulse.
    const bool solid_held = valid[h] && first_solid[h] >= 0;
    const float push = solid_held ? solid_push[h] : 0.0f;
    if (solid_held) {
      ++s.solid_frames;
      ++g_solid_frames;
      if (push > s.most_push) s.most_push = push;
    }
    const float units = g_units_per_meter > 0.0f ? g_units_per_meter : 1.0f;
    if (solid_held != s.solid_held && g_solid_logs < kMaxSolidLogs) {
      ++g_solid_logs;
      if (solid_held) {
        const sm::Solid& solid = solids[first_solid[h]];
        const sm::Touch& t = s.touch[0].touch;
        char where[64];
        if (solid.kind == sm::kBox) {
          sprintf_s(where, "the face %s of its box (axis 1 %.2f %.2f %.2f)",
                    kFaceName[t.face], solid.axis[0], solid.axis[1],
                    solid.axis[2]);
        } else {
          sprintf_s(where, "its trunk");
        }
        ModLog("hand land    : the %s hand touches the %s %p at %s, frame "
               "%llu. The solid holds the drawn hand %.2f cm of the room out "
               "of it, along %.2f %.2f %.2f in the world. The palm of the "
               "real hand is at %.1f %.1f %.1f, the middle of the solid at "
               "%.1f %.1f %.1f. %d solids are near",
               h == 0 ? "left" : "right",
               HandDirectSolidClass(objects[first_solid[h]]),
               objects[first_solid[h]], where, g_frame, push / units * 100.0f,
               t.dir[0], t.dir[1], t.dir[2], s.palm[0], s.palm[1], s.palm[2],
               solid.middle[0], solid.middle[1], solid.middle[2], solid_count);
        s.logged_push = push;
      } else {
        ModLog("hand land    : the %s hand is free of the solids again, frame "
               "%llu, after %llu frames. A solid held it out by %.2f cm of "
               "the room at the most, and the drawn hand is the real hand "
               "again%s",
               h == 0 ? "left" : "right", g_frame, s.solid_frames,
               s.most_push / units * 100.0f,
               held ? " over the land" : "");
      }
    }
    // A push that changes by kPushLogM or more while the touch holds.
    if (solid_held && s.solid_held &&
        fabsf(push - s.logged_push) >= kPushLogM * units &&
        g_solid_logs < kMaxSolidLogs) {
      ++g_solid_logs;
      ModLog("hand land    : the %s hand stays on the solid, frame %llu. The "
             "solid holds the drawn hand %.2f cm of the room out of it now "
             "(%.2f cm before). The palm of the real hand is at %.1f %.1f "
             "%.1f",
             h == 0 ? "left" : "right", g_frame, push / units * 100.0f,
             s.logged_push / units * 100.0f, s.palm[0], s.palm[1], s.palm[2]);
      s.logged_push = push;
    }
    if (solid_held && !s.solid_held) {
      s.solid_frames = 1;
      s.most_push = push;
    }
    s.solid_held = solid_held;
    if (hl::TouchPulse(solid_held, dt, &s.solid_quiet)) {
      ++g_solid_pulses;
      const int mask = h == g_pointing ? 2 : 1;
      HapticsPulseHandsClear(mask, haptics_math::kSurface);
    }
  }
}

}  // namespace

bool HandLandOffset(int hand, float offset[3]) {
  for (int k = 0; k < 3; ++k) offset[k] = 0.0f;
  if (hand < 0 || hand >= kHands) return false;
  Latch();
  if ((!g_live && !g_objects_live) || !g_hand[hand].have) return false;
  for (int k = 0; k < 3; ++k) offset[k] = g_hand[hand].offset[k];
  return true;
}

bool HandLandPointLift(const float point[3], float* lift) {
  *lift = 0.0f;
  Latch();
  const HandState& s = g_hand[g_pointing];
  if (!g_live || !s.have || !(s.lift > 0.0f)) return false;
  if (!hl::PointOnHand(point, s.tip, g_units_per_meter, kPointOnHandM)) {
    return false;
  }
  *lift = s.lift;
  ++g_point_frames;
  if (!g_said_point) {
    g_said_point = true;
    ModLog("hand land    : the Hand's point goes up with the drawn hand, %.2f "
           "cm of the room, frame %llu", s.lift / g_units_per_meter * 100.0f,
           g_frame);
  }
  return true;
}

bool HandLandHeld(int hand) {
  if (hand < 0 || hand >= kHands) return false;
  Latch();
  return g_live && g_hand[hand].have && g_hand[hand].held;
}

int HandLandSolidPoints(float points[][3], float bound[]) {
  LoadSettings();
  int n = 0;
  if (!g_objects_live) return 0;
  for (int h = 0; h < kHands; ++h) {
    const HandState& s = g_hand[h];
    if (!s.have_palm) continue;
    for (int k = 0; k < 3; ++k) points[n][k] = s.palm[k];
    bound[n] = s.bound;
    ++n;
  }
  return n;
}

int HandLandStopValue() {
  LoadSettings();
  return g_live ? 1 : 0;
}

void HandLandStopChange(const char* by) {
  LoadSettings();
  g_live = !g_live;
  ControllersGesturePulse(0.5f, g_live ? 0.30f : 0.08f);
  ModLog("hand land    : %s: %s, frame %llu", by,
         g_live ? "the drawn hands stop at the land"
                : "the drawn hands are the real hands also in the land",
         g_frame);
}

int HandObjectStopValue() {
  LoadSettings();
  return g_objects_live ? 1 : 0;
}

void HandObjectStopChange(const char* by) {
  LoadSettings();
  g_objects_live = !g_objects_live;
  if (!g_objects_live) {
    for (int h = 0; h < kHands; ++h) g_hand[h].touches = 0;
  }
  ControllersGesturePulse(0.5f, g_objects_live ? 0.30f : 0.08f);
  ModLog("hand land    : %s: %s, frame %llu", by,
         g_objects_live
             ? "the drawn hands stop at a house, a rock and a tree trunk"
             : "the drawn hands go into a house, a rock and a tree trunk",
         g_frame);
}

void HandLandReport() {
  LoadSettings();
  uint64_t latches = 0, kept = 0, offers = 0;
  HandDirectSolidCounts(&latches, &kept, &offers);
  if (!g_on && !g_live && !g_objects_on && !g_objects_live) return;
  ModLog("hand land    : this window the land held a hand up on %llu hand "
         "frames, the Hand's point went up with it on %llu frames, %llu "
         "pulses (hand_feel=0 only), %llu land heights",
         g_frames_held, g_point_frames, g_pulses, g_land_calls);
  if (g_objects_on || g_objects_live) {
    ModLog("hand land    : this window a solid held a hand out on %llu hand "
           "frames, with %llu pulses. The first eye passes kept the solids "
           "%llu times, %llu solids in all, and %llu solids were near the "
           "hands in the frames of the stop. %llu objects went to the pick "
           "for a hand or the head only",
           g_solid_frames, g_solid_pulses, latches, kept, g_solids_near,
           offers);
  }
  g_frames_held = 0;
  g_point_frames = 0;
  g_pulses = 0;
  g_land_calls = 0;
  g_solid_frames = 0;
  g_solid_pulses = 0;
  g_solids_near = 0;
}
