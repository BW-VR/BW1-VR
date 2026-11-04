// The late camera, a test (src/mod/late_camera_math.h). main.cpp includes this
// file after Check and Near, just before main.
#pragma once

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../src/mod/late_camera_math.h"

namespace late_camera_checks {

namespace lc = late_camera_math;
namespace mc = motion_comp_math;
namespace ic = input_channel;

const float kWidth = 2000.0f;
const float kHeight = 1600.0f;

// The pixel of a point in the space of the eye: x right, y up, -z forward. The
// frustum of the game: pixel 0 has the tangent of half the width.
inline void PixelOf(const lc::Push& p, const float at[3], float* x, float* y) {
  const float d = -at[2];
  const float a = p.room[3];
  *x = (at[0] / d) * a + kWidth * 0.5f - 0.5f;
  *y = kHeight * 0.5f - (at[1] / d) * a - 0.5f;
}

inline lc::Input PlainInput() {
  lc::Input in = {};
  const float half = 1.1f;
  in.eye_position[0][0] = -0.03f;
  in.eye_position[1][0] = 0.03f;
  in.eye_position[0][1] = in.eye_position[1][1] = 1.5f;
  in.eye_orientation[3] = 1.0f;
  in.width = 2000;
  in.height = 1600;
  const float aspect = atanf(tanf(half) * kHeight / kWidth);
  in.fov[0] = -half;
  in.fov[1] = half;
  in.fov[2] = aspect;
  in.fov[3] = -aspect;
  in.head_now[1] = 1.5f;
  return in;
}

inline void ViewChecks() {
  const lc::Keys keys;
  lc::Input in = PlainInput();
  // With no motion the picture stays as the game drew it.
  lc::Result still = lc::Compute(in, keys);
  Check(still.push.move[3] == 0.0f && still.move_cm == 0.0f, "late camera: no motion, no late camera", still.push.move[3], 0.0);
  // A step of the head to the right moves each point to the left in the eye.
  in.head_now[0] = 0.01f;
  lc::Result step = lc::Compute(in, keys);
  Check(step.push.move[3] > 0.0f, "late camera: a step of the head gives a move", step.push.move[3], 0.0);
  Near("late camera: the move of a point for a step of 1 cm", step.push.move[0], -0.01f, 1e-5f);
  Near("late camera: no move in y for a step in x", step.push.move[1], 0.0f, 1e-5f);
  Near("late camera: the eyes of the layer go with the head", step.head_move[0], 0.01f, 1e-5f);
  Near("late camera: the length of the move in cm", step.move_cm, 1.0f, 1e-3f);
  // The separation times the pixels of a tangent: width / (2 tan half).
  Near("late camera: k of the shader", step.push.move[3], 0.06f * 2000.0f / (2.0f * tanf(1.1f)), 1e-2f);
  Near("late camera: a step of the head moves a thing of the room too", step.push.room[0], -0.01f, 1e-5f);

  // A head that looks to the left (a turn of 90 degrees about up): the step to
  // the right in the room is a step backwards in the eye.
  lc::Input turned = PlainInput();
  const float half_turn = 0.5f * 90.0f * 3.14159265f / 180.0f;
  turned.eye_orientation[1] = sinf(half_turn);
  turned.eye_orientation[3] = cosf(half_turn);
  turned.head_now[0] = 0.01f;
  lc::Result back = lc::Compute(turned, keys);
  Near("late camera: a turned eye: no move to the side", back.push.move[0], 0.0f, 1e-5f);
  Near("late camera: a turned eye: a step to the side of the room is a step along z of the eye", back.push.move[2], -0.01f, 1e-5f);

  // A frustum that is not symmetric has no late camera.
  lc::Input other = PlainInput();
  other.head_now[0] = 0.01f;
  other.fov[0] = -0.9f;
  Check(lc::Compute(other, keys).push.move[3] == 0.0f, "late camera: another frustum, no late camera", 0.0, 0.0);

  // THE PLACE OF A VERTEX: the clip position of the shader gives the pixel of the
  // moved point, also with a move in depth.
  const float view_port[4] = {0.0f, 0.0f, 2.0f / kWidth, 2.0f / kHeight};
  const float near_m = 0.2f;
  // The move of the right eye, for k = near_m as a rhw of 1 at near_m.
  const float per_rhw = -step.push.move[3] / near_m;
  const float point[3] = {0.3f, -0.2f, -1.5f};
  float x = 0.0f, y = 0.0f;
  PixelOf(step.push, point, &x, &y);
  lc::Vertex v;
  v.x = x;
  v.y = y;
  v.rhw = near_m / 1.5f;
  v.z = 1.0f - v.rhw;
  lc::Push moved = step.push;
  moved.move[0] = 0.05f;
  moved.move[1] = -0.02f;
  moved.move[2] = 0.1f;
  const lc::Clip c = lc::ClipOf(moved, v, per_rhw, view_port, 0.0f, 1.0f, false);
  const float want[3] = {0.35f, -0.22f, -1.4f};
  float wx = 0.0f, wy = 0.0f;
  PixelOf(step.push, want, &wx, &wy);
  Near("late camera: x of a moved vertex", (c.x / c.w + 1.0f) * 1000.0f - 0.5f, wx, 0.05f);
  Near("late camera: y of a moved vertex", (c.y / c.w + 1.0f) * 800.0f - 0.5f, wy, 0.05f);
  Near("late camera: w is the new depth", c.w, 1.4f / near_m, 1e-3f);
  Near("late camera: z is the depth of the game at the new place", c.z / c.w, 1.0f - near_m / 1.4f, 1e-4f);

  // A thing of the room takes its own move.
  lc::Push hand = moved;
  hand.room[0] = hand.room[1] = hand.room[2] = 0.0f;
  const lc::Clip h = lc::ClipOf(hand, v, per_rhw, view_port, 0.0f, 1.0f, true);
  Near("late camera: a thing of the room stays at its pixel in x", (h.x / h.w + 1.0f) * 1000.0f - 0.5f, x, 0.05f);
  Near("late camera: a thing of the room stays at its pixel in y", (h.y / h.w + 1.0f) * 800.0f - 0.5f, y, 0.05f);

  // No move: the shader gives the clip position of the plain path.
  lc::Push zero = step.push;
  zero.move[0] = zero.move[1] = zero.move[2] = 0.0f;
  const lc::Clip o = lc::ClipOf(zero, v, per_rhw, view_port, 0.0f, 1.0f, false);
  const float plain_w = 1.0f / v.rhw;
  Near("late camera: no move: w", o.w, plain_w, 1e-4f);
  Near("late camera: no move: x", o.x, (v.x + 0.5f) * view_port[2] * plain_w - plain_w, 1e-3f);
  Near("late camera: no move: z", o.z, v.z * plain_w, 1e-3f);

  // A point past the new place of the eye still has a linear clip position, so
  // the GPU clips it.
  lc::Push through = step.push;
  through.move[2] = 2.0f;
  const lc::Clip behind = lc::ClipOf(through, v, per_rhw, view_port, 0.0f, 1.0f, false);
  Check(behind.w < 0.0f, "late camera: a point behind the eye has w below 0", behind.w, 0.0);

  // The margin of the test against the centre image.
  Check(lc::MarginPixels(step.push, 0.15f) > 2.0f, "late camera: a move gives a margin", lc::MarginPixels(step.push, 0.15f), 0.0);
  Check(lc::MarginPixels(lc::NoPush(), 0.15f) == 0.0f, "late camera: no move, no margin", 0.0f, 0.0);
}

inline void MapChecks() {
  const lc::Keys keys;
  lc::Input in = PlainInput();
  const float basis[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  const float origin[3] = {0.0f, 1.5f, 0.0f};
  const float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  const float place[3] = {100.0f, 20.0f, 300.0f};
  in.have_maps = true;
  in.drawn = mc::MakeMap(place, basis, 100.0f, origin, rotation);
  // The player walks 25 cm of the room to +x: 25 units.
  in.now = in.drawn;
  in.now.offset[0] += 25.0f;
  lc::Result walk = lc::Compute(in, keys);
  Near("late camera: the walk in the map, cm", walk.world_cm, 25.0f, 0.01f);
  Near("late camera: the land moves back under the player", walk.push.move[0], -0.25f, 1e-4f);
  Near("late camera: a walk does not move the eyes of the layer", walk.head_move[0], 0.0f, 1e-6f);
  Near("late camera: a walk does not move a thing of the room", walk.push.room[0], 0.0f, 1e-6f);

  // A snap turn is a cut: the world stays as the game drew it.
  float turn[4] = {0.0f, sinf(0.3927f), 0.0f, cosf(0.3927f)};
  in.now = mc::MakeMap(place, basis, 100.0f, origin, turn);
  lc::Result cut = lc::Compute(in, keys);
  Check(cut.cut && cut.world_cm == 0.0f, "late camera: a snap turn is a cut", cut.world_cm, 0.0);
}

// A walk at an even speed: each tick has the place of the player at the display
// time of the slot before the newest slot, whichever frame of the mod is the
// newest one. So each picture moves by one slot. The map of a frame of the mod is
// the place at the display time of the slot of its camera call (input_frame), and
// the mod is 0 to 5 slots behind.
inline void AheadChecks() {
  const float period_s = 0.01f;
  const int64_t period_ns = 10000000;
  const float speed = 200.0f;  // units a second
  mc::SlotPush slots[64];
  float worst = 0.0f;
  float last = 0.0f;
  bool have_last = false;
  bool even = true;
  for (uint32_t newest = 20; newest < 60; ++newest) {
    const uint32_t behind = (newest * 7u) % 6u;
    ic::World w = {};
    w.flags = ic::kWorldValid | ic::kWorldWalk;
    w.input_frame = newest - behind;
    w.display_time = static_cast<int64_t>(w.input_frame) * period_ns;
    w.walk_velocity[0] = speed;
    w.walk_hand = 0;
    w.walk_share = 1.0f;
    w.walk_dead = 0.1f;
    w.walk_full = 1.0f;
    w.offset[0] = speed * period_s * static_cast<float>(w.input_frame);
    w.linear[0] = w.linear[4] = w.linear[8] = 1.0f;
    int count = 0;
    for (uint32_t f = w.input_frame + 1; f <= newest; ++f) {
      slots[count] = mc::SlotPush();
      slots[count].frame = f;
      slots[count].display_time = static_cast<int64_t>(f) * period_ns;
      slots[count].stick[0][1] = 1.0f;
      ++count;
    }
    const float ahead = lc::AheadSeconds(w, slots, count, newest, period_s, 250);
    const mc::Map now = lc::MapNow(w, ahead);
    const float want = speed * period_s * static_cast<float>(newest - 1);
    const float off = fabsf(now.offset[0] - want);
    if (off > worst) worst = off;
    if (have_last && lc::StepUneven(now.offset[0] - last, speed, period_s)) even = false;
    last = now.offset[0];
    have_last = true;
  }
  Near("late camera: an even walk, each picture one slot on", worst, 0.0f, 0.01f);
  Check(even, "late camera: an even walk has no uneven step", even ? 1.0 : 0.0, 1.0);
  Check(lc::StepUneven(0.0f, speed, period_s), "late camera: a step of 0 at a speed is uneven", 1.0, 1.0);
  Check(lc::StepUneven(4.0f, speed, period_s), "late camera: a step of two slots is uneven", 1.0, 1.0);

  // No walk: nothing ahead.
  ic::World still = {};
  still.flags = ic::kWorldValid;
  still.input_frame = 10;
  Check(lc::AheadSeconds(still, slots, 0, 20, period_s, 250) == 0.0f, "late camera: no walk, nothing ahead", 0.0f, 0.0);
}

inline void TakeChecks() {
  using lc::TickPicture;
  const auto ends = [](TickPicture p, uint64_t handled, uint64_t serial, double waited) {
    return lc::TakeWaitEnds(p, handled, serial, waited, 4) ? 1.0 : 0.0;
  };
  Check(ends(TickPicture::kNone, 0, 0, 0.0) == 1.0, "late take: no tick before, no wait",
        ends(TickPicture::kNone, 0, 0, 0.0), 1.0);
  Check(ends(TickPicture::kDone, 5, 6, 0.0) == 1.0, "late take: a finished picture ends the wait",
        ends(TickPicture::kDone, 5, 6, 0.0), 1.0);
  Check(ends(TickPicture::kOnGpu, 6, 6, 1.0) == 0.0, "late take: a picture on the GPU, it waits",
        ends(TickPicture::kOnGpu, 6, 6, 1.0), 0.0);
  Check(ends(TickPicture::kNone, 5, 6, 1.0) == 0.0, "late take: the play thread still works, it waits",
        ends(TickPicture::kNone, 5, 6, 1.0), 0.0);
  Check(ends(TickPicture::kNone, 6, 6, 0.0) == 1.0, "late take: a tick with no picture, no wait",
        ends(TickPicture::kNone, 6, 6, 0.0), 1.0);
  Check(ends(TickPicture::kNone, 7, 6, 0.0) == 1.0, "late take: a newer tick was handled, no wait",
        ends(TickPicture::kNone, 7, 6, 0.0), 1.0);
  Check(ends(TickPicture::kOnGpu, 6, 6, 4.0) == 1.0, "late take: the limit ends the wait",
        ends(TickPicture::kOnGpu, 6, 6, 4.0), 1.0);
  Check(ends(TickPicture::kNone, 5, 6, 4.5) == 1.0, "late take: the limit also with no picture",
        ends(TickPicture::kNone, 5, 6, 4.5), 1.0);
}

inline void LateCameraChecks() {
  ViewChecks();
  MapChecks();
  AheadChecks();
  TakeChecks();
}

}  // namespace late_camera_checks
