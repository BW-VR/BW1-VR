// WHAT A CAST MAKES IN THE WORLD. See docs/miracles.md, "What a cast makes".
//
// This file only reads the game and writes the log. It changes nothing.
//
// WHAT THE GAME HAS [V, runblack.exe 1.42].
// - Each SpellShield (Spiritual Shield and Physical Shield) is in a list:
//   the first one at 0x00DA07F0, the next one at +0x100 (SpellShield::
//   ToBeDeleted 0x0072B500). Its domes are MapShield objects in a list at
//   +0xEC, with the count at +0xF0 (SpellShield::InitWithPos 0x0072B5F0).
// - Each SpellStormAndTornado is in a list: the first one at 0x00DA07F8, the
//   next one at +0xF4 (0x0072DA20). Its particle system is at +0xEC. Its
//   Process (0x0072DB90) deletes that particle system when the spell has no
//   caster (+0xA0).
// - A Spell has its place at +0x14 (MapCoords), its life (the chants that
//   it holds) at +0x38, its strength at +0x94, its age in seconds at +0xB8,
//   the limit of the age at +0x50 (-1 for none), and its size (the radius of
//   a shield or a storm, Spell::GetSpellMagnitude 0x007202C0) at +0xBC.
//   Spell::CoreProcess (0x00720660) closes a spell when its strength is 0 or
//   less.
// - A Creature miracle (SpellCreature::InitWithObject 0x007222D0) puts an
//   entry in the table of the spells on the Creature (CreatureReceiveSpell,
//   Creature+0x370): 16 entries of 0x18 bytes from +4. An entry is active
//   while its +4 is not 0. The size of the Creature for the game is
//   fn_004F5E60 (thiscall on that table): entry 2 +0xC while it is active,
//   else entry 1 +0xC while it is active, else the base size
//   [Creature+0x160]+0x6C. The body (the LH3DCreature at [Creature+0x160]+
//   0x58) has the size that the draw uses at +0x90. The start, step and end
//   of Shrink and Enlarge (0x004F5520 to 0x004F5672) move the base size. On
//   each game turn Creature::ProcessState calls fn_004EFCE0, which gives the
//   base size to the body (LH3DCreature::SetSize, vtable 8, which writes
//   +0x90). Near the pen of the Temple the body gets a smaller size: at 16 m
//   of the game or less from its point at the pen (Creature+0x1200), in a
//   cone in front of the pen (GPlayer+0xA48, the Citadel, +0x30, with its
//   slot 0x890 not 0), the size goes from the base size at 16 m to 0.22 at
//   14 m and nearer.
// - Invisibility (the entry 7, 0x004F58B0 and 0x004F5960) sets Creature+
//   0x106C to 1 and starts the fizz (Creature::SetFizz 0x0047AB90): its
//   target at +0x12B4 goes to 0.75, and +0x12B0 follows it. Creature::
//   ProcessState (0x004737F9) gives +0x12B0 and the fizz material to the
//   draw of the body ([LH3DCreature+0x482C]+0xA0 and +0x9C) while the global
//   0x00D00DE4 is set.
// - Water (SpellWater::Process 0x00724ED0) makes one drop each game turn: a
//   point 0.3 to 0.3 + 0.7 R units from the place of the spell (Spell+0xCC,
//   a MapCoords, R from fn_005FACE0 of GMagicWaterInfo) in a random
//   direction. Spell::ApplyDefaultSpellEffect (0x00720C30, event 2) puts the
//   place of the spell (Spell+0x14) at that point with a height of 0, and
//   EffectValues::ApplyEffectToMapPos (0x00525100) gives the effect to each
//   object that IsEffectReceiver accepts, when the point is less than its
//   fire radius (slot 0x5F4) plus the radius of the effect from the middle of
//   its fire (slot 0x5F0), and the land under the point is less than its
//   height (slot 0x42C) plus that radius from the base of the object.
//   FireEffect::ApplyEffectToFireEffectIfNecessary (0x00730670) then moves
//   the temperature of the fire (FireEffect+0x14) toward the temperature of
//   the land plus the burn of the effect, by 10 divided by the heat capacity
//   (slot 0x5E4) of the gap. An object burns while its temperature is at
//   least its combustion temperature ([Object+0x28]+0xB4, at least 40,
//   fn_00730180). The fires are a list from GGame+0x205C14, the next one at
//   +0x3C, the object at +0x1C (FireEffect::ProcessList 0x00730760).
// - A Physical Shield is a body of the physics that stays in the array of
//   the physics (PhysicalShield::GetAlwaysRemainsInPhysicsInternalSystem).
//   Its body is built one time, at the full size of the dome (PhysOb::
//   Initialise 0x007FB780 with the scale of the object, 0.017 times the
//   radius of the spell). In each substep PhysicsObject::GameTurnUpdate
//   tests two bodies when their balls (PhysOb+0xA0, +0x150) meet, and then
//   PhysOb::CheckVFaces (0x007FDA60) tests the points of one body against the
//   faces of the other. A contact adds 1 to PhysOb+0x158.
// - Each teleport site of a player (MagicTeleport) is in a list from
//   GPlayer+0xA58, the next one at +0x88, its spell at +0x9C.
//   MagicTeleport::DoTeleport (0x005FC790, thiscall, the Living and a flag,
//   ret 8) moves a Living to the other site that is nearest its
//   destination.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "MinHook.h"
#include "miracle.h"

extern void ModLog(const char* format, ...);

// hand_direct.cpp: game units in a meter of the room, the class of an
// object, and the point of the real hand.
float HandDirectRoomUnits();
const char* HandDirectClass(void* object);
bool HandDirectRealPoint(float out[3]);

