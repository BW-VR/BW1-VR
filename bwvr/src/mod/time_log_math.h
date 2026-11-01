// THE TIME LOG (time_log=1, docs/backend.md, "The time log of the mod", and
// docs/android.md, "The time log of the app"): the mean and the most of a
// time, and the parts of a frame of the game in the mod. No Windows code, so
// the headset app uses it too.
#pragma once

#include <stdint.h>

namespace time_log_math {

// The count, the sum and the most of a time in ms.
struct Stat {
  uint64_t count = 0;
  double sum = 0.0;
  double most = 0.0;
  void Add(double ms) {
    if (ms < 0.0) ms = 0.0;
    ++count;
    sum += ms;
    if (ms > most) most = ms;
  }
  // The times of another Stat go into this one.
  void Merge(const Stat& o) {
    count += o.count;
    sum += o.sum;
    if (o.most > most) most = o.most;
  }
  double Mean() const { return count > 0 ? sum / static_cast<double>(count) : 0.0; }
  void Reset() { *this = Stat(); }
};

// THE CAUSE OF A FRAME OF THE GAME THAT DID NOT SHOW in the headset app: a
// newer frame came before the app took it, because the app was still at the
// picture before. Of the time since that picture the largest part says why:
// the app waited for the GPU (the fences of its copies and of the walks of the
// renderer), the app was in xrWaitFrame or xrEndFrame of the runtime, or the
// app worked on the CPU (the rest of the time).
enum class Cause { kGpu = 0, kRuntime = 1, kCpu = 2 };
inline Cause CauseOf(double total_ms, double gpu_wait_ms, double runtime_wait_ms) {
  double cpu = total_ms - gpu_wait_ms - runtime_wait_ms;
  if (cpu < 0.0) cpu = 0.0;
  if (gpu_wait_ms >= runtime_wait_ms && gpu_wait_ms >= cpu) return Cause::kGpu;
  if (runtime_wait_ms >= cpu) return Cause::kRuntime;
  return Cause::kCpu;
}

// The time between two log lines: 10 s.
constexpr double kWindowMs = 10000.0;

// THE PARTS OF A FRAME OF THE GAME IN THE MOD. A frame is the time from one
// present of the game (the end of a FinishFrame) to the next. Its parts are
// the time in the scene walk, in the write of the live stream and in the wait
// of the pace (vk_pace) in that time, and the rest: the logic of the game,
// the frame work of the mod and each call of the game outside them.
enum Part { kScene = 0, kStream = 1, kPace = 2, kPartCount = 3 };

struct Frames {
  // The time of each part since the last present.
  double open[kPartCount] = {0.0, 0.0, 0.0};
  // The start of the walk that runs now, or -1. The FinishFrame of a walk
  // is inside it, so a present in a walk splits it.
  double walk_start = -1.0;
  double last_present = -1.0;  // the clock at the last present, or -1
  double window_start = -1.0;  // the clock at the first present of the window
  Stat frame;
  Stat part[kPartCount];
  Stat rest;
  uint64_t in_walk = 0;  // the presents in a scene walk
  // The frame that ended at the last present, for the line of a long frame
  // (long_frame_math.h): its time, the time of each part and the rest. All
  // 0 before the first frame.
  double last_frame = 0.0;
  double last_part[kPartCount] = {0.0, 0.0, 0.0};
  double last_rest = 0.0;

  void Add(Part p, double ms) {
    if (ms > 0.0) open[p] += ms;
  }
  void WalkBegin(double now) { walk_start = now; }
  void WalkEnd(double now) {
    if (walk_start >= 0.0 && now > walk_start) open[kScene] += now - walk_start;
    walk_start = -1.0;
  }
  // A present at the clock `now`: the frame since the last present ends. The
  // first present starts the first frame.
  void Present(double now) {
    if (walk_start >= 0.0) {
      if (now > walk_start) open[kScene] += now - walk_start;
      walk_start = now;
      ++in_walk;
    }
    if (window_start < 0.0) window_start = now;
    if (last_present >= 0.0) {
      const double ms = now - last_present;
      frame.Add(ms);
      double parts = 0.0;
      for (int i = 0; i < kPartCount; ++i) {
        part[i].Add(open[i]);
        parts += open[i];
        last_part[i] = open[i];
      }
      rest.Add(ms - parts);
      last_frame = ms;
      last_rest = ms > parts ? ms - parts : 0.0;
    }
    for (int i = 0; i < kPartCount; ++i) open[i] = 0.0;
    last_present = now;
  }
  // True when the window is 10 s long or longer at the clock `now`.
  bool Due(double now) const {
    return window_start >= 0.0 && now - window_start >= kWindowMs && frame.count > 0;
  }
  double WindowMs(double now) const { return window_start >= 0.0 ? now - window_start : 0.0; }
  // A new window from the clock `now`. The frame that is open stays open.
  void NewWindow(double now) {
    frame.Reset();
    rest.Reset();
    for (int i = 0; i < kPartCount; ++i) part[i].Reset();
    in_walk = 0;
    window_start = now;
  }
};

}  // namespace time_log_math
