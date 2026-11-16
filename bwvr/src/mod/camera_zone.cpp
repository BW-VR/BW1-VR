// THE CAMERA ZONE OF A SCRIPT OF THE GAME, IN THE GOD VIEW (docs/god-view.md,
// "The camera zone").
//
// WHAT THE GAME DOES. A script of Land 1 sets a camera zone with the script
// word "set camera zone" (GScript::SetCameraZone 0x006ED890). The word loads
// a file of Data\Zones, for example Land1Choose.exc at the Creature select.
// The file gives a polygon on the map (CameraModeNew3::ForceFieldPoints
// 0x00C5B130, its count at 0x00C5E130), and the word sets
// CameraModeNew3::DrawForceField 0x00C5E144. On each update
// CameraModeNew3::Update (0x0045F983 to 0x0045FCE1) tests the position of the
// game camera against the polygon (CameraExclusion::InsideInclusion
// 0x00455E20). A position outside it:
//
// - goes to a point in the zone, up to 50 units away, with its focus,
// - sends the help event 0x202 and the effect 42 of the force feedback
//   (GInterface::StartImmersion), which the controllers give as a pulse,
// - plays the sound 0x2B of the block at the camera (fn_0042A000),
// - starts a shake of the camera of 400 ms (LH3DCameraChecker::Create
//   0x00821050, return address 0x0045FC89),
// - and pulses the force field at the edge (LightSheet::PulseForceField).
//
// LH3DTech::UpdateCamera adds the shake to the camera that it gets
// (fn_008210C0), and in the God view that is the camera of the head. So the
// shake is in the headset: each frame the eye and its focus go a random
// amount of up to 1 unit, for 400 ms.
//
// WHY THE GOD VIEW HIT IT. The God view puts the game camera 400 units
// behind the land under the head and 50 degrees above it (tabletop.cpp,
// TabletopGodGameCamera). That is 257 units back on the map. The zone of the
// Creature select is about 250 units across, so each move of the view put the
// game camera out of the zone, and each update of a move had the push, the
// sound and the shake.
//
// WHAT THIS FILE DOES (god_zone=1):
//
// - The game camera stays in the zone. Its focus goes into the zone, and its
//   position comes along the line to the focus, 5 units before the edge.
//   When that leaves less than 40 units across the map, the camera goes 40
//   units from its focus toward the middle of the zone: the test of the game
//   casts a ray from the camera toward its focus, and with the two at one
//   place it finds the camera outside on each update. So the game does not
//   push it.
// - The player's place (the waist point of the God view) goes at most
//   god_zone_margin units past the zone. A move past that stops at the limit
//   and keeps its part along the edge. The turn and the zoom stay free: a
//   snap turn is not limited, and a zoom that moves the place is limited like
//   a move.
// - While the player pushes past the limit, the game camera goes 10 units out
//   of the zone for one update, at most once each 500 ms. The game then
//   plays its sound of the block, gives the pulse and the force field, and
//   moves its camera back in. The view does not move with the game camera.
// - The shake of the push is not started while the God view owns the view.
//
// With god_zone=0 the God view does none of this, and the game pushes its
// camera, with the shake.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "MinHook.h"
#include "camera_zone.h"
#include "camera_zone_math.h"
#include "debug_menu.h"

extern void ModLog(const char* format, ...);
bool TabletopGodOwnsView();

