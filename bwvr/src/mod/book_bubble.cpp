// The game's own bubble of a miracle that the hand pulls out of the spell
// book. See docs/miracles.md, "The pull".
//
// WHY. The bubble on the elastic band of the book is the bubble of the game:
// the glass ball with the miracle in it, as the player sees it on the land.
// At the snap it goes, and the game's Miracle is in the Hand.
//
// WHAT THE GAME DRAWS [V]. A bubble on the land has two parts:
// - A glass ball: the mesh .\data\spells\meshes\O_Bibble_up.l3d with a
//   texture of 4 by 4 frames
//   (OneOffSpellSeed::CallVirtualFunctionsForCreation 0x0072A450).
// - A SpellSeedGraphic in the ball: the icon mesh of the miracle
//   (GSpellSeedInfo+0x140, drawn when +0x178 is 1) and the particles of the
//   miracle (+0x174). Storm, Fire, Lightning, Water and Teleport have
//   particles only.
// The gesture help picture of the game draws both parts at a free place, at
// once, with no object in the world (fn_0068B7E0). This file does the same.
//
// NOTHING IN THE WORLD [V]. SpellSeedGraphic::Create puts nothing on the map:
// its MapCoords only give the first matrix. No save writes a graphic that no
// object owns. With auto update off, the game turn does not step it. Its
// particles use the local random of the game, not the random of the game
// state. As the gesture help does, the mod keeps one graphic for each seed
// type and one glass ball, and does not delete them.
//
// WHERE. sky_detail.cpp calls BookBubbleDraw after the overlay's meshes, at
// 0x0082F4C9 in LH3DRender::FinishFrame. The sorted list is drawn at that
// point, so each part draws at once: the virtual function +0x104 of
// LH3DObject, and DrawSpellGraphic with now = 1, as in the gesture help. The
// depth buffer holds the world, so land in front hides the bubble. The camera
// of this eye is put in place for the draw (StereoReapplyEye), and the four
// camera matrices are put back after it, as OverlayEyeMatrix does. With one
// walk, the game's draws of the left eye pass go to the right eye as each
// other draw of the game.
//
// TIME. DrawUpdateAtPos and DrawSpellGraphic step the particles and the turn
// of the icon by LH3DTech::g_game_time_inc. stereo.cpp holds it at 0 in the
// pass of the right eye, so the bubble steps one time in each frame. The
// texture of the ball steps as OneOffSpellSeed::UpdateFrame 0x0072A570 does:
// 18 frames a second, of 16.
//
// THE SIZE. The ball has the radius kBubbleRadiusM in the room at the scale
// now. The mesh of the ball has a radius of 2.19 units at the scale 1. The
// graphic has 0.6 of the scale of the ball, as on the land
// (OneOffSpellSeed::GetSpellGraphicPos 0x0072A840).
//
// CHECKS. The first bytes of each function of the game that this file calls
// must be as in v1.42. If one is not, the file draws nothing, and the book
// shows its own picture of the bubble.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "book_bubble.h"
#include "spell_book_math.h"
#include "../common/bw_addresses.h"

extern void ModLog(const char* format, ...);

// stereo.cpp: the camera of this eye pass.
void StereoReapplyEye();
uint64_t ControllersFrameCount();

