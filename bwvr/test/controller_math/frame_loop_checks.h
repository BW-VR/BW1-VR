// The frame loop of the headset app, a test (android/src/frame_loop.h, and
// the row of the page "Testing" of android/src/menu_model.h). main.cpp
// includes this file after Check and Near, just before main.
#pragma once

#include <string>
#include <vector>

#include "../../android/src/frame_loop.h"
#include "../../android/src/menu_model.h"
#include "../../src/common/ini_text.h"

namespace frame_loop_checks {

inline void RingChecks() {
  namespace fl = frame_loop;
  using fl::Slot;
  printf("\nThe ring of the pictures of the frame loop\n");
  fl::Ring r;
  const bool none[fl::kSlots] = {false, false, false};
  const bool all[fl::kSlots] = {true, true, true};
  Check(r.Take(all) == -1 && r.Shown() == -1, "no picture yet: the session takes none", 0, 0);

  // THE CHOICE OF THE NEWEST READY PICTURE.
  bool dropped = true;
  const int a = r.BeginWrite(&dropped);
  Check(a == 0 && !dropped && r.State(0) == Slot::kWriting, "the first picture: the free slot 0",
        a, 0);
  Check(r.Take(all) == -1, "a slot in writing never shows", 0, 0);
  r.EndWrite(a);
  Check(r.State(0) == Slot::kReady && r.Serial(0) == 1, "the submit makes it ready, serial 1",
        static_cast<double>(r.Serial(0)), 1);
  Check(r.Take(none) == -1 && r.State(0) == Slot::kReady,
        "the GPU did not finish it: no new picture, and it stays ready", 0, 0);
  Check(r.Take(all) == 0 && r.State(0) == Slot::kShown && r.Shown() == 0,
        "the GPU finished it: the session shows slot 0", r.Shown(), 0);
  Check(r.Take(all) == -1 && r.Shown() == 0,
        "the next frame with no new picture shows slot 0 again", r.Shown(), 0);

  const int b = r.BeginWrite(&dropped);
  r.EndWrite(b);
  const int c = r.BeginWrite(&dropped);
  r.EndWrite(c);
  Check(b == 1 && c == 2 && !dropped, "two more pictures go into the free slots 1 and 2", b, 1);
  bool done_b_only[fl::kSlots] = {true, true, false};
  Check(r.Take(done_b_only) == 1 && r.State(2) == Slot::kReady && r.State(0) == Slot::kFree,
        "only slot 1 is finished: the session shows it, slot 2 waits, and slot 0 is free", 0, 0);
  Check(r.Take(all) == 2 && r.State(1) == Slot::kFree && r.Replaced() == 0,
        "slot 2 is finished in the next frame: it shows, and slot 1 is free", r.Replaced(), 0);

  // Two finished pictures in one frame: the newest shows, the older is
  // replaced.
  const int d = r.BeginWrite(&dropped);
  r.EndWrite(d);
  const int e = r.BeginWrite(&dropped);
  r.EndWrite(e);
  Check(d == 0 && e == 1 && !dropped, "two pictures into the free slots 0 and 1", d, 0);
  Check(r.Take(all) == 1 && r.State(0) == Slot::kFree && r.State(2) == Slot::kFree &&
            r.Replaced() == 1,
        "both are finished: the newest (slot 1) shows, the older one is replaced and free",
        static_cast<double>(r.Replaced()), 1);
  Check(r.Serial(1) > r.Serial(2), "the shown picture is newer than the picture before", 0, 0);

  // THE DROP RULE: no free slot, so the play thread takes the oldest ready
  // slot, and its picture never shows.
  fl::Ring q;
  int s0 = q.BeginWrite(&dropped);
  q.EndWrite(s0);
  q.Take(all);  // slot 0 shows
  int s1 = q.BeginWrite(&dropped);
  q.EndWrite(s1);
  int s2 = q.BeginWrite(&dropped);
  q.EndWrite(s2);
  const int s3 = q.BeginWrite(&dropped);
  Check(s3 == 1 && dropped && q.Dropped() == 1 && q.State(0) == Slot::kShown,
        "no free slot: the play thread drops the oldest ready slot (1), never the shown one",
        s3, 1);
  Check(q.Take(all) == 2 && q.State(1) == Slot::kWriting,
        "the session shows slot 2, and slot 1 stays with the play thread", 0, 0);
  q.EndWrite(s3);
  Check(q.Take(all) == 1 && q.Serial(1) > q.Serial(2), "the dropped slot comes back with a newer "
        "picture", 0, 0);
  q.CancelWrite(q.BeginWrite(&dropped));
  Check(q.State(0) == Slot::kFree && q.State(2) == Slot::kFree, "a write that the play thread "
        "cancels leaves the slot free", 0, 0);

  // THE END OF THE GAME.
  int s4 = q.BeginWrite(&dropped);
  q.EndWrite(s4);
  q.Clear();
  Check(q.Shown() == -1 && q.State(0) == Slot::kFree && q.State(1) == Slot::kFree &&
            q.State(2) == Slot::kFree,
        "the end of the game: no slot shows, and each slot is free", 0, 0);

  // A long run: a slot that the session shows is never written, and the
  // session never shows an older picture than the one before.
  fl::Ring w;
  uint64_t last_shown = 0;
  bool shown_written = false, older = false, none_free = false;
  for (int step = 0; step < 300; ++step) {
    // The play thread makes 1 or 2 pictures, the GPU finishes them in this
    // frame or in the next.
    const int pictures = (step % 3 == 0) ? 2 : 1;
    for (int p = 0; p < pictures; ++p) {
      const int i = w.BeginWrite(&dropped);
      if (i < 0) {
        none_free = true;
        continue;
      }
      if (i == w.Shown()) shown_written = true;
      w.EndWrite(i);
    }
    bool done[fl::kSlots];
    for (int i = 0; i < fl::kSlots; ++i) done[i] = (step % 4 != 1);
    const int t = w.Take(done);
    if (t >= 0) {
      if (w.Serial(t) < last_shown) older = true;
      last_shown = w.Serial(t);
    }
  }
  Check(!shown_written && !older && !none_free,
        "300 frames: no write into the shown slot, no older picture, always a slot to write", 0,
        0);
}

// THE QUEUE OF THE PICTURES (frame_queue): which slot a take shows, which
// slots it frees, the drop rule, and the cadence in a model of the Frame.
// The model: takes at each whole step. The play thread writes and submits
// the picture k at k - 0.5 (it plays the next picture after the GPU work of
// the one before, so a write is short). The GPU ends the picture k at k + e,
// with e from -0.3 to 0.3 of a step: each picture ends at the edge of its
// take. gap: steps with no new picture of the game
// (a long frame). skip: steps with no take (a slow session).
struct QueueRun {
  int takes = 0, repeats = 0, out_of_order = 0, most_delay = 0;
  int repeats_after = 0;  // takes with no new picture after the first one
  int bins[frame_loop::kDelayBins] = {0, 0, 0};
  uint64_t replaced = 0, dropped = 0;
  bool shown_written = false;
};
inline QueueRun RunQueue(bool queue, int steps, int gap_at, int gap, int skip_at, int skip) {
  namespace fl = frame_loop;
  fl::Ring r(queue);
  uint64_t picture_of[fl::kSlots] = {0, 0, 0, 0};
  uint32_t seed = 12345u;
  double end_of[fl::kSlots] = {0, 0, 0, 0};
  QueueRun out;
  bool dropped = false;
  uint64_t next_picture = 1, last_shown = 0;
  bool any = false;
  for (int m = 1; m <= steps; ++m) {
    const bool game_idle = m >= gap_at && m < gap_at + gap;
    const int writing = game_idle ? -1 : r.BeginWrite(&dropped);
    if (writing >= 0) {
      if (writing == r.Shown()) out.shown_written = true;
      seed = seed * 1103515245u + 12345u;
      const double e = (static_cast<double>((seed >> 8) % 1001) / 1000.0 - 0.5) * 0.6;
      picture_of[writing] = next_picture;
      end_of[writing] = static_cast<double>(m) + e;
      ++next_picture;
      r.EndWrite(writing);
    }
    if (m >= skip_at && m < skip_at + skip) continue;
    bool done[fl::kSlots];
    for (int i = 0; i < fl::kSlots; ++i) done[i] = end_of[i] <= static_cast<double>(m);
    const uint64_t replaced = r.Replaced();
    const int t = r.Take(done);
    out.replaced += r.Replaced() - replaced;
    ++out.takes;
    if (t < 0) {
      ++out.repeats;
      if (any) ++out.repeats_after;
      continue;
    }
    if (any && picture_of[t] != last_shown + 1) ++out.out_of_order;
    any = true;
    last_shown = picture_of[t];
    if (r.ShownDelay() > out.most_delay) out.most_delay = r.ShownDelay();
    ++out.bins[fl::DelayBin(r.ShownDelay())];
  }
  out.dropped = r.Dropped();
  return out;
}

inline void QueueChecks() {
  namespace fl = frame_loop;
  using fl::Slot;
  printf("\nThe queue of the pictures (frame_queue)\n");
  Check(!fl::QueueOn(0) && fl::QueueOn(1) && fl::QueueOn(2), "frame_queue: 0 is the old way, each "
        "other value the queue", 0, 0);
  Check(fl::SlotsOf(false) == 3 && fl::SlotsOf(true) == 4 && fl::kSlots == 4,
        "the ring uses 3 slots with frame_queue=0 and 4 with 1", fl::SlotsOf(true), 4);
  Check(fl::QueuePick(false, 0) == -1 && fl::QueuePick(false, 1) == 0 &&
            fl::QueuePick(false, 3) == 2,
        "frame_queue=0: the newest of the finished pictures", fl::QueuePick(false, 3), 2);
  Check(fl::QueuePick(true, 0) == -1 && fl::QueuePick(true, 1) == 0 && fl::QueuePick(true, 2) == 0,
        "frame_queue=1: the oldest of one or two finished pictures", fl::QueuePick(true, 2), 0);
  Check(fl::QueuePick(true, 3) == 1 && fl::QueuePick(true, 4) == 2,
        "frame_queue=1, more than two: only the newest one waits after the take",
        fl::QueuePick(true, 4), 2);
  Check(fl::DelayBin(0) == 0 && fl::DelayBin(1) == 1 && fl::DelayBin(2) == 2 &&
            fl::DelayBin(7) == 2 && fl::DelayBin(-1) == 0,
        "the bins of the delay: 0, 1, 2 or more", fl::DelayBin(7), 2);

  // The default ring is the old way: 3 slots.
  fl::Ring old;
  Check(!old.Queue() && old.Count() == 3, "a ring with no key: the old way, 3 slots", old.Count(),
        3);

  // FOUR SLOTS: shown, waiting, at the GPU, in writing.
  fl::Ring q(true);
  const bool all[fl::kSlots] = {true, true, true, true};
  bool dropped = false;
  int a = q.BeginWrite(&dropped);
  q.EndWrite(a);
  const int t_a = q.Take(all);
  Check(t_a == a && q.ShownDelay() == 0, "the first picture shows at once, delay 0",
        q.ShownDelay(), 0);
  int b = q.BeginWrite(&dropped);
  q.EndWrite(b);
  int c = q.BeginWrite(&dropped);
  q.EndWrite(c);
  int d = q.BeginWrite(&dropped);
  Check(a == 0 && b == 1 && c == 2 && d == 3 && !dropped && q.Dropped() == 0,
        "shown, two ready and one in writing: four slots, no drop", d, 3);
  bool done_bc[fl::kSlots] = {true, true, true, false};
  const int t_bc = q.Take(done_bc);
  Check(t_bc == b && q.State(c) == Slot::kReady && q.State(a) == Slot::kFree &&
            q.Replaced() == 0,
        "two finished pictures: the older (slot 1) shows, the newer waits, the shown slot before "
        "is free",
        static_cast<double>(q.Replaced()), 0);
  q.EndWrite(d);
  const int t_c = q.Take(all);
  Check(t_c == c && q.ShownDelay() == 1 && q.State(d) == Slot::kReady,
        "the next take shows the waiting picture (slot 2) with a delay of 1 take, slot 3 waits",
        q.ShownDelay(), 1);
  const int t_d = q.Take(all);
  Check(t_d == d && q.ShownDelay() == 1, "and then slot 3, also with a delay of 1",
        q.ShownDelay(), 1);
  const int t_none = q.Take(all);
  Check(t_none == -1 && q.Shown() == d, "no new picture: the shown slot shows again (a miss)",
        q.Shown(), d);

  // THE DROP RULE OF THE TAKE: three finished pictures wait (the session was
  // slower than the game). The oldest goes, the middle one shows, the newest
  // waits.
  int e = q.BeginWrite(&dropped);
  q.EndWrite(e);
  int f = q.BeginWrite(&dropped);
  q.EndWrite(f);
  int g = q.BeginWrite(&dropped);
  q.EndWrite(g);
  Check(!dropped && q.State(d) == Slot::kShown, "three pictures into the three free slots", 0, 0);
  const int t_f = q.Take(all);
  Check(t_f == f && q.State(e) == Slot::kFree && q.State(g) == Slot::kReady &&
            q.Replaced() == 1,
        "three finished pictures: the oldest is freed (replaced), the middle one shows, the "
        "newest waits",
        static_cast<double>(q.Replaced()), 1);
  Check(q.Serial(f) > q.Serial(e) && q.Serial(g) > q.Serial(f), "the shown picture is newer than "
        "the freed one, the waiting one newer than both", 0, 0);
  // An unfinished picture is no candidate: it waits, also when it is older.
  int h = q.BeginWrite(&dropped);
  q.EndWrite(h);
  bool done_h_only[fl::kSlots] = {false, false, false, false};
  done_h_only[h] = true;
  const int t_h = q.Take(done_h_only);
  Check(t_h == h && q.State(g) == Slot::kFree && q.Replaced() == 2,
        "only the newer picture is finished: it shows, and the older unfinished one is freed",
        static_cast<double>(q.Replaced()), 2);

  // THE DROP RULE OF THE PLAY THREAD with four slots: no free slot, the
  // oldest ready slot, never the shown one.
  fl::Ring p(true);
  int s0 = p.BeginWrite(&dropped);
  p.EndWrite(s0);
  p.Take(all);
  for (int k = 0; k < 3; ++k) p.EndWrite(p.BeginWrite(&dropped));
  const int s4 = p.BeginWrite(&dropped);
  Check(s4 == 1 && dropped && p.Dropped() == 1 && p.State(0) == Slot::kShown,
        "no free slot of four: the play thread drops the oldest ready slot (1), never the shown "
        "one",
        s4, 1);

  // A NEW VALUE WHILE THE RING RUNS: from 4 slots to 3, slot 3 takes no new
  // write, and a take frees it.
  fl::Ring live(true);
  for (int k = 0; k < 4; ++k) live.EndWrite(live.BeginWrite(&dropped));
  live.SetQueue(false);
  const int t_live = live.Take(all);
  Check(live.Count() == 3 && t_live == 3 && live.State(0) == Slot::kFree &&
            live.State(1) == Slot::kFree && live.State(2) == Slot::kFree,
        "frame_queue to 0 with 4 ready slots: the newest (slot 3) shows, the others are free", 0,
        0);
  int n0 = live.BeginWrite(&dropped);
  live.EndWrite(n0);
  int n1 = live.BeginWrite(&dropped);
  live.EndWrite(n1);
  const int t_n = live.Take(all);
  Check(n0 == 0 && n1 == 1 && t_n == 1 && live.State(3) == Slot::kFree,
        "the writes go to the slots 0 to 2, and the next take frees slot 3", 0, 0);
  bool any3 = false;
  for (int k = 0; k < 20; ++k) {
    const int i = live.BeginWrite(&dropped);
    if (i == 3) any3 = true;
    live.EndWrite(i);
    if (k % 2 == 0) live.Take(all);
  }
  Check(!any3, "20 writes with 3 slots: none into slot 3", 0, 0);

  // THE MODEL OF THE FRAME: each picture ends at the edge of its take.
  const QueueRun o = RunQueue(false, 900, 0, 0, 0, 0);
  const QueueRun n = RunQueue(true, 900, 0, 0, 0, 0);
  printf("    model, 900 takes: frame_queue=0 %d takes with no new picture, %d pictures out of "
         "order, %llu replaced, %llu dropped. frame_queue=1 %d, %d, %llu, %llu, delay 0/1/2+ "
         "%d/%d/%d\n",
         o.repeats_after, o.out_of_order, static_cast<unsigned long long>(o.replaced),
         static_cast<unsigned long long>(o.dropped), n.repeats_after, n.out_of_order,
         static_cast<unsigned long long>(n.replaced), static_cast<unsigned long long>(n.dropped),
         n.bins[0], n.bins[1], n.bins[2]);
  Check(o.repeats_after > 100 && o.out_of_order > 100,
        "the model with frame_queue=0: many takes with no new picture and many skipped pictures",
        o.repeats_after, 100);
  Check(n.repeats_after <= 1 && n.out_of_order == 0 && n.replaced == 0 && n.dropped == 0,
        "frame_queue=1: at most one take with no new picture, then each picture in its order",
        n.repeats_after, 1);
  Check(n.most_delay == 1 && n.bins[1] > 850 && !n.shown_written && !o.shown_written,
        "frame_queue=1: a delay of 1 take, never more, and no write into the shown slot",
        n.bins[1], 850);
  // A long frame of the game (3 steps with no new picture): the misses of the
  // gap, and then the order again with a delay of at most 1.
  const QueueRun lg = RunQueue(true, 900, 400, 3, 0, 0);
  Check(lg.repeats_after <= 1 + 3 + 1 && lg.out_of_order == 0 && lg.most_delay == 1,
        "frame_queue=1 with a long frame: the misses of the gap, no picture out of order",
        lg.repeats_after, 5);
  // A slow session (2 takes that did not happen): the older pictures go in
  // the first takes after it, and the delay stays at 1.
  const QueueRun sl = RunQueue(true, 900, 0, 0, 500, 2);
  printf("    model, a slow session: %d pictures out of order, %llu replaced, %llu dropped, delay "
         "0/1/2+ %d/%d/%d\n",
         sl.out_of_order, static_cast<unsigned long long>(sl.replaced),
         static_cast<unsigned long long>(sl.dropped), sl.bins[0], sl.bins[1], sl.bins[2]);
  Check(sl.out_of_order >= 1 && sl.out_of_order <= 2 &&
            sl.replaced + sl.dropped == 2 && sl.most_delay == 1 && sl.repeats_after <= 1,
        "frame_queue=1 with a slow session of 2 takes: 2 pictures go (in 1 or 2 jumps), and the "
        "delay stays at 1",
        static_cast<double>(sl.replaced + sl.dropped), 2);
}

inline void CommandRingChecks() {
  namespace fl = frame_loop;
  printf("\nThe ring of the command buffers of the session\n");
  fl::CommandRing c(4);
  fl::CommandPick p = c.Pick(true);
  Check(p.grow && p.index == 0 && !p.wait && c.Count() == 1,
        "the first command buffer: a new entry", c.Count(), 1);
  p = c.Pick(true);
  Check(!p.grow && p.index == 0 && !p.wait, "its GPU work is finished: it is used again", p.index,
        0);
  p = c.Pick(false);
  Check(p.grow && p.index == 0 && !p.wait && c.Count() == 2 && c.Next() == 1,
        "its GPU work is not finished: a new entry, no wait, and the old one is next", c.Next(),
        1);
  p = c.Pick(false);
  Check(p.grow && p.index == 1 && c.Count() == 3 && c.Next() == 2,
        "again not finished: one more entry", c.Count(), 3);
  p = c.Pick(true);
  Check(!p.grow && p.index == 2 && !p.wait && c.Next() == 0,
        "the least recently used entry is finished: it is used", p.index, 2);
  p = c.Pick(false);
  Check(p.grow && p.index == 0 && c.Count() == 4, "the fourth entry", c.Count(), 4);
  p = c.Pick(false);
  Check(!p.grow && p.wait && p.index == 1 && c.Count() == 4,
        "the ring has its most entries: only then the CPU waits", p.index, 1);
  // In steady use with finished entries the ring does not grow and never
  // waits.
  fl::CommandRing d(16);
  int grows = 0, waits = 0;
  for (int i = 0; i < 1000; ++i) {
    const fl::CommandPick k = d.Pick(i < 3 ? false : true);
    grows += k.grow ? 1 : 0;
    waits += k.wait ? 1 : 0;
  }
  Check(grows == 3 && waits == 0 && d.Count() == 3,
        "1000 picks, the GPU behind for the first three: 3 entries, no wait", grows, 3);
  // A new entry that the caller could not make: no growth, a wait for the
  // least recently used entry, or no entry at all.
  fl::CommandRing e(4);
  Check(e.WillGrow(true) && e.Pick(true, false).index == -1 && e.Count() == 0,
        "no entry and none can be made: index -1, the ring stays empty", e.Count(), 0);
  e.Pick(true);
  Check(e.WillGrow(false) && !e.WillGrow(true), "one entry: it grows only when the GPU is "
        "not done", 0, 0);
  fl::CommandPick f = e.Pick(false, false);
  Check(!f.grow && f.wait && f.index == 0 && e.Count() == 1,
        "the GPU is not done and no entry can be made: a wait for the entry, no growth",
        e.Count(), 1);

  Check(fl::SerialDone(5, 5) && fl::SerialDone(4, 5) && !fl::SerialDone(6, 5),
        "a serial at or below the finished serial is finished", 0, 0);
  Check(fl::NextAge(3, true) == 0 && fl::NextAge(0, false) == 1 && fl::NextAge(4, false) == 5,
        "the age of the shown picture: 0 for a new picture, one more for each frame with it", 0, 0);
  Check(fl::On(1) && !fl::On(0) && fl::On(2), "frame_loop: 0 is the old loop, each other value "
        "the new loop", 0, 0);
}

inline void MenuRowChecks() {
  printf("\nThe row of the frame loop in the menu\n");
  menu_model::Menu menu;
  menu.Load(bwvr_ini::IniText(""));
  Check(menu.FrameLoop() == 1, "no key: the new loop (1)", menu.FrameLoop(), 1);
  menu.Load(bwvr_ini::IniText("[bwvr]\r\nframe_loop=0\r\n"));
  Check(menu.FrameLoop() == 0, "frame_loop=0 loads", menu.FrameLoop(), 0);
  menu.Load(bwvr_ini::IniText("[bwvr]\r\nframe_loop=7\r\n"));
  Check(menu.FrameLoop() == 1, "another value: 1", menu.FrameLoop(), 1);
  int row = -1;
  std::vector<menu_model::Item> items = menu.Items();
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (items[r].kind == menu_model::ItemKind::kGroup &&
        menu.Label(items[r]) == std::string("Testing")) {
      row = r;
    }
  }
  Check(row >= 0, "the main page has the group Testing", row, 0);
  Check(menu.Help(items[row]).find("The headset gets each frame (no wait for the GPU)") !=
            std::string::npos,
        "the help of the group names the row", 0, 0);
  menu.SetCursor(row);
  menu.Select();
  items = menu.Items();
  int at = -1, cpu = -1;
  for (int r = 0; r < static_cast<int>(items.size()); ++r) {
    if (items[r].kind == menu_model::ItemKind::kFrameLoop) at = r;
    if (items[r].kind == menu_model::ItemKind::kWow64Cpu) cpu = r;
  }
  Check(at >= 0 && cpu == at + 1, "the page Testing has the row above the row of the translator",
        at, 0);
  const menu_model::Item item = items[at];
  Check(menu.Label(item) == "The headset gets each frame (no wait for the GPU)" &&
            menu.ValueText(item) == "On",
        "its label and its value", 0, 0);
  const std::string help = menu.Help(item);
  Check(help.find("a frame at each refresh of its display") != std::string::npos &&
            help.find("Off: the old way") != std::string::npos &&
            help.find("next Play") != std::string::npos,
        "its help says what On and Off do and when the change counts", 0, 0);
  menu.SetCursor(at);
  menu.Step(1);
  Check(menu.FrameLoop() == 0 && menu.ValueText(item) == "Off" && menu.Changed(),
        "a step: Off", menu.FrameLoop(), 0);
  bwvr_ini::IniText out("");
  menu.Store(&out);
  Check(out.GetInt("bwvr", "frame_loop", -1) == 0, "the save writes frame_loop=0",
        out.GetInt("bwvr", "frame_loop", -1), 0);
  menu.Select();
  Check(menu.FrameLoop() == 1, "the trigger gives On again", menu.FrameLoop(), 1);
  menu.Step(-1);
  menu.Step(-1);
  Check(menu.FrameLoop() == 1, "two steps left: On", menu.FrameLoop(), 1);
  menu.Store(&out);
  Check(out.GetInt("bwvr", "frame_loop", -1) == 1, "the save writes frame_loop=1",
        out.GetInt("bwvr", "frame_loop", -1), 1);
}

