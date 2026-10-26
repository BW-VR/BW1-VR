// The sharpness of the middle in the headset: the middle as its own layer
// (fovea_res_math.h, PackAtlas, CenterFov, OuterRect) and the pixels of the
// panel for each pixel of an image (android/src/eye_size.h). main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include "../../android/src/eye_size.h"
#include "../../src/mod/fovea_res_math.h"

namespace sharp_checks {

inline void SharpChecks() {
  using namespace fovea_res_math;
  printf("\nThe sharpness of the middle: the middle as its own layer\n");

  // The picture of a Quest 3 at 1680 by 1760: a render of 2200 by 1760 with
  // the half angle 60.74 degrees of the game (one walk).
  const float half = 1.0602f;
  const float th = tanf(half), tv = th * 1760.0f / 2200.0f;
  const float down = DownGaze(9.0f);
  Layout l = MakeLayout(2200, 1760, th, tv, 0.0f, down, 20.0f, 195.0f, 22.0f);
  const Layout before = l;
  Atlas a = PackAtlas(2200, 1760, &l);
  Check(a.on && a.mres, "outer 20 and centre 195 percent at 22 degrees fit in 2200 by 1760",
        a.on ? 1 : 0, 1);
  Check(a.outer[0] == 0 && a.outer[1] == 0 && a.outer[2] == static_cast<int32_t>(l.outer_w) &&
            a.outer[3] == static_cast<int32_t>(l.outer_h),
        "the outer image is at the top left at its own size", a.outer[2], l.outer_w);
  Check(a.center[0] == a.outer[2] && a.center[1] == 0,
        "the centre image is to the right of the outer image", a.center[0], a.outer[2]);
  Check(l.center_w == before.center_w && l.center_h == before.center_h,
        "a centre that fits keeps its size", l.center_w, before.center_w);
  Check(a.center[0] + a.center[2] <= 2200 && a.center[1] + a.center[3] <= 1760,
        "the centre rect is in the eye picture", a.center[0] + a.center[2], 2200);
  Near("the rect of the centre in the view: x0 is inset_x0 of the picture", a.view[0],
       l.inset_x0 / 2200.0f, 1e-6);
  Near("the rect of the centre in the view: y1", a.view[3], (l.inset_y0 + l.inset_h) / 1760.0f,
       1e-6);
  Check(0.5f * (a.view[1] + a.view[3]) > 0.5f,
        "9 degrees down: the rect of the centre is below the middle of the picture",
        0.5f * (a.view[1] + a.view[3]), 0.5);

  // The same direction in both layers: a pixel of the picture in the rect of
  // the centre has the same tangent in the layer of the whole view and in
  // the centre layer.
  const float fov[4] = {-half, half, atanf(tv), -atanf(tv)};
  float cf[4];
  CenterFov(fov, a.view, cf);
  Check(cf[0] < cf[1] && cf[3] < cf[2], "the centre layer has angles of view", cf[1], cf[0]);
  for (int i = 0; i < 3; ++i) {
    const float px = l.inset_x0 + (0.1f + 0.4f * i) * l.inset_w;
    const float py = l.inset_y0 + (0.9f - 0.4f * i) * l.inset_h;
    const float outer_tx = tanf(fov[0]) + px / 2200.0f * (tanf(fov[1]) - tanf(fov[0]));
    const float outer_ty = tanf(fov[2]) - py / 1760.0f * (tanf(fov[2]) - tanf(fov[3]));
    const float u = (px - l.inset_x0) / l.inset_w, v = (py - l.inset_y0) / l.inset_h;
    const float center_tx = tanf(cf[0]) + u * (tanf(cf[1]) - tanf(cf[0]));
    const float center_ty = tanf(cf[2]) - v * (tanf(cf[2]) - tanf(cf[3]));
    Near("a pixel of the centre has the same tangent x in both layers", center_tx, outer_tx,
         1e-5);
    Near("a pixel of the centre has the same tangent y in both layers", center_ty, outer_ty,
         1e-5);
  }
  float whole[4];
  const float all[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  CenterFov(fov, all, whole);
  Near("a rect of the whole picture has the angles of the whole view", whole[0], fov[0], 1e-6);
  Near("a rect of the whole picture has the angles of the whole view: down", whole[3], fov[3],
       1e-6);

  // A centre that does not fit at its size gets a smaller size that fits:
  // the render of the test runtime at 50 percent, 1380 by 1104.
  Layout s = MakeLayout(1380, 1104, th, th * 1104.0f / 1380.0f, 0.0f, 0.0f, 50.0f, 200.0f,
                        25.0f);
  const uint32_t big_w = s.center_w;
  Atlas sa = PackAtlas(1380, 1104, &s);
  Check(sa.on && s.center_w < big_w,
        "outer 50 and centre 200 percent in 1380 by 1104: the centre gets smaller",
        s.center_w, big_w);
  Check(sa.center[0] + sa.center[2] <= 1380 && sa.center[1] + sa.center[3] <= 1104 &&
            (sa.center[0] >= sa.outer[2] || sa.center[1] >= sa.outer[3]),
        "the smaller centre is in the picture and not over the outer image",
        sa.center[0] + sa.center[2], 1380);
  Near("the scale of the smaller centre follows its size", s.center_scale_x,
       static_cast<float>(s.center_w) / s.inset_w, 1e-6);
  Near("the atlas has the scale of the smaller centre", sa.center_scale, s.center_scale_x,
       1e-6);
  // The outer image at the whole size leaves no room.
  Layout f = MakeLayout(1380, 1104, th, th * 1104.0f / 1380.0f, 0.0f, 0.0f, 100.0f, 150.0f,
                        25.0f);
  const Atlas fa = PackAtlas(1380, 1104, &f);
  Check(!fa.on && fa.mres,
        "outer 100 percent: no room for the centre, the eye picture is made as before",
        fa.on ? 1 : 0, 0);
  // A narrow picture with a tall outer image: the centre goes under it.
  Layout t = MakeLayout(1000, 3000, 0.5f, 1.5f, 0.0f, 0.0f, 90.0f, 100.0f, 10.0f);
  const Atlas ta = PackAtlas(1000, 3000, &t);
  Check(ta.on && ta.center[0] == 0 && ta.center[1] == static_cast<int32_t>(t.outer_h),
        "a picture with no room to the right: the centre is under the outer image",
        ta.center[1], t.outer_h);

  // The columns of the cut scene window in the outer image.
  const int32_t columns[4] = {100, 0, 1000, 1760};
  int32_t mapped[4];
  OuterRect(a, 2200, 1760, columns, mapped);
  Check(mapped[0] == 20 && mapped[1] == 0 && mapped[2] == 200 &&
            mapped[3] == static_cast<int32_t>(l.outer_h),
        "outer 20 percent: the columns 100 to 1100 are the columns 20 to 220 of the outer "
        "image", mapped[2], 200);
  Atlas off = a;
  off.on = false;
  OuterRect(off, 2200, 1760, columns, mapped);
  Check(mapped[0] == 100 && mapped[2] == 1000, "with no atlas the rect stays", mapped[2],
        1000);

  printf("\nThe sharpness of the middle: the pixels of the panel for a pixel\n");
  namespace es = eye_size;
  // The left eye of a Quest 3 (the log of the input channel): left -0.942,
  // right 0.698 radians.
  const float tl = tanf(-0.942f), tr = tanf(0.698f);
  const float panel = es::PanelMiddle(es::kPanelWidth, tl, tr);
  Near("the middle of the panel of a Quest 3: about 1212 pixels for a unit of tangent",
       panel, 1212.0, 1.0);
  const float render = es::PixelsPerTangent(2200.0f, -th, th);
  Near("a render of 2200 across the half angle 60.74 degrees: 616 pixels for a unit of "
       "tangent",
       render, 616.2, 0.5);
  Near("one pixel of that render covers about 1.97 pixels of the panel in the middle",
       es::PanelPerPixel(panel, render), 1.967, 0.005);
  Check(es::PanelPerPixel(panel, render * 2.0f) < 1.0f,
        "a centre of 200 percent reaches the panel in the middle",
        es::PanelPerPixel(panel, render * 2.0f), 1.0);
  Check(es::PixelsPerTangent(100.0f, 1.0f, 1.0f) == 0.0f &&
            es::PanelPerPixel(1000.0f, 0.0f) == 0.0f,
        "an empty span or an image with no pixels gives 0", 0, 0);
}

}  // namespace sharp_checks
