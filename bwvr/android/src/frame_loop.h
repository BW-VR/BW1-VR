// THE FRAME LOOP OF THE HEADSET APP (frame_loop, docs/android.md, "The frame
// loop"). The pure rules, with no Vulkan and no OpenXR, so the math test of
// the PC checks them (test/controller_math/frame_loop_checks.h).
//
//   frame_loop=1  the new loop (the default): a play thread plays each
//                 picture of the game into a slot of a ring, and the thread
//                 of the session shows a slot that the GPU finished (the
//                 newest, or the next in order with frame_queue=1). No
//                 thread of the session waits on the CPU for
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
// kSlots is the size of the arrays of the slots. The ring uses kNewestSlots
// of them with frame_queue=0 and kSlots with frame_queue=1.
constexpr int kSlots = 4;
constexpr int kNewestSlots = 3;
enum class Slot : uint8_t { kFree, kWriting, kReady, kShown };

// THE QUEUE OF THE PICTURES (frame_queue of bwvr.ini, 0 by default).
//   frame_queue=0  each take shows the newest finished picture, in a ring of
//                  3 slots
//   frame_queue=1  each take shows the oldest finished picture that is newer
//                  than the shown one, in a ring of 4 slots: one shown, one
//                  finished that waits for its take, one at the GPU and one
//                  that the play thread writes. When more than one finished
//                  picture waits after a take, the older ones go, so the
//                  delay is at most one take more than with 0
inline bool QueueOn(int key) { return key != 0; }
inline int SlotsOf(bool queue) { return queue ? kSlots : kNewestSlots; }
// The finished pictures that may wait after a take with frame_queue=1.
constexpr int kQueueWaits = 1;
// The pick of a take from n finished pictures that are newer than the shown
// one, in the order of their serials (0 the oldest): the index of the
// picture that shows, or -1 with none. Each picture before it goes.
inline int QueuePick(bool queue, int n) {
  if (n <= 0) return -1;
  if (!queue) return n - 1;
  return n > kQueueWaits ? n - 1 - kQueueWaits : 0;
}
// THE DELAY OF A SHOWN PICTURE in takes: the takes after the submit of its
// copies and before the take that showed it. With each picture in time it is
// 0 with frame_queue=0 and 1 with frame_queue=1. 0, 1, and 2 or more for the
// counts of the line "late picture".
constexpr int kDelayBins = 3;
inline int DelayBin(int takes) { return takes <= 0 ? 0 : takes >= 2 ? 2 : 1; }

class Ring {
 public:
  // frame_queue: the queue of the pictures (true) or the newest finished
  // picture (false, the default).
  explicit Ring(bool queue = false) { SetQueue(queue); }
  // A new value of frame_queue. The ring can take it at each time: a slot
  // above the slots of the new value takes no new write, and the session
  // frees it when a newer picture shows.
  void SetQueue(bool queue) {
    queue_ = queue;
    count_ = SlotsOf(queue);
  }
  bool Queue() const { return queue_; }
  int Count() const { return count_; }

