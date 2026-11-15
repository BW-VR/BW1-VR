#!/system/bin/sh
# The guard of a test run with no player on a Quest (docs/android.md, "The test run with no
# player"). It runs on the Quest, so it acts also when the PC or the network fails.
# quest_test.py copies it to /data/local/tmp/bwvr-guard and calls it.
#
#   sh quest_guard.sh backup         a copy of the profile, the Creature, bwvr.ini and the hint
#                                    counts, checked by MD5
#   sh quest_guard.sh testsave       the test save of the folder save in the place of slot 20
#   sh quest_guard.sh awake SECONDS  the timer first, then the sensor says that the Quest is worn
#   sh quest_guard.sh finish         restore, then the display may go off, and the Quest sleeps
#   sh quest_guard.sh restore        stops the app and puts each file of the copy back
#   sh quest_guard.sh disarm         stops the timer
#   sh quest_guard.sh status
#
# The display of a Quest that nobody wears must not stay on: an LCD can get burn-in. So the only
# way to wake the display is "awake", and it starts the timer before the sensor. The timer does
# "finish" after SECONDS. Android on the Quest turns the display off only after 24 hours.
APP=org.bwvr.quest
GAME=/sdcard/Android/data/$APP/files/game
GUARD=/data/local/tmp/bwvr-guard
SELF=$GUARD/quest_guard.sh
COPY=$GUARD/copy
SAVE=$GUARD/save
# Slot 20 of the player waits here during a run: the same file system as the game folder, so a
# move keeps each file as it is, with its owner.
SIDE=/sdcard/Android/data/$APP/files/bwvr-guard-slot20
LOG=$GUARD/guard.log
LOCK=$GUARD/lock
TREES="Profiles Scripts/CreatureMind"
FILES="bwvr.ini bwvr-hint-counts.ini"
AWAKE="am broadcast -a com.oculus.vrpowermanager.prox_close"
RESET="am broadcast -a com.oculus.vrpowermanager.automation_disable"

mkdir -p $GUARD

say() {
  echo "$(date +%T) $*" >> $LOG
  echo "$*"
}

# One time at a time: a second call waits, and a lock of a process that ended counts for nothing.
take_lock() {
  n=0
  while ! mkdir $LOCK 2>/dev/null; do
    p=$(cat $LOCK/pid 2>/dev/null)
    if [ -z "$p" ] || ! kill -0 "$p" 2>/dev/null; then
      rm -rf $LOCK
      continue
    fi
    n=$((n + 1))
    if [ $n -gt 120 ]; then
      say "busy: the process $p has the lock"
      exit 9
    fi
    sleep 1
  done
  echo $$ > $LOCK/pid
  trap 'rm -rf $LOCK' EXIT
}

# The files of the guard under a root, one path a line, sorted.
list_files() {
  (cd "$1" && { find $TREES -type f 2>/dev/null
                for f in $FILES; do [ -f "$f" ] && echo "$f"; done; } | sort)
}

list_dirs() {
  (cd "$1" && find $TREES -type d 2>/dev/null | sort)
}

# The MD5 of each file of the list on stdin, under a root.
sums() {
  (cd "$1" && while IFS= read -r f; do md5sum "$f"; done)
}

# The mode, the owner and the group of each file of the list on stdin, under a root, sorted
# for comm.
modes() {
  (cd "$1" && while IFS= read -r f; do stat -c '%a %U %G %n' "$f"; done | sort)
}

game_runs() {
  pidof $APP >/dev/null 2>&1 || pgrep -f '[r]unblack' >/dev/null 2>&1
}

# A start of the app that waited can come some seconds after a stop. So the stop holds only when
# no process of the app or of the game shows for 5 s in a row, for at most 60 s.
stop_all() {
  quiet=0
  n=0
  while [ $quiet -lt 5 ] && [ $n -lt 60 ]; do
    if game_runs; then
      am force-stop $APP
      quiet=0
    else
      quiet=$((quiet + 1))
    fi
    n=$((n + 1))
    sleep 1
  done
  [ $quiet -ge 5 ]
}

