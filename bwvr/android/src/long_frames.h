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
//                     lines a second, and each 10 s one line, with the new
//                     pictures by their delay in takes and the pictures that
//                     did not show (frame_queue)
//   The lines wait in memory until their window of 10 s ends. Then they go
//   to the log one at each frame (KeptLines), the line of the window last,
//   so the log never writes many lines on the thread at one time.
//
// THE PHASE OF THE TAKE (TakePhase, with slow_frame_ms above 0 or
// time_log=1, frame_loop=1): one line "take phase" each 10 s with the time
// from the write of a slot to the start of the frame of the game that took
// it, the time from that start to the end of the GPU work of its picture,
// and the times of the frame of the session around the take
// (take_phase_math.h).
#pragma once

#include <stdint.h>

#include <atomic>
#include <mutex>
#include <string>

#include "../../src/mod/long_frame_math.h"
#include "frame_loop.h"
#include "../../src/mod/take_phase_math.h"
#include "../../src/mod/time_log_math.h"

class InputLive;

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
  // At each take, after Take: the cadence of the ring (frame_queue,
  // frame_loop.h). delay: the takes after the submit of the copies of the
  // new picture and before this take (fresh only). replaced: the pictures that the take freed before
  // they showed. dropped: the pictures that the play thread dropped since the
  // take before.
  void Cadence(bool fresh, int delay, uint64_t replaced, uint64_t dropped);
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
  // The new pictures by their delay in takes (frame_loop::DelayBin).
  uint64_t delay_bins_[frame_loop::kDelayBins] = {0, 0, 0};
  uint64_t replaced_ = 0;  // pictures that a take freed before they showed
  uint64_t dropped_ = 0;   // pictures that the play thread dropped
};

// THE PHASE OF THE TAKE. Three threads give it times (CLOCK_MONOTONIC, ms):
// the reader of the live stream sees the start of each frame of the game (a
// new world of the mod in the input channel, which the mod writes at the
// start of a frame of the scene, after the wait of the pace) and its arrival,
// the play thread gives the play and the end of the GPU work of each picture,
// and the thread of the session gives the writes of the slots, the takes and
// the ends of its frames. The times of the reader and of the play thread go
// under the lock. The start of a frame is at most one step of the loop of the
// reader (0.5 ms when the ring is empty) after the write of its world.
class TakePhase {
 public:
  // Before the start of the reader: on with slow_frame_ms above 0 or
  // time_log=1.
  void Start(bool on);
  bool On() const { return on_; }
  // The thread of the session, at each Play: a new game and a new window.
  // channel: the input channel, which the reader reads the world of.
  void GameStarts(double now_ms, const InputLive* channel);
  // The thread of the session: the app wrote the slot with this frame.
  void Wrote(uint32_t slot, double ms);
  // The reader thread, at each turn of its loop.
  void Poll();
  // The reader thread: a frame of the game came.
  void Arrive(uint64_t frame, double ms);
  // The play thread: the picture of the frame of the game `frame` went to
  // the GPU at submit_ms. The play of its batch started at play_ms.
  void Played(uint64_t frame, double arrival_ms, double play_ms, double submit_ms);
  // The play thread: the GPU finished the picture of the last Played at ms.
  // waited: the play thread waited for its fence, so ms is the end. Else
  // the GPU ended before ms.
  void GpuDone(double ms, bool waited);
  // The thread of the session, after xrEndFrame of each frame: the end of
  // xrWaitFrame and the wait in it, the take (below 0: no take in this
  // frame), the end of xrEndFrame, the display period, and the display time
  // (ns of the runtime, and ms on the clock of the app, below 0 when not
  // known).
  void SessionFrame(double wait_end_ms, double xr_wait_ms, double take_ms, double end_ms,
                    double period_ms, int64_t display_ns, int64_t period_ns, double display_ms);
  // The thread of the session, each frame: the end of a window of 10 s, and
  // at most one line.
  void Tick(double now_ms);

 private:
  struct Pending {
    bool on = false;
    uint64_t frame = 0;
    double start = 0.0, arrival = 0.0, play = 0.0, submit = 0.0;
  };
  void Flush(double done_ms, bool have_done, bool waited);  // under lock_

  bool on_ = false;
  std::atomic<const InputLive*> channel_{nullptr};
  std::atomic<bool> reset_{false};      // the reader starts again (a new game)
  std::atomic<uint64_t> polls_{0};       // the turns of the loop of the reader
  uint64_t seen_frame_ = 0;              // the reader: the frame of the newest world
  KeptLines kept_;

  // UNDER lock_: the times of the reader and of the play thread.
  mutable std::mutex lock_;
  take_phase_math::FrameTimes<256> writes_;   // the slots: the time of the write
  take_phase_math::FrameTimes<256> starts_;   // the frames of the game: the start
  double last_arrival_ = -1.0;                // the newest arrival of a frame
  take_phase_math::Spread start_waited_;      // the write to the start, the game was free
  uint64_t start_bins_[take_phase_math::kStartBins] = {0, 0, 0, 0};
  take_phase_math::Spread start_busy_;        // the write to the start, the game was busy
  uint64_t start_no_write_ = 0;               // starts with no time of the write
  Pending pending_;
  take_phase_math::Spread way_, play_wait_, play_, gpu_, total_;
  uint64_t gpu_waited_ = 0;   // pictures with a wait of the play thread for the GPU
  uint64_t no_gpu_ = 0;       // pictures with no end of the GPU work
  uint64_t no_start_ = 0;     // pictures with no start of their frame

  // THE THREAD OF THE SESSION.
  double window_ms_ = -1.0;
  uint64_t polls_at_ = 0;
  take_phase_math::Spread to_take_, to_end_, xr_wait_, room_, lead_;
  take_phase_math::Least room_least_, lead_least_;
  // The display time of the frame of the session before, and if that frame
  // was in time (take_phase_math::SkippedPeriods).
  int64_t display_before_ = 0;
  bool before_in_time_ = false;
  uint64_t skipped_ = 0;
  uint64_t frames_ = 0;
};
