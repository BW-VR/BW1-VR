// The motion of the stick in the headset app, a test
// (src/mod/motion_comp_math.h, the slot of the place of the player in
// src/common/input_channel.h, PictureView of src/mod/view_layer_math.h and
// AgeParts of android/src/frame_loop.h). main.cpp includes this file after
// Check and Near, just before main, and calls it after the other checks:
// it recenters the head of head_pose.cpp.
#pragma once

#include <math.h>
#include <string.h>

#include "../../android/src/frame_loop.h"
#include "../../src/common/input_channel.h"
#include "../../src/mod/motion_comp_math.h"
#include "../../src/mod/view_layer_math.h"

void HeadPoseRecenter();
void HeadPoseSetHead(const float center[3]);

namespace motion_comp_checks {

namespace mc = motion_comp_math;
namespace ic = input_channel;

// A turn about the up axis, as a quaternion x y z w.
inline void Yaw(float degrees, float q[4]) {
  const float half = 0.5f * degrees * 3.14159265f / 180.0f;
  q[0] = 0.0f;
  q[1] = sinf(half);
  q[2] = 0.0f;
  q[3] = cosf(half);
}

// The levelled basis of a room with a heading (head_camera.cpp): columns
// right, up and forward of the left handed world.
inline void Basis(float degrees, float b[9]) {
  const float a = degrees * 3.14159265f / 180.0f;
  const float forward[3] = {sinf(a), 0.0f, cosf(a)};
  const float up[3] = {0.0f, 1.0f, 0.0f};
  const float right[3] = {forward[2], 0.0f, -forward[0]};  // up x forward
  for (int i = 0; i < 3; ++i) {
    b[i * 3 + 0] = right[i];
    b[i * 3 + 1] = up[i];
    b[i * 3 + 2] = forward[i];
  }
}

inline float Distance(const float a[3], const float b[3]) {
  const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
  return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

// The map of head_camera.cpp is the camera of the main call: the real
// head_pose.cpp gives the lean and the origin, and the camera is
// camera + basis * lean * gain.
inline void MapChecks() {
  printf("\nThe motion of the stick: the map of the room to the world\n");
  const float old_scale = HeadPoseScale();
  HeadPoseSetRotationOnly(false);
  HeadPoseSetScale(7.5f);
  HeadPoseRecenter();
  const float start[3] = {0.10f, 1.60f, -0.20f};
  float start_q[4];
  Yaw(25.0f, start_q);
  HeadPoseSetHead(start);
  HeadPoseUpdate(start, start_q);  // the recenter pose: yaw 25 degrees at start
  // The head now: leaned and turned.
  const float head[3] = {0.25f, 1.52f, -0.45f};
  float head_q[4];
  Yaw(60.0f, head_q);
  const float tilt = 0.3f;
  const float pitch[4] = {sinf(tilt * 0.5f), 0.0f, 0.0f, cosf(tilt * 0.5f)};
  mc::QuatMul(head_q, pitch, head_q);
  HeadPoseSetHead(head);
  HeadPoseUpdate(head, head_q);
  float h[9], lean[3], origin[3], origin_q[4];
  const bool have = HeadPoseCurrent(h, lean) && HeadPoseOrigin(origin, origin_q);
  Check(have, "head_pose.cpp gives the head and the recenter pose", have, 1);
  const float camera[3] = {1858.8f, 61.0f, 2528.7f};
  float basis[9];
  Basis(40.0f, basis);
  const float gain = 100.0f / 7.5f;  // the God view at 100 units a meter
  const mc::Map m = mc::MakeMap(camera, basis, gain * HeadPoseScale(), origin, origin_q);
  float drawn[3], want[3];
  mc::Point(m, head, drawn);
  for (int i = 0; i < 3; ++i) {
    want[i] = camera[i] + (basis[i * 3 + 0] * lean[0] + basis[i * 3 + 1] * lean[1] +
                           basis[i * 3 + 2] * lean[2]) * gain;
  }
  Near("the map of the head is the camera of the main call, units", Distance(drawn, want), 0.0,
       1e-3);
  // The turn: the camera basis L * h is the turn of the map times the head.
  float render[9];
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      render[i * 3 + j] = basis[i * 3 + 0] * h[0 * 3 + j] + basis[i * 3 + 1] * h[1 * 3 + j] +
                          basis[i * 3 + 2] * h[2 * 3 + j];
    }
  }
  float rq[9];
  mc::QuatMatrix(head_q, rq);
  const float k = gain * HeadPoseScale();
  float off = 0.0f;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      // (linear / k) * R(q) * F, F = diag(1, 1, -1)
      float sum = 0.0f;
      for (int n = 0; n < 3; ++n) sum += m.linear[i * 3 + n] / k * rq[n * 3 + j];
      if (j == 2) sum = -sum;
      off += fabsf(sum - render[i * 3 + j]);
    }
  }
  Near("the turn of the map times the head is the basis of the camera", off, 0.0, 1e-4);
  const float right_units = sqrtf(m.linear[0] * m.linear[0] + m.linear[3] * m.linear[3] +
                                  m.linear[6] * m.linear[6]);
  Near("a meter of the room is 100 units", right_units, 100.0, 1e-2);
  Check(mc::Det(m.linear) < 0.0f, "the map turns the right handed room into the left handed world",
        mc::Det(m.linear) < 0.0f, 1);
  HeadPoseRecenter();
  HeadPoseSetScale(old_scale);
}

