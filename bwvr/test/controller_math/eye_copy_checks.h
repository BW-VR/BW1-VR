// The copies of the eye pictures in the headset app, a test
// (src/mod/eye_copy_math.h). main.cpp includes this file after Check and
// Near, just before main.
#pragma once

#include <math.h>

#include "../../src/mod/eye_copy_math.h"
#include "../../src/mod/fovea_res_math.h"

namespace eye_copy_checks {

inline void EyeCopyChecks() {
  namespace ec = eye_copy_math;
  using fovea_res_math::Atlas;
  using fovea_res_math::kAtlasGap;
  using fovea_res_math::Layout;
  printf("\nThe copies of the eye pictures of the headset app\n");

  // THE SHOWN PART with no atlas: the whole picture.
  Atlas none = {};
  ec::Rect r = ec::ShownPart(none, 1380, 1104);
  Check(r.x == 0 && r.y == 0 && r.w == 1380 && r.h == 1104,
        "with no atlas the shown part is the whole picture", r.w, 1380);
  r = ec::CopyPart(false, none, 1380, 1104);
  Check(r.w == 1380 && r.h == 1104, "xr_eye_rects=0: a copy takes the whole picture", r.w, 1380);
  Check(ec::Bytes(r, 2) == 1380ull * 1104ull * 8ull, "the bytes of two layers of RGBA8",
        static_cast<double>(ec::Bytes(r, 2)), 1380.0 * 1104.0 * 8.0);

  // An atlas with the centre to the right of the outer image, at the render
  // of the Steam Frame (4096 by 3456) with the outer image at 25 percent and
  // the centre at 200 percent for 22 degrees.
  const float half = 1.0602f;
  const float th = tanf(half), tv = th * 3456.0f / 4096.0f;
  Layout l = fovea_res_math::MakeLayout(4096, 3456, th, tv, 0.0f, 0.0f, 25.0f, 200.0f, 22.0f);
  Atlas a = fovea_res_math::PackAtlas(4096, 3456, &l);
  Check(a.on, "the atlas of 25 and 200 percent fits in 4096 by 3456", a.on ? 1 : 0, 1);
  r = ec::ShownPart(a, 4096, 3456);
  const int32_t want_w = a.center[0] + a.center[2] + kAtlasGap;
  const int32_t tall = a.outer[3] > a.center[3] ? a.outer[3] : a.center[3];
  Check(a.center[1] == 0 && r.x == 0 && r.y == 0 && r.w == want_w && r.h == tall + kAtlasGap,
        "the shown part holds the outer rect, the centre rect to its right and the gap",
        r.w, want_w);
  Check(ec::CopyPart(true, a, 4096, 3456).w == r.w, "xr_eye_rects=1: a copy takes the shown part",
        ec::CopyPart(true, a, 4096, 3456).w, r.w);
  const double part = static_cast<double>(ec::Bytes(r, 2)) /
                      static_cast<double>(ec::Bytes(ec::Whole(4096, 3456), 2));
  printf("  the shown part of 4096 by 3456 with 25 and 200 percent at 22 degrees: %d by %d, "
         "%.1f percent of the bytes\n", r.w, r.h, 100.0 * part);
  Check(part < 0.40, "the shown part is less than 40 percent of the bytes of the picture",
        part, 0.40);
  // Each pixel that a layer can read is in the part: the outer rect, the
  // centre rect, and one pixel past each edge.
  Check(a.outer[0] + a.outer[2] + 1 <= r.w && a.outer[1] + a.outer[3] + 1 <= r.h &&
            a.center[0] - 1 >= 0 && a.center[0] + a.center[2] + 1 <= r.w &&
            a.center[1] + a.center[3] + 1 <= r.h,
        "the part has one pixel past each edge of each rect", r.w, a.center[0] + a.center[2] + 1);

  // A centre under the outer image.
  Atlas below = a;
  below.center[0] = 0;
  below.center[1] = a.outer[3] + kAtlasGap;
  below.center[2] = 600;
  below.center[3] = 500;
  r = ec::ShownPart(below, 4096, 3456);
  Check(r.w == a.outer[2] + kAtlasGap && r.h == below.center[1] + 500 + kAtlasGap,
        "a centre under the outer image: the part ends under it", r.h,
        below.center[1] + 500 + kAtlasGap);
  // A part that reaches the edge of the picture is cut to it.
  Atlas edge = a;
  edge.center[0] = 4096 - edge.center[2];
  r = ec::ShownPart(edge, 4096, 3456);
  Check(r.w == 4096, "a part at the right edge of the picture is cut to the picture", r.w, 4096);
  // An atlas of another picture is no atlas of this one.
  r = ec::ShownPart(a, 2048, 1728);
  Check(r.w == 2048 && r.h == 1728, "an atlas of another size: the whole picture", r.w, 2048);

  // THE EYES STRAIGHT INTO THE SWAPCHAINS.
  uint32_t sw = 0, sh = 0;
  ec::DirectSize(a, 4096, 3456, &sw, &sh);
  const ec::Rect shown_part = ec::ShownPart(a, 4096, 3456);
  Check(sw == static_cast<uint32_t>(shown_part.w) && sh == static_cast<uint32_t>(shown_part.h),
        "the swapchains of a slot have the size of the shown part", sw, shown_part.w);
  ec::DirectSize(none, 1380, 1104, &sw, &sh);
  Check(sw == 1380 && sh == 1104, "with no atlas the swapchains have the size of the picture",
        sw, 1380);
  ec::DirectSize(a, 4096, 3456, &sw, &sh);
  Check(ec::DirectFits(sw, sh, a, 4096, 3456), "a picture with the same atlas fits", 1, 1);
  Check(!ec::DirectFits(sw, sh, none, 4096, 3456),
        "a picture with no atlas does not fit in swapchains of the shown part", 0, 0);
  Check(!ec::DirectFits(sw - 1, sh, a, 4096, 3456), "swapchains one pixel too narrow: no fit", 0,
        0);
  Check(ec::DirectFits(1380, 1104, none, 1380, 1104),
        "a picture with no atlas fits in swapchains of its size", 1, 1);
  Check(!ec::DirectFits(1380, 1104, none, 1350, 1080),
        "a picture with no atlas of another size does not fit", 0, 0);
  Check(!ec::DirectFits(0, 0, none, 1380, 1104), "no swapchains: no fit", 0, 0);

  // THE EVEN RATE.
  Check(ec::Hold(-1) == 0 && ec::Hold(0) == 0 && ec::Hold(2) == 2 && ec::Hold(99) == ec::kMostHold,
        "frame_loop_hold: below 0 is 0, above the most is the most", ec::Hold(99), ec::kMostHold);
  Check(ec::HoldTakes(0, 1, true) && ec::HoldTakes(1, 1, true),
        "frame_loop_hold 0 and 1: a new picture at each frame", 1, 1);
  Check(ec::HoldTakes(2, 0, false), "no picture showed yet: the session takes one", 1, 1);
  // Pictures at each frame of the session, hold 2: each shows in 2 frames.
  {
    uint64_t shown = 0;
    bool any = false;
    int takes = 0, frames = 0;
    for (int f = 0; f < 72; ++f) {
      const bool take = ec::HoldTakes(2, shown, any);
      if (take) {
        ++takes;
        any = true;
      }
      shown = ec::NextShown(shown, take);
      ++frames;
    }
    Check(takes == 36, "hold 2 with a new picture at each frame: 36 pictures in 72 frames",
          takes, 36);
  }
  // A picture that is late at the frame of the take: the session shows the
  // old one again, and takes the new one at the next frame.
  {
    uint64_t shown = 0;
    bool any = false;
    const bool ready[6] = {true, false, false, true, true, true};
    int took[6];
    for (int f = 0; f < 6; ++f) {
      const bool take = ec::HoldTakes(2, shown, any) && ready[f];
      if (take) any = true;
      took[f] = take ? 1 : 0;
      shown = ec::NextShown(shown, take);
    }
    Check(took[0] == 1 && took[1] == 0 && took[2] == 0 && took[3] == 1 && took[4] == 0 &&
              took[5] == 1,
          "hold 2: a late picture shows at the first frame where it is ready, then 2 frames",
          took[3] + took[5], 2);
  }
  Check(ec::NextShown(5, true) == 1 && ec::NextShown(5, false) == 6,
        "the count of frames: 1 after a new picture, else one more",
        static_cast<double>(ec::NextShown(5, false)), 6);
}

}  // namespace eye_copy_checks
