#!/usr/bin/env python3
"""Reads a timeline file of the mod (Trace Event Format, src/common/trace_events.h)
and prints where the long frames spend their time.

  python trace_report.py trace-20261103-141024.json
  python trace_report.py trace.json --game-ms 13 --top 10 --write-fixed fixed.json

The file has the process "game" (pid 1) and, from the headset, the process
"app" (pid 2) on one clock. The script prints:

  1. The clock check: the frames that both processes know (the write of the
     frame into the ring in the game, the arrival in the reader of the app).
     The arrival is 0 to 1 ms after the end of the write. When the two clocks
     differ, the script says so and gives the shift (--write-fixed writes a
     file with the shift applied to the game).
  2. The --max-game longest frames of the game among those longer than
     --game-ms: the --top longest bars inside each, with their thread, their
     start in the frame and their own time (the time without the bars inside
     them).
  3. Each frame of the session of the app with no new picture (the point "frame
     of the session with no new picture"): the same list, for the frame of the
     session around the point (from the end of one xrWaitFrame to the end of the
     next one).
  4. Each picture in which the place of the camera goes back against the
     direction of the pictures before (the points "picture shown"), and each
     picture of a frame of the game that is older than the one before. With
     each step back it prints what the motion of the stick did in that frame of
     the session (the point "motion of the stick (state, shift of the pose)"):
     a shift of the pose means that the motion made the jump, no shift means
     that the picture of the game jumped.
  5. A check of the file: bars that cross each other on one thread.

Only the standard library. The script does not change the file.
"""

import argparse
import collections
import json
import statistics
import sys

GAME, APP = 1, 2
PID_NAME = {GAME: "game", APP: "app"}


class Bar:
    __slots__ = ("pid", "tid", "name", "cat", "ts", "dur", "args", "self_us")

    def __init__(self, e):
        self.pid = e.get("pid", 0)
        self.tid = e.get("tid", 0)
        self.name = e.get("name", "")
        self.cat = e.get("cat", "")
        self.ts = float(e.get("ts", 0.0))
        self.dur = float(e.get("dur", 0.0))
        self.args = e.get("args", {}) or {}
        self.self_us = self.dur

    @property
    def end(self):
        return self.ts + self.dur


def load(path):
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    events = data["traceEvents"] if isinstance(data, dict) else data
    threads = {}
    bars, points, counters = [], [], []
    for e in events:
        ph = e.get("ph")
        if ph == "M" and e.get("name") == "thread_name":
            threads[(e["pid"], e["tid"])] = e["args"]["name"]
        elif ph == "X":
            bars.append(Bar(e))
        elif ph == "i":
            points.append(Bar(e))
        elif ph == "C":
            counters.append(Bar(e))
    return data, threads, bars, points, counters


def thread_label(threads, pid, tid):
    return "%s/%s" % (PID_NAME.get(pid, pid), threads.get((pid, tid), tid))


def compute_self_times(bars):
    """The own time of each bar: its length less the bars directly inside it
    on the same thread. Bars that cross each other are counted in
    `crossings`."""
    by_thread = collections.defaultdict(list)
    for b in bars:
        by_thread[(b.pid, b.tid)].append(b)
    crossings = collections.Counter()
    for key, lst in by_thread.items():
        lst.sort(key=lambda b: (b.ts, -b.dur))
        stack = []
        for b in lst:
            while stack and stack[-1].end <= b.ts + 1e-6:
                stack.pop()
            if stack:
                parent = stack[-1]
                if b.end > parent.end + 1.0:
                    crossings[key] += 1
                else:
                    parent.self_us -= b.dur
            stack.append(b)
    for b in bars:
        if b.self_us < 0:
            b.self_us = 0.0
    return crossings


def clip(b, lo, hi):
    return max(0.0, min(b.end, hi) - max(b.ts, lo))


def longest_inside(bars, lo, hi, top, skip_names):
    inside = []
    for b in bars:
        if b.name in skip_names:
            continue
        if b.end <= lo or b.ts >= hi:
            continue
        inside.append(b)
    inside.sort(key=lambda b: clip(b, lo, hi), reverse=True)
    return inside[:top]


def print_bars(threads, rows, lo, hi):
    for b in rows:
        extra = ""
        if b.args:
            extra = "  " + " ".join("%s=%g" % (k, v) for k, v in b.args.items())
        idle = "  [a wait]" if b.cat in ("wait", "xr") else ""
        print("      %7.2f ms  own %7.2f ms  at %+8.2f  %-34s %s%s%s" % (
            clip(b, lo, hi) / 1000.0, b.self_us / 1000.0, (b.ts - lo) / 1000.0,
            thread_label(threads, b.pid, b.tid), b.name, extra, idle))


