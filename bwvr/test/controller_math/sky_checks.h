// THE SKY WITH NO HEAD IN IT (docs/backend.md, "Sky, clouds and detail").
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/sky_math.h"

namespace sky_checks {

// A camera matrix of the engine for a head at `place` that looks along yaw
// and pitch, with roll about the forward. Degrees.
inline void HeadCamera(const float place[3], float yaw, float pitch,
                       float roll, float m[12]) {
  const float d = 3.14159265f / 180.0f;
  const float cy = cosf(yaw * d), sy = sinf(yaw * d);
  const float cp = cosf(pitch * d), sp = sinf(pitch * d);
  const float cr = cosf(roll * d), sr = sinf(roll * d);
  const float forward[3] = {sy * cp, sp, cy * cp};
  const float right0[3] = {cy, 0.0f, -sy};
  float up0[3];
  sky_math::Cross3(forward, right0, up0);
  float axes[3][3];
  for (int k = 0; k < 3; ++k) {
    axes[0][k] = cr * right0[k] + sr * up0[k];
    axes[1][k] = cr * up0[k] - sr * right0[k];
    axes[2][k] = forward[k];
  }
  for (int j = 0; j < 3; ++j) {
    for (int k = 0; k < 3; ++k) m[3 * k + j] = axes[j][k];
    m[9 + j] = -sky_math::Dot3(place, axes[j]);
  }
}

inline float Largest9(const float a[9], const float b[9]) {
  float most = 0.0f;
  for (int i = 0; i < 9; ++i) {
    const float d = fabsf(a[i] - b[i]);
    if (d > most) most = d;
  }
  return most;
}

// The lowest height of each point of the half ball of a dome, each degree, as
// the engine draws it with `rows` at `place`, less the land under the point.
template <class Land>
inline float DomeLeastOverLand(const float place[3], const float rows[9],
                               Land land) {
  const float d = 3.14159265f / 180.0f;
  float least = 1e9f;
  for (int rise = 0; rise <= 90; ++rise) {
    for (int turn = 0; turn < 360; ++turn) {
      const float p[3] = {
          sky_math::kDomeRadius * cosf(rise * d) * cosf(turn * d),
          sky_math::kDomeRadius * sinf(rise * d),
          sky_math::kDomeRadius * cosf(rise * d) * sinf(turn * d)};
      float w[3];
      for (int k = 0; k < 3; ++k) {
        w[k] = place[k] + p[0] * rows[k] + p[1] * rows[3 + k] +
               p[2] * rows[6 + k];
      }
      const float over = w[1] - land(w[0], w[2]);
      if (over < least) least = over;
    }
  }
  return least;
}

// The rows of a round dome of scale `scale` seen from a camera at `rise`
// degrees under it, along world z, as h_MistDraw makes them.
inline void SideRows(float scale, float rise, float rows[9]) {
  const float d = 3.14159265f / 180.0f;
  const float f[3] = {0.0f, sinf(rise * d), cosf(rise * d)};
  const float r[3] = {1.0f, 0.0f, 0.0f};
  float u[3];
  sky_math::Cross3(f, r, u);
  for (int k = 0; k < 3; ++k) {
    rows[k] = r[k] * scale;
    rows[3 + k] = -f[k] * scale;
    rows[6 + k] = u[k] * scale;
  }
}

inline void LandClearChecks() {
  using namespace sky_math;
  printf("\nA drifting cloud over the land\n");
  auto flat = [](float, float) { return 50.0f; };
  auto sea = [](float, float) { return 0.0f; };
  // A round cloud of scale 30 (600 units wide) at 400, seen level from the
  // side: its rim goes 600 units under its place, into the sea.
  const float place[3] = {2000.0f, 400.0f, 2000.0f};
  float rows[9];
  SideRows(30.0f, 0.0f, rows);
  Near("a round cloud seen level goes 600 units under its place",
       DomeDrop(rows), 600.0, 1e-3);
  Check(DomeLeastOverLand(place, rows, sea) < 0.0f,
        "with no limit the round cloud goes into the sea",
        DomeLeastOverLand(place, rows, sea) < 0.0f, 1);
  int asked = 0;
  const float share =
      LandClearShare(place, rows, 10.0f, 171.0f, 0.05f, flat, &asked);
  Near("over flat land at 50 the share keeps the rim 10 units over it",
       share, (400.0 - 60.0) / 600.0, 1e-4);
  float kept[9];
  for (int i = 0; i < 9; ++i) kept[i] = i < 3 ? rows[i] : rows[i] * share;
  Near("with the share the lowest point is 10 units over the land",
       DomeLeastOverLand(place, kept, flat), 10.0, 0.05);
  Near("the share keeps the width of the cloud", kept[0], 30.0, 1e-6);
  Check(asked > 0 && asked <= kDomePoints,
        "the land is asked once at most for each test point", asked,
        kDomePoints);

  // A flat cloud high up: no point comes near the highest land, and the land
  // is not asked.
  float high[9];
  SideRows(30.0f, 0.0f, high);
  for (int i = 3; i < 9; ++i) high[i] *= 0.15f;  // a flatten of about 6.7
  asked = 0;
  Near("a flat cloud high up keeps its shape",
       LandClearShare(place, high, 10.0f, 171.0f, 0.05f, flat, &asked), 1.0,
       1e-6);
  Check(asked == 0, "a flat cloud high up does not ask the land", asked, 0);

  // A hill under one side of a lower cloud makes it flatter than flat land.
  auto hill = [](float x, float) { return x >= 2250.0f ? 160.0f : 0.0f; };
  const float low[3] = {2000.0f, 300.0f, 2000.0f};
  const float over_sea =
      LandClearShare(low, rows, 10.0f, 171.0f, 0.05f, sea, nullptr);
  const float over_hill =
      LandClearShare(low, rows, 10.0f, 171.0f, 0.05f, hill, nullptr);
  Near("over the sea the rim stays 10 units over it", over_sea,
       (300.0 - 10.0) / 600.0, 1e-4);
  Check(over_hill < over_sea,
        "a hill under the side of the cloud makes it flatter",
        over_hill < over_sea, 1);
  for (int i = 0; i < 9; ++i) kept[i] = i < 3 ? rows[i] : rows[i] * over_hill;
  // The edge of the hill is a wall between two test points, so a point
  // between them can go under the gap, by less than 5 percent of the drop.
  const float in_hill = DomeLeastOverLand(low, kept, hill);
  Check(in_hill > 10.0f - 0.05f * 600.0f * over_hill,
        "with that share the cloud stays within 5 percent of its drop of the "
        "gap over a wall of a hill",
        in_hill, 10.0f - 0.05f * 600.0f * over_hill);

  // A cloud seen from 20 degrees under it: the pole points down at the
  // camera, and the lowest point is on the curved surface.
  float tilted[9];
  SideRows(40.0f, 20.0f, tilted);
  Near("the drop of a tilted dome is the radius times its scale",
       DomeDrop(tilted), 800.0, 0.05);
  Near("the drop is the lowest point of the half ball",
       DomeDrop(tilted), place[1] - DomeLeastOverLand(place, tilted, sea),
       1.0);
  const float tilt_share =
      LandClearShare(place, tilted, 10.0f, 171.0f, 0.05f, sea, nullptr);
  for (int i = 0; i < 9; ++i) {
    kept[i] = i < 3 ? tilted[i] : tilted[i] * tilt_share;
  }
  Check(DomeLeastOverLand(place, kept, sea) > 10.0f - 0.02f * 800.0f,
        "between the test points the tilted dome stays within 2 percent of "
        "its drop of the gap",
        DomeLeastOverLand(place, kept, sea) > 10.0f - 0.02f * 800.0f, 1);

  // A mist with its place under the land gets the flattest dome.
  const float under[3] = {2000.0f, 20.0f, 2000.0f};
  Near("a dome with its place in the land gets the least share",
       LandClearShare(under, rows, 10.0f, 171.0f, 0.05f, flat, nullptr), 0.05,
       1e-6);
}

inline void SkyChecks() {
  using namespace sky_math;
  printf("\nThe sky with no head in it\n");

  const float place[3] = {1200.0f, 340.0f, -560.0f};
  const float moon[3] = {place[0] + 30000.0f, place[1] + 18000.0f,
                         place[2] + 9000.0f};

  // The camera of a head: its place is the place that it was given.
  float head[12];
  HeadCamera(place, 35.0f, -50.0f, 20.0f, head);
  float at[3];
  CameraPlace(head, at);
  Near("the place of a head camera", at[0] - place[0] + at[1] - place[1] +
       at[2] - place[2], 0.0, 0.01);

  // The level camera keeps the place and has world up.
  float level[12];
  const bool made = LevelCamera(head, level);
  Check(made, "a head camera with roll gives a level camera", made, 1);
  CameraPlace(level, at);
  Near("the level camera keeps the place, x", at[0], place[0], 0.05);
  Near("the level camera keeps the place, y", at[1], place[1], 0.05);
  Near("the level camera keeps the place, z", at[2], place[2], 0.05);
  float up[3];
  CameraAxis(level, 1, up);
  Near("the up of the level camera is world up", up[1], 1.0, 1e-6);
  Check(IsRotation(level), "the level camera is a true rotation",
        IsRotation(level), 1);

  // With the head camera, the moon turns with the roll of the head. The head
  // looks at the moon (yaw 73.3 and pitch 29.9 degrees), with a roll of 20
  // degrees. The right of the moon is then 20 degrees from its right with no
  // roll.
  float rolled[9], straight[9], rolled_cam[12], straight_cam[12];
  HeadCamera(place, 73.3f, 29.9f, 20.0f, rolled_cam);
  HeadCamera(place, 73.3f, 29.9f, 0.0f, straight_cam);
  MoonTurn(rolled_cam, moon, rolled);
  MoonTurn(straight_cam, moon, straight);
  Near("with the head camera a roll of 20 degrees turns the moon 20 degrees",
       acosf(Dot3(rolled, straight)) * 57.29578f, 20.0, 0.5);

  // With the level camera, the turn of the moon is the same for each turn of
  // the head at that place, and its right is level.
  const float poses[5][3] = {{35.0f, -50.0f, 20.0f}, {35.0f, -50.0f, -25.0f},
                             {-80.0f, 10.0f, 0.0f}, {170.0f, -89.9f, 40.0f},
                             {0.0f, 0.0f, 0.0f}};
  float first[9];
  float most = 0.0f, tilt_most = 0.0f;
  bool all_made = true;
  for (int i = 0; i < 5; ++i) {
    float m[12], lv[12], turn[9];
    HeadCamera(place, poses[i][0], poses[i][1], poses[i][2], m);
    if (!LevelCamera(m, lv)) all_made = false;
    MoonTurn(lv, moon, turn);
    if (i == 0) {
      for (int k = 0; k < 9; ++k) first[k] = turn[k];
    } else {
      const float d = Largest9(first, turn);
      if (d > most) most = d;
    }
    if (fabsf(turn[1]) > tilt_most) tilt_most = fabsf(turn[1]);
  }
  Check(all_made, "each head pose gives a level camera, also straight down",
        all_made, 1);
  Near("the moon has one turn for five head poses", most, 0.0, 1e-4);
  Near("the right of the moon is level", tilt_most, 0.0, 1e-5);

  // With the head straight ahead, the level camera gives the game's own
  // turn.
  float game[12], game_level[12], game_turn[9], level_turn[9];
  HeadCamera(place, 35.0f, 0.0f, 0.0f, game);
  LevelCamera(game, game_level);
  MoonTurn(game, moon, game_turn);
  MoonTurn(game_level, moon, level_turn);
  Near("a level head gives the game's own turn of the moon",
       Largest9(game_turn, level_turn), 0.0, 1e-5);

  // A matrix with a scale is not a camera: it stays as it is.
  float scaled[12];
  for (int i = 0; i < 12; ++i) scaled[i] = head[i] * 1.5f;
  float unused[12];
  const bool refused = !LevelCamera(scaled, unused);
  Check(refused, "a matrix with a scale gives no level camera", refused, 1);

  // The scale of a cloud: the engine's formula, from two eyes.
  const float cloud[3] = {0.0f, 400.0f, 0.0f};
  const float below[3] = {0.0f, 0.0f, 0.0f};
  const float side[3] = {1000.0f, 400.0f, 0.0f};
  Near("a cloud straight above is not flattened",
       MistScale(30.0f, true, 4.0f, cloud, below), 30.0, 1e-4);
  Near("a cloud at the horizon is flattened by the flatten term",
       MistScale(30.0f, true, 4.0f, cloud, side), 7.5, 1e-4);
  Near("ground mist is not flattened",
       MistScale(30.0f, false, 4.0f, cloud, side), 30.0, 1e-6);
  Near("a cloud at the eye has no scale",
       MistScale(30.0f, true, 4.0f, cloud, cloud), 0.0, 1e-6);
  // The ratio that the mod gives rows 1 and 2: the engine's scale from the
  // head times the ratio is the scale from the table camera.
  const float a_head = MistScale(30.0f, true, 4.0f, cloud, side);
  const float a_table = MistScale(30.0f, true, 4.0f, cloud, below);
  Near("the ratio gives the scale from the table camera",
       a_head * (a_table / a_head), a_table, 1e-4);
  // The flatten of a near sky cloud (cloud_round_near=1000).
  Near("a sky cloud nearer than the limit is round",
       NearFlatten(3.0f, 600.0f, 1000.0f), 1.0, 1e-6);
  Near("half way to 2 times the limit it has half of the flatten",
       NearFlatten(3.0f, 1500.0f, 1000.0f), 2.0, 1e-6);
  Near("from 2 times the limit it has the full flatten",
       NearFlatten(3.0f, 2500.0f, 1000.0f), 3.0, 1e-6);
  Near("a limit of 0 keeps the flatten of the engine",
       NearFlatten(3.0f, 10.0f, 0.0f), 3.0, 1e-6);
  Near("a flatten of 1 stays 1", NearFlatten(1.0f, 10.0f, 1000.0f), 1.0,
       1e-6);
  // The fade of a near sky cloud (15 cm and 60 cm of the room).
  Near("the eye in a cloud: the cloud is clear",
       CloudFade(-5.0f, 15.0f, 60.0f), 0.0, 1e-6);
  Near("at 15 cm from its surface the cloud is clear",
       CloudFade(15.0f, 15.0f, 60.0f), 0.0, 1e-6);
  Near("half way the cloud keeps half of its draw",
       CloudFade(37.5f, 15.0f, 60.0f), 0.5, 1e-6);
  Near("at 60 cm and more the cloud is whole",
       CloudFade(80.0f, 15.0f, 60.0f), 1.0, 1e-6);
  Near("a far limit of 0 keeps the cloud whole",
       CloudFade(-5.0f, 15.0f, 0.0f), 1.0, 1e-6);
  Near("a cloud of 300 units 315 units away at 100 units a meter is 15 cm "
       "from the eye",
       CloudGapCm(315.0f, 300.0f, 100.0f), 15.0, 1e-4);
  Near("the same cloud at 10 units a meter is 150 cm from the eye",
       CloudGapCm(315.0f, 300.0f, 10.0f), 150.0, 1e-4);
  Near("a round near cloud is not flattened at the horizon",
       MistScale(30.0f, true, NearFlatten(4.0f, 600.0f, 1000.0f), cloud,
                 side),
       30.0, 1e-4);
  LandClearChecks();
}

}  // namespace sky_checks
