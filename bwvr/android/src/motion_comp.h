// THE MOTION OF THE STICK in the projection layer of the app (motion_comp,
// docs/android.md, "The motion of the stick"). Each picture of the game has
// the map of its frame: the place of the player in the world when the game
// drew it (src/mod/motion_comp_math.h). The session knows the newest map of
// the mod from the input channel. The pose of each view of the projection
// layer is the pose of the picture with the difference of the two maps, so a
// picture that shows in a later frame of the session is at the place in the
// world where the game drew it, seen from the place of the player now.
//
//   motion_comp=0  the old way: each view with the pose of its picture
//   motion_comp=1  the difference to the newest map of the mod
//   motion_comp=2  (the default) the same, and the newest map goes on with
//                  the walk of the stick to the display time of the frame,
//                  while the stick of the walk is pushed
//
// The thread of the session alone uses it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../src/common/input_channel.h"
#include "../../src/mod/motion_comp_math.h"
#include "../../src/mod/time_log_math.h"
#include "frame_loop.h"

class MotionComp {
 public:
  struct Keys {
    int mode = motion_comp_math::kPredict;  // motion_comp
    int depth_cm = 100;       // motion_comp_depth_cm
    int most_cm = 50;         // motion_comp_most_cm
    int most_deg = 10;        // motion_comp_most_deg
    int most_scale_pct = 25;  // motion_comp_most_scale_pct
    int predict_ms = 50;      // motion_comp_predict_ms
  };
  // The keys of bwvr.ini, with one line of the log.
  void SetKeys(const Keys& keys);
  // A new value of motion_comp while the game runs (the menu of the app),
  // with one line of the log when it changes.
  void SetMode(int mode);
  int Mode() const { return keys_.mode; }

  // The place of the player in the world of the picture that the eye shows, or
  // false (the timeline, trace_app.h).
  bool ShownWorld(int eye, input_channel::World* out) const {
    if (eye < 0 || eye > 1 || !shown_has_[eye]) return false;
    *out = shown_[eye];
    return true;
  }

  // The map of the newest picture (with XrApp::SetScreenView).
  void SetNewest(bool has, const input_channel::World& world);
  // An eye took the newest picture: the map of its picture.
  void TakeNewest(int eye);
  // ONCE IN EACH FRAME of the session, before the layers: the newest map of
  // the mod in the input channel (has: the read was whole), and the slots of
  // the input channel after the slot of that map, in the order of their
  // frames (InputLive::SlotsAfter), for the prediction.
  void FrameNow(bool has, const input_channel::World& channel,
                const motion_comp_math::SlotPush* slots, int count);
  // THE POSE OF A VIEW of the projection layer of this frame: position and
  // orientation (x y z w, LOCAL) of the eye of the picture, changed in place.
  // head: the middle of the eyes of the picture. True when the pose changed.
  bool View(int eye, const float head[3], float position[3], float orientation[4]);
  // The difference of the left eye in this frame, for the line of a new
  // picture. Empty with motion_comp=0.
  void Describe(char* text, size_t size) const;
  // THE LINE OF THE LOG each 10 s while the time log is on.
  void Tick(double now_ms, bool on);

 private:
  enum class Last { kNone, kOff, kNoMap, kStill, kMoved, kCut };
  Keys keys_;
  motion_comp_math::Limits limits_;
  bool newest_has_ = false;
  input_channel::World newest_ = {};
  bool shown_has_[2] = {false, false};
  input_channel::World shown_[2] = {};
  bool now_has_ = false;
  motion_comp_math::Map now_ = {};
  uint64_t now_frame_ = 0;
  float ahead_s_ = 0.0f;
  bool from_channel_ = false;
  // The left eye in this frame.
  Last last_ = Last::kNone;
  motion_comp_math::Motion last_motion_;
  // The counts since the last line.
  double window_ms_ = 0.0;
  uint64_t frames_ = 0;    // views of the projection layer
  uint64_t still_ = 0;     // with no difference
  uint64_t moved_ = 0;     // with a difference
  uint64_t cut_ = 0;       // with a difference over the limits
  uint64_t no_map_ = 0;    // with no map of the picture or of now
  uint64_t ahead_frames_ = 0;
  uint64_t channel_frames_ = 0;  // the map now from the input channel
  time_log_math::Stat step_cm_;
  time_log_math::Stat turn_deg_;
  time_log_math::Stat ahead_ms_;
};

// THE PARTS OF THE AGE of each new picture (frame_loop::AgeParts): one line
// of the log each 10 s while the time log is on. The thread of the session
// alone uses it.
class PictureAge {
 public:
  void Add(const frame_loop::AgeTimes& times);
  void Tick(double now_ms, bool on);

 private:
  double window_ms_ = 0.0;
  time_log_math::Stat parts_[frame_loop::kAgeParts];
  time_log_math::Stat all_;
};
