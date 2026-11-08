# Job 60: the start dialog as the first menu (agent/startmenu 76e508f)

`.\dev.ps1 build -Clean`: 6077 checks, 0 failed. Each log has "build 76e508f", "game renders at 2760x2208" and "backend : dxwrapper\ddraw.dll". The three runs ended normally, with no crash and no fault line.

## sm-empty (-EmptySave) and sm-save (-Save temple)

Both logs:
- "game start : start_new_game=1. The mod gives the game the word -NEWGAME, so the game starts a new game and the first menu is the start dialog of the fan patch ...".
- "game start : a new game starts, frame 0 (game mode 1, land 1) ... The word -NEWGAME came from the mod (start_new_game=1)". The mod dropped 16 writes of the game to its keys of the registry.
- "controllers : a dialog box is up now, frame 0".
- "menu room : frame 1, the menu panel goes in front of the head: 2.35 by 1.76 m, its center at 0.00 -0.24 -1.38 m ... It shows the rect 740 624 1280 960 of the 2760 by 2208 render", and "the menus go to the panel in the room".
- The ray: before frame 400 the pixel is 1489.0 1231.8 (the rest pose). After frame 400: 1238.4 1081.6 (target 1238 1082). After frame 500: 1232.9 1215.8 (target 1233 1216). Both are within 1 px.

The panel pictures (bwvr-ui) show the start dialog: "Please choose from one of the following options:" with four rows with a check box each (Start the game normally, Skip straight to the Creature select, Skip all of the Creature tutorial, Keep your old Creature) and OK. In sm-empty the first row stays checked (no trigger). The eye pictures show the land of the start, with no dialog.

sm-save:
- Before OK (ui 509), the checked row is "Skip straight to the Creature select". The ray end is on OK.
- "controllers : a dialog box is closed now, frame 551". No dialog comes back (one "dialog box is up" line in the whole log).
- The eye pictures after OK: black, then the gate, then a green valley, and at log frame 3072 the three Creatures to choose from (a cow, an ape and a tiger). Then land 1 from a hill. There is no tutorial.

## sm-cont (-Args -CONTINUE)

- "game start : start_new_game=1, and the command line has the word -CONTINUE, so the mod does not change the start".
- "game start : the saved game is loaded, frame 0. The game loaded the saved game of slot 20, and it opens its Main Menu over it (game mode 0, land 1)".
- The panel shows the Main Menu tab: "Continue Game", "Start Skirmish Game", "Join Online Game", "Options", "Quit Black & White". The second tab is "Statistics". The Main Menu tab has no button for a new game. No picture shows the Statistics tab or the Save Game Room, so I cannot say if it lists "Auto Save On Exit".

The pictures stay on the laptop: C:\wt-startmenu\bwvr\tools\desk\desk-sm-*.
