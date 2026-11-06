// The influence at the hand (docs/miracles.md, "The influence at the hand").
//
// WHAT THE PLAIN GAME DOES AT THE EDGE [V]. The Hand of the mouse can go past
// the edge of the player's influence. There it acts on nothing: the hand mode
// of an object is 0x12 (a touch) and not a pick up
// (Object::InterfaceMustBeInInfluenceForInteraction), and each game turn of
// a take ends it (GInterfaceStatus::Process, 0x005DC540). The Hand itself
// does not change. GVirtualInfluence (GInterfaceStatus + 0x130) keeps the
// last land point of the Hand inside the influence (+0x08). Its Process
// (0x0076CD40) gives a strength (+0x30) of 1 there, and outside the strength
// goes down with the distance from that point and with the time, to 0 under
// 0.1. While the strength is above 0, GVirtualInfluence::Draw 0x0076D000
// finds the edge between that point and the Hand (+0x14), and calls
// PSysGlobal::AddManaPathSprite 0x0068FB20 at 0x0076D1C2: a stream of light
// from the edge to the land point under the Hand (MapCoords of CHand+0x78,
// with 0 over the land), and a sound (SetSoundFraction). A script can turn it
// off (SET_VIRTUAL_INFLUENCE, GPlayer::SetVirtualInfluence 0x0064C3B0).
//
// THE REAL HAND is in the air over the land, so three things change, each
// with a key:
//
//   influence_fade         outside the influence each hand fades to 45
//                          percent in 0.25 s (HandModelSetOpacity), and comes
//                          back inside it. The test is the game's own
//                          (Influence::CalculatePlayerInfluence at the land
//                          point of the grip, as fn_005D1120 tests it)
//   influence_stream_hand  the stream of the game ends at the Hand's point in
//                          the air: the hook of AddManaPathSprite gives the
//                          MapCoords of the Hand the height of the Hand over
//                          the land (the third field, which
//                          MapCoords::GetLHPoint adds to the land)
//   influence_wall         a faint wall of light stands on the edge near the
//                          pointing hand while the hand is less than 15 cm of
//                          the room from the edge (influence_edge_math.h).
//                          overlay.cpp draws it with the glow, as added light
//
// The fade and the wall act only while the controller places the Hand. The
// stream acts only for the land point of the Hand of the controller.
//
// THE TEST KEY influence_test_circle_cm=R: from the first frame with the Hand
// of the controller, the influence of the player is a circle of R cm of the
// room around the land point of the game camera of that frame, which the God
// view keeps over the land under the head. The fade, the wall and the stream
// see it (the test hook of Influence::IsInPlayerRawInfluence 0x005CD460,
// which GVirtualInfluence uses). The pick of the game keeps the real
// influence. A desktop run with a fixed head then has the edge at a known
// distance in front of the head. Never for play.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"
#include "influence_edge_math.h"

extern void ModLog(const char* format, ...);
// hand_direct.cpp
bool HandDirectInsideInfluence(float x, float z, bool* inside);
bool HandDirectHandPosition(float out[3], bool* by_controller);
// controllers.cpp
unsigned long long ControllersFrameCount();
// head_camera.cpp: the game camera of the last main call.
bool HeadCameraGameCamera(float position[3], float focus[3]);

