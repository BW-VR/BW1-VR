// THE HAND AND THE HEAD STOP AT THE SOLID OBJECTS (docs/hand-model.md, "The
// Hand stops at the land", and docs/god-view.md, "The head under the land"):
// the push out of a box and out of a trunk, the face that holds, the slide
// along a wall, the return to the real hand, two solids, a turned box, the
// size rule and the distance of the head. main.cpp includes this file after
// hand_land_checks.h.
//
// The test has no game files. The hand is the low poly hand of
// overlay_math.h, 19 cm long at 100 units a meter, and a house is a box of 20
// by 10 by 20 units on flat land.
#pragma once

#include "../../src/mod/solid_math.h"
#include "hand_land_checks.h"

namespace solid_checks {

namespace sm = solid_math;
namespace hl = hand_land;

const float kUnits = 100.0f;

inline sm::Solid House() {
  sm::Solid s = {};
  s.kind = sm::kBox;
  s.middle[0] = 0.0f;
  s.middle[1] = 5.0f;
  s.middle[2] = 0.0f;
  for (int k = 0; k < 9; ++k) s.axis[k] = (k % 4) == 0 ? 1.0f : 0.0f;
  s.half[0] = 10.0f;
  s.half[1] = 5.0f;
  s.half[2] = 10.0f;
  return s;
}

inline sm::Solid Trunk() {
  sm::Solid s = {};
  s.kind = sm::kTrunk;
  s.radius = 2.0f;
  s.height = 10.0f;
  return s;
}

// One point with a gap of 0.5 units, and its offset from a set of solids.
inline bool PointOffset(const sm::Solid* solids, int count, const float p[3],
                        sm::Touch* touches, float offset[3]) {
  hl::Sample s;
  for (int k = 0; k < 3; ++k) s.p[k] = p[k];
  s.gap = 0.5f;
  return sm::Offset(solids, count, &s, 1, p, 1e30f, touches, offset);
}

// The most a point of the drawn hand is in a solid: 0 when each point keeps
// its gap.
inline float Inside(const sm::Solid& solid, const hl::Sample* s, int n,
                    const float offset[3]) {
  float most = 0.0f;
  for (int face = 0; face < 6; ++face) {
    const float push = sm::BoxPush(solid, s, n, offset, face);
    if (face == 0 || push < most) most = push;
  }
  return most;
}

inline void SolidChecks() {
  printf("-- the Hand and the head stop at the solid objects\n");
  const sm::Solid house = House();
  float offset[3];

  // OUT OF THE HOUSE: one to one.
  {
    sm::Touch t = {};
    const float p[3] = {12.0f, 5.0f, 0.0f};
    Check(!PointOffset(&house, 1, p, &t, offset) && offset[0] == 0.0f &&
              !t.on,
          "a point 1.5 units out of the wall of a house has no push", offset[0],
          0.0);
  }
  // INTO THE WALL: the drawn point stays on the wall, along the face of the
  // first touch, also past the middle of the house.
  {
    sm::Touch t = {};
    const float in[3] = {9.0f, 5.0f, 1.0f};
    PointOffset(&house, 1, in, &t, offset);
    Check(t.on && t.face == 0, "a point 1 unit into the wall at +x touches the "
                               "face +x", t.face, 0);
    Near("and goes out along +x to the wall and its gap", in[0] + offset[0],
         10.5, 1e-4);
    Check(offset[1] == 0.0f && offset[2] == 0.0f,
          "and it does not move up or sideways", offset[2], 0.0);
    const float deep[3] = {-3.0f, 5.0f, 1.0f};
    PointOffset(&house, 1, deep, &t, offset);
    Near("the real point goes past the middle: the drawn point stays on the "
         "face +x", deep[0] + offset[0], 10.5, 1e-4);
    // The slide along the wall: the real point goes along z in the wall.
    bool slides = true;
    for (int step = 0; step <= 5; ++step) {
      const float p[3] = {8.0f, 5.0f, -8.0f + 3.0f * step};
      PointOffset(&house, 1, p, &t, offset);
      slides = slides && fabsf(p[0] + offset[0] - 10.5f) < 1e-4f &&
               offset[2] == 0.0f;
    }
    Check(slides, "a point that slides along z in the wall stays on the wall "
                  "and slides with the real point", 0, 0);
    // The return: out of the box, the real point at once.
    const float out[3] = {11.0f, 5.0f, 7.0f};
    PointOffset(&house, 1, out, &t, offset);
    Check(!t.on && offset[0] == 0.0f,
          "the real point comes out of the wall: no push, and the touch ends",
          offset[0], 0.0);
    // Past the corner of the wall: the point is free.
    sm::Touch c = {};
    const float corner[3] = {9.5f, 5.0f, 9.5f};
    PointOffset(&house, 1, corner, &c, offset);
    t = c;
    const float past[3] = {9.5f, 5.0f, 11.0f};
    PointOffset(&house, 1, past, &t, offset);
    Check(!t.on && offset[0] == 0.0f && offset[2] == 0.0f,
          "a point that slides past the corner of the house is free", offset[0],
          0.0);
  }
  // DEEP AT ONCE: a point that a teleport puts 4 units into the house starts
  // no touch with a start bound of 2 units. It is the real point until it
  // comes out, and a touch from outside starts again.
  {
    hl::Sample s;
    s.gap = 0.5f;
    sm::Touch t = {};
    const float deep[3] = {6.0f, 5.0f, 0.0f};
    for (int k = 0; k < 3; ++k) s.p[k] = deep[k];
    Check(!sm::Offset(&house, 1, &s, 1, deep, 2.0f, &t, offset) && !t.on &&
              offset[0] == 0.0f,
          "a point that is 4 units into the house at once starts no touch",
          offset[0], 0.0);
    const float edge[3] = {9.0f, 5.0f, 0.0f};
    for (int k = 0; k < 3; ++k) s.p[k] = edge[k];
    sm::Offset(&house, 1, &s, 1, edge, 2.0f, &t, offset);
    Check(t.on && fabsf(edge[0] + offset[0] - 10.5f) < 1e-4f,
          "a point 1 unit into the wall starts the touch", offset[0], 1.5);
    const float deeper[3] = {2.0f, 5.0f, 0.0f};
    for (int k = 0; k < 3; ++k) s.p[k] = deeper[k];
    sm::Offset(&house, 1, &s, 1, deeper, 2.0f, &t, offset);
    Near("and the touch holds when the point goes deeper than the start bound",
         deeper[0] + offset[0], 10.5, 1e-4);
  }
  // ON THE ROOF: a point that comes down into the top of the house rests on
  // it.
  {
    sm::Touch t = {};
    const float p[3] = {1.0f, 9.0f, -2.0f};
    PointOffset(&house, 1, p, &t, offset);
    Check(t.face == 2, "a point 1 unit into the roof touches the face +y",
          t.face, 2);
    Near("and goes up onto the roof with its gap", p[1] + offset[1], 10.5,
         1e-4);
  }
  // NEVER DOWN: a point near the floor of the house goes out by a wall or
  // up, never down into the land.
  {
    sm::Touch t = {};
    const float p[3] = {0.0f, 0.2f, 9.0f};
    PointOffset(&house, 1, p, &t, offset);
    Check(t.on && t.face != 3 && offset[1] >= 0.0f,
          "a point at the floor of the house does not go down", offset[1], 0.0);
    Check(t.face == 4, "it goes out by the nearest wall (+z)", t.face, 4);
  }
  // A TURNED HOUSE: the push follows the turn of the box.
  {
    sm::Solid turned = house;
    const float c = 0.70710678f;
    const float axis[9] = {c, 0.0f, c, 0.0f, 1.0f, 0.0f, -c, 0.0f, c};
    for (int k = 0; k < 9; ++k) turned.axis[k] = axis[k];
    sm::Touch t = {};
    // 9 units along the first axis of the box.
    const float p[3] = {9.0f * c, 5.0f, 9.0f * c};
    PointOffset(&turned, 1, p, &t, offset);
    Near("a point in the wall of a house turned by 45 degrees goes out along "
         "the turned axis (x)", offset[0], 1.5 * c, 1e-4);
    Near("and (z)", offset[2], 1.5 * c, 1e-4);
  }
  // THE TRUNK: out of it along the level line from its axis to the palm, and
  // round it with the palm. The leaves over it do not count.
  {
    const sm::Solid trunk = Trunk();
    sm::Touch t = {};
    const float a[3] = {1.0f, 5.0f, 0.0f};
    PointOffset(&trunk, 1, a, &t, offset);
    Near("a point 1 unit from the axis of a trunk of radius 2 goes out to the "
         "radius and its gap", a[0] + offset[0], 2.5, 1e-4);
    // The point goes round the trunk in steps of 30 degrees.
    for (int step = 1; step <= 3; ++step) {
      const float angle = 0.5235988f * static_cast<float>(step);
      const float p[3] = {cosf(angle), 5.0f, sinf(angle)};
      PointOffset(&trunk, 1, p, &t, offset);
    }
    const float b[3] = {0.0f, 5.0f, 1.0f};
    PointOffset(&trunk, 1, b, &t, offset);
    Near("the point goes round the trunk to +z: it goes out along +z",
         b[2] + offset[2], 2.5, 1e-4);
    Check(fabsf(offset[0]) < 1e-5f, "and not along x", offset[0], 0.0);
    const float past[3] = {0.0f, 5.0f, -1.0f};
    PointOffset(&trunk, 1, past, &t, offset);
    Near("the point goes past the axis: it keeps its side (+z)",
         past[2] + offset[2], 2.5, 1e-4);
    sm::Touch u = {};
    const float leaves[3] = {0.5f, 11.0f, 0.0f};
    Check(!PointOffset(&trunk, 1, leaves, &u, offset),
          "a point over the trunk, in the leaves, has no push", offset[0], 0.0);
  }
  // TWO HOUSES: a point in the gap between two houses that touch goes out of
  // both.
  {
    sm::Solid two[2] = {House(), House()};
    two[1].middle[2] = 21.0f;  // a gap of 1 unit between the walls
    sm::Touch t[2] = {};
    const float p[3] = {9.8f, 5.0f, 10.4f};
    PointOffset(two, 2, p, t, offset);
    const float drawn[3] = {p[0] + offset[0], p[1] + offset[1],
                            p[2] + offset[2]};
    Check(sm::Distance(two[0], drawn) >= 0.5f - 1e-4f &&
              sm::Distance(two[1], drawn) >= 0.5f - 1e-4f,
          "a point at the corner of two houses goes out of both",
          sm::Distance(two[0], drawn), 0.5);
  }
  // THE HAND: the low poly hand pushed into the wall stops on it, with each
  // point out of the box with its gap, and it moves only along the face.
  {
    // The back of the hand is up, and the fingers (-y of the frame) point to
    // -x of the world, into the wall.
    const float palm_x[9] = {0.0f, 1.0f, 0.0f,    // world x
                             1.0f, 0.0f, 0.0f,    // world y
                             0.0f, 0.0f, -1.0f};  // world z
    hl::Sample s[hl::kMostSamples];
    sm::Touch t = {};
    float first[3] = {0, 0, 0};
    bool still = true;
    // The grip goes where the finger tips are 2 units into the wall, at the
    // middle of its height.
    const float away[3] = {100.0f, 5.0f, 0.0f};
    const int count =
        hand_land_checks::Samples(away, palm_x, overlay_math::kRestCurl, s);
    float least_x = 1e30f;
    for (int i = 0; i < count; ++i) {
      if (s[i].p[0] < least_x) least_x = s[i].p[0];
    }
    const float grip_x = 8.0f + (100.0f - least_x);
    for (int frame = 0; frame < 3; ++frame) {
      const float at[3] = {grip_x, 5.0f, 0.0f};
      const int n = hand_land_checks::Samples(at, palm_x, overlay_math::kRestCurl, s);
      sm::Offset(&house, 1, s, n, at, 1e30f, &t, offset);
      if (frame == 0) {
        for (int k = 0; k < 3; ++k) first[k] = offset[k];
        Check(t.on && offset[0] > 0.0f,
              "a hand with its fingers into the wall at +x goes out along +x",
              offset[0], 0);
        Near("and no point of it is in the box", Inside(house, s, n, offset),
             0.0, 1e-3);
        Check(offset[1] == 0.0f && offset[2] == 0.0f,
              "and it does not move up or sideways", offset[1], 0.0);
      } else {
        still = still && offset[0] == first[0];
      }
    }
    Check(still, "a still hand at a still wall has the same push in each frame",
          first[0], first[0]);
  }
  // THE SIZE: a pebble does not stop the hand, a house does.
  {
    sm::Solid pebble = House();
    pebble.half[0] = pebble.half[1] = pebble.half[2] = 1.0f;
    Check(sm::Counts(house, kUnits), "a house of 20 units at 100 units a "
                                     "meter counts", 1, 1);
    Check(!sm::Counts(pebble, kUnits),
          "a stone of 2 units (2 cm of the room) does not", 0, 0);
    Check(sm::Counts(pebble, 13.2f), "the same stone at 13.2 units a meter (15 "
                                     "cm of the room) counts", 1, 1);
    sm::Solid trunk = Trunk();
    trunk.radius = 0.5f;
    Check(!sm::Counts(trunk, kUnits) && sm::Counts(trunk, 13.2f),
          "a trunk of 0.5 units counts at 13.2 units a meter, not at 100", 0,
          0);
  }
  // THE HEAD: the distance to a solid, less than 0 inside.
  {
    const float middle[3] = {0.0f, 5.0f, 0.0f};
    const float wall[3] = {13.0f, 5.0f, 0.0f};
    const float corner[3] = {13.0f, 14.0f, 0.0f};
    Near("the head in the middle of the house is 5 units in it",
         sm::Distance(house, middle), -5.0, 1e-4);
    Near("a head 3 units from the wall is 3 units out", sm::Distance(house, wall),
         3.0, 1e-4);
    Near("a head past the edge of the roof is at the distance to the edge",
         sm::Distance(house, corner), 5.0, 1e-4);
    const sm::Solid trunk = Trunk();
    const float in_trunk[3] = {0.5f, 3.0f, 0.0f};
    const float over[3] = {0.0f, 12.0f, 0.0f};
    Near("a head in a trunk is in it", sm::Distance(trunk, in_trunk), -1.5,
         1e-4);
    Near("a head over the trunk is out by the height", sm::Distance(trunk, over),
         2.0, 1e-4);
  }

  // THE WALLS OF A HOUSE BELOW THE LIFE SIZE SCALE (InsetLevel): the level
  // half sizes get smaller, the height stays, and a trunk does not change.
  {
    sm::Solid walls = House();
    sm::InsetLevel(&walls, 0.2f);
    Near("an inset of 20% makes the half width 8 units", walls.half[0], 8.0,
         1e-5);
    Near("the inset keeps the height", walls.half[1], 5.0, 1e-5);
    Near("the inset makes the half depth 8 units", walls.half[2], 8.0, 1e-5);
    const float eave[3] = {9.0f, 5.0f, 0.0f};
    const float deep[3] = {4.0f, 5.0f, 0.0f};
    Check(sm::Distance(house, eave) < 0.0f && sm::Distance(walls, eave) > 0.0f,
          "a head under the eave is in the whole box and out of the walls",
          sm::Distance(walls, eave), 1.0);
    Near("a head deep in the house is still in the walls",
         sm::Distance(walls, deep), -4.0, 1e-4);
    sm::Solid trunk = Trunk();
    sm::InsetLevel(&trunk, 0.2f);
    Near("the inset does not change a trunk", trunk.radius, 2.0, 1e-6);
    sm::Solid most = House();
    sm::InsetLevel(&most, 0.9f);
    Near("the inset is at most half of the box", most.half[0], 5.0, 1e-5);
    // The nearest solid.
    const sm::Solid two[2] = {House(), Trunk()};
    const float p[3] = {0.0f, 12.0f, 0.0f};
    int which = -2;
    const float least = sm::Least(two, 2, p, &which);
    Check(which == 0 && least > 1.99f && least < 2.01f,
          "the nearest of a house and a trunk is the roof 2 units under the "
          "head", least, 2.0);
    sm::Least(two, 0, p, &which);
    Check(which == -1, "no solid gives no index", which, -1);
  }
}

}  // namespace solid_checks
