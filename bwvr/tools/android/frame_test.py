#!/usr/bin/env python3
"""The test run on the Steam Frame (docs/android.md, "The test run").

The player only starts the game. A script on the headset changes the switches of the
debug page in turns while the game runs. The PC waits, gets the logs, and prints one
table with the numbers of each step.

On the PC:
  python frame_test.py run <test.json> <folder>   the whole test run
  python frame_test.py fetch <folder>             only get the logs
  python frame_test.py report <folder>            only print the table

On the headset (the run starts this):
  python3 frame_test.py conduct <test.json>

bwvr.ini of the headset needs remote_control=1 and pose_log=1.

A test goes through "cycle" in turns, or with "mode": "marks" through "stages": each stage
waits until the player clicks a stick, and the table has the scale of the world at the click.
"""
import json
import os
import statistics
import struct
import subprocess
import sys
import time

HOST = 'steamos@frame'
KEY = os.path.expanduser('~/.ssh/bwvr_frame')
APP_FILES = ('/home/steamos/.local/share/Steam/steamapps/compatdata/2729671027/external/'
             'Android/data/org.bwvr.quest/files')
PERF_LOGS = '.local/share/Steam/logs'
TEST_DIR = 'bwvr-test'
SCENE_LINE = b'the frames of the scene start'
WAIT_MOST_S = 43200.0  # how long the script waits for a game, if the test does not say


# --- on the headset --------------------------------------------------------------------

def game_runs():
    return subprocess.run(['pgrep', '-f', '[r]unblack'], stdout=subprocess.DEVNULL).returncode == 0


def wait_for_scene(log_path, most_s, say):
    """Until a game shows its first scene. A game that shows a scene now also counts."""
    limit = time.monotonic() + most_s
    def read_from(at):
        try:
            with open(log_path, 'rb') as f:
                f.seek(at)
                return f.read()
        except OSError:
            return b''
    def size_now():
        try:
            return os.path.getsize(log_path)
        except OSError:
            return 0
    if game_runs() and SCENE_LINE in read_from(0):
        say('a game with a scene runs now')
        return True
    at = size_now()
    while time.monotonic() < limit:
        size = size_now()
        if size < at:
            at = 0  # The app started a new log.
        if size > at:
            text = read_from(at)
            at += len(text)
            if SCENE_LINE in text and game_runs():
                say('the first scene of the game is here')
                return True
        time.sleep(1.0)
    say('no game came')
    return False


class Control:
    """The file of the remote control. Each step replaces the whole file."""

    def __init__(self, path):
        self.path = path
        self.serial = 0

    def send(self, step, keys=None, prompt=None):
        self.serial += 1
        lines = [f'serial={self.serial}', f'step={step}']
        for key, value in (keys or {}).items():
            lines.append(f'{key}={value}')
        if prompt is not None:
            lines.append(f'prompt={prompt}')
        temp = self.path + '.tmp'
        with open(temp, 'w') as f:
            f.write('\n'.join(lines) + '\n')
        os.chmod(temp, 0o666)
        os.replace(temp, self.path)

    def remove(self):
        try:
            os.remove(self.path)
        except OSError:
            pass


def run_cycle(test, control, result, say):
    """The steps in turns, each for step_s, until most_s ends."""
    step_s = float(test.get('step_s', 8.0))
    step_prompt_s = float(test.get('step_prompt_s', 0.0))
    cycle = test['cycle']
    limit = time.monotonic() + float(test.get('most_s', 240.0))
    index = 0
    while time.monotonic() < limit:
        if not game_runs():
            result['game_ended'] = True
            say('the game ended')
            return
        step = cycle[index % len(cycle)]
        index += 1
        prompt = step.get('prompt')
        control.send(step['step'], step.get('keys', {}), prompt)
        result['steps'] += 1
        say(step['step'])
        if prompt and step_prompt_s > 0.0:
            time.sleep(step_prompt_s)
            control.send(step['step'], None, '')
            time.sleep(max(step_s - step_prompt_s, 0.0))
        else:
            time.sleep(step_s)