namespace {

const uintptr_t kShieldFirst = 0x00DA07F0;
const int kShieldNext = 0x100;
const int kShieldDomes = 0xEC;
const int kShieldDomeCount = 0xF0;
const uintptr_t kStormFirst = 0x00DA07F8;
const int kStormNext = 0xF4;
const int kStormParticles = 0xEC;
const int kSpellPlace = 0x14;
const int kSpellLife = 0x38;
const int kSpellAgeLimit = 0x50;
const int kSpellStrength = 0x94;
const int kSpellCaster = 0xA0;
const int kSpellAge = 0xB8;
const int kSpellSize = 0xBC;
const int kSpellBytes = 0x110;
const float kMapScale = 6553.6f;

const int kCreatureSpells = 0x370;
const int kCreatureLink = 0x160;
const int kLinkBody = 0x58;
const int kLinkBaseSize = 0x6C;
const int kBodySize = 0x90;
const int kBodyPlace = 0x78;          // the base of the body, in the world
const int kBodyDraw = 0x482C;         // the draw data of the body
const int kDrawFizz = 0xA0;
const int kDrawFizzMaterial = 0x9C;
const int kCreatureInvisible = 0x106C;
const int kCreatureFizz = 0x12B0;     // now, then the target at +0x12B4
const uintptr_t kFizzOn = 0x00D00DE4;
// LH3DIsland::GetAltitude, thiscall on a MapCoords, the height in st0.
const uintptr_t kAltitude = 0x00803090;
struct MapCoords {
  int32_t x, z;
  float over;
};
typedef float(__fastcall* AltitudeFn)(const MapCoords* at, void* unused);
// The pen of the Temple and the size of the body near it (fn_004EFCE0).
const int kCreaturePenPoint = 0x1200;  // MapCoords
const int kPlayerCitadel = 0xA48;
const int kCitadelPen = 0x30;
const int kPenIsOpen = 0x890;          // a vtable slot, thiscall, no arguments
const int kGetPlayer = 0x1C;           // a vtable slot of Object
// GUtils::GetDistanceInMetres, cdecl (const MapCoords*, const MapCoords*).
const uintptr_t kMetres = 0x0074CD70;
typedef float(__cdecl* MetresFn)(const void* a, const void* b);
typedef void*(__fastcall* ObjectFn)(void* self, void* unused);
typedef int(__fastcall* ObjectIntFn)(void* self, void* unused);

const int kReceiveEntries = 16;
const int kReceiveEntry = 0x18;
const int kReceiveBytes = 4 + kReceiveEntries * kReceiveEntry;
// fn_004F5E60, thiscall on the CreatureReceiveSpell, the size in st0.
const uintptr_t kCreatureSizeFn = 0x004F5E60;
const uint8_t kCreatureSizeBytes[6] = {0x56, 0x8B, 0xF1, 0x6A, 0x00, 0xE8};
typedef float(__fastcall* SizeFn)(void* spells, void* unused);
// The vtables of the game are in its .rdata.
const uintptr_t kRdataStart = 0x008A9000;
const uintptr_t kRdataEnd = 0x009C6000;

const int kMaxLogs = 320;
int g_logs = 0;

bool Log() {
  if (g_logs >= kMaxLogs) return false;
  ++g_logs;
  return true;
}

double NowSeconds() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return static_cast<double>(count.QuadPart) /
         static_cast<double>(frequency.QuadPart);
}

bool LiveObject(const void* object, size_t bytes) {
  if (object == nullptr || IsBadReadPtr(object, bytes)) return false;
  const uintptr_t table =
      reinterpret_cast<uintptr_t>(*static_cast<void* const*>(object));
  return table >= kRdataStart && table < kRdataEnd;
}

template <typename T>
T Read(const void* object, int offset) {
  return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(object) +
                                     offset);
}

float Cm(float units_of_world) {
  const float units = HandDirectRoomUnits();
  return units > 0.0f ? units_of_world / units * 100.0f : 0.0f;
}

// The metres of the game from the Creature to its point at the pen, or -1
// with no pen that counts.
float PenMetres(void* creature) {
  void* player = reinterpret_cast<ObjectFn>(
      (*static_cast<void***>(creature))[kGetPlayer / 4])(creature, nullptr);
  if (player == nullptr || IsBadReadPtr(player, kPlayerCitadel + 4)) {
    return -1.0f;
  }
  uint8_t* citadel = Read<uint8_t*>(player, kPlayerCitadel);
  if (citadel == nullptr || IsBadReadPtr(citadel, kCitadelPen + 4)) {
    return -1.0f;
  }
  void* pen = Read<void*>(citadel, kCitadelPen);
  if (!LiveObject(pen, 4)) return -1.0f;
  if (reinterpret_cast<ObjectIntFn>(
          (*static_cast<void***>(pen))[kPenIsOpen / 4])(pen, nullptr) == 0) {
    return -1.0f;
  }
  const uint8_t* c = static_cast<const uint8_t*>(creature);
  return reinterpret_cast<MetresFn>(kMetres)(c + 0x14, c + kCreaturePenPoint);
}

// --- the spells of a circle ---------------------------------------------

// The last cast at a circle, for the distance of a new spell from it.
const char* g_cast_name = "";
float g_cast_at[2] = {0.0f, 0.0f};
double g_cast_time = -1e9;

struct Watched {
  void* spell;
  bool storm;
  double since;
  double said;
  int lines;
};
const int kMaxWatched = 8;
Watched g_watched[kMaxWatched];
int g_watched_count = 0;
uint64_t g_spells_seen = 0;

const int kLines = 7;              // status lines every 2 s at the start
const double kLineSeconds = 2.0;
const int kMostLines = 30;         // then a line every 10 s, to this count
const double kLateSeconds = 10.0;

void SpellLine(const Watched& w, const char* what, uint64_t frame) {
  const void* s = w.spell;
  const float x = Read<int32_t>(s, kSpellPlace) / kMapScale;
  const float z = Read<int32_t>(s, kSpellPlace + 4) / kMapScale;
  const float dx = x - g_cast_at[0], dz = z - g_cast_at[1];
  const float size = Read<float>(s, kSpellSize);
  char extra[96];
  if (w.storm) {
    snprintf(extra, sizeof(extra), "Its particle system is %s",
             Read<void*>(s, kStormParticles) != nullptr ? "there" : "gone");
  } else {
    snprintf(extra, sizeof(extra), "It has %d domes (MapShield)",
             Read<int32_t>(s, kShieldDomeCount));
  }
  ModLog("miracle watch: the %s %p %s, %.1f s after it started: at %.1f %.1f, "
         "%.1f units from the cast of %s, with a radius of %.1f units (%.1f "
         "cm of the room). Its life is %.0f, its strength %.3f, its age "
         "%.1f s of %.1f s, the caster %p. %s, frame %llu",
         w.storm ? "storm" : "shield", w.spell, what,
         NowSeconds() - w.since, x, z, sqrtf(dx * dx + dz * dz), g_cast_name,
         size, Cm(size), Read<float>(s, kSpellLife),
         Read<float>(s, kSpellStrength), Read<float>(s, kSpellAge),
         Read<float>(s, kSpellAgeLimit), Read<void*>(s, kSpellCaster), extra,
         frame);
}

int ListSpells(uintptr_t first, int next, void* out[], int most) {
  int count = 0;
  void* const* head = reinterpret_cast<void* const*>(first);
  if (IsBadReadPtr(head, sizeof(void*))) return 0;
  for (void* s = *head; s != nullptr && count < most;
       s = Read<void*>(s, next)) {
    if (!LiveObject(s, kSpellBytes)) break;
    out[count++] = s;
  }
  return count;
}

