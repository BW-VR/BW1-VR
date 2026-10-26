// THE MOTION OF THE STICK IN THE HEADSET APP (motion_comp, docs/android.md,
// "The motion of the stick"). No Windows code, so the mod, the headset app
// and the math test use it (test/controller_math/motion_comp_checks.h).
//
// THE MAP of a frame of the mod: the place of the player in the world. It
// takes a point of the LOCAL space of the session (meters) to the world of
// the game (game units), with the numbers of the main camera call of the
// frame (head_camera.cpp, MakeMap):
//
//   world = camera + units * L * F * R(origin)^-1 * (local - origin)
//
//   camera  the camera of the main call with no head: the recenter pose in
//           the world in the God view and on the table, else the camera of
//           the game
//   L       the levelled basis of the room, columns right, up and forward
//   F       diag(1, 1, -1): the right handed LOCAL space to the left handed
//           world
//   origin  the recenter pose of head_pose.cpp in the LOCAL space
//   units   game units for a meter of the room (the lean gain of the main
//           call times HeadPoseScale)
//
// The camera of the frame is this map of the head, so the eye pictures of a
// frame show the world as the map of that frame puts it in the room.
//
// THE DIFFERENCE. A picture was drawn with the map of its frame (drawn), and
// the player is now at the map now. A point of the LOCAL space of the picture
// is at now^-1(drawn(point)) in the LOCAL space now. With no motion of the
// player between the two frames there is no difference. A step of the stick
// moves each point by the opposite step in the room, and a turn of the view
// about the head turns each point about the head.
//
// THE POSE OF A VIEW of the projection layer (MovePose). The eye of the
// picture goes to the place of the difference, and its orientation turns by
// the turn of the difference. A runtime with no depth of the picture turns a
// projection layer to the head and does not move it: a picture is as far as
// the horizon for it. So for a step of the stick the orientation also turns
// so that the point at depth_m in front of the eye shows in its direction now
// (0: no such turn).
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../common/input_channel.h"
#include "walk_math.h"

