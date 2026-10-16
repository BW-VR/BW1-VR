// The SHA-1 of runblack.exe and the builds of the game that the mod knows.
//
// exe_check.h reads the file with Windows and compares its SHA-1 with these
// values. The headset app (android/src/game_folder.cpp) reads it with POSIX.
// This file has no Windows code, so both builds use it.
#pragma once

#include <stdint.h>
#include <string.h>

#include "bw_addresses.h"

namespace bwvr {

// One source for the value: bw_addresses.h. The installer has a copy.
constexpr uint64_t kGameExeSize = bw::kExpectedSize;
constexpr const char* kGameExeSha1 = bw::kExpectedSha1;

// Builds that are known and not supported, so that a message can name them.
struct KnownBuild {
  const char* sha1;
  const char* name;
};
constexpr KnownBuild kKnownBuilds[] = {
    {"85dc3180803153162a46d43433f8dcaa883af02c", "version 1.00"},
    {"800254f0c91684024cc51bc3f479192d7fec1068", "a changed version 1.00"},
    {"43ad8e54f51e0de9f97fcb884b37bf7d315c19d6", "a changed version 1.00"},
    {"3c7297dae357834860ff46a89ca5d1a552747100", "version 1.10"},
    {"c9bba68e17f5ec162ff235a496779f43090508db", "version 1.20"},
    {"a0fd87fdb1352fef65ba728ab732bcb1671f67e9", "version 1.30"},
};

namespace detail {

// SHA-1 as in FIPS 180-4. Small and slow is good enough: it runs once, on
// 8.6 MB, and it needs nothing but the CPU.
struct Sha1 {
  uint32_t h[5];
  uint64_t bytes;
  uint8_t block[64];
  uint32_t used;
};

inline uint32_t Rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

inline void Sha1Block(uint32_t h[5], const uint8_t* p) {
  uint32_t w[80];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(p[4 * i]) << 24) |
           (static_cast<uint32_t>(p[4 * i + 1]) << 16) |
           (static_cast<uint32_t>(p[4 * i + 2]) << 8) |
           static_cast<uint32_t>(p[4 * i + 3]);
  }
  for (int i = 16; i < 80; ++i) {
    w[i] = Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }
  uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
  for (int i = 0; i < 80; ++i) {
    uint32_t f, k;
    if (i < 20) {
      f = (b & c) | (~b & d);
      k = 0x5A827999u;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1u;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDCu;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6u;
    }
    const uint32_t t = Rol(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = Rol(b, 30);
    b = a;
    a = t;
  }
  h[0] += a;
  h[1] += b;
  h[2] += c;
  h[3] += d;
  h[4] += e;
}

inline void Sha1Start(Sha1* s) {
  s->h[0] = 0x67452301u;
  s->h[1] = 0xEFCDAB89u;
  s->h[2] = 0x98BADCFEu;
  s->h[3] = 0x10325476u;
  s->h[4] = 0xC3D2E1F0u;
  s->bytes = 0;
  s->used = 0;
}

inline void Sha1Add(Sha1* s, const uint8_t* data, uint32_t n) {
  s->bytes += n;
  while (n > 0) {
    if (s->used == 0 && n >= 64) {
      Sha1Block(s->h, data);
      data += 64;
      n -= 64;
      continue;
    }
    uint32_t take = 64 - s->used;
    if (take > n) take = n;
    memcpy(s->block + s->used, data, take);
    s->used += take;
    data += take;
    n -= take;
    if (s->used == 64) {
      Sha1Block(s->h, s->block);
      s->used = 0;
    }
  }
}

inline void Sha1Finish(Sha1* s, char hex[41]) {
  const uint64_t bits = s->bytes * 8;
  const uint8_t one = 0x80;
  const uint8_t zero = 0;
  Sha1Add(s, &one, 1);
  while (s->used != 56) Sha1Add(s, &zero, 1);
  uint8_t length[8];
  for (int i = 0; i < 8; ++i) {
    length[i] = static_cast<uint8_t>(bits >> (56 - 8 * i));
  }
  Sha1Add(s, length, 8);
  static const char kDigits[] = "0123456789abcdef";
  for (int i = 0; i < 20; ++i) {
    const uint8_t byte = static_cast<uint8_t>(s->h[i / 4] >> (24 - 8 * (i % 4)));
    hex[2 * i] = kDigits[byte >> 4];
    hex[2 * i + 1] = kDigits[byte & 15];
  }
  hex[40] = 0;
}

}  // namespace detail

// The name of a known build for a SHA-1, or null.
inline const char* KnownBuildName(const char* sha1) {
  for (const KnownBuild& known : kKnownBuilds) {
    if (strcmp(sha1, known.sha1) == 0) return known.name;
  }
  return nullptr;
}

}  // namespace bwvr
