// The camera of the player that the Creature reads: the pure rules. See
// docs/creature.md, "The Creature turns to the head". creature_head.cpp uses
// them, and test/controller_math checks them (head_checks.h). It has no game
// and no headset in it.
//
// THE GAME. Each Creature that turns to the player, looks at the player,
// points or throws at the player, or measures how far the player is, reads
// the camera of the nearest player: Creature::GetNearestCameraInterfaceStatus
// gives a GInterfaceStatus, and the Creature reads its camera place at +0xB0.
// With the mouse that camera is the eye of the player. In the God view the
// game's own camera is 400 units from the place of the player, 50 degrees
// above the land (docs/god-view.md, section 7), and the eye of the player is
// the head. So in the God view the head goes in the place of that camera,
// for the time of the Creature code only.

#pragma once

#include <math.h>

namespace head_math {

// creature_player_head.
const int kModeGame = 0;  // the Creature reads the game's own camera
const int kModeHead = 1;  // the Creature reads the head of the player while
                          // the player stands for the camera

inline int ClampMode(int mode) { return mode != 0 ? kModeHead : kModeGame; }

// Whether the head goes in the place of the camera now. stands: the player
// stands for the camera now (docs/god-view.md, "When the player stands for
// the camera"). A head that is not a number is no head.
inline bool UseHead(int mode, bool stands, const float head[3]) {
  if (mode != kModeHead || !stands) return false;
  for (int i = 0; i < 3; ++i) {
    if (!(head[i] == head[i]) || fabsf(head[i]) > 1.0e6f) return false;
  }
  return true;
}

// The game's value comes back at the end of the Creature code only while the
// field still holds the head that the mod wrote. A packet of the camera that
// came in between holds the newer value of the game, and it stays.
inline bool PutBack(const float now[3], const float wrote[3]) {
  return now[0] == wrote[0] && now[1] == wrote[1] && now[2] == wrote[2];
}

// THE TURN OF THE LOCKED SELECT. Creature::GoIntoStateOfFinishingAction
// 0x00477B20 takes the line from the base of the body to the camera. When
// the camera is more than 2 units away on x or on z, the Creature turns
// along that line. Else the game gives the angle 0 [V]. The heading is in
// degrees on the map, from +z towards +x, for the log: the game's own angle
// has a different zero, and the angle between two headings is the same.
const float kTurnLeast = 2.0f;

inline bool TurnHeading(const float body[3], const float target[3],
                        float* degrees) {
  const float dx = target[0] - body[0];
  const float dz = target[2] - body[2];
  if (!(dx == dx) || !(dz == dz)) return false;
  if (!(fabsf(dx) > kTurnLeast) && !(fabsf(dz) > kTurnLeast)) return false;
  *degrees = atan2f(dx, dz) * 57.29578f;
  return true;
}

// The angle between two headings, from 0 to 180 degrees.
inline float HeadingGap(float a, float b) {
  float d = fmodf(fabsf(a - b), 360.0f);
  if (d > 180.0f) d = 360.0f - d;
  return d;
}

// The distance on the map, with x and z only, as the game measures.
inline float MapDistance(const float a[3], const float b[3]) {
  const float dx = b[0] - a[0], dz = b[2] - a[2];
  return sqrtf(dx * dx + dz * dz);
}

}  // namespace head_math
