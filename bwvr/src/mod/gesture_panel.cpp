// The game's gesture pictures over the palm. See gesture_panel.h and
// docs/miracles.md, "The gesture row over the palm".
//
// WHAT THE GAME DRAWS [V]. At the end of each run of ProcessPowerUpSystem the
// game gives its "looking for" list to fn_0068ABA0. When the list changes,
// that makes one DisplayGesture for each gesture type that the player can
// draw now (PSysGesture.cpp), in the list at 0x00D4EAC8, the count at
// 0x00D4EAD0. PSysGlobal::DrawLoop (fn_0068AA60) puts each one on the sorted
// list of the frame, with the draw fn_0068B480. That draw has three parts:
// - fn_0068B7E0: the bubble of the miracle. It gives the engine a camera of
//   its own (fn_0068B740), moves the middle of the projection to the place
//   of the picture on the screen, draws, and puts the stored camera back
//   (fn_0068B7D0). A chain start has an empty glass ball, a leash its own
//   mesh, and a miracle its SpellSeedGraphic.
// - the card: a sprite with the picture of the gesture
//   (data\textures\S_Gesture*.raw).
// - a dot that runs along the path of the gesture, two times when the path
//   comes round.
// The card and the dot get their place from fn_0068B420: a point of the
// square of the picture on the screen, 10 units in front of the camera
// (LH3DTech::Get3DPointFromScreen). The squares are at the lower right of
// the game screen, each 3.5 percent of its width, in a row from the right
// (fn_0068AED0). The pictures fade out while the help system shows a text.
//
// WHY THEY DO NOT SHOW IN THE HEADSET. See docs/miracles.md, "Where the
// gesture pictures of the game go".
//
// WHAT THIS FILE DOES (gesture_panel=1). The draw of the sorted list draws no
// picture. In each eye pass, after the game's bubble of the spell book, this
// file calls the game's own draw of each picture, with three changes:
// - fn_0068B420 gives a point of a plane over the palm of the hand that does
//   not point, in place of the point in front of the camera. The plane
//   faces the head, and the row of the game is gesture_panel_cm wide on it.
// - fn_0068B7E0 draws the bubble at its point of the plane, with the glass
//   ball and the graphics of book_bubble.cpp. The leash meshes do not show.
// - The sizes of the card and of the dot (0x00C02674 and 0x00D4EAFC) are
//   scaled for the plane while the draw runs, and then put back.
// The camera of this eye is put in place for the draw and put back after
// it, as book_bubble.cpp does. The card and the dot draw with no depth test,
// as in the game, so they show over the hand.
//
// WHEN. The row shows while the palm is up (gesture_panel_math.h), the game
// has a picture, the spell book is closed, and no ring, dialog box or cut
// scene window is up. With spell_book_palm=1 a palm that turns up at rest
// opens the book. Then the row wants the palm only while the draw button is
// down, or while the game shows a picture that goes on from a gesture that
// the player drew: a chain, a power up, the scribble.
//
// CHECKS. The first bytes of each function of the game that this file hooks
// or calls must be as in v1.42. If one is not, this file hooks nothing, and
// the game draws its pictures as before.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "MinHook.h"
#include "book_bubble.h"
#include "controllers.h"
#include "cutscene_window.h"
#include "gesture_panel.h"
#include "gesture_panel_math.h"
#include "radial_menu.h"
#include "spell_book.h"
#include "spell_book_math.h"
#include "../common/bw_addresses.h"

extern void ModLog(const char* format, ...);
uint64_t ControllersFrameCount();
uint64_t ControllersTestFrame(uint64_t key_frame, uint64_t frame);
bool ControllersCardPose(int which, float grip_position[3],
                         float grip_orientation[4], float head[3],
                         bool* left);
bool ControllersGestureInput(GestureInput* out);
// stereo.cpp: the camera of this eye pass.
void StereoReapplyEye();

