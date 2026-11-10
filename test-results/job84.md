# Job 84: no ellipsis, the game's orbs in the spell book, and the group Miracles of the debug window (agent/spellbook 82786e4)

`.\dev.ps1 build -Clean`: 6517 checks, 0 failed. Each log has "Black & White VR Mod 1.2.0 (build 82786e4)". `village_run.ps1`, `card_send_test_save=0`. Frame numbers are log frames (log frame = step frame - 729). The page of the book and the page of the debug window are quad layers, so the eye pictures do not show them. The saved pictures of the panels show them.

## 1. sbU1 (-Kind unlock, backend=dxwrapper\ddraw.dll, card_send_test_save=0): PASS

The recipe set test_frame_rate=60 and text_fit_test=1: "present      : test_frame_rate=60, a test key of the desktop runs. ..." and the text fit lines below.

The ring and the debug window:
- "ring         : the ring opens at the left hand (the button, and the hand picks), at -0.200 -0.300 -0.350 m, 50 cm from the head. Page 1: leash, places_page, recenter, debug_window, more, settings, names, pause, frame 271"
- "ring         : the choice is debug_window, with the hand 6.9 cm to the right and -7.2 cm up from the middle, frame 277"
- "ring         : the player chose "debug": the debug window opens, frame 301"
- "debug page   : debug_menu=1. The ring has the item "debug" with 147 switches in 10 groups, 126 of them can change now. test_keys=1"
- "debug window : the window opens, 0.56 by 0.56 m, its middle at -0.49 -0.21 -0.85 m, the group View with 22 switches, frame 301"
- "debug window : the trigger presses the tab Miracles: 9 switches, frame 361"
- "debug window : the trigger presses Unlock all miracles (row 0), frame 411"
- "debug page   : the player chose "Unlock all": debug_all_miracles is 1 now (on), it was 0, frame 411"
- "spell book   : debug_all_miracles=1: the town 13B000C4 now knows 21 more of the 24 magic types of the miracles (Town::AddMagicTypesHeld), and its Worship Site 13A072A0 has 6 miracle icons (it had 6), frame 411"
- "spell book   : debug_all_miracles=1: the Worship Site 13A072A0 has no limit of prayer power now (+0x108), frame 411"
- "debug window : the window closes: the player pressed Close, frame 461"

The hand cut (the question of job 86): yes. "hand cut     : frame 271, a hand is in front of the page of the ring: 51626 px of holes for the left eye and 53513 px for the right eye. The card goes out as one quad for each eye". The total of that window: "hand cut     : this window a hand was in front of a page in 40 frames: the card went out as one quad for each eye. The largest hole of an eye had 53513 px, and the longest cut took 14.36 ms".

The spell book and the orbs (N = 6):
- "spell book   : spell_book_orbs=1: each cell shows the orb of the game, the glass ball with the miracle in it"
- "spell book   : the book opens over the left hand, at -0.081 -0.280 -0.335 m, 44 cm from the head, 20 by 16 cm. 6 miracles: Fireball, Forest, Storm, Water, Food, Heal, frame 571"
- "spell book   : the game draws 6 orbs in the cells of the book, each 4.3 cm across, frame 571"
- "book bubble  : the game draws 6 orbs of the spell book in this eye pass, the first at 1905.18 45.23 2518.01, frame 571"
- "spell book   : the orbs of the game showed in the cells of the book in 150 frames this window, with 906 draws of an orb"
- The steps of the word "book": "the pointing hand is at Fireball, frame 611", "the pointing hand is at Forest, frame 651". "spell book   : the book closes: the button, frame 721".
- "hand cut     : frame 611, a hand is in front of the page of the spell book: 40683 px of holes for the left eye and 39058 px for the right eye. The card goes out as one quad for each eye"

The text fit: no line ends in "is cut". Each check line says "fits in". The totals line:
- "text fit     : check: text_fit_test=1: 2703 texts of the cards and panels measured in their boxes: 320 at a smaller font, 101 on more lines, 0 cut"

