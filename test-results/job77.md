# Job 77: the fog moves only as far as the objects (agent/cull 3695b2a)

`.\dev.ps1 build -Clean`: 6364 checks, 0 failed. Each new log has "build 3695b2a". cullc (bench_run, 1 run) and cullbird2 are done. `card_send_test_save` is not in the player's bwvr.ini of the laptop now.

## a. The job 75 logs (build 8cbb6f2): PASS

Each of desk-cullb-a1, -a2, -b1 and -b2 has 15 lines "god view     : map objects stop at". Each line ends with "the fog is off", from the first window on.
- a1 and a2: "map objects stop at 8000 units (the game set 700), the fog is off" in windows 1 to 3 and 7 to 15, and "map objects stop at 4148 units (the game set 700), the fog is off" in windows 4 to 6.
- b1 and b2: "map objects stop at 8000 units (the game set 700), the fog is off" in all 15 windows.

## b. cullc (cull_distance=3, the normal scales): PASS

The scales of the scenes, from the log: scenes 1 and 2 at 100.0 units per meter, scenes 3 and 4 at 44.4, scene 5 at 337.5, and scene 6 zooms out to 4000. Each "god view" line is "map objects stop at 8000 units (the game set 700), the fog is off".

| scene | cull lines |
|---|---|
| 1 (100) | "the sky asked for the fog from 204 to 865 units, the game got 204 to 865. 1 of 454 fog sets scaled" |
| 2 (100) | "from 133 to 831 units, the game got 133 to 831. 0 of 450", "from 100 to 800 units, the game got 100 to 800. 0 of 450" |
| 3 (44.4) | "from 100 to 800 units, the game got 193 to 1543. 409 of 450", then "193 to 1543. 450 of 450" |
| 4 (44.4, then the zoom out) | "from 100 to 800 units, the game got 193 to 1543. 450 of 450", then "from 113 to 814 units, the game got 113 to 814. 301 of 450" |
| 5 (337.5) | "from 171 to 852 units, the game got 171 to 852. 0 of 450", "from 263 to 881 units, the game got 263 to 881. 0 of 450" |
| 6 (to 4000) | "from 370 to 897, the game got 370 to 897. 0 of 450", then four lines "from 400 to 900 units, the game got 400 to 900. 0 of 450" |

So at 100, 337.5 and 4000 units per meter the game got the fog that the sky asked for. At 44.4 units per meter it got 1.93 times (100 to 800 becomes 193 to 1543), with "the fog is off".

## c. cullbird2 (1 unit per meter, Game to 10x): PASS

The steps are at frames 770, 1470, 2170, 2870 and 3570, so scenes 1 to 6 are Game, 2x, 3x, 5x, 10x and Game.

| scene | cull line | god view line |
|---|---|---|
| 1 Game | "map objects stop at 700 units ... from 204 to 865 units, the game got 204 to 865. 0 of 454" | none |
| 2 2x | "stop at 1400 units ... from 133 to 831 units, the game got 266 to 1661. 131 of 450", then "from 100 to 800 units, the game got 200 to 1600. 450 of 450" | "map objects stop at 1400 units (the game set 700), the fog is the game's" (2 lines) |
| 3 3x | "stop at 2100 units ... from 100 to 800 units, the game got 300 to 2400" | "map objects stop at 2100 units (the game set 700), the fog is the game's" |
| 4 5x | "stop at 3500 units ... the game got 500 to 4000" (2 lines) | "map objects stop at 3500 units ..., the fog is the game's" (2 lines) |
| 5 10x | "stop at 7000 units ... from 113 to 814 units, the game got 1127 to 8136" | "map objects stop at 7000 units ..., the fog is the game's" |
| 6 Game | "stop at 700 units ... from 171 to 852 units, the game got 171 to 852. 419 of 450", then "263 to 881, the game got 263 to 881. 0 of 450" | none |

So the fog is 1, 2, 3, 5, 10 and 1 times what the sky asked for, and the objects stop at 700, 1400, 2100, 3500, 7000 and 700 units. When the cull distance is 0, no "god view     : map objects stop at" line is written. After scene 6 the test goes on stepping (from frame 4270), with the same factors.
