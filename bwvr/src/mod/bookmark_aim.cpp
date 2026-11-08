// The aim of a new bookmark. See bookmark_aim.h and docs/controllers.md,
// "The sheet of the places".
//
// WHAT THE GAME DOES [V, runblack.exe 1.42]. Ctrl and a number key 1 to 7
// call Bookmark::SetBookMark 0x00439E50 (cdecl: the bookmark, a MapCoords*).
// When the pick record of the interface has an object (GInterface+0x400), it
// calls fn_00439F70 (cdecl: the bookmark, the object) in its place, and the
// bookmark holds that object. Else the bookmark holds the map point. Both
// end a follow first (fn_0043A390 with -1), and both keep the offset of the
// game camera from the point. The 7 records are at 0x00C58B30, 0x24 bytes
// each: bit 0 of +4 is set while the bookmark is set, +8 is the map point (x
// and z in 1/6553.6 of a unit, then a height over the land as a float), +0x14
// is the object or null. Bookmark::ProcessAll 0x00439DD0 copies the map point
// of the object of each bookmark into the record in each frame, and it drops
// an object that the game deleted. So the key of the bookmark (fn_0043A070)
// goes to the place where the object is at that time.
//
// WHAT THE MOD DOES. The game sets a bookmark at the Hand, and a player in
// the God view cannot put the Hand on a far place. So the seal with the plus
// on the sheet of the places starts an aim: the pointing hand shows its ray
// (ControllersMenuRay), an object in a tight cone of the ray gets the gold
// halo, and else the land point of the ray gets a gold ring. A press of the
// grip calls the game's own function for that target, on the game thread at
// the pump, behind the gates of the bookmark keys (radial_menu.cpp,
// RadialMenuMarks). For a land point the mod clears the pick record of the
// interface for the call and puts it back, so the game takes the land point
// and not the object under the Hand. Nothing follows a bookmark: the follow of
// the game (Shift and the number) is not on the ring.
//
// While the aim is on, the grip belongs to it: no grab of the world, no far
// grab and no press of the game. B of the pointing hand, the menu button and
// the ring stop the aim.
//
// THE KEYS of bwvr.ini:
//
//   bookmark_aim=1        1 the seal with the plus starts the aim. 0 it sets
//                         the bookmark at the Hand, as Ctrl and the number do
//   bookmark_test_aim     a test key: the word "aimmark" of the scripted
//                         controller aims at "land" (the land in front) or
//                         "living" (the nearest living thing of the cone)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bookmark_aim.h"
#include "controller_hints.h"
#include "controller_math.h"
#include "creature_command_math.h"
#include "far_grab.h"
#include "haptics.h"
#include "radial_menu.h"
#include "teleport.h"

extern void ModLog(const char* format, ...);

uint64_t ControllersFrameCount();
bool ControllersFarRay(float origin[3], float direction[3], float* units,
                       float eye[3]);
// hand_direct.cpp: the objects that the draw offered in the cone of the aim
// ray, their size, whether the land hides them, and whether they live.
int HandDirectConeCount();
bool HandDirectConeAt(int index, void** object, float position[3]);
float HandDirectRadius(void* object);
bool HandDirectSeen(const float at[3], float radius);
bool HandDirectLiving(void* object);
const char* HandDirectClass(void* object);

namespace {

const uintptr_t kGame = 0x00D0195C;
// GGame::MyInterface, thiscall, no arguments.
const uintptr_t kMyInterface = 0x00555850;
// GInterface: the object of the pick record.
const int kFaceObject = 0x400;
// Bookmark::SetBookMark and the bookmark on an object, both cdecl.
const uintptr_t kSetMark = 0x00439E50;
const uintptr_t kSetMarkObject = 0x00439F70;
// A world unit in map units: a MapCoords has x and z in 1/6553.6 of a unit.
const float kMapPerUnit = 6553.6f;
const float kMapSize = 5120.0f;

typedef void*(__fastcall* MyInterfaceFn)(void* self, void* unused);
typedef void(__cdecl* SetMarkFn)(uint32_t mark, const void* coords);
typedef void(__cdecl* SetMarkObjectFn)(uint32_t mark, void* object);

struct MapCoords {
  int32_t x;
  int32_t z;
  float height;
};

// The aim: an object in a cone this wide past its own size, and the land up
// to this far along the ray, in meters of the room.
const float kConeDeg = 1.5f;
// THE HOLD on an object: a player cannot keep a ray on a villager that
// walks. So the object of the last frame stays the target while the ray is
// within kKeepSize of its size and kKeepDeg round it, and for kKeepFrames
// frames in which the draw does not offer it.
const float kKeepSize = 1.6f;
const float kKeepDeg = 4.0f;
const int kKeepFrames = 10;
const float kLandMostM = 60.0f;
const float kLandStepM = 0.05f;
const float kRingCm = 10.0f;

enum TargetKind { kTargetNone, kTargetLand, kTargetObject };

struct Target {
  int kind;
  void* object;
  void* vtable;
  float point[3];    // the land point, or the middle of the object, in the world
  float base[3];     // the land under the object
  float radius;      // of the object, in world units
  float meters;      // along the ray, in meters of the room
  float units;       // world units in a meter of the room
  const char* name;  // the class of the object, for the log
};

bool g_read = false;
bool g_on = true;
char g_test_aim[16] = "";
bool g_active = false;
int g_mark = -1;
Target g_target = {};
int g_last_kind = kTargetNone;
void* g_last_object = nullptr;
Target g_kept = {};
int g_kept_misses = 0;
bool g_grip_before = false;
bool g_need_up = false;    // the grip was down at the start of the aim
bool g_hold = false;       // the grip that set the bookmark is still down
uint64_t g_starts = 0, g_sets = 0, g_misses = 0, g_stops = 0;

bool IniPath(char path[MAX_PATH]) {
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return false;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr) return false;
  slash[1] = 0;
  if (strlen(path) + 9 >= MAX_PATH) return false;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return true;
}

