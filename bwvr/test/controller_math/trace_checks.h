// The timeline recorder, a test (src/common/trace_events.h, trace_join.h,
// trace_combo.h, docs/backend.md, "The timeline of the game and the app"): the
// JSON of each kind of event, the exact frame numbers, a ring that runs over,
// the last N seconds, the clock in ns, the clock shift of the join, the
// automatic save, the button combinations, and the cost of one event.
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <math.h>

#include <stdlib.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "../../src/common/trace_combo.h"
#include "../../src/common/trace_events.h"
#include "../../src/common/trace_join.h"
#include "../../src/mod/screen_quad_math.h"

namespace trace_checks {

namespace te = trace_events;

inline bool Has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

// A check of the shape of a JSON text: the brackets and the quotes are
// balanced and each string ends. It does not read the values.
inline bool Balanced(const std::string& s) {
  int brace = 0, bracket = 0;
  bool in_string = false;
  for (size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (in_string) {
      if (c == '\\') ++i;
      else if (c == '"') in_string = false;
      continue;
    }
    if (c == '"') in_string = true;
    else if (c == '{') ++brace;
    else if (c == '}') --brace;
    else if (c == '[') ++bracket;
    else if (c == ']') --bracket;
    if (brace < 0 || bracket < 0) return false;
  }
  return !in_string && brace == 0 && bracket == 0;
}

// The helper for the tests: ticks of a time in ns.
inline int64_t NsToTicksHelper(int64_t ns, int64_t freq) { return te::NsToTicks(ns, freq); }

inline void FormatChecks() {
  printf("\nThe timeline: the JSON of each kind of event\n");
  static const te::Def bar = {"turn", "game", {"frame", "ms", nullptr, nullptr}};
  static const te::Def point = {"no new picture", "app", {"frame", nullptr, nullptr, nullptr}};
  static const te::Def counter = {"game fps", "game", {"fps", "queue", nullptr, nullptr}};
  static const te::Def quote = {"na\"me\\x", "g\"", {nullptr, nullptr, nullptr, nullptr}};
  const int64_t freq = 10000000;  // 100 ns ticks as in Wine
  const int64_t base_ns = 5000000000LL;
  char buf[1024];

  te::Event e = {};
  e.def = &bar;
  e.kind = te::kBar;
  e.ts = NsToTicksHelper(5002500000LL, freq);
  e.dur = 12500;  // 1.25 ms
  e.v[0] = 41;
  e.v[1] = 13.5f;
  size_t n = te::FormatEvent(buf, sizeof(buf), e, 1, 77, freq, base_ns, false);
  std::string s(buf, n);
  Check(n > 0 && Has(s, "\"ph\":\"X\"") && Has(s, "\"ts\":2500.000") && Has(s, "\"dur\":1250.000"),
        "a bar: ph X, ts and dur in us from the base", static_cast<double>(n), 1);
  Check(Has(s, "\"pid\":1") && Has(s, "\"tid\":77") && Has(s, "\"frame\":41") &&
            Has(s, "\"ms\":13.5"),
        "a bar: pid, tid and its two numbers", 0, 0);
  Check(Balanced(s), "a bar is balanced JSON", 0, 0);

  e.def = &point;
  e.kind = te::kPoint;
  e.v[0] = 7;
  n = te::FormatEvent(buf, sizeof(buf), e, 2, 5, freq, base_ns, true);
  s.assign(buf, n);
  Check(n > 0 && Has(s, "\"ph\":\"i\"") && Has(s, "\"s\":\"t\"") && !Has(s, "\"dur\"") &&
            s.compare(0, 2, ",\n") == 0,
        "a point: ph i, no dur, a comma before it", 0, 0);

  e.def = &counter;
  e.kind = te::kCounter;
  e.v[0] = 71.5f;
  e.v[1] = 3;
  n = te::FormatEvent(buf, sizeof(buf), e, 1, 9, freq, base_ns, false);
  s.assign(buf, n);
  Check(n > 0 && Has(s, "\"ph\":\"C\"") && Has(s, "\"fps\":71.5") && Has(s, "\"queue\":3"),
        "a counter: ph C with its values", 0, 0);

  e.def = &quote;
  e.kind = te::kBar;
  n = te::FormatEvent(buf, sizeof(buf), e, 1, 9, freq, base_ns, false);
  s.assign(buf, n);
  Check(n > 0 && Has(s, "na\\\"me\\\\x") && Balanced(s), "a name with a quote and a backslash is escaped",
        0, 0);

  e.def = &bar;
  e.v[0] = NAN;
  e.v[1] = INFINITY;
  n = te::FormatEvent(buf, sizeof(buf), e, 1, 9, freq, base_ns, false);
  s.assign(buf, n);
  Check(n > 0 && !Has(s, "nan") && !Has(s, "inf") && Balanced(s), "NaN and infinity become finite numbers",
        0, 0);

  char tiny[32];
  Check(te::FormatEvent(tiny, sizeof(tiny), e, 1, 9, freq, base_ns, false) == 0,
        "a buffer that is too small gives 0", 0, 0);

  n = te::FormatProcessName(buf, sizeof(buf), 1, "game", false);
  s.assign(buf, n);
  Check(Has(s, "\"process_name\"") && Has(s, "\"ph\":\"M\"") && Has(s, "\"name\":\"game\""),
        "the name of a process (ph M)", 0, 0);
  n = te::FormatThreadName(buf, sizeof(buf), 1, 77, "frame", 2, false);
  s.assign(buf, n);
  Check(Has(s, "\"thread_name\"") && Has(s, "\"sort_index\":2") && Has(s, "\"tid\":77"),
        "the name and the order of a thread (ph M)", 0, 0);

  // A whole file: head, a part, a comma, a second part, end.
  std::string file;
  n = te::FormatHeader(buf, sizeof(buf), base_ns, 10.0, "a \"test\"");
  file.append(buf, n);
  n = te::FormatProcessName(buf, sizeof(buf), 1, "game", false);
  file.append(buf, n);
  e.v[0] = 1;
  e.v[1] = 2;
  n = te::FormatEvent(buf, sizeof(buf), e, 1, 9, freq, base_ns, true);
  file.append(buf, n);
  n = te::FormatProcessName(buf, sizeof(buf), 2, "app", true);
  file.append(buf, n);
  n = te::FormatFooter(buf, sizeof(buf));
  file.append(buf, n);
  Check(Balanced(file) && Has(file, "\"traceEvents\":[") && Has(file, "\"base_boot_ns\":5000000000"),
        "a whole file with two processes is balanced JSON", 0, 0);
}

inline void ClockChecks() {
  printf("\nThe timeline: the clock\n");
  // A boot time of 3 days in 100 ns ticks: a multiply of ticks by 1e9 would
  // overflow 64 bits.
  const int64_t freq = 10000000;
  const int64_t ticks = 3LL * 86400 * freq + 1234567;
  const int64_t ns = te::TicksToNs(ticks, freq);
  Check(ns == 3LL * 86400 * 1000000000LL + 123456700LL, "3 days of 100 ns ticks to ns, no overflow",
        static_cast<double>(ns), 0);
  Check(te::NsToTicks(ns, freq) == ticks, "ns to ticks and back", 0, 0);
  const int64_t ns_freq = 1000000000;
  Check(te::TicksToNs(987654321012LL, ns_freq) == 987654321012LL, "ns ticks stay ns", 0, 0);
  Check(te::TicksToNs(5, 0) == 0, "no frequency gives 0", 0, 0);
  Check(te::ClipDur(-5) == 0 && te::ClipDur(0x1FFFFFFFFLL) == 0xFFFFFFFFu && te::ClipDur(77) == 77,
        "the length of a bar is clipped to 32 bits", 0, 0);
  uint32_t lo, hi;
  te::SplitNs(ns, &lo, &hi);
  Check(te::JoinNs(lo, hi) == ns, "a time in ns goes through two 32 bit words", 0, 0);
}

inline void RingChecks() {
  printf("\nThe timeline: the ring, the window and the recorder\n");
  static const te::Def d = {"x", "t", {"i", nullptr, nullptr, nullptr}};
  te::Recorder rec;
  rec.Init(1000000, 100, 1024);  // 1 MHz: one tick is 1 us. A bar under 100 ticks is not kept.
  rec.Name(11, "frame", 1, 1024);
  for (int i = 0; i < 10; ++i) rec.Bar(11, &d, i * 1000, i * 1000 + 500, static_cast<float>(i));
  rec.Bar(11, &d, 20000, 20050);  // 50 ticks: too short
  rec.Point(11, &d, 21000, 99);
  Check(rec.Count() == 1, "one ring after the first thread", rec.Count(), 1);
  const te::Ring* r = rec.At(0);
  Check(r->head == 11 && std::string(r->name) == "frame",
        "10 bars and a point (the short bar is not kept), and the ring has its name", r->head, 11);

  std::vector<te::Event> out(1024);
  bool overrun = false;
  uint32_t n = te::CopyWindow(*r, 0, 100000, out.data(), 1024, &overrun);
  Check(n == 11 && !overrun, "the window of all time has the 11 events", n, 11);
  Check(out[0].ts == 0 && out[9].ts == 9000 && out[10].kind == te::kPoint,
        "the events are in the order of the pushes", 0, 0);
  n = te::CopyWindow(*r, 5400, 100000, out.data(), 1024, &overrun);
  Check(n == 6 && out[0].ts == 5000,
        "the last window: the 5 bars that end at or after the start, and the point", n, 6);
  n = te::CopyWindow(*r, 0, 3500, out.data(), 1024, &overrun);
  Check(n == 4, "events that start at or after the end of the window do not count", n, 4);
  n = te::CopyWindow(*r, 0, 100000, out.data(), 3, &overrun);
  Check(n == 3, "at most max events", n, 3);

  // The first name of a ring stays.
  rec.Name(11, "other name", 5, 0);
  Check(std::string(rec.At(0)->name) == "frame" && rec.At(0)->sort == 1,
        "a second call of Name does not rename a ring", 0, 0);
  rec.Name(33, "late name", 4, 1024);
  Check(rec.Count() == 2 && std::string(rec.At(1)->name) == "late name",
        "Name makes the ring of a thread that has none", rec.Count(), 2);
  rec.Bar(33, &d, 10, 900);
  Check(rec.Count() == 2, "a push after Name uses that ring", rec.Count(), 2);
  rec.Reset();
  rec.Init(1000000, 100, 1024);
  rec.Name(11, "frame", 1, 1024);
  for (int i = 0; i < 10; ++i) rec.Bar(11, &d, i * 1000, i * 1000 + 500, static_cast<float>(i));
  rec.Bar(11, &d, 20000, 20050);
  rec.Point(11, &d, 21000, 99);
  // A second thread has its own ring.
  rec.Bar(22, &d, 100, 900);
  Check(rec.Count() == 2 && rec.At(1)->tid == 22, "a second thread has a second ring",
        rec.Count(), 2);
  Check(rec.rejected() == 0, "nothing was rejected", rec.rejected(), 0);

  // A ring that runs over: 1024 events, push 3000. It keeps the newest ones
  // less the margin.
  te::Recorder big;
  big.Init(1000000, 0, 1024);
  big.Name(5, "x", 0, 1024);
  for (int i = 0; i < 3000; ++i) big.Bar(5, &d, i * 10, i * 10 + 5, static_cast<float>(i));
  const te::Ring* br = big.At(0);
  Check(br->head == 3000, "3000 pushes into a ring of 1024", br->head, 3000);
  out.assign(4096, te::Event());
  n = te::CopyWindow(*br, 0, 1000000, out.data(), 4096, &overrun);
  Check(n == 1024 - te::kMargin && overrun, "the full ring gives its events less the margin, overrun",
        n, 1024 - te::kMargin);
  Check(out[n - 1].v[0] == 2999.0f && out[0].v[0] == static_cast<float>(3000 - n),
        "those are the newest events, in order", out[n - 1].v[0], 2999);
  n = te::CopyWindow(*br, 29000, 1000000, out.data(), 4096, &overrun);
  Check(n == 100 && !overrun, "a window of the newest 100 events in a full ring: no overrun", n, 100);

  // The save of the last N seconds: 1 s of events at 1 kHz, take the last 0.25 s.
  te::Recorder sec;
  sec.Init(1000000, 0, 4096);
  for (int i = 0; i < 1000; ++i) sec.Bar(1, &d, i * 1000, i * 1000 + 300);
  n = te::CopyWindow(*sec.At(0), 750000, 1000000, out.data(), 4096, &overrun);
  Check(n >= 250 && n <= 252, "the last quarter second of 1000 events a second", n, 250);

  // Many threads: the table gives each one its ring and refuses past kMaxRings.
  te::Recorder many;
  many.Init(1000000, 0, 1024);
  for (uint32_t t = 1; t <= 40; ++t) many.Point(t * 7919, &d, 1);
  Check(many.Count() == te::kMaxRings && many.rejected() == 40 - te::kMaxRings,
        "more threads than kMaxRings: the rest are rejected and counted", many.Count(),
        te::kMaxRings);
  many.Reset();
  big.Reset();
  sec.Reset();
  rec.Reset();
}

// The name of the file of the part of the game: one file for each save.
inline void PartNameChecks() {
  printf("\nThe timeline: the name of the part of the game\n");
  char a[64], b[64], c[64], d[64];
  const size_t na = te::FormatPartName(a, sizeof(a), true, 5, false);
  const size_t nb = te::FormatPartName(b, sizeof(b), false, 5, false);
  const size_t nc = te::FormatPartName(c, sizeof(c), true, 6, false);
  const size_t nd = te::FormatPartName(d, sizeof(d), true, 5, true);
  Check(na > 0 && std::string(a) == "bwvr-trace-game-a5.json",
        "the part for the request 5 of the app is bwvr-trace-game-a5.json", static_cast<double>(na), 1);
  Check(nb > 0 && std::string(b) == "bwvr-trace-game-g5.json" && std::string(a) != std::string(b),
        "the part for the save 5 of the game has another name than the request 5 of the app", 0, 0);
  Check(nc > 0 && std::string(c) != std::string(a),
        "two saves close together (request 5 and request 6) have two names", 0, 0);
  Check(nd > 0 && std::string(d) == "bwvr-trace-game-a5.tmp",
        "the file that is being written has the suffix tmp", 0, 0);
  char tiny[10];
  Check(te::FormatPartName(tiny, sizeof(tiny), true, 5, false) == 0,
        "a buffer that is too small gives 0", 0, 0);
  // The count of a request counts up one by one, so the names never repeat.
  bool repeat = false;
  std::string last;
  for (uint32_t ask = 1; ask <= 200; ++ask) {
    char name[64];
    te::FormatPartName(name, sizeof(name), true, ask, false);
    if (name == last) repeat = true;
    last = name;
  }
  Check(!repeat, "the names of 200 requests in a row differ", 0, 0);
}

// CopyWindow while the writer keeps writing: the oldest events of a full ring
// are overwritten during the copy and must not be in the result.
inline void MovedHeadChecks() {
  printf("\nThe timeline: a copy of the window while the head moves\n");
  static const te::Def d = {"x", "t", {"i", nullptr, nullptr, nullptr}};
  te::Recorder rec;
  rec.Init(1000000, 0, 1024);
  rec.Name(5, "x", 0, 1024);
  // 2000 events into a ring of 1024. The value of an event is its index.
  for (int i = 0; i < 2000; ++i) rec.Bar(5, &d, i * 10, i * 10 + 5, static_cast<float>(i));
  const te::Ring* r = rec.At(0);
  std::vector<te::Event> out(4096);
  bool overrun = false;

  // A head that does not move: the whole readable span.
  uint32_t n = te::CopyWindow(*r, 0, 100000000, out.data(), 4096, &overrun);
  Check(n == 1024 - te::kMargin && overrun, "a quiet writer: the ring less the margin, overrun", n,
        1024 - te::kMargin);

  // The writer pushes 300 events after the first chunk was read.
  struct Writer {
    te::Recorder* rec;
    int pushed = 0;
    int count = 0;
    void operator()(uint32_t) {
      if (pushed != 0) return;
      static const te::Def def = {"x", "t", {"i", nullptr, nullptr, nullptr}};
      for (int i = 0; i < count; ++i) {
        rec->Bar(5, &def, (2000 + i) * 10, (2000 + i) * 10 + 5, static_cast<float>(2000 + i));
      }
      pushed = count;
    }
  };
  Writer writer;
  writer.rec = &rec;
  writer.count = 300;
  n = te::CopyWindowHooked(*r, 0, 100000000, out.data(), 4096, &overrun, writer);
  const uint32_t head_after = r->head;
  // Every event that the copy keeps must be at most capacity - kMargin events
  // below the head of the end of the copy, so the writer did not reach it.
  const float oldest_safe = static_cast<float>(head_after - (1024 - te::kMargin));
  bool all_safe = n > 0;
  bool in_order = true;
  for (uint32_t k = 0; k < n; ++k) {
    if (out[k].v[0] < oldest_safe) all_safe = false;
    if (k > 0 && out[k].v[0] != out[k - 1].v[0] + 1.0f) in_order = false;
  }
  Check(head_after == 2300, "the writer pushed 300 events during the copy", head_after, 2300);
  Check(all_safe, "no event below the margin of the new head is in the copy", n, 0);
  Check(in_order && out[n - 1].v[0] == 1999.0f,
        "the events are whole and in order up to the newest one that the copy saw", out[n - 1].v[0],
        1999);
  // The copy dropped exactly the 300 events that the writer came over.
  Check(n == 1024 - te::kMargin - 300 && overrun,
        "the copy lost the 300 oldest events and says overrun", n, 1024 - te::kMargin - 300);

  // A writer that comes over the whole ring: nothing safe is left.
  te::Recorder fast;
  fast.Init(1000000, 0, 1024);
  fast.Name(6, "y", 0, 1024);
  for (int i = 0; i < 2000; ++i) fast.Bar(6, &d, i * 10, i * 10 + 5, static_cast<float>(i));
  struct Flood {
    te::Recorder* rec;
    bool done = false;
    void operator()(uint32_t) {
      if (done) return;
      done = true;
      static const te::Def def = {"x", "t", {"i", nullptr, nullptr, nullptr}};
      for (int i = 0; i < 5000; ++i) {
        rec->Bar(6, &def, (2000 + i) * 10, (2000 + i) * 10 + 5, static_cast<float>(2000 + i));
      }
    }
  };
  Flood flood;
  flood.rec = &fast;
  n = te::CopyWindowHooked(*fast.At(0), 0, 100000000, out.data(), 4096, &overrun, flood);
  Check(n == 0 && overrun, "a writer that ran over the whole ring gives an empty copy and overrun",
        n, 0);

  // A ring that is not full: the writer cannot reach what the copy read.
  te::Recorder part;
  part.Init(1000000, 0, 1024);
  part.Name(7, "z", 0, 1024);
  for (int i = 0; i < 500; ++i) part.Bar(7, &d, i * 10, i * 10 + 5, static_cast<float>(i));
  struct Few {
    te::Recorder* rec;
    bool done = false;
    void operator()(uint32_t) {
      if (done) return;
      done = true;
      static const te::Def def = {"x", "t", {"i", nullptr, nullptr, nullptr}};
      for (int i = 0; i < 100; ++i) rec->Bar(7, &def, 9000 + i, 9001 + i, 1.0f);
    }
  };
  Few few;
  few.rec = &part;
  n = te::CopyWindowHooked(*part.At(0), 0, 5000, out.data(), 4096, &overrun, few);
  Check(n == 500 && !overrun, "a ring that is not full keeps all its events", n, 500);
  rec.Reset();
  fast.Reset();
  part.Reset();
}

// The head moves again while the walk reads the chunk the second time (the
// read that cuts at the safe limit). The second read must be checked again.
inline void MovedTwiceChecks() {
  printf("\nThe timeline: the head moves during the second read\n");
  static const te::Def d = {"x", "t", {"i", nullptr, nullptr, nullptr}};
  // A writer with a script: pushes[k] events after the read number k of a chunk.
  struct Script {
    te::Recorder* rec = nullptr;
    uint32_t tid = 0;
    std::vector<int> pushes;
    int call = 0;
    int next_value = 2000;
    void operator()(uint32_t) {
      const int k = call++;
      const int count = k < static_cast<int>(pushes.size()) ? pushes[k] : 0;
      static const te::Def def = {"x", "t", {"i", nullptr, nullptr, nullptr}};
      for (int i = 0; i < count; ++i) {
        rec->Bar(tid, &def, next_value * 10, next_value * 10 + 5, static_cast<float>(next_value));
        ++next_value;
      }
    }
  };
  struct Case {
    const char* what;
    std::vector<int> pushes;
    uint32_t expect_n;  // the events of the result, newest 1999 down
  };
  // 2000 events into a ring of 1024, the readable room is 960. The chunks are
  // 512 events (k 0 to 511, then 512 to 959). After the read of chunk 1 the
  // writer pushes a, after the read of chunk 2 it pushes b, after the second read
  // of chunk 2 it pushes c, and so on.
  //   a=300: chunk 1 safe to k 660. Chunk 2 read to 960 > 660: cut to 660. If the
  //          writer stays quiet after that, n = 660.
  //   a=300, b=0, c=300: the head moved 600, the safe limit is 360, which is
  //          below chunk 2 (k 512): only chunk 1 is kept (n = 512).
  //   a=300, b=0, c=100: the limit is 560: the read to 660 is cut again to 560,
  //          and the next check holds (n = 560).
  //   a=300, b=0, c=100, d=100: the limit moves again to 460: only chunk 1 stays.
  const Case cases[] = {
      {"the writer stays quiet after the cut", {300, 0, 0}, 660},
      {"the writer moves 300 more during the second read", {300, 0, 300}, 512},
      {"the writer moves 100 more during the second read", {300, 0, 100, 0}, 560},
      {"the writer moves 100 more again during the third read", {300, 0, 100, 100}, 512},
  };
  std::vector<te::Event> out(4096);
  for (const Case& c : cases) {
    te::Recorder rec;
    rec.Init(1000000, 0, 1024);
    rec.Name(11, "x", 0, 1024);
    for (int i = 0; i < 2000; ++i) rec.Bar(11, &d, i * 10, i * 10 + 5, static_cast<float>(i));
    Script script;
    script.rec = &rec;
    script.tid = 11;
    script.pushes = c.pushes;
    bool overrun = false;
    const uint32_t n = te::CopyWindowHooked(*rec.At(0), 0, 100000000, out.data(), 4096, &overrun, script);
    // An event is whole when its value is an index that the ring held at the
    // start (below 2000), and the events are in order with no gap.
    bool whole = n > 0;
    for (uint32_t k = 0; k < n; ++k) {
      if (out[k].v[0] >= 2000.0f) whole = false;
      if (k > 0 && out[k].v[0] != out[k - 1].v[0] + 1.0f) whole = false;
    }
    char text[160];
    snprintf(text, sizeof(text), "%s: %u events, all whole, newest 1999", c.what, c.expect_n);
    Check(n == c.expect_n && whole && out[n - 1].v[0] == 1999.0f && overrun, text, n, c.expect_n);
    rec.Reset();
  }
}

// Claim: the ring count never passes kMaxRings, also when threads claim at
// the same moment.
inline void ClaimBoundChecks() {
  printf("\nThe timeline: the bound of the table of rings\n");
  static const te::Def d = {"x", "t", {nullptr, nullptr, nullptr, nullptr}};
  te::Recorder rec;
  rec.Init(1000000, 0, 1024);
  // The count as a race can leave it: more than kMaxRings.
  rec.SetCountForTest(te::kMaxRings + 5);
  Check(rec.Count() == te::kMaxRings, "a count past kMaxRings reads as kMaxRings", rec.Count(),
        te::kMaxRings);
  rec.Reset();
  rec.Init(1000000, 0, 1024);
  // The last free index goes to the first thread. The next thread has no ring.
  rec.SetCountForTest(te::kMaxRings - 1);
  rec.Point(1001, &d, 1);
  rec.Point(1002, &d, 1);
  Check(rec.Count() == te::kMaxRings && rec.At(te::kMaxRings - 1) != nullptr &&
            rec.At(te::kMaxRings - 1)->tid == 1001,
        "the last free index goes to the first thread", rec.Count(), te::kMaxRings);
  Check(rec.rejected() >= 1, "the next thread is rejected", rec.rejected(), 1);
  rec.Reset();

  // 48 threads claim at the same moment, 20 rounds.
  int worst = 0;
  bool all_whole = true;
  for (int round = 0; round < 20; ++round) {
    te::Recorder r;
    r.Init(1000000, 0, 1024);
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for (uint32_t t = 0; t < 48; ++t) {
      threads.emplace_back([&r, &go, t] {
        while (!go.load()) {
        }
        r.Point(5000 + t * 977, &d, static_cast<int64_t>(t));
      });
    }
    go.store(true);
    for (std::thread& th : threads) th.join();
    if (r.Count() > worst) worst = r.Count();
    for (int i = 0; i < r.Count(); ++i) {
      if (r.At(i) == nullptr || r.At(i)->events == nullptr) all_whole = false;
    }
    r.Reset();
  }
  Check(worst <= te::kMaxRings, "48 threads at once: the count stays within kMaxRings", worst,
        te::kMaxRings);
  Check(all_whole, "48 threads at once: each ring below the count is there", 0, 0);
}

inline void TriggerChecks() {
  printf("\nThe timeline: the automatic save and the button combination\n");
  const int64_t s = 1000000000LL;
  Check(!te::AutoShouldSave(39.0, 40, 100 * s, 0), "a frame of 39 ms with the limit 40: no save", 0, 0);
  Check(te::AutoShouldSave(41.0, 40, 100 * s, 0), "a frame of 41 ms: the first save", 0, 0);
  Check(!te::AutoShouldSave(80.0, 40, 130 * s, 100 * s), "a long frame 30 s after a save: no save", 0, 0);
  Check(te::AutoShouldSave(80.0, 40, 160 * s, 100 * s), "a long frame 60 s after a save: save", 0, 0);
  Check(!te::AutoShouldSave(500.0, 0, 160 * s, 0), "trace_auto_ms=0: never", 0, 0);

  te::ComboLatch c;
  const int64_t ms = 1000000LL;
  Check(!c.Step(false, 0, 300 * ms), "no button: no fire", 0, 0);
  Check(!c.Step(true, 1000 * ms, 300 * ms), "all held at once: not yet", 0, 0);
  Check(!c.Step(true, 1200 * ms, 300 * ms), "held 200 ms of 300: not yet", 0, 0);
  Check(c.Step(true, 1300 * ms, 300 * ms), "held 300 ms: fires", 0, 0);
  Check(!c.Step(true, 1400 * ms, 300 * ms) && !c.Step(true, 9000 * ms, 300 * ms),
        "held longer: no second fire", 0, 0);
  Check(!c.Step(false, 9100 * ms, 300 * ms), "one button let go", 0, 0);
  Check(!c.Step(true, 9200 * ms, 300 * ms) && c.Step(true, 9500 * ms, 300 * ms),
        "pressed again: fires once more after 300 ms", 0, 0);
}

// The frames of the stream are about 1e9 + N. A float loses the low bits of
// them, so the file would show the same number for many frames.
inline void FrameNumberChecks() {
  printf("\nThe timeline: exact frame numbers\n");
  Check(te::kScreenFrameBase == screen_quad_math::kScreenFrameBase,
        "the base of the screen frames is the one of the stream", 0, 0);
  const uint64_t base = screen_quad_math::kScreenFrameBase;
  // The old way: a float of 1e9 + N has steps of 64.
  int lost = 0;
  for (uint64_t n = 300; n < 400; ++n) {
    if (static_cast<uint64_t>(static_cast<float>(base + n)) != base + n) ++lost;
  }
  Check(lost > 90, "a float of 1e9 + N loses the number for most N (the fault)", lost, 90);
  int wrong = 0;
  for (uint64_t n = 0; n < 5000; ++n) {
    if (te::FrameFromArg(te::FrameArg(base + n)) != static_cast<int64_t>(base + n)) ++wrong;
    if (te::FrameFromArg(te::FrameArg(n)) != static_cast<int64_t>(n)) ++wrong;
  }
  Check(wrong == 0, "FrameArg and FrameFromArg give back 5000 screen frames and 5000 frames", wrong, 0);
  Check(te::FrameFromArg(te::FrameArg(base + 16777214)) == static_cast<int64_t>(base + 16777214),
        "a screen frame count of 2^24 - 2 is still exact", 0, 0);
  Check(te::FrameArg(base) < 0.0f && te::FrameArg(0) == 0.0f && te::FrameArg(base) != te::FrameArg(0),
        "screen frame 0 and frame 0 differ", 0, 0);
  // Two screen frames that are 1 apart have two numbers in the file.
  static const te::Def put = {te::kStreamPutName, "stream", {"#frame", "bytes", nullptr, nullptr}};
  static const te::Def plain = {"plain", "x", {"frame", nullptr, nullptr, nullptr}};
  te::Event e = {};
  e.def = &put;
  e.kind = te::kBar;
  e.ts = 1000;
  e.dur = 10;
  char buf[512];
  std::string a, b;
  e.v[0] = te::FrameArg(base + 320);
  e.v[1] = 2572;
  a.assign(buf, te::FormatEvent(buf, sizeof(buf), e, 1, 3, 1000000000LL, 0, false));
  e.v[0] = te::FrameArg(base + 321);
  b.assign(buf, te::FormatEvent(buf, sizeof(buf), e, 1, 3, 1000000000LL, 0, false));
  Check(Has(a, "\"frame\":1000000320") && Has(b, "\"frame\":1000000321") && Has(a, "\"bytes\":2572") &&
            !Has(a, "#"),
        "the file has the exact number of the stream under the name with no #", 0, 0);
  e.def = &plain;
  e.v[0] = 313.0f;
  a.assign(buf, te::FormatEvent(buf, sizeof(buf), e, 1, 3, 1000000000LL, 0, false));
  Check(Has(a, "\"frame\":313"), "an arg with no # is written as it is", 0, 0);
}

// The join: the shift of the clock of the game from the frames that both
// processes know.
inline void JoinChecks() {
  printf("\nThe timeline: the shift of the clock from the frames of both parts\n");
  namespace tj = trace_join;
  const int64_t base = static_cast<int64_t>(te::kScreenFrameBase);
  // The game clock is 3956.3 ms behind the app clock. The arrival is 0.4 ms
  // after the end of the write, with a jitter of up to 0.3 ms.
  const double clock_gap = 3956300.0;
  std::vector<tj::Sample> puts, arrivals;
  for (int i = 0; i < 200; ++i) {
    const double end = 1000.0 + i * 13333.3;
    puts.push_back({base + 260 + i, end});
    const double jitter = (i % 7) * 50.0;
    arrivals.push_back({base + 260 + i, end + clock_gap + 400.0 + jitter});
  }
  tj::Shift s = tj::EstimateShift(puts, arrivals);
  Check(s.pairs == 200 && s.apply, "200 common frames give a shift", s.pairs, 200);
  // median of jitter 0 to 300 is 150 (i % 7 = 3), so median = gap + 550.
  Check(fabs(s.shift_us - (clock_gap + 550.0 - tj::kArrivalUs)) < 60.0,
        "the shift is the median difference less 0.5 ms", s.shift_us, clock_gap + 50.0);
  Check(s.spread_us < 400.0 && s.least_us <= s.median_us && s.median_us <= s.most_us,
        "the spread is small and the order least, median, most holds", s.spread_us, 400);
  // The shift puts the median arrival 0.5 ms after the end of the write.
  Check(fabs((s.median_us - s.shift_us) - tj::kArrivalUs) < 1e-6, "the shifted median is 0.5 ms",
        s.median_us - s.shift_us, tj::kArrivalUs);

  // Frames in one list only, and a frame twice: the last write and the first
  // arrival count.
  std::vector<tj::Sample> p2 = {{base + 1, 100.0}, {base + 2, 200.0}, {base + 2, 215.0},
                                {base + 3, 300.0}, {base + 4, 400.0}, {base + 9, 900.0}};
  std::vector<tj::Sample> a2 = {{base + 2, 260.0}, {base + 2, 290.0}, {base + 3, 360.0},
                                {base + 4, 460.0}, {base + 1, 160.0}, {base + 7, 700.0}};
  s = tj::EstimateShift(p2, a2);
  Check(s.pairs == 4 && fabs(s.median_us - 60.0) < 1e-6 && fabs(s.least_us - 45.0) < 1e-6,
        "4 common frames, the last write of frame 2 (215) counts, the first arrival (260)", s.pairs, 4);

  // No common frame: no shift. Two frames: not enough.
  s = tj::EstimateShift({{base + 1, 1.0}}, {{base + 2, 2.0}});
  Check(s.pairs == 0 && !s.apply && s.shift_us == 0.0, "no common frame gives no shift", s.pairs, 0);
  s = tj::EstimateShift({{1, 10.0}, {2, 20.0}}, {{1, 5010.0}, {2, 5020.0}});
  Check(s.pairs == 2 && !s.apply, "2 common frames are too few", s.pairs, 3);
  // A wrong match: the differences spread over seconds.
  std::vector<tj::Sample> p3, a3;
  for (int i = 0; i < 50; ++i) {
    p3.push_back({base + i, i * 10000.0});
    a3.push_back({base + i, i * 10000.0 + ((i * 37) % 50) * 100000.0});
  }
  s = tj::EstimateShift(p3, a3);
  Check(s.pairs == 50 && !s.apply && s.spread_us > tj::kMaxSpreadUs,
        "a spread of seconds is a wrong match and gives no shift", s.spread_us, tj::kMaxSpreadUs);

  // The text of a part: the puts are found with their exact frame, and the
  // shift changes each ts and no other number.
  static const te::Def put = {te::kStreamPutName, "stream", {"#frame", "bytes", nullptr, nullptr}};
  static const te::Def bar = {"turn ts \"x\"", "game", {"frame", nullptr, nullptr, nullptr}};
  std::string part;
  char buf[512];
  size_t n = te::FormatProcessName(buf, sizeof(buf), 1, "game", false);
  part.append(buf, n);
  n = te::FormatThreadName(buf, sizeof(buf), 1, 7, "stream worker", 3, true);
  part.append(buf, n);
  const int64_t freq = 10000000;
  for (int i = 0; i < 5; ++i) {
    te::Event e = {};
    e.def = &put;
    e.kind = te::kBar;
    e.ts = 1000000 + i * 133333;
    e.dur = 3000;  // 300 us
    e.v[0] = te::FrameArg(te::kScreenFrameBase + 4000 + i);
    e.v[1] = 100.0f;
    n = te::FormatEvent(buf, sizeof(buf), e, 1, 7, freq, 0, true);
    part.append(buf, n);
    e.def = &bar;
    e.v[0] = 5.0f;
    n = te::FormatEvent(buf, sizeof(buf), e, 1, 7, freq, 0, true);
    part.append(buf, n);
  }
  std::vector<tj::Sample> found;
  tj::ScanPuts(part.c_str(), part.size(), &found);
  Check(found.size() == 5 && found[0].frame == base + 4000 && found[4].frame == base + 4004,
        "ScanPuts finds the 5 writes with exact frame numbers", found.size(), 5);
  Check(fabs(found[0].us - (100000.0 + 300.0)) < 0.01 && fabs(found[4].us - (100000.0 + 4 * 13333.3 + 300.0)) < 0.5,
        "the end of a write is its ts plus its dur in us", found[0].us, 100300.0);
  std::string moved;
  tj::ShiftPart(part.c_str(), part.size(), -3956300.0 + 123.456, &moved);
  std::vector<tj::Sample> after;
  tj::ScanPuts(moved.c_str(), moved.size(), &after);
  Check(after.size() == 5 && fabs((after[2].us - found[2].us) - (-3956300.0 + 123.456)) < 0.01,
        "ShiftPart moves each time by the shift", after.size(), 5);
  Check(Has(moved, "\"thread_name\"") && Has(moved, "\"sort_index\":3") && Has(moved, "\"bytes\":100") &&
            Has(moved, "turn ts \\\"x\\\"") && Balanced("{\"a\":[" + moved + "]}"),
        "the thread names, the args and a name with ts stay whole", 0, 0);
  std::string same;
  tj::ShiftPart(part.c_str(), part.size(), 0.0, &same);
  Check(same == part, "a shift of 0 gives the same text", same.size(), part.size());
}

// The keys trace_auto_gap_s and trace_auto_after_s, and the first frame of a
// scene.
inline void AutoKeyChecks() {
  printf("\nThe timeline: the gap and the wait after the first frame of a scene\n");
  const int64_t s = 1000000000LL;
  // gap 20 s: a save 15 s after the last one does not come, 20 s after does.
  Check(!te::AutoShouldSave(80.0, 40, 115 * s, 100 * s, 20 * s, 1, 0),
        "gap 20 s: a long frame 15 s after a save gives no save", 0, 0);
  Check(te::AutoShouldSave(80.0, 40, 120 * s, 100 * s, 20 * s, 1, 0),
        "gap 20 s: a long frame 20 s after a save gives a save", 0, 0);
  Check(!te::AutoShouldSave(80.0, 40, 150 * s, 100 * s, 60 * s, 1, 0),
        "gap 60 s: 50 s after a save gives no save", 0, 0);
  Check(te::AutoShouldSave(80.0, 40, 100 * s, 0, 20 * s, 1, 0), "the first save has no gap", 0, 0);
  // after 20 s: no save in the first 20 s of a scene.
  Check(!te::AutoShouldSave(80.0, 40, 110 * s, 0, 20 * s, 100 * s, 20 * s),
        "after 20 s: a long frame 10 s after the first frame of the scene gives no save", 0, 0);
  Check(!te::AutoShouldSave(80.0, 40, 119 * s, 0, 20 * s, 100 * s, 20 * s),
        "after 20 s: 19 s gives no save", 0, 0);
  Check(te::AutoShouldSave(80.0, 40, 121 * s, 0, 20 * s, 100 * s, 20 * s),
        "after 20 s: 21 s gives a save", 0, 0);
  Check(!te::AutoShouldSave(80.0, 40, 500 * s, 0, 20 * s, 0, 20 * s),
        "no scene start yet: no save", 0, 0);
  Check(te::AutoShouldSave(80.0, 40, 101 * s, 0, 20 * s, 100 * s, 0),
        "trace_auto_after_s=0: no wait after the scene start", 0, 0);

  // The first frame of a scene. Screens make presents with no scene walk, then
  // the scene starts with slow frames (a load): no save for 20 s.
  te::SceneWatch w;
  int64_t t = 0;
  for (int i = 0; i < 250; ++i) {  // 250 presents of screens, 11 ms each
    w.Present(false, t);
    t += 11000000;
  }
  Check(w.start_ns == 0, "screens make no scene", 0, 0);
  w.Present(true, t);  // frame 1 of the scene: 770 ms
  const int64_t start = t;
  Check(w.start_ns == start, "the first scene frame is the start of the scene", 0, 0);
  t += 770000000;
  w.Present(true, t);  // frame 2: 145 ms
  t += 145000000;
  w.Present(true, t);
  Check(w.start_ns == start, "the next scene frames keep the start", 0, 0);
  const int64_t gap = 20 * s;
  Check(!te::AutoShouldSave(145.0, 40, t, 0, gap, w.start_ns, 20 * s),
        "a slow frame of the load 1 s after the first scene frame gives no save", 0, 0);
  // The play goes on with 11 ms frames and one slow frame after 25 s.
  for (int i = 0; i < 2000; ++i) {
    t += 11000000;
    w.Present(true, t);
  }
  Check(w.start_ns == start, "a long run of scene frames has one start", 0, 0);
  Check(te::AutoShouldSave(60.0, 40, t, 0, gap, w.start_ns, 20 * s),
        "a slow frame 22 s into the scene gives a save", 0, 0);
  // One slow scene frame of 3 s in the play (no idle presents before it) does
  // not start a new scene.
  t += 3 * s;
  w.Present(true, t);
  Check(w.start_ns == start, "a hitch of 3 s with no presents between is no new scene", 0, 0);
  // A load again: 30 presents of a screen, then the scene.
  for (int i = 0; i < 30; ++i) {
    t += 40000000;
    w.Present(false, t);
  }
  t += 40000000;
  w.Present(true, t);
  Check(w.start_ns == t, "30 screen presents (1.2 s) then a scene frame start a new scene", 0, 0);
  // Fewer than 10 idle presents do not start a new scene, nor does a short idle time.
  te::SceneWatch v;
  v.Present(true, 1 * s);
  for (int i = 0; i < 12; ++i) v.Present(false, 1 * s + (i + 1) * 20000000LL);
  v.Present(true, 1 * s + 300000000LL);
  Check(v.start_ns == 1 * s, "12 idle presents in 0.3 s are no new scene", 0, 0);
}

// The buttons of the two combinations.
inline void ComboChecks() {
  printf("\nThe timeline: the two button combinations\n");
  namespace tc = trace_combo;
  namespace ic = input_channel;
  ic::SlotData slot;
  memset(&slot, 0, sizeof(slot));
  Check(tc::Held(tc::Bits(slot), true) == 0, "no button: no combination", 0, 0);
  // The first: X and the click of the left stick on the active left hand.
  slot.hands[0].flags = ic::kActive | ic::kPrimary | ic::kStickClick;
  Check(tc::Held(tc::Bits(slot), true) == 1 && tc::Held(tc::Bits(slot), false) == 1,
        "X and the left stick click: the first combination", 0, 0);
  slot.hands[0].flags = ic::kActive | ic::kPrimary;
  Check(tc::Held(tc::Bits(slot), true) == 0, "X alone: none", 0, 0);
  slot.hands[0].flags = ic::kActive | ic::kStickClick;
  Check(tc::Held(tc::Bits(slot), true) == 0, "the left stick click alone: none", 0, 0);
  slot.hands[0].flags = ic::kPrimary | ic::kStickClick;
  Check(tc::Held(tc::Bits(slot), true) == 0, "X and the stick click of a hand that is not active: none", 0, 0);
  // The second: the left trigger and the click of the right stick.
  memset(&slot, 0, sizeof(slot));
  slot.hands[0].flags = ic::kActive;
  slot.hands[1].flags = ic::kActive | ic::kStickClick;
  slot.hands[0].trigger = 0.9f;
  Check(tc::Held(tc::Bits(slot), true) == 2, "the left trigger and the right stick click: the second", 0, 0);
  Check(tc::Held(tc::Bits(slot), false) == 0, "trace_combo2=0: the second is off", 0, 0);
  slot.hands[0].trigger = 0.5f;
  Check(tc::Held(tc::Bits(slot), true) == 0, "a trigger at 0.5 is not held", 0, 0);
  slot.hands[0].trigger = 0.9f;
  slot.hands[1].flags = ic::kActive;
  Check(tc::Held(tc::Bits(slot), true) == 0, "the left trigger alone: none", 0, 0);
  // Both clicks of the sticks together are not a combination (the app shows its
  // menu for them).
  memset(&slot, 0, sizeof(slot));
  slot.hands[0].flags = ic::kActive | ic::kStickClick;
  slot.hands[1].flags = ic::kActive | ic::kStickClick;
  Check(tc::Held(tc::Bits(slot), true) == 0, "both stick clicks together: none", 0, 0);
  // Y, the menu button and the grip are none.
  memset(&slot, 0, sizeof(slot));
  slot.hands[0].flags = ic::kActive | ic::kSecondary | ic::kMenu;
  slot.hands[0].squeeze = 1.0f;
  Check(tc::Held(tc::Bits(slot), true) == 0, "Y, the menu button and the grip: none", 0, 0);
  // The bits tell each button for the log.
  memset(&slot, 0, sizeof(slot));
  slot.hands[0].flags = ic::kActive | ic::kSecondary | ic::kMenu | ic::kPrimary | ic::kStickClick;
  slot.hands[1].flags = ic::kStickClick;
  const uint32_t bits = tc::Bits(slot);
  Check((bits & tc::kLeftY) != 0 && (bits & tc::kLeftMenu) != 0 && (bits & tc::kLeftX) != 0 &&
            (bits & tc::kLeftStick) != 0 && (bits & tc::kRightStick) != 0 && (bits & tc::kRightActive) == 0,
        "Bits has each button of the left hand and the right stick click", bits, 0);
  // One latch for both: a hold of the first and then of the second counts as
  // one hold, and a save comes once.
  te::ComboLatch latch;
  const int64_t ms = 1000000LL;
  memset(&slot, 0, sizeof(slot));
  slot.hands[0].flags = ic::kActive | ic::kPrimary | ic::kStickClick;
  bool fired = latch.Step(tc::Held(tc::Bits(slot), true) != 0, 1000 * ms, 300 * ms);
  slot.hands[0].trigger = 1.0f;
  slot.hands[1].flags = ic::kActive | ic::kStickClick;
  fired = latch.Step(tc::Held(tc::Bits(slot), true) != 0, 1200 * ms, 300 * ms) || fired;
  const bool at300 = latch.Step(tc::Held(tc::Bits(slot), true) != 0, 1300 * ms, 300 * ms);
  const bool at900 = latch.Step(tc::Held(tc::Bits(slot), true) != 0, 1900 * ms, 300 * ms);
  Check(!fired && at300 && !at900, "both held at once: one save", 0, 0);
}

inline void CostChecks() {
  printf("\nThe timeline: the cost of one event\n");
  static const te::Def d = {"cost", "t", {"i", "j", nullptr, nullptr}};
  te::Recorder rec;
  rec.Init(1000000000, 0, 1u << 17);
  rec.Name(3, "frame", 0, 1u << 17);
  const int n = 2000000;
  const auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < n; ++i) {
    rec.Bar(3, &d, static_cast<int64_t>(i) * 100, static_cast<int64_t>(i) * 100 + 50,
            static_cast<float>(i), 1.0f);
  }
  const auto t1 = std::chrono::steady_clock::now();
  const double ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / static_cast<double>(n);
  printf("  info  one bar costs %.1f ns on this PC (the push without the clock read)\n", ns);
  Check(ns < 1000.0, "one event costs less than 1 us", ns, 1000.0);
  // The same with the two reads of the clock that a bar of a timer needs.
  const auto c0 = std::chrono::steady_clock::now();
  for (int i = 0; i < n; ++i) {
    LARGE_INTEGER a, z;
    QueryPerformanceCounter(&a);
    QueryPerformanceCounter(&z);
    rec.Bar(3, &d, a.QuadPart, z.QuadPart + 1, static_cast<float>(i), 1.0f);
  }
  const auto c1 = std::chrono::steady_clock::now();
  const double with_clock =
      std::chrono::duration_cast<std::chrono::nanoseconds>(c1 - c0).count() / static_cast<double>(n);
  printf("  info  one bar with its two reads of QueryPerformanceCounter costs %.1f ns on this PC\n",
         with_clock);
  Check(with_clock < 1000.0, "one bar with its clock reads costs less than 1 us", with_clock, 1000.0);
  rec.Reset();
}