void SpellsFrame(uint64_t frame) {
  void* now[2 * kMaxWatched];
  bool storm[2 * kMaxWatched];
  int count = ListSpells(kShieldFirst, kShieldNext, now, kMaxWatched);
  for (int i = 0; i < count; ++i) storm[i] = false;
  const int shields = count;
  count += ListSpells(kStormFirst, kStormNext, now + count, kMaxWatched);
  for (int i = shields; i < count; ++i) storm[i] = true;
  const double t = NowSeconds();
  // The spells that are gone.
  for (int k = 0; k < g_watched_count;) {
    bool there = false;
    for (int i = 0; i < count; ++i) there |= now[i] == g_watched[k].spell;
    if (there) {
      ++k;
      continue;
    }
    if (Log()) {
      ModLog("miracle watch: the %s %p is gone after %.1f s, frame %llu",
             g_watched[k].storm ? "storm" : "shield", g_watched[k].spell,
             t - g_watched[k].since, frame);
    }
    g_watched[k] = g_watched[--g_watched_count];
  }
  // The new spells, and a line every 2 s for each one.
  for (int i = 0; i < count; ++i) {
    Watched* w = nullptr;
    for (int k = 0; k < g_watched_count; ++k) {
      if (g_watched[k].spell == now[i]) w = &g_watched[k];
    }
    if (w == nullptr) {
      if (g_watched_count >= kMaxWatched) continue;
      w = &g_watched[g_watched_count++];
      w->spell = now[i];
      w->storm = storm[i];
      w->since = t;
      w->said = t;
      w->lines = 1;
      ++g_spells_seen;
      if (Log()) {
        SpellLine(*w, t - g_cast_time < 3.0 ? "starts after the cast"
                                            : "is there",
                  frame);
      }
      continue;
    }
    const double every = w->lines < kLines ? kLineSeconds : kLateSeconds;
    if (w->lines < kMostLines && t - w->said >= every) {
      w->said = t;
      ++w->lines;
      if (Log()) SpellLine(*w, "stands", frame);
    }
  }
}

// --- the Creature after a Creature miracle --------------------------------

void* g_creature = nullptr;
const char* g_creature_cast = "";
double g_creature_since = 0.0;
double g_creature_said = 0.0;
uint32_t g_creature_mask = 0;
float g_creature_size = 0.0f;
const double kCreatureSeconds = 25.0;
const double kCreatureEvery = 3.0;

void CreatureFrame(uint64_t frame, int hand_state) {
  if (g_creature == nullptr) return;
  const double t = NowSeconds();
  if (t - g_creature_since > kCreatureSeconds) {
    g_creature = nullptr;
    return;
  }
  if (!LiveObject(g_creature, kCreatureFizz + 8)) {
    if (Log()) {
      ModLog("miracle watch: the Creature %p is gone, frame %llu", g_creature,
             frame);
    }
    g_creature = nullptr;
    return;
  }
  uint8_t* spells = Read<uint8_t*>(g_creature, kCreatureSpells);
  const uint8_t* link = Read<uint8_t*>(g_creature, kCreatureLink);
  if (spells == nullptr || IsBadReadPtr(spells, kReceiveBytes) ||
      link == nullptr || IsBadReadPtr(link, kLinkBaseSize + 4)) {
    return;
  }
  uint32_t mask = 0;
  for (int i = 0; i < kReceiveEntries; ++i) {
    if (Read<uint32_t>(spells, 4 + i * kReceiveEntry + 4) != 0) mask |= 1u << i;
  }
  float size = 0.0f;
  const bool have_size =
      !IsBadReadPtr(reinterpret_cast<const void*>(kCreatureSizeFn),
                    sizeof(kCreatureSizeBytes)) &&
      memcmp(reinterpret_cast<const void*>(kCreatureSizeFn),
             kCreatureSizeBytes, sizeof(kCreatureSizeBytes)) == 0;
  if (have_size) {
    size = reinterpret_cast<SizeFn>(kCreatureSizeFn)(spells, nullptr);
  }
  const bool changed =
      mask != g_creature_mask || fabsf(size - g_creature_size) > 0.001f;
  if (!changed && t - g_creature_said < kCreatureEvery) return;
  g_creature_mask = mask;
  g_creature_size = size;
  g_creature_said = t;
  if (!Log()) return;
  const uint8_t* body = Read<uint8_t*>(link, kLinkBody);
  const bool have_body = body != nullptr && !IsBadReadPtr(body, kBodyDraw + 4);
  const float drawn = have_body ? Read<float>(body, kBodySize) : 0.0f;
  // How high the base of the body is over the land, and the fizz of the
  // draw.
  float over = 0.0f, draw_fizz = -1.0f;
  void* material = nullptr;
  if (have_body) {
    const float* at = reinterpret_cast<const float*>(body + kBodyPlace);
    const MapCoords coords = {static_cast<int32_t>(at[0] * kMapScale),
                              static_cast<int32_t>(at[2] * kMapScale), 0.0f};
    if (coords.x > 0 && coords.z > 0) {
      over = at[1] - reinterpret_cast<AltitudeFn>(kAltitude)(&coords, nullptr);
    }
    const uint8_t* draw = Read<uint8_t*>(body, kBodyDraw);
    if (draw != nullptr && !IsBadReadPtr(draw, kDrawFizz + 4)) {
      draw_fizz = Read<float>(draw, kDrawFizz);
      material = Read<void*>(draw, kDrawFizzMaterial);
    }
  }
  const float pen = PenMetres(g_creature);
  char pen_text[160];
  if (pen < 0.0f) {
    snprintf(pen_text, sizeof(pen_text), "No pen of the Temple counts");
  } else {
    const float base = Read<float>(link, kLinkBaseSize);
    const float d = pen < 14.0f ? 14.0f : pen > 16.0f ? 16.0f : pen;
    const float near_pen =
        pen >= 16.0f ? base : (d - 14.0f) * (base - 0.22f) / 2.0f + 0.22f;
    snprintf(pen_text, sizeof(pen_text),
             "It is %.1f m of the game from its point at the pen of the "
             "Temple, so in front of the pen the body gets the size %.3f",
             pen, near_pen);
  }
  const void* fizz_on = reinterpret_cast<const void*>(kFizzOn);
  const int fizz_global =
      IsBadReadPtr(fizz_on, 4) ? -1 : Read<int32_t>(fizz_on, 0);
  char list[400];
  int used = 0;
  list[0] = 0;
  for (int i = 0; i < kReceiveEntries && used < 360; ++i) {
    if ((mask & (1u << i)) == 0) continue;
    const uint8_t* e = spells + 4 + i * kReceiveEntry;
    used += snprintf(list + used, sizeof(list) - used,
                     "%sentry %d (type %d, %08X, %.4g %.4g %.4g %.4g)",
                     used > 0 ? ", " : "", i, Read<int32_t>(e, 0),
                     Read<uint32_t>(e, 4), Read<float>(e, 8),
                     Read<float>(e, 0xC), Read<float>(e, 0x10),
                     Read<float>(e, 0x14));
  }
  ModLog("miracle watch: the Creature %p, %.1f s after the cast of %s: its "
         "size for the game is %.3f%s, the base size %.3f, the size of the "
         "drawn body %.3f. The base of the body is %.1f cm of the room over "
         "the land, and the Hand is in state %d. Invisible %d, the fizz %.2f "
         "to %.2f, the fizz of the draw %.2f with the material %p (the "
         "switch 0x00D00DE4 is %d). %s. %s%s, frame %llu", g_creature,
         t - g_creature_since, g_creature_cast, size,
         have_size ? "" : " (fn_004F5E60 has other bytes, so no size)",
         Read<float>(link, kLinkBaseSize), drawn, Cm(over), hand_state,
         Read<int32_t>(g_creature, kCreatureInvisible),
         Read<float>(g_creature, kCreatureFizz),
         Read<float>(g_creature, kCreatureFizz + 4), draw_fizz, material,
         fizz_global, pen_text,
         mask == 0 ? "No spell is on it" : "The spells on it: ",
         list, frame);
}