  // THE PLAY THREAD: the slot for the next picture. A free slot first (the
  // lowest index), else the oldest ready slot: its picture never shows, and
  // the ring counts a drop. -1 when no slot is free or ready, which the ring
  // cannot give: one is shown and the play thread writes one at most.
  int BeginWrite(bool* dropped) {
    *dropped = false;
    int oldest = -1;
    for (int i = 0; i < kSlots; ++i) {
      if (state_[i] == Slot::kFree && i < count_) {
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
    waits_[i] = 0;
  }
  // The play thread could not write slot i.
  void CancelWrite(int i) {
    if (i >= 0 && i < kSlots && state_[i] == Slot::kWriting) state_[i] = Slot::kFree;
  }

  // THE SESSION, once in each frame of the session. The candidates are the
  // ready slots whose GPU work is finished (done[i]) and that are newer than
  // the shown slot. frame_queue=0: the newest candidate shows. frame_queue=1:
  // the oldest candidate shows, or with more than kQueueWaits candidates the
  // oldest of the newest kQueueWaits + 1 (QueuePick). The picked slot
  // becomes the shown slot, and the shown slot before becomes free. Each
  // older ready slot becomes free too: a newer picture replaced it before a
  // frame of the session. Each ready slot that stays after a take counts
  // one take more of delay, also after a take with no new picture. -1: no
  // new picture, and the session shows the shown slot again. A ready slot
  // that the GPU did not finish never holds a frame of the session.
  int Take(const bool done[kSlots]) {
    int order[kSlots];
    int n = 0;
    for (int i = 0; i < kSlots; ++i) {
      if (state_[i] != Slot::kReady || !done[i]) continue;
      if (shown_ >= 0 && serial_[i] < serial_[shown_]) continue;
      // In the order of the serials: few slots, so an insertion.
      int at = n++;
      while (at > 0 && serial_[order[at - 1]] > serial_[i]) {
        order[at] = order[at - 1];
        --at;
      }
      order[at] = i;
    }
    const int pick = QueuePick(queue_, n);
    if (pick < 0) {
      AddWaits();
      return -1;
    }
    const int best = order[pick];
    for (int i = 0; i < kSlots; ++i) {
      if (i != best && state_[i] == Slot::kReady && serial_[i] < serial_[best]) {
        state_[i] = Slot::kFree;
        ++replaced_;
      }
    }
    if (shown_ >= 0) state_[shown_] = Slot::kFree;
    state_[best] = Slot::kShown;
    shown_ = best;
    shown_delay_ = waits_[best];
    AddWaits();
    return best;
  }
  // The delay of the shown slot in takes (DelayBin).
  int ShownDelay() const { return shown_delay_; }
  // The game ended: no slot shows, and each ready slot is free. A slot in
  // writing stays with the play thread.
  void Clear() {
    for (int i = 0; i < kSlots; ++i) {
      if (state_[i] == Slot::kReady || state_[i] == Slot::kShown) state_[i] = Slot::kFree;
    }
    shown_ = -1;
    shown_delay_ = 0;
  }

  Slot State(int i) const { return state_[i]; }
  uint64_t Serial(int i) const { return serial_[i]; }
  int Shown() const { return shown_; }
  // The pictures that the play thread dropped (no free slot), and the
  // pictures that a newer picture replaced before a frame of the session.
  uint64_t Dropped() const { return dropped_; }
  uint64_t Replaced() const { return replaced_; }

 private:
  void AddWaits() {
    for (int i = 0; i < kSlots; ++i) {
      if (state_[i] == Slot::kReady) ++waits_[i];
    }
  }

  bool queue_ = false;
  int count_ = kNewestSlots;
  Slot state_[kSlots] = {Slot::kFree, Slot::kFree, Slot::kFree, Slot::kFree};
  uint64_t serial_[kSlots] = {0, 0, 0, 0};
  int waits_[kSlots] = {0, 0, 0, 0};  // the takes since the submit of the ready slot
  uint64_t next_serial_ = 1;
  int shown_ = -1;
  int shown_delay_ = 0;
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
  // True when Pick(next_done) gives a new entry, so the caller can make its
  // command buffer and its fence first.
  bool WillGrow(bool next_done) const {
    return count_ == 0 || (!next_done && count_ < most_);
  }
  // next_done: the GPU finished the entry Next(), or it has no submit yet.
  // can_grow: false when the caller could not make a new entry. Then the
  // least recently used entry comes with a wait. With no entry at all the
  // index is -1.
  CommandPick Pick(bool next_done, bool can_grow = true) {
    CommandPick p;
    p.index = next_;
    if (!can_grow && count_ == 0) {
      p.index = -1;
      return p;
    }
    if (can_grow && WillGrow(next_done)) {
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

// THE COPIES RECORDED ONE TIME (xr_copy_once): the entries of a set name
// images of the ring of the pictures. They stay valid only while their
// source has the size of the set and the ring made and freed no image since
// the entries were recorded: a new image can have the handle of a freed one.
inline bool OnceSetValid(uint32_t set_width, uint32_t set_height, uint64_t set_changes,
                         uint32_t width, uint32_t height, uint64_t changes) {
  return set_width == width && set_height == height && set_changes == changes;
}

// THE AGE OF THE SHOWN PICTURE in frames of the session: 0 in the frame that
// shows a new picture, one more in each later frame with the same picture.
inline uint64_t NextAge(uint64_t age, bool fresh) { return fresh ? 0 : age + 1; }

// THE PARTS OF THE AGE OF A NEW PICTURE (the time log, docs/android.md, "The
// time log of the app"). The times of one picture on CLOCK_MONOTONIC in ms,
// 0 where a time is not known:
//   input    the app wrote the slot of the input channel that the frame of
//            the mod read
//   arrival  the newest frame of the picture came to the reader of the app
//   play     the play of its batch started
//   ready    the copies of the picture into a slot of the ring went to the
//            GPU (the old loop: the end of the play)
//   taken    the session took the picture
//   ended    xrEndFrame of the frame that shows it first returned
//   display  the display time of that frame
struct AgeTimes {
  double input = 0.0;
  double arrival = 0.0;
  double play = 0.0;
  double ready = 0.0;
  double taken = 0.0;
  double ended = 0.0;
  double display = 0.0;
};
// The parts from one time to the next: 0 the game (the input to the
// arrival: the frame of the mod, its walks and the handover), 1 the reader
// (to the play), 2 the play (to the copies into the slot), 3 the ring (the
// GPU and the wait for a frame of the session), 4 the session (to the end of
// xrEndFrame), 5 the runtime (to the display time). A part with a time that
// is not known is -1. The result is the age from the input to the display
// time, or -1 when one of the two is not known.
constexpr int kAgeParts = 6;
inline double AgeParts(const AgeTimes& t, double parts[kAgeParts]) {
  const double at[kAgeParts + 1] = {t.input, t.arrival, t.play, t.ready,
                                    t.taken, t.ended,   t.display};
  for (int i = 0; i < kAgeParts; ++i) {
    parts[i] = at[i] > 0.0 && at[i + 1] > 0.0 ? at[i + 1] - at[i] : -1.0;
  }
  return t.input > 0.0 && t.display > 0.0 ? t.display - t.input : -1.0;
}

// THE QUEUE AT THE GPU (frame_loop_gpu_depth of bwvr.ini, 1 by default). The
// play thread starts the play of a new picture only while fewer than `depth`
// pictures are not finished on the GPU. Until then the reader keeps the
// frames of the game, and the next batch has the newest of them. 0 (or a
// value below 0) is no limit: the play thread plays each batch at once.
inline int GpuDepth(int key) { return key > 0 ? key : 0; }
inline bool MayPlay(int depth, int unfinished) { return depth <= 0 || unfinished < depth; }

// THE PIXELS OF THE CPU of a layer (XrApp::PixelsFor). With
// xr_staging_direct=1 (key) new pixels go straight into the staging buffer
// when no copy in memory waits (dirty) and the GPU finished the last copy of
// the buffer (done). Else they go into the copy in memory.
inline bool Direct(bool key, bool dirty, bool done) { return key && !dirty && done; }

// THE RECT OF A COPY of the pixels of the CPU (xr_layer_rect of bwvr.ini, 1
// by default): the rect x, y, width, height of the image of width by height
// that the quad shows, cut to the image. The whole image when the key is off,
// when the rect has no width or no height, or when no part of it is in the
// image.
// A card with xr_layer_rect=1 (key) shows only when the image that its
// swapchain released has the rect of its quad: a copy of a rect leaves the
// rest of the image undefined, and a copy that did not happen (a busy staging
// buffer or swapchain image) leaves the rect of the picture before.
inline bool RectShows(bool key, const int32_t released[4], const int32_t rect[4]) {
  if (!key) return true;
  for (int i = 0; i < 4; ++i) {
    if (released[i] != rect[i]) return false;
  }
  return true;
}
struct Rect {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};
inline Rect CopyRect(bool key, const int32_t rect[4], uint32_t width, uint32_t height) {
  Rect whole;
  whole.width = width;
  whole.height = height;
  if (!key || rect[2] <= 0 || rect[3] <= 0) return whole;
  const int64_t x0 = rect[0] < 0 ? 0 : rect[0];
  const int64_t y0 = rect[1] < 0 ? 0 : rect[1];
  int64_t x1 = static_cast<int64_t>(rect[0]) + rect[2];
  int64_t y1 = static_cast<int64_t>(rect[1]) + rect[3];
  if (x1 > width) x1 = width;
  if (y1 > height) y1 = height;
  if (x1 <= x0 || y1 <= y0) return whole;
  Rect r;
  r.x = static_cast<uint32_t>(x0);
  r.y = static_cast<uint32_t>(y0);
  r.width = static_cast<uint32_t>(x1 - x0);
  r.height = static_cast<uint32_t>(y1 - y0);
  return r;
}

}  // namespace frame_loop
