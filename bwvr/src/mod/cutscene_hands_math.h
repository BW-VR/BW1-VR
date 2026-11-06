// The hands in the cut scene window (docs/cutscene-window.md, "The hands in
// the window"): the pure math. No game code and no Direct3D here, so the math
// test can check it.
//
// THE ROOM. All the points are in the room of the hands: the frame of
// HeadPoseToRoom, in game units at the life size scale. The screen of the
// window is a rectangle in that room, and each eye sees its own eye picture
// on it (xr_screen.cpp, one quad for each eye).
//
// A HAND IN THE ROOM ON THE SCREEN. An eye at E sees a point P of the room
// along the line from E to P. That line meets the plane of the screen at one
// point. When each eye picture has P at the pixel of that point, the eye sees
// P in its true direction, and the two eyes together see it at its true
// place, also in front of the screen. EyeCamera makes this a camera of the
// form that the overlay projects with (overlay_math::Project):
//
//   x = (1 + cx / cz) * hx      y = (1 - cy / cz) * hy
//
// cz is the distance along the normal of the screen. A pixel of the eye
// picture is x = Ax + Bx * cx / cz and y = Ay - By * cy / cz, with the foot
// of the eye on the screen in Ax and Ay, and the distance of the eye from the
// screen in Bx and By. The camera rows take both into the form above.
//
// THE CLICK. The ray of the pointing hand meets the opening at one point. The
// scene there is on the plane of the screen, so it is at one place of the
// opening in both eye pictures. The game camera of a flat frame is the middle
// of the two eye cameras, so its pixel of that point is the mean of the two
// eye pixels.
#pragma once

#include <math.h>

namespace cutscene_hands_math {

// The screen of a flat frame in the room of the hands.
struct View {
  float center[3];   // the middle of the opening
  float right[3];    // unit axes. Forward goes into the screen, away from
  float up[3];       // the player
  float forward[3];
  float width;       // the size of the opening, in room units
  float height;
  float eye[2][3];   // 0 the left eye, 1 the right eye
  float column[2];   // the first column of each eye picture that shows
  float columns[2];  // how many columns of each eye picture show
  float rows;        // the rows of an eye picture: all of them show
};

inline float Dot(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// Where the ray from `origin` along `direction` meets the plane of the
// screen: u to the right and v up from the middle of the opening, in room
// units, and t along the ray. False when the ray goes away from the screen or
// along it. `inside` says if the point is in the opening.
inline bool HitPlane(const View& v, const float origin[3],
                     const float direction[3], float* u, float* w, float* t,
                     bool* inside) {
  const float along = Dot(direction, v.forward);
  if (!(along > 1e-6f)) return false;
  const float to[3] = {v.center[0] - origin[0], v.center[1] - origin[1],
                       v.center[2] - origin[2]};
  const float s = Dot(to, v.forward) / along;
  if (!(s > 0.0f)) return false;
  const float p[3] = {origin[0] + s * direction[0] - v.center[0],
                      origin[1] + s * direction[1] - v.center[1],
                      origin[2] + s * direction[2] - v.center[2]};
  *u = Dot(p, v.right);
  *w = Dot(p, v.up);
  *t = s;
  *inside = fabsf(*u) <= 0.5f * v.width && fabsf(*w) <= 0.5f * v.height;
  return true;
}

// The pixel of the eye picture of `eye` at the point u, v of the opening.
inline void EyePixel(const View& v, int eye, float u, float w, float* x,
                     float* y) {
  *x = v.column[eye] + (u / v.width + 0.5f) * v.columns[eye];
  *y = (0.5f - w / v.height) * v.rows;
}

// The pixel of the game camera of a flat frame at the point u, v of the
// opening: the mean of the two eye pixels.
inline void GamePixel(const View& v, float u, float w, float* x, float* y) {
  float lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f;
  EyePixel(v, 0, u, w, &lx, &ly);
  EyePixel(v, 1, u, w, &rx, &ry);
  *x = 0.5f * (lx + rx);
  *y = 0.5f * (ly + ry);
}

// The camera of one eye for the overlay, in the layout of
// overlay_math::ToCamera: m[0], m[3], m[6] give cx, m[1], m[4], m[7] give cy,
// m[2], m[5], m[8] give cz, and m[9], m[10], m[11] are added. hx and hy are
// half the size of the render. False when the eye is not in front of the
// screen.
inline bool EyeCamera(const View& v, int eye, float hx, float hy,
                      float m[12]) {
  if (!(hx > 0.0f) || !(hy > 0.0f) || !(v.width > 0.0f) ||
      !(v.height > 0.0f)) {
    return false;
  }
  const float* e = v.eye[eye];
  const float off[3] = {e[0] - v.center[0], e[1] - v.center[1],
                        e[2] - v.center[2]};
  const float d = -Dot(off, v.forward);
  if (!(d > 0.0f)) return false;
  const float ue = Dot(off, v.right);
  const float we = Dot(off, v.up);
  const float sx = v.columns[eye] / v.width;
  const float sy = v.rows / v.height;
  const float ax = v.column[eye] + 0.5f * v.columns[eye] + sx * ue;
  const float bx = sx * d;
  const float ay = 0.5f * v.rows - sy * we;
  const float by = sy * d;
  float row_x[3], row_y[3];
  for (int k = 0; k < 3; ++k) {
    row_x[k] = (bx / hx) * v.right[k] + ((ax - hx) / hx) * v.forward[k];
    row_y[k] = (by / hy) * v.up[k] - ((ay - hy) / hy) * v.forward[k];
  }
  for (int k = 0; k < 3; ++k) {
    m[k * 3 + 0] = row_x[k];
    m[k * 3 + 1] = row_y[k];
    m[k * 3 + 2] = v.forward[k];
  }
  m[9] = -Dot(row_x, e);
  m[10] = -Dot(row_y, e);
  m[11] = -Dot(v.forward, e);
  return true;
}

// THE DEPTH OF A HAND. The hands are nearer than all of the scene, which is
// on the screen or behind it. So the depth that the game would give a hand
// goes into a small range at the front of the depth buffer. The order of the
// parts of one hand stays, so its fingers still hide its palm.
const float kFrontBase = 0.0005f;
const float kFrontRange = 0.02f;
inline float FrontDepth(float z) {
  if (!(z > 0.0f)) z = 0.0f;
  if (z > 1.0f) z = 1.0f;
  return kFrontBase + kFrontRange * z;
}

}  // namespace cutscene_hands_math
