// THE SSE PATHS OF THE GAME, ONE AT A TIME (game_sse_parts, docs/backend.md,
// "The SSE paths of the game, one at a time"). The pure part: the table of
// the readers of the flags, the bits, and the rules of the compare of the
// test aid game_sse_check. game_sse_parts.cpp has the patches and the hooks.
//
// THE FLAGS. fn_007AC9E0 sets the flag 0x00EA9EB4 from the class of the CPU
// (fn_007ACA00 and fn_008A2610, CPUID 1): 1 only for the family 15 with SSE2.
// fn_007ACA00 also sets the flag 0x00E83A20 for the same class. Each function
// below starts with a test of its flag: when the flag is not 0, it calls code
// of its own with SSE and returns. Else its own x87 code runs. The flag
// 0x00E83A30 has one more reader, a debug draw in fn_005F89F0 that also needs
// the flag 0x00C37C00, so it is not a part.
//
// THE PATCH. Each reader is one instruction "mov reg, [flag]". The mod puts
// the address of a flag of its own for the part in that instruction. Then
// the part takes its SSE path only when its flag of the mod is 1.
#pragma once

#include <stdint.h>
#include <string.h>

namespace game_sse_parts_math {

enum Part {
  kLand = 0,       // a patch of the land
  kTriangles,      // the triangles of a patch
  kTrianglesClip,  // the triangles of a patch that the near plane cuts
  kClipTriangle,   // the clip of a triangle
  kClipVertex,     // the new vertex of a clip
  kSprite,         // a sprite (LH3DSprite::Draw)
  kSphere,         // the test of a bounding sphere on the screen
  kLightPoint,     // the light of the land at a point
  kLightCell,      // the light of the land at a cell
  kTile,           // a tile of a texture of the land
  kTileBlend,      // the blend of four tiles of a texture of the land
  kSpans,          // the spans of a byte map
  kFloatToInt,     // float to integer (flag 0x00E83A20)
  kPartCount
};

const uint32_t kAllParts = (1u << kPartCount) - 1u;
const uint32_t kPathFlag = 0x00EA9EB4;   // set by fn_007AC9E0
const uint32_t kFloatFlag = 0x00E83A20;  // set by fn_007ACA00

struct PartInfo {
  const char* name;   // the short name of the part in the log
  uint32_t function;  // the function of the game that tests the flag
  uint32_t sse;       // the code with SSE that it calls
  bool compare;       // game_sse_check can compare its output
};

inline const PartInfo kParts[kPartCount] = {
    {"land patch", 0x00874AA0, 0x007A1800, true},
    {"patch triangles", 0x00876910, 0x007AA8F0, true},
    {"patch triangles with clip", 0x00875C60, 0x007A98B0, true},
    {"triangle clip", 0x0081A760, 0x007A3A50, true},
    {"clip vertex", 0x0081DD90, 0x007A1480, true},
    {"sprite", 0x00840530, 0x007A8DB0, false},
    {"sphere on screen", 0x00868C80, 0x007AC640, false},
    {"land light at a point", 0x00801C90, 0x007A3EC0, true},
    {"land light at a cell", 0x00802120, 0x007A4170, true},
    {"land texture tile", 0x008732C0, 0x007AB4B0, false},
    {"land texture blend", 0x00871850, 0x007ABF90, false},
    {"byte map spans", 0x00874440, 0x007AC4D0, false},
    {"float to integer", 0x007A1400, 0x007A1414, true},
};

// One reader: the instruction at `address` has `size` bytes, and its last
// four bytes are the address of the flag.
struct Reader {
  uint32_t address;
  uint8_t size;
  uint8_t bytes[6];
  int part;
};

inline const Reader kReaders[] = {
    {0x00874AA6, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kLand},
    {0x00876913, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kTriangles},
    {0x00875C63, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kTrianglesClip},
    {0x0081A760, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kClipTriangle},
    {0x0081A97D, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kClipTriangle},
    {0x0081DD96, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kClipVertex},
    {0x00840530, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kSprite},
    {0x00868C82, 6, {0x8B, 0x0D, 0xB4, 0x9E, 0xEA, 0x00}, kSphere},
    {0x00801C90, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kLightPoint},
    {0x00802120, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kLightCell},
    {0x008732C3, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kTile},
    {0x00871850, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kTileBlend},
    {0x00874446, 5, {0xA1, 0xB4, 0x9E, 0xEA, 0x00}, kSpans},
    {0x007A1400, 5, {0xA1, 0x20, 0x3A, 0xE8, 0x00}, kFloatToInt},
};
const int kReaderCount = static_cast<int>(sizeof(kReaders) / sizeof(kReaders[0]));

// The flag of the game that a reader reads: the last four bytes.
inline uint32_t ReaderFlag(const Reader& r) {
  uint32_t flag;
  memcpy(&flag, r.bytes + r.size - 4, 4);
  return flag;
}

// The flag of the game that the part reads without the patch.
inline uint32_t GameFlag(int part) {
  return part == kFloatToInt ? kFloatFlag : kPathFlag;
}

// game_sse_parts of bwvr.ini: 0 (the default) no patch, and the readers read
// the flags of the game. A value with bits: the bits of the parts. A bit
// that is not a part is dropped. A negative value is all parts.
inline uint32_t Mask(int value) {
  if (value < 0) return kAllParts;
  return static_cast<uint32_t>(value) & kAllParts;
}

// The flag of the mod for a part. `old_way`: the A/B of the test key gives
// each part the flag of the game, as with no patch.
inline int32_t PartFlag(uint32_t mask, int part, bool old_way,
                        int32_t game_flag) {
  if (old_way) return game_flag;
  return static_cast<int32_t>((mask >> part) & 1u);
}

// THE COMPARE. The largest difference of a float field. A NaN in one of the
// two counts as a NaN. Bits that are the same are no difference, also for a
// NaN in both.
struct FloatDiff {
  double most = 0.0;
  uint32_t differ = 0;  // values with a bit that differs
  uint32_t nan = 0;     // of those, values with a NaN in one or both