namespace {

using namespace gesture_panel_math;

// The list of the pictures [V].
const uintptr_t kList = 0x00D4EAC8;       // DisplayGesture**
const uintptr_t kListCount = 0x00D4EAD0;  // int
const int kMaxList = 24;
// DisplayGesture [V].
const int kItemType = 0x0C;     // the gesture type
const int kItemSeed = 0x10;     // the seed type of the miracle, or -1
const int kItemLeash = 0x14;    // the leash type, or -1
const int kItemOrder = 0x34;    // GESTURE_DISPLAY_ORDER
const int kItemTopLeft = 0x44;  // x, y, z: the square on the screen, pixels
const int kItemBottomRight = 0x50;
// fn_0068B7E0 [V]: display order 9 shows the bubble of seed type 0x16, and
// display order 8 with no seed and no leash shows the empty glass ball.
const int kOrderAgain = 9;
const int kOrderStart = 8;
const int kAgainSeed = 0x16;
// The layout of the squares, shares of the width of the game screen [V].
const uintptr_t kSideShare = 0x00C02654;  // 0.035
const uintptr_t kGapShare = 0x00C02658;   // 0.005
// The sizes of the card and of the dot, written into the sprites by the
// draw [V].
const uintptr_t kCardSize = 0x00C02674;  // 0.2
const uintptr_t kDotSize = 0x00D4EAFC;
// LH3DTech::Get3DPointFromScreen: x of a pixel is (px - kHalfX) * kTanX /
// kHalfX at the near plane [V].
const uintptr_t kTanX = 0x00C3812C;
const uintptr_t kHalfX = 0x00E839F0;
const float kGameDepth = 10.0f;  // fn_0068B420 asks for 10 units
// The camera place [V].
const uintptr_t kCamera = 0x00EA1DB8;
// The game screen: the width in the low 16 bits, the height in the high 16
// bits [V].
const uintptr_t kScreenSize = 0x00E85058;
// The fade of the pictures, 0 to 1 (fn_0068AA60) [V].
const uintptr_t kFade = 0x00C02710;
// GSpellSystemInfo: the gesture types of "again" and of the two chain
// starts. These three are the pictures of an empty Hand at rest [V].
const uintptr_t kAgainType = 0x00DA0570;
const uintptr_t kStartA = 0x00DA0574;
const uintptr_t kStartB = 0x00DA0578;

// The functions [V].
const uintptr_t kDraw = 0x0068B480;    // thiscall, no arguments
const uintptr_t kPoint = 0x0068B420;   // thiscall (unit, out), ret 8
const uintptr_t kBubble = 0x0068B7E0;  // thiscall (seed, leash), ret 8
const uint8_t kDrawBytes[] = {0x83, 0xEC, 0x10, 0x53, 0x56, 0x8B, 0xF1,
                              0x8B, 0x4E, 0x1C};
const uint8_t kPointBytes[] = {0x83, 0xEC, 0x08, 0xD9, 0x41, 0x50, 0x8B,
                               0x44, 0x24, 0x0C};
const uint8_t kBubbleBytes[] = {0x83, 0xEC, 0x64, 0x53, 0x55, 0x56, 0x57,
                                0x8B, 0xF9, 0x83, 0x7F, 0x34, 0x09};

using DrawFn = void(__fastcall*)(void* item, void* unused);
using PointFn = void(__fastcall*)(void* item, void* unused, const float* unit,
                                  float* out);
using BubbleFn = void(__fastcall*)(void* item, void* unused, int seed,
                                   int leash);
DrawFn o_Draw = nullptr;
PointFn o_Point = nullptr;
BubbleFn o_Bubble = nullptr;

// The ball of a bubble on the plane: this share of the side of a square.
const float kBallShare = 0.45f;

bool g_read = false;
bool g_on = true;
int g_width_cm = 30;
bool g_hooked = false;
// gesture_panel_test_frame: at this frame of the scripted controller
// gesture_panel changes, so one desktop run shows both. A test key.
uint64_t g_test_frame = 0;
bool g_test_done = false;

// The state of this frame. GesturePanelFrame writes it on the game thread,
// and the draw of the same thread reads it.
bool g_palm_up = false;
uint64_t g_show_frame = 0;
float g_w0[3], g_wr[3], g_wu[3];  // the plane in the world: the middle, and
                                  // the world for one meter right and up
float g_units = 0.0f;             // world units in a meter of the room
bool g_goes_on = false;           // a picture goes on from a drawn gesture
bool g_draw_held = false;

// The draw now.
bool g_in_panel = false;
RowMap g_map = {};
float g_side_px = 0.0f;

// The counts and the log.
uint64_t g_draws = 0, g_corner_skips = 0, g_corner_draws = 0;
int g_logs = 0;
const int kMaxLogs = 300;
int g_logged_count = -1;
int g_logged_types[kMaxList];
bool g_said_corner = false;
bool g_said_first = false;
bool g_showing = false;
const char* g_hide_why = "";

bool LogRoom() {
  if (g_logs >= kMaxLogs) return false;
  ++g_logs;
  return true;
}

bool SameBytes(uintptr_t address, const uint8_t* bytes, size_t count) {
  const void* at = reinterpret_cast<const void*>(address);
  return !IsBadReadPtr(at, count) && memcmp(at, bytes, count) == 0;
}

template <typename T>
T Read(uintptr_t address) {
  return *reinterpret_cast<const T*>(address);
}

template <typename T>
T Field(const void* item, int offset) {
  return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(item) +
                                     offset);
}

