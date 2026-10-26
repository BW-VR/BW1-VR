// The GPU parts of the walk of the headset app, a test
// (android/src/walk_parts.h). main.cpp includes this file after Check and
// Near, just before main.
#pragma once

#include "../../android/src/walk_parts.h"

namespace walk_parts_checks {

inline bool Close(double a, double b, double tolerance) {
  return a - b <= tolerance && b - a <= tolerance;
}

inline void WalkPartsChecks() {
  namespace wp = walk_parts;
  printf("\nThe GPU parts of the walk\n");
  Check(wp::ViewCount(0) == 0 && wp::ViewCount(3) == 2 && wp::ViewCount(2) == 1,
        "the views of a view mask: 0, 3 and 2 give 0, 2 and 1", wp::ViewCount(3), 2);
  Check(wp::On(1) && !wp::On(0), "gpu_parts: 1 is on, 0 is off", 0, 0);

  // A WALK: the start, the uploads, a pass of the eyes with 3 draws, a blit,
  // a pass of the panel with 1 draw, the end.
  wp::Recording r;
  uint32_t m = 99;
  Check(r.Begin(&m) && m == 0, "the begin of a command buffer: timestamp 0", m, 0);
  r.Upload(2, 100);
  wp::PassKey eyes;
  eyes.views = 2;
  eyes.width = 2200;
  eyes.height = 1760;
  eyes.clear = true;
  eyes.rate = true;
  Check(r.PassBegin(eyes, &m) && m == 1, "the start of the pass of the eyes: timestamp 1", m, 1);
  r.Draw();
  r.Draw();
  r.Draw();
  r.Pipeline();
  r.Sets();
  r.Writes(4);
  r.Upload(1, 1);  // in a pass: a count, but the pass stays a pass
  Check(r.PassEnd(&m) && m == 2, "the end of the pass: timestamp 2", m, 2);
  r.ReadBack();
  r.Copy();
  wp::PassKey panel;
  panel.width = 2200;
  panel.height = 1760;
  Check(r.PassBegin(panel, &m) && m == 3, "the pass of the panel: timestamp 3", m, 3);
  r.Draw();
  Check(r.PassEnd(&m) && m == 4, "its end: timestamp 4", m, 4);
  Check(r.End(&m) && m == 5 && r.Marks() == 6, "the end of the command buffer: timestamp 5, 6 in all",
        r.Marks(), 6);
  Check(!r.PartAt(0).pass && r.PartAt(0).gap == wp::kUploads,
        "part 0 (the start to the pass) has the uploads", r.PartAt(0).gap, wp::kUploads);
  Check(r.PartAt(1).pass && r.PartAt(1).key.Same(eyes) && r.PartAt(1).draws == 3,
        "part 1 is the pass of the eyes with 3 draws", static_cast<double>(r.PartAt(1).draws), 3);
  Check(!r.PartAt(2).pass && r.PartAt(2).gap == wp::kCopies,
        "part 2 has a read back and a blit: a copy is the more costly kind", r.PartAt(2).gap,
        wp::kCopies);
  Check(r.PartAt(3).pass && r.PartAt(3).key.Same(panel) && r.PartAt(3).draws == 1,
        "part 3 is the pass of the panel", static_cast<double>(r.PartAt(3).draws), 1);
  Check(!r.PartAt(4).pass && r.PartAt(4).gap == wp::kBarriers,
        "part 4 (the pass to the end) has only barriers", r.PartAt(4).gap, wp::kBarriers);
  const wp::Counts& c = r.CountsOf();
  Check(c.passes == 2 && c.draws == 4 && c.pipelines == 1 && c.sets == 1 && c.writes == 4 &&
            c.uploads == 3 && c.upload_bytes == 404,
        "the counts: 2 passes, 4 draws, 1 pipeline, 1 binding, 4 writes, 3 uploads of 404 bytes",
        static_cast<double>(c.upload_bytes), 404);

  // THE TOTALS of two walks, and a command buffer of uploads (no walk).
  wp::Totals t;
  const double ms1[5] = {0.5, 4.0, 0.25, 1.0, 0.25};
  const double ms2[5] = {1.5, 6.0, 0.25, 1.0, 0.25};
  t.Add(r, ms1);
  t.Add(r, ms2);
  wp::Recording up;
  up.Begin(&m);
  up.Upload(5, 10);
  up.End(&m);
  const double ms3[1] = {2.0};
  t.Add(up, ms3);
  Check(t.Walks() == 2 && t.Untimed() == 0, "two walks: the command buffer of uploads is no walk",
        static_cast<double>(t.Walks()), 2);
  Check(t.Keys() == 2 && t.Key(0).Same(eyes) && t.Key(1).Same(panel),
        "two keys of passes, in the order they came", t.Keys(), 2);
  Check(Close(t.Pass(0).ms, 10.0, 1e-9) && Close(t.Pass(0).most, 6.0, 1e-9) &&
            t.Pass(0).count == 2 && t.Pass(0).draws == 6,
        "the pass of the eyes: 10 ms in all, 6 at most in a walk, 2 passes, 6 draws",
        t.Pass(0).ms, 10.0);
  Check(Close(t.GapStat(wp::kUploads).ms, 4.0, 1e-9) && Close(t.GapStat(wp::kUploads).most, 2.0, 1e-9),
        "the uploads: 0.5 + 1.5 in the walks and 2 in their own command buffer, 2 at most",
        t.GapStat(wp::kUploads).ms, 4.0);
  Check(Close(t.TotalMs(), 17.0, 1e-9),
        "the sum of each part: 6 + 9 + 2", t.TotalMs(), 17.0);
  Check(t.CountsOf().uploads == 2 * 3 + 5 && t.CountsOf().draws == 8,
        "the counts add up over the command buffers", static_cast<double>(t.CountsOf().uploads),
        11);
  t.Add(r, nullptr);
  Check(t.Walks() == 3 && t.Untimed() == 1 && Close(t.Pass(0).ms, 10.0, 1e-9),
        "a walk with no times counts, and adds no time", static_cast<double>(t.Untimed()), 1);
  t.Reset();
  Check(t.Walks() == 0 && t.Pass(0).count == 0 && t.Keys() == 2 && t.CountsOf().draws == 0,
        "a reset: no walk, and the keys keep their places", t.Keys(), 2);

  // THE LIMIT of the timestamps: after 47 the rest is one part, and End has
  // the last one.
  wp::Recording big;
  big.Begin(&m);
  uint32_t got = 1;
  for (int i = 0; i < 40; ++i) {
    if (big.PassBegin(eyes, &m)) ++got;
    big.Draw();
    if (big.PassEnd(&m)) ++got;
  }
  Check(got == wp::kMostMarks - 1, "47 timestamps before the end of a long command buffer", got,
        wp::kMostMarks - 1);
  Check(big.End(&m) && m == wp::kMostMarks - 1 && big.Marks() == wp::kMostMarks,
        "the end takes the last timestamp", big.Marks(), wp::kMostMarks);
  const wp::Part& last = big.PartAt(wp::kMostMarks - 2);
  Check(!last.pass && last.gap == wp::kAfterLimit && last.draws > 0,
        "the last part is the rest after the limit, with its draws", last.gap, wp::kAfterLimit);
  Check(big.CountsOf().passes == 40 && big.CountsOf().draws == 40,
        "the counts have each pass and each draw after the limit too",
        static_cast<double>(big.CountsOf().draws), 40);

  // MORE KEYS than the totals keep: they go into the passes of other keys.
  wp::Totals many;
  for (uint32_t k = 0; k < wp::kMostPassKeys + 2; ++k) {
    wp::Recording one;
    one.Begin(&m);
    wp::PassKey key;
    key.width = 100 + k;
    one.PassBegin(key, &m);
    one.PassEnd(&m);
    one.End(&m);
    const double ms[3] = {0.0, 1.0, 0.0};
    many.Add(one, ms);
  }
  Check(many.Keys() == wp::kMostPassKeys && many.GapStat(wp::kOtherPasses).count == 2 &&
            Close(many.GapStat(wp::kOtherPasses).ms, 2.0, 1e-9),
        "12 keys: 10 keep their own place, 2 go into the passes of other keys",
        static_cast<double>(many.GapStat(wp::kOtherPasses).count), 2);
}

}  // namespace walk_parts_checks
