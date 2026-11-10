# Job 81: the game's Hand hidden in each state, the flow of a miracle on the mod's hand, and the save picture at the size of the read (agent/handdup bb1f49b)

`.\dev.ps1 build -Clean`: 6448 checks, 0 failed. Each log has "build bb1f49b". Commits c2176fa and bb1f49b. In each run folder, desk-state.txt has "the real cursor: at the same place before and after the run". Runs 1 to 4 use dxwrapper, run 5 uses Vulkan. The desk has no headset, so the openxr lines cannot show.

The picture frames of the job are step frames. The BMP names are log frames. The nearest BMPs are: step 1070 is log 343, step 1840 is log 1113, step 1920 is log 1193, step 1135 is log 408, step 1225 is log 498, step 1390 is log 663. The crops and sheets are in C:\BW1-VR\local\shots\job81\.

## 1. hdFire (hand_hide_all=1, hand_flow_glow=1): PASS

- Line 187: "hand exact   : hand_hide_all=1: the mesh of the game's Hand hides in each state (the Totem state too), and also its flow of a miracle. hand_flow_glow=1: the flow of a miracle shows on the mod's pointing hand"
- Line 1010: "hand exact   : the game draws the flow on its Hand (PHandFX::Draw, the call from 0x0068D578). The mod does not draw that copy of the Hand, and its pointing hand takes the flow: a texture of 256 by 256, the color 0xCCFF4646, cell 31 of 32 (frame 63), frame 281"
- Line 1011: "hand model   : the flow of a miracle is drawn on the right hand, frame 281: 97 triangles, the color 0xCCFF4646, the cell at 0.875 0.375"
- The report of the Fireball window, line 1309: "hand exact   : this window 0 draws of the mesh in the Totem state hidden, 133 draws of the flow of a miracle hidden (133 went to the mod's hand), and 0 draws by other calls". Line 1314: "hand model   : the flow of a miracle was on the pointing hand in 133 eye passes".
- The report of the Water window, line 2091: "hand exact   : this window 0 draws of the mesh in the Totem state hidden, 311 draws of the flow of a miracle hidden (311 went to the mod's hand), and 0 draws by other calls". Line 2095: "hand model   : the flow of a miracle was on the pointing hand in 311 eye passes".
- Each of the 6 "hand exact" reports has "draws of the Hand's mesh hidden": "379 composes with the wrist lock, 379 draws of the Hand's mesh hidden" (line 1308), and "450 composes with the wrist lock, 450 draws of the Hand's mesh hidden" (lines 1746, 2090, 2258, 2391, 2522).
- No line "is also drawn by the call before" in any of the five logs (grep count 0).
- The miracles: "Fireball is active in the Hand", frame 281, "the game casts Fireball", frame 412. "Water is active in the Hand", frame 970, "the game casts Water", frame 1052.

Pictures:
- 343 (Fireball held): one pointing hand in a large orange and white blaze of the Fireball, with the red arc of the flow over it. Both eyes show it at the same place. The picture of hdFireOld at 343 looks the same: the blaze covers the hand, so the desk picture cannot show the second hand at this frame.
- 1113 (Water pour): one orange pointing hand, with a blue and white glow of drops and a light purple haze on its lower part. There is no second hand. Both eyes show the glow at the same place on the hand.
- 1193 (Water pour): one pointing hand, with a red and pink ring of glow around its upper part and blue drops on its lower part. The ring has a different shape than at 1113, so the glow moves. Both eyes show it at the same place. There is no second hand.

## 2. hdFireOld (hand_hide_all=0): PASS

- Line 196: "hand exact   : hand_hide_all=0: the mesh of the game's Hand hides only in the exact states. hand_flow_glow=1: the flow of a miracle shows on the mod's pointing hand"
- No "flow of a miracle hidden" count, no "this window ... Totem state hidden" line, no "PHandFX::Draw" line and no "hand model   : the flow of a miracle is drawn on the right hand" line in the log.
- The "hand exact" reports still have "379 draws of the Hand's mesh hidden" (line 1310) and "450 draws of the Hand's mesh hidden" (lines 1755, 2141, 2305, 2436, 2561): the exact states hide the mesh as before.

Pictures, side by side with hdFire:
- 343: the same as hdFire. The Fireball blaze covers the place of the hand.
- 1113 and 1193: a second hand, pink and red, lies along the left side of the mod's orange pointing hand, from the fingers to the wrist. It is the game's Hand with its flow. In hdFire this pink hand is not there. Both eyes show it.

