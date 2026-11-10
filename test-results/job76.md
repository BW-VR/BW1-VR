# Job 76: the floor of the fly by the measure of the blinder (agent/move b6397f5)

`.\dev.ps1 build -Clean`: 6366 checks, 0 failed. Each log has "build b6397f5". `spawn_run.ps1`, test save temple, Vulkan, `card_send_test_save=0`. fly1c and hill1 are RUN DONE. Before these runs I removed the line `card_send_test_save=8` from bwvr.ini of the player in the game folder of the laptop.

## fly1c: PASS, with a stop at a tree

- No line "head blinder : the overlay" in the log.
- First fly (log 171 to 411): the start line has "45.0 degrees below level". Each of the 135 trace lines has "a clearance of" 10.00 cm or more (the least is 10.00). "below" reaches 10.00 at fly frame 30 (log 200). Where the slope rises at the floor, the step up is above 0 (42 lines, for example "0.633 cm up ... a clearance of 10.00 cm" at fly frame 70 and "1.214 cm up" at 100).
- The end line: "The fly went 48.6 cm down in the room (move_type=1). The floor of the fly lifted the view 218.5 cm on 89 frames, and the least clearance of your head after a step was 10.0 cm. 107 steps stopped short of a solid. The speed ramp went up to 2.83 times", frame 411.
- The note: "god view     : the fly stops short: the whole step puts your head 26.7 cm out of the Tree 1632AEE0, so your head stays 30 cm out of it (god_zoom_solid_stop=1), frame 304". After that the step is 0.085 cm (frame 304) and 0.000 cm (frame 305), and the head stays at 2118.0 2464.2. The stick is pushed until frame 410, so for about 106 frames the fly does not move ("the view moved on 135 of 240 frames", "which gives 66.8 cm of the room each second"). The log has one "stops short" line for the 107 steps. The fly stops at the tree. It does not slide past it.
- Second fly (log 471 to 871): "below" goes from 38.2 past 250 to 2034.3 cm. "The fly went 1993.5 cm up in the room (move_type=1). The floor of the fly lifted the view 0.2 cm on 1 frames ... 0 steps stopped short of a solid. The speed ramp went up to 7.15 times". The camera zone of the script stops the place 50 units past the zone, as in job 74.
- Strafe (log 971 to 1031): "0.000 cm up" in all 60 trace lines. "The fly went 0.0 cm up ... the least clearance of your head after a step was 2034.3 cm".
- "walk check ... 0.000 units apart. In those frames the drawn head moved 0.00 cm of the room" after each of the three stops.

## hill1: PASS

- No line "head blinder : the overlay" in the log.
- Start: "it is 0.0 degrees to the right of straight ahead in the room and 0.0 degrees below level ... The speed stays the speed of the push (move_stick_ramp=0), frame 171".
- Trace: over level land the steps are 2.49 to 2.65 cm with "0.000 cm up" (fly frames 1 to about 195, clearance 60 cm down to 12.63 cm). Where the hill comes within 10 cm, the steps go up: fly frame 200 "a step of 4.597 cm of the room (3.855 cm up ...), a clearance of 10.00 cm", frame 210 "0.492 cm up ... a clearance of 10.00 cm". Past the top: "0.000 cm up" again (frame 220, clearance 17.85 cm). 18 lines have a step up above 0.
- End: "The fly went 37.3 cm up in the room (move_type=1). The floor of the fly lifted the view 37.3 cm on 18 frames, and the least clearance of your head after a step was 10.0 cm. 0 steps stopped short of a solid", frame 471. "which gives 150.0 cm of the room each second".
- The walk check is 0.000 units apart.
