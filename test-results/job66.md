# Job 66: the ring with icons, the places sheet and the leash sheet (agent/radial e66b245)

`.\dev.ps1 build -Clean`: 6160 checks, 0 failed. Each log has "build e66b245" and "game renders at 2760x2208". `ring_run.ps1`, test save temple. rg4 (leash), rg5 (places) and rg6 (pages) are RUN DONE.

## rg4 (leash)

- "leash : leash_pictures=1: the pictures of the leashes come from Data\Misc\leash.l3d (68 points, 64 triangles) and Data\Textures\leash.raw with leasha.raw".
- "the ring goes to the sheet of the leashes, its middle at the hand ... The tiles, in cm from the middle of the sheet: leash3 at -9.4 7.8, leash2 at 0.0 7.8, leash1 at 9.4 7.8, frame 205".
- The choice moves over the tiles: Leash of Aggression (tile 2, frame 221), Leash of Learning (tile 1, frame 261), Leash of Compassion (tile 0, frame 301). "the player chose "leash_compassion": the key scan code 0x2F", and "the game: the leash is kind 3 and off (it was kind 2 and off), frame 343".
- 40 lines "a new picture of the ring in the scroll look took": 12.24 ms at the open of the ring (9 parts new), 7.97 ms when the sheet opens, then 1.81 to 3.46 ms. "the sheet of the leashes drew 219 pictures of a leash. The longest took 0.89 ms".
- The pictures (game art, the leash meshes: not pushed): three parchment tiles, Compassion, Learning and Aggression. Each has its band: a rainbow band, a rope band, and a steel band with spikes. The band turns from picture to picture. No hole and no black box round a band. A soft shadow is under each band. No tile is dim (the Creature has each leash). The red tick seal is on Learning, the leash at the start. The scroll under the tiles says "Choose a leash", then the name of the leash under the hand, and "Leash of Learning / It is the leash now".

## rg5 (places)

- "the ring goes to the sheet of the places ... creature at -7.2 8.0, town at -2.6 8.0, temple at 2.1 8.0, enter at 6.8 8.0, land at 11.5 8.0, mark_back at -13.2 8.0, add at 0.0 -5.6, frame 205". There was no bookmark at the start.
- "the choice on the sheet is Add a bookmark (tile 6)", "the aim of bookmark 1 starts", "the aim is on the land at 2250.8 2473.7", "the grip sets bookmark 1 on the land at 2250.8 2473.7 (Bookmark::SetBookMark), 3.0 m of the room along the ray, frame 371", "the game: bookmark 1 is set, frame 372".
- The second aim (Add a bookmark, tile 8): "test: aimmark aims at the nearest living thing of the cone, frame 592", "the aim is on the Sheep 13AC7D30, frame 593", then "the aim is on the land at 1998.3 2324.4, frame 595", and "the grip sets bookmark 2 on the land at 1998.3 2324.4 (Bookmark::SetBookMark), 2.0 m of the room along the ray, frame 631". So bookmark 2 is set on the land, not on the Sheep. No fn_00439F70 line.
- "the player chose "land": the key F3, frame 861", and the view jumps to the game camera at 900.6 3.0 3010.6. "the player chose "Bookmark 1": the key 1, frame 1111", and the view jumps again. "the player chose "Remove bookmark 2": the key Ctrl, Shift and 2, frame 1361", "the game: bookmark 2 is not set, frame 1362".
- The last sheet (frame 1555) has "mark1 at -1.9 -5.6, remove1 at -1.9 -9.2, add at 1.9 -5.6". The hand stays in the middle, and "the ring closes with no choice, frame 1611". So no third aim starts, and there is no line "the aim of bookmark 2 stops: the player pressed B".
- No grab of the world and no far grab line while an aim is on (far_grab=0 in this run).
- Eye shot 1070 (log 343): the ray ends in a gold ring on the land. Eye shot 1340 (log 613): the ray goes to the land at the right, and no living thing has a gold halo.
- The ring pictures (mod art, in test-results/job66): the places row with Back at the left, Creature, Town, Temple, Inside, Land, and the bookmark seals (1, 2, +, the remove seals) under "Bookmarks".

## rg6 (pages)

- "page 1: leash, places_page, recenter, save, more, settings, names, pause", "page 2: snapshot, help, details, faster, more, slower, none, none".
- The round over all items (frames 177 to 317) ends with "the ring closes with no choice, frame 371". No sheet opens.
- More shows Page 2 (frame 554), then the ring closes with no choice.
- "the player chose "names": the key scan code 0x31, frame 801", "the villager names are on, frame 802". Again at frame 951: "the villager names are off".
- The pause is not tested: at frame 1221 the ring opens at -0.160 -0.240 -0.420 m (not at -0.200 -0.300 -0.350 m), the hand is on "save", and "the player chose "save"" at frame 1251. "save card : the game saved in 216 ms (result 1), frame 1377". desk_run put the profile back after the run: the files of the profile in the game folder have their old times.
- The ring pictures (mod art, in test-results/job66): page 1 with the new icons (the leash, the places pin, the names card), page 2, and the cards "Show villager names", "Hide villager names", "Pause the game" and "Quick save".

## Job 70: rg5b and rg6b (agent/radial 57fd9c9)

`.\dev.ps1 build -Clean`: 6160 checks, 0 failed. Both are RUN DONE. Page 1: "leash, places_page, recenter, none, more, settings, names, pause". Each ring opens at -0.200 -0.300 -0.350 m.

- rg5b: bookmark 1 is set on the land. For bookmark 2 the test aim is on the Sheep 13AD5D30 at frame 593, then "the aim is on the land at 1998.3 2324.4, frame 605", and the grip at 631 sets bookmark 2 on the land. No fn_00439F70 line. Eye shot 1340: the ray ends at a gold mark on the land, and no creature has a halo. Bookmark 1 moves the view. Remove bookmark 2 gives "bookmark 2 is not set". The last sheet has "add at 1.9 -5.6", and "the aim of bookmark 2 stops: the player pressed B, frame 1651".
- rg6b: the round closes with no choice, More shows page 2, the names go on and off. No save line.
- The profile after the runs: the files of Profiles\_B_u_c_k in the game folder and in its VirtualStore copy have their old times (6 November). The rg6 save of job 66 is not in the profile.