void Read() {
  if (g_read) return;
  g_read = true;
  char path[MAX_PATH];
  if (!IniPath(path)) return;
  g_on = GetPrivateProfileIntA("bwvr", "bookmark_aim", 1, path) != 0;
  GetPrivateProfileStringA("bwvr", "bookmark_test_aim", "", g_test_aim,
                           sizeof(g_test_aim), path);
  ModLog("bookmark aim : bookmark_aim=%d: %s", g_on ? 1 : 0,
         g_on ? "the seal with the plus on the sheet of the places waits for "
                "the grip at the point or the object of the aim ray"
              : "the seal with the plus sets the bookmark at the Hand");
}

void* Interface() {
  void* const* slot = reinterpret_cast<void* const*>(kGame);
  if (IsBadReadPtr(slot, sizeof(void*)) || *slot == nullptr) return nullptr;
  return reinterpret_cast<MyInterfaceFn>(kMyInterface)(*slot, nullptr);
}

float Ground(float x, float z) { return TeleportGround(x, z); }

// The target of the aim ray now: the object that it is on, or its land point.
void Aim() {
  const Target last = g_target;
  g_target = Target{};
  float origin[3], direction[3], eye[3], units = 0.0f;
  if (!ControllersFarRay(origin, direction, &units, eye) || !(units > 0.0f)) {
    return;
  }
  g_target.units = units;
  float step = kLandStepM * units;
  if (step < 1.0f) step = 1.0f;
  const float land = command_math::RayLandDistance(
      origin, direction, step, kLandMostM * units,
      [](float x, float z) { return Ground(x, z); });
  // THE OBJECT: the nearest to the ray in its size and a cone of kConeDeg,
  // and not behind the land of the ray.
  const float cone = tanf(kConeDeg * 0.01745329f);
  const float keep_cone = tanf(kKeepDeg * 0.01745329f);
  float best = 1e30f;
  bool last_seen = false;
  for (int i = 0; i < HandDirectConeCount(); ++i) {
    void* object = nullptr;
    float at[3];
    if (!HandDirectConeAt(i, &object, at) || object == nullptr) continue;
    const float radius = HandDirectRadius(object);
    if (!(radius > 0.0f)) continue;
    const float middle[3] = {at[0], at[1] + 0.7f * radius, at[2]};
    const float v[3] = {middle[0] - origin[0], middle[1] - origin[1],
                        middle[2] - origin[2]};
    const float along =
        v[0] * direction[0] + v[1] * direction[1] + v[2] * direction[2];
    if (!(along > 0.3f * units)) continue;
    const bool is_last = last.kind == kTargetObject && object == last.object;
    if (land >= 0.0f && along > land + (is_last ? 2.0f : 1.0f) * radius) {
      continue;
    }
    float off[3];
    for (int k = 0; k < 3; ++k) off[k] = v[k] - along * direction[k];
    const float across = controller_math::Length(off);
    const float allowed = is_last ? kKeepSize * radius + along * keep_cone
                                  : 0.9f * radius + along * cone;
    if (across > allowed) continue;
    if (is_last) last_seen = true;
    // The object of the last frame wins over a new one.
    const float score = across / allowed + along / (200.0f * units) -
                        (is_last ? 1.0f : 0.0f);
    if (score >= best || !HandDirectSeen(at, radius)) continue;
    best = score;
    g_target.kind = kTargetObject;
    g_target.object = object;
    g_target.vtable = *reinterpret_cast<void* const*>(object);
    g_target.radius = radius;
    g_target.meters = along / units;
    g_target.name = HandDirectClass(object);
    for (int k = 0; k < 3; ++k) {
      g_target.point[k] = middle[k];
      g_target.base[k] = at[k];
    }
  }
  // The object of the last frame that the draw did not offer stays for a
  // few frames, while it lives.
  if (g_target.kind != kTargetObject && last.kind == kTargetObject &&
      !last_seen && g_kept_misses < kKeepFrames &&
      !IsBadReadPtr(last.object, 0x20) &&
      *reinterpret_cast<void* const*>(last.object) == last.vtable) {
    ++g_kept_misses;
    g_target = last;
    return;
  }
  if (g_target.kind == kTargetObject) g_kept_misses = 0;
  if (g_target.kind == kTargetObject || !(land > 0.0f)) return;
  float point[3];
  for (int k = 0; k < 3; ++k) point[k] = origin[k] + direction[k] * land;
  if (!(point[0] >= 0.0f && point[0] < kMapSize && point[2] >= 0.0f &&
        point[2] < kMapSize)) {
    return;
  }
  g_target.kind = kTargetLand;
  g_target.meters = land / units;
  for (int k = 0; k < 3; ++k) {
    g_target.point[k] = point[k];
    g_target.base[k] = point[k];
  }
}

