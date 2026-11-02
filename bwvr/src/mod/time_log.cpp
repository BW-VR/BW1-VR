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
//
// THE LONG FRAMES (slow_frame_ms, long_frame_math.h, docs/android.md, "The
// long frames"), only with time_log=1:
//
//   slow_frame_ms=0  no more clock read and no more line (the default)
//   slow_frame_ms=N  each frame longer than N ms gives one line "long frame"
//                    with its parts, the calls of the frame loop of the game,
//                    the CPU time of the frame thread and the counts of the
//                    frame, at most 5 lines a second (the CPU time only
//                    with slow_frame_cpu=1, 0 by default: one call of
//                    GetThreadTimes at each present). The lines wait in
//                    memory until their window of 10 s ends. Then they go
//                    to the log one at each present, so no frame has more
//                    than one write of such a line. One more line counts
//                    the long frames of the window

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "long_frame_math.h"
#include "scene_sample.h"
#include "time_log.h"
#include "time_log_math.h"
#include "audio_watch.h"
#include "turn_parts.h"

void ModLog(const char* format, ...);
// game_start.cpp: the clock of the log.
double SecondsSinceProcessStart();
// file_read_ahead.cpp: the calls of LHFile::Open, of them the opens to
// write, and the reads of the game on the files that it opened to read.
void FileReadAheadCounts(uint32_t* opens, uint32_t* writes, uint32_t* reads);
// vk_stream.cpp: the surfaces whose pixels went into the live stream, and
// their bytes, and the frame of the stream of this present.
void VkStreamPixelCounts(uint64_t* surfaces, uint64_t* bytes);
uint64_t VkStreamPresentFrame();

bool g_time_log_slow = false;

namespace {

// Off until TimeLogSetOn reads the key and the frequency of the clock.
bool g_on = false;
LARGE_INTEGER g_hz = {};
time_log_math::Frames g_frames;
bool g_said = false;

// THE LONG FRAMES (slow_frame_ms). All on the frame thread.
long_frame_math::Gate g_gate;
long_frame_math::Calls g_calls;
long_frame_math::CpuClock g_cpu;
bool g_cpu_on = false;        // slow_frame_cpu=1: GetThreadTimes at each present
bool g_cpu_ok = true;         // GetThreadTimes works
uint64_t g_presents = 0;      // the presents since the start
// The counts at the last present.
uint32_t g_opens = 0, g_writes = 0, g_reads = 0;
uint64_t g_pixel_surfaces = 0, g_pixel_bytes = 0;
// The long frames of the window: their time, and the CPU time of the frame
// thread in them.
double g_long_ms = 0.0;
double g_long_cpu_ms = 0.0;
// The lines of the long frames. They wait until their window ends, and then
// go to the log one at each present (long_frame_math::KeptOrder). The gate
// gives at most 5 lines a second, so a window of 10 s has about 50.
const int kPending = 64;
char g_pending[kPending][3200];
long_frame_math::KeptOrder<kPending> g_kept;
uint64_t g_kept_lost = 0;  // the lost lines at the start of the window
// The lines of this file that went to the log since the last present: the
// line of a long frame of the frame after it says them, so a frame that a
// write made long shows it. And the time of these writes in the window.
int g_log_writes = 0;
double g_log_ms = 0.0;

double NowMs() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return static_cast<double>(now.QuadPart) * 1000.0 / static_cast<double>(g_hz.QuadPart);
}

