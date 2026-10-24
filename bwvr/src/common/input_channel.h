// THE INPUT CHANNEL FROM THE HEADSET APP TO THE MOD (docs/android.md, "The
// input channel"). No Windows code, so the mod, the headset app, the test
// writer of the PC and the math test use it
// (test/controller_math/input_channel_checks.h).
//
// In the headset the game runs in Wine, and Wine has no OpenXR. The app has
// the OpenXR session. So each frame of its session the app writes what the
// mod reads from a session of its own (src/vr/xr_screen.h): the views of the
// eyes, the hands, the display time, the focus, the recenter and the
// profiles. The mod gives the app the pulses of the hands back.
//
// The channel is a real file that two processes map: the mod
// (CreateFileMapping on the PC and in Wine) and the app (mmap with
// MAP_SHARED). A named section of Wine is not visible to the app, so it must
// be a file, as the ring of the live stream (src/mod/stream_ring.h).
//
// THE FILE (kFileBytes): the head, the slot of the newest input, and the
// ring of the events back to the app. Each part has a fixed offset, the same
// on 32 bit x86 and on 64 bit ARM.
//
// THE SLOT. One writer (the app) and one reader (the mod). The writer makes
// the sequence odd before it writes the data and even after it. The reader
// loads the sequence, copies the data, and loads the sequence again. When
// the two are not the same, or the first is odd, the copy is torn, and the
// reader reads again. The writer owns the data, so it never waits.
//
// THE EVENTS. A ring of kEvents events with two indices that count the
// events since the start (mod 2^32): event_write of the mod and event_read
// of the app. Each side stores its index with a release after its copy, and
// loads the index of the other side with an acquire before its copy. A full
// ring drops the new event and counts it.
//
// THE KEYS. A second ring of the same kind goes the other way: the keys of
// the keyboard of the app (docs/android.md, "The keyboard of the app") with
// key_write of the app and key_read of the mod. It is in the tail of the
// file and in fields of the head that were reserved, so the version stays
// 1: a mod that does not read the keys and an app that writes none use the
// file as before.
//
// THE BARRIERS. x86 has TSO, but in the headset the mod runs in Box64 on
// ARM, and ARM has a weak memory model. The interlocked calls of MSVC are
// lock cmpxchg and xchg, which are full barriers that Box64 translates with
// barriers. The app uses the __atomic builtins.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace input_channel {

constexpr uint32_t kMagic = 0x43495742u;  // "BWIC"
constexpr uint32_t kVersion = 1;
constexpr uint32_t kFileBytes = 4096;
constexpr uint32_t kHeadBytes = 256;
constexpr uint32_t kSlotOffset = 256;
constexpr uint32_t kEventsOffset = 1024;
constexpr uint32_t kEvents = 64;  // a power of two
constexpr uint32_t kKeysOffset = 2048;
constexpr uint32_t kKeys = 32;  // a power of two
constexpr uint32_t kProfileChars = 64;

// The bits of Hand::flags.
constexpr uint32_t kActive = 1u << 0;
constexpr uint32_t kAimValid = 1u << 1;
constexpr uint32_t kGripValid = 1u << 2;
constexpr uint32_t kAimTracked = 1u << 3;
constexpr uint32_t kGripTracked = 1u << 4;
constexpr uint32_t kStickClick = 1u << 5;
constexpr uint32_t kPrimary = 1u << 6;
constexpr uint32_t kSecondary = 1u << 7;
constexpr uint32_t kMenu = 1u << 8;

// The bits of SlotData::flags.
constexpr uint32_t kFocused = 1u << 0;    // the session has the input focus
constexpr uint32_t kGazeValid = 1u << 1;  // gaze has a direction of this frame
constexpr uint32_t kRunning = 1u << 2;    // the session runs