uint64_t g_frames = 0;
uint64_t g_last_frame = 0;

typedef float(__fastcall* ObjectFloatFn)(void* self, void* unused);
typedef void(__fastcall* ObjectCoordsFn)(void* self, void* unused,
                                         MapCoords* out);

float SlotFloat(void* object, int slot) {
  return reinterpret_cast<ObjectFloatFn>(
      (*static_cast<void***>(object))[slot / 4])(object, nullptr);
}

bool HookAt(uintptr_t address, const uint8_t* bytes, size_t size,
            void* replacement, void** original, const char* name) {
  const void* target = reinterpret_cast<const void*>(address);
  MH_STATUS status = MH_ERROR_NOT_EXECUTABLE;
  if (!IsBadReadPtr(target, size) && memcmp(target, bytes, size) == 0) {
    status = MH_CreateHook(const_cast<void*>(target), replacement, original);
    if (status == MH_OK) status = MH_EnableHook(const_cast<void*>(target));
  }
  ModLog("hooks        : %s at 0x%08X %s", name,
         static_cast<unsigned>(address),
         status == MH_OK ? "ok" : "FAILED (other bytes, or no hook)");
  return status == MH_OK;
}

// --- the drops of Water -----------------------------------------------------

const uintptr_t kSpellEffect = 0x00720C30;  // thiscall (SpellEventInfo*), ret 4
const uint8_t kSpellEffectBytes[6] = {0x83, 0xEC, 0x68, 0x53, 0x55, 0x56};
const uintptr_t kSpellWaterTable = 0x008F553C;
const int kEventDrop = 2;
const int kEventPoint = 0x04;        // an LHPoint
const int kSpellCastPlace = 0xCC;    // MapCoords
const uintptr_t kGame = 0x00D0195C;
const int kGameFires = 0x205C14;
const int kFireFlags = 0x0A;         // bit 1: ProcessList skips the fire
const int kFireTemperature = 0x14;
const int kFireObject = 0x1C;
const int kFireNext = 0x3C;
const int kObjectPlace = 0x14;       // MapCoords
const int kObjectInfo = 0x28;
const int kObjectFire = 0x44;
const int kInfoCombustion = 0xB4;
const int kSlotFireCentre = 0x5F0;   // GetDefaultFireCentrePos (MapCoords*)
const int kSlotFireRadius = 0x5F4;   // GetDefaultFireRadius
const int kSlotHeight = 0x42C;       // GetHeight
const int kSlotHeatCapacity = 0x5E4; // GetHeatCapacity
const float kFireRange = 30.0f;      // units: the fires that a line counts
const float kLeastIgnition = 40.0f;  // fn_00730180

typedef int(__fastcall* SpellEffectFn)(void* spell, void* unused,
                                       const void* event);
SpellEffectFn o_SpellEffect = nullptr;

struct Fire {
  uint8_t* fire;
  void* object;
  float before;
};
const int kMostFires = 16;

int CollectFires(const MapCoords& at, Fire out[], int most) {
  const uint8_t* game = *reinterpret_cast<uint8_t* const*>(kGame);
  if (game == nullptr || IsBadReadPtr(game + kGameFires, 4)) return 0;
  int count = 0, guard = 0;
  for (uint8_t* f = Read<uint8_t*>(game, kGameFires);
       f != nullptr && count < most && guard < 4096;
       f = Read<uint8_t*>(f, kFireNext), ++guard) {
    if (IsBadReadPtr(f, kFireNext + 4)) break;
    if ((f[kFireFlags] & 1) != 0) continue;
    void* object = Read<void*>(f, kFireObject);
    if (!LiveObject(object, kObjectFire + 4)) continue;
    const MapCoords& p = *reinterpret_cast<const MapCoords*>(
        static_cast<uint8_t*>(object) + kObjectPlace);
    const float dx = (p.x - at.x) / kMapScale, dz = (p.z - at.z) / kMapScale;
    if (dx * dx + dz * dz > kFireRange * kFireRange) continue;
    out[count].fire = f;
    out[count].object = object;
    out[count].before = Read<float>(f, kFireTemperature);
    ++count;
  }
  return count;
}

float Ignition(void* object) {
  const uint8_t* info = Read<uint8_t*>(object, kObjectInfo);
  if (info == nullptr || IsBadReadPtr(info, kInfoCombustion + 4)) {
    return kLeastIgnition;
  }
  const float t = Read<float>(info, kInfoCombustion);
  return t > kLeastIgnition ? t : kLeastIgnition;
}

void* g_drop_spell = nullptr;
int g_drops = 0;           // the drops of this spell
int g_drops_cooled = 0;    // the drops that cooled a burning object
double g_drop_since = 0.0;
double g_drop_last = 0.0;
int g_drop_logs = 0;
const int kMaxDropLogs = 150;
// The burning objects near the pour, for the lines each second: each object
// that was within 30 units of a drop.
const int kMaxWatchFires = 6;
void* g_watch_fires[kMaxWatchFires];
bool g_watch_fire_out[kMaxWatchFires];   // its line "no fire now" came
int g_watch_fire_count = 0;
MapCoords g_last_drop = {};
double g_watch_fire_said = 0.0;
int g_watch_fire_logs = 0;
const int kMaxWatchFireLogs = 200;
const double kWatchFireSeconds = 30.0;
const int kFireGraphic = 0x2C;   // FireEffect::Draw draws this, or nothing
const float kFractionStart = 0.9f;  // GetFireFraction 0x007303E0

void WatchFire(void* object) {
  for (int i = 0; i < g_watch_fire_count; ++i) {
    if (g_watch_fires[i] == object) return;
  }
  if (g_watch_fire_count >= kMaxWatchFires) return;
  g_watch_fire_out[g_watch_fire_count] = false;
  g_watch_fires[g_watch_fire_count++] = object;
}