int IniKey(const char* key, int fallback) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return fallback;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return fallback;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  g_on = IniKey("gesture_panel", 1) != 0;
  g_width_cm = IniKey("gesture_panel_cm", 30);
  if (g_width_cm < 10) g_width_cm = 10;
  if (g_width_cm > 80) g_width_cm = 80;
  const int test = IniKey("gesture_panel_test_frame", 0);
  g_test_frame = test > 0 ? static_cast<uint64_t>(test) : 0;
}

// The pictures of this frame. Returns the count, 0 with none.
int List(void* out[kMaxList]) {
  const int count = Read<int>(kListCount);
  void* const* items = Read<void* const*>(kList);
  if (count <= 0 || items == nullptr ||
      IsBadReadPtr(items, sizeof(void*) * (count < kMaxList ? count
                                                            : kMaxList))) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < count && n < kMaxList; ++i) {
    void* item = items[i];
    if (item != nullptr && !IsBadReadPtr(item, 0x5C)) out[n++] = item;
  }
  return n;
}

// A picture that is not one of the pictures of an empty Hand at rest: the
// three types of GSpellSystemInfo, and the leash gestures, which the game
// shows while the leash of the Creature is on.
bool GoesOn(void* const items[], int count) {
  const int again = Read<int>(kAgainType);
  const int a = Read<int>(kStartA);
  const int b = Read<int>(kStartB);
  for (int i = 0; i < count; ++i) {
    if (Field<int>(items[i], kItemLeash) != -1) continue;
    const int type = Field<int>(items[i], kItemType);
    if (type != again && type != a && type != b) return true;
  }
  return false;
}

void LogList(void* const items[], int count, uint64_t frame) {
  bool same = count == g_logged_count;
  for (int i = 0; same && i < count; ++i) {
    if (Field<int>(items[i], kItemType) != g_logged_types[i]) same = false;
  }
  if (same) return;
  g_logged_count = count;
  for (int i = 0; i < count; ++i) {
    g_logged_types[i] = Field<int>(items[i], kItemType);
  }
  g_said_corner = false;
  if (!LogRoom()) return;
  char words[600] = "";
  for (int i = 0; i < count; ++i) {
    const float* tl =
        reinterpret_cast<const float*>(static_cast<uint8_t*>(items[i]) +
                                       kItemTopLeft);
    char one[96];
    sprintf_s(one, "%stype %d (order %d, seed %d, leash %d) at %.0f %.0f px",
              i > 0 ? ", " : "", Field<int>(items[i], kItemType),
              Field<int>(items[i], kItemOrder), Field<int>(items[i], kItemSeed),
              Field<int>(items[i], kItemLeash), tl[0], tl[1]);
    strncat_s(words, one, _TRUNCATE);
  }
  const uint32_t screen = Read<uint32_t>(kScreenSize);
  ModLog("gesture panel: the game shows %d gesture pictures%s%s. They go on "
         "from a drawn gesture: %s. The fade is %.2f. The game screen is %u "
         "by %u, frame %llu",
         count, count > 0 ? ": " : "", words,
         GoesOn(items, count) ? "yes" : "no", Read<float>(kFade),
         screen & 0xFFFF, screen >> 16, frame);
}

// The point of the plane for a pixel of the game screen.
void PlanePoint(float px, float py, float out[3]) {
  float r = 0.0f, u = 0.0f;
  PixelOnPlane(g_map, px, py, &r, &u);
  for (int i = 0; i < 3; ++i) out[i] = g_w0[i] + g_wr[i] * r + g_wu[i] * u;
}

