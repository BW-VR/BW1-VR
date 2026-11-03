// The rules for freeing a set of swapchains in the headset app, a test
// (src/mod/swapchain_free_math.h). main.cpp includes this file after Check
// and Near, just before main.
#pragma once

#include "../../src/mod/eye_copy_math.h"
#include "../../src/mod/fovea_res_math.h"
#include "../../src/mod/swapchain_free_math.h"

namespace swapchain_free_checks {

inline void SwapchainFreeChecks() {
  namespace sf = swapchain_free_math;
  printf("\nWhen the headset app frees a set of swapchains\n");

  // THE WANTED SIZE of each kind for a render of 2808 by 2246.
  uint32_t w = 0, h = 0;
  sf::WantedSize(sf::kEyes, 2808, 2246, &w, &h);
  Check(w == 2808 && h == 2246, "the projection layer has the size of the render", w, 2808);
  sf::WantedSize(sf::kScreen, 2808, 2246, &w, &h);
  Check(w == 1404 && h == 1123, "the stereo screen has half the width and the height", w, 1404);
  sf::WantedSize(sf::kScreen, 1, 1, &w, &h);
  Check(w == 1 && h == 1, "the stereo screen is never smaller than one pixel", w, 1);
  sf::WantedSize(sf::kDirect, 1080, 864, &w, &h);
  Check(w == 1080 && h == 864, "the swapchains of a slot are made for the size of the picture", w,
        1080);

  // A NEW PLAY at the size of the sets: nothing goes. At another size: it goes.
  Check(!sf::FreeAtPlay(true, true, 1080, 864, sf::kEyes, 1080, 864),
        "a set of the same size stays", 0, 0);
  Check(sf::FreeAtPlay(true, true, 1080, 864, sf::kEyes, 2808, 2246),
        "a set of another size goes", 1, 1);
  Check(sf::FreeAtPlay(true, true, 2808, 2246, sf::kEyes, 1080, 864),
        "a set that is larger than the new render goes too", 1, 1);
  Check(sf::FreeAtPlay(true, true, 1080, 864, sf::kEyes, 1080, 865),
        "one row of difference counts", 1, 1);
  Check(!sf::FreeAtPlay(true, true, 540, 432, sf::kScreen, 1080, 864),
        "the stereo screen of half the render stays", 0, 0);
  Check(sf::FreeAtPlay(true, true, 540, 432, sf::kScreen, 2808, 2246),
        "the stereo screen of the old render goes (it is the small flat screen)", 1, 1);
  Check(!sf::FreeAtPlay(true, false, 540, 432, sf::kEyes, 2808, 2246),
        "a set that is not made has nothing to free", 0, 0);
  Check(!sf::FreeAtPlay(false, true, 1080, 864, sf::kEyes, 2808, 2246),
        "xr_resize_free=0: the sets stay as they are", 0, 0);
  Check(sf::ClearFailedAtPlay(true) && !sf::ClearFailedAtPlay(false),
        "the failed flags clear only with the key", 1, 1);

  // A PICTURE THAT DOES NOT FIT in the swapchains of its slot.
  fovea_res_math::Atlas none = {};
  const bool fits = eye_copy_math::DirectFits(1080, 864, none, 2808, 2246);
  Check(!fits, "a picture of 2808 by 2246 does not fit in swapchains of 1080 by 864", fits ? 1 : 0,
        0);
  Check(sf::ReplaceDirect(true, fits, 0), "the slot replaces its swapchains", 1, 1);
  Check(!sf::ReplaceDirect(false, fits, 0), "xr_resize_free=0: the slot keeps them", 0, 0);
  Check(!sf::ReplaceDirect(true, true, 0), "a picture that fits needs no replacement", 0, 0);
  Check(sf::ReplaceDirect(true, false, sf::kMostReplaces - 1),
        "the last allowed replacement runs", 1, 1);
  Check(!sf::ReplaceDirect(true, false, sf::kMostReplaces),
        "after the most replacements the slot keeps its set (the old way)", 0, 0);
  Check(!sf::ReplaceDirect(true, false, -1), "a count below zero is no count", 0, 0);

  // The set that the slot makes after the replacement fits the same picture.
  uint32_t sw = 0, sh = 0;
  eye_copy_math::DirectSize(none, 2808, 2246, &sw, &sh);
  Check(eye_copy_math::DirectFits(sw, sh, none, 2808, 2246),
        "the new set of the slot fits the picture", sw, 2808);
  // And a picture that is smaller than the set, with no atlas, needs a new set too.
  Check(sf::ReplaceDirect(true, eye_copy_math::DirectFits(2808, 2246, none, 1080, 864), 0),
        "a smaller picture with no atlas does not fit a larger set, so the slot replaces it", 1, 1);
}

}  // namespace swapchain_free_checks