def run_marks(test, control, result, say, marks_path):
    """Each stage ends with a click of a stick, or after stage_most_s."""
    def marks_now():
        try:
            with open(marks_path) as f:
                return sum(1 for _ in f)
        except OSError:
            return 0
    prompt_s = float(test.get('stage_prompt_s', 7.0))
    most_s = float(test.get('stage_most_s', 180.0))
    for stage in test['stages']:
        keys = dict(stage.get('keys', {}))
        keys['mark'] = 1
        count = marks_now()
        control.send(stage['step'], keys, stage.get('prompt', ''))
        result['steps'] += 1
        say(stage['step'])
        start = time.monotonic()
        hidden = False
        while time.monotonic() < start + most_s:
            if not game_runs():
                result['game_ended'] = True
                say('the game ended')
                return
            if not hidden and time.monotonic() >= start + prompt_s:
                control.send(stage['step'], None, '')
                hidden = True
            if marks_now() > count:
                say('a mark')
                break
            time.sleep(0.25)
        else:
            say('no mark in this stage')


def conduct(test_path):
    with open(test_path) as f:
        test = json.load(f)
    home = os.path.expanduser('~/' + TEST_DIR)
    done_path = os.path.join(home, 'done')
    if os.path.exists(done_path):
        os.remove(done_path)
    log = open(os.path.join(home, 'conduct.log'), 'w', buffering=1)
    def say(text):
        log.write(f'{time.monotonic():.3f} {text}\n')
    control = Control(os.path.join(APP_FILES, 'game', 'bwvr-control.txt'))
    control.remove()
    marks_path = os.path.join(APP_FILES, 'game', 'bwvr-marks.csv')
    if os.path.exists(marks_path):
        os.remove(marks_path)
    marks = test.get('mode') == 'marks'
    result = {'name': test.get('name', ''), 'steps': 0, 'game_ended': False, 'game_came': False}
    if wait_for_scene(os.path.join(APP_FILES, 'bwvr-quest.log'), test.get('wait_most_s', WAIT_MOST_S), say):
        result['game_came'] = True
        first = test['stages'][0] if marks else test['cycle'][0]
        control.send('start', first.get('keys', {}), test.get('start_prompt', ''))
        say('the start')
        time.sleep(float(test.get('start_prompt_s', 0.0)))
        control.send('start', None, '')
        if marks:
            run_marks(test, control, result, say, marks_path)
        else:
            run_cycle(test, control, result, say)
        if game_runs():
            end_keys = dict(test.get('end_keys', {}))
            if marks:
                end_keys['mark'] = 0
            control.send('end', end_keys, test.get('end_prompt', ''))
            say('the end')
            time.sleep(float(test.get('end_prompt_s', 0.0)))
            control.send('end', None, '')
            time.sleep(1.0)
    control.remove()
    with open(done_path + '.tmp', 'w') as f:
        json.dump(result, f)
    os.replace(done_path + '.tmp', done_path)
    say('done')


# --- on the PC ------------------------------------------------------------------------

def ssh(command, check=True, capture=True):
    result = subprocess.run(['ssh', '-i', KEY, '-o', 'ConnectTimeout=15', '-o', 'BatchMode=yes',
                             HOST, command],
                            stdout=subprocess.PIPE if capture else None, text=True)
    if check and result.returncode != 0:
        raise SystemExit(f'ssh failed with code {result.returncode}: {command}')
    return result.stdout if capture else ''


def scp_to(local_dir, name, remote):
    # scp reads a path with a drive letter as a host, so the path is relative.
    subprocess.run(['scp', '-q', '-i', KEY, name, f'{HOST}:{remote}'], cwd=local_dir, check=True)


def scp_from(remote, local_dir):
    # A missing log is no fault of the run.
    return subprocess.run(['scp', '-q', '-i', KEY, f'{HOST}:{remote}', '.'],
                          cwd=local_dir).returncode == 0


