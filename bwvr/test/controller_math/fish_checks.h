// THE FISH OF A FISH FARM (fish_math.h). main.cpp includes this file after
// Check and Near, just before main.
#pragma once

#include "../../src/mod/fish_math.h"

namespace fish_checks {

inline void FishChecks() {
  using namespace fish_math;
  printf("\nThe fish of a fish farm\n");

  // The count of the game: ftol(fill * 15).
  Check(DrawnFish(1.0f) == 15, "a full farm draws 15 fish", DrawnFish(1.0f),
        15);
  Check(DrawnFish(0.5f) == 7, "a half farm draws 7 fish", DrawnFish(0.5f), 7);
  Check(DrawnFish(0.06f) == 0, "a farm with less than 1/15 draws no fish",
        DrawnFish(0.06f), 0);
  Check(DrawnFish(0.0f) == 0, "an empty farm draws no fish", DrawnFish(0.0f),
        0);
  Check(DrawnFish(2.0f) == 15, "a fill over 1 draws 15 fish",
        DrawnFish(2.0f), 15);
  const float nan = sqrtf(-1.0f);
  Check(DrawnFish(nan) == 0, "a fill that is not a number draws no fish",
        DrawnFish(nan), 0);

  // A farm in the sea at 1800, 2465. The game puts the home of its school
  // on the first ring where two rings are sea: 4 units east. The fish swim
  // within 7 units of the home in x and z.
  const float farm[3] = {1800.0f, 0.0f, 2465.0f};
  const float home[3] = {1804.0f, -0.5f, 2465.0f};
  float fish[5][3] = {{1811.0f, -0.6f, 2465.0f},
                      {1797.5f, -0.2f, 2470.0f},
                      {1804.0f, -0.9f, 2458.0f},
                      {1806.0f, -0.4f, 2471.0f},
                      {1803.0f, -0.5f, 2463.0f}};
  School school;
  const bool made = SchoolBody(fish, 5, &school);
  Check(made, "five fish make a body", made ? 1 : 0, 1);
  Near("its point is the middle of the fish in x", school.point[0], 1804.3,
       1e-3);
  Near("in y", school.point[1], -0.52, 1e-3);
  Near("and in z", school.point[2], 2465.4, 1e-3);
  {
    float most = 0.0f;
    for (int i = 0; i < 5; ++i) {
      const float dx = fish[i][0] - school.point[0];
      const float dz = fish[i][2] - school.point[2];
      const float d = sqrtf(dx * dx + dz * dz);
      if (d > most) most = d;
    }
    Near("its radius is 2 units past the farthest fish", school.radius,
         most + 2.0f, 1e-4);
  }
  {
    const float dx = home[0] - school.point[0];
    const float dz = home[2] - school.point[2];
    const float d = sqrtf(dx * dx + dz * dz);
    Check(d < 1.0f, "the middle of the fish is near the home of the school",
          d, 0.5);
  }

  // Each land point where the game's test finds a fish is in the body.
  {
    int found = 0, outside = 0;
    for (float x = 1780.0f; x <= 1830.0f; x += 0.25f) {
      for (float z = 2440.0f; z <= 2490.0f; z += 0.25f) {
        if (!GameFindsFish(fish, 5, x, z)) continue;
        ++found;
        if (!InSchool(school, x, z)) ++outside;
      }
    }
    Check(found > 100, "the game finds a fish at many land points", found,
          found);
    Check(outside == 0,
          "each land point where the game finds a fish is in the body",
          outside, 0);
  }

  // THE FAULT IN NUMBERS: the body of 5 units at the point of the farm. The
  // Hand at the fish 11 units east of the farm is outside it, and the game
  // finds that fish.
  {
    const float dx = fish[0][0] - farm[0];
    const float dz = fish[0][2] - farm[2];
    const float from_farm = sqrtf(dx * dx + dz * dz);
    Check(from_farm > 5.0f, "a fish swims outside 5 units of the farm",
          from_farm, 11.0);
    Check(GameFindsFish(fish, 5, fish[0][0], fish[0][2]),
          "the game finds the fish at its own place", 1, 1);
    Check(InSchool(school, fish[0][0], fish[0][2]),
          "and the place is in the body of the school", 1, 1);
  }
  Check(!GameFindsFish(fish, 5, farm[0], farm[2] - 8.0f),
        "the game finds no fish 8 units north of the farm", 0, 0);

  // One fish: the body is the reach of the game around it.
  {
    const float one[1][3] = {{10.0f, 0.0f, 20.0f}};
    School s;
    const bool ok = SchoolBody(one, 1, &s);
    Check(ok && s.radius == kFishReach, "one fish gives a body of 2 units",
          ok ? s.radius : -1.0f, 2.0);
  }

  // Bad memory gives no body.
  {
    School s;
    Check(!SchoolBody(fish, 0, &s), "no fish give no body", 0, 0);
    float bad[2][3] = {{1.0f, 0.0f, 1.0f}, {nan, 0.0f, 1.0f}};
    Check(!SchoolBody(bad, 2, &s), "a fish that is not a number gives no body",
          0, 0);
    float apart[2][3] = {{0.0f, 0.0f, 0.0f}, {500.0f, 0.0f, 0.0f}};
    Check(!SchoolBody(apart, 2, &s), "fish 250 units from their middle give no body",
          0, 0);
  }
}

}  // namespace fish_checks
