# Job 55: the hints again (agent/hlahints 8425eaf)

`.\dev.ps1 build -Clean`: 5970 checks, 0 failed. Each log has "build 8425eaf". `hand_run.ps1 -Kind hints`, save pen, `prompts=2;hint_learn=0;hint_card_save_test=1`, 60 frames a second. All 6 runs are RUN DONE, in new desk folders.

The load and the cost (the first 120 frames, at most, sent a frame, the last window; 1.0 eye pass a frame):

| Run | Load line | First 120 frames | At most | Sent a frame | Last window |
|---|---|---|---|---|---|
| hla-hints (Touch Plus, D3D9) | 1927 vertices (3340 in the file), 3528 triangles, the body 2221 (3163 in the file, cells of 2.0 mm) | 0.223 ms | 0.317 | 7168 tris, 4530 vertices | 0.200 ms |
| hla-video (Touch Plus, Vulkan) | the same | 0.151 ms | 0.240 | 7168, 4530 | 0.149 ms |
| htc-vive | 5894 vertices (17551), 4986 triangles, the body 1930 (14288, cells of 7.4 mm) | 0.421 ms | 0.798 | 10012, 12492 | 0.399 ms |
| htc-vive-cosmos | 2295 (14838), 4438, the body 1702 (15740, 7.4 mm) | 0.231 ms | 0.360 | 8270, 4913 | 0.220 ms |
| oculus-touch-v3 | 2074 (12111), 3901, the body 2411 (4341, 4.4 mm) | 0.241 ms | 0.313 | 7908, 5102 | 0.220 ms |
| valve-index | 2045 (7507), 3850, the body 2130 (7213, 5.7 mm) | 0.235 ms | 0.391 | 7871, 4882 | 0.210 ms |

So the Touch Plus is under 0.3 ms, and each model is under 0.6 ms. The Vive is the heaviest at 0.40 ms.

- hla-video (vk-video 313): PASS. The cards are level (not turned). The four cards of the left hand ("Move", "Ring menu", "Spells", "Move the land") are a vertical column on the outer side of the left controller, with their right edges in one line, from the top of the controller to its lower part. "Teleport" is to the right of the right controller, at the height of its face buttons. Each stroke ends at the inner edge of its card. No crossings and no overlaps. The words are clear.
- hla-hints: the strokes are level and outward on both hands.
- The look of the models (eye pictures of frame 313): the Touch Plus is clean, with an even ink outline. The simpler bodies of the other models show breaks: the Vive has a jagged outline at the top of its ring and dark patches inside the body, the Cosmos and the Touch v3 have broken dark spots along the edge of the ring, and the strap of the Index breaks into short dark dashes. The shapes still read as their controllers, and the lit parts (the Vive pad, the Index gold square at the handle) are clear.

The pictures stay on the laptop: C:\BW1-VR\local\shots\job55 (video-313.png, models-crop.png).