def fetch(folder):
    os.makedirs(os.path.join(folder, 'game'), exist_ok=True)
    for name in ('bwvr-pose.bin', 'bwvr-pictures.bin', 'bwvr-steps.csv', 'bwvr-quest.log'):
        scp_from(f'{APP_FILES}/{name}', folder)
    for name in ('bwvr.log', 'bwvr.ini', 'bwvr-marks.csv'):
        scp_from(f'{APP_FILES}/game/{name}', os.path.join(folder, 'game'))
    for name in ('test.json', 'conduct.log', 'done'):
        scp_from(f'{TEST_DIR}/{name}', folder)
    scp_from(f'{PERF_LOGS}/vrcompositor.txt', folder)
    newest = ssh(f'ls -t {PERF_LOGS}/perfrecording-steam.app.2729671027-*.csv | head -1',
                 check=False).strip()
    if newest:
        if scp_from(newest, folder):
            target = os.path.join(folder, 'perfrecording.csv')
            if os.path.exists(target):
                os.remove(target)
            os.rename(os.path.join(folder, os.path.basename(newest)), target)


def start_conduct():
    ssh(f'cd {TEST_DIR} && nohup python3 frame_test.py conduct test.json '
        f'> conduct.out 2>&1 < /dev/null & echo started')


def run(test_path, folder):
    here = os.path.dirname(os.path.abspath(__file__))
    test_dir = os.path.dirname(os.path.abspath(test_path))
    with open(test_path) as f:
        test = json.load(f)
    os.makedirs(folder, exist_ok=True)
    ssh(f'mkdir -p {TEST_DIR}; rm -f {TEST_DIR}/done; '
        f'pkill -f "[f]rame_test.py conduct"; true')
    scp_to(here, 'frame_test.py', f'{TEST_DIR}/frame_test.py')
    scp_to(test_dir, os.path.basename(test_path), f'{TEST_DIR}/test.json')
    start_conduct()
    print('The script on the headset waits for the game. Start the game now.', flush=True)
    # One hour more than the wait for the game, for the test itself.
    limit = time.monotonic() + float(test.get('wait_most_s', WAIT_MOST_S)) + 3600.0
    result = None
    while time.monotonic() < limit:
        time.sleep(10.0)
        # A short loss of the network does not end the run, and a new start of the
        # headset ends the script there, so it starts again.
        answer = subprocess.run(
            ['ssh', '-i', KEY, '-o', 'ConnectTimeout=10', '-o', 'BatchMode=yes', HOST,
             f'if test -f {TEST_DIR}/done; then cat {TEST_DIR}/done; '
             f'elif pgrep -f "[f]rame_test.py conduct" > /dev/null; then echo waits; '
             f'else echo gone; fi'],
            stdout=subprocess.PIPE, text=True)
        word = answer.stdout.strip() if answer.returncode == 0 else ''
        if word == 'gone':
            print('The script on the headset is gone. It starts again.', flush=True)
            start_conduct()
        elif word and word != 'waits':
            print('The test run ended:', word, flush=True)
            result = json.loads(word)
            break
    if result is None:
        print('The time for the test run ended, and the script on the headset did not end.', flush=True)
        return
    if not result.get('game_came'):
        print('No game came, so no test ran. The logs on the headset are from a run before, and '
              'this tool does not get them.', flush=True)
        return
    # The file of the pictures goes to disk one time each second.
    time.sleep(3.0)
    fetch(folder)
    report(folder)


# --- the table ------------------------------------------------------------------------

PICTURE = struct.Struct('<ddddIIffIIff')
POSE = struct.Struct('<dqII6f4f4f3f4xQII3f4f3f3ffIIff')
POSE_FIELDS = {'t': 0, 'fresh': 3, 'frame': 21, 'tick': 38, 'pic_tick': 39}
PICTURE_MAGIC = 0x53434950


def records(path, shape):
    with open(path, 'rb') as f:
        data = f.read()
    size = struct.unpack_from('<II', data, 0)[1]
    if size != shape.size:
        raise SystemExit(f'{path}: a record has {size} bytes, this tool reads {shape.size}')
    return [shape.unpack_from(data, at) for at in range(8, len(data) - size + 1, size)]