// fn_0068B420: the point of the square of a picture, (unit[0], unit[2]) from
// its top left, 0 to 1.
void __fastcall h_Point(void* item, void* unused, const float* unit,
                        float* out) {
  if (!g_in_panel) {
    o_Point(item, unused, unit, out);
    if (!g_said_corner && LogRoom()) {
      g_said_corner = true;
      const float* cam = reinterpret_cast<const float*>(kCamera);
      const float d[3] = {out[0] - cam[0], out[1] - cam[1], out[2] - cam[2]};
      const float units = g_units > 0.0f ? g_units : 1.0f;
      // The pixel of that point in the camera of now: the camera that the
      // sprite of the card reads (LH3DSprite::Draw).
      const float* m =
          reinterpret_cast<const float*>(bw::tech::kWorldToCameraUnscaled);
      const float cx = m[0] * out[0] + m[3] * out[1] + m[6] * out[2] + m[9];
      const float cy = m[1] * out[0] + m[4] * out[1] + m[7] * out[2] + m[10];
      const float cz = m[2] * out[0] + m[5] * out[1] + m[8] * out[2] + m[11];
      const float* info =
          reinterpret_cast<const float*>(bw::tech::kInfoTransform);
      const float px = cz > 0.0f ? (1.0f + cx * info[7] / cz) * info[3] : -1.0f;
      const float py = cz > 0.0f ? (1.0f - cy * info[8] / cz) * info[4] : -1.0f;
      ModLog("gesture panel: the game puts a gesture picture at the world "
             "point %.2f %.2f %.2f, %.1f units from the camera (%.1f cm of "
             "the room at %.1f units for each meter), at the pixel %.0f %.0f "
             "of the camera of now. The fade is %.2f. It asks for the pixel "
             "%.0f %.0f of a game screen of %d by %d (render), frame %llu",
             out[0], out[1], out[2], sqrtf(d[0] * d[0] + d[1] * d[1] +
                                           d[2] * d[2]),
             100.0f * sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / units,
             units, px, py, Read<float>(kFade),
             Field<float>(item, kItemTopLeft) +
                 (Field<float>(item, kItemBottomRight) -
                  Field<float>(item, kItemTopLeft)) * unit[0],
             Field<float>(item, kItemTopLeft + 4) +
                 (Field<float>(item, kItemBottomRight + 4) -
                  Field<float>(item, kItemTopLeft + 4)) * unit[2],
             Read<int>(bw::tech::kInfoTransform),
             Read<int>(bw::tech::kInfoTransform + 4), ControllersFrameCount());
    }
    return;
  }
  const float x0 = Field<float>(item, kItemTopLeft);
  const float y0 = Field<float>(item, kItemTopLeft + 4);
  const float x1 = Field<float>(item, kItemBottomRight);
  const float y1 = Field<float>(item, kItemBottomRight + 4);
  PlanePoint((x1 - x0) * unit[0] + x0, (y1 - y0) * unit[2] + y0, out);
}

// fn_0068B7E0: the bubble over the square.
void __fastcall h_Bubble(void* item, void* unused, int seed, int leash) {
  if (!g_in_panel) {
    o_Bubble(item, unused, seed, leash);
    return;
  }
  if (leash != -1) return;
  const int order = Field<int>(item, kItemOrder);
  if (order == kOrderAgain) seed = kAgainSeed;
  const bool ball = seed == -1;
  if (ball && order != kOrderStart) return;
  const float x0 = Field<float>(item, kItemTopLeft);
  const float y0 = Field<float>(item, kItemTopLeft + 4);
  const float x1 = Field<float>(item, kItemBottomRight);
  const float side = x1 - x0;
  float at[3];
  PlanePoint(0.5f * (x0 + x1), y0 - 0.5f * side, at);
  const float radius = kBallShare * side * g_map.meters_per_pixel * g_units;
  float fade = Read<float>(kFade);
  fade = fade < 0.0f ? 0.0f : (fade > 1.0f ? 1.0f : fade);
  BookBubbleDrawPart(ball ? -1 : seed, at, radius,
                     static_cast<int>(fade * 255.0f + 0.5f));
}

