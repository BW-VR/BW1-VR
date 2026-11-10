# Job 73: the switch hint_controller (agent/launcher 7f34c16)

`.\dev.ps1 build -Clean`: 6291 checks, 0 failed. Each log has "build 7f34c16" and "vr           : game renders at 2760x2208". `hand_run.ps1 -Kind hints`, test save pen, Vulkan. lc1 and lc2 are RUN DONE (32 s and 31 s).

## lc1 (hint_controller=0): PASS

- "hint model   : hint_controller=0, hint_style=1, the hints are on the cards at the wrists. The models are in C:\wt-launcher\bwvr\assets\controllers\".
- "hint card    : hint_card_pictures=0, the lines have the button words only".
- "hint card    : 6 new pictures of the cards in the scroll look, 1.78 ms each on average and 2.90 ms at the most. 0 new pictures of buttons (hint_card_pictures=0)".
- No line "the controller ... shows in the ... hand", and no line of the cost of a drawn controller.
- The models of the two hands load also in this run: "the model meta-quest-touch-plus of the left hand loaded in 9.3 ms on a thread" and "of the right hand loaded in 5.5 ms".

## lc2 (the defaults): PASS

- "hint model   : hint_controller=1, hint_style=2, while a hand has a hint, the model of its controller shows in the hand with the buttons of the hint lit, and each lit button has its own card".
- "hint card    : hint_card_pictures=1, a line of a button has a small picture of the controller with that button lit, before the button word".
- "hint model   : the model meta-quest-touch-plus of the left hand loaded in 1.2 ms on a thread: 31 nodes, 6 meshes, 2463 vertices ..." and "of the right hand loaded in 1.1 ms on a thread: 31 nodes, 6 meshes, 2461 vertices ...".
- "the controller meta-quest-touch-plus shows in the right hand, with 1 cards", "... in the left hand, with 4 cards", "the first controller is drawn in an eye picture, 7414 triangles".
- "the controllers cost 0.160 ms a frame on the CPU over 120 frames ..., 0.309 ms at most".
- The card summary: "9 new pictures of the cards in the scroll look, 0.78 ms each on average and 1.35 ms at the most. 0 new pictures of buttons (hint_card_pictures=1)".

## Notes

- With hint_controller=0 the mod loads the two models but draws no controller.
- With hint_card_pictures=1 the summary gives 0 new pictures of buttons. With hint_style=2 each lit button has its own card, so possibly no line uses a picture of a button. This run does not show which.