// A sample file of both processes, for a test of tools/trace/trace_report.py:
// the checks write it to the path in the variable BWVR_TRACE_SAMPLE when the
// variable is set. The game runs at 75 frames a second with one long frame,
// the app at 90 frames a second with frames that have no new picture, and the
// camera of the pictures goes back once. The clock of the game is in 100 ns
// ticks, the clock of the app in ns, as in the headset.
inline void SampleFile() {
  char* path = nullptr;
  size_t path_size = 0;
  if (_dupenv_s(&path, &path_size, "BWVR_TRACE_SAMPLE") != 0 || path == nullptr || path[0] == 0) {
    free(path);
    return;
  }
  printf("\nThe timeline: a sample file of both processes, %s\n", path);
  namespace tj = trace_join;
  static const te::Def frame_def = {"frame", "game", {"#frame", "ms", nullptr, nullptr}};
  static const te::Def put_def = {te::kStreamPutName, "stream", {"#frame", "bytes", "wait_ms", "dropped"}};
  static const te::Def pge_def = {"GGame::ProcessGraphicsEngine", "game", {nullptr, nullptr, nullptr, nullptr}};
  static const te::Def draw_def = {"GLandscape::Draw 0x005E42E0", "game", {nullptr, nullptr, nullptr, nullptr}};
  static const te::Def turn_def = {"GGame::ProcessNetworkPackets (game turn)", "game", {nullptr, nullptr, nullptr, nullptr}};
  static const te::Def wait_def = {"xrWaitFrame", "xr", {nullptr, nullptr, nullptr, nullptr}};
  static const te::Def arrive_def = {te::kStreamArriveName, "reader", {"#frame", "bytes", "queue", nullptr}};
  static const te::Def nopic_def = {"frame of the session with no new picture", "session", {"#shown frame", nullptr, nullptr, nullptr}};
  static const te::Def motion_def = {"motion of the stick (state, shift of the pose)", "session", {"state of the left eye", "shift left cm", "shift right cm", "#game frame"}};
  static const te::Def shown_def = {"picture shown (game frame, place of the camera in the world)", "session", {"#frame", "world x", "world y", "world z"}};
  const int64_t boot_ns = 364000000000000LL;
  // The clock of the game is 2.5 s behind the clock of the app.
  const int64_t game_behind_ns = 2500000000LL;
  const int64_t gf = 10000000, af = 1000000000;
  const uint64_t base = te::kScreenFrameBase;
  te::Recorder game, app;
  game.Init(gf, 0, 4096);
  app.Init(af, 0, 4096);
  game.Name(100, "frame thread", 1, 4096);
  game.Name(101, "frames (present to present)", 0, 4096);
  game.Name(102, "stream worker", 3, 4096);
  app.Name(200, "session thread", 0, 4096);
  app.Name(201, "reader (live stream)", 3, 4096);
  // The game frames: 13.3 ms, frame 60 is 28 ms long (the draw of the land).
  // The frame number is the number of a screen frame (1e9 + count), as on the
  // headset. The game ticks are on its own clock.
  int64_t t_ns = 0;
  for (int f = 1; f <= 140; ++f) {
    const int64_t len = f == 60 ? 28000000LL : 13300000LL;
    const int64_t b = boot_ns + t_ns, e = b + len;
    const auto tk = [&](int64_t ns) { return te::NsToTicks(ns - game_behind_ns, gf); };
    const float frame = te::FrameArg(base + 4000 + f);
    game.Bar(101, &frame_def, tk(b), tk(e), frame, static_cast<float>(len / 1e6));
    game.Bar(100, &turn_def, tk(b + 100000), tk(b + 2500000));
    const int64_t pge_b = b + 3000000;
    const int64_t draw_len = f == 60 ? 18000000LL : 4500000LL;
    game.Bar(100, &pge_def, tk(pge_b), tk(pge_b + draw_len + 1500000));
    game.Bar(100, &draw_def, tk(pge_b + 500000), tk(pge_b + 500000 + draw_len));
    game.BarKept(102, &put_def, tk(e - 600000), tk(e - 100000), frame, 20000.0f);
    // The app reader sees the frame 0.4 ms after the end of the write.
    app.Point(201, &arrive_def, e - 100000 + 400000, frame, 20000.0f, 1.0f);
    t_ns += len;
  }
  // The app frames: 11.1 ms. Every 7th frame has no new picture. The camera
  // goes on in x with the frames, and goes back at the picture of frame 90.
  int64_t a_ns = 0;
  int shown_game = 0;
  float x = 0.0f;
  for (int i = 0; i < 160; ++i) {
    const int64_t b = boot_ns + a_ns;
    app.Bar(200, &wait_def, b, b + 2000000);
    const int game_frame = static_cast<int>((a_ns - 1500000) / 13300000) + 1;
    const bool fresh = i % 7 != 6 && game_frame > shown_game && game_frame >= 1;
    if (fresh) {
      shown_game = game_frame;
      x += game_frame == 90 ? -0.9f : 0.3f;
      app.Point(200, &shown_def, b + 3000000, te::FrameArg(base + 4000 + game_frame), x, 0.0f, 100.0f);
      // The motion of the stick does nothing in the sample: the jump comes from
      // the picture of the game.
      app.Point(200, &motion_def, b + 3000100, 3.0f, 0.0f, 0.0f, te::FrameArg(base + 4000 + game_frame));
    } else if (i > 5) {
      app.Point(200, &nopic_def, b + 3000000, te::FrameArg(base + 4000 + shown_game));
    }
    a_ns += 11111111LL;
  }
  const int64_t end_ns = boot_ns + t_ns;
  const int64_t base_ns = boot_ns;
  char buf[2048];
  // The part of the app, and the arrivals in it.
  std::string app_part, game_part;
  std::vector<tj::Sample> arrivals;
  size_t n = te::FormatProcessName(buf, sizeof(buf), 2, "app", false);
  app_part.append(buf, n);
  uint32_t total = 0;
  for (int r = 0; r < app.Count(); ++r) {
    const te::Ring* ring = app.At(r);
    std::vector<te::Event> copy(ring->capacity);
    bool overrun = false;
    const uint32_t count = te::CopyWindow(*ring, te::NsToTicks(base_ns, af),
                                          te::NsToTicks(end_ns + 1000000, af), copy.data(), ring->capacity, &overrun);
    n = te::FormatThreadName(buf, sizeof(buf), 2, ring->tid, ring->name, ring->sort, true);
    app_part.append(buf, n);
    for (uint32_t k = 0; k < count; ++k) {
      n = te::FormatEvent(buf, sizeof(buf), copy[k], 2, ring->tid, af, base_ns, true);
      app_part.append(buf, n);
      ++total;
      if (copy[k].def == &arrive_def) {
        arrivals.push_back({te::FrameFromArg(copy[k].v[0]), te::UsFrom(copy[k].ts, af, base_ns)});
      }
    }
  }
  // The part of the game: the ticks are on the clock of the game, and the
  // start of the window is the same number.
  n = te::FormatProcessName(buf, sizeof(buf), 1, "game", false);
  game_part.append(buf, n);
  for (int r = 0; r < game.Count(); ++r) {
    const te::Ring* ring = game.At(r);
    std::vector<te::Event> copy(ring->capacity);
    bool overrun = false;
    const uint32_t count = te::CopyWindow(*ring, 0, te::NsToTicks(end_ns, gf), copy.data(),
                                          ring->capacity, &overrun);
    n = te::FormatThreadName(buf, sizeof(buf), 1, ring->tid, ring->name, ring->sort, true);
    game_part.append(buf, n);
    for (uint32_t k = 0; k < count; ++k) {
      n = te::FormatEvent(buf, sizeof(buf), copy[k], 1, ring->tid, gf, base_ns, true);
      game_part.append(buf, n);
      ++total;
    }
  }
  // The join as the app makes it.
  std::vector<tj::Sample> puts;
  tj::ScanPuts(game_part.c_str(), game_part.size(), &puts);
  const tj::Shift shift = tj::EstimateShift(puts, arrivals);
  Check(shift.pairs == 140 && shift.apply, "the sample join finds the 140 frames of both parts",
        shift.pairs, 140);
  Check(fabs(shift.shift_us - 2499900.0) < 5.0, "the join finds the shift of 2.5 s less 0.1 ms",
        shift.shift_us, 2499900.0);
  std::string shifted;
  tj::ShiftPart(game_part.c_str(), game_part.size(), shift.shift_us, &shifted);
  const auto write_file = [&](const char* file_path, const std::string& game_text, bool marker) {
    FILE* f = nullptr;
    if (fopen_s(&f, file_path, "wb") != 0) f = nullptr;
    if (f == nullptr) {
      Check(false, "the sample file can be written", 0, 1);
      return;
    }
    size_t w = te::FormatHeader(buf, sizeof(buf), base_ns, 2.0, "a sample for trace_report.py");
    fwrite(buf, 1, w, f);
    fwrite(app_part.data(), 1, app_part.size(), f);
    fwrite(",\n", 1, 2, f);
    fwrite(game_text.data(), 1, game_text.size(), f);
    if (marker) {
      w = static_cast<size_t>(snprintf(
          buf, sizeof(buf),
          ",\n{\"name\":\"%s\",\"cat\":\"join\",\"ph\":\"i\",\"s\":\"g\",\"ts\":0.000,\"pid\":2,"
          "\"tid\":0,\"args\":{\"pairs\":%d,\"shift_ms\":%.3f,\"applied\":1,\"median_ms\":%.3f,"
          "\"spread_ms\":%.3f}}",
          te::kJoinName, shift.pairs, shift.shift_us / 1000.0, shift.median_us / 1000.0,
          shift.spread_us / 1000.0));
      fwrite(buf, 1, w, f);
    }
    w = te::FormatFooter(buf, sizeof(buf));
    fwrite(buf, 1, w, f);
    fclose(f);
  };
  write_file(path, shifted, true);
  // The same file with the game on its own clock, as a join with trace_shift=0
  // writes it.
  char* raw = nullptr;
  size_t raw_size = 0;
  if (_dupenv_s(&raw, &raw_size, "BWVR_TRACE_SAMPLE_RAW") == 0 && raw != nullptr && raw[0] != 0) {
    write_file(raw, game_part, false);
  }
  free(raw);
  game.Reset();
  app.Reset();
  free(path);
  Check(total > 1000, "the sample file has events of both processes", total, 1000);
}

inline void TraceChecks() {
  FormatChecks();
  ClockChecks();
  RingChecks();
  PartNameChecks();
  MovedHeadChecks();
  MovedTwiceChecks();
  ClaimBoundChecks();
  TriggerChecks();
  FrameNumberChecks();
  JoinChecks();
  AutoKeyChecks();
  ComboChecks();
  CostChecks();
  SampleFile();
}

}  // namespace trace_checks
