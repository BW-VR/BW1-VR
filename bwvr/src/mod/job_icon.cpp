// THE JOB ICON AND THE NAME OVER A VILLAGER (job_icon_scale_pct,
// villager_name_scale_pct, job_icon_math.h, and docs/village.md, "The job
// icon and the name over a villager").
//
// THE ICON. GInterface::PreDrawProcess (0x005CE9E0) gives one villager to
// Villager::ShowDiscipleIcon (0x00756170): the villager in the Hand, else the
// villager under the Hand, when the word at +0xE0 has the bit 0x200. That
// function keeps his 3D object (+0x40) at 0x00EB99E4 and the icon of his job
// at 0x00EB99E8. The object draws (fn_00812170, fn_00815A70) call
// fn_008241B0 with the top of the bounding sphere and the radius. When the
// object is the one at 0x00EB99E4, that function fills the sprite at
// 0x00EB99D8 and gives it to the Z sort with LH3DSprite::AddDrawing
// (0x00840C70, the call at 0x008243E2). The half size is the distance from
// the camera times 8 / half_res_x, so the icon has one angle at each
// distance, and that angle is smaller at a larger render. [V]
//
// The hook of AddDrawing acts only on that call. It makes the icon
// job_icon_scale_pct / 100 times the size of the game, with its lower edge
// at the place of the game (job_icon_math::ScaleIcon). The game computes the
// size again in each pass, so the hook scales each pass one time. With 100
// the hook changes nothing and only reads the icon for the log.
//
// THE NAME. VillagerName::Add (0x007629E0) makes the card of the name of a
// person of a quest (the action Show Villager Names) and of the details of a
// villager (Show Villager Details). It gets a fade of 0 to 1, and its text
// is 16 px of the render high at full fade: the fade times the 16.0 at
// 0x008AB6E8 (0x00762BA5). The edge of the text is the fade times the 6.0 at
// 0x008AB35C (0x00762BBF), and the box comes from the text and the edge.
// With villager_name_scale_pct the two instructions read floats of the mod,
// 16 and 6 times the scale, so the game makes the whole card larger with its
// own layout. With 100 the mod does not change the code. The hook of Add
// only reads the card for the log. [V]

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../common/bw_addresses.h"
#include "MinHook.h"
#include "job_icon_math.h"

extern void ModLog(const char* format, ...);
// sprite_up.cpp: the turn that LH3DSprite::Draw adds to a sprite now.
float SpriteUpTurnNow();

