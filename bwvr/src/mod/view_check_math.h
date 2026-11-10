// The check of the view (docs/controller-hints.md, "The place of the
// controller"). Pure math with no game and no runtime in it. view_check.cpp
// uses it in the game, and test/controller_math checks it.
//
// THE QUESTION. A point at the grip pose of a controller is drawn in each eye
// picture by the matrix of the eye pass of the game. The projection layer
// tells the runtime the pose and the frustum of each eye picture. The runtime
// shows a pixel of the picture in the direction that the declared view gives
// it. So the point shows at its true place only when the pixel of the game
// and the pixel of the declared view are the same pixel, in both eyes.
//
// THE TWO PIXELS of one point:
//
//   the game      x = (1 + cx / cz) * half_x    y = (1 - cy / cz) * half_y
//                 with c the point in the camera space of the eye pass
//                 (overlay_math.h, Project)
//   the declared  the point in the space of the view (LOCAL, turned into
//                 the view: +X right, +Y up, -Z forward), then the tangents
//                 of its direction on the frustum of the view
//
// THE PLACE THAT THE PLAYER SEES. Each pixel of the game, put back through
// its declared view, is a ray from the eye. The two rays of the two eyes meet
// (or pass nearest) at the place where the two eyes see the point. The
// difference from the true point, along the line from the middle of the head
// to the point, is the error of depth: a negative value is nearer to the
// head.
//
// Positions are meters in the LOCAL space of the runtime. Quaternions are
// x y z w.
#pragma once

#include <math.h>

