# Job 91: the swing of 100 ms, the corrected panel logs, four slap steps (agent/creature 03369fb)

`.\dev.ps1 build -Clean`: 6611 checks, 0 failed. Each log has "build 03369fb". `creature_recipe.ps1`, test save temple, `test_frame_rate=60`, `card_send_test_save=0`, `subtitle_card_test=1` (in each `bwvr.ini.run`). Each render is 2760x2208. The log frame is the step frame minus about 729.

Card pictures (mod art) and sheets are in `C:\BW1-VR\local\shots\job91\` (not in a repo).

## The strokes and the slaps

The recipe steps of `-Kind touch` in log frames: the nudges of 2 cm at 270 to 510 (each 15 frames), the "slap" steps at 600 (the wind up), 640, 720 and 760, the seek onto the body at 802, up at 817, down at 832, and the hand goes away at 920. The two speeds of each line are "the hand moved at" (the speed of the mod over 100 ms) and "the game read".

| run | state starts | strokes (frame, mod m/s, game m/s) | slaps (frame, kind, mod m/s, game m/s, height S) | press of the open hand ends |
|---|---|---|---|---|
| c14touch | 241 | 347 (0.40, 0.40), 437 (0.48, 0.48), 497 (0.48, 0.48) | 641 hard 2.41/2.41 (0.47), 721 hard 2.86/2.86 (0.47), 761 hard 2.40/2.40 (0.47), 803 slap 1.44/1.44 (0.47), 833 slap 1.44/1.44 (1.00) | 940, 41.6 cm out, past 15 cm for 300 ms |
| c14wait | 241 | none | 641 hard 2.88/2.88 (0.47), 721 hard 2.86/2.86 (0.47), 761 hard 2.88/2.88 (0.47), 803 slap 1.44/1.44 (0.47), 833 slap 1.20/1.20 (1.00) | 940, 41.6 cm out, past 15 cm for 300 ms |
| c14vk | 259 | 407 (0.40, 0.40), 497 (0.48, 0.48) | 641 hard 2.88/2.88 (0.47), 721 hard 2.83/2.83 (0.47), 761 hard 2.88/2.88 (0.47), 803 slap 1.44/1.44 (0.47), 833 slap 1.44/1.44 (1.00) | 940, 41.6 cm out, past 15 cm for 300 ms |
| c14open | 241 | 582 (0.48, 0.48), 682 (0.40, 0.40) | 931 hard 2.40/2.40 (0.47) | 1090, 42.8 cm out, past 15 cm for 300 ms |

No run has a slap before frame 600, and no run has a slap at the wind up (600). No nudge reads as a slap: each nudge reads 0.40 or 0.48 m/s (the limit of a slap is 1.00 m/s). The mod speed and the game speed are the same in each line. Each window line has "0 frames in which the game read the open hand slower than a slap".

## 1. c14touch (backend=dxwrapper\ddraw.dll, C): PASS

The keys:
- "hand creature: creature_swing=1. The speed of a stroke or a slap is the distance of the hand over 100 ms. A swing starts at the speed of a slap and ends under 0.6 of it. Out of a swing the game reads 0.8 of the speed of a slap at most"
- "hand creature: creature_stroke_wait=0. ...", "creature_slap_height=1. ...", "controllers  : creature_open_slap=1. ...", "controllers  : creature_touch_easy=1. ...", "ui panel     : creature_panel_over=1. ..."

The press and the state:
- "controllers  : the open hand is at the body of your Creature 13AD6A10 for 100 ms: ... The Hand's point is 2.3 cm in the body (creature_stroke_open=1, creature_touch_easy=1), frame 210"
- "hand creature: the Creature state starts, frame 241. A slap can count at once. A stroke can count at once (creature_stroke_wait=0)."

The strokes in the nudges: PASS (three strokes, no slap).
- "hand creature: a stroke with the index finger, to the left as the eye sees it. The hand moved at 0.40 m/s (the grip 0.40, the Hand's point 0.40), and the game read 0.40 m/s, 6.6 units/s (creature_swing=1). A slap needs 1.00 m/s (16.5 units/s) and a hard slap 1.80 m/s (29.7). ... frame 347"
- "hand creature: a stroke ... The hand moved at 0.48 m/s (the grip 0.48, the Hand's point 0.48), and the game read 0.48 m/s, 7.9 units/s (creature_swing=1). ... frame 437", and the same at frame 497.
- The window line: "2 strokes and 0 slaps of the Creature this window" (frames 0 to 449).

The wind up: PASS. "controllers  : the fake hand steps -12.0 cm to the side at frame 600", and the next creature line is the slap at 641.

The slaps: PASS.
- "hand creature: a hard slap with the index finger, to the right as the eye sees it. The hand moved at 2.41 m/s (the grip 2.41, the Hand's point 2.41), and the game read 2.41 m/s, 39.7 units/s (creature_swing=1). ... The contact point is 2.08 units outside the body, and it went into the body since the last frame (the game read the height 0.47 S). ... frame 641"
- "a hard slap ... to the left ... 2.86 m/s ... the game read 2.86 m/s, 47.2 units/s ... (the game read the height 0.47 S) ... frame 721"
- "a hard slap ... to the right ... 2.40 m/s ... the game read 2.40 m/s, 39.6 units/s ... (the game read the height 0.47 S) ... frame 761"
- Onto the body: "hand creature: a slap with the index finger, to the left as the eye sees it. The hand moved at 1.44 m/s ... and the game read 1.44 m/s, 23.7 units/s ... The contact point is 3.25 units inside the body, and it went into the body since the last frame (the game read the height 0.47 S). ... frame 803"
- From over the head: "hand creature: a slap with the index finger, to the right as the eye sees it. The hand moved at 1.44 m/s (the grip 1.44, the Hand's point 1.44), and the game read 1.44 m/s, 23.7 units/s (creature_swing=1). ... The motion in the room: 0.00 right, -1.00 up, 0.00 forward. The contact point is 3.25 units inside the body, and it went into the body since the last frame (the game read the height 1.00 S). ... frame 833". S is 1.00, in the limit.
- The window line: "1 strokes and 5 slaps of the Creature this window", "5 slap branches forced by the swept test, 5 of them with the height of the hit".
- The press: "controllers  : the press of the open hand ends after 12172 ms: the Hand's point is 41.6 cm out of the body, past 15 cm for 300 ms (creature_touch_easy=1), frame 940".

The panel: PASS.
- "ui panel     : the Creature panel goes over the Creature at -0.01 -0.36 -0.83 m, 0.90 m from the eyes. The top of the Creature is 1.00 m from the eyes. The panel is 11 degrees wide (creature_panel_over=1), frame 205"
- "ui panel     : the Creature panel goes, 3.00 s after the Hand left your Creature or the press ended, frame 1161". The line now gives the correct time. 1161 is 221 frames after the end of the press at 940.
- "ui panel     : frame 973, the Creature card: the box 48 544 912 768 of the render, 456 by 384 px, at -0.015 -0.357 -0.829 m, 0.17 by 0.15 m". The box is 1.7 to 34.8 % across and 24.6 to 59.4 % down.
- The report lines: "246 frames, 246 of them on the Creature card ... 2 frames had the box and no copy of it yet (the read ahead gives the copy of a frame before), so the box showed on no card", then "450 ... 0 frames had the box and no copy of it yet", then "260 ... 0 frames ...".

The subtitle card: PASS. Each line has the rect of the strip, not the box:
- "subtitles    : frame 990, the subtitle card: the rect 0 1856 2760 304 of the 2760 by 2208 render, 1380 by 152 px on the card, at 0.000 -0.452 -1.535 m, turn -0.1478 0.0000 0.0000 0.9890, 1.00 by 0.11 m, 1.60 m from the head and 16 degrees below eye level. ..."
- The lines at frames 2, 80 and 1223 have the same rect. The rect is 0 to 100 % across and 84.1 to 97.8 % down, so it does not hold the box (y 25 to 59 %). The size agrees with the card: 2760 by 304 is 9.08 to 1, and 1.00 by 0.11 m is 9.09 to 1 (1380 by 152 px is half of the rect).

The pictures:
- `bwvr-creature-card-00973.bmp`: the whole box of the Creature panel on a gray to white card. Four rows: "Damage" 0%, "Hunger" 1% (a thin yellow line), "Tiredness" 0%, and "Reward" with a red bar and "Bad Boy! 70%". No part is cut. `bwvr-creature-card-over-00973.bmp`: the same box over a checker, the bars and the text opaque, the back of the box half clear.
- `bwvr-subtitle-card-01223.bmp`: only the strip of the subtitles with the text "From now on ..." and no box.

## 2. c14wait (as 1, with creature_stroke_wait=1): PASS (no stroke comes at all)

- "hand creature: creature_stroke_wait=1. The first stroke of a Creature state comes 2 s after the start, as in the game"
- "hand creature: the Creature state starts, frame 241. A slap can count at once. The game's wait of 2 s before the first stroke stays (creature_stroke_wait=1)."
- No "a stroke" line is in the log. The nudges go on to frame 510 ("controllers  : the fake hand steps 2.0 cm to the side at frame 510"), 269 frames after the state start, so 150 frames of nudges are after the wait of 2 s. The window lines: "0 strokes and 5 slaps of the Creature this window", "0 states with no wait for the first stroke".
- The slaps are as in c14touch: hard slaps at 641 (2.88 m/s), 721 (2.86), 761 (2.88), a slap at 803 (1.44, S 0.47), and "a slap ... The hand moved at 1.20 m/s ... the game read 1.20 m/s, 19.8 units/s ... (the game read the height 1.00 S) ... frame 833". No slap before 600 and none at 600.
- The press ends at 940 (41.6 cm out). "ui panel     : the Creature panel goes, 3.00 s after the Hand left your Creature or the press ended, frame 1162".
- "frame 973, the Creature card: the box 48 544 912 768 ..., 456 by 384 px". The subtitle lines at 991 and 1223 have the rect 0 1856 2760 304, 1.00 by 0.11 m.
- `bwvr-creature-card-00973.bmp`: the whole box, "Damage" 0%, "Hunger" 1%, "Tiredness" 0%, "Reward" "Bad Boy! 80%" with a red bar. Nothing is cut. `bwvr-subtitle-card-01223.bmp`: only the strip "From now on ...".
- Odd: the first window has "worst frame  : 1256.80 ms, and 412 of 449 frames missed 11.6 ms" (54.4 fps). c14touch and c14open have a similar worst frame of about 1260 ms in the first window, so it is a property of the start of the save, not of this key.

## 3. c14vk (as 1, Vulkan): PASS

- `bwvr.ini.run` has backend=vulkan. "vk scene     : vk_black_check=1. 0 of 448 checked eye pictures were black", and "0 of 450" in each later window.
- The frame rate: "stereo       : 77.4 fps ... frame 450" (worst frame 172.00 ms, 301 of 449 frames missed 11.6 ms), then 60.0 fps at frame 900, 58.5 fps at 1350, 60.0 fps at 1800 and 2250.
- "hand creature: the Creature state starts, frame 259. ... A stroke can count at once (creature_stroke_wait=0)."
- The strokes: "hand creature: a stroke with the index finger, to the left as the eye sees it. The hand moved at 0.40 m/s (the grip 0.40, the Hand's point 0.40), and the game read 0.40 m/s, 6.6 units/s (creature_swing=1). ..." at frame 407, and a stroke at 0.48 m/s (game 0.48) at frame 497. No nudge reads as a slap: "1 strokes and 0 slaps of the Creature this window" in the first window. The state starts 18 frames later than in c14touch, so there is one stroke fewer.
- The slaps: hard slaps at 641 (2.88 m/s), 721 (2.83), 761 (2.88), all S 0.47, slaps at 803 (1.44, S 0.47) and 833 (1.44, S 1.00). No slap at 600.
- "ui panel     : frame 973, the Creature card: the box 48 544 912 768 of the render, 456 by 384 px, at -0.015 -0.357 -0.829 m, 0.17 by 0.15 m"
- The report lines: "the Creature panel was over the Creature on 248 frames, 248 of them on the Creature card, and it went to a new place 0 times. 0 frames had the box and no copy of it yet ...", then 450 and 264 frames, each with "0 frames had the box and no copy of it yet".
- "ui panel     : the Creature panel goes, 3.02 s after the Hand left your Creature or the press ended, frame 1165". The press ends at 940.
- The subtitle lines at 997 and 1223: the rect 0 1856 2760 304, 1.00 by 0.11 m. Not the box.
- `bwvr-creature-card-00973.bmp`: the whole box, "Damage" 0%, "Hunger" 1%, "Tiredness" 0%, "Reward" "Bad Boy! 60%" with a red bar. Nothing is cut. `bwvr-subtitle-card-01223.bmp`: only the strip with "Your Creature ...".

## 4. c14open (-Kind open, backend=dxwrapper\ddraw.dll, C): PASS

- "ui panel     : the Hand is at your Creature 13AC5A10 with no press: the mod shows the Creature panel (creature_panel_linger=1), frame 203"
- "ui panel     : the Creature panel goes over the Creature at -0.01 -0.36 -0.83 m, 0.90 m from the eyes. ... frame 205"
- "hand creature: the Creature state starts, frame 241. A slap can count at once. A stroke can count at once (creature_stroke_wait=0)."
- The four nudges (530, 580, 630, 680) give no slap. They give two strokes: "hand creature: a stroke with the index finger, to the left as the eye sees it. The hand moved at 0.48 m/s (the grip 0.48, the Hand's point 0.48), and the game read 0.48 m/s, 8.0 units/s (creature_swing=1). ..." at frame 582, and a stroke at 0.40 m/s (game 0.40) at frame 682. The window line: "2 strokes and 0 slaps of the Creature this window".
- The swing: "controllers  : the fake hand steps 12.0 cm to the side at frame 850" gives no slap (the wind up). "controllers  : the fake hand steps -12.0 cm to the side at frame 930", then "hand creature: a hard slap with the index finger, to the left as the eye sees it. The hand moved at 2.40 m/s (the grip 2.40, the Hand's point 2.40), and the game read 2.40 m/s, 39.6 units/s (creature_swing=1). ... The contact point is 2.08 units outside the body, and it went into the body since the last frame (the game read the height 0.47 S). ... frame 931". The window line: "0 strokes and 1 slaps of the Creature this window", "1 slap branches forced by the swept test, 1 of them with the height of the hit".
- "controllers  : the press of the open hand ends after 14671 ms: the Hand's point is 42.8 cm out of the body, past 15 cm for 300 ms (creature_touch_easy=1), frame 1090"
- The panel: "ui panel     : the Creature panel goes, 3.00 s after the Hand left your Creature or the press ended, frame 1299". 1299 is 209 frames after the end of the press, and before the shot at 2060 (log 1333).
- "ui panel     : frame 1113, the Creature card: the box 48 544 928 768 of the render, 464 by 384 px, at -0.015 -0.357 -0.829 m, 0.17 by 0.14 m". The subtitle lines at 1128 and 1333 have the rect 0 1856 2760 304, 1.00 by 0.11 m.
- `bwvr-creature-card-01113.bmp`: the whole box, "Damage" 0%, "Hunger" 1%, "Tiredness" 0%, and "Reward" "No Reward 0%" with an empty bar. Nothing is cut. `bwvr-subtitle-card-01333.bmp`: only the strip with "You...". No Creature card picture at 1333.
- Odd: the card at 1113 shows "No Reward 0%", 182 frames after the hard slap at 931. In c13open the same shot showed "Bad Boy! 30%". The touch runs show "Bad Boy!" 60 to 80% at 973, 140 frames after their last slap. So the bar of the game can drop to 0 in about 3 s after one slap. The log does not say more.
- The run also saved `bwvr-hintcard-draw-*.bmp` pictures (21 of them), which c14touch does not have. The job does not ask about them.

## What only a headset can show

- The place of the Creature card against the Creature in the eyes. The desk eye pictures do not hold the layers of the headset.
- The feel of the swing of 100 ms with a real hand: a real stroke is not a set of steps of 2 cm, and a real slap is not a step of 12 to 24 cm in one frame.
