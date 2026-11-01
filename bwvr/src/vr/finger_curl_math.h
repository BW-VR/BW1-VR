// THE CURL OF EACH FINGER from the joints of XR_EXT_hand_tracking
// (docs/hand-model.md, "Finger tracking"), as pure functions with no
// OpenXR call.
//
// A controller with finger tracking (the Valve Index controllers, the
// controllers of the Steam Frame) gives the runtime the place of each
// finger, and the runtime gives the 26 joints of each hand. The hand of the
// mod takes one curl for each finger: 0 is an open finger and 1 is the
// tightest pose of the game's Hand (hand-model.md, "What the mod does").
// So the curl is the sum of the bends of the finger joints, divided by the
// sum of the bends of the game's tightest pose.
//
// A joint of XR_EXT_hand_tracking: -z goes along the bone to the tip, +y
// goes out of the back of the hand, and +x is y cross z. Both hands have
// this frame, so a bend to the palm turns the bone about -x on both hands.

#pragma once

#include <math.h>

namespace finger_curl_math {

// The joints of XR_EXT_hand_tracking (XrHandJointEXT).
const int kJointCount = 26;
const int kPalm = 0;
const int kThumbMetacarpal = 2;   // then proximal, distal, tip
const int kIndexMetacarpal = 6;   // then proximal, intermediate, distal, tip
const int kFingerStride = 5;      // index 6, middle 11, ring 16, little 21

// The bend of each finger in the game's tightest pose, in degrees: the sum
// over the bones of a curl of 1 (hand-model.md, the table of bends). The
// thumb has two bones, each other finger has three.
const float kFullBendDeg[5] = {
    44.9f + 90.3f,          // thumb
    73.8f + 66.6f + 66.6f,  // index
    89.4f + 83.5f + 53.9f,  // middle
    113.8f + 94.3f + 64.7f, // ring
    123.7f + 70.9f + 59.3f, // little
};

inline void Sub(const float a[3], const float b[3], float out[3]) {
  for (int i = 0; i < 3; ++i) out[i] = a[i] - b[i];
}

inline float Dot(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline void Cross(const float a[3], const float b[3], float out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

inline bool Normalize(float v[3]) {
  const float length = sqrtf(Dot(v, v));
  if (!(length > 1e-6f)) return false;
  for (int i = 0; i < 3; ++i) v[i] /= length;
  return true;
}

// The +x axis of a joint with the unit quaternion q (x y z w).
inline void JointSide(const float q[4], float out[3]) {
  out[0] = 1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]);
  out[1] = 2.0f * (q[0] * q[1] + q[3] * q[2]);
  out[2] = 2.0f * (q[0] * q[2] - q[3] * q[1]);
}

// The bend in degrees from the bone `from` to the bone `to` (two unit
// vectors). With a side axis, a bend to the palm is plus and a bend to the
// back of the hand is minus. With no side axis (the thumb, whose bend is
// not in the plane of the palm), each bend is plus.
inline float BendDeg(const float from[3], const float to[3],
                     const float* side) {
  float axis[3];
  Cross(from, to, axis);
  const float c = Dot(from, to);
  float s = sqrtf(Dot(axis, axis));
  if (side != nullptr) {
    // A bend to the palm turns the bone about -x.
    s = -Dot(axis, side);
  }
  return atan2f(s, c) * 57.2957795f;
}

// The curl of each finger from the positions of the 26 joints (meters, in
// any one space) and the orientation of the palm joint. False when a bone
// has no length. A bend to the back of the hand counts as 0.
inline bool FingerCurls(const float positions[kJointCount][3],
                        const float palm_orientation[4], float curls[5]) {
  float side[3];
  JointSide(palm_orientation, side);
  for (int f = 0; f < 5; ++f) {
    const int first = f == 0 ? kThumbMetacarpal
                             : kIndexMetacarpal + (f - 1) * kFingerStride;
    // The thumb: metacarpal, proximal, distal, tip (3 bones). Each other
    // finger: metacarpal, proximal, intermediate, distal, tip (4 bones).
    const int bones = f == 0 ? 3 : 4;
    float dir[4][3];
    for (int b = 0; b < bones; ++b) {
      Sub(positions[first + b + 1], positions[first + b], dir[b]);
      if (!Normalize(dir[b])) return false;
    }
    // The bends at the joints past the metacarpal: two on the thumb (the
    // two bones that the curl of the game's thumb bends), three on each
    // other finger.
    float sum = 0.0f;
    for (int b = 0; b + 1 < bones; ++b) {
      const float bend = BendDeg(dir[b], dir[b + 1], f == 0 ? nullptr : side);
      if (bend > 0.0f) sum += bend;
    }
    float curl = sum / kFullBendDeg[f];
    if (curl < 0.0f) curl = 0.0f;
    if (curl > 1.0f) curl = 1.0f;
    curls[f] = curl;
  }
  return true;
}

}  // namespace finger_curl_math
