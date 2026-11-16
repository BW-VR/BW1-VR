// THE ANGLE OF THE HAND ON THE CONTROLLER (docs/hand.md, "The angle of the
// hand"). main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <math.h>

#include "../../src/mod/grip_fit_math.h"
#include "../../src/mod/hand_angle_math.h"

namespace hand_angle_checks {

namespace ha = hand_angle_math;
namespace gf = grip_fit_math;

// The pose of b in the frame of a: a^-1 * b.
inline void InFrame(const float a[4], const float b[4], float out[4]) {
  const float back[4] = {-a[0], -a[1], -a[2], a[3]};
  controller_math::QuatMultiply(back, b, out);
}

inline void HandAngleChecks() {
  printf("\nThe angle of the hand on the controller\n");

  // THE PALM POSE OF THE OCULUS RUNTIME, from the constants of Virtual
  // Desktop (rebindControllerActions, oculusGripPose and oculusPalmPose, in
  // the same space). The palm pose is the grip pose turned 70 degrees about
  // x, so its fingers (-z) are 20 degrees from -y of the grip, toward -z.
  const float grip[4] = {-0.406630576f, -0.480359077f, -0.434706628f,
                         0.644155979f};
  const float palm[4] = {-0.702564955f, -0.144149646f, -0.631613493f,
                         0.294427931f};
  float palm_in_grip[4];
  InFrame(grip, palm, palm_in_grip);
  float axis[3];
  const float identity[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  const float palm_turn = ha::TurnBetween(identity, palm_in_grip, axis);
  Near("the palm pose of the Oculus runtime is the grip pose turned 70 "
       "degrees", palm_turn, 70.0, 0.05);
  Near("that turn is about -x of the grip", axis[0], -1.0, 0.001);
  const float minus_z[3] = {0.0f, 0.0f, -1.0f};
  float real_fingers[3], real_palm[3];
  controller_math::QuatRotate(palm_in_grip, minus_z, real_fingers);
  ha::PalmAxis(palm_in_grip, real_palm);
  const float minus_y[3] = {0.0f, -1.0f, 0.0f};
  Near("the fingers of the palm pose are 20 degrees from -y of the grip",
       ha::AngleOf(real_fingers, minus_y), ha::kPalmTurnDeg, 0.05);
  Check(real_fingers[2] < -0.3f,
        "they lean toward -z, the thumb end of the handle", real_fingers[2],
        -0.34);
  const float plus_x[3] = {1.0f, 0.0f, 0.0f};
  Near("the palm normal of the palm pose is x of the grip",
       ha::AngleOf(real_palm, plus_x), 0.0, 0.05);

  // THE FIT of the Touch Plus, for each hand: the turn of the frame of the
  // model from the grip pose, and the angle of the fingers on that frame
  // against the fingers of the palm pose.
  for (int side = 0; side < 2; ++side) {
    const bool left = side == 0;
    const char* hand = left ? "left" : "right";
    float fit[4], offset[3];
    gf::Mirror(gf::kFits[0], left, fit, offset);
    float fit_axis[3];
    const float fit_turn = ha::TurnBetween(identity, fit, fit_axis);
    char what[200];
    snprintf(what, sizeof(what), "%s hand: the fit turns the frame %.1f "
             "degrees, about the axis %.3f %.3f %.3f of the grip", hand,
             fit_turn, fit_axis[0], fit_axis[1], fit_axis[2]);
    Near(what, fit_turn, 17.98, 0.05);
    snprintf(what, sizeof(what), "%s hand: the axis of the fit is within 7 "
             "degrees of -x, the palm normal", hand);
    const float minus_x[3] = {-1.0f, 0.0f, 0.0f};
    Check(ha::AngleOf(fit_axis, minus_x) < 7.0f, what,
          ha::AngleOf(fit_axis, minus_x), 6.2);
    // The parts of that turn about the three axes of the grip (a rotation
    // vector): -17.9 degrees about x, 0.1 about y and 1.9 about z on the
    // right hand.
    const float about_x = fit_turn * fit_axis[0];
    const float about_z = fit_turn * fit_axis[2];
    snprintf(what, sizeof(what), "%s hand: the fit is -17.9 degrees about x "
             "(the fingers toward +z, the bottom of the handle)", hand);
    Near(what, about_x, -17.9, 0.1);
    snprintf(what, sizeof(what), "%s hand: and %.1f degrees about z, the "
             "handle", hand, about_z);
    Near(what, fabsf(about_z), 1.95, 0.1);

    // The old angle: the hand on the frame of the model.
    float runtime[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float frame[4];
    controller_math::QuatMultiply(runtime, fit, frame);
    float old_fingers[3];
    ha::Fingers(frame, old_fingers);
    snprintf(what, sizeof(what), "%s hand: on the frame of the model the "
             "fingers are 37.9 degrees from the fingers of the palm pose",
             hand);
    Near(what, ha::AngleOf(old_fingers, real_fingers), 37.9, 0.1);
    float no_fit[3];
    ha::Fingers(runtime, no_fit);
    snprintf(what, sizeof(what), "%s hand: on the grip pose of the runtime "
             "they are 20 degrees from them", hand);
    Near(what, ha::AngleOf(no_fit, real_fingers), 20.0, 0.05);

    // hand_angle=1: the palm pose, with no trim.
    const ha::Trim zero = {0, 0, 0};
    float now[4];
    ha::HandOrientation(runtime, frame, true, zero, left, now);
    float fingers[3], palm_axis[3];
    ha::Fingers(now, fingers);
    ha::PalmAxis(now, palm_axis);
    snprintf(what, sizeof(what), "%s hand: hand_angle=1 puts the fingers on "
             "the fingers of the palm pose", hand);
    Near(what, ha::AngleOf(fingers, real_fingers), 0.0, 0.05);
    snprintf(what, sizeof(what), "%s hand: and the palm normal on the palm "
             "normal of the palm pose", hand);
    Near(what, ha::AngleOf(palm_axis, real_palm), 0.0, 0.05);
    float frame_palm[3];
    ha::PalmAxis(frame, frame_palm);
    snprintf(what, sizeof(what), "%s hand: the palm normal is within 2 "
             "degrees of the palm normal of the frame of the model (the look "
             "at the palm does not change)", hand);
    Check(ha::AngleOf(palm_axis, frame_palm) < 2.0f, what,
          ha::AngleOf(palm_axis, frame_palm), 1.9);
    float turn_axis[3];
    const float turn = ha::TurnBetween(frame, now, turn_axis);
    snprintf(what, sizeof(what), "%s hand: the hand turns %.1f degrees from "
             "the frame of the model, about %.3f %.3f %.3f", hand, turn,
             turn_axis[0], turn_axis[1], turn_axis[2]);
    Near(what, turn, 37.9, 0.15);
    snprintf(what, sizeof(what), "%s hand: that turn is about +x, the palm "
             "normal, within 4 degrees", hand);
    Check(ha::AngleOf(turn_axis, plus_x) < 4.0f, what,
          ha::AngleOf(turn_axis, plus_x), 3.0);

    // hand_angle=0 with no trim is the frame of the model, exactly.
    float old_now[4];
    ha::HandOrientation(runtime, frame, false, zero, left, old_now);
    snprintf(what, sizeof(what), "%s hand: hand_angle=0 is the frame of the "
             "model", hand);
    Near(what, ha::TurnBetween(frame, old_now, turn_axis), 0.0, 0.05);

    // The trims, on the hand frame of the palm pose. A trim of 10 degrees
    // turns the hand 10 degrees, the plus way of the doc.
    const ha::Trim yaw = {10, 0, 0}, pitch = {0, 10, 0}, roll = {0, 0, 10};
    float q[4], v[3], p[3];
    ha::HandOrientation(runtime, frame, true, yaw, left, q);
    ha::Fingers(q, v);
    ha::PalmAxis(q, p);
    snprintf(what, sizeof(what), "%s hand: a yaw of 10 turns the fingers 10 "
             "degrees", hand);
    Near(what, ha::AngleOf(v, fingers), 10.0, 0.05);
    snprintf(what, sizeof(what), "%s hand: toward the thumb (-z)", hand);
    Check(v[2] < fingers[2], what, v[2], fingers[2] - 0.15);
    snprintf(what, sizeof(what), "%s hand: and keeps the palm normal", hand);
    Near(what, ha::AngleOf(p, palm_axis), 0.0, 0.05);

    ha::HandOrientation(runtime, frame, true, pitch, left, q);
    ha::Fingers(q, v);
    snprintf(what, sizeof(what), "%s hand: a pitch of 10 turns the fingers "
             "10 degrees", hand);
    Near(what, ha::AngleOf(v, fingers), 10.0, 0.05);
    // The palm of the right hand faces -x, the palm of the left hand +x.
    const float toward_palm = left ? v[0] : -v[0];
    snprintf(what, sizeof(what), "%s hand: toward the palm", hand);
    Check(toward_palm > 0.15f, what, toward_palm, 0.17);

    ha::HandOrientation(runtime, frame, true, roll, left, q);
    ha::Fingers(q, v);
    ha::PalmAxis(q, p);
    const float palm_out[3] = {left ? palm_axis[0] : -palm_axis[0],
                               left ? palm_axis[1] : -palm_axis[1],
                               left ? palm_axis[2] : -palm_axis[2]};
    const float p_out[3] = {left ? p[0] : -p[0], left ? p[1] : -p[1],
                            left ? p[2] : -p[2]};
    snprintf(what, sizeof(what), "%s hand: a roll of 10 keeps the fingers",
             hand);
    Near(what, ha::AngleOf(v, fingers), 0.0, 0.05);
    snprintf(what, sizeof(what), "%s hand: and turns the palm 10 degrees",
             hand);
    Near(what, ha::AngleOf(p_out, palm_out), 10.0, 0.05);
    snprintf(what, sizeof(what), "%s hand: toward the thumb (-z)", hand);
    Check(p_out[2] < palm_out[2] - 0.1f, what, p_out[2], palm_out[2] - 0.17);
  }

  // THE PLACE. The hand turns about the grip point of the frame of the
  // model. On a hand of 19 cm the knuckle of the middle finger
  // (overlay_math.h: 0.13, -0.03, 0.05 of the hand frame) moves less than 1
  // cm, so the hand stays at the handle.
  {
    float fit[4], offset[3];
    gf::Mirror(gf::kFits[0], false, fit, offset);
    const float runtime[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    const ha::Trim zero = {0, 0, 0};
    float now[4];
    ha::HandOrientation(runtime, fit, true, zero, false, now);
    const float knuckle[3] = {0.13f * 0.19f, -0.03f * 0.19f, -0.05f * 0.19f};
    float a[3], b[3];
    controller_math::QuatRotate(fit, knuckle, a);
    controller_math::QuatRotate(now, knuckle, b);
    const float move = controller_math::Distance(a, b);
    Check(move < 0.01f, "the knuckle of the middle finger moves less than 1 "
          "cm with the new angle", move * 100.0f, 0.7);
  }

  // The trim keys: the clamp and the step.
  Check(ha::ClampTrim(60) == 45 && ha::ClampTrim(-60) == -45 &&
            ha::ClampTrim(15) == 15,
        "a trim stays from -45 to 45 degrees", ha::ClampTrim(60), 45);
  Check(ha::kTrimStepDeg == 5, "a step of a trim is 5 degrees",
        ha::kTrimStepDeg, 5);
}

}  // namespace hand_angle_checks
