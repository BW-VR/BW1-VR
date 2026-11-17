// The math of a miracle that the pointer ray aims, as pure functions with no
// game state.
//
// miracle.cpp and overlay.cpp call these with the land point of the ray of
// the pointing hand, and test/controller_math calls them with numbers it
// makes up. Nothing here reads a global or touches the game. See
// docs/miracles.md, "The point of the pointer ray".
//
// THE TABLE. A miracle of the table casts at the land point of the ray and
// not at the land under the Hand (miracle_point_aim=1). Each row is a seed
// type (GSpellSeedInfo, SPELL_SEED_TYPE) and the power levels (POWER_UP_TYPE
// 0, 1 and 2) that aim by the ray. Only Explosion (the Mega-Blast of the
// game) is in it now. Another miracle that casts at a place can go in later.
//
// THE CAST. The game casts a miracle of the press at the land point of its
// pick record (GInterface+0x3F0) when the influence at that point
// (GInterface+0x48) is set. The mod puts the land point of the ray there for
// the press only. With no land at the end of the ray it clears the influence
// for the press, so the game casts nothing, and it does not cast under the
// Hand.
//
// THE MARK. A ring on the land around the land point of the ray, with a
// radius in meters of the room. So it has the same size to the eye at each
// scale of the God view.

#pragma once

#include <math.h>
#include <stdint.h>

#include "circle_aim_math.h"

namespace point_aim_math {

// SPELL_SEED_TYPE of Explosion (MAGIC_TYPE 7 to 9, Mega-Blast).
const int kSeedExplosion = 29;

// A row of the table: the seed type, and a bit for each power level that
// aims by the ray (bit 0 for level 0).
struct Row {
  int seed_type;
  uint32_t levels;
};

const Row kTable[] = {
    {kSeedExplosion, 0x7u},  // Explosion, Increase and Extreme
};
const int kTableRows = static_cast<int>(sizeof(kTable) / sizeof(kTable[0]));

// True when this seed type at this power level aims by the ray.
inline bool AimsByRay(int seed_type, int power) {
  if (power < 0 || power > 31) return false;
  for (int i = 0; i < kTableRows; ++i) {
    if (kTable[i].seed_type == seed_type &&
        (kTable[i].levels & (1u << power)) != 0) {
      return true;
    }
  }
  return false;
}

// WHAT A PRESS DOES with a miracle of the table.
enum Press {
  kPressGame,  // the game's own cast: the key is off, or a target is in the
               // record, so nothing changes
  kPressRay,   // the land point of the ray goes to the record for the press
  kPressNone,  // the ray meets no land: the game gets no influence, so the
               // press casts nothing
};

inline int PressOf(bool point_aim, bool aims, bool have_target,
                   bool have_land) {
  if (!point_aim || !aims || have_target) return kPressGame;
  return have_land ? kPressRay : kPressNone;
}

// The map coordinate of the game for a world coordinate (MapCoords: the
// world units times 6553.6, as an integer).
const float kMapScale = 6553.6f;
inline int32_t ToMap(float world) {
  return static_cast<int32_t>(world * kMapScale);
}
inline float FromMap(int32_t map) {
  return static_cast<float>(map) / kMapScale;
}

// The distance across the land from the Hand's point to the land point of
// the ray, in meters of the room. 0 with no scale.
inline float AcrossM(const float hand[3], const float land[3],
                     float units_per_meter) {
  if (!(units_per_meter > 0.0f)) return 0.0f;
  return circle_aim_math::FlatDistance(hand, land) / units_per_meter;
}

// THE MARK: the radius of the ring in meters of the room, and its points.
const float kMarkRadiusM = 0.06f;
const int kMarkPoints = 25;  // 24 steps, and the first point again

// The points of the ring around the land point, flat at its height. The
// last point is the first one again, so the line is closed. Returns the
// count, 0 with no scale or no room.
inline int MarkRing(const float land[3], float units_per_meter,
                    float out[][3], int most) {
  if (!(units_per_meter > 0.0f) || most < kMarkPoints) return 0;
  const float r = kMarkRadiusM * units_per_meter;
  const int steps = kMarkPoints - 1;
  for (int i = 0; i < kMarkPoints; ++i) {
    const int k = i == steps ? 0 : i;
    const float a = 6.2831853f * static_cast<float>(k) /
                    static_cast<float>(steps);
    out[i][0] = land[0] + r * cosf(a);
    out[i][1] = land[1];
    out[i][2] = land[2] + r * sinf(a);
  }
  return kMarkPoints;
}

}  // namespace point_aim_math