// The corner draw of the sorted list, and the draw of this file.
void __fastcall h_Draw(void* item, void* unused) {
  if (g_in_panel || !g_on) {
    if (!g_in_panel) ++g_corner_draws;
    o_Draw(item, unused);
    return;
  }
  ++g_corner_skips;
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

void GesturePanelInstall() {
  ReadSettings();
  ModLog("gesture panel: gesture_panel=%d: %s. gesture_panel_cm=%d",
         g_on ? 1 : 0,
         g_on ? "the gesture pictures of the game show in a row over the palm "
                "of the hand that does not point, while the palm is up"
              : "the game draws its gesture pictures at the corner of its "
                "screen",
         g_width_cm);
  if (g_hooked) return;
  if (!SameBytes(kDraw, kDrawBytes, sizeof(kDrawBytes)) ||
      !SameBytes(kPoint, kPointBytes, sizeof(kPointBytes)) ||
      !SameBytes(kBubble, kBubbleBytes, sizeof(kBubbleBytes))) {
    ModLog("gesture panel: the draw of the gesture pictures is not as in "
           "v1.42, so the mod hooks nothing, and the game draws the pictures "
           "at the corner of its screen");
    return;
  }
  g_hooked = Hook(kDraw, reinterpret_cast<void*>(&h_Draw),
                  reinterpret_cast<void**>(&o_Draw),
                  "DisplayGesture draw") &&
             Hook(kPoint, reinterpret_cast<void*>(&h_Point),
                  reinterpret_cast<void**>(&o_Point),
                  "the point of a gesture picture") &&
             Hook(kBubble, reinterpret_cast<void*>(&h_Bubble),
                  reinterpret_cast<void**>(&o_Bubble),
                  "the bubble of a gesture picture");
}

void GesturePanelFrame() {
  ReadSettings();
  if (!g_hooked) return;
  const uint64_t frame = ControllersFrameCount();
  if (g_test_frame != 0 && !g_test_done &&
      ControllersTestFrame(g_test_frame, frame) >= g_test_frame) {
    g_test_done = true;
    g_on = !g_on;
    ModLog("gesture panel: gesture_panel_test_frame: gesture_panel=%d now, "
           "frame %llu", g_on ? 1 : 0, frame);
  }
  void* items[kMaxList];
  const int count = List(items);
  LogList(items, count, frame);
  g_goes_on = count > 0 && GoesOn(items, count);
  GestureInput gesture;
  g_draw_held = ControllersGestureInput(&gesture) && gesture.button_down;

  float grip[3], turn[4], head[3];
  bool left = true;
  const bool have_pose = ControllersCardPose(1, grip, turn, head, &left);
  if (have_pose) {
    g_palm_up = PalmUpStep(g_palm_up, PalmUp(turn, left), kUpOnDeg,
                           kUpOffDeg);
  } else {
    g_palm_up = false;
  }
  RingHand hand;
  const bool have_hand = ControllersRingHand(&hand);
  if (have_pose) {
    float world[3], units = 0.0f;
    if (ControllersLocalToWorld(grip, world, &units) && units > 0.0f) {
      g_units = units;
    }
  }
  const char* why = "";
  if (!g_on) {
    why = "gesture_panel=0";
  } else if (count == 0) {
    why = "the game shows no gesture picture";
  } else if (!have_pose || !have_hand || !hand.active) {
    why = "the hand has no pose";
  } else if (!g_palm_up) {
    why = "the palm is not up";
  } else if (SpellBookOpen()) {
    why = "the spell book is open";
  } else if (SpellBookPalmOn() && !g_draw_held && !g_goes_on) {
    why = "the palm opens the spell book now";
  } else if (!hand.usable || hand.dialog) {
    why = "the controllers cannot act, or a dialog box is up";
  } else if (RadialMenuShows()) {
    why = "the ring shows";
  } else if (CutsceneWindowFlat()) {
    why = "the cut scene window is up";
  }
  const bool show = why[0] == 0;
  if (show) {
    float center[3];
    spell_book_math::BookCenter(grip, turn, left, center);
    spell_book_math::Plane plane;
    spell_book_math::PlaneAt(center, head, &plane);
    float right[3], up[3], units = 0.0f, unused = 0.0f;
    for (int i = 0; i < 3; ++i) {
      right[i] = center[i] + plane.right[i];
      up[i] = center[i] + plane.up[i];
    }
    float w_right[3], w_up[3];
    if (ControllersLocalToWorld(center, g_w0, &units) &&
        ControllersLocalToWorld(right, w_right, &unused) &&
        ControllersLocalToWorld(up, w_up, &unused) && units > 0.0f) {
      for (int i = 0; i < 3; ++i) {
        g_wr[i] = w_right[i] - g_w0[i];
        g_wu[i] = w_up[i] - g_w0[i];
      }
      g_units = units;
      g_show_frame = frame;
    } else {
      why = "the aim is not known";
    }
  }
  const bool shown = g_show_frame == frame && frame != 0;
  if (shown != g_showing && LogRoom()) {
    if (shown) {
      ModLog("gesture panel: the row of %d gesture pictures shows over the "
             "%s palm, frame %llu", count, left ? "left" : "right", frame);
    } else if (strcmp(why, g_hide_why) != 0 || g_showing) {
      ModLog("gesture panel: the row does not show: %s, frame %llu", why,
             frame);
    }
  }
  if (!shown) g_hide_why = why;
  g_showing = shown;
}

void GesturePanelDraw() {
  if (!g_hooked || !g_on || g_show_frame == 0) return;
  const uint64_t now = ControllersFrameCount();
  if (now > g_show_frame + 1) return;
  void* items[kMaxList];
  const int count = List(items);
  if (count == 0) return;

  // The row of the game, in pixels of its screen.
  float x0 = 1e9f, x1 = -1e9f, top = 1e9f, bottom = -1e9f;
  for (int i = 0; i < count; ++i) {
    const float* tl = reinterpret_cast<const float*>(
        static_cast<uint8_t*>(items[i]) + kItemTopLeft);
    const float* br = reinterpret_cast<const float*>(
        static_cast<uint8_t*>(items[i]) + kItemBottomRight);
    x0 = fminf(x0, tl[0]);
    x1 = fmaxf(x1, br[0]);
    top = fminf(top, tl[1] - (br[0] - tl[0]));
    bottom = fmaxf(bottom, br[1]);
  }
  const uint32_t screen = Read<uint32_t>(kScreenSize);
  const float width = static_cast<float>(screen & 0xFFFF);
  const float pitch = (Read<float>(kSideShare) + Read<float>(kGapShare)) *
                      width;
  if (!MapRow(x0, x1, top, bottom, count, pitch, g_width_cm * 0.01f, &g_map)) {
    return;
  }
  g_side_px = Read<float>(kSideShare) * width;

  // The sizes of the card and of the dot: the game's size for a pixel at 10
  // units, times the size of a pixel on the plane.
  const float near_plane = Read<float>(bw::tech::kNearPlane);
  const float half_x = Read<float>(kHalfX);
  const float tan_x = Read<float>(kTanX);
  if (!(near_plane > 0.0f) || !(half_x > 0.0f) || !(tan_x > 0.0f)) return;
  const float game_per_px = tan_x / half_x * (kGameDepth / near_plane);
  const float plane_per_px = g_map.meters_per_pixel * g_units;
  const float scale = plane_per_px / game_per_px;
  float* card = reinterpret_cast<float*>(kCardSize);
  float* dot = reinterpret_cast<float*>(kDotSize);
  const float card_was = *card, dot_was = *dot;

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

  *card = card_was * scale;
  *dot = dot_was * scale;
  g_in_panel = true;
  for (int i = 0; i < count; ++i) o_Draw(items[i], nullptr);
  g_in_panel = false;
  *card = card_was;
  *dot = dot_was;

  memcpy(reinterpret_cast<void*>(bw::tech::kWorldToCamera), scaled, bytes);
  memcpy(reinterpret_cast<void*>(bw::tech::kWorldToCameraUnscaled), unscaled,
         bytes);
  memcpy(reinterpret_cast<void*>(bw::tech::kWorldToCameraCopy), copy, bytes);
  memcpy(reinterpret_cast<void*>(bw::tech::kCameraToWorldInverse), inverse,
         bytes);
  ++g_draws;
  if (!g_said_first && LogRoom()) {
    g_said_first = true;
    ModLog("gesture panel: the first draw of the row: %d pictures, %.1f cm "
           "for each picture, at the world point %.2f %.2f %.2f, %.1f units "
           "for each meter. The card and the dot are %.2f times the size of "
           "the game, frame %llu", count,
           g_side_px * g_map.meters_per_pixel * 100.0f, g_w0[0], g_w0[1],
           g_w0[2], g_units, scale, now);
  }
}

bool GesturePanelWantsPalm() {
  return g_hooked && g_on && g_logged_count > 0 && (g_draw_held || g_goes_on);
}

void GesturePanelReport() {
  if (!g_hooked) return;
  if (g_show_frame == 0 && g_corner_skips == 0 && g_corner_draws == 0) return;
  ModLog("gesture panel: %llu draws of the row, %llu corner draws that the "
         "mod skipped, %llu corner draws of the game. The fade is %.2f now",
         g_draws, g_corner_skips, g_corner_draws, Read<float>(kFade));
  g_draws = 0;
  g_corner_skips = 0;
  g_corner_draws = 0;
}
