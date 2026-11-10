# Job 80: the hologram on the wall of the edge of the influence (agent/influence 7844904)

`.\dev.ps1 build -Clean`: 6470 checks, 0 failed. Each log has "build 7844904" and "vr           : game renders at 2760x2208". Each run is RUN DONE, and each run has its 10 shots in both eyes (frames 233 to 1313).

## The fault of the recipe edgeside (runs 1 and 2)

The recipe `-Kind edgeside` of scene_run.ps1 gives the side points with decimals (for example "-5.13,-30,34.1"). ParsePoints in controllers.cpp reads each value with strtol, so it stops at the first decimal point. The ini of ss1 and ss0 has "fake_controller_points=0,0,100 0,-30,20 -5.13,-30,34.1 -3.88,-30,34.49 ...", and both logs have:

- "controllers  : a fake point is not right,up,forward in cm, the rest is skipped"
- "controllers  : fake hands, offset 20 -30 35 cm, yaw 8 pitch 1 roll 0 degrees, 22 script steps, 2 points, 1 left points"

So each "point" step goes to the next of 2 points only: "the fake hand steps to point 0 (0 0 100 cm) at frame 290", "the fake hand steps to point 1 (0 -30 20 cm) at frame 410", and so on to frame 1250. The right hand goes between the middle of the test circle and 1 m in front of the head. It never stays 10 cm inside the edge and never steps along it. Runs 1 and 2 cannot show the side steps. The recipe needs whole cm values, or the parser needs decimals. Whole cm at a radius of 15 cm do not give equal steps of 5 degrees (1.3 cm a step).

## 1. ss1 (influence_wall_look=1, influence_wall_test_light_pct=300, edgeside): FAIL (the recipe)

Lines that show:
- "influence    : influence_wall=1, influence_wall_look=1, influence_wall_ripple=1. A red hologram shows the wall of the edge of the influence at each hand, from 45 cm of the room before the edge"
- "influence    : test key influence_test_circle_cm=25: from frame 230 the influence of the player is a circle of 25.0 units around 1880.3 2523.6 (the land point of the Hand's point), for the fade, the glow and the stream"
- "influence    : the hologram at the edge shows for the right hand at 1880.3 2523.6, inside the influence, 25.0 cm of the room from the edge, strength 0.42, on the wall of a circle of the game, 514 triangles, frame 230"
- "influence    : the hologram: 322 frames on the wall of a circle of the game, 0 frames by the search of the rays, 0 tests where the circles and the test of the game did not agree" (later reports: 660 and 680 frames, each with 0 frames by the search and 0 tests that did not agree).

Lines that do not show: no "the hologram of the right hand: 10.0 cm of the room inside the edge" line is in the log. Each line of the right hand is at the middle of the circle, so the angle stays at 90 degrees (the middle has no nearest edge point):
- "influence    : the hologram of the right hand: 25.0 cm of the room inside the edge, strength 0.42, radius 20.7 cm, edge point 1880.27 50.9 2548.61 on the circle at 1880.3 2523.6 of radius 25.0, at 90.00 degrees on it, 514 triangles, frame 230"
- At point 0 the hand is out: "influence    : the right hand goes out of the influence at 1955.9 2504.6, so it fades to 45 percent, frame 291" and "the hologram at the edge goes for the right hand at 1957.9 2504.3, outside the influence, 0.0 cm of the room from the edge, strength 0.00, no edge near, 0 triangles, frame 291". It comes back at frames 411, 651, 891 and 1131.

The X degrees for each step (the two lines of each step, at the middle of the circle or outside):

| shot frame | right hand at | X degrees (line 1, line 2) |
|---|---|---|
| 233 | point 1, the middle | 90.00, 90.00 |
| 353 | point 0, outside | no line (hologram gone) |
| 473 | point 1 | 90.00, 90.00 |
| 593 | point 0 | no line |
| 713 | point 1 | 90.00, 90.00 |
| 833 | point 0 | no line |
| 953 | point 1 | 90.00, 90.00 |
| 1073 | point 0 | no line |
| 1193 | point 1 | 90.00, 90.00 |
| 1313 | point 0 | no line |

The left hand stays at its fixed point near the edge all the run: "influence    : the hologram of the left hand: 0.3 cm of the room inside the edge, strength 1.00, radius 30.0 cm, edge point 1899.82 50.9 2539.19 on the circle at 1880.3 2523.6 of radius 25.0, at 38.57 degrees on it, 704 triangles, frame 230". Its angle stays at 38.57 degrees to frame 2210, with no change.

Pictures (crops in C:\BW1-VR\local\shots\job80\ss1-crop.png): the right hand is not in the view in any shot. At the left controller there is a red hologram in each shot: upright red grid lines and red rings on a curved upright surface, the rings tilt with the curve of the wall. Its radius is about two lengths of the controller (the log gives 30 cm). It is the same in all 10 shots, because the left hand does not move. It shows clearly against the green and grey land and against the Temple. Behind the left controller there is also a soft orange upright band. The same band is at the left hand in ss0 (look 0), so it is the soft fill or a different glow, not the grid. In both eyes the hologram is at the same place on the land, with a small shift between the eyes.

## 2. ss0 (influence_wall_look=0, edgeside): FAIL (the recipe)

