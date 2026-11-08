# Job 65: the head blinder at the small zoom (agent/zoom 0deda4d)

`.\dev.ps1 build -Clean`: 6088 checks, 0 failed. `village_run.ps1 -Kind small`, dxwrapper, 60 frames a second. zoomA4 and zoomA5 are RUN DONE. zoomA6 was not run (the peer asked to skip it when step 0 named the solid).

## Step 0: the head blinder lines of zoomA2 and zoomA3

- zoomA2: "the overlay starts, your head is 7.2 cm in the Abode 13AF75F0 at 1852.2 2665.5, frame 512", "the view is opaque ..., frame 518", and "the overlay is gone after 11.34 s. The least clearance of your head was -139.5 cm, frame 1176".
- zoomA3: "the overlay starts, your head is 38.8 cm in the Abode 13AF65F0 at 1852.9 2666.0, frame 542", "the view is opaque ..., frame 548". After that each window has "450 frames opaque".

## zoomA4 (head_blinder=1)

"zoom check, the scene at X units per meter" after each step:

| Scale | Eye over the land | Near plane | Fog of the game | Blinder cover | Clearance |
|---|---|---|---|---|---|
| 13.17 | 5.88 units | 0.658 (5.0 cm) | 208 to 867 | 0% | 43.6 cm from the land |
| 8.78 | 3.52 | 0.439 | 202 to 865 | 0% | 39.3 cm |
| 5.85 | 2.19 | 0.293 | 196 to 863 | 0% | 36.5 cm |
| 3.90 | 1.30 | 0.195 | 191 to 861 | 0% | 32.5 cm |
| 2.60 | 0.69 | 0.130 | 185 to 858 | 17% | -8.6 cm from the Abode 13AF35F0 |
| 1.73 | 0.34 | 0.087 | 180 to 856 | 100% | -42.0 cm from the Abode |
| 1.16 | 0.15 | 0.058 | 175 to 854 | 100% | -53.2 cm |
| 1.00 | 0.13 | 0.050 | 170 to 852 | 100% | -59.4 cm |

"head blinder : the overlay starts, your head is 8.6 cm in the Abode 13AF35F0 at 1852.2 2665.5, frame 512", and "the view is opaque", frame 518. On the steps out the cover is 100% at 1.50 and 2.25, 93% at 3.38, and 0% from 5.06. The overlay is gone at frame 1176. The near plane is 5.0 cm of the room at each step.

This run took 1259 s from the start of the step to its end, not about 100 s. The run log does not show why.

## zoomA5 (head_blinder=0)

The zoom check lines have the same eye heights, near planes and fog, and "the head blinder covers 0%" at each step.

The pictures (left eye):
- 1030 (44.4): the Hand at the Village Store, normal.
- 1400 and 1460 (1.0): the world draws, but the eye is in the house. The yellow thatch of the roof fills the top half of the view, from inside. The lower half is the dark inside of the house, and the land outside shows at the lower left. The Hand is whole, in a pinch, near the middle.
- 1660, 1730, 1790 (1.0, after the grab): the roof from inside and a dark floor. The Hand does not show.
- 1990 (7.6): the land and the trees outside, normal, with a large see-through Hand near the eyes.

So the blinder makes the blank view, because the zoom puts the head into the Abode. With the blinder off, the view at 1.0 is inside the Abode.

The pictures stay on the laptop: C:\wt-zoom\bwvr\tools\desk\desk-zoomA4 and desk-zoomA5.
