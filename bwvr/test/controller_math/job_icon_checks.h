// THE JOB ICON AND THE NAME OVER A VILLAGER (docs/village.md, "The job icon
// and the name over a villager").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/job_icon_math.h"
#include "../../src/mod/sprite_math.h"

namespace job_icon_checks {

// The camera matrix of the engine for a camera at `eye` that looks down by
// `pitch` radians along +z, with no roll: rows of 3 as at 0x00EA1D28.
inline void Camera(const float eye[3], float pitch, float m[12]) {
  const float c = cosf(pitch), s = sinf(pitch);
  // The camera axes in the world.
  const float x[3] = {1.0f, 0.0f, 0.0f};
  const float y[3] = {0.0f, c, s};
  const float z[3] = {0.0f, -s, c};
  for (int i = 0; i < 3; ++i) {
    m[3 * i + 0] = x[i];
    m[3 * i + 1] = y[i];
    m[3 * i + 2] = z[i];
  }
  for (int j = 0; j < 3; ++j) {
    const float* axis = j == 0 ? x : j == 1 ? y : z;
    m[9 + j] = -(axis[0] * eye[0] + axis[1] * eye[1] + axis[2] * eye[2]);
  }
}

// The camera y of the lower edge of a sprite, as LH3DSprite::Draw makes it
// with no turn.
inline float LowerEdge(const float m[12], const job_icon_math::Sprite& s) {
  const float c = s.place[0] * m[1] + s.place[1] * m[4] + s.place[2] * m[7] +
                  m[10];
  return c - s.size * s.aspect - s.off_y;
}

inline void JobIconChecks() {
  using namespace job_icon_math;
  printf("\nThe job icon and the name over a villager\n");

  // The God view: the camera 40 units up and 30 back, 50 degrees down.
  const float eye[3] = {100.0f, 40.0f, 170.0f};
  float m[12];
  Camera(eye, 0.8727f, m);
  // The icon as the game gives it: 2.5 radii over the top of a villager
  // with a radius of 1, at a half size of 8 / 1800 of the distance (a render
  // of 3600 across).
  Sprite icon;
  icon.place[0] = 100.0f;
  icon.place[1] = 5.0f;
  icon.place[2] = 200.0f;
  float d2 = 0.0f;
  for (int i = 0; i < 3; ++i) {
    d2 += (icon.place[i] - eye[i]) * (icon.place[i] - eye[i]);
  }
  const float distance = sqrtf(d2);
  icon.size = distance * 16.0f / 1800.0f * 0.5f;
  icon.aspect = 1.0f;
  icon.off_x = 0.0f;
  icon.off_y = 0.0f;

  // SCALE 2.
  {
    const Sprite big = ScaleIcon(icon, m, 0.0f, 2.0f);
    Near("the icon at 200 percent has twice the half size", big.size,
         2.0 * icon.size, 1e-6);
    Near("its lower edge stays where the game puts it", LowerEdge(m, big),
         LowerEdge(m, icon), 1e-4);
    Near("its depth in the camera does not change",
         Depth(m, big.place), Depth(m, icon.place), 1e-4);
    Check(big.place[1] > icon.place[1], "the middle goes up in the world",
          big.place[1], icon.place[1]);
    Near("the icon is 2 * atan(8 / 1800) across as the game gives it",
         AcrossDegrees(icon.size, distance), 0.5093, 1e-3);
    Near("and twice that at 200 percent", AcrossDegrees(big.size, distance),
         1.0186, 2e-3);
  }

  // SCALE 1 changes nothing.
  {
    const Sprite same = ScaleIcon(icon, m, 0.0f, 1.0f);
    Near("at 100 percent the half size is the game's", same.size, icon.size,
         0.0);
    Near("and the place too", same.place[1], icon.place[1], 0.0);
  }

  // OFFSETS AND AN ASPECT: the edge still stays.
  {
    Sprite odd = icon;
    odd.aspect = 1.5f;
    odd.off_x = 0.1f;
    odd.off_y = -0.05f;
    const Sprite big = ScaleIcon(odd, m, 0.0f, 2.0f);
    Near("with an aspect and offsets the lower edge stays", LowerEdge(m, big),
         LowerEdge(m, odd), 1e-4);
    Near("the offsets grow with the size", big.off_x, 0.2, 1e-6);
  }

  // A TURN OF THE DRAW: the middle goes along the turned up of the sprite.
  {
    const float turn = 0.3f;
    const Sprite big = ScaleIcon(icon, m, turn, 2.0f);
    // The move in camera space.
    float move[3];
    for (int j = 0; j < 3; ++j) {
      move[j] = (big.place[0] - icon.place[0]) * m[j] +
                (big.place[1] - icon.place[1]) * m[3 + j] +
                (big.place[2] - icon.place[2]) * m[6 + j];
    }
    Near("a turned sprite moves along (sin a, cos a): x", move[0],
         icon.size * sinf(turn), 1e-5);
    Near("and y", move[1], icon.size * cosf(turn), 1e-5);
    Near("and not in depth", move[2], 0.0, 1e-5);
    // With the turn of sprite_world_up for a camera with no roll the up of
    // the sprite is camera up.
    Near("a camera with no roll gets no turn of sprite_world_up",
         sprite_math::SpriteTurn(m), 0.0, 1e-6);
  }

  // THE SIZES IN A PICTURE.
  {
    // A render of 3600 by 2880 at 121.5 degrees across: the focal length is
    // 1800 / tan(60.75 degrees).
    const float focal = 1800.0f / tanf(60.75f * 0.017453293f);
    Near("the render has 17.6 px in a degree in its middle",
         PixelsPerDegree(focal), 17.6, 0.05);
    Near("a line of 16 px is 0.91 degrees", PixelsDegrees(16.0f, focal),
         0.909, 0.005);
    Near("a thing of half size 1 at depth 100 is 2 / 100 of the focal "
         "length across", AcrossPixels(1.0f, 100.0f, focal),
         0.02 * focal, 1e-3);
    Near("a percent of 200 is a factor of 2", ScaleFromPercent(200), 2.0,
         0.0);
    Near("a percent under 50 is 50", ScaleFromPercent(10), 0.5, 0.0);
    Near("a percent over 400 is 400", ScaleFromPercent(900), 4.0, 0.0);
  }
}

}  // namespace job_icon_checks
