// THE ANGLES AND THE ROTATIONS OF THE ANIMATIONS, the pure part (anim_trig,
// anim_trig.cpp, docs/backend.md, "The long game turn"). No Windows, no
// game. The checks are in test/controller_math/anim_trig_checks.h.
//
// Four functions of the game turn the angles of a bone into a rotation and
// back. They use the x87 instructions FSIN, FCOS and FPATAN. A translator of
// the code (FEX on a headset) runs each of these instructions as a call of a
// function, so a mixed animation of the Creature (Morphable::GetSetAnim)
// takes 19 to 33 ms there. The functions here give the same results with
// double arithmetic and polynomials, with no call:
//
//   Angle             fn_007FA990 (cdecl, two floats a and b): the angle of
//                     the point (a, b), by the octant of the point, from the
//                     arc tangent of the smaller over the larger coordinate
//   EulerToMatrix3    fn_007FAC10 (thiscall): three angles to a rotation of
//                     3 by 3 floats
//   EulerToMatrix34   fn_007FACC0 (thiscall): three angles to a transform of
//                     3 by 4 floats, with the zero of the game
//   MatrixToEuler     fn_007FAB30 (thiscall): a rotation to three angles
//
// Each value that the game keeps in a float (a store to memory) is a float
// here too, at the same step. The game keeps the other steps in the x87
// registers, at the precision control of the x87 unit: 24 bits of mantissa
// in a desktop run (the check shows it), 53 bits under FEX with its reduced
// precision. Here they are doubles. So a float result can differ from the
// result of the game by some units of the last place, and the check of
// anim_trig.cpp counts these cases.
//
// Sin and Cos: a reduction by pi / 2 in three parts (Cody and Waite), then
// the Taylor series on [-pi / 4, pi / 4] to the power 17 and 18. The first
// term that is left out is less than 1e-19. A value larger than 1e8 goes to
// the sine and the cosine of the C library. Atan: the arc tangent of the
// reciprocal for a value over 1, a step of pi / 4 for a value over
// tan(pi / 8), one half angle step, then the series to the power 25 on
// [-tan(pi / 16), tan(pi / 16)].

#pragma once

#include <emmintrin.h>
#include <math.h>
#include <stdint.h>

// The functions are small and run some 100000 times a second, so each one
// goes into its caller.
#if defined(_MSC_VER)
#define ANIM_TRIG_INLINE __forceinline
#else
#define ANIM_TRIG_INLINE inline __attribute__((always_inline))
#endif

