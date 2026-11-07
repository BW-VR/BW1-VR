# Job 53: the hints with the 3D controllers, the hints recipe (agent/hlahints 985c6dc)

`.\dev.ps1 build -Clean`: 5967 checks, 0 failed. Each log has "build 985c6dc" and 2760x2208. `hand_run.ps1 -Kind hints`, save pen, `prompts=2;hint_learn=0;hint_card_save_test=1`. All 14 runs are RUN DONE.

- hla-hints (Direct3D 9): "the model meta-quest-touch-plus of the right hand loaded in 0.4 ms ... 4470 triangles ... The controls with a node: first face button, second face button, stick click, trigger, grip, stick", "shows in the right hand, with 1 cards", "shows in the left hand, with 4 cards", "frame 190, the controller of the right hand has 1 cards: Teleport (first face button ...)" and "the controller of the left hand has 4 cards: Move the land (grip ...); Move (stick ...); Ring menu (second face button ...); Spells (first face button ...)". There is no menu card. The cost: "the controllers cost 0.386 ms a frame on the CPU over 120 frames (the build and 1.0 eye passes a frame), 0.569 ms at most, 12623 triangles a frame", and the windows have 0.339 and 0.369 ms. That is over 0.3 ms, with 1.0 eye pass a frame, not 2.0 (one walk).
- Pictures (log frames 113 to 473): 113 has the hands and no controller. From 233 both controllers are in the hands at each side, low: a warm see-through parchment body with a thin dark ink edge. The left has the grip, the stick, Y and X lit in gold, and the right has A. Each lit control has a dark dot and a thin ink stroke. In this model the strokes go INWARD, toward the middle of the view, not outward. At 393 the left grip is in its pressed place, and the right hand is a hand again with no controller.
- hla-video (Vulkan, video_view=1): "video view : the first card of a button is in the video for the first time, frame 229". The cost is 0.235, 0.203 and 0.234 ms (1.0 eye pass). The video pictures show the controllers, the strokes and the cards together. The cards are small parchment scrolls with clear dark words: "Move", "Ring menu", "Spells", "Move the land" and "Teleport". The cards are square to the view, they do not overlap, and the strokes do not cross. Each stroke ends at the middle of the near edge of its card. But the cards are on the INNER side, between the hands (the left cards to the right of the left controller, the Teleport card to the left of the right controller), and they make a slanted line, not a column. The top card "Move" is cut by the top edge of the video. There is no menu line.
- hla-hints1 (hint_style=1): no "shows" line, the old wrist cards "[A] Teleport" and "[Grip] Move the land, [Stick] Move, [Y] Ring menu, [X] Spells", and no controller.
- The models (the cost of the last window, ms a frame, at most, and triangles a frame for both hands):

| Model | Cost | At most | Triangles |
|---|---|---|---|
| htc-vive | 1.632 | 1.969 | 52731 |
| htc-vive-cosmos | 1.531 | 4.789 | 52509 |
| oculus-touch-v3 | 1.067 | 3.073 | 16605 |
| oculus-touch-v2 | 0.887 | 1.059 | 9629 |
| valve-index | 0.777 | 1.252 | 24782 |
| hp-mixed-reality | 0.519 | 0.726 | 18043 |
| pico-4 | 0.435 | 3.989 | 12255 |
| microsoft-mixed-reality | 0.401 | 0.560 | 16374 |
| htc-vive-focus-3 | 0.400 | 0.516 | 13321 |
| meta-quest-touch-pro | 0.360 | 0.579 | 12259 |
| meta-quest-touch-plus (hla-hints) | 0.369 | 0.546 | 12623 |
| generic-trigger | 0.261 | 0.610 | 11805 |

  The shapes read well: the Index with its ring and a small gold square at the side of the handle (the grip), the Vive with the large pad lit gold (the stick lines) and no lit Y, the Cosmos, HP and Pico with their rings. generic-trigger has nothing lit and no strokes, because no hint of this recipe is on the trigger. In these models the strokes go OUTWARD (to the left from the left hand).

The pictures stay on the laptop: C:\BW1-VR\local\shots\job53 (hints-crop.png, video-313-crop.png, video-sheet.png, models-crop.png).