namespace {

const uintptr_t kManaPathSprite = 0x0068FB20;  // cdecl, 4 arguments
const uintptr_t kRawInfluence = 0x005CD460;    // cdecl (coords, player)
const uintptr_t kGetLHPoint = 0x00605C40;      // MapCoords, thiscall
const float kMapScale = 6553.6f;

struct MapCoords {
  int32_t x, z;
  float over;  // added to the height of the land by GetLHPoint
};

typedef void(__cdecl* ManaPathFn)(MapCoords* from, MapCoords* to,
                                  uint32_t color, float fraction);
typedef int(__cdecl* RawInfluenceFn)(const MapCoords* at, void* player);
typedef float*(__fastcall* GetLHPointFn)(const MapCoords* self, void* unused,
                                         float* out);

ManaPathFn o_ManaPath = nullptr;
RawInfluenceFn o_RawInfluence = nullptr;

bool g_read = false;
bool g_fade = true;
bool g_stream_hand = true;
bool g_wall = true;
bool g_installed = false;

// THE FADE: the opacity outside, and the time of the whole way.
const float kOutsideOpacity = 0.45f;
const float kFadeSeconds = 0.25f;
float g_opacity[2] = {1.0f, 1.0f};
bool g_inside[2] = {true, true};
bool g_have_inside[2] = {false, false};
uint64_t g_fade_frames = 0;
int g_fade_logs = 0;
const int kMaxFadeLogs = 40;

// THE WALL: the range from the hand, its height and its light, in meters of
// the room and in the share of full light. 24 rays, 6 steps and 5 halvings:
// a point of the edge to 1/192 of the range.
const float kWallRangeM = 0.15f;
const float kWallHeightM = 0.5f;
const float kWallLight = 0.35f;
const int kWallRays = 24;
const int kWallSteps = 6;
const int kWallHalvings = 5;
uint64_t g_wall_frames = 0;
uint64_t g_wall_triangles = 0;
bool g_wall_was = false;
int g_wall_logs = 0;
const int kMaxWallLogs = 40;

// THE STREAM.
uint64_t g_stream_calls = 0;
uint64_t g_stream_lifted = 0;
float g_last_lift = 0.0f;
bool g_said_stream = false;

// THE TEST CIRCLE.
int g_test_cm = 0;
bool g_have_circle = false;
float g_circle[3] = {0.0f, 0.0f, 0.0f};  // x, z and the radius in units
uint64_t g_raw_tests = 0;

void ReadKeys() {
  if (g_read) return;
  g_read = true;
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash =
      length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  const auto number = [&path](const char* key, int fallback) {
    return static_cast<int>(GetPrivateProfileIntA(
        "bwvr", key, static_cast<UINT>(fallback), path));
  };
  g_fade = number("influence_fade", 1) != 0;
  g_stream_hand = number("influence_stream_hand", 1) != 0;
  g_wall = number("influence_wall", 1) != 0;
  g_test_cm = number("influence_test_circle_cm", 0);
  if (g_test_cm < 0) g_test_cm = 0;
  if (g_test_cm > 1000) g_test_cm = 1000;
}

bool CircleInside(float x, float z) {
  const float dx = x - g_circle[0], dz = z - g_circle[1];
  return dx * dx + dz * dz <= g_circle[2] * g_circle[2];
}

// The test of the influence that the fade and the wall use.
bool Inside(float x, float z, bool* inside) {
  if (g_have_circle) {
    *inside = CircleInside(x, z);
    return true;
  }
  return HandDirectInsideInfluence(x, z, inside);
}

float LandHeight(float x, float z) {
  MapCoords at = {static_cast<int32_t>(x * kMapScale),
                  static_cast<int32_t>(z * kMapScale), 0.0f};
  float point[3] = {x, 0.0f, z};
  reinterpret_cast<GetLHPointFn>(kGetLHPoint)(&at, nullptr, point);
  return point[1];
}

void __cdecl h_ManaPath(MapCoords* from, MapCoords* to, uint32_t color,
                        float fraction) {
  ++g_stream_calls;
  float hand[3];
  bool by_controller = false;
  if (g_stream_hand && to != nullptr && !IsBadWritePtr(to, sizeof(*to)) &&
      HandDirectHandPosition(hand, &by_controller) && by_controller) {
    MapCoords flat = *to;
    flat.over = 0.0f;
    float land[3] = {0.0f, 0.0f, 0.0f};
    reinterpret_cast<GetLHPointFn>(kGetLHPoint)(&flat, nullptr, land);
    const float over = hand[1] - land[1];
    if (over > 0.0f) {
      to->over = over;
      ++g_stream_lifted;
      g_last_lift = over;
      if (!g_said_stream) {
        g_said_stream = true;
        ModLog("influence    : the stream of the influence ends at the "
               "Hand's point, %.1f units over the land at %.1f %.1f "
               "(influence_stream_hand=1), frame %llu", over, hand[0],
               hand[2], ControllersFrameCount());
      }
    }
  }
  o_ManaPath(from, to, color, fraction);
}

int __cdecl h_RawInfluence(const MapCoords* at, void* player) {
  if (g_have_circle && at != nullptr && !IsBadReadPtr(at, sizeof(*at))) {
    ++g_raw_tests;
    return CircleInside(static_cast<float>(at->x) / kMapScale,
                        static_cast<float>(at->z) / kMapScale)
               ? 1
               : 0;
  }
  return o_RawInfluence(at, player);
}

bool Hook(uintptr_t address, void* replacement, void** original,
          const char* name) {
  void* target = reinterpret_cast<void*>(address);
  MH_STATUS status = MH_CreateHook(target, replacement, original);
  if (status == MH_OK) status = MH_EnableHook(target);
  ModLog("hooks        : %s at 0x%08X %s", name, address,
         status == MH_OK ? "ok" : "FAILED");
  return status == MH_OK;
}

}  // namespace

