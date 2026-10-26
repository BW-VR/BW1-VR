// THE SSE PATHS OF THE GAME, ONE AT A TIME (game_sse_parts_math.h,
// docs/backend.md, "The SSE paths of the game, one at a time"). main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../../src/mod/game_sse_parts_math.h"

namespace sse_parts_checks {

using namespace game_sse_parts_math;

inline uint32_t Bits(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}

inline void TableChecks() {
  // Each part has a reader, and each reader is at the start of the function
  // of its part and reads the flag of the game of its part.
  int readers[kPartCount] = {};
  bool unique = true;
  bool flags_ok = true;
  bool near_start = true;
  bool sizes_ok = true;
  for (int i = 0; i < kReaderCount; ++i) {
    const Reader& r = kReaders[i];
    ++readers[r.part];
    if (ReaderFlag(r) != GameFlag(r.part)) flags_ok = false;
    if (r.size != 5 && r.size != 6) sizes_ok = false;
    const uint32_t f = kParts[r.part].function;
    // The clip of a triangle tests its flag a second time, at its end.
    const uint32_t reach = r.part == kClipTriangle ? 0x240u : 0x10u;
    if (r.address < f || r.address >= f + reach) near_start = false;
    for (int j = 0; j < i; ++j) {
      if (kReaders[j].address == r.address) unique = false;
    }
  }
  bool each_part = true;
  for (int p = 0; p < kPartCount; ++p) {
    if (readers[p] < 1) each_part = false;
  }
  Check(each_part, "sse parts: each part has a reader", each_part, 1);
  Check(readers[kClipTriangle] == 2, "sse parts: the clip of a triangle has two readers",
        readers[kClipTriangle], 2);
  Check(kReaderCount == 14, "sse parts: 14 readers", kReaderCount, 14);
  Check(unique, "sse parts: each reader has its own address", unique, 1);
  Check(flags_ok, "sse parts: each reader reads the flag of its part", flags_ok, 1);
  Check(sizes_ok, "sse parts: each reader is 5 or 6 bytes", sizes_ok, 1);
  Check(near_start, "sse parts: each reader is at the start of its function",
        near_start, 1);
  Check(GameFlag(kFloatToInt) == 0x00E83A20u, "sse parts: float to integer reads 0x00E83A20",
        GameFlag(kFloatToInt), 0x00E83A20u);
  Check(GameFlag(kLand) == 0x00EA9EB4u, "sse parts: the land reads 0x00EA9EB4",
        GameFlag(kLand), 0x00EA9EB4u);
  Check(kAllParts == 0x1FFFu, "sse parts: 13 bits", kAllParts, 0x1FFFu);
}

inline void MaskChecks() {
  Check(Mask(0) == 0u, "sse parts: 0 is no part", Mask(0), 0);
  Check(Mask(-1) == kAllParts, "sse parts: -1 is each part", Mask(-1), kAllParts);
  Check(Mask(5) == 5u, "sse parts: 5 is the land and the triangles with clip",
        Mask(5), 5);
  Check(Mask(0x13001) == 0x1001u, "sse parts: bits that are no part go",
        Mask(0x13001), 0x1001);
  Check(PartFlag(4, kTrianglesClip, false, 0) == 1, "sse parts: bit 2 gives part 2 its SSE path",
        PartFlag(4, kTrianglesClip, false, 0), 1);
  Check(PartFlag(4, kLand, false, 1) == 0, "sse parts: no bit is the x87 path, also on a CPU of family 15",
        PartFlag(4, kLand, false, 1), 0);
  Check(PartFlag(4, kLand, true, 1) == 1, "sse parts: the old way gives the flag of the game",
        PartFlag(4, kLand, true, 1), 1);
  Check(PartFlag(kAllParts, kSpans, true, 0) == 0, "sse parts: the old way with the flag 0",
        PartFlag(kAllParts, kSpans, true, 0), 0);
  Check(PartFlag(1u << kFloatToInt, kFloatToInt, false, 0) == 1, "sse parts: bit 12 is float to integer",
        PartFlag(1u << kFloatToInt, kFloatToInt, false, 0), 1);
}

inline void DiffChecks() {
  FloatDiff f;
  f.Add(Bits(1.0f), Bits(1.0f));
  Check(f.differ == 0, "sse parts: the same bits do not differ", f.differ, 0);
  f.Add(Bits(1.0f), Bits(1.5f));
  f.Add(Bits(-2.0f), Bits(-2.25f));
  Check(f.differ == 2, "sse parts: two values differ", f.differ, 2);
  Near("sse parts: the largest difference", f.most, 0.5, 0.0);
  const float nan = sqrtf(-1.0f);
  f.Add(Bits(nan), Bits(3.0f));
  Check(f.nan == 1 && f.differ == 3, "sse parts: a NaN in one value", f.nan, 1);
  Near("sse parts: a NaN does not change the largest difference", f.most, 0.5, 0.0);
  FloatDiff z;
  z.Add(Bits(0.0f), Bits(-0.0f));
  Check(z.differ == 1, "sse parts: +0 and -0 differ in a bit", z.differ, 1);
  Near("sse parts: +0 and -0 have no difference of value", z.most, 0.0, 0.0);

  Check(ColorDiff(0xFF102030u, 0xFF102030u) == 0, "sse parts: the same color",
        ColorDiff(0xFF102030u, 0xFF102030u), 0);
  Check(ColorDiff(0xFF102030u, 0xF0122030u) == 0x0F, "sse parts: the largest channel difference",
        ColorDiff(0xFF102030u, 0xF0122030u), 0x0F);

  VertexDiff v;
  uint32_t a[8] = {Bits(10.0f), Bits(20.0f), Bits(0.5f), Bits(0.25f),
                   0xFF808080u, 0xFF000000u, Bits(0.0f), Bits(1.0f)};
  uint32_t b[8];
  memcpy(b, a, sizeof(a));
  Check(!v.Add(a, b, true, 0, 0), "sse parts: the same vertex", v.vertices, 0);
  b[1] = Bits(20.125f);
  b[4] = 0xFF818080u;
  Check(v.Add(a, b, false, 0, 4), "sse parts: a vertex that differs", v.vertices, 1);
  Near("sse parts: the y of the vertex", v.field[1].most, 0.125, 0.0);
  Check(v.colors == 1 && v.color_most == 1, "sse parts: the diffuse of the vertex",
        v.color_most, 1);
  Check(v.flags == 0, "sse parts: no flags with no clip", v.flags, 0);
  memcpy(b, a, sizeof(a));
  Check(v.Add(a, b, true, 0, 4), "sse parts: only the clip flags differ", v.flags, 1);
  Check(v.vertices == 2, "sse parts: two vertices differ", v.vertices, 2);
  Check(v.values == 1, "sse parts: one vertex has a value that differs", v.values, 1);

  VertexDiff cz;
  uint32_t zc[8];
  memcpy(zc, a, sizeof(a));
  zc[2] = Bits(3.0e38f);
  Check(!cz.Add(a, zc, true, 4, 4), "sse parts: z of a clipped vertex is no difference",
        cz.vertices, 0);
  Check(cz.clipped_z == 1 && cz.field[2].differ == 0, "sse parts: z of a clipped vertex counts apart",
        cz.clipped_z, 1);
  Check(cz.Add(a, zc, true, 0, 0), "sse parts: z of a vertex on the screen is a difference",
        cz.vertices, 1);
  Near("sse parts: the z of the vertex on the screen", cz.field[2].most, 3.0e38 - 0.5, 1e32);

  uint32_t c[8];
  memcpy(c, a, sizeof(a));
  Near("sse parts: the same vertex has no largest difference", VertexMost(a, c), 0.0, 0.0);
  c[2] = Bits(4.5f);
  c[7] = Bits(1.25f);
  Near("sse parts: the largest difference of a vertex", VertexMost(a, c), 4.0, 0.0);
  c[3] = Bits(sqrtf(-1.0f));
  Check(VertexMost(a, c) < 0, "sse parts: a NaN in a vertex", VertexMost(a, c), -1);
  Check(WorseThan(-1.0, 1e30) && !WorseThan(1e30, -1.0) && WorseThan(2.0, 1.0) &&
            !WorseThan(1.0, 1.0),
        "sse parts: the order of the differences", 1, 1);

  uint64_t bits;
  double d = 2147483647.0;
  memcpy(&bits, &d, 8);
  Check(FtolInRange(bits), "sse parts: 2^31 - 1 is in the range", 1, 1);
  d = -1e9;
  memcpy(&bits, &d, 8);
  Check(FtolInRange(bits), "sse parts: -1e9 is in the range", 1, 1);
  d = 2147483648.0;
  memcpy(&bits, &d, 8);
  Check(!FtolInRange(bits), "sse parts: 2^31 is out of the range", 0, 0);
  d = HUGE_VAL;
  memcpy(&bits, &d, 8);
  Check(!FtolInRange(bits), "sse parts: an infinity is out of the range", 0, 0);
}

inline void LandLocalsChecks() {
  LandLocals l;
  Check(!MakeLandLocals(5, 0, 0, 0, 1, 1, 1, 1, &l), "sse parts: a step over 16 is no patch",
        0, 0);
  Check(!MakeLandLocals(-1, 0, 0, 0, 1, 1, 1, 1, &l), "sse parts: a negative shift is no patch",
        0, 0);
  Check(MakeLandLocals(2, 0, 7.0f, 9.0f, 0.5f, 0.0625f, 3.0f, 0.25f, &l),
        "sse parts: the land locals", 1, 1);
  Check(l.step == 4 && l.row_skip == 48, "sse parts: the step and the cells to skip",
        l.row_skip, 48);
  Near("sse parts: the step of the coordinates", l.d, 2.0, 0.0);
  Near("sse parts: the step of the texture", l.du, 0.25, 0.0);
  Check(l.u0 == 0.0f && l.v0 == 0.0f, "sse parts: u and v start at 0", l.u0, 0);
  MakeLandLocals(4, 1, 7.0f, 9.0f, 0.5f, 0.0625f, 3.0f, 0.25f, &l);
  Check(l.step == 16 && l.row_skip == 240, "sse parts: the largest step", l.row_skip, 240);
  Near("sse parts: the step of the texture of kind 1", l.du, 3.0, 0.0);
  Near("sse parts: the first u of kind 1", l.u0, 7.25, 0.0);
  Near("sse parts: the first v of kind 1", l.v0, 9.25, 0.0);
}

inline void SsePartsChecks() {
  TableChecks();
  MaskChecks();
  DiffChecks();
  LandLocalsChecks();
}

}  // namespace sse_parts_checks
