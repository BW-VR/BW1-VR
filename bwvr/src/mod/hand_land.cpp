// The Hand stops at the land. See docs/hand-model.md, "The Hand stops at the
// land", and hand_land_math.h for the rule and the math.
//
// WHAT. Over the land the drawn hand is the real hand, one to one. When the
// real hand goes into the land, the drawn hand stays on the land and slides
// along it with the real hand. It comes back to the real hand when the real
// hand comes up. The land hides no part of the hand, and no faint copy of the
// real hand shows under the land.
//
// WHO USES THE LIFT. overlay.cpp draws each hand with its lift, and hand feel
// gets the lifted place. hand_direct.cpp lifts the Hand's point of the
// pointing hand by the same amount, so the glow, the pick, a take and a click
// act at the hand that the player sees. The grab of the land, the grab with
// two hands, the far grab and the throw keep the real hand: a grip that holds
// the land with the hand in it moves the land as before.
//
// THE LAND. LH3DIsland::GetAltitude at each point of the hand (at most 44 a
// hand), with the sea and the places off the map at 0, as hand feel has it.
// Only the land: a building, a rock and a tree do not stop the hand.
//
// THE FINGERS. With hand feel on the land (hand_feel_land=1) the fingers bend
// on the land, and only the palm and the knuckles stop the hand. With no
// such fingers each point of each finger stops it too (hand_land_math.h,
// HandPoints).
//
// ONCE A FRAME. The first call of a frame (hand_direct.cpp, before the
// overlay) makes the lift of both hands from the grip poses of this frame
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
// pulse. With hand_feel=0 this file sends it by the same rule.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "controllers.h"
#include "debug_menu.h"
#include "hand_feel.h"
#include "hand_land.h"
#include "hand_land_math.h"
#include "hand_model.h"
#include "haptics.h"
#include "overlay_math.h"

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

namespace {

namespace hl = hand_land;

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

bool g_loaded = false;
bool g_on = true;    // hand_land_stop
bool g_live = true;  // the switch of the debug page

struct HandState {
  bool have = false;
  float lift = 0.0f;        // world units
  float tip[3] = {0, 0, 0};  // the tip of the index finger of the real hand
  bool held = false;        // the land holds the hand up
  float quiet = 10.0f;      // seconds with no touch, for the pulse
  uint64_t held_frames = 0;
  float most_lift = 0.0f;
  float real_y = 0.0f;
};
HandState g_hand[kHands];
uint64_t g_frame = 0;  // the frame of the latch
int g_pointing = 1;
float g_units_per_meter = 0.0f;
int64_t g_last_ticks = 0;

// For the log and the report.
int g_logs = 0;
const int kMaxLogs = 80;
uint64_t g_frames_held = 0;
uint64_t g_pulses = 0;
uint64_t g_point_frames = 0;
uint64_t g_land_calls = 0;
bool g_said_point = false;

const char* const kPointName[] = {"a corner of the palm",
                                  "a joint of a finger",
                                  "the middle of a segment of a finger",
                                  "the tip of a finger", "the Hand's point"};

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
  }
  g_live = g_on;
  ModLog("hand land    : hand_land_stop=%d. %s", g_on ? 1 : 0,
         g_on ? "A drawn hand that the real hand pushes into the land stays "
                "on the land and slides along it, and the Hand's point goes "
                "with it. The grab of the land and the far grab keep the real "
                "hand"
              : "The drawn hands are the real hands also in the land, and "
                "only the Hand's point stops 5 mm over the land");
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

// The points of one hand in the world, with their gaps, and the kind of
// each point for the log. Gives the count.
int MakeSamples(int h, const OverlayHand& hand, float length, float units,
                bool fingers_bend, hl::Sample* out, int* kind) {
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
  const float skin = kSkinGapM * units / length;
  const int n =
      hl::HandPoints(joints, fingers_bend, skin, tip, local, gap, kind);
  for (int i = 0; i < n; ++i) {
    overlay_math::PlacePoint(hand.position, hand.rotation, length, left,
                             local[i], out[i].p);
    out[i].gap = gap[i] * length;
  }
  return n;
}

double Seconds(int64_t ticks) {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  return frequency.QuadPart > 0
             ? static_cast<double>(ticks) / static_cast<double>(frequency.QuadPart)
             : 0.0;
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
  int lowest_kind[kHands] = {-1, -1};
  OverlayHands hands = {};
  const bool acts = g_live && TabletopOwnsView() && ControllersOwnPointer() &&
                    ControllersOverlayHands(&hands) &&
                    hands.units_per_meter > 0.0f;
  // Fingers that bend on the land (hand feel) leave the stop to the palm and
  // the knuckles.
  const bool fingers_bend = HandFeelBendsOnLand();
  if (acts) {
    g_pointing = hands.pointing;
    g_units_per_meter = hands.units_per_meter;
    const float length = ControllersHandCm() * 0.01f *
                         ControllersHandScalePct() * 0.01f *
                         hands.units_per_meter;
    for (int h = 0; h < kHands && length > 0.0f; ++h) {
      const OverlayHand& hand = hands.hand[h];
      if (!hand.valid) continue;
      hl::Sample samples[hl::kMostSamples];
      int kind[hl::kMostSamples];
      const int n = MakeSamples(h, hand, length, hands.units_per_meter,
                                fingers_bend, samples, kind);
      int lowest = -1;
      lift[h] = hl::Lift(samples, n, Ground, &lowest);
      if (lowest >= 0) lowest_kind[h] = kind[lowest];
      valid[h] = true;
      g_hand[h].real_y = hand.position[1];
    }
  }
  const bool feel_pulses = HandFeelValue() != kDebugCannot;
  for (int h = 0; h < kHands; ++h) {
    HandState& s = g_hand[h];
    s.have = valid[h];
    s.lift = valid[h] ? lift[h] : 0.0f;
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
  }
}

}  // namespace

bool HandLandLift(int hand, float* lift) {
  *lift = 0.0f;
  if (hand < 0 || hand >= kHands) return false;
  Latch();
  if (!g_live || !g_hand[hand].have) return false;
  *lift = g_hand[hand].lift;
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

void HandLandReport() {
  LoadSettings();
  if (!g_on && !g_live) return;
  ModLog("hand land    : this window the land held a hand up on %llu hand "
         "frames, the Hand's point went up with it on %llu frames, %llu "
         "pulses (hand_feel=0 only), %llu land heights",
         g_frames_held, g_point_frames, g_pulses, g_land_calls);
  g_frames_held = 0;
  g_point_frames = 0;
  g_pulses = 0;
  g_land_calls = 0;
}
