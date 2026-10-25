// THE MODULE LIST AT A STALL AND THE TEST SPIN, the pure part
// (stall_watch.cpp, docs/backend.md, "The module list at a stall"). No
// Windows, no game. The checks are in test/controller_math/stall_checks.h.

#pragma once

#include <stdint.h>
#include <stdlib.h>

namespace stall_math {

// A stall: no frame of the game ended for this time.
const uint32_t kStallMs = 5000;
// The test spin (test_stall_ms) comes this long after the first frame of a
// scene.
const uint32_t kSpinAfterMs = 20000;
// The addresses of stall_addrs that the mod takes, at most.
const int kMaxAddrs = 16;

// stall_addrs: hexadecimal addresses with spaces or commas between them,
// with or without "0x". An address can have 16 digits, as Wine writes it
// ("000000007AF441E8"), but it must fit in 32 bits. Gives the count of the
// addresses in `out` (at most `max`), and in `bad` the count of the words
// that are not such an address.
inline int ParseAddrs(const char* text, uint32_t* out, int max, int* bad) {
  int count = 0;
  *bad = 0;
  const char* p = text;
  while (*p != 0) {
    while (*p == ' ' || *p == ',' || *p == '\t') ++p;
    if (*p == 0) break;
    const char* start = p;
    while (*p != 0 && *p != ' ' && *p != ',' && *p != '\t') ++p;
    const char* digits = start;
    if (p - digits > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
      digits += 2;
    }
    bool ok = digits < p && p - digits <= 16;
    unsigned long long value = 0;
    for (const char* c = digits; ok && c < p; ++c) {
      int d = -1;
      if (*c >= '0' && *c <= '9') d = *c - '0';
      if (*c >= 'a' && *c <= 'f') d = *c - 'a' + 10;
      if (*c >= 'A' && *c <= 'F') d = *c - 'A' + 10;
      if (d < 0) ok = false;
      value = value * 16 + static_cast<unsigned long long>(d < 0 ? 0 : d);
    }
    if (ok && value > 0xFFFFFFFFull) ok = false;
    if (!ok || count >= max) {
      ++*bad;
      continue;
    }
    out[count++] = static_cast<uint32_t>(value);
  }
  return count;
}

// THE WATCH: the thread of the watch looks at the count of the frames that
// ended. It says a stall one time, when the count stays the same for
// kStallMs. A new frame ends the stall. Before the first frame there is no
// stall.
struct Watch {
  uint32_t last_count = 0;
  uint64_t since_ms = 0;
  bool started = false;
  bool said = false;

  // True one time for each stall.
  bool Step(uint32_t count, uint64_t now_ms) {
    if (count == 0) return false;
    if (count != last_count || !started) {
      started = true;
      last_count = count;
      since_ms = now_ms;
      said = false;
      return false;
    }
    if (said || now_ms - since_ms < kStallMs) return false;
    said = true;
    return true;
  }
};

// THE TEST SPIN (test_stall_ms): one time, kSpinAfterMs after the first walk
// of a scene.
struct Spin {
  int ms = 0;
  double first_ms = -1.0;
  bool done = false;

  // At the end of each scene walk. True one time: spin now for `ms`.
  bool Due(double now_ms) {
    if (ms <= 0 || done) return false;
    if (first_ms < 0.0) {
      first_ms = now_ms;
      return false;
    }
    if (now_ms - first_ms < kSpinAfterMs) return false;
    done = true;
    return true;
  }
};

}  // namespace stall_math
