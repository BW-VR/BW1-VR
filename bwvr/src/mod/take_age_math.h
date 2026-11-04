// THE AGE OF A PICTURE IN THE LAYER OF THE HEADSET APP (take_age_log,
// docs/android.md, "The age of a picture in the layer"). No Windows code and
// no Android code, so the app and the math test use it
// (test/controller_math/take_age_checks.h).
//
// The headset must never show a picture of the land that is older than the
// picture it showed before. Four things can make that happen, and this file
// names each one so that a log line can say which one it was:
//
//   - A TAKE of the session picks a picture with a frame number below the
//     shown one (kOlderFrame), or with a view of an older frame (kOlderView).
//   - A picture is OLDER THAN ITS FRAME NUMBER (kViewLags): the newest frame
//     of its batch had no walk, so the eye pictures and the view are of an
//     earlier frame.
//   - A picture has no eye pictures at all (kNoEyes): the layer has nothing to
//     name.
//   - A LAYER names a pair of pictures of two frames, or a frame below the
//     layer before (the pair and the layer rules of Layer).
//   - The PLAY THREAD writes the swapchains of a slot soon after a frame named
//     them (Writes): a runtime that reads the swapchain late would show the
//     new, unfinished picture or an image of an older write.
#pragma once

#include <stdint.h>

namespace take_age {

// The reasons of one take, as bits.
enum Reason : uint32_t {
  kNew = 0,
  kOlderFrame = 1u << 0,   // the frame number is below the frame of the shown picture
  kOlderView = 1u << 1,    // the view is of a frame below the view of the shown picture
  kSameFrame = 1u << 2,    // the same frame number as the shown picture
  kViewLags = 1u << 3,     // the view is of a frame below the frame number of the picture
  kNoEyes = 1u << 4,       // no eye pictures
};
constexpr int kReasons = 5;

// The name of a reason bit (0 to kReasons - 1), for the log.
inline const char* ReasonName(int bit) {
  switch (bit) {
    case 0: return "a frame below the picture before";
    case 1: return "a view of a frame below the view before";
    case 2: return "the same frame again";
    case 3: return "a view of a frame below its own frame number (the newest frame of its batch had no walk)";
    case 4: return "no eye pictures";
    default: return "";
  }
}

// What a take knows about its picture.
struct TakeFacts {
  bool screen = false;      // a screen with no scene (its frame numbers are above kScreenFrameBase)
  bool eyes = false;        // eye pictures (a world picture), or the screen picture (a screen)
  uint64_t frame = 0;       // the newest frame of the batch of the picture
  bool have_view = false;   // the picture has a view
  uint64_t view_frame = 0;  // the frame of that view
};

// The shown picture, as the last take that had eye pictures left it.
struct Shown {
  bool any = false;
  bool screen = false;
  uint64_t frame = 0;
  bool have_view = false;
  uint64_t view_frame = 0;
};

// The reasons of a take (the bits of Reason, 0 for a new picture). A screen
// and a world picture have their own frame numbers, so a take that changes
// the kind compares nothing.
inline uint32_t Classify(const Shown& before, const TakeFacts& t) {
  uint32_t r = kNew;
  if (!t.eyes) return kNoEyes;
  const bool same_kind = before.any && before.screen == t.screen;
  if (same_kind) {
    if (t.frame < before.frame) r |= kOlderFrame;
    if (t.frame == before.frame) r |= kSameFrame;
    if (t.have_view && before.have_view && t.view_frame < before.view_frame) r |= kOlderView;
  }
  if (!t.screen && t.have_view && t.view_frame < t.frame) r |= kViewLags;
  return r;
}

// The shown picture after a take. A take with no eyes leaves the shown
// picture as it was.
inline Shown After(const Shown& before, const TakeFacts& t) {
  if (!t.eyes) return before;
  Shown s;
  s.any = true;
  s.screen = t.screen;
  s.frame = t.frame;
  s.have_view = t.have_view;
  s.view_frame = t.view_frame;
  return s;
}

// The frames that a view lags behind its frame number (0 with none).
inline uint64_t Lag(const TakeFacts& t) {
  return !t.screen && t.have_view && t.view_frame < t.frame ? t.frame - t.view_frame : 0;
}

// THE LAYER OF A FRAME OF THE SESSION: the frame of the picture of each eye.
// The two eyes must show the same frame, and the frame must not be below the
// frame of the layer before.
enum LayerReason : uint32_t {
  kLayerOk = 0,
  kPairDiffers = 1u << 0,  // the two eyes show two frames
  kLayerOlder = 1u << 1,   // a frame below the layer before
};

class Layer {
 public:
  // The reasons of a layer with the frames `left` and `right`. The layer
  // before is the last layer that Check saw.
  uint32_t Check(uint64_t left, uint64_t right) {
    uint32_t r = kLayerOk;
    if (left != right) r |= kPairDiffers;
    const uint64_t low = left < right ? left : right;
    if (any_ && low < last_) r |= kLayerOlder;
    last_ = left > right ? left : right;
    any_ = true;
    return r;
  }
  // The frame number of the layer before.
  uint64_t Last() const { return last_; }
  // A new game or a screen: the layer before does not count.
  void Reset() { any_ = false; }

