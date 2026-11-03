// THE TIMELINE RECORDER (trace, docs/backend.md, "The timeline of the game
// and the app", docs/android.md, "The timeline"). The pure part, with no
// Windows code and no Android code: the mod (32 bit, in the game), the
// headset app and the math test use it
// (test/controller_math/trace_checks.h).
//
// THE EVENTS. A bar (Trace Event Format "X": a start and a length), a point
// ("i": one moment) and a counter ("C": values of a moment). An event has a
// pointer to a static Def (the name, the category and the names of up to four
// numbers), the start in ticks of the clock of the process, the length in
// ticks, and up to four numbers. The Def lives as long as the process: the
// recorder never copies a name.
//
// THE RING. Each thread has its own fixed ring of events, with the one
// writer of the thread. A push is a store of the event and one interlocked
// store of the head, so it costs less than 1 us and makes no call of the
// system. The ring is made at the first push of the thread, or with Name
// before it (this is the only allocation). The saver copies the newest events
// of a ring from another thread. It reads the head with an acquire, and it
// keeps a margin of kMargin events below the oldest event of a full ring,
// where the writer may write at the same time.
//
// THE CLOCK. The recorder keeps the raw ticks of one clock for each process.
// The save turns them into ns of the clock "boot time" of the system with
// TicksToNs. In the headset the app reads CLOCK_BOOTTIME (ns) and the game
// reads QueryPerformanceCounter, which Wine takes from CLOCK_BOOTTIME in
// 100 ns ticks. So the two processes share one clock. The save makes each
// time in us from the start of the window (base_ns), the same for both
// processes.
//
// THE FILE. The Trace Event Format of Chrome, as JSON: ui.perfetto.dev and
// chrome://tracing read it. A part has the events of one process, separated
// by ",\n", with no head and no end, so the app can put the part of the game
// after its own part in one file (FormatHeader, the parts, FormatFooter).
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace trace_events {

constexpr int kArgs = 4;
// The events that the saver does not read below the oldest event of a full
// ring: the writer may write over them at the same time.
constexpr uint32_t kMargin = 64;
// The most rings of one process.
constexpr int kMaxRings = 32;
constexpr int kNameChars = 32;

// The static description of an event. args[i] is the name of the number i, or
// nullptr when the event has no such number.
struct Def {
  const char* name;
  const char* cat;
  const char* args[kArgs];
};

enum Kind : uint16_t { kBar = 1, kPoint = 2, kCounter = 3 };

struct Event {
  const Def* def;
  int64_t ts;     // the start, in ticks
  uint32_t dur;   // the length, in ticks (bars)
  uint16_t kind;  // Kind
  uint16_t pad;
  float v[kArgs];
};

// ---------------------------------------------------------------------------
// The interlocked store and load of an index.
inline void StoreRelease(volatile uint32_t* p, uint32_t v) {
#ifdef _MSC_VER
  _InterlockedExchange(reinterpret_cast<volatile long*>(p), static_cast<long>(v));
#else
  __atomic_store_n(p, v, __ATOMIC_RELEASE);
#endif
}
inline uint32_t LoadAcquire(volatile uint32_t* p) {
#ifdef _MSC_VER
  return static_cast<uint32_t>(
      _InterlockedCompareExchange(reinterpret_cast<volatile long*>(p), 0, 0));
#else
  return __atomic_load_n(p, __ATOMIC_ACQUIRE);
#endif
}
inline bool CompareSwap(volatile uint32_t* p, uint32_t from, uint32_t to) {
#ifdef _MSC_VER
  return _InterlockedCompareExchange(reinterpret_cast<volatile long*>(p),
                                     static_cast<long>(to), static_cast<long>(from)) ==
         static_cast<long>(from);
#else
  return __atomic_compare_exchange_n(p, &from, to, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
}

// ---------------------------------------------------------------------------
// TIME. Ticks to ns with no loss of the high digits: the clock counts from the
// start of the system, so ticks * 1e9 does not fit in 64 bits for a long run.
inline int64_t TicksToNs(int64_t ticks, int64_t freq) {
  if (freq <= 0) return 0;
  const int64_t s = ticks / freq;
  const int64_t r = ticks % freq;
  return s * 1000000000LL + r * 1000000000LL / freq;
}
inline int64_t NsToTicks(int64_t ns, int64_t freq) {
  if (freq <= 0) return 0;
  const int64_t s = ns / 1000000000LL;
  const int64_t r = ns % 1000000000LL;
  return s * freq + r * freq / 1000000000LL;
}
// The length of a bar in ticks, kept in 32 bits. A bar that is longer is
// clipped (4.29 s with ns ticks, 429 s with 100 ns ticks).
inline uint32_t ClipDur(int64_t ticks) {
  if (ticks <= 0) return 0;
  return ticks > 0xFFFFFFFFLL ? 0xFFFFFFFFu : static_cast<uint32_t>(ticks);
}

// ---------------------------------------------------------------------------
// THE RING of one thread.
struct Ring {
  Event* events = nullptr;
  uint32_t capacity = 0;  // a power of two
  uint32_t mask = 0;
  volatile uint32_t head = 0;  // the events pushed since the start (mod 2^32)
  uint32_t tid = 0;
  int sort = 0;
  char name[kNameChars] = {};
  uint32_t pushed_total = 0;  // the writer only, for a check

  static uint32_t RoundUp(uint32_t n) {
    uint32_t c = 1024;
    while (c < n && c < (1u << 24)) c <<= 1;
    return c;
  }
  bool Make(uint32_t want) {
    capacity = RoundUp(want);
    mask = capacity - 1;
    events = static_cast<Event*>(calloc(capacity, sizeof(Event)));
    if (events == nullptr) {
      capacity = mask = 0;
      return false;
    }
    return true;
  }
  void Free() {
    free(events);
    events = nullptr;
    capacity = mask = 0;
    head = 0;
  }
  // The writer of the thread only.
  void Push(const Event& e) {
    const uint32_t i = head;
    events[i & mask] = e;
    StoreRelease(&head, i + 1);
  }
};

// The events of a ring that the saver may read: from the oldest one (past the
// margin of a full ring) to the newest. `first` is the index since the start.
struct Span {
  uint32_t first = 0;
  uint32_t count = 0;
  bool overrun = false;  // the ring is full: older events were lost
};
inline Span Readable(const Ring& r, uint32_t head) {
  Span s;
  if (r.events == nullptr) return s;
  const uint32_t room = r.capacity > kMargin ? r.capacity - kMargin : 0;
  if (head > room) {
    s.count = room;
    s.first = head - room;
    s.overrun = true;
  } else {
    s.count = head;
    s.first = 0;
  }
  return s;
}

// THE LAST N SECONDS of a ring: the events whose end is at or after `from`
// ticks and whose start is before `to`, in the order of the pushes. A bar is
// pushed at its end, so an outer bar comes after the bars inside it, and the
// starts are not sorted. The walk goes back from the newest event and stops
// after kQuiet events in a row that ended before `from`. Gives the count
// written to `out` (at most max) and `overrun` when the oldest event that
// the ring keeps is inside the window.
constexpr uint32_t kQuiet = 64;
inline uint32_t CopyWindow(const Ring& r, int64_t from, int64_t to, Event* out, uint32_t max,
                           bool* overrun) {
  if (overrun != nullptr) *overrun = false;
  if (r.events == nullptr || max == 0) return 0;
  const uint32_t head = LoadAcquire(const_cast<volatile uint32_t*>(&r.head));
  const Span s = Readable(r, head);
  uint32_t quiet = 0;
  uint32_t n = 0;
  uint32_t walked = 0;
  // Collect from the newest back, then reverse.
  for (uint32_t k = 0; k < s.count; ++k) {
    const Event& e = r.events[(head - 1 - k) & r.mask];
    ++walked;
    const int64_t end = e.ts + static_cast<int64_t>(e.dur);
    if (end < from) {
      if (++quiet >= kQuiet) break;
      continue;
    }
    quiet = 0;
    if (e.ts >= to) continue;
    if (n < max) out[n++] = e;
  }
  if (overrun != nullptr) *overrun = s.overrun && walked >= s.count && quiet < kQuiet;
  for (uint32_t i = 0; i < n / 2; ++i) {
    const Event t = out[i];
    out[i] = out[n - 1 - i];
    out[n - 1 - i] = t;
  }
  return n;
}

// ---------------------------------------------------------------------------
// THE RECORDER of a process: the rings of its threads.
class Recorder {
 public:
  // freq: the ticks a second of the clock of the events. min_ticks: a bar
  // shorter than this is not recorded. default_events: the ring of a thread
  // that was not named.
  void Init(int64_t freq, int64_t min_ticks, uint32_t default_events) {
    freq_ = freq;
    min_ticks_ = min_ticks;
    default_events_ = default_events;
    on_ = freq > 0;
  }
  bool on() const { return on_; }
  int64_t freq() const { return freq_; }
  int64_t min_ticks() const { return min_ticks_; }
  uint32_t rejected() const { return rejected_; }

  // The ring of the thread `tid`, made at the first call. nullptr when the
  // table is full or there is no memory.
  Ring* Mine(uint32_t tid) {
    uint32_t slot = Hash(tid);
    for (int probe = 0; probe < kTable; ++probe, slot = (slot + 1) & (kTable - 1)) {
      const uint32_t t = LoadAcquire(&table_[slot].tid);
      if (t == tid) {
        // Only the thread `tid` makes this entry, so the ring is there when
        // the tid is, for that thread.
        return table_[slot].ring;
      }
      if (t == 0) return Claim(slot, tid, nullptr, 0, 0);
    }
    ++rejected_;
    return nullptr;
  }
  // Names the ring of the calling thread, and sets its size. Before the first
  // event of the thread; later it only renames.
  Ring* Name(uint32_t tid, const char* name, int sort, uint32_t events) {
    Ring* r = FindOrNull(tid);
    if (r == nullptr) return Claim(Hash(tid), tid, name, sort, events);
    // The first name of a thread stays: a function that more than one thread
    // runs (the mixer) must not rename the ring of the frame thread.
    if (r->name[0] == 0) {
      CopyName(r, name);
      r->sort = sort;
    }
    return r;
  }

  void Bar(uint32_t tid, const Def* def, int64_t begin, int64_t end, float a = 0, float b = 0,
           float c = 0, float d = 0) {
    const int64_t len = end - begin;
    if (len < min_ticks_) return;
    Push(tid, kBar, def, begin, ClipDur(len), a, b, c, d);
  }
  void Point(uint32_t tid, const Def* def, int64_t at, float a = 0, float b = 0, float c = 0,
             float d = 0) {
    Push(tid, kPoint, def, at, 0, a, b, c, d);
  }
  void Counter(uint32_t tid, const Def* def, int64_t at, float a = 0, float b = 0, float c = 0,
               float d = 0) {
    Push(tid, kCounter, def, at, 0, a, b, c, d);
  }

  // The rings, for the saver. Gives the count, and the ring i.
  int Count() const { return count_; }
  const Ring* At(int i) const { return list_[i]; }

  // For the tests and for a stop: frees all rings.
  void Reset() {
    for (int i = 0; i < count_; ++i) {
      if (list_[i] != nullptr) {
        list_[i]->Free();
        delete list_[i];
        list_[i] = nullptr;
      }
    }
    for (int i = 0; i < kTable; ++i) {
      table_[i].tid = 0;
      table_[i].ring = nullptr;
    }
    count_ = 0;
    on_ = false;
  }

 private:
  static constexpr int kTable = 64;  // a power of two, twice kMaxRings
  struct Entry {
    volatile uint32_t tid;
    Ring* ring;
  };
  static uint32_t Hash(uint32_t tid) { return (tid * 2654435761u) >> 26; }
  static void CopyName(Ring* r, const char* name) {
    if (name == nullptr) return;
    size_t i = 0;
    for (; name[i] != 0 && i + 1 < sizeof(r->name); ++i) r->name[i] = name[i];
    r->name[i] = 0;
  }
  Ring* FindOrNull(uint32_t tid) {
    uint32_t slot = Hash(tid);
    for (int probe = 0; probe < kTable; ++probe, slot = (slot + 1) & (kTable - 1)) {
      const uint32_t t = LoadAcquire(&table_[slot].tid);
      if (t == tid) return table_[slot].ring;
      if (t == 0) return nullptr;
    }
    return nullptr;
  }
  Ring* Claim(uint32_t slot, uint32_t tid, const char* name, int sort, uint32_t events) {
    if (count_ >= kMaxRings) {
      ++rejected_;
      return nullptr;
    }
    Ring* r = new Ring();
    if (!r->Make(events != 0 ? events : default_events_)) {
      delete r;
      ++rejected_;
      return nullptr;
    }
    r->tid = tid;
    r->sort = sort;
    CopyName(r, name);
    // A slot is claimed by the thread itself: the compare-and-swap keeps two
    // threads with the same hash apart.
    for (int probe = 0; probe < kTable; ++probe, slot = (slot + 1) & (kTable - 1)) {
      if (CompareSwap(&table_[slot].tid, 0, tid)) {
        table_[slot].ring = r;
        const int index = static_cast<int>(NextIndex(&count_));
        list_[index] = r;
        return r;
      }
      if (table_[slot].tid == tid) {  // the same thread twice: cannot occur
        r->Free();
        delete r;
        return table_[slot].ring;
      }
    }
    r->Free();
    delete r;
    ++rejected_;
    return nullptr;
  }
  static uint32_t NextIndex(volatile int* p) {
    // Not needed for correctness of the writers (each thread claims once), but
    // two threads can claim at the same moment, so the count is interlocked.
#ifdef _MSC_VER
    return static_cast<uint32_t>(_InterlockedIncrement(reinterpret_cast<volatile long*>(p)) - 1);
#else
    return static_cast<uint32_t>(__atomic_fetch_add(p, 1, __ATOMIC_ACQ_REL));
#endif
  }
  void Push(uint32_t tid, Kind kind, const Def* def, int64_t ts, uint32_t dur, float a, float b,
            float c, float d) {
    Ring* r = Mine(tid);
    if (r == nullptr) return;
    Event e;
    e.def = def;
    e.ts = ts;
    e.dur = dur;
    e.kind = kind;
    e.pad = 0;
    e.v[0] = a;
    e.v[1] = b;
    e.v[2] = c;
    e.v[3] = d;
    r->Push(e);
  }

  bool on_ = false;
  int64_t freq_ = 0;
  int64_t min_ticks_ = 0;
  uint32_t default_events_ = 16384;
  Entry table_[kTable] = {};
  Ring* list_[kMaxRings] = {};
  volatile int count_ = 0;
  uint32_t rejected_ = 0;
};

// ---------------------------------------------------------------------------
// THE JSON. Each Format call writes at `out` (room `cap`) and gives the bytes
// written, or 0 when the text does not fit (nothing is written then, but the
// bytes before the end may hold part of it).
inline size_t EscapeInto(char* out, size_t cap, const char* text) {
  size_t n = 0;
  for (const char* p = text != nullptr ? text : ""; *p != 0; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c == '"' || c == '\\') {
      if (n + 2 >= cap) return 0;
      out[n++] = '\\';
      out[n++] = static_cast<char>(c);
    } else if (c < 0x20 || c >= 0x7F) {
      // Control and non-ASCII characters become a space: names are ASCII.
      if (n + 1 >= cap) return 0;
      out[n++] = ' ';
    } else {
      if (n + 1 >= cap) return 0;
      out[n++] = static_cast<char>(c);
    }
  }
  if (n >= cap) return 0;
  out[n] = 0;
  return n;
}

// A float that JSON can hold: no NaN and no infinity.
inline double Finite(float v) {
  if (v != v) return 0.0;
  if (v > 3.0e38f) return 3.0e38;
  if (v < -3.0e38f) return -3.0e38;
  return static_cast<double>(v);
}

// The start of an event in us from the start of the window.
inline double UsFrom(int64_t ticks, int64_t freq, int64_t base_ns) {
  return static_cast<double>(TicksToNs(ticks, freq) - base_ns) / 1000.0;
}

inline size_t FormatEvent(char* out, size_t cap, const Event& e, int pid, uint32_t tid,
                          int64_t freq, int64_t base_ns, bool comma) {
  if (e.def == nullptr || e.def->name == nullptr || cap < 64) return 0;
  char name[96];
  char cat[48];
  if (EscapeInto(name, sizeof(name), e.def->name) == 0 && e.def->name[0] != 0) return 0;
  if (EscapeInto(cat, sizeof(cat), e.def->cat != nullptr ? e.def->cat : "bw") == 0) return 0;
  const double ts = UsFrom(e.ts, freq, base_ns);
  char args[256];
  size_t an = 0;
  args[0] = 0;
  for (int i = 0; i < kArgs; ++i) {
    if (e.def->args[i] == nullptr) continue;
    char key[40];
    if (EscapeInto(key, sizeof(key), e.def->args[i]) == 0) continue;
    const int w = snprintf(args + an, sizeof(args) - an, "%s\"%s\":%.9g", an > 0 ? "," : "", key,
                           Finite(e.v[i]));
    if (w < 0 || static_cast<size_t>(w) >= sizeof(args) - an) break;
    an += static_cast<size_t>(w);
  }
  int w;
  const char* sep = comma ? ",\n" : "";
  if (e.kind == kBar) {
    const double dur = static_cast<double>(TicksToNs(static_cast<int64_t>(e.dur), freq)) / 1000.0;
    w = snprintf(out, cap,
                 "%s{\"name\":\"%s\",\"cat\":\"%s\",\"ph\":\"X\",\"ts\":%.3f,\"dur\":%.3f,"
                 "\"pid\":%d,\"tid\":%u,\"args\":{%s}}",
                 sep, name, cat, ts, dur, pid, static_cast<unsigned>(tid), args);
  } else if (e.kind == kPoint) {
    w = snprintf(out, cap,
                 "%s{\"name\":\"%s\",\"cat\":\"%s\",\"ph\":\"i\",\"s\":\"t\",\"ts\":%.3f,"
                 "\"pid\":%d,\"tid\":%u,\"args\":{%s}}",
                 sep, name, cat, ts, pid, static_cast<unsigned>(tid), args);
  } else if (e.kind == kCounter) {
    w = snprintf(out, cap,
                 "%s{\"name\":\"%s\",\"cat\":\"%s\",\"ph\":\"C\",\"ts\":%.3f,\"pid\":%d,"
                 "\"tid\":%u,\"args\":{%s}}",
                 sep, name, cat, ts, pid, static_cast<unsigned>(tid), args);
  } else {
    return 0;
  }
  if (w < 0 || static_cast<size_t>(w) >= cap) return 0;
  return static_cast<size_t>(w);
}

inline size_t FormatProcessName(char* out, size_t cap, int pid, const char* name, bool comma) {
  char esc[64];
  if (EscapeInto(esc, sizeof(esc), name) == 0 && name[0] != 0) return 0;
  const int w = snprintf(out, cap,
                         "%s{\"name\":\"process_name\",\"ph\":\"M\",\"pid\":%d,\"tid\":0,"
                         "\"args\":{\"name\":\"%s\"}}",
                         comma ? ",\n" : "", pid, esc);
  return w < 0 || static_cast<size_t>(w) >= cap ? 0 : static_cast<size_t>(w);
}

inline size_t FormatThreadName(char* out, size_t cap, int pid, uint32_t tid, const char* name,
                               int sort, bool comma) {
  char esc[64];
  if (EscapeInto(esc, sizeof(esc), name) == 0 && name[0] != 0) return 0;
  const int w = snprintf(out, cap,
                         "%s{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":%d,\"tid\":%u,"
                         "\"args\":{\"name\":\"%s\"}},\n"
                         "{\"name\":\"thread_sort_index\",\"ph\":\"M\",\"pid\":%d,\"tid\":%u,"
                         "\"args\":{\"sort_index\":%d}}",
                         comma ? ",\n" : "", pid, static_cast<unsigned>(tid), esc, pid,
                         static_cast<unsigned>(tid), sort);
  return w < 0 || static_cast<size_t>(w) >= cap ? 0 : static_cast<size_t>(w);
}

// The head of a whole file. base_ns: the boot time of the start of the window
// (time 0 of the file). note: a text of the writer (escaped here).
inline size_t FormatHeader(char* out, size_t cap, int64_t base_ns, double seconds,
                           const char* note) {
  char esc[256];
  if (EscapeInto(esc, sizeof(esc), note != nullptr ? note : "") == 0 && note != nullptr &&
      note[0] != 0) {
    esc[0] = 0;
  }
  const int w = snprintf(out, cap,
                         "{\"displayTimeUnit\":\"ms\",\"otherData\":{\"note\":\"%s\","
                         "\"base_boot_ns\":%lld,\"seconds\":%.3f},\"traceEvents\":[\n",
                         esc, static_cast<long long>(base_ns), seconds);
  return w < 0 || static_cast<size_t>(w) >= cap ? 0 : static_cast<size_t>(w);
}
inline size_t FormatFooter(char* out, size_t cap) {
  const int w = snprintf(out, cap, "\n]}\n");
  return w < 0 || static_cast<size_t>(w) >= cap ? 0 : static_cast<size_t>(w);
}

// ---------------------------------------------------------------------------
// THE SAVE REQUEST between the game and the app. The end of the window is a
// time of the shared clock, in ns, in two 32 bit words of the input channel.
inline void SplitNs(int64_t ns, uint32_t* lo, uint32_t* hi) {
  *lo = static_cast<uint32_t>(static_cast<uint64_t>(ns) & 0xFFFFFFFFu);
  *hi = static_cast<uint32_t>(static_cast<uint64_t>(ns) >> 32);
}
inline int64_t JoinNs(uint32_t lo, uint32_t hi) {
  return static_cast<int64_t>((static_cast<uint64_t>(hi) << 32) | lo);
}

// THE AUTOMATIC SAVE (trace_auto_ms): a frame longer than limit_ms saves,
// when the last save was at least min_gap_ns before. limit_ms 0 is off.
constexpr int64_t kAutoGapNs = 60LL * 1000000000LL;
inline bool AutoShouldSave(double frame_ms, int limit_ms, int64_t now_ns, int64_t last_save_ns) {
  if (limit_ms <= 0 || frame_ms <= static_cast<double>(limit_ms)) return false;
  return last_save_ns == 0 || now_ns - last_save_ns >= kAutoGapNs;
}

// THE BUTTON COMBINATION. The combination fires once when all its buttons
// are held for hold_ms, and fires again only after one of them was let go.
struct ComboLatch {
  int64_t since_ns = 0;  // when all were held, or 0
  bool fired = false;
  bool Step(bool all_held, int64_t now_ns, int64_t hold_ns) {
    if (!all_held) {
      since_ns = 0;
      fired = false;
      return false;
    }
    if (since_ns == 0) since_ns = now_ns;
    if (!fired && now_ns - since_ns >= hold_ns) {
      fired = true;
      return true;
    }
    return false;
  }
};

}  // namespace trace_events