namespace {

using job_icon_math::Sprite;

// LH3DSprite::AddDrawing: __thiscall, the sprite in ecx, no argument, a
// plain ret.
const uintptr_t kAddDrawing = 0x00840C70;
const uint8_t kAddDrawingBytes[6] = {0xD9, 0x41, 0x08, 0x6A, 0x00, 0x51};
// The return address of the call for the job icon in fn_008241B0.
const uintptr_t kIconFrom = 0x008243E7;
const uintptr_t kIconSprite = 0x00EB99D8;  // LH3DSprite*
const uintptr_t kIconKind = 0x00EB99E8;    // the icon of ShowDiscipleIcon
const int kSpriteAngle = 0x14;
const uintptr_t kCameraMatrix = 0x00EA1D28;

// VillagerName::Add: cdecl (float fade, LHPoint place, const wchar_t* text,
// uint32_t* color), the name or 0.
const uintptr_t kNameAdd = 0x007629E0;
const uint8_t kNameAddBytes[5] = {0xA1, 0x5C, 0x19, 0xD0, 0x00};
// fmul dword ptr [0x008AB6E8] (16.0) and fmul dword ptr [0x008AB35C] (6.0)
// in Add. The mod changes the four bytes of the address in each one.
const uintptr_t kNameHeightMul = 0x00762BA5;
const uint8_t kNameHeightBytes[6] = {0xD8, 0x0D, 0xE8, 0xB6, 0x8A, 0x00};
const uintptr_t kNameEdgeMul = 0x00762BBF;
const uint8_t kNameEdgeBytes[6] = {0xD8, 0x0D, 0x5C, 0xB3, 0x8A, 0x00};
const float kNameHeight = 16.0f;
const float kNameEdge = 6.0f;
// The fields of a VillagerName that Add writes.
const int kNameDepth = 0x1C;       // the depth of the villager in the camera
const int kNameTextHeight = 0x20;  // px of the render, a line of the text
const int kNameLines = 0x28;

typedef void(__fastcall* AddDrawingFn)(void* self, void* unused);
typedef void*(__cdecl* NameAddFn)(float fade, float x, float y, float z,
                                  const wchar_t* text, uint32_t* color);
AddDrawingFn o_AddDrawing = nullptr;
NameAddFn o_NameAdd = nullptr;

int g_icon_pct = 200;  // job_icon_scale_pct
int g_name_pct = 100;  // villager_name_scale_pct
float g_icon_k = 2.0f;
float g_name_k = 1.0f;
// The floats that the two instructions of Add read with a scale.
float g_name_height = kNameHeight;
float g_name_edge = kNameEdge;

// For the log.
const int kMaxLogs = 4;
int g_icon_logs = 0;
int g_name_logs = 0;
uint64_t g_icons = 0;
float g_icon_game_deg = 0.0f;
float g_icon_deg = 0.0f;
float g_icon_px = 0.0f;
float g_icon_distance = 0.0f;
uint64_t g_names = 0;
float g_name_px = 0.0f;
float g_name_deg = 0.0f;
float g_name_depth = 0.0f;

void Load() {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash =
      length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  g_icon_pct = GetPrivateProfileIntA("bwvr", "job_icon_scale_pct", 200, path);
  g_name_pct =
      GetPrivateProfileIntA("bwvr", "villager_name_scale_pct", 100, path);
  g_icon_k = job_icon_math::ScaleFromPercent(g_icon_pct);
  g_name_k = job_icon_math::ScaleFromPercent(g_name_pct);
  g_icon_pct = static_cast<int>(g_icon_k * 100.0f + 0.5f);
  g_name_pct = static_cast<int>(g_name_k * 100.0f + 0.5f);
}

// The focal lengths of the picture of this pass, in px.
bool Focal(float* x, float* y) {
  const bw::tech::InfoTransform* info =
      reinterpret_cast<const bw::tech::InfoTransform*>(
          bw::tech::kInfoTransform);
  if (IsBadReadPtr(info, sizeof(*info))) return false;
  *x = info->half_res_x * info->inv_half_tan_fov_x;
  *y = info->half_res_y * info->inv_half_tan_fov_y;
  return *x > 0.0f && *y > 0.0f;
}

void ScaleTheIcon(uint8_t* sprite) {
  const float* m = reinterpret_cast<const float*>(kCameraMatrix);
  float* f = reinterpret_cast<float*>(sprite);
  Sprite in;
  for (int i = 0; i < 3; ++i) in.place[i] = f[i];
  in.size = f[3];
  in.aspect = f[4];
  in.off_x = f[6];
  in.off_y = f[7];
  const float angle =
      *reinterpret_cast<const float*>(sprite + kSpriteAngle) +
      SpriteUpTurnNow();
  const Sprite out = job_icon_math::ScaleIcon(in, m, angle, g_icon_k);
  for (int i = 0; i < 3; ++i) f[i] = out.place[i];
  f[3] = out.size;
  f[6] = out.off_x;
  f[7] = out.off_y;

  // The numbers for the log. The game takes the distance from the stored
  // camera of the pass.
  const float* camera = reinterpret_cast<const float*>(bw::tech::kCameraPos);
  float d2 = 0.0f;
  for (int i = 0; i < 3; ++i) {
    d2 += (in.place[i] - camera[i]) * (in.place[i] - camera[i]);
  }
  const float distance = sqrtf(d2);
  float fx = 0.0f, fy = 0.0f;
  Focal(&fx, &fy);
  ++g_icons;
  g_icon_distance = distance;
  g_icon_game_deg = job_icon_math::AcrossDegrees(in.size, distance);
  g_icon_deg = job_icon_math::AcrossDegrees(out.size, distance);
  g_icon_px = job_icon_math::AcrossPixels(
      out.size, job_icon_math::Depth(m, out.place), fx);
  if (g_icon_logs < kMaxLogs) {
    ++g_icon_logs;
    float rise = 0.0f;
    for (int i = 0; i < 3; ++i) {
      rise += (out.place[i] - in.place[i]) * (out.place[i] - in.place[i]);
    }
    ModLog("job icon     : the job icon %d over a villager at %.1f units from "
           "the camera: the game gives a half size of %.3f units (%.2f "
           "degrees across, %.1f px of the render). The mod draws it %.2f "
           "degrees across (%.1f px, %.1f px in a degree), with its lower "
           "edge in place and its middle %.3f units higher (aspect %.2f, "
           "offsets %.3f %.3f, job_icon_scale_pct=%d)",
           *reinterpret_cast<const int*>(kIconKind), distance, in.size,
           g_icon_game_deg,
           job_icon_math::AcrossPixels(in.size,
                                       job_icon_math::Depth(m, in.place), fx),
           g_icon_deg, g_icon_px, job_icon_math::PixelsPerDegree(fx),
           sqrtf(rise), in.aspect, in.off_x, in.off_y, g_icon_pct);
  }
}

void __fastcall h_AddDrawing(void* self, void* unused) {
  if (self != nullptr &&
      reinterpret_cast<uintptr_t>(_ReturnAddress()) == kIconFrom &&
      self == *reinterpret_cast<void* const*>(kIconSprite)) {
    ScaleTheIcon(static_cast<uint8_t*>(self));
  }
  o_AddDrawing(self, unused);
}

void* __cdecl h_NameAdd(float fade, float x, float y, float z,
                        const wchar_t* text, uint32_t* color) {
  void* name = o_NameAdd(fade, x, y, z, text, color);
  if (name == nullptr) return name;
  const uint8_t* n = static_cast<const uint8_t*>(name);
  const float px = *reinterpret_cast<const float*>(n + kNameTextHeight);
  float fx = 0.0f, fy = 0.0f;
  Focal(&fx, &fy);
  ++g_names;
  g_name_px = px;
  g_name_deg = job_icon_math::PixelsDegrees(px, fy);
  g_name_depth = *reinterpret_cast<const float*>(n + kNameDepth);
  if (g_name_logs < kMaxLogs) {
    ++g_name_logs;
    ModLog("names        : a name card of %d line(s) at %.1f units deep: a "
           "line of its text is %.1f px of the render high (%.1f px at the "
           "size of the game), %.2f degrees in the middle of the view, at "
           "%.1f px in a degree (fade %.2f, villager_name_scale_pct=%d)",
           *reinterpret_cast<const int*>(n + kNameLines), g_name_depth, px,
           px / g_name_k, g_name_deg, job_icon_math::PixelsPerDegree(fy),
           fade, g_name_pct);
  }
  return name;
}

bool Hook(uintptr_t address, const uint8_t* bytes, size_t count,
          void* detour, void** original, const char* what) {
  void* target = reinterpret_cast<void*>(address);
  if (IsBadReadPtr(target, count) || memcmp(target, bytes, count) != 0) {
    ModLog("job icon     : %s at 0x%08X does not have the bytes of the "
           "disassembly, so the mod does not change it", what,
           static_cast<unsigned>(address));
    return false;
  }
  if (MH_CreateHook(target, detour, original) != MH_OK ||
      MH_EnableHook(target) != MH_OK) {
    ModLog("job icon     : %s at 0x%08X FAILED to hook", what,
           static_cast<unsigned>(address));
    return false;
  }
  return true;
}

// Points the fmul at `address` to `value`. False when the code is not the
// code of the disassembly.
bool PointFmulAt(uintptr_t address, const uint8_t bytes[6],
                 const float* value) {
  uint8_t* code = reinterpret_cast<uint8_t*>(address);
  if (IsBadReadPtr(code, 6) || memcmp(code, bytes, 6) != 0) return false;
  DWORD old = 0;
  if (!VirtualProtect(code + 2, 4, PAGE_EXECUTE_READWRITE, &old)) {
    return false;
  }
  const uint32_t target = static_cast<uint32_t>(
      reinterpret_cast<uintptr_t>(value));
  memcpy(code + 2, &target, 4);
  VirtualProtect(code + 2, 4, old, &old);
  FlushInstructionCache(GetCurrentProcess(), code, 6);
  return true;
}

void InstallNameScale() {
  if (g_name_k == 1.0f) return;
  g_name_height = kNameHeight * g_name_k;
  g_name_edge = kNameEdge * g_name_k;
  // Check both before the first change, so the card never gets one scale
  // only.
  const uint8_t* a = reinterpret_cast<const uint8_t*>(kNameHeightMul);
  const uint8_t* b = reinterpret_cast<const uint8_t*>(kNameEdgeMul);
  if (IsBadReadPtr(a, 6) || IsBadReadPtr(b, 6) ||
      memcmp(a, kNameHeightBytes, 6) != 0 ||
      memcmp(b, kNameEdgeBytes, 6) != 0) {
    ModLog("names        : VillagerName::Add does not have the bytes of the "
           "disassembly at 0x%08X and 0x%08X, so the names have the size of "
           "the game", static_cast<unsigned>(kNameHeightMul),
           static_cast<unsigned>(kNameEdgeMul));
    return;
  }
  PointFmulAt(kNameHeightMul, kNameHeightBytes, &g_name_height);
  PointFmulAt(kNameEdgeMul, kNameEdgeBytes, &g_name_edge);
}

}  // namespace

