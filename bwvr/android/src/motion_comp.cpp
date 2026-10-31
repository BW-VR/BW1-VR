// THE MOTION OF THE STICK in the projection layer of the app (motion_comp.h).
#include "motion_comp.h"

#include <stdio.h>
#include <string.h>

#include "app_log.h"

namespace mc = motion_comp_math;

namespace {

const char* ModeText(int mode) {
  switch (mode) {
    case mc::kOff:
      return "each view has the pose of its picture";
    case mc::kNewest:
      return "each picture shows at the place in the world where the game drew it, from "
             "the newest place of the player that the mod gave";
    default:
      return "each picture shows at the place in the world where the game drew it, from "
             "the newest place of the player that the mod gave, and that place goes on "
             "with the walk of the stick to the display time while the stick is pushed";
  }
}

}  // namespace

void MotionComp::SetKeys(const Keys& keys) {
  keys_ = keys;
  keys_.mode = mc::Mode(keys.mode);
  if (keys_.depth_cm < 0) keys_.depth_cm = 0;
  if (keys_.most_cm < 0) keys_.most_cm = 0;
  if (keys_.most_deg < 0) keys_.most_deg = 0;
  if (keys_.most_scale_pct < 0) keys_.most_scale_pct = 0;
  if (keys_.predict_ms < 0) keys_.predict_ms = 0;
  limits_.step = keys_.most_cm * 0.01f;
  limits_.turn = static_cast<float>(keys_.most_deg);
  limits_.scale = keys_.most_scale_pct * 0.01f;
  AppLog("motion       : motion_comp=%d: %s. motion_comp_depth_cm=%d, motion_comp_most_cm=%d, "
         "motion_comp_most_deg=%d, motion_comp_most_scale_pct=%d, motion_comp_predict_ms=%d",
         keys_.mode, ModeText(keys_.mode), keys_.depth_cm, keys_.most_cm, keys_.most_deg,
         keys_.most_scale_pct, keys_.predict_ms);
}

void MotionComp::SetMode(int mode) {
  mode = mc::Mode(mode);
  if (mode == keys_.mode) return;
  AppLog("motion       : motion_comp is now %d (it was %d): %s", mode, keys_.mode,
         ModeText(mode));
  keys_.mode = mode;
}

void MotionComp::SetNewest(bool has, const input_channel::World& world) {
  newest_has_ = has && mc::Valid(world);
  if (newest_has_) newest_ = world;
}

void MotionComp::TakeNewest(int eye) {
  if (eye < 0 || eye > 1) return;
  shown_has_[eye] = newest_has_;
  if (newest_has_) shown_[eye] = newest_;
}

void MotionComp::FrameNow(bool has, const input_channel::World& channel,
                          const mc::SlotPush* slots, int count) {
  last_ = Last::kNone;
  now_has_ = false;
  ahead_s_ = 0.0f;
  from_channel_ = false;
  if (keys_.mode == mc::kOff) return;
  const input_channel::World* newest =
      mc::Newest(has ? &channel : nullptr, newest_has_ ? &newest_ : nullptr);
  if (newest == nullptr) return;
  from_channel_ = newest == &channel;
  now_ = mc::Now(*newest, keys_.mode, slots, count, keys_.predict_ms, &ahead_s_);
  now_frame_ = newest->frame;
  now_has_ = true;
}

bool MotionComp::View(int eye, const float head[3], float position[3], float orientation[4]) {
  if (keys_.mode == mc::kOff) {
    if (eye == 0) last_ = Last::kOff;
    return false;
  }
  if (eye == 0) {
    ++frames_;
    if (from_channel_) ++channel_frames_;
  }
  if (!now_has_ || eye < 0 || eye > 1 || !shown_has_[eye]) {
    if (eye == 0) {
      last_ = Last::kNoMap;
      ++no_map_;
    }
    return false;
  }
  mc::Map d;
  if (!mc::Difference(now_, mc::MapOf(shown_[eye]), &d)) {
    if (eye == 0) {
      last_ = Last::kNoMap;
      ++no_map_;
    }
    return false;
  }
  const mc::Motion m = mc::Measure(d, head);
  if (eye == 0) {
    last_motion_ = m;
    if (ahead_s_ > 0.0f) {
      ++ahead_frames_;
      ahead_ms_.Add(ahead_s_ * 1000.0);
    }
  }
  if (mc::Still(m)) {
    if (eye == 0) {
      last_ = Last::kStill;
      ++still_;
    }
    return false;
  }
  if (!mc::Within(m, limits_)) {
    if (eye == 0) {
      last_ = Last::kCut;
      ++cut_;
    }
    return false;
  }
  if (eye == 0) {
    last_ = Last::kMoved;
    ++moved_;
    step_cm_.Add(m.step * 100.0);
    turn_deg_.Add(m.turn);
  }
  float p[3], q[4];
  mc::MovePose(d, keys_.depth_cm * 0.01f, position, orientation, p, q);
  for (int i = 0; i < 3; ++i) position[i] = p[i];
  for (int i = 0; i < 4; ++i) orientation[i] = q[i];
  return true;
}

