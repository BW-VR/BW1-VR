// THE RING OF THE LIVE STREAM OF THE WALKS (docs/android.md, "The live
// stream"). No Windows code, so the mod, the headset app, the test reader of
// the PC and the math test use it (test/controller_math/stream_ring_checks.h).
//
// The ring is a real file that two processes map: the mod (the writer,
// CreateFileMapping on the PC and in Wine) and the reader (mmap with
// MAP_SHARED in the app, CreateFileMapping in the test reader of the PC). A
// named section of Wine is not visible to the app, so it must be a file.
//
// The file is a head of kHeadBytes and then the data: capacity bytes, a
// power of two. The data is a pipe of bytes: the records of scene_stream.h
// with no head of a stream, one frame after the other. A frame (a unit) is
// the records from one kFrame of the game to its kFrameEnd. A unit that
// starts with kStart holds the whole state: the options, and each surface
// before its first use.
//
// The indices write and read count the bytes since the start (mod 2^32). The
// writer owns write, the reader owns read. Each one stores its index with a
// release after its copy, and loads the index of the other one with an
// acquire before its copy. x86 has TSO, but the writer runs in Box64 on ARM
// in the headset, and ARM has a weak memory model: the interlocked calls
// give the translated code its barriers.
//
// A READER THAT COMES LATE (or one that lost its place) asks for the whole
// state: it adds one to ask. At the start of its next frame the writer sets
// sync to its write index and answer to that ask, and the frame that it
// writes then starts with kStart. The reader sets read to sync and reads
// from there. A reader that leaves adds one to bye, and the writer stops
// until the next ask.
//
// A FULL RING (a slow reader). A frame with no kStart, kSurface or kWritten
// record waits up to Policy::drop_us (vk_stream_live_drop_us, 0 by default)
// for room for the whole frame. Then the writer drops it: the reader keeps
// each texture, because such a frame changes no surface. A frame with one of
// these records is never dropped: it goes into the ring in parts as the
// reader makes room. When the reader takes no byte for Policy::gone_us, the
// writer gives up on it (lost) and stops until the next ask.
#pragma once

#include <stdint.h>
#include <string.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include "scene_stream.h"