## 3. hdTotem (hand_hide_all=1): PASS

- Line 187: the same "hand_hide_all=1 ... hand_flow_glow=1" line as run 1.
- Line 1131: "hand exact   : this window 137 draws of the mesh in the Totem state hidden, 0 draws of the flow of a miracle hidden (0 went to the mod's hand), and 0 draws by other calls"
- Line 1505: "hand exact   : this window 142 draws of the mesh in the Totem state hidden, 0 draws of the flow of a miracle hidden (0 went to the mod's hand), and 0 draws by other calls"
- The pole moves with the grip: "the totem is at 0.22 ... frame 364", "0.44 ... frame 394", "0.67 ... frame 424", "0.44 ... frame 454", "0.22 ... frame 484", "0.00 ... frame 514". "the Hand let the totem 13AFAF04 go after 284 frames at 0.00", frame 598.

Pictures (crops of the pole, both eyes):
- 408 (pole at 0.44 to 0.67, going up): the statue on the pole is high and half clear (the held look). The mod's orange hand is at the side of the statue. No game hand is on the pole.
- 498 (pole at 0.22, going down): the statue is lower than at 408. No game hand is on the pole.
- 663 (after the release at 0.00): the statue is at its lowest and not clear. No game hand.
- Both eyes agree in each picture.

## 4. hdTotemOld (hand_hide_all=0): PASS

- Line 187: "hand exact   : hand_hide_all=0: the mesh of the game's Hand hides only in the exact states. hand_flow_glow=1: the flow of a miracle shows on the mod's pointing hand"
- No "Totem state hidden" line in the log. The pole values are the same as run 3 (0.22, 0.44, 0.67, 0.44, 0.22, 0.00, release at frame 596).

Pictures, side by side with hdTotem:
- 408 and 498: a small orange game hand (the fingers, as a fist) is on the front of the statue, at the middle of the pole, in both eyes. In hdTotem it is not there. The rest of the picture is the same.
- 663: the same as hdTotem (no game hand, because the Hand let the pole go).

## 5. hdSave (Vulkan, save_picture_test_dump=1, ring_pages=0): PASS

- The run has ring_pages=0 in -Set (see the note of the planner): temple_run.ps1 picks slot 7 of page 1, and only page 1 of ring_pages=0 has "enter" there. Line 549: "ring         : page 1: temple, creature, town, leash_learning, more, leash_compassion, leash_aggression, enter". Line 853: "ring         : the choice is enter, with the hand -6.9 cm to the right and 7.2 cm up from the middle, frame 97". Line 859: "ring         : the player chose "enter": the key F4, 1 time, frame 121". Line 875: "ring         : the game: the player is in the Temple, in the hall, frame 122". The ring choice is correct.
- Line 15: "vk ddraw     : vk_dxt_fit=1. The DXT1 blocks of a save picture get the two colors of least squares when they are nearer to the pixels"
- Line 347: "snapshot     : save_picture=1. The game makes each save picture from a surface of the mod with the middle of the left eye picture, because its screen in VR is not the picture that the player sees. save_picture_fit=1: the surface has the size of the read of the game, 256 x 128 for a save and 400 x 300 for a full picture, so each pixel of the save is the mean of its box"
- Line 869: "snapshot     : the game made a save picture from the left eye of the Vulkan renderer (save_picture=1): the middle 60% of the width of 2760 x 2208, at 256 x 128 (the size of the read of the game, save_picture_fit=1). Its mean light is 0.16, and 0.0% of it is black. The eye picture took 2.7 ms and the call of the game 15.2 ms. The texture is .\Profiles\_B_u_c_k\Saved Games\Pictures\save_game_picture_temporary.raw"
- Line 998: "vk ddraw     : 2 copies from RGB into DXT1 blocks, and 0 copies of the blocks of a rect (vk_blt_blocks=1)"

The dump bwvr-save-picture-1.bmp is 256 x 128, 32 bits. A copy and a 4x enlargement are in C:\BW1-VR\local\shots\job81\. It shows the island from above the land: dark red and purple sky with a grey cloud, grey mountains, the green land at the bottom, the sea at the right, the red beam of light down the middle, and orange parts of the hands at the left. The edges are smooth: the mountain lines, the cloud and the beam have soft edges, with no steps and no repeated pixels. The dump is the surface of the mod before the DXT1 copy, so it does not show the blocks of the texture of the game. The .raw texture was not looked at.
