// The math of the aim of a new bookmark. Pure functions with no game state.
// bookmark_aim.cpp calls them, and test/controller_math checks them
// (bookmark_aim_checks.h). See docs/controllers.md, "The sheet of the places".
#pragma once

#include <math.h>

namespace bookmark_aim_math {

// A NEW OBJECT of the aim must be in its own size and a cone of kConeDeg
// round the ray (0.9 of its radius and the cone).
const float kConeDeg = 1.5f;
const float kNewSize = 0.9f;
// THE HOLD on an object: a player cannot keep a ray on a villager that
// walks. So the object of the last frame stays the target while the ray is
// within kKeepSize of its size and kKeepDeg round it.
const float kKeepSize = 1.6f;
const float kKeepDeg = 4.0f;
// The frames that the target of the last frame stays when its place is not
// known: the draw did not offer it, and its place cannot be read.
const int kKeepFrames = 10;
// An object nearer to the origin of the ray than this, in meters of the
// room, is no target.
const float kLeastM = 0.3f;
// The middle of an object is this share of its radius over its base.
const float kMiddleShare = 0.7f;

struct Fit {
  bool ok;
  float along;    // world units along the ray to the middle
  float across;   // world units from the ray to the middle
  float allowed;  // the most of across
  float score;    // less is better
};

// THE FIT of an object to the aim ray. middle is the middle of the object,
// radius its 2D radius, land the world units along the ray to the land (less
// than 0 for no land), units the world units in a meter of the room, and last
// is true for the target of the last frame. The land may hide the object
// past its radius (2 radii for the last target). The last target has the
// wider hold, and its score is 1 lower, so it wins over a new object.
inline Fit ObjectFit(const float origin[3], const float direction[3],
                     const float middle[3], float radius, float land,
                     float units, bool last) {
  Fit out = {false, 0.0f, 0.0f, 0.0f, 0.0f};
  if (!(radius > 0.0f) || !(units > 0.0f)) return out;
  float v[3];
  for (int k = 0; k < 3; ++k) v[k] = middle[k] - origin[k];
  out.along = v[0] * direction[0] + v[1] * direction[1] + v[2] * direction[2];
  if (!(out.along > kLeastM * units)) return out;
  if (land >= 0.0f && out.along > land + (last ? 2.0f : 1.0f) * radius) {
    return out;
  }
  float off[3];
  for (int k = 0; k < 3; ++k) off[k] = v[k] - out.along * direction[k];
  out.across = sqrtf(off[0] * off[0] + off[1] * off[1] + off[2] * off[2]);
  const float deg = last ? kKeepDeg : kConeDeg;
  out.allowed = (last ? kKeepSize : kNewSize) * radius +
                out.along * tanf(deg * 0.01745329f);
  if (out.across > out.allowed) return out;
  out.score = out.across / out.allowed + out.along / (200.0f * units) -
              (last ? 1.0f : 0.0f);
  out.ok = true;
  return out;
}

// THE PLACE OF THE LAST TARGET when the draw did not offer it this frame.
// With the hold that reads the place (bookmark_aim_hold=1), a place that the
// mod can read gives the fit of that place, with no limit of frames. With no
// place (or with bookmark_aim_hold=0) the target stays for kKeepFrames frames.
enum Keep { kKeepNo, kKeepFit, kKeepCount };
inline Keep KeepLast(bool hold_reads, bool have_place, bool place_fits,
                     int misses) {
  if (hold_reads && have_place) return place_fits ? kKeepFit : kKeepNo;
  return misses < kKeepFrames ? kKeepCount : kKeepNo;
}

}  // namespace bookmark_aim_math
