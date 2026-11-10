# Job 75: the cull distance (agent/cull 8cbb6f2)

`.\dev.ps1 build -Clean`: 6355 checks, 0 failed. Each log has "build 8cbb6f2" and "vr           : game renders at 2760x2208", and "hooks        : the fog set of the sky at 0x007FEAA0 ok". Each run has `card_send_test_save=0`. cullbird, cullpic and the four runs of cullb are done.

The player's bwvr.ini in the game folder of the laptop had the line `card_send_test_save=8`, a leftover of the crisp test. I removed that one line after job 75 and before job 76, when no game ran. No other line changed.

## The view at 1 unit per meter (cullbird and cullpic)

- "head         : the invented head leans 0 cm right, 10000 cm up and 0 cm forward", "invented head, yaw 0.0 pitch -20.0".
- "god view     : ... Now it is at 1906.9 29.5 2516.8, heading 104.0 degrees, 1.0 units per meter. The land at 1907 29 2517 is 50 cm in front of your head and 100.60 m below it, and the land under your head is 100.60 m below it (god_spawn=1)".
- "near plane   : lowered on 450 of 451 calls. Last: the game gave 0.78 units, 0.100 used (10.0 cm at 1.0 units per meter)".
- No line of the log has "zoom check". The lines above are the nearest ones.

## The cull lines beside each scene (cullbird; cullpic has the same values)

The test steps the distance at log frames 770, 1470, 2170, 2870 and 3570 (step frames 1500, 2200, 2900, 3600 and 4300). So the six scenes are Game, 2x, 3x, 5x, 10x, Game, as planned.

| scene | cull line | god view line |
|---|---|---|
| 1 Game | "cull_distance=0, map objects stop at 700 units. ... the sky asked for the fog from 203 to 865 units, the game got 203 to 865. 0 of 454 fog sets scaled" | "at most 957 of 2999 map objects and 114 of 2048 sorted objects" |
| 2 2x | "cull_distance=1, map objects stop at 1400 units. ... the sky asked for the fog from 100 to 800 units, the game got 200 to 1600. 450 of 450 fog sets scaled" | "at most 1039 of 2999 ... and 112 of 2048" |
| 3 3x | "cull_distance=2, map objects stop at 2100 units. ... the game got 300 to 2400. 450 of 450" | "at most 1036 of 2999 ... and 68 of 2048" |
| 4 5x | "cull_distance=3, map objects stop at 3500 units. ... the game got 500 to 4000. 450 of 450" | "at most 1040 and 1042 of 2999 ... 69 and 75 of 2048" |
| 5 10x | "cull_distance=4, map objects stop at 7000 units. ... the sky asked for the fog from 115 to 816 units, the game got 1151 to 8159. 450 of 450" | "at most 1041 of 2999 ... and 78 of 2048" |
| 6 Game | "cull_distance=0, map objects stop at 700 units. ... the game got 176 to 855. 419 of 450 fog sets scaled", then "270 to 882. 0 of 450" | "at most 1036 of 2999 ... 79", then "958 of 2999 ... 75" |

- The window where the distance changes (the first window of 2x, of the second Game) has part of its fog sets scaled (131 of 450 at 2x, 419 of 450 at the second Game).
- Each "god view" line ends "Full in 0 and 0 passes". No log has "a list is full".
- From 2x on, the map objects grow only from about 957 to about 1040, the same at 10x.

## Run 1, cullbird (Vulkan, not paced)

`bench_run.ps1 -Read cullbird` (its scene names; scenes 1 to 6 are Game, 2x, 3x, 5x, 10x, Game):

| scene | work mean | median | p99 | eye passes | game turn | draw calls |
|---|---|---|---|---|---|---|
| 1 Game | 2.89 | 2.77 | 5.12 | 2.55 | 0.09 | 946 |
| 2 2x | 2.96 | 2.85 | 3.83 | 2.63 | 0.08 | 1013 |
| 3 3x | 2.89 | 2.81 | 3.72 | 2.57 | 0.07 | 1008 |
| 4 5x | 2.91 | 2.78 | 3.62 | 2.54 | 0.12 | 1017 |
| 5 10x | 2.74 | 2.62 | 3.56 | 2.41 | 0.08 | 1039 |
| 6 Game | 2.85 | 2.76 | 3.59 | 2.53 | 0.07 | 974 |

