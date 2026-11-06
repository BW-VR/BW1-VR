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
//   0x58) has the size that the draw uses at +0x90. Only Creature::SetHeight
//   (vtable 0x144, from the scripts) writes it. The start, step and end of
//   Shrink and Enlarge (0x004F5520 to 0x004F5672) move only the base size,
//   which the walk reads (LH3DCreature::GetNavRadius 0x00480A60).
// - Invisibility (the entry 7, 0x004F58B0 and 0x004F5960) sets Creature+
//   0x106C to 1 and starts the fizz (Creature::SetFizz 0x0047AB90): its
//   target at +0x12B4 goes to 0.75, and +0x12B0 follows it. Creature::
//   ProcessState (0x004737F9) gives +0x12B0 and the fizz material to the
//   draw of the body ([LH3DCreature+0x482C]+0xA0 and +0x9C) while the global
//   0x00D00DE4 is set.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "miracle.h"

extern void ModLog(const char* format, ...);

// hand_direct.cpp: game units in a meter of the room.
float HandDirectRoomUnits();

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

const int kMaxLogs = 160;
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

const int kLines = 7;            // status lines of one spell
const double kLineSeconds = 2.0;  // between two status lines

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
    if (w->lines < kLines && t - w->said >= kLineSeconds) {
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
         "switch 0x00D00DE4 is %d). %s%s, frame %llu", g_creature,
         t - g_creature_since, g_creature_cast, size,
         have_size ? "" : " (fn_004F5E60 has other bytes, so no size)",
         Read<float>(link, kLinkBaseSize), drawn, Cm(over), hand_state,
         Read<int32_t>(g_creature, kCreatureInvisible),
         Read<float>(g_creature, kCreatureFizz),
         Read<float>(g_creature, kCreatureFizz + 4), draw_fizz, material,
         fizz_global, mask == 0 ? "No spell is on it" : "The spells on it: ",
         list, frame);
}

uint64_t g_frames = 0;

}  // namespace

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
  SpellsFrame(frame);
  CreatureFrame(frame, hand_state);
}

void MiracleWatchReport() {
  if (g_spells_seen == 0 && g_watched_count == 0) return;
  ModLog("miracle watch: %llu new shields and storms this window, %d stand "
         "now", g_spells_seen, g_watched_count);
  g_spells_seen = 0;
}
