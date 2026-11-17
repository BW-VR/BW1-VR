// THE POINT OF THE POINTER RAY (docs/miracles.md, "The point of the pointer
// ray"). main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/point_aim_math.h"

namespace point_aim_checks {

inline void PointAimChecks() {
  using namespace point_aim_math;
  printf("\nThe point of the pointer ray\n");

  // THE TABLE: Explosion and its two levels, and no other miracle.
  Check(AimsByRay(29, 0) && AimsByRay(29, 1) && AimsByRay(29, 2),
        "Explosion aims by the ray at each power level", 1, 1);
  Check(!AimsByRay(29, 3) && !AimsByRay(29, -1) && !AimsByRay(29, 40),
        "a power level that the game does not have does not aim", 0, 0);
  int others = 0;
  for (int type = 0; type < 29; ++type) {
    for (int power = 0; power < 3; ++power) {
      if (AimsByRay(type, power)) ++others;
    }
  }
  Check(others == 0, "no other seed type aims by the ray", others, 0);
  Check(kTableRows == 1, "the table has one row", kTableRows, 1);

  // WHAT A PRESS DOES.
  Check(PressOf(true, true, false, true) == kPressRay,
        "the key on, land at the ray: the cast goes to the ray",
        PressOf(true, true, false, true), kPressRay);
  Check(PressOf(true, true, false, false) == kPressNone,
        "the key on, no land at the ray: the press casts nothing",
        PressOf(true, true, false, false), kPressNone);
  Check(PressOf(false, true, false, true) == kPressGame,
        "the key off: the game casts under the Hand",
        PressOf(false, true, false, true), kPressGame);
  Check(PressOf(false, true, false, false) == kPressGame,
        "the key off and no land at the ray: still the game's cast",
        PressOf(false, true, false, false), kPressGame);
  Check(PressOf(true, false, false, true) == kPressGame,
        "a miracle that is not in the table: the game's cast",
        PressOf(true, false, false, true), kPressGame);
  Check(PressOf(true, true, true, false) == kPressGame,
        "a target in the record: the game's cast on the object",
        PressOf(true, true, true, false), kPressGame);

  // THE MAP COORDINATES of the game.
  Check(ToMap(1000.0f) == 6553600, "1000 units are 6553600 on the map",
        ToMap(1000.0f), 6553600);
  Near("a map coordinate goes back to the world", FromMap(ToMap(1827.25f)),
       1827.25, 0.001);

  // THE DISTANCE ACROSS THE LAND: the height does not count.
  const float hand[3] = {1000.0f, 80.0f, 2000.0f};
  const float land[3] = {1030.0f, 40.0f, 2040.0f};
  Near("50 units across the land at 50 units a meter is 1 m",
       AcrossM(hand, land, 50.0f), 1.0, 1e-4);
  Near("no scale gives 0", AcrossM(hand, land, 0.0f), 0.0, 1e-6);

  // THE MARK: a closed ring at the land point, 6 cm of the room wide at each
  // scale.
  float ring[kMarkPoints][3];
  const int n = MarkRing(land, 44.4f, ring, kMarkPoints);
  Check(n == kMarkPoints, "the ring has its points", n, kMarkPoints);
  Near("the ring is closed: x", ring[n - 1][0], ring[0][0], 1e-4);
  Near("the ring is closed: z", ring[n - 1][2], ring[0][2], 1e-4);
  double most = 0.0, least = 1e9;
  int off_height = 0;
  for (int i = 0; i < n; ++i) {
    const double dx = ring[i][0] - land[0], dz = ring[i][2] - land[2];
    const double d = sqrt(dx * dx + dz * dz);
    if (d > most) most = d;
    if (d < least) least = d;
    if (ring[i][1] != land[1]) ++off_height;
  }
  Check(off_height == 0, "each point of the ring has the height of the land "
        "point", off_height, 0);
  Near("the ring is 6 cm of the room from the point (most)", most,
       0.06 * 44.4, 0.01);
  Near("the ring is 6 cm of the room from the point (least)", least,
       0.06 * 44.4, 0.01);
  float big[kMarkPoints][3];
  MarkRing(land, 1973.0f, big, kMarkPoints);
  Near("at 1973 units a meter the ring is 118 units from the point",
       big[0][0] - land[0], 0.06 * 1973.0, 0.05);
  Check(MarkRing(land, 0.0f, ring, kMarkPoints) == 0,
        "no scale gives no ring", 0, 0);
  Check(MarkRing(land, 44.4f, ring, kMarkPoints - 1) == 0,
        "too little room gives no ring", 0, 0);
}

}  // namespace point_aim_checks
