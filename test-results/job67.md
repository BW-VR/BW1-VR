# Job 67: the zoom stops out of a house (agent/zoom 1f5c08e)

`.\dev.ps1 build -Clean`: 6127 checks, 0 failed. Each log has "build 1f5c08e". `village_run.ps1 -Kind small`, dxwrapper, 60 frames a second, 100 s. All three runs are RUN DONE.

## zoomA7 (the defaults)

- "hand land : solid_house_inset_pct=20. Below the life size scale the box of a house is that much smaller across, so it is nearer to its walls".
- In: 66.7, 44.4, 29.6, 19.8, 13.2, 8.8, 5.9, 3.9, 2.6, 1.7, then "the zoom step stops short at 1.55 units per meter, not 1.16: the whole step puts your head 26.1 cm into the Abode 13AF45F0, so your head stays 5 cm out of it (god_zoom_solid_stop=1)". Then "the zoom step is refused at 1.55 units per meter: it puts your head 40.7 cm into the Abode 13AF45F0" two times. The final line has 5 steps and 2 refused.
- The zoom check at 1.55: the eye is 0.15 units over the land, the near plane 0.077 units (5.0 cm), the blinder covers 0%, "your head is 5.0 cm from the Abode 13AF45F0". The blinder covers 0% at each step in.
- "head blinder : the overlay starts, your head is 54.3 cm in the Abode 13AF45F0 at 1854.2 2665.8, frame 1083", opaque at frame 1089, gone after 1.36 s at frame 1158. That is the lean of the head.
- Out: 2.3, 3.5, 5.2, 7.8, 11.8.

The pictures (left eye):
- 1030 (44.4): normal.
- 1400, 1460 (1.55): the wall of the Abode fills the view, 5 cm from the eyes. It is a blurred yellow thatch on the upper half and a dark lower half. No land, villager or Hand shows.
- 1660, 1730, 1790 (after the grab): the thatch close in front, with a slope of the roof. No Hand and no villager shows.
- 1840, 1860 (the lean): the blinder covers the view (gold middle, red edges).
- 2060 (11.8): normal land and trees.

So the head stays out of the house, but at the stop the eyes are against its wall, and the view is only the wall.

## zoomA8 (god_zoom_about=0)

- In to 1.2, then 1.0: "the zoom is at its lowest scale, 1.00 units per meter (god_near_units_per_meter_x10=10). A villager of 1.6 units is 1.60 m high in the room, and the land under your head is 0.71 m below it", then "the zoom is at its limit, 1.0 units per meter". No "stops short" line and no "is refused" line during the zoom (the final line has 6 steps and 1 refused, the step at the limit).
- The blinder covers 0% at each step. The head is 71.4 cm from the land at each step in (151.4 cm after the grab).
- The pictures: at 1.0 (1400, 1460) the side of a house roof (yellow thatch) fills the left half, seen from outside, with its corner and edges whole. To the right are the land, a fence, a fire and trees. The Hand is whole. At 1660 to 1790 the roof is to the left, the Hand is near and whole, and the fire is to the right. The head is outside the walls: the outer surface of the roof shows, and no blinder starts. No villager is clearly in view. At 2060 the view is normal.

## zoomA9 (god_zoom_solid_stop=0, solid_house_inset_pct=0)

- "solid_house_inset_pct=0. A house is the whole box of its mesh at each scale".
- "head blinder : the overlay starts, your head is 3.7 cm in the Abode 13AF85F0 at 1852.1 2665.5, frame 512" (at the step to 2.6, cover 17%), opaque at frame 517. Cover 100% at 1.73, 1.16 and 1.00, and on the way out at 1.50 and 2.25. 93% at 3.38. Gone at frame 1247.
- The pictures from 1400 to 1860 show only the blinder. 2060 is normal.
- So zoomA9 repeats zoomA3, but the overlay starts one step earlier (at 2.6, as in zoomA2 and zoomA4), not after 1.7.

The pictures stay on the laptop: C:\wt-zoom\bwvr\tools\desk\desk-zoomA7, zoomA8, zoomA9.

## zoomA10 (job 69, agent/zoom 2347b4b)

`.\dev.ps1 build -Clean`: 6127 checks, 0 failed. RUN DONE.

- In to 2.6, then "the zoom step stops short at 2.17 units per meter, not 1.73: the whole step puts your head 13.2 cm out of the Abode 13AF25F0, so your head stays 30 cm out of it (god_zoom_solid_stop=1)", and three steps "is refused at 2.17 units per meter: it puts your head 3.3 cm into the Abode".
- The zoom check at 2.17: the eye 0.29 units over the land, the near plane 0.109 units (5.0 cm), cover 0%, "your head is 12.5 cm from the land".
- The lean: the overlay starts in the Abode at frame 1083 and is gone at frame 1157.
- The zoom out: "the zoom step stops short at 2.17 units per meter, not 3.26: the whole step puts your head 23.4 cm out of the Abode 13AF25F0, so your head stays 30 cm out of it", then four steps out "is refused at 2.17 units per meter". The scale stays at 2.17 to the end of the run.
- The pictures: 1400 and 1460, the wall and door of the Abode about 30 cm in front, the Hand whole. 1660, the roof slope with land and trees beside it. 1730 and 1790, the Hand whole at 15 and 8 cm. 1840 and 1860, the blinder. 2060, the same view as 1790, because the zoom out was refused.

## game-sp1b and game-spvkb (job 63 again, more shots)

The same save pictures, save card and guide (frames 3008 to 4217). The shots after the guide (4273, 4573) show the empty slot frames with the green pattern, not the land picture. No third save picture. spvkb: "4 copies from RGB into DXT1 blocks, and 0 copies of the blocks of a rect".

## zoomA11 (job 71, agent/zoom 70e043d)

`.\dev.ps1 build -Clean`: 6130 checks, 0 failed. RUN DONE. The zoom stops at 2.25 units per meter, 30 cm out of the Abode 13AF15F0, and three steps in are refused. The blinder shows in the lean (frames 1083 to 1158). Out: 3.4, 5.1, 7.6, 11.4, 17.1, with no "refused" or "stops short" line. Picture 2060: a normal view from higher up.