// The CPU time of this thread in units of 100 ns, or false.
bool ThreadCpu(uint64_t* out) {
  FILETIME created, ended, kernel, user;
  if (!GetThreadTimes(GetCurrentThread(), &created, &ended, &kernel, &user)) return false;
  const uint64_t k =
      (static_cast<uint64_t>(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime;
  const uint64_t u = (static_cast<uint64_t>(user.dwHighDateTime) << 32) | user.dwLowDateTime;
  *out = k + u;
  return true;
}

// The frame that ended at this present: its line when it is long.
void SlowFrame(double now) {
  namespace lf = long_frame_math;
  namespace tl = time_log_math;
  ++g_presents;
  g_calls.Present(now);
  double cpu_ms = -1.0;
  uint64_t cpu = 0;
  if (!g_cpu_on) {
    // slow_frame_cpu=0: no call of GetThreadTimes.
  } else if (g_cpu_ok && ThreadCpu(&cpu)) {
    const bool first = !g_cpu.have;
    const uint64_t step = g_cpu.Take(cpu);
    if (!first) cpu_ms = static_cast<double>(step) / 10000.0;
  } else {
    g_cpu_ok = false;
  }
  uint32_t opens = 0, writes = 0, reads = 0;
  FileReadAheadCounts(&opens, &writes, &reads);
  uint64_t surfaces = 0, bytes = 0;
  VkStreamPixelCounts(&surfaces, &bytes);
  const tl::Frames& f = g_frames;
  bool say = false;
  const bool is_long = g_gate.Frame(f.last_frame, now, &say);
  SceneSampleFrameEnd(is_long, f.last_frame);
  if (is_long) {
    g_long_ms += f.last_frame;
    if (cpu_ms >= 0.0) g_long_cpu_ms += cpu_ms;
  }
  const int slot = say ? g_kept.Push() : -1;
  if (slot >= 0) {
    char before[64];
    if (g_gate.since_last_ms >= 0.0) {
      _snprintf_s(before, sizeof(before), _TRUNCATE, "%.0f ms after the long frame before",
                  g_gate.since_last_ms);
    } else {
      sprintf_s(before, "the first long frame");
    }
    char cpu_text[48];
    if (cpu_ms >= 0.0) {
      _snprintf_s(cpu_text, sizeof(cpu_text), _TRUNCATE, "%.1f ms", cpu_ms);
    } else if (!g_cpu_on) {
      sprintf_s(cpu_text, "not read (slow_frame_cpu=0)");
    } else {
      sprintf_s(cpu_text, "not known");
    }
    const lf::Calls& c = g_calls;
    // _TRUNCATE: a line that does not fit is cut, and no handler of a fault
    // of the C runtime runs.
    _snprintf_s(g_pending[slot], sizeof(g_pending[0]), _TRUNCATE,
           "long frame   : present %llu, the frame of the stream %llu, at %.3f s, "
           "%s. It took %.2f ms: the scene walk %.2f, the write of the stream %.2f, the "
           "wait of the pace %.2f, the rest %.2f. The calls of the frame loop: the input "
           "%.2f, the game turn %.2f, ProcessGraphicsEngine %.2f, FlipScreen %.2f, the "
           "network flush %.2f, outside them %.2f. The CPU time of the frame thread: %s. "
           "In the frame: %u opens of a file of the game (%u to write), %u reads of the "
           "game, %llu surfaces with pixels into the stream (%llu KB), %d lines of the "
           "time log",
           g_presents, VkStreamPresentFrame(), SecondsSinceProcessStart(), before,
           f.last_frame,
           f.last_part[tl::kScene], f.last_part[tl::kStream], f.last_part[tl::kPace],
           f.last_rest, c.last[lf::kInput], c.last[lf::kTurn], c.last[lf::kGraphics],
           c.last[lf::kFlip], c.last[lf::kFlush], c.Outside(f.last_frame), cpu_text,
           opens - g_opens, writes - g_writes, reads - g_reads,
           surfaces - g_pixel_surfaces, (bytes - g_pixel_bytes) / 1024, g_log_writes);
    // turn_parts=1: the parts of the game turn in this frame, in the same line.
    if (g_turn_parts) {
      const size_t used = strlen(g_pending[slot]);
      char text[1024];
      if (TurnPartsFrameText(text, sizeof(text)) > 0) {
        _snprintf_s(g_pending[slot] + used, sizeof(g_pending[0]) - used, _TRUNCATE,
                    ". The parts of the game turn (turn_parts=1): %s", text);
      }
      // audio_turn_watch=1: the parts of an audio turn of 5 ms or more.
      if (g_audio_watch) {
        const size_t used2 = strlen(g_pending[slot]);
        char audio[1400];
        if (AudioWatchFrameText(audio, sizeof(audio)) > 0) {
          _snprintf_s(g_pending[slot] + used2, sizeof(g_pending[0]) - used2, _TRUNCATE,
                      ". The audio turn (audio_turn_watch=1): %s", audio);
        }
      }
    }
  }
  if (g_turn_parts) TurnPartsFrameEnd();
  g_log_writes = 0;
  g_opens = opens;
  g_writes = writes;
  g_reads = reads;
  g_pixel_surfaces = surfaces;
  g_pixel_bytes = bytes;
}

// After the present: the next kept line of an ended window goes to the log.
void WriteKept() {
  const int slot = g_kept.Pop();
  if (slot < 0) return;
  const double start = NowMs();
  ModLog("%s", g_pending[slot]);
  g_log_ms += NowMs() - start;
  ++g_log_writes;
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

void TimeLogSetSlow(int slow_frame_ms, bool cpu) {
  const int limit = long_frame_math::Limit(slow_frame_ms);
  g_gate.limit_ms = g_on ? limit : 0;
  g_time_log_slow = g_gate.limit_ms > 0;
  g_cpu_on = g_time_log_slow && cpu;
  if (limit == 0) return;
  if (!g_on) {
    ModLog("long frame   : slow_frame_ms=%d needs time_log=1. No line of a long frame",
           limit);
    return;
  }
  ModLog("long frame   : slow_frame_ms=%d, slow_frame_cpu=%d. Each frame of the game "
         "longer than %d ms gives one line with its parts, the calls of the frame loop, "
         "%s and the counts of the frame, at most %d lines a second. The lines go to "
         "the log after the end of their window of 10 s, one at each present",
         limit, g_cpu_on ? 1 : 0, limit,
         g_cpu_on ? "the CPU time of the frame thread (GetThreadTimes at each present)"
                  : "no CPU time of the frame thread",
         long_frame_math::kLinesPerSecond);
}

void TimeLogCall(int call, bool begin) {
  if (!g_time_log_slow) return;
  if (begin) {
    g_calls.Begin(call, NowMs());
  } else {
    g_calls.End(call, NowMs());
  }
}


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
  if (g_time_log_slow) SlowFrame(now);
  if (!g_frames.Due(now)) {
    if (g_time_log_slow) WriteKept();
    return;
  }
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
  if (g_time_log_slow) {
    const double start = NowMs();
    char step[64];
    if (!g_cpu_on) {
      sprintf_s(step, "not read, slow_frame_cpu=0");
    } else if (!g_cpu_ok) {
      sprintf_s(step, "GetThreadTimes fails");
    } else if (g_cpu.least_step == 0) {
      sprintf_s(step, "the clock made no step yet");
    } else {
      sprintf_s(step, "the least step of the clock is %.3f ms",
                static_cast<double>(g_cpu.least_step) / 10000.0);
    }
    char cpu_text[64];
    if (g_cpu_on && g_cpu_ok) {
      _snprintf_s(cpu_text, sizeof(cpu_text), _TRUNCATE, "%.1f ms", g_long_cpu_ms);
    } else {
      sprintf_s(cpu_text, "not read");
    }
    const uint64_t lost = g_kept.lost - g_kept_lost;
    ModLog("long frame   : %llu frames longer than %d ms in %.1f s, %llu of them with a "
           "line. They took %.1f ms. The CPU time of the frame thread in them: %s "
           "(GetThreadTimes: %s). Since the line of the window before, the writes of "
           "that line and of the lines of the long frames took %.2f ms. The lines of "
           "this window go to the log now, one at each present",
           g_gate.long_frames, g_gate.limit_ms, seconds, g_gate.lines - lost, g_long_ms,
           cpu_text, step, g_log_ms);
    g_gate.NewWindow();
    g_kept.WindowEnds();
    g_kept_lost = g_kept.lost;
    g_long_ms = g_long_cpu_ms = 0.0;
    g_log_ms = NowMs() - start;
    // The two lines of the window are writes of this present.
    g_log_writes += 2;
  }
  if (g_turn_parts && TurnPartsWindowLine(seconds)) ++g_log_writes;
  if (g_audio_watch && AudioWatchWindowLine(seconds)) ++g_log_writes;
  if (!g_said) {
    g_said = true;
    ModLog("time log     : the rest is the logic of the game, the frame work of the mod "
           "and each call outside the scene walk, the write of the stream and the wait "
           "of the pace");
  }
  g_frames.NewWindow(now);
}
