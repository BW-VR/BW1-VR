// The carry of the other hand (left_hold, docs/controllers.md, 5.7, "The
// carry of the other hand").
//
// THE GAME HAS ONE HAND. Its CHand holds one object (+0x4904). The
// interface status of the player (GInterface+0x39C) has the count of the
// hands in use (+0x90, a byte) and one GMagicHand (+0x74) with one object:
// its count (+0x88, a byte) and its object (+0x8C). The pick up
// (fn_005DC2A0, fn_005FAFC0) puts the object there and sets the bit 0x4 of
// Object+0x24: the object is in a hand, so the physics and the map leave it
// alone. The drop (fn_005DC1E0, fn_005FB060) clears the bit and calls
// Object::RemoveFromHand (vtable +0x704).
//
// THE PARK. A take with the trigger of the other hand borrows the Hand
// (controllers.cpp, LeftTakeStep). Once the Hand holds the object in its
// Holding state, the mod empties the GMagicHand and the count of the hands,
// and calls CHand::ThrowObject (0x0046DDD0). That function gives the object
// back the draw settings that it had before the take and lets go of it on the
// side of the CHand. The object keeps the bit 0x4, so it stays out of the
// physics and out of the map. Then fn_005D1260 ends the action of the
// interface, as the game does when a pick up fails. The Hand is free, and it
// goes back to the pointing controller.
//
// THE CARRY. At the park the mod keeps the place and the axes of the object
// in the frame of the wrist of the other hand. After the draw setup of the
// Hand (each pass) it writes the matrix of the mesh of the object
// (Object+0x40, then +0x14) from the wrist of this frame. The object turns
// and moves with the hand, one to one.
//
// THE DROP. For one call the mod puts the object back as the only object of
// the GMagicHand, with the count of the hands at 1 and no object in the
// CHand. It writes the data of a release to the interface status, as packet
// 0x4D does: +0x44 the speed, +0x50 the point, +0x5C the place, and +0x68,
// +0x6C, +0x70 the angles of LHMatrix::SetYXZMatrixOnly. Then it calls the
// drop action fn_005DA400 (thiscall on the GInterface, a packet with the
// place at +0x08). The game lets go of the object as at its own release:
// ValidToApplyThisToMapCoord, ApplyThisToMapCoord (for a mobile object
// ThrowObjectFromHand and the physics of a throw or of a set down), and
// fn_005DC1E0. After the call the mod puts back what the Hand of the game
// holds, and the data of the last release.

#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "left_hold.h"
#include "MinHook.h"


void ModLog(const char* format, ...);
bool HandExactAddPrepareHook(void (*hook)(void* hand));
bool HandDirectGripFacts(bool* holding, bool* settled, bool* spring,
                         bool* take);
bool OverlayFreeWrist(float point[3], float axes[9], float* length,
                      bool pointing);

