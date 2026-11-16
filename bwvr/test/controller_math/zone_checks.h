// THE CAMERA ZONE OF THE GAME IN THE GOD VIEW (docs/god-view.md, "The camera
// zone"): camera_zone_math.h. main.cpp includes this file after Check and
// Near, just before main.
#pragma once

#include "../../src/mod/camera_zone_math.h"

namespace zone_checks {

inline void ZoneChecks() {
  using namespace camera_zone_math;
  printf("\nThe camera zone of the game in the God view\n");

  // A square of 100 units, and an L of a square with a corner cut out.
  const float square[] = {0, 0, 100, 0, 100, 100, 0, 100};
  const float ell[] = {0, 0, 100, 0, 100, 50, 50, 50, 50, 100, 0, 100};

  // INSIDE.
  {
    Check(Inside(square, 4, 50, 50), "the middle of the square is inside", 1,
          1);
    Check(!Inside(square, 4, 150, 50), "a point past the square is outside",
          0, 0);
    Check(!Inside(square, 4, -1, -1), "a point past a corner is outside", 0,
          0);
    Check(Inside(ell, 6, 25, 75), "the arm of the L is inside", 1, 1);
    Check(!Inside(ell, 6, 75, 75), "the cut corner of the L is outside", 0,
          0);
    Check(!Inside(square, 2, 50, 50), "two points have no inside", 0, 0);
  }

  // THE DISTANCE TO THE EDGE.
  {
    float nx = 0.0f, nz = 0.0f;
    Near("a point 30 units right of the square is 30 units from it",
         EdgeDistance(square, 4, 130, 50, &nx, &nz), 30.0, 1e-4);
    Near("its nearest edge point has x 100", nx, 100.0, 1e-4);
    Near("its nearest edge point has z 50", nz, 50.0, 1e-4);
    Near("a point past a corner is the diagonal from it",
         EdgeDistance(square, 4, 103, 104, &nx, &nz), 5.0, 1e-4);
    Near("a point inside is 0 units outside",
         OutsideDistance(square, 4, 40, 40), 0.0, 1e-6);
    Near("the cut corner of the L is 25 units from its edge",
         OutsideDistance(ell, 6, 75, 75), 25.0, 1e-4);
  }

  // THE LIMIT OF A MOVE.
  {
    float x = 80.0f, z = 50.0f;
    Check(!LimitMove(square, 4, 20.0f, 50, 50, &x, &z),
          "a move inside the zone is not limited", 0, 0);
    x = 115.0f;
    z = 50.0f;
    Check(!LimitMove(square, 4, 20.0f, 50, 50, &x, &z),
          "a move into the margin is not limited", 0, 0);
    x = 160.0f;
    z = 50.0f;
    Check(LimitMove(square, 4, 20.0f, 115, 50, &x, &z),
          "a move past the margin is limited", 1, 1);
    Near("it stops at the margin, x 120", x, 120.0, 1e-4);
    Near("and does not go along z", z, 50.0, 1e-4);
    // A move out and along the edge keeps the part along the edge.
    x = 150.0f;
    z = 70.0f;
    LimitMove(square, 4, 20.0f, 120, 50, &x, &z);
    Near("a move out and along the edge stays at the margin", x, 120.0, 1e-4);
    Near("and keeps its part along the edge", z, 70.0, 1e-4);
    // A place that the game put farther out than the margin does not jump.
    x = 300.0f;
    z = 50.0f;
    Check(!LimitMove(square, 4, 20.0f, 300, 50, &x, &z),
          "a place far out that does not move is not limited", 0, 0);
    x = 310.0f;
    z = 50.0f;
    LimitMove(square, 4, 20.0f, 300, 50, &x, &z);
    Near("from far out a move out stops where the place was", x, 300.0, 1e-4);
    x = 250.0f;
    z = 50.0f;
    Check(!LimitMove(square, 4, 20.0f, 300, 50, &x, &z),
          "from far out a move in is not limited", 0, 0);
    x = 130.0f;
    z = 50.0f;
    Check(LimitMove(square, 4, 0.0f, 100, 50, &x, &z),
          "with no margin a move out of the zone is limited", 1, 1);
    Near("and the place stays on the edge", x, 100.0, 1e-4);
    // A sweep of moves out: the place never goes past the margin, and it
    // never moves back toward the zone.
    float px = 50.0f, pz = 50.0f, worst = 0.0f, back = 0.0f;
    for (int i = 0; i < 400; ++i) {
      float nx = px + 1.3f, nz = pz + 0.4f;
      LimitMove(square, 4, 20.0f, px, pz, &nx, &nz);
      const float out = OutsideDistance(square, 4, nx, nz);
      if (out > worst) worst = out;
      const float was = OutsideDistance(square, 4, px, pz);
      if (was - out > back) back = was - out;
      px = nx;
      pz = nz;
    }
    Near("a sweep out never goes past the margin of 20 units", worst, 20.0,
         1e-3);
    Near("a sweep out never moves back toward the zone", back, 0.0, 1e-3);
  }

  // A POINT IN THE ZONE.
  {
    float x = 140.0f, z = 50.0f;
    Check(PointInside(square, 4, kInset, &x, &z),
          "a point outside moves into the zone", 1, 1);
    Near("to 5 units inside the edge", x, 95.0, 1e-4);
    Check(Inside(square, 4, x, z), "and it is inside", 1, 1);
    x = 75.0f;
    z = 75.0f;
    PointInside(ell, 6, kInset, &x, &z);
    Check(Inside(ell, 6, x, z), "the cut corner of the L moves inside", 1, 1);
    x = 30.0f;
    z = 30.0f;
    Check(!PointInside(square, 4, kInset, &x, &z),
          "a point inside does not move", 0, 0);
  }

  // THE GAME CAMERA STAYS IN THE ZONE.
  {
    const float focus[3] = {50.0f, 10.0f, 50.0f};
    float position[3] = {50.0f, 310.0f, -207.0f};
    Check(KeepCamera(square, 4, kInset, focus, position),
          "a camera behind the zone comes into it", 1, 1);
    Check(Inside(square, 4, position[0], position[2]),
          "and it is inside the zone", 1, 1);
    Near("it comes to 5 units before the edge, z 5", position[2], 5.0, 1e-3);
    // The line to the focus keeps its direction.
    const float was = 300.0f / 257.0f;
    const float is = (position[1] - focus[1]) / (focus[2] - position[2]);
    Near("the camera keeps its pitch", is, was, 1e-4);
    float inside[3] = {60.0f, 300.0f, 40.0f};
    Check(!KeepCamera(square, 4, kInset, focus, inside),
          "a camera in the zone does not move", 0, 0);
    // A focus near the edge, with the camera behind it out of the zone: the
    // camera goes to the side of the middle, kLeastAcross from the focus.
    const float edge_focus[3] = {50.0f, 10.0f, 8.0f};
    float behind[3] = {50.0f, 310.0f, -249.0f};
    KeepCamera(square, 4, kInset, edge_focus, behind);
    Check(Inside(square, 4, behind[0], behind[2]),
          "a camera behind a focus at the edge comes into the zone", 1, 1);
    const float bx = behind[0] - edge_focus[0], bz = behind[2] - edge_focus[2];
    Near("and it is kLeastAcross from its focus across the map",
         sqrtf(bx * bx + bz * bz), kLeastAcross, 1e-3);
    Near("with the same pitch", (behind[1] - edge_focus[1]) / kLeastAcross,
         300.0 / 257.0, 1e-4);
    const float far_focus[3] = {300.0f, 0.0f, 50.0f};
    float out[3] = {500.0f, 300.0f, 50.0f};
    KeepCamera(square, 4, kInset, far_focus, out);
    Check(Inside(square, 4, out[0], out[2]),
          "with a focus outside the zone the camera still comes into it", 1,
          1);
  }

  // THE POINT OF A BUMP.
  {
    float bx = 0.0f, bz = 0.0f;
    BumpPoint(square, 4, 10.0f, 120, 50, &bx, &bz);
    Near("a bump from a place past the edge is 10 units out, x 110", bx,
         110.0, 1e-4);
    Check(!Inside(square, 4, bx, bz), "and it is outside the zone", 0, 0);
    BumpPoint(square, 4, 10.0f, 100, 50, &bx, &bz);
    Check(!Inside(square, 4, bx, bz),
          "a bump from a place on the edge is outside the zone", 0, 0);
    BumpPoint(ell, 6, 10.0f, 75, 70, &bx, &bz);
    Check(!Inside(ell, 6, bx, bz),
          "a bump in the cut corner of the L is outside the zone", 0, 0);
    Near("and it is 10 units past the edge",
         OutsideDistance(ell, 6, bx, bz), 10.0, 1e-3);
  }
}

}  // namespace zone_checks