backup() {
  if [ -f $GUARD/ACTIVE ]; then
    say "backup NOT DONE: a run before did not put the files back. Run restore first"
    return 3
  fi
  if game_runs; then
    say "backup NOT DONE: the app or the game runs"
    return 4
  fi
  list_files "$GAME" > $GUARD/files.txt
  list_dirs "$GAME" > $GUARD/dirs.txt
  if ! grep -q '^Profiles/.*/' $GUARD/files.txt; then
    say "backup NOT DONE: the game folder has no file of a profile"
    return 5
  fi
  rm -rf $GUARD/copy.old
  [ -d $COPY ] && mv $COPY $GUARD/copy.old
  if [ -e "$SIDE" ]; then
    say "backup NOT DONE: $SIDE is there from a run before. Run restore first"
    return 3
  fi
  mkdir -p $COPY
  while IFS= read -r f; do
    mkdir -p "$COPY/$(dirname "$f")"
    if ! cp --preserve=mt "$GAME/$f" "$COPY/$f"; then
      say "backup NOT DONE: no copy of $f"
      return 6
    fi
  done < $GUARD/files.txt
  sums "$GAME" < $GUARD/files.txt > $GUARD/sums.txt
  sums "$COPY" < $GUARD/files.txt > $GUARD/copy-sums.txt
  modes "$GAME" < $GUARD/files.txt > $GUARD/modes.txt
  rm -f $GUARD/slot.txt
  if ! cmp -s $GUARD/sums.txt $GUARD/copy-sums.txt; then
    say "backup NOT DONE: the copy differs from the game folder"
    return 7
  fi
  touch $GUARD/ACTIVE
  say "backup OK: $(wc -l < $GUARD/files.txt) files and $(wc -l < $GUARD/dirs.txt) folders, each copy with the MD5 of its file"
}

# The one folder of a profile. Empty when there is none or more than one.
profile() {
  n=$(ls -1 "$GAME/Profiles" | wc -l)
  [ "$n" -eq 1 ] && ls -1 "$GAME/Profiles"
}

