// The LARGE_ADDRESS_AWARE flag of runblack.exe, a test (large_address.h).
// main.cpp includes this file after Check, just before main.
#pragma once

#include <string.h>

#include "../../src/common/large_address.h"

namespace large_address_checks {

// The start of a PE image for x86 with its PE header at 0x140 and the
// Characteristics 0x010F, as in runblack.exe v1.42.
inline void MakeHead(uint8_t* head, uint32_t n) {
  memset(head, 0, n);
  head[0] = 'M';
  head[1] = 'Z';
  head[0x3C] = 0x40;
  head[0x3D] = 0x01;
  head[0x140] = 'P';
  head[0x141] = 'E';
  head[0x144] = 0x4C;
  head[0x145] = 0x01;
  head[0x156] = 0x0F;
  head[0x157] = 0x01;
}

inline void LargeAddressChecks() {
  printf("\nThe LARGE_ADDRESS_AWARE flag of runblack.exe\n");
  uint8_t head[1024];
  MakeHead(head, sizeof(head));

  Check(bwvr::CharacteristicsOffset(head, sizeof(head)) == 0x156,
        "the field is 22 bytes after the PE header",
        bwvr::CharacteristicsOffset(head, sizeof(head)), 0x156);

  Check(bwvr::SetLargeAddressInHead(head, sizeof(head), true) &&
            head[0x156] == 0x2F && head[0x157] == 0x01,
        "set: 0x010F becomes 0x012F", head[0x156], 0x2F);
  uint8_t copy[1024];
  memcpy(copy, head, sizeof(head));
  Check(bwvr::SetLargeAddressInHead(head, sizeof(head), true) &&
            memcmp(copy, head, sizeof(head)) == 0,
        "a second set changes nothing", 1, 1);

  Check(bwvr::SetLargeAddressInHead(head, sizeof(head), false) &&
            head[0x156] == 0x0F && head[0x157] == 0x01,
        "clear: 0x012F becomes 0x010F again", head[0x156], 0x0F);
  uint8_t plain[1024];
  MakeHead(plain, sizeof(plain));
  Check(memcmp(plain, head, sizeof(head)) == 0,
        "set and clear give the same bytes as before", 1, 1);

  // Bytes that are not an image for x86 are not touched.
  uint8_t bad[1024];
  MakeHead(bad, sizeof(bad));
  bad[0] = 'X';
  Check(!bwvr::SetLargeAddressInHead(bad, sizeof(bad), true) &&
            bad[0x156] == 0x0F,
        "no MZ: no change", bad[0x156], 0x0F);
  MakeHead(bad, sizeof(bad));
  bad[0x141] = 'X';
  Check(bwvr::CharacteristicsOffset(bad, sizeof(bad)) == 0,
        "no PE signature: no field", 1, 1);
  MakeHead(bad, sizeof(bad));
  bad[0x144] = 0x64;
  bad[0x145] = 0x86;
  Check(bwvr::CharacteristicsOffset(bad, sizeof(bad)) == 0,
        "an image for x64: no field", 1, 1);
  Check(bwvr::CharacteristicsOffset(head, 0x150) == 0,
        "the field past the bytes that were read: no field", 1, 1);
  MakeHead(bad, sizeof(bad));
  bad[0x3C] = 0xFF;
  bad[0x3D] = 0xFF;
  bad[0x3E] = 0xFF;
  bad[0x3F] = 0xFF;
  Check(bwvr::CharacteristicsOffset(bad, sizeof(bad)) == 0,
        "a PE offset near 4 GB does not wrap", 1, 1);
}

}  // namespace large_address_checks
