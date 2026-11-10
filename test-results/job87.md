# Job 87: the speed of the stick with a factor of the scale, and the head measure with two pairings (agent/speed 88a3995)

`.\dev.ps1 build -Clean`: 6525 checks, 0 failed. Each log has "build 88a3995". `spawn_run.ps1`, test save temple, Vulkan, the keys S and the steps T of the job. All six runs are RUN DONE.

## The table

| run | scale | factor | walk stop line, cm each second | scale check across | across over the factor | grab camera / grips cm | head, frame before / same frame |
|---|---|---|---|---|---|---|---|
| speed-scale-1 | 1.000 | 4.00 | 600.0 | 600.0 (0.0 up and down) | 150.0 | 40.0 / 40.0 | 0.500 / 0.500 |
| speed-scale-7 | 7.500 | 1.46 | 219.1 | 219.1 (0.0) | 150.0 | 40.0 / 40.0 | 3.750 / 3.750 |
| speed-scale-100 | 100.000 | 0.60 | 90.0 | 90.0 (8.9) | 150.1 | 40.0 / 40.0 | 50.000 / 50.000 |
| speed-scale-1-off | 1.000 | 1.00 | 150.0 | 150.0 (0.0) | 150.0 | 40.0 / 40.0 | 0.500 / 0.500 |
| speed-scale-7-off | 7.500 | 1.00 | 150.0 | 150.0 (0.0) | 150.0 | 40.0 / 40.0 | 3.750 / 3.750 |
| speed-scale-100-off | 100.000 | 1.00 | 150.0 | 150.1 (32.2) | 150.1 | 40.0 / 40.0 | 50.000 / 50.000 |

The speed of the stick, the grab, the eyes and the grip PASS in all six runs. The head FAILS in all six runs: the pairing with the declared eye of the frame before gives the same half scale as the pairing of the same frame.

## The speed of the stick (move_stick_scale=1): PASS

- speed-scale-1, start: "At 1.00 units per meter the speed of the stick is times 4.00 (move_stick_scale=1: (16.0 / scale) ^ 0.50, from 0.60 to 4.00): 600 cm of the room each second", frame 521.
- speed-scale-1, stop: "You went 12.00 units over the land, which is 1199.98 cm of the room ... The stick was pushed for 2.000 s, which gives 600.0 cm of the room each second ... The view went 0.0 cm up and 0.0 cm down with the land (move_stick_follow_land=1). The scale made the speed of the stick 4.00 to 4.00 times move_stick_speed_cm (move_stick_scale=1)", frame 641.
- speed-scale-1, scale check: "In 119 frames of a walk the camera of the game went 11.90 units in 1.98 s: 600.0 cm of the room each second at the units per meter of the eyes, 600.0 across the land and 0.0 up and down. The scale makes the speed of the stick 4.00 times move_stick_speed_cm (move_stick_scale), and the speed across over that factor is 150.0 cm each second. In 149 frames of a grab of the world the camera of the game went 0.40 units, 40.0 cm of the room at the units per meter of the eyes, and the grips went 40.0 cm."
- speed-scale-7, start: "At 7.50 units per meter the speed of the stick is times 1.46 (move_stick_scale=1: (16.0 / scale) ^ 0.50, from 0.60 to 4.00): 219 cm of the room each second".
- speed-scale-7, stop: "You went 32.86 units over the land, which is 438.17 cm of the room ... which gives 219.1 cm of the room each second ... The scale made the speed of the stick 1.46 to 1.46 times move_stick_speed_cm (move_stick_scale=1)", frame 641.
- speed-scale-7, scale check: "In 119 frames of a walk the camera of the game went 32.58 units in 1.98 s: 219.1 cm of the room each second at the units per meter of the eyes, 219.1 across the land and 0.0 up and down. The scale makes the speed of the stick 1.46 times move_stick_speed_cm (move_stick_scale), and the speed across over that factor is 150.0 cm each second. In 149 frames of a grab of the world the camera of the game went 3.00 units, 40.0 cm of the room at the units per meter of the eyes, and the grips went 40.0 cm."
- speed-scale-100, start: "At 100.00 units per meter the speed of the stick is times 0.60 (move_stick_scale=1: (16.0 / scale) ^ 0.50, from 0.60 to 4.00): 90 cm of the room each second".
- speed-scale-100, stop: "You went 179.98 units over the land, which is 179.98 cm of the room ... which gives 90.0 cm of the room each second ... The view went 17.6 cm up and 0.0 cm down with the land (move_stick_follow_land=1). The scale made the speed of the stick 0.60 to 0.60 times move_stick_speed_cm (move_stick_scale=1)", frame 641.
- speed-scale-100, scale check: "In 119 frames of a walk the camera of the game went 180.60 units in 1.98 s: 91.1 cm of the room each second at the units per meter of the eyes, 90.0 across the land and 8.9 up and down. The scale makes the speed of the stick 0.60 times move_stick_speed_cm (move_stick_scale), and the speed across over that factor is 150.1 cm each second. In 149 frames of a grab of the world the camera of the game went 40.00 units, 40.0 cm of the room at the units per meter of the eyes, and the grips went 40.0 cm."

