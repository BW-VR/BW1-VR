# Job 61: the hand during hints and the view check (agent/handhint b4ca17e)

`.\dev.ps1 build -Clean`: 6102 checks, 0 failed. Each log has "build b4ca17e" and "game renders at 2760x2208". `hand_run.ps1 -Kind hints`. All four runs are RUN DONE.

## The keys

- hh-hints-dx, hh-hints-vk, hh-hints-cant: "hint model : hint_hand_opacity_pct=68, the hand is at 68 percent of its opacity while its controller shows. hint_pulse=1, the lit buttons pulse from dim to full light, one beat a second".
- hh-hints-dx-old: "hint_hand_opacity_pct=35 ... hint_pulse=0, the lit buttons have a steady light".
- Each run: "view check : view_check=1. In the God view the mod compares the place where it draws the grip point of each hand in each eye picture with the place that the declared views give it".

## The view check

| Run | The first check (frame 42, left hand) | Grip from the head | Pixel gaps | Depth and side | Summaries (818 and 900 points) |
|---|---|---|---|---|---|
| hh-hints-dx (dxwrapper) | 1017.42 1373.82 and 872.04 1373.82, the same as declared | 50.2 cm | 0.00, 0.00 | 0.00 cm, 0.00 cm | worst 0.00 cm, worst pixel gap 0.00 |
| hh-hints-dx-old | the same | 50.2 cm | 0.00, 0.00 | 0.00, 0.00 | worst 0.00 cm, 0.00 px |
| hh-hints-vk (Vulkan, head yaw 10, roll 12, 5 cm right, 8 cm back) | 1171.11 1294.84 and 1053.74 1294.84, declared 1171.11 1294.85 and 1053.74 1294.85 | 58.1 cm | 0.01, 0.01 | 0.00, 0.00 | worst 0.00 cm, 0.01 px |
| hh-hints-cant (eye cant 10 degrees) | 1199.73 1360.98 and 632.36 1405.97, the same as declared | 50.2 cm | 0.00, 0.01 | 0.00, 0.00 | worst 0.00 cm, 0.01 px |

The declared eyes are 66.8 mm apart in each run. Each run is well within the limits (under 1 px and under 0.1 cm).

## The pictures (left eye, the right hand)

- With the controller shown (log frames 233, 313, 473), the hand at 68 percent is clearly more solid: the fingers and the palm show behind the controller. At 35 percent the hand is faint and is hard to see behind the controller.
- The lit button. The brightest 40 gold pixels of the button (R plus G, of 510):
  - hh-hints-dx: 328 at 233, 367 at 313, 328 at 473. So the light changes from shot to shot.
  - hh-hints-dx-old: 431 at each of the three shots, a steady light.
  - The full light of the pulse (367) is dimmer than the steady light of the old way (431).

The pictures stay on the laptop: C:\wt-handhint\bwvr\tools\desk\desk-hh-*.
