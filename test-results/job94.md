# Job 94: a smoke test of main with batch 13 (main 0e9c153)

`.\dev.ps1 build -Clean`: 6736 checks, 0 failed. Each log has "build 0e9c153". Vulkan, the keys of the player and of each recipe. m94book, m94touch and m94walk are RUN DONE, and the bench is "BENCH DONE".

No run has a fault: each log has only the start lines of the crash report ("crash report : at the first frame the filter of the mod is first ... MiniDumpWriteDump is ready"), and no run folder has a dump. The lines with "error", "failed", "refused" or "not right" are the same as in the run of the same recipe on its branch (m94book against sbB2 of job 90, m94touch against c14vk of job 91, m94walk against speed-scale-1 of job 87). After a change of the numbers to N, no line is new on main.

## 1. m94book (-Kind book): PASS

- "spell book   : the game draws 3 orbs in the cells of the book, each 4.3 cm across, frame 271". "spell book   : the orbs of the game showed in the cells of the book in 180 frames this window, with 474 draws of an orb", and 211 frames in the next window.
- The pull: "the pointing hand grips Food and pulls it out of the book: ... frame 382", then "the grip opens at 92 percent of the pull (the most was 92 percent), so Food springs back into the book, frame 422".
- The snap: "Food snaps into the hand: the hand is 10.1 cm from its cell, frame 542", "the Miracle Food is in the Hand, 5 frames after the request, frame 547".
- The refusal: "hint card    : frame 691, the pointing card now says: Water: No prayer power. Villagers must worship", "frame 711 ... Food: No prayer power. Villagers must worship", and "spell book   : the pointing hand takes Food from the book, and the game refuses it: No prayer power. Villagers must worship, frame 742".
- The card after the book closes (the check that job 93 could not make): "spell book   : the book closes: the button, frame 811", then "hint card    : frame 819, the pointing card now says: nothing. The context of the Hand is the land since frame 811", then "frame 898, the pointing card now says: [A] Teleport". No card line after frame 811 has "No prayer power". In job 90 (sbB2) the card kept it from frame 811 to 983.
- The hand cut: "this window the cut ran in 180 frames, 0.319 ms on average and 1.426 ms at the most (with no save of a test picture). On average: the hands into the room 0.003 ms, the masks 0.225 ms, the copies 0.031 ms, the holes 0.058 ms. In 109 frames nothing moved ...", and "211 frames, 0.165 ms on average and 1.137 ms at the most". So the longest cut is over 1 ms, as in job 93.

## 2. m94touch (-Kind touch, Vulkan): PASS

| state start | strokes (frame, m/s, the game read the same) | slaps (frame, m/s, height) | press ends |
|---|---|---|---|
| 259 | 407 (0.48), 497 (0.48) | 641 hard 2.87 (0.47 S), 721 hard 2.84 (0.47), 761 hard 2.88 (0.47), 803 slap 1.44 (0.47), 833 slap 1.44 (1.00) | 940, 41.6 cm out, past 15 cm for 300 ms |

- "hand creature: creature_swing=1. The speed of a stroke or a slap is the distance of the hand over 100 ms. ...", "hand creature: the Creature state starts, frame 259. A slap can count at once. A stroke can count at once (creature_stroke_wait=0)."
- "hand creature: a stroke with the index finger, to the left as the eye sees it. The hand moved at 0.48 m/s (the grip 0.48, the Hand's point 0.48), and the game read 0.48 m/s, 7.9 units/s (creature_swing=1)". No nudge reads as a slap.
- "ui panel     : the Creature panel goes over the Creature at -0.01 -0.36 -0.83 m, 0.90 m from the eyes. The top of the Creature is 1.00 m from the eyes. The panel is 11 degrees wide (creature_panel_over=1), frame 203".
- "ui panel     : frame 973, the Creature card: the box 48 544 912 768 of the render, 456 by 384 px, at -0.015 -0.357 -0.829 m, 0.17 by 0.15 m".
- "ui panel     : the Creature panel goes, 3.00 s after the Hand left your Creature or the press ended, frame 1165".
- Each report: "the Creature panel was over the Creature on 248 frames, 248 of them on the Creature card, ... 0 frames had the box and no copy of it yet", then 450 of 450 and 264 of 264.
- The same as c14vk of job 91: the same frames of the strokes and the slaps, and the first window at 77.5 fps.

