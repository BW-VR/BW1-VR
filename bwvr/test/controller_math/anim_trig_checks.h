// The angles and the rotations of the animations, a test (anim_trig_math.h).
// main.cpp includes this file after Check and Near, just before main.
//
// The sine, the cosine and the arc tangent are compared with the C library
// and, in the 32 bit build, with the x87 instructions FSIN, FCOS and FPATAN.
// The four functions of the game are compared with the same formulas in
// doubles of the C library.
#pragma once

#include <math.h>
#include <stdint.h>

#include "../../src/mod/anim_trig_math.h"

namespace anim_trig_checks {

#if defined(_MSC_VER) && defined(_M_IX86)
inline double X87Sin(double x) {
  double r;
  __asm {
    fld x
    fsin
    fstp r
  }
  return r;
}
inline double X87Cos(double x) {
  double r;
  __asm {
    fld x
    fcos
    fstp r
  }
  return r;
}
inline double X87Atan(double t) {
  double r;
  __asm {
    fld t
    fld1
    fpatan
    fstp r
  }
  return r;
}
#endif

// The formulas of the four functions with the C library, for the compare.
inline double RefAngle(float a, float b, const anim_trig::Constants& k) {
  using anim_trig::Less;
  using anim_trig::LessOrEqual;
  const double da = a, db = b;
  if (!Less(da, db) && LessOrEqual(-db, da)) return atan(db / da);
  if (!Less(db, da) && LessOrEqual(-da, db)) {
    return k.plus_quarter - atan(da / db);
  }
  if (!Less(-db, da) && Less(da, db)) {
    const double r = atan(db / da);
    return Less(db, k.zero) ? r - k.half : r + k.half;
  }
  return k.minus_quarter - atan(da / db);
}

// The rotation of the three angles as the product of three turns, a matrix
// of rows: m[0..8] of SetYXZMatrixOnly.
inline void RefMatrix3(double* m, double a, double b, double c) {
  const double sa = sin(a), ca = cos(a), sb = sin(b), cb = cos(b);
  const double sc = sin(c), cc = cos(c);
  m[0] = cc * ca - sc * sb * sa;
  m[1] = -sc * cb;
  m[2] = sc * sb * ca + cc * sa;
  m[3] = sc * ca + cc * sa * sb;
  m[4] = cc * cb;
  m[5] = sc * sa - cc * ca * sb;
  m[6] = -cb * sa;
  m[7] = sb;
  m[8] = cb * ca;
}

inline void AnimTrigChecks() {
  using namespace anim_trig;
  printf("\nThe angles and the rotations of the animations\n");

  // The sine and the cosine against the C library, from -40 to 40.
  double worst_sin = 0.0, worst_cos = 0.0;
  for (int i = -40000; i <= 40000; ++i) {
    const double x = i * 0.001 + 0.000123;
    double s, c;
    SinCos(x, &s, &c);
    if (fabs(s - sin(x)) > worst_sin) worst_sin = fabs(s - sin(x));
    if (fabs(c - cos(x)) > worst_cos) worst_cos = fabs(c - cos(x));
  }
  Check(worst_sin < 4e-16, "the sine is the sine of the C library within 4e-16",
        worst_sin, 4e-16);
  Check(worst_cos < 4e-16,
        "the cosine is the cosine of the C library within 4e-16", worst_cos,
        4e-16);
  Near("the sine of 0 is 0", Sin(0.0), 0.0, 0.0);
  Near("the cosine of 0 is 1", Cos(0.0), 1.0, 0.0);
  Near("the sine of pi / 2 is 1", Sin(kHalfPi), 1.0, 1e-16);
  Near("the cosine of pi is -1", Cos(kPi), -1.0, 1e-16);
  // Values near the largest of the reduction (about 600000 quarter turns),
  // against values with 60 digits (a series in decimal arithmetic with pi to
  // 100 digits). The C library of the 32 bit build is 1.2e-15 off there.
  Near("the sine of 900000.3 is right within 3e-17", Sin(900000.3),
       0.02180602182407620019, 3e-17);
  Near("the cosine of -999999.7 is right within 3e-16", Cos(-999999.7),
       0.79148333657820919655, 3e-16);
  Near("a large value goes to the C library", Sin(1e9), sin(1e9), 0.0);
  const double nan_value = sqrt(-1.0);
  Check(Sin(nan_value) != Sin(nan_value), "the sine of NaN is NaN", 0, 0);

  // The arc tangent against the C library, from -50 to 50.
  double worst_atan = 0.0;
  for (int i = -50000; i <= 50000; ++i) {
    const double t = i * 0.001 + 0.0000371;
    const double e = fabs(Atan(t) - atan(t));
    if (e > worst_atan) worst_atan = e;
  }
  Check(worst_atan < 4e-16,
        "the arc tangent is the arc tangent of the C library within 4e-16",
        worst_atan, 4e-16);
  Near("the arc tangent of 1 is pi / 4", Atan(1.0), kQuarterPi, 1e-16);
  Near("the arc tangent of -1e30 is -pi / 2", Atan(-1e30), -kHalfPi, 1e-16);
  Near("the arc tangent of 0 is 0", Atan(0.0), 0.0, 0.0);
  Check(signbit(Atan(-0.0)) != 0, "the arc tangent of -0 is -0, as FPATAN", 0,
        0);

#if defined(_MSC_VER) && defined(_M_IX86)
  // The x87 instructions of the game, rounded to a float as the game keeps
  // them. Count the floats that differ.
  int sin_diff = 0, atan_diff = 0, count = 0;
  for (int i = -20000; i <= 20000; ++i) {
    const float x = static_cast<float>(i * 0.00031);
    double s, c;
    SinCos(x, &s, &c);
    if (static_cast<float>(s) != static_cast<float>(X87Sin(x))) ++sin_diff;
    if (static_cast<float>(c) != static_cast<float>(X87Cos(x))) ++sin_diff;
    const float t = static_cast<float>(i * 0.00005);
    if (static_cast<float>(Atan(t)) != static_cast<float>(X87Atan(t))) {
      ++atan_diff;
    }
    ++count;
  }
  Check(sin_diff == 0,
        "the float of the sine and the cosine is the float of FSIN and FCOS",
        sin_diff, 0);
  Check(atan_diff == 0,
        "the float of the arc tangent is the float of FPATAN", atan_diff, 0);
#endif

  // The constants of the game: the y axis, the half turn, the minus y axis.
  Constants k;
  k.plus_quarter = kHalfPi;
  k.half = kPi;
  k.minus_quarter = -kHalfPi;
  k.zero = 0.0f;

  // The angle of a point in each octant, and its compare with atan2.
  Near("the angle of (1, 0) is 0", Angle(1.0f, 0.0f, k), 0.0, 0.0);
  Check(signbit(Angle(1.0f, -0.0f, k)) != 0,
        "the angle of (1, -0) is -0, as the code of the game", 0, 0);
  Near("the angle of (0, 1) is pi / 2", Angle(0.0f, 1.0f, k), kHalfPi, 1e-15);
  Near("the angle of (-1, 0) is pi", Angle(-1.0f, 0.0f, k), kPi, 1e-15);
  Near("the angle of (0, -1) is -pi / 2", Angle(0.0f, -1.0f, k), -kHalfPi,
       1e-15);
  Near("the angle of (-1, -0.5) is below -pi / 2", Angle(-1.0f, -0.5f, k),
       atan2(-0.5, -1.0), 1e-15);
  double worst_angle = 0.0;
  for (int i = 0; i < 3600; ++i) {
    const double w = i * (2.0 * kPi / 3600.0) + 0.0003;
    const float a = static_cast<float>(cos(w) * 3.0);
    const float b = static_cast<float>(sin(w) * 3.0);
    const double e = fabs(Angle(a, b, k) - RefAngle(a, b, k));
    if (e > worst_angle) worst_angle = e;
  }
  Check(worst_angle < 1e-15,
        "the angle of 3600 points is the angle of the C library within 1e-15",
        worst_angle, 1e-15);

  // SetYXZMatrixOnly against the product of the three turns, and the way back.
  float m[12];
  float back_a = 0.0f, back_b = 0.0f, back_c = 0.0f;
  double worst_matrix = 0.0, worst_back = 0.0;
  for (int i = 0; i < 2000; ++i) {
    const float a = static_cast<float>(sin(i * 0.37) * 1.4);
    const float b = static_cast<float>(sin(i * 0.53) * 1.4);
    const float c = static_cast<float>(sin(i * 0.71) * 1.4);
    SetYXZMatrixOnly(m, a, b, c);
    double r[9];
    RefMatrix3(r, a, b, c);
    for (int j = 0; j < 9; ++j) {
      if (fabs(m[j] - r[j]) > worst_matrix) worst_matrix = fabs(m[j] - r[j]);
    }
    m[9] = m[10] = m[11] = 0.0f;
    GetYXZ(m, &back_a, &back_b, &back_c, k);
    // The matrix of the three angles back is the same rotation.
    double r2[9];
    RefMatrix3(r2, back_a, back_b, back_c);
    for (int j = 0; j < 9; ++j) {
      if (fabs(r2[j] - r[j]) > worst_back) worst_back = fabs(r2[j] - r[j]);
    }
  }
  Check(worst_matrix < 3e-7,
        "the rotation of three angles is the product of the three turns",
        worst_matrix, 3e-7);
  Check(worst_back < 1e-6,
        "the angles of a rotation give the same rotation again", worst_back,
        1e-6);

  // EulerToMatrix34: the same turns, with no translation.
  EulerToMatrix34(m, 0.0f, 0.0f, 0.0f, 0.0f);
  Check(m[0] == 1.0f && m[4] == 1.0f && m[8] == 1.0f && m[1] == 0.0f &&
            m[9] == 0.0f && m[11] == 0.0f,
        "the transform of three zero angles is the identity", 0, 0);
  EulerToMatrix34(m, 0.3f, -0.2f, 0.5f, 0.0f);
  const double len0 = m[0] * m[0] + m[1] * m[1] + m[2] * m[2];
  const double len1 = m[3] * m[3] + m[4] * m[4] + m[5] * m[5];
  const double dot01 = m[0] * m[3] + m[1] * m[4] + m[2] * m[5];
  Near("a row of the transform has the length 1", len0, 1.0, 1e-6);
  Near("a second row has the length 1", len1, 1.0, 1e-6);
  Near("two rows are at a right angle", dot01, 0.0, 1e-6);

  // The units of the last place.
  Near("the same float is 0 units", Ulps(1.0f, 1.0f), 0, 0);
  Near("+0 and -0 are 0 units", Ulps(0.0f, -0.0f), 0, 0);
  Near("the next float is 1 unit", Ulps(1.0f, 1.0000001f), 1, 0);
  Near("the floats round 0 are 2 units apart",
       Ulps(1.4e-45f, -1.4e-45f), 2, 0);
}

}  // namespace anim_trig_checks
