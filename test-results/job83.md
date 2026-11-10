# Job 83: the scale and the speed from the eye cameras, and the land depth (agent/speed 4e2db48)

`.\dev.ps1 build -Clean`: 6471 checks, 0 failed. Each log has "build 4e2db48". `spawn_run.ps1`, test save temple, Vulkan, `card_send_test_save=0`. All runs are RUN DONE, and after each run the log of the chain has "profile put back".

## Runs 1 to 3: the scale check

| run | God view | eyes | head | grip | walk, cm of the room each second | grab: camera cm / grip cm |
|---|---|---|---|---|---|---|
| speed-scale-1 | 1.000 | 1.000 | 0.500 | 1.000 | 150.0 | 40.0 / 40.0 |
| speed-scale-7 | 7.500 | 7.500 | 3.750 | 7.500 | 150.0 | 40.0 / 40.0 |
| speed-scale-100 | 100.000 | 100.000 | 50.000 | 100.000 | 157.1 | 40.0 / 40.0 |

The eyes, the grip, the walk and the grab PASS. The head FAILS: it is half the scale of the God view in each run.

### speed-scale-1: the eyes, the grip, the walk and the grab PASS, the head FAILS

- "scale check  : a report window. The God view is at 1.000 units per meter. The eye cameras of the game give 1.000 units per meter (1.000 to 1.000, 408 frames), a move of the head 0.500 (2 moves of 5 cm), a move of the drawn grip 1.000 (3), and a move of the Hand of the game 0.000 (0)."
- "scale check  : a report window. The God view is at 1.000 units per meter. The eye cameras of the game give 1.000 units per meter (1.000 to 1.000, 450 frames), a move of the head 0.000 (0 moves of 5 cm), a move of the drawn grip 1.000 (1), and a move of the Hand of the game 0.000 (0). In 119 frames of a walk the camera of the game went 2.98 units in 1.98 s: 150.0 cm of the room each second at the units per meter of the eyes. In 149 frames of a grab of the world the camera of the game went 0.40 units, 40.0 cm of the room at the units per meter of the eyes, and the grips went 40.0 cm."
- The walk: "You went 3.00 units over the land, which is 300.13 cm of the room ... which gives 150.1 cm of the room each second, and the view moved on 120 of 120 frames", frame 641.
- The grab: "The hand moved 0.00 0.00 -40.00 cm ... The table center moved 0.400 units", frame 853. "the land moved 41.1 cm in the room under the grip", frame 871.

### speed-scale-7: the eyes, the grip, the walk and the grab PASS, the head FAILS

- "god view     : zoomed in, 15.0 to 7.5 units per meter, about the point at 1900.4 29.5 2519.9, 0.60 m below your head: the land under your head (god_zoom_about=0)".
- "scale check  : the scale changed. The God view is at 15.000 units per meter. The eye cameras of the game give 15.000 units per meter (15.000 to 15.000, 80 frames), a move of the head 0.000 (0 moves of 5 cm), a move of the drawn grip 0.000 (0), and a move of the Hand of the game 0.000 (0)."
- "scale check  : a report window. The God view is at 7.500 units per meter. The eye cameras of the game give 7.500 units per meter (7.500 to 7.500, 328 frames), a move of the head 3.750 (2 moves of 5 cm), a move of the drawn grip 7.500 (3), and a move of the Hand of the game 0.000 (0)."
- "scale check  : a report window. The God view is at 7.500 units per meter. The eye cameras of the game give 7.500 units per meter (7.500 to 7.500, 450 frames), a move of the head 0.000 (0 moves of 5 cm), a move of the drawn grip 7.500 (1), and a move of the Hand of the game 0.000 (0). In 119 frames of a walk the camera of the game went 22.31 units in 1.98 s: 150.0 cm of the room each second at the units per meter of the eyes. In 149 frames of a grab of the world the camera of the game went 3.00 units, 40.0 cm of the room at the units per meter of the eyes, and the grips went 40.0 cm."
- The walk: "You went 22.50 units over the land, which is 299.99 cm of the room ... 150.0 cm of the room each second", frame 641. The grab: "The table center moved 3.000 units", frame 853.

### speed-scale-100: the eyes, the grip and the grab PASS, the walk is 4.7% fast, the head FAILS

- "scale check  : a report window. The God view is at 100.000 units per meter. The eye cameras of the game give 100.000 units per meter (100.000 to 100.000, 408 frames), a move of the head 50.000 (2 moves of 5 cm), a move of the drawn grip 100.000 (3), and a move of the Hand of the game 0.000 (0)."
- "scale check  : a report window. The God view is at 100.000 units per meter. The eye cameras of the game give 100.000 units per meter (100.000 to 100.000, 450 frames), a move of the head 0.000 (0 moves of 5 cm), a move of the drawn grip 100.000 (1), and a move of the Hand of the game 0.000 (0). In 119 frames of a walk the camera of the game went 311.54 units in 1.98 s: 157.1 cm of the room each second at the units per meter of the eyes. In 149 frames of a grab of the world the camera of the game went 40.00 units, 40.0 cm of the room at the units per meter of the eyes, and the grips went 40.0 cm."
- The walk line of the God view: "You went 300.05 units over the land, which is 300.05 cm of the room ... 150.0 cm of the room each second", frame 641, with "The view went 29.3 cm up and 34.6 cm down with the land". Only this run has a walk over a slope. So the 157.1 cm of the camera of the game can include the up and down moves of the view. The runs at 1 and 7.5 units per meter go 0.0 cm up and down, and give 150.0.

