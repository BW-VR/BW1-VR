# Job 85: the Creature card over the Creature, the slap of the open hand, the easy touch (agent/creature 4227b20)

`.\dev.ps1 build -Clean`: 6469 checks, 0 failed. Each log has "build 4227b20". `creature_recipe.ps1`, test save temple, `test_frame_rate=60`, `card_send_test_save=0`, `subtitle_card_test=1`. Each render is 2760x2208. In each `desk-state.txt` the real cursor is at the same place before and after the run, and the window in front is Progman. The log frame is the step frame minus about 729 (step 1700 gives the pictures of log frame 973).

Crops and sheets are in `C:\BW1-VR\local\shots\job85\` (not in a repo).

## The strokes and the slaps

The recipe steps in log frames: the nudges at 270 to 510 (each 15 frames), the "slap" steps at 600, 640 and 720, the seek onto the body at 802, up at 817, down at 832, and the hand goes away at 920.

| run | state starts | strokes (frame, m/s) | slaps (frame, kind, m/s, height S) | press of the open hand ends |
|---|---|---|---|---|
| c13touch | 240 | 347 (0.80), 437 (0.80) | 497 slap 1.19, 641 hard 7.20 (0.47), 721 hard 7.16 (0.47), 803 hard 3.60 (0.47), 833 hard 3.56 (1.05) | 939, 41.6 cm out, past 15 cm for 300 ms |
| c13old | 242 | none | none | 601, 9.7 cm out, past 5 cm for 0 ms |
| c13vk | 259 | none | 407 slap 1.20, 497 slap 1.20, 641 hard 4.80 (0.47), 721 hard 6.93 (0.47), 803 hard 3.60 (0.47), 833 hard 3.60 (1.05) | 940, 41.6 cm out, past 15 cm for 300 ms |

No run has a slap at the step of frame 600. The hand is at +2 cm in the body after the last nudge, so the step to -12 cm goes out of the body and not into it. The slap steps at 640 and 720 come from the other side, and they count. In c13touch and c13vk one or two nudges of 2 cm read as a slap of 1.19 to 1.20 m/s (the limit is 1.00 m/s), and the other nudges read 0.80 m/s. The speed of a nudge is near the limit of a slap.

## 1. c13touch (the new keys): FAIL (the slap near frame 600 is missing, and the logged rect of the subtitle card holds the box)

The keys at the start:
- "ui panel     : creature_panel_over=1. The Creature panel shows over the head of the Creature, turned to the eyes, and it stays there while you touch the Creature"
- "hand creature: creature_stroke_wait=0. The first stroke of a Creature state comes with the first motion of the hand on the body. After a stroke the game waits 1 s"
- "hand creature: creature_slap_height=1. In a slap that went through the body between two frames, the game reads the height of the path in the body"
- "controllers  : creature_open_slap=1. The open hand slaps too: a slow motion on the body strokes the Creature, and a fast hit slaps it"
- "controllers  : creature_touch_easy=1. The press of the open hand starts with the Hand's point 8 cm or less from the body for 100 ms, and it ends when the point is more than 15 cm from the body for 300 ms"

The press: PASS.
- "controllers  : the open hand is at the body of your Creature 13AD8A10 for 100 ms: the mod holds the Action button of the game for a stroke or a slap. The Hand's point is 2.3 cm in the body (creature_stroke_open=1, creature_touch_easy=1), frame 209"
- "hand creature: a press on the Creature 13AD8A10. The camera does not move (creature_camera=2), frame 216"
- "hand creature: the Creature state starts, frame 240. A slap can count at once. A stroke can count at once (creature_stroke_wait=0)."

The strokes: PASS. The first stroke is at frame 347, 107 frames after the state start (less than 120).
- "hand creature: a stroke with the index finger, to the left as the eye sees it. The hand moved at 0.80 m/s (the grip 0.80, the Hand's point 0.80), which the game read as 13.2 units/s. ..." frame 347. The second stroke is at frame 437.
- The nudges start at frame 270, so the first nudges in the state (270 to 330) give no stroke. The first stroke comes 77 frames after the first nudge.

The slaps: FAIL for the slap near frame 600, PASS for the others.
- Near 600: no slap line. "controllers  : the fake hand steps -12.0 cm to the side at frame 600", and the next lines are of the hint card.
- "hand creature: a hard slap with the index finger, to the right as the eye sees it. The hand moved at 7.20 m/s ... The contact point is 2.08 units outside the body, and it went into the body since the last frame (the game read the height 0.47 S)." frame 641.
- "hand creature: a hard slap ... 7.16 m/s ... (the game read the height 0.47 S)" frame 721.
- From over the head: "hand creature: a hard slap with the index finger, to the right as the eye sees it. The hand moved at 3.56 m/s ... The motion in the room: 0.00 right, -1.00 up, 0.00 forward. The contact point is 3.25 units inside the body, and it went into the body since the last frame (the game read the height 1.05 S)." frame 833. S is 1.05, on the top limit of 0.05 to 1.05.
- One more hard slap at frame 803 (3.60 m/s, S 0.47), from the seek onto the body. One slap at frame 497 (1.19 m/s), from a nudge.
- The press stays to the hand leaves: "controllers  : the fake hand steps to point 0 (0 0 100 cm) at frame 920", then "controllers  : the press of the open hand ends after 12188 ms: the Hand's point is 41.6 cm out of the body, past 15 cm for 300 ms (creature_touch_easy=1), frame 939". No other "press of the open hand ends" line.
- The window line: "hand creature: ... 4 slap branches forced by the swept test, 4 of them with the height of the hit ...".

The panel: PASS.
- "ui panel     : the Creature panel goes over the Creature at -0.01 -0.36 -0.83 m, 0.90 m from the eyes. The top of the Creature is 1.00 m from the eyes. The panel is 11 degrees wide (creature_panel_over=1), frame 205"
- At the 1700 shot: "ui panel     : frame 973, the Creature card: the box 48 544 912 768 of the render, 456 by 384 px, at -0.015 -0.357 -0.829 m, 0.17 by 0.15 m". The box is 1.7 to 34.8 % across and 24.6 to 59.4 % down.
- The report lines: "ui panel     : the Creature panel was over the Creature on 246 frames, 246 of them on the Creature card, and it went to a new place 0 times. 2 frames had the box and no Creature card (creature_panel_over=1)", then "on 450 frames, 450 of them on the Creature card ... 0 frames had the box and no Creature card", then "on 259 frames, 259 of them on the Creature card ...".
- The panel goes: "ui panel     : the Creature panel goes, 275719.45 s after the Hand left your Creature, frame 1160". The frame is 221 frames (3.7 s) after the hand left, and 1160 is before the 1950 shot (log 1223). The number of seconds in this line is wrong (275719.45 s). The same fault is in c13open and c13vk. In c13old the line says "2.99 s".
- `bwvr-creature-card-00973.bmp`: the whole box of the Creature panel on a white to gray card. Four rows: "Damage" 0%, "Hunger" 1%, "Tiredness" 0%, and "Reward" with a red bar and "Bad Boy! 70%". No part is cut: the labels at the left, the bars and the text at the right ("70%") are all in the picture. `bwvr-creature-card-over-00973.bmp` is the same box over a checker: the bars and the text are opaque, the back of the box is half clear.

The subtitle card: FAIL as the job states it, but the size of the card shows that the card does not hold the box.
- "subtitles    : frame 989, the subtitle card: the rect 0 544 2760 1616 of the 2760 by 2208 render, 1380 by 808 px on the card, at 0.000 -0.452 -1.535 m, turn -0.1478 0.0000 0.0000 0.9890, 1.00 by 0.11 m, 1.60 m from the head and 16 degrees below eye level. ..."
- The rect 0 544 2760 1616 holds the area of the box. But the card is 1.00 by 0.11 m, the size of the strip of the subtitles. In c13old the same rect gives a card of 1.00 by 0.59 m. The code (`bwvr/src/mod/ui_panel.cpp`, the log line near line 1333) logs `used`, the rect of the copy. With the Creature card the copy holds the box and the help text together, and the quad of the subtitle card shows only the part of the help text. So the logged rect is the copy, not what the card shows.
- `bwvr-subtitle-card-01223.bmp` (the 1950 shot): the strip with "From now on, your Creature will pay less attention to you." and no box. The run has no subtitle card picture at frame 973.

The eye pictures: NOT SHOWN for the place of the panel.
- `bwvr-shared-left-00973.bmp` and `bwvr-shared-right-00973.bmp`: the temple with its door and the courtyard, the orange Hand at the left of the courtyard. The Creature is small, in the courtyard in front of the door of the temple, near the middle right of the picture (about 1850 937 in the left eye, where the pointer is). No panel is in either eye. The Creature card is a layer of the headset, and the desk eye pictures do not hold it. So only a headset can show where the panel is against the Creature.
- The log gives the place: the panel is 0.90 m from the eyes and the top of the Creature 1.00 m. So the panel is 0.10 m nearer than the top of the Creature, not in it and not behind it.
- At 01223: the same view, the Hand in the courtyard, a white advisor spirit next to the Hand, no panel.

## 2. c13old (the old keys): FAIL (no stroke at all), the other items PASS

The keys at the start:
- "controllers  : creature_open_slap=0. The open hand never slaps. A slap needs the grip"
- "controllers  : creature_touch_easy=0. The press of the open hand starts with the Hand's point 0 cm or less from the body for 200 ms, and it ends when the point is more than 5 cm from the body for 0 ms"
- "hand creature: creature_stroke_wait=1. The first stroke of a Creature state comes 2 s after the start, as in the game"
- "hand creature: creature_slap_height=0. ..." and "ui panel     : creature_panel_over=0. The Creature panel shows on the subtitle card with ui_panel=0, and beside the Creature with ui_panel=1"

The old behavior:
- The press: "controllers  : the open hand is at the body of your Creature 13AC5A10 for 200 ms: ... (creature_stroke_open=1, creature_touch_easy=0), frame 215". "hand creature: the Creature state starts, frame 242. A slap can count at once. The game's wait of 2 s before the first stroke stays (creature_stroke_wait=1)."
- The end of the press at the first wait out of the body: PASS. "controllers  : the press of the open hand ends after 6422 ms: the Hand's point is 9.7 cm out of the body, past 5 cm for 0 ms (creature_touch_easy=0), frame 601", and "hand creature: the press ends after 6416 ms (hand mode now 0x12). A press of the open hand, so no leash click, frame 603".
- No slap of the open hand: PASS. The log has no "a slap" or "a hard slap" line.
- The first stroke 2 s or more after the state start: FAIL. The log has no "a stroke" line at all. The nudges go on to frame 510, so frames 362 to 510 are after the 2 s wait. The window line has "28 frames in which the game read the open hand slower than a slap". The line of 30 frames in the state has "hand+0x78 is 0.8887 units from the target", and c13touch has 0.0005 there.
- No "goes over" line: PASS. Each report line: "ui panel     : the Creature panel was over the Creature on 0 frames, 0 of them on the Creature card ... (creature_panel_over=0)".
- The rect of the subtitle card holds the box: PASS. "subtitles    : frame 973, the subtitle card: the rect 0 544 2760 1616 of the 2760 by 2208 render, 1380 by 808 px on the card, at 0.000 -0.225 -1.604 m, ... 1.00 by 0.59 m, 1.62 m from the head and 8 degrees below eye level. ..."
- "ui panel     : the Creature panel goes, 2.99 s after the Hand left your Creature, frame 1048".

The pictures:
- `bwvr-subtitle-card-00973.bmp`: the Creature panel at the top left ("Damage" 0%, "Hunger" 1%, "Tiredness" 0%, "Reward" "No Reward 0%"), a large white area, and the subtitle strip at the bottom: "Your Creature wants to have a look around."
- `bwvr-shared-left-00973.bmp`: the Creature walked out of the courtyard. It is on the grass below the temple, about the middle of the picture. The Hand is at the left of the courtyard. At 01223 the Creature is near the gate of the courtyard, and no panel shows.

## 3. c13open (-Kind open, the new keys): PASS

- The keys are as in c13touch (creature_open_slap=1, creature_touch_easy=1, creature_stroke_wait=0, creature_slap_height=1, creature_panel_over=1).
- "ui panel     : the Hand is at your Creature 13ADAA10 with no press: the mod shows the Creature panel (creature_panel_linger=1), frame 203"
- "ui panel     : the Creature panel goes over the Creature at -0.01 -0.36 -0.83 m, 0.90 m from the eyes. The top of the Creature is 1.00 m from the eyes. The panel is 11 degrees wide (creature_panel_over=1), frame 205"
- "controllers  : the open hand is at the body of your Creature 13ADAA10 for 100 ms: ... (creature_stroke_open=1, creature_touch_easy=1), frame 210". "hand creature: the Creature state starts, frame 241. A slap can count at once. A stroke can count at once (creature_stroke_wait=0)."
- The swing of slap@1580/1660: "controllers  : the fake hand steps 12.0 cm to the side at frame 850" gives no slap. "controllers  : the fake hand steps -12.0 cm to the side at frame 930", then "hand creature: a hard slap with the index finger, to the left as the eye sees it. The hand moved at 7.13 m/s ... The contact point is 2.08 units outside the body, and it went into the body si[nce the last frame]" at frame 931. The window line: "1 slap branches forced by the swept test, 1 of them with the height of the hit".
- The four nudges (530, 580, 630, 680) give no stroke line. The nudge at 580 gives "hand creature: a slap with the index finger, to the left as the eye sees it. The hand moved at 1.20 m/s ..." at frame 582. The window line: "0 strokes and 1 slaps of the Creature this window". The job does not ask for strokes in this run.
- The panel stays 3 s: "controllers  : the fake hand steps to point 0 (0 0 100 cm) at frame 1070", "controllers  : the press of the open hand ends after 14672 ms ... past 15 cm for 300 ms (creature_touch_easy=1), frame 1090", "ui panel     : the Creature panel goes, 275819.19 s after the Hand left your Creature, frame 1300". 1300 is 230 frames (3.8 s) after the hand left.
- In the shot at 1840: "ui panel     : frame 1113, the Creature card: the box 48 544 912 768 of the render, 456 by 384 px, at -0.015 -0.357 -0.829 m, 0.17 by 0.15 m". `bwvr-creature-card-01113.bmp`: the whole box, "Damage" 0%, "Hunger" 1%, "Tiredness" 0%, "Reward" "Bad Boy! 30%" with a short red bar. Nothing is cut.
- Not in the shot at 2060: no Creature card picture at 1333. `bwvr-subtitle-card-01333.bmp` is only the strip "From now on, your Creature will pay less attention to you." The subtitle line of frame 1333 has the rect 0 1856 2760 304.
- The eye pictures at 01113 and 01333: the temple and the courtyard, the Hand at the left of the courtyard. At 01113 the Creature is small in the courtyard. At 01333 a white advisor spirit is above the Hand. No panel is in an eye picture (the panel is on the Creature card, a layer of the headset).
- This run uses the Creature card, so "the panel in the shot" is the Creature card picture, not the eye picture.

## 4. c13vk (Vulkan, the new keys): FAIL (no stroke, and no slap near frame 600)

- `bwvr.ini.run` has backend=vulkan.
- The eye pictures are not black: "vk scene     : vk_black_check=1. 0 of 448 checked eye pictures were black", and "0 of 450". `bwvr-vk-left-00973.bmp` and `bwvr-vk-right-00973.bmp` show the temple, the courtyard and the Hand, as in c13touch. The 01223 pictures are the same view with the Hand in the courtyard. No panel is in the eyes.
- "ui panel     : the Creature panel goes over the Creature at -0.01 -0.36 -0.83 m, 0.90 m from the eyes. The top of the Creature is 1.00 m from the eyes. ... frame 203"
- "controllers  : the open hand is at the body of your Creature 1632BA10 for 100 ms: ... frame 210". "hand creature: the Creature state starts, frame 259. A slap can count at once. A stroke can count at once (creature_stroke_wait=0)."
- The Creature card: PASS. "ui panel     : frame 973, the Creature card: the box 48 544 928 768 of the render, 464 by 384 px, at -0.015 -0.357 -0.829 m, 0.17 by 0.14 m". `bwvr-creature-card-00973.bmp`: the whole box, "Damage" 0%, "Hunger" 1%, "Tiredness" 0%, "Reward" "Bad Boy! 100%" with a red bar to the middle. Nothing is cut. `bwvr-vk-panel-00973.bmp` (the panel target, all of the render): the small box at the top left on black. `bwvr-vk-panel-01223.bmp`: only the subtitle "Your Creature is trying to show you how he is feeling." at the bottom.
- The report lines: "on 248 frames, 248 of them on the Creature card ... 0 frames had the box and no Creature card", "on 450 frames, 450 of them ...", "on 262 frames, 262 of them ...".
- The strokes: FAIL. No "a stroke" line. The nudges at 407 and 497 give "a slap" at 1.20 m/s. The window line: "0 strokes and 1 slaps of the Creature this window". The first window runs at "77.4 fps" and has "worst frame  : 171.43 ms, and 301 of 449 frames missed 11.6 ms". The second window is at 60.0 fps.
- The slaps: as in c13touch, except near 600. Hard slaps at 641 (4.80 m/s, S 0.47), 721 (6.93 m/s, S 0.47), 803 (3.60 m/s, S 0.47) and 833 (3.60 m/s, S 1.05). No slap near 600.
- The press ends at the hand leave: "controllers  : the press of the open hand ends after 12172 ms: the Hand's point is 41.6 cm out of the body, past 15 cm for 300 ms (creature_touch_easy=1), frame 940".
- The subtitle card: "frame 996, the subtitle card: the rect 0 544 2760 1616 ... 1.00 by 0.11 m", the same as in c13touch. `bwvr-subtitle-card-01223.bmp`, `-over-` and `-vk-` show only the strip "Your Creature is trying to show you how he is feeling."

## What only a headset can show

- The place of the Creature card against the Creature in the eyes: over its head, not in it, not behind a hill. The desk eye pictures do not hold the layers of the headset.
- The feel of the slap and of the stroke with a real hand.