namespace view_check {

// A view as the projection layer declares it.
struct View {
  float position[3];
  float orientation[4];
  // The tangents of the four angles of the frustum: left and down are
  // negative.
  float tan_left, tan_right, tan_up, tan_down;
  // The size of the eye picture in pixels.
  float width, height;
};

inline void Rotate(const float q[4], const float v[3], float out[3]) {
  const float tx = 2.0f * (q[1] * v[2] - q[2] * v[1]);
  const float ty = 2.0f * (q[2] * v[0] - q[0] * v[2]);
  const float tz = 2.0f * (q[0] * v[1] - q[1] * v[0]);
  out[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
  out[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
  out[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
}

inline void RotateBack(const float q[4], const float v[3], float out[3]) {
  const float c[4] = {-q[0], -q[1], -q[2], q[3]};
  Rotate(c, v, out);
}

// A view from the angles of a frustum in radians (XrFovf).
inline void MakeView(const float position[3], const float orientation[4],
                     float angle_left, float angle_right, float angle_up,
                     float angle_down, float width, float height, View* out) {
  for (int i = 0; i < 3; ++i) out->position[i] = position[i];
  for (int i = 0; i < 4; ++i) out->orientation[i] = orientation[i];
  out->tan_left = tanf(angle_left);
  out->tan_right = tanf(angle_right);
  out->tan_up = tanf(angle_up);
  out->tan_down = tanf(angle_down);
  out->width = width;
  out->height = height;
}

// The pixel of a LOCAL point in the picture of a declared view. False when
// the point is not in front of the view.
inline bool DeclaredPixel(const View& v, const float p[3], float pixel[2]) {
  const float d[3] = {p[0] - v.position[0], p[1] - v.position[1],
                      p[2] - v.position[2]};
  float in_view[3];
  RotateBack(v.orientation, d, in_view);
  if (!(in_view[2] < -1e-6f)) return false;
  const float tx = in_view[0] / -in_view[2];
  const float ty = in_view[1] / -in_view[2];
  pixel[0] = (tx - v.tan_left) / (v.tan_right - v.tan_left) * v.width;
  pixel[1] = (v.tan_up - ty) / (v.tan_up - v.tan_down) * v.height;
  return true;
}

// The ray of a pixel of the picture of a declared view, as a unit direction
// in LOCAL from the position of the view.
inline void DeclaredRay(const View& v, const float pixel[2], float out[3]) {
  const float tx =
      v.tan_left + pixel[0] / v.width * (v.tan_right - v.tan_left);
  const float ty = v.tan_up - pixel[1] / v.height * (v.tan_up - v.tan_down);
  const float in_view[3] = {tx, ty, -1.0f};
  Rotate(v.orientation, in_view, out);
  const float n =
      sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
  for (int i = 0; i < 3; ++i) out[i] /= n;
}

// The pixel of a world point in the picture of an eye pass of the game: the
// scaled world to camera matrix of the game (12 floats, row vector, the
// translation last) and half the size of the picture. False when the point
// is not in front of the camera.
inline bool GamePixel(const float m[12], float half_x, float half_y,
                      const float p[3], float pixel[2], float* depth) {
  const float cx = p[0] * m[0] + p[1] * m[3] + p[2] * m[6] + m[9];
  const float cy = p[0] * m[1] + p[1] * m[4] + p[2] * m[7] + m[10];
  const float cz = p[0] * m[2] + p[1] * m[5] + p[2] * m[8] + m[11];
  if (!(cz > 1e-6f)) return false;
  pixel[0] = (1.0f + cx / cz) * half_x;
  pixel[1] = (1.0f - cy / cz) * half_y;
  if (depth != nullptr) *depth = cz;
  return true;
}

// THE SCALE CHECK (docs/controller-hints.md, "The scale that the eyes see").
//
// The place of the camera of an eye pass in the world, from the matrix of
// the pass (12 floats, row vector, the translation last). The camera is the
// point that the matrix takes to the origin of the camera space:
//
//     c * A + t = 0      so      c = -t * inverse(A)
//
// A holds the turn and the scale of the field of view, so it is not a pure
// turn, and the inverse is the full one. False when A has no inverse.
inline bool CameraPosition(const float m[12], float out[3]) {
  const float a = m[0], b = m[1], c = m[2];
  const float d = m[3], e = m[4], f = m[5];
  const float g = m[6], h = m[7], k = m[8];
  const float det = a * (e * k - f * h) - b * (d * k - f * g) +
                    c * (d * h - e * g);
  if (!(fabsf(det) > 1e-20f)) return false;
  // The inverse of A, by its cofactors.
  const float inv[9] = {(e * k - f * h) / det, (c * h - b * k) / det,
                        (b * f - c * e) / det, (f * g - d * k) / det,
                        (a * k - c * g) / det, (c * d - a * f) / det,
                        (d * h - e * g) / det, (b * g - a * h) / det,
                        (a * e - b * d) / det};
  const float t[3] = {-m[9], -m[10], -m[11]};
  for (int j = 0; j < 3; ++j) {
    out[j] = t[0] * inv[0 * 3 + j] + t[1] * inv[1 * 3 + j] +
             t[2] * inv[2 * 3 + j];
  }
  return true;
}

// The eyes apart in world units with one walk (one_walk_math.h): the right
// eye is the left eye with each vertex moved per_rhw * rhw pixels, and
//
//     per_rhw = -k * separation * half_x / near
//
// with k the scale of x in the matrix of the pass (the length of its first
// column). So the separation that the right eye is drawn with is
// |per_rhw| * near / (k * half_x). 0 when a number is not usable.
inline float OneWalkSeparation(const float m[12], float per_rhw,
                               float near_plane, float half_x) {
  const float k = sqrtf(m[0] * m[0] + m[3] * m[3] + m[6] * m[6]);
  if (!(k > 0.0f) || !(half_x > 0.0f) || !(near_plane > 0.0f)) return 0.0f;
  return fabsf(per_rhw) * near_plane / (k * half_x);
}

inline float Distance3(const float a[3], const float b[3]) {
  const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
  return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

// The point nearest to two rays (the middle of the shortest line between
// them). False when the rays are parallel.
inline bool Meet(const float o0[3], const float d0[3], const float o1[3],
                 const float d1[3], float out[3]) {
  const float w[3] = {o0[0] - o1[0], o0[1] - o1[1], o0[2] - o1[2]};
  const float a = d0[0] * d0[0] + d0[1] * d0[1] + d0[2] * d0[2];
  const float b = d0[0] * d1[0] + d0[1] * d1[1] + d0[2] * d1[2];
  const float c = d1[0] * d1[0] + d1[1] * d1[1] + d1[2] * d1[2];
  const float d = d0[0] * w[0] + d0[1] * w[1] + d0[2] * w[2];
  const float e = d1[0] * w[0] + d1[1] * w[1] + d1[2] * w[2];
  const float den = a * c - b * b;
  if (!(fabsf(den) > 1e-12f)) return false;
  const float s = (b * e - c * d) / den;
  const float t = (a * e - b * d) / den;
  for (int i = 0; i < 3; ++i) {
    out[i] = 0.5f * ((o0[i] + s * d0[i]) + (o1[i] + t * d1[i]));
  }
  return true;
}

// The result of the check for one point.
struct Result {
  float pixel_error[2];  // pixels between the two pixels of each eye
  float seen[3];         // where the two eyes see the point, LOCAL
  float distance;        // meters from the middle of the head to the point
  float along;           // meters, the error of depth: negative is nearer
  float across;          // meters, the error square to the line of sight
};

// The check of one point. game_pixel holds the pixels of the game in eye 0
// and eye 1. False when the point is not in front of both views or the rays
// do not meet.
inline bool CheckPoint(const View view[2], const float game_pixel[2][2],
                       const float p[3], Result* out) {
  for (int e = 0; e < 2; ++e) {
    float want[2];
    if (!DeclaredPixel(view[e], p, want)) return false;
    const float dx = game_pixel[e][0] - want[0];
    const float dy = game_pixel[e][1] - want[1];
    out->pixel_error[e] = sqrtf(dx * dx + dy * dy);
  }
  float ray[2][3];
  for (int e = 0; e < 2; ++e) DeclaredRay(view[e], game_pixel[e], ray[e]);
  if (!Meet(view[0].position, ray[0], view[1].position, ray[1], out->seen)) {
    return false;
  }
  float head[3], line[3], error[3];
  for (int i = 0; i < 3; ++i) {
    head[i] = 0.5f * (view[0].position[i] + view[1].position[i]);
    line[i] = p[i] - head[i];
    error[i] = out->seen[i] - p[i];
  }
  out->distance =
      sqrtf(line[0] * line[0] + line[1] * line[1] + line[2] * line[2]);
  if (!(out->distance > 1e-6f)) return false;
  float along = 0.0f;
  for (int i = 0; i < 3; ++i) along += error[i] * line[i] / out->distance;
  out->along = along;
  float rest = 0.0f;
  for (int i = 0; i < 3; ++i) {
    const float r = error[i] - along * line[i] / out->distance;
    rest += r * r;
  }
  out->across = sqrtf(rest);
  return true;
}

}  // namespace view_check
