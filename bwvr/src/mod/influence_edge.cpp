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
//                          0 by default: no desktop run has had a stream of
//                          the game yet. GVirtualInfluence keeps the land
//                          point of the interface (GInterfaceStatus+0x14),
//                          and the report logs it with its state
//   influence_wall         a glow on the edge near each hand. It has the red
//                          of the game's own influence ring: the color of the
//                          ring of the player in the table at 0x00EA9EFC,
//                          which fn_00826510 fills and InfluenceCircle
//                          (fn_008265F0) reads by the player index of
//                          GetRemapedPlayer 0x0064D790. overlay.cpp draws it
//                          with the glow, as added light, with the depth test.
//                          influence_wall_look gives its look:
//     1  the hologram: a part of the wall of the edge near the hand, with a
//        grid that is fixed to the wall and rings that go out from the point
//        of the wall nearest to the hand (influence_edge_math.h,
//        BuildHologram). It starts 45 cm of the room inside the edge, gets
//        brighter and larger as the hand comes nearer, and goes 40 cm past
//        the edge. The wall is the real edge of the game: the influence of a
//        player is the union of the disc of the Temple (Citadel+0x14, radius
//        GGame+0x25007C times Citadel+0x6C) and the disc of each town
//        (GPlayer+0xA50, next +0x75C, its place +0x14, its radius +0x5C8)
//        [V, Influence::CalculatePlayerRawInfluence 0x005CD230,
//        fn_004630F0, fn_007479E0]. So the point of the edge comes from the
//        circles, with no search, and the wall of a circle does not turn
//        when the hand turns. The test of the game checks the point 2 cm
//        each side of it. When the test and the circles do not agree (an
//        influence ring of a script, a friend's influence), the point comes
//        from the search of the round patch and the hologram is level and
//        faces the hand. influence_wall_ripple=1 lets the rings go out at
//        5 cm of the room a second, 0 keeps them still
//     0  the round patch: a soft round glow on the edge at the point nearest
//        to the hand, found by 24 rays, in the plane that faces the hand,
//        from 20 cm of the room inside the edge (GlowStrength)
//
// THE GAME'S OWN FIELD [V]. fn_00827820, from the draw of the land
// (0x005E61B5), tests the place of the 3D object of the Hand
// (CHand+0x482C, +0x38, copied to 0x00E9A100 by GLandscape::Draw) against
// each disc of InfluenceCircle. When the Hand goes through the ring of a
// player whose ring shows (0x00EB9A1C), it makes one burst at the point of
// the ring between the two places (fn_00827250: 7 sprites of 0 to 12 units
// that grow for 2 s, in the color of that ring), a sound and a pulse. So the
// game shows a ring only at the moment of the crossing, at a fixed size in
// world units, and not while the Hand comes near. The mod does not change it.
//
// The fade and the glow act only while the controller places the Hand. The
// stream acts only for the land point of the Hand of the controller.
//
// THE TEST KEY influence_stream_test_log=1 logs each stream of light of the
// game that SpellPointInf::SpellPointInf 0x0069AC10 makes: the two ends, the
// height of each end over the land, the color, the caller and the Hand's
// point, 24 times and then each 200th time. Two callers make one: the
// stream of the influence (PSysGlobal::AddManaPathSprite, 0x0068FC19) and
// the prayer power that a Worship Site gives a spell
// (WorshipSite::CreateManaPathSprite 0x0077B2C0, from
// Spell::CreateSpellPoint). The mod changes nothing in the stream.
//
// THE TEST KEY influence_test_circle_cm=R with influence_test_circle_frame=F:
// from the first frame of the controllers at F or later with the Hand of the
// controller, the influence of the player is a circle of R cm of the room
// around the land point of the Hand's point of that frame. The fade, the glow
// and the stream see it (the test hook of Influence::IsInPlayerRawInfluence
// 0x005CD460, which GVirtualInfluence uses). Before that frame the fade and
// the glow count each point as inside. The pick of the game keeps the real
// influence. A desktop run that steps the hand from that point then has the
// edge at a known distance. Never for play.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "MinHook.h"
#include "debug_menu.h"
#include "influence_edge_math.h"

extern void ModLog(const char* format, ...);
void ControllersGesturePulse(float amplitude, float seconds);
// hand_direct.cpp
bool HandDirectInsideInfluence(float x, float z, bool* inside);
void* HandDirectPlayer();
bool HandDirectHandPosition(float out[3], bool* by_controller);
void* HandDirectGameHand();
// controllers.cpp
unsigned long long ControllersFrameCount();


