// THE PHYSICS SWITCH. See physics_switch.h and docs/throw.md, "The physics
// switch".
//
// The preset holds each key of throw.md that changes how a thrown thing
// flies, lands, bounces, turns or comes to rest. The keys of the hand
// (release_at_once, grip_*, catch_at_once, release_fall_cm) and
// miracle_throw are not in it: the direction and the speed of the real hand
// at the release stay. throw_law=1 is the value of the plain game: the
// square root law with the game's physics lands a throw where the real
// throw lands, in the game's own time. throw_law=0 sends the speed of the
// hand in the world times throw_gain_pct, which is at the game's cap of 124
// units a second for most throws in the God view.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "miracle_throw.h"
#include "physics_switch.h"
#include "throw_physics.h"

extern void ModLog(const char* format, ...);

// controllers.cpp
void ControllersGesturePulse(float amplitude, float seconds);

namespace {

struct PresetKey {
  const char* key;
  int vr;    // the value of the mod (physics=1)
  int game;  // the value of the plain game (physics=0)
  void (*apply)(int value);
  bool from_ini;
  int ini_value;
};

PresetKey g_keys[] = {
    {"throw_law", 2, 1, [](int v) { ThrowPhysicsSetLaw(v); }, false, 0},
    {"throw_land", 1, 0, [](int v) { ThrowPhysicsSetLand(v); }, false, 0},
    {"throw_land_rest", 1, 0, [](int v) { ThrowPhysicsSetRest(v); }, false,
     0},
    {"throw_turn", 1, 0, [](int v) { ThrowPhysicsSetTurn(v != 0); }, false,
     0},
    {"throw_trees", 1, 0, [](int v) { ThrowPhysicsSetTrees(v != 0); }, false,
     0},
    {"throw_tree_settle", 1, 0, [](int v) { ThrowPhysicsSetSettle(v != 0); },
     false, 0},
    {"throw_shield", 1, 0, [](int v) { ThrowPhysicsSetShield(v != 0); },
     false, 0},
    {"throw_arc_objects", 1, 0,
     [](int v) { ThrowPhysicsSetArcObjects(v != 0); }, false, 0},
    {"miracle_flight", 1, 0, [](int v) { MiracleThrowSetFlight(v); }, false,
     0},
};
const int kKeyCount = sizeof(g_keys) / sizeof(g_keys[0]);

bool g_read = false;
bool g_vr = true;

PresetKey* Find(const char* key) {
  for (int i = 0; i < kKeyCount; ++i) {
    if (strcmp(g_keys[i].key, key) == 0) return &g_keys[i];
  }
  return nullptr;
}

int ValueOf(const PresetKey& k) {
  if (k.from_ini) return k.ini_value;
  return g_vr ? k.vr : k.game;
}

// One line: physics and the value of each key of the preset, with "ini"
// after a key that bwvr.ini sets.
void LogValues(const char* from) {
  char list[512] = "";
  size_t used = 0;
  for (int i = 0; i < kKeyCount && used + 48 < sizeof(list); ++i) {
    const int wrote =
        sprintf_s(list + used, sizeof(list) - used, "%s%s=%d%s",
                  i == 0 ? "" : ", ", g_keys[i].key, ValueOf(g_keys[i]),
                  g_keys[i].from_ini ? " (bwvr.ini)" : "");
    if (wrote > 0) used += static_cast<size_t>(wrote);
  }
  ModLog("physics      : %s: physics=%d (%s). The keys of the preset: %s",
         from, g_vr ? 1 : 0,
         g_vr ? "VR: the throw physics of the mod"
              : "Original: the physics of the plain game for a thrown object "
                "and a thrown Fireball",
         list);
}

}  // namespace

void PhysicsSwitchRead(const char* ini_path) {
  g_read = true;
  g_vr = GetPrivateProfileIntA("bwvr", "physics", 1, ini_path) != 0;
  for (int i = 0; i < kKeyCount; ++i) {
    char text[32] = "";
    GetPrivateProfileStringA("bwvr", g_keys[i].key, "", text, sizeof(text),
                             ini_path);
    g_keys[i].from_ini = text[0] != 0;
    g_keys[i].ini_value = g_keys[i].from_ini ? atoi(text) : 0;
  }
}

int PhysicsSwitchDefault(const char* key, int fallback) {
  const PresetKey* k = Find(key);
  if (k == nullptr) return fallback;
  return g_vr ? k->vr : k->game;
}

bool PhysicsSwitchSets(const char* key) {
  const PresetKey* k = Find(key);
  return k != nullptr && !k->from_ini;
}

void PhysicsSwitchLog() { LogValues("the start"); }

int PhysicsSwitchValue() { return g_vr ? 1 : 0; }

void PhysicsSwitchChange(const char* by) {
  if (!g_read) return;
  g_vr = !g_vr;
  for (int i = 0; i < kKeyCount; ++i) {
    if (!g_keys[i].from_ini) g_keys[i].apply(ValueOf(g_keys[i]));
  }
  ControllersGesturePulse(0.5f, g_vr ? 0.30f : 0.06f);
  LogValues(by);
}
