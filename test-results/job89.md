# Job 89: the gain of the one-hand grab, and the step watch of the head (agent/speed 6633f8b)

`.\dev.ps1 build -Clean`: 6603 checks, 0 failed. Each log has "build 6633f8b". `spawn_run.ps1`, test save temple, Vulkan, the keys G and the steps GT of the job. All five runs are RUN DONE. Job 88 had already started when this job came, so this job ran after it.

## 1. grab-100: PASS

- The press: "grab         : the gain of the grab (grab_boost=1): 1.00 up, with the land 0.30 m below the hand (reach 50 cm, at most 50), and 1.00 across at 100.00 units per meter, frame 371".
- The release: "grab         : the hand went 40.0 cm and the land 40.0 cm in the room. The gain of the grab was 1.00 to 1.00 up and 1.00 to 1.00 across (grab_boost=1)", after "released the table (the grips opened) after 150 frames, frame 521".
- The grab crosses two report windows of the scale check, and each window has one half: "In 77 frames of a grab of the world the camera of the game went 20.00 units, 20.0 cm of the room at the units per meter of the eyes, and the grips went 20.0 cm. The gain of the grab was 1.00 to 1.00 (grab_boost, 1 is one to one)." and "In 72 frames ... 20.00 units, 20.0 cm ... and the grips went 20.0 cm. The gain of the grab was 1.00 to 1.00". So the camera went 40.0 cm for 40.0 cm of the grips.

## 2. grab-1: PASS

- The press: "the gain of the grab (grab_boost=1): 1.00 up, with the land 0.30 m below the hand (reach 50 cm, at most 50), and 4.00 across at 1.00 units per meter, frame 371".
- The release: "the hand went 40.0 cm and the land 161.0 cm in the room. The gain of the grab was 1.00 to 1.00 up and 4.00 to 4.00 across (grab_boost=1)", frame 521. Also "the land moved 161.0 cm in the room under the grip, and the hand felt 17 ticks of the land".
- The scale check: "In 77 frames of a grab of the world the camera of the game went 0.80 units, 80.0 cm of the room at the units per meter of the eyes, and the grips went 20.0 cm. The gain of the grab was 4.00 to 4.00 (grab_boost, 1 is one to one)." and the same for the 72 frames of the next window. So the camera went 160.0 cm for 40.0 cm of the grips.

## 3. grab-deep-1: PASS

- The fly: "frame 571. The land under your head was 61.6 cm below your head and is 1289.2 cm below it", "The fly went 1199.7 cm up in the room (move_type=1)". So the land is about 12.9 m below the head after the fly, not about 20 m.
- No "ground stops" line in the log.
- The three pulls (the hand goes 50 cm up in each):

| pull | press, frame | land below the hand at the press | gain up at the press | release line | land below the hand at the release |
|---|---|---|---|---|---|
| 1 | 671 | 12.29 m | 24.58 | "the hand went 50.0 cm and the land 801.6 cm in the room. The gain of the grab was 9.55 to 24.96 up and 1.00 to 1.00 across", frame 731 | 4.77 m |
| 2 | 771 | 4.28 m | 8.55 | "the hand went 50.0 cm and the land 294.9 cm ... 3.65 to 8.94 up", frame 831 | 1.83 m |
| 3 | 871 | 1.33 m | 2.65 | "the hand went 50.0 cm and the land 108.5 cm ... 1.48 to 3.03 up", frame 931 | 0.75 m |

- The press lines are "the gain of the grab (grab_boost=1): 24.58 up, with the land 12.29 m below the hand (reach 50 cm, at most 50), and 1.00 across at 100.00 units per meter, frame 671", then "8.55 up, with the land 4.28 m below the hand", then "2.65 up, with the land 1.33 m below the hand".
- The land below the hand at the release is the depth at the press, plus the 50 cm of the hand, less the cm of the land. The depth at the next press is 50 cm less, because the hand goes down again between the pulls (4.77 to 4.28, 1.83 to 1.33).
- The land never passes the hand: after the third pull it is 0.75 m below the hand. The gain of the third pull goes down to 1.48, so the land does not come nearer than 50 cm to the hand.
- After the third pull: "god view     : the waist point is at 845.2 25.2 2873.9, heading 103.97 degrees, 100.0 units per meter. The land under your head is 0.85 m below it", then 0.84 m. That is less than 2.5 m.

## 4. grab-deep-0 (grab_boost=0): PASS

- The fly: "frame 571. The land under your head was 61.6 cm below your head and is 1285.8 cm below it", "The fly went 1196.3 cm up".
- No press line of a gain. Each pull: "grab         : the ground went 50.00 cm up in the room since the anchor, and the hand went 50.00 cm up", frames 725, 825 and 925.
- After the third pull: "The land under your head is 11.37 m below it", then 11.36 m. So the land came up 1.5 m in all, and stays about 11.4 m below (12.86 m less 1.5 m).

## 5. head-step: PASS

- "scale check  : a step of the head of 14.14 cm at frame 171. The camera of the left eye pass from its place in the frame before: 0.0000 0.0000 0.1413 0.1413 0.1413 0.1413 units in that frame and the 5 after (the full move is 0.1414 units at 1.000 units per meter of the eyes). So the camera has the full move 2 frames after the declared eye, and with the declared eye of 2 frames before the head gives 0.999 units per meter".
- The line for frame 271 is the same, with the same six values.
- The six values are 0.1413 and not 0.1414, and the end is 0.999 and not 1.000. The difference is in the last digit.
- The window line: "a move of the head 0.500 with the declared eye of the frame before (2 moves of 5 cm) and 0.500 with the declared eye of the same frame (2), a step of the head 0.999 at the lag (2 steps, the camera at most 2 frames after the declared eye, 0 steps with no full move in 6 frames)".
- So the camera of the eye pass has the move of the head 2 frames after the declared eye.
