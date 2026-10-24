// THE PACE OF THE FRAMES OF THE GAME WITH NO VULKAN DEVICE OF THE MOD
// (vk_pace, docs/android.md, "The pace of the frames"). A pure header: the
// math test checks it (test/controller_math/frame_pace_checks.h).
//
// WHY. With vk_device=0 the mod waits for no GPU and for no present, so the
// game makes frames as fast as the CPU can. The headset app shows at most one
// frame of the game for each frame of its session, and it writes one slot of
// the input channel for each frame of its session (src/common/input_channel.h).
// So with vk_pace=1 a frame of the game waits for the next write of the slot.
//
// THE CLOCK. The mod does not compare the display time of a slot with its
// own clock. The display time is an XrTime of the runtime of the headset, in
// ns. Wine gives QueryPerformanceCounter from CLOCK_BOOTTIME
// (dlls/ntdll/unix/sync.c, monotonic_counter), and a headset that slept has
// a CLOCK_BOOTTIME that is ahead of CLOCK_MONOTONIC by the time of the sleep.
// The test writer of the PC writes the display times of a record. So the
// mod paces by the count of the writes of the slot (Slot::seq, two more for
// each write) and takes only the display period from the slot. The times
// below are of the clock of the mod alone, in ns.
//
// THE RULE. A wait ends at the first write of the slot that no frame took
// yet (a new frame of the session), or at its time limit. The limit counts
// from the end of the wait before:
// - a fresh slot (a write in the last kStalePeriods periods, with a display
//   period from kMinPeriodNs to kMaxPeriodNs): 5/4 of the period. A frame of
//   the session that comes a little late still ends the wait, and a frame
//   that the session does not make costs the game at most one period.
// - no slot yet, a slot that does not change, or a period of 0: the period
//   of kIdleHz. Then no frame of the session shows the frames of the game,
//   but the game must go on (its logic, its sound, the stale rule of the
//   channel, the next start of the session). One period of the lowest rate
//   of a Quest 3 keeps the frames at that rate, and the next write of the
//   app ends the wait at once.
// A frame that takes longer than the limit does not wait.
#pragma once

#include <stdint.h>

