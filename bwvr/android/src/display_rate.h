// THE DISPLAY RATE OF THE HEADSET (display_hz, docs/android.md, "The display
// rate"). It has no Android code, so the math test of the PC checks it
// (test/controller_math/display_rate_checks.h).
//
// A runtime with XR_FB_display_refresh_rate lists its rates
// (xrEnumerateDisplayRefreshRatesFB), and the app can ask for one of them
// (xrRequestDisplayRefreshRateFB). The menu offers each rate of kOffered that
// the runtime lists. display_hz=0 (the default) asks for no rate, and the
// headset uses its own rate (72 Hz on a Quest 3).
#pragma once

#include <math.h>

#include <vector>

namespace display_rate {

// The rates that the menu can offer, in Hz.
constexpr int kOffered[] = {72, 80, 90, 120};
constexpr int kOfferedCount = static_cast<int>(sizeof(kOffered) / sizeof(kOffered[0]));
// A rate of the runtime is a float, for example 72.0 or 89.9: it counts as
// a rate of kOffered within this many Hz.
constexpr float kMatchHz = 0.5f;

// The rate of the runtime for `hz`, or -1 when the runtime does not list it.
inline int IndexOf(int hz, const std::vector<float>& runtime) {
  for (size_t i = 0; i < runtime.size(); ++i) {
    if (fabsf(runtime[i] - static_cast<float>(hz)) <= kMatchHz) return static_cast<int>(i);
  }
  return -1;
}

// The rates that the menu offers: each rate of kOffered that the runtime
// lists, in the order of kOffered. Empty when the runtime lists none of them
// (or has no XR_FB_display_refresh_rate): then the menu has no row.
inline std::vector<int> Offered(const std::vector<float>& runtime) {
  std::vector<int> out;
  for (int i = 0; i < kOfferedCount; ++i) {
    if (IndexOf(kOffered[i], runtime) >= 0) out.push_back(kOffered[i]);
  }
  return out;
}

// The value of display_hz that the app uses: 0, or a rate of `offered`. Each
// other value is 0 (the app asks for no rate).
inline int Checked(int wanted, const std::vector<int>& offered) {
  for (int hz : offered) {
    if (hz == wanted) return hz;
  }
  return 0;
}

// The float that the app asks the runtime for: the rate of the runtime list
// for `hz`. 0 for 0 or for a rate that the runtime does not list (0 asks for
// no rate, XR_FB_display_refresh_rate: "no preference").
inline float RuntimeRate(int hz, const std::vector<float>& runtime) {
  if (hz <= 0) return 0.0f;
  const int i = IndexOf(hz, runtime);
  return i >= 0 ? runtime[i] : 0.0f;
}

// The next value of the row in the menu: 0, then each rate of `offered`. A
// step goes round. A value that is not in the list counts as 0.
inline int Step(int now, int direction, const std::vector<int>& offered) {
  const int count = static_cast<int>(offered.size()) + 1;
  int index = 0;
  for (int i = 0; i < static_cast<int>(offered.size()); ++i) {
    if (offered[i] == now) index = i + 1;
  }
  index = ((index + direction) % count + count) % count;
  return index == 0 ? 0 : offered[index - 1];
}

}  // namespace display_rate