// THE QUEUE AT THE GPU, the direct writes and the rect of a copy.
inline void GpuQueueChecks() {
  namespace fl = frame_loop;
  printf("\nThe queue at the GPU and the pixels of the CPU of the frame loop\n");
  Check(fl::GpuDepth(1) == 1 && fl::GpuDepth(0) == 0 && fl::GpuDepth(-3) == 0 &&
            fl::GpuDepth(2) == 2,
        "frame_loop_gpu_depth: 1 and 2 stay, 0 and below are no limit", fl::GpuDepth(-3), 0);
  Check(fl::MayPlay(1, 0) && !fl::MayPlay(1, 1) && !fl::MayPlay(1, 2),
        "depth 1: a play only when no picture is unfinished on the GPU", 0, 0);
  Check(fl::MayPlay(2, 1) && !fl::MayPlay(2, 2), "depth 2: a play with one unfinished picture",
        0, 0);
  Check(fl::MayPlay(0, 0) && fl::MayPlay(0, 3), "depth 0: each play at once (the old way)", 0, 0);
  // A loop of the play thread with depth 1: the GPU takes 3 steps of the
  // loop for each picture. It never has two unfinished pictures.
  int unfinished = 0, left = 0, plays = 0, most = 0;
  for (int step = 0; step < 30; ++step) {
    if (left > 0 && --left == 0) unfinished = 0;
    if (fl::MayPlay(1, unfinished)) {
      ++plays;
      unfinished = 1;
      left = 3;
    }
    if (unfinished > most) most = unfinished;
  }
  Check(most == 1 && plays == 10, "30 steps with a GPU of 3 steps a picture: 10 plays, 1 unfinished at most",
        plays, 10);
  Check(fl::Direct(true, false, true), "direct: a free staging buffer with no copy in memory waiting",
        0, 0);
  Check(!fl::Direct(true, true, true), "not direct when a copy in memory waits (it is older)", 0, 0);
  Check(!fl::Direct(true, false, false), "not direct while the GPU copies the staging buffer", 0, 0);
  Check(!fl::Direct(false, false, true), "xr_staging_direct=0: never direct (the old way)", 0, 0);

  const int32_t card[4] = {10, 20, 357, 86};
  fl::Rect r = fl::CopyRect(true, card, 768, 320);
  Check(r.x == 10 && r.y == 20 && r.width == 357 && r.height == 86,
        "a card of 768 by 320 copies its rect 357 by 86 at 10, 20", r.width, 357);
  r = fl::CopyRect(false, card, 768, 320);
  Check(r.x == 0 && r.y == 0 && r.width == 768 && r.height == 320,
        "xr_layer_rect=0: the whole image (the old way)", r.width, 768);
  const int32_t none[4] = {0, 0, 0, 0};
  r = fl::CopyRect(true, none, 64, 32);
  Check(r.width == 64 && r.height == 32, "a layer with no rect: the whole image", r.width, 64);
  const int32_t over[4] = {700, 300, 200, 100};
  r = fl::CopyRect(true, over, 768, 320);
  Check(r.x == 700 && r.y == 300 && r.width == 68 && r.height == 20,
        "a rect over the edge is cut to the image", r.width, 68);
  const int32_t out[4] = {800, 0, 10, 10};
  r = fl::CopyRect(true, out, 768, 320);
  Check(r.width == 768 && r.height == 320, "a rect outside the image: the whole image", r.width,
        768);
  // A card shows only when the released image has the rect of its quad.
  const int32_t moved[4] = {12, 20, 357, 86};
  Check(fl::RectShows(true, card, card), "the released image has the rect: the card shows", 0, 0);
  Check(!fl::RectShows(true, card, moved),
        "a new rect with no copy yet: the card does not show (its image has the old rect)", 0, 0);
  Check(!fl::RectShows(true, none, card), "no image released yet: the card does not show", 0, 0);
  Check(fl::RectShows(false, none, card), "xr_layer_rect=0: the rect is no condition (the old way)",
        0, 0);
  const int32_t neg[4] = {-5, -5, 20, 20};
  r = fl::CopyRect(true, neg, 768, 320);
  Check(r.x == 0 && r.y == 0 && r.width == 15 && r.height == 15,
        "a rect that starts before the image is cut too", r.width, 15);
}

// The copies recorded one time (xr_copy_once): when a set of entries stays.
inline void OnceSetChecks() {
  using frame_loop::OnceSetValid;
  Check(OnceSetValid(1380, 1104, 3, 1380, 1104, 3),
        "a set stays with the same size and no new image of the ring", 1, 1);
  Check(!OnceSetValid(1380, 1104, 3, 1380, 1104, 5),
        "a set goes when the ring made or freed an image, also at the same size "
        "(a slot that went to another size and back)", 0, 0);
  Check(!OnceSetValid(1380, 1104, 3, 1024, 1104, 3), "a set goes at another width", 0, 0);
  Check(!OnceSetValid(1380, 1104, 3, 1380, 800, 3), "a set goes at another height", 0, 0);
  Check(!OnceSetValid(0, 0, 0, 1380, 1104, 0), "an empty set takes the first size", 0, 0);
}

inline void FrameLoopChecks() {
  RingChecks();
  QueueChecks();
  OnceSetChecks();
  CommandRingChecks();
  GpuQueueChecks();
  MenuRowChecks();
}

}  // namespace frame_loop_checks