namespace {

// GGame and the players [V]: the players from +0x18, 0xA60 bytes each, and
// the index of the local player at +0x205A59.
const uintptr_t kGame = 0x00D0195C;
const int kGamePlayers = 0x18;
const int kGamePlayerSize = 0xA60;
const int kGameLocalPlayer = 0x205A59;

// The functions of the game [V].
const uintptr_t kSeedGraphicCreate = 0x00726F60;  // cdecl
const uintptr_t kSetAutoUpdate = 0x00727680;      // thiscall, writes 0
const uintptr_t kDrawUpdateAtPos = 0x00727630;    // thiscall
const uintptr_t kDrawSpellGraphic = 0x00519AD0;   // thiscall, ret 0x10
const uintptr_t kObjectCreate = 0x0080B4D0;       // fastcall, the type in ecx
const uintptr_t kGetSharedMesh = 0x0057DFB0;      // cdecl
const uintptr_t kMapCoordsMake = 0x00603160;      // thiscall, from an LHPoint
const uintptr_t kBallPath = 0x00C0285C;  // ".\data\spells\meshes\O_Bibble_up.l3d"
const uintptr_t kSeedGraphicTable = 0x00981AA8;

const uint8_t kSeedGraphicCreateBytes[] = {0x83, 0xEC, 0x0C, 0x56, 0x57, 0x68};
const uint8_t kSetAutoUpdateBytes[] = {0xC6, 0x41, 0x5C, 0x00, 0xC2, 0x04};
const uint8_t kDrawUpdateAtPosBytes[] = {0x8B, 0x44, 0x24, 0x08, 0x83, 0xEC};
const uint8_t kDrawSpellGraphicBytes[] = {0x83, 0xEC, 0x34, 0xA1, 0xF0, 0x1A};
const uint8_t kObjectCreateBytes[] = {0x6A, 0xFF, 0x68, 0xC3, 0x7E, 0x8A};
const uint8_t kGetSharedMeshBytes[] = {0x83, 0xEC, 0x10, 0x8A, 0x44, 0x24};
const uint8_t kMapCoordsMakeBytes[] = {0x33, 0xC0, 0x56, 0x8B, 0xF1, 0x89};
const char kBallPathText[] = ".\\data\\spells\\meshes\\O_Bibble_up.l3d";

using SeedGraphicCreateFn = void*(__cdecl*)(const void* coords, int seed,
                                            void* player, float scale,
                                            int power_up);
using SetAutoUpdateFn = void(__thiscall*)(void* graphic, int on);
using DrawUpdateAtPosFn = void(__thiscall*)(void* graphic, const float* matrix,
                                            float scale);
using DrawSpellGraphicFn = void(__thiscall*)(void* graphic, void* owner,
                                             int now, int collide, int alpha);
using ObjectCreateFn = void*(__fastcall*)(int type);
using GetSharedMeshFn = void*(__cdecl*)(const char* path,
                                        const uint8_t* properties);
using MapCoordsMakeFn = void*(__thiscall*)(void* coords, const float* point);

// The virtual functions of LH3DObject that the game calls on the ball [V,
// fn_0068B7E0 and OneOffSpellSeed::CallVirtualFunctionsForCreation].
using SetMeshFn = void(__fastcall*)(void* object, void* mesh, int, int);  // +0xF4
using SetWordFn = void(__fastcall*)(void* object, int value);  // +0x40, +0x78, +0x80
using SetFrameFn = void(__thiscall*)(void* object, float u, float v);  // +0xE8
using DrawNowFn = void(__thiscall*)(void* object);              // +0x104
using BoxFn = const uint8_t*(__thiscall*)(void* object);         // +0xF8

// LH3DObject [V]: the matrix (3 rows, then the place), the scale, and the
// color with the alpha in its top byte.
const int kObjectMatrix = 0x14;
const int kObjectScale = 0x44;
const int kObjectColor = 0x4C;

// The radius of the ball mesh at the scale 1, and the height of its middle
// over the origin of the mesh [V, the vertices of O_bibble_up.l3d].
const float kBallUnits = 2.19f;
const float kBallMiddle = 2.185f;
// The scale of the graphic, a share of the scale of the ball [V].
const float kGraphicShare = 0.6f;
// The texture of the ball: 18 frames a second, of 16 [V].
const float kBallFramesPerMs = 0.018f;
const float kBallFrames = 16.0f;

const int kSeedTypes = 30;

bool g_checked = false;
bool g_failed = false;
void* g_ball = nullptr;
float g_ball_middle[3] = {0.0f, kBallMiddle, 0.0f};
float g_ball_frame = 0.0f;
uint64_t g_ball_step_frame = 0;
void* g_graphic[kSeedTypes] = {};

// What shows in this frame. The game thread writes it in SpellBookFrame, and
// the draw of the same thread reads it.
int g_seed = -1;
float g_world[3] = {0.0f, 0.0f, 0.0f};
float g_units = 0.0f;
uint64_t g_frame = 0;

int g_shown_frames = 0;
int g_draws = 0;
int g_made = 0;
bool g_said_first = false;

bool SameBytes(uintptr_t address, const void* bytes, size_t count) {
  const void* at = reinterpret_cast<const void*>(address);
  return !IsBadReadPtr(at, count) && memcmp(at, bytes, count) == 0;
}

void** VTable(void* object) { return *static_cast<void***>(object); }

bool Check() {
  if (g_checked) return !g_failed;
  g_checked = true;
  struct Want {
    uintptr_t address;
    const void* bytes;
    size_t count;
    const char* name;
  };
  const Want wants[] = {
      {kSeedGraphicCreate, kSeedGraphicCreateBytes,
       sizeof(kSeedGraphicCreateBytes), "SpellSeedGraphic::Create"},
      {kSetAutoUpdate, kSetAutoUpdateBytes, sizeof(kSetAutoUpdateBytes),
       "SpellSeedGraphic::SetAutoUpdate"},
      {kDrawUpdateAtPos, kDrawUpdateAtPosBytes, sizeof(kDrawUpdateAtPosBytes),
       "SpellSeedGraphic::DrawUpdateAtPos"},
      {kDrawSpellGraphic, kDrawSpellGraphicBytes,
       sizeof(kDrawSpellGraphicBytes), "SpellSeedGraphic::DrawSpellGraphic"},
      {kObjectCreate, kObjectCreateBytes, sizeof(kObjectCreateBytes),
       "LH3DObject::Create"},
      {kGetSharedMesh, kGetSharedMeshBytes, sizeof(kGetSharedMeshBytes),
       "GJUtils::GetSharedMesh"},
      {kMapCoordsMake, kMapCoordsMakeBytes, sizeof(kMapCoordsMakeBytes),
       "MapCoords::MapCoords"},
      {kBallPath, kBallPathText, sizeof(kBallPathText), "the path of the ball"},
  };
  for (const Want& w : wants) {
    if (!SameBytes(w.address, w.bytes, w.count)) {
      g_failed = true;
      ModLog("book bubble  : %s at 0x%08X is not as in v1.42, so the book "
             "shows its own picture of the bubble", w.name,
             static_cast<unsigned>(w.address));
      return false;
    }
  }
  return true;
}

void* LocalPlayer() {
  const uint8_t* const* slot = reinterpret_cast<const uint8_t* const*>(kGame);
  if (IsBadReadPtr(slot, sizeof(void*)) || *slot == nullptr) return nullptr;
  uint8_t* game = const_cast<uint8_t*>(*slot);
  if (IsBadReadPtr(game + kGameLocalPlayer, 1)) return nullptr;
  return game + kGamePlayers + kGamePlayerSize * game[kGameLocalPlayer];
}

// The glass ball, as fn_0068B7E0 makes it.
bool MakeBall() {
  if (g_ball != nullptr) return true;
  void* ball = reinterpret_cast<ObjectCreateFn>(kObjectCreate)(0);
  if (ball == nullptr) return false;
  const uint8_t properties[8] = {1, 1, 0, 1, 1, 0, 0, 0};
  void* mesh = reinterpret_cast<GetSharedMeshFn>(kGetSharedMesh)(
      reinterpret_cast<const char*>(kBallPath), properties);
  if (mesh == nullptr) {
    g_failed = true;
    ModLog("book bubble  : the game gave no mesh of the glass ball, so the "
           "book shows its own picture of the bubble");
    return false;
  }
  void** table = VTable(ball);
  reinterpret_cast<SetMeshFn>(table[0xF4 / 4])(ball, mesh, 0, 0);
  reinterpret_cast<SetWordFn>(table[0x40 / 4])(ball, 1);
  reinterpret_cast<SetWordFn>(table[0x78 / 4])(ball, 0);
  reinterpret_cast<SetWordFn>(table[0x80 / 4])(ball, 0);
  // The middle of the box of the mesh, as GetSpellGraphicPos reads it.
  const uint8_t* box = reinterpret_cast<BoxFn>(table[0xF8 / 4])(ball);
  if (box != nullptr && !IsBadReadPtr(box + 0x18, 12)) {
    const float* middle = reinterpret_cast<const float*>(box + 0x18);
    if (fabsf(middle[0]) < 10.0f && fabsf(middle[1]) < 10.0f &&
        fabsf(middle[2]) < 10.0f) {
      for (int i = 0; i < 3; ++i) g_ball_middle[i] = middle[i];
    }
  }
  g_ball = ball;
  ++g_made;
  ModLog("book bubble  : the glass ball is made, its middle at %.3f %.3f %.3f "
         "of its mesh", g_ball_middle[0], g_ball_middle[1], g_ball_middle[2]);
  return true;
}

// The graphic of one seed type, as the gesture help makes it.
void* Graphic(int seed) {
  if (seed < 0 || seed >= kSeedTypes) return nullptr;
  if (g_graphic[seed] != nullptr) return g_graphic[seed];
  void* player = LocalPlayer();
  if (player == nullptr) return nullptr;
  const float zero[3] = {0.0f, 0.0f, 0.0f};
  uint8_t coords[16] = {};
  reinterpret_cast<MapCoordsMakeFn>(kMapCoordsMake)(coords, zero);
  void* graphic = reinterpret_cast<SeedGraphicCreateFn>(kSeedGraphicCreate)(
      coords, seed, player, 1.0f, -1);
  if (graphic == nullptr ||
      *static_cast<uintptr_t*>(graphic) != kSeedGraphicTable) {
    g_failed = true;
    ModLog("book bubble  : the game made no graphic of the seed type %d, so "
           "the book shows its own picture of the bubble", seed);
    return nullptr;
  }
  reinterpret_cast<SetAutoUpdateFn>(kSetAutoUpdate)(graphic, 0);
  g_graphic[seed] = graphic;
  ++g_made;
  ModLog("book bubble  : the graphic of the seed type %d is made", seed);
  return graphic;
}

// The texture of the ball steps once in each frame, by the time of the game.
void StepBall() {
  const uint64_t now = ControllersFrameCount();
  if (now == g_ball_step_frame) return;
  g_ball_step_frame = now;
  const uint32_t step =
      *reinterpret_cast<const uint32_t*>(bw::tech::kGameTimeInc);
  g_ball_frame = fmodf(g_ball_frame + step * kBallFramesPerMs, kBallFrames);
}

// The graphic of a miracle with its middle at the world point, at the scale
// of a ball of scale s. The camera of the eye must be in place.
void DrawGraphic(void* graphic, const float world[3], float s, int alpha) {
  // Only the place of the matrix counts.
  float middle[12] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  for (int i = 0; i < 3; ++i) middle[9 + i] = world[i];
  reinterpret_cast<DrawUpdateAtPosFn>(kDrawUpdateAtPos)(graphic, middle,
                                                        kGraphicShare * s);
  reinterpret_cast<DrawSpellGraphicFn>(kDrawSpellGraphic)(graphic, nullptr, 1,
                                                          0, alpha);
}

// The glass ball with its middle at the world point: no turn, the scale s.
void DrawBall(const float world[3], float s, int alpha) {
  StepBall();
  const int f = static_cast<int>(g_ball_frame);
  float ball[12] = {s, 0.0f, 0.0f, 0.0f, s, 0.0f, 0.0f, 0.0f, s};
  for (int i = 0; i < 3; ++i) ball[9 + i] = world[i] - g_ball_middle[i] * s;
  uint8_t* object = static_cast<uint8_t*>(g_ball);
  memcpy(object + kObjectMatrix, ball, sizeof(ball));
  memcpy(object + kObjectScale, &s, sizeof(s));
  const uint32_t color =
      (static_cast<uint32_t>(alpha & 0xFF) << 24) | 0x00FFFFFFu;
  memcpy(object + kObjectColor, &color, sizeof(color));
  void** table = VTable(g_ball);
  reinterpret_cast<SetFrameFn>(table[0xE8 / 4])(g_ball, (f % 4) * 0.25f,
                                                (f / 4) * 0.25f);
  reinterpret_cast<DrawNowFn>(table[0x104 / 4])(g_ball);
}

}  // namespace