void DropSummary(const char* why) {
  if (g_drop_spell == nullptr || g_drops == 0) return;
  if (g_drop_logs < kMaxDropLogs) {
    ++g_drop_logs;
    ModLog("miracle watch: the Water spell %p made %d drops in %.1f s (%s), "
           "and %d of them cooled a burning object, frame %llu",
           g_drop_spell, g_drops, g_drop_last - g_drop_since, why,
           g_drops_cooled, g_last_frame);
  }
}

int __fastcall h_SpellEffect(void* spell, void* unused, const void* event) {
  const bool water =
      spell != nullptr && !IsBadReadPtr(spell, kSpellCastPlace + 12) &&
      *static_cast<const uintptr_t*>(spell) == kSpellWaterTable &&
      event != nullptr && !IsBadReadPtr(event, kEventPoint + 12) &&
      Read<int32_t>(event, 0) == kEventDrop;
  if (!water) return o_SpellEffect(spell, unused, event);
  const float* point =
      reinterpret_cast<const float*>(static_cast<const uint8_t*>(event) +
                                     kEventPoint);
  // The point as the game makes it (0x00720C83 to 0x00720CB1).
  const MapCoords drop = {static_cast<int32_t>(point[0] * kMapScale),
                          static_cast<int32_t>(point[2] * kMapScale), 0.0f};
  Fire fires[kMostFires];
  const int count = CollectFires(drop, fires, kMostFires);
  const int result = o_SpellEffect(spell, unused, event);
  const double t = NowSeconds();
  if (spell != g_drop_spell) {
    DropSummary("a new spell starts");
    g_drop_spell = spell;
    g_drops = 0;
    g_drops_cooled = 0;
    g_drop_since = t;
  }
  ++g_drops;
  g_drop_last = t;
  int cooled = 0, nearest = -1;
  float best = 1e30f;
  float centre_d[kMostFires];
  MapCoords centre[kMostFires];
  for (int i = 0; i < count; ++i) {
    if (Read<float>(fires[i].fire, kFireTemperature) < fires[i].before) {
      ++cooled;
    }
    reinterpret_cast<ObjectCoordsFn>(
        (*static_cast<void***>(fires[i].object))[kSlotFireCentre / 4])(
        fires[i].object, nullptr, &centre[i]);
    centre_d[i] = reinterpret_cast<MetresFn>(kMetres)(&drop, &centre[i]);
    if (centre_d[i] < best) {
      best = centre_d[i];
      nearest = i;
    }
  }
  if (cooled > 0) ++g_drops_cooled;
  if (g_drops == 1) g_watch_fire_count = 0;
  for (int i = 0; i < count; ++i) WatchFire(fires[i].object);
  g_last_drop = drop;
  if (count > 0) g_watch_fire_said = t;
  if (!(g_drops <= 12 || g_drops % 10 == 0) || g_drop_logs >= kMaxDropLogs) {
    return result;
  }
  ++g_drop_logs;
  const uint8_t* s = static_cast<const uint8_t*>(spell);
  const MapCoords& place =
      *reinterpret_cast<const MapCoords*>(s + kSpellCastPlace);
  const float px = place.x / kMapScale, pz = place.z / kMapScale;
  const float x = drop.x / kMapScale, z = drop.z / kMapScale;
  float hand[3];
  char from_hand[96] = "";
  if (HandDirectRealPoint(hand)) {
    snprintf(from_hand, sizeof(from_hand),
             " and %.1f units across the land from the Hand's point",
             sqrtf((x - hand[0]) * (x - hand[0]) + (z - hand[2]) * (z - hand[2])));
  }
  if (nearest < 0) {
    ModLog("miracle watch: drop %d of the Water spell %p at %.1f %.1f, %.1f "
           "units from the place of the spell (%.1f %.1f)%s. No burning "
           "object is within %.0f units, frame %llu", g_drops, spell, x, z,
           sqrtf((x - px) * (x - px) + (z - pz) * (z - pz)), px, pz,
           from_hand, kFireRange, g_last_frame);
    return result;
  }
  const Fire& f = fires[nearest];
  uint8_t* object = static_cast<uint8_t*>(f.object);
  const float radius = SlotFloat(object, kSlotFireRadius);
  const float height = SlotFloat(object, kSlotHeight);
  const float capacity = SlotFloat(object, kSlotHeatCapacity);
  const AltitudeFn altitude = reinterpret_cast<AltitudeFn>(kAltitude);
  const MapCoords& base =
      *reinterpret_cast<const MapCoords*>(object + kObjectPlace);
  const float up = fabsf(altitude(&drop, nullptr) -
                         (altitude(&base, nullptr) + base.over));
  ModLog("miracle watch: drop %d of the Water spell %p at %.1f %.1f, %.1f "
         "units from the place of the spell (%.1f %.1f)%s. The nearest "
         "burning object is %p (%s) with the middle of its fire at %.1f "
         "%.1f: the drop is %.2f units from that middle, and its fire radius "
         "is %.2f (the effect comes when the distance is less than that "
         "radius plus the radius of the effect). The land under the drop is "
         "%.2f units from the base of the object, which is %.2f high. Its "
         "temperature goes from %.1f to %.1f: it burns at %.1f or more, and "
         "its heat capacity is %.2f. The drop cooled %d of the %d burning "
         "objects within %.0f units, frame %llu", g_drops, spell, x, z,
         sqrtf((x - px) * (x - px) + (z - pz) * (z - pz)), px, pz, from_hand,
         object, HandDirectClass(object), centre[nearest].x / kMapScale,
         centre[nearest].z / kMapScale, centre_d[nearest], radius, up, height,
         f.before, Read<float>(f.fire, kFireTemperature), Ignition(object),
         capacity, cooled, count, kFireRange, g_last_frame);
  return result;
}