namespace motion_comp_math {

// motion_comp of bwvr.ini: 0 the old way (each view with the pose of its
// picture), 1 the difference to the newest map of the mod, 2 the same and
// the newest map goes on with the walk of the stick to the display time.
constexpr int kOff = 0;
constexpr int kNewest = 1;
constexpr int kPredict = 2;
inline int Mode(int key) { return key <= kOff ? kOff : (key >= kPredict ? kPredict : key); }

// world = linear * local + offset. linear is row major.
struct Map {
  float linear[9];
  float offset[3];
};

inline Map Identity() {
  Map m;
  memset(&m, 0, sizeof(m));
  m.linear[0] = m.linear[4] = m.linear[8] = 1.0f;
  return m;
}

// The rotation of a quaternion x y z w as a matrix: m * v = q v q^-1.
inline void QuatMatrix(const float q[4], float m[9]) {
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  m[0] = 1.0f - 2.0f * (y * y + z * z);
  m[1] = 2.0f * (x * y - w * z);
  m[2] = 2.0f * (x * z + w * y);
  m[3] = 2.0f * (x * y + w * z);
  m[4] = 1.0f - 2.0f * (x * x + z * z);
  m[5] = 2.0f * (y * z - w * x);
  m[6] = 2.0f * (x * z - w * y);
  m[7] = 2.0f * (y * z + w * x);
  m[8] = 1.0f - 2.0f * (x * x + y * y);
}

// The quaternion of a rotation matrix, with w >= 0 and a length of 1.
inline void MatrixQuat(const float m[9], float q[4]) {
  const float trace = m[0] + m[4] + m[8];
  if (trace > 0.0f) {
    const float s = sqrtf(trace + 1.0f) * 2.0f;
    q[3] = 0.25f * s;
    q[0] = (m[7] - m[5]) / s;
    q[1] = (m[2] - m[6]) / s;
    q[2] = (m[3] - m[1]) / s;
  } else if (m[0] > m[4] && m[0] > m[8]) {
    const float s = sqrtf(1.0f + m[0] - m[4] - m[8]) * 2.0f;
    q[3] = (m[7] - m[5]) / s;
    q[0] = 0.25f * s;
    q[1] = (m[1] + m[3]) / s;
    q[2] = (m[2] + m[6]) / s;
  } else if (m[4] > m[8]) {
    const float s = sqrtf(1.0f + m[4] - m[0] - m[8]) * 2.0f;
    q[3] = (m[2] - m[6]) / s;
    q[0] = (m[1] + m[3]) / s;
    q[1] = 0.25f * s;
    q[2] = (m[5] + m[7]) / s;
  } else {
    const float s = sqrtf(1.0f + m[8] - m[0] - m[4]) * 2.0f;
    q[3] = (m[3] - m[1]) / s;
    q[0] = (m[2] + m[6]) / s;
    q[1] = (m[5] + m[7]) / s;
    q[2] = 0.25f * s;
  }
  float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (q[3] < 0.0f) n = -n;
  if (n == 0.0f) {
    q[0] = q[1] = q[2] = 0.0f;
    q[3] = 1.0f;
    return;
  }
  for (int i = 0; i < 4; ++i) q[i] /= n;
}

// a * b: the rotation b, then a.
inline void QuatMul(const float a[4], const float b[4], float out[4]) {
  const float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
  const float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
  const float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
  const float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
  out[0] = x;
  out[1] = y;
  out[2] = z;
  out[3] = w;
}

inline void Rotate(const float m[9], const float v[3], float out[3]) {
  for (int i = 0; i < 3; ++i) {
    out[i] = m[i * 3 + 0] * v[0] + m[i * 3 + 1] * v[1] + m[i * 3 + 2] * v[2];
  }
}

inline void Point(const Map& m, const float p[3], float out[3]) {
  float r[3];
  Rotate(m.linear, p, r);
  for (int i = 0; i < 3; ++i) out[i] = r[i] + m.offset[i];
}

// THE MAP OF A FRAME (see the top of the file). basis: columns right, up and
// forward, row major (head_camera.cpp, the levelled basis). origin_rotation:
// x y z w.
inline Map MakeMap(const float camera[3], const float basis[9], float units,
                   const float origin[3], const float origin_rotation[4]) {
  float r[9];
  QuatMatrix(origin_rotation, r);
  Map m;
  // linear = units * L * F * R^T: element i j is the sum over k of
  // L[i][k] * F[k] * R[j][k].
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      float sum = 0.0f;
      for (int k = 0; k < 3; ++k) {
        const float f = k == 2 ? -1.0f : 1.0f;
        sum += basis[i * 3 + k] * f * r[j * 3 + k];
      }
      m.linear[i * 3 + j] = units * sum;
    }
  }
  float at[3];
  Rotate(m.linear, origin, at);
  for (int i = 0; i < 3; ++i) m.offset[i] = camera[i] - at[i];
  return m;
}

// a after b: a(b(x)).
inline Map Compose(const Map& a, const Map& b) {
  Map m;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      m.linear[i * 3 + j] = a.linear[i * 3 + 0] * b.linear[0 * 3 + j] +
                            a.linear[i * 3 + 1] * b.linear[1 * 3 + j] +
                            a.linear[i * 3 + 2] * b.linear[2 * 3 + j];
    }
  }
  Point(a, b.offset, m.offset);
  return m;
}

inline float Det(const float m[9]) {
  return m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) +
         m[2] * (m[3] * m[7] - m[4] * m[6]);
}

// False for a map with no inverse.
inline bool Invert(const Map& m, Map* out) {
  const float* a = m.linear;
  const float det = Det(a);
  if (!(fabsf(det) > 1e-12f)) return false;
  const float s = 1.0f / det;
  float* b = out->linear;
  b[0] = (a[4] * a[8] - a[5] * a[7]) * s;
  b[1] = (a[2] * a[7] - a[1] * a[8]) * s;
  b[2] = (a[1] * a[5] - a[2] * a[4]) * s;
  b[3] = (a[5] * a[6] - a[3] * a[8]) * s;
  b[4] = (a[0] * a[8] - a[2] * a[6]) * s;
  b[5] = (a[2] * a[3] - a[0] * a[5]) * s;
  b[6] = (a[3] * a[7] - a[4] * a[6]) * s;
  b[7] = (a[1] * a[6] - a[0] * a[7]) * s;
  b[8] = (a[0] * a[4] - a[1] * a[3]) * s;
  float back[3];
  Rotate(b, m.offset, back);
  for (int i = 0; i < 3; ++i) out->offset[i] = -back[i];
  return true;
}

