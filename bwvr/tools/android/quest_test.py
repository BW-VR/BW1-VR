#!/usr/bin/env python3
"""The test run with no player on a Quest (docs/android.md, "The test run with no player").

  python quest_test.py run <test.json> <folder> [--save <name>]   the whole test run
  python quest_test.py finish                                     the end of a run now
  python quest_test.py state                                      the guard and the display

The PC starts the app, and the app starts the game one time (auto_play=1) in a test save. The PC
changes the switches by bwvr-control.txt in turns, as frame_test.py does, gets the logs, and prints
the table of frame_test.py. quest_guard.sh on the Quest keeps the profile of the player and the
display safe, also when the PC or the network fails:

- Before the run: a copy of the profile, the Creature, bwvr.ini and the hint counts on the Quest,
  checked by MD5, and a second copy in local/quest-guard of the main checkout. Then the test save
  takes the place of slot 20, and slot 20 of the player waits beside the game folder.
- The display wakes only with a timer on the Quest. When the time is over, the timer stops the
  app, puts each file back, and lets the display go off.
- After the run the PC does the same at once, reads back that the display is off, and stops the
  timer only then.

A run starts only when the display of the Quest is off and neither the app nor the game runs, so
nobody uses the Quest. The test file is the one of frame_test.py, with two more keys: "ini" (the
keys of bwvr.ini for this run, for example a resolution) and "start_most_s" (the wait for the first
scene of the game, 240 s by default).

The serials: --serial <serial>, more than one time (the first that answers counts). A serial with
a colon is an address for ADB over the network, and the tool connects to it. With no --serial the
tool reads the serials from local/quest-serials.txt of the main checkout, one a line.
"""
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frame_test  # noqa: E402  The report and its tables.

APP = 'org.bwvr.quest'
ACTIVITY = APP + '/android.app.NativeActivity'
APP_FILES = f'/sdcard/Android/data/{APP}/files'
GAME = APP_FILES + '/game'
GUARD = '/data/local/tmp/bwvr-guard'
SCENE_LINE = 'the frames of the scene start'
# The Quest stops the session of the app when it takes the focus, for example for its window of
# the boundary when the cameras see too little light.
STOP_LINE = 'the session state is 6'
HERE = os.path.dirname(os.path.abspath(__file__))
# The main checkout: local/ of a git worktree is the one of the main checkout.
MAIN = subprocess.run(['git', '-C', HERE, 'rev-parse', '--path-format=absolute', '--git-common-dir'],
                      capture_output=True, text=True).stdout.strip()
MAIN = os.path.dirname(MAIN) if MAIN else os.path.abspath(os.path.join(HERE, '..', '..', '..'))
LOCAL = os.path.join(MAIN, 'local')
# The keys of each run. auto_play=1: the app starts "Play in VR", and the game continues slot 20,
# the test save. menu_skip_test_frame: the mod closes the Main Menu of the start. The logos take
# 38 s, and intro_skip_test_frame skips them. registry_test_guard=1: the game writes no value of
# the profile into the registry of the prefix.
RUN_KEYS = {
    'auto_play': 1, 'remote_control': 1, 'pose_log': 1,
    'menu_skip_test_frame': 70, 'intro_skip_test_frame': 5, 'registry_test_guard': 1,
}
SERIAL = None
ADB = None


def find_adb():
    for name in ('ANDROID_HOME', 'ANDROID_SDK_ROOT'):
        root = os.environ.get(name)
        if root and os.path.exists(os.path.join(root, 'platform-tools', 'adb.exe')):
            return os.path.join(root, 'platform-tools', 'adb.exe')
    # The SDK of a program from the store is in LocalCache of its package (android_env.ps1).
    packages = os.path.join(os.environ.get('LOCALAPPDATA', ''), 'Packages')
    if os.path.isdir(packages):
        for package in os.listdir(packages):
            path = os.path.join(packages, package, 'LocalCache', 'Local', 'Android', 'Sdk',
                                'platform-tools', 'adb.exe')
            if os.path.exists(path):
                return path
    return 'adb'


def say(text):
    print(time.strftime('%H:%M:%S'), text, flush=True)


def adb(*args, timeout=60, check=False):
    result = subprocess.run([ADB, '-s', SERIAL] + list(args), capture_output=True, text=True,
                            timeout=timeout)
    if check and result.returncode != 0:
        raise RuntimeError(f'adb {" ".join(args)}: {result.stderr.strip() or result.stdout.strip()}')
    return result.stdout.replace('\r', '')