inline void DifferenceChecks() {
  printf("\nThe motion of the stick: the difference of two maps\n");
  const float camera[3] = {2000.0f, 50.0f, 2400.0f};
  float basis[9], origin_q[4];
  Basis(30.0f, basis);
  Yaw(20.0f, origin_q);
  const float origin[3] = {0.1f, 1.6f, -0.3f};
  const float units = 100.0f;
  const mc::Map drawn = mc::MakeMap(camera, basis, units, origin, origin_q);
  const float head[3] = {0.2f, 1.55f, -0.4f};

  // A STILL PLAYER: the same map gives no difference, to the bit.
  mc::Map d;
  Check(mc::Difference(drawn, drawn, &d), "a still player has a difference", 1, 1);
  const mc::Motion still = mc::Measure(d, head);
  const mc::Map identity = mc::Identity();
  Check(mc::Still(still) && memcmp(&d, &identity, sizeof(d)) == 0,
        "a still player: the difference is the identity", still.step, 0);
  float p[3], q[4];
  const float eye_q[4] = {0.05f, 0.30f, -0.02f, sqrtf(1.0f - 0.05f * 0.05f - 0.30f * 0.30f - 0.02f * 0.02f)};
  mc::MovePose(d, 1.0f, head, eye_q, p, q);
  Check(memcmp(p, head, sizeof(p)) == 0, "a still player: the eye keeps its place to the bit",
        Distance(p, head), 0);
  float qd = 0.0f;
  for (int i = 0; i < 4; ++i) qd += fabsf(q[i] - eye_q[i]);
  Near("a still player: the eye keeps its turn", qd, 0.0, 1e-6);

  // A STEP OF THE STICK: the walk moves the camera by s * L * (step) in the
  // world. A point of the room moves by the opposite step.
  const float step_room[3] = {0.03f, 0.0f, 0.12f};  // right and forward in the room
  float world_step[3];
  for (int i = 0; i < 3; ++i) {
    world_step[i] = units * (basis[i * 3 + 0] * step_room[0] + basis[i * 3 + 2] * step_room[2]);
  }
  const float walked[3] = {camera[0] + world_step[0], camera[1], camera[2] + world_step[2]};
  const mc::Map now = mc::MakeMap(walked, basis, units, origin, origin_q);
  Check(mc::Difference(now, drawn, &d), "a step has a difference", 1, 1);
  const mc::Motion m = mc::Measure(d, head);
  Near("a step of 12.4 cm gives a step of 12.4 cm at the head", m.step,
       sqrtf(0.03f * 0.03f + 0.12f * 0.12f), 1e-4);
  Near("a step gives no turn, degrees", m.turn, 0.0, 0.05);
  Near("a step gives no change of the scale", m.scale, 1.0, 1e-5);
  // In the LOCAL space the step of the room is R(origin) * F * step.
  float r[9];
  mc::QuatMatrix(origin_q, r);
  const float flipped[3] = {step_room[0], step_room[1], -step_room[2]};
  float local_step[3];
  mc::Rotate(r, flipped, local_step);
  float moved[3];
  mc::Point(d, head, moved);
  const float want[3] = {head[0] - local_step[0], head[1] - local_step[1],
                         head[2] - local_step[2]};
  Near("the head goes back by the step, in the LOCAL space, m", Distance(moved, want), 0.0, 1e-4);
  Check(mc::Within(m, mc::Limits()), "a step of 12 cm is within the limits", m.step, 0.5);
  // The pose of a view: the eye goes back by the step. With a depth the view
  // also turns so that the point 1 m ahead shows in its direction now.
  const float ahead_q[4] = {0.0f, 0.0f, 0.0f, 1.0f};  // looks along -z
  mc::MovePose(d, 0.0f, head, ahead_q, p, q);
  Near("with no depth the eye moves by the step, m", Distance(p, want), 0.0, 1e-4);
  Near("with no depth a step does not turn the view", fabsf(q[3]), 1.0, 1e-6);
  mc::MovePose(d, 1.0f, head, ahead_q, p, q);
  float qm[9];
  mc::QuatMatrix(q, qm);
  const float forward[3] = {-qm[2], -qm[5], -qm[8]};
  const float point[3] = {head[0], head[1], head[2] - 1.0f};
  float point_now[3];
  mc::Point(d, point, point_now);
  float dir[3] = {point_now[0] - head[0], point_now[1] - head[1], point_now[2] - head[2]};
  const float dl = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
  for (float& v : dir) v /= dl;
  const float dot = forward[0] * dir[0] + forward[1] * dir[1] + forward[2] * dir[2];
  Near("with a depth of 1 m the view looks at the point 1 m ahead where it is now", dot, 1.0,
       1e-5);
  const float swing = 2.0f * acosf(fabsf(q[3]) > 1.0f ? 1.0f : fabsf(q[3])) * 57.29578f;
  // The point 1 m ahead (along -z) is back by the step: the angle between
  // (0, 0, -1) and (-x, -y, -1 - z) of the step.
  const float side = sqrtf(local_step[0] * local_step[0] + local_step[1] * local_step[1]);
  Near("and that turn is the angle of the step at 1 m, degrees", swing,
       atan2f(side, 1.0f + local_step[2]) * 57.29578f, 0.05);
  Check(swing > 0.5f, "a step to the side turns the view (the runtime has no depth)", swing, 1);

  // A TURN OF THE VIEW ABOUT THE HEAD (HeadPoseTurn, or the snap turn of the
  // God view): the origin turns about the head, and the head keeps its place.
  float turn_q[4];
  Yaw(4.0f, turn_q);
  float turned_q[4];
  mc::QuatMul(origin_q, turn_q, turned_q);
  float turned_r[9];
  mc::QuatMatrix(turned_q, turned_r);
  // in_room = R(origin)^T (head - origin), back = R(turned) in_room
  const float off_head[3] = {head[0] - origin[0], head[1] - origin[1], head[2] - origin[2]};
  float in_room[3] = {0, 0, 0};
  for (int i = 0; i < 3; ++i) {
    for (int k = 0; k < 3; ++k) in_room[i] += r[k * 3 + i] * off_head[k];
  }
  float back[3];
  mc::Rotate(turned_r, in_room, back);
  const float turned_origin[3] = {head[0] - back[0], head[1] - back[1], head[2] - back[2]};
  const mc::Map turned = mc::MakeMap(camera, basis, units, turned_origin, turned_q);
  Check(mc::Difference(turned, drawn, &d), "a turn has a difference", 1, 1);
  const mc::Motion t = mc::Measure(d, head);
  Near("a turn of 4 degrees about the head gives no step at the head, m", t.step, 0.0, 1e-4);
  Near("a turn of 4 degrees gives a turn of 4 degrees", t.turn, 4.0, 0.02);
  const float point_far[3] = {head[0], head[1], head[2] - 2.0f};
  float far_now[3];
  mc::Point(d, point_far, far_now);
  Near("a point 2 m ahead moves on a circle about the head: 2 sin(2) * 2 m", Distance(point_far, far_now),
       2.0 * 2.0 * sin(2.0 * 3.14159265 / 180.0), 1e-3);
  Check(mc::Within(t, mc::Limits()), "a turn of 4 degrees is within the limits", t.turn, 10);
  // A snap turn of 30 degrees is a cut.
  Yaw(30.0f, turn_q);
  mc::QuatMul(origin_q, turn_q, turned_q);
  mc::QuatMatrix(turned_q, turned_r);
  mc::Rotate(turned_r, in_room, back);
  const float snap_origin[3] = {head[0] - back[0], head[1] - back[1], head[2] - back[2]};
  mc::Difference(mc::MakeMap(camera, basis, units, snap_origin, turned_q), drawn, &d);
  Check(!mc::Within(mc::Measure(d, head), mc::Limits()),
        "a snap turn of 30 degrees is over the limits (a cut)", mc::Measure(d, head).turn, 10);
  // A teleport of 3 m is a cut.
  const float far_camera[3] = {camera[0] + 300.0f, camera[1], camera[2]};
  mc::Difference(mc::MakeMap(far_camera, basis, units, origin, origin_q), drawn, &d);
  Check(!mc::Within(mc::Measure(d, head), mc::Limits()), "a teleport of 3 m is a cut",
        mc::Measure(d, head).step, 0.5);
  // A zoom of 2 percent is within, a zoom step of 1.5 times is a cut.
  mc::Difference(mc::MakeMap(camera, basis, units * 1.02f, origin, origin_q), drawn, &d);
  Near("a zoom of 2 percent: the scale of the difference", mc::Measure(d, head).scale,
       1.0 / 1.02, 1e-4);
  mc::Difference(mc::MakeMap(camera, basis, units * 1.5f, origin, origin_q), drawn, &d);
  Check(!mc::Within(mc::Measure(d, head), mc::Limits()), "a zoom step of 1.5 times is a cut",
        mc::Measure(d, head).scale, 0.667);
}

