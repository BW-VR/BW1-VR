// THE WRITER OF THE INPUT CHANNEL in the app (input_live=1, docs/android.md,
// "The input channel"). Each frame of the session the app writes the input
// of the mod into the slot of bwvr-input.chan in the game folder
// (src/common/input_channel.h), and it plays the pulses that the mod puts
// into the ring of the events.
#pragma once

#include <stdint.h>

#include <string>

#include "../../src/common/input_channel.h"
#include "../../src/mod/motion_comp_math.h"

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
  // The mapped file, or nullptr (the timeline reads and writes its fields of
  // the head: trace_app.h).
  input_channel::File* Channel() const { return file_; }
  // THE PLACE OF THE PLAYER that the mod wrote last (input_channel::World,
  // docs/android.md, "The motion of the stick"). False when the channel is
  // not open or each read was torn.
  bool ReadWorld(input_channel::World* out) const;
  // The time (CLOCK_MONOTONIC, ms) of the write of the slot with this frame
  // (SlotData::frame), for the parts of the age of a picture. 0 when the
  // slot is not among the last kSlotTimes slots.
  double SlotMs(uint32_t frame) const;
  static constexpr uint32_t kSlotTimes = 256;  // a power of two
  // The newest slots after the slot with this frame, in the order of their frames,
  // with their display times and the sticks that the mod got, at most max
  // (the prediction of motion_comp=2, motion_comp_math::AheadSeconds).
  int SlotsAfter(uint32_t frame, motion_comp_math::SlotPush* out, int max) const;
  // The frame of the newest slot (SlotData::frame), 0 with none.
  uint32_t NewestSlot() const { return slots_; }

 private:
  uint32_t slot_frame_[kSlotTimes] = {};
  double slot_ms_[kSlotTimes] = {};
  motion_comp_math::SlotPush slot_push_[kSlotTimes];
  input_channel::File* file_ = nullptr;
  int fd_ = -1;
  std::string path_;
  uint32_t slots_ = 0;
  uint32_t test_pulse_ = 0;
  uint64_t pulses_ = 0;
  uint64_t keys_ = 0;
  uint64_t keys_dropped_ = 0;
  int lines_ = 0;
  int64_t period_ = 0;  // the display period of the last slot, for one line at a change
  int period_lines_ = 0;
  bool no_input_ = false;
  input_channel::SlotData last_ = {};
};