// Each second after a drop, for 30 s: each burning object that was near the
// pour, with the temperature of its fire, the fire fraction of the game
// (FireEffect::GetFireFraction 0x007303E0: 0 at 0.9 times the temperature at
// which it burns, 1 at 2 times it) and whether it has a graphic of flames
// (FireEffect+0x2C, which FireEffect::Draw 0x00730330 draws).
void DropsFrame() {
  const double t = NowSeconds();
  if (g_drop_spell != nullptr && g_drops > 0 && t - g_drop_last > 3.0) {
    DropSummary("no drop for 3 s");
    g_drop_spell = nullptr;
  }
  if (g_watch_fire_count == 0 || t - g_watch_fire_said < 1.0) return;
  if (t - g_drop_last > kWatchFireSeconds ||
      g_watch_fire_logs >= kMaxWatchFireLogs) {
    g_watch_fire_count = 0;
    return;
  }
  g_watch_fire_said = t;
  for (int i = 0; i < g_watch_fire_count; ++i) {
    void* object = g_watch_fires[i];
    if (g_watch_fire_out[i] || g_watch_fire_logs >= kMaxWatchFireLogs) {
      continue;
    }
    ++g_watch_fire_logs;
    if (!LiveObject(object, kObjectFire + 4)) {
      ModLog("miracle watch: the object %p that was near the Water is gone, "
             "%.1f s after the last drop, frame %llu", object,
             t - g_drop_last, g_last_frame);
      g_watch_fire_out[i] = true;
      continue;
    }
    const MapCoords& at = *reinterpret_cast<const MapCoords*>(
        static_cast<uint8_t*>(object) + kObjectPlace);
    const float dx = (at.x - g_last_drop.x) / kMapScale;
    const float dz = (at.z - g_last_drop.z) / kMapScale;
    const float from_drop = sqrtf(dx * dx + dz * dz);
    const uint8_t* fire = Read<uint8_t*>(object, kObjectFire);
    if (fire == nullptr || IsBadReadPtr(fire, kFireGraphic + 4)) {
      ModLog("miracle watch: %p (%s) at %.1f %.1f, %.1f units from the last "
             "drop, has no fire now, %.1f s after the last drop of Water, "
             "frame %llu", object, HandDirectClass(object), at.x / kMapScale,
             at.z / kMapScale, from_drop, t - g_drop_last, g_last_frame);
      g_watch_fire_out[i] = true;
      continue;
    }
    const float temperature = Read<float>(fire, kFireTemperature);
    const float ignition = Ignition(object);
    float fraction = (temperature - kFractionStart * ignition) /
                     ((2.0f - kFractionStart) * ignition);
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    ModLog("miracle watch: %p (%s) at %.1f %.1f, %.1f units from the last "
           "drop, %.1f s after the last drop of Water: the temperature of "
           "its fire is %.1f, so it %s (it burns at %.1f or more). The fire "
           "fraction is %.2f, and it %s a graphic of flames, frame %llu",
           object, HandDirectClass(object), at.x / kMapScale,
           at.z / kMapScale, from_drop, t - g_drop_last, temperature,
           temperature >= ignition ? "burns" : "does not burn", ignition,
           fraction,
           Read<void*>(fire, kFireGraphic) != nullptr ? "has" : "has no",
           g_last_frame);
  }
}

// --- the physics of a Physical Shield ---------------------------------------

const uintptr_t kPhysicsArray = 0x00D47814;
const uintptr_t kPhysicsCount = 0x00D47818;
const int kEntrySize = 0x1DC;
const int kEntryObject = 0x18;
const int kEntryPhysOb = 0x28;
const int kObCentre = 0xA0;          // LHPoint
const int kObVelocity = 0xDC;        // LHPoint
const int kObRadius = 0x150;         // the ball of the body
const int kObPoints = 0x154;
const int kObContacts = 0x158;
const int kObPointList = 0x15C;
const int kObFaces = 0x160;
const int kObBox = 0x168;            // a CollideBox, or null
const int kObRest = 0x174;           // a byte
const int kPointSize = 0x50;
const int kPointPlace = 0x34;        // LHPoint in the world
const int kObjectScale = 0x50;       // Object::GetScale
const int kShieldSpell = 0x60;       // MapShield: its SpellShield
const uintptr_t kPhysicalShieldTable = 0x00983F64;
const uintptr_t kCheckFaces = 0x007FDA60;  // thiscall (PhysOb* other), ret 4
const uint8_t kCheckFacesBytes[5] = {0x83, 0xEC, 0x3C, 0x53, 0x55};

typedef void(__fastcall* CheckFacesFn)(void* ob, void* unused, void* other);
CheckFacesFn o_CheckFaces = nullptr;

// The tests of CheckVFaces with a shield since the last line.
struct ShieldPairs {
  int into_calls, into_contacts;   // the points of a mover against a shield
  int out_calls, out_contacts;     // the points of a shield against a mover
};
ShieldPairs g_pairs = {};
bool g_shield_in_physics = false;
void* g_shield_said = nullptr;
int g_shield_logs = 0;
const int kMaxShieldLogs = 120;
uint64_t g_shield_line_frame = 0;

uint8_t* EntryOfOb(void* ob) {
  uint8_t* base = *reinterpret_cast<uint8_t* const*>(kPhysicsArray);
  const int32_t count = *reinterpret_cast<const int32_t*>(kPhysicsCount);
  uint8_t* entry = static_cast<uint8_t*>(ob) - kEntryPhysOb;
  if (base == nullptr || count <= 0 || entry < base ||
      entry >= base + count * kEntrySize ||
      (entry - base) % kEntrySize != 0) {
    return nullptr;
  }
  return entry;
}

bool ShieldEntry(const uint8_t* entry) {
  void* object = Read<void*>(entry, kEntryObject);
  return object != nullptr && !IsBadReadPtr(object, 4) &&
         *static_cast<const uintptr_t*>(object) == kPhysicalShieldTable;
}

void __fastcall h_CheckFaces(void* ob, void* unused, void* other) {
  if (!g_shield_in_physics || ob == nullptr || other == nullptr) {
    o_CheckFaces(ob, unused, other);
    return;
  }
  const int32_t before = Read<int32_t>(ob, kObContacts);
  o_CheckFaces(ob, unused, other);
  const int32_t added = Read<int32_t>(ob, kObContacts) - before;
  const uint8_t* to = EntryOfOb(other);
  const uint8_t* from = EntryOfOb(ob);
  if (to != nullptr && ShieldEntry(to)) {
    ++g_pairs.into_calls;
    g_pairs.into_contacts += added;
  } else if (from != nullptr && ShieldEntry(from)) {
    ++g_pairs.out_calls;
    g_pairs.out_contacts += added;
  }
}

