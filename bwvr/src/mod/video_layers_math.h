// THE LAYERS IN THE VIEW FOR VIDEOS (video_view.cpp, docs/backend.md, "The
// view for videos"): where a quad layer of the headset is in the left eye
// picture, and in the area of the video. Pure math, with checks in
// test/controller_math/video_layers_checks.h.
//
// A MAP takes a point of the LOCAL space (meters) to the homogeneous
// coordinates (U, V, W) of the left eye picture: u = U / W and v = V / W go
// from 0 to 1 across and down the picture. Each of U, V and W is linear in
// the point, so the GPU interpolates a texture across the quad with the
// perspective of the eye. W is above 0 for a point in front of the eye.
//
// Two maps:
//   EyeMap     a frame of the world: the eye picture has the pose of the
//              left eye and a symmetric projection.
//   ScreenMap  a screen in the room (the cut scene window, the menus in the
//              room): the left eye picture is on a quad, and a point goes
//              to the place on that quad where the eye sees it.

#pragma once

#include <math.h>

#include "video_view_math.h"

namespace video_layers_math {

struct Map {
  float row[3][4];  // U, V, W = row . (x, y, z, 1)
  float near_w;     // the least W that the video draws
};

// The near limit of a map of the eye: 1 cm in front of the eye.
const float kNearMeters = 0.01f;
// The near limit of a screen: 1 percent of the way from the eye to the
// screen.
const float kNearScreen = 0.01f;

inline void Apply(const Map& m, const float p[3], float out[3]) {
  for (int r = 0; r < 3; ++r) {
    out[r] = m.row[r][0] * p[0] + m.row[r][1] * p[1] + m.row[r][2] * p[2] +
             m.row[r][3];
  }
}

// The left eye at `position` with `orientation` (x y z w, x right, y up,
// forward -z). tan_x and tan_y: the tangents of the half angles of the
// picture across and down. A point at view coordinates (x, y, z) has the
// depth d = -z, and u = (x / d / tan_x + 1) / 2, v = (1 - y / d / tan_y) / 2.
inline Map EyeMap(const float position[3], const float orientation[4],
                  float tan_x, float tan_y) {
  // The rows of the inverse rotation: the axes of the eye in LOCAL space.
  const float ex[3] = {1.0f, 0.0f, 0.0f}, ey[3] = {0.0f, 1.0f, 0.0f},
              ez[3] = {0.0f, 0.0f, 1.0f};
  float right[3], up[3], back[3];
  video_view_math::Rotate(orientation, ex, right);
  video_view_math::Rotate(orientation, ey, up);
  video_view_math::Rotate(orientation, ez, back);
  // view x = right . (p - e), y = up . (p - e), d = -back . (p - e)
  float vx[4], vy[4], vd[4];
  for (int i = 0; i < 3; ++i) {
    vx[i] = right[i];
    vy[i] = up[i];
    vd[i] = -back[i];
  }
  vx[3] = -(right[0] * position[0] + right[1] * position[1] +
            right[2] * position[2]);
  vy[3] = -(up[0] * position[0] + up[1] * position[1] + up[2] * position[2]);
  vd[3] = back[0] * position[0] + back[1] * position[1] +
          back[2] * position[2];
  const float ax = 0.5f / tan_x, ay = 0.5f / tan_y;
  Map m;
  for (int i = 0; i < 4; ++i) {
    m.row[0][i] = ax * vx[i] + 0.5f * vd[i];
    m.row[1][i] = -ay * vy[i] + 0.5f * vd[i];
    m.row[2][i] = vd[i];
  }
  m.near_w = kNearMeters;
  return m;
}

// The left eye at eye_position, and the screen: a quad at screen_position
// with screen_orientation, width by height meters, that shows the part
// (x0, y0, x1, y1 from 0 to 1) of the eye picture. A point P goes to the
// place where the line from the eye through P meets the plane of the
// screen. With n the front of the screen and C its middle, the line meets
// the plane at E + k (P - E) with k = ((C - E) . n) / ((P - E) . n). So
// W = (P - E) . n / ((C - E) . n) is 1 on the screen and 0 at the eye.
// False when the eye is on the plane or behind the screen.
inline bool ScreenMap(const float eye_position[3],
                      const float screen_position[3],
                      const float screen_orientation[4], float width,
                      float height, const float part[4], Map* out) {
  const float ex[3] = {1.0f, 0.0f, 0.0f}, ey[3] = {0.0f, 1.0f, 0.0f},
              ez[3] = {0.0f, 0.0f, 1.0f};
  float ax[3], ay[3], n[3];
  video_view_math::Rotate(screen_orientation, ex, ax);
  video_view_math::Rotate(screen_orientation, ey, ay);
  video_view_math::Rotate(screen_orientation, ez, n);
  float ce[3];  // C - E
  for (int i = 0; i < 3; ++i) ce[i] = screen_position[i] - eye_position[i];
  const float depth = ce[0] * n[0] + ce[1] * n[1] + ce[2] * n[2];
  // The eye is on the front side, so (C - E) . n is below 0.
  if (!(depth < -1e-4f) || !(width > 0.0f) || !(height > 0.0f)) return false;
  // W = (P - E) . n / depth.
  float w[4];
  for (int i = 0; i < 3; ++i) w[i] = n[i] / depth;
  w[3] = -(n[0] * eye_position[0] + n[1] * eye_position[1] +
           n[2] * eye_position[2]) /
         depth;
  // On the plane, H - C = (E - C) + (P - E) / W. So W s = W (E - C) . ax +
  // (P - E) . ax, with s across the screen in meters, and the same for t up.
  float s[4], t[4];
  const float ec_x = -(ce[0] * ax[0] + ce[1] * ax[1] + ce[2] * ax[2]);
  const float ec_y = -(ce[0] * ay[0] + ce[1] * ay[1] + ce[2] * ay[2]);
  for (int i = 0; i < 3; ++i) {
    s[i] = ec_x * w[i] + ax[i];
    t[i] = ec_y * w[i] + ay[i];
  }
  s[3] = ec_x * w[3] - (ax[0] * eye_position[0] + ax[1] * eye_position[1] +
                        ax[2] * eye_position[2]);
  t[3] = ec_y * w[3] - (ay[0] * eye_position[0] + ay[1] * eye_position[1] +
                        ay[2] * eye_position[2]);
  // u = part x0 + (s / width + 1/2) (x1 - x0), and v = part y0 +
  // (1/2 - t / height) (y1 - y0), each times W.
  const float pw = part[2] - part[0], ph = part[3] - part[1];
  for (int i = 0; i < 4; ++i) {
    out->row[0][i] = (part[0] + 0.5f * pw) * w[i] + pw / width * s[i];
    out->row[1][i] = (part[1] + 0.5f * ph) * w[i] - ph / height * t[i];
    out->row[2][i] = w[i];
  }
  out->near_w = kNearScreen;
  return true;
}

// The corners of a quad of the headset: top left, top right, bottom left,
// bottom right (the order of a strip of two triangles). The top row of its
// picture is at +y of the quad.
inline void Corners(const float position[3], const float orientation[4],
                    float width, float height, float out[4][3]) {
  const float ex[3] = {0.5f * width, 0.0f, 0.0f};
  const float ey[3] = {0.0f, 0.5f * height, 0.0f};
  float ax[3], ay[3];
  video_view_math::Rotate(orientation, ex, ax);
  video_view_math::Rotate(orientation, ey, ay);
  const float sx[4] = {-1.0f, 1.0f, -1.0f, 1.0f};
  const float sy[4] = {1.0f, 1.0f, -1.0f, -1.0f};
  for (int c = 0; c < 4; ++c) {
    for (int i = 0; i < 3; ++i) {
      out[c][i] = position[i] + sx[c] * ax[i] + sy[c] * ay[i];
    }
  }
}

// A point of the eye picture (U, V, W) in the coordinates of the area of the
// video: the area is the part (x0, y0, x1, y1 from 0 to 1) of the eye
// picture, and x and y go from 0 to 1 across and down it (after the
// division by w). z is W less the near limit, so the GPU cuts the quad at
// that limit, as it cuts at z = 0.
inline void ToArea(const float uvw[3], const float area[4], float near_w,
                   float out[4]) {
  const float aw = area[2] - area[0], ah = area[3] - area[1];
  out[0] = (uvw[0] - area[0] * uvw[2]) / aw;
  out[1] = (uvw[1] - area[1] * uvw[2]) / ah;
  out[2] = uvw[2] - near_w;
  out[3] = uvw[2];
}

// The clip position in a window of win_w by win_h pixels of a point of the
// area (ToArea), when the area fills the rect x0, y0, x1, y1 of the window.
// Clip x = -1 at the left of the window and y = -1 at the top.
inline void AreaToClip(const float a[4], const float place[4], float win_w,
                       float win_h, float out[4]) {
  const float sx = 2.0f * (place[2] - place[0]) / win_w;
  const float ox = 2.0f * place[0] / win_w - 1.0f;
  const float sy = 2.0f * (place[3] - place[1]) / win_h;
  const float oy = 2.0f * place[1] / win_h - 1.0f;
  out[0] = sx * a[0] + ox * a[3];
  out[1] = sy * a[1] + oy * a[3];
  out[2] = a[2];
  out[3] = a[3];
}

// False when the quad cannot be in the area: each corner is behind the near
// limit, or each corner is past one side of the area. Points of the area
// are 0 <= x <= w and 0 <= y <= w. The test of a side holds only for a
// point in front of the eye (w above 0), so a quad with a corner behind the
// eye is not tested at its sides.
inline bool InArea(const float c[4][4]) {
  int behind = 0, at_eye = 0, left = 0, right = 0, top = 0, bottom = 0;
  for (int i = 0; i < 4; ++i) {
    const float x = c[i][0], y = c[i][1], z = c[i][2], w = c[i][3];
    if (z < 0.0f) ++behind;
    if (!(w > 0.0f)) ++at_eye;
    if (x < 0.0f) ++left;
    if (x > w) ++right;
    if (y < 0.0f) ++top;
    if (y > w) ++bottom;
  }
  if (behind == 4) return false;
  if (at_eye > 0) return true;
  return left < 4 && right < 4 && top < 4 && bottom < 4;
}

// The part of a texture that a quad shows, as texture coordinates. The rect
// is x, y, width, height in pixels. The coordinates stay half a texel inside
// the rect, so the linear filter does not read a pixel outside it.
inline void TextureRect(const int rect[4], float texture_w, float texture_h,
                        float out[4]) {
  const float x0 = static_cast<float>(rect[0]);
  const float y0 = static_cast<float>(rect[1]);
  const float x1 = x0 + static_cast<float>(rect[2]);
  const float y1 = y0 + static_cast<float>(rect[3]);
  out[0] = (x0 + 0.5f) / texture_w;
  out[1] = (y0 + 0.5f) / texture_h;
  out[2] = (x1 - 0.5f) / texture_w;
  out[3] = (y1 - 0.5f) / texture_h;
}

}  // namespace video_layers_math
