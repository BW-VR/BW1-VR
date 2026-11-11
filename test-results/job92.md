# Job 92: the head pose of the frame, and the kept picture of the intro with a turn (agent/headlag 4eb77f8)

`.\dev.ps1 build -Clean`: 6658 checks, 0 failed. Each log has "build 4eb77f8". All runs are RUN DONE, and the bench is "BENCH DONE".

## 1. hl-new (head_pose_frame=1) and hl-old (head_pose_frame=0): PASS

hl-old:
- "scale check  : a step of the head of 14.14 cm at frame 171. The camera of the left eye pass from its place in the frame before: 0.0000 0.0000 0.1413 0.1413 0.1413 0.1413 units in that frame and the 5 after (the full move is 0.1414 units at 1.000 units per meter of the eyes). So the camera has the full move 2 frames after the declared eye, and with the declared eye of 2 frames before the head gives 0.999 units per meter". The line for frame 271 is the same.
- "head lag     : head_pose_frame=0, the invented head. In 450 frames the camera of the game took the head pose of its own frame in 0 frames and the pose of the frame before in 450. The newest pose of the frame was 0.94 mm and 0.000 degrees from the pose of the camera in the mean, and 141.42 mm and 0.000 degrees at the most. 3 frames had a difference"
- The next window: "... took the head pose of its own frame in 0 frames and the pose of the frame before in 450. The newest pose of the frame was 0.00 mm and 0.000 degrees ... 0 frames had a difference".
- The window line of the scale check: "a move of the head 0.500 with the declared eye of the frame before (2 moves of 5 cm) and 0.500 with the declared eye of the same frame (2), a step of the head 0.999 at the lag (2 steps, the camera at most 2 frames after the declared eye, 0 steps with no full move in 6 frames)".

hl-new:
- "scale check  : a step of the head of 14.14 cm at frame 171. The camera of the left eye pass from its place in the frame before: 0.0000 0.1413 0.1413 0.1413 0.1413 0.1413 units in that frame and the 5 after (the full move is 0.1414 units at 1.000 units per meter of the eyes). So the camera has the full move 1 frames after the declared eye, and with the declared eye of 1 frames before the head gives 0.999 units per meter". The line for frame 271 is the same.
- "head lag     : head_pose_frame=1, the invented head. In 450 frames the camera of the game took the head pose of its own frame in 449 frames and the pose of the frame before in 1. The newest pose of the frame was 0.00 mm and 0.000 degrees from the pose of the camera in the mean, and 0.00 mm and 0.000 degrees at the most. 0 frames had a difference"
- The next window: "... took the head pose of its own frame in 450 frames and the pose of the frame before in 0. ... 0 frames had a difference".
- So N is equal to the count of frames in each window except the first, which has 449 of 450 (1 frame took the pose of the frame before).
- The window line of the scale check: "a move of the head 0.999 with the declared eye of the frame before (2 moves of 5 cm) and 0.500 with the declared eye of the same frame (2), a step of the head 0.999 at the lag (2 steps, the camera at most 1 frames after the declared eye, 0 steps with no full move in 6 frames)".

## 2. hl-bench (A head_pose_frame=1, B head_pose_frame=0, 3 runs each): PASS

"the frame cost measure: 6 runs of 122 s, 600 frames for each of the 6 scenes, the test save 'temple'". Each line of the comparison is "in the spread" or "near the spread". No line is "clear".

