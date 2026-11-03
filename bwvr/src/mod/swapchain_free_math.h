// WHEN THE HEADSET APP FREES A SET OF SWAPCHAINS (xr_resize_free,
// docs/android.md, "A new render size in the same process"). The rules are
// pure, with no Android code, so a test can check them.
//
// The swapchains of the app have the size of the render of the game. The
// player can set another size in the menu of the app and play again in the
// same process. A set that keeps its old size needs the old way for each
// picture, and the runtime can refuse the second set of the new size
// (XR_ERROR_LIMIT_REACHED). So the app frees a set of another size and makes
// it again at the new size.
#pragma once

#include <stdint.h>

namespace swapchain_free_math {

// The most replacements of the swapchains of one slot in one Play. A render
// size is fixed in a Play, so more than a few mean a fault, and then the
// slot keeps its set and its pictures take the old way.
constexpr int kMostReplaces = 8;

// The kinds of set that a new Play checks.
enum Kind : int {
  kEyes = 0,    // the projection layer of the old way
  kScreen,      // the stereo screen
  kPanel,       // the panel image of the renderer
  kFlat,        // the screen quad of the frames with no scene
  kDirect,      // the swapchains of a slot (xr_eye_direct=1), by the size of its pictures
  kKinds
};

// The size that a Play with a render of width by height wants of a set. The
// stereo screen has one quarter of the pixels. A set of a slot is made for
// the size of its pictures, so it is that size too.
inline void WantedSize(Kind kind, uint32_t width, uint32_t height, uint32_t* out_width,
                       uint32_t* out_height) {
  if (kind == kScreen) {
    *out_width = width / 2 > 0 ? width / 2 : 1;
    *out_height = height / 2 > 0 ? height / 2 : 1;
    return;
  }
  *out_width = width;
  *out_height = height;
}

// A new Play frees a set that is made when its size is not the wanted size.
// key: xr_resize_free (0: the sets stay as they are).
inline bool FreeAtPlay(bool key, bool made, uint32_t made_width, uint32_t made_height,
                       Kind kind, uint32_t width, uint32_t height) {
  if (!key || !made) return false;
  uint32_t want_width = 0, want_height = 0;
  WantedSize(kind, width, height, &want_width, &want_height);
  return made_width != want_width || made_height != want_height;
}

// A new Play lets the app try again a set that could not be made (the
// failed flag), also with a size that did not change: the first fault can
// come from the old sets that held the memory.
inline bool ClearFailedAtPlay(bool key) { return key; }

// A picture that goes straight into the swapchains of its slot does not fit
// in them (eye_copy_math::DirectFits is false): the slot frees them and
// makes two at the size of the picture. `replaced`: the replacements of the
// slot since the Play started.
inline bool ReplaceDirect(bool key, bool fits, int replaced) {
  return key && !fits && replaced >= 0 && replaced < kMostReplaces;
}

}  // namespace swapchain_free_math