namespace {

const uintptr_t kDrawForceField = 0x00C5E144;
const uintptr_t kForceFieldCount = 0x00C5E130;
const uintptr_t kForceFieldPoints = 0x00C5B130;
// The array runs to the count: 0x3000 bytes of LHPoint, 12 bytes each.
const int kMostPoints = 1024;
// LH3DCameraChecker::Create, cdecl (float radius, LHPoint place, float
// strength, long ms, bool), ret. The shake that LH3DTech::UpdateCamera adds.
const uintptr_t kCheckerCreate = 0x00821050;
// The return address of its call in the push of CameraModeNew3::Update.
const uintptr_t kFromZonePush = 0x0045FC89;

// How far out of the zone a bump puts the game camera, and the time between
// two bumps. The push of the game finds a point in the zone within 50 units.
const float kBumpOut = 10.0f;
const ULONGLONG kBumpGapMs = 500;
// A push of the player that is older than this asks for no bump.
const ULONGLONG kPushFreshMs = 150;
const int kMostLogs = 40;

typedef void(__cdecl* CreateFn)(float radius, float x, float y, float z,
                                float strength, int32_t ms, uint32_t flag);
CreateFn o_Create = nullptr;

bool g_installed = false;
int g_on = 1;
float g_margin = static_cast<float>(camera_zone_math::kMarginDefault);

// The zone of the game, read again on each call that needs it.
float g_xz[kMostPoints * 2];
int g_count = 0;
// The zone of the last log line.
int g_said_count = -1;
float g_said_first[2] = {0.0f, 0.0f};

// The push of the player past the limit, for the bump and the log.
bool g_pushing = false;
ULONGLONG g_push_ms = 0;
float g_push_place[2] = {0.0f, 0.0f};
ULONGLONG g_bump_ms = 0;
int g_logs = 0;

// The counts of one window of the report.
uint64_t g_limited = 0;    // moves that the limit changed
uint64_t g_pushes = 0;     // pushes past the limit
uint64_t g_bumps = 0;      // bumps asked of the game
uint64_t g_kept = 0;       // writes of the game camera kept in the zone
uint64_t g_focus_kept = 0; // focus points moved into the zone
uint64_t g_shakes = 0;     // shakes of the push not started
uint64_t g_shakes_game = 0;  // shakes of the push that the game started

// Reads the zone of the game. False when there is none.
bool Read() {
  g_count = 0;
  const int32_t* draw = reinterpret_cast<const int32_t*>(kDrawForceField);
  const int32_t* count = reinterpret_cast<const int32_t*>(kForceFieldCount);
  if (IsBadReadPtr(draw, 4) || IsBadReadPtr(count, 4)) return false;
  if (*draw == 0) return false;
  const int n = *count;
  if (n < 3 || n > kMostPoints) return false;
  const float* points = reinterpret_cast<const float*>(kForceFieldPoints);
  if (IsBadReadPtr(points, static_cast<UINT_PTR>(n) * 12)) return false;
  for (int i = 0; i < n; ++i) {
    g_xz[i * 2] = points[i * 3];
    g_xz[i * 2 + 1] = points[i * 3 + 2];
  }
  g_count = n;
  return true;
}

// The zone is on: god_zone=1 and a script set a zone. tabletop.cpp asks
// this for the moves of the God view. Those come in the main camera call,
// before the God view takes the frame, so they do not ask who owns the view.
bool On() {
  if (!g_installed || g_on == 0) return false;
  return Read();
}

// The zone is on, and the God view owns the view.
bool Active() { return TabletopGodOwnsView() && On(); }

void __cdecl h_Create(float radius, float x, float y, float z, float strength,
                      int32_t ms, uint32_t flag) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  if (caller == kFromZonePush) {
    if (g_on != 0 && TabletopGodOwnsView()) {
      ++g_shakes;
      return;
    }
    ++g_shakes_game;
  }
  o_Create(radius, x, y, z, strength, ms, flag);
}

}  // namespace

void CameraZoneInstall(int on, int margin) {
  g_on = on != 0 ? 1 : 0;
  if (margin < 0) margin = 0;
  if (margin > camera_zone_math::kMarginMost) {
    margin = camera_zone_math::kMarginMost;
  }
  g_margin = static_cast<float>(margin);
  if (!g_installed) {
    void* target = reinterpret_cast<void*>(kCheckerCreate);
    MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(&h_Create),
                                     reinterpret_cast<void**>(&o_Create));
    if (status == MH_OK) status = MH_EnableHook(target);
    ModLog("hooks        : LH3DCameraChecker::Create (the shake of the camera "
           "zone) at 0x%08X %s", kCheckerCreate,
           status == MH_OK ? "ok" : "FAILED");
    g_installed = status == MH_OK;
  }
  if (g_on != 0) {
    ModLog("camera zone  : god_zone=1. In a camera zone of a script the game "
           "camera stays in the zone, so the game does not push it back or "
           "shake the view. Your place goes at most %.0f units past the zone "
           "(god_zone_margin), and a push past that plays the sound of the "
           "game", g_margin);
  } else {
    ModLog("camera zone  : god_zone=0. In a camera zone of a script the game "
           "pushes its camera back into the zone, with its sound and its "
           "shake");
  }
}

bool CameraZoneActive() { return Active(); }

void CameraZoneWatch() {
  if (!g_installed) return;
  const bool have = Read();
  const int count = have ? g_count : 0;
  const float first[2] = {have ? g_xz[0] : 0.0f, have ? g_xz[1] : 0.0f};
  if (count == g_said_count && first[0] == g_said_first[0] &&
      first[1] == g_said_first[1]) {
    return;
  }
  g_said_count = count;
  g_said_first[0] = first[0];
  g_said_first[1] = first[1];
  g_pushing = false;
  if (!have) {
    ModLog("camera zone  : the game has no camera zone now");
    return;
  }
  float lx = g_xz[0], hx = g_xz[0], lz = g_xz[1], hz = g_xz[1];
  for (int i = 1; i < count; ++i) {
    const float x = g_xz[i * 2], z = g_xz[i * 2 + 1];
    if (x < lx) lx = x;
    if (x > hx) hx = x;
    if (z < lz) lz = z;
    if (z > hz) hz = z;
  }
  float mx = 0.0f, mz = 0.0f;
  camera_zone_math::Middle(g_xz, count, &mx, &mz);
  ModLog("camera zone  : a script set a camera zone of %d points, x %.1f to "
         "%.1f and z %.1f to %.1f, the middle at %.1f %.1f (god_zone=%d)",
         count, lx, hx, lz, hz, mx, mz, g_on);
}

