# Job 74: the fly with the stick and the speed ramp (agent/move 693ffb7)

`.\dev.ps1 build -Clean`: 6360 checks, 0 failed. Each log has "build 693ffb7". `spawn_run.ps1`, test save temple, Vulkan. All eight runs are RUN DONE.

## The runs as sent (fly1, fly2, walk1, ramp1): FAIL because of the recipe

In each run one frame near log frame 230 to 304 takes about 300 ms ("worst frame  : 298.72 ms" in fly1, 304.37 ms in fly2, 298.90 ms in walk1, 307.46 ms in ramp1). In that frame the mod writes the BMP dumps of the cards (bwvr-cardsend-NN-levels and -views), because `card_send_test_save=8` is in the run. The key comes from bwvr.ini of the player in the game folder: the run copy `bwvr.ini.player` has `card_send_test_save=8`, and that file is dated 8 November 17:32. spawn_run does not set the key.

Usable() counts a gap of more than 250 ms between frames as a reason that the controllers cannot act. So the next frame gives "controllers  : the stick of the other hand does not move you: the controllers cannot act now" (fly1 frame 292, fly2 304, walk1 235, ramp1 235). The push must then come back to the middle before it moves the player again.
- fly1: the first fly stops after 121 of 240 frames, at frame 292. The stick comes back at 410.
- walk1: the first walk stops after 64 of 300 frames.
- ramp1: the stick is not in the middle from step 900 to step 1500. So after the refusal the half push and the second full push do not move the player. Only the last push (frames 871 to 931) walks.
- fly2: the start line says "8.0 degrees to the right of straight ahead in the room and 6.0 degrees below level". The log has "fake hands, offset 20 -30 35 cm, yaw 8 pitch 40", but the direction of the stick uses `fake_controller_sweep_yaw=8` and `fake_controller_sweep_pitch=-6` of the script.

The runs fly1b, fly2b, walk1b and ramp1b are the same recipes with `card_send_test_save=0`, and fly2b also has `fake_controller_sweep_pitch=40`. In these runs the worst frame of the first window is 168 to 175 ms, and no push is refused.

## fly1b: PASS, with two notes

- Start: "the fly with the stick starts at 100.00 units per meter, with the land under your head at 1858.8 2528.7, 61.6 cm below your head. Forward is the direction of your head (move_stick=1), and it is 0.0 degrees to the right of straight ahead in the room and 45.0 degrees below level. ... your head at most 10000 units over the land (move_type=1). After 1 s of a push of 90% or more the speed doubles each 2 s, up to 32 times (move_stick_ramp=1), frame 171".
- "vignette     : the edges of the view go dark (gentle), the fly with the stick" at frames 172, 472 and 972.
- First fly (240 frames, 171 to 410): "below" goes from 59.54 to 10.00 at fly frame 29 (log 199). From fly frame 30 to 60 it stays at 10.00 with "0.000 cm up" while x and z change (1910.3 2515.9 to 1961.7 2503.1). Where the land falls away, "below" goes up to 20.8 (fly frame 120) and the step up is negative, because the fly goes on down at 45 degrees. 113 of 240 frames have a step up that is not 0. Stop: "The fly went 252.7 cm down in the room (move_type=1). The speed ramp went up to 2.83 times", frame 411.
- Note 1: the head blinder starts four times in the first fly at the floor of 10 cm: frames 235 to 248 (least clearance 1.5 cm), 272 to 287 (-0.0 cm), 288 to 293 (3.4 cm) and 330 to 406 (1.27 s, "The least clearance of your head was -5.0 cm (less than 0: in the land or in a solid)"). Each start line says "your head is N cm over the land" with N from 2.7 to 4.8. So the floor of 10 cm under the head does not keep the blinder off on this slope.
- Second fly (400 frames, 471 to 870): "below" 22.39, down to 10.00 at fly frame 27, past 250 near fly frame 130, and 2024.56 at the end. The factor grows from 1.00 to 7.15. Stop: "The land under your head was 20.6 cm below your head and is 2024.6 cm below it. The fly went 1993.3 cm up in the room (move_type=1). The speed ramp went up to 7.15 times the speed of the push", frame 871.
- Strafe (60 frames, 971 to 1030): "0.000 cm up" in each frame, "below" 2024.56 in each frame, factor 1.00. "The fly went 0.0 cm up".
- "walk check, 30 frames after the stop ... 0.000 units apart. In those frames the drawn head moved 0.00 cm of the room" after each of the three stops.
- Note 2: the top of the second fly, about 20 m up. No error line, no blinder line, and no line of a limit of the view. One line of the camera zone of the script: "camera zone  : the fly with the stick asked for your place at 879.4 2772.5, 57.4 units past the zone. It stops at 886.2 2775.6, 50.0 units past it (the limit of god_zone_margin=50, or the place before the move)", and 8 lines "the game camera stays in the zone". The fly then goes on along the edge of the zone (x stays near 846, z changes).

## fly2b: PASS

- "Forward is the direction in which the controller points (move_stick=2), and it is 8.0 degrees to the right of straight ahead in the room and 40.0 degrees above level", frame 171.
- "below" goes from 62.91 to 1895.43 with no stop: 124.28 at fly frame 40, 296.23 at 120, 1159.59 at 320. Each step up is positive.
- The factor is 1.00 to fly frame 60, then 1.01 at 61, 1.12 at 80, 2.83 at 240 and 7.13 at 400.
- Stop: "The land under your head was 61.6 cm below your head and is 1895.4 cm below it. The fly went 1806.0 cm up in the room (move_type=1). The speed ramp went up to 7.13 times", frame 571. The walk check is 0.000 units apart. No blinder line.

## walk1b: PASS, with other heights

- Three "the walk with the stick starts" lines, each with "The height of the view follows the land under your head (move_stick_follow_land=1). The speed stays the speed of the push (move_stick_ramp=0)". No line in the log has "fly with" or "fly trace".
- Each stop line: "which gives 150.0 cm of the room each second", over 300, 300 and 200 of the frames.
- 800 "walk trace" lines, each with "the speed times 1.00".
- Land below the head at the starts: 61.6, 62.9 and 61.6 cm. At the ends: 62.9, 61.6 and 61.6 cm (not 63.8 at the first end). The second walk starts where the first walk ends, so it starts at 62.9.
- The walk check is 0.000 units apart after each stop.

## ramp1b: PASS, with the factor 4.00 at frame 300

- Full push, walk frames 1 to 60: a step of 2.45 to 2.51 cm, "times 1.00". 1.01 at frame 61.
- Frames 60 to 300: 1.41 at 120, 2.00 at 180, 2.83 at 240 and 4.00 at 300, with a step of 10.02 cm at 300. That is the rule of the start line (the speed doubles each 2 s after 1 s), not about 3.2.
- Half push: at walk frame 301 (log 471) the factor is 1.00 and the step 0.768 cm, in the same frame. No stop line between the pushes.
- Full push again (walk frame 421, log 591): a step of 2.507 cm and 1.00 for 60 frames, then 1.41 at 540 and 2.00 at 600.
- Stop: "The stick was pushed for 10.000 s ... The speed ramp went up to 4.00 times the speed of the push (move_stick_ramp=1)", frame 771. The walk check is 0.000 units apart.
- The last push (871 to 930) starts at 1.00 and stays at 1.00 for its 60 frames: "The speed ramp went up to 1.00 times".
