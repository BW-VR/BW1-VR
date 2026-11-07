# Job 57: the ink outline of the simpler body (agent/bodyfine fa0d8c8)

`.\dev.ps1 build -Clean`: 6077 checks, 0 failed. Each log has "build fa0d8c8". `hand_run.ps1 -Kind hints`, save pen, `backend=dxwrapper\ddraw.dll;test_frame_rate=60;prompts=2;hint_learn=0;hint_card_save_test=1;hint_model_test=<id>`, and `hint_body_fine=0` for the bf0 runs. All 10 runs are RUN DONE, in new desk folders.

## The load lines (the right hand)

| Model | bf1 body | bf0 body |
|---|---|---|
| htc-vive | 6853 triangles, cells of 3.4 mm, hint_body_fine | 1930 triangles, cells of 7.4 mm |
| htc-vive-cosmos | 6934, 3.4 mm, hint_body_fine | 1702, 7.4 mm |
| oculus-touch-v3 | 3104, 3.4 mm, hint_body_fine | 2411, 4.4 mm |
| valve-index | 4333, 3.4 mm, hint_body_fine | 2130, 5.7 mm |
| meta-quest-touch-plus | 2345, 2.0 mm, hint_body_fine | 2221, 2.0 mm |

The cells are as expected for each run. The fine body loads in 7 to 14 ms on the thread (the old body in 1 to 6 ms).

## The cost

The cost line of the first 120 frames (1.0 eye pass a frame), and the line of the last window of 450 frames:

| Run | First 120 frames | At most | Sent a frame | Last window |
|---|---|---|---|---|
| bf1-htc-vive | 0.624 ms | 1.148 | 19858 tris, 19543 vertices | 0.587 ms |
| bf0-htc-vive | 0.386 ms | 1.639 | 10012, 12492 | 0.360 ms |
| bf1-htc-vive-cosmos | 0.434 ms | 0.612 | 18595, 10723 | 0.414 ms |
| bf0-htc-vive-cosmos | 0.226 ms | 0.343 | 8270, 4913 | 0.206 ms |
| bf1-oculus-touch-v3 | 0.250 ms | 0.324 | 9310, 6243 | 0.239 ms |
| bf0-oculus-touch-v3 | 0.230 ms | 0.297 | 7908, 5102 | 0.206 ms |
| bf1-valve-index | 0.319 ms | 0.441 | 12275, 8461 | 0.305 ms |
| bf0-valve-index | 0.205 ms | 0.257 | 7871, 4882 | 0.196 ms |
| bf1-meta-quest-touch-plus | 0.216 ms | 0.452 | 7414, 5402 | 0.205 ms |
| bf0-meta-quest-touch-plus | 0.210 ms | 2.021 | 7168, 4530 | 0.183 ms |

The Touch Plus is under 0.3 ms and the Cosmos is under 0.6 ms. The Vive with fine=1 is at the limit: 0.624 ms in the first 120 frames and 0.587 ms in the last window. Fine=1 doubles the triangles of the Vive and of the Cosmos.

## The outline (the right controller, eye pictures of frames 233 and 313)

The pose is still, so frames 233 and 313 are the same picture for each run.

- htc-vive: with fine=0, the right edge of the handle has dark triangle blobs and a gap at the middle, and the left end of the ring has a jagged edge. With fine=1, the outline of the handle is one continuous line. The left end of the ring is smoother but still has some small jagged specks.
- htc-vive-cosmos: with fine=0, the lower edge of the ring has gaps and the line under the ring breaks. With fine=1, the outline of the ring is continuous all around. A few short dark dashes show inside the body, under the ring near the trigger.
- oculus-touch-v3: with fine=0, the lower edge of the ring breaks into dark blobs and patches. With fine=1, the outline of the ring is a continuous line with no blobs. This is the clearest gain.
- valve-index: with fine=0, the inner edge of the strap and the bottom of the strap have a saw-tooth of dark blobs. With fine=1, the outline of the strap is continuous. But fine=1 shows many short dark dashes inside the faint body, along the handle (the ink specks of the job). They are thin and do not hide the shape, but they are visible.
- meta-quest-touch-plus: the same at 1 and at 0. The outline is clean and even.

## The crops

The crops are not pushed. The body of the controller is faint, so each crop shows the land of the game through the body and around it. They stay on the laptop: C:\BW1-VR\local\shots\job57 (p-1-htc-vive.png to p-5-meta.png, bf0 on the left and bf1 on the right, 440 by 600 pixels of the left eye).
