# Job 88: one hologram of two hands, and test points with decimals (agent/influence 1ec7913)

`.\dev.ps1 build -Clean`: 6571 checks, 0 failed. Each log has "build 1ec7913". `scene_run.ps1`, dxwrapper. ss1, ss0, ssw and se1 are RUN DONE. The pictures stay on the laptop (game art): the run folders in C:\wt-influence\bwvr\tools\desk, and the crops in C:\BW1-VR\local\shots\job88.

## 1. ss1 (-Kind edgeside): PASS

- No line "a fake point is not right,up,forward". "controllers  : fake hands, offset 20 -30 35 cm, yaw 8 pitch 1 roll 0 degrees, 22 script steps, 11 points, 1 left points". The 11 points are the 2 first points of the recipe and the 9 side points: "fake_controller_points=0,0,100 0,-30,20 -5.13,-30,34.1 -3.88,-30,34.49 -2.6,-30,34.77 -1.31,-30,34.94 0,-30,35 1.31,-30,34.94 2.6,-30,34.77 3.88,-30,34.49 5.13,-30,34.1".
- "controllers  : the fake hand steps to point 2 (-5.13 -30 34.1 cm) at frame 290", and "the fake hand steps to point 10 (5.13 -30 34.1 cm) at frame 1250".
- The side steps, each with two lines "the hologram of the right hand: 10.0 cm of the room inside the edge, strength 0.87, radius 28.0 cm, edge point ... on the circle at 1880.3 2523.6 of radius 25.0, at X degrees on it, ... one hologram with the other hand":

| frames | X degrees | change |
|---|---|---|
| 350, 410 | 6.02 | |
| 470, 530 | 1.02 | -5.00 |
| 590, 650 | -3.99 | -5.01 |
| 710, 770 | -8.96 | -4.97 |
| 830, 890 | -13.97 | -5.01 |
| 950, 1010 | -18.98 | -5.01 |
| 1070, 1130 | -23.96 | -4.98 |
| 1190, 1250 | -28.96 | -5.00 |
| 1310 to 2090 | -33.97 | -5.01 |

- X is the same in the two lines of each step. Before the side steps: "25.0 cm of the room inside the edge, strength 0.42, ... at 90.00 degrees on it", frames 230 and 290.
- Each report: "influence    : the hologram: 900 frames on the wall of a circle of the game, 0 frames by the search of the rays, 0 tests where the circles and the test of the game did not agree" (442 frames in the first window).
- Pictures (left eye, 353 to 1313): a red hologram at each hand. Rings lie round the point of each hand and move with the hand. Upright red lines of the grid go up from the land. While the right hand steps along the edge, the upright lines stay at about the same places in the view, and the rings follow the hand. The grid shows clearly against the land. A soft orange upright band is at the left hand in each picture (see run 3).

## 2. ss0 (influence_wall_look=0): the line PASSES, the point moves at each step

- "influence    : influence_wall=1, influence_wall_look=0, influence_wall_ripple=1. A soft red patch shows the edge of the influence at each hand, from 20 cm of the room before the edge".
- "influence    : the glow of the right hand: 10.1 cm of the room inside the edge, strength 0.50, at 1904.9 50.9 2527.8, frame 291", and the same point at frame 351.
- The point of each step (x and z, two lines each): 1904.9 2527.8, 1905.3 2523.9, 1905.3 2522.6, 1904.8 2518.7, 1904.5 2517.4, 1904.2 2516.1, 1903.7 2514.9, 1902.1 2511.3, 1901.4 2510.2.
- So the point changes at each step, not only each 2 to 3 steps. But the jumps are not equal: z changes by 3.9, 1.3, 3.9, 1.3, 1.3, 1.2, 3.6 and 1.1 units, against 5.00 degrees (about 2.2 units) at each step of the hologram.

## 3. ssw (influence_wall=0): the line PASSES, the orange band stays

- "influence    : influence_wall=0, influence_wall_look=1, influence_wall_ripple=1. No glow at the edge of the influence".
- Pictures: no red grid and no rings. The soft orange upright band at the left hand is still there, the same as in ss1 at frame 593. So the band is not the glow of the mod. It comes from the game or from another part of the mod (for example the stream of the influence: "influence_stream_hand=0. The stream of the influence ends on the land under the Hand, as the game draws it").

## 4. se1 (-Kind edge): the strength PASSES, the "goes" line FAILS

- "controllers  : fake hands, ... 32 script steps, 16 points, 1 left points".
- The strength for each step, from the lines "the hologram of the right hand":

| point | place | strength | radius | frames |
|---|---|---|---|---|
| 1 | 25 cm inside | 0.42 | 20.7 cm | 230, 290 |
| 2 | 20 cm inside | 0.58 | 23.3 cm | 350, 410 |
| 3 | 15 cm inside | 0.74 | 25.9 cm | 470, 530 |
| 4 | 10 cm inside | 0.87 | 28.0 cm | 590, 650 |
| 5 | 5 cm inside | 0.97 | 29.5 cm | 710, 770 |
| 6 | at the edge | 1.00 | 30.0 cm | 830, 890 |
| 7 | 5 cm past | 0.96 | 29.3 cm | 950, 1010 |
| 8 | 10 cm past | 0.84 | 27.5 cm | 1070, 1130 |
| 9 | 15 cm past | 0.68 | 24.9 cm | 1190, 1250 |
| 10 | 20 cm past | 0.50 | 22.0 cm | 1310, 1370 |
| 11 | 25 cm past | 0.32 | 19.1 cm | 1430, 1490 |
| 12 | 30 cm past | 0.16 | 16.5 cm | 1550, 1610 |
| 13 | 35 cm past | 0.04 | 14.7 cm | 1670, 1730 |
| 14 | 40 cm past | 0.00 | 14.0 cm | 1790, 1850 |
| 15 | 5 cm inside | 0.97 | 29.5 cm | 1910 to 2570 |

- At 40 cm past: "influence    : the hologram of the right hand: 40.0 cm of the room past the edge, strength 0.00, radius 14.0 cm, ... 42 triangles, one hologram with the other hand, frame 1790", and "... 34 triangles ..., frame 1850". The log has no line "the hologram at the edge goes for the right hand". So at strength 0.00 the hologram still has 34 to 42 triangles, and no "goes" line.
- Each line has ", one hologram with the other hand". Each report has "influence    : 900 frames of a hand whose hologram is one with the hologram of the other hand on the same wall" (442 in the first window). Each report also has "0 frames by the search of the rays".
- Pictures at 833 (at the edge), both eyes: rings round each hand, and one set of upright lines over both discs. Where the two discs overlap there is one grid with no doubled lines. The land, the hill and the Temple show under the grid. The two eyes agree.
