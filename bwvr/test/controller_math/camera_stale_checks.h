// THE EYE CAMERA AFTER A CAMERA OF ITS OWN (docs/backend.md, "The eye camera
// after a camera of its own"). main.cpp includes this file after
// sprite_checks.h, just before main.
#pragma once

#include "../../src/mod/camera_stale_math.h"

namespace camera_stale_checks {

inline void CameraStaleChecks() {
  using namespace camera_stale_math;
  printf("\nThe eye camera after a camera of its own\n");

  // A matrix that was not changed is the same. One float that changed is not.
  const float place[3] = {1200.0f, 340.0f, -560.0f};
  float eye[12];
  sky_checks::HeadCamera(place, 35.0f, -40.0f, 20.0f, eye);
  float copy[12];
  memcpy(copy, eye, sizeof(copy));
  Check(SameMatrix(eye, copy), "a copy of a matrix is the same matrix", 0, 0);
  copy[9] += 0.0001f;
  Check(!SameMatrix(eye, copy), "a matrix with one changed float is not the "
        "same", 0, 0);

  // The camera that a restore builds has the place of the eye camera and no
  // roll: it is not the eye matrix, and the roll tells the two apart.
  float restored[12];
  sky_checks::HeadCamera(place, 35.0f, -40.0f, 0.0f, restored);
  Check(!SameMatrix(eye, restored), "a camera with no roll is not the eye "
        "camera", 0, 0);
  Near("the roll of the eye camera", RollDegrees(eye), 20.0, 1e-3);
  Near("the roll of the camera that a restore builds",
       RollDegrees(restored), 0.0, 1e-3);

  // The reasons.
  Check(Classify(true, 0, false) == kEyeCamera,
        "the matrix of the eye gives the camera of the eye", 0, 0);
  Check(Classify(false, 2, false) == kInsideCamera,
        "a camera of its own that is open gives the reason inside", 0, 0);
  Check(Classify(false, 2, true) == kAfterRestore,
        "a restore that left another matrix gives the reason after restore",
        0, 0);
  Check(Classify(false, 0, false) == kOtherChange,
        "a change with no camera call gives the reason other", 0, 0);

  // The stack of the calls. The effect saves the stored camera A, gives the
  // engine B (the engine stores B), and then gives A back.
  const float a[6] = {1200, 340, -560, 1100, 300, -500};
  const float b[6] = {0, 4, 0, 0, 0, 20};
  CameraStack stack;
  stack.Clear();
  Check(!stack.Call(b, a), "a camera of its own is not a restore", 0, 0);
  Check(stack.depth == 1, "the call is open", stack.depth, 1);
  Check(stack.Call(a, b), "the saved camera is a restore", 0, 0);
  Check(stack.depth == 0, "the restore closes the call", stack.depth, 0);
  Check(!stack.Call(a, a), "a camera with no open call is not a restore", 0,
        0);

  // Two effects, one inside the other: the inner restore gives the camera
  // of the outer effect back, and the outer restore gives A back.
  stack.Clear();
  stack.Call(b, a);
  const float c[6] = {5, 5, 5, 5, 5, 25};
  Check(!stack.Call(c, b), "a second camera of its own is not a restore", 0,
        0);
  Check(stack.depth == 2, "two calls are open", stack.depth, 2);
  Check(stack.Call(b, c), "the camera of the outer effect is a restore", 0, 0);
  Check(stack.depth == 1, "the inner call is closed", stack.depth, 1);
  Check(!EyeBackAfter(stack, true), "the inner restore leaves the eye camera "
        "alone, because the outer effect is open", 0, 0);
  Check(stack.Call(a, b), "the outer restore gives A back", 0, 0);
  Check(stack.depth == 0, "no call is open at the end", stack.depth, 0);
  Check(EyeBackAfter(stack, true), "the outer restore puts the eye camera "
        "back", 0, 0);
  Check(!EyeBackAfter(stack, false), "a call that gives no camera back does "
        "not put the eye camera back", 0, 0);

  // A restore of the outer camera closes the inner call too.
  stack.Clear();
  stack.Call(b, a);
  stack.Call(c, b);
  Check(stack.Call(a, c), "a restore of A closes two calls", 0, 0);
  Check(stack.depth == 0, "both calls are closed", stack.depth, 0);

  // More open calls than the stack holds do not overrun it.
  stack.Clear();
  float k[6];
  for (int i = 0; i < CameraStack::kDepth + 3; ++i) {
    for (int j = 0; j < 6; ++j) k[j] = 10.0f * i + j;
    stack.Call(k, a);
  }
  Check(stack.depth == CameraStack::kDepth, "the stack stops at its depth",
        stack.depth, CameraStack::kDepth);

  // The tally: the frames count once, and the first frame is the first stale
  // draw.
  Tally tally;
  tally.Clear();
  tally.Add(10, false);
  tally.Add(11, true);
  tally.Add(11, true);
  tally.Add(12, false);
  tally.Add(13, true);
  Check(tally.draws == 5 && tally.stale == 3, "the tally counts the draws "
        "and the stale draws", static_cast<double>(tally.stale), 3);
  Check(tally.stale_frames == 2, "two frames had a stale draw",
        static_cast<double>(tally.stale_frames), 2);
  Check(tally.first_frame == 11, "the first stale frame", 
        static_cast<double>(tally.first_frame), 11);

  // The census: one slot for a key, and a key beyond the table counts as an
  // overflow.
  Census<2> census;
  census.Clear();
  census.Find(0x4246E0)->Add(1, true);
  census.Find(0x4246E0)->Add(2, false);
  census.Find(0x840530)->Add(1, false);
  Check(census.used == 2, "two keys use two slots", census.used, 2);
  Check(census.Find(0x4246E0)->draws == 2, "a key keeps its slot",
        static_cast<double>(census.Find(0x4246E0)->draws), 2);
  Check(census.Find(0x7FA980) == nullptr && census.overflow == 1,
        "a third key does not fit", static_cast<double>(census.overflow), 1);

  // The share of world up in the camera plane: the cosine of the pitch. It
  // is below the fade of the sprite turn within about 10 degrees of straight
  // down.
  float down[12];
  sky_checks::HeadCamera(place, 35.0f, -60.0f, 20.0f, down);
  Near("the plane share at 60 degrees down", PlaneShare(down), 0.5, 1e-3);
  float steep[12];
  sky_checks::HeadCamera(place, 35.0f, -85.0f, 20.0f, steep);
  Check(PlaneShare(steep) < 0.17f, "the plane share at 85 degrees down is in "
        "the fade of the sprite turn", PlaneShare(steep), 0.0872);
}

}  // namespace camera_stale_checks