// mod_main.cpp, once with the other hooks, in the God view and on the table.
void InfluenceEdgeInstall() {
  ReadKeys();
  if (g_installed) return;
  g_installed = true;
  ModLog("influence    : influence_fade=%d. %s", g_fade ? 1 : 0,
         g_fade ? "Outside the influence of the player each hand fades to 45 "
                  "percent"
                : "The hands do not fade outside the influence");
  ModLog("influence    : influence_stream_hand=%d. %s",
         g_stream_hand ? 1 : 0,
         g_stream_hand ? "The stream of the influence ends at the Hand's "
                         "point in the air"
                       : "The stream of the influence ends on the land under "
                         "the Hand, as the game draws it");
  ModLog("influence    : influence_wall=%d. %s", g_wall ? 1 : 0,
         g_wall ? "A faint wall of light shows the edge of the influence "
                  "near the pointing hand, from 15 cm of the room before it"
                : "No wall at the edge of the influence");
  if (g_stream_hand) {
    Hook(kManaPathSprite, reinterpret_cast<void*>(&h_ManaPath),
         reinterpret_cast<void**>(&o_ManaPath),
         "PSysGlobal::AddManaPathSprite (the stream of the influence)");
  }
  if (g_test_cm > 0) {
    Hook(kRawInfluence, reinterpret_cast<void*>(&h_RawInfluence),
         reinterpret_cast<void**>(&o_RawInfluence),
         "Influence::IsInPlayerRawInfluence (test key "
         "influence_test_circle_cm)");
  }
}

// overlay.cpp, once a frame before the hands are built, while the controller
// places the Hand. The test circle starts here.
void InfluenceEdgeFrame(float units_per_meter) {
  ReadKeys();
  if (g_test_cm <= 0 || g_have_circle || !(units_per_meter > 0.0f)) return;
  float hand[3], camera[3], focus[3];
  bool by_controller = false;
  if (!HandDirectHandPosition(hand, &by_controller) || !by_controller ||
      !HeadCameraGameCamera(camera, focus)) {
    return;
  }
  g_circle[0] = camera[0];
  g_circle[1] = camera[2];
  g_circle[2] = static_cast<float>(g_test_cm) * 0.01f * units_per_meter;
  g_have_circle = true;
  ModLog("influence    : test key influence_test_circle_cm=%d: from frame "
         "%llu the influence of the player is a circle of %.1f units around "
         "%.1f %.1f (the game camera), for the fade, the wall and the "
         "stream. The Hand is at %.1f %.1f, %.1f units from the middle",
         g_test_cm, ControllersFrameCount(), g_circle[2], g_circle[0],
         g_circle[1], hand[0], hand[2],
         sqrtf((hand[0] - g_circle[0]) * (hand[0] - g_circle[0]) +
               (hand[2] - g_circle[1]) * (hand[2] - g_circle[1])));
}

