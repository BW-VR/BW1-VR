// THE READER OF THE LIVE STREAM OF THE WALKS (docs/android.md, "The live
// stream"). A thread maps the ring file of the mod (src/mod/stream_ring.h)
// with MAP_SHARED, asks the writer for the whole state, and takes each whole
// frame that waits in the ring into a queue. At each frame of the session the
// thread of the session takes the whole queue (a batch) and gives its frames
// to the renderer (scene_player.h), because the renderer is not thread safe.
// With stream_live_frames=N the queue keeps the newest N frames whole, and
// each older frame keeps only its records of state (stream_split.h).
#pragma once

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

#include <atomic>
#include <deque>
#include <string>
#include <vector>

// A frame of the queue.
struct QueuedFrame {
  std::vector<uint8_t> bytes;
  uint64_t frame = 0;
  bool whole = true;     // false: only its records of state
  bool picture = false;  // a picture of a test (vk_scene_test_frame or F12)
  bool start = false;    // a kStart record
  int32_t test_frame = 0;
};

class StreamReader {
 public:
  // The thread for the ring at path. frames > 0: the reader takes each
  // frame of the ring at once, and the newest frames of the queue stay
  // whole. A queue that passes queue_bytes (0: no limit) is cleared, the
  // reader leaves the ring and asks again when the session takes frames
  // again (stream_split.h, QueueFull). frames == 0: queue_bytes is the most
  // bytes of frames that wait for the session. A full queue takes no byte
  // from the ring, so the writer waits or drops (stream_ring.h).
  bool Start(const std::string& path, size_t frames, size_t queue_bytes);
  // Leaves the ring (the writer stops) and ends the thread.
  void Stop();
  // Each frame that waits, the oldest first, or false when none waits.
  bool TakeAll(std::vector<QueuedFrame>* frames);
  // The most bytes that waited in the queue, and the limit (0: none).
  size_t MostBytes();
  size_t LimitBytes() const { return whole_frames_ > 0 ? queue_max_bytes_ : 0; }
  // THE TIME LOG (time_log.h): the frames that the thread took from the ring
  // since its start, and the clock (CLOCK_MONOTONIC, ms) at the newest one.
  // They do not wait for the thread of the session. Only after SetTimeLog
  // (true), before Start: else they stay 0 and the thread reads no clock.
  void SetTimeLog(bool on) { time_log_ = on; }
  uint64_t Arrived() const { return arrived_.load(std::memory_order_relaxed); }
  double LastArrivalMs() const { return last_arrival_ms_.load(std::memory_order_relaxed); }

  // A TEST AID (stream_live_feed=1): a child process writes the frames of
  // the stream file feed into the ring as the mod does, so the reader takes
  // them from a second process through the file. The child waits for the
  // ask, gives each frame and never drops one. pace_ms: the time between
  // two frames (0: no wait). It ends with the status 0 when the reader took
  // each byte.
  bool StartFeed(const std::string& ring, const std::string& feed, int pace_ms);

 private:
  static void* Main(void* self);
  void Run();

  std::string path_;
  size_t whole_frames_ = 3;
  size_t queue_max_bytes_ = 64u << 20;
  size_t queue_bytes_ = 0;
  pthread_t thread_ = {};
  bool started_ = false;
  std::atomic<bool> stop_{false};
  pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER;
  std::deque<QueuedFrame> queue_;
  uint64_t takes_ = 0;  // the calls of TakeAll: the session takes frames
  size_t most_bytes_ = 0;
  pid_t feed_ = 0;
  bool time_log_ = false;
  std::atomic<uint64_t> arrived_{0};
  std::atomic<double> last_arrival_ms_{0.0};
};