// One eye at the display time, in meters in the LOCAL space of the session.
// fov: left, right, up, down in radians, signed, as XrFovf.
struct Eye {
  float position[3];
  float orientation[4];  // x y z w
  float fov[4];
  uint32_t valid;
};
static_assert(sizeof(Eye) == 48, "Eye");

// One hand at the display time. Poses in the LOCAL space, in meters.
struct Hand {
  uint32_t flags;
  float aim_position[3];
  float aim_orientation[4];
  float grip_position[3];
  float grip_orientation[4];
  float trigger;  // 0 to 1
  float squeeze;  // 0 to 1
  float stick[2];  // -1 to 1, x right, y forward
  uint32_t serial;  // one more on each sync of the actions
  char profile[kProfileChars];  // after /interaction_profiles/, or empty
};
static_assert(sizeof(Hand) == 144, "Hand");
static_assert(offsetof(Hand, profile) == 80, "Hand profile");

// The input of one frame of the session.
struct SlotData {
  uint32_t frame;  // the writer: one more for each slot that it writes
  uint32_t flags;  // kFocused, kGazeValid, kRunning
  int64_t display_time;    // the predicted display time, ns, the clock of the runtime
  int64_t display_period;  // ns
  uint32_t recenters;      // the count of the recenters of the LOCAL space
  uint32_t recommended_width;   // the size of an eye that the headset asks for
  uint32_t recommended_height;
  float gaze[3];  // the direction of the gaze in the space of the head
  Eye eyes[2];
  Hand hands[2];
  uint32_t reserved[4];
};
static_assert(offsetof(SlotData, display_time) == 8, "SlotData time");
static_assert(offsetof(SlotData, eyes) == 48, "SlotData eyes");
static_assert(offsetof(SlotData, hands) == 144, "SlotData hands");
static_assert(sizeof(SlotData) == 448, "SlotData");

struct Slot {
  uint32_t seq;  // odd while the writer writes
  uint32_t pad;
  SlotData data;
};
static_assert(sizeof(Slot) == 456, "Slot");

// One event back to the app: a pulse of a hand.
struct Event {
  uint32_t frame;     // the frame of the controllers of the mod
  uint32_t hand;      // 0 left, 1 right
  float amplitude;    // 0 to 1
  float seconds;
};
static_assert(sizeof(Event) == 16, "Event");

// The codes of a key that are not a character.
constexpr uint32_t kKeyBack = 8;    // back space
constexpr uint32_t kKeyEnter = 13;  // Enter

// One key of the keyboard of the app to the mod: the character (a letter, a
// digit, a space) or kKeyBack or kKeyEnter.
struct KeyEvent {
  uint32_t code;
  uint32_t slot;  // SlotData::frame of the newest slot when the app took it
};
static_assert(sizeof(KeyEvent) == 8, "KeyEvent");

struct Head {
  uint32_t magic;          // kMagic when the head is whole
  uint32_t version;        // kVersion
  uint32_t file_bytes;     // kFileBytes
  uint32_t slot_offset;    // kSlotOffset
  uint32_t events_offset;  // kEventsOffset
  uint32_t events;         // kEvents
  uint32_t app_session;    // the app: new for each start of its writer
  uint32_t mod_session;    // the mod: new for each start of its reader
  uint32_t want;           // the mod: the frame of the controllers of its last latch
  uint32_t latches;        // the mod: the count of its reads of the slot
  uint32_t torn;           // the mod: the count of the torn reads that it read again
  uint32_t event_write;    // the mod: the events written
  uint32_t event_read;     // the app: the events read
  uint32_t events_dropped; // the mod: the events that a full ring dropped
  uint32_t key_write;      // the app: the keys written
  uint32_t key_read;       // the mod: the keys read
  uint32_t keys_dropped;   // the app: the keys that a full ring dropped
  uint32_t reserved[47];
};
static_assert(sizeof(Head) == kHeadBytes, "Head");
static_assert(offsetof(Head, want) == 32 && offsetof(Head, event_read) == 48,
              "Head indices");
