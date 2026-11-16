// The frame of the controller on the grip pose, and the trim of the player,
// as pure functions with no game state. grip_fit.cpp calls these with the
// poses of the runtime, and test/controller_math calls them with poses that
// it makes up (grip_fit_checks.h). See docs/controller-hints.md, "9.15 The
// place of the controller".
//
// A pose is in the frame OpenXR uses: x right, y up and -z forward. A
// quaternion is x y z w.
//
// THE FIT. The mod draws the model of a controller (the WebXR Input
// Profiles) and the hand on one frame, and the Hand of the game takes its
// point from the same frame. The origin of a WebXR model is the grip space of
// the browser of the Quest. The grip pose of Virtual Desktop, of the Oculus
// runtime and of SteamVR is a different frame: Virtual Desktop copies the
// grip pose of the Oculus runtime (the constants in action.cpp of
// VirtualDesktop-OpenXR), and SteamVR puts its grip pose at the component
// "openxr_grip" of its own model of each Quest controller. The same aim pose
// is 60 degrees down from the grip pose and 9.96 cm from it in all three.
//
// tools/grip_fit.py fits the model of SteamVR, in its openxr_grip frame, on
// the WebXR model of the same controller (points on the surfaces, the
// nearest points, about 1.2 to 1.8 mm apart after the fit). A fit gives the
// frame of the WebXR model in the grip frame of the runtime. For each Meta
// Touch controller that frame is 16 to 18 degrees down about x, and its
// origin is 2.1 to 2.7 cm along -z (toward the top of the controller). With
// no fit the drawn controller and the hand are 2.7 cm too far toward the
// bottom of the handle at the middle of the controller: back toward the
// player in a normal hold.

#pragma once

#include <math.h>
#include <string.h>

#include "controller_math.h"