namespace {

const uintptr_t kManaPathSprite = 0x0068FB20;  // cdecl, 4 arguments
const uintptr_t kRawInfluence = 0x005CD460;    // cdecl (coords, player)
const uintptr_t kGetLHPoint = 0x00605C40;      // MapCoords, thiscall
// SpellPointInf::SpellPointInf, thiscall (const LHPoint& a, float size,
// const LHPoint& b, unsigned color), ret 0x10. It keeps a at +0x24, b at
// +0x18, the size at +0x14 and the color at +0x30 [V, 0x0069AC18 to
// 0x0069AC53]. The return addresses of its two callers:
const uintptr_t kSpellPoint = 0x0069AC10;
const uintptr_t kSpellPointByInfluence = 0x0068FC1E;
const uintptr_t kSpellPointByWorshipSite = 0x0077B386;
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

typedef void*(__fastcall* SpellPointFn)(void* self, void* unused,
                                        const float* a, float size,
                                        const float* b, uint32_t color);

ManaPathFn o_ManaPath = nullptr;
SpellPointFn o_SpellPoint = nullptr;
RawInfluenceFn o_RawInfluence = nullptr;

bool g_read = false;
bool g_fade = true;
bool g_stream_hand = false;
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

// THE GLOW AT THE EDGE, in meters of the room. Inside the influence it starts
// 20 cm from the edge: about one hand length, so it shows only when the hand
// is about to touch the edge and not while the hand works near it. It is
// strongest at the touch. Past the edge it falls to 35 percent in 3 cm, and
// to 0 at 30 cm: the faint patch where the hand went through. The patch is
// 12 cm across from its middle, about the size of the hand, and its light is
// 0.6 at the most. 24 rays, 6 steps and 5 halvings:
// a point of the edge to 1/192 of the range.
const float kWallRangeM = 0.20f;
const float kWallTouchM = 0.03f;
const float kWallOutsideM = 0.30f;
const float kWallOutsideShare = 0.35f;
const float kPatchRadiusM = 0.12f;
const float kWallLight = 0.6f;
// The test key influence_wall_test_light_pct: the light of the glow times
// this share, for an eye picture of a desktop run. 1 to 3.
float g_wall_light = 1.0f;
const int kWallRays = 24;
const int kWallSteps = 6;
const int kWallHalvings = 5;
uint64_t g_wall_frames = 0;
uint64_t g_wall_triangles = 0;
bool g_wall_was[2] = {false, false};
// THE COLOR of the influence ring of the player, 0 to 1, and its index.
const uintptr_t kRingColors = 0x00EA9EFC;  // 8 D3D colors, 0xAARRGGBB
const uintptr_t kGetPlayerNumber = 0x0064A790;  // GPlayer, thiscall
const uintptr_t kGetRemappedPlayer = 0x0064D790;  // cdecl (number)
float g_color[3] = {1.0f, 70.0f / 255.0f, 70.0f / 255.0f};
int g_color_index = -1;
int g_wall_logs = 0;
const int kMaxWallLogs = 40;

// THE HOLOGRAM (influence_wall_look=1), in meters of the room. It starts 45
// cm inside the edge, so it shows before the hand is at the edge, and it goes
// 40 cm past the edge, so it shows the way back. Its disc on the wall is 14
// cm from the middle to the rim when it is faint and 30 cm at the touch. The
// grid has a line each 7.5 cm, the rings are 8 cm apart, and a line is 8 mm
// wide. The test of the game checks the edge point 2 cm each side of it.
bool g_look = true;    // influence_wall_look
bool g_ripple = true;  // influence_wall_ripple
const float kHoloInsideM = 0.45f;
const float kHoloOutsideM = 0.40f;
const float kHoloSmallM = 0.14f;
const float kHoloLargeM = 0.30f;
const float kHoloLight = 0.8f;
const float kHoloGridM = 0.075f;
const float kHoloRingGapM = 0.08f;
const float kHoloRingSpeedMps = 0.05f;
const float kHoloLineHalfM = 0.004f;
const float kHoloCheckM = 0.02f;
const float kHoloMarginM = 0.002f;
const int kMaxCircles = 48;
// How each frame of the hologram found its wall, for the report.
uint64_t g_holo_by_circles = 0;
uint64_t g_holo_by_search = 0;
uint64_t g_holo_disagree = 0;
uint64_t g_holo_shared = 0;
// The lines each 60 frames while the hologram shows.
int g_holo_logs = 0;
const int kMaxHoloLogs = 80;
// THE CIRCLES of the influence of the player in the objects of the game.
const uintptr_t kGamePointer = 0x00D0195C;  // GGame*
const int kGameInfluenceScale = 0x25007C;   // float
const int kPlayerCitadel = 0xA48;
const int kPlayerTowns = 0xA50;
const int kTownNext = 0x75C;
const int kTownInfluence = 0x5C8;
const int kThingCoords = 0x14;  // MapCoords of the Citadel and of a town
const int kCitadelInfluence = 0x6C;

// THE STREAM.
uint64_t g_stream_calls = 0;
uint64_t g_stream_lifted = 0;
float g_last_lift = 0.0f;
bool g_said_stream = false;
bool g_said_call = false;
// influence_stream_test_log: the log of each stream of light of the game.
bool g_stream_log = false;
uint64_t g_spell_points = 0;
const uint64_t kSpellPointLogs = 24;
const uint64_t kSpellPointEvery = 200;

// THE TEST CIRCLE.
int g_test_cm = 0;
uint64_t g_test_frame = 0;
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
  g_stream_hand = number("influence_stream_hand", 0) != 0;
  g_wall_light = static_cast<float>(number("influence_wall_test_light_pct",
                                           100)) /
                 100.0f;
  if (!(g_wall_light > 0.0f)) g_wall_light = 1.0f;
  if (g_wall_light > 3.0f) g_wall_light = 3.0f;
  g_wall = number("influence_wall", 1) != 0;
  g_look = number("influence_wall_look", 1) != 0;
  g_ripple = number("influence_wall_ripple", 1) != 0;
  g_stream_log = number("influence_stream_test_log", 0) != 0;
  g_test_cm = number("influence_test_circle_cm", 0);
  if (g_test_cm < 0) g_test_cm = 0;
  if (g_test_cm > 1000) g_test_cm = 1000;
  const int frame = number("influence_test_circle_frame", 0);
  g_test_frame = frame > 0 ? static_cast<uint64_t>(frame) : 0;
}