  void Add(uint32_t want, uint32_t got) {
    if (want == got) return;
    ++differ;
    float a, b;
    memcpy(&a, &want, 4);
    memcpy(&b, &got, 4);
    if (a != a || b != b) {
      ++nan;
      return;
    }
    double d = static_cast<double>(a) - static_cast<double>(b);
    if (d < 0) d = -d;
    if (d > most) most = d;
  }
};

// The largest difference of one byte of a color (a channel of a D3DCOLOR).
inline uint32_t ColorDiff(uint32_t a, uint32_t b) {
  uint32_t most = 0;
  for (int s = 0; s < 32; s += 8) {
    const int x = static_cast<int>((a >> s) & 0xFFu);
    const int y = static_cast<int>((b >> s) & 0xFFu);
    const uint32_t d = static_cast<uint32_t>(x > y ? x - y : y - x);
    if (d > most) most = d;
  }
  return most;
}

// The largest difference of the float fields of two vertices, as in
// VertexDiff. A NaN in one of the two gives a NaN difference: more than each
// number in the order of WorseThan. `skip_z`: the vertex stays clipped, so
// its z is no output of the x87 code.
inline double VertexMost(const uint32_t want[8], const uint32_t got[8],
                         bool skip_z = false) {
  static const int kFloatWord[6] = {0, 1, 2, 3, 6, 7};
  double most = 0.0;
  for (int f = 0; f < 6; ++f) {
    const int w = kFloatWord[f];
    if (want[w] == got[w] || (skip_z && w == 2)) continue;
    float a, b;
    memcpy(&a, &want[w], 4);
    memcpy(&b, &got[w], 4);
    if (a != a || b != b) return -1.0;
    double d = static_cast<double>(a) - static_cast<double>(b);
    if (d < 0) d = -d;
    if (d > most) most = d;
  }
  return most;
}

// True when the difference `d` of VertexMost is worse than `than`. -1 is a
// NaN, the worst.
inline bool WorseThan(double d, double than) {
  if (than < 0) return false;
  if (d < 0) return true;
  return d > than;
}

// A D3DTLVERTEX of the tables of LH3DP3 (32 bytes): x, y, z, rhw, diffuse,
// specular, u, v. The float fields are 0 to 5: x, y, z, rhw, u, v.
struct VertexDiff {
  FloatDiff field[6];
  uint32_t color_most = 0;  // the largest difference of a channel
  uint32_t colors = 0;      // colors with a bit that differs
  uint32_t flags = 0;       // clip flags that differ
  uint32_t vertices = 0;    // vertices with a bit that differs
  uint32_t values = 0;      // of those, vertices with a value that differs
  uint32_t clipped_z = 0;   // z of a clipped vertex that differs, not in field[2]