def shell(command, timeout=60):
    return adb('shell', command, timeout=timeout)


def guard(command, timeout=300):
    return shell(f'sh {GUARD}/quest_guard.sh {command}', timeout=timeout).strip()


def try_shell(command, timeout=30):
    """A shell command that gives an empty text when ADB fails, for the end of a run. The timer
    on the Quest does the end also when the PC never gets through."""
    try:
        return shell(command, timeout=timeout)
    except (subprocess.SubprocessError, OSError) as e:
        say(f'ADB did not get through ({e.__class__.__name__}), the next try comes')
        if ':' in SERIAL:
            subprocess.run([ADB, 'connect', SERIAL], capture_output=True, timeout=20)
        return ''


def pick(serials):
    global SERIAL
    devices = ''
    for serial in serials:
        if ':' in serial:
            subprocess.run([ADB, 'connect', serial], capture_output=True, timeout=20)
    devices = subprocess.run([ADB, 'devices'], capture_output=True, text=True, timeout=20).stdout
    for serial in serials:
        if f'{serial}\tdevice' in devices:
            SERIAL = serial
            return
    raise SystemExit('NOT RUN: no Quest answers (' + ', '.join(serials) + ')')


def display():
    text = try_shell('dumpsys power 2>/dev/null | grep -m1 mWakefulness=; '
                     'dumpsys display 2>/dev/null | grep -m1 " mState="')
    wake = 'Awake' if 'mWakefulness=Awake' in text else 'Asleep' if 'mWakefulness=Asleep' in text \
        else 'Dozing' if 'Dozing' in text else '?'
    state = 'ON' if 'mState=ON' in text else 'OFF' if 'mState=OFF' in text else '?'
    return wake, state


def app_runs():
    return shell(f'pidof {APP}').strip() != ''


def game_runs():
    return shell("pgrep -f '[r]unblack'").strip() != ''


def push_text(text, path):
    with tempfile.NamedTemporaryFile('w', delete=False, newline='\n', suffix='.txt') as f:
        f.write(text)
        local = f.name
    try:
        adb('push', local, path + '.tmp', check=True)
        shell(f'mv {path}.tmp {path}')
    finally:
        os.remove(local)


class Control:
    """bwvr-control.txt on the Quest. Each step replaces the whole file (frame_test.Control)."""

    def __init__(self):
        self.serial = 0

    def send(self, step, keys=None):
        self.serial += 1
        lines = [f'serial={self.serial}', f'step={step}']
        lines += [f'{k}={v}' for k, v in (keys or {}).items()]
        push_text('\n'.join(lines) + '\n', GAME + '/bwvr-control.txt')


def with_keys(text, keys):
    """The text of bwvr.ini with each key in [bwvr], in place of a line of the same key."""
    lines = text.replace('\r\n', '\n').split('\n')
    out = []
    added = False
    for line in lines:
        name = line.split('=', 1)[0].strip()
        if '=' in line and name in keys and not line.lstrip().startswith(';'):
            continue
        out.append(line)
        if not added and line.strip().lower() == '[bwvr]':
            out += [f'{k}={v}' for k, v in keys.items()]
            added = True
    if not added:
        out = ['[bwvr]'] + [f'{k}={v}' for k, v in keys.items()] + out
    return '\r\n'.join(out)