The saved pictures (mod panels only, copies in C:\BW1-VR\local\shots\job84\u1-book.png and u1-debug.png):
- bwvr-book-00571, 00611, 00651: the parchment page, title "Miracles: all (test)", then "Fireball" and "Forest" as the hand steps. Two rows of three cells: Fireball, Forest, Storm, Water, Food, Heal. Each cell is a round hole with a tan rim (the checkerboard of the alpha shows through). The cell under the hand has an orange ring and a light square. No word is cut, and there is no ellipsis.
- bwvr-debugwin-00301, 00361, 00411, 00422: "Debug switches" with 10 tabs (View, Picture, Render, Interface, Hand, Throw, Creature, Miracles, Sound, Temple). At 361 the tab Miracles is lit, and row 0 is "Unlock all miracles OFF" (red seal), then Spell book, Book orbs ORBS, Book point, Book pull, Book bubble GAME, Miracle hand, Gesture point FINGERTIP, Gesture trail. At 411 the row shows "ON" with a green seal, and the foot says "Unlock all miracles: on". At 422 the note shows: "Unlock all miracles, no cost (test)", "For a test only. Each town knows each miracle, the spell book shows up to 6, and the miracles cost no prayer power.", "Key: debug_all_miracles". No word is cut, and there is no ellipsis.

The eye pictures near steps 1320 to 1392: bwvr-shared-left/right-00593 (step 1322), 00625 (step 1354), 00665 (step 1394). In each picture six glass balls sit in a grid of 3 by 2 next to the orange hand, at the place of the book. The grid is in both eyes, and the shift between the eyes is the same as the shift of the hand and of the temple behind it, so the depth agrees. The size is sane: each ball is about one third of the width of the hand. At 625 and 665 the pointing hand (dim and brown) is in front of the grid. Two points to look at in the headset:
- Each ball looks flat: about 1.4 times wider than it is high. The other objects of the scene do not look flat, so the cause can be the scale of the orb and not the eye camera.
- The icon of each miracle (fire, a trunk, lightning, a horn) shows at the bottom edge of its ball and partly below it, not in its middle. The log says "book bubble  : the glass ball is made, its middle at 0.000 2.228 0.000 of its mesh".
The eye pictures cannot show the page of the book under the balls (a quad layer). The place over the left hand is from the log line of the book.

No save was written. The mod log has no line of a save of the game: no "Saving..." card, no ring choice "save", no quick save, and no line of GameOSFile::SaveAllGame other than its hook ("hooks        : GameOSFile::SaveAllGame at 0x00558160 ok (the card of a save)"). The game opens ".\Profiles\_B_u_c_k\Saved Games\20\20.sav" and "vortex.txt" with write access only at the load, before "game start   : the saved game is loaded, frame 0" (lines 414 to 419). The runs sbB1 and sbB0 have the same opens at the load, so they come from the load of slot 20 and not from the unlock. After frame 0 the only file writes are the BMP pictures of the run and bwvr-hint-counts.ini.new in the run folder. The registry guard dropped each write of the game to the registry. After the run the chain log has "profile put back: 8 files restored (7 of them had different bytes, the others only a different time), 0 new files removed, 0 new folders removed, in C:\Users\PeakDesign\AppData\Local\VirtualStore\Program Files (x86)\Lionhead Studios Ltd\Black & White\Profiles" and "profile put back: 6 files restored (6 of them had different bytes, the others only a different time), 0 new files removed, 0 new folders removed, in C:\Program Files (x86)\Lionhead Studios Ltd\Black & White\Profiles". These are the same counts as in the other runs of the chain: the files of the test save and of the profile go back, and the run made no new file.

## 2. sbB1 (-Kind book, Vulkan, test_frame_rate=60, card_send_test_save=0): PASS

"backend      : vulkan, DirectDraw of the mod (vk_ddraw.cpp)". "spell book   : spell_book_orbs=1: each cell shows the orb of the game, the glass ball with the miracle in it".

The orbs:
- "spell book   : the book opens over the left hand, at -0.081 -0.280 -0.335 m, 44 cm from the head, 20 by 10 cm. 3 miracles: Water, Food, Heal, frame 271"
- "spell book   : the game draws 3 orbs in the cells of the book, each 4.3 cm across, frame 271"
- "book bubble  : the game draws 3 orbs of the spell book in this eye pass, the first at 1904.27 44.11 2518.02, frame 271"
- "spell book   : the orbs of the game showed in the cells of the book in 180 frames this window, with 482 draws of an orb"
- "spell book   : the orbs of the game showed in the cells of the book in 211 frames this window, with 606 draws of an orb"