 private:
  bool any_ = false;
  uint64_t last_ = 0;
};

// THE WRITES OF THE PLAY THREAD into the swapchains of a slot. A frame of the
// session names the swapchains of the slot of its picture. The play thread
// writes them again after the slot is free. The gap in frames of the session
// from the last frame that named them to the start of the write, and the time
// from the display time of that frame to the start of the write (negative:
// the frame is not displayed yet), say how soon that is.
constexpr int kSlots = 4;
constexpr int kGapBins = 4;  // 0, 1, 2, 3 or more frames

struct WriteStats {
  uint64_t writes = 0;           // writes into a slot that a frame named before
  uint64_t gap[kGapBins] = {0, 0, 0, 0};
  uint64_t before_display = 0;   // writes that started before the display time of the naming frame
  double least_ms = 0.0;         // the least time after the display time (negative: before it)
  bool have_least = false;
};

class Writes {
 public:
  // A frame of the session named the swapchains of `slot`. frame: the count of
  // the frames of the session so far (1 for the first), display_ms: the
  // display time of that frame on the clock of the app, or a value below 0.
  void Named(int slot, uint64_t frame, double display_ms) {
    if (slot < 0 || slot >= kSlots) return;
    named_[slot] = frame;
    display_[slot] = display_ms;
  }
  // The play thread starts a write into `slot` after `frames` frames of the
  // session and at `now_ms`. A slot that no frame named has no gap.
  void Write(int slot, uint64_t frames, double now_ms) {
    if (slot < 0 || slot >= kSlots || named_[slot] == 0) return;
    ++stats_.writes;
    const uint64_t gap = frames >= named_[slot] ? frames - named_[slot] : 0;
    ++stats_.gap[gap >= static_cast<uint64_t>(kGapBins) ? kGapBins - 1 : static_cast<int>(gap)];
    if (display_[slot] >= 0.0) {
      const double after = now_ms - display_[slot];
      if (after < 0.0) ++stats_.before_display;
      if (!stats_.have_least || after < stats_.least_ms) {
        stats_.least_ms = after;
        stats_.have_least = true;
      }
    }
  }
  // The stats since the last Take.
  WriteStats Take() {
    const WriteStats s = stats_;
    stats_ = WriteStats();
    return s;
  }
  // A new game: no slot is named.
  void Reset() {
    for (int i = 0; i < kSlots; ++i) {
      named_[i] = 0;
      display_[i] = -1.0;
    }
  }

 private:
  uint64_t named_[kSlots] = {0, 0, 0, 0};
  double display_[kSlots] = {-1.0, -1.0, -1.0, -1.0};
  WriteStats stats_;
};

// THE COUNTS OF A WINDOW of the log (10 s).
struct Window {
  uint64_t takes = 0;              // takes with a new picture
  uint64_t reasons[kReasons] = {0, 0, 0, 0, 0};
  uint64_t most_lag = 0;           // the most frames that a view lagged
  uint64_t layers = 0;             // frames of the session with a projection layer
  uint64_t pair_differs = 0;
  uint64_t layer_older = 0;
};

// The count of the reasons of one take into a window.
inline void Count(Window* w, uint32_t reasons, uint64_t lag) {
  ++w->takes;
  for (int i = 0; i < kReasons; ++i) {
    if ((reasons & (1u << i)) != 0) ++w->reasons[i];
  }
  if (lag > w->most_lag) w->most_lag = lag;
}

// True for a take that the log names with a line of its own: a picture below
// the shown one, a lagging view, or no eyes. The same frame again is only a
// count.
inline bool NamesLine(uint32_t reasons) {
  return (reasons & (kOlderFrame | kOlderView | kViewLags | kNoEyes)) != 0;
}

// The most lines of one kind that the log writes in a run. The windows count
// the rest.
constexpr int kMostLines = 40;

}  // namespace take_age
