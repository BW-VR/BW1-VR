// The math of the trail at the fingertip, as pure functions with no game
// state.
//
// overlay.cpp calls these with the point of the draw that it reads each
// frame, and test/controller_math calls them with numbers it makes up. Nothing
// here reads a global or touches the game. See docs/miracles.md, "The trail
// at the fingertip".
//
// THE TRAIL is the last part of the path of the draw. The points are in the
// room (game units), not in the world, so a move of the game camera or a
// turn of the view does not bend them: the trail stays where the hand drew it,
// as the plane of the draw does (controller_math.h, GesturePlane).
//
// A point goes in when the point of the draw is kStepM of the room away from
// the last point. The newest end of the trail is the point of the draw
// itself, so the ribbon always reaches the fingertip. A point stays for
// kKeepSeconds, and its light falls to nothing over that time. When the draw
// ends, all the light falls to nothing over kReleaseSeconds, and the trail is
// then empty.

#pragma once

#include <math.h>

namespace gesture_trail_math {

const int kMaxPoints = 96;
// How long a point of a draw stays.
const float kKeepSeconds = 1.5f;
// How long the trail takes to go out after the draw ends.
const float kReleaseSeconds = 0.30f;
// The distance, in meters of the room, between two points of the trail.
const float kStepM = 0.005f;

struct Trail {
  float point[kMaxPoints][3];  // in the room, game units
  float time[kMaxPoints];      // seconds, when the point went in
  int count;
  bool down;           // a draw runs now
  float release_time;  // when the last draw ended
  float live[3];       // the point of the draw now, while a draw runs
};

inline void Clear(Trail* t) {
  t->count = 0;
  t->down = false;
  t->release_time = 0.0f;
}

inline float Distance(const float a[3], const float b[3]) {
  const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
  return sqrtf(x * x + y * y + z * z);
}

// One frame. held: the draw runs now. point: the point of the draw in the
// room. seconds: the time now. step: kStepM in game units.
inline void Step(Trail* t, bool held, const float point[3], float seconds,
                 float step) {
  if (held) {
    if (!t->down) {
      // A new draw: nothing of the old trail stays.
      t->count = 0;
      t->down = true;
    }
    for (int k = 0; k < 3; ++k) t->live[k] = point[k];
    if (t->count == 0 || Distance(t->point[t->count - 1], point) >= step) {
      if (t->count == kMaxPoints) {
        for (int i = 1; i < kMaxPoints; ++i) {
          for (int k = 0; k < 3; ++k) t->point[i - 1][k] = t->point[i][k];
          t->time[i - 1] = t->time[i];
        }
        --t->count;
      }
      for (int k = 0; k < 3; ++k) t->point[t->count][k] = point[k];
      t->time[t->count] = seconds;
      ++t->count;
    }
  } else if (t->down) {
    t->down = false;
    t->release_time = seconds;
  }
  // The points that stayed too long go.
  int first = 0;
  while (first < t->count && seconds - t->time[first] > kKeepSeconds) ++first;
  if (first > 0) {
    for (int i = first; i < t->count; ++i) {
      for (int k = 0; k < 3; ++k) t->point[i - first][k] = t->point[i][k];
      t->time[i - first] = t->time[i];
    }
    t->count -= first;
  }
  if (!t->down && seconds - t->release_time > kReleaseSeconds) t->count = 0;
}

// The light of a point of age `age` seconds, 0 to 1: 1 for a new point and 0
// for a point at kKeepSeconds, with a soft end.
inline float Light(float age) {
  float a = 1.0f - age / kKeepSeconds;
  if (!(a > 0.0f)) return 0.0f;
  if (a > 1.0f) a = 1.0f;
  return a * a * (3.0f - 2.0f * a);
}

// The share of the light that is left after the draw ended: 1 while a draw
// runs, and 0 at kReleaseSeconds after its end.
inline float ReleaseLight(bool down, float seconds, float release_time) {
  if (down) return 1.0f;
  const float a = 1.0f - (seconds - release_time) / kReleaseSeconds;
  if (!(a > 0.0f)) return 0.0f;
  return a > 1.0f ? 1.0f : a;
}

}  // namespace gesture_trail_math