def percent(values, part):
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[int(part * (len(ordered) - 1))]


def read_steps(folder):
    steps = []
    with open(os.path.join(folder, 'bwvr-steps.csv')) as f:
        for line in f:
            parts = line.rstrip('\n').split(',', 2)
            if len(parts) == 3:
                steps.append((float(parts[0]), parts[2]))
    return steps


class Window:
    """The numbers of one step, with no first part of the step."""

    def __init__(self, name, start, end):
        self.name = name
        self.start = start
        self.end = end
        self.pictures = []
        self.frames = 0
        self.fresh = 0
        self.game_frames = set()

    def seconds(self):
        return (self.end - self.start) / 1000.0

    def scale(self):
        return statistics.median(p[10] for p in self.pictures) if self.pictures else 0.0


def windows_of(folder, skip_ms):
    steps = read_steps(folder)
    pictures = records(os.path.join(folder, 'bwvr-pictures.bin'), PICTURE)
    pose = records(os.path.join(folder, 'bwvr-pose.bin'), POSE)
    windows = []
    for i, (at, name) in enumerate(steps):
        if name in ('start', 'end'):
            continue
        end = steps[i + 1][0] if i + 1 < len(steps) else at + 8000.0
        if end - at - skip_ms > 500.0:
            windows.append(Window(name, at + skip_ms, end))
    w = 0
    for p in pictures:
        while w < len(windows) and p[0] >= windows[w].end:
            w += 1
        if w == len(windows):
            break
        if p[0] >= windows[w].start:
            windows[w].pictures.append(p)
    w = 0
    for r in pose:
        t = r[POSE_FIELDS['t']]
        while w < len(windows) and t >= windows[w].end:
            w += 1
        if w == len(windows):
            break
        if t >= windows[w].start:
            windows[w].frames += 1
            windows[w].fresh += 1 if r[POSE_FIELDS['fresh']] else 0
            windows[w].game_frames.add(r[POSE_FIELDS['frame']])
    # A window with few frames is the end of the game, not a result of its step.
    return [x for x in windows if x.frames / max(x.seconds(), 0.001) >= 30.0]


def row(label, group):
    seconds = sum(x.seconds() for x in group)
    pictures = [p for x in group for p in x.pictures]
    frames = sum(x.frames for x in group)
    fresh = sum(x.fresh for x in group)
    game = sum(len(x.game_frames) for x in group)
    done = [p[3] - p[0] for p in pictures if p[3] > 0.0]
    play = [p[2] - p[1] for p in pictures]
    gpu = [p[7] for p in pictures if p[7] > 0.0]
    draws = [p[8] + p[9] for p in pictures]
    compose = [p[11] for p in pictures if p[11] > 0.0]
    print(f'{label:26s} {len(group):4d} {seconds:6.0f} {statistics.mean(draws) if draws else 0:7.0f} '
          f'{frames / max(seconds, 0.001):7.1f} {fresh / max(seconds, 0.001):7.1f} '
          f'{game / max(seconds, 0.001):6.1f} {percent(play, 0.5):6.2f} {percent(gpu, 0.5):6.2f} '
          f'{percent(compose, 0.5):6.2f} {percent(done, 0.5):6.2f} {percent(done, 0.9):6.2f}')


HEAD = (f'{"":26s} {"runs":>4s} {"s":>6s} {"draws":>7s} {"frames":>7s} {"new":>7s} '
        f'{"game":>6s} {"play":>6s} {"GPU":>6s} {"comp":>6s} {"done50":>6s} {"done90":>6s}')
UNITS = ('frames: frames of the session a second, new: new pictures a second, game: frames of '
         'the game a second,\nplay and GPU: ms for a walk (50th percentile), comp: GPU ms of the '
         'compose, done: ms from the tick to the end of the GPU (50th and 90th percentile)')