// THE DIFFERENCE from the map of a picture (drawn) to the map now: a point of
// the LOCAL space of the picture to the LOCAL space now. Two maps with the
// same bytes give the identity, so a player who did not move gets the poses
// of the picture with no change. False when the map now has no inverse.
inline bool Difference(const Map& now, const Map& drawn, Map* out) {
  if (memcmp(&now, &drawn, sizeof(Map)) == 0) {
    *out = Identity();
    return true;
  }
  Map inverse;
  if (!Invert(now, &inverse)) return false;
  *out = Compose(inverse, drawn);
  return true;
}

// The change of the scale of a difference: 1 when the scale of the world did
// not change.
inline float Scale(const Map& d) {
  const float det = Det(d.linear);
  return det > 0.0f ? cbrtf(det) : 0.0f;
}

// The turn of a difference as a quaternion: its linear part with no scale.
inline void Turn(const Map& d, float q[4]) {
  const float s = Scale(d);
  if (!(s > 0.0f)) {
    q[0] = q[1] = q[2] = 0.0f;
    q[3] = 1.0f;
    return;
  }
  float r[9];
  for (int i = 0; i < 9; ++i) r[i] = d.linear[i] / s;
  MatrixQuat(r, q);
}

// THE SIZE OF A DIFFERENCE at a point of the room (the head): the step of
// that point in meters, the angle of the turn in degrees, and the change of
// the scale.
struct Motion {
  float step = 0.0f;
  float turn = 0.0f;
  float scale = 1.0f;
};
inline Motion Measure(const Map& d, const float at[3]) {
  Motion m;
  float moved[3];
  Point(d, at, moved);
  const float dx = moved[0] - at[0], dy = moved[1] - at[1], dz = moved[2] - at[2];
  m.step = sqrtf(dx * dx + dy * dy + dz * dz);
  float q[4];
  Turn(d, q);
  const float w = q[3] > 1.0f ? 1.0f : q[3];
  m.turn = 2.0f * acosf(w) * 57.29578f;
  m.scale = Scale(d);
  return m;
}

// A difference that the app shows: not larger than the limits. A larger one
// is a cut of the view (a teleport, a snap turn, a zoom step, a new place of
// the game camera, a recenter), and the picture shows as the game drew it.
struct Limits {
  float step = 0.5f;     // meters at the head
  float turn = 10.0f;    // degrees
  float scale = 0.25f;   // the change of the scale, as a share of 1
};
inline bool Within(const Motion& m, const Limits& l) {
  return m.step <= l.step && m.turn <= l.turn && fabsf(m.scale - 1.0f) <= l.scale;
}

// No difference that a view can show: the poses of the picture stay as they
// are, to the bit.
inline bool Still(const Motion& m) {
  return m.step < 1e-5f && m.turn < 1e-3f && fabsf(m.scale - 1.0f) < 1e-6f;
}

// THE POSE OF A VIEW with the difference d (see the top of the file). p, q:
// the pose of the eye of the picture in the LOCAL space, q as x y z w. The
// view of the LOCAL space looks along -z.
inline void MovePose(const Map& d, float depth_m, const float p[3], const float q[4],
                     float p_out[3], float q_out[4]) {
  Point(d, p, p_out);
  float turn[4];
  Turn(d, turn);
  float turned[4];
  QuatMul(turn, q, turned);
  if (depth_m > 0.0f) {
    // The point at depth_m in front of the eye of the picture, where it is
    // now, seen from the eye: the runtime puts the picture at the head.
    float m[9];
    QuatMatrix(q, m);
    const float ahead[3] = {-m[2] * depth_m, -m[5] * depth_m, -m[8] * depth_m};
    const float drawn[3] = {p[0] + ahead[0], p[1] + ahead[1], p[2] + ahead[2]};
    float now[3];
    Point(d, drawn, now);
    float want[3] = {now[0] - p[0], now[1] - p[1], now[2] - p[2]};
    float t[9];
    QuatMatrix(turned, t);
    float have[3] = {-t[2], -t[5], -t[8]};
    const float wl = sqrtf(want[0] * want[0] + want[1] * want[1] + want[2] * want[2]);
    if (wl > 1e-6f) {
      for (float& v : want) v /= wl;
      // The shortest turn from have to want.
      const float c[3] = {have[1] * want[2] - have[2] * want[1],
                          have[2] * want[0] - have[0] * want[2],
                          have[0] * want[1] - have[1] * want[0]};
      const float dot = have[0] * want[0] + have[1] * want[1] + have[2] * want[2];
      float swing[4] = {c[0], c[1], c[2], 1.0f + dot};
      const float sl = sqrtf(swing[0] * swing[0] + swing[1] * swing[1] +
                             swing[2] * swing[2] + swing[3] * swing[3]);
      if (dot > -0.99f && sl > 1e-9f) {
        for (float& v : swing) v /= sl;
        QuatMul(swing, turned, q_out);
        float n = sqrtf(q_out[0] * q_out[0] + q_out[1] * q_out[1] + q_out[2] * q_out[2] +
                        q_out[3] * q_out[3]);
        for (int i = 0; i < 4; ++i) q_out[i] /= n;
        return;
      }
    }
  }
  for (int i = 0; i < 4; ++i) q_out[i] = turned[i];
}

