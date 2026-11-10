// THE WALK WITH THE STICK (docs/god-view.md, "The walk with the stick"). The
// stick of the hand that does not point moves the player over the land of
// the God view while it is pushed. Pure functions: no Windows and no game.
// The checks are in test/controller_math/walk_checks.h.
//
// A direction on the land is a unit vector of the room with two parts: x to
// the right and z forward. A speed is in meters of the room for each second,
// so the walk feels the same at each scale of the view.

#pragma once

#include <math.h>

#include "table_math.h"
#include "zoom_math.h"

namespace walk_math {

// move_stick: which direction is forward for the stick.
const int kRuleOff = 0;         // the stick does not move the player
const int kRuleHead = 1;        // the direction of the head
const int kRuleController = 2;  // the direction in which the controller points

// The value of the key from a number: 0, 1 or 2.
inline int Rule(int value) {
  if (value < kRuleOff) return kRuleOff;
  return value > kRuleController ? kRuleController : value;
}

// The next value for the test key: from 1 to 2 to 0 and back to 1.
inline int NextRule(int value) {
  return value >= kRuleController ? kRuleOff : value + 1;
}

// move_type: how the stick moves the player.
const int kTypeWalk = 0;  // over the land (Walk, Speed and Step)
const int kTypeFly = 1;   // along the forward direction in 3D (Fly)

// The value of the key from a number: 1 is the fly, each other value the
// walk.
inline int Type(int value) { return value == kTypeFly ? kTypeFly : kTypeWalk; }

// THE KINDS OF MOVE for the test switch: the walk that follows the land
// (move_type=0, move_stick_follow_land=1), the walk that keeps its height
// (move_type=0, move_stick_follow_land=0), and the fly (move_type=1).
const int kKindWalk = 0;
const int kKindWalkLevel = 1;
const int kKindFly = 2;

inline int Kind(int type, int follow_land) {
  if (Type(type) == kTypeFly) return kKindFly;
  return follow_land != 0 ? kKindWalk : kKindWalkLevel;
}

// The next kind for the test switch: from the walk to the level walk, to the
// fly and back to the walk.
inline int NextKind(int kind) {
  return kind == kKindWalk ? kKindWalkLevel
         : kind == kKindWalkLevel ? kKindFly
                                  : kKindWalk;
}

// THE PUSH. The stick as a vector, x to the right and y forward, with the
// dead zone and the curve of the smooth zoom (zoom_math::StickShare) along
// its length. So a push in each direction has the same dead zone, a small
// push is a slow walk, and a diagonal is not faster than a straight push.
// out keeps the direction of the stick, and its length is the share of the
// top speed, 0 to 1. Returns that share.
inline float Push(float x, float y, float dead, float full, float out[2]) {
  out[0] = out[1] = 0.0f;
  if (!(x == x) || !(y == y)) return 0.0f;
  const float length = sqrtf(x * x + y * y);
  const float share = zoom_math::StickShare(length, dead, full);
  if (!(share > 0.0f)) return 0.0f;
  out[0] = x / length * share;
  out[1] = y / length * share;
  return share;
}

// FORWARD OF THE HEAD, flat on the land, from the matrix of the head in the
// room (the columns are its right, its up and its forward). The direction
// comes from the right axis of the head (table_math::HeadYaw), which stays
// level when the head looks down at the land. So a look straight down still
// has a direction. False for a head that lies on its side.
inline bool HeadForward(const float head[9], float out[2]) {
  float yaw = 0.0f;
  if (!table_math::HeadYaw(head, &yaw)) return false;
  out[0] = sinf(yaw);
  out[1] = cosf(yaw);
  return true;
}

// A controller has a direction on the land while the flat part of its unit
// direction is this long or more: it points more than 11.5 degrees away from
// straight up and from straight down.
const float kAimLeast = 0.2f;

// FORWARD OF A CONTROLLER, flat on the land, from the direction in which it
// points in the room. A controller that points almost straight up or down
// has no direction on the land: the flat part of a unit direction must be
// `least` long or more. False then, and out is not written.
inline bool AimForward(const float direction[3], float least, float out[2]) {
  const float flat =
      sqrtf(direction[0] * direction[0] + direction[2] * direction[2]);
  if (!(flat >= least) || !(flat > 0.0f)) return false;
  out[0] = direction[0] / flat;
  out[1] = direction[2] / flat;
  return true;
}

// THE SPEED IN THE ROOM, in meters for each second. forward is the forward
// direction on the land, push is the stick (Push), and top is the speed of a
// full push. The stick forward moves along forward, and the stick to the
// right moves along the direction 90 degrees to the right of it.
inline void Speed(const float forward[2], const float push[2], float top,
                  float out[2]) {
  const float right[2] = {forward[1], -forward[0]};
  out[0] = top * (push[1] * forward[0] + push[0] * right[0]);
  out[1] = top * (push[1] * forward[1] + push[0] * right[1]);
}

// THE STEP OF ONE FRAME, in meters of the room: the speed times the time of
// the frame (zoom_math::FrameSeconds gives that time, with its cap). A time
// that is not above 0 gives no step.
inline void Step(const float speed[2], float seconds, float out[2]) {
  out[0] = out[1] = 0.0f;
  if (!(seconds > 0.0f)) return;
  out[0] = speed[0] * seconds;
  out[1] = speed[1] * seconds;
}

// THE VIEW AFTER A STEP. The player moves `step` over the land, so the waist
// point G goes to the land point that was `step` from it in the room:
//
//     G' = G + s * Lt * (step.x, 0, step.z)
//
// The height of G, the heading and the scale stay. It is the pan of a grab
// with one hand whose hand moved by minus `step` (TableGrabSolve).
inline void Walk(const table_math::TablePose& pose, const float step[2],
                 table_math::TablePose* out) {
  float basis[9];
  table_math::Basis(pose.heading, basis);
  const float across[3] = {step[0] * pose.scale, 0.0f, step[1] * pose.scale};
  *out = pose;
  table_math::Along(pose.center, basis, across, out->center);
  out->center[1] = pose.center[1];
}

// THE LAND THAT THE VIEW FOLLOWS (move_stick_follow_land=1). While the player
// walks, the height of the view follows the land under the head, so the land
// stays at the same height below the hands. The land height goes through two
// stages of a first order filter. The filter counts the way that the head
// went in the room, not the time: a frame with no step changes nothing, so
// the view cannot move while the stick is let go. The land height of the new
// place is the input for the whole step, and the stages take the exact
// answer of the filter for that input:
//
//     x = way / length, e = exp(-x)
//     stage0' = land + (stage0 - land) * e
//     stage1' = land + (stage1 - land) * e + (stage0 - land) * x * e
//
// So one long step and many short steps over the same land give the same
// result, and a low frame rate gives the same heights as a high one.
//
// The length of a stage is 25 cm of the room. At a full push of 150 cm each
// second that is 0.17 s for each stage. A swell of the land with a
// wavelength of 40 cm of the room comes through at 6 percent of its height,
// one of 1 m at 29 percent, and a hill of 3 m at 78 percent. The second
// stage starts from rest, so the view does not start to rise with a jolt
// when the land under the head changes in a step.
const float kFollowLength = 0.25f;  // meters of the room, for each stage

struct LandFollow {
  float stage[2] = {0.0f, 0.0f};  // land heights in world units
};

// The start of a follow: both stages at the land height now, so the land
// keeps the height below the head that it has now.
inline void FollowStart(float land, LandFollow* follow) {
  follow->stage[0] = follow->stage[1] = land;
}

// One step of the walk. land is the land height under the head at the new
// place, in world units, way is the length of the step in meters of the
// room, and length is the length of a stage. Returns how far the filtered
// land went up, in world units (down is less than 0). The view goes up by
// the same height, so the filtered land keeps its height in the room.
inline float FollowStep(LandFollow* follow, float land, float way,
                        float length) {
  if (!(way > 0.0f) || !(length > 0.0f) || !(land == land)) return 0.0f;
  const float x = way / length;
  const float e = expf(-x);
  const float before = follow->stage[1];
  const float gap0 = follow->stage[0] - land;
  const float gap1 = follow->stage[1] - land;
  follow->stage[0] = land + gap0 * e;
  follow->stage[1] = land + gap1 * e + gap0 * x * e;
  return follow->stage[1] - before;
}

// THE SPEED RAMP (move_stick_ramp=1). While the stick is pushed to
// kRampShare of the top speed or more, the time of that push counts. For
// the first kRampHoldSeconds nothing changes. Then the speed doubles each
// kRampDoubleSeconds, up to `most` times the speed of the push:
//
//     factor = 2 ^ ((held - 1 s) / 2 s), at most `most`
//
// A push under kRampShare sets the time to 0, so the speed goes back to the
// speed of the push in that frame. A frame with no push stops the walk, as
// before. At the villager scale (1 unit for each meter) a full push is 1.5
// units each second. With `most` 32 it is 48 units each second after 11 s,
// and 1000 units of the land take about 30 s from rest.
const float kRampShare = 0.9f;
const float kRampHoldSeconds = 1.0f;
const float kRampDoubleSeconds = 2.0f;

// The time of the full push after a frame: before plus the time of the
// frame while share is kRampShare or more, else 0.
inline float RampSeconds(float before, float share, float seconds) {
  if (!(share >= kRampShare)) return 0.0f;
  if (!(seconds > 0.0f)) return before;
  return before + seconds;
}

// The factor of the speed after `held` seconds of a full push. 1 for a most
// of 1 or less.
inline float RampFactor(float held, float most) {
  if (!(most > 1.0f) || !(held > kRampHoldSeconds)) return 1.0f;
  const float factor =
      powf(2.0f, (held - kRampHoldSeconds) / kRampDoubleSeconds);
  return factor > most ? most : factor;
}

// THE FLY (move_type=1). The stick moves the view along the forward
// direction of the head or of the controller in 3D, so a push while the
// player looks or points down goes down. The stick to the side moves level,
// at right angles to that direction. The view does not follow the land.
// The height has a floor and a ceiling (FlyHeight).

// A unit vector from a direction. False for a direction with no length or
// with a part that is not a number, and then out is not written.
inline bool Unit(const float direction[3], float out[3]) {
  const float length =
      sqrtf(direction[0] * direction[0] + direction[1] * direction[1] +
            direction[2] * direction[2]);
  if (!(length > 1e-6f)) return false;
  for (int i = 0; i < 3; ++i) out[i] = direction[i] / length;
  return true;
}

// THE LEVEL RIGHT of a fly, as x and z of the room: 90 degrees to the right
// of the flat part of forward. A forward that points almost straight up or
// down has a flat part shorter than `least`, and then the right is
// `fallback` (the level right of the head).
inline void FlyRight(const float forward[3], const float fallback[2],
                     float least, float out[2]) {
  const float flat = sqrtf(forward[0] * forward[0] + forward[2] * forward[2]);
  if (flat >= least && flat > 0.0f) {
    out[0] = forward[2] / flat;
    out[1] = -forward[0] / flat;
    return;
  }
  out[0] = fallback[0];
  out[1] = fallback[1];
}

// THE SPEED OF A FLY IN THE ROOM, in meters for each second, x, y and z.
// forward is a unit direction in the room, right is its level right
// (FlyRight), push is the stick (Push), and top is the speed of a full push.
inline void FlySpeed(const float forward[3], const float right[2],
                     const float push[2], float top, float out[3]) {
  out[0] = top * (push[1] * forward[0] + push[0] * right[0]);
  out[1] = top * push[1] * forward[1];
  out[2] = top * (push[1] * forward[2] + push[0] * right[1]);
}

// THE STEP OF ONE FRAME OF A FLY, in meters of the room. A time that is not
// above 0 gives no step.
inline void FlyStep(const float speed[3], float seconds, float out[3]) {
  out[0] = out[1] = out[2] = 0.0f;
  if (!(seconds > 0.0f)) return;
  for (int i = 0; i < 3; ++i) out[i] = speed[i] * seconds;
}

// THE VIEW AFTER A STEP OF A FLY. The player moves `step` in the room, so
// the waist point G goes to the world point that was `step` from it:
//
//     G' = G + s * Lt * step
//
// The heading and the scale stay. A step up is a move of G up, so the land
// goes down in the room.
inline void Fly(const table_math::TablePose& pose, const float step[3],
                table_math::TablePose* out) {
  float basis[9];
  table_math::Basis(pose.heading, basis);
  const float across[3] = {step[0] * pose.scale, step[1] * pose.scale,
                           step[2] * pose.scale};
  *out = pose;
  table_math::Along(pose.center, basis, across, out->center);
}

// THE HEIGHT OF A FLY. The floor is the floor of the grab: the land under
// the head stays nearest_m or more below the head (head_y, in the room).
// The ceiling is in world units, so that a small scale does not get a low
// ceiling: the head stays at most ceiling_units over the land. The far zoom
// (4000 units for each meter, the land 2.5 m below the head) puts the head
// 10000 units over the land, so that height is one that the view already
// draws. As in GodGroundHeight (table_math.h), the result never moves the
// height farther out of the range than `current` already is, so at a limit
// the height stops and never jumps.
//
//     under(C.y) = room_y + (land - C.y) / s
//     floor:   C.y >= land - (head_y - nearest_m - room_y) * s
//     ceiling: C.y <= land - (head_y - room_y) * s + ceiling_units
const float kFlyCeilingUnits = 10000.0f;

inline float FlyHeight(float wanted, float current, float land, float room_y,
                       float scale, float head_y, float nearest_m,
                       float ceiling_units) {
  if (!(scale > 0.0f)) return current;
  float low = land - (head_y - nearest_m - room_y) * scale;
  float high = land - (head_y - room_y) * scale + ceiling_units;
  if (current < low) low = current;
  if (current > high) high = current;
  if (wanted < low) return low;
  if (wanted > high) return high;
  return wanted;
}

// The degrees of a direction below level, for the log: up is less than 0.
inline float DegreesDown(const float direction[3]) {
  const float flat =
      sqrtf(direction[0] * direction[0] + direction[2] * direction[2]);
  return atan2f(-direction[1], flat) * 57.29578f;
}

// A direction on the land in degrees, for the log: 0 is straight ahead in the
// room, and a direction to the right is more than 0.
inline float DegreesRight(const float direction[2]) {
  return atan2f(direction[0], direction[1]) * 57.29578f;
}

// A move over the land in the world as a move in the room, in meters: the
// map of Walk backward. For the log.
inline void InRoom(const table_math::TablePose& pose, float world_dx,
                   float world_dz, float out[2]) {
  float basis[9];
  table_math::Basis(pose.heading, basis);
  out[0] = out[1] = 0.0f;
  if (!(pose.scale > 0.0f)) return;
  // The transpose of the basis, then the scale.
  out[0] = (basis[0] * world_dx + basis[6] * world_dz) / pose.scale;
  out[1] = (basis[2] * world_dx + basis[8] * world_dz) / pose.scale;
}

}  // namespace walk_math
