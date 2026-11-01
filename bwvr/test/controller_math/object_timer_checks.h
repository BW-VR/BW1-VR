// The timer of the objects on the land, a test: the forms of a call, the
// address of the function from the registers, the bytes of the call of the
// stub, and the table of the keys (src/mod/object_timer_math.h). main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include <stdint.h>
#include <string.h>

#include "../../src/mod/object_timer_math.h"

namespace object_timer_checks {

// A memory of 64 words at the address 0x1000, for Target.
struct Memory {
  uint32_t word[64] = {};
  uint32_t operator()(uint32_t a) const {
    const uint32_t i = (a - 0x1000) / 4;
    return i < 64 ? word[i] : 0xDEADBEEF;
  }
};

inline void ObjectTimerChecks() {
  namespace ot = object_timer_math;
  printf("\nThe timer of the objects on the land (object_timer)\n");

  // call 0x005E5CD0 at 0x005E4EDD (the bytes of GLandscape::Draw).
  const uint8_t direct[8] = {0xE8, 0xEE, 0x0D, 0x00, 0x00, 0, 0, 0};
  ot::Call c = ot::DecodeCall(direct, 0x005E4EDD);
  Check(c.form == ot::kDirect && c.length == 5, "E8 rel32 is a direct call of 5 bytes", c.length, 5);
  Check(c.target == 0x005E5CD0, "the target of the direct call", c.target, 0x005E5CD0);

  // call dword ptr [eax + 0x610]
  const uint8_t disp32[8] = {0xFF, 0x90, 0x10, 0x06, 0x00, 0x00, 0, 0};
  c = ot::DecodeCall(disp32, 0x005E6086);
  Check(c.form == ot::kBaseDisp32 && c.length == 6 && c.base == 0 && c.disp == 0x610,
        "FF 90 disp32 is a call through eax + 0x610", c.disp, 0x610);
  // call dword ptr [edx + 0x610]
  const uint8_t disp32_edx[8] = {0xFF, 0x92, 0x10, 0x06, 0x00, 0x00, 0, 0};
  c = ot::DecodeCall(disp32_edx, 0x005E610F);
  Check(c.form == ot::kBaseDisp32 && c.base == 2, "FF 92 disp32 is a call through edx", c.base, 2);
  // call dword ptr [esp + disp32] needs a SIB byte: not taken.
  const uint8_t sib[8] = {0xFF, 0x94, 0x24, 0x10, 0x00, 0x00, 0x00, 0};
  c = ot::DecodeCall(sib, 0x1000);
  Check(c.form == ot::kNoCall, "a call with a SIB byte is not taken", c.form, ot::kNoCall);

  // mov edx, [ecx]; call dword ptr [edx + 0x74]
  const uint8_t mov_load[8] = {0x8B, 0x11, 0xFF, 0x52, 0x74, 0, 0, 0};
  c = ot::DecodeCall(mov_load, 0x005E5E40);
  Check(c.form == ot::kMovDisp8 && c.length == 5 && c.load && c.dst == 2 && c.src == 1 &&
            c.base == 2 && c.disp == 0x74,
        "8B 11 FF 52 74 is mov edx, [ecx] and a call through edx + 0x74", c.disp, 0x74);
  // mov ecx, esi; call dword ptr [eax + 0x38]
  const uint8_t mov_reg[8] = {0x8B, 0xCE, 0xFF, 0x50, 0x38, 0, 0, 0};
  c = ot::DecodeCall(mov_reg, 0x0063B60C);
  Check(c.form == ot::kMovDisp8 && !c.load && c.dst == 1 && c.src == 6 && c.base == 0 &&
            c.disp == 0x38,
        "8B CE FF 50 38 is mov ecx, esi and a call through eax + 0x38", c.disp, 0x38);
  // mov edx, [ebp + 8] has a displacement: not taken.
  const uint8_t mov_disp[8] = {0x8B, 0x55, 0x08, 0xFF, 0x52, 0x74, 0, 0};
  c = ot::DecodeCall(mov_disp, 0x1000);
  Check(c.form == ot::kNoCall, "a mov with a displacement is not taken", c.form, ot::kNoCall);
  // A negative disp8.
  const uint8_t mov_neg[8] = {0x8B, 0x11, 0xFF, 0x52, 0xFC, 0, 0, 0};
  c = ot::DecodeCall(mov_neg, 0x1000);
  Check(c.form == ot::kMovDisp8 && c.disp == -4, "a disp8 of 0xFC is -4", c.disp, -4);
  // call dword ptr [0x008A975C]
  const uint8_t memory[8] = {0xFF, 0x15, 0x5C, 0x97, 0x8A, 0x00, 0, 0};
  c = ot::DecodeCall(memory, 0x005E61CA);
  Check(c.form == ot::kMemory && c.length == 6 && c.target == 0x008A975C,
        "FF 15 abs32 is a call through a pointer in memory", c.target, 0x008A975C);
  // push eax is no call.
  const uint8_t none[8] = {0x50, 0x8B, 0xCE, 0xFF, 0x52, 0x34, 0, 0};
  c = ot::DecodeCall(none, 0x1000);
  Check(c.form == ot::kNoCall, "push eax is no call", c.form, ot::kNoCall);

  // The bytes of the call of the stub. A call of 6 bytes gets a nop.
  uint8_t out[8];
  bool ok = ot::CallBytes(0x005E6086, 0x10000000, 6, out);
  int32_t rel;
  memcpy(&rel, out + 1, 4);
  Check(ok && out[0] == 0xE8 && out[5] == 0x90 &&
            static_cast<uint32_t>(rel) == 0x10000000u - (0x005E6086u + 5u),
        "the call of the stub returns to the nop after it", out[5], 0x90);
  Check(!ot::CallBytes(0x1000, 0x2000, 4, out), "4 bytes are too few for a call", 0, 0);

  // Target. The block of pushad: edi esi ebp esp ebx edx ecx eax.
  Memory mem;
  mem.word[0] = 0x1010;  // at 0x1000: an object with its vtable at 0x1010
  // The vtable at 0x1010: the entry +0x74 is at 0x1084, word 0x21.
  mem.word[0x21] = 0x00401234;
  uint32_t regs[11] = {};
  regs[ot::PushadIndex(1)] = 0x1000;  // ecx is the object
  regs[ot::PushadIndex(2)] = 0x5555;  // edx is something else
  c = ot::DecodeCall(mov_load, 0x005E5E40);
  uint32_t t = ot::Target(c, regs, mem);
  Check(t == 0x00401234, "mov edx, [ecx]; call [edx + 0x74] reads the vtable of ecx", t,
        0x00401234);
  Check(regs[ot::PushadIndex(2)] == 0x1010, "the mov writes edx for the function",
        regs[ot::PushadIndex(2)], 0x1010);
  // call [eax + 0x10]: eax holds 0x1000, the word at 0x1010.
  const uint8_t disp_small[8] = {0xFF, 0x90, 0x10, 0x00, 0x00, 0x00, 0, 0};
  c = ot::DecodeCall(disp_small, 0x2000);
  regs[ot::PushadIndex(0)] = 0x1000;
  mem.word[4] = 0x00405678;
  t = ot::Target(c, regs, mem);
  Check(t == 0x00405678, "call [eax + 0x10] reads the word at eax + 0x10", t, 0x00405678);
  // mov ecx, esi; call [eax + 0x38]: ecx gets esi, the word at eax + 0x38.
  regs[ot::PushadIndex(6)] = 0x7777;
  mem.word[0x38 / 4] = 0x00409999;
  c = ot::DecodeCall(mov_reg, 0x0063B60C);
  t = ot::Target(c, regs, mem);
  Check(t == 0x00409999 && regs[ot::PushadIndex(1)] == 0x7777,
        "mov ecx, esi gives ecx the value of esi", regs[ot::PushadIndex(1)], 0x7777);

  // The table of the keys.
  static ot::KeyTable keys;
  keys = ot::KeyTable();
  Check(keys.Find(0x008C1234) == -1, "an empty table has no key", keys.Find(0x008C1234), -1);
  Check(!keys.Add(0, 5), "the key 0 is not taken", 0, 0);
  bool all = true;
  for (int i = 0; i < 300; ++i) all = all && keys.Add(0x008C0000u + 0x40u * i, i + 1);
  int found = 0;
  for (int i = 0; i < 300; ++i) found += keys.Find(0x008C0000u + 0x40u * i) == i + 1 ? 1 : 0;
  Check(all && found == 300, "300 vtables 0x40 bytes apart each find their timer", found, 300);
  keys.Add(0x008C0040u, 77);
  Check(keys.Find(0x008C0040u) == 77, "a key that is there gets its new timer",
        keys.Find(0x008C0040u), 77);
  int filled = 300;
  for (int i = 300; i < 2000 && keys.Add(0x00900000u + 4u * i, 1); ++i) ++filled;
  Check(filled == ot::kKeySlots, "the table takes keys until each slot is full", filled,
        ot::kKeySlots);
  Check(keys.Find(0x00A00000u) == -1, "a full table finds no new key", keys.Find(0x00A00000u),
        -1);
}

}  // namespace object_timer_checks
