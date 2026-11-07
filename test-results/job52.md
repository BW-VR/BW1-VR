# Job 52: the hints with the 3D controllers (agent/hlahints 56aa635)

`.\dev.ps1 build -Clean`: "The math tests failed: 5967 checks, 1 failed": "FAIL the cards face the head (99 percent and more of the cards are within 18 degrees of the head) (got 6739.0000, want 7531.0000)". The DLL was built, so the runs went on. Each log has "build 56aa635" and 2760x2208. Save pen, `backend=dxwrapper\ddraw.dll;test_frame_rate=60;prompts=2;hint_learn=0;hint_card_save_test=1`. All 14 runs are RUN DONE.

- hla-one: "hint_style=2 ... The models are in C:\wt-hlahints\bwvr\assets\controllers\", "the right hand reports the profile "fake" and the headset "", so the model is meta-quest-touch-plus", "the model meta-quest-touch-plus of the right hand loaded in 6.2 ms on a thread: 31 nodes, 6 meshes, 3340 vertices, 4470 triangles, a box of 6.8 by 6.0 by 12.3 cm. The controls with a node: first face button, second face button, stick click, trigger, grip, stick", "the controller meta-quest-touch-plus shows in the right hand, with 1 cards", "the first controller is drawn in an eye picture, 8980 triangles". Cards: "frame 178, the controller of the right hand has 1 cards: Teleport (first face button, 6.3 by 3.0 cm, at 0.095 0.006 -0.987 m)" and "the controller of the left hand has 4 cards: Move the land (grip ...); Move (stick ...); Ring menu (second face button ...); Spells (first face button ...)". No "a draw of a controller failed".
- FAULT of the cost line: each window has "The cost: 0.000 ms a frame, 0.000 ms at most, 0 triangles a frame, over 0 frames", while the same line says "the controller showed in 239 frames of the right hand". So the cost is not measured.
- The eye pictures of the shots (log frames 478 to 1133): the right hand points far, and the Hand is about 1 m away on the island. No controller shows in them, at the Hand or below it. They look the same as hla-style1. The real controllers are below the view in this recipe.
- The card files bwvr-hintcard-right-button0-NNNNN.bmp: a small scroll (189 by 119 px) with one word, for example "Plant", with no seal.
- hla-style1: "hint_style=1, the hints are on the cards at the wrists", no "shows in the ... hand" line, and "the card of the right hand: [A] Teleport" and "[Grip] Take".
- The models (hla-<id>, `hint_model_test`): each has its load line, and the cost line is 0 as above.

| Model | Load | Triangles a hand | Box (cm) | Controls with a node |
|---|---|---|---|---|
| valve-index | 9.1 ms | 8933 | 12.4 x 8.2 x 18.1 | first and second face button, stick click, trigger, grip (no mesh), stick, pad |
| htc-vive | 8.4 ms | 17344 | 11.7 x 8.6 x 21.8 | stick click, menu button, trigger, grip, stick, pad |
| htc-vive-cosmos | 7.0 ms | 18476 | 11.0 x 12.2 x 15.3 | face buttons, stick click, trigger, grip, stick |
| htc-vive-focus-3 | 6.5 ms | 4619 | 10.0 x 12.4 x 16.3 | face buttons, stick click, trigger, grip, stick |
| hp-mixed-reality | 7.3 ms | 6716 | 11.9 x 11.4 x 17.3 | face buttons, stick click, trigger, grip, stick |
| microsoft-mixed-reality | 6.9 ms | 6006 | 11.6 x 10.9 x 18.6 | stick click, trigger, grip, stick, pad |
| pico-4 | 6.3 ms | 4525 | 7.4 x 13.4 x 13.2 | face buttons, stick click, trigger, grip, stick |
| oculus-touch-v3 | 8.2 ms | 5831 | 9.2 x 10.9 x 13.2 | face buttons, stick click, trigger, grip, stick |
| oculus-touch-v2 | 8.3 ms | 3335 | 9.2 x 11.1 x 12.5 | face buttons, stick click, trigger, grip, stick |
| meta-quest-touch-pro | 6.2 ms | 4106 | 6.8 x 6.4 x 12.6 | face buttons, stick click, trigger, grip, stick |
| generic-trigger | 6.5 ms | 3814 | 6.5 x 5.6 x 13.2 | trigger |

  No controller shows in their pictures either (the same view as hla-one), so the shapes, the Index diamond and the generic trigger cannot be judged from these runs.
- hla-left (`-Kind left`): "the left hand reports the profile "fake" ...", "the model meta-quest-touch-plus of the left hand loaded in 0.5 ms", "shows in the left hand, with 1 cards", "frame 72, the controller of the left hand has 1 cards: Take (grip, 4.5 by 3.0 cm, at -0.246 -0.304 -0.410 m)", and "the first controller is drawn in an eye picture, 4478 triangles". At frame 433: "the controller of the left hand has 2 cards: [Menu] Game menu (no button ...); Teleport (first face button ...)" and "the controller of the right hand has 4 cards". The pictures of frames 103 and 433 show the controller: a faint glass-like body, pale, with the lit parts in solid gold (the grip at 103, the grip, the stick and two face buttons at 433), and thin dark lines out from them. The lines end in empty space: no card is drawn at their ends in the eye pictures. At 103 the controller is in front of the left hand. At 433 the controller lies on its side, near the middle right of the view, and four lines go to the right.

The pictures stay on the laptop: C:\BW1-VR\local\shots\job52 (left-00103-crop.png, left-00433-crop.png, left-00433-wide.png, hand-crop.png, models-hand-crop.png, button-card.png).