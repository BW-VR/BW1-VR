// The camera of the player that the Creature reads. See docs/creature.md,
// "The Creature turns to the head".
//
// WHAT THE GAME DOES [V, runblack.exe 1.42]. GInterface::Process calls
// fn_005D2250 in each game turn. It reads the engine camera
// (LH3DTech::g_camera 0x00EA1DB8 and its focus 0x00EA1DC4) and sends it in
// the packets 0x15 and 0x17 when it changed. Their handlers (fn_005DBFB0 and
// fn_005DC0D0) copy it to the GInterfaceStatus of the player: the camera at
// +0xB0 and the focus at +0xBC. Each Creature that turns to the player, looks
// at the player, points or throws at the player, or measures how far the
// player is, asks Creature::GetNearestCameraInterfaceStatus 0x0047D640 for
// the status of the nearest player and reads +0xB0. The readers are the
// locked select (Creature::GoIntoStateOfFinishingAction 0x00477B20), the look
// (Creature::LookAtCamera 0x004D1A30), the agenda (LookAtCameraInWideScreen,
// PointAtCamera, ThrowAtCamera, PointOutHighlight), the sub actions of the
// Creature, and some tests of its mind.
//
// WHY THE GOD VIEW NEEDS THE HEAD. With the mouse the engine camera is the
// eye of the player. In the God view the game's own camera is 400 units
// from the place of the player, 50 degrees above the land
// (comfort_camera.cpp, GodAfter): about 300 units up and 257 units back on
// the map. So the Creature turned to a point behind the player and looked up
// into the sky.
//
// WHAT THE MOD DOES (creature_player_head=1). For the time of the Creature
// code (Creature::ProcessState, GoIntoStateOfFinishingAction and
// LookAtCamera) the camera of the local GInterfaceStatus holds the head of
// the player. At the end of that code the game's value comes back. This
// happens only while the player stands for the camera
// (docs/god-view.md, "When the player stands for the camera"). In a story
// the Creature reads the camera of the story, as in the plain game. The
// interface, the spells and the sound read the game's value, because the
// field holds the head only inside the Creature code.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "MinHook.h"
#include "creature_head.h"
#include "creature_head_math.h"

extern void ModLog(const char* format, ...);

// controllers.cpp: a pulse of a test key.
void ControllersGesturePulse(float amplitude, float seconds);
// hand_direct.cpp: the head of the player in the world, while the player
// stands for the camera now. why gets the reason when the player does not.
bool HandDirectPlayerHead(float head[3], const char** why);