bool CircleInside(float x, float z) {
  const float dx = x - g_circle[0], dz = z - g_circle[1];
  return dx * dx + dz * dz <= g_circle[2] * g_circle[2];
}

// The test of the influence that the fade and the glow use.
bool Inside(float x, float z, bool* inside) {
  if (g_have_circle) {
    *inside = CircleInside(x, z);
    return true;
  }
  if (g_test_cm > 0) {
    *inside = true;
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

// The land point of the MapCoords at `thing` + kThingCoords.
void ThingPlace(const uint8_t* thing, float* x, float* z) {
  const int32_t* at = reinterpret_cast<const int32_t*>(thing + kThingCoords);
  *x = static_cast<float>(at[0]) / kMapScale;
  *z = static_cast<float>(at[1]) / kMapScale;
}

// THE CIRCLES of the influence of the player, as the game sums them: the
// disc of the Temple and the disc of each town. With the test circle, that
// circle alone. Before the test circle starts, none. Returns the count.
int GameCircles(influence_math::Circle* out, int room) {
  if (g_have_circle) {
    out[0].x = g_circle[0];
    out[0].z = g_circle[1];
    out[0].r = g_circle[2];
    return 1;
  }
  if (g_test_cm > 0) return 0;
  const uint8_t* player = static_cast<const uint8_t*>(HandDirectPlayer());
  if (player == nullptr || IsBadReadPtr(player + kPlayerCitadel, 12)) {
    return 0;
  }
  int n = 0;
  const uint8_t* game = *reinterpret_cast<const uint8_t* const*>(kGamePointer);
  const uint8_t* citadel =
      *reinterpret_cast<const uint8_t* const*>(player + kPlayerCitadel);
  if (citadel != nullptr && game != nullptr &&
      !IsBadReadPtr(citadel, kCitadelInfluence + 4) &&
      !IsBadReadPtr(game + kGameInfluenceScale, 4)) {
    const float r =
        *reinterpret_cast<const float*>(game + kGameInfluenceScale) *
        *reinterpret_cast<const float*>(citadel + kCitadelInfluence);
    if (r > 0.0f && n < room) {
      ThingPlace(citadel, &out[n].x, &out[n].z);
      out[n++].r = r;
    }
  }
  const uint8_t* town =
      *reinterpret_cast<const uint8_t* const*>(player + kPlayerTowns);
  for (int guard = 0; town != nullptr && n < room && guard < 256; ++guard) {
    if (IsBadReadPtr(town, kTownNext + 4)) break;
    const float r = *reinterpret_cast<const float*>(town + kTownInfluence);
    if (r > 0.0f) {
      ThingPlace(town, &out[n].x, &out[n].z);
      out[n++].r = r;
    }
    town = *reinterpret_cast<const uint8_t* const*>(town + kTownNext);
  }
  return n;
}

void __cdecl h_ManaPath(MapCoords* from, MapCoords* to, uint32_t color,
                        float fraction) {
  ++g_stream_calls;
  float hand[3] = {0.0f, 0.0f, 0.0f};
  bool by_controller = false;
  const bool have_hand = HandDirectHandPosition(hand, &by_controller);
  if (!g_said_call && to != nullptr && from != nullptr &&
      !IsBadReadPtr(to, sizeof(*to)) && !IsBadReadPtr(from, sizeof(*from))) {
    g_said_call = true;
    ModLog("influence    : the game draws the stream of the influence from "
           "%.1f %.1f to %.1f %.1f (strength %.2f). The Hand's point is at "
           "%.1f %.1f %.1f, %s, frame %llu",
           static_cast<float>(from->x) / kMapScale,
           static_cast<float>(from->z) / kMapScale,
           static_cast<float>(to->x) / kMapScale,
           static_cast<float>(to->z) / kMapScale, fraction, hand[0], hand[1],
           hand[2],
           !have_hand       ? "with no Hand"
           : by_controller  ? "placed by the controller"
                            : "not placed by the controller",
           ControllersFrameCount());
  }
  if (g_stream_hand && to != nullptr && !IsBadWritePtr(to, sizeof(*to)) &&
      have_hand && by_controller) {
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

void* __fastcall h_SpellPoint(void* self, void* unused, const float* a,
                               float size, const float* b, uint32_t color) {
  const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
  ++g_spell_points;
  if ((g_spell_points <= kSpellPointLogs ||
       g_spell_points % kSpellPointEvery == 0) &&
      a != nullptr && b != nullptr && !IsBadReadPtr(a, 12) &&
      !IsBadReadPtr(b, 12)) {
    float hand[3] = {0.0f, 0.0f, 0.0f};
    bool by_controller = false;
    const bool have_hand = HandDirectHandPosition(hand, &by_controller);
    ModLog("influence    : stream %llu of the game (%s, caller 0x%08X): from "
           "%.1f %.1f %.1f (%.1f units over the land) to %.1f %.1f %.1f (%.1f "
           "units over the land), size %.1f, color %08X. The Hand's point is "
           "at %.1f %.1f %.1f%s, frame %llu", g_spell_points,
           caller == kSpellPointByInfluence     ? "the influence"
           : caller == kSpellPointByWorshipSite ? "the prayer of a Worship "
                                                  "Site for a spell"
                                                : "another caller",
           static_cast<unsigned>(caller), a[0], a[1], a[2],
           a[1] - LandHeight(a[0], a[2]), b[0], b[1], b[2],
           b[1] - LandHeight(b[0], b[2]), size, color, hand[0], hand[1],
           hand[2], have_hand ? "" : " (no Hand)", ControllersFrameCount());
  }
  return o_SpellPoint(self, unused, a, size, b, color);
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
  ModLog("influence    : influence_wall=%d, influence_wall_look=%d, "
         "influence_wall_ripple=%d. %s", g_wall ? 1 : 0, g_look ? 1 : 0,
         g_ripple ? 1 : 0,
         !g_wall ? "No glow at the edge of the influence"
         : g_look
             ? "A red hologram shows the wall of the edge of the influence at "
               "each hand, from 45 cm of the room before the edge"
             : "A soft red patch shows the edge of the influence at each "
               "hand, from 20 cm of the room before the edge");
  if (g_stream_hand) {
    Hook(kManaPathSprite, reinterpret_cast<void*>(&h_ManaPath),
         reinterpret_cast<void**>(&o_ManaPath),
         "PSysGlobal::AddManaPathSprite (the stream of the influence)");
  }
  if (g_stream_log) {
    Hook(kSpellPoint, reinterpret_cast<void*>(&h_SpellPoint),
         reinterpret_cast<void**>(&o_SpellPoint),
         "SpellPointInf::SpellPointInf (test key influence_stream_test_log)");
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
  if (ControllersFrameCount() < g_test_frame) return;
  float hand[3];
  bool by_controller = false;
  if (!HandDirectHandPosition(hand, &by_controller) || !by_controller) return;
  g_circle[0] = hand[0];
  g_circle[1] = hand[2];
  g_circle[2] = static_cast<float>(g_test_cm) * 0.01f * units_per_meter;
  g_have_circle = true;
  ModLog("influence    : test key influence_test_circle_cm=%d: from frame "
         "%llu the influence of the player is a circle of %.1f units around "
         "%.1f %.1f (the land point of the Hand's point), for the fade, the "
         "glow and the stream", g_test_cm, ControllersFrameCount(),
         g_circle[2], g_circle[0], g_circle[1]);
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

// overlay.cpp, once a frame: the color of the influence ring of the player,
// 0 to 1, as the game draws the ring (see the top of this file).
void InfluenceEdgeColor(float rgb[3]) {
  void* player = HandDirectPlayer();
  if (player != nullptr) {
    typedef int(__fastcall * NumberFn)(void* self, void* unused);
    typedef int(__cdecl * RemapFn)(int number);
    const int number =
        reinterpret_cast<NumberFn>(kGetPlayerNumber)(player, nullptr);
    const int index = reinterpret_cast<RemapFn>(kGetRemappedPlayer)(number);
    const uint32_t* table = reinterpret_cast<const uint32_t*>(kRingColors);
    if (index >= 0 && index < 8 && !IsBadReadPtr(table + index, 4)) {
      const uint32_t c = table[index];
      g_color[0] = static_cast<float>((c >> 16) & 0xFFu) / 255.0f;
      g_color[1] = static_cast<float>((c >> 8) & 0xFFu) / 255.0f;
      g_color[2] = static_cast<float>(c & 0xFFu) / 255.0f;
      if (index != g_color_index) {
        g_color_index = index;
        ModLog("influence    : the glow at the edge has the color of the "
               "influence ring of the player: %d %d %d (player %d, ring "
               "color %d of the game)", static_cast<int>((c >> 16) & 0xFFu),
               static_cast<int>((c >> 8) & 0xFFu),
               static_cast<int>(c & 0xFFu), number, index);
      }
    }
  }
  for (int k = 0; k < 3; ++k) rgb[k] = g_color[k];
}

namespace {

// The edge near the hand by the search of 24 rays (the round patch, and the
// hologram where the circles do not agree with the test of the game). True
// when a ray found it, with the point and its distance along the land.
bool SearchEdge(const float point[3], float range, float at[2],
                float* distance, int* rays) {
  bool found[influence_math::kMaxDirections];
  float edge[influence_math::kMaxDirections][2];
  const auto inside = [](float x, float z) {
    bool in = true;
    if (!Inside(x, z, &in)) in = true;
    return in;
  };
  *rays = influence_math::FindEdge(inside, point[0], point[2], range,
                                   kWallRays, kWallSteps, kWallHalvings,
                                   found, edge);
  return *rays > 0 && influence_math::NearestEdge(point, found, edge,
                                                  kWallRays, at, distance);
}

// THE PLAN of the hologram of one hand for this frame: where its wall is and
// how it looks. InfluenceEdgeWallsFrame makes the plans of both hands before
// the build of either, so the build of one hand knows the other.
struct HoloPlan {
  bool on;       // the hand has a hologram this frame
  bool here;     // the hand is inside the influence
  int how;       // 1 the circles of the game, 2 the search, 0 or -1 none
  int circle;    // with how 1: the circle of the wall
  int rays;      // with how 2: the rays that found the edge
  float distance;
  float strength;
  float point[2];  // the land point of the hand
  influence_math::HoloWall wall;
  influence_math::HoloLook look;
};
HoloPlan g_plan[2];
influence_math::Circle g_circles[kMaxCircles];
int g_circle_count = 0;

void PlanHologram(int hand, const float point[3], float units_per_meter,
                  float seconds) {
  using namespace influence_math;
  HoloPlan& plan = g_plan[hand];
  plan.on = false;
  plan.how = 0;
  plan.circle = -1;
  plan.rays = 0;
  plan.distance = 0.0f;
  plan.strength = 0.0f;
  plan.point[0] = point[0];
  plan.point[1] = point[2];
  const float u = units_per_meter;
  bool here = true;
  if (!Inside(point[0], point[2], &here)) {
    plan.how = -1;
    return;
  }
  plan.here = here;
  const Circle* circles = g_circles;
  const int count = g_circle_count;
  const float margin = kHoloMarginM * u;
  const float range = (here ? kHoloInsideM : kHoloOutsideM) * u;
  HoloWall wall = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  EdgePoint e = {-1, {0.0f, 0.0f}, 0.0f, true};
  int how = 0;
  float distance = 0.0f;
  if (count > 0 &&
      NearestUnionEdge(circles, count, point[0], point[2], margin, &e)) {
    if (e.inside != here) {
      ++g_holo_disagree;
    } else if (e.distance > range) {
      // Far from the edge of the circles: no hologram, and no more tests.
      how = -1;
    } else {
      const Circle& c = circles[e.circle];
      float nx = e.at[0] - c.x, nz = e.at[1] - c.z;
      const float length = sqrtf(nx * nx + nz * nz);
      if (length > 1e-4f) {
        nx /= length;
        nz /= length;
        const float d = kHoloCheckM * u;
        bool in_side = false, out_side = true;
        Inside(e.at[0] - nx * d, e.at[1] - nz * d, &in_side);
        Inside(e.at[0] + nx * d, e.at[1] + nz * d, &out_side);
        if (in_side && !out_side) {
          wall.cx = c.x;
          wall.cz = c.z;
          wall.r = c.r;
          wall.qx = e.at[0];
          wall.qz = e.at[1];
          distance = e.distance;
          how = 1;
          plan.circle = e.circle;
        } else {
          ++g_holo_disagree;
        }
      }
    }
  }
  if (how == 0) {
    float at[2] = {0.0f, 0.0f};
    if (SearchEdge(point, range, at, &distance, &plan.rays)) {
      float tx = -(at[1] - point[2]), tz = at[0] - point[0];
      float length = sqrtf(tx * tx + tz * tz);
      if (!(length > 1e-6f)) {
        tx = 1.0f;
        tz = 0.0f;
        length = 1.0f;
      }
      wall.qx = at[0];
      wall.qz = at[1];
      wall.tx = tx / length;
      wall.tz = tz / length;
      how = 2;
    }
  }
  plan.how = how;
  plan.wall = wall;
  plan.distance = distance;
  plan.strength =
      how > 0 ? HologramStrength(here, distance, kHoloInsideM * u,
                                 kHoloOutsideM * u)
              : 0.0f;
  if (!(plan.strength > 0.0f)) return;
  // The middle is at the height of the hand, and never under the land at the
  // edge.
  float y = point[1];
  const float land = LandHeight(wall.qx, wall.qz);
  if (y < land) y = land;
  HoloLook& look = plan.look;
  look.y = y;
  look.radius = (kHoloSmallM + (kHoloLargeM - kHoloSmallM) * plan.strength) * u;
  look.light = kHoloLight * g_wall_light * plan.strength;
  look.grid = kHoloGridM * u;
  look.ring_gap = kHoloRingGapM * u;
  float phase = 0.5f;
  if (g_ripple && seconds >= 0.0f) {
    const float turns = seconds * kHoloRingSpeedMps / kHoloRingGapM;
    phase = turns - floorf(turns);
  }
  look.ring_phase = phase;
  look.line_half = kHoloLineHalfM * u;
  plan.on = true;
}

// The light of the hologram of a plan at the point p of the world, by its
// falloff from its middle. 0 off its disc.
float PlanLight(const HoloPlan& plan, const float p[3]) {
  if (!plan.on || !(plan.look.radius > 0.0f)) return 0.0f;
  const float dx = p[0] - plan.wall.qx, dy = p[1] - plan.look.y,
              dz = p[2] - plan.wall.qz;
  return plan.look.light *
         influence_math::HoloFalloff(sqrtf(dx * dx + dy * dy + dz * dz) /
                                     plan.look.radius);
}

// True when the two plans lie on the same wall: the same circle of the game.
bool SameWall(const HoloPlan& a, const HoloPlan& b) {
  return a.on && b.on && a.how == 1 && b.how == 1 && a.circle == b.circle;
}

// THE HOLOGRAM (influence_wall_look=1) of one hand, from its plan. Where the
// discs of the two hands overlap on one wall, each point of the wall shows
// only the disc that is brighter there (the left hand where they are equal).
// So the two discs make one hologram: one grid, one fill, and no doubled
// light.
int HologramWall(int hand, float units_per_meter,
                 influence_math::WallTriangle* out, int room) {
  using namespace influence_math;
  const float u = units_per_meter;
  const HoloPlan& plan = g_plan[hand];
  const HoloPlan& other = g_plan[1 - hand];
  const bool shared = SameWall(plan, other);
  int n = 0;
  if (plan.on) {
    const float margin = kHoloMarginM * u;
    const int own = plan.circle;
    const auto show = [&](float x, float z) {
      if (plan.how == 1 &&
          InOther(g_circles, g_circle_count, own, x, z, margin)) {
        return false;
      }
      if (!shared) return true;
      // The height of the point is not known here: the test takes the
      // height of the middle of this disc.
      const float p[3] = {x, plan.look.y, z};
      const float mine = PlanLight(plan, p), theirs = PlanLight(other, p);
      return hand == 0 ? mine >= theirs : mine > theirs;
    };
    n = BuildHologram(plan.wall, plan.look, show, out, room);
    if (plan.how == 1) {
      ++g_holo_by_circles;
    } else {
      ++g_holo_by_search;
    }
    if (shared) ++g_holo_shared;
  }
  const bool shows = n > 0;
  if (shows != g_wall_was[hand] && g_wall_logs < kMaxWallLogs) {
    ++g_wall_logs;
    ModLog("influence    : the hologram at the edge %s for the %s hand at "
           "%.1f %.1f, %s the influence, %.1f cm of the room from the edge, "
           "strength %.2f, %s, %d triangles, frame %llu",
           shows ? "shows" : "goes", hand == 0 ? "left" : "right",
           plan.point[0], plan.point[1],
           plan.here ? "inside" : "outside", plan.distance / u * 100.0f,
           plan.strength,
           plan.how == 1   ? "on the wall of a circle of the game"
           : plan.how == 2 ? "by the search of the rays"
                           : "no edge near",
           n, ControllersFrameCount());
  }
  // A line each 60 frames while it shows: the edge point, the wall and the
  // strength, for the steps of a desktop run.
  static uint64_t said_frame[2] = {0, 0};
  if (shows && g_holo_logs < kMaxHoloLogs &&
      ControllersFrameCount() >= said_frame[hand] + 60) {
    said_frame[hand] = ControllersFrameCount();
    ++g_holo_logs;
    const HoloWall& wall = plan.wall;
    if (plan.how == 1) {
      const float angle =
          atan2f(wall.qz - wall.cz, wall.qx - wall.cx) * 57.29578f;
      ModLog("influence    : the hologram of the %s hand: %.1f cm of the "
             "room %s the edge, strength %.2f, radius %.1f cm, edge point "
             "%.2f %.1f %.2f on the circle at %.1f %.1f of radius %.1f, at "
             "%.2f degrees on it, %d triangles%s, frame %llu",
             hand == 0 ? "left" : "right", plan.distance / u * 100.0f,
             plan.here ? "inside" : "past", plan.strength,
             plan.look.radius / u * 100.0f, wall.qx, plan.look.y, wall.qz,
             wall.cx, wall.cz, wall.r, angle, n,
             shared ? ", one hologram with the other hand" : "",
             ControllersFrameCount());
    } else {
      ModLog("influence    : the hologram of the %s hand: %.1f cm of the "
             "room %s the edge, strength %.2f, edge point %.2f %.1f %.2f by "
             "the search (%d rays), %d triangles, frame %llu",
             hand == 0 ? "left" : "right", plan.distance / u * 100.0f,
             plan.here ? "inside" : "past", plan.strength, wall.qx,
             plan.look.y, wall.qz, plan.rays, n, ControllersFrameCount());
    }
  }
  g_wall_was[hand] = shows;
  if (shows) {
    ++g_wall_frames;
    g_wall_triangles += static_cast<uint64_t>(n);
  }
  return n;
}

}  // namespace

// overlay.cpp, once a frame before InfluenceEdgeWall: the plans of the
// hologram of both hands, at the points of the hands in the world (valid[h]
// false for a hand that is not drawn), at `seconds` of the clock of the glow.
void InfluenceEdgeWallsFrame(const bool valid[2], const float points[2][3],
                             float units_per_meter, float seconds) {
  ReadKeys();
  for (int h = 0; h < 2; ++h) {
    g_plan[h].on = false;
    g_plan[h].how = -1;
  }
  if (!g_wall || !g_look || !(units_per_meter > 0.0f)) return;
  if (!valid[0] && !valid[1]) return;
  g_circle_count = GameCircles(g_circles, kMaxCircles);
  for (int h = 0; h < 2; ++h) {
    if (valid[h]) PlanHologram(h, points[h], units_per_meter, seconds);
  }
}

// overlay.cpp, once a frame for each hand (0 the left hand, 1 the right
// hand): the glow at the edge near the point of that hand in the world.
// Returns the count of triangles in `out`, at most `room`.
int InfluenceEdgeWall(int hand, const float point[3], float units_per_meter,
                      influence_math::WallTriangle* out, int room) {
  ReadKeys();
  if (hand < 0 || hand > 1) return 0;
  if (!g_wall || !(units_per_meter > 0.0f)) return 0;
  if (g_look) return HologramWall(hand, units_per_meter, out, room);
  bool here = true;
  if (!Inside(point[0], point[2], &here)) return 0;
  const float range =
      (here ? kWallRangeM : kWallOutsideM) * units_per_meter;
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
  float at[2] = {0.0f, 0.0f}, distance = 0.0f, strength = 0.0f;
  float y = point[1];
  int n = 0;
  if (count > 0 &&
      influence_math::NearestEdge(point, found, edge, kWallRays, at,
                                  &distance)) {
    strength = influence_math::GlowStrength(
        here, distance, kWallRangeM * units_per_meter,
        kWallTouchM * units_per_meter, kWallOutsideM * units_per_meter,
        kWallOutsideShare);
    // The middle of the patch is at the height of the hand, and never under
    // the land at the edge.
    const float land = LandHeight(at[0], at[1]);
    if (y < land) y = land;
    n = influence_math::BuildPatch(at, y, point,
                                   kPatchRadiusM * units_per_meter,
                                   kWallLight * g_wall_light * strength, out,
                                   room);
  }
  const bool shows = n > 0;
  if (shows != g_wall_was[hand] && g_wall_logs < kMaxWallLogs) {
    ++g_wall_logs;
    ModLog("influence    : the glow at the edge %s for the %s hand at %.1f "
           "%.1f, %s the influence, %.1f cm of the room from the edge, "
           "strength %.2f, %d of %d rays found the edge, frame %llu",
           shows ? "shows" : "goes", hand == 0 ? "left" : "right", point[0],
           point[2], here ? "inside" : "outside",
           distance / units_per_meter * 100.0f, strength, count, kWallRays,
           ControllersFrameCount());
  }
  // A line each 60 frames while it shows, with the strength, for the eye
  // pictures of a desktop run.
  static uint64_t said_frame[2] = {0, 0};
  if (shows && g_wall_logs < kMaxWallLogs &&
      ControllersFrameCount() >= said_frame[hand] + 60) {
    said_frame[hand] = ControllersFrameCount();
    ++g_wall_logs;
    ModLog("influence    : the glow of the %s hand: %.1f cm of the room %s "
           "the edge, strength %.2f, at %.1f %.1f %.1f, frame %llu",
           hand == 0 ? "left" : "right",
           distance / units_per_meter * 100.0f, here ? "inside" : "past",
           strength, at[0], y, at[1], ControllersFrameCount());
  }
  g_wall_was[hand] = shows;
  if (shows) {
    ++g_wall_frames;
    g_wall_triangles += static_cast<uint64_t>(n);
  }
  return n;
}

// The debug page (debug_menu.cpp): the look of the glow at the edge.
int InfluenceEdgeLookValue() {
  ReadKeys();
  if (!g_wall) return kDebugCannot;
  return g_look ? 1 : 0;
}

void InfluenceEdgeLookChange(const char* by) {
  ReadKeys();
  if (!g_wall) return;
  g_look = !g_look;
  for (int h = 0; h < 2; ++h) g_wall_was[h] = false;
  ControllersGesturePulse(0.5f, g_look ? 0.30f : 0.08f);
  ModLog("influence    : %s: influence_wall_look is %d now: %s, frame %llu",
         by, g_look ? 1 : 0,
         g_look ? "the hologram on the wall of the edge"
                : "the round patch at the edge",
         ControllersFrameCount());
}

// CHand+0x4850 is the GInterfaceStatus of the Hand, and its +0x130 is the
// GVirtualInfluence, which keeps the land point of the interface (+0x14 of
// the status) while it is inside the influence.
const int kHandStatus = 0x4850;
const int kStatusVirtual = 0x130;
const int kStatusPoint = 0x14;

// The state of the stream of the game, for the report: the last point inside
// (+0x08, 0 before the first one), the edge (+0x14), the strength (+0x30)
// and the switch of a script (+0x44, 1 is off).
void LogVirtualInfluence() {
  const uint8_t* hand = static_cast<const uint8_t*>(HandDirectGameHand());
  if (hand == nullptr || IsBadReadPtr(hand + kHandStatus, 4)) return;
  const uint8_t* status =
      *reinterpret_cast<const uint8_t* const*>(hand + kHandStatus);
  if (status == nullptr || IsBadReadPtr(status, kStatusVirtual + 4)) return;
  const uint8_t* v =
      *reinterpret_cast<const uint8_t* const*>(status + kStatusVirtual);
  if (v == nullptr || IsBadReadPtr(v, 0x48)) return;
  const int32_t* inside = reinterpret_cast<const int32_t*>(v + 0x08);
  const int32_t* edge = reinterpret_cast<const int32_t*>(v + 0x14);
  const int32_t* point = reinterpret_cast<const int32_t*>(status + kStatusPoint);
  const float* hand_point = reinterpret_cast<const float*>(hand + 0x78);
  ModLog("influence    : the stream of the game: the last point inside %.1f "
         "%.1f, the edge %.1f %.1f, strength %.2f, %s. The land point of the "
         "interface is %.1f %.1f, and the Hand's point is at %.1f %.1f",
         inside[0] / kMapScale, inside[1] / kMapScale, edge[0] / kMapScale,
         edge[1] / kMapScale, *reinterpret_cast<const float*>(v + 0x30),
         *reinterpret_cast<const int32_t*>(v + 0x44) != 0
             ? "off by a script"
             : "on",
         point[0] / kMapScale, point[1] / kMapScale, hand_point[0],
         hand_point[2]);
}

// Once a report window.
void InfluenceEdgeReport() {
  if (g_test_cm > 0 || g_stream_hand) LogVirtualInfluence();
  // influence_stream_test_log: the count of the streams, also when it is 0,
  // so the log shows that the hook was there and the game made none.
  if (g_stream_log) {
    static uint64_t said = 0;
    ModLog("influence    : the game made %llu streams of light in this "
           "window, %llu in all (influence_stream_test_log=1)",
           g_spell_points - said, g_spell_points);
    said = g_spell_points;
  }
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
  if (g_holo_by_circles > 0 || g_holo_by_search > 0 || g_holo_disagree > 0) {
    ModLog("influence    : the hologram: %llu frames on the wall of a circle "
           "of the game, %llu frames by the search of the rays, %llu tests "
           "where the circles and the test of the game did not agree",
           g_holo_by_circles, g_holo_by_search, g_holo_disagree);
    ModLog("influence    : %llu frames of a hand whose hologram is one with "
           "the hologram of the other hand on the same wall", g_holo_shared);
  }
  g_holo_by_circles = g_holo_by_search = g_holo_disagree = 0;
  g_holo_shared = 0;
  g_fade_frames = g_wall_frames = g_wall_triangles = 0;
  g_stream_calls = g_stream_lifted = g_raw_tests = 0;
}
