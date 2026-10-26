// THE FRAME LOOP OF THE HEADSET APP (frame_loop, docs/android.md, "The frame
// loop"). The pure rules, with no Vulkan and no OpenXR, so the math test of
// the PC checks them (test/controller_math/frame_loop_checks.h).
//
//   frame_loop=1  the new loop (the default): a play thread plays each
//                 picture of the game into a slot of a ring, and the thread
//                 of the session shows the newest slot that the GPU
//                 finished. No thread of the session waits on the CPU for
//                 GPU work of a picture of the game
//   frame_loop=0  the old loop: the thread of the session plays the frames
//                 of the game, copies the picture, and waits for the fence
//                 of the command buffer before each copy
#pragma once

#include <stdint.h>

namespace frame_loop {

// frame_loop of bwvr.ini: each value but 0 is the new loop.
inline bool On(int key) { return key != 0; }

// THE RING OF THE PICTURES. A slot holds the eye pictures of one picture of
// the game, with the poses of its views and the frame number of the game.
//   kFree     the play thread can write it
//   kWriting  the play thread records and submits the copies of a picture
//   kReady    the GPU work of the picture is submitted. The fence of the
//             slot says when it is finished
//   kShown    the session shows it. The play thread does not touch it
constexpr int kSlots = 3;
enum class Slot : uint8_t { kFree, kWriting, kReady, kShown };

class Ring {
 public:
  // THE PLAY THREAD: the slot for the next picture. A free slot first (the
  // lowest index), else the oldest ready slot: its picture never shows, and
  // the ring counts a drop. -1 when no slot is free or ready, which three
  // slots cannot give: one is shown and the play thread writes one at most.
  int BeginWrite(bool* dropped) {
    *dropped = false;
    int oldest = -1;
    for (int i = 0; i < kSlots; ++i) {
      if (state_[i] == Slot::kFree) {
        state_[i] = Slot::kWriting;
        return i;
      }
      if (state_[i] == Slot::kReady && (oldest < 0 || serial_[i] < serial_[oldest])) {
        oldest = i;
      }
    }
    if (oldest < 0) return -1;
    state_[oldest] = Slot::kWriting;
    *dropped = true;
    ++dropped_;
    return oldest;
  }
  // The GPU work of slot i is submitted. It gets the next serial, so a
  // newer picture has a higher serial.
  void EndWrite(int i) {
    if (i < 0 || i >= kSlots || state_[i] != Slot::kWriting) return;
    state_[i] = Slot::kReady;
    serial_[i] = next_serial_++;
  }
  // The play thread could not write slot i.
  void CancelWrite(int i) {
    if (i >= 0 && i < kSlots && state_[i] == Slot::kWriting) state_[i] = Slot::kFree;
  }

  // THE SESSION, once in each frame of the session: the newest ready slot
  // whose GPU work is finished (done[i]) and newer than the shown slot. It
  // becomes the shown slot, and the shown slot before becomes free. Each
  // older ready slot becomes free too: a newer picture replaced it before a
  // frame of the session. -1: no new picture, and the session shows the
  // shown slot again. A ready slot that the GPU did not finish never holds a
  // frame of the session.
  int Take(const bool done[kSlots]) {
    int best = -1;
    for (int i = 0; i < kSlots; ++i) {
      if (state_[i] != Slot::kReady || !done[i]) continue;
      if (shown_ >= 0 && serial_[i] < serial_[shown_]) continue;
      if (best < 0 || serial_[i] > serial_[best]) best = i;
    }
    if (best < 0) return -1;
    for (int i = 0; i < kSlots; ++i) {
      if (i != best && state_[i] == Slot::kReady && serial_[i] < serial_[best]) {
        state_[i] = Slot::kFree;
        ++replaced_;
      }
    }
    if (shown_ >= 0) state_[shown_] = Slot::kFree;
    state_[best] = Slot::kShown;
    shown_ = best;
    return best;
  }
  // The game ended: no slot shows, and each ready slot is free. A slot in
  // writing stays with the play thread.
  void Clear() {
    for (int i = 0; i < kSlots; ++i) {
      if (state_[i] == Slot::kReady || state_[i] == Slot::kShown) state_[i] = Slot::kFree;
    }
    shown_ = -1;
  }

  Slot State(int i) const { return state_[i]; }
  uint64_t Serial(int i) const { return serial_[i]; }
  int Shown() const { return shown_; }
  // The pictures that the play thread dropped (no free slot), and the
  // pictures that a newer picture replaced before a frame of the session.
  uint64_t Dropped() const { return dropped_; }
  uint64_t Replaced() const { return replaced_; }

 private:
  Slot state_[kSlots] = {Slot::kFree, Slot::kFree, Slot::kFree};
  uint64_t serial_[kSlots] = {0, 0, 0};
  uint64_t next_serial_ = 1;
  int shown_ = -1;
  uint64_t dropped_ = 0;
  uint64_t replaced_ = 0;
};

// THE RING OF THE COMMAND BUFFERS of the session (gpu.cpp, GpuBegin). The
// entries go round, the least recently used first. The fence of an entry is
// checked only when the entry comes again. When its GPU work is not
// finished, the ring gets a new entry in its place (up to `most` entries),
// so the CPU does not wait. Only a ring of `most` entries waits.
struct CommandPick {
  int index = 0;      // the entry to record
  bool grow = false;  // a new entry goes in at index, before the old one
  bool wait = false;  // the CPU waits for the fence of the entry first
};

class CommandRing {
 public:
  explicit CommandRing(int most = 16) : most_(most < 1 ? 1 : most) {}
  int Count() const { return count_; }
  int Most() const { return most_; }
  // The entry that the next Pick checks: the least recently used.
  int Next() const { return next_; }
  // next_done: the GPU finished the entry Next(), or it has no submit yet.
  CommandPick Pick(bool next_done) {
    CommandPick p;
    p.index = next_;
    if (count_ == 0 || (!next_done && count_ < most_)) {
      // The new entry is the most recently used. The old entry at the index
      // moves up by one and stays the least recently used.
      p.grow = true;
      ++count_;
      next_ = (next_ + 1) % count_;
      return p;
    }
    p.wait = !next_done;
    next_ = (next_ + 1) % count_;
    return p;
  }

 private:
  int most_;
  int count_ = 0;
  int next_ = 0;
};

// The serials of the submits of the session: 1 for the first. A serial at
// or below `done` is finished. A staging buffer gets new pixels only when the
// last submit that read it is finished, so the CPU never waits for it.
inline bool SerialDone(uint64_t serial, uint64_t done) { return serial <= done; }

// THE AGE OF THE SHOWN PICTURE in frames of the session: 0 in the frame that
// shows a new picture, one more in each later frame with the same picture.
inline uint64_t NextAge(uint64_t age, bool fresh) { return fresh ? 0 : age + 1; }

}  // namespace frame_loop