namespace frame_pace_math {

constexpr int64_t kIdleHz = 72;  // the lowest display rate of a Quest 3
constexpr int64_t kIdlePeriodNs = 1000000000 / kIdleHz;
constexpr int64_t kMinPeriodNs = 1000000;    // 1000 Hz
constexpr int64_t kMaxPeriodNs = 100000000;  // 10 Hz
constexpr int64_t kStalePeriods = 4;
// The steps of the wait: a look at the sequence of the slot after each step.
// The step before a write that the period gives is longer (EarlyNs).
constexpr int64_t kPollNs = 500000;
constexpr int64_t kIdlePollNs = 1000000;
constexpr int64_t kEarlyNs = 1000000;

inline bool UsablePeriod(int64_t period_ns) {
  return period_ns >= kMinPeriodNs && period_ns <= kMaxPeriodNs;
}

// The state of the pace. The mod has one.
struct State {
  bool have_write = false;  // the app wrote a slot since the mod looked first
  uint32_t seq = 0;         // the sequence of the newest write that the mod saw (even)
  int64_t seen_ns = 0;      // the clock of the mod when it saw that write first
  int64_t period_ns = 0;    // the display period of the newest whole slot that it read
  bool have_taken = false;
  uint32_t taken = 0;       // the sequence of the newest write that a frame took
  bool have_end = false;
  int64_t end_ns = 0;       // the clock of the mod at the end of the last wait
};

// A sequence while the writer writes is odd: it counts as the write that
// ends it. 0: the app wrote no slot yet.
inline uint32_t WriteOf(uint32_t seq) { return (seq + 1u) & ~1u; }

// A look at the sequence of the slot at the clock now. Gives true for a write
// that the mod did not see before.
inline bool Observe(State* s, uint32_t seq, int64_t now_ns) {
  const uint32_t w = WriteOf(seq);
  if (w == 0) return false;
  if (s->have_write && w == s->seq) return false;
  s->have_write = true;
  s->seq = w;
  s->seen_ns = now_ns;
  return true;
}

// The display period of a whole slot.
inline void SetPeriod(State* s, int64_t period_ns) { s->period_ns = period_ns; }

// A frame took the write with this sequence (the latch of the channel).
inline void Took(State* s, uint32_t seq, int64_t now_ns) {
  if (WriteOf(seq) == 0) return;
  Observe(s, seq, now_ns);
  s->have_taken = true;
  s->taken = WriteOf(seq);
}

// True when the newest write paces the game: it has a usable period and it
// is not older than kStalePeriods periods.
inline bool Fresh(const State& s, int64_t now_ns) {
  return s.have_write && UsablePeriod(s.period_ns) &&
         now_ns - s.seen_ns <= kStalePeriods * s.period_ns;
}

// The longest time from the end of the last wait to the end of this one.
inline int64_t Limit(const State& s, int64_t now_ns) {
  return Fresh(s, now_ns) ? s.period_ns + s.period_ns / 4 : kIdlePeriodNs;
}

// The time limit of a wait that starts at start_ns.
inline int64_t Deadline(const State& s, int64_t start_ns) {
  return (s.have_end ? s.end_ns : start_ns) + Limit(s, start_ns);
}

// A write that no frame took yet.
inline bool NewWrite(const State& s) {
  return s.have_write && (!s.have_taken || s.seq != s.taken);
}

enum class Step {
  kGo,      // a new write: the frame starts
  kTimeUp,  // the time limit: the frame starts with no new write
  kWait,
};

inline Step Next(const State& s, int64_t deadline_ns, int64_t now_ns) {
  if (NewWrite(s)) return Step::kGo;
  if (now_ns >= deadline_ns) return Step::kTimeUp;
  return Step::kWait;
}

// The time to sleep before the next look, never past the time limit. With a
// fresh slot one sleep goes to kEarlyNs before the write that the period
// gives, and then the steps are kPollNs. Else the steps are kIdlePollNs.
inline int64_t SleepNs(const State& s, int64_t deadline_ns, int64_t now_ns) {
  const int64_t left = deadline_ns - now_ns;
  if (left <= 0) return 0;
  int64_t step = kIdlePollNs;
  if (Fresh(s, now_ns)) {
    const int64_t early = s.seen_ns + s.period_ns - kEarlyNs - now_ns;
    step = early > kPollNs ? early : kPollNs;
  }
  return step < left ? step : left;
}

// The end of a wait: the frame takes the newest write.
inline void End(State* s, int64_t now_ns) {
  s->have_end = true;
  s->end_ns = now_ns;
  if (s->have_write) {
    s->have_taken = true;
    s->taken = s->seq;
  }
}

// THE READS OF THE INPUT CHANNEL with the pace. The first read of the input
// channel in a frame of the game latches the input of that frame. With the
// pace on, a read before the wait of its frame (the intro skip in
// FinishFrame) reads only the newest slot: it does not ask the writer for
// the slot of a frame of the controllers, it takes no write for the pace,
// and the first read after the wait latches the input of the frame. Else
// each read is a read of the frame.
enum class Read {
  kFrame,       // the read of the frame: the latch, as with no pace
  kBeforeWait,  // a read of the newest slot before the wait
};

inline Read ReadKind(bool pace_on, bool wait_due) {
  return pace_on && wait_due ? Read::kBeforeWait : Read::kFrame;
}

// The clock of the mod in ns from a count of QueryPerformanceCounter, with
// no overflow for a count of many days.
inline int64_t CountToNs(int64_t count, int64_t frequency) {
  if (frequency <= 0) return 0;
  return (count / frequency) * 1000000000 +
         (count % frequency) * 1000000000 / frequency;
}

// The ticks of QueryPerformanceCounter for a time in ns.
inline int64_t NsToCount(int64_t ns, int64_t frequency) {
  return ns / 1000000000 * frequency + (ns % 1000000000) * frequency / 1000000000;
}

}  // namespace frame_pace_math
