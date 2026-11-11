# Job 93: the hand cut at half resolution, the solid hand in front of the page, and the card when the book closes (agent/spellbook 95a8557)

`.\dev.ps1 build -Clean`: 6718 checks, 0 failed. Each log has "Black & White VR Mod 1.2.0 (build 95a8557)". `village_run.ps1`, `card_send_test_save=0`. Each run is "RUN DONE". Frame numbers are log frames (log frame = step frame - 729). The page of the book and the page of the ring are quad layers. The eye pictures bwvr-shared-* do not show them. The pictures bwvr-vk-video-* show the left eye with the book over it. The crops, the sheets and the zooms are in `C:\BW1-VR\local\shots\job93\`.

The chain log (scratchpad job93.log) has for each run "profile put back: 8 files restored (7 of them had different bytes, the others only a different time), 0 new files removed, 0 new folders removed, in C:\Users\PeakDesign\AppData\Local\VirtualStore\...\Profiles" and "profile put back: 6 files restored (6 of them had different bytes, the others only a different time), 0 new files removed, 0 new folders removed, in C:\Program Files (x86)\...\Profiles". These are the same counts as in job 90.

## 1. sbH3 (-Kind bookhand, Vulkan, card_send_test_save=0): FAIL

Three items fail: the longest cut of the first window is 1.308 ms (the job asks for under 1 ms), land shows inside the hole next to the solid arm, and the masks still go from the top edge to the bottom edge of the page. The key line, the frame lines, the averages and the card after the close are correct.

The key line:
- "hand cut     : card_hand_front=1: a hand in front of the page of the spell book or of the ring shows in front of it, in each eye, where the hand is opaque. card_hand_solid=1: a hand in front of the page has its full opacity"

The steps and the frame lines:
- "spell book   : the book opens over the left hand, at -0.081 -0.280 -0.335 m, 44 cm from the head, 20 by 10 cm. 3 miracles: Water, Food, Heal, frame 271"
- "spell book   : test: the word "book" steps the pointing hand to item 0 (Water) of the book, 0 cm in front of the page, frame 300"
- "hand cut     : frame 301, a hand is in front of the page of the spell book: 33824 px of holes for the left eye and 36660 px for the right eye. The card goes out as one quad for each eye" (sbH2: 23722 and 25467 px)
- "spell book   : test: the word "book" steps the pointing hand to item 1 (Food) of the book, 5 cm in front of the page, frame 350"
- "hand cut     : frame 351, the pictures of the spell book with the holes of the hand are saved: 52252 px for the left eye and 45008 px for the right eye" (sbH2: 36921 and 34722 px)
- "spell book   : test: the word "book" steps the pointing hand to item 2 (Heal) of the book, -8 cm in front of the page, frame 400"
- "hand cut     : frame 401, no hand is in front of the page of the spell book now. The card goes out as one quad for both eyes" (1 frame after the -8 cm step)
- "controllers  : the fake hand steps 15 cm toward the head over 10 frames at frame 440"
- "hand cut     : frame 442, a hand is in front of the page of the spell book: 108 px of holes for the left eye and 96 px for the right eye. The card goes out as one quad for each eye"
- "controllers  : the fake hand steps back to where it was over 10 frames at frame 490"
- "hand cut     : frame 491, the pictures of the spell book with the holes of the hand are saved: 65280 px for the left eye and 51356 px for the right eye"
- "hand cut     : frame 499, no hand is in front of the page of the spell book now. The card goes out as one quad for both eyes"
- "spell book   : the book closes: the button, frame 571"

The window lines (X must be under 0.3 ms, Y under 1 ms):
- "hand cut     : this window the cut ran in 180 frames, 0.045 ms on average and 1.308 ms at the most (with no save of a test picture). On average: the hands into the room 0.004 ms, the masks 0.024 ms, the copies 0.010 ms, the holes 0.006 ms. In 166 frames nothing moved and the cut of the frame before stayed. A hand was in front of a page in 109 frames: the card went out as one quad for each eye. The largest hole of an eye had 75820 px"
- "hand cut     : this window the cut ran in 120 frames, 0.042 ms on average and 0.894 ms at the most (with no save of a test picture). On average: the hands into the room 0.005 ms, the masks 0.022 ms, the copies 0.008 ms, the holes 0.007 ms. In 110 frames nothing moved and the cut of the frame before stayed. A hand was in front of a page in 48 frames: the card went out as one quad for each eye. The largest hole of an eye had 65280 px"
- X: 0.045 ms and 0.042 ms, PASS (sbH2: 1.029 and 0.977 ms). Y: 1.308 ms is FAIL, 0.894 ms is PASS (sbH2: 2.664 and 2.949 ms).
- The holes are larger than in sbH2: 33824 / 36660 px at 0 cm (sbH2 23722 / 25467), 52252 / 45008 px at 5 cm, and 75820 px the largest (sbH2 39870).

The card after the close: "hint card    : frame 580, the pointing card now says: nothing. The context of the Hand is the land since frame 571 ..." and then "[A] Teleport" at frame 662. No line of this run has "No prayer power" on a card. Note: this run has no refusal at all (the totals have "0 refusals"), so it does not test the card of a refusal. The run of the refusal is -Kind book (sbB), and job 93 does not have it.

The pictures of the mod page (bwvr-handcut-left/right-00301, 00351, 00442, 00491, sheet h3-handcut.png, against h2-handcut-left.png):
- 00301 (0 cm, Water): the hole has the shape of the hand: a V notch at the top, the thumb, and a wider part at the bottom over the right half of the ring of Water. It still goes from the top edge to the bottom edge of the page. The shape is almost the same as in sbH2 00301.
- 00351 (5 cm, Food): the hole has a V notch at the top and goes down through the right half of the cell Food and the left part of the cell Heal to the bottom edge. It cuts the title to "F" and "d". It is narrower than the blob of sbH2 00361, but it also goes from the top edge to the bottom edge.
- 00442 (the start of the pull, 108 / 96 px): the page is whole. I see no hole at this size.
- 00491 (the pull): a hole at the right edge over the cell Heal from the top edge to the bottom edge, with a V notch at the top in the right eye.
- FAIL: at 0 cm and at 5 cm the hole still reaches the top edge and the bottom edge of the page. The arm crosses the full height of the page in these poses, so the hole is the hand, the wrist and the forearm.

The eye pictures as the headset composes them (bwvr-vk-video-00323, 00373, 00423, 00473; sheets h3-video.png, sbH3-video-crop.png, sbH2-video-crop.png; zooms h3-323-zoom.png, h3-373-zoom.png, h3-373-edge.png, h3-473-zoom.png):
- 00323 (0 cm): the arm in the hole is solid orange. The temple does not show through the arm (in sbH2 the arm was half clear). NO: in the bottom part of the hole, left of the arm and under the ring of Water, the land (the wall of the temple) shows in a round area of the hole where no solid hand is drawn.
- 00373 (5 cm): the arm in the hole is solid orange from the top edge of the page to the bottom edge. NO: between the ring of Food and the arm, below the ring, the hole shows the roof tiles and the wall of the temple in an area about 40 px wide (h3-373-edge.png). A thin faint outline of the hand crosses this area. In the eye picture without the page (bwvr-vk-left-00373) the fingers and the palm at this place are almost clear, so the hole follows the hand but the hand is not solid there. A triangle of parchment shows in the V notch at the top, between the thumb and the arm. In bwvr-vk-left-00373 that V is empty (the land shows between the thumb and the arm), so the parchment there is correct. Above the top edge of the page the forearm is half clear with the tiles through it. Inside the page I see no faded forearm: the forearm in the hole is solid.
- 00423 (-8 cm): the page covers the hand. The page is whole with its three orbs. The fingers show only below the bottom edge. This is the same as in sbH2.
- 00473 (the pull): the arm is solid orange at the right edge of the page. NO: left of the arm and under the ring of Heal, the hole shows an arch of the temple and the roof tiles in a large round area with a faint outline of the hand. A triangle of parchment shows in the V between the thumb and the arm.
- FAIL: the hand over the page is solid where the game draws it opaque, but the hole also includes the nearly clear palm and fingers, and the land shows there. This is a halo of land at the side of the hand in each of 00323, 00373 and 00473.

What only a headset can show: the depth of the hand and the page in the two eyes, and how the solid arm and the land in the hole look in the headset.

## 2. sbU3 (-Kind unlock, backend=dxwrapper\ddraw.dll, card_send_test_save=0): FAIL

Two items fail: the window of the ring has 0.448 ms on average and 3.023 ms at the most, and the window of the book has 2.076 ms at the most (the job asks for X under 0.3 ms and Y under 1 ms). The debug lines, the book bubble, the text fit, the orbs and the save check are correct.

The debug page, the debug window and the book bubble (as in job 90):
- "ring         : the ring opens at the left hand (the button, and the hand picks), at -0.200 -0.300 -0.350 m, 50 cm from the head. Page 1: leash, places_page, recenter, debug_window, more, settings, names, pause, frame 271"
- "ring         : the choice is debug_window, with the hand 6.9 cm to the right and -7.2 cm up from the middle, frame 277"
- "debug page   : debug_menu=1. The ring has the item "debug" with 161 switches in 10 groups, 140 of them can change now. test_keys=1" (job 90: 150 and 129)
- "debug window : the window opens, 0.56 by 0.56 m, its middle at -0.49 -0.21 -0.85 m, the group View with 25 switches, frame 301" (job 90: 22 switches)
- "debug window : the trigger presses the tab Miracles: 9 switches, frame 361"
- "debug window : the trigger presses Unlock all miracles (row 0), frame 411"
- "debug page   : the player chose "Unlock all": debug_all_miracles is 1 now (on), it was 0, frame 411"
- "spell book   : debug_all_miracles=1: the town 13AFD0C4 now knows 21 more of the 24 magic types of the miracles (Town::AddMagicTypesHeld), and its Worship Site 13A042A0 has 6 miracle icons (it had 6), frame 411"
- "spell book   : debug_all_miracles=1: the Worship Site 13A042A0 has no limit of prayer power now (+0x108), frame 411"
- "debug window : the window closes: the player pressed Close, frame 461"
- "spell book   : the book opens over the left hand, at -0.081 -0.280 -0.335 m, 44 cm from the head, 20 by 16 cm. 6 miracles: Fireball, Forest, Storm, Water, Food, Heal, frame 571"
- "spell book   : the game draws 6 orbs in the cells of the book, each 4.3 cm across, frame 571"
- "book bubble  : the glass ball is made, its middle at 0.000 2.228 0.000 of its mesh"
- "book bubble  : the game draws 6 orbs of the spell book in this eye pass, the first at 1905.18 45.23 2518.01, frame 571"
- "spell book   : the pointing hand is at Fireball, frame 611", "spell book   : the pointing hand is at Forest, frame 651", "spell book   : the book closes: the button, frame 721"
- "spell book   : the orbs of the game showed in the cells of the book in 150 frames this window, with 906 draws of an orb"
- "text fit     : check: text_fit_test=1: 2899 texts of the cards and panels measured in their boxes: 320 at a smaller font, 101 on more lines, 0 cut" (job 90: 2778 texts). No line has "is cut".

The hand cut:
- "hand cut     : card_hand_front=1: ... card_hand_solid=1: a hand in front of the page has its full opacity" (line 533)
- "hand cut     : frame 271, a hand is in front of the page of the ring: 31180 px of holes for the left eye and 30684 px for the right eye. The card goes out as one quad for each eye" (sbU2: 30733 and 30945 px)
- "hand cut     : this window the cut ran in 41 frames, 0.448 ms on average and 3.023 ms at the most (with no save of a test picture). On average: the hands into the room 0.004 ms, the masks 0.125 ms, the copies 0.223 ms, the holes 0.095 ms. In 23 frames nothing moved and the cut of the frame before stayed. A hand was in front of a page in 41 frames: the card went out as one quad for each eye. The largest hole of an eye had 31180 px". FAIL: X 0.448 ms is not under 0.3 ms, and Y 3.023 ms is not under 1 ms (sbU2: 1.116 and 4.825 ms).
- The book: "hand cut     : frame 571, no hand is in front of the page of the spell book now. ...", "hand cut     : frame 611, a hand is in front of the page of the spell book: 38592 px of holes for the left eye and 36640 px for the right eye. ..." and "hand cut     : this window the cut ran in 150 frames, 0.025 ms on average and 2.076 ms at the most (with no save of a test picture). On average: the hands into the room 0.005 ms, the masks 0.005 ms, the copies 0.011 ms, the holes 0.003 ms. In 147 frames nothing moved and the cut of the frame before stayed. A hand was in front of a page in 110 frames: the card went out as one quad for each eye. The largest hole of an eye had 39360 px". X passes, Y 2.076 ms is not under 1 ms (sbU2: 1.040 and 2.876 ms).
- In each run the longest cut is much larger than the average. Possibly the longest cut is the first frame after a move, but the log does not give the frame of the longest cut.

The orbs in the eye pictures (bwvr-shared-left/right-00593, 00625, 00665). I measured each ball with a mask of its pale blue pixels, as in job 90 (bounding box in px of the 2760 x 2208 picture, width x height):
- 00593 left: Forest 87 x 87 (1.00), Storm 97 x 91 (1.07), Water 86 x 92 (0.93), Food 91 x 96 (0.95), Heal 103 x 101 (1.02). Fireball is partly over the hand and the mask cuts it.
- 00593 right: Fireball 79 x 76 (1.04), Forest 81 x 86 (0.94), Storm 89 x 90 (0.99), Water 77 x 91 (0.85), Food 88 x 96 (0.92), Heal 95 x 102 (0.93).
- These are the same numbers as sbU2 00593.
- 00625 Storm: 95 x 97 left and 88 x 96 right. 00665 Storm: 95 x 95 left and 88 x 95 right. The other balls at 00625 and 00665 have the pointing hand in front of them, and the mask cuts them (0.74 to 0.95).
- The icons (u3-left-593-zoom.png): each icon is inside its ball near the middle. The glow of Fireball, the horn of Food and the red point of Heal are in the middle area. No icon shows at the bottom edge or below the ball.

The card of the pointing hand: each "the pointing card now says" line has one text: "[A] Teleport", "nothing", "[Trigger] Select", "Put your hand on a miracle", "[Grip] Hold and pull out Fireball", "[Grip] Hold and pull out Forest". After the close at frame 721 the card says "nothing" (frame 729) and "[A] Teleport" (frame 812). No card line has "No prayer power".

No save was written. After "game start   : the saved game is loaded, frame 0" (line 485) the only file writes of the log are the BMP pictures of the run, bwvr-hint-counts.ini.new in the run folder, and files of the NVIDIA driver. The game opens ".\Profiles\_B_u_c_k\Saved Games\20\20.sav" and "vortex.txt" for a write only at the load (lines 421 to 426), as in sbU2. The log has no "Saving..." card and no line of GameOSFile::SaveAllGame other than its hook. The registry guard dropped each write of the game to the registry ("The mod drops the write", 116 lines, as in sbU2).

## Only a headset can show

- The page of the book with the round orbs in its holes, as one picture.
- How the solid arm and the land at the side of the hand in the hole look in the headset, and the depth of the hand against the page in the two eyes.
