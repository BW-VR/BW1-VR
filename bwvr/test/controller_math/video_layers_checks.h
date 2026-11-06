// THE LAYERS IN THE VIEW FOR VIDEOS (docs/backend.md, "The view for
// videos"): where a quad layer of the headset is in the left eye picture and
// in the area of the video (video_layers_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <string.h>

#include "../../src/mod/video_layers_math.h"
#include "video_view_checks.h"

namespace video_layers_checks {

inline void Uv(const video_layers_math::Map& m, float x, float y, float z,
               float out[3]) {
  const float p[3] = {x, y, z};
  video_layers_math::Apply(m, p, out);
}

inline void VideoLayersChecks() {
  using namespace video_layers_math;
  printf("\nThe layers in the view for videos\n");

  // THE EYE. At the origin, forward -z, half tangents of 1.6 across and
  // 1.25 down.
  const float origin[3] = {0.0f, 0.0f, 0.0f};
  const float ahead[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  const Map eye = EyeMap(origin, ahead, 1.6f, 1.25f);
  float h[3];
  Uv(eye, 0.0f, 0.0f, -2.0f, h);
  Near("a point straight ahead is in the middle across", h[0] / h[2], 0.5,
       1e-5);
  Near("a point straight ahead is in the middle down", h[1] / h[2], 0.5,
       1e-5);
  Near("W is the depth in front of the eye", h[2], 2.0, 1e-5);
  Uv(eye, 3.2f, 0.0f, -2.0f, h);
  Near("a point at the tangent of the half angle is at the right edge",
       h[0] / h[2], 1.0, 1e-5);
  Uv(eye, 0.0f, 2.5f, -2.0f, h);
  Near("a point up at the half angle is at the top edge", h[1] / h[2], 0.0,
       1e-5);
  Uv(eye, 0.0f, -1.25f, -2.0f, h);
  Near("a point down at half the tangent is three quarters down",
       h[1] / h[2], 0.75, 1e-5);
  Uv(eye, 0.0f, 0.0f, 1.0f, h);
  Check(h[2] < 0.0f, "a point behind the eye has W below 0", h[2], -1.0);

  // A head turned 90 degrees to the left looks along -x.
  float left90[4];
  video_view_checks::Head(90.0f, 0.0f, left90);
  const float at[3] = {1.0f, 1.7f, 0.5f};
  const Map turned = EyeMap(at, left90, 1.6f, 1.25f);
  Uv(turned, 1.0f - 3.0f, 1.7f, 0.5f, h);
  Near("a turned eye: its forward point is in the middle across",
       h[0] / h[2], 0.5, 1e-4);
  Near("a turned eye: its forward point is in the middle down", h[1] / h[2],
       0.5, 1e-4);
  Near("a turned eye: W is the distance", h[2], 3.0, 1e-4);
  // To the right of that eye is -z of the room.
  Uv(turned, 1.0f - 3.0f, 1.7f, 0.5f - 3.0f, h);
  Near("a turned eye: a point 45 degrees to its right", h[0] / h[2],
       0.5 + 0.5 / 1.6, 1e-4);

  // A map is linear in the point, so the middle of a quad goes where the
  // homogeneous mean of its corners goes: the texture has the perspective.
  float corners[4][3];
  float tilt[4];
  video_view_checks::Head(30.0f, -40.0f, tilt);
  const float quad_at[3] = {0.3f, -0.4f, -1.2f};
  Corners(quad_at, tilt, 0.4f, 0.2f, corners);
  float sum[3] = {0.0f, 0.0f, 0.0f};
  for (int c = 0; c < 4; ++c) {
    float q[3];
    Apply(eye, corners[c], q);
    for (int i = 0; i < 3; ++i) sum[i] += q[i];
  }
  float mid[3];
  Apply(eye, quad_at, mid);
  Near("the middle of a tilted quad, across", sum[0] / sum[2],
       mid[0] / mid[2], 1e-5);
  Near("the middle of a tilted quad, down", sum[1] / sum[2], mid[1] / mid[2],
       1e-5);

  // THE CORNERS: top left, top right, bottom left, bottom right, with the
  // top of the picture at +y of the quad.
  const float front[3] = {0.0f, 0.0f, -1.0f};
  Corners(front, ahead, 0.4f, 0.2f, corners);
  Near("corner 0 is at the top left, x", corners[0][0], -0.2, 1e-6);
  Near("corner 0 is at the top left, y", corners[0][1], 0.1, 1e-6);
  Near("corner 3 is at the bottom right, x", corners[3][0], 0.2, 1e-6);
  Near("corner 3 is at the bottom right, y", corners[3][1], -0.1, 1e-6);
  Uv(eye, corners[0][0], corners[0][1], corners[0][2], h);
  Check(h[0] / h[2] < 0.5f && h[1] / h[2] < 0.5f,
        "the top left corner is left of and above the middle", h[0] / h[2],
        0.4);

  // THE SCREEN. A screen 2 by 1 m, 2 m in front of the eye, that shows all
  // of the eye picture.
  const float screen_at[3] = {0.0f, 0.0f, -2.0f};
  const float all[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  Map screen;
  Check(ScreenMap(origin, screen_at, ahead, 2.0f, 1.0f, all, &screen),
        "a screen in front of the eye has a map", 1.0, 1.0);
  Uv(screen, 0.5f, 0.25f, -2.0f, h);
  Near("a point on the screen: across", h[0] / h[2], 0.75, 1e-5);
  Near("a point on the screen: down", h[1] / h[2], 0.25, 1e-5);
  Near("a point on the screen has W 1", h[2], 1.0, 1e-5);
  Uv(screen, 0.25f, 0.125f, -1.0f, h);
  Near("half way to the screen on the same line: the same place across",
       h[0] / h[2], 0.75, 1e-5);
  Near("half way to the screen: W is one half", h[2], 0.5, 1e-5);
  Uv(screen, 0.0f, 0.0f, 0.5f, h);
  Check(h[2] < 0.0f, "a point behind the eye: W below 0", h[2], -0.25);
  const float part[4] = {0.1f, 0.2f, 0.9f, 0.6f};
  Map screen_part;
  ScreenMap(origin, screen_at, ahead, 2.0f, 1.0f, part, &screen_part);
  Uv(screen_part, 0.5f, 0.25f, -2.0f, h);
  Near("a screen of a part: across", h[0] / h[2], 0.1 + 0.75 * 0.8, 1e-5);
  Near("a screen of a part: down", h[1] / h[2], 0.2 + 0.25 * 0.4, 1e-5);
  // An eye to the side of a turned screen.
  float screen_turn[4];
  video_view_checks::Head(-30.0f, 0.0f, screen_turn);
  const float side_eye[3] = {0.4f, 0.1f, 0.3f};
  Map side;
  Check(ScreenMap(side_eye, screen_at, screen_turn, 2.0f, 1.0f, all, &side),
        "a turned screen in front of an eye to the side has a map", 1.0,
        1.0);
  float screen_corners[4][3];
  Corners(screen_at, screen_turn, 2.0f, 1.0f, screen_corners);
  Uv(side, screen_corners[1][0], screen_corners[1][1], screen_corners[1][2],
     h);
  Near("the top right corner of a turned screen: across", h[0] / h[2], 1.0,
       1e-4);
  Near("the top right corner of a turned screen: down", h[1] / h[2], 0.0,
       1e-4);
  const float behind_eye[3] = {0.0f, 0.0f, -3.0f};
  Check(!ScreenMap(behind_eye, screen_at, ahead, 2.0f, 1.0f, all, &side),
        "an eye behind the screen has no map", 0.0, 0.0);

  // THE AREA AND THE WINDOW. The area is the middle half of the picture.
  const float area[4] = {0.25f, 0.25f, 0.75f, 0.75f};
  const float centre[3] = {1.0f, 1.0f, 2.0f};  // u = v = 0.5, W = 2
  float a[4];
  ToArea(centre, area, kNearMeters, a);
  Near("the middle of the picture is the middle of the area", a[0] / a[3],
       0.5, 1e-6);
  Near("z is W less the near limit", a[2], 2.0 - kNearMeters, 1e-6);
  const float window_all[4] = {0.0f, 0.0f, 1000.0f, 600.0f};
  float clip[4];
  AreaToClip(a, window_all, 1000.0f, 600.0f, clip);
  Near("an area on the whole window: the middle is at clip x 0",
       clip[0] / clip[3], 0.0, 1e-6);
  const float place[4] = {100.0f, 50.0f, 900.0f, 500.0f};
  const float corner_a[4] = {0.0f, 0.0f, 1.0f, 2.0f};
  AreaToClip(corner_a, place, 1000.0f, 600.0f, clip);
  Near("the top left of the area is at the left of its rect",
       clip[0] / clip[3], 2.0 * 100.0 / 1000.0 - 1.0, 1e-6);
  Near("the top left of the area is at the top of its rect",
       clip[1] / clip[3], 2.0 * 50.0 / 600.0 - 1.0, 1e-6);

  // IN THE AREA OR NOT.
  float in[4][4];
  const float inside[4][4] = {{0.2f, 0.2f, 1.0f, 1.0f},
                              {0.4f, 0.2f, 1.0f, 1.0f},
                              {0.2f, 0.4f, 1.0f, 1.0f},
                              {0.4f, 0.4f, 1.0f, 1.0f}};
  memcpy(in, inside, sizeof(in));
  Check(InArea(in), "a quad in the area is drawn", 1.0, 1.0);
  for (int c = 0; c < 4; ++c) in[c][0] = -0.5f - 0.1f * c;
  Check(!InArea(in), "a quad to the left of the area is not drawn", 0.0,
        0.0);
  memcpy(in, inside, sizeof(in));
  for (int c = 0; c < 4; ++c) in[c][2] = -0.5f;
  Check(!InArea(in), "a quad behind the near limit is not drawn", 0.0, 0.0);
  memcpy(in, inside, sizeof(in));
  in[0][3] = -0.5f;
  in[0][2] = -0.51f;
  for (int c = 1; c < 4; ++c) in[c][0] = -0.5f;
  in[0][0] = -0.2f;
  Check(InArea(in), "a quad past the eye at one corner is drawn", 1.0, 1.0);

  // THE TEXTURE: half a texel inside the rect.
  const int rect[4] = {0, 0, 768, 320};
  float t[4];
  TextureRect(rect, 768.0f, 320.0f, t);
  Near("the left of the texture is half a texel in", t[0], 0.5 / 768.0, 1e-7);
  Near("the bottom of the texture is half a texel in", t[3],
       319.5 / 320.0, 1e-7);
}

}  // namespace video_layers_checks