  // `want` is the output of the x87 path, `got` the output of the SSE path.
  // Gives true when a bit differs. The x87 code of the game writes no z for
  // a vertex that stays clipped (clip flags not 0), so its z is a value of
  // before. The SSE code writes each field. So z of a vertex with the same
  // clip flags not 0 in both counts in clipped_z only.
  bool Add(const uint32_t want[8], const uint32_t got[8], bool flags_on,
           uint32_t want_flag, uint32_t got_flag) {
    static const int kFloat[8] = {0, 1, 2, 3, -1, -1, 4, 5};
    const bool flag_differs = flags_on && want_flag != got_flag;
    const bool clipped = flags_on && want_flag != 0 && want_flag == got_flag;
    bool value_differs = false;
    for (int w = 0; w < 8; ++w) {
      if (want[w] == got[w]) continue;
      if (clipped && w == 2) {
        ++clipped_z;
        continue;
      }
      value_differs = true;
    }
    if (!value_differs && !flag_differs) return false;
    ++vertices;
    if (value_differs) ++values;
    for (int w = 0; w < 8; ++w) {
      if (clipped && w == 2) continue;
      if (kFloat[w] >= 0) {
        field[kFloat[w]].Add(want[w], got[w]);
      } else if (want[w] != got[w]) {
        ++colors;
        const uint32_t d = ColorDiff(want[w], got[w]);
        if (d > color_most) color_most = d;
      }
    }
    if (flag_differs) ++flags;
    return true;
  }
};

// FLOAT TO INTEGER. True when the double with these bits is a number of
// less than 2^31 in size. The two paths differ for the other values: the
// x87 path gives the low 32 bits of a 64 bit integer, the SSE path gives
// 0x80000000. Only the bits are read, so the compare needs no x87 register.
inline bool FtolInRange(uint64_t bits) {
  const uint32_t exponent = static_cast<uint32_t>(bits >> 52) & 0x7FFu;
  return exponent < 1023u + 31u;
}

// THE LAND. The x87 path of fn_00874AA0 sets the locals of its vertex loop
// from the patch before the loop (0x00874AC3 to 0x00874B3D). The SSE path
// of the game does not have them. So the compare makes them the same way:
// the step of the grid is 1 << [patch+0x930], the cells to skip at the end
// of a row are (step << 4) - 16, the step of the coordinates and of the
// texture is the step times a constant of the game. With [patch+0x938] == 1
// the texture step has one more factor, and the first u and v are values of
// the patch plus a constant. Else the first u and v are 0. Each product is
// of a power of 2, and each sum is one float add, so float math gives the
// bits of the x87 unit at 24 bits.
struct LandLocals {
  int32_t step;
  int32_t row_skip;
  float d;
  float du;
  float u0;
  float v0;
};

// shift: [patch+0x930]. kind: [patch+0x938]. u_at, v_at: [patch+0x9C4] and
// [patch+0x9C8]. The constants: [0x008AB414] (d), [0x008AB3D8] (du),
// [0x009A3C18] (the factor of du) and [0x009A3C14] (the add to u and v).
inline bool MakeLandLocals(int32_t shift, int32_t kind, float u_at, float v_at,
                           float k_d, float k_du, float k_du2, float k_uv,
                           LandLocals* out) {
  if (shift < 0 || shift > 4) return false;
  const int32_t step = 1 << shift;
  out->step = step;
  out->row_skip = (step << 4) - 16;
  const float s = static_cast<float>(step);
  out->d = s * k_d;
  out->du = s * k_du;
  out->u0 = 0.0f;
  out->v0 = 0.0f;
  if (kind == 1) {
    out->du = out->du * k_du2;
    out->v0 = v_at + k_uv;
    out->u0 = u_at + k_uv;
  }
  return true;
}

}  // namespace game_sse_parts_math