| scene | number | A | B | B - A | percent | spread | result |
|---|---|---|---|---|---|---|---|
| 1 start, level head | frame mean | 15.75 | 15.74 | 0.00 | 0.0 | 0.01 | in the spread |
| 1 start, level head | work mean | 2.48 | 2.47 | 0.00 | -0.1 | 0.01 | in the spread |
| 1 start, level head | work p99 | 3.76 | 4.01 | +0.25 | +6.6 | 0.47 | in the spread |
| 2 start, head down | frame mean | 16.67 | 16.67 | 0.00 | 0.0 | 0.00 | in the spread |
| 2 start, head down | work mean | 2.55 | 2.54 | -0.01 | -0.4 | 0.01 | in the spread |
| 2 start, head down | work p99 | 3.33 | 3.33 | +0.01 | +0.2 | 0.20 | in the spread |
| 3 village | frame mean | 16.67 | 16.67 | 0.00 | 0.0 | 0.00 | in the spread |
| 3 village | work mean | 2.95 | 2.93 | -0.02 | -0.7 | 0.03 | in the spread |
| 3 village | work p99 | 3.77 | 3.67 | -0.09 | -2.5 | 0.24 | in the spread |
| 4 tree in the Hand | frame mean | 16.67 | 16.67 | 0.00 | 0.0 | 0.00 | in the spread |
| 4 tree in the Hand | work mean | 2.91 | 2.89 | -0.02 | -0.7 | 0.04 | in the spread |
| 4 tree in the Hand | work p99 | 3.68 | 3.57 | -0.11 | -3.1 | 0.22 | in the spread |
| 5 337 units a meter | frame mean | 16.67 | 16.67 | 0.00 | 0.0 | 0.00 | in the spread |
| 5 337 units a meter | work mean | 2.78 | 2.78 | 0.00 | 0.0 | 0.04 | in the spread |
| 5 337 units a meter | work p99 | 3.47 | 3.53 | +0.06 | +1.7 | 0.26 | in the spread |
| 6 widest zoom | frame mean | 16.67 | 16.67 | 0.00 | 0.0 | 0.01 | in the spread |
| 6 widest zoom | work mean | 0.90 | 0.93 | +0.02 | +2.6 | 0.08 | in the spread |
| 6 widest zoom | work p99 | 1.55 | 1.46 | -0.09 | -5.6 | 0.17 | in the spread |

"near the spread" is on "2 start, head down work median" (-0.01, spread 0.01), "3 village game turn" and "5 337 units a meter work median" (+0.01, spread 0.01). The worst frame of scene 6 is 6.04 ms in A and 11.96 ms in B, with a spread of 17.51 ms: one run of A had 23.33 ms.

## 3. The intro, 3a. in-old (dxwrapper, intro_keep_turn=0): PASS (the doubled image shows)

Log lines:
- "intro        : intro_keep_turn=0. The kept picture of each eye stays in its pixels when the head turns"
- line 687: "intro        : the invented head turned 35.9 degrees at pair 702. The kept picture of each eye is not moved by -0.404 of the half width and -0.368 of the half height (intro_keep_turn=0, intro_eye_trails=1). The pictures of this pair are saved"
- lines 697, 707: bwvr-eye-left-100702.bmp and bwvr-eye-right-100702.bmp are saved.
- line 786: "intro        : the kept picture moved with a turn of the head in 0 eye passes, by 0.0000 of the half width or height in the mean and 0.0000 at the most (intro_keep_turn=0)"

Pictures:
- Pair 668 (before the turn): one cloud of orbs in the middle, with the normal radial trails of the effect. No copy.
- Pair 702 (the turn), both eyes: YES, a second, weaker copy of the orb cloud shows. The main cloud (white orbs, black orbs) is in the top right. The copy is a full cloud of gray orbs to its left and below it, in the middle left of the picture. The copy is to the left by about 564 px (20 percent of the width). A search for the best match between the gray orbs and the white orbs gives a vertical offset of about 144 px (7 percent of the height), with the copy lower. That is less than the 18 percent in the log line. The camera of the effect also moves between pairs, and that can change the vertical match.
- Strength of the copy: the orbs of the copy are about 25 to 35 percent of the contrast of the main orbs. Measured as the peak over the local background: copy p75 35 and p90 39 levels, main p75 123 and p90 146 levels. The brightest copy orbs reach 175 on a background of about 100. The main orbs clip at 255 on a background of about 90. The copy is clearly visible at normal contrast in the full picture.
- Pair 787 (85 pairs after the turn): one copy of the logo (the lion of the orbs) on its plane at the right. No second copy. The trails decayed.
- Pair 906: one dense cloud, no copy.

## 3b. in-new (dxwrapper, intro_keep_turn=1): PASS

Log lines:
- "intro        : intro_keep_turn=1. The kept picture of each eye moves with a turn of the head"
- line 687: "intro        : the invented head turned 35.9 degrees at pair 702. The kept picture of each eye is moved by -0.404 of the half width and -0.368 of the half height (intro_keep_turn=1, intro_eye_trails=1). The pictures of this pair are saved"
- lines 697, 707: bwvr-eye-left-100702.bmp and bwvr-eye-right-100702.bmp are saved.
- line 786: "intro        : the kept picture moved with a turn of the head in 683 eye passes, by 0.0016 of the half width or height in the mean and 0.5466 at the most (intro_keep_turn=1)". N = 683, over 0.

