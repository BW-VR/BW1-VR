// THE CHECK OF THE VIEW (docs/controller-hints.md, "The place of the
// controller"). main.cpp includes this file after Check and Near, just
// before main.
//
// The whole chain of the God view, from the poses of the runtime to the
// pixels of the eye pictures, with the real head_pose.cpp and
// controller_math.h, and the steps of head_camera.cpp, eye_camera.cpp,
// LH3DTech::UpdateWorldToCamera, LH3DTech::ChangeFov and one walk written out
// as those files do them. A grip point must land on the pixel that the
// declared view of each eye gives it, and the two eyes must see it at the
// grip pose. A planted error of the eye separation or of the place of the
// eyes must show in the check, with the size that the geometry gives.
#pragma once

#include "../../src/mod/eye_cant_math.h"
#include "../../src/mod/one_walk_math.h"
#include "../../src/mod/view_check_math.h"

// head_pose.cpp
void HeadPoseSetHead(const float center[3]);
void HeadPoseRecenter();

namespace view_check_checks {

// One case: the God view, the player and the render.
struct Case {
  float origin[3];      // P0, the recenter pose in the world
  float heading;        // degrees: the room forward in the world
  float scale;          // units per meter
  float recenter[3];    // the head center at the recenter, LOCAL
  float recenter_yaw;   // degrees
  float head[3];        // the head center now, LOCAL
  float yaw, pitch, roll;  // degrees, the head now
  float ipd;            // meters
  float grip[3];        // LOCAL
  float half_deg;       // the horizontal half angle of the render
  float width, height;  // the eye picture
  // Planted errors, 0 in the true chain.
  float separation_error;  // the render separation is (1 + this) times
  float eye_forward_m;     // the render eyes move this far forward
};

inline void Cross(const float a[3], const float b[3], float out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

inline float Dot(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline void Unit(float v[3]) {
  const float n = sqrtf(Dot(v, v));
  for (int i = 0; i < 3; ++i) v[i] /= n;
}

// Row major 3x3: out = a * b.
inline void MatMul(const float a[9], const float b[9], float out[9]) {
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) {
      out[r * 3 + c] = a[r * 3 + 0] * b[0 * 3 + c] +
                       a[r * 3 + 1] * b[1 * 3 + c] +
                       a[r * 3 + 2] * b[2 * 3 + c];
    }
  }
}

// The orientation of a head of a neck: yaw, then pitch, then roll, in the
// frame of the runtime, as the invented head of head_pose.cpp builds it.
inline void HeadTurn(float yaw, float pitch, float roll, float out[4]) {
  const float y = 0.5f * yaw / eye_cant::kDegrees;
  const float p = 0.5f * pitch / eye_cant::kDegrees;
  const float r = 0.5f * roll / eye_cant::kDegrees;
  const float qy[4] = {0.0f, sinf(y), 0.0f, cosf(y)};
  const float qp[4] = {sinf(p), 0.0f, 0.0f, cosf(p)};
  const float qr[4] = {0.0f, 0.0f, -sinf(r), cosf(r)};
  float yp[4];
  eye_cant::Multiply(qy, qp, yp);
  eye_cant::Multiply(yp, qr, out);
}

// The result of a case.
struct Outcome {
  bool ok;
  view_check::Result result;
  float walk_gap;  // pixels: one walk against a pass of its own for eye 1
};

// The chain of the mod for one case.
inline Outcome Run(const Case& k) {
  Outcome out = {};
  const float kLife = 7.5f;
  HeadPoseSetScale(kLife);

  // THE RECENTER, then the head of this frame (stereo.cpp).
  float q_recenter[4];
  HeadTurn(k.recenter_yaw, 0.0f, 0.0f, q_recenter);
  HeadPoseRecenter();
  HeadPoseSetHead(k.recenter);
  HeadPoseUpdate(k.recenter, q_recenter);
  float q[4];
  HeadTurn(k.yaw, k.pitch, k.roll, q);
  float eye_local[2][3];
  for (int e = 0; e < 2; ++e) {
    const float side[3] = {(e == 0 ? -0.5f : 0.5f) * k.ipd, 0.0f, 0.0f};
    eye_cant::Rotate(q, side, eye_local[e]);
    for (int i = 0; i < 3; ++i) eye_local[e][i] += k.head[i];
  }
  HeadPoseSetHead(k.head);
  HeadPoseUpdate(k.head, q);
  float h[9], lean[3], t_head[3], m_grip[9], t_grip[3];
  const float grip_turn[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  if (!HeadPoseCurrent(h, lean) || !HeadPoseHeadInRoom(t_head) ||
      !HeadPoseToRoom(k.grip, grip_turn, m_grip, t_grip)) {
    return out;
  }

  // THE GOD VIEW: the room basis, the gain, and the grip in the world
  // (controllers.cpp, ControllersOverlayHands).
  const float gain = k.scale / kLife;
  float room[9];
  {
    const float c = cosf(k.heading / eye_cant::kDegrees);
    const float s = sinf(k.heading / eye_cant::kDegrees);
    const float right[3] = {c, 0.0f, -s};
    const float up[3] = {0.0f, 1.0f, 0.0f};
    const float forward[3] = {s, 0.0f, c};
    for (int i = 0; i < 3; ++i) {
      room[i * 3 + 0] = right[i];
      room[i * 3 + 1] = up[i];
      room[i * 3 + 2] = forward[i];
    }
  }
  float lean_w[3], head_w[3], grip_w[3], eye[3], world_grip[3], turn[9];
  controller_math::ScaleRoom(lean, gain, lean_w);
  controller_math::ScaleRoom(t_head, gain, head_w);
  controller_math::ScaleRoom(t_grip, gain, grip_w);
  controller_math::EyeFromLean(k.origin, room, lean_w, eye);
  controller_math::HandToWorld(eye, room, head_w, grip_w, m_grip, world_grip,
                               turn);

  // THE MAIN CAMERA CALL (head_camera.cpp): R = L * H, the lean in the room
  // axes, and the roll that the engine cannot carry.
  float render[9];
  MatMul(room, h, render);
  const float forward[3] = {render[2], render[5], render[8]};
  const float head_up[3] = {render[1], render[4], render[7]};
  float moved[3];
  for (int i = 0; i < 3; ++i) {
    moved[i] = k.origin[i] + room[i * 3 + 0] * lean_w[0] +
               room[i * 3 + 1] * lean_w[1] + room[i * 3 + 2] * lean_w[2] +
               forward[i] * k.eye_forward_m * k.scale;
  }
  const float up_world[3] = {0.0f, 1.0f, 0.0f};
  float engine_right[3], engine_up[3];
  Cross(up_world, forward, engine_right);
  Unit(engine_right);
  Cross(forward, engine_right, engine_up);
  const float roll = atan2f(Dot(head_up, engine_right), Dot(head_up, engine_up));

  // LH3DTech::UpdateWorldToCamera: the side, the up and the look in the
  // columns, and the translation of the position.
  float look[3] = {forward[0], forward[1], forward[2]};
  Unit(look);
  const float along = Dot(up_world, look);
  float up2[3] = {up_world[0] - along * look[0], up_world[1] - along * look[1],
                  up_world[2] - along * look[2]};
  Unit(up2);
  float side[3];
  Cross(up2, look, side);
  float u[12];
  for (int i = 0; i < 3; ++i) {
    u[i * 3 + 0] = side[i];
    u[i * 3 + 1] = up2[i];
    u[i * 3 + 2] = look[i];
  }
  u[9] = -Dot(moved, side);
  u[10] = -Dot(moved, up2);
  u[11] = -Dot(moved, look);

  // LH3DTech::ChangeFov with the square pixels of the render.
  const float half = k.half_deg / eye_cant::kDegrees;
  const float aspect = k.width / k.height;
  const float inv_x = 1.0f / tanf(half);
  const float inv_y = aspect / tanf(half);
  const float hx = 0.5f * k.width, hy = 0.5f * k.height;
  const float near_plane = 0.3f;

  // THE EYE PASSES (stereo.cpp, eye_camera.cpp EyeCameraSetPose): the roll,
  // then the eye on the rolled right axis, then the scale of the columns.
  const float separation =
      controller_math::Distance(eye_local[0], eye_local[1]) * k.scale * (1.0f + k.separation_error);
  float pixel[2][2], depth[2];
  for (int e = 0; e < 2; ++e) {
    const float d = (e == 0 ? -0.5f : 0.5f) * separation;
    const float c = cosf(roll), s = sinf(roll);
    const float rz[9] = {c, s, 0.0f, -s, c, 0.0f, 0.0f, 0.0f, 1.0f};
    float m[12];
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        m[row * 3 + col] = u[row * 3 + 0] * rz[0 * 3 + col] +
                           u[row * 3 + 1] * rz[1 * 3 + col] +
                           u[row * 3 + 2] * rz[2 * 3 + col];
      }
    }
    const float dv[3] = {d, 0.0f, 0.0f};
    for (int col = 0; col < 3; ++col) {
      m[9 + col] = u[9] * rz[0 * 3 + col] + u[10] * rz[1 * 3 + col] +
                   u[11] * rz[2 * 3 + col] - dv[col];
    }
    const float factor[3] = {inv_x, inv_y, 1.0f};
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 3; ++col) m[row * 3 + col] *= factor[col];
    }
    if (!view_check::GamePixel(m, hx, hy, world_grip, pixel[e], &depth[e])) {
      return out;
    }
  }
  // ONE WALK: the right eye from the left eye.
  const float per_rhw =
      one_walk_math::ShiftPerRhw(inv_x, separation, hx, near_plane);
  const float walk_x = pixel[0][0] + per_rhw * near_plane / depth[0];
  out.walk_gap = fabsf(walk_x - pixel[1][0]) + fabsf(pixel[0][1] - pixel[1][1]);

  // THE DECLARED VIEWS (XrScreen::DeclaredView).
  view_check::View view[2];
  const float aspect_half = atanf(tanf(half) * k.height / k.width);
  for (int e = 0; e < 2; ++e) {
    view_check::MakeView(eye_local[e], q, -half, half, aspect_half,
                         -aspect_half, k.width, k.height, &view[e]);
  }
  out.ok = view_check::CheckPoint(view, pixel, k.grip, &out.result);
  return out;
}