- "influence    : influence_wall=1, influence_wall_look=0, influence_wall_ripple=1. A soft red patch shows the edge of the influence at each hand, from 20 cm of the room before the edge"
- The log has 0 lines "the glow of the right hand". The right hand is 25 cm inside the edge at point 1 (more than the 20 cm of the patch) and far outside at point 0, so its patch never shows. The fade lines are the same as in ss1 (out at 291, 531, 771, 1011, 1251, in at 411, 651, 891, 1131).
- The left hand: "influence    : the glow at the edge shows for the left hand at 1899.6 2539.0, inside the influence, 0.4 cm of the room from the edge, strength 1.00, 15 of 24 rays found the edge, frame 230" and "influence    : the glow of the left hand: 0.4 cm of the room inside the edge, strength 1.00, at 1899.7 50.9 2539.3, frame 230". Its point does not change, because the left hand does not move.
- So the job item "the point changes only each 2 to 3 steps" cannot show.

Pictures (C:\BW1-VR\local\shots\job80\ss0-crop.png): at the left controller there is a soft orange-red patch with no lines and no rings. It looks long and upright, not round, in this view (the view is at a low angle to it). There is no hologram grid. The right hand is not in the view.

## 3. se1 (influence_wall_look=1, edge): the hologram PASS, the strength numbers FAIL, "gone at 40 cm past" NOT SHOWN

The steps go well: "fake hands, ... 22 script steps, 11 points", and the hand steps to points 1 to 10 at frames 170 to 1250.

- The first step: "influence    : the hologram at the edge shows for the right hand at 1880.3 2523.6, inside the influence, 25.0 cm of the room from the edge, strength 0.42, on the wall of a circle of the game, 514 triangles, frame 230".
- The fade lines go out and come back: "influence    : the right hand goes out of the influence at 1907.3 2516.7, so it fades to 45 percent, frame 891" and "influence    : the right hand comes back into the influence at 1897.6 2519.1, so it shows in full again, frame 1251".
- Still there past the edge: "influence    : the hologram of the right hand: 15.0 cm of the room past the edge, strength 0.68, radius 24.9 cm, edge point 1904.53 50.9 2517.57 on the circle at 1880.3 2523.6 of radius 25.0, at -13.97 degrees on it, 660 triangles, frame 1250".
- "influence    : the hologram: 900 frames on the wall of a circle of the game, 0 frames by the search of the rays, 0 tests where the circles and the test of the game did not agree".

The strength in the 60 frame lines:

| step (hand) | frames | strength in the log | strength of the job |
|---|---|---|---|
| 25 cm inside | 230, 290 | 0.42 | about 0.26 |
| 20 cm inside | 350, 410 | 0.58 | |
| 15 cm inside | 470, 530 | 0.74 | |
| 10 cm inside | 590, 650 | 0.87 | |
| 5 cm inside | 710, 770 | 0.97 | |
| at the edge (0.0 cm) | 830, 890 | 1.00 | about 1.0 |
| 5 cm past | 950, 1010 | 0.96 | about 0.84 |
| 10 cm past | 1070, 1130 | 0.84 | |
| 15 cm past | 1190, 1250 | 0.68 | about 0.57 |
| 5 cm inside again | 1310 to 2210 | 0.97 | |

The strength goes up at each step to the edge and down past it, in a smooth curve. But the numbers are not the numbers of the job. The log numbers agree with HologramStrength in influence_edge_math.h: t * t * (3 - 2 * t) with t = 1 - distance / range, and the ranges 45 cm inside and 40 cm past (kHoloInsideM, kHoloOutsideM). For example 25 cm inside gives t = 0.444 and 0.42, and 5 cm past gives t = 0.875 and 0.96. The job numbers fit a different curve. The lead must say which is correct.

"Gone at 40 cm past" cannot show: the farthest point of the recipe (0,-30,60) is 40 cm from the middle, so 15 cm past the edge. The log has no "the hologram at the edge goes for the right hand" line in se1.

The edge point stays at -13.97 degrees for all steps after the first. This agrees with a hand that moves on one line out from the middle.

Pictures (C:\BW1-VR\local\shots\job80\se1-crop.png and se1-eyes-833.png):
- At 233 (25 cm inside) the right hologram is not clear in the view. Only the hologram of the left hand shows.
- From 353 on, a second red hologram is at the right controller: upright red grid lines and rings that go out from a bright pink middle. It grows and gets brighter at each step to 833 (the edge). At 833 it is larger than the controller, about 3 controller lengths across, and the middle is a bright pink spot behind the controller.
- At 953 to 1193 (past the edge) it is still there and slightly less bright. At 1313 (5 cm inside again) it is bright again.
- At 833 the left and right eyes show the hologram at the same place on the Temple and on the land, with a small shift between the eyes. Nothing in it is doubled or cut.
- The grid and the rings show clearly against the land and the Temple at light 300 percent. Where the two holograms overlap (the left and the right hand) the view has many red lines, and the land under them is hard to see.

## What only a headset can show

The size of the hologram against a real hand, its depth on the wall, and how it looks at the normal light (100 percent) are for a headset.

The game-art pictures stay on the laptop: C:\wt-influence\bwvr\tools\desk\desk-ss1, desk-ss0, desk-se1, and the crops in C:\BW1-VR\local\shots\job80.