float Gap3(const float* a, const float* b) {
  const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
  return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

void ShieldLine(const uint8_t* shield_entry) {
  void* shield = Read<void*>(shield_entry, kEntryObject);
  const uint8_t* ob = shield_entry + kEntryPhysOb;
  const float* c = reinterpret_cast<const float*>(ob + kObCentre);
  const int32_t points = Read<int32_t>(ob, kObPoints);
  const uint8_t* list = Read<uint8_t*>(ob, kObPointList);
  float far_point = 0.0f, low = 0.0f, high = 0.0f;
  if (list != nullptr && points > 0 && points < 4096 &&
      !IsBadReadPtr(list, points * kPointSize)) {
    for (int i = 0; i < points; ++i) {
      const float* p =
          reinterpret_cast<const float*>(list + i * kPointSize + kPointPlace);
      const float d = Gap3(p, c);
      if (d > far_point) far_point = d;
      if (i == 0 || p[1] < low) low = p[1];
      if (i == 0 || p[1] > high) high = p[1];
    }
  }
  const uint8_t* spell = Read<uint8_t*>(shield, kShieldSpell);
  const float spell_radius =
      spell != nullptr && !IsBadReadPtr(spell, kSpellSize + 4)
          ? Read<float>(spell, kSpellSize)
          : -1.0f;
  ModLog("miracle watch: the physics has the body of the Physical Shield %p "
         "(the scale of the object %.3f, the radius of its spell %.1f units): "
         "its middle is at %.1f %.1f %.1f, the radius of its ball is %.1f, "
         "and it has %d points from %.1f to %.1f high and to %.1f units from "
         "the middle, %d faces, %s, and its rest byte is %d, frame %llu",
         shield, Read<float>(shield, kObjectScale), spell_radius, c[0], c[1],
         c[2], Read<float>(ob, kObRadius), points, low, high, far_point,
         Read<int32_t>(ob, kObFaces),
         Read<void*>(ob, kObBox) != nullptr ? "a box of collision"
                                            : "no box of collision",
         ob[kObRest], g_last_frame);
}

// Once a frame: the shields in the physics, and each moving body near one.
void ShieldsFrame() {
  uint8_t* base = *reinterpret_cast<uint8_t* const*>(kPhysicsArray);
  const int32_t count = *reinterpret_cast<const int32_t*>(kPhysicsCount);
  g_shield_in_physics = false;
  if (base == nullptr || count <= 0 || count > 4096 ||
      IsBadReadPtr(base, count * kEntrySize)) {
    return;
  }
  const uint8_t* shield = nullptr;
  for (int i = 0; i < count && shield == nullptr; ++i) {
    if (ShieldEntry(base + i * kEntrySize)) shield = base + i * kEntrySize;
  }
  if (shield == nullptr) return;
  g_shield_in_physics = true;
  void* shield_object = Read<void*>(shield, kEntryObject);
  if (shield_object != g_shield_said && g_shield_logs < kMaxShieldLogs) {
    ++g_shield_logs;
    g_shield_said = shield_object;
    ShieldLine(shield);
  }
  // A line each 6 frames while a moving body is near the shield.
  if (g_last_frame < g_shield_line_frame + 6 ||
      g_shield_logs >= kMaxShieldLogs) {
    return;
  }
  const uint8_t* sob = shield + kEntryPhysOb;
  const float* sc = reinterpret_cast<const float*>(sob + kObCentre);
  const float sr = Read<float>(sob, kObRadius);
  bool said = false;
  for (int i = 0; i < count && g_shield_logs < kMaxShieldLogs; ++i) {
    const uint8_t* entry = base + i * kEntrySize;
    const uint8_t* ob = entry + kEntryPhysOb;
    if (entry == shield || ob[kObRest] != 0 || ShieldEntry(entry)) continue;
    const float* c = reinterpret_cast<const float*>(ob + kObCentre);
    const float d = Gap3(c, sc);
    const float r = Read<float>(ob, kObRadius);
    if (!(d < 3.0f * sr + 20.0f)) continue;
    const int32_t points = Read<int32_t>(ob, kObPoints);
    const uint8_t* list = Read<uint8_t*>(ob, kObPointList);
    int inside = 0;
    if (list != nullptr && points > 0 && points < 1024 &&
        !IsBadReadPtr(list, points * kPointSize)) {
      for (int k = 0; k < points; ++k) {
        const float* p = reinterpret_cast<const float*>(
            list + k * kPointSize + kPointPlace);
        if (Gap3(p, sc) < sr) ++inside;
      }
    }
    void* object = Read<void*>(entry, kEntryObject);
    const float* v = reinterpret_cast<const float*>(ob + kObVelocity);
    ++g_shield_logs;
    said = true;
    ModLog("miracle watch: the moving body of %p (%s) at %.1f %.1f %.1f is "
           "%.1f units from the middle of the body of the Physical Shield "
           "(the sum of the two radii is %.1f: the game tests the pair when "
           "the distance is less). %d of its %d points are inside the ball "
           "of the shield, and its speed is %.1f units/s. Since the last "
           "line the game tested the points of moving bodies against the "
           "shield %d times with %d contacts, and the points of the shield "
           "against them %d times with %d contacts, frame %llu",
           object, LiveObject(object, 4) ? HandDirectClass(object) : "gone",
           c[0], c[1], c[2], d, sr + r, inside, points,
           sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]),
           g_pairs.into_calls, g_pairs.into_contacts, g_pairs.out_calls,
           g_pairs.out_contacts, g_last_frame);
  }
  if (said) {
    g_shield_line_frame = g_last_frame;
    g_pairs = {};
  }
}

// --- the teleport sites -----------------------------------------------------

const int kGamePlayers = 0x18;
const int kGamePlayerSize = 0xA60;
const int kGameLocalPlayer = 0x205A59;  // a byte
const int kPlayerTeleports = 0xA58;
const int kTeleportNext = 0x88;
const int kTeleportSpell = 0x9C;
const int kSpellSeedOf = 0xAC;
const uintptr_t kMagicTeleportTable = 0x0092C110;
const uintptr_t kDoTeleport = 0x005FC790;
const uint8_t kDoTeleportBytes[7] = {0x83, 0xEC, 0x3C, 0x53, 0x55, 0x56, 0x57};

typedef int(__fastcall* DoTeleportFn)(void* site, void* unused, void* living,
                                      int flag);
DoTeleportFn o_DoTeleport = nullptr;

const int kMaxSites = 8;
void* g_sites[kMaxSites];
int g_site_count = -1;
int g_site_logs = 0;
const int kMaxSiteLogs = 60;

int ListSites(void* out[], int most) {
  const uint8_t* game = *reinterpret_cast<uint8_t* const*>(kGame);
  if (game == nullptr || IsBadReadPtr(game + kGameLocalPlayer, 1)) return 0;
  const uint8_t* player =
      game + kGamePlayers + kGamePlayerSize * game[kGameLocalPlayer];
  if (IsBadReadPtr(player + kPlayerTeleports, 4)) return 0;
  int count = 0;
  for (void* site = Read<void*>(player, kPlayerTeleports);
       site != nullptr && count < most; site = Read<void*>(site, kTeleportNext)) {
    if (IsBadReadPtr(site, kTeleportSpell + 4) ||
        *static_cast<const uintptr_t*>(site) != kMagicTeleportTable) {
      break;
    }
    out[count++] = site;
  }
  return count;
}

void SitesFrame() {
  if (g_last_frame % 30 != 0 || g_site_logs >= kMaxSiteLogs) return;
  void* now[kMaxSites];
  const int count = ListSites(now, kMaxSites);
  bool same = count == g_site_count;
  for (int i = 0; same && i < count; ++i) same = now[i] == g_sites[i];
  if (same) return;
  for (int i = 0; i < count; ++i) g_sites[i] = now[i];
  g_site_count = count;
  ++g_site_logs;
  char list[400];
  int used = 0;
  list[0] = 0;
  for (int i = 0; i < count && used < 360; ++i) {
    const uint8_t* site = static_cast<const uint8_t*>(now[i]);
    const MapCoords& p = *reinterpret_cast<const MapCoords*>(site + kObjectPlace);
    const uint8_t* spell = Read<uint8_t*>(site, kTeleportSpell);
    void* seed = spell != nullptr && !IsBadReadPtr(spell, kSpellSeedOf + 4)
                     ? Read<void*>(spell, kSpellSeedOf)
                     : nullptr;
    used += snprintf(list + used, sizeof(list) - used,
                     "%s%p at %.1f %.1f (spell %p, seed %p)",
                     used > 0 ? ", " : "", now[i], p.x / kMapScale,
                     p.z / kMapScale, spell, seed);
  }
  ModLog("miracle watch: the player has %d teleport sites%s%s, frame %llu",
         count, count > 0 ? ": " : "", list, g_last_frame);
}

