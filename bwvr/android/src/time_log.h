// THE TIME LOG OF THE HEADSET APP (time_log, docs/android.md, "The time log
// of the app"). While the game runs, each 10 s some lines in bwvr-quest.log
// say where the time of a frame goes: the frames of the game and the
// pictures of the app, why a frame of the game did not show, the CPU time of
// the play of a picture, the waits in xrWaitFrame and for the GPU, the time
// of the GPU, the CPU time of each thread of the game and of the app, and
// the load of each core. One more line names the state of each thread of the
// game when no new frame of the game came for 5 s.
//
//   time_log=1  the lines (the default): a few reads of a clock in a frame,
//               two timestamps in each command buffer of the app, and one
//               read of /proc each 10 s
//   time_log=0  no line, no timestamp and no read of /proc
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "../../src/mod/time_log_math.h"
#include "frame_loop.h"
#include "proc_stat.h"
#include "xr_app.h"

// THE PLAY THREAD (frame_loop=1, docs/android.md, "The frame loop"): what it
// did since the last call of TimeLog::Play. The thread of the session takes
// it in each of its frames.
struct PlayStats {
  uint64_t pictures = 0;  // the batches, each one picture
  uint64_t frames = 0;    // the frames of the game in them
  // The frames of a batch but the newest did not show, by the cause of the
  // time of the picture before (time_log_math::Cause): the GPU, the wait
  // for the lock of the queue, the work of the play thread.
  uint64_t skipped[3] = {0, 0, 0};
  time_log_math::Stat play;       // ms of the play of a batch
  time_log_math::Stat walk_gpu;   // the GPU time of each walk of the renderer
  time_log_math::Stat walk_wait;  // the waits of the walks for the fence of their slot
  time_log_math::Stat slot_wait;  // the waits for the fence of a slot of the ring
  time_log_math::Stat busy;       // the GPU time of the copies into the slots
  uint64_t dropped = 0;   // pictures that the play thread dropped (no free slot)
  uint64_t replaced = 0;  // pictures that a newer picture replaced before a frame of the session
  double cpu_ms = 0.0;    // the CPU time of the play thread
  void Merge(const PlayStats& o) {
    pictures += o.pictures;
    frames += o.frames;
    for (int i = 0; i < 3; ++i) skipped[i] += o.skipped[i];
    play.Merge(o.play);
    walk_gpu.Merge(o.walk_gpu);
    walk_wait.Merge(o.walk_wait);
    slot_wait.Merge(o.slot_wait);
    busy.Merge(o.busy);
    dropped += o.dropped;
    replaced += o.replaced;
    cpu_ms += o.cpu_ms;
  }
};

class TimeLog {
 public:
  // time_log of bwvr.ini, at the start of the app.
  void Start(bool on);
  bool On() const { return on_; }

  // Play: a new window starts. The game is in Wine, and the app finds its
  // process (runblack.exe) at the next window.
  void GameStarts(double now_ms);
  void GameEnds();
  // THE FRAME LOOP at Play, before the play thread starts. On: the play
  // thread gives the batches, the times of the renderer and the causes of
  // the frames that did not show (Play), and the log names the play thread.
  // Off: the thread of the session gives them (Batch).
  void SetFrameLoop(bool on) { frame_loop_ = on; }
  // frame_loop=1: the stats of the play thread, at each frame of the session.
  void Play(const PlayStats& stats);
  // At each frame of the session while the game runs, before SessionFrame:
  // fresh, the frame shows a new picture of the game. It gives the age of
  // the shown picture in frames of the session.
  void Shown(bool fresh);

  // PlayLive, after the play of a batch: `frames` frames of the game in one
  // frame of the session, the play took `play_ms` on the CPU, and the wait in
  // xrWaitFrame of this frame was `wait_ms`. Each frame of the batch but the
  // newest did not show.
  void Batch(double now_ms, size_t frames, double play_ms, double wait_ms);
  // At the end of each frame of the session while the game runs: the times
  // of the frame (XrApp::TakeTimes).
  void SessionFrame(double now_ms, const XrApp::FrameTimes& times);
  // At each step of the loop while the game runs: the lines of a window of
  // 10 s, and the line of a stop of the game. arrived: the frames that the
  // reader of the live stream took from the ring since its start (they do
  // not wait for this loop). arrival_ms: the clock at the newest of them, or
  // 0 before the first frame of this game. last_frame: the number of the
  // newest frame that the app played.
  void Tick(double now_ms, uint64_t arrived, double arrival_ms, uint64_t last_frame);

 private:
  struct Sample {
    double ms = -1.0;
    std::map<int, proc_stat::Task> game;
    std::map<int, proc_stat::Task> app;
    uint64_t game_ticks = 0;   // the process, all threads
    uint64_t app_ticks = 0;
    uint64_t other_ticks = 0;  // the other processes of the user (Wine)
    std::vector<proc_stat::Cpu> cpus;
  };
  void TakeRenderer();
  void Lines(double now_ms, uint64_t arrived, uint64_t last_frame);
  void StopLine(double now_ms, double arrival_ms, uint64_t last_frame);
  void ResetWindow(double now_ms);
  // The threads and the processes now. The game process is found again when
  // it is gone.
  void Read(Sample* s, double now_ms);
  std::vector<proc_stat::Load> Loads(const Sample& before, const Sample& now) const;
  void ThreadLines(const Sample& before, const Sample& now, const char* why);

  bool on_ = false;
  bool game_on_ = false;
  bool frame_loop_ = false;
  // THE FRAME LOOP AND THE AGE in the window.
  uint64_t dropped_ = 0;
  uint64_t replaced_ = 0;
  time_log_math::Stat slot_wait_;
  double play_cpu_ms_ = 0.0;
  bool fresh_ = false;
  bool any_shown_ = false;
  uint64_t age_now_ = 0;
  time_log_math::Stat age_;
  long ticks_per_second_ = 100;
  int game_pid_ = 0;
  int said_no_stat_ = 0;  // 1: the log said that /proc/stat cannot be read

  // THE WINDOW.
  double window_ms_ = -1.0;
  bool arrived_set_ = false;
  uint64_t arrived_at_window_ = 0;  // the frames of the reader at the start of the window
  uint64_t session_frames_ = 0;
  uint64_t no_new_ = 0;      // frames of the session with no new frame of the game
  uint64_t pictures_ = 0;
  uint64_t frames_ = 0;      // frames of the game that came
  uint64_t skipped_[3] = {0, 0, 0};  // by time_log_math::Cause
  bool batch_now_ = false;   // this frame of the session had a batch
  double batch_ms_ = 0.0;    // the clock at the batch of this frame
  time_log_math::Stat play_;
  XrApp::FrameTimes xr_;
  time_log_math::Stat walk_gpu_;   // the GPU time of each walk of the renderer
  time_log_math::Stat walk_wait_;  // the waits of the walks for the fence of their slot
  // Since the picture before: the time, the waits for the GPU and in
  // xrWaitFrame.
  double since_total_ = 0.0;
  double since_gpu_ = 0.0;
  double since_wait_ = 0.0;
  double last_end_ms_ = -1.0;  // the end of the frame of the session before

  // THE THREADS: the samples of the last two windows.
  Sample last_;
  Sample before_;
  // THE STOP: the clock of the last frame of the game at the line of a stop.
  double stop_said_for_ = -1.0;
};
