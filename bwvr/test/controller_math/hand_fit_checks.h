// THE FIT OF THE HAND ON THE CONTROLLER AND THE CURL OF THE TRACKED FINGERS
// (docs/hand-model.md, "The fit of the hand on the controller" and "Finger
// tracking"). main.cpp includes this file after Check and Near, just before
// main.
#pragma once

#include "../../src/mod/hand_fit_math.h"
#include "../../src/vr/finger_curl_math.h"

namespace hand_fit_checks {

inline float Length3(const float v[3]) {
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// A grip pose turned 90 degrees to the left about up, at 1 2 3.
inline void TurnedGrip(float position[3], float orientation[4]) {
  position[0] = 1.0f;
  position[1] = 2.0f;
  position[2] = 3.0f;
  hand_fit_math::AxisTurn(0.0f, 1.0f, 0.0f, 90.0f, orientation);
}

inline void FitChecks() {
  using namespace hand_fit_math;
  printf("\nThe fit of the hand on the controller\n");

  // No fit: the grip pose stays.
  {
    HandFit zero = {};
    float p[3], q[4];
    TurnedGrip(p, q);
    const float q0[4] = {q[0], q[1], q[2], q[3]};
    ApplyHandFit(zero, false, p, q);
    Check(IsZero(zero), "a fit of zeros is no fit", 1, 1);
    Near("a fit of zeros keeps the grip point", p[0] + p[1] + p[2], 6.0, 1e-6);
    Near("a fit of zeros keeps the turn",
         fabs(q[0] - q0[0]) + fabs(q[1] - q0[1]) + fabs(q[2] - q0[2]) +
             fabs(q[3] - q0[3]),
         0.0, 1e-6);
  }

  // The offset goes along the axes of the grip, not of the room.
  {
    HandFit fit = {};
    fit.forward_mm = 30.0f;
    float p[3], q[4];
    TurnedGrip(p, q);
    ApplyHandFit(fit, false, p, q);
    // Forward of the grip (-z) turned 90 degrees to the left is -x.
    Near("30 mm forward on a grip that looks left moves the point left",
         p[0], 1.0 - 0.030, 1e-5);
    Near("and not along the room's forward", p[2], 3.0, 1e-5);
  }

  // The left hand is the mirror image: out goes to -x.
  {
    HandFit fit = {};
    fit.out_mm = 10.0f;
    fit.up_mm = 20.0f;
    float pr[3] = {0, 0, 0}, qr[4] = {0, 0, 0, 1};
    float pl[3] = {0, 0, 0}, ql[4] = {0, 0, 0, 1};
    ApplyHandFit(fit, false, pr, qr);
    ApplyHandFit(fit, true, pl, ql);
    Near("out moves the right hand to the right", pr[0], 0.010, 1e-6);
    Near("out moves the left hand to the left", pl[0], -0.010, 1e-6);
    Near("up is the same on both hands", pl[1] - pr[1], 0.0, 1e-6);
  }

  // The turns: each one moves the front or the top of the right hand the
  // way its key says, and the left hand is the mirror image.
  {
    const float front[3] = {0.0f, 0.0f, -1.0f};
    const float top[3] = {0.0f, 1.0f, 0.0f};
    float turned[3];
    for (int left = 0; left < 2; ++left) {
      const float in = left ? 1.0f : -1.0f;  // the x toward the other hand
      HandFit fit = {};
      float p[3] = {0, 0, 0}, q[4] = {0, 0, 0, 1};
      fit.pitch_deg = 20.0f;
      ApplyHandFit(fit, left != 0, p, q);
      hand_fit_math::QuatRotate(q, front, turned);
      Near(left ? "a pitch turns the front of the left hand up"
                : "a pitch turns the front of the right hand up",
           turned[1], sinf(20.0f * 3.14159265f / 180.0f), 1e-5);

      fit = HandFit();
      q[0] = q[1] = q[2] = 0.0f;
      q[3] = 1.0f;
      fit.yaw_deg = 15.0f;
      ApplyHandFit(fit, left != 0, p, q);
      hand_fit_math::QuatRotate(q, front, turned);
      Near(left ? "a yaw turns the front of the left hand in"
                : "a yaw turns the front of the right hand in",
           turned[0] * in, sinf(15.0f * 3.14159265f / 180.0f), 1e-5);

      fit = HandFit();
      q[0] = q[1] = q[2] = 0.0f;
      q[3] = 1.0f;
      fit.roll_deg = 25.0f;
      ApplyHandFit(fit, left != 0, p, q);
      hand_fit_math::QuatRotate(q, top, turned);
      Near(left ? "a roll turns the top of the left hand in"
                : "a roll turns the top of the right hand in",
           turned[0] * in, sinf(25.0f * 3.14159265f / 180.0f), 1e-5);
    }
  }

  // A turn does not move the point that the offset chose, and the result is
  // a unit quaternion.
  {
    HandFit fit = {};
    fit.forward_mm = -12.0f;
    fit.pitch_deg = 40.0f;
    fit.yaw_deg = -30.0f;
    fit.roll_deg = 70.0f;
    float p[3], q[4];
    TurnedGrip(p, q);
    HandFit only_offset = fit;
    only_offset.pitch_deg = only_offset.yaw_deg = only_offset.roll_deg = 0.0f;
    float p2[3], q2[4];
    TurnedGrip(p2, q2);
    ApplyHandFit(fit, false, p, q);
    ApplyHandFit(only_offset, false, p2, q2);
    const float d[3] = {p[0] - p2[0], p[1] - p2[1], p[2] - p2[2]};
    Near("the turns of the fit keep the point of its offset", Length3(d), 0.0,
         1e-6);
    Near("the turned grip is a unit quaternion",
         sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]), 1.0,
         1e-5);
  }

