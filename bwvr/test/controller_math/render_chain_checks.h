// The way of a draw from the game to the headset app, a test
// (render_chain_math.h). main.cpp includes this file after Check, just
// before main.
#pragma once

#include "../../src/mod/render_chain_math.h"

namespace render_chain_checks {

inline void RenderChainChecks() {
  using namespace render_chain_math;
  printf("\nThe way of a draw from the game to the headset app\n");

  // THE CALLS THAT CHANGE NOTHING.
  Check(SkipRenderState(5, 5), "a render state with the value of the device changes nothing", 1, 1);
  Check(!SkipRenderState(5, 6), "a render state with another value goes", 0, 0);

  uint32_t ts[32] = {};
  ts[1] = 4;  // D3DTSS_COLOROP
  Check(SkipStageState(ts, 32, 1, 4), "a stage state with the value changes nothing", 1, 1);
  Check(!SkipStageState(ts, 32, 1, 2), "a stage state with another value goes", 0, 0);
  Check(!SkipStageState(ts, 32, 32, 0), "a stage state outside the table goes", 0, 0);
  Check(!SkipStageState(nullptr, 32, 1, 4), "with no table each stage state goes", 0, 0);
  ts[kAddress] = 1;
  ts[kAddressU] = 1;
  ts[kAddressV] = 3;
  Check(!SkipStageState(ts, 32, kAddress, 1),
        "D3DTSS_ADDRESS goes when the address mode of v has another value", 0, 0);
  ts[kAddressV] = 1;
  Check(SkipStageState(ts, 32, kAddress, 1),
        "D3DTSS_ADDRESS changes nothing when u and v have the value too", 1, 1);
  ts[kAddressU] = 3;
  Check(!SkipStageState(ts, 32, kAddress, 1),
        "D3DTSS_ADDRESS goes when the address mode of u has another value", 0, 0);
  Check(!SkipStageState(ts, 13, kAddress, 1),
        "D3DTSS_ADDRESS goes when the table has no address mode of v", 0, 0);
  ts[kAddressU] = 1;
  Check(SkipStageState(ts, 32, kAddressU, 1),
        "D3DTSS_ADDRESSU alone follows the plain rule", 1, 1);

  int a = 0, b = 0;
  Check(SkipTexture(&a, &a, 7, 7), "the same texture with no write changes nothing", 1, 1);
  Check(!SkipTexture(&a, &a, 8, 7), "the same texture after a write goes", 0, 0);
  Check(!SkipTexture(&a, &b, 7, 7), "another texture goes", 0, 0);
  Check(SkipTexture(nullptr, nullptr, 7, 7), "no texture again changes nothing", 1, 1);
  Check(!SkipTexture(&a, nullptr, 7, 7), "no texture after a texture goes", 0, 0);
  Check(!SkipTexture(&a, &a, 0, 0xFFFFFFFFu),
        "the count of the writes that wraps past 0 is a write", 0, 0);

  // THE TABLE OF THE SURFACES.
  SurfaceCache cache;
  const uint64_t s1 = 0x01230010, s2 = 0x01230020;
  Check(!cache.Has(s1), "an empty table has no surface", 0, 0);
  cache.Put(s1);
  Check(cache.Has(s1), "a surface that the table got is found", 1, 1);
  Check(!cache.Has(s2), "another surface is not found", 0, 0);
  cache.Put(s2);
  Check(cache.Has(s1) && cache.Has(s2), "two surfaces are found",
        cache.Has(s1) + cache.Has(s2), 2);
  cache.Drop(s1);
  Check(!cache.Has(s1) && cache.Has(s2),
        "a write of a surface takes it out, and only it", cache.Has(s2), 1);
  cache.Put(0);
  Check(!cache.Has(0), "the id 0 is never in the table", 0, 0);
  cache.Clear();
  Check(!cache.Has(s2), "a frame with the whole state clears the table", 0, 0);
  // Two ids with the same place: the second takes the place, and the first
  // is not found, so the sets of the stream answer for it.
  uint64_t other = 0;
  for (uint64_t id = s1 + 16; id < s1 + 16 * 100000; id += 16) {
    if (SurfaceCache::Place(id) == SurfaceCache::Place(s1)) {
      other = id;
      break;
    }
  }
  Check(other != 0, "two ids of the heap can have the same place", other != 0, 1);
  cache.Put(s1);
  cache.Put(other);
  Check(!cache.Has(s1) && cache.Has(other),
        "the id of the last put has the place, the first one is not found",
        cache.Has(other), 1);
  cache.Drop(s1);
  Check(cache.Has(other), "a drop of an id that is not there keeps the place", 1, 1);
  // The places of 512 surfaces of the heap 64 bytes apart.
  int used[SurfaceCache::kPlaces] = {};
  int places = 0;
  for (uint64_t i = 0; i < SurfaceCache::kPlaces; ++i) {
    const uint32_t p = SurfaceCache::Place(0x00A00000 + i * 64);
    if (used[p]++ == 0) ++places;
  }
  Check(places > 300, "512 surfaces 64 bytes apart use more than 300 places",
        places, 300);

  // THE TIMES.
  Check(Elapsed(100, 250) == 150, "the ticks from a start to now",
        static_cast<double>(Elapsed(100, 250)), 150);
  Check(Elapsed(250, 100) == 0,
        "a clock that is behind the start of another core gives 0 ticks",
        static_cast<double>(Elapsed(250, 100)), 0);
  Check(Own(10, 4) == 6, "the own time is the time less the inner step",
        static_cast<double>(Own(10, 4)), 6);
  Check(Own(4, 10) == 0, "the own time is never below 0",
        static_cast<double>(Own(4, 10)), 0);
  Check(fabs(MsPerFrame(3000000, 1000.0, 3) - 1.0) < 1e-9,
        "3 million ticks of 1000 MHz in 3 frames are 1 ms a frame",
        MsPerFrame(3000000, 1000.0, 3), 1.0);
  Check(MsPerFrame(5, 0.0, 3) == 0.0 && MsPerFrame(5, 10.0, 0) == 0.0,
        "no rate or no frame gives 0", 0, 0);
  Check(fabs(ClockMhz(2400000000ull, 10000000, 10000000) - 2400.0) < 1e-6,
        "2.4 billion ticks in 1 s are 2400 MHz",
        ClockMhz(2400000000ull, 10000000, 10000000), 2400.0);
  Check(ClockMhz(5, 0, 10) == 0.0, "no time of the counter gives no rate", 0, 0);
  Check(fabs(Percent(1.5, 12.0) - 12.5) < 1e-9, "1.5 of 12 is 12.5 percent",
        Percent(1.5, 12.0), 12.5);
  Check(Percent(1.0, 0.0) == 0.0, "a share of nothing is 0", 0, 0);
  Window w;
  w.Clear();
  w.ticks[kStream][kDraw] = 5;
  w.ticks[kStream][kTexture] = 7;
  w.ticks[kDevice][kDraw] = 100;
  Check(w.StepTicks(kStream) == 12, "the ticks of a step are the sum of its kinds",
        static_cast<double>(w.StepTicks(kStream)), 12);
  w.Clear();
  Check(w.StepTicks(kDevice) == 0 && w.handover_in_device == 0,
        "a new window has no ticks", 0, 0);
}

}  // namespace render_chain_checks