namespace anim_trig {

// The constants of fn_007FA990, read from the game: three doubles and the
// float zero of the game.
struct Constants {
  double plus_quarter;   // 0x008C7B48: the angle of the y axis
  double half;           // 0x008D45D0: the half turn
  double minus_quarter;  // 0x009361E8: the angle of the minus y axis
  float zero;            // 0x008AA398
};

const double kPi = 3.14159265358979323846;
const double kHalfPi = 1.57079632679489661923;
const double kQuarterPi = 0.78539816339744830962;
// pi / 2 in three parts. The first two have 33 bits, so k times them is exact
// for each k below 2^20.
const double kHalfPi1 = 1.57079632673412561417e+00;
const double kHalfPi2 = 6.07710050650619224932e-11;
const double kHalfPi3 = 2.02226624879595063154e-21;
const double kTwoOverPi = 6.36619772367581382433e-01;
const double kTanEighthPi = 0.41421356237309504880;

// The sine and the cosine of r, for |r| <= pi / 4.
ANIM_TRIG_INLINE double SinSmall(double r) {
  const double z = r * r;
  // r - r^3/3! + r^5/5! - ... + r^17/17!
  double p = 1.0 / 355687428096000.0;  // 1/17!
  p = p * z - 1.0 / 1307674368000.0;   // 1/15!
  p = p * z + 1.0 / 6227020800.0;      // 1/13!
  p = p * z - 1.0 / 39916800.0;        // 1/11!
  p = p * z + 1.0 / 362880.0;          // 1/9!
  p = p * z - 1.0 / 5040.0;            // 1/7!
  p = p * z + 1.0 / 120.0;             // 1/5!
  p = p * z - 1.0 / 6.0;               // 1/3!
  return r + r * z * p;
}

ANIM_TRIG_INLINE double CosSmall(double r) {
  const double z = r * r;
  // 1 - r^2/2! + r^4/4! - ... - r^18/18!
  double p = -1.0 / 6402373705728000.0;  // 1/18!
  p = p * z + 1.0 / 20922789888000.0;    // 1/16!
  p = p * z - 1.0 / 87178291200.0;       // 1/14!
  p = p * z + 1.0 / 479001600.0;         // 1/12!
  p = p * z - 1.0 / 3628800.0;           // 1/10!
  p = p * z + 1.0 / 40320.0;             // 1/8!
  p = p * z - 1.0 / 720.0;               // 1/6!
  p = p * z + 1.0 / 24.0;                // 1/4!
  return 1.0 - 0.5 * z + z * z * p;
}

ANIM_TRIG_INLINE void SinCos(double x, double* s, double* c) {
  if (!(fabs(x) <= 1e8)) {
    // A large value, an infinity or NaN.
    *s = sin(x);
    *c = cos(x);
    return;
  }
  // The nearest whole number of quarter turns. cvtsd2si rounds to the
  // nearest (the rounding of MXCSR), and |x| <= 1e8 fits in 32 bits.
  const int quarters = _mm_cvtsd_si32(_mm_set_sd(x * kTwoOverPi));
  const double k = static_cast<double>(quarters);
  const double r = ((x - k * kHalfPi1) - k * kHalfPi2) - k * kHalfPi3;
  const double sr = SinSmall(r);
  const double cr = CosSmall(r);
  const int quadrant = quarters & 3;
  switch (quadrant) {
    case 0: *s = sr; *c = cr; break;
    case 1: *s = cr; *c = -sr; break;
    case 2: *s = -sr; *c = -cr; break;
    default: *s = -cr; *c = sr; break;
  }
}

ANIM_TRIG_INLINE double Sin(double x) {
  double s, c;
  SinCos(x, &s, &c);
  return s;
}

ANIM_TRIG_INLINE double Cos(double x) {
  double s, c;
  SinCos(x, &s, &c);
  return c;
}

// The arc tangent of t, for |t| <= tan(pi / 8), with the half angle step in
// it: atan(t) = 2 atan(t / (1 + sqrt(1 + t^2))), and |t / (1 + sqrt(1 +
// t^2))| <= tan(pi / 16).
ANIM_TRIG_INLINE double AtanSmall(double t) {
  const double v = t / (1.0 + sqrt(1.0 + t * t));
  const double z = v * v;
  // v - v^3/3 + v^5/5 - ... + v^25/25
  double p = 1.0 / 25.0;
  p = p * z - 1.0 / 23.0;
  p = p * z + 1.0 / 21.0;
  p = p * z - 1.0 / 19.0;
  p = p * z + 1.0 / 17.0;
  p = p * z - 1.0 / 15.0;
  p = p * z + 1.0 / 13.0;
  p = p * z - 1.0 / 11.0;
  p = p * z + 1.0 / 9.0;
  p = p * z - 1.0 / 7.0;
  p = p * z + 1.0 / 5.0;
  p = p * z - 1.0 / 3.0;
  return 2.0 * (v + v * z * p);
}

ANIM_TRIG_INLINE double Atan(double t) {
  if (t != t) return t;
  const bool negative = t < 0.0;
  double a = negative ? -t : t;
  double base = 0.0;
  bool reciprocal = false;
  if (a > 1.0) {
    a = 1.0 / a;
    reciprocal = true;
  }
  if (a > kTanEighthPi) {
    a = (a - 1.0) / (a + 1.0);
    base = kQuarterPi;
  }
  double r = base + AtanSmall(a);
  if (reciprocal) r = kHalfPi - r;
  return negative ? -r : r;
}

// The tests of the x87 compare of the game: FCOMP and a test of C0 (and of
// C3). An unordered compare sets both, so a NaN counts as less.
ANIM_TRIG_INLINE bool Less(double x, double y) { return !(x >= y); }
ANIM_TRIG_INLINE bool LessOrEqual(double x, double y) { return !(x > y); }

// fn_007FA990. a is the first argument, b the second.
ANIM_TRIG_INLINE double Angle(float a, float b, const Constants& k) {
  const double da = a, db = b;
  if (!Less(da, db) && LessOrEqual(-db, da)) return Atan(db / da);
  if (!Less(db, da) && LessOrEqual(-da, db)) {
    return k.plus_quarter - Atan(da / db);
  }
  if (!Less(-db, da) && Less(da, db)) {
    const double r = Atan(db / da);
    return Less(db, static_cast<double>(k.zero)) ? r - k.half : r + k.half;
  }
  return k.minus_quarter - Atan(da / db);
}

// fn_007FAC10: the angles a, b and c (its three arguments) to m[0..8].
ANIM_TRIG_INLINE void EulerToMatrix3(float* m, float a, float b, float c) {
  double sa, ca, sb, cb, sc, cc;
  SinCos(a, &sa, &ca);
  SinCos(b, &sb, &cb);
  SinCos(c, &sc, &cc);
  const float cbf = static_cast<float>(cb);
  const float scf = static_cast<float>(sc);
  const float t1 = static_cast<float>(cc * ca);
  const double x = static_cast<double>(scf) * sb;
  const float t2 = static_cast<float>(cc * sa);
  m[0] = static_cast<float>(t1 - x * sa);
  m[1] = static_cast<float>(-(static_cast<double>(scf) * cbf));
  m[2] = static_cast<float>(x * ca + t2);
  m[3] = static_cast<float>(t2 * sb + static_cast<double>(scf) * ca);
  m[4] = static_cast<float>(cc * cbf);
  m[5] = static_cast<float>(static_cast<double>(scf) * sa - t1 * sb);
  m[6] = static_cast<float>(-(static_cast<double>(cbf) * sa));
  m[7] = static_cast<float>(sb);
  m[8] = static_cast<float>(static_cast<double>(cbf) * ca);
}

// fn_007FACC0: the angles a, b and c (its three arguments) to m[0..11]. The
// sums with the zero of the game stay, so a zero keeps the sign that the
// game gives it.
ANIM_TRIG_INLINE void EulerToMatrix34(float* m, float a, float b, float c,
                                      float zero) {
  double sa, ca, sb, cb, sc, cc;
  SinCos(c, &sc, &cc);
  SinCos(b, &sb, &cb);
  SinCos(a, &sa, &ca);
  const double z = zero;
  const float cbf = static_cast<float>(cb);
  const float sbf = static_cast<float>(sb);
  const float caf = static_cast<float>(ca);
  const float saf = static_cast<float>(sa);
  const float n10 = static_cast<float>(-sc);
  const float k8 = static_cast<float>(sc * sbf + 0.0f);
  const float kc = static_cast<float>(z + cbf);
  const float k0 = static_cast<float>(cc * sbf + 0.0f);
  const float k4 = static_cast<float>(0.0f + z);
  const float q18 = static_cast<float>(cc * saf);
  m[0] = static_cast<float>(cc * caf + static_cast<double>(k8) * saf);
  m[1] = static_cast<float>(static_cast<double>(k8) * caf - q18);
  m[2] = static_cast<float>(sc * cbf - z);
  m[3] = static_cast<float>(static_cast<double>(kc) * saf + 0.0f);
  m[4] = static_cast<float>(static_cast<double>(kc) * caf - z);
  m[5] = static_cast<float>(0.0f - static_cast<double>(sbf));
  m[6] = static_cast<float>(static_cast<double>(n10) * caf +
                            static_cast<double>(k0) * saf);
  m[7] = static_cast<float>(static_cast<double>(k0) * caf -
                            static_cast<double>(n10) * saf);
  m[8] = static_cast<float>(cc * cbf - z);
  m[9] = static_cast<float>(static_cast<double>(k4) * saf + 0.0f);
  m[10] = static_cast<float>(static_cast<double>(k4) * caf - z);
  m[11] = static_cast<float>(0.0f - z);
}

// fn_007FAB30: the rotation src[0..11] (it reads 0, 1, 2, 6, 7 and 8) to the
// three angles. The game reads the source from a copy, so an output can be
// in the source.
ANIM_TRIG_INLINE void MatrixToEuler(const float* src, float* out_a,
                                    float* out_b, float* out_c,
                                    const Constants& k) {
  const float m0 = src[0], m1 = src[1], m2 = src[2];
  const float m6 = src[6], m7 = src[7], m8 = src[8];
  const double r1 = Angle(m8, -m6, k);
  *out_a = static_cast<float>(r1);
  double s1, c1;
  SinCos(-r1, &s1, &c1);
  const float c1f = static_cast<float>(c1);
  const float m0p = static_cast<float>(static_cast<double>(c1f) * m0 - m2 * s1);
  const float m2p = static_cast<float>(static_cast<double>(m2) * c1f + s1 * m0);
  const float arg = static_cast<float>(s1 * m6 + static_cast<double>(c1f) * m8);
  const double r2 = Angle(arg, m7, k);
  *out_b = static_cast<float>(r2);
  double s2, c2;
  SinCos(-r2, &s2, &c2);
  const float c2f = static_cast<float>(c2);
  const float arg2 =
      static_cast<float>(-(s2 * m2p + static_cast<double>(c2f) * m1));
  const double r3 = Angle(m0p, arg2, k);
  *out_c = static_cast<float>(r3);
}

// The distance of two floats in units of the last place (0 when they are the
// same, also +0 and -0). A NaN on one side only is far.
ANIM_TRIG_INLINE uint32_t Ulps(float x, float y) {
  if (x == y) return 0;
  if (x != x || y != y) return (x != x && y != y) ? 0 : 0xFFFFFFFFu;
  union { float f; int32_t i; } ux, uy;
  ux.f = x;
  uy.f = y;
  int64_t ix = ux.i < 0 ? static_cast<int64_t>(INT32_MIN) - ux.i : ux.i;
  int64_t iy = uy.i < 0 ? static_cast<int64_t>(INT32_MIN) - uy.i : uy.i;
  const int64_t d = ix > iy ? ix - iy : iy - ix;
  return d > 0xFFFFFFFFLL ? 0xFFFFFFFFu : static_cast<uint32_t>(d);
}

}  // namespace anim_trig
