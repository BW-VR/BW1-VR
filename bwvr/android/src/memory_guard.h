// THE LIMIT OF THE MEMORY (memory_guard of bwvr.ini, docs/android.md, "The limit of the memory").
// A render that is too large for the memory of the headset stops each thread for seconds, also the
// threads of the system, and then Android ends the app. So at each Play the app takes the largest
// scale up to resolution_pct whose memory fits, and bwvr.ini keeps the value of the player. It has
// no Android code, so the math test of the PC checks it (test/controller_math/remote_checks.h).
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>

namespace memory_guard {

// The memory of a game: a part that does not depend on the render, and the images of the size of
// the render. The numbers come from the fall of MemAvailable in runs on a Quest 3 at 100 and at 150
// percent.
struct Cost {
  int base_mb;
  int images;
};
constexpr Cost kComposeAtTake = {1650, 27};  // xr_compose_take=1 with one queue
constexpr Cost kEyesInRing = {3800, 29};     // the eye pictures go through the slots of the ring

// The memory that stays free for the system and for a peak of the game.
constexpr int kKeepFreeMb = 500;
// The limit does not go below this scale: a game at 100 percent has run with the least memory.
constexpr int kLowestPct = 100;
// The step of the row "Resolution scale".
constexpr int kStepPct = 5;

// MemAvailable of the text of /proc/meminfo in MB, or -1 with no such line.
inline int AvailableMb(const std::string& meminfo) {
  const char* const name = "MemAvailable:";
  const size_t at = meminfo.find(name);
  if (at == std::string::npos) return -1;
  const long kb = strtol(meminfo.c_str() + at + strlen(name), nullptr, 10);
  return kb > 0 ? static_cast<int>(kb / 1024) : -1;
}

// The memory of a game with a render of this size, in MB.
inline int NeedMb(const Cost& cost, int width, int height) {
  const int64_t image = static_cast<int64_t>(width) * height * 4;
  return cost.base_mb + static_cast<int>(image * cost.images / (1024 * 1024));
}

inline bool Fits(const Cost& cost, int width, int height, int available_mb) {
  return NeedMb(cost, width, height) + kKeepFreeMb <= available_mb;
}

// The scale that the render takes. size(percent, &width, &height) gives the render of a scale. A
// scale of 100 percent or less stays, and so does each scale when the free memory is not known.
template <typename Size>
inline int LimitPct(int wanted, const Cost& cost, int available_mb, Size size) {
  if (available_mb < 0 || wanted <= kLowestPct) return wanted;
  for (int percent = wanted; percent > kLowestPct; percent -= kStepPct) {
    int width = 0, height = 0;
    size(percent, &width, &height);
    if (Fits(cost, width, height, available_mb)) return percent;
  }
  return kLowestPct;
}

}  // namespace memory_guard
