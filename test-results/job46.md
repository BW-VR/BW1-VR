# Job 46: the ray glow and the ray over the panels (agent/pointer 315d2a6)

`.\dev.ps1 build -Clean`: 5975 checks, 0 failed. Each log has "build 315d2a6" and "game renders at 2760x2208". All runs are RUN DONE.

- rayA: "ray card : the ray goes over the panels as a quad layer, frame 280: from 0.000 0.000 -1.000 to -0.277 -0.209 -2.970 m (2.00 m), a strip 1.9 cm across and 2.00 m long at -0.139 -0.105 -1.987 m. Its front is 85.0 degrees from the way to the head. The look: the glow alone, and a mark of 1.0 cm at the end. The eye pictures have no ray", then "the ray is no longer over the panels, frame 821, after 541 frames", and "overlay : ray_glow_only=1". Possible fault: the front is 85 degrees from the way to the head, not near 0, so the strip is nearly edge-on. 3 bwvr-raycard files of 32 by 1024 px: a soft warm strip with bright spots and a dark ring at the far end. The eye pictures have the hand and no ray.
- rayB (ray_over_panels=0): no line "ray card : the ray goes". The ray is one solid soft cream band.
- rayC (ray_over_panels=0;ray_glow_only=0): the tube with thin dark edges in a wider glow.
- rayD (video_view=1, Vulkan): the same ray card lines (frame 317, front 85.0 degrees). bwvr-vk-video-00693 has the welcome page with a thin pale strip to the Settings button, and 00783 has the settings window with the strip to Controls. The strip is thin, which fits the 85 degrees.
- rayE (video_view=1;ray_over_panels=0): the panels cover the ray in the video pictures.
- ring1 g1 and g0 (aim_queue): g1 "ray_glow_only=1 ... no tube", g0 "the glow is over the tube". At frame 553 the arc of g1 is a soft cyan glow, and the arc of g0 has a solid dark cyan tube inside the glow.
- tetherG1 and tetherG0 (-Kind fartotem): at frame 503 the gold tether of G1 is a soft glow, and G0 has a solid gold tube inside the glow.

Pictures: C:\BW1-VR\local\shots\job46 (rayB-rayC-crop.png, raycard-wide.png, rayDE-video-sheet.png, arc-crop.png, tether-crop.png, ray-sheet.png).