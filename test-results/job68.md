# Job 68: the VR Settings window and the Controls page (agent/vrsettings)

Runs C, D and E ran at afbbf9f (6136 checks, 0 failed). Runs A and B ran again at 8ef0eff (6137 checks, 0 failed) as vrsA2 and vrsB2, because the parts of the page moved at 8ef0eff. Each run is RUN DONE, with no crash and no fault.

## vrsA2 (Vulkan) and vrsB2 (dxwrapper), 8ef0eff

The two logs have the same lines:
- "controls_page=1: the settings window has the tab Controls. controls_page_models=1 ...".
- "panel_warm=1: the models of the controllers are loaded (meta-quest-touch-plus and meta-quest-touch-plus), so a thread of the mod draws their pictures for the page Controls, frame 2".
- "the window opens, 0.56 by 0.56 m, its middle at 0.00 -0.21 -0.98 m, the group Moving with 7 switches, frame 681".
- "the trigger presses the tab Controls: 0 switches, frame 741".
- "the page with the models (meta-quest-touch-plus and meta-quest-touch-plus, parchment): part 0 left Stick click: Recenter (a list), part 1 left Stick: Walk, part 2 left Trigger: Not used, part 3 left Y button: Ring (a list), part 4 left X button: Spell book (a list), part 5 left Grip: Move the land, part 6 left Menu: Game menu (a list), part 8 right Stick click: Not used (a list), part 9 right Stick: Turn and zoom, part 10 right Trigger: Click, part 11 right B button: Gesture (a list), part 12 right A button: Teleport (a list), part 13 right Grip: Take, throw".
- "the part 3 of the page: its list of 4 items opens, frame 801".
- "the list of Left Y: it goes from Ring to Recenter, and Left stick click takes Ring. button_map_other=1324 is in bwvr.ini. It acts when each button is up, frame 861", and "the new map of the buttons acts now: button_map_hand=1234, button_map_other=1324, frame 861".
- The ray on part 11 (frame 911), then "the window closes: the player pressed Close, frame 981".
- The left stick click hold opens the ring (frame 1051), and "a hold of Y of the other hand recenters the view, frame 1251".
- "a picture of the page took 28.50 ms" (27.36 ms in vrsB2) at frame 741, the first picture of the page. The later pictures take 3.41 to 4.17 ms. No line says that the page waited for the warm.
- "hints_follow_game=1, the ToolTips option of the game chooses whether a line goes away when the player knows it. All ToolTips: each line shows always ...". No line gives the value of the ToolTips option.

The window pictures (mod art, the PNG files in test-results/job68):
- The Controls page: the two Touch Plus models with a box for each button. On the left the Trigger box is above the Y box, and on the right the Trigger box is above the B box. No leader line crosses another, and no leader runs over a ring of a button. The text of the boxes reads well.
- The open list of Left Y (Spell book, Ring, Recenter, Game menu) is a small scroll over the boxes. It covers part of the right B and A boxes.
- After the choice: Stick click Ring, Y button Recenter, and the bottom row "Left Y: Recenter. Left stick click: Ring". Reset Defaults is lit.
- The ray on the right B button (911): the B box and the B button on the right model are gold.

The eye pictures of vrsB2 (1500, 1560, 1620, 1680) do not show the settings window. They show the land, the Hand and the Temple only. The same is true in vrsC and vrsD. So the window is not in the eye pictures of dxwrapper.

## vrsC (button_map_other=1324, tab2 reset close), afbbf9f

- "Reset Defaults: each button has its job as made. button_map_hand=1234 and button_map_other=1234 are in bwvr.ini, frame 821", and "the new map of the buttons acts now: button_map_hand=1234, button_map_other=1234, frame 821".
- The window picture at 821: "Each button has its default job again." and Reset Defaults is dim. Before it (741) the page has Stick click Ring and Y button Recenter. No window picture shows the gold button before the press. The eye shots 1540 and 1580 do not show the window.

## vrsD (tab1 right0 close), afbbf9f

- "grab_hand goes from 1 to 2 (Either grip), written to bwvr.ini. It acts at the next start of the game, frame 801", and "the bottom row of the window asks for a restart of the game: a setting that acts at the next start has a new value, frame 801".
- The window pictures: the Hands tab has 2 rows (Grab the world, Gesture trail), and Gesture trail is a toggle. At 801 and 813 the bottom row reads "Grab the world: Either grip", and at 813 the note of the setting is open. No window picture shows the red round arrow and the restart line. The eye shot 1720 does not show the window.

## vrsE (hand_run -Kind left), afbbf9f

hand_follow_game=0 and left_hand=1 in bwvr.ini.run. "the left controller is the Hand, and the right controller is the other hand (left_hand=1, the game's option 0)". The hand feel lines show the land near the hand. No crash.
