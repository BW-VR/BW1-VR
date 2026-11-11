# Job 90: the cost of the hand cut, the hole where the hand is opaque, the orbs to each eye, and the card of a refusal (agent/spellbook 4a1c684)

`.\dev.ps1 build -Clean`: 6572 checks, 0 failed. Each log has "Black & White VR Mod 1.2.0 (build 4a1c684)". `village_run.ps1`, `card_send_test_save=0`. Each run is "RUN DONE". Frame numbers are log frames (log frame = step frame - 729). The page of the book and the page of the ring are quad layers. The eye pictures bwvr-shared-* do not show them. The pictures bwvr-vk-video-* show the left eye with the book over it. The crops, the sheets and the masks are in `C:\BW1-VR\local\shots\job90\`.

The chain log (scratchpad job90.log) has for each run "profile put back: 8 files restored (7 of them had different bytes, the others only a different time), 0 new files removed, 0 new folders removed, in C:\Users\PeakDesign\AppData\Local\VirtualStore\...\Profiles" and "profile put back: 6 files restored (6 of them had different bytes, the others only a different time), 0 new files removed, 0 new folders removed, in C:\Program Files (x86)\...\Profiles". These are the same counts as in job 84.

## 1. sbH2 (-Kind bookhand, Vulkan, card_send_test_save=0): FAIL

Two items fail: the longest cut takes 2.664 ms and 2.949 ms (the job asks for under 1 ms), and the land shows through the arm inside the hole of the page. The steps, the lines of a hand in front and of no hand, and the cover of the page at -8 cm are correct.

The steps and the frame lines:
- "hand cut     : card_hand_front=1: a hand in front of the page of the spell book or of the ring shows in front of it, in each eye, where the hand is opaque"
- "spell book   : test: the word "book" steps the pointing hand to item 0 (Water) of the book, 0 cm in front of the page, frame 300"
- "hand cut     : frame 301, a hand is in front of the page of the spell book: 23722 px of holes for the left eye and 25467 px for the right eye. The card goes out as one quad for each eye"
- "spell book   : test: the word "book" steps the pointing hand to item 1 (Food) of the book, 5 cm in front of the page, frame 350"
- "hand cut     : frame 361, the pictures of the spell book with the holes of the hand are saved: 36921 px for the left eye and 34722 px for the right eye"
- "spell book   : test: the word "book" steps the pointing hand to item 2 (Heal) of the book, -8 cm in front of the page, frame 400"
- "hand cut     : frame 401, no hand is in front of the page of the spell book now. The card goes out as one quad for both eyes" (1 frame after the -8 cm step)
- "controllers  : the fake hand steps 15 cm toward the head over 10 frames at frame 440"
- "hand cut     : frame 442, a hand is in front of the page of the spell book: 55 px of holes for the left eye and 55 px for the right eye. The card goes out as one quad for each eye"
- "hand cut     : frame 472, the pictures of the spell book with the holes of the hand are saved: 36228 px for the left eye and 39870 px for the right eye"
- "controllers  : the fake hand steps back to where it was over 10 frames at frame 490"
- "hand cut     : frame 499, no hand is in front of the page of the spell book now. The card goes out as one quad for both eyes"
- "spell book   : the book closes: the button, frame 571"

The window lines (Y must be under 1 ms):
- "hand cut     : this window the cut ran in 180 frames, 1.029 ms on average and 2.664 ms at the most (with no save of a test picture). A hand was in front of a page in 109 frames: the card went out as one quad for each eye. The largest hole of an eye had 39870 px"
- "hand cut     : this window the cut ran in 120 frames, 0.977 ms on average and 2.949 ms at the most (with no save of a test picture). A hand was in front of a page in 48 frames: the card went out as one quad for each eye. The largest hole of an eye had 39870 px"
- FAIL: Y is 2.664 ms and 2.949 ms. The average is about 1 ms. In sbH1 the longest cut (with the saves) was 20.80 ms and 23.61 ms, so the cost is much lower, but not under 1 ms.

The holes against sbH1: 0 cm 23722 / 25467 px (sbH1 29649 / 31879, 20% less). 5 cm 36921 / 34722 px (sbH1 37327 / 35090, 1% less). -8 cm no hole (sbH1 at -5 cm 3287 / 2692 px). The pull 36228 / 39870 px (sbH1 29910 / 41689).

The pictures of the mod page (bwvr-handcut-left/right-*, sheet h2-handcut.png):
- 00301 and 00331 (0 cm, Water): a band of holes goes from the top edge to the bottom edge of the page, between the cell Water and the cell Food. It cuts the right half of the ring of Water and the end of "Water". At the top it has a notch. The pick mark is in the cell Water, outside the hole. The right eye band is a little to the right of the left eye band.
- 00361 and 00391 (5 cm, Food): a wider band from the top edge down through the right half of the cell Food to the bottom edge. It cuts the title to "F" and "Food" to "Foo".
- 00442 (the start of the pull, 55 px): the page is whole. I see no hole at this size.
- 00472 (the pull): a hole at the right edge over the cell Heal and the top right corner. The title is "Heal".
- The band reaches the top edge and the bottom edge of the page at 0 cm and at 5 cm, as in sbH1. The masks do not show a change that keeps the faded forearm out of the hole.

The eye pictures as the headset composes them (bwvr-vk-video-N.bmp, crops h2-video-crop.png, h2-323-zoom.png, h2-373-zoom.png):
- 00323 (0 cm): YES, the hand shows over the page in the band between Water and Food. NO, the land does show through: inside the hole the arm is half clear, and the walls of the temple show through it. A part of the hole next to the arm shows the temple with only a faint orange tint. A piece of tan parchment shows in the notch at the top of the hole.
- 00373 (5 cm): YES, the hand shows over the page through the right half of the cell Food. NO, the land does show through: inside the hole the arm is half clear, and the tiles of the temple roof show clearly through it from the top edge to the bottom edge of the page.
- 00423 (-8 cm): YES, the page covers the hand. The page is whole with its three orbs. The fingers show only below the bottom edge. The thin wedge of sbH1 is gone.
- 00473 (the pull): the hand shows over the page at the right edge, and the temple shows through the half clear arm inside the hole.

What only a headset can show: the depth of the hand and the page in the two eyes, and how clear the arm looks in the headset.

## 2. sbU2 (-Kind unlock, backend=dxwrapper\ddraw.dll, card_send_test_save=0): FAIL

One item fails: the longest cut of the ring is 4.825 ms (the job asks for under 1 ms). The debug lines, the orbs and the card are correct.

The debug page, the debug window and the book bubble (as in job 84):
- "ring         : the ring opens at the left hand (the button, and the hand picks), at -0.200 -0.300 -0.350 m, 50 cm from the head. Page 1: leash, places_page, recenter, debug_window, more, settings, names, pause, frame 271"
- "ring         : the choice is debug_window, with the hand 6.9 cm to the right and -7.2 cm up from the middle, frame 277"
- "debug page   : debug_menu=1. The ring has the item "debug" with 150 switches in 10 groups, 129 of them can change now. test_keys=1" (job 84: 147 and 126)
- "debug window : the window opens, 0.56 by 0.56 m, its middle at -0.49 -0.21 -0.85 m, the group View with 22 switches, frame 301"
- "debug window : the trigger presses the tab Miracles: 9 switches, frame 361"
- "debug window : the trigger presses Unlock all miracles (row 0), frame 411"
- "debug page   : the player chose "Unlock all": debug_all_miracles is 1 now (on), it was 0, frame 411"
- "spell book   : debug_all_miracles=1: the town 13B060C4 now knows 21 more of the 24 magic types of the miracles (Town::AddMagicTypesHeld), and its Worship Site 13A0D2A0 has 6 miracle icons (it had 6), frame 411"
- "spell book   : debug_all_miracles=1: the Worship Site 13A0D2A0 has no limit of prayer power now (+0x108), frame 411"
- "debug window : the window closes: the player pressed Close, frame 461"
- "spell book   : the book opens over the left hand, at -0.081 -0.280 -0.335 m, 44 cm from the head, 20 by 16 cm. 6 miracles: Fireball, Forest, Storm, Water, Food, Heal, frame 571"
- "spell book   : the game draws 6 orbs in the cells of the book, each 4.3 cm across, frame 571"
- "book bubble  : the glass ball is made, its middle at 0.000 2.228 0.000 of its mesh"
- "book bubble  : the game draws 6 orbs of the spell book in this eye pass, the first at 1905.18 45.23 2518.01, frame 571"
- "spell book   : the pointing hand is at Fireball, frame 611", "spell book   : the pointing hand is at Forest, frame 651", "spell book   : the book closes: the button, frame 721"
- "spell book   : the orbs of the game showed in the cells of the book in 150 frames this window, with 906 draws of an orb"
- "text fit     : check: text_fit_test=1: 2778 texts of the cards and panels measured in their boxes: 320 at a smaller font, 101 on more lines, 0 cut". No line has "is cut".

The hand cut:
- "hand cut     : frame 271, a hand is in front of the page of the ring: 30733 px of holes for the left eye and 30945 px for the right eye. The card goes out as one quad for each eye" (sbU1: 51626 and 53513 px)
- "hand cut     : this window the cut ran in 41 frames, 1.116 ms on average and 4.825 ms at the most (with no save of a test picture). A hand was in front of a page in 41 frames: the card went out as one quad for each eye. The largest hole of an eye had 30945 px". FAIL: 4.825 ms is not under 1 ms (sbU1: 14.36 ms).
- The book: "hand cut     : frame 611, a hand is in front of the page of the spell book: 36992 px of holes for the left eye and 35102 px for the right eye. ..." and "hand cut     : this window the cut ran in 150 frames, 1.040 ms on average and 2.876 ms at the most (with no save of a test picture). A hand was in front of a page in 110 frames: ... The largest hole of an eye had 38128 px". Also not under 1 ms.

The orbs in the eye pictures (bwvr-shared-left/right-00593, 00625, 00665, steps 1322, 1354, 1394). Crops sbU2-593-orbs.png, sbU1-593-orbs.png, u2-left-593-zoom.png, u2-right-593-zoom.png, u2-00625.png, u2-00665.png. I measured each ball with a mask of its pale blue pixels (bounding box in px of the 2760 x 2208 picture):
- sbU2 00593 left: Forest 87 x 87 (1.00), Storm 97 x 91 (1.07), Water 86 x 92 (0.93), Food 91 x 96 (0.95), Heal 103 x 101 (1.02). Fireball is partly over the hand and the mask cuts it.
- sbU2 00593 right: Fireball 79 x 76 (1.04), Forest 81 x 86 (0.94), Storm 89 x 90 (0.99), Water 77 x 91 (0.85, the edge of a wall cuts it), Food 88 x 96 (0.92), Heal about 96 x 102 (0.94, the mask has two parts).
- sbU2 00625 Storm: 95 x 97 left and 88 x 96 right. 00665 Storm: 95 x 95 left and 88 x 95 right.
- sbU1 00593 for a compare: left 75 x 52 (1.44), 85 x 61 (1.39), 80 x 65 (1.23), 89 x 68 (1.31), 99 x 77 (1.29). Right 82 x 56 (1.46), 86 x 60 (1.43), 86 x 68 (1.26), 92 x 61 (1.51).
- So each ball in sbU2 is round in both eyes: width / height 0.92 to 1.07 where nothing covers the ball. In sbU1 it was 1.23 to 1.51. The balls are also taller than in sbU1 (about 90 px high, sbU1 about 60 px).
- The icons: each icon is inside its ball, near the middle, a little to the right of and below the middle. The orange glow of Fireball and the red point of Heal are in the middle area. The horn of Food is in the middle, and at 00665 its end shows straight in the middle of the ball. No icon shows at the bottom edge or below the ball, as in sbU1. The highlights of the glass make some icons faint (Water, Storm).
- The grid of 3 by 2 is in both eyes at the place of the book, with the same shift between the eyes as the hand and the temple. At 00625 and 00665 the pointing hand (half clear) is in front of the grid.

The card of the pointing hand: each "the pointing card now says" line in sbU2 has one text: "[A] Teleport", "[Trigger] Select", "Put your hand on a miracle", "[Grip] Hold and pull out Fireball", "[Grip] Hold and pull out Forest", or "nothing". No line has "Miracle" with a line of the book. No line has a text of the card of a miracle in the Hand.

No save was written. After "game start   : the saved game is loaded, frame 0" (line 479) the only file writes of the log are the BMP pictures of the run, bwvr-hint-counts.ini.new in the run folder, and files of the NVIDIA driver (nvdrssel.bin, nvAppTimestamps, CaptureCore.log, a DXCache file). The game opens ".\Profiles\_B_u_c_k\Saved Games\20\20.sav" and "vortex.txt" for a write only at the load (lines 415 to 420), as in sbU1. The log has no "Saving..." card, no ring choice of a save, and no line of GameOSFile::SaveAllGame other than its hook. The registry guard dropped each write of the game to the registry ("The mod drops the write", 116 lines). The profile put back counts are the same as in the other runs (see the top).

## 3. sbB2 (-Kind book, Vulkan, test_frame_rate=60, card_send_test_save=0): PASS

The pull and the spring back:
- "spell book   : the book opens over the left hand, at -0.081 -0.280 -0.335 m, 44 cm from the head, 20 by 10 cm. 3 miracles: Water, Food, Heal, frame 271"
- "spell book   : the pointing hand grips Food and pulls it out of the book: the hand is 0.0 cm from its cell, and 10 cm more takes it, frame 382"
- "book bubble  : the game draws the bubble of the seed type 3 at 1904.06 43.88 2515.13, the ball 6.0 cm across at the scale 0.6088, frame 382"
- "spell book   : the grip opens at 92 percent of the pull (the most was 92 percent), so Food springs back into the book, frame 422"

The snap, the miracle in the Hand and the cancel:
- "spell book   : Food snaps into the hand: the hand is 10.1 cm from its cell, frame 542"
- "spell book   : the request of Food goes to the game: packet 0x25 with the seed type 3, as at the end of a chain of gestures, 0 frames after the take, frame 542"
- "spell book   : the Miracle Food is in the Hand, 5 frames after the request, frame 547"
- "miracle      : Food left the Hand (a short press of the second face button). The Hand is free, Hand state 8, hand mode 0x3, frame 632"

The refusal:
- "village      : test key village_test_chants: 1 Worship Sites of the player have a pool of 0 chants now, frame 670"
- "spell book   : the book opens over the left hand, ... 3 miracles: Food (dim), Water (dim), Heal (dim), frame 691"
- "spell book   : the pointing hand is at Water, which the game refuses now, frame 691"
- "spell book   : the pointing hand takes Food from the book, and the game refuses it: No prayer power. Villagers must worship, frame 742"
- "haptics      : refused, 0.35 for 30 ms (strength 1.00, the pointing hand), frame 742"
- "spell book   : the book closes: the button, frame 811"
- The totals: "1 pulls (1 snaps, 0 back into the book), 1 miracles taken, 1 requests to the game, 1 Miracles in the Hand, 1 refusals".

The card at the refusal:
- "hint card    : frame 691, the pointing card now says: Water: No prayer power. Villagers must worship. The context of the Hand is the spell book since frame 691, so the card is 0 frames and 0 ms behind it (prompt_timing=1)"
- "hint card    : frame 691, the controller of the right hand has 1 cards: Water: No prayer power. Villagers must worship (no button, 18.3 by 3.0 cm, at 0.075 -0.267 -0.339 m). The prompt is for the spell book"
- The card before it is "Food Miracle" (frame 632). The next change is at frame 711: "Food: No prayer power. Villagers must worship". There is no change line at frame 696. In sbB1 the card said "Food Miracle, Water: No prayer power. Villagers must worship" at frame 691 (18.3 by 4.6 cm, two lines) and changed at frame 696. In sbB2 the card is one line high (3.0 cm).
- In the whole run, no card line has two texts that do not go together. The lines with more than one text are the cards of one state: "Pull farther to take it, [Grip] Open to put it back" (2 cards) and "Food Miracle, [Grip] Hold to pour, [B] Tap to cancel, or shake" (3 cards).

The saved pictures (B2-book.png, B2-card.png):
- bwvr-book-00271 to 00512: as in sbB1. One row of three round holes (Water, Food, Heal). At 382 and 512 the cell of Food is empty (a thin ring), because the bubble is out of the book.
- bwvr-book-00691 and 00711: the title "No prayer power. Villagers must worship" in red. The holes are pale and the names are grey. The orange ring is on Water at 691 and on Food at 711.
- bwvr-hintcard-right-button0-00725 and 00755: one scroll, "Food: No prayer power. Villagers must worship". No word is cut.
- "text fit     : this window 5 texts had a smaller font, 0 went on more lines, and 0 were cut"

Odd, not a fault of this job:
- The hand cut in this run: "this window the cut ran in 180 frames, 1.438 ms on average and 3.353 ms at the most" and "211 frames, 1.491 ms on average and 3.441 ms at the most". The holes go up to 60381 px.
- From frame 811 (the book closes) to frame 983, the card says "No prayer power. Villagers must worship" and the context stays "the spell book since frame 691". sbB1 has the same lines (811 and 981).

## Only a headset can show

- The page of the book with the round orbs in its holes, as one picture.
- How clear the arm in the hole of the page looks, and the depth of the hand against the page in the two eyes.