namespace {

const uintptr_t kGame = 0x00D0195C;         // GGame::g_game
const uintptr_t kMyInterface = 0x00555850;  // GGame::MyInterface
const uintptr_t kThrowObject = 0x0046DDD0;  // CHand::ThrowObject
const uintptr_t kEndAction = 0x005D1260;    // the interface ends its action
const uintptr_t kDropAction = 0x005DA400;   // the drop action
const int kFaceLocked = 0x38;   // a byte: the pick of the game is locked
const int kFaceStatus = 0x39C;  // GInterfaceStatus*
const int kFaceHand = 0x3A0;    // CHand*
const int kHandState = 0x4878;
const int kHandHeld = 0x4904;
const int32_t kStateHolding = 4;
const int kStatusHands = 0x90;        // a byte: the hands in use
const int kStatusMagicCount = 0x88;   // a byte: the objects of the GMagicHand
const int kStatusMagicObject = 0x8C;  // its object
const int kStatusRelease = 0x44;      // 48 bytes: the data of a release
const int kObjectFlags = 0x24;
const uint16_t kFlagInHand = 0x4;
const int kObjectMesh = 0x40;
const int kMeshMatrix = 0x14;         // 3 rows (the axes), then the place
const int kIsSpellSeed = 0x4C4;
const int kDrawItself = 0x614;  // the draw of an object, as the Hand draws it
const uintptr_t kFaceDraw = 0x00518640;  // GInterface::Draw
const float kSpeedCap = 124.0f;       // the cap of the game's release
const double kParkAfterSeconds = 0.2; // the Hand holds the object this long
const int kMaxLogs = 60;
uint64_t g_carry_draws = 0;  // the draws of the carried object

typedef void*(__fastcall* MyInterfaceFn)(void* game, void* unused);
typedef void(__fastcall* ThisFn)(void* self, void* unused);
typedef int(__fastcall* DropFn)(void* face, void* unused, void* packet);
typedef int(__fastcall* VirtualIntFn)(void* self, void* unused);

struct Carry {
  uint8_t* object = nullptr;
  float axes[9];      // the axes of the object in the frame of the wrist
  float place[3];     // its place in the frame of the wrist
  float scale = 1.0f;
};
Carry g_carry;
// The take of the borrowed Hand: since when it holds the object.
void* g_take_held = nullptr;
LARGE_INTEGER g_take_since = {};
// The place of the object in the world, in the last frames, for the speed.
const int kSamples = 8;
float g_sample[kSamples][3];
double g_sample_time[kSamples];
int g_samples = 0;
int g_logs = 0;
uint64_t g_frame = 0;

double Now() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return frequency.QuadPart > 0
             ? static_cast<double>(count.QuadPart) / frequency.QuadPart
             : 0.0;
}

uint8_t* Face() {
  void* const* game = reinterpret_cast<void* const*>(kGame);
  if (IsBadReadPtr(game, sizeof(void*)) || *game == nullptr) return nullptr;
  return static_cast<uint8_t*>(
      reinterpret_cast<MyInterfaceFn>(kMyInterface)(*game, nullptr));
}

template <typename T>
T& At(uint8_t* base, int offset) {
  return *reinterpret_cast<T*>(base + offset);
}

float* MeshMatrix(uint8_t* object) {
  if (object == nullptr || IsBadReadPtr(object + kObjectMesh, 4)) {
    return nullptr;
  }
  uint8_t* mesh = At<uint8_t*>(object, kObjectMesh);
  if (mesh == nullptr || IsBadWritePtr(mesh + kMeshMatrix, 48)) return nullptr;
  return reinterpret_cast<float*>(mesh + kMeshMatrix);
}

