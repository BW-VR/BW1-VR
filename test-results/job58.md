# Job 58: the zoom to 1 unit per meter (agent/zoom 9aac1cb)

`.\dev.ps1 build -Clean`: 6088 checks, 0 failed. Each log has "build 9aac1cb", render 2760x2208, backend dxwrapper\ddraw.dll.

The first two runs (zoomA, zoomB, the default length of 60 s) stopped at log frame 450, at 8.8 units per meter, before the steps at 1.0. With test_frame_rate=60 the kind small needs more time. The runs zoomA2 and zoomB2 are the same with `-Seconds 100`. They are RUN DONE.

## zoomA2 (the log)

- "god view     : on. ... it zooms from 1.0 to 4000 units per meter, 50% a step."
- seek plan gostore (frame 192), then seek plan villager (frame 272, the Villager 85.3 cm from the Hand's point).
- Zoomed in: 100, 66.7, 44.4, 29.6, 19.8, 13.2, 8.8, 5.9, 3.9, 2.6, 1.7, 1.2, 1.0.
- "the zoom is at its lowest scale, 1.00 units per meter (god_near_units_per_meter_x10=10). A villager of 1.6 units is 1.60 m high in the room, and the land under your head is 0.12 m below it", then "the zoom is at its limit, 1.0 units per meter".
- The grab: the other hand at frame 771, the ground went -80.00 cm, the land moved 81.1 cm. After it the land under the head is 0.92 m below it, not 1.4 m.
- "near plane   : lowered on 450 of 450 calls. Last: the game gave 3.50 units, 0.050 used (5.0 cm at 1.0 units per meter), Z buffer 24 bit".
- Zoomed out: 1.0, 1.5, 2.2, 3.4, 5.1, 7.6.
- No crash and no fault. The "address space:" lines are the usual lines of each window (1208 MB in use, the largest free block 2045 MB).

## zoomB2 (god_near_units_per_meter_x10=75)

- "it zooms from 7.5 to 4000 units per meter".
- In: 8.8 to 7.5, then "the zoom is at its lowest scale, 7.50 units per meter (god_near_units_per_meter_x10=75). A villager of 1.6 units is 0.21 m high in the room", then "the zoom is at its limit, 7.5 units per meter" six times. The final line has 1 step and 6 refused.
- The grab lowered the ground by 80 cm. The land under the head is 1.19 m below it.
- Out: 7.5, 11.2, 16.9, 25.3, 38.0, 57.0.

## The pictures (left eye)

zoomA2:
- 1030 (log 303), at 44.4: the Hand is at the Village Store, with the store and the fence near, and houses behind. Normal.
- 1400, 1460, 1660, 1730, 1790 (log 673 to 1063), at 1.0: no land, no villager, no building and no Hand. The whole view is a flat orange-brown color, lighter at the middle (the color of the fog). Only the two advisors, the subtitle card and a small white shape show. This is the same for all five pictures. So the villager at 1.6 m, the land near the feet, the Hand at 15 cm and at 8 cm cannot be seen.
- 1990 (log 1263), at 7.6: the land, trees and houses are back, and a large see-through Hand and the yellow controller are near the eyes.

zoomB2 (the stop at 7.5): each picture shows the land. At 1400 and 1460 a house is near and the Hand is whole. At 1660 the view is lower, with houses and a fence near, and no holes or stripes. At 1730 the Hand (15 cm) is whole. At 1790 the Hand (8 cm) is large and see-through, and it is not cut. At 1990 (57.0) the view is wide.

So the scale 7.5 draws the world, and the scale 1.0 draws none of it. The pictures do not show at which step between 7.5 and 1.0 the world goes. A run with a picture after each step (zoomA3) is queued after the jobs 59 and 60.

The pictures stay on the laptop: C:\wt-zoom\bwvr\tools\desk\desk-zoomA2 and desk-zoomB2.
