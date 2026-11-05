// A TEST AID (pose_log of bwvr.ini, docs/android.md, "The late camera"): one record
// for each frame of the session, into bwvr-pose.bin of the folder of the app. A
// burst of a few wrong pictures does not show in the means of a window of 10 s,
// and tools/android/pose_report.py reads the file. The thread of the session only
// writes into memory. A thread of low priority writes the file.
#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atomic>
#include <string>
#include <vector>

struct PoseRecord {
  double t_ms;             // CLOCK_MONOTONIC at the record
  int64_t display_time;    // ns, the display time of this frame of the session
  uint32_t slot;           // the newest slot of the input channel
  uint32_t fresh;          // 1: this frame shows a new picture
  float eye[2][3];         // the eyes of the slot at the display time
  float orientation[4];    // the left eye of the slot, x y z w
  float fov[4];            // the left eye of the slot
  float next_head[3];      // the head at the display time plus one period
  uint64_t pic_frame;      // the frame of the game of the shown picture
  uint32_t pic_input;      // the slot of the view of the shown picture
  uint32_t pic_again;      // 1: the list drew again, 0: a new frame of the game
  float pic_eye[3];        // the left eye of the layer of the shown picture
  float pic_orientation[4];
  float pic_place[3];      // the place of the map of the shown picture
  float pic_move[3];       // the move of the world in the shown picture, meters
  float wait_ms;           // the wait in xrWaitFrame of this frame
  uint32_t tick;           // the tick of the late camera of this frame
  uint32_t pic_tick;       // the tick that the shown picture is for
  float take_wait_ms;      // the wait of the take of this frame for its picture
  float pic_work_ms;       // the shown picture: the start of its play to its submit
};

// A TEST AID with pose_log=1: one record for each picture of the late camera, into
// bwvr-pictures.bin. So a picture that did not show has its times too.
struct PictureRecord {
  double tick_ms;    // CLOCK_MONOTONIC: the session sent the tick
  double start_ms;   // the play thread took the tick
  double submit_ms;  // the play thread submitted the picture
  double done_ms;    // the play thread saw the end of the GPU work
  uint32_t tick;     // the tick of the picture
  uint32_t flags;    // 1: frames of the game in the batch, 2: the list again
  float gate_ms;     // the wait for the GPU before this picture
  float gpu_ms;      // the GPU time of the walks that ended before this picture
};

template <typename T>
class RecordLog {
 public:
  bool On() const { return on_; }

  void Start(const std::string& path, size_t most, uint32_t magic = 0x474F4C50u) {
    if (on_) return;
    file_ = fopen(path.c_str(), "wb");
    if (file_ == nullptr) return;
    const uint32_t head[2] = {magic, static_cast<uint32_t>(sizeof(T))};
    fwrite(head, sizeof(head), 1, file_);
    records_.resize(most);
    count_.store(0);
    stop_.store(false);
    on_ = pthread_create(&thread_, nullptr, &RecordLog::Main, this) == 0;
    if (!on_) {
      fclose(file_);
      file_ = nullptr;
    }
  }

  void Stop() {
    if (!on_) return;
    stop_.store(true);
    pthread_join(thread_, nullptr);
    on_ = false;
    fclose(file_);
    file_ = nullptr;
  }

  // One thread adds. The records past the end of the memory go.
  void Add(const T& r) {
    const size_t n = count_.load(std::memory_order_relaxed);
    if (n >= records_.size()) return;
    records_[n] = r;
    count_.store(n + 1, std::memory_order_release);
  }

 private:
  static void* Main(void* self) {
    RecordLog* log = static_cast<RecordLog*>(self);
    size_t written = 0;
    while (!log->stop_.load()) {
      for (int i = 0; i < 10 && !log->stop_.load(); ++i) usleep(100000);
      const size_t n = log->count_.load(std::memory_order_acquire);
      if (n > written) {
        fwrite(log->records_.data() + written, sizeof(T), n - written, log->file_);
        fflush(log->file_);
        written = n;
      }
    }
    return nullptr;
  }

  bool on_ = false;
  FILE* file_ = nullptr;
  pthread_t thread_ = {};
  std::atomic<bool> stop_{false};
  std::atomic<size_t> count_{0};
  std::vector<T> records_;
};

using PoseLog = RecordLog<PoseRecord>;