inline ic::World WorldOf(const mc::Map& m, uint64_t frame, int64_t display) {
  ic::World w = {};
  w.flags = ic::kWorldValid;
  w.frame = frame;
  w.display_time = display;
  memcpy(w.linear, m.linear, sizeof(w.linear));
  memcpy(w.offset, m.offset, sizeof(w.offset));
  w.walk_hand = -1;
  return w;
}

inline void PredictChecks() {
  printf("\nThe motion of the stick: the newest map and the prediction\n");
  float basis[9], origin_q[4];
  Basis(0.0f, basis);
  Yaw(0.0f, origin_q);
  const float origin[3] = {0, 0, 0};
  const float camera[3] = {100.0f, 0.0f, 100.0f};
  const mc::Map m = mc::MakeMap(camera, basis, 100.0f, origin, origin_q);
  ic::World picture = WorldOf(m, 500, 1000000000);
  ic::World channel = WorldOf(m, 502, 1030000000);
  Check(mc::Newest(&channel, &picture) == &channel, "the channel ahead of the picture is newer",
        0, 0);
  channel.frame = 498;
  Check(mc::Newest(&channel, &picture) == &picture,
        "a channel behind the picture: the map of the picture", 0, 0);
  channel.frame = 500 + mc::kAhead + 1;
  Check(mc::Newest(&channel, &picture) == &picture,
        "a channel far ahead (an earlier game): the map of the picture", 0, 0);
  channel.frame = 502;
  channel.flags = 0;
  Check(mc::Newest(&channel, &picture) == &picture, "a channel with no map: the picture", 0, 0);
  picture.flags = 0;
  Check(mc::Newest(&channel, &picture) == nullptr, "no map at all: none", 0, 0);

  // The walk of the frame: 150 units a second forward at a full push of the
  // left stick.
  ic::World w = WorldOf(m, 600, 2000000000);
  w.flags |= ic::kWorldWalk;
  w.walk_hand = 0;
  w.walk_share = 1.0f;
  w.walk_dead = 0.3f;
  w.walk_full = 0.95f;
  w.walk_velocity[2] = 150.0f;
  const float pushed[2][2] = {{0.0f, 1.0f}, {0.0f, 0.0f}};
  const float half[2][2] = {{0.0f, 0.625f}, {0.0f, 0.0f}};
  const float back[2][2] = {{0.0f, 0.1f}, {0.8f, 0.0f}};
  float ahead = 0.0f;
  mc::Map now = mc::Now(w, mc::kPredict, pushed, 2020000000, 50, &ahead);
  Near("20 ms later at a full push: 20 ms ahead", ahead, 0.020, 1e-6);
  Near("and the map goes 3 units forward", now.offset[2] - m.offset[2], 3.0, 1e-3);
  now = mc::Now(w, mc::kPredict, half, 2020000000, 50, &ahead);
  Near("at half the push: 1.5 units", now.offset[2] - m.offset[2], 1.5, 1e-3);
  now = mc::Now(w, mc::kPredict, pushed, 2200000000, 50, &ahead);
  Near("200 ms later: at most motion_comp_predict_ms (50 ms), 7.5 units", now.offset[2] - m.offset[2], 7.5,
       1e-3);
  now = mc::Now(w, mc::kPredict, back, 2020000000, 50, &ahead);
  Check(memcmp(&now, &m, sizeof(now)) == 0 && ahead == 0.0f,
        "the stick of the walk is back (the other stick is pushed): no prediction", ahead, 0);
  now = mc::Now(w, mc::kNewest, pushed, 2020000000, 50, &ahead);
  Check(memcmp(&now, &m, sizeof(now)) == 0, "motion_comp=1: no prediction", ahead, 0);
  now = mc::Now(w, mc::kPredict, pushed, 1990000000, 50, &ahead);
  Check(memcmp(&now, &m, sizeof(now)) == 0, "a display time before the map: no prediction",
        ahead, 0);
  Check(mc::Mode(-1) == 0 && mc::Mode(1) == 1 && mc::Mode(7) == 2, "the values of motion_comp",
        mc::Mode(7), 2);
}