## The same speed at each scale (move_stick_scale=0): PASS

- Each start line has "The speed is the same at each scale (move_stick_scale=0): 150 cm of the room each second".
- speed-scale-1-off: "You went 3.00 units over the land, which is 299.94 cm ... which gives 150.0 cm of the room each second", frame 641. Scale check: "150.0 cm of the room each second at the units per meter of the eyes, 150.0 across the land and 0.0 up and down. The scale makes the speed of the stick 1.00 times move_stick_speed_cm (move_stick_scale), and the speed across over that factor is 150.0 cm each second. ... the camera of the game went 0.40 units, 40.0 cm of the room ..., and the grips went 40.0 cm."
- speed-scale-7-off: "You went 22.50 units over the land, which is 300.00 cm ... 150.0 cm of the room each second". Scale check: "150.0 across the land and 0.0 up and down ... 1.00 times ... 150.0 cm each second ... 3.00 units, 40.0 cm of the room ..., and the grips went 40.0 cm."
- speed-scale-100-off: "You went 299.99 units over the land, which is 299.99 cm ... 150.0 cm of the room each second", with "The view went 29.3 cm up and 34.6 cm down with the land". Scale check: "157.1 cm of the room each second at the units per meter of the eyes, 150.1 across the land and 32.2 up and down ... 1.00 times ... 150.1 cm each second ... 40.00 units, 40.0 cm of the room ..., and the grips went 40.0 cm." So the 157.1 of job 83 is the up and down move on the slope.

## The eyes and the grip: PASS

Each run has the eyes and the grip at the scale of the God view, for example "The eye cameras of the game give 7.500 units per meter (7.500 to 7.500, 328 frames) ... a move of the drawn grip 7.500 (3)" and "The eye cameras of the game give 100.000 units per meter (100.000 to 100.000, 408 frames) ... a move of the drawn grip 100.000 (3)".

## The head: FAIL in all six runs

The report lines:
- speed-scale-1: "a move of the head 0.500 with the declared eye of the frame before (2 moves of 5 cm) and 0.500 with the declared eye of the same frame (2)".
- speed-scale-7: "a move of the head 3.750 with the declared eye of the frame before (2 moves of 5 cm) and 3.750 with the declared eye of the same frame (2)".
- speed-scale-100: "a move of the head 50.000 with the declared eye of the frame before (2 moves of 5 cm) and 50.000 with the declared eye of the same frame (2)".
- The three runs with move_stick_scale=0 have the same numbers.

The single lines of speed-scale-1 (speed-scale-1-off has the same lines):
- "scale check  : a move of the head of 14.14 cm from frame 42 to frame 171, with the declared eye of the same frame: the camera of the left eye pass went 0.0000 units, 0.000 units per meter"
- "scale check  : a move of the head of 14.14 cm from frame 43 to frame 172, with the declared eye of the frame before: the camera of the left eye pass went 0.0000 units, 0.000 units per meter"
- "scale check  : a move of the head of 14.14 cm from frame 171 to frame 271, with the declared eye of the same frame: the camera of the left eye pass went 0.1413 units, 0.999 units per meter"
- "scale check  : a move of the head of 14.14 cm from frame 172 to frame 272, with the declared eye of the frame before: the camera of the left eye pass went 0.1413 units, 0.999 units per meter"

speed-scale-7 has "from frame 122 to frame 171 ... 0.0000 units, 0.000 units per meter", "from frame 122 to frame 172 ... 0.0000 units", "from frame 171 to frame 271 ... 1.0607 units, 7.500 units per meter" and "from frame 172 to frame 272 ... 1.0607 units, 7.500 units per meter". speed-scale-100 has 0.0000 units at 171 and 172, and "14.1420 units, 99.999 units per meter" at 271 and 272.

So the pairing with the frame before also gives 0 at the step down (frame 172) and the full scale at the step up (frame 272). This is not as the job expects: the frame-before pairing does not give the full scale on both moves.

What these lines show:
- At frame 171 and at frame 172 the camera of the left eye pass is at the same place as at the start (0.0000 units), with the declared eye 14.14 cm away.
- At frame 271 and 272 the declared eye is back at its place before the lean. But the camera of the eye pass is 14.14 cm (times the scale) from its place at frame 171 and 172, and that was the place before the lean. So at 271 and 272 the camera is still at the leaned place.
- This agrees with a camera of the eye pass that follows the declared eye two frames or more late. A lag of one frame would give the full scale with the frame-before pairing at frame 172. A lag of two frames or more gives 0 at 172 and the full scale at 272, as in these logs.
- These runs do not give the length of the lag. A pairing with the declared eye of 2, 3 and 4 frames before, or a log of the camera for the 5 frames after the step, can give it.
