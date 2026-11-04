#!/usr/bin/env python3
"""Reads bwvr-pose.bin of the headset app (pose_log=1, docs/android.md, "The late
camera") and names the frames of the session that do not fit the frames around
them: a turn of the head that jumps, a place of the head that jumps, a wrong
prediction of the runtime, a picture of an old slot, and a move of the world
that jumps.

    python pose_report.py bwvr-pose.bin [--lines 30]
"""
import argparse
import math
import struct
import sys

FORMAT = '<dqII' + '6f' + '4f' + '4f' + '3f' + '4x' + 'QII' + '3f' + '4f' + '3f' + '3f' + 'f' + 'IIff'
NAMES = ('t', 'display', 'slot', 'fresh', 'lx', 'ly', 'lz', 'rx', 'ry', 'rz', 'qx', 'qy', 'qz', 'qw',
         'fl', 'fr', 'fu', 'fd', 'nx', 'ny', 'nz', 'frame', 'input', 'again', 'px', 'py', 'pz',
         'pqx', 'pqy', 'pqz', 'pqw', 'wx', 'wy', 'wz', 'mx', 'my', 'mz', 'wait',
         'tick', 'pic_tick', 'take_wait', 'pic_work')


def angle(a, b):
    d = abs(sum(x * y for x, y in zip(a, b)))
    return 2.0 * math.degrees(math.acos(min(1.0, d)))


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def percentile(v, q):
    s = sorted(v)
    return s[int(q * (len(s) - 1))] if s else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('file')
    ap.add_argument('--lines', type=int, default=30)
    args = ap.parse_args()
    data = open(args.file, 'rb').read()
    magic, size = struct.unpack_from('<II', data, 0)
    if magic != 0x474F4C50:
        sys.exit('no pose log')
    rec = struct.Struct(FORMAT)
    if rec.size != size:
        print(f'note: a record has {size} bytes here and {rec.size} in this tool')
    rows = []
    for at in range(8, len(data) - size + 1, size):
        rows.append(dict(zip(NAMES, rec.unpack_from(data, at)[:len(NAMES)])))
    print(f'{len(rows)} frames of the session, {(rows[-1]["t"] - rows[0]["t"]) / 1000.0:.1f} s' if rows else 'no records')
    if len(rows) < 3:
        return
    t0 = rows[0]['t']
    turn, step, pred, age, moves, pic_turn, dt_disp, mv_step = [], [], [], [], [], [], [], []
    events = []
    for i in range(1, len(rows)):
        a, b = rows[i - 1], rows[i]
        q0 = (a['qx'], a['qy'], a['qz'], a['qw'])
        q1 = (b['qx'], b['qy'], b['qz'], b['qw'])
        ha = ((a['lx'] + a['rx']) / 2, (a['ly'] + a['ry']) / 2, (a['lz'] + a['rz']) / 2)
        hb = ((b['lx'] + b['rx']) / 2, (b['ly'] + b['ry']) / 2, (b['lz'] + b['rz']) / 2)
        slots = max(b['slot'] - a['slot'], 1)
        tu = angle(q0, q1) / slots
        st = dist(ha, hb) / slots * 100.0
        pe = dist((a['nx'], a['ny'], a['nz']), hb) * 100.0
        ag = b['slot'] - b['input']
        pq = (b['pqx'], b['pqy'], b['pqz'], b['pqw'])
        pt = angle(pq, q1)
        mv = math.sqrt(b['mx'] ** 2 + b['my'] ** 2 + b['mz'] ** 2) * 100.0
        ma = math.sqrt(a['mx'] ** 2 + a['my'] ** 2 + a['mz'] ** 2) * 100.0
        ms = math.sqrt((b['mx'] - a['mx']) ** 2 + (b['my'] - a['my']) ** 2 + (b['mz'] - a['mz']) ** 2) * 100.0
        dd = (b['display'] - a['display']) / 1e6
        turn.append(tu); step.append(st); pred.append(pe); age.append(ag); moves.append(mv)
        pic_turn.append(pt); dt_disp.append(dd); mv_step.append(ms)
        events.append(dict(i=i, t=(b['t'] - t0) / 1000.0, turn=tu, step=st, pred=pe, age=ag, pic_turn=pt, move=mv, mstep=ms, fresh=b['fresh'],
                           again=b['again'], frame=b['frame'], slot=b['slot'], dd=dd, wait=b['wait'],
                           fl=b['fl'], fr=b['fr'], fu=b['fu'], fd=b['fd']))

    def line(name, v, unit):
        print(f'{name:44s} median {percentile(v, .5):8.3f}  99th {percentile(v, .99):8.3f}  most {max(v):8.3f} {unit}')
    line('the turn of the head from a slot to the next', turn, 'degrees')
    line('the step of the head from a slot to the next', step, 'cm')
    line('the error of the prediction of the head', pred, 'cm')
    line('the age of the shown picture', age, 'slots')
    line('the turn from the picture to the head', pic_turn, 'degrees')
    line('the move of the world in the picture', moves, 'cm')
    line('the change of that move from a frame to the next', mv_step, 'cm')
    line('the display time from a frame to the next', dt_disp, 'ms')

    # A frame that does not fit its neighbours: over 3 times the median of the 15
    # frames before and after it, and over a floor.
    def odd(key, floor):
        out = []
        for i, e in enumerate(events):
            around = [x[key] for x in events[max(0, i - 15):i] + events[i + 1:i + 16]]
            if len(around) < 8:
                continue
            m = sorted(around)[len(around) // 2]
            if e[key] > floor and e[key] > 3.0 * m:
                out.append(e)
        return out

    def bursts(items, gap=0.12):
        groups = []
        for e in items:
            if groups and e['t'] - groups[-1][-1]['t'] <= gap:
                groups[-1].append(e)
            else:
                groups.append([e])
        return groups

    for key, floor, title in (('turn', 0.35, 'a jump of the turn of the head (degrees for a slot)'),
                              ('step', 0.25, 'a jump of the place of the head (cm for a slot)'),
                              ('pred', 0.5, 'a wrong prediction of the head (cm)'),
                              ('mstep', 1.0, 'a jump of the move of the world (cm)'),
                              ('pic_turn', 2.0, 'a picture far from the head (degrees)'),
                              ('age', 5, 'an old picture (slots)')):
        items = odd(key, floor)
        groups = bursts(items)
        print(f'\n{title}: {len(items)} frames in {len(groups)} bursts')
        for g in sorted(groups, key=lambda g: -max(e[key] for e in g))[:args.lines]:
            top = max(g, key=lambda e: e[key])
            print(f'  at {g[0]["t"]:7.2f} s, {len(g):2d} frames in {(g[-1]["t"] - g[0]["t"]) * 1000:5.0f} ms: '
                  f'most {top[key]:6.2f}, age {top["age"]}, turn {top["turn"]:.2f}, step {top["step"]:.3f}, pred {top["pred"]:.3f}, '
                  f'move {top["move"]:.2f}, new picture {top["fresh"]}, list again {top["again"]}, wait {top["wait"]:.1f} ms')

    no_new = [e for e in events if not e['fresh']]
    groups = bursts(no_new, gap=0.03)
    print(f'\nframes with no new picture: {len(no_new)} in {len(groups)} runs, the longest {max((len(g) for g in groups), default=0)} frames')
    runs = [e for e in events if e['again']]
    groups = bursts(runs, gap=0.03)
    long_runs = sorted(groups, key=len, reverse=True)[:8]
    print(f'frames with the list again: {len(runs)} in {len(groups)} runs, the longest: ' +
          ', '.join(f'{len(g)} at {g[0]["t"]:.1f} s' for g in long_runs))
    fov_changes = sum(1 for a, b in zip(events, events[1:]) if abs(a['fl'] - b['fl']) > 0.002 or abs(a['fu'] - b['fu']) > 0.002)
    print(f'frames in which the runtime changed the angles of the view: {fov_changes}')


if __name__ == '__main__':
    main()
