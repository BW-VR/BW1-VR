// THE JOB ICON AND THE NAME OVER A VILLAGER (docs/village.md, "The job icon
// and the name over a villager"). Pure math: no Windows, no game. The checks
// are in test/controller_math/job_icon_checks.h.
//
// THE ICON. fn_008241B0 puts the job icon in one LH3DSprite (0x00EB99D8). Its
// middle is 2.5 radii of the villager over the top of his bounding sphere.
// Its half size is the distance from the camera times 16 / half_res_x times
// 0.5, so the icon has one angle in the view at each distance:
// 2 * atan(8 / half_res_x). LH3DSprite::Draw makes the corners
//
//     x from -size - off_x to size - off_x
//     y from -size * aspect - off_y to size * aspect - off_y
//
// in the camera plane, turned by the angle of the draw. So the up of the
// sprite (0, 1) goes to (sin a, cos a) in the camera plane (sprite_math.h).
//
// ScaleIcon makes the half size, the aspect part and the two offsets k times
// as large, and moves the middle up along the up of the sprite so that the
// lower edge stays where the game puts it. The icon then grows away from the
// head, not into it.
//
// THE CAMERA MATRIX. The matrix at 0x00EA1D28 has 12 floats, and a world
// point w goes to camera space as
//     c[j] = w.x * m[j] + w.y * m[3 + j] + w.z * m[6 + j] + m[9 + j]
// so camera axis j in the world is (m[j], m[3 + j], m[6 + j]) when the matrix
// has no scale.

#pragma once

#include <math.h>

namespace job_icon_math {

// The fields of an LH3DSprite that the draw reads for its corners.
struct Sprite {
  float place[3];  // +0x00
  float size;      // +0x0C, the half size across
  float aspect;    // +0x10, the half size up is size * aspect
  float off_x;     // +0x18
  float off_y;     // +0x1C
};

// The world direction of the up of a sprite that the draw turns by `angle`
// (radians), with the camera matrix m.
inline void SpriteUp(const float m[12], float angle, float out[3]) {
  const float s = sinf(angle);
  const float c = cosf(angle);
  for (int i = 0; i < 3; ++i) out[i] = s * m[3 * i] + c * m[3 * i + 1];
}

// The sprite k times as large, with its lower edge at the same place.
// `angle` is the angle that the draw uses (the angle of the sprite and the
// turn of sprite_world_up). A k that is not above zero changes nothing.
inline Sprite ScaleIcon(const Sprite& in, const float m[12], float angle,
                        float k) {
  if (!(k > 0.0f)) return in;
  Sprite out = in;
  out.size = in.size * k;
  out.off_x = in.off_x * k;
  out.off_y = in.off_y * k;
  // The lower edge is at -(size * aspect + off_y) along the up of the
  // sprite. It stays when the middle moves up by (k - 1) times that.
  const float rise = (k - 1.0f) * (in.size * in.aspect + in.off_y);
  float up[3];
  SpriteUp(m, angle, up);
  for (int i = 0; i < 3; ++i) out.place[i] = in.place[i] + rise * up[i];
  return out;
}

// The depth of a world point in the camera.
inline float Depth(const float m[12], const float w[3]) {
  return w[0] * m[2] + w[1] * m[5] + w[2] * m[8] + m[11];
}

// The angle in degrees across a thing of the half size `half` at the
// distance `distance`.
inline float AcrossDegrees(float half, float distance) {
  if (!(distance > 0.0f)) return 0.0f;
  return 2.0f * atanf(half / distance) * 57.29578f;
}

// The pixels across a thing of the half size `half` at the depth `depth` in
// the camera, in the middle of a picture with the focal length `focal_px`
// (half_res_x * inv_half_tan_fov_x).
inline float AcrossPixels(float half, float depth, float focal_px) {
  if (!(depth > 0.0f)) return 0.0f;
  return 2.0f * half / depth * focal_px;
}

// The angle in degrees of a height of `px` pixels in the middle of a picture
// with the focal length `focal_px` (half_res_y * inv_half_tan_fov_y).
inline float PixelsDegrees(float px, float focal_px) {
  if (!(focal_px > 0.0f)) return 0.0f;
  return 2.0f * atanf(0.5f * px / focal_px) * 57.29578f;
}

// The pixels of a picture in one degree in its middle.
inline float PixelsPerDegree(float focal_px) {
  return focal_px * 0.017453293f;
}

// A percent of the ini as a factor, 50 to 400.
inline float ScaleFromPercent(int percent) {
  if (percent < 50) percent = 50;
  if (percent > 400) percent = 400;
  return percent / 100.0f;
}

}  // namespace job_icon_math
