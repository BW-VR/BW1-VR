// The stream of the scene walks, a test (src/mod/scene_stream.h). main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include "../../src/mod/scene_stream.h"

namespace stream_checks {

inline void StreamChecks() {
  namespace ss = scene_stream;
  printf("\nThe stream of the scene walks for the headset app\n");

  ss::Writer w;
  const uint32_t rs[2] = {27, 1};
  w.Record(ss::kRenderState, rs, sizeof(rs));
  // A draw with 3 vertices of 20 bytes and 3 indices: 6 bytes of indices,
  // so the next record starts 2 bytes later on 4 bytes.
  ss::DrawBody draw = {};
  draw.type = 4;
  draw.fvf = 0x044;
  draw.vertex_count = 3;
  draw.index_count = 3;
  draw.vertex_bytes = 60;
  uint8_t vertices[60];
  for (int i = 0; i < 60; ++i) vertices[i] = static_cast<uint8_t>(i);
  const uint16_t indices[3] = {0, 1, 2};
  w.Record(ss::kDraw, &draw, sizeof(draw), vertices, 60, indices, 6);
  const uint64_t frame = 77;
  w.Record(ss::kFrameEnd, &frame, sizeof(frame));
  Check(w.Bytes().size() % 4 == 0, "the stream ends on 4 bytes",
        static_cast<double>(w.Bytes().size() % 4), 0);

  ss::Reader r(w.Bytes().data(), w.Bytes().size());
  Check(r.Ok(), "the reader knows the head of the stream", 0, 0);
  ss::Kind kind;
  const uint8_t* body = nullptr;
  uint32_t bytes = 0;
  Check(r.Next(&kind, &body, &bytes) && kind == ss::kRenderState && bytes == 8,
        "the first record is the render state", bytes, 8);
  Check(r.Next(&kind, &body, &bytes) && kind == ss::kDraw &&
            bytes == sizeof(ss::DrawBody) + 66,
        "the second record is the draw with its vertices and indices", bytes,
        sizeof(ss::DrawBody) + 66);
  ss::DrawBody got = {};
  Check(ss::Body(body, bytes, &got) && got.vertex_count == 3 && got.fvf == 0x044,
        "the body of the draw reads back", got.vertex_count, 3);
  const uint8_t* v = body + sizeof(ss::DrawBody);
  uint16_t i2 = 0;
  memcpy(&i2, v + 60 + 4, 2);
  Check(v[59] == 59 && i2 == 2, "the vertices and the indices read back", i2, 2);
  uint64_t end = 0;
  Check(r.Next(&kind, &body, &bytes) && kind == ss::kFrameEnd &&
            ss::Body(body, bytes, &end) && end == 77,
        "the third record starts on 4 bytes after the indices",
        static_cast<double>(end), 77);
  Check(!r.Next(&kind, &body, &bytes) && !r.Broken(), "then the stream ends",
        0, 0);

  // A stream that stops in a record is broken, not ended.
  ss::Reader cut(w.Bytes().data(), w.Bytes().size() - 6);
  int records = 0;
  while (cut.Next(&kind, &body, &bytes)) ++records;
  Check(records == 2 && cut.Broken(), "a cut stream gives 2 records and Broken",
        records, 2);
  const uint8_t junk[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  ss::Reader bad(junk, sizeof(junk));
  Check(!bad.Ok() && !bad.Next(&kind, &body, &bytes),
        "a file that is not a stream gives no record", 0, 0);

  // The layouts that the PC (32 bit) and the app (64 bit) share.
  Check(sizeof(ss::StreamState) == 40 && sizeof(ss::DrawBody) == 64 &&
            sizeof(ss::WalkBeginBody) == 8 + 40 + 64 * 4 + 2 * 32 * 4 + 16 + 24 + 16,
        "the bodies have one size on 32 and on 64 bits",
        static_cast<double>(sizeof(ss::WalkBeginBody)),
        8 + 40 + 256 + 256 + 16 + 24 + 16);
}

}  // namespace stream_checks