The pull and the spring back (as the recipe expects):
- "spell book   : the pointing hand grips Food and pulls it out of the book: the hand is 0.0 cm from its cell, and 10 cm more takes it, frame 382"
- "book bubble  : the game draws the bubble of the seed type 3 at 1904.06 43.88 2515.13, the ball 6.0 cm across at the scale 0.6088, frame 382"
- "spell book   : the grip opens at 92 percent of the pull (the most was 92 percent), so Food springs back into the book, frame 422"

The snap, the miracle in the Hand and the cancel with B:
- "spell book   : Food snaps into the hand: the hand is 10.1 cm from its cell, frame 542"
- "spell book   : the request of Food goes to the game: packet 0x25 with the seed type 3, as at the end of a chain of gestures, 0 frames after the take, frame 542"
- "spell book   : the Miracle Food is in the Hand, 5 frames after the request, frame 547"
- "miracle      : Food left the Hand (a short press of the second face button). The Hand is free, Hand state 8, hand mode 0x3, frame 633"

The refusal:
- "village      : test key village_test_chants: 1 Worship Sites of the player have a pool of 0 chants now, frame 670"
- "spell book   : the book opens over the left hand, ... 3 miracles: Food (dim), Water (dim), Heal (dim), frame 691"
- "spell book   : the pointing hand takes Food from the book, and the game refuses it: No prayer power. Villagers must worship, frame 742"
- "haptics      : refused, 0.35 for 30 ms (strength 1.00, the pointing hand), frame 742"
- "spell book   : the book closes: the button, frame 811"
- The totals: "1 pulls (1 snaps, 0 back into the book), 1 miracles taken, 1 requests to the game, 1 Miracles in the Hand, 1 refusals".
- "text fit     : this window 6 texts had a smaller font, 0 went on more lines, and 0 were cut"

The saved pictures (C:\BW1-VR\local\shots\job84\B1-book.png and B1-bubble.png). card_send_test_save=0 did not stop them: bwvr-book-* (9) and bwvr-bubble-* (2) are in the run folder.
- bwvr-book-00271 to 00512: one row of three round holes (Water, Food, Heal), title "Miracles", "Water" or "Food". At 382 and 512 (the pull) the cell of Food is empty: a thin ring and no hole, because the bubble is out of the book.
- bwvr-book-00691 and 00711: the title "No prayer power. Villagers must worship" in red. The holes are pale (dim) and the names are grey. No word is cut, and there is no ellipsis.
- bwvr-bubble-00382 and 00512: the flat picture of Food (a blue disc with the two gestures and a yellow rim).
- The eye pictures bwvr-vk-left/right-00285, 00315, 00355 show three glass balls in a row at the place of the book, in both eyes. They look flat in the same way as in run 1.

One odd line: at frame 691, for 5 frames, the card of the pointing hand says "Food Miracle, Water: No prayer power. Villagers must worship" (the old text and the new text together). At frame 696 it says "Water: No prayer power. Villagers must worship".

## 3. sbB0 (-Kind book, Vulkan, spell_book_orbs=0): PASS

- "spell book   : spell_book_orbs=0: each cell shows a flat picture with the two gestures of the miracle"
- No orb line: grep -i "orb" finds only that key line. There is no "the game draws N orbs" line and no "the orbs of the game showed" line.
- The pull, the spring back, the snap, the cancel and the refusal are the same as in sbB1, at the same frames: "grips Food ... frame 382", "springs back into the book, frame 422", "Food snaps into the hand: the hand is 10.1 cm from its cell, frame 542", "the Miracle Food is in the Hand, 5 frames after the request, frame 547", "Food left the Hand (a short press of the second face button) ... frame 628", "the game refuses it: No prayer power. Villagers must worship, frame 742". The bubble of the pull still shows: "book bubble  : the game draws the bubble of the seed type 3 ... frame 382".
- "text fit     : this window 6 texts had a smaller font, 0 went on more lines, and 0 were cut"
- The saved pictures (C:\BW1-VR\local\shots\job84\B0-book.png): each cell is a blue disc with the two white gestures of the miracle, no hole. The cells of the refusal are grey discs. The empty cell of the pull is the same as in sbB1.
- The eye pictures bwvr-vk-left/right-00285, 00315, 00355 show no glass balls at the place of the book.

## Only a headset can show

- The page of the book with the orbs in its holes, as one picture. The desk eye pictures do not have the quad layers.
- Whether the balls look flat and the icons sit low in the headset.