namespace grip_fit_math {

// The frame of a WebXR model in the grip frame of the runtime, for the right
// hand: a point p of the model is at q * p + offset in the grip frame. The
// left hand has the mirror image (Mirror).
struct Fit {
  const char* model;   // the folder of the WebXR model
  float q[4];          // x y z w
  float offset_m[3];
};

// From tools/grip_fit.py, the right hand of each model against the SteamVR
// model of the same controller.
const Fit kFits[] = {
    {"meta-quest-touch-plus", {-0.15558f, 0.00106f, 0.01684f, 0.98768f},
     {0.0032f, -0.0109f, -0.0262f}},
    {"meta-quest-touch-pro", {-0.13963f, -0.00104f, -0.00175f, 0.99020f},
     {0.0035f, -0.0094f, -0.0265f}},
    {"oculus-touch-v3", {-0.14289f, -0.00767f, -0.01472f, 0.98960f},
     {-0.0004f, -0.0103f, -0.0231f}},
    {"oculus-touch-v2", {-0.15524f, -0.00174f, -0.00400f, 0.98787f},
     {-0.0002f, -0.0122f, -0.0211f}},
};
const int kFitCount = static_cast<int>(sizeof(kFits) / sizeof(kFits[0]));

// The runtimes with the grip pose of the Oculus runtime: the names that
// xrGetInstanceProperties gives. A name starts with one of these.
const char* const kOculusGripRuntimes[] = {"VirtualDesktopXR", "Oculus",
                                           "SteamVR"};

inline bool StartsWith(const char* text, const char* start) {
  return text != nullptr && start != nullptr &&
         strncmp(text, start, strlen(start)) == 0;
}

inline bool OculusGripRuntime(const char* runtime) {
  for (const char* name : kOculusGripRuntimes) {
    if (StartsWith(runtime, name)) return true;
  }
  return false;
}

// The fit of a model on a runtime, or null: the grip pose is the frame of
// the model then.
inline const Fit* FitOf(const char* runtime, const char* model) {
  if (model == nullptr || !OculusGripRuntime(runtime)) return nullptr;
  for (int i = 0; i < kFitCount; ++i) {
    if (strcmp(kFits[i].model, model) == 0) return &kFits[i];
  }
  return nullptr;
}

// The fit of a hand: the right hand as the table has it, the left hand as
// its mirror image across the plane x = 0, as Virtual Desktop mirrors its
// poses.
inline void Mirror(const Fit& fit, bool left, float q[4], float offset[3]) {
  for (int i = 0; i < 4; ++i) q[i] = fit.q[i];
  for (int i = 0; i < 3; ++i) offset[i] = fit.offset_m[i];
  if (left) {
    q[1] = -q[1];
    q[2] = -q[2];
    offset[0] = -offset[0];
  }
  const float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (n > 1e-6f) {
    for (int i = 0; i < 4; ++i) q[i] /= n;
  }
}

// The pose of the frame of the model from the grip pose of the runtime:
// position + orientation * offset, and orientation * q.
inline void Apply(const float q[4], const float offset[3], float position[3],
                  float orientation[4]) {
  float moved[3];
  controller_math::QuatRotate(orientation, offset, moved);
  for (int i = 0; i < 3; ++i) position[i] += moved[i];
  float turned[4];
  controller_math::QuatMultiply(orientation, q, turned);
  for (int i = 0; i < 4; ++i) orientation[i] = turned[i];
}

// The angle of the turn of a fit, in degrees.
inline float TurnDegrees(const float q[4]) {
  float w = fabsf(q[3]);
  if (w > 1.0f) w = 1.0f;
  return 2.0f * acosf(w) * 57.2957795f;
}

// How far the fit moves a point p of the controller (in the grip frame):
// the distance from p to q * p + offset.
inline float MoveOf(const float q[4], const float offset[3],
                    const float p[3]) {
  float turned[3];
  controller_math::QuatRotate(q, p, turned);
  const float to[3] = {turned[0] + offset[0], turned[1] + offset[1],
                       turned[2] + offset[2]};
  return controller_math::Distance(p, to);
}

// The middle of a Touch controller in its grip frame, for the log: the mean
// of the points on the surface of the SteamVR model of the Touch Plus.
const float kTouchMiddle[3] = {0.0041f, -0.0149f, -0.0241f};

// The point of the frame of the model that a point p of the grip frame is
// at: q^-1 * (p - offset).
inline void ModelPointOf(const float q[4], const float offset[3],
                         const float p[3], float out[3]) {
  const float back[4] = {-q[0], -q[1], -q[2], q[3]};
  const float d[3] = {p[0] - offset[0], p[1] - offset[1], p[2] - offset[2]};
  controller_math::QuatRotate(back, d, out);
}

// How far the fit moves the drawn middle of the controller: from its place
// on the grip pose to its place on the frame of the fit.
inline float MiddleMove(const float q[4], const float offset[3], bool left) {
  float middle[3] = {kTouchMiddle[0], kTouchMiddle[1], kTouchMiddle[2]};
  if (left) middle[0] = -middle[0];
  float model[3];
  ModelPointOf(q, offset, middle, model);
  return MoveOf(q, offset, model);
}

// THE TRIM of the player: a move of the whole controller in steps of 5 mm,
// in the frame of the aim pose of the runtime. Ahead is where the controller
// points, up is the top of the controller, and out is away from the other
// hand (+x on the right hand, -x on the left hand). The trim moves the grip
// pose and the aim pose, so the drawn controller, the hand, the Hand of the
// game and the ray move together.
const int kTrimStepMm = 5;
const int kTrimLimitMm = 30;

inline int ClampTrim(int mm) {
  if (mm > kTrimLimitMm) return kTrimLimitMm;
  if (mm < -kTrimLimitMm) return -kTrimLimitMm;
  return mm;
}

struct Trim {
  int ahead_mm;
  int up_mm;
  int out_mm;
};

inline bool TrimZero(const Trim& t) {
  return t.ahead_mm == 0 && t.up_mm == 0 && t.out_mm == 0;
}

// The move of the trim in the LOCAL space, from the orientation of the aim
// pose.
inline void TrimMove(const Trim& t, bool left, const float aim_q[4],
                     float out[3]) {
  const float local[3] = {
      (left ? -1.0f : 1.0f) * static_cast<float>(t.out_mm) * 0.001f,
      static_cast<float>(t.up_mm) * 0.001f,
      -static_cast<float>(t.ahead_mm) * 0.001f};
  controller_math::QuatRotate(aim_q, local, out);
}

}  // namespace grip_fit_math
