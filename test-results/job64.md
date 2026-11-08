# Job 64: the job icon hook (agent/jobicon dd91433)

`.\dev.ps1 build -Clean`: 6097 checks, 0 failed. Each log has "build dd91433". `village_run.ps1 -Kind job -Seconds 62` with the steps of the job, dxwrapper, 60 frames a second. jobC (job_icon_scale_pct=100) and jobD (200) are RUN DONE.

1. The first lines: "game renders at 2760x2208". At the start a window of a different program (Chrome_WidgetWin_1) is in front, then "in front is the game window. The cursor is at 2624 0, frame 1".
2. No line "AddDrawing ... does not have the bytes" in either run.
3. C: "job icon : job_icon_scale_pct=100. The job icon has the size that the game gives". D: "job_icon_scale_pct=200 ... and its lower edge stays where the game puts it". Both: "names : villager_name_scale_pct=150".
4. Both runs: zoomed in 100 to 66.7 (frame 71) and 66.7 to 44.4 (frame 101). Seek plan gostore (frame 192), store (272), villager 13AE1054 or 13AE9054 (312, 150.5 cm from the Hand's point), "the grip picks it up" (frame 313), "the grip pressed on the target ... (Villager)" (frame 318), and store (452).
5. The job icon lines (the icon 8, at 75.3 to 74.7 units from the camera):
   - C: "the game gives a half size of 0.440 units (0.67 degrees across, 13.7 px of the render). The mod draws it 0.67 degrees across (13.7 px, 16.9 px in a degree), with its lower edge in place and its middle 0.000 units higher". Windows: "118 draws of the job icon this window. The last one is 0.67 degrees across (0.67 as the game gives it), 13.8 px", then "9 draws ... 0.69 degrees across (0.69 as the game gives it), 13.4 px, at 30.2 units".
   - D: "the game gives a half size of 0.440 units (0.67 degrees across, 13.7 px). The mod draws it 1.34 degrees across (27.3 px, 16.9 px in a degree), with its lower edge in place and its middle 0.440 units higher". The middle moves up by one old half size at each line (0.440, 0.438, 0.436, 0.436). Windows: "118 draws ... 1.34 degrees across (0.67 as the game gives it), 27.6 px", then "10 draws ... 1.39 degrees across (0.69 as the game gives it), 26.9 px, at 30.2 units".
6. The pictures (left eye, log 373, 423, 493, 553, 693): the job icon does not show. The fake hand reaches 150 cm to the left for the villager, and the head looks ahead at the Village Store, so the held villager and its icon are out of the view. So the icon width, the gap over the head and the icon over the store could not be measured.
7. The seek acted (see 4), and the zoom acted in these runs.

The pictures stay on the laptop: C:\wt-jobicon\bwvr\tools\desk\desk-jobC and desk-jobD.
