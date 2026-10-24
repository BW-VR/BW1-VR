#include "input_live.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "app_log.h"
#include "xr_app.h"

namespace ic = input_channel;

bool InputLive::Start(const std::string& path, uint32_t test_pulse) {
  path_ = path;
  test_pulse_ = test_pulse;
  fd_ = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0660);
  struct stat st = {};
  if (fd_ < 0 || fstat(fd_, &st) != 0) {
    AppLog("input        : input_live=1, but %s cannot be opened (errno %d)",
           path.c_str(), errno);
    Stop();
    return false;
  }
  if (st.st_size < static_cast<off_t>(ic::kFileBytes) &&
      ftruncate(fd_, ic::kFileBytes) != 0) {
    AppLog("input        : the size of %s cannot be set (errno %d)", path.c_str(),
           errno);
    Stop();
    return false;
  }
  void* v = mmap(nullptr, ic::kFileBytes, PROT_READ | PROT_WRITE, MAP_SHARED,
                 fd_, 0);
  if (v == MAP_FAILED) {
    AppLog("input        : mmap of %s failed (errno %d)", path.c_str(), errno);
    Stop();
    return false;
  }
  file_ = static_cast<ic::File*>(v);
  const bool whole = ic::HeadOk(file_, ic::kFileBytes);
  if (!whole) ic::MakeHead(file_);
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  ic::StoreRelease(&file_->head.app_session,
                   static_cast<uint32_t>(getpid()) ^ static_cast<uint32_t>(t.tv_nsec));
  // After a crash the slot of an earlier run of the app can hold the focus
  // and active hands. The first slot of this run has neither, so the mod
  // takes no input of the earlier run.
  ic::SlotData none = {};
  for (int e = 0; e < 2; ++e) none.eyes[e].orientation[3] = 1.0f;
  for (int h = 0; h < 2; ++h) {
    none.hands[h].aim_orientation[3] = 1.0f;
    none.hands[h].grip_orientation[3] = 1.0f;
  }
  Write(&none);
  no_input_ = true;
  // A pulse of an earlier run is not for now.
  const uint32_t old = ic::DropOldEvents(file_);
  AppLog("input        : input_live=1. Each frame of the session writes the "
         "input of the mod into %s (%s), and the app plays the pulses of the "
         "mod. %u old pulses dropped%s",
         path.c_str(), whole ? "the head was there" : "a new head", old,
         test_pulse_ > 0 ? ". input_live_test_pulse is on (a test aid)" : "");
  return true;
}

void InputLive::Write(ic::SlotData* slot) {
  if (file_ == nullptr) return;
  slot->frame = ++slots_;
  ic::Write(&file_->slot, *slot);
  last_ = *slot;
  no_input_ = false;
  if (test_pulse_ > 0 && slots_ == test_pulse_) {
    // As XrInputHaptic of the mod: the right hand, 80 ms at 0.5.
    const ic::Event e = {slots_, 1u, 0.5f, 0.08f};
    const bool in = ic::PushEvent(file_, e);
    AppLog("input        : input_live_test_pulse: a pulse of 80 ms at 0.50 for "
           "the right hand %s the ring at slot %u", in ? "goes into" : "does not fit",
           slots_);
  }
  // Some lines for the log: the first slot, then one each 72 slots.
  const bool say = lines_ < 12 && (slots_ == 1 || slots_ % 72 == 0);
  if (!say) return;
  ++lines_;
  const ic::Hand& r = slot->hands[1];
  const ic::Eye& e = slot->eyes[0];
  AppLog("input        : slot %u, focus %d, the left eye %s at %.3f %.3f %.3f, "
         "fov %.3f %.3f %.3f %.3f, the right hand flags 0x%03X aim %.3f %.3f "
         "%.3f %.4f %.4f %.4f %.4f trigger %.2f, profile \"%s\", display "
         "period %lld ns, %u recenters",
         slots_, (slot->flags & ic::kFocused) != 0 ? 1 : 0,
         e.valid ? "valid" : "not valid", e.position[0], e.position[1],
         e.position[2], e.fov[0], e.fov[1], e.fov[2], e.fov[3], r.flags,
         r.aim_position[0], r.aim_position[1], r.aim_position[2],
         r.aim_orientation[0], r.aim_orientation[1], r.aim_orientation[2],
         r.aim_orientation[3], r.trigger, r.profile,
         static_cast<long long>(slot->display_period), slot->recenters);
}

void InputLive::WriteNoInput() {
  if (file_ == nullptr || no_input_ || slots_ == 0) return;
  ic::SlotData s = last_;
  s.flags &= ~(ic::kFocused | ic::kRunning);
  for (int h = 0; h < 2; ++h) {
    const uint32_t serial = s.hands[h].serial;
    memset(&s.hands[h], 0, sizeof(s.hands[h]));
    s.hands[h].serial = serial;
    s.hands[h].aim_orientation[3] = 1.0f;
    s.hands[h].grip_orientation[3] = 1.0f;
  }
  Write(&s);
  no_input_ = true;
  AppLog("input        : slot %u: the session gives no input. No hand is "
         "active, and the views stay", slots_);
}

void InputLive::PlayPulses(XrApp* xr) {
  if (file_ == nullptr) return;
  ic::Event events[ic::kEvents];
  bool broken = false;
  const uint32_t n = ic::TakeEvents(file_, events, ic::kEvents, &broken);
  if (broken) AppLog("input        : the indices of the events were not valid");
  for (uint32_t i = 0; i < n; ++i) {
    xr->PulseWith(static_cast<int>(events[i].hand), events[i].amplitude,
                  events[i].seconds);
    ++pulses_;
    if (pulses_ <= 20) {
      AppLog("input        : a pulse of the mod, %.0f ms at %.2f for the %s "
             "hand, frame %u of the mod", events[i].seconds * 1000.0f,
             events[i].amplitude, events[i].hand ? "right" : "left",
             events[i].frame);
    }
  }
}

bool InputLive::PushKey(uint32_t code) {
  if (file_ == nullptr) return false;
  const ic::KeyEvent k = {code, slots_};
  const bool in = ic::PushKey(file_, k);
  if (in) {
    ++keys_;
  } else {
    ++keys_dropped_;
  }
  return in;
}

void InputLive::Stop() {
  if (file_ != nullptr) {
    AppLog("input        : %u slots written, %llu pulses of the mod played, %llu "
           "keys of the keyboard written and %llu dropped. The mod read the slot "
           "%u times, read %u torn copies again and read %u keys",
           slots_, static_cast<unsigned long long>(pulses_),
           static_cast<unsigned long long>(keys_),
           static_cast<unsigned long long>(keys_dropped_),
           ic::LoadAcquire(&file_->head.latches),
           ic::LoadAcquire(&file_->head.torn), ic::LoadAcquire(&file_->head.key_read));
    munmap(file_, ic::kFileBytes);
  }
  file_ = nullptr;
  if (fd_ >= 0) close(fd_);
  fd_ = -1;
}
