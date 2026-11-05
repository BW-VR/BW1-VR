// A TEST AID: the remote control on the side of the app (remote_control of bwvr.ini,
// 0 by default, docs/android.md, "The test run"). A thread reads bwvr-control.txt
// of the folder of the game. The mod reads the same file for the switches. The app
// takes the name of the step for bwvr-steps.csv, the text for the player in the
// headset, the test state of the renderer, and the limit of the tiny draws.
#pragma once

#include <pthread.h>

#include <atomic>
#include <mutex>
#include <string>

class RemoteControl {
 public:
  bool On() const { return on_; }
  void Start(const std::string& control_path, const std::string& steps_path);
  void Stop();

  // The thread of the session: true one time for each new text. An empty text
  // hides the text in the headset.
  bool TakePrompt(std::string* text);
  // The play thread: the test state of the renderer that the file asks for, or -1.
  int GpuState() const { return gpu_state_.load(std::memory_order_relaxed); }
  // The play thread: the limit of the tiny draws in tenths of a pixel, or -1.
  int TinyPixels() const { return tiny_pixels_.load(std::memory_order_relaxed); }
  // The play thread: the shading rate of the compose in parts (1, 2 or 4), or -1.
  int ComposeRate() const { return compose_rate_.load(std::memory_order_relaxed); }

 private:
  static void* Main(void* self);
  void ReadFile();

  bool on_ = false;
  pthread_t thread_ = {};
  std::atomic<bool> stop_{false};
  std::string control_path_;
  std::string steps_path_;
  int serial_ = -1;
  std::string step_;
  std::mutex lock_;
  std::string prompt_;
  bool prompt_new_ = false;
  std::atomic<int> gpu_state_{-1};
  std::atomic<int> tiny_pixels_{-1};
  std::atomic<int> compose_rate_{-1};
};
