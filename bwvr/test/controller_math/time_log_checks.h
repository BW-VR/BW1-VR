// The time log, a test: the cap of the size of the eye pictures of the
// headset app (android/src/eye_size.h), the lines of /proc of the time log of
// the app (android/src/proc_stat.h), and the parts of a frame of the game of
// the time log of the mod (src/mod/time_log_math.h). main.cpp includes this
// file after Check and Near, just before main.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../../android/src/eye_size.h"
#include "../../android/src/proc_stat.h"
#include "../../src/common/settings_table.h"
#include "../../src/mod/time_log_math.h"

namespace time_log_checks {

inline void EyeSizeChecks() {
  namespace es = eye_size;
  printf("\nThe size of the eye pictures of the headset app (eye_size_max)\n");
  // The size that a Quest 3 asked for with the profile of the player.
  es::Size s = es::FromKey(1, 4096, 4291);
  Check(s.capped && s.width == 2064 && s.height == 2162,
        "4096 by 4291 with eye_size_max=1: 2064 by 2162, the width of the panel and "
        "the aspect of the runtime",
        s.height, 2162);
  Near("the aspect stays within one pixel", static_cast<double>(s.width) / s.height,
       4096.0 / 4291.0, 0.5 / 2162.0);
  s = es::FromKey(0, 4096, 4291);
  Check(!s.capped && s.width == 4096 && s.height == 4291,
        "eye_size_max=0: no cap, the size of the runtime", s.width, 4096);
  s = es::FromKey(1, 1680, 1760);
  Check(!s.capped && s.width == 1680 && s.height == 1760,
        "1680 by 1760 (the first start of the app) fits: it stays", s.width, 1680);
  s = es::FromKey(1, 2064, 2208);
  Check(!s.capped && s.width == 2064 && s.height == 2208,
        "the size of the panel itself stays (the test runtime of the emulator)", s.width, 2064);
  s = es::FromKey(1, 2208, 2208);
  Check(s.capped && s.width == 2064 && s.height == 2064,
        "a square of 2208 is too wide: the width of the panel sets the scale", s.height, 2064);
  s = es::FromKey(1, 2000, 3000);
  Check(s.capped && s.height == 2208 && s.width == 1472,
        "2000 by 3000 is too high: the height of the panel sets the scale", s.width, 1472);
  s = es::FromKey(1, 8192, 8192);
  Check(s.width == 2064 && s.height == 2064, "8192 by 8192: 2064 by 2064", s.width, 2064);
  s = es::Cap(5000, 1, es::kPanelWidth, es::kPanelHeight);
  Check(s.width == 2064 && s.height == 1, "a thin size keeps one pixel of height", s.height, 1);
  s = es::FromKey(1, 0, 0);
  Check(!s.capped && s.width == 0 && s.height == 0, "no size of the runtime: no size", 0, 0);
  s = es::FromKey(7, 4096, 4291);
  Check(s.capped && s.width == 2064, "a value of the key other than 0 is the cap", s.width,
        2064);
  // The render of the game from the size after the cap (ScaleToPixels of
  // the menu): the side is the larger side, so resolution_pct stays a
  // percentage of the size after the cap.
  int w = 0, h = 0;
  bwvr_settings::ScaleToPixels(100, 2064, 2162, 0, true, 25, &w, &h);
  Check(w == 2702 && h == 2162, "the render at 100 percent of 2064 by 2162 with one walk: 2702 "
        "by 2162", w, 2702);
  bwvr_settings::ScaleToPixels(100, 4096, 4291, 0, true, 25, &w, &h);
  Check(w == 4096 && h == 4096, "the render at 100 percent of 4096 by 4291 (no cap): 4096 by "
        "4096, the most of the game", w, 4096);
  bwvr_settings::ScaleToPixels(50, 2064, 2162, 0, true, 25, &w, &h);
  Check(w == 1350 && h == 1080, "the render at 50 percent of 2064 by 2162: 1350 by 1080", w,
        1350);
}

inline void ProcStatChecks() {
  namespace ps = proc_stat;
  printf("\nThe lines of /proc of the time log of the app\n");
  // A line of /proc/<pid>/task/<tid>/stat of Android 14 (52 fields).
  const std::string line =
      "13314 (runblack.exe) R 1 13300 13300 0 -1 4194560 81234 0 12 0 94200 3412 0 0 20 0 "
      "14 0 2822900 1234567890 45678 18446744073709551615 1 1 0 0 0 0 0 4096 1260 0 0 0 "
      "17 5 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
  ps::Task t;
  const bool first = ps::ParseTask(line, &t);
  Check(first, "a line of stat parses", t.id, 13314);
  Check(t.id == 13314 && t.name == "runblack.exe" && t.state == 'R',
        "the id, the name and the state", t.id, 13314);
  Check(t.user == 94200 && t.system == 3412, "utime (field 14) and stime (field 15)",
        static_cast<double>(t.user), 94200);
  Check(t.processor == 5, "processor (field 39), the core of the thread", t.processor, 5);
  // A name with a space and brackets, as Wine names a thread.
  const std::string odd =
      "13445 (wine (dsound) mix) S 1 13300 13300 0 -1 4194624 10 0 0 0 1400 600 0 0 -2 0 "
      "14 0 2823000 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 -1 3 1 0 0 0 0 0 0 0 0 0 0 0 0\n";
  bool parsed = ps::ParseTask(odd, &t);
  Check(parsed && t.name == "wine (dsound) mix" && t.state == 'S' &&
            t.user == 1400 && t.system == 600 && t.processor == 3,
        "a name with a space and ')' ends at the last ')'", t.processor, 3);
  Check(!ps::ParseTask("13445 (short) S 1 2 3", &t), "a line with too few fields: no task", 0,
        0);
  Check(!ps::ParseTask("no name here", &t), "a line with no name: no task", 0, 0);
  // A line with no field 39 (an old kernel): the core is not known.
  parsed = ps::ParseTask("7 (x) S 1 1 1 0 -1 0 0 0 0 0 5 6 0 0", &t);
  Check(parsed && t.user == 5 &&
            t.system == 6 && t.processor == -1,
        "a line with 15 fields: the times, and no core", t.processor, -1);

  ps::Cpu c;
  parsed = ps::ParseCpu("cpu  100 20 30 400 50 6 7 8 0 0", &c);
  Check(parsed && c.index == -1 &&
            c.busy == 171 && c.total == 621,
        "the line cpu of /proc/stat: busy is user, nice, system, irq, softirq and steal",
        static_cast<double>(c.busy), 171);
  parsed = ps::ParseCpu("cpu7 1 2 3 4 5 6 7 8 9 10", &c);
  Check(parsed && c.index == 7 && c.busy == 27 &&
            c.total == 36,
        "the line of core 7", static_cast<double>(c.total), 36);
  Check(!ps::ParseCpu("intr 12345 0 0", &c), "a line that is not of a core", 0, 0);
  Check(!ps::ParseCpu("cpufreq 1 2", &c), "a word that starts with cpu is not a core", 0, 0);

  Near("100 ticks in 1 s at 100 a second: 100 percent of one core",
       ps::Percent(100, 1.0, 100), 100.0, 1e-9);
  Near("950 ticks in 10 s: 95 percent", ps::Percent(950, 10.0, 100), 95.0, 1e-9);
  Check(ps::Percent(5, 0.0, 100) == 0.0, "no time: 0 percent", 0, 0);

  // The loads and the first 8.
  ps::Task before, now;
  before.id = now.id = 13314;
  before.user = 1000;
  before.system = 100;
  now.user = 1900;
  now.system = 400;
  ps::Load l = ps::LoadOf(13314, true, before, true, now);
  Check(l.user == 900 && l.system == 300 && ps::IsMainThread(l),
        "the ticks since the sample before, and the main thread has the id of its process",
        static_cast<double>(l.user), 900);
  l = ps::LoadOf(13314, true, before, false, now);
  Check(l.user == 1900 && l.system == 400, "a new thread counts each of its ticks",
        static_cast<double>(l.user), 1900);
  std::vector<ps::Load> loads;
  for (int i = 0; i < 12; ++i) {
    ps::Task a, b;
    a.id = b.id = 100 + i;
    b.user = static_cast<uint64_t>((i * 37) % 11);
    loads.push_back(ps::LoadOf(100, false, a, true, b));
  }
  const std::vector<ps::Load> top = ps::Top(loads, 8);
  bool sorted = top.size() == 8;
  for (size_t i = 1; i < top.size(); ++i) {
    const uint64_t pa = top[i - 1].user + top[i - 1].system;
    const uint64_t pb = top[i].user + top[i].system;
    if (pa < pb || (pa == pb && top[i - 1].now.id > top[i].now.id)) sorted = false;
  }
  Check(sorted && top[0].user == 10, "the first 8 threads, the most ticks first",
        static_cast<double>(top.size()), 8);
}

inline void FrameChecks() {
  namespace tl = time_log_math;
  printf("\nThe parts of a frame of the game in the time log of the mod\n");
  tl::Stat st;
  st.Add(2.0);
  st.Add(4.0);
  st.Add(-1.0);
  Check(st.count == 3 && st.most == 4.0, "a stat: the count and the most, a time below 0 is 0",
        st.most, 4.0);
  Near("the mean", st.Mean(), 2.0, 1e-9);
  tl::Stat other;
  other.Add(9.0);
  st.Merge(other);
  Check(st.count == 4 && st.most == 9.0 && st.sum == 15.0, "a merge", st.sum, 15.0);

  // Three frames of 30 ms. Each has a wait of the pace of 5 ms, a walk of 20
  // ms with its present 15 ms into the walk, and a write of the stream of 2
  // ms after the walk.
  tl::Frames f;
  double t = 1000.0;
  f.Present(t);  // the first present starts the first frame
  Check(f.frame.count == 0, "the first present gives no frame", 0, 0);
  for (int i = 0; i < 3; ++i) {
    f.Add(tl::kStream, 2.0);  // the write of the frame before, after its present
    f.Add(tl::kPace, 5.0);
    f.WalkBegin(t + 10.0);
    f.Present(t + 25.0);  // 15 ms of the walk before the present
    f.WalkEnd(t + 30.0);  // 5 ms of the walk after it
    t += 25.0;
    // The next walk starts at t + 10 again, so a frame is 25 ms here.
  }
  Check(f.frame.count == 3, "three presents after the first: three frames", f.frame.count, 3);
  Near("a frame is 25 ms", f.frame.Mean(), 25.0, 1e-9);
  // The first frame has 15 ms of walk, each later one 5 + 15 ms.
  Near("the scene walk: the part before the present, then the part after the present "
       "before",
       f.part[tl::kScene].most, 20.0, 1e-9);
  Near("the scene walk of the first frame", f.part[tl::kScene].sum, 55.0, 1e-9);
  Near("the write of the stream", f.part[tl::kStream].Mean(), 2.0, 1e-9);
  Near("the wait of the pace", f.part[tl::kPace].Mean(), 5.0, 1e-9);
  // The rest: 25 - 15 - 7 = 3 in the first frame, 25 - 20 - 7 = -2 is 0 later
  // (the walk after the present of the frame before is in this frame).
  Near("the rest is the frame less the parts, not below 0", f.rest.most, 3.0, 1e-9);
  Check(f.in_walk == 3, "the presents in a walk", f.in_walk, 3);
  Check(!f.Due(t), "less than 10 s: no line", 0, 0);
  f.Present(1000.0 + 10000.0);
  Check(f.Due(1000.0 + 10000.0), "10 s after the first present: the line", 1, 1);
  f.NewWindow(11000.0);
  Check(f.frame.count == 0 && f.part[tl::kScene].count == 0 && !f.Due(11000.0),
        "a new window starts with no frame", 0, 0);
  f.Present(11030.0);
  Near("the frame across the new window", f.frame.Mean(), 30.0, 1e-9);

  // A frame with no scene: no walk, a wait and a write.
  tl::Frames g;
  g.Present(0.0);
  g.Add(tl::kPace, 8.0);
  g.Add(tl::kStream, 1.0);
  g.Present(14.0);
  Check(g.part[tl::kScene].sum == 0.0 && g.in_walk == 0, "a frame with no walk", 0, 0);
  Near("its rest: 14 - 8 - 1", g.rest.Mean(), 5.0, 1e-9);

  // THE CAUSE of a frame of the game that did not show.
  Check(tl::CauseOf(100.0, 60.0, 10.0) == tl::Cause::kGpu,
        "60 of 100 ms in waits for the GPU: the GPU", 0, 0);
  Check(tl::CauseOf(100.0, 10.0, 60.0) == tl::Cause::kRuntime,
        "60 of 100 ms in xrWaitFrame: the runtime", 0, 0);
  Check(tl::CauseOf(100.0, 10.0, 20.0) == tl::Cause::kCpu,
        "70 of 100 ms of work of the app: the CPU of the app", 0, 0);
  Check(tl::CauseOf(10.0, 20.0, 0.0) == tl::Cause::kGpu,
        "waits longer than the time: the GPU, and the CPU part is 0", 0, 0);
}

inline void TimeLogChecks() {
  EyeSizeChecks();
  ProcStatChecks();
  FrameChecks();
}

}  // namespace time_log_checks
