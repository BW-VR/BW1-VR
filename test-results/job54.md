# Job 54: the hints again (agent/hlahints 8a2f548)

`.\dev.ps1 build -Clean`: 5968 checks, 0 failed. Each log has "build 8a2f548". `hand_run.ps1 -Kind hints`, save pen, `prompts=2;hint_learn=0;hint_card_save_test=1`, 60 frames a second. All 6 runs are RUN DONE. Note: the desk folders of the model runs also hold the old pictures 478 to 1133 of job 52.

The cost (the first line "the controllers cost ... over 120 frames", 1.0 eye pass a frame, and the last window):

| Run | Load | Vertices (in the file) | First 120 frames | At most | Sent a frame | Last window |
|---|---|---|---|---|---|---|
| hla-hints (Touch Plus) | 0.7 ms | 3247 (3340) | 0.353 ms | 0.491 | 12620 tris, 13351 vertices | 0.331 ms (2.592 at most) |
| hla-video (Touch Plus, Vulkan) | 0.8 ms | 3247 (3340) | 0.239 ms | 0.412 | 12620, 13351 | 0.229 ms |
| htc-vive | 3.4 ms | 15190 (17551) | 1.418 ms | 2.188 | 52738, 63040 | 1.391 ms |
| htc-vive-cosmos | 3.6 ms | 10001 (14838) | 1.159 ms | 1.750 | 52412, 43062 | 1.128 ms |
| oculus-touch-v3 | 2.0 ms | 4048 (12111) | 0.433 ms | 0.624 | 16609, 16566 | 0.402 ms |
| valve-index | 5.1 ms | 6956 (7507) | 0.697 ms | 2.711 | 24811, 29180 | 0.632 ms |

So the Touch Plus is 0.33 to 0.35 ms with Direct3D 9 (over 0.3) and 0.23 ms with Vulkan. The Vive is 1.4 ms and the Cosmos 1.1 ms, over 0.6.

- The strokes: PROVEN. In hla-hints and hla-valve-index the strokes of the left hand go to the left, and the stroke of the right hand goes to the right (outward).
- hla-video: the cards of each hand are on its outer side (the four left cards to the left of the left controller, Teleport to the right of the right controller). The top card "Move" is not cut. The strokes end at the inner edges, with no crossings and no overlaps. But each card is turned about 15 to 20 degrees (the left cards lean up to the right, Teleport leans down to the right), and the left column goes down and outward, to below the controller: "Move the land" is about one controller length lower than the controller. So it is a slanted column, not a vertical one at the height of the controller.

The pictures stay on the laptop: C:\BW1-VR\local\shots\job54 (hints-crop.png, video-sheet.png, video-313-crop.png).