namespace stream_ring {

constexpr uint32_t kMagic = 0x52535742u;  // "BWSR"
constexpr uint32_t kVersion = 1;
constexpr uint32_t kHeadBytes = 4096;
constexpr uint32_t kCapacity = 32u << 20;  // 32 MB
constexpr int kStats = 64;

// The size, the wait and the fate of one frame, for a test reader.
struct FrameStat {
  uint32_t frame;
  uint32_t bytes;
  uint32_t wait_us;  // the time that the writer waited for room
  uint32_t put_us;   // the time of the frame in the writer: the waits and the copy
  uint32_t dropped;  // 1: the frame did not go into the ring
};

struct Head {
  uint32_t magic;       // kMagic when the head is whole
  uint32_t version;     // kVersion
  uint32_t head_bytes;  // the data starts here
  uint32_t capacity;    // the bytes of the data, a power of two
  uint32_t session;     // new for each start of the writer
  uint32_t frame;       // the writer: the last frame of the game
  uint32_t write;       // the writer: the bytes written
  uint32_t read;        // the reader: the bytes read
  uint32_t ask;         // the reader: the count of its asks for the whole state
  uint32_t answer;      // the writer: the last ask that it answered
  uint32_t sync;        // the writer: the write index of the frame of the answer
  uint32_t lost;        // the writer: the count of the times it gave up on the reader
  uint32_t bye;         // the reader: the count of the times it left
  uint32_t units;       // the writer: the frames that went into the ring
  uint32_t dropped;     // the writer: the frames that it dropped
  uint32_t wait_ms;     // the writer: the sum of its waits for room
  uint32_t wait_us_max; // the writer: the longest wait for room for one frame
  uint32_t drop_us_max; // the writer: the longest time in the writer of a dropped frame
  uint32_t reserved[14];
  FrameStat stats[kStats];  // the frame f is at f % kStats
};
static_assert(sizeof(Head) == 128 + kStats * 20, "Head");
static_assert(sizeof(Head) <= kHeadBytes, "Head fits");

// THE INDICES between the processes. The writer is x86 code: the interlocked
// calls are lock cmpxchg and xchg, which Box64 translates with barriers.
inline uint32_t LoadAcquire(const uint32_t* p) {
#ifdef _MSC_VER
  return static_cast<uint32_t>(_InterlockedCompareExchange(
      reinterpret_cast<volatile long*>(const_cast<uint32_t*>(p)), 0, 0));
#else
  return __atomic_load_n(p, __ATOMIC_ACQUIRE);
#endif
}

inline void StoreRelease(uint32_t* p, uint32_t v) {
#ifdef _MSC_VER
  _InterlockedExchange(reinterpret_cast<volatile long*>(p), static_cast<long>(v));
#else
  __atomic_store_n(p, v, __ATOMIC_RELEASE);
#endif
}

// THE MATH OF THE RING. The indices wrap at 2^32, and the capacity is a
// power of two, so an index modulo the capacity is its place in the data.
inline bool PowerOfTwo(uint32_t n) { return n != 0 && (n & (n - 1)) == 0; }
inline uint32_t Used(uint32_t write, uint32_t read) { return write - read; }
// The room for the writer. 0 when the indices are not valid.
inline uint32_t Room(uint32_t capacity, uint32_t write, uint32_t read) {
  const uint32_t used = Used(write, read);
  return used > capacity ? 0 : capacity - used;
}
// The two parts of n bytes at an index: up to the end of the data, and from
// its start.
inline void Split(uint32_t capacity, uint32_t index, uint32_t n,
                  uint32_t* at, uint32_t* first, uint32_t* second) {
  *at = index & (capacity - 1);
  const uint32_t to_end = capacity - *at;
  *first = n < to_end ? n : to_end;
  *second = n - *first;
}
inline void CopyIn(uint8_t* data, uint32_t capacity, uint32_t index,
                   const uint8_t* from, uint32_t n) {
  uint32_t at, first, second;
  Split(capacity, index, n, &at, &first, &second);
  if (first > 0) memcpy(data + at, from, first);
  if (second > 0) memcpy(data, from + first, second);
}
inline void CopyOut(const uint8_t* data, uint32_t capacity, uint32_t index,
                    uint8_t* to, uint32_t n) {
  uint32_t at, first, second;
  Split(capacity, index, n, &at, &first, &second);
  if (first > 0) memcpy(to, data + at, first);
  if (second > 0) memcpy(to + first, data, second);
}

// THE WRITER: as many of n bytes as there is room for. Gives the count.
inline uint32_t Put(Head* h, uint8_t* data, const uint8_t* from, uint32_t n) {
  const uint32_t w = h->write;  // the writer owns it
  const uint32_t r = LoadAcquire(&h->read);
  const uint32_t room = Room(h->capacity, w, r);
  const uint32_t k = n < room ? n : room;
  if (k == 0) return 0;
  CopyIn(data, h->capacity, w, from, k);
  StoreRelease(&h->write, w + k);
  return k;
}

// THE READER: up to max bytes. Gives the count. broken: the indices are not
// valid (more bytes than the capacity), and the reader must ask again.
inline uint32_t Take(Head* h, const uint8_t* data, uint8_t* to, uint32_t max,
                     bool* broken) {
  const uint32_t r = h->read;  // the reader owns it
  const uint32_t w = LoadAcquire(&h->write);
  uint32_t n = Used(w, r);
  *broken = n > h->capacity;
  if (*broken) return 0;
  if (n > max) n = max;
  if (n == 0) return 0;
  CopyOut(data, h->capacity, r, to, n);
  StoreRelease(&h->read, r + n);
  return n;
}

// THE HEAD OF A NEW WRITER. magic goes in last, so a reader sees a whole
// head or none.
inline void MakeHead(Head* h, uint32_t capacity, uint32_t session) {
  StoreRelease(&h->magic, 0);
  memset(reinterpret_cast<uint8_t*>(h) + 4, 0, sizeof(Head) - 4);
  h->version = kVersion;
  h->head_bytes = kHeadBytes;
  h->capacity = capacity;
  h->session = session;
  StoreRelease(&h->magic, kMagic);
}

// True when the head is a whole head of this version.
inline bool HeadOk(const Head* h, uint64_t file_bytes) {
  return LoadAcquire(&h->magic) == kMagic && h->version == kVersion &&
         h->head_bytes == kHeadBytes && PowerOfTwo(h->capacity) &&
         file_bytes >= static_cast<uint64_t>(h->head_bytes) + h->capacity;
}

// THE ASK FOR THE WHOLE STATE.
// The reader: gives the number of its ask.
inline uint32_t Ask(Head* h) {
  const uint32_t ask = LoadAcquire(&h->ask) + 1;
  StoreRelease(&h->ask, ask);
  return ask;
}
// The reader: true when the writer answered the ask. Then sync is the index
// where the frame with the whole state starts.
inline bool Answered(const Head* h, uint32_t ask, uint32_t* sync) {
  if (LoadAcquire(&h->answer) != ask) return false;
  *sync = LoadAcquire(&h->sync);
  return true;
}
// The writer, at the start of a frame, before it writes the frame: true when
// a reader asked since the last answer. Then the writer must write the whole
// state in this frame. sync goes in before answer, so the reader that sees
// the answer sees its sync.
inline bool TakeAsk(Head* h, uint32_t* answered) {
  const uint32_t ask = LoadAcquire(&h->ask);
  if (ask == *answered) return false;
  StoreRelease(&h->sync, h->write);
  StoreRelease(&h->answer, ask);
  *answered = ask;
  return true;
}
// The reader: it leaves. The writer: true when the reader left since the
// last look.
inline void Leave(Head* h) { StoreRelease(&h->bye, LoadAcquire(&h->bye) + 1); }
inline bool TakeBye(Head* h, uint32_t* seen) {
  const uint32_t bye = LoadAcquire(&h->bye);
  if (bye == *seen) return false;
  *seen = bye;
  return true;
}

// THE CHOICE OF THE WRITER for one frame, at each step of its wait.
struct Policy {
  uint32_t drop_us = 0;        // the wait for a frame that the writer can drop
  uint32_t gone_us = 2000000;  // no byte taken for this time: the reader is lost
  // With a drop that keeps the state (vk_stream_live_drop=1): the room that
  // the walks leave free for the records of state, so the state of a dropped
  // frame goes in with no wait.
  uint32_t state_reserve = kCapacity / 8;
};

// The room for a whole frame when the walks leave state_reserve bytes free.
inline uint32_t RoomForWalks(uint32_t room, uint32_t reserve) {
  return room > reserve ? room - reserve : 0;
}

enum class Step { kWrite, kWait, kDrop, kLost };

// left: the bytes of the frame that are not in the ring yet. started: a part
// of the frame is in the ring. keep: the frame has a kStart, a kSurface or a
// kWritten record. waited_us: the wait for this frame so far. stalled_us:
// the time since the reader last took a byte, or since the ring was empty.
inline Step Choose(uint32_t capacity, uint32_t room, uint32_t left,
                   bool started, bool keep, uint32_t waited_us,
                   uint32_t stalled_us, const Policy& p) {
  if (left == 0) return Step::kWrite;
  if (left <= room) return Step::kWrite;
  // A frame that is larger than the ring cannot wait for room for all of
  // it, and a frame in the ring in part must be whole.
  const bool parts = keep || started || left > capacity;
  if (parts && room > 0) return Step::kWrite;
  if (stalled_us >= p.gone_us) return Step::kLost;
  if (!parts && waited_us >= p.drop_us) return Step::kDrop;
  return Step::kWait;
}

// True for the records that change the state of a surface or the options.
// The writer never drops a frame with one of them.
inline bool KeepKind(uint32_t kind) {
  return kind == scene_stream::kStart || kind == scene_stream::kSurface ||
         kind == scene_stream::kWritten;
}

// THE FRAMES OF THE READER. The length of the first whole frame at the start
// of bytes (its records to a kFrameEnd), or 0 when the frame is not whole
// yet. bad: a record head that cannot be in the stream (a kind of no record,
// or more bytes than limit). frame: the frame of its first kFrame.
// starts_whole: the frame starts with kStart.
inline size_t UnitBytes(const uint8_t* bytes, size_t n, size_t limit,
                        bool* bad, uint64_t* frame, bool* starts_whole) {
  *bad = false;
  *frame = 0;
  *starts_whole = false;
  size_t at = 0;
  bool first = true;
  bool have_frame = false;
  while (at + 8 <= n) {
    uint32_t head[2];
    memcpy(head, bytes + at, sizeof(head));
    if (head[0] < scene_stream::kStart || head[0] > scene_stream::kLastKind ||
        head[1] > limit) {
      *bad = true;
      return 0;
    }
    if (first) *starts_whole = head[0] == scene_stream::kStart;
    first = false;
    const size_t end = (at + 8 + head[1] + 3) & ~static_cast<size_t>(3);
    if (at + 8 + head[1] > n) return 0;
    if (head[0] == scene_stream::kFrame && !have_frame && head[1] >= 8) {
      memcpy(frame, bytes + at + 8, 8);
      have_frame = true;
    }
    if (head[0] == scene_stream::kFrameEnd) return end <= n ? end : n;
    at = end;
  }
  return 0;
}

// THE FRAMES OF A BATCH (the app). The app takes all whole frames that wait
// in the ring at one time (a batch).
struct UnitInfo {
  uint64_t frame = 0;      // the frame of its first kFrame
  bool keep = false;       // a kStart, a kSurface or a kWritten record
  bool capture = false;    // a kFrame with a picture of a test (F12)
  bool start = false;      // a kStart record
  int32_t test_frame = 0;  // vk_scene_test_frame of its kStart
};

inline UnitInfo Inspect(const uint8_t* unit, size_t n) {
  UnitInfo u;
  bool have_frame = false;
  size_t at = 0;
  while (at + 8 <= n) {
    uint32_t head[2];
    memcpy(head, unit + at, sizeof(head));
    if (at + 8 + head[1] > n) break;
    const uint8_t* body = unit + at + 8;
    if (KeepKind(head[0])) u.keep = true;
    if (head[0] == scene_stream::kStart && head[1] >= 8) {
      u.start = true;
      memcpy(&u.test_frame, body + 4, 4);  // StartBody: mode, test_frame
    }
    if (head[0] == scene_stream::kFrame && head[1] >= sizeof(scene_stream::FrameBody)) {
      scene_stream::FrameBody f;
      memcpy(&f, body, sizeof(f));
      if (!have_frame) u.frame = f.frame;
      have_frame = true;
      if (f.capture != 0) u.capture = true;
    }
    at = (at + 8 + head[1] + 3) & ~static_cast<size_t>(3);
  }
  return u;
}

// True when the app plays a frame of a batch: each frame that changes a
// surface or the options, each frame with a picture of a test, and the
// newest frame. The renderer draws the other frames for no one: the newest
// frame replaces their picture, and they change no surface.
inline bool PlayInBatch(const UnitInfo& u, bool newest, int32_t test_frame) {
  return u.keep || newest || u.capture ||
         (test_frame > 0 && u.frame == static_cast<uint64_t>(test_frame));
}

}  // namespace stream_ring
