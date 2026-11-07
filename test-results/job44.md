# Job 44: the canted eyes (agent/canted dff00c9) and the smooth turn (agent/smoothturn 0aed745)

Each branch: `.\dev.ps1 build -Clean` (5948 and 5951 checks, 0 failed). Each log has the build of its branch and "game renders at 2760x2208". All runs: `hand_run.ps1 -Kind fixed -Seconds 100`, `backend=dxwrapper\ddraw.dll;test_frame_rate=60`, test save pen. The kind sets `fake_controller_play_frame=4600`, so the shots at the steps 4630 to 4800 are the log frames 103 to 273. `-Seconds 100` is necessary: with no `-Seconds` the run stops after about 450 frames.

## The canted eyes (`fake_head_pitch=-25`)

- cant1 (`fake_eye_cant_deg=10;eye_cant=1`): PROVEN. "each view of the invented head turns 10.0 degrees outward from the head (fake_eye_cant_deg)", "the view of eye 0 turns 10.00 degrees from the head (yaw 10.00 to the left ...), and the view of eye 1 10.00 degrees (yaw -10.00 ...)", and "eye_cant=1. The views are canted. Each eye pass turns its camera by the turn of its view, and the scene has a walk for each eye (no one walk)". The log has no line "frames in one walk". With two walks the pictures are bwvr-eye-left/right-NNNNN.bmp, not bwvr-shared. Against cantnone at frame 173 the left picture moves about 160 px to the right (the view turns left), and the right picture about 175 px to the left (the view turns right). At the outer edge of each eye the land and the hills are whole.
- cant0 (`eye_cant=0`): PROVEN. "eye_cant=0. The views are canted, but both eye passes draw from the orientation of the view of eye 0" and "450 of 449 frames in one walk". Both pictures move about 160 to 190 px to the right, as the left eye of cant1.
- cantnone (`fake_eye_cant_deg=0;eye_cant=1`): PROVEN. "eye_cant=1. The views are not canted, so both eye passes draw from the orientation of the head" and "450 of 449 frames in one walk".
- Possible fault, also in cantnone: with one walk the right eye has a vertical cut at its right edge, where the near hill stops and a far hill shows. cant1, with a walk for each eye, has no cut there.
- cant1 took 616 s: the "test now" flag had run out, so it waited 10 minutes for an idle PC.

## The smooth turn (`-Points 16,-30,50`, `stick_right@4650+60`)

- st-smooth-full (`snap_turn=4;comfort_vignette=1`): PROVEN IN PART. "the smooth turn starts to the right, 100% of a full push, at 90 degrees a second", 60 lines "smooth turn frame N of the hold", "the smooth turn stops ... It turned 73.02 degrees right in 0.811 s of 60 frames (90.0 degrees a second), heading 56.14 to 129.16 degrees", and "turn check, 30 frames after the stop the heading is 129.163 degrees, and it was 129.163 degrees at the stop". The vignette: "the edges of the view go dark (gentle), the smooth turn, frame 122" and "clear again after 1.11 s". Pictures: frame 153 is mid-turn with dark edges, and 213 is equal to 273. Possible fault: the hold is 60 frames at 60 frames a second, 1.0 s, but each frame of the turn counts only 8 to 9 ms ("0.738 degrees in 0.0082 s"), so the turn is 73 degrees, not 90.
- st-smooth-half (`fake_controller_stick_pct=50`): "20% of a full push", 0.15 to 0.16 degrees a frame, "turned 13.72 degrees right in 0.758 s of 60 frames (18.1 degrees a second)", and the turn check gives the same heading (69.863). The same short time of a frame.
- st-snap45 (`snap_turn=2`): PROVEN. One line "turned right by 45 degrees about your head, heading now 101.1 degrees", and no smooth turn lines. The pictures turn 45 degrees.

Pictures: C:\BW1-VR\local\shots\job44\cant-sheet.png and smoothturn-sheet.png.