// From mod_main.cpp, with the other hooks of the stereo picture.
void JobIconInstall() {
  Load();
  ModLog("job icon     : job_icon_scale_pct=%d. %s", g_icon_pct,
         g_icon_k != 1.0f
             ? "The job icon over a villager in the Hand or under it has "
               "that percent of the size that the game gives, and its lower "
               "edge stays where the game puts it"
             : "The job icon has the size that the game gives");
  ModLog("names        : villager_name_scale_pct=%d. %s", g_name_pct,
         g_name_k != 1.0f
             ? "The card of the name and of the details of a villager has "
               "that percent of the size that the game gives"
             : "The card of the name and of the details of a villager has "
               "the size that the game gives");
  // With 100 the hook of AddDrawing only reads the icon for the log.
  Hook(kAddDrawing, kAddDrawingBytes, sizeof(kAddDrawingBytes),
       reinterpret_cast<void*>(&h_AddDrawing),
       reinterpret_cast<void**>(&o_AddDrawing), "LH3DSprite::AddDrawing");
  InstallNameScale();
  Hook(kNameAdd, kNameAddBytes, sizeof(kNameAddBytes),
       reinterpret_cast<void*>(&h_NameAdd),
       reinterpret_cast<void**>(&o_NameAdd), "VillagerName::Add");
}

void JobIconReport() {
  if (g_icons > 0) {
    ModLog("job icon     : %llu draws of the job icon this window. The last "
           "one is %.2f degrees across (%.2f as the game gives it), %.1f px "
           "of the render, at %.1f units", g_icons, g_icon_deg,
           g_icon_game_deg, g_icon_px, g_icon_distance);
    g_icons = 0;
  }
  if (g_names > 0) {
    ModLog("names        : %llu name cards this window. The last one has a "
           "line of %.1f px of the render, %.2f degrees, at %.1f units deep",
           g_names, g_name_px, g_name_deg, g_name_depth);
    g_names = 0;
  }
}