static_assert(offsetof(Head, key_write) == 56 && offsetof(Head, key_read) == 60,
              "Head keys");

// The whole file.
struct File {
  Head head;
  Slot slot;
  uint8_t pad[kEventsOffset - kSlotOffset - sizeof(Slot)];
  Event events[kEvents];  // the keys follow the events at once
  KeyEvent keys[kKeys];
  uint8_t tail[kFileBytes - kKeysOffset - kKeys * sizeof(KeyEvent)];
};
static_assert(sizeof(File) == kFileBytes, "File");
static_assert(offsetof(File, slot) == kSlotOffset, "File slot");
static_assert(offsetof(File, events) == kEventsOffset, "File events");
static_assert(offsetof(File, keys) == kKeysOffset, "File keys");

// THE INDICES between the processes.
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

// A full barrier: no load and no store passes it.
inline void FullFence() {
#ifdef _MSC_VER
  volatile long fence = 0;
  _InterlockedOr(&fence, 0);
#else
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

// THE HEAD OF A NEW FILE. magic goes in last, so a side sees a whole head or
// none. Both sides make the head when it is not whole, with the same values.
inline void MakeHead(File* f) {
  StoreRelease(&f->head.magic, 0);
  memset(reinterpret_cast<uint8_t*>(f) + 4, 0, sizeof(File) - 4);
  f->head.version = kVersion;
  f->head.file_bytes = kFileBytes;
  f->head.slot_offset = kSlotOffset;
  f->head.events_offset = kEventsOffset;
  f->head.events = kEvents;
  StoreRelease(&f->head.magic, kMagic);
}

// True when the head is a whole head of this version.
inline bool HeadOk(const File* f, uint64_t file_bytes) {
  return LoadAcquire(&f->head.magic) == kMagic && f->head.version == kVersion &&
         f->head.file_bytes == kFileBytes && f->head.slot_offset == kSlotOffset &&
         f->head.events_offset == kEventsOffset && f->head.events == kEvents &&
         file_bytes >= kFileBytes;
}

// THE WRITER OF THE SLOT (the app), in two steps, so a test can put a read
// between them.
inline void WriteBegin(Slot* s) {
  StoreRelease(&s->seq, s->seq + 1);  // odd
  FullFence();  // the data goes in after the odd sequence
}
inline void WriteEnd(Slot* s) {
  StoreRelease(&s->seq, s->seq + 1);  // even, after the data
}
inline void Write(Slot* s, const SlotData& data) {
  WriteBegin(s);
  memcpy(&s->data, &data, sizeof(data));
  WriteEnd(s);
}

// THE READER OF THE SLOT (the mod), in two steps. ReadBegin gives the
// sequence before the copy. ReadEnd is true when the copy after ReadBegin is
// whole: the sequence was even and did not change.
inline uint32_t ReadBegin(const Slot* s) { return LoadAcquire(&s->seq); }
inline bool ReadEnd(const Slot* s, uint32_t begin) {
  FullFence();  // the copy is done before the second load
  return (begin & 1u) == 0 && LoadAcquire(&s->seq) == begin;
}
// One try. False: the copy is torn, or the writer writes now.
inline bool TryRead(const Slot* s, SlotData* out) {
  const uint32_t begin = ReadBegin(s);
  if ((begin & 1u) != 0) return false;
  memcpy(out, &s->data, sizeof(*out));
  return ReadEnd(s, begin);
}
// Up to tries tries. Gives the count of the torn tries in *torn. False when
// each try was torn, and then *out is not whole.
inline bool Read(const Slot* s, SlotData* out, int tries, uint32_t* torn) {
  for (int i = 0; i < tries; ++i) {
    if (TryRead(s, out)) return true;
    ++*torn;
  }
  return false;
}

// THE EVENTS.
inline uint32_t EventsWaiting(uint32_t write, uint32_t read) { return write - read; }
// The mod: false when the ring is full, and then the event is dropped.
inline bool PushEvent(File* f, const Event& e) {
  const uint32_t w = f->head.event_write;  // the mod owns it
  const uint32_t r = LoadAcquire(&f->head.event_read);
  if (EventsWaiting(w, r) >= kEvents) {
    StoreRelease(&f->head.events_dropped, f->head.events_dropped + 1);
    return false;
  }
  f->events[w & (kEvents - 1)] = e;
  StoreRelease(&f->head.event_write, w + 1);
  return true;
}
// The app: up to max events in their order. Gives the count. broken: the
// indices are not valid (more events than the ring holds), and the app takes
// the write index as its read index.
inline uint32_t TakeEvents(File* f, Event* out, uint32_t max, bool* broken) {
  const uint32_t r = f->head.event_read;  // the app owns it
  const uint32_t w = LoadAcquire(&f->head.event_write);
  uint32_t n = EventsWaiting(w, r);
  *broken = n > kEvents;
  if (*broken) {
    StoreRelease(&f->head.event_read, w);
    return 0;
  }
  if (n > max) n = max;
  for (uint32_t i = 0; i < n; ++i) out[i] = f->events[(r + i) & (kEvents - 1)];
  if (n > 0) StoreRelease(&f->head.event_read, r + n);
  return n;
}
// The app at its start: the events of an earlier run are not for now.
inline uint32_t DropOldEvents(File* f) {
  const uint32_t w = LoadAcquire(&f->head.event_write);
  const uint32_t r = f->head.event_read;
  StoreRelease(&f->head.event_read, w);
  return EventsWaiting(w, r);
}

// THE KEYS, from the app to the mod.
// The app: false when the ring is full, and then the key is dropped.
inline bool PushKey(File* f, const KeyEvent& k) {
  const uint32_t w = f->head.key_write;  // the app owns it
  const uint32_t r = LoadAcquire(&f->head.key_read);
  if (EventsWaiting(w, r) >= kKeys) {
    StoreRelease(&f->head.keys_dropped, f->head.keys_dropped + 1);
    return false;
  }
  f->keys[w & (kKeys - 1)] = k;
  StoreRelease(&f->head.key_write, w + 1);
  return true;
}
// The mod: up to max keys in their order. Gives the count. broken: the
// indices are not valid (more keys than the ring holds), and the mod takes
// the write index as its read index.
inline uint32_t TakeKeys(File* f, KeyEvent* out, uint32_t max, bool* broken) {
  const uint32_t r = f->head.key_read;  // the mod owns it
  const uint32_t w = LoadAcquire(&f->head.key_write);
  uint32_t n = EventsWaiting(w, r);
  *broken = n > kKeys;
  if (*broken) {
    StoreRelease(&f->head.key_read, w);
    return 0;
  }
  if (n > max) n = max;
  for (uint32_t i = 0; i < n; ++i) out[i] = f->keys[(r + i) & (kKeys - 1)];
  if (n > 0) StoreRelease(&f->head.key_read, r + n);
  return n;
}
// The mod at its start: the keys of an earlier run are not for this game.
inline uint32_t DropOldKeys(File* f) {
  const uint32_t w = LoadAcquire(&f->head.key_write);
  const uint32_t r = f->head.key_read;
  StoreRelease(&f->head.key_read, w);
  return EventsWaiting(w, r);
}

// THE RECORD of the input of a run (a test aid of the mod, xr_input_record).
// A head and then one SlotData for each frame of the controllers, with the
// frame of the controllers in SlotData::frame.
constexpr uint32_t kRecordMagic = 0x52495742u;  // "BWIR"
struct RecordHead {
  uint32_t magic;
  uint32_t version;     // kVersion
  uint32_t slot_bytes;  // sizeof(SlotData)
  uint32_t source;      // 0 the session, 1 the fake controller, 2 the input channel
};
static_assert(sizeof(RecordHead) == 16, "RecordHead");

}  // namespace input_channel
