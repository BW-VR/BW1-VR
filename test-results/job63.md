# Job 63: the save picture in VR and the stream of a take at the pile (agent/game 2648a25)

`.\dev.ps1 build -Clean`: 6112 checks, 0 failed. Each log has "build 2648a25" and "game renders at 2760x2208". All five runs are RUN DONE. The saves went into the profile, and desk_run put the profile back after each run.

## The save picture (game-sp1, game-sp0, game-spvk)

- sp1: "snapshot : save_picture=1. The game makes each save picture from a surface of the mod with the middle of the left eye picture, 512 x 384 ...".
- The ring choice "save" at frame 151 (the key Ctrl and scan code 0x1F). sp1: "the game made a save picture from the left eye of the wide target of one walk (save_picture=1): the middle 60% of the width of 2760 x 2208, at 512 x 384. Its mean light is 0.17, and 0.0% of it is black. The eye picture took 11.7 ms and the call of the game 19.2 ms". "save card : the game saved in 215 ms (result 1), frame 284".
- The ring choice "enter" at frame 401: a second save picture, mean light 0.19, 0.0% black, 6.6 ms and 19.8 ms.
- The Temple ring choice "room_save" at frame 3001: "the Save Game Room has 2 saved games (the number and the name of each one): 19 "Quick Save", 20 "Auto Save On Exit"". Then "in the Save Game Room a guide of the game plays, so the ray is off and the room takes no click" from frame 3006 to frame 4221. No third save picture is made in the room. There is one "save card" line in each run.
- sp0: "save_picture=0. The game makes no save picture in VR, because it reads past the end of a square window". "save card : the game saved in 212 ms (result 1), frame 276". "2 save picture requests skipped".
- spvk: "vk_blt_blocks=1 ...". Two save pictures "from the left eye of the Vulkan renderer": mean light 0.16 and 0.19, 0.0% black, 8.7 and 3.5 ms. "save card : the game saved in 212 ms (result 1), frame 1133". The report: "4 copies from RGB into DXT1 blocks, and 0 copies of the blocks of a rect (vk_blt_blocks=1)". No "which the backend does not make" line.
- The dumps bwvr-save-picture-1 and -2 (BMP) of sp1: the land and the sea at the save, and the same land at the Temple entry with the Hand in a corner. Not black and not garbage. There is no third dump. The .raw files stay in the game folder.
- Shot 4000 (log 3273) falls during the guide: the Save Game Room with the subtitle "This is the Room where you save and load the game.". The frames of the empty slots on the walls show the same green pattern in sp1 and in sp0. They do not show the land picture, and they are not magenta or black. Shot 5800 (log 5073): the room, the same in sp1 and sp0.

So the guide of the Save Game Room runs about 1215 frames. game-sp1b and game-spvkb repeat the runs with more shots after the guide (5000, 5300).

## The stream of a take (game-tk1, game-tk0)

- tk1: "take_stream_hand=1. A take with the Hand at the source has the curve too, from the top of the source of the take of the game". "the stream of a take with the Hand at the source starts. The source 13AF8B1C (PileWood) has its point at 1830.1 28.1 2642.6 and its top at 31.2 ..." with "the mod places the pieces".
- tk0: "take_stream_hand=0. A take with the Hand at the source has the line of the game, from the land under the Hand", and the trace says "the line of the game".
- 400 trace lines in each run (frames 355 to 754). The Hand moves from 1830.1 28.6 2642.6 to x 1809.1 and y 58.3 (21 units across and 30 units up). The top of the source stays at 1830.1 31.2 2642.6 in each line.
  - tk1: a piece that is 0.00 s old is at the top of the source (0.00 units) in each line. The newest piece is 1.91 units from the top on average. It is up to 10.1 units away when it is about 0.1 s old and on its way, and 123 of 400 lines have it more than 2 units away. Across, it is 10.3 units from the Hand on average.
  - tk0: the newest piece is on average 0.22 units across from the Hand (at most 5.2), and 11.1 units from the top of the source. So it follows the Hand, not the pile.
- The pictures (log 533, 693, 933): the Hand over the Village Store with its piles. At this size no piece of the stream is clear in tk1 or in tk0.

The pictures stay on the laptop: C:\wt-game\bwvr\tools\desk\desk-game-*.
