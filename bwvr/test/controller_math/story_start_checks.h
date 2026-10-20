// THE START OF A STORY AT THE HEAD (docs/god-view.md, "A story starts at the
// head"). main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/story_start_math.h"

namespace story_start_checks {

inline void StoryStartChecks() {
  using namespace story_start_math;
  printf("\nThe start of a story at the head\n");

  // THE RULE.
  Check(Wanted(true, true, true, true, true),
        "a script takes the camera from the player in the God view: the "
        "camera goes to the head", 1, 1);
  Check(!Wanted(false, true, true, true, true),
        "story_start_at_head=0: the camera stays", 0, 0);
  Check(!Wanted(true, false, true, true, true),
        "the God view did not own the last frame: the camera stays", 0, 0);
  Check(!Wanted(true, true, false, true, true),
        "a story mode was on top already: the camera stays", 0, 0);
  Check(!Wanted(true, true, true, false, true),
        "the script did not push a story mode: the camera stays", 0, 0);
  Check(!Wanted(true, true, true, true, false),
        "no head: the camera stays", 0, 0);

  // THE CAMERA AT THE HEAD. A head 60 units over the land at 100 units for
  // each meter, that looks 50 degrees down along +z.
  const float kDegree = 3.14159265f / 180.0f;
  const float down = 50.0f * kDegree;
  const float basis[9] = {1.0f, 0.0f,          0.0f,
                          0.0f, cosf(down),    -sinf(down),
                          0.0f, sinf(down),    cosf(down)};
  const float head[3] = {1860.0f, 100.0f, 2700.0f};
  float position[3], focus[3];
  const bool ok = StartPose(head, basis, 100.0f, position, focus);
  Check(ok, "a head gives a camera", ok ? 1 : 0, 1);
  Near("the camera is at the head, x", position[0], 1860.0, 1e-4);
  Near("the camera is at the head, y", position[1], 100.0, 1e-4);
  Near("the camera is at the head, z", position[2], 2700.0, 1e-4);
  const float dx = focus[0] - position[0];
  const float dy = focus[1] - position[1];
  const float dz = focus[2] - position[2];
  Near("the focus is 1 m of the room ahead at 100 units a meter, units",
       sqrtf(dx * dx + dy * dy + dz * dz), 100.0, 1e-3);
  Near("the camera looks down as the head does, degrees",
       atan2f(-dy, sqrtf(dx * dx + dz * dz)) / kDegree, 50.0, 1e-3);
  Near("the camera has the heading of the head, x", dx, 0.0, 1e-4);

  StartPose(head, basis, 7.5f, position, focus);
  Near("at the life size scale the focus is the least distance ahead",
       sqrtf((focus[0] - head[0]) * (focus[0] - head[0]) +
             (focus[1] - head[1]) * (focus[1] - head[1]) +
             (focus[2] - head[2]) * (focus[2] - head[2])),
       kLeastFocus, 1e-3);

  const float flat[9] = {0.0f};
  Check(!StartPose(head, flat, 100.0f, position, focus),
        "a forward axis with no length gives no camera", 0, 0);
  const float lost[3] = {NAN, 100.0f, 2700.0f};
  Check(!StartPose(lost, basis, 100.0f, position, focus),
        "a head that is not a number gives no camera", 0, 0);
  const bool any = StartPose(head, basis, NAN, position, focus);
  Near("a scale that is not a number takes the least distance",
       any ? sqrtf((focus[0] - head[0]) * (focus[0] - head[0]) +
                   (focus[1] - head[1]) * (focus[1] - head[1]) +
                   (focus[2] - head[2]) * (focus[2] - head[2]))
           : 0.0,
       kLeastFocus, 1e-3);
}

}  // namespace story_start_checks