def clock_check(bars, points):
    puts = {}
    for b in bars:
        if b.pid == GAME and b.name.startswith("stream put") and "frame" in b.args:
            puts[int(b.args["frame"])] = b.end
    arrivals = {}
    for p in points:
        if p.pid == APP and p.name.startswith("game frame arrives") and "frame" in p.args:
            arrivals.setdefault(int(p.args["frame"]), p.ts)
    common = sorted(set(puts) & set(arrivals))
    if not common:
        print("clock check: no frame is in both processes (the file has the game alone, or the "
              "app did not record the arrivals)")
        return 0.0, False
    d = [(arrivals[f] - puts[f]) / 1000.0 for f in common]
    least, med = min(d), statistics.median(d)
    ok = -1.0 <= least and med <= 5.0
    print("clock check: %d frames in both processes. Arrival less end of the write: least %.2f ms, "
          "median %.2f ms, most %.2f ms (the arrival is 0 to 1 ms after the write)" % (
              len(d), least, med, max(d)))
    if not ok:
        print("  THE CLOCKS DO NOT MATCH. Shift the game by %+0.2f ms (--write-fixed) before you "
              "compare the two processes" % least)
    return least * 1000.0, ok


def game_frames(bars, points, threads, args, all_bars):
    frames = [b for b in bars if b.pid == GAME and b.name == "frame"]
    frames.sort(key=lambda b: b.ts)
    print("\ngame: %d frames in the file, %s" % (
        len(frames),
        "median %.2f ms, longest %.2f ms" % (
            statistics.median(b.dur for b in frames) / 1000.0, max(b.dur for b in frames) / 1000.0)
        if frames else "none"))
    long_frames = [b for b in frames if b.dur > args.game_ms * 1000.0]
    print("game: %d frames longer than %.1f ms" % (len(long_frames), args.game_ms))
    # The longest ones, printed in the order of time.
    shown = sorted(sorted(long_frames, key=lambda b: b.dur, reverse=True)[:args.max_game],
                   key=lambda b: b.ts)
    if len(long_frames) > len(shown):
        print("game: the %d longest of them follow (--max-game)" % len(shown))
    for f in shown:
        lo, hi = f.ts, f.end
        print("  frame %s  %.2f ms  at %.3f s" % (
            int(f.args.get("frame", -1)), f.dur / 1000.0, lo / 1e6))
        rows = longest_inside(all_bars, lo, hi, args.top, {"frame"})
        print_bars(threads, rows, lo, hi)


def session_frames(bars, points, threads, args, all_bars):
    waits = sorted([b for b in bars if b.pid == APP and b.name == "xrWaitFrame"], key=lambda b: b.ts)
    nopic = sorted([p for p in points if p.pid == APP and p.name.startswith("frame of the session with no new picture")],
                   key=lambda p: p.ts)
    print("\napp: %d frames of the session (xrWaitFrame), %d of them with no new picture" % (
        len(waits), len(nopic)))
    if not waits:
        return
    ends = [w.end for w in waits]
    shown = 0
    for p in nopic:
        # The frame of the session: from the end of the xrWaitFrame before the
        # point to the end of the next xrWaitFrame.
        lo = None
        hi = None
        for i, e in enumerate(ends):
            if e <= p.ts:
                lo = e
                hi = ends[i + 1] if i + 1 < len(ends) else p.ts
        if lo is None:
            continue
        shown += 1
        if shown > args.max_session:
            continue
        print("  no new picture at %.3f s, frame of the session %.2f ms, the picture shown is "
              "from frame %d" % (p.ts / 1e6, (hi - lo) / 1000.0, int(p.args.get("shown frame", -1))))
        rows = longest_inside(all_bars, lo, hi, args.top, {"frame"})
        print_bars(threads, rows, lo, hi)
    if shown > args.max_session:
        print("  ... %d more (--max-session)" % (shown - args.max_session))


MOTION = "motion of the stick (state, shift of the pose)"
MOTION_STATE = {0: "nothing yet", 1: "motion_comp=0", 2: "no map", 3: "no difference",
                4: "moved", 5: "over the limits (the pose stays)"}


def motion_text(motion, ts):
    """What the motion of the stick did in the frame of the session at ts."""
    if not motion:
        return ""
    near = min(motion, key=lambda m: abs(m.ts - ts))
    if abs(near.ts - ts) > 2000.0:
        return ""
    state = int(near.args.get("state of the left eye", 0))
    return ". Motion of the stick in that frame: %s, shift of the pose %.2f cm (left) %.2f cm (right)" % (
        MOTION_STATE.get(state, state), near.args.get("shift left cm", 0.0),
        near.args.get("shift right cm", 0.0))


