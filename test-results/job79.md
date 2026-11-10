# Job 79: the round soft glow at the end of the menu ray (agent/raytip c66a07d)

`.\dev.ps1 build -Clean`: 6479 checks, 0 failed. Each log has "build c66a07d" and "vr           : game renders at 2760x2208". All runs are RUN DONE. The run names are rt1, rt2, rt3b and rt4b. rt3 and rt4 are the runs of the recipe as sent (see run 3).

## The openxr lines (runs 1 and 2): NOT SHOWN

A desk run has no headset session: "subtitles    : no subtitle card (no headset)", "menu room    : frame 1, the menus stay in the eye pictures: no headset". So no "openxr: the card of the end of the ray has its own swapchain" line and no "goes out as one quad layer" line is in a log, and no "the runtime refused the quad layer of the glow" line. Only a headset can show these lines.

## 1. rt1, village_run -Kind rayover (ray_tip=1): PASS

- "overlay      : ray_tip=1: the end of the menu ray is a round soft glow that faces the eye, with no outline, in the eye pictures and over the panels".
- "ray card     : the ray goes over the panels as a quad layer, frame 317: from 0.000 0.000 -1.000 to -0.277 -0.209 -2.970 m (2.00 m). It meets no panel, so it is 2 m long. ... The look: the glow alone, and a round glow 7.8 cm across at the end, 2.99 m from the head, on a quad layer of its own that faces the head. The eye pictures have no ray".
- "ray card     : the end of the ray over the panels moves, frame 651: ... (0.61 m). It ends on the panel, and the glow is there. ... The look: the glow alone, and a round glow 2.6 cm across at the end, 1.01 m from the head, on a quad layer of its own that faces the head."
- card_send_test_save=0 does not stop these dumps: ray_card_test_save=3 of the recipe gives them. The run has bwvr-raycard-00317 to -00319 (32x1024) and bwvr-raytip-00317 (64x64).
- bwvr-raytip-00317: a round soft glow over the checkerboard. It is white in the middle (255 255 255 at 6 pixels from the middle, 255 255 234 in the middle), pale gold farther out (255 255 205 at 12 pixels, 211 191 151 at 22 pixels), and it fades into the checkerboard. No dark ring. The four corners are the bare checkerboard (96 96 96).
- bwvr-raycard-00317: the strip is a soft white to gold core with a fade to the sides. Its top end has no disc and no ink: the core goes to the edge of the picture. The bottom end fades out.

## 2. rt2, ray_tip=0: PASS

- "overlay      : ray_tip=0: the end of the menu ray is a small cross in the eye pictures, and a disc in a ring of dark ink over the panels".
- "ray card     : ... frame 318: ... The look: the glow alone, and a mark of 1.6 cm at the end. The eye pictures have no ray". At frame 651: "It ends on the panel, and the mark is there. ... and a mark of 1.6 cm at the end".
- bwvr-raycard-00318: at the top end of the strip a pale gold disc with a ring of dark ink. The rest of the strip is the same as in rt1. The run has no bwvr-raytip picture.

## 3. temple_run (ray_tip=1): the recipe as sent FAILS, rt3b with ring_pages=0 PASSES

temple_run.ps1 picks "enter" in slot 7 of ring page 1. On this branch ring_pages=1, and page 1 is "leash, places_page, recenter, save, more, settings, names, pause". So in rt3 the press chose "pause": "ring         : the player chose "pause": the key P, 1 time, frame 121". The game paused on the land, and the run did not go into the Temple. In rt4 the press of room_save then chose the ring item "save": "ring         : the player chose "save": the key Ctrl and scan code 0x1F, 1 time, frame 2701". desk_run puts the profile back after each run. temple_run.ps1 on main has the same slot 7.

rt3b is the recipe with ring_pages=0 added (the old page 1 has "enter" in slot 7):
- "ring         : the player chose "enter": the key F4, 1 time, frame 121", "the player chose "room_save": the key F7, 1 time, frame 2701", "the player chose "leave": the key Esc, 1 time, frame 4901".
- "overlay      : the glow at the end of the ray is drawn, frame 42: 180 triangles in this eye, 3.5 cm across, 1.34 m from the eye". The log has this line one time.
- Pictures at 1673: the hall during its guide (black bars at the top and bottom, the subtitles, the advisors). The ray and its end are not in the view.
- Pictures at 4273, the Save Game Room: the ray goes from the Hand down to the foot of the wall in front. At its end in both eyes there is a round soft glow, white in the middle and gold at the edge, with no cross. It is at the same place on the wall in both eyes, at the line where the floor meets the wall. The wall does not cut the glow. The bottom edge of the glow touches the line of the floor.

## 4. rt4b, ray_tip=0: PASS

- "overlay      : ray_tip=0: the end of the menu ray is a small cross in the eye pictures, and a disc in a ring of dark ink over the panels". No "glow at the end" line in the log. The ring choices are the same as rt3b.
- Pictures at 4273: the same view. At the end of the ray in both eyes there is the small pale cross, at the same place as the glow of rt3b, with no glow.

The game-art pictures stay on the laptop: C:\wt-raytip\bwvr\tools\desk\desk-rt*, and the crops in C:\BW1-VR\local\shots\job79.
