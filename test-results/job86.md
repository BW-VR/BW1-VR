# Job 86: the selecting hand in front of the page of the spell book (agent/spellbook 82786e4)

`.\dev.ps1 build -Clean`: 6517 checks, 0 failed. Each log has "build 82786e4". `village_run.ps1 -Kind bookhand`, Vulkan, `card_send_test_save=0`. The recipe sets `video_view=1`, `test_frame_rate=60`, `spell_book_test_depths=0 5 -5`, `hand_cut_test_save=6` and `spell_book_test_save=8`. In each run the real cursor is at the same place before and after the run (desk-state.txt).

The shots of the recipe and the steps of the hand (log frame = step frame - 729):

| vk-video picture | step | the hand |
|---|---|---|
| bwvr-vk-video-00323 | shot@1050 | item 0 (Water), 0 cm in front of the page (step at log frame 300) |
| bwvr-vk-video-00373 | shot@1100 | item 1 (Food), 5 cm in front of the page (log frame 350) |
| bwvr-vk-video-00423 | shot@1150 | item 2 (Heal), 5 cm behind the page (log frame 400) |
| bwvr-vk-video-00473 | shot@1200 | the pull: 15 cm toward the head (log frame 440 to 450, back at 490) |

The crops and the sheets are in `C:\BW1-VR\local\shots\job86\` (h1-video-crop.png and h0-video-crop.png show the two runs at the same frames, h1-handcut.png and h1-book.png show the mod pages).

## 1. sbH1 (card_hand_front=1): FAIL

The hand shows in front of the page at 0 cm, at 5 cm and in the pull, and the page covers the hand at -5 cm in the picture. But the line "no hand is in front of the page of the spell book now" is not in the log. At -5 cm a small hole stays (3287 px and 2692 px), and the hand is in front of the page in each frame while the book is open.

Log lines:
- "hand cut     : card_hand_front=1: a hand in front of the page of the spell book or of the ring shows in front of it, in each eye"
- "spell book   : test: the word "book" steps the pointing hand to item 0 (Water) of the book, 0 cm in front of the page, frame 300"
- "spell book   : test: the word "book" steps the pointing hand to item 1 (Food) of the book, 5 cm in front of the page, frame 350"
- "spell book   : test: the word "book" steps the pointing hand to item 2 (Heal) of the book, -5 cm in front of the page, frame 400"
- "controllers  : the fake hand steps 15 cm toward the head over 10 frames at frame 440", and "the fake hand steps back to where it was over 10 frames at frame 490"
- "hand cut     : frame 301, a hand is in front of the page of the spell book: 29649 px of holes for the left eye and 31879 px for the right eye. The card goes out as one quad for each eye". This is the only line of this form.
- The saved pictures: "hand cut     : frame N, the pictures of the spell book with the holes of the hand are saved: A px for the left eye and B px for the right eye" at frame 301 and 331 (29649 / 31879), 361 and 391 (37327 / 35090), 421 (3287 / 2692) and 451 (29910 / 41689).
- MISSING: "hand cut     : frame N, no hand is in front of the page of the spell book now". No line has "no hand is in front". At frame 421, 21 frames after the -5 cm step, the holes are 3287 px and 2692 px, not 0.
- "hand cut     : this window a hand was in front of a page in 150 frames: the card went out as one quad for each eye. The largest hole of an eye had 41689 px, and the longest cut took 20.80 ms" (the window to about frame 450, so each frame from 301 has a hand in front).
- "hand cut     : this window a hand was in front of a page in 120 frames: the card went out as one quad for each eye. The largest hole of an eye had 41689 px, and the longest cut took 23.61 ms" (frame 451 to the close at frame 571).
- "spell book   : the book closes: the button, frame 571"

A and B differ a little at 0 cm (7%) and at 5 cm (6%). In the pull they differ more: 29910 and 41689 px (39%).

The longest cut takes 20.80 ms and 23.61 ms. That is more than a frame at 60 Hz. The log does not tell if this cost is in each frame or only in the frames of a save.

The pictures of the mod page (bwvr-handcut-left-*, bwvr-handcut-right-*):
- 00301 and 00331 (0 cm, Water): a band of holes in the shape of a forearm goes from the top edge to the bottom edge of the page, between the cell Water and the cell Food. It cuts the right half of the ring of Water and the end of the word "Water" below the cell. The band of the right eye is a little apart from the band of the left eye and a little wider. The pick mark (a small ring with a dot) is in the cell Water, outside the hole.
- 00361 and 00391 (5 cm, Food): a wider band, the forearm with the wrist, goes from the top edge down through the right half of the cell Food. It cuts the title to "F" and the word "Food" to "Foo". The two eyes are a little apart. The pick mark is in the cell Food.
- 00421 (-5 cm, Heal): only a thin wedge at the top edge of the page, over the top right of the ring of Heal. The rest of the page is whole. The pick mark is in the cell Heal. So a part of the hand or of the wrist stays in front of the page.
- 00451 (the pull): the title is "Miracles", so no item is chosen. A hole in the shape of the hand is at the right edge of the page, over the cell Heal and the top right corner. The right eye hole is much larger than the left eye hole.
- bwvr-book-00271 to 00493: the whole page, no holes. The chosen item has its orange ring. These pictures do not show the pick mark. Only the bwvr-handcut pictures show it.

The eye pictures as the headset composes them (bwvr-vk-video-N.bmp, the left eye with the book over it):
- 00323 (0 cm): YES, the pointing hand shows over the page. The band between Water and Food shows the arm (the Hand of the game is drawn half clear, so the land shows through the arm). In sbH0 at the same frame the page is whole over the arm. The hole fits the arm. I see no wide band of plain land beside it.
- 00373 (5 cm): YES, the hand shows over the page in a band through the right half of the cell Food. The hole fits the arm and the wrist. The hand under the page (the fingers) also shows below the bottom edge.
- 00423 (-5 cm): YES, the page covers the hand. The fingers show only below the bottom edge of the page, as in sbH0. A thin wedge at the top edge, over the top right of the ring of Heal, shows a little of the arm and of the land (the 3287 px of the log).
- 00473 (the pull): YES, the hand shows over the page at the right edge, over the cell Heal and the top right corner. The page in sbH0 covers it there.
- The orbs and the words: the orbs of Water, Food and Heal are in their cells and the words are right outside the holes. Where a hole is, the orb and the word are cut, as the hand is in front of them. The bubble "Hold and pull out ..." and the bubble "Put y..." show as in sbH0.

What only a headset can show: the depth of the hand and the page in the two eyes, and if the hole and the hand agree in each eye.

## 2. sbH0 (card_hand_front=0): PASS

- "hand cut     : card_hand_front=0: the page of the spell book and of the ring is over the hands". This is the only "hand cut" line. No line of a frame and no line of a window.
- The same steps as sbH1: "spell book   : test: the word "book" steps the pointing hand to item 0 (Water) of the book, 0 cm in front of the page, frame 300", item 1 (Food) "5 cm in front of the page, frame 350", item 2 (Heal) "-5 cm in front of the page, frame 400", and "controllers  : the fake hand steps 15 cm toward the head over 10 frames at frame 440".
- No bwvr-handcut picture is saved.
- bwvr-vk-video-00323, 00373, 00423, 00473: the page covers the hand in each shot. The page is whole, with all of its orbs and words. The arm shows only outside the edges of the page. The rest of the picture is the same as in sbH1 at the same frame.

## 3. Job 84 run 1 (-Kind unlock): the hand in front of the page of the ring

See job84.md, run 1.