Pictures (compared side by side with in-old of the same pair):
- Pair 702 (the turn), both eyes: one copy of the orb cloud only, at the same place as the main cloud of in-old. The gray copy at the middle left of in-old is not there. In the same region of in-old (705..1290, 800..1440 px) the peak search finds 893 orb peaks. In in-new it finds 1.
- The strip of the old picture: the strip has no orbs in it, only the background of the effect (gray streaks). So the strip shows no second logo. Faint straight seams in the background show the edges of blocks of the kept picture: a vertical seam at about x 556 px (20 percent of the width) from the top down to about y 1400 px, and horizontal seams at about y 1020, 1400 and 1800 px (the bottom strip is about 18 percent of the height). These seams are low contrast. A high-pass filter shows them clearly. At normal contrast they are just visible.
- One block in the top right (about x 1900..2760, y 210..615 px) has more faint small orb dots than the same block of in-old: 406 peaks against 180, with median peak contrast 34 levels (about 25 percent of the main orbs). These dots are like the normal faint trail dots of the effect (pair 667, before the turn, has the same kind of dots along the radial trails). They do not form a shifted copy of the cloud. Only a headset can show if the block edge is visible to the eye.
- Pair 785 (83 pairs after the turn): one copy of the logo, the same as in-old pair 787. No seam is visible.
- Pair 904: one dense cloud, the same as in-old pair 906.

## 4. Vulkan (no backend key): PASS for the log lines

Both Vulkan logs start with "build 4eb77f8". The job asked for the log lines only. The runs also saved eye pictures (bwvr-eye-*-100702.bmp and others). I did not examine them.

in-old-vk:
- "intro        : intro_keep_turn=0. The kept picture of each eye stays in its pixels when the head turns"
- "intro        : the invented head turned 35.9 degrees at pair 702. The kept picture of each eye is not moved by -0.404 of the half width and -0.368 of the half height (intro_keep_turn=0, intro_eye_trails=1). The pictures of this pair are saved"
- line 870: "intro        : the kept picture moved with a turn of the head in 0 eye passes, by 0.0000 of the half width or height in the mean and 0.0000 at the most (intro_keep_turn=0)"

in-new-vk:
- "intro        : intro_keep_turn=1. The kept picture of each eye moves with a turn of the head"
- "intro        : the invented head turned 35.9 degrees at pair 702. The kept picture of each eye is moved by -0.404 of the half width and -0.368 of the half height (intro_keep_turn=1, intro_eye_trails=1). The pictures of this pair are saved"
- line 870: "intro        : the kept picture moved with a turn of the head in 789 eye passes, by 0.0014 of the half width or height in the mean and 0.5466 at the most (intro_keep_turn=1)". N = 789, over 0.

## Odd items

- The new key moves the kept picture in 683 (dxwrapper) or 789 (Vulkan) eye passes, with a mean of 0.0016 of the half width. The script has one turn only. So the invented head also has small pose changes in most pairs, and the key acts on each of them. This is not a fault, but the count is not "the number of turns".
- The vertical offset of the copy that I measured in in-old (about 7 percent of the height) is less than the offset in the log line (18 percent). The horizontal offset agrees (20 percent).
- No turn line near pair 1100, because the logo ends first.

Crops and high-pass pictures (game art, local only): C:\BW1-VR\local\shots\job92\ (pair702-old-new.png, in-old-left-all.png, in-new-left-all.png, enh-old-left-702.png, enh-new-left-702.png, pair702-left-upright.png, enh-new-left-702-ur.png, enh-new-left-667.png, enh-new-left-785.png, enh-old-left-787.png).

The intro, general: the script look@700+400 gives one turn of the invented head. Each log has one turn line, at pair 702. No log has a turn line near pair 1100, because the logo ends first (1044 and 1043 pairs on dxwrapper, 1095 and 1096 pairs on Vulkan). The offset in the turn line is 0.404 of the half width (20.2 percent of the full width, 557 px of 2760) and 0.368 of the half height (18.4 percent of the full height). The "about 40% of the width" of the job is 40 percent of the half width. All four runs have test_keys=1 in bwvr.ini.run, from the bwvr.ini of the player. The pictures are saved without a change of the recipe.