float Dot(const float* a, const float* b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// The matrix of the carried object in the world from the wrist of now.
bool CarryMatrix(float out[12]) {
  float wrist[3], axes[9], length = 0.0f;
  if (!OverlayFreeWrist(wrist, axes, &length, true)) return false;
  for (int row = 0; row < 3; ++row) {
    for (int r = 0; r < 3; ++r) {
      out[row * 3 + r] = g_carry.axes[row * 3 + 0] * axes[0 + r] +
                         g_carry.axes[row * 3 + 1] * axes[3 + r] +
                         g_carry.axes[row * 3 + 2] * axes[6 + r];
    }
  }
  for (int r = 0; r < 3; ++r) {
    out[9 + r] = wrist[r] + g_carry.place[0] * axes[0 + r] +
                 g_carry.place[1] * axes[3 + r] +
                 g_carry.place[2] * axes[6 + r];
  }
  return true;
}

// After the draw setup of the Hand, in each pass.
void PlaceCarry(void*) {
  if (g_carry.object == nullptr) return;
  float* matrix = MeshMatrix(g_carry.object);
  float out[12];
  if (matrix != nullptr && CarryMatrix(out)) memcpy(matrix, out, sizeof(out));
}

void Log(const char* text) {
  if (g_logs >= kMaxLogs) return;
  ++g_logs;
  ModLog("left hold    : %s, frame %llu", text, g_frame);
}

// The park, when the borrowed Hand holds the object of the take.
void TryPark() {
  uint8_t* face = Face();
  if (face == nullptr) return;
  uint8_t* hand = At<uint8_t*>(face, kFaceHand);
  uint8_t* status = At<uint8_t*>(face, kFaceStatus);
  if (hand == nullptr || status == nullptr) return;
  uint8_t* held = At<uint8_t*>(hand, kHandHeld);
  bool holding = false, settled = false, spring = false, take = false;
  const bool facts = HandDirectGripFacts(&holding, &settled, &spring, &take);
  const bool ready =
      facts && holding && settled && !spring && !take && held != nullptr &&
      At<int32_t>(hand, kHandState) == kStateHolding &&
      At<uint8_t>(face, kFaceLocked) == 0 &&
      At<uint8_t>(status, kStatusHands) == 1 &&
      At<uint8_t>(status, kStatusMagicCount) == 1 &&
      At<uint8_t*>(status, kStatusMagicObject) == held &&
      (At<uint16_t>(held, kObjectFlags) & kFlagInHand) != 0;
  if (!ready) {
    g_take_held = nullptr;
    return;
  }
  // A miracle in the Hand stays with the Hand.
  void** vtable = *reinterpret_cast<void***>(held);
  if (reinterpret_cast<VirtualIntFn>(vtable[kIsSpellSeed / 4])(held,
                                                                nullptr) != 0) {
    return;
  }
  if (held != g_take_held) {
    g_take_held = held;
    QueryPerformanceCounter(&g_take_since);
    return;
  }
  LARGE_INTEGER now, frequency;
  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&frequency);
  if (frequency.QuadPart <= 0 ||
      static_cast<double>(now.QuadPart - g_take_since.QuadPart) /
              frequency.QuadPart < kParkAfterSeconds) {
    return;
  }
  // The place and the axes of the object in the frame of the wrist.
  float* matrix = MeshMatrix(held);
  float wrist[3], axes[9], length = 0.0f;
  if (matrix == nullptr || !OverlayFreeWrist(wrist, axes, &length, true)) {
    return;
  }
  Carry carry;
  carry.object = held;
  carry.scale = sqrtf(Dot(matrix, matrix));
  for (int row = 0; row < 3; ++row) {
    for (int k = 0; k < 3; ++k) {
      carry.axes[row * 3 + k] = Dot(&matrix[row * 3], &axes[k * 3]);
    }
  }
  const float gap[3] = {matrix[9] - wrist[0], matrix[10] - wrist[1],
                        matrix[11] - wrist[2]};
  for (int k = 0; k < 3; ++k) carry.place[k] = Dot(gap, &axes[k * 3]);
  // The park: the hands of the game are empty, and the object stays in a
  // hand.
  At<uint8_t>(status, kStatusHands) = 0;
  At<uint8_t>(status, kStatusMagicCount) = 0;
  At<uint8_t*>(status, kStatusMagicObject) = nullptr;
  reinterpret_cast<ThisFn>(kThrowObject)(hand, nullptr);
  reinterpret_cast<ThisFn>(kEndAction)(face, nullptr);
  g_carry = carry;
  g_take_held = nullptr;
  g_samples = 0;
  if (g_logs < kMaxLogs) {
    ++g_logs;
    ModLog("left hold    : the other hand carries the object %p, %.1f %.1f "
           "%.1f, %.1f units from its wrist. The Hand of the game is free "
           "(left_hold=1), frame %llu", held, matrix[9], matrix[10],
           matrix[11], sqrtf(Dot(gap, gap)), g_frame);
  }
}

// The carried object is still an object in a hand.
bool CarryAlive() {
  uint8_t* object = g_carry.object;
  if (object == nullptr || IsBadReadPtr(object, 0x48)) return false;
  void** vtable = *reinterpret_cast<void***>(object);
  if (vtable == nullptr || IsBadReadPtr(vtable, sizeof(void*))) return false;
  return (At<uint16_t>(object, kObjectFlags) & kFlagInHand) != 0;
}

void Sample() {
  float out[12];
  if (!CarryMatrix(out)) return;
  const int slot = g_samples % kSamples;
  memcpy(g_sample[slot], &out[9], sizeof(g_sample[slot]));
  g_sample_time[slot] = Now();
  ++g_samples;
}

