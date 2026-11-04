// THE EYE CAMERA AFTER A CAMERA OF ITS OWN (docs/backend.md, "The eye camera
// after a camera of its own"). Pure math: no Windows, no game. The checks are
// in test/controller_math/camera_stale_checks.h.
//
// A camera call inside an eye pass can build a camera of its own for one
// effect and then give the stored camera back. That build has no eye offset
// and no roll. Anything that the game draws after it, in the same pass, is
// drawn from a camera that is not the camera of the eye: it does not roll
// with the world, so it seems fixed to the head.
//
// The mod puts the eye camera back after such a call. This header has the
// rules that tell the cases apart and the counters of the trace.

#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

namespace camera_stale_math {

// Two matrices of 12 floats that are the same, bit for bit. The mod writes
// the eye matrix itself and compares it with what the draw sees, so a
// tolerance would hide a small change.
inline bool SameMatrix(const float a[12], const float b[12]) {
  return memcmp(a, b, sizeof(float) * 12) == 0;
}

// A camera is a position and a focus: 6 floats. The game gives the stored
// camera back to the engine with the same 6 floats that the engine stored.
inline bool SameCamera(const float a[6], const float b[6]) {
  return memcmp(a, b, sizeof(float) * 6) == 0;
}

// THE CALLS THAT GIVE A CAMERA BACK. An effect saves the stored camera, gives
// the engine a camera of its own, draws, and gives the saved camera to the
// engine again. The engine stores each camera that it gets, so at the second
// call the stored camera is the camera of the effect, and the camera in the
// call is the camera that the stored camera was before the first call. The
// stack keeps the stored camera from before each open call. A call whose
// camera is in the stack gives a camera back, and it closes that call and the
// calls opened after it.
struct CameraStack {
  static const int kDepth = 6;
  float before[kDepth][6];
  int depth;

  void Clear() { depth = 0; }

  // One call with the camera `args` (position and focus) while the stored
  // camera is `stored`. True when the call gives a camera back. A call that
  // does not is open now.
  bool Call(const float args[6], const float stored[6]) {
    for (int i = depth - 1; i >= 0; --i) {
      if (SameCamera(args, before[i])) {
        depth = i;
        return true;
      }
    }
    if (depth < kDepth) memcpy(before[depth++], stored, sizeof(float) * 6);
    return false;
  }
};

// True when the eye camera goes back after a call: the call gave a camera
// back, and no call is open any more. The restore of an inner effect gives
// the camera of the outer effect back. The outer effect still draws with it,
// so the eye camera waits for the restore of the outer effect.
inline bool EyeBackAfter(const CameraStack& stack, bool restore) {
  return restore && stack.depth == 0;
}

// Why a draw sees a camera that is not the camera of the eye.
enum Reason {
  kEyeCamera = 0,    // the matrix is the matrix the eye loop wrote
  kInsideCamera,     // a camera of its own is open: no restore call yet
  kAfterRestore,     // the stored camera came back, and the eye did not
  kOtherChange,      // the matrix changed with no camera call in the pass
  kReasonCount
};

inline const char* ReasonText(int reason) {
  switch (reason) {
    case kEyeCamera:
      return "the camera of the eye";
    case kInsideCamera:
      return "inside a camera of its own";
    case kAfterRestore:
      return "after the stored camera came back";
    default:
      return "a change with no camera call";
  }
}

// The reason for one draw. matrix_is_eye: the matrix is the one that the eye
// loop wrote. camera_calls_since: camera calls inside the pass since the eye
// loop wrote it. last_was_restore: the last of them gave the stored camera
// back.
inline Reason Classify(bool matrix_is_eye, uint64_t camera_calls_since,
                       bool last_was_restore) {
  if (matrix_is_eye) return kEyeCamera;
  if (camera_calls_since == 0) return kOtherChange;
  return last_was_restore ? kAfterRestore : kInsideCamera;
}

// How many draws, and how many of them saw a camera that is not the camera of
// the eye, with the first frame and the number of frames that had one.
struct Tally {
  uint64_t draws;
  uint64_t stale;
  uint64_t stale_frames;
  uint64_t first_frame;
  uint64_t last_frame;

  void Clear() { memset(this, 0, sizeof(*this)); }
  void Add(uint64_t frame, bool is_stale) {
    ++draws;
    if (!is_stale) return;
    ++stale;
    if (stale == 1) first_frame = frame;
    if (stale == 1 || frame != last_frame) ++stale_frames;
    last_frame = frame;
  }
};

// A table of tallies by key (the address of a draw function). A key that does
// not fit goes in the last slot, so no draw is lost from the totals.
template <int kSlots>
struct Census {
  uintptr_t key[kSlots];
  Tally tally[kSlots];
  int used;
  uint64_t overflow;

  void Clear() {
    memset(this, 0, sizeof(*this));
  }
  // The tally for a key, or null when the table is full.
  Tally* Find(uintptr_t k) {
    for (int i = 0; i < used; ++i) {
      if (key[i] == k) return &tally[i];
    }
    if (used == kSlots) {
      ++overflow;
      return nullptr;
    }
    key[used] = k;
    return &tally[used++];
  }
};

// The share of the length of world up that lies in the camera plane. It is
// the cosine of the angle from the forward to the horizon. A sprite turns to
// world up by the full roll while the share is at least `fade`, and by less
// below it (sprite_math.h, kFadeShare).
inline float PlaneShare(const float m[12]) {
  const float x = m[3];
  const float y = m[4];
  const float z = m[5];
  const float all = sqrtf(x * x + y * y + z * z);
  if (!(all > 1e-6f)) return 0.0f;
  return sqrtf(x * x + y * y) / all;
}

// The roll of a camera matrix in degrees: the angle of world up in the camera
// plane, from the camera up. Zero when world up has no length in the plane.
inline float RollDegrees(const float m[12]) {
  if (m[3] == 0.0f && m[4] == 0.0f) return 0.0f;
  return atan2f(m[3], m[4]) * 57.29578f;
}

}  // namespace camera_stale_math