  // The yaw comes after the pitch: a pitch up, then a yaw in, leaves the
  // front up by the pitch.
  {
    HandFit fit = {};
    fit.pitch_deg = 30.0f;
    fit.yaw_deg = 45.0f;
    float p[3] = {0, 0, 0}, q[4] = {0, 0, 0, 1};
    ApplyHandFit(fit, false, p, q);
    const float front[3] = {0.0f, 0.0f, -1.0f};
    float turned[3];
    hand_fit_math::QuatRotate(q, front, turned);
    Near("a yaw after a pitch keeps the front up by the pitch", turned[1],
         sinf(30.0f * 3.14159265f / 180.0f), 1e-5);
  }
}

// A hand with its palm down and its fingers along -z, in the frame of the
// joints of XR_EXT_hand_tracking. Each finger bends at each of its joints by
// `bend` degrees, to the palm (-y). The right hand has its thumb at -x, the
// left hand at +x. The palm joint has no turn on either hand.
inline void MakeHand(float bend_deg, bool left, float joints[26][3]) {
  using namespace finger_curl_math;
  for (int j = 0; j < kJointCount; ++j) {
    joints[j][0] = joints[j][1] = joints[j][2] = 0.0f;
  }
  const float side = left ? 1.0f : -1.0f;  // the x of the thumb side
  const float rad = bend_deg * 3.14159265f / 180.0f;
  // The thumb: from the side of the palm, forward. Its two last joints bend
  // to the palm too.
  for (int f = 0; f < 5; ++f) {
    const int first = f == 0 ? kThumbMetacarpal
                             : kIndexMetacarpal + (f - 1) * kFingerStride;
    const int bones = f == 0 ? 3 : 4;
    float x = f == 0 ? side * 0.03f : side * (0.025f - 0.015f * (f - 1));
    float y = 0.0f, z = f == 0 ? -0.01f : 0.0f;
    float angle = 0.0f;  // the bend so far, from -z to -y
    const float length[4] = {0.05f, 0.04f, 0.025f, 0.02f};
    joints[first][0] = x;
    joints[first][1] = y;
    joints[first][2] = z;
    for (int b = 0; b < bones; ++b) {
      // The metacarpal is straight. Each joint after it bends.
      if (b > 0) angle += rad;
      y -= length[b] * sinf(angle);
      z -= length[b] * cosf(angle);
      joints[first + b + 1][0] = x;
      joints[first + b + 1][1] = y;
      joints[first + b + 1][2] = z;
    }
  }
}