The frame mean is 15.77 ms in scene 1 and 16.67 ms in scenes 2 to 6 (60 frames a second). Scene 4 has one slow frame with a game turn of 29.72 ms. Scene 1 has 5 slow frames (game turn and eye 0). So the cost of 2x to 10x is in the noise at this height, and the draw calls grow by at most 93 (10 percent).

## Run 2, cullpic (dxwrapper): the left eye pictures

The shots are at log frames 723, 1423, 2123, 2823, 3523 and 4223, inside the Game, 2x, 3x, 5x, 10x and Game windows. The pictures stay on the laptop (game art): C:\wt-cull\bwvr\tools\desk\desk-cullpic, and the sheets in C:\BW1-VR\local\shots\job75.

- In each picture the head is high over the land: the view looks down on the island, a range of peaks in the middle distance, valleys and a coast to the right, and the sea to the horizon. This agrees with 100.6 m over the land at 1 unit per meter.
- The time of day of the game changes during the run. 723 to 2823 have a red sunset sky, 3523 a grey pink sky, and 4223 a blue day sky with green sea. So the colour of the fog and of the land changes from picture to picture for that reason as well.
- Trees: small trees show on the near slopes, in the valley in the middle, and along the coast at the right in all six pictures. I see no clear difference in the number of far trees between Game and 10x at this size. No house and no villager is in this view.
- Fog: at 723 (Game) the far land at the left of the peaks and the far sea are dim and grey. From 1423 (2x) on the far land is clearer, with more colour. At 3523 (10x) and 4223 (Game, day) a far green headland shows to the right past the peaks.
- No picture shows a hard line where objects stop, no hole in the land, and no fault of the sky. One still picture cannot show a flicker of depth.

## Run 3, cullb: Game (A) against 5x (B) at the normal scales

The cull lines of the normal scales: in A and in B "map objects stop at 8000 units" (scenes 1 to 3 and 6) or "4148 units" (scenes 4 and 5). In A the fog is the game's ("the game got 100 to 800"), in B it is 5 times further ("the game got 500 to 4000"). So at the normal scales the game's own cull is already past the cull of 5x, and only the fog changes. The map objects in A and B are the same in each window (1034, 1375, 1372, 1366, 1545). No "a list is full" line.

The comparison table of bench_run (means of 2 runs):

| scene | work mean A / B | median A / B | p99 A / B | eye passes A / B | game turn A / B | draw calls A / B | result |
|---|---|---|---|---|---|---|---|
| 1 start, level head | 2.62 / 2.55 | 2.52 / 2.48 | 4.61 / 4.11 | 2.28 / 2.23 | 0.08 / 0.07 | 1001 / 1001.5 | in the spread (p99 near the spread) |
| 2 start, head down | 2.67 / 2.64 | 2.56 / 2.54 | 3.54 / 3.76 | 2.35 / 2.32 | 0.07 / 0.07 | 1239.5 / 1240 | in the spread |
| 3 village | 3.09 / 3.03 | 3.01 / 2.98 | 4.08 / 3.84 | 2.78 / 2.74 | 0.07 / 0.07 | 1400.5 / 1401 | p99 and worst "clear", lower in B |
| 4 tree in the Hand | 2.98 / 2.96 | 2.89 / 2.87 | 3.85 / 3.72 | 2.70 / 2.68 | 0.08 / 0.08 | 1398 / 1398 | worst "clear", lower in B |
| 5 337 units a meter | 2.88 / 2.86 | 2.77 / 2.77 | 3.63 / 3.64 | 2.56 / 2.54 | 0.07 / 0.06 | 1380.5 / 1380.5 | in the spread |
| 6 widest zoom | 0.90 / 0.90 | 0.82 / 0.82 | 1.40 / 1.46 | 0.59 / 0.59 | 0.06 / 0.06 | 53 / 53 | in the spread |

So 5x costs nothing at the normal scales. The "clear" results of scene 3 and 4 are lower in B by 0.24 to 0.43 ms, with the same draw calls, so they are not a cost of 5x.

After each cullb run: "ini restored: True", "installed dll put back: True", "profile put back", "the real cursor: at the same place before and after the run".