def keep_copy(stamp):
    """The second copy of the guard on the PC, checked by MD5."""
    target = os.path.join(LOCAL, 'quest-guard', stamp)
    os.makedirs(target, exist_ok=True)
    adb('pull', f'{GUARD}/copy', target, timeout=600, check=True)
    adb('pull', f'{GUARD}/sums.txt', target, check=True)
    wrong = 0
    count = 0
    with open(os.path.join(target, 'sums.txt'), encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\n')
            if '  ' not in line:
                continue
            want, path = line.split('  ', 1)
            count += 1
            try:
                with open(os.path.join(target, 'copy', path), 'rb') as g:
                    if hashlib.md5(g.read()).hexdigest() != want:
                        wrong += 1
            except OSError:
                wrong += 1
    if wrong or not count:
        raise RuntimeError(f'the copy on the PC is not whole: {wrong} of {count} files differ')
    say(f'the second copy is on the PC: {count} files, each with its MD5, in {target}')


def wait_new_log(old_head, most_s=60.0):
    """Until the app has a new log: each start of the app writes a new bwvr-quest.log, and its
    first line has the time of the start."""
    limit = time.monotonic() + most_s
    while time.monotonic() < limit:
        head = try_shell(f'head -1 {APP_FILES}/bwvr-quest.log 2>/dev/null').strip()
        if head and head != old_head:
            return True
        time.sleep(1.0)
    say(f'the app wrote no new log in {most_s:.0f} s')
    return False


def wait_scene(most_s):
    """Until the log of this start of the app has the first scene of the game."""
    start = time.monotonic()
    limit = start + most_s
    log = APP_FILES + '/bwvr-quest.log'
    while time.monotonic() < limit:
        if try_shell(f'grep -c "{SCENE_LINE}" {log}').strip() not in ('', '0'):
            return True
        if stops_after() > 0:
            say('the Quest stopped the session of the app before the first scene')
            return False
        # The process of the app comes some seconds after am start.
        if time.monotonic() - start > 30.0 and not app_runs():
            say('the app ended before the first scene')
            return False
        time.sleep(3.0)
    say(f'no scene of the game in {most_s:.0f} s')
    return False


def stops_after():
    """How many times the session of the app stopped in this start of the app."""
    out = try_shell(f'grep -c "{STOP_LINE}" {APP_FILES}/bwvr-quest.log')
    return int(out.strip() or '0')


def run_steps(test, control, result):
    step_s = float(test.get('step_s', 8.0))
    cycle = test['cycle']
    limit = time.monotonic() + float(test.get('most_s', 240.0))
    index = 0
    while time.monotonic() < limit:
        if not game_runs():
            result['game_ended'] = True
            say('the game ended')
            return
        if stops_after() > 0:
            result['session_stopped'] = True
            wake, state = display()
            say(f'NOT MEASURED: the Quest stopped the session of the app (the display: {wake}, '
                f'{state}). A window of the system took the focus, for example the boundary '
                'when the cameras see too little light')
            return
        step = cycle[index % len(cycle)]
        index += 1
        control.send(step['step'], step.get('keys', {}))
        result['steps'] += 1
        say(f'the step "{step["step"]}"')
        time.sleep(step_s)


def finish():
    """Restore and sleep on the Quest. True when the display is off and the files are back."""
    started = False
    for _ in range(10):
        if try_shell(f'rm -f {GUARD}/finish.out; nohup sh {GUARD}/quest_guard.sh finish '
                     f'> {GUARD}/finish.out 2>&1 & echo started').strip() == 'started':
            started = True
            break
        time.sleep(3.0)
    if not started:
        say('the finish did not start on the Quest. Its timer does it when its time is over')
    restored = None
    for _ in range(60 if started else 0):
        time.sleep(3.0)
        out = try_shell(f'cat {GUARD}/finish.out 2>/dev/null')
        if 'finish:' in out:
            for line in out.splitlines():
                if line.startswith('restore'):
                    say(line)
                    if line.startswith('restore OK') or line.startswith('restore: no run'):
                        restored = True
                    elif restored is None:
                        restored = False
            break
    else:
        say('the finish on the Quest gave no answer in 180 s')
    off = False
    for attempt in range(24):
        wake, state = display()
        say(f'the display: {wake}, {state}')
        if state == 'OFF' and wake != 'Awake':
            off = True
            break
        if attempt in (2, 8, 16):
            try_shell('input keyevent KEYCODE_SLEEP')
            say('the sleep key went out')
        time.sleep(5.0)
    if off:
        try_shell(f'sh {GUARD}/quest_guard.sh disarm')
    else:
        say('NOT CONFIRMED: the display is not reported as off. The timer on the Quest stays armed')
    return off and restored is True


def fetch(folder):
    os.makedirs(os.path.join(folder, 'game'), exist_ok=True)
    for name in ('bwvr-pose.bin', 'bwvr-pictures.bin', 'bwvr-steps.csv', 'bwvr-quest.log'):
        adb('pull', f'{APP_FILES}/{name}', folder, timeout=300)
    for name in ('bwvr.log', 'bwvr-marks.csv'):
        adb('pull', f'{GAME}/{name}', os.path.join(folder, 'game'), timeout=300)


def run(test_path, folder, save, serials):
    with open(test_path) as f:
        test = json.load(f)
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, 'test.json'), 'w') as f:
        json.dump(test, f, indent=2)
    save_dir = os.path.join(LOCAL, 'test-saves', save, '20')
    if not os.path.exists(os.path.join(save_dir, '20.sav')):
        raise SystemExit(f'NOT RUN: there is no test save "{save}" in {os.path.dirname(save_dir)}')
    pick(serials)
    say(f'the Quest by {SERIAL}')
    wake, state = display()
    if state != 'OFF' or wake == 'Awake':
        raise SystemExit(f'NOT RUN: the display is {state} ({wake}). Somebody can use the Quest')
    if app_runs() or game_runs():
        raise SystemExit('NOT RUN: the app or the game runs. Somebody can use the Quest')
    shell(f'mkdir -p {GUARD}/save')
    adb('push', os.path.join(HERE, 'quest_guard.sh'), f'{GUARD}/quest_guard.sh', check=True)
    status = guard('status')
    if 'a run is on' in status:
        raise SystemExit('NOT RUN: a run before did not put the files back. Run: quest_test.py finish')
    out = guard('backup')
    say(out)
    if not out.startswith('backup OK'):
        raise SystemExit('NOT RUN: no copy of the profile')
    ok = False
    result = {'name': test.get('name', ''), 'steps': 0, 'game_ended': False, 'game_came': False}
    try:
        keep_copy(time.strftime('%Y%m%d-%H%M%S'))
        shell(f'rm -f {GUARD}/save/*')
        for name in sorted(os.listdir(save_dir)):
            adb('push', os.path.join(save_dir, name), f'{GUARD}/save/{name}', check=True)
        out = guard('testsave')
        say(out)
        if not out.startswith('testsave OK'):
            raise RuntimeError('no test save in slot 20')
        keys = dict(RUN_KEYS)
        keys.update(test.get('ini', {}))
        ini = shell(f'cat {GAME}/bwvr.ini')
        push_text(with_keys(ini, keys), GAME + '/bwvr.ini')
        with open(os.path.join(folder, 'bwvr.ini.run'), 'w', newline='') as f:
            f.write(with_keys(ini, keys))
        say('bwvr.ini has the keys of the run: ' + ', '.join(f'{k}={v}' for k, v in keys.items()))
        shell(f'rm -f {GAME}/bwvr-control.txt {GAME}/bwvr-marks.csv')
        start_most_s = float(test.get('start_most_s', 240.0))
        timer_s = int(min(start_most_s + float(test.get('most_s', 240.0)) + 120.0, 1800.0))
        out = guard(f'awake {timer_s}')
        say(out)
        if not out.startswith('awake:'):
            raise RuntimeError('the timer on the Quest did not start')
        old_head = shell(f'head -1 {APP_FILES}/bwvr-quest.log 2>/dev/null').strip()
        shell(f'am start -W -n {ACTIVITY}', timeout=120)
        say('the app starts')
        if wait_new_log(old_head) and wait_scene(start_most_s):
            result['game_came'] = True
            say('the first scene of the game is here')
            control = Control()
            first = test['cycle'][0]
            control.send('start', first.get('keys', {}))
            run_steps(test, control, result)
            if game_runs():
                control.send('end', test.get('end_keys', {}))
                time.sleep(2.0)
        ok = True
    except Exception as e:  # The finish runs in each case.
        say(f'FAULT: {e}')
    finally:
        # The finish comes first, so the display is on for the shortest time. The logs stay.
        clean = finish()
    fetch(folder)
    with open(os.path.join(folder, 'done'), 'w') as f:
        json.dump(result, f)
    say(f'the result: {result}')
    if not clean:
        say('RESULT: NOT CLEAN. Check the lines of the restore and of the display above')
        return 2
    say('RESULT: the files of the player are back, and the display is off')
    if ok and result['game_came']:
        print()
        frame_test.report(folder)
        return 0
    return 1


def serials_of(args):
    serials = [args[i + 1] for i, a in enumerate(args[:-1]) if a == '--serial']
    if serials:
        return serials
    path = os.path.join(LOCAL, 'quest-serials.txt')
    if os.path.exists(path):
        with open(path) as f:
            return [line.strip() for line in f if line.strip() and not line.startswith('#')]
    raise SystemExit(f'NOT RUN: give --serial, or put the serials into {path}')


def main():
    global ADB
    ADB = find_adb()
    args = sys.argv[1:]
    if not args:
        raise SystemExit(__doc__)
    what = args[0]
    if what == 'run' and len(args) >= 3:
        save = args[args.index('--save') + 1] if '--save' in args else 'pen'
        sys.exit(run(args[1], args[2], save, serials_of(args)))
    pick(serials_of(args))
    if what == 'finish':
        adb('push', os.path.join(HERE, 'quest_guard.sh'), f'{GUARD}/quest_guard.sh')
        sys.exit(0 if finish() else 2)
    if what == 'state':
        print(guard('status'))
        return
    raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
