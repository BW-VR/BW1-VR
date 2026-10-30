// THE FISH OF A FISH FARM (docs/grab.md, "The fish in the sea"). Pure math:
// no Windows and no game. The checks are in
// test/controller_math/fish_checks.h.
//
// The game does not draw a FishFarm at its point. It draws the fish of the
// farm as a school of sprites near it. The creation of the farm
// (FishFarm::CallVirtualFunctionsForCreation 0x0052CC10) looks on rings of 2,
// 4, 6 ... units around the point of the farm, in 32 directions, for the
// first direction where two rings one after the other are sea. The home of
// the school is there, and the school has 15 fish. Each fish swims to a point
// within the spread of the school (7 units) of the home, in x and in z
// (fn_00824DA0). The game draws ftol(fill * 15) fish: fill is the food of the
// farm over its most food (FishFarm::Process 0x0052D130).
//
// With the mouse, a press over the sea finds the farm when a drawn fish is
// within 2 units of the land point under the Hand, in x and z (fn_00824B10,
// from the search at the palm fn_005D39E0 at 0x005D3A79). So the fish, not
// the point of the farm, are where the player takes fish.
//
// THE BODY OF A SCHOOL. Its point is the middle of the drawn fish. Its
// radius goes 2 units past the drawn fish that is farthest from the middle.
// So each land point where the game's own test finds a fish is in the body.
#pragma once

#include <math.h>

namespace fish_math {

const int kSchoolFish = 15;     // the fish of a school (0x0052CE37)
const float kFishReach = 2.0f;  // fn_00824B10: 4, the square of the reach
// A fish farther than this from the middle of its school is not a fish that
// the game swims (a check of the memory that the mod reads).
const float kMostSpan = 64.0f;

inline bool Finite(float v) { return v == v && v < 1e30f && v > -1e30f; }

// The fish that the game draws for a fill, as the game counts them:
// ftol(fill * 15), at most 15.
inline int DrawnFish(float fill) {
  if (!(fill > 0.0f)) return 0;
  if (fill >= 1.0f) return kSchoolFish;
  const int count = static_cast<int>(fill * static_cast<float>(kSchoolFish));
  return count < kSchoolFish ? count : kSchoolFish;
}

struct School {
  float point[3];  // the middle of the drawn fish
  float radius;    // in x and z, from the point
};

// The body of a school from the places of its drawn fish. False with no fish,
// with a number that is not finite, or with a fish too far from the middle.
inline bool SchoolBody(const float (*fish)[3], int count, School* out) {
  if (fish == nullptr || count <= 0) return false;
  double sum[3] = {0.0, 0.0, 0.0};
  for (int i = 0; i < count; ++i) {
    for (int k = 0; k < 3; ++k) {
      if (!Finite(fish[i][k])) return false;
      sum[k] += fish[i][k];
    }
  }
  for (int k = 0; k < 3; ++k) {
    out->point[k] = static_cast<float>(sum[k] / count);
  }
  float most = 0.0f;
  for (int i = 0; i < count; ++i) {
    const float dx = fish[i][0] - out->point[0];
    const float dz = fish[i][2] - out->point[2];
    const float d = sqrtf(dx * dx + dz * dz);
    if (d > most) most = d;
  }
  if (most > kMostSpan) return false;
  out->radius = most + kFishReach;
  return true;
}

// The game's test of a press over the sea: a drawn fish within the reach of
// the land point (x, z).
inline bool GameFindsFish(const float (*fish)[3], int count, float x,
                          float z) {
  for (int i = 0; i < count; ++i) {
    const float dx = fish[i][0] - x;
    const float dz = fish[i][2] - z;
    if (dx * dx + dz * dz < kFishReach * kFishReach) return true;
  }
  return false;
}

// Whether the land point (x, z) is in the body of a school, in x and z.
inline bool InSchool(const School& school, float x, float z) {
  const float dx = x - school.point[0];
  const float dz = z - school.point[2];
  return dx * dx + dz * dz <= school.radius * school.radius;
}

}  // namespace fish_math
