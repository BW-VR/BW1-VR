# Job 78: the fly slides past a solid (agent/move 7137e46)

`.\dev.ps1 build -Clean`: 6436 checks, 0 failed. Each log has "build 7137e46". `spawn_run.ps1`, test save temple, Vulkan, `card_send_test_save=0`. fly1d, fly1e and stop1 are RUN DONE. No log has a line "head blinder : the overlay".

## A. fly1d (fly_solid_slide=1): PASS

- "god view     : the fly slides past the Tree 16332EE0 at 2127.7 2.7 2456.1: the whole step puts your head 26.7 cm out of it, and your head stays 30.2 cm out of it (the shell is 30 cm, fly_solid_slide=1). Your head is at 2118.0 41.0 2464.2, and the middle of the solid is 25.9 degrees to the right of straight ahead in the room, frame 304".
- "god view     : the fly slides past the Rock 1628F248 at 2197.0 2.3 2470.2: the whole step puts your head 28.9 cm out of it, and your head stays 32.1 cm out of it (the shell is 30 cm, fly_solid_slide=1). Your head is at 2167.0 16.0 2458.1, and the middle of the solid is 35.8 degrees to the left of straight ahead in the room, frame 322".
- The first fly has 240 trace lines, no step of 0.000 cm, and the least clearance is 10.00 cm. After the tree the waist point goes on: 2117.963 -19.040 2464.239, then 2119.652 -18.572 2464.375, 2121.585 ..., 2128.569 ..., with steps of 1.76 to 2.78 cm that grow again to 4 cm. At the rock the path turns to the side (z 2458.1 to 2451.6), then goes on.
- Note: three trace lines after the rock have the waist point at y -50.000 with "0.000 cm up" (for example "the waist point at 2169.253 -50.000 2449.397"), then the y goes up again. These runs do not show whether -50.000 is a limit.
- End of the first fly: "which gives 165.2 cm of the room each second, and the view moved on 240 of 240 frames ... The floor of the fly lifted the view 591.8 cm on 160 frames, and the least clearance of your head after a step was 10.0 cm. The fly met 2 solids: 17 steps slid past one, and 0 steps stopped at one. The speed ramp went up to 2.83 times", frame 411. Job 76 had 66.8 cm each second.
- Second fly: "The fly went 1994.1 cm up ... The fly met 0 solids ... The speed ramp went up to 7.15 times", frame 871. The camera zone stops the place 50 units past the zone, as before.
- The walk check is 0.000 units apart after the stops.

## B. fly1e (fly_solid_slide=0): PASS

- "god view     : the fly stops at the Tree 16327EE0 at 2127.7 2.7 2456.1: the whole step puts your head 26.7 cm out of it, and your head stays 30.0 cm out of it (the shell is 30 cm, fly_solid_slide=0). Your head is at 2118.0 40.9 2464.2, and the middle of the solid is 25.9 degrees to the right of straight ahead in the room, frame 304".
- End of the first fly: "which gives 66.8 cm of the room each second, and the view moved on 135 of 240 frames ... The fly met 1 solids: 0 steps slid past one, and 107 steps stopped at one", frame 411. The place stays at 2118.0 2464.2 to the release, as in job 76.
- The walk check: "the waist point is at 2118.02 2464.23, and it was at 2118.02 2464.23 at the stop: 0.000 units apart".

## C. stop1 (a level fly at the tree): the fly goes over the tree

- The aim from the log of fly1d: the head starts at 1858.8 2528.7, the tree is at 2127.7 2456.1, and the heading is 103.97 degrees. So dx = 268.9, dz = -72.6, right = 5.5, fwd = 278.5, and D = 1.1 degrees to the right.
- The sign of the key: in job 74 (fly2b), `fake_controller_sweep_yaw=8` gave "8.0 degrees to the right". So a positive value turns to the right, and the run used `fake_controller_sweep_yaw=1.1`.
- Start: "Forward is the direction in which the controller points (move_stick=2), and it is 1.0 degrees to the right of straight ahead in the room and 0.0 degrees below level ... (move_stick_ramp=0), frame 171". The line rounds the angle to whole degrees.
- At frame 281 the waist point is at 2126.937 29.480 2457.015, 0.9 units from the middle of the tree. There the head is "86.80 cm below your head ... a clearance of 86.80 cm", with "0.000 cm up". The land falls from 37.9 (frame 266) to 2.7 (frame 281) before the tree. So the level fly from the start height goes over the tree.
- End: "You went 1000.00 units over the land ... 150.0 cm of the room each second ... The floor of the fly lifted the view 29.3 cm on 11 frames, and the least clearance of your head after a step was 10.0 cm. The fly met 0 solids: 0 steps slid past one, and 0 steps stopped at one", frame 571.
- No "stops at" line. The aim was on the tree to 0.9 units, and only the height is wrong. A second aim of the yaw cannot change that, so I did not run one. A level stop needs a lower start, for example a fly down to the land near the tree first, or a different tree on level land.
