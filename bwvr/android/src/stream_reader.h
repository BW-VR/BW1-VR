// THE READER OF THE LIVE STREAM OF THE WALKS (docs/android.md, "The live
// stream"). A thread maps the ring file of the mod (src/mod/stream_ring.h)
// with MAP_SHARED, asks the writer for the whole state, and takes each whole
// frame that waits in the ring into a queue. At each frame of the session the
// thread of the session takes the whole queue (a batch) and gives its frames
// to the renderer (scene_player.h, stream_ring.h PlayInBatch), because the
// renderer is not thread safe.
#pragma once

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

#include <atomic>
#include <deque>
#include <string>
#include <vector>

class StreamReader {
 public:
  // The thread for the ring at path. queue_bytes: the most bytes of frames
  // that wait for the session. A full queue takes no byte from the ring, so
  // the writer waits or drops a frame with no surface (stream_ring.h).
  bool Start(const std::string& path, size_t queue_bytes);
  // Leaves the ring (the writer stops) and ends the thread.
  void Stop();
  // Each whole frame that waits, the oldest first, or false when none waits.
  bool TakeAll(std::vector<std::vector<uint8_t>>* frames);

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
  size_t queue_max_bytes_ = 64u << 20;
  size_t queue_bytes_ = 0;
  pthread_t thread_ = {};
  bool started_ = false;
  std::atomic<bool> stop_{false};
  pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER;
  std::deque<std::vector<uint8_t>> queue_;
  pid_t feed_ = 0;
};
