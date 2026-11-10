// The model cull distance (cull_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/cull_math.h"

namespace cull_checks {

inline void CullChecks() {
  using namespace cull_math;
  printf("\nThe model cull distance\n");

  // The values of the key, and the value 0 is the game's own.
  Near("cull_distance=0 is the game's own distance", CullFactor(0), 1, 0);
  Near("cull_distance=1 is 2 times", CullFactor(1), 2, 0);
  Near("cull_distance=2 is 3 times", CullFactor(2), 3, 0);
  Near("cull_distance=3 is 5 times", CullFactor(3), 5, 0);
  Near("cull_distance=4 is 10 times", CullFactor(4), 10, 0);
  Check(CullIndex(-1) == kCullDefault && CullIndex(kCullCount) == kCullDefault,
        "a value out of the range gets the default", 0, 0);
  for (int i = 1; i < kCullCount; ++i) {
    Check(CullFactor(i) > CullFactor(i - 1), "each value is further out", i,
          0);
  }
  // 10 times the game's 700 units reaches past the far corner of the map
  // from its middle (2560 * sqrt 2), and stays under the cap of 8000 of the
  // table.
  Check(700.0f * CullFactor(4) > 3620.0f && 700.0f * CullFactor(4) <= 8000.0f,
        "10 times reaches the corners and stays under the cap", 0, 0);

  // The debug switch steps through the values and back to the game's own.
  int step = 0;
  for (int i = 0; i < kCullCount; ++i) step = CullNext(step);
  Check(step == 0, "the switch comes back to the game's own value", 0, 0);
  Check(CullNext(0) == 1 && CullNext(kCullCount - 1) == 0,
        "the switch steps up, and after the last value to the first", 0, 0);

  // The gain of the map object distance in the God view. With the game's own
  // cull distance it is the gain of the view, as the view gives it alone.
  Near("the game's distance, the view does not own the frame",
       VanishGain(false, 4.0f, 1.0f), 1, 0);
  Near("the game's distance below the life size scale",
       VanishGain(true, 0.13f, 1.0f), 1, 0);
  Near("the game's distance zoomed out: the gain of the view",
       VanishGain(true, 4.0f, 1.0f), 4, 0);
  Near("5 times at the villager scale", VanishGain(true, 0.13f, 5.0f), 5, 0);
  Near("5 times in a frame of a story", VanishGain(false, 4.0f, 5.0f), 5, 0);
  Near("5 times zoomed out: on top of the gain of the view",
       VanishGain(true, 4.0f, 5.0f), 20, 1e-5);
  Near("a factor under 1 counts as 1", VanishGain(true, 0.5f, 0.5f), 1, 0);

  // The rule of the distances of tabletop.cpp.
  Near("a gain of 1 keeps the game's value", ScaledValue(700.0f, 1.0f, 8000.0f),
       700, 0);
  Near("a gain of 5 gives 3500", ScaledValue(700.0f, 5.0f, 8000.0f), 3500,
       1e-3);
  Near("the cap holds the value", ScaledValue(700.0f, 20.0f, 8000.0f), 8000,
       0);
  Near("never less than the game's value",
       ScaledValue(9000.0f, 2.0f, 8000.0f), 9000, 0);

  // The fog moves out only as far as the cull factor moves the distance out.
  Near("the villager scale, 5 times: the fog 5 times",
       FogRatio(700.0f, VanishGain(true, 0.13f, 1.0f),
                VanishGain(true, 0.13f, 5.0f), 8000.0f),
       5, 1e-5);
  Near("the game's cull distance keeps the fog of the game",
       FogRatio(700.0f, VanishGain(true, 0.13f, 1.0f),
                VanishGain(true, 0.13f, 1.0f), 8000.0f),
       1, 0);
  Near("a gain of 2, 5 times: the fog 5 times",
       FogRatio(700.0f, VanishGain(true, 2.0f, 1.0f),
                VanishGain(true, 2.0f, 5.0f), 8000.0f),
       5, 1e-5);
  Near("a gain of 2.5, 5 times: the cap gives 8000 over 1750",
       FogRatio(700.0f, VanishGain(true, 2.5f, 1.0f),
                VanishGain(true, 2.5f, 5.0f), 8000.0f),
       8000.0 / 1750.0, 1e-4);
  // 100 units per meter is a gain of 13.3: the cap holds the distance with
  // and without the factor, so the fog does not change.
  Near("100 units per meter, 5 times: the fog of the game",
       FogRatio(700.0f, VanishGain(true, 13.33f, 1.0f),
                VanishGain(true, 13.33f, 5.0f), 8000.0f),
       1, 0);
  Near("the table: 8000 with and without the factor, the fog of the game",
       FogRatio(700.0f, 1.0e6f, 1.0e6f, 8000.0f), 1, 0);
  Near("the table off: the fog 5 times",
       FogRatio(700.0f, 1.0f, 5.0f, 8000.0f), 5, 1e-5);
  Near("no game value: the fog of the game", FogRatio(0.0f, 1.0f, 5.0f,
       8000.0f), 1, 0);

  // The fog of clear weather and of rain, times the factor.
  {
    float start = 0.0f, end = 0.0f;
    ScaleFog(400.0f, 900.0f, 5.0f, &start, &end);
    Near("clear weather, 5 times: the fog starts at 2000 units", start, 2000,
         1e-3);
    Near("clear weather, 5 times: the fog ends at 4500 units", end, 4500,
         1e-3);
    ScaleFog(400.0f, 900.0f, 1.0f, &start, &end);
    Check(start == 400.0f && end == 900.0f,
          "the factor 1 gives the game's fog exactly", 0, 0);
    ScaleFog(15.0f, 350.0f, 0.0f, &start, &end);
    Check(start == 15.0f && end == 350.0f,
          "a factor of 0 gives the game's fog of rain", 0, 0);
  }

  // The far edge of the objects is at the same share of the fog as in the
  // game, for each factor and each weather.
  {
    const float weathers[3][2] = {{400.0f, 900.0f}, {15.0f, 350.0f},
                                  {200.0f, 700.0f}};
    for (int w = 0; w < 3; ++w) {
      const float game = FogShare(700.0f, weathers[w][0], weathers[w][1]);
      for (int i = 0; i < kCullCount; ++i) {
        const float f = CullFactor(i);
        float start = 0.0f, end = 0.0f;
        ScaleFog(weathers[w][0], weathers[w][1], f, &start, &end);
        Near("the far edge of the objects has the fog share of the game",
             FogShare(700.0f * f, start, end), game, 1e-5);
      }
    }
    Near("in clear weather the far edge is 60 percent in the fog",
         FogShare(700.0f, 400.0f, 900.0f), 0.6, 1e-5);
    Near("in rain the far edge is all in the fog",
         FogShare(700.0f, 15.0f, 350.0f), 1, 0);
  }

  // The fog share of the game.
  Near("no fog before the start", FogShare(100.0f, 400.0f, 900.0f), 0, 0);
  Near("all fog after the end", FogShare(1000.0f, 400.0f, 900.0f), 1, 0);
  Near("half the fog at the middle", FogShare(650.0f, 400.0f, 900.0f), 0.5,
       1e-6);
  Near("a fog with no range is a step", FogShare(500.0f, 500.0f, 500.0f), 1,
       0);
}

}  // namespace cull_checks