// The Living of the last teleport, for the lines after it.
void* g_follow_living = nullptr;
double g_follow_since = 0.0;
int g_follow_step = 0;
const double kFollowSeconds[3] = {0.5, 2.0, 5.0};

// The place of the Living 0.5 s, 2 s and 5 s after its teleport.
void TeleportFollow() {
  if (g_follow_living == nullptr) return;
  const double t = NowSeconds() - g_follow_since;
  if (t < kFollowSeconds[g_follow_step]) return;
  void* living = g_follow_living;
  if (++g_follow_step >= 3) g_follow_living = nullptr;
  if (!LiveObject(living, kObjectPlace + 12) || g_site_logs >= kMaxSiteLogs) {
    return;
  }
  ++g_site_logs;
  const MapCoords& at = *reinterpret_cast<const MapCoords*>(
      static_cast<uint8_t*>(living) + kObjectPlace);
  void* sites[kMaxSites];
  const int count = ListSites(sites, kMaxSites);
  char list[200];
  int used = 0;
  list[0] = 0;
  for (int i = 0; i < count && used < 160; ++i) {
    const MapCoords& p = *reinterpret_cast<const MapCoords*>(
        static_cast<uint8_t*>(sites[i]) + kObjectPlace);
    const float dx = (p.x - at.x) / kMapScale, dz = (p.z - at.z) / kMapScale;
    used += snprintf(list + used, sizeof(list) - used, "%s%.1f units from %p",
                     used > 0 ? ", " : "", sqrtf(dx * dx + dz * dz), sites[i]);
  }
  ModLog("miracle watch: %.1f s after its teleport %p (%s) is at %.1f %.1f: "
         "%s, frame %llu", t, living, HandDirectClass(living),
         at.x / kMapScale, at.z / kMapScale, count > 0 ? list : "no site",
         g_last_frame);
}

int __fastcall h_DoTeleport(void* site, void* unused, void* living,
                            int flag) {
  MapCoords before = {};
  const bool have = LiveObject(living, kObjectPlace + 12);
  if (have) {
    before = *reinterpret_cast<const MapCoords*>(
        static_cast<uint8_t*>(living) + kObjectPlace);
  }
  const int result = o_DoTeleport(site, unused, living, flag);
  if (!have || g_site_logs >= kMaxSiteLogs) return result;
  ++g_site_logs;
  const MapCoords after = *reinterpret_cast<const MapCoords*>(
      static_cast<uint8_t*>(living) + kObjectPlace);
  const MapCoords& in = *reinterpret_cast<const MapCoords*>(
      static_cast<uint8_t*>(site) + kObjectPlace);
  void* sites[kMaxSites];
  const int count = ListSites(sites, kMaxSites);
  // The site nearest to the place after the call.
  void* out = nullptr;
  float best = 1e30f;
  for (int i = 0; i < count; ++i) {
    const MapCoords& p = *reinterpret_cast<const MapCoords*>(
        static_cast<uint8_t*>(sites[i]) + kObjectPlace);
    const float dx = (p.x - after.x) / kMapScale;
    const float dz = (p.z - after.z) / kMapScale;
    const float d = sqrtf(dx * dx + dz * dz);
    if (d < best) {
      best = d;
      out = sites[i];
    }
  }
  const float mx = (after.x - before.x) / kMapScale;
  const float mz = (after.z - before.z) / kMapScale;
  g_follow_living = living;
  g_follow_since = NowSeconds();
  g_follow_step = 0;
  ModLog("miracle watch: the game sends %p (%s) into the teleport site %p at "
         "%.1f %.1f (MagicTeleport::DoTeleport gives %d). It was at %.1f "
         "%.1f and is at %.1f %.1f now, %.1f units away. The nearest site to "
         "it now is %p, %.1f units away, frame %llu", living,
         HandDirectClass(living), site, in.x / kMapScale, in.z / kMapScale,
         result, before.x / kMapScale, before.z / kMapScale,
         after.x / kMapScale, after.z / kMapScale, sqrtf(mx * mx + mz * mz),
         out, best, g_last_frame);
  return result;
}

bool g_installed = false;

}  // namespace

void MiracleWatchInstall() {
  if (g_installed) return;
  g_installed = true;
  HookAt(kSpellEffect, kSpellEffectBytes, sizeof(kSpellEffectBytes),
         reinterpret_cast<void*>(&h_SpellEffect),
         reinterpret_cast<void**>(&o_SpellEffect),
         "Spell::ApplyDefaultSpellEffect (the log of the drops of Water)");
  HookAt(kCheckFaces, kCheckFacesBytes, sizeof(kCheckFacesBytes),
         reinterpret_cast<void*>(&h_CheckFaces),
         reinterpret_cast<void**>(&o_CheckFaces),
         "PhysOb::CheckVFaces (the log of a Physical Shield)");
  HookAt(kDoTeleport, kDoTeleportBytes, sizeof(kDoTeleportBytes),
         reinterpret_cast<void*>(&h_DoTeleport),
         reinterpret_cast<void**>(&o_DoTeleport),
         "MagicTeleport::DoTeleport (the log of a teleport)");
}

void MiracleWatchCircle(const char* name, float x, float z) {
  g_cast_name = name != nullptr ? name : "";
  g_cast_at[0] = x;
  g_cast_at[1] = z;
  g_cast_time = NowSeconds();
}

void MiracleWatchCreature(void* creature, const char* name) {
  g_creature = creature;
  g_creature_cast = name != nullptr ? name : "";
  g_creature_since = NowSeconds();
  g_creature_said = -1e9;
  g_creature_mask = 0xFFFFFFFFu;
  g_creature_size = -1.0f;
}

void MiracleWatchFrame(uint64_t frame, int hand_state) {
  ++g_frames;
  g_last_frame = frame;
  SpellsFrame(frame);
  CreatureFrame(frame, hand_state);
  DropsFrame();
  ShieldsFrame();
  SitesFrame();
  TeleportFollow();
}

void MiracleWatchReport() {
  if (g_spells_seen == 0 && g_watched_count == 0) return;
  ModLog("miracle watch: %llu new shields and storms this window, %d stand "
         "now", g_spells_seen, g_watched_count);
  g_spells_seen = 0;
}