inline void SlotChecks() {
  printf("\nThe motion of the stick: the slot of the place in the input channel\n");
  static ic::File file;
  memset(&file, 0, sizeof(file));
  ic::MakeHead(&file);
  ic::World w = {};
  w.flags = ic::kWorldValid | ic::kWorldWalk;
  w.frame = 1234;
  w.offset[0] = 5.0f;
  w.walk_hand = 1;
  ic::WriteWorld(&file.world, w);
  ic::World out = {};
  Check(ic::ReadWorld(&file.world, &out, 4) && memcmp(&out, &w, sizeof(w)) == 0,
        "the app reads the place that the mod wrote", static_cast<double>(out.frame), 1234);
  Check((file.world.seq & 1u) == 0, "the sequence is even after a write", file.world.seq, 2);
  // A writer that stopped in its write leaves an odd sequence: no read is
  // whole, and the next write makes it whole again.
  file.world.seq = 7;
  Check(!ic::ReadWorld(&file.world, &out, 4), "an odd sequence: no whole read", 0, 0);
  w.frame = 1235;
  ic::WriteWorld(&file.world, w);
  Check(ic::ReadWorld(&file.world, &out, 4) && out.frame == 1235 && (file.world.seq & 1u) == 0,
        "the next write after an odd sequence is whole", static_cast<double>(out.frame), 1235);
  Check(offsetof(ic::File, world) == ic::kWorldOffset &&
            ic::kWorldOffset == ic::kKeysOffset + ic::kKeys * sizeof(ic::KeyEvent),
        "the slot of the place is after the keys", static_cast<double>(offsetof(ic::File, world)),
        2304);
  Check(ic::HeadOk(&file, ic::kFileBytes), "the head of the file stays whole: version 1",
        file.head.version, 1);
}

