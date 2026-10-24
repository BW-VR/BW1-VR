// The pace of the frames of the game with no Vulkan device of the mod, a
// test (src/mod/frame_pace_math.h). main.cpp includes this file after Check
// and Near, just before main.
#pragma once

#include <stdint.h>

#include "../../src/mod/frame_pace_math.h"

namespace frame_pace_checks {

namespace fp = frame_pace_math;

constexpr int64_t kMs = 1000000;
constexpr int64_t kPeriod = 13888889;  // 72 Hz, as the slot of a Quest 3 has it

// A wait as the mod makes it (xr_input.cpp, XrInputPace): the writes of the
// app come at the times in writes_ns (each two more in the sequence), the
// mod looks after each sleep. Gives the time at the end of the wait.
struct Run {
  fp::State s;
  uint32_t seq = 0;  // the sequence of the slot now
  int sleeps = 0;
  fp::Step step = fp::Step::kWait;
};

inline int64_t Wait(Run* r, int64_t start, int64_t* writes, int count,
                    int64_t period) {
  int64_t now = start;
  auto look = [&](int64_t t) {
    for (int i = 0; i < count; ++i) {
      if (writes[i] >= 0 && writes[i] <= t) {
        r->seq += 2;
        writes[i] = -1;  // a write is seen one time
      }
    }
    if (fp::Observe(&r->s, r->seq, t)) fp::SetPeriod(&r->s, period);
  };
  look(now);
  const int64_t deadline = fp::Deadline(r->s, now);
  r->sleeps = 0;
  while ((r->step = fp::Next(r->s, deadline, now)) == fp::Step::kWait) {
    const int64_t ns = fp::SleepNs(r->s, deadline, now);
    now += ns > 0 ? ns : 1;
    ++r->sleeps;
    look(now);
  }
  fp::End(&r->s, now);
  return now;
}

inline void FramePaceChecks() {
  printf("\nThe pace of the frames with no Vulkan device of the mod\n");

  // THE PERIOD.
  Check(fp::kIdlePeriodNs == 13888888, "the period with no slot is 72 Hz",
        static_cast<double>(fp::kIdlePeriodNs), 13888888.0);
  Check(!fp::UsablePeriod(0) && !fp::UsablePeriod(-kPeriod) &&
            !fp::UsablePeriod(500000) && !fp::UsablePeriod(200 * kMs),
        "a period of 0, below 1 ms or above 100 ms does not pace", 0, 0);
  Check(fp::UsablePeriod(kPeriod) && fp::UsablePeriod(8333333),
        "a period of 72 Hz and of 120 Hz paces", 0, 0);

  // THE SEQUENCE. A write in progress counts as the write that ends it.
  Check(fp::WriteOf(0) == 0 && fp::WriteOf(1) == 2 && fp::WriteOf(2) == 2 &&
            fp::WriteOf(0xFFFFFFFFu) == 0,
        "the write of a sequence: 0 none, 1 and 2 the first, a wrap to 0", 0, 0);

  // NO SLOT YET: the limit is one period of 72 Hz from the start.
  {
    Run r;
    const int64_t end = Wait(&r, 1000 * kMs, nullptr, 0, 0);
    Check(r.step == fp::Step::kTimeUp && end == 1000 * kMs + fp::kIdlePeriodNs,
          "no slot yet: the wait ends at one period of 72 Hz",
          static_cast<double>(end - 1000 * kMs), static_cast<double>(fp::kIdlePeriodNs));
    Check(!r.s.have_write && !fp::NewWrite(r.s), "no slot yet: no write to take", 0, 0);
    // The next wait counts from the end of this one, so the frames come at
    // 72 Hz with a frame of 3 ms in between.
    const int64_t end2 = Wait(&r, end + 3 * kMs, nullptr, 0, 0);
    Check(end2 == end + fp::kIdlePeriodNs,
          "no slot yet: the next wait ends one period after the end of the wait before",
          static_cast<double>(end2 - end), static_cast<double>(fp::kIdlePeriodNs));
    // A frame longer than the period does not wait.
    const int64_t end3 = Wait(&r, end2 + 20 * kMs, nullptr, 0, 0);
    Check(end3 == end2 + 20 * kMs && r.sleeps == 0,
          "a frame longer than the limit does not wait", static_cast<double>(r.sleeps), 0);
  }

  // A FRESH SLOT: the wait ends at the next write of the app.
  {
    Run r;
    int64_t first[] = {0};
    // The app wrote at 0, the mod starts at 1 ms: a write that no frame took.
    int64_t end = Wait(&r, 1 * kMs, first, 1, kPeriod);
    Check(r.step == fp::Step::kGo && end == 1 * kMs && r.sleeps == 0,
          "a write that no frame took: no wait", static_cast<double>(r.sleeps), 0);
    // The frame took 4 ms. The next write comes at one period.
    int64_t next[] = {kPeriod};
    end = Wait(&r, end + 4 * kMs, next, 1, kPeriod);
    Check(r.step == fp::Step::kGo && end >= kPeriod && end <= kPeriod + fp::kPollNs,
          "a fresh slot: the wait ends at the next write (within one step)",
          static_cast<double>(end - kPeriod), 0);
    // One long sleep to 1 ms before the write, then steps of 0.5 ms.
    Check(r.sleeps <= 4, "a fresh slot: few looks at the sequence in a wait",
          static_cast<double>(r.sleeps), 4);
    // The latch of the frame read the same write: the next frame waits for
    // the write after it.
    fp::Took(&r.s, r.seq, end);
    Check(!fp::NewWrite(r.s), "the write that the latch took is no new write", 0, 0);
    // The app misses a frame: no write at 2 periods. The wait ends at 5/4 of
    // a period after the end of the wait before.
    const int64_t before = end;
    end = Wait(&r, before + 4 * kMs, nullptr, 0, kPeriod);
    Check(r.step == fp::Step::kTimeUp && end == before + kPeriod + kPeriod / 4,
          "a frame that the app missed: the wait ends at 5/4 of a period",
          static_cast<double>(end - before), static_cast<double>(kPeriod + kPeriod / 4));
    // The write after the missed frame (3 periods after the first) ends the
    // next wait.
    int64_t late[] = {3 * kPeriod};
    end = Wait(&r, end + 4 * kMs, late, 1, kPeriod);
    Check(r.step == fp::Step::kGo && end >= 3 * kPeriod && end <= 3 * kPeriod + fp::kPollNs,
          "the write after a missed frame ends the wait", static_cast<double>(end - 3 * kPeriod), 0);
  }

  // A SLOT THAT DOES NOT CHANGE: after kStalePeriods periods with no write
  // the frames go at 72 Hz.
  {
    Run r;
    int64_t first[] = {0};
    Wait(&r, 0, first, 1, 8333333);  // 120 Hz
    const int64_t at = 0 + 5 * 8333333;
    Check(!fp::Fresh(r.s, at) && fp::Fresh(r.s, 4 * 8333333),
          "a slot is fresh for 4 periods, not for 5", 0, 0);
    r.s.end_ns = at;
    const int64_t end = Wait(&r, at, nullptr, 0, 8333333);
    Check(r.step == fp::Step::kTimeUp && end == at + fp::kIdlePeriodNs,
          "a slot that does not change: one period of 72 Hz, not of the slot",
          static_cast<double>(end - at), static_cast<double>(fp::kIdlePeriodNs));
  }

  // A PERIOD OF 0 (the record of a fake run has none): the limit of 72 Hz,
  // and a write still ends the wait.
  {
    Run r;
    int64_t first[] = {0};
    int64_t end = Wait(&r, 0, first, 1, 0);
    Check(!fp::Fresh(r.s, end), "a period of 0 is not fresh", 0, 0);
    int64_t next[] = {11 * kMs};  // a writer at 90 Hz
    end = Wait(&r, end + 2 * kMs, next, 1, 0);
    Check(r.step == fp::Step::kGo && end >= 11 * kMs && end <= 11 * kMs + fp::kIdlePollNs,
          "a period of 0: a write ends the wait (within one step of 1 ms)",
          static_cast<double>(end - 11 * kMs), 0);
    end = Wait(&r, end + 2 * kMs, nullptr, 0, 0);
    Check(r.step == fp::Step::kTimeUp && r.s.end_ns == end,
          "a period of 0 and no write: the wait ends at the limit of 72 Hz", 0, 0);
  }

  // THE LOCKSTEP OF A TEST WRITER: the writer answers the latch of a frame at
  // once and writes again at its rate. The answer is no new write for the
  // next frame.
  {
    Run r;
    int64_t tick[] = {0};
    int64_t end = Wait(&r, 0, tick, 1, 0);
    r.seq += 2;  // the answer to the latch
    fp::Took(&r.s, r.seq, end + 100000);
    Check(!fp::NewWrite(r.s) && r.s.taken == r.seq,
          "the answer of the writer that the latch took is no new write",
          static_cast<double>(r.s.taken), static_cast<double>(r.seq));
    int64_t tick2[] = {11 * kMs};
    end = Wait(&r, 3 * kMs, tick2, 1, 0);
    Check(r.step == fp::Step::kGo && end >= 11 * kMs, "the next write of the writer ends the wait",
          static_cast<double>(end), static_cast<double>(11 * kMs));
  }

  // THE READS OF THE INPUT CHANNEL. A frame of the intro or of the scene
  // after a frame with a read of the intro skip in FinishFrame: the read
  // before the wait does not use up the read of the frame.
  Check(fp::ReadKind(false, true) == fp::Read::kFrame &&
            fp::ReadKind(false, false) == fp::Read::kFrame,
        "no pace: each read is the read of the frame", 0, 0);
  Check(fp::ReadKind(true, true) == fp::Read::kBeforeWait,
        "the pace on, a read before the wait reads only the newest slot", 0, 0);
  Check(fp::ReadKind(true, false) == fp::Read::kFrame,
        "the pace on, the first read after the wait is the read of the frame", 0, 0);
  {
    // FinishFrame (the wait is due), the intro skip reads, then the wait,
    // then the read of the intro: as in xr_input.cpp, a read of the frame
    // uses up the latch of the frame, a read before the wait does not.
    bool new_frame = true;  // XrInputNewFrame
    bool due = true;
    int frame_reads = 0, early_reads = 0;
    auto read = [&]() {
      if (!new_frame) return;
      if (fp::ReadKind(true, due) == fp::Read::kBeforeWait) {
        ++early_reads;
        return;
      }
      new_frame = false;
      ++frame_reads;
    };
    read();       // the intro skip in FinishFrame
    due = false;  // the wait (XrInputPace)
    read();       // the intro (intro_vr.cpp) after the wait
    read();       // a later read of the same frame gets the same values
    Check(early_reads == 1 && frame_reads == 1 && !new_frame,
          "a read before the wait and one read of the frame after the wait",
          frame_reads, 1);
  }

  // THE CLOCK of the mod.
  Check(fp::CountToNs(10000000, 10000000) == 1000000000 &&
            fp::CountToNs(15, 10) == 1500000000 && fp::CountToNs(5, 0) == 0,
        "a count of the performance counter in ns", 0, 0);
  const int64_t days = 10000000LL * 86400 * 400;  // 400 days at 10 MHz
  Check(fp::CountToNs(days, 10000000) == 86400LL * 400 * 1000000000,
        "400 days of a counter of 10 MHz in ns, with no overflow", 0, 0);
  Check(fp::NsToCount(kPeriod, 10000000) == 138888 &&
            fp::NsToCount(2500000000LL, 10000000) == 25000000,
        "a time in ns as a count of the performance counter", 0, 0);
}

}  // namespace frame_pace_checks