void Stop(const char* why) {
  if (!g_active) return;
  g_active = false;
  g_target = Target{};
  ++g_stops;
  ModLog("bookmark aim : the aim of bookmark %d stops: %s, frame %llu",
         g_mark + 1, why, ControllersFrameCount());
}

// THE SET, on the game thread at the pump.
bool Set() {
  const uint64_t frame = ControllersFrameCount();
  bool set[7];
  const wchar_t* note = nullptr;
  if (!RadialMenuMarks(set, &note)) {
    ModLog("bookmark aim : the grip does not set bookmark %d: the game takes "
           "no bookmark now, frame %llu", g_mark + 1, frame);
    return false;
  }
  if (g_target.kind == kTargetObject) {
    void* object = g_target.object;
    if (IsBadReadPtr(object, 0x20) ||
        *reinterpret_cast<void* const*>(object) != g_target.vtable) {
      ModLog("bookmark aim : the grip does not set bookmark %d: the object "
             "of the aim is gone, frame %llu", g_mark + 1, frame);
      return false;
    }
    reinterpret_cast<SetMarkObjectFn>(kSetMarkObject)(
        static_cast<uint32_t>(g_mark), object);
    ModLog("bookmark aim : the grip sets bookmark %d on the %s %p at %.1f "
           "%.1f (fn_00439F70), %.1f m of the room along the ray, frame "
           "%llu", g_mark + 1, g_target.name != nullptr ? g_target.name : "?",
           object, g_target.base[0], g_target.base[2], g_target.meters,
           frame);
    return true;
  }
  if (g_target.kind != kTargetLand) {
    ModLog("bookmark aim : the grip does not set bookmark %d: the ray is on "
           "no land and on no object, frame %llu", g_mark + 1, frame);
    return false;
  }
  uint8_t* face = static_cast<uint8_t*>(Interface());
  if (face == nullptr || IsBadReadPtr(face + kFaceObject, 4)) return false;
  // The game takes the object of the pick record over the land point. That
  // object is under the Hand, not under the ray, so it is out for the call.
  void** pick = reinterpret_cast<void**>(face + kFaceObject);
  void* const kept = *pick;
  const MapCoords at = {static_cast<int32_t>(g_target.point[0] * kMapPerUnit),
                        static_cast<int32_t>(g_target.point[2] * kMapPerUnit),
                        0.0f};
  *pick = nullptr;
  reinterpret_cast<SetMarkFn>(kSetMark)(static_cast<uint32_t>(g_mark), &at);
  *pick = kept;
  ModLog("bookmark aim : the grip sets bookmark %d on the land at %.1f %.1f "
         "(Bookmark::SetBookMark), %.1f m of the room along the ray, frame "
         "%llu", g_mark + 1, g_target.point[0], g_target.point[2],
         g_target.meters, frame);
  return true;
}

}  // namespace

bool BookmarkAimOn() {
  Read();
  return g_on;
}

bool BookmarkAimStart(int mark) {
  Read();
  if (!g_on || mark < 0 || mark >= 7) return false;
  g_active = true;
  g_mark = mark;
  g_target = Target{};
  g_last_kind = kTargetNone;
  g_last_object = nullptr;
  g_need_up = true;
  ++g_starts;
  ModLog("bookmark aim : the aim of bookmark %d starts. The pointing hand "
         "shows its ray, the grip sets the bookmark at its target, and B or "
         "the menu button stops it, frame %llu", mark + 1,
         ControllersFrameCount());
  return true;
}

bool BookmarkAimActive() { return g_active; }

