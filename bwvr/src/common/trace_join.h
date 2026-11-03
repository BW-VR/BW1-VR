// THE JOIN OF THE TWO PARTS OF A TIMELINE FILE (docs/android.md, "The
// timeline", docs/backend.md, "The timeline of the game and the app"). The pure
// part, with no Android code: the app and the math test use it
// (test/controller_math/trace_checks.h).
//
// The game and the app write their events on the clock CLOCK_BOOTTIME, but the
// clock of the game (QueryPerformanceCounter of Wine) can have another start.
// Both processes know the frames of the stream: the game writes a frame into
// the ring (the bar "stream put", it ends when the app can see the frame), and
// the reader of the app sees it 0 to 1 ms later (the point "game frame
// arrives"). The shift of the game against the app is the median of
// (arrival less end of the write) less kArrivalUs. The join adds the shift to
// each time in the part of the game.
//
// The part of the game is a text of events, one event in a line, as
// trace_events.h writes it. The functions here read and change that text. They
// need the frame numbers in the args to be exact (FrameArg). The text must end
// with a 0 byte after its last character (a std::string does), because the
// numbers are read with strtod.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "trace_events.h"

namespace trace_join {

// The time of a frame in us of the file, or its end of the write.
struct Sample {
  int64_t frame;
  double us;
};

// The time that the reader of the app needs to see a frame after the end of
// its write: between 0 and 1 ms.
constexpr double kArrivalUs = 500.0;
// Fewer common frames than this give no shift.
constexpr int kMinPairs = 3;
// The 10th to the 90th percentile of the differences must be inside this, or
// the frames do not belong together (a wrong match gives a wide spread).
constexpr double kMaxSpreadUs = 100000.0;

struct Shift {
  int pairs = 0;          // the frames that both processes know
  double least_us = 0.0;  // the differences: arrival less end of the write
  double p10_us = 0.0;
  double median_us = 0.0;
  double p90_us = 0.0;
  double most_us = 0.0;
  double spread_us = 0.0;  // p90 less p10
  double shift_us = 0.0;   // median less kArrivalUs: add it to the game
  bool apply = false;      // enough frames and a small spread
};

inline double Percentile(const std::vector<double>& sorted, double q) {
  if (sorted.empty()) return 0.0;
  const double pos = q * static_cast<double>(sorted.size() - 1);
  const size_t lo = static_cast<size_t>(pos);
  const size_t hi = lo + 1 < sorted.size() ? lo + 1 : lo;
  const double f = pos - static_cast<double>(lo);
  return sorted[lo] + (sorted[hi] - sorted[lo]) * f;
}

// puts: the end of the write of each frame in the game. arrivals: the arrival
// of each frame in the app. A frame may be in a list more than once: the last
// write of a frame and its first arrival count.
inline Shift EstimateShift(const std::vector<Sample>& puts, const std::vector<Sample>& arrivals) {
  Shift s;
  std::vector<Sample> p = puts;
  std::vector<Sample> a = arrivals;
  // Sorted by frame, then by time: for equal frames the last put is the one
  // before the next frame, and the first arrival is the one that counts.
  std::stable_sort(p.begin(), p.end(), [](const Sample& x, const Sample& y) {
    return x.frame != y.frame ? x.frame < y.frame : x.us < y.us;
  });
  std::stable_sort(a.begin(), a.end(), [](const Sample& x, const Sample& y) {
    return x.frame != y.frame ? x.frame < y.frame : x.us < y.us;
  });
  std::vector<double> diff;
  size_t i = 0, j = 0;
  while (i < p.size() && j < a.size()) {
    if (p[i].frame < a[j].frame) {
      ++i;
    } else if (p[i].frame > a[j].frame) {
      ++j;
    } else {
      const int64_t frame = p[i].frame;
      double last_put = p[i].us;
      while (i < p.size() && p[i].frame == frame) last_put = p[i++].us;
      const double first_arrival = a[j].us;
      while (j < a.size() && a[j].frame == frame) ++j;
      diff.push_back(first_arrival - last_put);
    }
  }
  s.pairs = static_cast<int>(diff.size());
  if (diff.empty()) return s;
  std::sort(diff.begin(), diff.end());
  s.least_us = diff.front();
  s.most_us = diff.back();
  s.p10_us = Percentile(diff, 0.10);
  s.median_us = Percentile(diff, 0.50);
  s.p90_us = Percentile(diff, 0.90);
  s.spread_us = s.p90_us - s.p10_us;
  s.shift_us = s.median_us - kArrivalUs;
  s.apply = s.pairs >= kMinPairs && s.spread_us <= kMaxSpreadUs;
  return s;
}

// ---------------------------------------------------------------------------
// THE TEXT OF THE PART OF THE GAME.

// The value after `"key":` in [line, end), as a number. Gives false when the
// key is not there.
inline bool FindNumber(const char* line, const char* end, const char* key, double* out) {
  char pattern[48];
  const int w = snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  if (w <= 0 || static_cast<size_t>(w) >= sizeof(pattern)) return false;
  const size_t plen = static_cast<size_t>(w);
  for (const char* p = line; p + plen <= end; ++p) {
    if (memcmp(p, pattern, plen) != 0) continue;
    const char* v = p + plen;
    if (v >= end) return false;
    char* stop = nullptr;
    const double value = strtod(v, &stop);
    if (stop == v) return false;
    *out = value;
    return true;
  }
  return false;
}

// The end of each write of a frame in the text of a part: the events "stream
// put" with the arg "frame".
inline void ScanPuts(const char* text, size_t n, std::vector<Sample>* puts) {
  char name[96];
  snprintf(name, sizeof(name), "\"name\":\"%s\"", trace_events::kStreamPutName);
  const char* end = text + n;
  const char* line = text;
  while (line < end) {
    const char* stop = static_cast<const char*>(memchr(line, '\n', static_cast<size_t>(end - line)));
    const char* line_end = stop != nullptr ? stop : end;
    const size_t len = static_cast<size_t>(line_end - line);
    const size_t nlen = strlen(name);
    bool named = false;
    for (size_t k = 0; k + nlen <= len; ++k) {
      if (memcmp(line + k, name, nlen) == 0) {
        named = true;
        break;
      }
    }
    double ts = 0, dur = 0, frame = 0;
    if (named && FindNumber(line, line_end, "ts", &ts) && FindNumber(line, line_end, "dur", &dur) &&
        FindNumber(line, line_end, "frame", &frame)) {
      Sample s;
      s.frame = static_cast<int64_t>(frame + 0.5);
      s.us = ts + dur;
      puts->push_back(s);
    }
    line = stop != nullptr ? stop + 1 : end;
  }
}

// The text with shift_us added to each "ts". The thread names have no "ts",
// so they stay as they are. The new time has 3 decimals as the old one.
inline void ShiftPart(const char* text, size_t n, double shift_us, std::string* out) {
  out->clear();
  out->reserve(n + n / 64);
  const char* end = text + n;
  const char* p = text;
  static const char kKey[] = "\"ts\":";
  const size_t klen = sizeof(kKey) - 1;
  while (p < end) {
    const char* hit = nullptr;
    for (const char* q = p; q + klen <= end; ++q) {
      if (*q == '"' && memcmp(q, kKey, klen) == 0) {
        hit = q;
        break;
      }
    }
    if (hit == nullptr) {
      out->append(p, static_cast<size_t>(end - p));
      return;
    }
    const char* v = hit + klen;
    char* stop = nullptr;
    const double value = v < end ? strtod(v, &stop) : 0.0;
    if (stop == nullptr || stop == v) {
      out->append(p, static_cast<size_t>(v - p));
      p = v;
      continue;
    }
    out->append(p, static_cast<size_t>(v - p));
    char number[48];
    const int w = snprintf(number, sizeof(number), "%.3f", value + shift_us);
    if (w > 0) out->append(number, static_cast<size_t>(w));
    p = stop;
  }
}

}  // namespace trace_join
