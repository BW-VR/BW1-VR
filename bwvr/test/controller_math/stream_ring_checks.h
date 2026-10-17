// The ring of the live stream of the walks, a test (src/mod/stream_ring.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <stddef.h>

#include <thread>
#include <vector>

#include "../../src/mod/stream_ring.h"
#include "../../src/mod/stream_split.h"

namespace stream_ring_checks {

namespace sr = stream_ring;
namespace ss = scene_stream;

// A ring in the memory of the test: the head and the data, as in the file.
struct TestRing {
  std::vector<uint8_t> bytes;
  sr::Head* head = nullptr;
  uint8_t* data = nullptr;
  explicit TestRing(uint32_t capacity, uint32_t start = 0) {
    bytes.assign(sr::kHeadBytes + capacity, 0);
    head = reinterpret_cast<sr::Head*>(bytes.data());
    data = bytes.data() + sr::kHeadBytes;
    sr::MakeHead(head, capacity, 7);
    head->write = start;
    head->read = start;
  }
};

// One frame of the stream with no head of a stream: kFrame, a draw of n
// bytes of vertices, kFrameEnd. whole: kStart first.
inline std::vector<uint8_t> Unit(uint64_t frame, uint32_t n, bool whole) {
  ss::Writer w;
  if (whole) {
    ss::StartBody o = {};
    o.direct = 1;
    w.Record(ss::kStart, &o, sizeof(o));
  }
  ss::FrameBody f = {frame, 0, 0, 0};
  w.Record(ss::kFrame, &f, sizeof(f));
  std::vector<uint8_t> v(n);
  for (uint32_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(frame + i);
  ss::DrawBody d = {};
  d.vertex_bytes = n;
  w.Record(ss::kDraw, &d, sizeof(d), v.data(), n);
  w.Record(ss::kFrameEnd, &frame, sizeof(frame));
  return std::vector<uint8_t>(w.Bytes().begin() + 8, w.Bytes().end());
}

inline void StreamRingChecks() {
  printf("\nThe ring of the live stream of the walks\n");

  // THE HEAD has one layout on 32 and on 64 bits.
  Check(sizeof(sr::Head) == 1408 && offsetof(sr::Head, write) == 24 &&
            offsetof(sr::Head, read) == 28 && offsetof(sr::Head, stats) == 128,
        "the head has one layout for the mod and the app",
        static_cast<double>(sizeof(sr::Head)), 1408);
  {
    TestRing ring(64);
    Check(sr::HeadOk(ring.head, ring.bytes.size()) && ring.head->capacity == 64,
          "a new head is whole", ring.head->capacity, 64);
    Check(!sr::HeadOk(ring.head, ring.bytes.size() - 1),
          "a file that is shorter than the head and the data is not a ring", 0, 0);
    ring.head->capacity = 48;
    Check(!sr::HeadOk(ring.head, ring.bytes.size()),
          "a capacity that is not a power of two is not a ring", 48, 0);
  }

  // THE WRAP. 8 bytes at the index 12 of 16: 4 to the end, 4 from the start.
  uint32_t at = 0, first = 0, second = 0;
  sr::Split(16, 12, 8, &at, &first, &second);
  Check(at == 12 && first == 4 && second == 4, "8 bytes at 12 of 16 go 4 and 4",
        second, 4);
  sr::Split(16, 0xFFFFFFFCu, 8, &at, &first, &second);
  Check(at == 12 && first == 4 && second == 4,
        "the index modulo the capacity is its place, also near 2^32", at, 12);
  {
    // The indices start 6 bytes before 2^32, so they wrap in the test.
    TestRing ring(16, 0xFFFFFFFAu);
    uint8_t in[24], out[24] = {};
    for (int i = 0; i < 24; ++i) in[i] = static_cast<uint8_t>(100 + i);
    uint32_t put = sr::Put(ring.head, ring.data, in, 10);
    bool broken = false;
    uint32_t took = sr::Take(ring.head, ring.data, out, 24, &broken);
    Check(put == 10 && took == 10 && !broken && memcmp(in, out, 10) == 0 &&
              ring.head->write == 4u && ring.head->read == 4u,
          "10 bytes go in and out across the end of the data and 2^32",
          ring.head->write, 4);

    // A FULL RING: the writer gets room for 16 bytes, and no more.
    put = sr::Put(ring.head, ring.data, in, 24);
    Check(put == 16 && sr::Room(16, ring.head->write, ring.head->read) == 0,
          "a full ring takes only its capacity", put, 16);
    Check(sr::Put(ring.head, ring.data, in + 16, 8) == 0,
          "a full ring takes no byte", 0, 0);
    took = sr::Take(ring.head, ring.data, out, 5, &broken);
    put = sr::Put(ring.head, ring.data, in + 16, 8);
    Check(took == 5 && put == 5, "the room that the reader makes is the room of the writer",
          put, 5);
    took = sr::Take(ring.head, ring.data, out + 5, 24, &broken);
    Check(took == 16 && memcmp(out, in, 21) == 0,
          "the bytes come out in their order", took, 16);

    // Indices that are not valid: the reader must ask again.
    ring.head->read = ring.head->write + 1;
    took = sr::Take(ring.head, ring.data, out, 24, &broken);
    Check(took == 0 && broken && sr::Room(16, ring.head->write, ring.head->read) == 0,
          "a read index after the write index is broken", took, 0);
  }

  // THE CHOICE OF THE WRITER.
  {
    // vk_stream_live_drop_us=4000: a wait of 4 ms.
    sr::Policy p;
    p.drop_us = 4000;
    const uint32_t cap = 1000;
    Check(sr::Choose(cap, 500, 400, false, false, 0, 0, p) == sr::Step::kWrite,
          "a frame that fits goes in", 0, 0);
    Check(sr::Choose(cap, 100, 400, false, false, 3999, 3999, p) == sr::Step::kWait,
          "a frame with no surface waits for room for all of it", 0, 0);
    Check(sr::Choose(cap, 100, 400, false, false, 4000, 4000, p) == sr::Step::kDrop,
          "after 4 ms the writer drops a frame with no surface", 0, 0);
    Check(sr::Choose(cap, 100, 400, false, true, 4000, 4000, p) == sr::Step::kWrite,
          "a frame with a surface goes in in parts", 0, 0);
    Check(sr::Choose(cap, 0, 400, false, true, 1999999, 1999999, p) == sr::Step::kWait,
          "a frame with a surface waits while the reader takes bytes", 0, 0);
    Check(sr::Choose(cap, 0, 400, false, true, 2000000, 2000000, p) == sr::Step::kLost,
          "after 2 s with no byte taken the reader is lost", 0, 0);
    Check(sr::Choose(cap, 100, 400, true, false, 9000, 9000, p) == sr::Step::kWrite,
          "a frame in the ring in part is never dropped", 0, 0);
    Check(sr::Choose(cap, 100, 4000, false, false, 9000, 9000, p) == sr::Step::kWrite,
          "a frame larger than the ring goes in parts", 0, 0);
    Check(sr::Choose(cap, 0, 400, false, false, 9000, 2000000, p) == sr::Step::kLost,
          "a frame with no surface also ends at a lost reader", 0, 0);
    // The default: vk_stream_live_drop_us=0, a drop with no wait.
    const sr::Policy now;
    Check(now.drop_us == 0 &&
              sr::Choose(cap, 100, 400, false, false, 0, 0, now) == sr::Step::kDrop,
          "with the default the writer drops a frame with no surface at once", 0, 0);
    Check(sr::Choose(cap, 100, 400, false, true, 0, 0, now) == sr::Step::kWrite &&
              sr::Choose(cap, 0, 400, false, true, 0, 0, now) == sr::Step::kWait,
          "with the default a frame with a surface still waits for room", 0, 0);
    // A writer that can drop the walks keeps room for the records of state.
    Check(now.state_reserve == sr::kCapacity / 8 && sr::RoomForWalks(5000, 4000) == 1000 &&
              sr::RoomForWalks(3000, 4000) == 0 &&
              sr::Choose(cap, sr::RoomForWalks(4300, 4000), 400, false, false, 0, 0, now) ==
                  sr::Step::kDrop &&
              sr::Choose(cap, 4300, 400, false, true, 0, 0, now) == sr::Step::kWrite,
          "the walks leave 4 MB of the ring free, so the state of a dropped frame goes in at once",
          sr::RoomForWalks(5000, 4000), 1000);
    Check(sr::KeepKind(ss::kStart) && sr::KeepKind(ss::kSurface) &&
              sr::KeepKind(ss::kWritten) && !sr::KeepKind(ss::kDraw) &&
              !sr::KeepKind(ss::kTexture),
          "kStart, kSurface and kWritten keep a frame", 0, 0);
  }

  // THE FRAMES OF THE READER.
  {
    const std::vector<uint8_t> a = Unit(41, 30, true);
    const std::vector<uint8_t> b = Unit(42, 7, false);
    std::vector<uint8_t> both = a;
    both.insert(both.end(), b.begin(), b.end());
    bool bad = false, whole = false;
    uint64_t frame = 0;
    int short_ok = 0;
    for (size_t n = 0; n < a.size(); ++n) {
      if (sr::UnitBytes(both.data(), n, 1u << 20, &bad, &frame, &whole) == 0 && !bad) ++short_ok;
    }
    Check(short_ok == static_cast<int>(a.size()),
          "a frame that is not whole yet gives 0, at each length", short_ok,
          static_cast<double>(a.size()));
    size_t len = sr::UnitBytes(both.data(), both.size(), 1u << 20, &bad, &frame, &whole);
    Check(len == a.size() && frame == 41 && whole && !bad,
          "the first frame is whole: its length, its frame and its kStart",
          static_cast<double>(len), static_cast<double>(a.size()));
    len = sr::UnitBytes(both.data() + a.size(), b.size(), 1u << 20, &bad, &frame, &whole);
    Check(len == b.size() && frame == 42 && !whole,
          "the second frame is whole and has no kStart", static_cast<double>(frame), 42);
    const uint8_t junk[16] = {99, 0, 0, 0, 4, 0, 0, 0};
    Check(sr::UnitBytes(junk, sizeof(junk), 1u << 20, &bad, &frame, &whole) == 0 && bad,
          "a record of no kind is bad", 0, 0);
    len = sr::UnitBytes(a.data(), a.size(), 16, &bad, &frame, &whole);
    Check(len == 0 && bad, "a record larger than the limit is bad", 0, 0);
    // The player of the app reads a frame with no head of a stream.
    ss::Reader records = ss::Reader::Records(a.data(), a.size());
    ss::Kind kind;
    const uint8_t* body = nullptr;
    uint32_t bytes = 0;
    int kinds = 0;
    bool order = true;
    const ss::Kind want[4] = {ss::kStart, ss::kFrame, ss::kDraw, ss::kFrameEnd};
    while (records.Next(&kind, &body, &bytes)) {
      if (kinds < 4 && kind != want[kinds]) order = false;
      ++kinds;
    }
    Check(records.Ok() && kinds == 4 && order && !records.Broken(),
          "the records of a frame read back with no head of a stream", kinds, 4);
  }

  // THE FRAMES OF A BATCH IN THE APP.
  {
    const std::vector<uint8_t> whole = Unit(50, 8, true);
    const std::vector<uint8_t> plain = Unit(51, 8, false);
    const sr::UnitInfo a = sr::Inspect(whole.data(), whole.size());
    const sr::UnitInfo b = sr::Inspect(plain.data(), plain.size());
    Check(a.frame == 50 && a.keep && a.start && !b.keep && !b.start && b.frame == 51,
          "the app sees the frame and the kStart of a frame", static_cast<double>(b.frame), 51);
    ss::Writer w;
    ss::StartBody o = {};
    o.test_frame = 52;
    w.Record(ss::kStart, &o, sizeof(o));
    ss::FrameBody f = {52, 0, 1, 0};
    w.Record(ss::kFrame, &f, sizeof(f));
    w.Record(ss::kFrameEnd, &f.frame, sizeof(f.frame));
    const sr::UnitInfo c = sr::Inspect(w.Bytes().data() + 8, w.Bytes().size() - 8);
    Check(c.test_frame == 52 && c.capture, "the app reads the test frame of kStart and a capture",
          c.test_frame, 52);
    Check(sr::PlayInBatch(a, false, 0) && !sr::PlayInBatch(b, false, 0) &&
              sr::PlayInBatch(b, true, 0) && sr::PlayInBatch(b, false, 51),
          "a batch plays each frame with a surface, the newest frame and the frame of a "
          "test picture, and no other",
          0, 0);
  }

  // THE SPLIT OF A FRAME (stream_split.h): a frame with no walks keeps each
  // record of state in its order.
  {
    namespace sp = stream_split;
    ss::Writer w;
    ss::StartBody o = {};
    w.Record(ss::kStart, &o, sizeof(o));
    ss::FrameBody f = {60, 0, 0, 0};
    w.Record(ss::kFrame, &f, sizeof(f));
    ss::GazeBody g = {1, 0.5f, 0.5f};
    w.Record(ss::kGaze, &g, sizeof(g));
    const uint32_t one = 1;
    w.Record(ss::kOnlyFrame, &one, sizeof(one));
    ss::WalkBeginBody b = {};
    w.Record(ss::kWalkBegin, &b, sizeof(b));
    const uint32_t rs[2] = {27, 1};
    w.Record(ss::kRenderState, rs, sizeof(rs));
    ss::SurfaceBody sb = {0x1234, 0, 1, 1, 1};
    const uint8_t px[4] = {1, 2, 3, 4};
    w.Record(ss::kSurface, &sb, sizeof(sb), px, 4);
    ss::TextureBody t = {0, 0, 0x1234};
    w.Record(ss::kTexture, &t, sizeof(t));
    ss::DrawBody d = {};
    const uint8_t v[6] = {9, 9, 9, 9, 9, 9};
    d.vertex_bytes = 6;
    w.Record(ss::kDraw, &d, sizeof(d), v, 6);
    const uint64_t id = 0x1234;
    w.Record(ss::kWritten, &id, sizeof(id));
    ss::TargetBody tb = {};
    w.Record(ss::kTarget, &tb, sizeof(tb));
    ss::ClearBody c = {0, 3, 0, 1.0f};
    w.Record(ss::kClear, &c, sizeof(c));
    ss::ViewportBody vp = {};
    w.Record(ss::kViewport, &vp, sizeof(vp));
    ss::StreamState st = {};
    w.Record(ss::kWalkEnd, &st, sizeof(st));
    w.Record(ss::kPanelBind, &one, sizeof(one));
    w.Record(ss::kFrameEnd, &f.frame, sizeof(f.frame));
    const uint8_t* unit = w.Bytes().data() + 8;
    const size_t n = w.Bytes().size() - 8;
    std::vector<uint8_t> state;
    const uint32_t walks = sp::StateOnly(unit, n, &state);
    const ss::Kind want[9] = {ss::kStart, ss::kFrame, ss::kGaze, ss::kOnlyFrame, ss::kSurface,
                              ss::kWritten, ss::kTarget, ss::kPanelBind, ss::kFrameEnd};
    ss::Reader r = ss::Reader::Records(state.data(), state.size());
    ss::Kind kind;
    const uint8_t* body = nullptr;
    uint32_t bytes = 0;
    int count = 0;
    bool order = true;
    bool pixels = false;
    while (r.Next(&kind, &body, &bytes)) {
      if (count >= 9 || kind != want[count]) order = false;
      if (kind == ss::kSurface && bytes == sizeof(sb) + 4 && body[sizeof(sb) + 3] == 4) {
        pixels = true;
      }
      ++count;
    }
    Check(walks == 1 && count == 9 && order && pixels && !r.Broken(),
          "a frame with no walks keeps the 9 records of state in their order, with the pixels",
          count, 9);
    Check(sp::Walks(unit, n) == 1 && sp::Walks(state.data(), state.size()) == 0 &&
              state.size() % 4 == 0,
          "the frame had one walk, and the frame of state has none", sp::Walks(unit, n), 1);
    bool bad = false, whole = false;
    uint64_t frame = 0;
    Check(sr::UnitBytes(state.data(), state.size(), 1u << 20, &bad, &frame, &whole) ==
                  state.size() &&
              whole && frame == 60,
          "the frame of state is a whole frame of the ring with its kStart",
          static_cast<double>(frame), 60);
    ss::StreamState s = {};
    s.one_walk = 1;
    s.pass_eye = 0;
    const bool one_walk = sp::WalkEndsFrame(s);
    s.one_walk = 0;
    const bool left = sp::WalkEndsFrame(s);
    s.pass_eye = 1;
    const bool right = sp::WalkEndsFrame(s);
    s.pass_eye = -1;
    const bool no_pass = sp::WalkEndsFrame(s);
    Check(one_walk && !left && right && no_pass,
          "the one walk, the walk of the right eye and a walk with no eye pass end a frame, "
          "the walk of the left eye does not",
          0, 0);
    Check(sp::KeepWalks(4, 5, 1, false) && !sp::KeepWalks(3, 5, 1, false) &&
              sp::KeepWalks(2, 5, 3, false) && !sp::KeepWalks(1, 5, 3, false) &&
              sp::KeepWalks(0, 5, 1, true),
          "the queue keeps the walks of the newest frames and of a picture of a test", 0, 0);
  }

  // A READER THAT COMES LATE. The first reader takes the first frame, then it
  // stops in the second one. A second reader asks, the writer answers at the
  // start of its next frame, and the second reader starts there with the
  // whole state.
  {
    TestRing ring(1024, 0xFFFFFF80u);
    uint32_t answered = 0;
    const std::vector<uint8_t> f1 = Unit(1, 40, true);
    const std::vector<uint8_t> f2 = Unit(2, 40, false);
    sr::Put(ring.head, ring.data, f1.data(), static_cast<uint32_t>(f1.size()));
    sr::Put(ring.head, ring.data, f2.data(), static_cast<uint32_t>(f2.size()));
    std::vector<uint8_t> got(512);
    bool broken = false;
    sr::Take(ring.head, ring.data, got.data(), static_cast<uint32_t>(f1.size() + 10), &broken);
    const uint32_t stale = ring.head->read;
    const uint32_t ask = sr::Ask(ring.head);
    uint32_t sync = 0;
    Check(!sr::Answered(ring.head, ask, &sync), "an ask has no answer before the next frame",
          0, 0);
    const bool take = sr::TakeAsk(ring.head, &answered);
    const uint32_t frame_at = ring.head->write;
    const std::vector<uint8_t> f3 = Unit(3, 40, true);
    const uint32_t room = sr::Room(1024, ring.head->write, ring.head->read);
    const uint32_t put = sr::Put(ring.head, ring.data, f3.data(), static_cast<uint32_t>(f3.size()));
    Check(take && !sr::TakeAsk(ring.head, &answered),
          "the writer takes an ask one time", answered, ask);
    const bool answered_now = sr::Answered(ring.head, ask, &sync);
    Check(answered_now && sync == frame_at,
          "the answer gives the index of the frame with the whole state",
          sync - stale, frame_at - stale);
    Check(put == f3.size() && room >= f3.size(),
          "the stale read index of the first reader still holds the room",
          put, static_cast<double>(f3.size()));
    ring.head->read = sync;
    const uint32_t took = sr::Take(ring.head, ring.data, got.data(), 512, &broken);
    bool bad = false, whole = false;
    uint64_t frame = 0;
    const size_t len = sr::UnitBytes(got.data(), took, 1u << 20, &bad, &frame, &whole);
    Check(len == f3.size() && frame == 3 && whole && memcmp(got.data(), f3.data(), len) == 0,
          "the late reader gets frame 3 whole, with kStart", static_cast<double>(frame), 3);
    uint32_t seen = 0;
    sr::Leave(ring.head);
    const bool bye = sr::TakeBye(ring.head, &seen);
    const bool bye_again = sr::TakeBye(ring.head, &seen);
    Check(bye && !bye_again, "the writer sees the leave of a reader one time", seen, 1);
  }

  // THE ORDER OF THE INDICES between two threads. A ring of 4 KB, frames of
  // 1 to 9000 bytes: the frames wrap, fill the ring and go in parts. The
  // reader checks each byte. On x86 this test cannot show a fault of the
  // order that only ARM has, but it shows the math of the parts and the wrap.
  {
    TestRing ring(4096, 0xFFFFF000u);
    const int frames = 3000;
    std::thread writer([&ring, frames]() {
      std::vector<uint8_t> f;
      for (int i = 0; i < frames; ++i) {
        f = Unit(static_cast<uint64_t>(i), 1 + (static_cast<uint32_t>(i) * 7919u) % 9000u, i == 0);
        uint32_t off = 0;
        while (off < f.size()) {
          off += sr::Put(ring.head, ring.data, f.data() + off,
                         static_cast<uint32_t>(f.size() - off));
          if (off < f.size()) std::this_thread::yield();
        }
      }
    });
    std::vector<uint8_t> pending;
    std::vector<uint8_t> chunk(3000);
    int whole = 0, wrong = 0;
    bool broken = false;
    while (whole < frames && !broken) {
      const uint32_t n = sr::Take(ring.head, ring.data, chunk.data(), 3000, &broken);
      if (n == 0) {
        std::this_thread::yield();
        continue;
      }
      pending.insert(pending.end(), chunk.begin(), chunk.begin() + n);
      for (;;) {
        bool bad = false, starts = false;
        uint64_t frame = 0;
        const size_t len = sr::UnitBytes(pending.data(), pending.size(), 1u << 20, &bad,
                                         &frame, &starts);
        if (bad) ++wrong;
        if (len == 0 || bad) break;
        const std::vector<uint8_t> want =
            Unit(static_cast<uint64_t>(whole),
                 1 + (static_cast<uint32_t>(whole) * 7919u) % 9000u, whole == 0);
        if (len != want.size() || memcmp(pending.data(), want.data(), len) != 0) ++wrong;
        pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(len));
        ++whole;
      }
      if (wrong > 0) break;
    }
    writer.join();
    Check(whole == frames && wrong == 0 && !broken && pending.empty(),
          "3000 frames go through a ring of 4 KB in two threads, each byte in its order",
          whole, frames);
  }
}

}  // namespace stream_ring_checks
