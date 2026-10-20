// The math of the start of a story at the head (docs/god-view.md, "A story
// starts at the head"). It has no game and no headset in it. story_start.cpp
// uses it, and test/controller_math checks it.
#pragma once

#include <math.h>

namespace story_start_math {

// The focus is this far ahead of the head: one meter of the room at the
// scale of the view, and never less than kLeastFocus units.
const float kFocusMeters = 1.0f;
const float kLeastFocus = 10.0f;

// THE RULE. The mod puts the game camera at the head when a script takes
// the camera from the player: before the call the top camera mode was a mode
// of the player, and after it the top mode is a story mode. The God view
// owned the last frame, so the head of that frame is the place of the
// player. A script that takes the camera again in its story changes
// nothing.
inline bool Wanted(bool on, bool god_owned, bool player_mode_before,
                   bool story_mode_after, bool have_head) {
  return on && god_owned && player_mode_before && story_mode_after &&
         have_head;
}

// THE CAMERA AT THE HEAD. `head` is the render head in the world, and
// `basis` has the right, up and forward axes of the head as its columns
// (row major, as HeadCameraRenderBasis gives it). `scale` is the units of the
// world for each meter of the room. The position is the head, and the focus
// is ahead of the head along its forward axis. False for a value that is not
// a number, or a forward axis with no length.
inline bool StartPose(const float head[3], const float basis[9], float scale,
                      float position[3], float focus[3]) {
  float forward[3] = {basis[2], basis[5], basis[8]};
  const float length = sqrtf(forward[0] * forward[0] +
                             forward[1] * forward[1] +
                             forward[2] * forward[2]);
  if (!(length > 0.001f)) return false;
  float ahead = kFocusMeters * scale;
  if (!(ahead >= kLeastFocus)) ahead = kLeastFocus;
  for (int i = 0; i < 3; ++i) {
    if (!(head[i] == head[i])) return false;
    position[i] = head[i];
    focus[i] = head[i] + forward[i] / length * ahead;
  }
  return true;
}

}  // namespace story_start_math
