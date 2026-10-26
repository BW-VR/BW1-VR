// THE CAMERA OF THE PLAYER THAT THE CREATURE READS (docs/creature.md, "The
// Creature turns to the head").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/creature_head_math.h"

namespace head_checks {

inline void HeadChecks() {
  using namespace head_math;
  printf("\nThe camera of the player that the Creature reads\n");

  // THE KEY.
  Check(ClampMode(1) == kModeHead && ClampMode(0) == kModeGame &&
            ClampMode(5) == kModeHead,
        "creature_player_head: 0 is the game's camera, and each other value "
        "is the head", ClampMode(5), kModeHead);

  // WHEN THE HEAD GOES IN.
  const float head[3] = {2000.0f, 100.0f, 2500.0f};
  Check(UseHead(kModeHead, true, head),
        "the head goes in while the player stands for the camera", 1, 1);
  Check(!UseHead(kModeHead, false, head),
        "not in a story, or while the game moves its own camera", 0, 0);
  Check(!UseHead(kModeGame, true, head),
        "not with creature_player_head=0", 0, 0);
  {
    const float nan = sqrtf(-1.0f);
    const float bad[3] = {2000.0f, nan, 2500.0f};
    const float far_away[3] = {2000.0f, 100.0f, 3.0e7f};
    Check(!UseHead(kModeHead, true, bad) && !UseHead(kModeHead, true, far_away),
          "a head that is not a number, or out of the world, is no head", 0,
          0);
  }

  // THE GAME'S VALUE COMES BACK only when the field still holds the head.
  {
    const float wrote[3] = {2000.0f, 100.0f, 2500.0f};
    const float same[3] = {2000.0f, 100.0f, 2500.0f};
    const float packet[3] = {2000.0f, 346.4f, 2242.9f};
    Check(PutBack(same, wrote),
          "the game's value comes back at the end of the Creature code", 1,
          1);
    Check(!PutBack(packet, wrote),
          "a packet of the camera that came in between keeps its newer value",
          0, 0);
  }

  // THE TURN. The God view puts the game's camera 400 units from the land
  // under the head, 50 degrees above, back along the heading of the room:
  // 257.1 units back on the map and 306.4 units up. The room faces +z.
  const float land = 40.0f;
  const float back = 400.0f * cosf(50.0f * 3.14159265f / 180.0f);
  const float up = 400.0f * sinf(50.0f * 3.14159265f / 180.0f);
  const float game[3] = {head[0], land + up, head[2] - back};
  {
    // The Creature is 100 units ahead of the player.
    const float body[3] = {2000.0f, land, 2600.0f};
    float to_head = 0.0f, to_game = 0.0f;
    Check(TurnHeading(body, head, &to_head) &&
              TurnHeading(body, game, &to_game),
          "a Creature 100 units ahead turns to the head and to the game's "
          "camera", 1, 1);
    Near("the two turns are the same: the camera is behind the head",
         HeadingGap(to_head, to_game), 0.0, 0.01);
    Near("the head is 100 units from the body on the map",
         MapDistance(body, head), 100.0, 0.01);
    Near("the game's camera is 357 units from the body on the map",
         MapDistance(body, game), 357.1, 0.2);
    Near("the head is 60 units above the base of the body", head[1] - body[1],
         60.0, 0.01);
    Near("the game's camera is 306 units above it: the look goes up into "
         "the sky", game[1] - body[1], 306.4, 0.2);
  }
  {
    // The Creature is 100 units to the right of the player.
    const float body[3] = {2100.0f, land, 2500.0f};
    float to_head = 0.0f, to_game = 0.0f;
    TurnHeading(body, head, &to_head);
    TurnHeading(body, game, &to_game);
    Near("a Creature 100 units to the side turns to the head along -x",
         to_head, -90.0, 0.01);
    Near("and the turn to the game's camera is 68.7 degrees from it",
         HeadingGap(to_head, to_game), 68.74, 0.2);
  }
  {
    // The head is over the Creature: less than 2 units on x and on z.
    const float body[3] = {2001.0f, land, 2501.5f};
    float degrees = 123.0f;
    Check(!TurnHeading(body, head, &degrees) && degrees == 123.0f,
          "a point less than 2 units away on x and z gives no heading: the "
          "game gives the angle 0", degrees, 123.0);
    const float side[3] = {2002.5f, land, 2500.0f};
    Check(TurnHeading(side, head, &degrees),
          "2.5 units on x is a turn", 1, 1);
  }

  // THE ANGLE BETWEEN TWO HEADINGS.
  Near("170 and -170 degrees are 20 degrees apart", HeadingGap(170.0f, -170.0f),
       20.0, 0.01);
  Near("10 and 350 degrees are 20 degrees apart", HeadingGap(10.0f, 350.0f),
       20.0, 0.01);
  Near("0 and 180 degrees are 180 degrees apart", HeadingGap(0.0f, 180.0f),
       180.0, 0.01);
}

}  // namespace head_checks