## 3. m94walk: PASS, with the speed ramp of the default keys

- The head: "scale check  : a step of the head of 14.14 cm at frame 171. The camera of the left eye pass from its place in the frame before: 0.0000 0.1413 0.1413 0.1413 0.1413 0.1413 units ... So the camera has the full move 1 frames after the declared eye, and with the declared eye of 1 frames before the head gives 0.999 units per meter". Frame 271 is the same.
- "head lag     : head_pose_frame=1, the invented head. In 450 frames the camera of the game took the head pose of its own frame in 449 frames and the pose of the frame before in 1. ... 0 frames had a difference". Each later window has 450 of 450.
- The walk start: "A full push moves you 150 cm of the room each second (move_stick_speed_cm). At 1.00 units per meter the speed of the stick is times 4.00 (move_stick_scale=1: (16.0 / scale) ^ 0.50, from 0.60 to 4.00): 600 cm of the room each second. ... After 1 s of a push of 90% or more the speed doubles each 2 s, up to 32 times (move_stick_ramp=1), frame 521."
- The walk stop: "You went 13.19 units over the land, which is 1319.16 cm ... The stick was pushed for 2.000 s, which gives 659.6 cm of the room each second ... The speed ramp went up to 1.41 times the speed of the push (move_stick_ramp=1) ... The scale made the speed of the stick 4.00 to 4.00 times move_stick_speed_cm", frame 641.
- The scale check: "In 119 frames of a walk the camera of the game went 13.09 units in 1.98 s: 660.2 cm of the room each second at the units per meter of the eyes, 660.2 across the land and 0.0 up and down. The scale makes the speed of the stick 4.00 times move_stick_speed_cm (move_stick_scale), and the speed across over that factor is 165.0 cm each second."
- So the walk is 660 cm each second and not 600. The recipe of this job does not set move_stick_ramp=0 (job 87 did), so the default move_stick_ramp=1 applies: after 1 s of the 2 s push the ramp goes up to 1.41. The mean of 1.00 for 1 s and a ramp to 1.41 in the second second is about 1.10 times 600, which is 660. This is the behavior that the owner gets with the default keys.
- The grab: "grab         : the gain of the grab (grab_boost=1): 1.00 up, with the land 0.30 m below the hand (reach 50 cm, at most 50), and 4.00 across at 1.00 units per meter, frame 721", "the hand went 40.0 cm and the land 161.0 cm in the room. The gain of the grab was 1.00 to 1.00 up and 4.00 to 4.00 across (grab_boost=1)". The scale check: "In 149 frames of a grab of the world the camera of the game went 1.60 units, 160.0 cm of the room at the units per meter of the eyes, and the grips went 40.0 cm. The gain of the grab was 4.00 to 4.00".

## 4. m94bench (1 run): PASS, no scene is slower

| scene | work mean main | work mean job 92 run 2 (A, 3 runs) | difference | p99 main | frame mean main |
|---|---|---|---|---|---|
| 1 start, level head | 2.47 | 2.48 | -0.01 | 3.61 | 15.74 |
| 2 start, head down | 2.55 | 2.55 | 0.00 | 3.48 | 16.67 |
| 3 village | 2.94 | 2.95 | -0.01 | 3.63 | 16.67 |
| 4 tree in the Hand | 2.90 | 2.91 | -0.01 | 3.74 | 16.67 |
| 5 337 units a meter | 2.78 | 2.78 | 0.00 | 3.72 | 16.67 |
| 6 widest zoom | 0.91 | 0.90 | +0.01 | 1.45 | 16.67 |

The spread of job 92 was 0.01 to 0.08 ms for the work mean. Each difference is in that spread. The slow frames are 2, 1, 0, 1, 1 and 1 in scenes 1 to 6, the same as in job 92.
