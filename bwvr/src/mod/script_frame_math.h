// The frame that a step of the scripted controller, or a test key with a
// frame, compares with its own frame (the test key
// fake_controller_play_frame). See docs/controllers.md, 4.3. No Windows and
// no game here, so the math test compiles this file.

#pragma once

#include <stdint.h>

namespace script_frame_math {

// The shift of the script: how many frames later than `play` the controller
// first placed the Hand. hand_frame: that frame. play:
// fake_controller_play_frame. play_after: fake_controller_play_after. With
// play_after at 1000 or more, a script keeps its frames when the Hand comes
// before the frame `play`: the shift is then 0. A run that loads a test save
// has a small play_after, and its shift is less than 0.
inline int64_t Shift(uint64_t hand_frame, int play, int play_after) {
  const int64_t shift = static_cast<int64_t>(hand_frame) - play;
  return shift < 0 && play_after >= 1000 ? 0 : shift;
}

// The number that a step or a test key with the frame `own` compares with
// `own`, when the frame of the game is `frame`. play: 0 or less is off, and
// the number is `frame`. A frame `own` before `play` keeps its number too.
// From `play` on the number is `frame` less the shift, and 0 while the shift
// is not known: such a step waits for the Hand.
inline uint64_t Counted(uint64_t frame, uint64_t own, int play, bool known,
                        int64_t shift) {
  if (play <= 0 || own < static_cast<uint64_t>(play)) return frame;
  if (!known) return 0;
  const int64_t counted = static_cast<int64_t>(frame) - shift;
  return counted > 0 ? static_cast<uint64_t>(counted) : 0;
}

// THE SECOND ANCHOR (the test key fake_controller_temple_frame). The steps
// from the frame `temple` on count from the frame at which the ray of the
// Temple was on for kTempleSteadyFrames frames in a row, and that frame is
// `temple`. The guide of the first visit of the hall has a length in
// seconds, so its end comes at a different frame in each run. The first
// anchor counts first: its number is the frame for the second anchor. While
// the first anchor waits (0), the second one waits too.
const int kTempleSteadyFrames = 30;

inline uint64_t CountedTwice(uint64_t frame, uint64_t own, int play,
                             bool known, int64_t shift, int temple,
                             bool temple_known, int64_t temple_shift) {
  const uint64_t first = Counted(frame, own, play, known, shift);
  if (first == 0) return 0;
  return Counted(first, own, temple, temple_known, temple_shift);
}

}  // namespace script_frame_math
