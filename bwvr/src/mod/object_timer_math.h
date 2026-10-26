// THE TIMER OF THE OBJECTS ON THE LAND (object_timer.cpp, docs/backend.md,
// "The timer of the objects on the land"): the forms of a call that the timer
// can take, the table of the timers of the classes, and the first line of a
// window. No Windows code.
//
// A timer goes on a call, not on the start of a function. The mod writes a
// call of a stub of its own in the place of the call of the game. The stub
// does what the bytes of the game did (the mov of a vtable, the read of the
// address of the function) and then jumps to the function, with the address
// of the exit of the timer as the return address. So the code of the game
// and its stack stay as they are, and the stub needs no copy of the code of
// the function.
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "walk_parts_math.h"

namespace object_timer_math {

// The forms of a call.
enum CallForm {
  kNoCall = 0,
  kDirect,       // E8 rel32: call fn
  kMemory,       // FF 15 abs32: call dword ptr [abs]
  kBaseDisp32,   // FF /2, mod 10: call dword ptr [reg + disp32]
  kMovDisp8,     // 8B /r then FF /2 mod 01: mov dst, [src] or mov dst, src,
                 // then call dword ptr [base + disp8]. 5 bytes together
  kSlot,         // an entry of a table of methods (a vtable), not code
};

// The registers in the order of the x86 code: eax 0, ecx 1, edx 2, ebx 3,
// esp 4, ebp 5, esi 6, edi 7. pushad stores them in the opposite order, so
// register r is at index 7 - r of the block that pushad writes.
inline int PushadIndex(int reg) { return 7 - reg; }

struct Call {
  int form = kNoCall;
  int length = 0;       // the bytes that the call of the stub takes
  int base = 0;         // the register of the address of the function
  int dst = 0;          // kMovDisp8: the register that the mov writes
  int src = 0;          // kMovDisp8: the register that the mov reads
  bool load = false;    // kMovDisp8: true for mov dst, [src]
  int32_t disp = 0;
  uint32_t target = 0;  // kDirect: the function. kMemory: the address of
                        // the pointer. kSlot: the function in the entry
};

// The form of the call at `at` from its first 6 bytes. kNoCall when the
// timer cannot take it. A register esp is not taken: the stub runs with a
// different esp.
inline Call DecodeCall(const uint8_t* b, uint32_t at) {
  Call c;
  if (b[0] == 0xE8) {
    int32_t rel;
    memcpy(&rel, b + 1, 4);
    c.form = kDirect;
    c.length = 5;
    c.target = at + 5 + static_cast<uint32_t>(rel);
    return c;
  }
  if (b[0] == 0xFF && b[1] == 0x15) {
    c.form = kMemory;
    c.length = 6;
    memcpy(&c.target, b + 2, 4);
    return c;
  }
  if (b[0] == 0xFF && (b[1] & 0xF8) == 0x90 && (b[1] & 7) != 4) {
    c.form = kBaseDisp32;
    c.length = 6;
    c.base = b[1] & 7;
    memcpy(&c.disp, b + 2, 4);
    return c;
  }
  if (b[0] == 0x8B && b[2] == 0xFF && (b[3] & 0xF8) == 0x50 && (b[3] & 7) != 4) {
    const int mod = b[1] >> 6;
    const int dst = (b[1] >> 3) & 7;
    const int src = b[1] & 7;
    if (dst == 4 || src == 4) return c;
    if (mod == 0 && src != 5) {
      c.load = true;
    } else if (mod != 3) {
      return c;
    }
    c.form = kMovDisp8;
    c.length = 5;
    c.dst = dst;
    c.src = src;
    c.base = b[3] & 7;
    c.disp = static_cast<int8_t>(b[4]);
    return c;
  }
  return c;
}

// The bytes that take the place of a call: a call of the stub, and a nop
// for each byte past 5. Gives false when the stub is too far (it cannot be
// in a 32 bit process).
inline bool CallBytes(uint32_t at, uint32_t stub, int length, uint8_t out[8]) {
  if (length < 5 || length > 8) return false;
  const int32_t rel = static_cast<int32_t>(stub - (at + 5));
  out[0] = 0xE8;
  memcpy(out + 1, &rel, 4);
  for (int i = 5; i < length; ++i) out[i] = 0x90;
  return true;
}

// The address of the function that the call goes to, from the registers
// that pushad stored (`regs`) and a reader of memory. For kMovDisp8 the mov
// writes its register in `regs`, so the function gets it. `read` is
// uint32_t(uint32_t address).
template <typename Read>
uint32_t Target(const Call& c, uint32_t* regs, Read read) {
  switch (c.form) {
    case kDirect:
    case kSlot:
      return c.target;
    case kMemory:
      return read(c.target);
    case kBaseDisp32:
      return read(regs[PushadIndex(c.base)] + static_cast<uint32_t>(c.disp));
    case kMovDisp8: {
      const uint32_t v = regs[PushadIndex(c.src)];
      regs[PushadIndex(c.dst)] = c.load ? read(v) : v;
      return read(regs[PushadIndex(c.base)] + static_cast<uint32_t>(c.disp));
    }
    default:
      return 0;
  }
}

// THE TIMERS OF THE KEYS. A call of a method of the objects gets a timer for
// each key: the vtable of the object for the draw of the objects (each class
// gets its line), the function for the other calls. A table with open
// addressing. A key is never 0.
constexpr int kKeySlots = 512;
struct KeyTable {
  uint32_t key[kKeySlots] = {};
  int16_t timer[kKeySlots] = {};