// THE PREDICTION (motion_comp=2). The newest map of the mod goes on with the
// walk of its frame to the display time of the frame of the session: the
// velocity of the walk times the push of the stick now over the push of the
// frame, for the time from the display time of the input of the frame to the
// display time now, at most most_ms. A stick in its dead zone now gives no
// prediction, so the view stops in the frame in which the stick comes back.
inline float PredictSeconds(int64_t display_now, int64_t display_map, int most_ms) {
  if (display_now <= 0 || display_map <= 0 || display_now <= display_map) return 0.0f;
  const double s = static_cast<double>(display_now - display_map) / 1e9;
  const double most = most_ms > 0 ? most_ms / 1000.0 : 0.0;
  return static_cast<float>(s < most ? s : most);
}
// The push of a stick now, as the walk of the mod takes it (walk_math::Push).
inline float PushShare(const float stick[2], float dead, float full) {
  float push[2];
  return walk_math::Push(stick[0], stick[1], dead, full, push);
}
inline void Predict(Map* m, const float velocity[3], float share_now, float share_map,
                    float seconds) {
  if (!(share_map > 0.0f) || !(share_now > 0.0f) || !(seconds > 0.0f)) return;
  const float k = share_now / share_map * seconds;
  for (int i = 0; i < 3; ++i) m->offset[i] += velocity[i] * k;
}

// THE MAPS OF THE INPUT CHANNEL AND OF THE STREAM.
inline Map MapOf(const input_channel::World& w) {
  Map m;
  memcpy(m.linear, w.linear, sizeof(m.linear));
  memcpy(m.offset, w.offset, sizeof(m.offset));
  return m;
}
inline bool Valid(const input_channel::World& w) {
  return (w.flags & input_channel::kWorldValid) != 0;
}

// THE NEWEST MAP for the session: the map of the input channel, or the map
// of the newest picture when the channel is behind it (the frame of the
// picture came after the read of the channel) or more than kAhead frames
// ahead of it (a channel of an earlier game). Null when neither has a map.
constexpr uint64_t kAhead = 600;
inline const input_channel::World* Newest(const input_channel::World* channel,
                                          const input_channel::World* picture) {
  const bool c = channel != nullptr && Valid(*channel);
  const bool p = picture != nullptr && Valid(*picture);
  if (!p) return c ? channel : nullptr;
  if (!c) return picture;
  if (channel->frame < picture->frame || channel->frame > picture->frame + kAhead) {
    return picture;
  }
  return channel;
}

// THE MAP NOW: the newest map, and with motion_comp=2 the prediction of the
// walk of its frame. sticks: the sticks of the hands now (x right, y
// forward). display_now: the display time of the frame of the session, ns.
// *ahead_s: the seconds of the prediction, 0 with none.
inline Map Now(const input_channel::World& newest, int mode, const float sticks[2][2],
               int64_t display_now, int most_ms, float* ahead_s) {
  Map m = MapOf(newest);
  *ahead_s = 0.0f;
  if (mode < kPredict || (newest.flags & input_channel::kWorldWalk) == 0 ||
      newest.walk_hand < 0 || newest.walk_hand > 1) {
    return m;
  }
  const float share = PushShare(sticks[newest.walk_hand], newest.walk_dead, newest.walk_full);
  const float seconds = PredictSeconds(display_now, newest.display_time, most_ms);
  if (!(share > 0.0f) || !(seconds > 0.0f)) return m;
  Predict(&m, newest.walk_velocity, share, newest.walk_share, seconds);
  *ahead_s = seconds;
  return m;
}

}  // namespace motion_comp_math