// The speed of the object in units for each second, over the last 50 ms.
void Speed(float out[3]) {
  out[0] = out[1] = out[2] = 0.0f;
  if (g_samples < 2) return;
  const int last = (g_samples - 1) % kSamples;
  const int count = g_samples < kSamples ? g_samples : kSamples;
  for (int back = 1; back < count; ++back) {
    const int slot = (g_samples - 1 - back) % kSamples;
    const double dt = g_sample_time[last] - g_sample_time[slot];
    if (dt >= 0.05 || back == count - 1) {
      if (dt <= 1e-4) return;
      for (int i = 0; i < 3; ++i) {
        out[i] = static_cast<float>((g_sample[last][i] - g_sample[slot][i]) /
                                    dt);
      }
      break;
    }
  }
  const float speed = sqrtf(Dot(out, out));
  if (speed > kSpeedCap) {
    for (int i = 0; i < 3; ++i) out[i] *= kSpeedCap / speed;
  }
}

void Drop(bool with_speed, const char* why) {
  uint8_t* object = g_carry.object;
  if (object == nullptr) return;
  uint8_t* face = Face();
  uint8_t* hand = face != nullptr ? At<uint8_t*>(face, kFaceHand) : nullptr;
  uint8_t* status =
      face != nullptr ? At<uint8_t*>(face, kFaceStatus) : nullptr;
  float matrix[12];
  if (hand == nullptr || status == nullptr || !CarryMatrix(matrix)) {
    float* now = MeshMatrix(object);
    if (now == nullptr || hand == nullptr || status == nullptr) return;
    memcpy(matrix, now, sizeof(matrix));
  }
  // The angles of LHMatrix::SetYXZMatrixOnly: row 2 is (-cos b sin a,
  // sin b, cos b cos a), and m1 = -sin c cos b, m4 = cos c cos b.
  float rows[9];
  for (int row = 0; row < 3; ++row) {
    const float n = sqrtf(Dot(&matrix[row * 3], &matrix[row * 3]));
    for (int i = 0; i < 3; ++i) {
      rows[row * 3 + i] = n > 1e-6f ? matrix[row * 3 + i] / n : 0.0f;
    }
  }
  float sb = rows[7];
  if (sb > 1.0f) sb = 1.0f;
  if (sb < -1.0f) sb = -1.0f;
  const float b = asinf(sb);
  const float a = atan2f(-rows[6], rows[8]);
  const float c = atan2f(-rows[1], rows[4]);
  float speed[3] = {0.0f, 0.0f, 0.0f};
  if (with_speed) Speed(speed);
  // Save what the Hand of the game holds.
  const uint8_t hands = At<uint8_t>(status, kStatusHands);
  const uint8_t count = At<uint8_t>(status, kStatusMagicCount);
  uint8_t* const magic = At<uint8_t*>(status, kStatusMagicObject);
  uint8_t* const held = At<uint8_t*>(hand, kHandHeld);
  uint8_t release[48];
  memcpy(release, status + kStatusRelease, sizeof(release));
  // The object of the other hand is the only object of the hands.
  At<uint8_t>(status, kStatusHands) = 1;
  At<uint8_t>(status, kStatusMagicCount) = 1;
  At<uint8_t*>(status, kStatusMagicObject) = object;
  At<uint8_t*>(hand, kHandHeld) = nullptr;
  float* data = reinterpret_cast<float*>(status + kStatusRelease);
  for (int i = 0; i < 3; ++i) {
    data[0 + i] = speed[i];
    data[3 + i] = matrix[9 + i];
    data[6 + i] = matrix[9 + i];
  }
  data[9] = b;
  data[10] = a;
  data[11] = c;
  float packet[8] = {0.0f};
  packet[2] = matrix[9];
  packet[3] = matrix[10];
  packet[4] = matrix[11];
  const int result =
      reinterpret_cast<DropFn>(kDropAction)(face, nullptr, packet);
  const bool gone = (At<uint16_t>(object, kObjectFlags) & kFlagInHand) == 0;
  // What the Hand of the game holds comes back.
  At<uint8_t>(status, kStatusHands) = hands;
  At<uint8_t>(status, kStatusMagicCount) = count;
  At<uint8_t*>(status, kStatusMagicObject) = magic;
  At<uint8_t*>(hand, kHandHeld) = held;
  memcpy(status + kStatusRelease, release, sizeof(release));
  if (g_logs < kMaxLogs) {
    ++g_logs;
    ModLog("left hold    : %s: the other hand %s the object %p at %.1f %.1f "
           "%.1f with a speed of %.1f units/s (the drop action of the game "
           "gave %d). The game drew it %llu times in the hand, frame %llu",
           why,
           gone ? "lets go of" : "cannot let go of, and keeps,", object,
           matrix[9], matrix[10], matrix[11], sqrtf(Dot(speed, speed)),
           result, g_carry_draws, g_frame);
  }
  if (gone) g_carry = Carry();
}