void BookmarkAimFrame() {
  if (!g_active) return;
  bool set[7];
  const wchar_t* note = nullptr;
  if (!RadialMenuMarks(set, &note)) {
    Stop("the game takes no bookmark now");
    return;
  }
  Aim();
  if (g_target.kind != g_last_kind ||
      (g_target.kind == kTargetObject && g_target.object != g_last_object)) {
    // A light tick for a new object in the aim, as for the far grab.
    if (g_target.kind == kTargetObject) {
      HapticsPulseHands(2, haptics_math::kTick, 0.5f);
      ModLog("bookmark aim : the aim is on the %s %p, frame %llu",
             g_target.name != nullptr ? g_target.name : "?", g_target.object,
             ControllersFrameCount());
    } else if (g_target.kind == kTargetLand) {
      ModLog("bookmark aim : the aim is on the land at %.1f %.1f, frame %llu",
             g_target.point[0], g_target.point[2], ControllersFrameCount());
    }
    g_last_kind = g_target.kind;
    g_last_object = g_target.object;
  }
}

bool BookmarkAimWantsCone() { return g_active; }

bool BookmarkAimPumpGrip(bool grip) {
  const bool press = grip && !g_grip_before;
  g_grip_before = grip;
  if (g_hold) {
    if (!grip) g_hold = false;
    return true;
  }
  if (!g_active) return false;
  if (!grip) g_need_up = false;
  if (!press || g_need_up) return true;
  if (Set()) {
    ++g_sets;
    g_active = false;
    g_target = Target{};
    g_hold = true;
    HapticsPulseHands(2, haptics_math::kActivate);
  } else {
    ++g_misses;
    HapticsPulseHands(2, haptics_math::kRefused);
  }
  return true;
}

bool BookmarkAimStopBy(const char* why) {
  if (!g_active) return false;
  Stop(why);
  return true;
}

bool BookmarkAimRay(float* meters) {
  if (!g_active) return false;
  *meters = g_target.kind != kTargetNone ? g_target.meters : 0.0f;
  return true;
}

bool BookmarkAimRing(TeleportDraw* out) {
  if (!g_active || g_target.kind != kTargetLand) return false;
  memset(out, 0, sizeof(*out));
  out->active = true;
  out->valid = true;
  out->ring = true;
  for (int k = 0; k < 3; ++k) out->target[k] = g_target.point[k];
  out->units_per_meter = g_target.units;
  out->ring_cm = kRingCm;
  return true;
}

bool BookmarkAimHalo(FarDraw* out) {
  if (!g_active || g_target.kind != kTargetObject) return false;
  memset(out, 0, sizeof(*out));
  out->active = true;
  out->valid = true;
  for (int k = 0; k < 3; ++k) out->object[k] = g_target.base[k];
  out->radius = g_target.radius;
  out->land[0] = g_target.base[0];
  out->land[1] = g_target.base[2];
  out->units_per_meter = g_target.units;
  out->target = g_target.object;
  return true;
}

int BookmarkAimHint(MiracleLine lines[kMiracleLines]) {
  if (!g_active) return 0;
  lines[0].word = kWordGrab;
  lines[0].text = L"Set a bookmark";
  lines[1].word = kWordGesture;
  lines[1].text = L"Stop";
  return 2;
}

bool BookmarkAimTestPoint(float out[3]) {
  Read();
  if (!g_active) return false;
  if (strcmp(g_test_aim, "living") != 0) return false;
  float origin[3], direction[3], eye[3], units = 0.0f;
  if (!ControllersFarRay(origin, direction, &units, eye) || !(units > 0.0f)) {
    return false;
  }
  float best = 1e30f;
  bool found = false;
  for (int i = 0; i < HandDirectConeCount(); ++i) {
    void* object = nullptr;
    float at[3];
    if (!HandDirectConeAt(i, &object, at) || object == nullptr ||
        !HandDirectLiving(object)) {
      continue;
    }
    const float radius = HandDirectRadius(object);
    const float d = sqrtf((at[0] - origin[0]) * (at[0] - origin[0]) +
                          (at[2] - origin[2]) * (at[2] - origin[2]));
    if (d / units < 0.3f || d >= best || !HandDirectSeen(at, radius)) {
      continue;
    }
    best = d;
    found = true;
    out[0] = at[0];
    out[1] = at[1] + 0.7f * radius;
    out[2] = at[2];
  }
  ModLog("bookmark aim : test: aimmark %s, frame %llu",
         found ? "aims at the nearest living thing of the cone"
               : "found no living thing in the cone",
         ControllersFrameCount());
  return found;
}

void BookmarkAimReport() {
  if (g_starts + g_sets + g_misses + g_stops == 0) return;
  ModLog("bookmark aim : %llu aims started, %llu bookmarks set, %llu grips "
         "that set nothing, %llu aims stopped this window", g_starts, g_sets,
         g_misses, g_stops);
  g_starts = g_sets = g_misses = g_stops = 0;
}
