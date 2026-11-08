# Job 62: the clear edge and the mip levels of the cards (agent/crisp 1138e62)

`.\dev.ps1 build -Clean`: 6103 checks, 0 failed. Each log has "build 1138e62" and "game renders at 2760x2208". Vulkan (the default). All four runs are RUN DONE, with no crash, no fault and no WRITE FAILED line.

## The card send lines

Keys at 1 (crispH1, crispS1): each picture has "4 level(s), a clear edge" and "0 straight texels". Keys at 0 (crispH0, crispS0): each picture has "1 level(s), no clear edge", the old and new numbers are equal, and "made ready in 0.00 ms".

The shimmer at the far distance (0.60 m for a hint card, 1.00 m for a panel), the old way, now, and the ideal sampler, with keys at 1:

| Picture | Size | Ready in | Old | Now | Ideal | Edge numbers |
|---|---|---|---|---|---|---|
| H1 1, hint card of the pointing hand | 252 x 119 | 0.04 ms | 6.88 | 4.93 | 4.99 | 0.00 and 0.00 |
| H1 2, hint card of the other hand | 359 x 119 | 0.04 | 7.38 | 4.89 | 4.96 | 0.00, 0.00 |
| H1 3, first card of a button | 193 x 119 | 0.03 | 6.80 | 5.36 | 5.43 | 0.00, 0.00 |
| H1 4, second card of a button | 300 x 119 | 0.04 | 6.77 | 4.81 | 4.79 | 0.00, 0.00 |
| H1 5, third card of a button | 200 x 119 | 0.03 | 6.85 | 5.12 | 5.22 | 0.00, 0.00 |
| H1 6, hint card of the other hand | 479 x 119 | 0.06 | 7.05 | 5.10 | 5.26 | 0.00, 0.00 |
| H1 7, first card of a button | 359 x 119 | 0.04 | 7.35 | 5.10 | 5.18 | 0.00, 0.00 |
| H1 8, hint card of the other hand | 359 x 119 | 0.04 | 7.38 | 4.89 | 4.96 | 0.15 and 0.00 |
| S1 6, "the ring" (the welcome page) | 1024 x 640 | 0.49 | 3.62 | 3.11 | 3.15 | 0.00, 0.00 |
| S1 7, hint card of the pointing hand | 202 x 119 | 0.03 | 7.22 | 5.02 | 5.12 | 0.05 and 0.00 |
| S1 8, "the ring" (the welcome page) | 1024 x 640 | 0.53 | 3.51 | 3.11 | 3.16 | 0.00, 0.00 |
| S1 9, "the ring" (the settings window) | 1024 x 1024 | 0.88 | 2.92 | 2.98 | 2.98 | 0.00, 0.00 |
| S1 10, "the ring" (the settings window) | 1024 x 1024 | 0.98 | 3.53 | 3.28 | 3.31 | 0.00, 0.00 |
| S1 11, "the ring" (the settings window) | 1024 x 1024 | 1.07 | 2.93 | 2.97 | 2.97 | 0.00, 0.00 |
| S1 12, "the ring" (the Controls page) | 1024 x 1024 | 1.05 | 3.71 | 3.55 | 3.59 | 0.00, 0.00 |

S1 pictures 1 to 5 are the same as H1 1 to 5. At the near distance the new way is also lower than the old way for each hint card. At 0.60 m for the panels it is about equal (2.69 against 2.66, 2.91 against 2.76 for picture 12).

Notes:
- For the pictures S1 9 and S1 11 (the settings window, 1024 x 1024), the shimmer at the far distance is a little higher now (2.98 against 2.92, 2.97 against 2.93), and the ideal number is the same as now. With keys at 0 the ideal number of these pictures is 2.69, lower than both.
- The log names the pictures S1 6 and 8 to 12 "the ring", but they are the welcome page and the VR Settings window.
- "made ready in" is 0.03 to 0.06 ms for a hint card and 0.49 to 1.07 ms for a panel.
- In the keys-at-0 runs, the picture of the hint card "of the other hand" (359 x 119) and the hint card of the pointing hand (202 x 119) also show an old edge number of 0.28/0.15 and 0.20/0.05, the same as with keys at 1.

## The pictures

All the card pictures are mod art (the parchment cards of the mod with the Alegreya font of bwvr-fonts, the welcome page, the VR Settings window). The PNG files are in test-results/job62. To keep the repo small, it has each hint card picture of the four runs, and of the panels only picture 06 (the welcome page) and 12 (the Controls page): the levels and the views of crispS1, and the views of crispS0. The other panel pictures stay on the laptop in C:\wt-crisp\bwvr\tools\desk\desk-crispS1 and desk-crispS0.

- The levels pictures show the card and three smaller copies, each a clean half of the one before, with a clear frame on the checker background.
- The views pictures: in both columns the text is steady between the rows. No line shows along the card edge in either column at this size. In the far rows the new way (right) is a little softer than the old way.