inline Case Base() {
  Case k = {};
  k.origin[0] = 1502.1f;
  k.origin[1] = 60.0f;
  k.origin[2] = 1935.6f;
  k.heading = -21.6f;
  k.scale = 100.0f;
  k.recenter[0] = 0.10f;
  k.recenter[1] = 1.62f;
  k.recenter[2] = -0.20f;
  k.recenter_yaw = 15.0f;
  for (int i = 0; i < 3; ++i) k.head[i] = k.recenter[i];
  k.ipd = 0.0633f;
  // 40 cm ahead of the head and 25 cm down, in the frame of the recenter.
  const float yaw = k.recenter_yaw / eye_cant::kDegrees;
  k.grip[0] = k.head[0] - 0.40f * sinf(yaw);
  k.grip[1] = k.head[1] - 0.25f;
  k.grip[2] = k.head[2] - 0.40f * cosf(yaw);
  k.half_deg = 60.75f;
  k.width = 3600.0f;
  k.height = 2880.0f;
  return k;
}

inline void ViewCheckChecks() {
  printf("\nThe check of the view\n");

  // THE MATH OF THE CHECK. A point and its own pixels: no error.
  {
    const float position[3] = {0.0f, 0.0f, 0.0f};
    const float orientation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    view_check::View v;
    view_check::MakeView(position, orientation, -0.9f, 0.7f, 0.8f, -0.95f,
                         2000.0f, 1800.0f, &v);
    const float p[3] = {0.12f, -0.07f, -0.5f};
    float pixel[2], ray[3];
    const bool front = view_check::DeclaredPixel(v, p, pixel);
    view_check::DeclaredRay(v, pixel, ray);
    const float length = sqrtf(Dot(p, p));
    Check(front, "a point ahead is in front of the view", pixel[0], 0.0);
    Near("the ray of its pixel goes through it, x", ray[0] * length, p[0],
         1e-5);
    Near("y", ray[1] * length, p[1], 1e-5);
    Near("z", ray[2] * length, p[2], 1e-5);
    const float behind[3] = {0.0f, 0.0f, 0.5f};
    Check(!view_check::DeclaredPixel(v, behind, pixel),
          "a point behind the view has no pixel", 0.0, 0.0);
  }

  // THE CHAIN, TRUE. Many heads, hands and God views.
  {
    float worst_pixel = 0.0f, worst_along = 0.0f, worst_across = 0.0f,
          worst_walk = 0.0f;
    int cases = 0, failed = 0;
    const float scales[] = {7.5f, 33.3f, 100.0f, 400.0f, 4000.0f};
    const float headings[] = {-21.6f, 0.0f, 137.0f};
    const float looks[][3] = {{0.0f, 0.0f, 0.0f},   {20.0f, -35.0f, 0.0f},
                              {-40.0f, -60.0f, 8.0f}, {10.0f, -20.0f, -15.0f},
                              {75.0f, 5.0f, 3.0f},    {-5.0f, -80.0f, 0.0f}};
    const float leans[][3] = {{0.0f, 0.0f, 0.0f}, {0.12f, -0.05f, -0.20f},
                              {-0.30f, 0.10f, 0.15f}};
    const float hands[][3] = {{0.0f, -0.25f, -0.40f}, {0.25f, -0.40f, -0.30f},
                              {-0.20f, 0.05f, -0.55f}, {0.05f, -0.55f, -0.15f}};
    for (float scale : scales) {
      for (float heading : headings) {
        for (const auto& look : looks) {
          for (const auto& lean_m : leans) {
            for (const auto& hand : hands) {
              Case k = Base();
              k.scale = scale;
              k.heading = heading;
              k.yaw = k.recenter_yaw + look[0];
              k.pitch = look[1];
              k.roll = look[2];
              for (int i = 0; i < 3; ++i) k.head[i] = k.recenter[i] + lean_m[i];
              // The hand in the frame of the head yaw, so it stays in view.
              const float yaw = k.yaw / eye_cant::kDegrees;
              const float pitch = k.pitch / eye_cant::kDegrees;
              const float fwd = -hand[2];
              // Turn the hand with the pitch of the head, then the yaw.
              const float y = hand[1] * cosf(pitch) + fwd * sinf(pitch);
              const float f = fwd * cosf(pitch) - hand[1] * sinf(pitch);
              k.grip[0] = k.head[0] + hand[0] * cosf(yaw) - f * sinf(yaw);
              k.grip[1] = k.head[1] + y;
              k.grip[2] = k.head[2] - hand[0] * sinf(yaw) - f * cosf(yaw);
              const Outcome o = Run(k);
              ++cases;
              if (!o.ok) {
                ++failed;
                continue;
              }
              const view_check::Result& r = o.result;
              for (int e = 0; e < 2; ++e) {
                if (r.pixel_error[e] > worst_pixel) worst_pixel = r.pixel_error[e];
              }
              if (fabsf(r.along) > worst_along) worst_along = fabsf(r.along);
              if (r.across > worst_across) worst_across = r.across;
              if (o.walk_gap > worst_walk) worst_walk = o.walk_gap;
            }
          }
        }
      }
    }
    Check(failed == 0, "each case has the grip in front of both eyes", failed,
          0);
    printf("  (%d cases)\n", cases);
    // The floats of the game: at the life size scale the world is 1500 units
    // from its origin and a hand is 3 units from the eye, so a float of the
    // world is 0.0001 units, and the worst case moves a pixel by 0.8 (0.03
    // degrees) and the depth by 0.3 mm.
    Near("the pixel of the game is the pixel of the declared view, pixels",
         worst_pixel, 0.0, 1.0);
    Near("the two eyes see the grip at its depth, cm", worst_along * 100.0f,
         0.0, 0.05);
    Near("and at its place to the side, cm", worst_across * 100.0f, 0.0, 0.05);
    Near("one walk gives the pixel of a pass of its own, pixels", worst_walk,
         0.0, 0.25);
  }

  // PLANTED ERRORS: the check sees them, with the size of the geometry.
  {
    // A render separation 6 percent too large brings a near point nearer:
    // the angle between the two eyes grows by 6 percent, so the depth is
    // 1 / 1.06 of the true depth: 2.6 cm at 47 cm.
    Case k = Base();
    k.separation_error = 0.06f;
    const Outcome o = Run(k);
    const float distance = o.result.distance;
    Check(o.ok, "the case with 6 percent more separation runs", o.ok, 1.0);
    Near("6 percent more separation shows the grip nearer, cm",
         o.result.along * 100.0f, -distance * (1.0f - 1.0f / 1.06f) * 100.0f,
         0.3);
    Check(o.result.along < -0.02f, "by more than 2 cm at the distance of a "
          "hand", o.result.along * 100.0f, -2.0);
  }
  {
    // Both render eyes 2.5 cm forward: the grip shows 2.5 cm nearer along
    // the view of the head. The grip is 40 cm ahead and 25 cm down, so the
    // line to it is 32 degrees under the view: 2.5 * cos(32) = 2.1 cm along
    // that line.
    Case k = Base();
    k.eye_forward_m = 0.025f;
    const Outcome o = Run(k);
    Check(o.ok, "the case with the eyes 2.5 cm forward runs", o.ok, 1.0);
    Near("eyes 2.5 cm forward show the grip 2.1 cm nearer, cm",
         o.result.along * 100.0f, -2.5 * cos(atan(25.0 / 40.0)), 0.15);
    Check(o.result.pixel_error[0] > 5.0f && o.result.pixel_error[1] > 5.0f,
          "and its pixels move in both eyes", o.result.pixel_error[0], 5.0);
  }
}

}  // namespace view_check_checks
