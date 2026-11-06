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
//   influence_wall         a soft round glow lies on the edge at the point
//                          nearest to each hand, at the height of the hand,
//                          while the hand is less than 20 cm of the room
//                          inside the edge, and fainter while the hand is
//                          past it (influence_edge_math.h, GlowStrength). It
//                          has the red of the game's own influence ring: the
//                          color of the ring of the player in the table at
//                          0x00EA9EFC, which fn_00826510 fills and
//                          InfluenceCircle (fn_008265F0) reads by the player
//                          index of GetRemapedPlayer 0x0064D790. overlay.cpp
//                          draws it with the glow, as added light
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
#include "influence_edge_math.h"

extern void ModLog(const char* format, ...);
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
  ModLog("influence    : influence_wall=%d. %s", g_wall ? 1 : 0,
         g_wall ? "A soft red glow shows the edge of the influence at each "
                  "hand, from 20 cm of the room before the edge"
                : "No glow at the edge of the influence");
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

// overlay.cpp, once a frame for each hand (0 the left hand, 1 the right
// hand): the glow at the edge near the point of that hand in the world.
// Returns the count of triangles in `out`, at most `room`.
int InfluenceEdgeWall(int hand, const float point[3], float units_per_meter,
                      influence_math::WallTriangle* out, int room) {
  ReadKeys();
  if (hand < 0 || hand > 1) return 0;
  if (!g_wall || !(units_per_meter > 0.0f)) return 0;
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