namespace {

// --- the game ---------------------------------------------------------------

const uintptr_t kGame = 0x00D0195C;
// GGame::MyInterface, thiscall, no arguments.
const uintptr_t kMyInterface = 0x00555850;
const int kFaceStatus = 0x39C;   // GInterface: the local GInterfaceStatus
const int kStatusCamera = 0xB0;  // GInterfaceStatus: the camera, an LHPoint
// Creature: [[+0x160]+0x58] is the LH3DCreature, and its +0x78 the base of
// the body in the world.
const int kCreatureLink = 0x160;
const int kModelLink = 0x58;
const int kModelPlace = 0x78;

// Creature::GetNearestCameraInterfaceStatus, thiscall, no arguments: the
// status. Creature::GoIntoStateOfFinishingAction, thiscall, no arguments.
// Creature::ProcessState, thiscall, no arguments (slot of the vtable at
// 0x008CCE30). Creature::LookAtCamera, thiscall (MapCoords*), ret 4: 1 with
// the point of the camera in the MapCoords.
const uintptr_t kNearestCamera = 0x0047D640;
const uintptr_t kFinishing = 0x00477B20;
const uintptr_t kProcessState = 0x00472DC0;
const uintptr_t kLookAtCamera = 0x004D1A30;
const uint8_t kNearestBytes[11] = {0x83, 0xEC, 0x14, 0x57, 0x8B, 0xF9,
                                   0x8B, 0x07, 0xFF, 0x50, 0x1C};
const uint8_t kFinishingBytes[11] = {0x83, 0xEC, 0x1C, 0x56, 0x8B, 0xF1,
                                     0x8B, 0x06, 0xFF, 0x50, 0x1C};
const uint8_t kProcessBytes[11] = {0xA1, 0x78, 0x41, 0xC6, 0x00, 0x81,
                                   0xEC, 0x48, 0x02, 0x00, 0x00};
const uint8_t kLookBytes[11] = {0x83, 0xEC, 0x18, 0x56, 0x8B, 0xF1,
                                0x8B, 0x06, 0xFF, 0x50, 0x1C};

typedef void*(__fastcall* MyInterfaceFn)(void* game, void* unused);
typedef uint8_t*(__fastcall* NearestFn)(void* creature, void* unused);
typedef void(__fastcall* FinishingFn)(void* creature, void* unused);
typedef uint32_t(__fastcall* ProcessFn)(void* creature, void* unused);
typedef int(__fastcall* LookFn)(void* creature, void* unused, void* coords);
NearestFn o_Nearest = nullptr;
FinishingFn o_Finishing = nullptr;
ProcessFn o_Process = nullptr;
LookFn o_Look = nullptr;

// --- the keys ---------------------------------------------------------------

bool g_ready = false;
bool g_hooked = false;
int g_mode = head_math::kModeHead;  // creature_player_head
bool g_test_keys = false;
uint64_t g_frame = 0;

int IniInt(const char* key, int fallback) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash =
      length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return fallback;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

void LogMode(const char* from) {
  if (g_mode == head_math::kModeHead) {
    ModLog("creature head: %s: creature_player_head=1. In the God view the "
           "Creature turns to, looks at, points at and measures the head of "
           "the player, where the game has its own camera. In a story, and "
           "while the game moves its own camera, the Creature reads the "
           "game's camera", from);
  } else {
    ModLog("creature head: %s: creature_player_head=0. The Creature turns "
           "to, looks at, points at and measures the game's own camera, 400 "
           "units from the player and about 300 units above the land in the "
           "God view", from);
  }
}

// --- the swap ---------------------------------------------------------------

// The Creature code that runs now: 0 when none. The outer call puts the head
// in, and its end puts the game's value back.
int g_depth = 0;
struct Swap {
  bool on;          // the field holds the head
  uint8_t* status;  // the local GInterfaceStatus
  float keep[3];    // the game's value
  float wrote[3];   // the head that the mod wrote
  const char* why;  // why the field holds the game's value, or null
};
Swap g_swap = {};
// The status that the Creature read last, in the code that runs now.
const uint8_t* g_read = nullptr;

// The log and the counts of a report window.
const int kMaxLogs = 240;
int g_logs = 0;
uint64_t g_head_reads = 0;   // the Creature read the head
uint64_t g_game_reads = 0;   // the Creature read the game's camera in its code
uint64_t g_key_reads = 0;    // of these, with creature_player_head=0
uint64_t g_out_reads = 0;    // the local camera, read outside the Creature code
uint64_t g_turns = 0;
uint64_t g_looks = 0;
int g_look_from = -1;        // the source of the last look: 0 game, 1 head
const char* g_look_why = nullptr;

bool Log() {
  if (g_logs >= kMaxLogs) return false;
  ++g_logs;
  return true;
}

uint8_t* LocalStatus() {
  void* const* slot = reinterpret_cast<void* const*>(kGame);
  if (IsBadReadPtr(slot, sizeof(void*)) || *slot == nullptr) return nullptr;
  uint8_t* face = static_cast<uint8_t*>(
      reinterpret_cast<MyInterfaceFn>(kMyInterface)(*slot, nullptr));
  if (face == nullptr) return nullptr;
  uint8_t* status = *reinterpret_cast<uint8_t* const*>(face + kFaceStatus);
  if (status == nullptr ||
      IsBadReadPtr(status + kStatusCamera, 3 * sizeof(float))) {
    return nullptr;
  }
  return status;
}

void Enter() {
  if (g_depth++ > 0) return;
  g_read = nullptr;
  g_swap.on = false;
  g_swap.status = nullptr;
  g_swap.why = nullptr;
  if (g_mode != head_math::kModeHead) {
    g_swap.why = "creature_player_head=0";
    return;
  }
  float head[3] = {0.0f, 0.0f, 0.0f};
  const char* why = nullptr;
  const bool stands = HandDirectPlayerHead(head, &why);
  if (!head_math::UseHead(g_mode, stands, head)) {
    g_swap.why = why != nullptr ? why : "the head is not known";
    return;
  }
  uint8_t* status = LocalStatus();
  if (status == nullptr) {
    g_swap.why = "the game has no interface of the player now";
    return;
  }
  float* camera = reinterpret_cast<float*>(status + kStatusCamera);
  for (int i = 0; i < 3; ++i) {
    g_swap.keep[i] = camera[i];
    g_swap.wrote[i] = head[i];
    camera[i] = head[i];
  }
  g_swap.status = status;
  g_swap.on = true;
}

void Leave() {
  if (--g_depth > 0) return;
  g_depth = 0;
  if (!g_swap.on) return;
  float* camera = reinterpret_cast<float*>(g_swap.status + kStatusCamera);
  if (head_math::PutBack(camera, g_swap.wrote)) {
    for (int i = 0; i < 3; ++i) camera[i] = g_swap.keep[i];
  }
  g_swap.on = false;
}

// The game's own camera now: the kept value while the field holds the head.
bool GameCamera(float out[3]) {
  if (g_swap.on) {
    for (int i = 0; i < 3; ++i) out[i] = g_swap.keep[i];
    return true;
  }
  const uint8_t* status = LocalStatus();
  if (status == nullptr) return false;
  memcpy(out, status + kStatusCamera, 3 * sizeof(float));
  return true;
}

bool BodyPlace(void* creature, float out[3]) {
  const uint8_t* c = static_cast<const uint8_t*>(creature);
  if (c == nullptr || IsBadReadPtr(c + kCreatureLink, sizeof(void*))) {
    return false;
  }
  const uint8_t* link = *reinterpret_cast<uint8_t* const*>(c + kCreatureLink);
  if (link == nullptr || IsBadReadPtr(link + kModelLink, sizeof(void*))) {
    return false;
  }
  const uint8_t* model = *reinterpret_cast<uint8_t* const*>(link + kModelLink);
  if (model == nullptr ||
      IsBadReadPtr(model + kModelPlace, 3 * sizeof(float))) {
    return false;
  }
  memcpy(out, model + kModelPlace, 3 * sizeof(float));
  return true;
}

// The point that the Creature read in the code that runs now.
bool ReadPoint(float out[3]) {
  if (g_read == nullptr) return false;
  memcpy(out, g_read + kStatusCamera, 3 * sizeof(float));
  return true;
}

const char* Source() {
  if (g_swap.on) return "the head of the player (creature_player_head=1)";
  static char text[160];
  _snprintf_s(text, sizeof(text), _TRUNCATE, "the game's camera (%s)",
              g_swap.why != nullptr ? g_swap.why : "not known");
  return text;
}

// --- the hooks --------------------------------------------------------------

uint8_t* __fastcall h_Nearest(void* creature, void* unused) {
  uint8_t* status = o_Nearest(creature, unused);
  if (status == nullptr) return status;
  if (g_depth > 0) g_read = status;
  if (status != LocalStatus()) return status;
  if (g_depth == 0) {
    ++g_out_reads;
  } else if (g_swap.on) {
    ++g_head_reads;
  } else {
    ++g_game_reads;
    if (g_mode != head_math::kModeHead) ++g_key_reads;
  }
  return status;
}

uint32_t __fastcall h_Process(void* creature, void* unused) {
  Enter();
  const uint32_t result = o_Process(creature, unused);
  Leave();
  return result;
}

// THE TURN OF THE LOCKED SELECT: a press on the Creature, and the stay
// (creature_stay.cpp). One line for each turn.
void __fastcall h_Finishing(void* creature, void* unused) {
  Enter();
  const uint8_t* before = g_read;
  g_read = nullptr;
  o_Finishing(creature, unused);
  ++g_turns;
  float point[3], game[3], body[3];
  if (Log() && ReadPoint(point) && GameCamera(game) &&
      BodyPlace(creature, body)) {
    float to_point = 0.0f, to_game = 0.0f;
    const bool turns = head_math::TurnHeading(body, point, &to_point);
    const bool game_turns = head_math::TurnHeading(body, game, &to_game);
    char gap[96] = "";
    if (turns && game_turns && g_swap.on) {
      _snprintf_s(gap, sizeof(gap), _TRUNCATE,
                  " The turn to the head is %.1f degrees from the turn to "
                  "the game's camera.",
                  head_math::HeadingGap(to_point, to_game));
    }
    ModLog("creature head: the Creature turns to the player "
           "(GoIntoStateOfFinishingAction). It turns to %.1f %.1f %.1f, %s: "
           "%.1f units from its body on the map and %.1f above its base%s. "
           "The game's camera is at %.1f %.1f %.1f: %.1f units on the map "
           "and %.1f above the base.%s The body is at %.1f %.1f %.1f, frame "
           "%llu",
           point[0], point[1], point[2], Source(),
           head_math::MapDistance(body, point), point[1] - body[1],
           turns ? "" : " (less than 2 units on x and z: the game gives the "
                        "angle 0)",
           game[0], game[1], game[2], head_math::MapDistance(body, game),
           game[1] - body[1], gap, body[0], body[1], body[2], g_frame);
  }
  g_read = before;
  Leave();
}

// THE LOOK AT THE CAMERA, which the sub actions of the Creature ask for. One
// line when the source of the look changes.
int __fastcall h_Look(void* creature, void* unused, void* coords) {
  Enter();
  const uint8_t* before = g_read;
  g_read = nullptr;
  const int result = o_Look(creature, unused, coords);
  ++g_looks;
  const int from = g_swap.on ? 1 : 0;
  const char* why = g_swap.on ? nullptr : g_swap.why;
  float point[3], body[3];
  if ((from != g_look_from || why != g_look_why) && ReadPoint(point) &&
      BodyPlace(creature, body)) {
    g_look_from = from;
    g_look_why = why;
    if (Log()) {
      ModLog("creature head: the Creature looks at the camera of the player "
             "(LookAtCamera): at %.1f %.1f %.1f, %s. That is %.1f units from "
             "its body on the map and %.1f above its base, frame %llu",
             point[0], point[1], point[2], Source(),
             head_math::MapDistance(body, point), point[1] - body[1],
             g_frame);
    }
  }
  g_read = before;
  Leave();
  return result;
}

bool HasBytes(uintptr_t address, const uint8_t* bytes, size_t count) {
  const void* at = reinterpret_cast<const void*>(address);
  return !IsBadReadPtr(at, count) && memcmp(at, bytes, count) == 0;
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

// THE A/B KEY (test_keys=1): Ctrl, Shift and the slash key (VK_OEM_2, the #
// key of a German keyboard) change creature_player_head between 1 and 0.
// The pointing controller gives a long pulse for 1 and a short pulse for 0.
// The key is read once a frame, and only while a window of the game is in
// front.
void PollKey() {
  static bool was = false;
  if (!g_test_keys || !g_hooked) return;
  const bool down = (GetAsyncKeyState(VK_OEM_2) & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool pressed = down && !was;
  was = down;
  if (!pressed) return;
  DWORD process = 0;
  const HWND window = GetForegroundWindow();
  if (window != nullptr) GetWindowThreadProcessId(window, &process);
  if (process != GetCurrentProcessId()) return;
  g_mode = g_mode == head_math::kModeHead ? head_math::kModeGame
                                          : head_math::kModeHead;
  ControllersGesturePulse(0.5f, g_mode == head_math::kModeHead ? 0.24f : 0.08f);
  char from[64];
  _snprintf_s(from, sizeof(from), _TRUNCATE,
              "Ctrl, Shift and the slash key, frame %llu", g_frame);
  LogMode(from);
}

}  // namespace

void CreatureHeadInstall() {
  if (g_ready) return;
  g_ready = true;
  g_mode = head_math::ClampMode(IniInt("creature_player_head", 1));
  g_test_keys = IniInt("test_keys", 0) != 0;
  if (!HasBytes(kNearestCamera, kNearestBytes, sizeof(kNearestBytes)) ||
      !HasBytes(kFinishing, kFinishingBytes, sizeof(kFinishingBytes)) ||
      !HasBytes(kProcessState, kProcessBytes, sizeof(kProcessBytes)) ||
      !HasBytes(kLookAtCamera, kLookBytes, sizeof(kLookBytes))) {
    g_mode = head_math::kModeGame;
    ModLog("hooks        : the Creature functions that read the camera of "
           "the player do not have the bytes of the disassembly, so the "
           "Creature reads the game's camera (creature_player_head=0)");
    return;
  }
  // The counting hook goes in first, so that each scope finds it.
  const bool nearest =
      Hook(kNearestCamera, reinterpret_cast<void*>(&h_Nearest),
           reinterpret_cast<void**>(&o_Nearest),
           "Creature::GetNearestCameraInterfaceStatus (the count)");
  const bool finishing =
      nearest && Hook(kFinishing, reinterpret_cast<void*>(&h_Finishing),
                      reinterpret_cast<void**>(&o_Finishing),
                      "Creature::GoIntoStateOfFinishingAction (the head)");
  const bool process =
      finishing && Hook(kProcessState, reinterpret_cast<void*>(&h_Process),
                        reinterpret_cast<void**>(&o_Process),
                        "Creature::ProcessState (the head)");
  const bool look =
      process && Hook(kLookAtCamera, reinterpret_cast<void*>(&h_Look),
                      reinterpret_cast<void**>(&o_Look),
                      "Creature::LookAtCamera (the head)");
  g_hooked = nearest && finishing && process && look;
  if (!g_hooked) {
    // With part of the scopes only, a Creature would read two cameras.
    if (nearest) MH_DisableHook(reinterpret_cast<void*>(kNearestCamera));
    if (finishing) MH_DisableHook(reinterpret_cast<void*>(kFinishing));
    if (process) MH_DisableHook(reinterpret_cast<void*>(kProcessState));
    g_mode = head_math::kModeGame;
    ModLog("creature head: a hook failed, so the Creature reads the game's "
           "camera (creature_player_head=0)");
    return;
  }
  LogMode("creature_player_head");
  if (g_test_keys) {
    ModLog("keys         : Ctrl, Shift and the slash key change "
           "creature_player_head between 1 and 0 (test_keys=1)");
  }
}

bool CreatureHeadHooked() { return g_hooked; }

void CreatureHeadFrame(uint64_t frame) {
  g_frame = frame;
  PollKey();
}

void CreatureHeadReport() {
  if (!g_hooked) return;
  const uint64_t reads = g_head_reads + g_game_reads + g_out_reads;
  if (reads == 0 && g_turns == 0 && g_looks == 0) return;
  ModLog("creature head: the Creature read the camera of the player %llu "
         "times: %llu times the head, %llu times the game's camera (%llu of "
         "them with creature_player_head=0), %llu times outside the Creature "
         "code. Turns of the locked select: %llu. Looks at the camera: %llu",
         reads, g_head_reads, g_game_reads, g_key_reads, g_out_reads,
         g_turns, g_looks);
  g_head_reads = g_game_reads = g_key_reads = g_out_reads = 0;
  g_turns = g_looks = 0;
}