def camera_back(points, args):
    motion = sorted([p for p in points if p.pid == APP and p.name == MOTION], key=lambda p: p.ts)
    pics = sorted([p for p in points if p.pid == APP and p.name.startswith("picture shown (game frame")],
                  key=lambda p: p.ts)
    print("\napp: %d pictures shown with a place of the camera" % len(pics))
    if len(pics) < 3:
        return
    # A picture is a new picture when its frame differs from the one before.
    last_frame = None
    seq = []
    for p in pics:
        f = int(p.args.get("frame", -1))
        pos = (p.args.get("world x", 0.0), p.args.get("world y", 0.0), p.args.get("world z", 0.0))
        if f != last_frame:
            seq.append((p.ts, f, pos))
            last_frame = f
    older = 0
    for i in range(1, len(seq)):
        if seq[i][1] < seq[i - 1][1]:
            older += 1
            print("  OLDER PICTURE at %.3f s: frame %d after frame %d" % (seq[i][0] / 1e6, seq[i][1], seq[i - 1][1]))
    back = 0
    k = args.history
    skip_until = 0
    for i in range(k + 1, len(seq)):
        # The steps after a step back have it in their history: they are not
        # new steps back.
        if i < skip_until:
            continue
        prev = []
        for j in range(i - k, i):
            a, b = seq[j - 1][2], seq[j][2]
            prev.append(tuple(b[c] - a[c] for c in range(3)))
        mean = tuple(sum(v[c] for v in prev) / len(prev) for c in range(3))
        step = tuple(seq[i][2][c] - seq[i - 1][2][c] for c in range(3))
        mm = sum(c * c for c in mean) ** 0.5
        sm = sum(c * c for c in step) ** 0.5
        if mm < args.min_step or sm < args.min_step:
            continue
        cos = sum(mean[c] * step[c] for c in range(3)) / (mm * sm)
        if cos < args.back_cos:
            back += 1
            skip_until = i + k + 1
            print("  CAMERA BACK at %.3f s: frame %d, step %.3f (%+.3f %+.3f %+.3f) against the mean "
                  "step %.3f of the %d pictures before, cos %.2f%s" % (
                      seq[i][0] / 1e6, seq[i][1], sm, step[0], step[1], step[2], mm, k, cos,
                      motion_text(motion, seq[i][0])))
    print("  %d pictures with an older frame, %d pictures with a step back against the steps before "
          "(--history %d, --back-cos %.2f, --min-step %.3f)" % (older, back, k, args.back_cos, args.min_step))


def write_fixed(data, shift_us, path):
    events = data["traceEvents"] if isinstance(data, dict) else data
    for e in events:
        if e.get("pid") == GAME and "ts" in e:
            e["ts"] = e["ts"] + shift_us
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f)
    print("wrote %s with the game shifted by %+0.3f ms" % (path, shift_us / 1000.0))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file")
    ap.add_argument("--game-ms", type=float, default=13.0, help="frames of the game longer than this (13)")
    ap.add_argument("--max-game", type=int, default=30, help="frames of the game to print, the longest (30)")
    ap.add_argument("--top", type=int, default=10, help="bars to print for each frame (10)")
    ap.add_argument("--max-session", type=int, default=30, help="frames of the session to print (30)")
    ap.add_argument("--history", type=int, default=3, help="pictures that give the direction (3)")
    ap.add_argument("--min-step", type=float, default=0.01, help="smallest step in game units that counts (0.01)")
    ap.add_argument("--back-cos", type=float, default=-0.5, help="cosine below which a step is back (-0.5)")
    ap.add_argument("--write-fixed", metavar="OUT", help="write a file with the clock shift applied to the game")
    args = ap.parse_args()

    data, threads, bars, points, counters = load(args.file)
    other = data.get("otherData", {}) if isinstance(data, dict) else {}
    print("file: %s  %d bars, %d points, %d counters, window %s s%s" % (
        args.file, len(bars), len(points), len(counters), other.get("seconds", "?"),
        ", note: " + other["note"] if "note" in other else ""))
    pids = sorted({b.pid for b in bars})
    for pid in pids:
        rows = sorted({thread_label(threads, pid, b.tid) for b in bars if b.pid == pid})
        print("  %s: %d threads: %s" % (PID_NAME.get(pid, pid), len(rows), ", ".join(rows)))

    shift_us, ok = clock_check(bars, points)
    if not ok and shift_us != 0.0:
        if args.write_fixed:
            write_fixed(data, shift_us, args.write_fixed)
        for b in bars + points:
            if b.pid == GAME:
                b.ts += shift_us
    crossings = compute_self_times(bars)
    all_bars = bars
    game_frames(bars, points, threads, args, all_bars)
    session_frames(bars, points, threads, args, all_bars)
    camera_back(points, args)

    print("\nfile check: bars that cross a bar on the same thread: %s" % (
        ", ".join("%s %d" % (thread_label(threads, k[0], k[1]), v) for k, v in crossings.items()) or "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
