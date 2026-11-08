# Job 59: the job icon and the name cards (agent/jobicon e4eb2d6)

`.\dev.ps1 build -Clean`: 6097 checks, 0 failed. Each log has "build e4eb2d6". Runs jobA (100) and jobB (200) with the steps and keys of the job, `-Kind job -Seconds 75`, save temple. Both are RUN DONE.

## The first lines

- "vr : game renders at 2760x2208".
- The window in front: "in front is a window of a different program, class Chrome_WidgetWin_1" at the start, then "in front is the game window. The cursor is at 2624 0, frame 1".
- "fov : absolute 110.0 degrees", and "the game asked for 1.2217 rad (70.0 deg), using 1.9199".

## The keys

- A: "job icon : job_icon_scale_pct=100. The job icon has the size that the game gives". "names : villager_name_scale_pct=100. The card ... has the size that the game gives".
- B: "job icon : job_icon_scale_pct=200. The job icon over a villager in the Hand or under it has that percent of the size that the game gives, and its lower edge stays where the game puts it". "names : villager_name_scale_pct=200 ...".
- In both runs: "job icon : LH3DSprite::AddDrawing at 0x00840C70 does not have the bytes of the disassembly, so the mod does not change it".

## The job icon

There is no "job icon : the job icon N over a villager" line and no "draws of the job icon this window" line in either run. The hook did not go in (the line above).

The seek steps and the grip acted: seek plan gostore (frame 192), store (272), villager 13AE1054 (312 and 422), store (532), and "the grip picks it up" for the Villager 13AE1054 at frame 424, with the fake grip down at frame 430. To reach the villager, the fake hand steps to -142.5 -39.8 64.8 cm, so the held villager is out of the left eye pictures. The zoom did not act: "the stick asks the God view to zoom in" at frames 71 and 101, and no "zoomed" line follows. The view stays at 100 units per meter.

## The names

- The ring: opens at frame 331, "the choice is details", "the player chose details", "the game: the villager details are on, frame 362".
- A: four "names : a name card of 2 or 3 line(s) at 41.6 to 60.6 units deep: a line of its text is 16.0 px of the render high (16.0 px at the size of the game), 0.95 degrees in the middle of the view, at 16.9 px in a degree". Windows: 2399, 12667 and 12440 name cards, a line of 16.0 px, 0.95 degrees.
- B: the same cards with "32.0 px of the render high (16.0 px at the size of the game), 1.90 degrees ... at 16.9 px in a degree". Windows: 2220, 11763 and 11541 name cards, a line of 32.0 px, 1.90 degrees.

## Measured in the left eye pictures (log frame 403)

- The job icon: not seen in A or in B (the held villager is out of the pictures, and no hook).
- A capital letter of a details card: about 10 px high in A, about 19 px in B.
- The B cards are whole, and the text is clear. Many cards are in view at the same time, so in B the cards of near villagers overlap each other, and the column of cards over the far villagers reaches the top edge of the view. A card does not overlap itself.

The pictures stay on the laptop: C:\wt-jobicon\bwvr\tools\desk\desk-jobA and desk-jobB.
