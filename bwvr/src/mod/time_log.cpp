// THE TIME LOG OF THE MOD (time_log.h, docs/backend.md, "The time log of the
// mod").
//
// A frame of the game goes from one present to the next: the end of one
// FinishFrame of the game to the end of the next. With one_walk=1 a frame of
// the scene has one FinishFrame, in its walk, so the frames of this line are
// the frames of the live stream. The parts come from the places that measure
// them already: the scene walk of stereo.cpp, the write of a frame into the
// ring of vk_stream.cpp (with its wait for room) and the wait of the pace of
// xr_input.cpp. The rest is the frame less these parts.
//
//   time_log=1  one line each 10 s (the default)
//   time_log=0  no clock read and no line

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "time_log.h"
#include "time_log_math.h"

void ModLog(const char* format, ...);

namespace {

// Off until TimeLogSetOn reads the key and the frequency of the clock.
bool g_on = false;
LARGE_INTEGER g_hz = {};
time_log_math::Frames g_frames;
bool g_said = false;

double NowMs() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return static_cast<double>(now.QuadPart) * 1000.0 / static_cast<double>(g_hz.QuadPart);
}

}  // namespace

void TimeLogSetOn(bool on) {
  g_on = on;
  QueryPerformanceFrequency(&g_hz);
  if (g_hz.QuadPart == 0) g_on = false;
  ModLog("time log     : time_log=%d. %s", on ? 1 : 0,
         g_on ? "Each 10 s one line with the mean and the most of the parts of a frame "
                "of the game: the frame from one present to the next, the scene walk, "
                "the write of the live stream, the wait of the pace and the rest"
              : "No line of the times");
}

bool TimeLogOn() { return g_on; }

void TimeLogWalk(bool begin) {
  if (!g_on) return;
  if (begin) {
    g_frames.WalkBegin(NowMs());
  } else {
    g_frames.WalkEnd(NowMs());
  }
}

void TimeLogStream(double ms) {
  if (g_on) g_frames.Add(time_log_math::kStream, ms);
}

void TimeLogPace(double ms) {
  if (g_on) g_frames.Add(time_log_math::kPace, ms);
}

void TimeLogPresent() {
  if (!g_on) return;
  const double now = NowMs();
  g_frames.Present(now);
  if (!g_frames.Due(now)) return;
  namespace tl = time_log_math;
  const tl::Frames& f = g_frames;
  const double seconds = f.WindowMs(now) / 1000.0;
  ModLog("time log     : %.1f s, %llu frames of the game (%.1f a second), %llu of them "
         "presented in a scene walk. A frame in ms, the mean and the most: the frame "
         "%.2f and %.2f, the scene walk %.2f and %.2f, the write of the stream %.2f "
         "and %.2f, the wait of the pace %.2f and %.2f, the rest %.2f and %.2f",
         seconds, f.frame.count, seconds > 0.0 ? f.frame.count / seconds : 0.0, f.in_walk,
         f.frame.Mean(), f.frame.most, f.part[tl::kScene].Mean(), f.part[tl::kScene].most,
         f.part[tl::kStream].Mean(), f.part[tl::kStream].most, f.part[tl::kPace].Mean(),
         f.part[tl::kPace].most, f.rest.Mean(), f.rest.most);
  if (!g_said) {
    g_said = true;
    ModLog("time log     : the rest is the logic of the game, the frame work of the mod "
           "and each call outside the scene walk, the write of the stream and the wait "
           "of the pace");
  }
  g_frames.NewWindow(now);
}
