// THE AIM OF A NEW BOOKMARK (docs/controllers.md, "The sheet of the places"):
// the fit of an object to the aim ray, the hold on the target of the last
// frame, and what keeps a target that the draw did not offer. main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include <math.h>

#include "../../src/mod/bookmark_aim_math.h"

namespace bookmark_aim_checks {

inline void BookmarkAimChecks() {
  using namespace bookmark_aim_math;
  printf("\nThe aim of a new bookmark\n");

  // A ray along +z from the origin, 10 world units in a meter of the room.
  const float origin[3] = {0.0f, 0.0f, 0.0f};
  const float ahead[3] = {0.0f, 0.0f, 1.0f};
  const float units = 10.0f;
  const float radius = 1.0f;

  // A NEW OBJECT on the ray fits. 20 m of the room away, the new cone is
  // 0.9 + 200 tan(1.5 degrees) = 6.14 units across.
  {
    const float on[3] = {0.0f, 0.0f, 200.0f};
    const Fit fit = ObjectFit(origin, ahead, on, radius, -1.0f, units, false);
    Check(fit.ok && fit.across < 1e-4f, "an object on the ray fits", fit.ok,
          1);
    Near("the new cone 20 m away is 6.14 units across", fit.allowed,
         0.9 + 200.0 * tan(1.5 * 3.14159265 / 180.0), 1e-3);
  }

  // A WALKING VILLAGER. The ray found him 20 m away. He walks 8 units to
  // the side: out of the new cone, but in the hold (1.6 + 200 tan(4
  // degrees) = 15.6 units). So the target of the last frame keeps the aim.
  {
    const float walked[3] = {8.0f, 0.0f, 200.0f};
    const Fit as_new =
        ObjectFit(origin, ahead, walked, radius, -1.0f, units, false);
    const Fit as_last =
        ObjectFit(origin, ahead, walked, radius, -1.0f, units, true);
    Check(!as_new.ok, "8 units to the side is out of the cone of a new object",
          as_new.ok, 0);
    Check(as_last.ok, "8 units to the side is in the hold of the last target",
          as_last.ok, 1);
    const float gone[3] = {17.0f, 0.0f, 200.0f};
    Check(!ObjectFit(origin, ahead, gone, radius, -1.0f, units, true).ok,
          "17 units to the side is out of the hold too", 0, 0);
  }

  // THE LAST TARGET WINS over a new object that is nearer to the ray.
  {
    const float kept[3] = {6.0f, 0.0f, 200.0f};
    const float fresh[3] = {0.0f, 0.0f, 210.0f};
    const Fit last = ObjectFit(origin, ahead, kept, radius, -1.0f, units, true);
    const Fit now = ObjectFit(origin, ahead, fresh, radius, -1.0f, units, false);
    Check(last.ok && now.ok && last.score < now.score,
          "the last target has the better score", last.score, now.score);
  }

  // THE LAND: a new object 1.5 radii past the land of the ray does not fit.
  // The last target may be up to 2 radii past it.
  {
    const float past[3] = {0.0f, 0.0f, 101.5f};
    Check(!ObjectFit(origin, ahead, past, radius, 100.0f, units, false).ok,
          "a new object 1.5 radii past the land does not fit", 0, 0);
    Check(ObjectFit(origin, ahead, past, radius, 100.0f, units, true).ok,
          "the last target 1.5 radii past the land fits", 1, 1);
  }

  // TOO NEAR, BEHIND, OR NO SIZE: no fit.
  {
    const float near_one[3] = {0.0f, 0.0f, 2.0f};
    const float behind[3] = {0.0f, 0.0f, -50.0f};
    const float on[3] = {0.0f, 0.0f, 50.0f};
    Check(!ObjectFit(origin, ahead, near_one, radius, -1.0f, units, true).ok,
          "an object 0.2 m of the room from the origin does not fit", 0, 0);
    Check(!ObjectFit(origin, ahead, behind, radius, -1.0f, units, true).ok,
          "an object behind the origin does not fit", 0, 0);
    Check(!ObjectFit(origin, ahead, on, 0.0f, -1.0f, units, true).ok,
          "an object with no radius does not fit", 0, 0);
    Check(!ObjectFit(origin, ahead, on, radius, -1.0f, 0.0f, true).ok,
          "no fit with no scale of the room", 0, 0);
  }

  // THE KEEP of a last target that the draw did not offer. With the hold
  // that reads the place, the place decides, with no limit of frames. With
  // no place, or with bookmark_aim_hold=0, kKeepFrames frames.
  {
    Check(KeepLast(true, true, true, 500) == kKeepFit,
          "a read place in the hold keeps the target after 500 frames", 1, 1);
    Check(KeepLast(true, true, false, 0) == kKeepNo,
          "a read place out of the hold drops the target at once", 1, 1);
    Check(KeepLast(true, false, false, kKeepFrames - 1) == kKeepCount &&
              KeepLast(true, false, false, kKeepFrames) == kKeepNo,
          "with no place the target stays for kKeepFrames frames",
          kKeepFrames, 10);
    Check(KeepLast(false, true, true, kKeepFrames - 1) == kKeepCount &&
              KeepLast(false, true, true, kKeepFrames) == kKeepNo,
          "bookmark_aim_hold=0 keeps the target for 10 frames only", 1, 1);
  }
}

}  // namespace bookmark_aim_checks
