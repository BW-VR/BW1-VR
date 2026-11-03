#!/usr/bin/env python3
"""Sums the own time of each bar on the frame thread of timeline files of the mod
(Trace Event Format, src/common/trace_events.h), split into the game and the
device, and names the largest own part of each long frame.

  python trace_self.py trace-20261103-141024.json
  python trace_self.py frames-folder --top 30 --long-ms 20 --list-long 10

Each argument is a file or a folder with trace-*.json files. The script reads
the frame thread of the process "game" (pid 1) and the row "frames (present to
present)".

The OWN TIME of a bar is its length less the length of the bars in it. A bar of
the group of the graphics engine (turn_parts.cpp, trace_device=1) has the
number device_us: the time in the calls of the device of the mod (vk_ddraw.cpp,
with the renderer and the record of the stream in them) inside the bar that no
bar in it covers. So the own time splits in two:

  device  the number device_us of the bar (never more than the own time)
  game    the rest of the own time

A bar shorter than trace_min_us is not in the file. Its time stays in the bar
that called it, and its device calls are in the bar that called it too. The
log of the game has the table of all timers with the calls a frame and the
bars that the file does not have ("turn parts   : graphics timers").

The script prints:

  1. The frames of the game: the count, the median, the 90th and the 99th
     percentile of the length, and the frames over --long-ms.
  2. The own time of each bar name, a game frame as a mean: the game part, the
     device part, the bars a frame and the draws, state calls and textures a
     frame (the numbers of the bars). The --top bars with the most own time.
  3. The counters of the device of a frame ("device calls (a frame)", "device us
     (a frame)", "device other (a frame)"), as means.
  4. The largest own part of each frame over --long-ms: for each bar name how
     many long frames it led, and its mean own time, game part and device part
     in those frames. --list-long N prints the N longest long frames one by
     one, with the five largest own parts of each.

Only the standard library. The script does not change a file.
"""

import argparse
import collections
import glob
import json
import os
import sys

# The bars that are waits or frames, not work of the game.
SKIP_PREFIX = ("wait of the pace",)
SKIP_NAMES = ("frame",)


def load(path):
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    events = data["traceEvents"] if isinstance(data, dict) else data
    names = {}
    for e in events:
        if e.get("ph") == "M" and e.get("name") == "thread_name":
            names[(e["pid"], e["tid"])] = e["args"]["name"]
    frame_thread = [k for k, v in names.items() if v == "frame thread" and k[0] == 1]
    rows = [k for k, v in names.items() if "present to present" in v and k[0] == 1]
    if not frame_thread:
        return None
    ft = frame_thread[0]
    bars = [e for e in events if e.get("ph") == "X" and (e["pid"], e["tid"]) == ft]
    bars.sort(key=lambda e: (e["ts"], -e["dur"]))
    frames = sorted((e for e in events if e.get("ph") == "X" and (e["pid"], e["tid"]) in rows
                     and e.get("name") == "frame"), key=lambda e: e["ts"])
    counters = [e for e in events if e.get("ph") == "C" and e.get("pid") == 1]
    return bars, frames, counters


class Own:
    __slots__ = ("name", "start", "end", "own", "device", "draws", "states", "textures")

    def __init__(self, e):
        self.name = e["name"]
        self.start = e["ts"]
        self.end = e["ts"] + e["dur"]
        self.own = float(e["dur"])
        a = e.get("args") or {}
        self.device = float(a.get("device_us", 0.0))
        self.draws = float(a.get("draws", 0.0))
        self.states = float(a.get("state_calls", 0.0))
        self.textures = float(a.get("textures", 0.0))

    @property
    def game(self):
        return self.own - self.device_own

    @property
    def device_own(self):
        return min(self.device, max(self.own, 0.0))


def own_times(bars):
    """The own time of each bar: its length less the bars in it."""
    out, stack = [], []
    for e in bars:
        rec = Own(e)
        while stack and stack[-1].end <= rec.start:
            stack.pop()
        if stack:
            stack[-1].own -= e["dur"]
        stack.append(rec)
        out.append(rec)
    for r in out:
        if r.own < 0:
            r.own = 0.0
    return out


def percentile(sorted_values, q):
    if not sorted_values:
        return 0.0
    i = min(len(sorted_values) - 1, int(len(sorted_values) * q))
    return sorted_values[i]