void BookBubbleShow(int seed, const float world[3], float units_per_meter,
                    uint64_t frame) {
  if (g_failed || seed < 0 || seed >= kSeedTypes ||
      !(units_per_meter > 0.0f)) {
    return;
  }
  g_seed = seed;
  for (int i = 0; i < 3; ++i) g_world[i] = world[i];
  g_units = units_per_meter;
  g_frame = frame;
  ++g_shown_frames;
}

void BookBubbleDraw() {
  if (g_seed < 0 || g_failed) return;
  // Only the bubble of this frame or of the frame before: a pull that
  // stopped draws nothing.
  const uint64_t now = ControllersFrameCount();
  if (now > g_frame + 1) {
    g_seed = -1;
    return;
  }
  if (!Check() || !MakeBall()) return;
  void* graphic = Graphic(g_seed);
  if (graphic == nullptr) return;

  const float s = spell_book_math::kBubbleRadiusM * g_units / kBallUnits;

  const size_t bytes = sizeof(float) * 12;
  float scaled[12], unscaled[12], copy[12], inverse[12];
  memcpy(scaled, reinterpret_cast<const void*>(bw::tech::kWorldToCamera),
         bytes);
  memcpy(unscaled,
         reinterpret_cast<const void*>(bw::tech::kWorldToCameraUnscaled),
         bytes);
  memcpy(copy, reinterpret_cast<const void*>(bw::tech::kWorldToCameraCopy),
         bytes);
  memcpy(inverse,
         reinterpret_cast<const void*>(bw::tech::kCameraToWorldInverse),
         bytes);
  StereoReapplyEye();

  // The miracle first, then the glass over it.
  DrawGraphic(graphic, g_world, s, 0xFF);
  DrawBall(g_world, s, 0xFF);

  memcpy(reinterpret_cast<void*>(bw::tech::kWorldToCamera), scaled, bytes);
  memcpy(reinterpret_cast<void*>(bw::tech::kWorldToCameraUnscaled), unscaled,
         bytes);
  memcpy(reinterpret_cast<void*>(bw::tech::kWorldToCameraCopy), copy, bytes);
  memcpy(reinterpret_cast<void*>(bw::tech::kCameraToWorldInverse), inverse,
         bytes);
  ++g_draws;
  if (!g_said_first) {
    g_said_first = true;
    ModLog("book bubble  : the game draws the bubble of the seed type %d at "
           "%.2f %.2f %.2f, the ball %.1f cm across at the scale %.4f, frame "
           "%llu", g_seed, g_world[0], g_world[1], g_world[2],
           2.0f * spell_book_math::kBubbleRadiusM * 100.0f, s, now);
  }
}

void BookBubbleDrawPart(int seed, const float world[3], float radius_units,
                        int alpha) {
  if (g_failed || !(radius_units > 0.0f) || !Check()) return;
  const float s = radius_units / kBallUnits;
  if (seed < 0) {
    if (MakeBall()) DrawBall(world, s, alpha);
    return;
  }
  void* graphic = Graphic(seed);
  if (graphic != nullptr) DrawGraphic(graphic, world, s, alpha);
}

bool BookBubbleFailed() { return g_failed; }

void BookBubbleCounts(int* shown_frames, int* draws, int* made) {
  *shown_frames = g_shown_frames;
  *draws = g_draws;
  *made = g_made;
  g_shown_frames = 0;
  g_draws = 0;
  g_made = 0;
}