inline void CurlChecks() {
  using namespace finger_curl_math;
  printf("\nThe curl of the tracked fingers\n");
  const float palm[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float joints[26][3];
  float curls[5];

  for (int left = 0; left < 2; ++left) {
    MakeHand(0.0f, left != 0, joints);
    Check(FingerCurls(joints, palm, curls),
          left ? "a straight left hand gives curls"
               : "a straight right hand gives curls", 1, 1);
    float most = 0.0f;
    for (int f = 0; f < 5; ++f) most = curls[f] > most ? curls[f] : most;
    Near(left ? "a straight left hand has a curl of 0 on each finger"
              : "a straight right hand has a curl of 0 on each finger",
         most, 0.0, 1e-4);

    // 30 degrees at each joint: 60 on the thumb, 90 on each other finger.
    MakeHand(30.0f, left != 0, joints);
    FingerCurls(joints, palm, curls);
    Near(left ? "the left thumb with 2 bends of 30 degrees"
              : "the right thumb with 2 bends of 30 degrees",
         curls[0], 60.0 / kFullBendDeg[0], 1e-3);
    Near(left ? "the left index with 3 bends of 30 degrees"
              : "the right index with 3 bends of 30 degrees",
         curls[1], 90.0 / kFullBendDeg[1], 1e-3);
    Near(left ? "the left little finger with 3 bends of 30 degrees"
              : "the right little finger with 3 bends of 30 degrees",
         curls[4], 90.0 / kFullBendDeg[4], 1e-3);
  }

  // A bend to the back of the hand is not a curl.
  MakeHand(-20.0f, false, joints);
  FingerCurls(joints, palm, curls);
  Near("a finger bent back has a curl of 0", curls[2], 0.0, 1e-4);

  // A fist past the game's tightest pose stops at 1.
  MakeHand(100.0f, false, joints);
  FingerCurls(joints, palm, curls);
  Near("a fist past the tightest pose has a curl of 1", curls[3], 1.0, 1e-6);

  // The curls do not change when the whole hand turns: a turn of the joints
  // and of the palm joint together.
  {
    MakeHand(40.0f, false, joints);
    float before[5];
    FingerCurls(joints, palm, before);
    float turn[4];
    hand_fit_math::AxisTurn(0.6f, 0.0f, 0.8f, 75.0f, turn);
    float turned[26][3];
    for (int j = 0; j < kJointCount; ++j) {
      hand_fit_math::QuatRotate(turn, joints[j], turned[j]);
    }
    FingerCurls(turned, turn, curls);
    float most = 0.0f;
    for (int f = 0; f < 5; ++f) {
      const float d = fabsf(curls[f] - before[f]);
      most = d > most ? d : most;
    }
    Near("a turn of the whole hand keeps each curl", most, 0.0, 1e-4);
  }

  // Two joints at one place give no curl.
  MakeHand(10.0f, false, joints);
  joints[8][0] = joints[7][0];
  joints[8][1] = joints[7][1];
  joints[8][2] = joints[7][2];
  Check(!FingerCurls(joints, palm, curls),
        "a bone with no length gives no curls", 0, 0);
}

// The tracked curls in the poses of the overlay.
inline void TrackedPoseChecks() {
  using namespace overlay_math;
  printf("\nThe tracked fingers in the poses\n");
  const float tracked[5] = {0.1f, 0.0f, 0.5f, 0.8f, 1.0f};
  float curls[5];
  for (int p = 0; p < kPoseCount; ++p) {
    if (p == kPoseHold) continue;
    float weights[kPoseCount] = {};
    weights[p] = 1.0f;
    TrackedCurls(weights, kPoseRanges[kPoseHold], tracked, curls);
    float most = 0.0f;
    for (int f = 0; f < 5; ++f) {
      const float d = fabsf(curls[f] - tracked[f]);
      most = d > most ? d : most;
    }
    Near("in each pose but the hold, each finger has its tracked curl", most,
         0.0, 1e-6);
  }
  // The hold keeps its range: a full curl stops at the top of the range.
  PoseRange hold = kPoseRanges[kPoseHold];
  hold.hi[4] = 0.6f;
  float weights[kPoseCount] = {};
  weights[kPoseHold] = 1.0f;
  TrackedCurls(weights, hold, tracked, curls);
  Near("in the hold a full tracked curl stops at the top of the hold range",
       curls[4], 0.6, 1e-6);
  // Half the blend from the point pose to the hold is half of each.
  weights[kPoseHold] = 0.5f;
  weights[kPosePoint] = 0.5f;
  TrackedCurls(weights, hold, tracked, curls);
  Near("half the blend to the hold is the middle of the two curls", curls[4],
       0.5 * (1.0 + 0.6), 1e-6);
}

inline void HandFitChecks() {
  FitChecks();
  CurlChecks();
  TrackedPoseChecks();
}

}  // namespace hand_fit_checks