def files_of(args):
    out = []
    for a in args:
        if os.path.isdir(a):
            out.extend(sorted(glob.glob(os.path.join(a, "trace-*.json"))))
        else:
            out.append(a)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="+", help="trace files or folders with trace-*.json")
    ap.add_argument("--top", type=int, default=25, help="bars to print (25)")
    ap.add_argument("--long-ms", type=float, default=20.0, help="a long frame is longer than this (20)")
    ap.add_argument("--culprits", type=int, default=12, help="bar names to print for the long frames (12)")
    ap.add_argument("--list-long", type=int, default=0, metavar="N",
                    help="print the N longest long frames one by one (0)")
    args = ap.parse_args()

    files = files_of(args.paths)
    if not files:
        sys.exit("no trace files")
    total = collections.Counter()
    device = collections.Counter()
    calls = collections.Counter()
    draws = collections.Counter()
    states = collections.Counter()
    textures = collections.Counter()
    culprit_n = collections.Counter()
    culprit_own = collections.Counter()
    culprit_dev = collections.Counter()
    frame_ms = []
    long_frames = []
    counter_sum = collections.defaultdict(lambda: collections.Counter())
    counter_n = collections.Counter()
    n_files = 0
    for path in files:
        got = load(path)
        if got is None:
            print("%s: no frame thread of the game" % path)
            continue
        n_files += 1
        bars, frames, counters = got
        rows = own_times(bars)
        for r in rows:
            total[r.name] += r.own
            device[r.name] += r.device_own
            calls[r.name] += 1
            draws[r.name] += r.draws
            states[r.name] += r.states
            textures[r.name] += r.textures
        for c in counters:
            counter_n[c["name"]] += 1
            for k, v in (c.get("args") or {}).items():
                counter_sum[c["name"]][k] += float(v)
        for fr in frames:
            frame_ms.append(fr["dur"] / 1000.0)
            if fr["dur"] / 1000.0 > args.long_ms:
                a, b = fr["ts"], fr["ts"] + fr["dur"]
                inside = [r for r in rows if a <= r.start < b and r.name not in SKIP_NAMES
                          and not r.name.startswith(SKIP_PREFIX)]
                if inside:
                    inside.sort(key=lambda r: -r.own)
                    long_frames.append((fr["dur"] / 1000.0, path, inside))
                    top = inside[0]
                    culprit_n[top.name] += 1
                    culprit_own[top.name] += top.own
                    culprit_dev[top.name] += top.device_own
    nframes = len(frame_ms)
    if nframes == 0:
        sys.exit("no frames in the files")
    frame_ms.sort()
    print("%d files, %d game frames, median %.2f ms, 90th %.2f, 99th %.2f, %d over %.0f ms" % (
        n_files, nframes, percentile(frame_ms, 0.5), percentile(frame_ms, 0.9),
        percentile(frame_ms, 0.99), len(long_frames), args.long_ms))
    have_device = sum(device.values()) > 0
    print("\nOWN time on the frame thread a game frame (mean), the %d bars with the most. "
          "game = own less device%s" % (args.top, "" if have_device else
                                         " (the file has no device_us: trace_device=0, or the "
                                         "backend is not Vulkan)"))
    print("  %8s %8s %8s %8s  %-8s %s" % ("own ms", "game ms", "dev ms", "bars", "draws/st/tx", "bar"))
    for name, v in total.most_common(args.top):
        if name in SKIP_NAMES or name.startswith(SKIP_PREFIX):
            continue
        n = calls[name]
        print("  %8.2f %8.2f %8.2f %8.2f  %-12s %s" % (
            v / 1000.0 / nframes, (v - device[name]) / 1000.0 / nframes,
            device[name] / 1000.0 / nframes, n / nframes,
            "%.0f/%.0f/%.0f" % (draws[name] / nframes, states[name] / nframes,
                                 textures[name] / nframes), name))
    body = sum(v for k, v in total.items() if k not in SKIP_NAMES and not k.startswith(SKIP_PREFIX))
    print("  %8.2f %8.2f %8.2f  all bars of the frame thread except the waits of the pace" % (
        body / 1000.0 / nframes, (body - sum(device.values())) / 1000.0 / nframes,
        sum(device.values()) / 1000.0 / nframes))

    if counter_n:
        print("\nThe counters of the device of a frame (mean of %d frames of the graphics engine):" %
              max(counter_n.values()))
        for name in sorted(counter_n):
            n = counter_n[name]
            print("  %-26s %s" % (name, ", ".join("%s %.1f" % (k, v / n)
                                                    for k, v in sorted(counter_sum[name].items()))))

    calls_n = counter_n.get("device us (a frame)", 0)
    if calls_n:
        device_us = sum(counter_sum["device us (a frame)"].values()) + \
            counter_sum["device other (a frame)"].get("us", 0.0)
        in_bars = sum(device.values())
        print("  the device in the whole graphics call %.3f ms a frame, in the bars above %.3f ms, "
              "outside them %.3f ms (the bars with no device_us: the scene walk, EndScene, the "
              "state blocks, FinishFrame)" % (
                  device_us / 1000.0 / calls_n, in_bars / 1000.0 / calls_n,
                  (device_us - in_bars) / 1000.0 / calls_n))

    print("\nThe largest own part in each frame over %.0f ms (%d frames):" % (args.long_ms, len(long_frames)))
    for name, k in culprit_n.most_common(args.culprits):
        print("  %4d frames  own %6.2f ms (game %6.2f, device %6.2f) as a mean  %s" % (
            k, culprit_own[name] / 1000.0 / k, (culprit_own[name] - culprit_dev[name]) / 1000.0 / k,
            culprit_dev[name] / 1000.0 / k, name))
    if args.list_long > 0:
        print("\nThe %d longest of them:" % args.list_long)
        for ms, path, inside in sorted(long_frames, key=lambda t: -t[0])[:args.list_long]:
            print("  %.1f ms  %s" % (ms, os.path.basename(path)))
            for r in inside[:5]:
                print("      own %6.2f ms (game %6.2f, device %6.2f)  %s" % (
                    r.own / 1000.0, r.game / 1000.0, r.device_own / 1000.0, r.name))


if __name__ == "__main__":
    main()