void MotionComp::Describe(char* text, size_t size) const {
  if (size == 0) return;
  text[0] = '\0';
  const char* mode = keys_.mode == mc::kPredict ? "motion_comp=2" : "motion_comp=1";
  char ahead[64] = "";
  if (ahead_s_ > 0.0f) {
    snprintf(ahead, sizeof(ahead), ", %.1f ms of the walk ahead", ahead_s_ * 1000.0f);
  }
  switch (last_) {
    case Last::kNone:
    case Last::kOff:
      return;
    case Last::kNoMap:
      snprintf(text, size, ", with no place of the player for the difference (%s)", mode);
      return;
    case Last::kStill:
      snprintf(text, size, ", with a difference of 0.000 cm and 0.000 degrees to the place of "
               "the player in frame %llu (%s)",
               static_cast<unsigned long long>(now_frame_), mode);
      return;
    case Last::kMoved:
    case Last::kCut:
      snprintf(text, size, ", with a difference of %.3f cm and %.3f degrees at the head and a "
               "scale of %.4f to the place of the player in frame %llu%s (%s)%s",
               last_motion_.step * 100.0f, last_motion_.turn, last_motion_.scale,
               static_cast<unsigned long long>(now_frame_), ahead, mode,
               last_ == Last::kCut ? ". That is over the limits, so the picture shows as "
                                     "the game drew it"
                                   : "");
      return;
  }
}

void MotionComp::Tick(double now_ms, bool on) {
  if (!on) return;
  if (window_ms_ <= 0.0) {
    window_ms_ = now_ms;
    return;
  }
  if (now_ms - window_ms_ < 10000.0) return;
  if (frames_ > 0) {
    AppLog("motion       : %.1f s, motion_comp=%d: %llu frames of the projection layer, the "
           "place of the player now from the input channel in %llu of them. %llu with no "
           "difference, %llu with a difference (the step at the head %.3f cm as a mean and "
           "%.3f at most, the turn %.3f and %.3f degrees), %llu over the limits (a cut), %llu "
           "with no place of the player. %llu with the walk ahead, %.1f ms as a mean and %.1f "
           "at most",
           (now_ms - window_ms_) / 1000.0, keys_.mode, static_cast<unsigned long long>(frames_),
           static_cast<unsigned long long>(channel_frames_),
           static_cast<unsigned long long>(still_), static_cast<unsigned long long>(moved_),
           step_cm_.Mean(), step_cm_.most, turn_deg_.Mean(), turn_deg_.most,
           static_cast<unsigned long long>(cut_), static_cast<unsigned long long>(no_map_),
           static_cast<unsigned long long>(ahead_frames_), ahead_ms_.Mean(), ahead_ms_.most);
  }
  window_ms_ = now_ms;
  frames_ = still_ = moved_ = cut_ = no_map_ = ahead_frames_ = channel_frames_ = 0;
  step_cm_.Reset();
  turn_deg_.Reset();
  ahead_ms_.Reset();
}

void PictureAge::Add(const frame_loop::AgeTimes& times) {
  double parts[frame_loop::kAgeParts];
  const double all = frame_loop::AgeParts(times, parts);
  for (int i = 0; i < frame_loop::kAgeParts; ++i) {
    if (parts[i] >= 0.0) parts_[i].Add(parts[i]);
  }
  if (all >= 0.0) all_.Add(all);
}

void PictureAge::Tick(double now_ms, bool on) {
  if (!on) return;
  if (window_ms_ <= 0.0) {
    window_ms_ = now_ms;
    return;
  }
  if (now_ms - window_ms_ < 10000.0) return;
  if (all_.count > 0 || parts_[1].count > 0) {
    const time_log_math::Stat* p = parts_;
    AppLog("time         : the parts of the age of a new picture, ms, the mean and the most: "
           "the game from the input of the app to the arrival of the frame %.1f and %.1f "
           "(%llu), the reader %.1f and %.1f, the play %.1f and %.1f, the GPU and the wait in "
           "the ring %.1f and %.1f, the session to the end of xrEndFrame %.1f and %.1f, the "
           "runtime to the display time %.1f and %.1f. From the input to the display time "
           "%.1f and %.1f (%llu pictures)",
           p[0].Mean(), p[0].most, static_cast<unsigned long long>(p[0].count), p[1].Mean(),
           p[1].most, p[2].Mean(), p[2].most, p[3].Mean(), p[3].most, p[4].Mean(), p[4].most,
           p[5].Mean(), p[5].most, all_.Mean(), all_.most,
           static_cast<unsigned long long>(all_.count));
  }
  window_ms_ = now_ms;
  for (time_log_math::Stat& s : parts_) s.Reset();
  all_.Reset();
}