// THE DRAW. GInterface::Draw (0x00518640, thiscall, once a frame) draws the
// object of the GMagicHand while a hand is in use: Object::DrawInHand, then
// for the local player CHand::DrawTheHeldObject, and the Holding state calls
// the draw of the object (vtable +0x614, 0). The carried object is in no
// GMagicHand, so after GInterface::Draw of the local player the hook calls
// that draw of the object.
typedef void(__fastcall* FaceDrawFn)(void* face, void* unused);
typedef void(__fastcall* DrawItselfFn)(void* self, void* unused, int how);
FaceDrawFn o_FaceDraw = nullptr;

void __fastcall h_FaceDraw(void* face, void* unused) {
  o_FaceDraw(face, unused);
  if (g_carry.object == nullptr || face == nullptr || face != Face()) return;
  void** vtable = *reinterpret_cast<void***>(g_carry.object);
  reinterpret_cast<DrawItselfFn>(vtable[kDrawItself / 4])(g_carry.object,
                                                          nullptr, 0);
  ++g_carry_draws;
}

}  // namespace

bool LeftHoldCarrying() { return g_carry.object != nullptr; }

void LeftHoldFrame(bool take, bool drop, bool usable, uint64_t frame) {
  g_frame = frame;
  if (g_carry.object != nullptr) {
    if (!CarryAlive()) {
      Log("the carried object is no longer in a hand, so the other hand lets "
          "it be");
      g_carry = Carry();
      return;
    }
    Sample();
    if (drop) {
      Drop(true, "the trigger opens");
    } else if (!usable) {
      Drop(false, "the controllers cannot act now");
    }
    return;
  }
  if (take) {
    TryPark();
  } else {
    g_take_held = nullptr;
  }
}

void LeftHoldLetGo(const char* why) {
  if (g_carry.object == nullptr) return;
  if (!CarryAlive()) {
    g_carry = Carry();
    return;
  }
  Drop(false, why);
}

void LeftHoldInstall() {
  static const uint8_t kFaceDrawBytes[9] = {0x56, 0x8B, 0xF1, 0x8B, 0x8E,
                                             0x9C, 0x03, 0x00, 0x00};
  const void* draw = reinterpret_cast<const void*>(kFaceDraw);
  if (IsBadReadPtr(draw, sizeof(kFaceDrawBytes)) ||
      memcmp(draw, kFaceDrawBytes, sizeof(kFaceDrawBytes)) != 0) {
    ModLog("hooks        : GInterface::Draw at 0x%08X does not have the "
           "bytes of the disassembly, so a carried object is not drawn",
           static_cast<unsigned>(kFaceDraw));
  } else {
    MH_STATUS status = MH_CreateHook(const_cast<void*>(draw),
                                     reinterpret_cast<void*>(&h_FaceDraw),
                                     reinterpret_cast<void**>(&o_FaceDraw));
    if (status == MH_OK) status = MH_EnableHook(const_cast<void*>(draw));
    ModLog("hooks        : GInterface::Draw (the carry of the other hand) at "
           "0x%08X %s", static_cast<unsigned>(kFaceDraw),
           status == MH_OK ? "ok" : "FAILED");
  }
  const bool ok = HandExactAddPrepareHook(&PlaceCarry);
  ModLog("left hold    : the call after the draw setup of the Hand %s",
         ok ? "is in" : "has no room, so a carried object is not placed");
}
