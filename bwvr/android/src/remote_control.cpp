#include "remote_control.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "../../src/mod/remote_math.h"
#include "app_log.h"

namespace rm = remote_math;

namespace {

double ClockNowMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

}  // namespace

void RemoteControl::Start(const std::string& control_path, const std::string& steps_path) {
  if (on_) return;
  control_path_ = control_path;
  steps_path_ = steps_path;
  serial_ = -1;
  step_.clear();
  gpu_state_.store(-1);
  {
    std::lock_guard<std::mutex> lock(lock_);
    prompt_.clear();
    prompt_new_ = false;
  }
  // The file of this game starts empty, so the steps of a game before do not count.
  FILE* steps = fopen(steps_path_.c_str(), "wb");
  if (steps != nullptr) fclose(steps);
  stop_.store(false);
  on_ = pthread_create(&thread_, nullptr, &RemoteControl::Main, this) == 0;
  AppLog("remote       : remote_control=1. The app follows %s and writes %s",
         control_path_.c_str(), steps_path_.c_str());
}

void RemoteControl::Stop() {
  if (!on_) return;
  stop_.store(true);
  pthread_join(thread_, nullptr);
  on_ = false;
}

bool RemoteControl::TakePrompt(std::string* text) {
  if (!on_) return false;
  std::lock_guard<std::mutex> lock(lock_);
  if (!prompt_new_) return false;
  prompt_new_ = false;
  *text = prompt_;
  return true;
}

void* RemoteControl::Main(void* self) {
  RemoteControl* remote = static_cast<RemoteControl*>(self);
  while (!remote->stop_.load()) {
    usleep(100000);
    remote->ReadFile();
  }
  return nullptr;
}

void RemoteControl::ReadFile() {
  FILE* file = fopen(control_path_.c_str(), "rb");
  if (file == nullptr) return;
  char text[rm::kMostBytes];
  const int size = static_cast<int>(fread(text, 1, sizeof(text), file));
  fclose(file);
  const int serial = rm::SerialOf(text, size);
  if (serial < 0 || serial == serial_) return;
  serial_ = serial;
  const double now = ClockNowMs();
  int at = 0;
  rm::Line line;
  while (rm::NextLine(text, size, &at, &line)) {
    if (rm::KeyIs(line, "step")) {
      const std::string step(line.value, static_cast<size_t>(line.value_size));
      if (step == step_) continue;
      step_ = step;
      FILE* steps = fopen(steps_path_.c_str(), "ab");
      if (steps != nullptr) {
        fprintf(steps, "%.3f,%d,%s\n", now, serial, step.c_str());
        fclose(steps);
      }
      AppLog("remote       : the step \"%s\" starts (serial %d)", step.c_str(), serial);
    } else if (rm::KeyIs(line, "prompt")) {
      std::lock_guard<std::mutex> lock(lock_);
      prompt_.assign(line.value, static_cast<size_t>(line.value_size));
      prompt_new_ = true;
    } else if (rm::KeyIs(line, "gpu_state")) {
      int state = -1;
      if (rm::IntOf(line, &state)) gpu_state_.store(state, std::memory_order_relaxed);
    }
  }
}
