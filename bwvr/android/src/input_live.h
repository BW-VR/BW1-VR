// THE WRITER OF THE INPUT CHANNEL in the app (input_live=1, docs/android.md,
// "The input channel"). Each frame of the session the app writes the input
// of the mod into the slot of bwvr-input.chan in the game folder
// (src/common/input_channel.h), and it plays the pulses that the mod puts
// into the ring of the events.
#pragma once

#include <stdint.h>

#include <string>

#include "../../src/common/input_channel.h"

class XrApp;

class InputLive {
 public:
  // Maps the file (it makes it when it is not there). test_pulse (a test
  // aid, input_live_test_pulse): at the slot of this number the app puts one
  // pulse into the ring of the events itself, as the mod does, so the read
  // and the play of the ring run with no mod. 0: none. False with the reason
  // in the log.
  bool Start(const std::string& path, uint32_t test_pulse);
  // One slot. The frame of the slot is the count of the slots.
  void Write(input_channel::SlotData* slot);
  // The pulses of the mod to the hands of the session.
  void PlayPulses(XrApp* xr);
  // A key of the keyboard of the app into the ring of the keys
  // (docs/android.md, "The keyboard of the app"): a character, or
  // input_channel::kKeyBack or kKeyEnter. False when the ring is full.
  bool PushKey(uint32_t code);
  // The session does not give input now (it does not run, or it ended): one
  // slot with no focus and no active hand, the views of the last slot.
  void WriteNoInput();
  void Stop();
  bool On() const { return file_ != nullptr; }

 private:
  input_channel::File* file_ = nullptr;
  int fd_ = -1;
  std::string path_;
  uint32_t slots_ = 0;
  uint32_t test_pulse_ = 0;
  uint64_t pulses_ = 0;
  uint64_t keys_ = 0;
  uint64_t keys_dropped_ = 0;
  int lines_ = 0;
  bool no_input_ = false;
  input_channel::SlotData last_ = {};
};