// overlay.cpp, once a frame for each hand that it builds: the opacity of the
// hand (0 the left hand, 1 the right hand) at its grip point in the world.
float InfluenceEdgeOpacity(int hand, const float point[3], float dt) {
  ReadKeys();
  if (hand < 0 || hand > 1) return 1.0f;
  if (!g_fade) return 1.0f;
  bool inside = true;
  if (!Inside(point[0], point[2], &inside)) inside = true;
  if (g_have_inside[hand] && inside != g_inside[hand] &&
      g_fade_logs < kMaxFadeLogs) {
    ++g_fade_logs;
    ModLog("influence    : the %s hand %s the influence at %.1f %.1f, so it "
           "%s, frame %llu", hand == 0 ? "left" : "right",
           inside ? "comes back into" : "goes out of", point[0], point[2],
           inside ? "shows in full again" : "fades to 45 percent",
           ControllersFrameCount());
  }
  g_inside[hand] = inside;
  g_have_inside[hand] = true;
  g_opacity[hand] = influence_math::FadeStep(g_opacity[hand], inside,
                                             kOutsideOpacity, kFadeSeconds,
                                             dt);
  if (g_opacity[hand] < 1.0f) ++g_fade_frames;
  return g_opacity[hand];
}

// overlay.cpp, once a frame: the wall near the pointing hand, at its point in
// the world. Returns the count of triangles in `out`, at most `room`.
int InfluenceEdgeWall(const float point[3], float units_per_meter,
                      influence_math::WallTriangle* out, int room) {
  ReadKeys();
  if (!g_wall || !(units_per_meter > 0.0f)) return 0;
  bool here = true;
  if (!Inside(point[0], point[2], &here)) return 0;
  const float range = kWallRangeM * units_per_meter;
  bool found[influence_math::kMaxDirections];
  float edge[influence_math::kMaxDirections][2];
  const auto inside = [](float x, float z) {
    bool in = true;
    if (!Inside(x, z, &in)) in = true;
    return in;
  };
  const int count = influence_math::FindEdge(
      inside, point[0], point[2], range, kWallRays, kWallSteps, kWallHalvings,
      found, edge);
  int n = 0;
  if (count > 0) {
    n = influence_math::BuildWall(
        [](float x, float z) { return LandHeight(x, z); }, point, found,
        edge, kWallRays, range, kWallHeightM * units_per_meter, 0.75f * range,
        kWallLight, out, room);
  }
  const bool shows = n > 0;
  if (shows != g_wall_was && g_wall_logs < kMaxWallLogs) {
    ++g_wall_logs;
    ModLog("influence    : the wall at the edge %s, the pointing hand at "
           "%.1f %.1f is %s the influence, %d of %d rays found the edge, %d "
           "triangles, strength %.2f, frame %llu",
           shows ? "shows" : "goes", point[0], point[2],
           here ? "inside" : "outside", count, kWallRays, n,
           count > 0 ? influence_math::Strength(point, found, edge, kWallRays,
                                                range)
                     : 0.0f,
           ControllersFrameCount());
  }
  g_wall_was = shows;
  if (shows) {
    ++g_wall_frames;
    g_wall_triangles += static_cast<uint64_t>(n);
  }
  return n;
}

// Once a report window.
void InfluenceEdgeReport() {
  if (g_fade_frames == 0 && g_wall_frames == 0 && g_stream_calls == 0 &&
      g_raw_tests == 0) {
    return;
  }
  ModLog("influence    : %llu frames with a hand that fades, %llu frames with "
         "the wall (%.0f triangles a frame). The stream of the game had %llu "
         "calls, %llu of them at the Hand's point (the last %.1f units over "
         "the land). %llu tests of the test circle",
         g_fade_frames, g_wall_frames,
         g_wall_frames > 0 ? static_cast<double>(g_wall_triangles) /
                                 static_cast<double>(g_wall_frames)
                           : 0.0,
         g_stream_calls, g_stream_lifted, g_last_lift, g_raw_tests);
  g_fade_frames = g_wall_frames = g_wall_triangles = 0;
  g_stream_calls = g_stream_lifted = g_raw_tests = 0;
}
