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

inline void FrameLoopChecks() {
  RingChecks();
  CommandRingChecks();
  MenuRowChecks();
}

}  // namespace frame_loop_checks