inline void ViewChecks() {
  printf("\nThe motion of the stick: the place goes with the picture of its frame\n");
  namespace vl = view_layer_math;
  vl::PictureView view;
  scene_stream::ViewBody v = {};
  ic::World w = {};
  w.flags = ic::kWorldValid;
  w.frame = 10;
  view.OnFrame();
  v.frame = 10;
  view.OnView(v);
  view.OnWorld(w);
  view.OnWalkEnd(true);
  ic::World got = {};
  Check(view.GetWorld(&got) && got.frame == 10, "the picture of frame 10 has its place",
        static_cast<double>(got.frame), 10);
  // A frame whose kView has no place: its picture has none.
  view.OnFrame();
  v.frame = 11;
  view.OnView(v);
  view.OnWalkEnd(true);
  Check(!view.GetWorld(&got), "a kView with no place: the picture has none", 0, 0);
  // A place with no kView before it does not count.
  view.OnFrame();
  w.frame = 12;
  view.OnWorld(w);
  view.OnWalkEnd(true);
  Check(!view.GetWorld(&got), "a place with no kView: none", 0, 0);
}

inline void AgeChecks() {
  printf("\nThe parts of the age of a new picture\n");
  frame_loop::AgeTimes t;
  t.input = 1000.0;
  t.arrival = 1030.0;
  t.play = 1031.0;
  t.ready = 1036.0;
  t.taken = 1052.0;
  t.ended = 1055.0;
  t.display = 1085.0;
  double parts[frame_loop::kAgeParts];
  const double all = frame_loop::AgeParts(t, parts);
  Near("the age from the input to the display time, ms", all, 85.0, 1e-9);
  double sum = 0.0;
  for (double p : parts) sum += p;
  Near("the parts add up to the age", sum, 85.0, 1e-9);
  Near("the game: the input to the arrival", parts[0], 30.0, 1e-9);
  Near("the ring: the copies to the take", parts[3], 16.0, 1e-9);
  Near("the runtime: xrEndFrame to the display time", parts[5], 30.0, 1e-9);
  t.input = 0.0;
  const double none = frame_loop::AgeParts(t, parts);
  Check(none < 0.0 && parts[0] < 0.0 && parts[1] == 1.0,
        "with no time of the input: no age and no part of the game, the rest stay", none, -1);
}

inline void MotionCompChecks() {
  MapChecks();
  DifferenceChecks();
  PredictChecks();
  SlotChecks();
  ViewChecks();
  AgeChecks();
}

}  // namespace motion_comp_checks
