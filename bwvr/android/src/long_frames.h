// THE LONG FRAMES OF THE GAME IN THE HEADSET APP (slow_frame_ms, docs/
// android.md, "The long frames", the rules in src/mod/long_frame_math.h).
//
//   slow_frame_ms=0  no line, no clock read and no read of /proc (the default)
//   slow_frame_ms=N  two measurements:
//     GameFrameClock  the reader of the live stream, at each frame of the game
//                     that came: the time since the frame before, and the time
//                     of the main thread of the game on a core and in the
//                     queue for a core in that time (schedstat of Linux, in
//                     ns). A gap longer than N ms gives one line "game frame",
//                     at most 5 lines a second, and each 10 s one line
//     LatchLog        the thread of the session, at each take of a picture
//                     (frame_loop=1): why a frame of the session had no new
//                     picture, how late the next picture was relative to the
//                     take, and how early the pictures in time were. One line
//                     "late picture" for each picture after a miss, at most 5
//                     lines a second, and each 10 s one line
//   The lines wait in memory until their window of 10 s ends. Then they go
//   to the log one at each frame (KeptLines), the line of the window last,
//   so the log never writes many lines on the thread at one time.
#pragma once

#include <stdint.h>

#include <atomic>
#include <string>

#include "../../src/mod/long_frame_math.h"
#include "../../src/mod/time_log_math.h"

// The lines of one thread that wait for the end of their window.
class KeptLines {
 public:
  // A line of the open window. The lines past the room of the ring go.
  void Add(const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
      __attribute__((format(printf, 2, 3)))
#endif
      ;
  // The open window ended: its lines may go.
  void WindowEnds() { order_.WindowEnds(); }
  // The next line of an ended window goes to the log, when there is one.
  void WriteOne();

 private:
  static constexpr int kRoom = 64;
  long_frame_math::KeptOrder<kRoom> order_;
  std::string slot_[kRoom];
};

class GameFrameClock {
 public:
  // Before the start of the reader: slow_frame_ms of bwvr.ini.
  void Start(int slow_frame_ms);
  bool On() const { return limit_ms_ > 0; }
  // Each thread: the process of the game (TimeLog::GamePid), 0 when not
  // known. Its main thread has the id of the process.
  void SetGamePid(int pid) { pid_.store(pid, std::memory_order_relaxed); }
  // Each thread, at Play: the next frame starts a new game. The reader
  // drops the frame before, the window and its counts at that frame.
  void GameStarts() { reset_.store(true, std::memory_order_relaxed); }
  // The reader thread, at each frame of the game that came at now_ms
  // (CLOCK_MONOTONIC): gives the time since the frame before, 0 for the
  // first frame.
  double Arrive(uint64_t frame, double now_ms);
  // The reader thread, after the frame is in the queue: at most one line.
  void AfterPush() { kept_.WriteOne(); }

 private:
  bool ReadThread(uint64_t* run_ns, uint64_t* wait_ns, uint64_t* slices);
  void WindowEnds(double now_ms);
  void NewWindow(double now_ms);

  int limit_ms_ = 0;
  std::atomic<int> pid_{0};
  std::atomic<bool> reset_{false};
  int open_pid_ = 0;
  int fd_ = -1;
  bool said_fail_ = false;
  bool have_ = false;  // the values of the frame before are valid
  uint64_t run_ns_ = 0, wait_ns_ = 0, slices_ = 0;
  double last_ms_ = -1.0;
  long_frame_math::Gate gate_;
  KeptLines kept_;
  // THE WINDOW of 10 s.
  double window_ms_ = -1.0;
  time_log_math::Stat gap_;
  double run_ms_ = 0.0, queue_ms_ = 0.0, timed_ms_ = 0.0;  // all frames with schedstat
  double long_ms_ = 0.0, long_run_ms_ = 0.0, long_queue_ms_ = 0.0, long_timed_ms_ = 0.0;
};

class LatchLog {
 public:
  void Start(int slow_frame_ms);
  bool On() const { return limit_ms_ > 0; }
  void GameStarts(double now_ms);
  // At each take of a picture by the session (frame_loop=1). fresh: a new
  // picture, with the arrival of its newest frame of the game, the end of
  // its copies, the time from the frame of the game before (gap_ms) and its
  // frame number. period_ms: the display period (0 when not known).
  void Take(double take_ms, bool fresh, double arrival_ms, double ready_ms, double gap_ms,
            uint64_t frame, double period_ms);
  // Each frame of the session: the end of a window of 10 s, and at most one
  // line.
  void Tick(double now_ms);

 private:
  int limit_ms_ = 0;
  long_frame_math::Latches latches_;
  long_frame_math::LineRate rate_;
  KeptLines kept_;
  double window_ms_ = -1.0;
  uint64_t takes_ = 0;
  uint64_t missed_ = 0;                                        // takes with no new picture
  uint64_t by_kind_[long_frame_math::kMisses] = {0, 0, 0, 0};  // those takes by the cause
  uint64_t late_bins_[long_frame_math::kLateBins] = {0, 0, 0, 0};
  time_log_math::Stat late_;      // kLate: the arrival after the take
  time_log_math::Stat slack_;     // pictures in time: the take after the end of their copies
  double slack_least_ = -1.0;     // the least of them, or -1
  time_log_math::Stat long_gap_;  // kLong: the gap of the frame of the game
};
