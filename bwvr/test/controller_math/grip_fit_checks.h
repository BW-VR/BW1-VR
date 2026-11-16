// THE FRAME OF THE CONTROLLER ON THE GRIP POSE, AND THE TRIM OF THE PLAYER
// (docs/controller-hints.md, "9.15 The place of the controller"). main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include <math.h>

#include "../../src/mod/grip_fit_math.h"
#include "../../src/mod/settings_panel_math.h"

namespace grip_fit_checks {

inline float Length3(const float v[3]) {
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// A point of the grip frame of a pose in the LOCAL space.
inline void PointOf(const float position[3], const float orientation[4],
                    const float p[3], float out[3]) {
  float turned[3];
  controller_math::QuatRotate(orientation, p, turned);
  for (int i = 0; i < 3; ++i) out[i] = position[i] + turned[i];
}

inline void GripFitChecks() {
  namespace gf = grip_fit_math;
  printf("\nThe frame of the controller on the grip pose\n");

  // The table: which runtime and which model get a fit.
  Check(gf::FitOf("VirtualDesktopXR", "meta-quest-touch-plus") != nullptr,
        "Virtual Desktop and the Touch Plus have a fit", 1, 1);
  Check(gf::FitOf("Oculus", "oculus-touch-v3") != nullptr,
        "the Oculus runtime and the Touch of the Quest 2 have a fit", 1, 1);
  Check(gf::FitOf("SteamVR/OpenXR", "meta-quest-touch-pro") != nullptr,
        "SteamVR and the Touch Pro have a fit", 1, 1);
  Check(gf::FitOf("Monado", "meta-quest-touch-plus") == nullptr,
        "a runtime that is not in the table has no fit", 0, 0);
  Check(gf::FitOf("", "meta-quest-touch-plus") == nullptr,
        "a runtime with no name has no fit (the bridge, the fake controller)",
        0, 0);
  Check(gf::FitOf("VirtualDesktopXR", "valve-index") == nullptr,
        "a model that is not in the table has no fit", 0, 0);
  Check(gf::FitOf("VirtualDesktopXR", nullptr) == nullptr,
        "a hand with no model has no fit", 0, 0);

  // Each fit: a unit turn of 16 to 18 degrees down about x, and an origin
  // 2.3 to 2.9 cm from the grip point, toward -z.
  for (int i = 0; i < gf::kFitCount; ++i) {
    const gf::Fit& f = gf::kFits[i];
    const float n = sqrtf(f.q[0] * f.q[0] + f.q[1] * f.q[1] +
                          f.q[2] * f.q[2] + f.q[3] * f.q[3]);
    char what[160];
    snprintf(what, sizeof(what), "%s: the turn of the fit is a unit "
             "quaternion", f.model);
    Near(what, n, 1.0, 0.0005);
    snprintf(what, sizeof(what), "%s: the fit turns 16 to 18.5 degrees",
             f.model);
    const float turn = gf::TurnDegrees(f.q);
    Check(turn > 16.0f && turn < 18.5f, what, turn, 17.0);
    snprintf(what, sizeof(what), "%s: the fit turns down about x (q.x < 0)",
             f.model);
    Check(f.q[0] < 0.0f, what, f.q[0], -0.15);
    snprintf(what, sizeof(what), "%s: the origin of the model is 2.3 to 2.9 "
             "cm from the grip point, toward the top (-z)", f.model);
    const float len = Length3(f.offset_m);
    Check(len > 0.023f && len < 0.029f && f.offset_m[2] < -0.02f, what,
          len * 100.0f, 2.6);
  }

  // The mirror: the left hand of the left hand is the right hand, and the
  // fit of the left hand on a mirrored pose is the mirror of the fit of the
  // right hand.
  {
    const gf::Fit& f = gf::kFits[0];
    float qr[4], or_[3], ql[4], ol[3];
    gf::Mirror(f, false, qr, or_);
    gf::Mirror(f, true, ql, ol);
    float position[3] = {0.20f, -0.40f, -0.35f};
    float orientation[4] = {0.30f, 0.20f, -0.10f, 0.92f};
    const float n = sqrtf(0.09f + 0.04f + 0.01f + 0.92f * 0.92f);
    for (int i = 0; i < 4; ++i) orientation[i] /= n;
    float pl[3] = {-position[0], position[1], position[2]};
    float ql_pose[4] = {orientation[0], -orientation[1], -orientation[2],
                        orientation[3]};
    gf::Apply(qr, or_, position, orientation);
    gf::Apply(ql, ol, pl, ql_pose);
    Near("the fit of the left hand is the mirror of the right hand: x",
         pl[0], -position[0], 1e-5);
    Near("the fit of the left hand is the mirror of the right hand: y",
         pl[1], position[1], 1e-5);
    Near("the fit of the left hand is the mirror of the right hand: q.y",
         ql_pose[1], -orientation[1], 1e-5);
    Near("the fit of the left hand is the mirror of the right hand: q.w",
         ql_pose[3], orientation[3], 1e-5);
  }

  // A normal hold of the right hand: the aim level and ahead, so the grip
  // pose is the aim pose turned 60 degrees up about x. The real middle of
  // the controller is kTouchMiddle in the grip frame of the runtime. The
  // model draws its point of that middle at the grip pose with no fit, and
  // at the frame of the fit with the fit.
  {
    const float half = 30.0f * 3.14159265f / 180.0f;
    const float grip_q[4] = {sinf(half), 0.0f, 0.0f, cosf(half)};
    const float grip_p[3] = {0.20f, -0.45f, -0.35f};
    float q[4], offset[3];
    gf::Mirror(gf::kFits[0], false, q, offset);
    float real[3];
    PointOf(grip_p, grip_q, gf::kTouchMiddle, real);
    float model_point[3];
    gf::ModelPointOf(q, offset, gf::kTouchMiddle, model_point);
    float plain[3];
    PointOf(grip_p, grip_q, model_point, plain);
    float fit_p[3] = {grip_p[0], grip_p[1], grip_p[2]};
    float fit_q[4] = {grip_q[0], grip_q[1], grip_q[2], grip_q[3]};
    gf::Apply(q, offset, fit_p, fit_q);
    float fitted[3];
    PointOf(fit_p, fit_q, model_point, fitted);
    const float off_plain[3] = {plain[0] - real[0], plain[1] - real[1],
                                plain[2] - real[2]};
    Near("with the fit the model draws the middle of the Touch Plus at the "
         "real middle, cm",
         controller_math::Distance(fitted, real) * 100.0f, 0.0, 0.01);
    Near("with no fit the drawn middle is 2.7 cm from the real middle, cm",
         Length3(off_plain) * 100.0f, 2.72, 0.1);
    Check(off_plain[2] > 0.018f,
          "with no fit and a level aim, the drawn middle is 1.8 cm or more "
          "back toward the player (+z), cm",
          off_plain[2] * 100.0f, 2.1);
    Near("MiddleMove gives the same 2.7 cm for the log, cm",
         gf::MiddleMove(q, offset, false) * 100.0f, 2.72, 0.1);
    float ql[4], ol[3];
    gf::Mirror(gf::kFits[0], true, ql, ol);
    Near("MiddleMove of the left hand is the same, cm",
         gf::MiddleMove(ql, ol, true) * 100.0f,
         gf::MiddleMove(q, offset, false) * 100.0f, 0.001);
  }

  // The trim: along the axes of the aim pose, out mirrored for the left
  // hand, and inside 30 mm.
  {
    const float aim[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    const gf::Trim t = {10, 5, 15};
    float right[3], left[3];
    gf::TrimMove(t, false, aim, right);
    gf::TrimMove(t, true, aim, left);
    Near("a trim of 10 mm ahead moves along -z of the aim, m", right[2],
         -0.010, 1e-6);
    Near("a trim of 5 mm up moves along +y of the aim, m", right[1], 0.005,
         1e-6);
    Near("a trim of 15 mm out moves the right hand to +x, m", right[0], 0.015,
         1e-6);
    Near("a trim of 15 mm out moves the left hand to -x, m", left[0], -0.015,
         1e-6);
    const float half = 45.0f * 3.14159265f / 180.0f;
    const float yawed[4] = {0.0f, sinf(half), 0.0f, cosf(half)};
    const gf::Trim ahead = {20, 0, 0};
    float moved[3];
    gf::TrimMove(ahead, false, yawed, moved);
    Near("a trim ahead follows an aim that turned 90 degrees to the left, x",
         moved[0], -0.020, 1e-5);
    Check(gf::ClampTrim(45) == 30 && gf::ClampTrim(-31) == -30 &&
              gf::ClampTrim(15) == 15,
          "a trim stays inside -30 to 30 mm", gf::ClampTrim(45), 30);
    Check(gf::TrimZero({0, 0, 0}) && !gf::TrimZero({0, 5, 0}),
          "a trim of 0 on each axis does not move the hands", 1, 1);
  }

  // The rows of the settings window: -30 to 30 mm, 0 by default, a step of
  // 5 mm.
  {
    using namespace settings_math;
    const wchar_t* v = kTable[kGripAhead].values;
    Check(ValueCount(v) == 13, "a trim row has 13 values", ValueCount(v), 13);
    Check(IndexOfValue(v, -30) == 0 && IndexOfValue(v, 0) == 6 &&
              IndexOfValue(v, 30) == 12,
          "a trim row goes from -30 to 30 mm with 0 in the middle",
          IndexOfValue(v, 0), 6);
    int next = 0;
    ValueAt(v, StepIndex(IndexOfValue(v, 0), ValueCount(v), false), nullptr,
            0, &next);
    Check(next == gf::kTrimStepMm, "a step of a trim row is 5 mm", next, 5);
    Check(strcmp(kTable[kGripAhead].key, "grip_trim_ahead_mm") == 0 &&
              strcmp(kTable[kGripUp].key, "grip_trim_up_mm") == 0 &&
              strcmp(kTable[kGripOut].key, "grip_trim_out_mm") == 0,
          "the trim rows write the keys of grip_fit.cpp", 1, 1);
  }
}

}  // namespace grip_fit_checks