testsave() {
  if [ ! -f $GUARD/ACTIVE ]; then
    say "testsave NOT DONE: no backup of this run"
    return 3
  fi
  name=$(profile)
  if [ -z "$name" ]; then
    say "testsave NOT DONE: the game folder needs exactly one profile"
    return 5
  fi
  if [ ! -f "$SAVE/20.sav" ]; then
    say "testsave NOT DONE: the folder of the test save has no 20.sav"
    return 6
  fi
  slot="$GAME/Profiles/$name/Saved Games/20"
  if [ -e "$SIDE" ]; then
    say "testsave NOT DONE: $SIDE is there already"
    return 3
  fi
  echo "$slot" > $GUARD/slot.txt
  if [ -d "$slot" ] && ! mv "$slot" "$SIDE"; then
    say "testsave NOT DONE: slot 20 of the player does not move to $SIDE"
    return 4
  fi
  mkdir -p "$slot"
  cp --preserve=mt "$SAVE"/* "$slot/"
  want=$(cd "$SAVE" && md5sum * | sort)
  got=$(cd "$slot" && md5sum * | sort)
  if [ "$want" != "$got" ]; then
    say "testsave NOT DONE: the files in slot 20 differ from the test save"
    return 7
  fi
  say "testsave OK: $(ls -1 "$slot" | wc -l) files of the test save in slot 20 of the profile $name"
}

# The restore again while the app or the game came up during a restore, at most 3 times.
restore() {
  rm -f "$GAME/bwvr-control.txt"
  if [ ! -f $GUARD/ACTIVE ]; then
    say "restore: no run changed the files, nothing to put back"
    return 0
  fi
  for turn in 1 2 3; do
    restore_once
    result=$?
    [ $result -ne 10 ] && return $result
  done
  say "restore NOT OK: the app or the game came up again in each of 3 restores"
  return 10
}

restore_once() {
  if ! stop_all; then
    say "restore NOT DONE: the app or the game still runs after the stop"
    return 4
  fi
  # Slot 20 of the player comes back as it was, by a move.
  slot=$(cat $GUARD/slot.txt 2>/dev/null)
  if [ -n "$slot" ] && [ -d "$SIDE" ]; then
    rm -rf "$slot"
    if ! mv "$SIDE" "$slot"; then
      say "restore NOT DONE: slot 20 of the player does not move back from $SIDE"
      return 5
    fi
  fi
  while IFS= read -r d; do mkdir -p "$GAME/$d"; done < $GUARD/dirs.txt
  # A file that is there gets the bytes of the copy and keeps its owner and its mode. Only the
  # owner can set the time of a file, so a file of the app keeps the time of the restore. A file
  # that is not there is a new file of the shell, so the app gets read and write for it.
  back=0
  new=0
  while IFS= read -r line; do
    want=${line%%  *}
    f=${line#*  }
    now=$(cd "$GAME" && md5sum "$f" 2>/dev/null)
    if [ "${now%%  *}" != "$want" ]; then
      if [ -f "$GAME/$f" ]; then
        cat "$COPY/$f" > "$GAME/$f"
        touch -r "$COPY/$f" "$GAME/$f" 2>/dev/null
      else
        mkdir -p "$GAME/$(dirname "$f")"
        cp --preserve=mt "$COPY/$f" "$GAME/$f"
        chgrp ext_data_rw "$GAME/$f"
        chmod 666 "$GAME/$f"
        new=$((new + 1))
      fi
      back=$((back + 1))
    fi
  done < $GUARD/sums.txt
  # A file of the shell that the app wrote again is a file of the app now. It comes back as a new
  # file of the shell, with the mode and the group of the copy.
  modes "$GAME" < $GUARD/files.txt > $GUARD/now-modes.txt
  comm -13 $GUARD/modes.txt $GUARD/now-modes.txt > $GUARD/other-modes.txt
  while IFS= read -r m; do
    f=${m#* * * }
    old=$(grep -F " $f" $GUARD/modes.txt | while IFS= read -r o; do
            [ "${o#* * * }" = "$f" ] && echo "$o"; done)
    set -- $old
    if [ "$2" = shell ]; then
      rm -f "$GAME/$f"
      cp --preserve=mt "$COPY/$f" "$GAME/$f"
      chgrp "$3" "$GAME/$f"
      chmod "$1" "$GAME/$f"
    fi
  done < $GUARD/other-modes.txt
  list_files "$GAME" > $GUARD/now.txt
  gone=0
  comm -13 $GUARD/files.txt $GUARD/now.txt > $GUARD/extra.txt
  while IFS= read -r f; do
    rm -f "$GAME/$f"
    gone=$((gone + 1))
  done < $GUARD/extra.txt
  list_dirs "$GAME" > $GUARD/now-dirs.txt
  comm -13 $GUARD/dirs.txt $GUARD/now-dirs.txt | sort -r > $GUARD/extra-dirs.txt
  while IFS= read -r d; do rmdir "$GAME/$d"; done < $GUARD/extra-dirs.txt
  list_files "$GAME" > $GUARD/now.txt
  list_dirs "$GAME" > $GUARD/now-dirs.txt
  sums "$GAME" < $GUARD/files.txt > $GUARD/now-sums.txt
  if game_runs; then
    say "restore: the app or the game came up during the restore, so it stops them and goes again"
    return 10
  fi
  if cmp -s $GUARD/files.txt $GUARD/now.txt && cmp -s $GUARD/dirs.txt $GUARD/now-dirs.txt &&
     cmp -s $GUARD/sums.txt $GUARD/now-sums.txt; then
    rm -f $GUARD/ACTIVE $GUARD/slot.txt
    modes "$GAME" < $GUARD/files.txt > $GUARD/now-modes.txt
    comm -13 $GUARD/modes.txt $GUARD/now-modes.txt > $GUARD/other-modes.txt
    while IFS= read -r m; do say "restore: another mode or owner now: $m"; done < $GUARD/other-modes.txt
    say "restore OK: $back files back from the copy ($new of them new, with read and write for the app), $gone files of the run removed, and each of the $(wc -l < $GUARD/files.txt) files has its MD5 of the copy. $(wc -l < $GUARD/other-modes.txt) files have another mode or owner than before"
    return 0
  fi
  say "restore NOT OK: the game folder differs from the copy. The copy stays in $COPY"
  return 8
}

finish() {
  restore
  result=$?
  $RESET > /dev/null
  sleep 3
  input keyevent KEYCODE_SLEEP
  say "finish: the restore gave $result, the reset of the sensor and the sleep key went out"
  return $result
}

disarm() {
  p=$(cat $GUARD/timer.pid 2>/dev/null)
  if [ -n "$p" ]; then
    pkill -P "$p" 2>/dev/null
    kill "$p" 2>/dev/null
    rm -f $GUARD/timer.pid
    say "the timer is stopped (process $p)"
  fi
}

case "$1" in
  backup) take_lock; backup ;;
  testsave) take_lock; testsave ;;
  restore) take_lock; restore ;;
  finish) take_lock; finish ;;
  awake)
    seconds=${2:-0}
    if [ "$seconds" -lt 30 ] || [ "$seconds" -gt 1800 ]; then
      say "awake NOT DONE: the time of the timer must be 30 to 1800 s"
      exit 2
    fi
    disarm
    nohup sh $SELF timer "$seconds" > /dev/null 2>&1 &
    echo $! > $GUARD/timer.pid
    sleep 1
    if ! kill -0 "$(cat $GUARD/timer.pid)" 2>/dev/null; then
      say "awake NOT DONE: the timer did not start"
      exit 3
    fi
    $AWAKE > /dev/null
    say "awake: the timer (process $(cat $GUARD/timer.pid)) does finish after $seconds s, and the sensor says now that the Quest is worn"
    ;;
  timer)
    sleep "$2"
    rm -f $GUARD/timer.pid
    say "the timer fires"
    take_lock
    finish
    ;;
  disarm) disarm ;;
  status)
    p=$(cat $GUARD/timer.pid 2>/dev/null)
    [ -f $GUARD/ACTIVE ] && echo "files: a run is on, and restore puts the copy of $COPY back" || echo "files: no run is on"
    [ -n "$p" ] && kill -0 "$p" 2>/dev/null && echo "timer: armed (process $p)" || echo "timer: off"
    dumpsys power 2>/dev/null | grep -m1 mWakefulness=
    dumpsys display 2>/dev/null | grep -m1 ' mState='
    tail -5 $LOG 2>/dev/null
    ;;
  *) echo "usage: sh quest_guard.sh backup|testsave|awake SECONDS|finish|restore|disarm|status"; exit 2 ;;
esac