### The head

The lean of the recipe is one step: "head         : the invented head leans 10 cm right, -10 cm up and 0 cm forward", "controllers  : the fake lean is down at frame 170", "the fake lean is up at frame 270". The head gives exactly half the scale of the God view in each run, as a mean of 2 moves. This agrees with one move at the full scale and one move at 0 (view_check.cpp, AnchorMove: the anchor ends at the first frame with a move of 5 cm). For example, a camera of the eye pass that is one frame later than the declared eye, at a lean in one frame, gives that. The log does not give each move, so these runs do not show which move gave 0. The eyes, the grip, the walk and the grab agree with the God view, so the cause can be in the measure and not in the view.

## Run 4, speed-place-1 (move_stick_follow_place=1): PASS with a change of the recipe

- As sent (spawn_test_temple=1400): the jump comes after the last walk. "comfort      : a test calls the game's zoom to the Temple, ... update 1400" is after the walk that stops at frame 1211, so no walk follows the jump.
- spawn_test_temple=900 (speed-place-1b): the jump came at about log frame 850, 40 frames after a walk. "comfort      : the player moved the view, so the move of the game camera is dropped, update 900". So the view did not go to the Temple.
- speed-place-1c: spawn_test_temple=1000, and the steps "lstick_up@900+300 lstick_right@1300+40 lstick_right@1400+40 lstick_right@1500+40 lstick_right@1800+40 lstick_right@1900+40 lstick_right@2000+40". The walks at step 1600 and 1700 are gone, so there are about 200 frames with no walk before the jump. The other keys are as sent.
  - "comfort      : a test calls the game's zoom to the Temple, to 2238 3156, 130 units away and 45.0 degrees down, with the flag 0, update 1000". This is after the walk that stops at frame 811 and before the walk that starts at frame 1071.
  - "god view     : the view goes to the game camera (a jump of the game camera). ... The waist point was at 1968.0 29.7 2470.6, heading 104.0 degrees. Now it is at 2235.4 16.3 3156.7, heading 104.0 degrees, 10.0 units per meter. ... the land under your head is 0.62 m below it (god_spawn=1)."
  - "god view     : the view went to a camera of the game since the last walk. The filter of the land holds 29.59, and the land under your head is at 16.08: -135.1 cm of the room apart. The filter starts again at the land under your head (move_stick_follow_place=1)".
  - The next walk: "... frame 1111. The land under your head was 62.1 cm below your head and is 62.1 cm below it. The view went 0.0 cm up and 0.0 cm down with the land (move_stick_follow_land=1)."
  - The two walks after it: "62.1 cm below your head and is 63.7 cm below it. The view went 0.0 cm up and 0.3 cm down", and "63.7 cm below your head and is 103.8 cm below it. The view went 0.0 cm up and 13.7 cm down".

## Run 5, speed-place-0c (move_stick_follow_place=0): PASS

The same recipe as speed-place-1c, with move_stick_follow_place=0.
- The jump is the same, at update 1000, with "Now it is at 2235.4 16.3 3156.7".
- "god view     : the view went to a camera of the game since the last walk. The filter of the land holds 29.59, and the land under your head is at 16.08: -135.1 cm of the room apart. The filter goes on from the land of the last walk (move_stick_follow_place=0)".
- The first walk after the jump: "... frame 1111. The land went 70.7 cm down in the room, so a hill under your head is 10 cm below it. The land under your head was 62.1 cm below your head and is 10.0 cm below it. The view went 0.0 cm up and 122.8 cm down with the land".
- The next walks: "10.0 cm below your head and is 10.9 cm below it. The view went 0.0 cm up and 12.2 cm down", and "10.9 cm below your head and is 51.0 cm below it. The view went 0.0 cm up and 14.1 cm down".
- So the walks after the jump go 122.8 + 12.2 + 14.1 = 149.1 cm down. The filter line gives 135.1 cm. In the first walk the floor of 10 cm stops the view, and the other two walks also follow the land, so the two numbers are not the same.

## Run 6, speed-deep-1 (god_zoom_far_level=1): PASS

- "god view     : the fly with the stick stopped (the stick is back). You went 1125.04 units over the land ... frame 571. The land under your head was 61.6 cm below your head and is 2105.1 cm below it. The fly went 2015.6 cm up in the room (move_type=1)."
- Zoom in: "It zoomed in from 100.00 to 44.44 units per meter, which is 2.00 zoom steps", then "god view     : in the smooth zoom the land under your head went from 21.05 m to 21.05 m below your head (god_zoom_far_level=1)".
- Zoom out: "It zoomed out from 44.44 to 99.99 units per meter", then "in the smooth zoom the land under your head went from 21.05 m to 10.75 m below your head (god_zoom_far_level=1)". The formula gives 2.5 + (21.05 - 2.5) / 2.25 = 10.74 m.

## Run 7, speed-deep-0 (god_zoom_far_level=0): PASS

- The fly ends with "The land under your head was 61.6 cm below your head and is 2082.9 cm below it", frame 571.
- Zoom in: "in the smooth zoom the land under your head went from 20.83 m to 43.74 m below your head (god_zoom_far_level=0)". The formula gives 2.5 + (20.83 - 2.5) * 2.25 = 43.74 m.
- Zoom out: "went from 43.74 m to 20.83 m below your head (god_zoom_far_level=0)".
