// THE CORES OF THE HEADSET (thread_cores, thread_bind, docs/android.md, "The
// cores of the headset"). While a game runs, a thread of its own ("bwvr
// cores") reads each second the line of each thread of the game process and
// of the app in /proc. With thread_cores=1 it writes the core and the CPU
// time of each thread to bwvr-quest.log. With thread_bind=1 or 2 it binds the
// threads to the cores (sched_setaffinity) by the rules of core_rules.h. The
// rules and the keys are in core_rules.h.
#pragma once

#include <pthread.h>

#include <atomic>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core_rules.h"
#include "proc_stat.h"

class ThreadCores {
 public:
  struct Options {
    bool log = false;        // thread_cores
    int bind = 0;            // thread_bind
    std::string bind_cores;  // thread_bind_cores
  };
  // Play: the thread starts when the log or the binding is on. Else nothing.
  void GameStarts(const Options& options);
  // The end of the game, and the end of the app: the thread stops, and each
  // thread of the app that the binding moved gets the cores of the app back.
  void GameEnds();
  // The thread that calls it is the session thread or the play thread of the
  // app. The binding puts it on the fast cores.
  static void MarkSession();
  static void MarkPlay();
  static void MarkReader();
  // THE FAST CORES (late_fast_cores): the session thread, the play thread and the reader of the
  // app on the fast cores, or on each core of the app again. It can change while a game runs.
  static void KeepOnFastCores(bool on);
  // THE PLACE OF THE GAME (late_fast_game, late_top_game): the main thread of the game goes to each
  // core of the app (0), to the fast cores (1) or to the top core (2). A short thread of its own
  // looks for the process, so the caller does not wait for the read of /proc.
  static void PlaceGame(int place);

 private:
  static void* Main(void* self);
  void Run();
  void Second(double seconds);
  void FindGame();
  void Bind(int process, bool game, const std::map<int, proc_stat::Task>& tasks);
  void Restore();

  Options options_;
  core_rules::Plan plan_;
  pthread_t thread_{};
  bool running_ = false;
  std::atomic<bool> stop_{false};
  int game_pid_ = 0;
  long ticks_per_second_ = 100;
  std::map<int, proc_stat::Task> game_before_;
  std::map<int, proc_stat::Task> app_before_;
  // The place that each thread got (the binding), by its id.
  std::map<int, core_rules::Place> game_placed_;
  std::map<int, core_rules::Place> app_placed_;
  // The faults of sched_setaffinity, by errno, each logged one time.
  std::map<int, int> faults_;
  std::set<int> failed_;  // the threads with a fault of sched_setaffinity
  int moved_ = 0;  // threads that got a place since the last line
  int again_ = 0;  // threads that got their cores again since the last line
};