def report(folder):
    skip_s = 1.5
    test_path = os.path.join(folder, 'test.json')
    if os.path.exists(test_path):
        with open(test_path) as f:
            skip_s = float(json.load(f).get('skip_s', skip_s))
    windows = windows_of(folder, skip_s * 1000.0)
    if not windows:
        print('No step has records. Check remote_control=1 and pose_log=1 in bwvr.ini.')
        return
    names = []
    for x in windows:
        if x.name not in names:
            names.append(x.name)
    print(UNITS)
    print()
    print('All windows of a step')
    print(HEAD)
    for name in names:
        row(name, [x for x in windows if x.name == name])
    # The player changes the size of the world during a test, so a compare needs
    # windows with about the same size.
    bands = [(0, 20), (20, 60), (60, 135), (135, 300), (300, 1 << 30)]
    for low, high in bands:
        band = [x for x in windows if low <= x.scale() < high]
        if not band:
            continue
        print()
        print(f'A world of {low} to {high if high < 1 << 30 else "more"} units a meter')
        print(HEAD)
        for name in names:
            group = [x for x in band if x.name == name]
            if group:
                row(name, group)
    print_marks(folder)
    print_perf(folder)
    print_lists(folder)
    print_remote_faults(folder)


def print_marks(folder):
    path = os.path.join(folder, 'game', 'bwvr-marks.csv')
    if not os.path.exists(path):
        return
    print()
    print('The clicks of the player')
    with open(path, errors='replace') as f:
        for line in f:
            parts = line.strip().split(',')
            if len(parts) == 3:
                print(f'  the factor {float(parts[2]):.2f} disturbs at {float(parts[1]):.0f} units a meter')


def print_perf(folder):
    path = os.path.join(folder, 'perfrecording.csv')
    if not os.path.exists(path):
        return
    totals = {}
    with open(path, errors='replace') as f:
        for line in f:
            parts = line.strip().split(',')
            if len(parts) == 2 and parts[0] in ('totalFrames', 'reprojectedFrames'):
                totals[parts[0]] = int(parts[1])
            if len(totals) == 2:
                break
    if len(totals) == 2 and totals['totalFrames'] > 0:
        print()
        print(f'SteamVR, the whole run: {totals["totalFrames"]} frames, '
              f'{totals["reprojectedFrames"]} reprojected '
              f'({100.0 * totals["reprojectedFrames"] / totals["totalFrames"]:.1f} percent)')


def print_lists(folder):
    path = os.path.join(folder, 'bwvr-quest.log')
    if not os.path.exists(path):
        return
    lines = []
    with open(path, errors='replace') as f:
        for line in f:
            if 'a list of the' in line:
                lines.append(line.strip())
    if not lines:
        return
    # The lines of the outer image with the most draws show the worst view.
    def draws_of(line):
        try:
            return int(line.split(' image: ')[1].split(' draws')[0])
        except (IndexError, ValueError):
            return 0
    outer = sorted((x for x in lines if 'outer image' in x), key=draws_of)
    center = sorted((x for x in lines if 'centre image' in x), key=draws_of)
    print()
    print('The lists with the most draws')
    for x in outer[-2:] + center[-2:]:
        print(x)


def print_remote_faults(folder):
    path = os.path.join(folder, 'game', 'bwvr.log')
    if not os.path.exists(path):
        return
    changes = 0
    faults = []
    with open(path, errors='replace') as f:
        for line in f:
            if 'remote       :' not in line:
                continue
            if 'is ' in line and 'now, it was' in line:
                changes += 1
            if 'cannot change' in line or 'no switch has' in line:
                faults.append(line.strip())
    print()
    print(f'The remote control: {changes} changes of switches, {len(faults)} faults')
    for x in faults[:5]:
        print(x)


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    what = sys.argv[1]
    if what == 'conduct':
        conduct(sys.argv[2])
    elif what == 'run' and len(sys.argv) >= 4:
        run(sys.argv[2], sys.argv[3])
    elif what == 'fetch':
        fetch(sys.argv[2])
    elif what == 'report':
        report(sys.argv[2])
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