  static int Hash(uint32_t k) { return static_cast<int>((k * 2654435761u) >> 23) & (kKeySlots - 1); }

  // The timer of `k`, or -1.
  int Find(uint32_t k) const {
    int i = Hash(k);
    for (int n = 0; n < kKeySlots; ++n) {
      if (key[i] == k) return timer[i];
      if (key[i] == 0) return -1;
      i = (i + 1) & (kKeySlots - 1);
    }
    return -1;
  }

  // Adds `k` with timer `t`. False when the table is full or `k` is 0.
  bool Add(uint32_t k, int t) {
    if (k == 0) return false;
    int i = Hash(k);
    for (int n = 0; n < kKeySlots; ++n) {
      if (key[i] == 0 || key[i] == k) {
        key[i] = k;
        timer[i] = static_cast<int16_t>(t);
        return true;
      }
      i = (i + 1) & (kKeySlots - 1);
    }
    return false;
  }
};

// The first line of a window. Timer 0 is the call of fn_005E5CD0 from
// GLandscape::Draw, so its inclusive time is the draw of the objects and
// its exclusive time the part that no other timer covers. `pair_ns` is the
// cost of one timed call, from the measure at the start. `objects` is the
// mean count of the list of objects at the end of a frame.
inline int HeadLine(char* out, size_t size, const walk_parts_math::Window& w, int timers,
                    double seconds, double pair_ns, double objects) {
  const double all = w.Ms(w.incl[0].Mean());
  const double timed = w.TimedMs(timers);
  const double outside = w.Ms(w.excl[0].Mean());
  const double cost = w.pairs.Mean() * pair_ns / 1.0e6;
  return snprintf(out, size,
                  "%.1f s, %llu frames. fn_005E5CD0 in a frame, ms, the mean and the most: "
                  "%.3f and %.3f. The timed calls %.3f ms, its own code outside each timer "
                  "%.3f ms. %.0f objects in its list. %.0f timed calls in a frame (most %.0f), "
                  "at %.0f ns for each call about %.3f ms",
                  seconds, static_cast<unsigned long long>(w.frames), all, w.Ms(w.incl[0].most),
                  timed, outside, objects, w.pairs.Mean(), w.pairs.most, pair_ns, cost);
}

}  // namespace object_timer_math