bool CameraZoneLimitMove(const float from[3], float* x, float* z,
                         const char* by) {
  if (!On()) return false;
  const float want_x = *x, want_z = *z;
  if (!camera_zone_math::LimitMove(g_xz, g_count, g_margin, from[0], from[2],
                                   x, z)) {
    if (g_pushing && g_logs < kMostLogs) {
      ++g_logs;
      ModLog("camera zone  : the push past the limit ended. Your place is at "
             "%.1f %.1f, %.1f units past the zone", *x, *z,
             camera_zone_math::OutsideDistance(g_xz, g_count, *x, *z));
    }
    g_pushing = false;
    return false;
  }
  ++g_limited;
  const ULONGLONG now = GetTickCount64();
  if (!g_pushing) {
    ++g_pushes;
    if (g_logs < kMostLogs) {
      ++g_logs;
      ModLog("camera zone  : %s asked for your place at %.1f %.1f, %.1f units "
             "past the zone. It stops at %.1f %.1f, %.1f units past it (the "
             "limit of god_zone_margin=%.0f, or the place before the move)",
             by, want_x, want_z,
             camera_zone_math::OutsideDistance(g_xz, g_count, want_x, want_z),
             *x, *z, camera_zone_math::OutsideDistance(g_xz, g_count, *x, *z),
             g_margin);
    }
  }
  g_pushing = true;
  g_push_ms = now;
  g_push_place[0] = *x;
  g_push_place[1] = *z;
  return true;
}

bool CameraZoneAllows(float x, float z) {
  if (!On()) return true;
  return camera_zone_math::OutsideDistance(g_xz, g_count, x, z) <= g_margin;
}

bool CameraZoneFocus(float* x, float* z) {
  if (!Active()) return false;
  const float was_x = *x, was_z = *z;
  if (!camera_zone_math::PointInside(g_xz, g_count, camera_zone_math::kInset,
                                     x, z)) {
    return false;
  }
  ++g_focus_kept;
  static int said = 0;
  if (said < 8) {
    ++said;
    ModLog("camera zone  : the land point of the game camera goes into the "
           "zone, from %.1f %.1f to %.1f %.1f", was_x, was_z, *x, *z);
  }
  return true;
}

bool CameraZoneKeepCamera(const float focus[3], float position[3]) {
  if (!Active()) return false;
  const float was[3] = {position[0], position[1], position[2]};
  if (!camera_zone_math::KeepCamera(g_xz, g_count, camera_zone_math::kInset,
                                    focus, position)) {
    return false;
  }
  ++g_kept;
  static int said = 0;
  if (said < 8) {
    ++said;
    ModLog("camera zone  : the game camera stays in the zone: at %.1f %.1f "
           "%.1f, not at %.1f %.1f %.1f, looking at %.1f %.1f %.1f",
           position[0], position[1], position[2], was[0], was[1], was[2],
           focus[0], focus[1], focus[2]);
  }
  return true;
}

bool CameraZoneTakeBump(float* x, float* z) {
  if (!g_pushing || !Active()) return false;
  const ULONGLONG now = GetTickCount64();
  if (now - g_push_ms > kPushFreshMs) return false;
  if (g_bump_ms != 0 && now - g_bump_ms < kBumpGapMs) return false;
  g_bump_ms = now;
  camera_zone_math::BumpPoint(g_xz, g_count, kBumpOut, g_push_place[0],
                              g_push_place[1], x, z);
  ++g_bumps;
  static int said = 0;
  if (said < 12) {
    ++said;
    ModLog("camera zone  : you push past the limit, so the game camera goes "
           "to %.1f %.1f, %.0f units out of the zone, for one update. The "
           "game plays its sound of the block and moves its camera back in",
           *x, *z, kBumpOut);
  }
  return true;
}

int CameraZoneValue() { return g_installed ? g_on : kDebugCannot; }

void CameraZoneToggle() {
  if (!g_installed) return;
  g_on = g_on != 0 ? 0 : 1;
  g_pushing = false;
  ModLog("test key     : god_zone=%d. %s", g_on,
         g_on != 0 ? "The game camera stays in the camera zone, and your "
                     "place stops at the limit past it"
                   : "The game pushes its camera back into the camera zone, "
                     "with its shake");
}

void CameraZoneReport() {
  if (!g_installed) return;
  if (g_limited == 0 && g_pushes == 0 && g_bumps == 0 && g_kept == 0 &&
      g_focus_kept == 0 && g_shakes == 0 && g_shakes_game == 0) {
    return;
  }
  ModLog("camera zone  : god_zone=%d, %s. The limit stopped %llu moves in %llu "
         "pushes, and %llu bumps asked the game for its sound. The game camera "
         "was kept in the zone on %llu writes, and its land point on %llu. "
         "%llu shakes of the zone were not started, and the game started "
         "%llu", g_on, Read() ? "a zone is set" : "no zone is set", g_limited,
         g_pushes, g_bumps, g_kept, g_focus_kept, g_shakes, g_shakes_game);
  g_limited = g_pushes = g_bumps = g_kept = g_focus_kept = g_shakes =
      g_shakes_game = 0;
}
