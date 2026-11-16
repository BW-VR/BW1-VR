# Roadmap

This file is the list of the open work and of the plans. It does not hold
the history: when an item is done, remove it, and write what the mod does
now in the doc of the area. The git history is the history.

The doc of each area in `bwvr/docs` ends with the open questions and the
known limits of that area. This file names the work, and the doc has the
detail.

The checks that only the owner in a headset can make, and the faults that
the log of a normal play tells, are in `headset-checks.md`. The checks that
a desktop run can prove are in R12. An item here keeps the work to build.

## Where it stands

The mod is at version 1.2.0, an alpha for a small group of testers.

- **The game in VR.** The game draws the scene one time for each eye, the
  head moves the camera, and the picture goes to the headset through
  OpenXR. The main mode is the God view: the land is at the height of the
  waist, and the player zooms, turns, grabs the land and teleports.
- **The hands are the Hand.** With motion controllers the player takes a
  thing with the grip, brings a far thing to the hand, throws, strokes and
  slaps the Creature, commands and leashes it, casts a miracle, and uses
  the village and the Temple. Hint cards say what each button does.
- **The mouse and the keyboard** play as in the plain game.
- **A story** of the game shows in a window in the room, and the sound comes
  from where its source is.
- **The build.** `dev.ps1` builds the mod and puts it in the game folder of
  a contributor. GitHub builds each push and each pull request and keeps an
  installer with the run. A release for the testers is made by hand.

Only one setup is tested by a person: a Quest 3 through Virtual Desktop.
The known faults for a player are in `bwvr/installer/README.txt`.

## To do

The most urgent part is first. An item that says "not run" or "not shown"
is built, and no test has proven it.

### Next

These items come before the other work.

- **Q1. The game with the profile of a player in a Quest 3: check it in the
  headset.** The game with the profile of the PC stopped on the loading
  screen. The last line of the game was its check for an internet
  connection (`LHCheckForInternetConnection`). The network part of Wine
  cannot read the network cards on the headset ("nsi: bind failed, errno
  13"), and the main thread of the game waits forever. A new profile does
  not come to this check, because its dialog comes first. The mod now
  answers the whole check with "no connection" (`net_check_skip=1`, the
  setting "Tell the game that there is no internet", `intro.md`, "The
  internet check"). Not run in the headset.

  Check in the headset, with the profile of the PC:
  - that the game goes from the loading screen to the Main Menu,
  - that `bwvr.log` has the line "The mod answered: no connection, with no
    check",
  - that slot 0 or slot 20 of the profile loads,
  - that "Tell the game that there is no internet" is on in the page
    Testing of the menu.


- **B35. A thrown object that hits the water turns fast.** A rock of
  `throw_queue.ps1 -Runs rockfast` that lands in the sea turns 2.36 times a
  second of the picture with no point that touches, over the cap of 1.5
  turns a second (`throw_turn`, `throw.md`, "The turn of a thrown
  object"). The cap acts in 0 steps there. The table of `throw.md` for a
  tree and a rock needs a run in which each throw lands on the land.
- **P20. The spell book: the open parts.** A tap of B (or X) opens a spell
  book over the hand that does not point, and the pointing hand pulls a
  miracle out of it with the grip: the game's own request (`spell_book=1`,
  `spell_book_b=1`, `spell_book_pull_cm=20`, the orbs of the game on a ring
  `spell_book_ring=1`, `spell_book_orbs=1`, `debug_all_miracles` for tests
  on the debug page, group Miracles, `miracles.md`, "The spell book"). The
  palm that turns up shows the sheet of the gestures (`gesture_sheet=1`).
  The recipe `village_run.ps1 -Kind book` shows the
  open, the halo, the card, the pull, the spring back, the take and a
  refusal. The checks of the book, the row of gestures and the draw are in
  `headset-checks.md`, "What the owner checks", and in R12. Open:
  - No line of a card says that B held down draws a gesture while the Hand
    is empty. The sheet of the gestures shows the shapes over the palm.
  - Only a headset shows if the sheet of the gestures is easy to read over
    the palm, and if a tap of B opens the book with no gesture by mistake.
  - The book of all miracles (`debug_all_book=1`, 34 items on 6 pages,
    `miracles.md`, "The book of all miracles") and the debug row "Spawn all
    miracle orbs" have no desktop run yet: the recipes `village_run.ps1
    -Kind allbook` and `-Kind orbsave` wait for the test laptop. No run has
    cast a power level (Increase, Extreme), or the Creature miracles Weak,
    Compassion, Angry and Itchy, in the mod. The tally line of each report
    window says which of the 34 a session cast.
  - Only a headset shows if the seals "Back" and "Next" are easy to pick,
    and if the orbs on the land are easy to reach.
  - A save keeps the miracles that "Unlock all" gives the towns
    (`miracles.md`, "What a save keeps"). The mod deletes its orbs and puts
    the limit of prayer power back before each save, but it does not take
    the miracles of the towns back. The recipe `village_run.ps1 -Kind
    orbsave` waits for the test laptop.
  - The orbs in the cells: only a headset shows how they look in their
    holes of the page, with the land behind the glass.
  - The leash gestures and the scribble are not in the book.
### Faults in play

- **B38. Vulkan: white or invisible story people, and crashes with no
  address space left.** With `backend=vulkan` (an NVIDIA RTX 5070 Ti, a new
  game), the Lost Brother and the Lost Sister of Land 1 draw flat white and
  are invisible after their quest (only the eyes show), Sable turns white at
  the end of the cow stone, and the game stops in `nvoglv32.dll` at the
  choice of the Creature. A second player (SteamVR, a render of 4096 by
  3966) had a crash in `vrclient.dll`. In both logs the dump failed with no
  memory (`backend.md`, "The address space"). The launcher now sets
  `LARGE_ADDRESS_AWARE` in `runblack.exe` (`large_address=1`), so the game
  has 4 GB. Desktop runs with all memory above 2 GB (`test_high_memory=1`)
  played as the runs with 2 GB did, with both renderers and through the
  choice of the Creature. `vk_upload_keep=1` keeps the old picture of a
  texture when the upload of its new image fails. The log has the address
  space and a reason line for each texture read that gives no image ("A
  read with no new image"). Testers still get up to two crashes a day,
  with the God view and with the flat view. The log lines of the crash and of
  the white people are in `headset-checks.md`, "Play as normal and send the
  log". The invisible body after the
  quest has no known cause yet.
- **B43. The clouds of a Storm in the God view.** The game flattens a sky
  cloud near the horizon of the eye, and the eye of the God view is low and
  near the clouds of a Storm, so they made a flat grey band.
  `cloud_round_near=1000` keeps a near sky cloud round, and
  `cloud_fade_near_cm=15` with `cloud_fade_far_cm=60` clears a cloud near
  the eye (`backend.md`, "Clouds"). A desktop run shows no band. Late in the
  storm a round cloud that is half clear lies over the Temple. With
  `cloud_round_near=0` and the fade, only a thin band shows at the horizon.
  Open: the owner chooses the look in the headset (round clouds or flat
  clouds, each with the fade).
- **B6. A script that waits for the dialogue has the camera of the game.**
  A script that has the camera and waits for the dialogue shows the camera
  of the game with a held shot and no cinema bars, and not the God view
  (`cutscene-window.md`, section 2). It can show when a quest starts while a
  text of a different script waits for a click (`text_click`,
  `controller-hints.md`, section 10). No run and no headset session has
  shown it.

- **B10. A thrown object that touches another object.** In a substep where
  another object touches the thrown object, the game runs one step, and the
  mod gives that substep 5 ms of the picture (`throw_draw=1`, `throw.md`, "The
  picture between the steps"). So the object moves through the touch at the
  speed of the game's own time, and the picture does not stand and jump. A
  desktop run with a thrown villager shows it, and the stand and jump with
  `throw_draw=0` (`village_run.ps1 -Kind throw` in `bwvr/tools/desk`). The
  check of the look is in `headset-checks.md`. Not run: a picture that stays
  at the place of a guess (the math checks cover it). A run shows the clock
  that falls back after a wait.
- **B13. The view of the Temple, and the black frames of the guide.** In a
  room the head owns the view: each eye pass draws from the head pose of the
  frame, with its roll and the place of its eye (`temple_view=1`). The head
  does not turn the camera of the room (`temple_camera_screen=1`), and the
  fade of the Temple shows in both eyes (`temple_fade_eyes=1`).
  `controllers.md`, 11.4, has the three rules. The math checks cover them.
  Still open:
  - A desktop run shows the roll of the head in a room with `temple_view=1`
    and a level room with 0, and the cut of the hall to its rest pose. Not
    shown: the place of each eye, and the fade in both eyes (R12).
  - Single black frames showed at random times in the guide of the first
    visit of the hall. The cause is not known.
  - The check of the view is in `headset-checks.md`, "The rooms of the
    Temple", and the log line of the black frames is in "Play as normal and
    send the log".
  - Not built: the depth of a guide in the cut scene window, and a ray of the
    room that starts at the eye (`controllers.md`, 11.9).
- **B14. In a pass the second eye does not have zero as its frame time.**
  After the two eye passes the game has the real time of the frame as its
  frame time (`frame_time_real=1`, `backend.md`, "Time, bars and the second
  eye pass"). So the cameras of the game move at the speed of the game, also
  in a game from a saved game.
  Not run: a story camera with no
  cinema bars, the camera of a Creature fight, the pull back past 3500 units
  and the moves of the game camera by the keys. They read the same number.
  Not known: why the Creature is one step further in its plan 1 s after the
  start of play with the real time (the first stay of `creature_recipe.ps1
  -Kind stay`, `backend.md`). Later in a run the plan of the Creature does not
  depend on the key.
  Still open: in the scene walk `LH3DRender::StartFrame` writes the frame
  time at the start of each pass, so the second eye pass has the time of the
  first pass there, and not zero. The readers in the walk count in both
  passes (the hand states, `HelpSystem::Draw3D`, `Temple::Update`), and
  their sum was 4 to 9 percent more than the real time in desktop runs. Not
  built: a hook on `StartFrame` for the scene walk, as the intro has for its
  call (`intro.md`). First find each reader that divides by the number: the
  game never has less than 1 there.
- **B16. The left eye showed pictures of frames before.** In a headset session
  the left eye started to show faint pictures that came late, and the right
  eye did not. In play the game does not clean its render target: the sky dome
  is the clean of a frame (`LH3DRender::g_mode_cleaning` is 0). The two eye
  passes share that target. So a pixel that an eye pass does not cover keeps
  the picture of the other eye: one frame old for the left eye, and of the
  same moment for the right eye. Each eye pass now starts from its own frame
  before (`eye_clean=1`, `stereo-artifacts.md`, "The frame before of each
  eye"). No desktop run has shown it: that section has the two runs that are
  made for it. Still open:
  - Not known: which thing of the game left such pixels in that session.
    The log of the session cannot place it.
  - The log line of the fault is in `headset-checks.md`, "Play as normal
    and send the log".
  - Not run: the video of a falling spell, the one scene of play that draws
    no sky. The first frame of such a scene is drawn as before.
- **B17. The size of a speech bubble, and the name of a villager in a
  headset.** The speech bubble of a signpost is one flat card at the depth
  of the sign (`bubble_card=1`, `ui-layer.md`, section 18), and a headset
  session read it well. The name of a villager is such a card at the depth
  of the villager (`name_card=1`). Desktop runs show both cards (section
  18.5 has the recipes). Still open:
  - The size of a bubble comes from the depth of the sign in units, with no
    change for the scale of the God view.
  - The check of the name is in `headset-checks.md`, "The words over the
    land".
- **B18. A flash of lightning in the left eye only.** In a headset session
  the white flashes of the lightning at the end of the quest of the singing
  stones showed in the left eye only. A lightning of a storm makes the land
  white with a light on the land, and the game keeps the lights on the land
  in a list that each scene walk empties. The second eye pass gets each
  light that it does not add itself (`land_light_eyes=1`, `backend.md`,
  "The lights on the land in the second eye pass"). A desktop run shows it
  with the test light (`land_light_test=1`), which is the light of a
  lightning of a storm. No run had a real lightning. Not known: whether the
  flash of that session was a light on the land. The log line of
  the flash is in `headset-checks.md`, "Play as normal and send the log".
- **B36. The address space of 2 GiB runs full: check the fix in a
  headset.** Headset sessions with `backend=vulkan` ended after 7 to 16
  minutes, when the 2 GiB of addresses of `runblack.exe` were full
  (`backend.md`, "The address space"): `malloc` gave a null pointer in
  `LH3DMem::Alloc` (`0x00842D4A`), or the game faulted at a save. The lines
  of `memory_blocks=1` showed the cause: the process heap grew by about 680
  MB in 15 minutes through `amdvlk32.dll`, the large blocks and the private
  allocations that no hook saw stayed the same. `BeginOnce` of `vk_core.cpp`
  allocated a new command buffer for each single job, and the reset of the
  pool does not free it. A copy of the panel for a card in the headset is a
  single job in each frame. `vk_once_reuse=1`
  gives each job the same command buffer. To see in a headset: the line
  "memory" stays near its value after the load, and the process heap does
  not grow, in a session of 30 minutes or more.

- **B33. The water with the leash on: test it in the headset.** While the
  ring of the leash is on the left wrist, the icon of the kind of leash at
  the lower right of the view is hidden (`leash_icon_hide=1`, test key Ctrl
  Shift L, `grab.md`, "The leash at the mod's hand"). The water shows both
  hands of the mod in place of the game's Hand (`hand_reflection`). No
  desktop run has shown the icon hidden. To see in the headset: with the leash on,
  no ring in the water moves with the head, and Ctrl Shift L shows the icon
  again.
- **B34. The Creature turns to the head: check it in a headset.** In the
  God view the Creature reads the head of the player as the camera of the
  player: its turn, its look, a point or a throw at the player, and the
  distance to the player (`creature_player_head=1`, `creature.md`, section
  5.4). The game's own camera there is about 300 units above the land, so
  the Creature looked up into the sky. Desktop runs show the turn of the
  stay and the look at the head, and each read of the camera in the code of
  the Creature got the head. Check in a headset:
  - that the Creature faces you and looks at your head after a stay and
    after a cut scene,
  - that the Creature does what you show it in the Creche quest of Land 2,
  - that in a story the Creature looks at the camera of the story.

  The A/B key is Ctrl, Shift and H. Not run: a story, a cut scene, a point
  or a throw at the player, and a Creature of a different player.

- **B44. The game ends with "abnormal program termination" after a fetch
  of a dead tree.** In a headset session on the second island, the Hand
  took a dead tree from a distance. About 0.3 s after the glide the game
  ended with the box of `abort`, with no line in the log and with no dump.
  The cause is not known. The crash log (`crash_log=1`, `backend.md`, "The
  crash log") now writes the C++ exception and the caller of `abort`. The
  next log of this end gives the place.
- **B39. The clouds, the beams of light and the miracle icons and the roll of
  the head: check it in a headset.** A camera of its own in the scene walk
  leaves the engine with a camera that has no roll and no eye offset, and the
  draws of the sorted list after it do not roll with the world
  (`eye_after_camera=1` puts the eye camera back, `backend.md`, "The eye
  camera after a camera of its own"). Check on the Frame with the head
  rolled to the side, in the God view, over the village centre and the
  Temple:
  - that the clouds, the miracle icons and the beams of light of a charge
    stay with the world at any time,
  - that Ctrl, Shift and E (or the debug page "Picture", "Eye camera") shows
    the old behavior with 0.

  With `roll_trace=1` the log names the address that gives the camera and the
  draw functions of the items that saw it. Not known: the name of the effect
  that gives the camera on each frame, and whether the billboards that turn
  by less than the roll within 10 degrees of a look straight down (line
  `sprite trace`) show in the headset.

B1b, B2, B4 and B11 are one package: a thing that is held, thrown, or
that moves while the Hand takes it.

### Changes to the controls

- **P2. A fast click on the Creature with the leash on.** With the leash
  on, the game starts its press on the Creature 17 to
  22 frames after the grip closes (2 to 7 frames with the leash off). So a
  grip of 100 ms with the leash on gives no leash click
  (`creature_recipe.ps1 -Kind wide`). Not known whether a fast click on the
  Creature with the leash on takes the leash off in the plain game.
- **P15. The glow on the mesh of a target, and the targets that keep the
  halo.** The mesh of a target of the Hand glows in
  the color of its halo, with a pulse and a band of light that goes up the
  mesh (`target_glow=1`, `hand-model.md`, "The glow on the mesh of a
  target"). Desktop runs show it in both eyes on a rock, a villager, a bush,
  a tree, a pile and the Village Store. Open:
  - The check of the strength, the pulse and the band of light is in
    `headset-checks.md`, "The aim and the halo".
  - The Creature (`LH3DCreature::DrawNow`, `0x0048E260`) and a building in
    work (`0x00517F90`) do not draw through `Game3DObject::AddForDrawing`.
    They keep the halo. Wanted: the glow on their mesh.
  - An object that the game draws as a picture far away keeps the halo.
  - No run has shown a mesh of the sorted list as a target, a mesh that
    comes through the clip of the engine (`LH3DRender::DrawAndClip`), or a
    target by day.
  - The light is even on the mesh. A glow that follows the bright parts of
    the texture needs a second draw of the triangles with additive blend.
    Not built.
- **P18. The hold of the God view at the start of a story: run it and check
  it in a headset.** When a story takes the camera (a tap on a scroll), the
  God view keeps its view until the cut scene window is closed
  (`cutscene_hold=1`, `cutscene-window.md`, section 3). The story camera
  starts at the game camera of the God view, about 300 units above the land,
  and the player saw that camera for the 0.5 s of the enter. The math checks
  have the rule. Not run: a desktop run of a story (the story of the scroll
  of the Creche, or the story to the pen) that shows the lines of the hold
  and no frame of the story camera before the first flat frame. Check in a
  headset that the walls close over the land with no jump, at the scroll of
  the Creche, and compare with Shift S (`test_keys=1`). The scroll of a silver
  quest moves the camera for about 8 s before its bars, so the hold ends
  after `cutscene_hold_wait_ms` (1000) and the full view shows the move,
  which starts at the head (P19). The hold needs the window: with "Around
  you" it does not act.
- **P19. A story starts at the head: check it in a headset.** When a script
  takes the camera in the God view, the game camera goes to the head, so the
  move of the story starts there and not 300 units above the player
  (`story_start_at_head=1`, `god-view.md`, section 18). Check in a headset,
  with "Around you" and with the window: the scroll of a silver quest, the
  cut to the life size scale at the head, and the move after it. The A/B key
  is Ctrl, Shift and S.
- **P5. Hand tracking.** On hold. The player plays with the hands and no
  controllers: the Hand follows the real hand and its fingers, one to one,
  as it follows a controller. The mod asks for `XR_EXT_hand_tracking` for
  finger tracking on the controllers (`hand-model.md`, "Finger tracking"),
  and the log says whether the runtime and the system have it. A log of a
  headset run tells whether Virtual Desktop (VDXR) gives it to the game.
  Then choose a gesture for each control: the grip, the trigger, the stick,
  and A, B, X and Y. The rules of the Hand stay: the real hand, one to one,
  and no motion that the player did not make.
- **P26. The fit of the hand and finger tracking: check them in a headset.**
  The six `hand_fit` keys move and turn both hands against the grip pose,
  and `finger_tracking` bends the fingers as the real fingers on a
  controller that sees them (`hand-model.md`). A desktop run showed the fit
  and the curls with the fake controller. Only a headset shows which values
  fit the Touch controllers and the controllers of the Steam Frame, and
  whether the Steam Frame gives the joints, also through the x64 host. A fit
  in the headset, with the controllers and no restart, is not built: the
  player sets the values in the launcher of the PC, and the menu of the
  headset app does not show them.
- **P7. A view for videos** (`video_view`, `backend.md`, "The view for
  videos"). Open:
  - The height changes in the game only on the debug window ("Video
    height"). A row of a settings panel or of the ring is not built.
  - With dxwrapper the view does not show. A copy through a DXGI swapchain
    of the Direct3D 11 device of the session is possible, with the limits
    in `backend.md`, "The view for videos with dxwrapper".
  - No headset session has run it. The cost in a session is in the lines
    `vk window` and `vk video` of the report. The session path of the quad
    layers (the copy of each card on the GPU, the panel quads of
    `ui_panel=1`, a screen in the room) has run in no session.
- **P9. The pointer on the interface panel** and the screens with no scene
  (menus, loading, videos) on a panel in the room, with a session of the mod
  on the PC (`ui-layer.md`, sections 5 and 6). The headset app has them: the
  screen quad and the panel quads, with the ray, the trigger and the
  keyboard of the app on a dialog box there (`controllers.md`, sections 12
  and 13). Open in the headset app: the last pixel of the world when the
  ray leaves the panel quads (`ui-layer.md`, 5.5).
- **P10. The tabletop mode** (play mode 3) is a test mode, on hold. Not built: a
  game camera over the table, the mouse, the wheel, the stick and the
  double click for the table, the ring and the depth key for the key color,
  and the place of the table in a real room (`tabletop.md`).
- **P12. Ideas from Half-Life: Alyx** that are not built are in
  `research-hla.md`, for the far grab, the teleport, the hands, the hints,
  the sound and comfort.
- **P21. A take at the Hand stops when the hand moves away.** With the grip
  closed at a pile of the store, the take stops when the hand moves out of
  the area of the pile. A take at a distance (the ray) keeps on while the
  player moves (`grab.md`, "A bulk source at a distance"). Wanted: the take
  at the Hand acts as the take at a distance. The game has no test of the
  distance: it ends a take only when the source is gone or empty, the Pot
  is full, or the Hand is outside the influence
  (`GInterfaceStatus::Process` `0x005DC4E0`). The desktop run cannot make
  the fault happen with a steady grip: in `village_run.ps1 -Kind neartake`
  the take goes on with the hand 15, 36 and 48 cm of the room from the
  pile, until the grip opens. The likely cause is a real squeeze that falls
  fast for a moment, which opens the grip (`grip_fast_open`). So
  `near_take_keeps=1` lets only a grip below 35% end a bulk take, and the
  same run makes such a fall with the word `gripdip`. Open: a headset log of
  a take with the hand moved away (`headset-checks.md`, "Play as normal and
  send the log").
- **P24. The Hand stops at a house, a rock and a tree.** The drawn hands
  stop at the land and at the box of a house and a rock and the trunk of a
  tree (`hand_land_stop`, `hand_object_stop`, `hand-model.md`, "The Hand
  stops at the land"). The head blinder counts the same solids
  (`head_blinder_objects`, `god-view.md`, section 5.9). Open: the desktop
  runs `village_run.ps1 -Kind housestop` and `-Kind headhouse`, each key at
  1 and 0. A solid is the box of the whole mesh, so an open building is no
  solid. The game's own collision of a building could give its walls.

### New features: miracles, comfort and help

The order is the order of the work. Each one gets a key whose old value
keeps the mod as it is.

**Miracles and gestures**

- **F6. The fireball and the speed of a throw.** On hold: the owner tests
  the throw with the mod's own physics first. A fireball leaves with the top
  speed of the grip of the real hand (`miracle_throw`, `throw.md`, "A thrown
  miracle"), and then has the game's physics. If it flies short or far in a
  headset, a key scales that speed. For other objects, a key
  `throw_speed_pct` (100: the real speed) can make a throw stronger.
**Comfort and moving**

- **F11. The controls of the player in the headset.** The VR Settings
  window, the welcome page and the page Controls with the models of the
  controllers are in (`ui-layer.md`, 24 and 24.6, `controllers.md`, 5.7).
  Open:
  - The desktop runs of the page Controls (`village_run.ps1 -Kind
    controls`: a list, a new job that acts, Reset Defaults, and
    `hint_profile`). A desktop run has a Touch controller only: the profile
    of a real Index, wand or Mixed Reality controller needs a headset with
    that controller.
  - The controller in the hand with its lit buttons and their cards
    (`hint_style=2`, `controller-hints.md`, 9.14). Desktop runs show the
    cards on the outer side of each hand, level in the view, with no
    crossings, with a continuous ink outline, at 0.2 to 0.6 ms a frame.
    The model is on the frame of the WebXR model, fitted on the grip pose
    of Virtual Desktop, the Oculus runtime and SteamVR (`grip_fit=1`,
    `controller-hints.md`, 9.15), and the VR Settings window has a trim of
    5 mm steps. The hand has the angle of the palm pose of the runtime at
    the same grip point (`hand_angle=1`, `hand.md`, "The angle of the
    hand"), with a trim in degrees on the debug page. Only a headset can
    show that they are on the real controllers and the real hands, and
    whether the cards read at 55 cm (`headset-checks.md`, 5 and 21).

**The village, the Creature and the help**

- **F27. The hand in front of a page.** The page of the spell book and of
  the ring has a hole for each eye where a hand or the bubble of a pull is
  in front of it (`card_hand_front=1`, `card_hand_both_sides=1`,
  `card_ball_cut=1`, `ui-layer.md` 28). Open: the first cut after a move
  takes 1 to 3 ms on the test laptop (0.05 ms on average). Where the
  texture of the game makes the palm almost clear, the land shows in the
  hole beside the arm. The controller model of the hints and the hand fight
  in depth where their surfaces meet (the owner sees it in the headset).
  The likely cause: `hint_model.cpp` draws the faint front faces of the
  body with the depth write before the mesh of the hands, so each part of
  the faint hand behind or inside the body fails the depth test, and the
  land shows there through the faint body. A fix changes the order or the
  depth write of the two draws, in both renderers, together with the draw
  of the hand with no culling.
- **F22. Words that the card never shows.** The card shows the game's
  "Drop" and "Throw" with the word of the grip (`controller-hints.md`,
  9.9). The second VR mod leaves those two words out
  (`hidden_hints=Throw,Drop`), because the arm decides at the opening of the
  grip whether the object drops or flies. Open: whether the player wants a
  list of the game's words that the card leaves out.
- **F23. The sea with depth.** On hold. `sea_depth=1` puts the land of the
  sea under the water and fades it into the sea by its depth (backend.md,
  "The sea with depth"). It is off by default, because in a headset the
  land of the coast has holes and the parents on the beach in the intro
  cut scene stand over the ground. The owner found the flat sea of the game
  good enough. Open: the other VR mod lists a fix of the sea, and it is
  maybe a fault of the depth test of the sea draw (the sea draws with the
  depth test ALWAYS), not the depth of the land. Find what that fix is
  before more work here.
- **F24. No wait of 20 ms at the stop of a sound.** `audio_stop_wait=0`
  skips the wait only for the sounds of the mod's mixer. The other sounds
  keep it: the logs of desktop runs have 1 or 2 such stops in each run. The
  log names each one (`audio.md`, 7.6): the kind of the sound, its
  wave and the time of the wait. In nine desktop runs each run had one such
  stop, at frame 0: an effect that QMixer plays (2 channels, 22050 Hz,
  31.25 s), 500 to 532 ms after its play, with a wait of 20.3 to 21.0 ms.
  So at the start only, not in play. Open: whether a play has more of them
  (the log of a play shows it, `headset-checks.md`).
- **F26. The first open of the VR Settings window.** The warm draws the
  page, the tabs and the rows of the first group of the settings, and a
  thread draws the pictures of the models of the page Controls
  (`panel_warm`, `ui-layer.md`, 23.3 and 24.6). With the page and the tabs
  only, the first picture took 4.3 ms (4.8 ms with no warm). Off the game,
  the pictures of two Touch Plus models take 42 ms each, and a picture of
  the page 3.7 ms. Open: the times in a desktop run.

### Faults of the plain game

These faults are in the game with the unofficial patch v1.42, not in the
mod. A fix is a change of behavior, with a key whose old value keeps the
game as it is.

- **G1. Silver Scrolls of Land 3.** On hold until the owner plays Land 3.
  Testers report that a guru (a yogi) sees the player from behind, and that
  trees go away in the puzzle of the trees. Not known: the quest of each
  fault, and the script or the code that has it. Make each fault occur in a
  desktop run first.

### Built, and not proven or not finished

- **N0. The late camera of the headset app: open checks on the Steam Frame.** Each frame of the
  session draws the newest list of draws of the game again from the place of the head and of the
  player at the next display time (`late_camera=1`, `android.md`, "The late camera"). The math
  checks have the place of a vertex. Check on the Steam Frame, with the row "Late camera" of the
  debug page for a comparison:
  - a flight to the side and a flight over the middle of Land 2, right after the start of a game:
    no double picture and no jump of the land,
  - the Hand in a flight (a thing in the Hand goes back by the speed times the age of its frame),
  - the snow on the roofs and each other layer with a depth test EQUAL in a move.

  Next: the time of the frame thread in the first 30 s of a game (the turns of the game are 40 to
  108 ms there with no read of a file: the first run of the code under FEX).
- **N0a. 144 Hz on the Steam Frame: what is open.** At 144 Hz a picture has 6.9 ms. With the values
  of the Steam Frame (`android.md`, "The values of the Steam Frame") at 200 percent, 99 percent of
  the pictures end on the GPU within 5.93 ms of their tick, and in a flight about 0.05 pictures a
  second repeat. For about 1600 draws a walk takes 1.9 ms on the GPU and the compose 1 ms, and the
  compositor of SteamVR takes about 1 ms. `tools/android/frame_test.py` measures each next step
  (`android.md`, "The test run"). Open:
  - A new install on a Steam Frame: no run has shown that a file with no keys gets all it needs.
    `steamvr_app_key` is not in the list, because its value depends on the install.
  - A Quest 3 (`android.md`, "The values of a Quest 3"). No run has shown a new install, 200
    percent with the compose at the take, or a Quest 3S. `tools/android/quest_test.py` runs a
    test with no player (`android.md`, "The test run with no player"). The head of a Quest on a
    table does not move, and with `xr_input=1` the mod has no scripted controller, so such a run
    has no flight and no walk. A scripted input of the app for the slots of the mod is open. At
    150 percent and 120 Hz a heavy view takes 17 ms a picture on the GPU (a walk 8.7 ms, the
    compose 5.5 ms, the panel 2.8 ms), and the session falls to 65 to 85 frames a second. A draw there writes at about 11.5 GB/s, so the cost goes with
    the pixels.
  - A fault of the game on a Quest 3: a read fault at `runblack.exe+0x47F822` when a dialog box
    closed, one time, with two queues. The cause is not known.
  - The async reprojection. `steamvr.disableAsync=1` of SteamVR is for a test that shows a missed
    picture as a stutter. Its log has no line that says that the compositor took the value.

  - The main thread of the game on the top core (`late_top_game=1`). No long game has shown the
    heat of core 7 or a lower top speed there. A thread that the main thread starts after the
    change can have only core 7, and no run with the key at 1 from the start has shown the masks.
  - The game has about 70 to 80 frames a second in a flight, and a thing that moves in the world
    goes in the steps of these frames. The main thread takes a whole core. The drawing part takes
    about three times the time of the game turn. `land_objects_threads` and `gpu_mesh=1` take work
    from it, and no run on the Steam Frame has measured them.
  - A turn and a zoom of two hands still go in the steps of the frames of the game: the shader
    would need a turn and a scale of each point.

  Do each of these only when a test run shows the need:
  1. **Many draws in one call.** A run of draws with one state becomes one
     `vkCmdDrawIndexedIndirect`, and the values of a draw go into a buffer. Each vertex then reads
     that buffer, and the texture places become flat values of each pixel, so the gain is not sure.
     With `vk_cheap_draws=1` the GPU time of a draw stays the same, so a bind of the vertices and
     of the indices is not the cost, and more than half of the draws start a new run.
  2. **Prepare and draw at the same time** (`frame_loop_gpu_depth=2`). The play thread starts a
     picture while the GPU draws the picture before. This needs a fourth set of images, a command
     buffer for each picture of a list, and a place of its own for the joined indices.
  3. **A merge across textures.** With the places of the textures in the values of a draw, the
     merge can join draws with different textures. This needs the buffer of step 1.
  4. **The outer image and the centre image as two layers of the runtime** (`fovea_res_layer`), in
     place of the compose. These layers have no soft edge.
  5. **Pictures between the walks, by depth.** For a view that is still too heavy, a picture
     between two walks comes from the last picture and its depth. This is the largest work, and
     the edges of near objects can show faults.
- **N19. The model cull distance on a busy land.** `cull_distance` (default
  5x) draws the map objects to 3500 units, and the fog moves out as far
  (`backend.md`, "Models are drawn further out"). On Land 1 it costs no
  measurable frame time, and the list of map objects stays under 1100 of
  2999. Open: a land with large towns and forests (Land 2 to 5). It needs a
  test save of such a land, and `make_test_save.ps1` makes saves of Land 1
  only.
- **P25. The glow alone and the ray over the panels.** With
  `ray_glow_only=1` the glow is the whole ray, the tether and the arc, with
  no tube under it (`hand-model.md`, "The glow ribbons"). With
  `ray_over_panels=1` the menu ray goes over the panels in the room as a
  quad layer of its own, with a mark at its end (`ui-layer.md`, 25). The
  desktop runs show the arc, the tether and the menu ray with each look, and
  the strip over the welcome page and the settings window in the view for
  videos. Open: a run of `village_run.ps1 -Kind rayover` with the hand below
  the eyes, which ends the strip on a button with its mark on the paper.
- **F5. The stream of the influence at the hand.** The hook that ends the
  stream of the game at the Hand's point is built, but no desktop run had a
  stream of the game, so `influence_stream_hand` is 0. The update of the
  game keeps the land point of the interface, which in the God view can be
  away from the land under the Hand (`miracles.md`, "The influence at the
  hand"). The report of `scene_run.ps1 -Kind edge` has the state of the
  stream. Open: which state stops it, and then a run that shows the stream
  at the Hand.
- **F5. The hologram at the edge.** The glow of the edge of the influence is
  a red hologram on the wall of the edge, near each hand, on the discs of
  the game (`influence_wall_look=1`, `miracles.md`, "The glow at the edge").
  It waits for its desktop runs (`scene_run.ps1 -Kind edge` and `-Kind
  edgeside`). Open: the look in a headset (size, light, the speed of the
  rings), and an edge of an influence ring of a script, where the hologram
  comes from the search of the rays.
- **N1. The frame time.** `bench_run.ps1` measures six fixed scenes on the
  desktop, and `profile=1` gives the same numbers for a session in a headset
  (`backend.md`, "Frame cost"). Still open:
  - **The log lines** of the frame time in a headset are in
    `headset-checks.md`, "Play as normal and send the log". A desktop run
    has no wait for the runtime, no copy to the bridge, no compositor and
    no encoder.
  - **Frames of about 100 ms in a headset.** The worst frame of many report
    windows of a session through Virtual Desktop is 94 to 108 ms, with
    `backend=vulkan` and with `backend=dxwrapper`. The time is in the calls
    of the runtime, not in the game turn, the scene walks or the files
    (`backend.md`, "The slow frames"). The line `runtime calls:` names the
    call.
  - **About 100 frames a second at 120 Hz with Virtual Desktop.** The cap
    is in the runtime. With Virtual Desktop on a Quest 3 the headset offers
    one rate, 120 Hz (`XR_FB_display_refresh_rate`), and `xrWaitFrame` gives
    a display period of 8.33 ms. But the display times come 10.0 ms apart
    (99.3 to 99.6 Hz), with 0 to 2 late steps in 450 frames, and the scene
    walks of both eyes take about 3 ms of the frame (`backend.md`, "The
    refresh rate of the headset"). `refresh_rate_hz` cannot help, because
    there is no other rate.
  - **The wait at the stop of a sound.** The mod skips the 20 ms that
    the audio DLL waits on the frame thread at the stop of a sound
    (`audio_stop_wait=0`, `audio.md`, 7.6). In a desktop run the worst frame
    at the village, less the copy to the monitor, is 10 to 12 ms with the key
    at 0, and 32 to 72 ms with the key at 1. A headset session showed no fault
    of the sounds. A sound that QMixer plays (an advisor, the speech of a
    story) keeps the wait.
  - **The wait of about 30 ms in the audio turn on the Steam Frame.**
    4 to 10 times in 10 s `QSWaveMixPlayEx` takes 17 to 30 ms for a sound of
    the atmosphere, also with `audio_stop_wait=0`. The time is in the code of
    QMixer, in no wait (`audio.md`, 7.8). The byte of `audio_play_fix` does
    not remove it, and `audio_play_now=2` (the sort and the pump return at
    once in that call) does not either: nearly all of the time is in `open`
    (RVA `0xC230`, `audio.md`, 7.8). The waves of the atmosphere on island 2
    include MS ADPCM waves. The open of one takes 23 to 26 ms on the frame
    thread: `acmFormatSuggest` is 22.5 to 23.8 ms and `acmStreamOpen` 2.0 to
    2.6 ms. `audio_acm_cache=1` (default 0) keeps the answer of
    `acmFormatSuggest`, and `audio_acm_cache=2` also keeps the driver of
    `acmStreamOpen`. Next: a Frame session with `audio_acm_cache=1`,
    `audio_acm_compare=5`, `audio_play_parts=1` and `time_log=1` (the hits, the
    compares and the time of an open are in the log), then one with
    `audio_acm_cache=2`. Then set the default. `audio_open_loader` stays a
    test value: the reference counts of QMixer have no `LOCK`, so a default
    above 0 needs the release on the loader thread and each add on the frame
    thread under one lock, or another way to take the open off the frame
    thread. Not built: the `QSWaveMix` calls of the audio turn on a thread of
    the mod.
  - **Slow frames that stay.** The first
    use of a texture or of an effect file makes an eye pass 8 to 15 ms
    longer, when Windows does not have the file in memory. One time a minute
    the game writes the alignment of the player and the mind of the Creature.
    Its file calls take 2 ms most times, and they took 28 ms one time. A load
    of these files before the game needs them is not built.
  - **The texture of 32 by 32 that the land draw writes.** With two writes
    in one scene walk, its two locks take 0.6 ms for each eye pass. That is
    1.2 to 1.5 ms of a frame of 8.7 to 10 ms at the door of the Temple. With
    one write the lock takes 0.005 ms. The cause is not known: a wait for
    the GPU in the backend is the first candidate. First step: read what
    `Lock` of dxwrapper does for a surface that a copy used in the same
    frame. Then name the thing that the second write is for (the list at
    `0x00FAA7E0` of `LH3DIsland::Draw`).
  - **The default of `frame_send`.** It stays 0 until a play with each value
    and the subtitle card on (`headset-checks.md`, "The cost of a key in a
    headset") compares them (`subtitle_read_ahead=1`,
    `ui-layer.md`, 17.2). A desktop run cannot show it: with no headset the
    mod makes no wait for the GPU.
  - **The cost of the lock of the Direct3D 9 device.** A device for two
    threads takes a lock on each call. A sampling profile of the frame
    thread had 3.8 percent of its samples in critical sections (`backend.md`,
    "The clock of the game and the hooks of dxwrapper"). The device of
    dxwrapper is a device for two threads in each case: the log has the
    behavior flags 0x84 with `test_keys` 0 and 1, so the request of the mod
    (`test_keys=1` or `frame_send=2`) changes nothing, and `bench_run.ps1`
    gives the same numbers for both values. To do: find whether dxwrapper
    calls the device from a second thread. If it does not, a test that takes
    `D3DCREATE_MULTITHREADED` off at the creation of the device when
    `frame_send` is not 2, and a benchmark of it.
  - **A render thread for the Direct3D calls.** The Direct3D draw calls are
    3.3 to 4.8 ms of a frame in scenes 1 to 5, and with one walk the game
    makes 1580 to 2610 state calls in a frame. A thread that gets a record
    of these calls and gives them to Direct3D takes that time off the frame
    thread. The thread
    is built as a test key (`render_thread`, `backend.md`, "The render
    thread"). In a desktop run it takes the driver off the frame thread.
    With the answers of the calls of a texture that take no lock, the drop
    of the calls that change nothing and the buffers of the render thread,
    a frame is 7.5 to 7.7 ms in place of 10.7 to 10.9 ms. In a headset over
    the whole island (with the lock answers only) the scene walk is 5.5 to
    5.9 ms in place of 11.3 ms, and a frame 11 ms in place of 16 to 20 ms.
    The render thread, which is the driver, sets the time of the walk. In a
    headset (SteamVR, Steam Frame) over the island with the filter, the
    buffers and one draw for both eyes, the scene walks are 4.5 to 5 ms, and
    the flush (`gpu_flush`) makes the time from `xrWaitFrame` to the end of
    `xrEndFrame` 6.3 to 7.4 ms in place of 8.6 to 9.1 ms. Desktop runs give the
    same picture with `render_thread` at 1 and 0: 0.06 to 0.15 percent of the
    pixels differ, as between two frames of one value. Open: the cost in
    `headset-checks.md`, "The cost of a key in a headset", then the default
    value of `render_thread`. One start stopped at the logos with
    `gpu_flush=128` (`hang_watch=1` shows the place if it comes again). The two
    vertex loops of the engine run with SSE, with the bits of the game
    (`fast_transform`, `backend.md`, "The vertex loops with SSE"): 0.25 ms less
    of the scene walks in a desktop run. The calls of Direct3D 7 of a walk can
    go to a thread of their own (`d3d7_thread`, `backend.md`, "The thread of
    Direct3D 7", on by default), with the same pixels in a desktop run and no
    fault in a headset at 144 Hz. The end of a walk does not wait for the render
    thread (`render_thread_late_end`, `backend.md`, "The late end of a walk"):
    the scene walks over the island are 2.6 to 3.0 ms in a headset at 144 Hz.
    The threads of the mod spin only for a short time and then sleep
    (`thread_wait`, `backend.md`, "How the threads wait"): in desktop runs with
    4 to 16 cores the frames are as fast, and the process uses about 0.5 cores
    less. The D3D7 thread merges the rows of draws with one state
    (`d3d7_thread_merge`), so the frame thread copies the vertices of a draw one
    time. **The goal of a port to the Steam Frame and the Quest 3 standalone**
    needs fewer layers of x86 code for each draw. The plan, in steps that a
    desktop run can check: (A) a Vulkan renderer of the scene walks in place of
    dxwrapper and Direct3D 9 (`backend.md`, "The Vulkan renderer"; the same
    picture as Direct3D 9, with the hand and the overlays of the mod and both
    eyes in one pass). Its eye pictures go straight into an OpenXR session with
    Vulkan (`xr_vulkan=1`, "The session with Vulkan"), and so do the pixels of
    the CPU: the cards, the walls of the cut scene window and an eye picture of
    the CPU (`xr_vulkan_upload`). The draws for the panel target go into a panel
    of the renderer, which the subtitle card and the panel layer take
    (`vk_panel`). In a session with Vulkan a walk of one walk goes to the
    renderer alone, and the D3D7 thread gives no draw of it to dxwrapper
    (`vk_only`, "The walk without Direct3D 9"; on the desktop the render thread
    then has 1 draw of a frame in place of 773). Still on Direct3D 9: a menu, a
    loading screen, a video and a frame of two walks, which go through the GPU
    bridge and a relay of Direct3D 11, and the calls that set a state. (A2) A
    backend of the mod makes DirectDraw 7 and Direct3D 7 on Vulkan in place of
    dxwrapper (`backend=vulkan`, `backend.md`, "The backend of the mod on
    Vulkan"): on the desktop the game runs from the logos to the start dialog of
    a new game with no Direct3D 9, and the renderer draws each draw. One walk,
    or the two walks of the eyes, give one image with both eyes for a session
    with Vulkan, and the help text goes into the panel of the renderer for the
    subtitle card. `ddraw_census=1` with `backend=system` lists the calls that
    the game makes. With `backend=vulkan` the mod uses only Vulkan
    (`vulkan_only`, `backend.md`, "Only Vulkan"): no part for Direct3D 9 starts,
    the session has no Direct3D 11 device, and the window of the game has a
    swapchain of Vulkan (`vk_window`, "The window with Vulkan"). A session in
    the headset ran with no Direct3D 11 device. The installer keeps dxwrapper:
    it is the renderer Direct3D 9 for a PC with no Vulkan 1.3, and the renderer
    of "Play, no VR". Next for the backend: find the part of the long frames
    after the load of a saved game that the hold of the last frame does not take
    (`xr_hold_ms`, the line `slow frame`, `backend.md`, "The hold of the last
    frame"). Foveated rendering (`foveation`, `backend.md`, "Foveated
    rendering") gives the eye pictures of the renderer fewer shades away from
    the gaze of an eye tracker, or from straight ahead, and foveated resolution
    (`fovea_res`, "Foveated resolution") makes each eye picture from an outer
    image with fewer pixels and a centre image with more. On a Radeon RX 7900
    XTX neither takes time off the GPU, because the time is in the draws. The
    centre follows the gaze of OpenXR or of OSC (Steam Link of the Steam Frame).
    On the Steam Frame the centre of `fovea_res` follows the gaze of OpenXR.
    There `fovea_res_layer=1` shakes the picture (`backend.md`, "The middle as
    its own layer", the known limit): find the cause in the compositor, or keep
    0 there. Not run with an eye tracker: `foveation`. Check with
    `foveation_show=1` that the marks follow the gaze (`backend.md`, "Foveated
    rendering", the open points). (B) one draw for many copies of a mesh (about
    870 draws of a frame are 36 meshes); (C) a new renderer of the land, after a
    count of its draws. (D) The headset app for Android (`android.md`): an APK
    for the Quest 3 and for other headsets with Android and OpenXR. It has an
    OpenXR session with Vulkan, the settings of the launcher in a menu in the
    form of the menu of the game, and the check of the game folder on the
    headset. It has the Vulkan renderer of the mod, which draws a stream of the
    walks that the mod records on the PC (`vk_stream_record`): in the emulator
    its pictures of the game are the same as the pictures of the PC, to the
    pixel (`emu_replay_test.ps1`). The emulator also runs the menu
    (`emu_test.ps1`) and the session with an OpenXR test runtime
    (`emu_xr_test.ps1`). The stream can be live: the mod writes each frame of
    its walks into a ring file (`vk_stream_live`), and a thread of the app reads
    the frames from the same file (`stream_live`). In a desktop run the frames
    of the ring have the bytes of the record of the same run
    (`live_stream_run.ps1`), and in the emulator the app takes them from a
    second process and draws the pictures of the PC (`emu_live_test.ps1`). The
    APK carries Wine 11.0 in the WoW64 mode for each ABI (`wowbox64.dll` of
    Box64 on ARM64), and the app unpacks it, makes a prefix and runs a 32 bit
    test program in it (`android.md`, "Wine in the app"): in the emulator with
    the x86_64 tree. "Play in VR" and "Play, new game" start `runblack.exe` in
    Wine with the `ddraw.dll` of the APK as a known DLL, `vk_device=0`, the live
    stream and the input channel (`android.md`, "Play in the app"). On a Quest 3
    the arm64-v8a tree runs the test program and the game with `wowbox64.dll` to
    the dialog of a new profile, and the mod reads the input channel under
    Box64. The logos, the videos, the front end and the dialog boxes go into the
    live stream as screen frames, and the app shows them on a quad fixed in the
    room (`android.md`, "The screens with no scene"). A controller points at a
    dialog box on the screen quad, the trigger clicks, and the keyboard of the
    app types the name of a new profile. A controller points at a dialog box of
    a frame of the scene on the panel quads in the same way (`android.md`, "The
    dialog boxes by the controllers"). `push_profile.ps1` copies the profile of
    the player of the PC (the values of the registry and the files) to the game
    folder of the app, and "Play" imports its values into the registry of the
    prefix one time for each copy (`pc_profile`, `android.md`, "The profile of
    the PC"). Open: the profile of a player in a headset (Q1), the sound in a
    headset (the audio driver `wineaaudio` of patch 0003 gives the sound of Wine
    to AAudio, `audio.md`, "In Wine on Android"), the pace of the frames with
    `vk_device=0` in a headset (`vk_pace`, `android.md`, "The pace of the
    frames"), the test reader of `input_channel_run.ps1 -Live` with screen
    frames, the time of the copy of a screen frame (the logos at 35 frames a
    second with the pace), and the screen quad in a headset. The input of the
    session goes to the mod through a second file, the input channel
    (`input_live` in the app, `xr_input` in the mod, `android.md`, "The input
    channel"): the views, the hands, the display time, the focus and the
    recenter, and the pulses back. Open for the input channel: the input of a
    game frame in a headset (the game did not reach a frame of the scene there
    yet). The mod gives the app the values of the projection layer of each frame
    in the frame of the live stream (a record `kView`), and the app shows the
    eye pictures in a projection layer (`stream_projection`, `android.md`, "The
    projection layer"). The cards, the panel quads and the cut scene window of
    the session go into the same frame (records `kCard`, `kCardPicture`,
    `kPanel`, `kWindow`), and the app shows them as quad layers over the
    projection layer in the order of the session of the mod (`xr_input_layers`,
    `stream_layers`, `android.md`, "The quad layers"). Open for it: a real
    headset, and the intro, which gives no frame of the live stream. With
    `vk_device=0` the mod makes no Vulkan device, so only the app draws the
    walks (`android.md`, "No Vulkan device of the mod"). Open for it: the
    subtitle card, which then has no pixels, and a frame of the game in Wine. A
    frame goes into the ring at the end of its first walk with both eyes, and in
    a desktop run each frame after a frame with the whole state was at the
    reader in its own game frame. A skip of the app and a drop of the writer
    keep the records of state and lose only the walks, and the queue of the app
    keeps 3 frames whole, so a slow reader does not hold the frame thread of the
    game. Open for the live stream: the ring file under Wine and Box64, the
    walks that keep the picture of the walk before (`vk_keep_picture`, the
    intro) after a frame with no walks, and a run in a scene with more than one
    walk in a frame or with records after the walk with both eyes (no run had
    such a tail, and the move of the tail has no math check). A frame with
    kStart drops the tail before it, also its kGaze and kOnlyFrame. No desk run
    compared a frame of state of the writer with the record. The picture of
    frame 3002 of the test save changes with the time of the run, so a desk
    comparison of pictures needs runs close in time (A, B, A). With the render
    thread, one walk draws most batches in one draw for both eyes
    (`one_walk_merge`, `backend.md`, "One draw for both eyes"). Before the
    thread, with one walk, a view of the whole island has about 1500 batches and
    a scene walk of 9.5 to 11 ms, and the runtime then shows 45 frames a second.
    Of that walk about 6 ms are Direct3D 9, the driver of AMD and the kernel,
    and less than 3 ms are the game (`profile_sample`, `backend.md`, "Where the
    time of the scene walk goes"). So the thread takes the calls of dxwrapper to
    Direct3D 9, not the calls of the game to dxwrapper. For 90 frames a second
    the scene walk must be under about 6 ms, because the frame waits 3 to 4 ms
    for the GPU after it. Most of that wait is the driver, which gives the calls
    to the GPU late: the GPU works about 2 ms on the walks of a frame
    (`profile_gpu`). DXVK can take the place of the Direct3D 9 of Windows
    (`d3d9_backend=1`, `backend.md`, "DXVK under dxwrapper"): the render thread
    is busy 0.95 ms in place of 2.1 ms in a desktop run, with the same picture.
    Open: the check in `headset-checks.md`, a GPU of NVIDIA or Intel, a PC with
    two GPUs, and then the default value. The installer does not ship DXVK yet.
  - **The other render ideas have a small gain.** A projection off the centre
    for each eye: a view of 70 to 98 degrees changes the walk by 4 percent
    (`backend.md`, "A wide view is nearly free"). So a narrower cull saves
    0.4 ms at the most. The gain for the GPU is not measured. The game turn
    off the render thread: a game turn is 0.4 ms in one frame of six, which
    is 0.05 to 0.11 ms as a mean. So the mean does not change. It can only
    take the turns of 4.5 to 9.5 ms out of the frame. A cache that drops a
    state call for the value that the state has: the game makes no such
    call, because it keeps the values itself (`backend.md`, "Draw calls with
    one state"). With one walk, each of its 1580 to 2610 state calls in a
    frame changes a value. The texture filter of the mod adds about 4 calls
    in a frame.
- **N2. The hands that react.** In a headset the fingers rest on objects
  that move (rocks, trees, villagers), but not on buildings, and the player
  saw nothing of the fingers in the sea. The mod now offers to the pick each
  object whose ball round its box is near the Hand's point or near the
  middle of either hand (`hand_feel_offer`), gives the part of a hand of the
  mesh under the sea the color of the sea (`hand_feel_water_look`), and
  starts a ring of the game on the water where a finger tip goes in
  (`hand_feel_ripple`). A desktop run shows the offer at work: 824 more
  objects near the hand in 450 frames, at no measurable cost. Eye pictures
  with Vulkan and with dxwrapper show the sea and the hand over it in both
  eyes. A finger tip in the sea starts the ring of the game with
  `hand_feel_ripple=1`, and none with 0. In the test save `temple` the
  fingers go into the body of the Village Store, and with `hand_feel_offer=0`
  the pick gets no more objects and the fingers act the same
  (`village_run.ps1 -Kind wall`). Not shown yet: a finger at the wall of a
  house, and the look of the water in a picture. The plain sea of the God
  view writes no depth, and no layer of the water that shows a thing in it
  is missing (`hand-model.md`, "What the game draws of the sea"). Not
  proven: why the fingers did not rest on a house in the headset. The code that reads the game shows two causes:
  the hand that does not point got only the bodies near the Hand's point, and
  a palm in the box of a small house turns that body off. Open: a body is the
  box of the whole mesh, and the boxes of the parts of a mesh (walls, roof)
  are not known in memory, so a round or a large building has no body at its
  walls when the hand is smaller than it. Not shown on the desktop: a rock and
  the crown of a tree as a body, a villager that walks into a still hand, a
  tree taken with the palm to the side, and a picture from the side of a
  finger tip that bends at a wall. The box of the Creature does not follow its
  animation, and the box of a hut with its roof down to the land stops a
  finger in the air before the roof. The thumb does not go round the trunk of
  a tree in the Hand: the open thumb is in the trunk. The idle wave is off
  (`hand_feel_idle`).
- **N3. The hand model.** The game's finger animations are not played (a
  pose is a curl for each finger). The shadow of the Hand on the land has
  the game's fingers. The textures of an edited hand file are not read.
- **N4. The Temple.** Not done: a click on a frame of the Challenge Room,
  a text of the Library, a value of the options box, a drag of a scroll.
  Not seen in a run: the picture of a save in the Save Game Room, which the
  game now makes from the eye picture (backend.md, "The save picture").
  Suspects: the waist point is 8.4 units higher after a load, the ray ends
  short of the pointer in the left eye in a dialog box inside the Temple,
  and the leash posts are 1.4 cm from each other at the scale of the start. In the ring menu: with
  `button_layout=0` a thumb on the left stick can open it, and a jump to the
  town or to a bookmark takes the heading of the game camera.
- **N5. The village.** Not run: food, an animal or crops set down at a
  store, the altar, a gift to the Creature, the red halo outside the
  influence, a Workshop that gives scaffolds, villagers that make chants,
  and Heal from its icon at the Worship Site.
- **N6. Miracles.** Not run: each reason of a refusal but "needs a
  Creature" and "needs a circle on the land". Open:
  - Heal has no run: a test save of Land 1 has no sick villager. The log
    check is in `headset-checks.md`, "Play as normal and send the log".
  - A shield of a small circle loses about 24 of its life a second, so a
    circle of 23 cm lasts about 3.5 minutes. The cost grows with the square
    of the size, as in the plain game.
- **N8. The scripts and the engine camera.** The scripts get the place of
  the player. Not changed: the 114 markers at "camera position",
  `GetFacingCameraPosition`, and the other readers of the engine camera.
  Not run: the scoreboard and the Creature scrolls.
- **N9. The sound.** `audio_loops_keep` is built and not proven. The open
  points of the sound are in `audio.md`.
- **N10. The cut scene window.** Limits: the plane stays near for some
  seconds after a near thing left, a thing smaller than one grid cell can
  be in front, and the advisors are in the measure.
- **N11. The God view.** No desktop run: the events that a
  step of the tutorial waits for, and the load of a saved game in play (a
  run with a test save shows the load at the start of the game). Of the
  teleport, not run: the red arc on the sea, the aim at the sky, the cancel
  with B and the mouse double click (`god-view.md`, 14.9). Some cameras of
  the game still glide: the follow of the Creature after a double click, the
  pull back past 3500 units, the arrow keys and the automatic tilt
  (`backend.md`).
- **N12. The far grab.** No desktop run: a villager in the sea, a Field as
  a target, a take while the Hand is outside the influence,
  and a second object in the Hand (`grab.md`).
- **N13. The exact Hand.** No desktop run in the PlayAnim state. The Totem
  and the Citadel states are not read (`hand.md`).
- **N14. The throw.** Not tested: a rock thrown into a pile (`throw.md`).
  Open: after a landing at 6.5 m/s a thrown villager hops 2.2 m high in the
  room, for 2.8 s. The bounce keeps 50 percent of the speed, in the time of
  the game. The physics constants of the game give a villager much more
  damping than a rock (`Data\PhysicsConstants.txt`, row 7: SpringK 26.25,
  DampK 3.05), so a large hop is not expected. Not known whether the plain
  game makes so high a hop.
- **N15. The interface panel** (`ui_panel=1`). A desktop run shows the
  route of the batches to the panel and none in the eyes (`ui-layer.md`,
  section 9). Desktop runs with the input channel and the emulator show the
  panel quads and the clicks of the ray on them. Not run: a scripted click
  with the menu only on the panel with a session of the mod on the PC, and
  the quads in a headset.
- **N17. Headset coverage.** Quest 3 through Virtual Desktop and Steam Frame
  through Proton have game coverage. Other headsets and runtimes need tests.
  A headset with canted views (Pimax) needs a test: with `eye_cant=1` each
  eye draws with the turn of its own view (backend.md, "Canted views"). A
  player with a Pimax must say if the two pictures fuse in play, and the log
  line `eye cant` gives the cant of his views.
- **N18. The x64 host for Steam Frame** (`xr_bridge=1`). Not tested: all
  controls, the haptics, the loss of the device, long sessions, and the
  install and the uninstall of the add-on on a clean setup. The game ran
  at 42 to 44 frames a second, and the host sends each frame on the frame
  thread (`backend.md`).
- **N20. 120 frames a second in the headset app on a Quest 3 with no PC.**
  The goal: the game at 120 frames a second in VR in each situation of the
  game. The size of the eyes has a cap at the panel of a Quest 3
  (`eye_size_max=1`, `android.md`, "The size of the eye pictures"), and the
  app and the mod log each 10 s where the time of a frame goes
  (`time_log=1`, `android.md`, "The time log of the app", `backend.md`,
  "The time log of the mod"). The emulator shows both. The thread of the
  session makes its frames at the display rate with no wait for the play
  of a picture or for the GPU: a play thread plays each picture into a ring
  of 3 slots, and each frame of the session shows the newest finished one
  (`frame_loop=1`, the row "The headset gets each frame (no wait for the
  GPU)", `android.md`, "The frame loop"), the renderer has a second queue
  with a lower priority (`frame_loop_queues=2`), and the session does not
  wait for a swapchain image that is not free (`frame_loop_image_wait=0`).
  The play thread keeps at most one picture at the GPU that is not finished
  (`frame_loop_gpu_depth=1`), and then plays the newest frame of the game.
  The time log splits the step "the layers" of the thread of the session
  into its parts, with the CPU time of the thread, and gives the GPU parts
  of the walk with the counts of a walk (`gpu_parts=1`, `android.md`, "The
  GPU parts of the walk"). In a Quest 3 with FEX and `frame_loop=1` the
  session made 50 to 64 frames a second at 72 Hz: the step "the layers"
  took 7 to 25 ms of a frame, nearly all of it CPU time of the thread in
  the kernel, the GPU was 75 to 99 percent busy, and a walk took 10 to 16
  ms of the GPU also with `fovea_res=1`. Next in the headset: the line of
  the parts of the layers (which part takes the time), the GPU parts of the
  walk, the frames of the session a second and the age of the shown picture
  with `frame_loop_gpu_depth` 1 and 0, and the judder and the latency that
  the player feels. Then the largest part gets a fix. Then a run in the
  headset with the profile of the player in the scene of the save, at 72,
  90 and 120 Hz and at `resolution_pct` 50, 75 and 100, to read the lines
  `time` and `time log`: the time of the GPU, the waits in `xrWaitFrame`
  and for the GPU, the CPU time of the main thread of the game under Box64,
  and the load of the cores. The step after it comes from those numbers.
  Before a merge to main: the desk run on the PC (the build before, the new
  build, and the new build with `time_log=0`).
  Open: why the Quest 3 asks for 4096 by 4291, and a way to find the hot
  code of the game under Box64 (`android.md`, "The hot code under Box64":
  no way works yet).
  **The draws and the copies of the session.** The renderer leaves out each
  bind of a state that is bound, sorts the opaque draws and merges draws
  with one state (`vk_bind_skip`, `vk_draw_sort`, `vk_draw_merge`,
  `backend.md`, "Fewer state changes and fewer draws"), and the session
  records each copy of the same size one time and gives each command buffer
  of its ring a pool of its own (`xr_copy_once`, `xr_pool_reset`,
  `android.md`, "The frame loop"). Next in the headset: the GPU time of a
  walk, the counts of the renderer in the line of the GPU parts, and the
  parts "the eye pictures" and "the panel image", with the five keys at 1
  and at 0.
  **The copies of the eye pictures.** A copy of a picture is bound by the
  memory of the GPU (about 108 MB of an eye picture at 4096 by 3456). With
  an atlas the renderer writes and the copies take only the part with the
  two rects (`vk_atlas_area`, `xr_eye_rects`), with `xr_eye_direct=1` the
  play thread copies straight into two swapchains of each slot and the
  session copies nothing for the eyes, and `frame_loop_hold=2` shows each
  picture in 2 frames of the session (`android.md`, "The copies of the eye
  pictures"). The time log gives the GPU time of each kind of copy. Next in
  the headset (Steam Frame and Quest 3): the line of the copies with
  `xr_eye_direct` 1 and 0, the memory, and the judder of a flight of the
  camera with `frame_loop_hold` 2 and 0. With `fovea_res=1` the pass that
  makes the eye picture can go straight into the swapchain images
  (`xr_compose_direct`, "The compose into the swapchains" in `android.md`).
  **The way of a draw to the app.** The device of the mod gives no call
  that changes nothing to the renderer and the stream, a thread of the
  stream puts each frame into the ring, and a table finds the surfaces of
  the stream (`vk_state_skip`, `vk_stream_worker`,
  `vk_stream_surface_cache`, `backend.md`, "The way of a draw to the
  headset app"). Next in the headset: the lines `render chain` with
  `render_chain_time=1` and the three keys at 1 and at 0, the time log, and
  the stutters with the stick. Then the next steps from those numbers: the
  pixels of a 16 bit texture in the stream as they are (half the bytes and
  no conversion on the frame thread, a change of the app), and the state
  of the stream (`StreamState`, 40 bytes and six calls) in a record only
  when it changes, not in each draw (a change of the stream and of the
  app).
  **The stop of the game** 6 to 15 s after the load of a saved game on the
  Quest: the main thread of the game runs at 99 percent of one core in user
  mode, and `wine.log` names the critical section 0x7AF441E8 in each stop:
  `box64_context.mutex_dyndump` of `wowbox64.dll` (RVA 0x4741E8), the lock
  of Box64 for the translation of a block of x86 code. The other threads
  wait for it. With `BOX64_DYNAREC_STRONGMEM=3`, `BOX64_DYNAREC_BIGBLOCK=0`
  and `BOX64_DYNAREC_LOG=1` the game stops too, and the log of Box64 has no
  message of an abort or of a repeat. Next run in the headset: the same
  save with the row "Translator of the game (Box64 or FEX)" at FEX
  (`wow64_cpu=fex`, `android.md`, "FEX as the CPU of the 32 bit code"),
  with `fex_env=FEX_SILENTLOG=0` for the lines of FEX in `wine.log`, then
  the frame rate with `FEX_X87REDUCEDPRECISION=1`.
- **N21. The scene walk of the game under FEX.** On the Quest the game
  takes the x87 paths of the engine (`backend.md`, "The vertex loop of the
  land with SSE"). `land_fast=1` makes the vertex loop of the land with SSE.
  Next in the headset: the `walk parts` lines with `land_fast=1` and 0, and
  with `game_sse_path=1` (the SSE paths of the game). Open: the exclusive
  time of `fn_005E5CD0` (4.5 to 8 ms on the Quest) is in its scan of the
  objects near the camera and in the draw methods of the objects on the
  land (the entry `+0x610` of each vtable), which no timer has yet, and `fn_00876910` (the triangles of a patch) is still x87
  code.
- **N24. The SSE paths of the game, one at a time, in the headset**
  (`game_sse_parts`, `game_sse_check`, `backend.md`, "The SSE paths of the
  game, one at a time"). On the PC each of the 13 parts with its SSE path
  gives the picture of the x87 path. The compare shows rounding, and three
  differences of the output: the SSE code of a patch (bit 1) sets more
  clip flags of a vertex off the screen, the light at a cell (bit 256)
  gives the alpha 0xFF to the second color (the x87 code 0), and float to
  integer (bit 4096) gives 0x80000000 for an infinity (the x87 code 0).
  Next on the Quest: `game_sse_parts=4095` with `game_sse_check=1` for the
  part that gives the large polygons under FEX, then the halves and the
  bits in the order of the doc. Open: the time of each path under FEX.
- **N33. FEX and the cores of the Quest.** On a Quest 3 with FEX the main
  thread of the game takes 87 to 89 percent of one core, and the threads of
  the game and of the app run on the cores 3 to 5 (the line of the threads
  of `time_log`). On the Steam Frame (8 cores) the game made 72 frames a
  second, and the time log gave the cores 40 to 69 percent busy. There 46
  to 145 of 721 takes of the session in 10 s show an old picture, and the
  player sees a stutter in a strafe with the stick. Most of these takes
  come from the phase of the frames of the game to the takes: the copies
  of the GPU end after the take, or a frame of normal length comes 7 to 9
  ms after it. A long game turn (19 to 33 ms, 4 to 8 times in 10 s) gives
  only 2 to 10 of them (`android.md`, "The long frames"). The line
  `take phase` of the app shows why: the frame of the game starts about 1
  ms after the write of the slot, and its picture needs 13 to 15 ms (a
  deviation of 2.4 to 5.1 ms, in a session with a wait at the take) to the
  end of its GPU work, so it ends at the next take. A later take in the
  frame of the session gains less than 0.2 ms, and with SteamVR about 2 ms more work of the session after
  `xrWaitFrame` makes the compositor time out presents. The Frame runs an
  APK at 72 Hz, the rate of SteamVR for each app (`android.md`, "The
  display rate"). At 90 Hz the game keeps the rate (88.6 to 89.2 frames a
  second), and SteamVR gets 89 pictures a second, but 35 to 102 of 900
  takes in 10 s have no new picture, and a newer picture replaces many
  pictures before their take: the player sees micro stutter.
  `frame_queue=1` shows each picture in its order with a delay of one take
  more, in a ring of 4 slots, and the line `late picture` gives the new
  pictures by their delay in takes and the pictures that did not show
  (`android.md`, "The frame loop", "The queue of the pictures"). On the Frame at 90 Hz with
  `frame_queue=1` the frames of normal length give almost no take with no
  new picture, and nearly each take with no new picture comes from a long
  frame of the game. Open: runs at 144 Hz, and pictures between the frames
  of the game in the app. In a
  desktop run about 57 percent of the samples of the long game turns are
  the first use of an animation of the Creature: the game makes its mixed
  animation with the x87 instructions FSIN, FCOS and FPATAN, which FEX runs
  as calls. Other long turns of up to 6.7 ms in a desktop run are a new
  BuildingSite (`fn_0043AE10`, with `FSIN` and `FCOS` in its own code),
  `Morphable::MorphTexture`, `GScript::Process` and a load of a mesh of
  the Temple through `fn_008067F0`. `anim_trig=1` makes the four functions
  of the animations with no x87 instruction (54 percent less time for the
  animations in a desktop run, `backend.md`, "The long game turn"). On the
  Frame with `anim_trig=1` the animations are not the long turn:
  `GetSetAnim` made at most 3.62 ms in one call and 0.2 to 29 ms in each 450
  frames, and the long game turns stay at about 30 ms (75th percentile 30.4
  ms, 28 to 47 long frames in 10 s). `turn_parts=1` puts a timer on each
  function that the game turn calls, and the lines `long frame` and
  `turn parts` give the time of each part. Open: which part takes the 30 ms
  on the Frame, how often a session there makes a BuildingSite, and how long
  these turns take under FEX. Next on the Frame: `turn_parts=1`,
  `slow_frame_ms=20`, `anim_trig=1`, the same scene for some minutes.
  The plan for the work of a frame on more cores is in `android.md`, "The
  work of a frame on more cores". The app gives each second the core and
  the CPU time of each thread (`thread_cores=1`), binds the main thread of
  the game, the session thread and the play thread to the fast cores
  (`thread_bind`), and gives FEX a code cache in files (`fex_disk_cache=1`,
  `android.md`, "The cores of the headset" and "FEX as the CPU of the 32 bit
  code"). Next in the headset: the cores and their speeds, if the binding
  works there, the frames a second with `thread_bind` 1 and 0, the load
  and the stutters of the second start with the code cache, and the frames
  a second with
  `FEX_TSOENABLED=1 FEX_EXTENDEDVOLATILEMETADATA=runblack.exe` against
  `FEX_TSOENABLED=0`. Open: `SuspendThread` under FEX in Wine stops a thread
  also in the JIT, so a hook that the mod enables while other threads run
  x86 code can hang the game (`android.md`, "FEX as the CPU of the 32 bit
  code", the part "SuspendThread under FEX in Wine"). Open: some runs of
  `emu_game_test.ps1` do not end (no "Quit game" in game 1), with and
  without these keys, and with no log of the app (`android.md`, "The cores
  of the headset"). The timeline (`trace=1`, `backend.md`, "The timeline of
  the game and the app") shows the bars of all threads of the game and of
  the app on one clock, and the picture shown with the place of the camera
  of its frame. The bars of `GLandscape::Draw` and of its callees
  (`trace_draw_parts=1`) have the time and the calls of the device of the
  mod in them (`trace_device=1`), and `tools/trace/trace_self.py` splits the
  own time of each bar into the game and the device, so the next Frame
  session shows which part of the time of `GLandscape::Draw` is the
  work of the game and which part is the calls of the device. Next on the
  Frame: `trace=1` in `bwvr.ini`, play, press a combination of the
  controllers right after a stutter or a jump of the picture (X with the
  click of the left stick, or the left trigger with the click of the right
  stick), take `trace-<time>.json` of the app folder and read it with
  `tools/trace/trace_report.py` or in `ui.perfetto.dev`. Not shown: which
  buttons the Steam Frame gives to the app (the lines `trace : buttons` of
  `bwvr-quest.log`), the cost of the recorder on the Frame, and the clock
  shift that the join finds.
- **N38. The display rate of the headset app** (`display_hz`, the row
  "Display rate", `android.md`, "The display rate"). The emulator with the
  test runtime shows the row, the requests, the event and the pace of the
  game in Wine at 90 Hz. Not shown: each rate in a headset (the feel of the
  head motion, the power and the heat of a Quest 3, the frames of the game
  under Box64), and a runtime with no `XR_FB_display_refresh_rate` on a
  real headset (the Steam Frame).
- **N23. The objects on the land under FEX.** `fn_005E5CD0` draws the
  objects on the land (`backend.md`, "The objects on the land").
  `land_objects=1` makes the vertex loop of the reflection meshes and the
  test of the view of the 3D parts with SSE, and `land_objects_threads=N`
  makes the test of the view of each object before the draws on N worker
  threads. Next in the headset: the `object timer` lines (`object_timer=1`)
  with `land_objects=1` and 0, and with `land_objects_threads=2` and 0.
  `part_draw=1` makes the loop over the triangles of each mesh and the
  vertex loops of `fn_00859530` (a person or an animal) with SSE (`backend.md`, "The parts with SSE"). Next on the Frame: the lines
  with `object_timer=1` and the sites of that section, with `part_draw=1` and
  0. Not shown: the time of the draw of the objects with `part_draw=1` under
  FEX (the goal is less than half of its time with the same picture).
- **N34. The meshes of the 3D parts on the GPU** (`gpu_mesh`, `software-tnl.md`,
  "The meshes on the GPU"). `gpu_mesh=1` takes the transform, the light, the
  clip and the cull of the meshes of `fn_00856D40`, `fn_0085AB10`,
  `fn_0084BA90`, `fn_0085BA30` and `fn_00859530` to the GPU of the app. This
  includes the meshes with a texture on stage 1, with a shift of the texture and
  with a matrix for each group of vertices, and with `gpu_mesh_blend=1` the
  meshes with blend (each part has a key with the old way as the value 0).
  Open: with `gpu_mesh=1` the snow on the roofs of Lethys is missing
  (`gpu_mesh_blend=0`) or flickers (`gpu_mesh_blend=1`), and with blend the
  pictures are old at the game menu when the head turns. The causes are not
  found. Next on the Frame: `vk_equal_depth` 1 and 0 (Ctrl Shift 2) at a roof
  with snow, `gpu_mesh_latch` 1 and 0 (Ctrl Shift 3), and `gpu_mesh_trace=1`
  with `gpu_mesh_blend=1` at the game menu. Then the lines "gpu mesh" with
  `gpu_mesh=1` and `gpu_mesh_timer=1` (the share of the meshes on the GPU path
  and the time of the hook for a mesh, with the goals of more than 95 percent
  and less than 1 us), and `object_timer=1` with `gpu_mesh=0` and 1 (the time of
  the draws of the meshes, with the goal of less than a quarter), and a look at the
  trees, the water, the glass, the people and the Creature. Next in the code:
  the own code of the objects (`fn_00815A70`, the test of the view and the
  choice of the detail) and the class draws that call `fn_0080DB30`.
- **N35. The GPU time of a picture on the Steam Frame** (`backend.md`, "The
  compose of the eye picture"). In the view of the middle of the island a
  picture needs 9.9 ms of GPU as a mean (11.7 ms in the worst window): the pass
  of the outer image 2.5 ms, the pass of the centre image 2.1 ms, the pass that
  makes the eye picture 1.6 ms and the copy into the swapchains 3.6 ms. The two
  small passes cost 1.2 and 2.3 us for each draw, not by their pixels. Six keys
  take parts away (`xr_compose_direct`, `vk_compose_light`, `vk_pass_store`,
  `vk_mres_draws`, `vk_mres_sort`, `xr_panel_rect`, each with the old way as the
  value 0, and each with a row on the debug page, group Render, that acts from the
  next picture in the headset). Next on the Frame: a session with the keys at 1
  and at 0 in the same view (the sort `vk_mres_sort` can change the order of
  coplanar layers, for example the snow on a roof, so check it first), the
  lines "the GPU in ms", "the GPU of the copies" and "the GPU parts of the walk"
  (the goal is less than 7 ms for a picture as a mean), the picture itself (the
  look of the soft edge of the centre, the panel quads with `xr_panel_rect=1`,
  a dialog box over the scene with `xr_compose_direct=1`), and whether the
  compositor shows the swapchain image of the pass as it shows the copy. Open: the
  commands of the mesh draws (`backend.md` says what they need), and a depth of
  16 bits for the two small passes (fewer bins on a GPU with tiles, but
  another precision for the Z test).
- **N37. The one pass with a density map on the Steam Frame** (`backend.md`, "The one
  pass with a density map", `android.md`, "The one pass with a density map"). With
  `xr_density_pass=1` the walk of both eyes draws its list of draws one time, straight
  into a swapchain image of two layers of its slot, with a fragment density map from the
  keys of `fovea_res`. It leaves out the second play of the list of draws (2.3 us for each
  draw), the pass that makes the eye picture and the copy. Next on the Frame: a session with
  the keys of the doc (`xr_density_pass=1`, `xr_eye_direct=1`, `fovea_res=1`,
  `fovea_res_layer=0`, `fovea_res_outer_pct=25`, `fovea_res_center_deg=20`) and the same
  view with `xr_density_pass=0`: the lines "probe :" (what the runtime gives), the line "the
  one pass" of the time log, the GPU parts of the walk, "the GPU in ms: a picture" (the goal
  is well below 5 to 8 ms in the view of the middle of the island), the look of the blocks
  outside the middle, the ring (`vk_density_ring_deg` 0 and 8), the follow of the gaze, and
  `vk_density_depth` 0 and 1. Open: the debug page has no row that shows the path (the page
  is in the game, and the path is in the app), the map of the runtime on its own swapchains
  (the probe logs what it gives), the offset of the gaze with
  `VK_QCOM_fragment_density_map_offset` (it needs a render pass of the old kind), and a depth
  that is not stored for the whole walk.
- **N36. Old pictures in the headset when the menu of the game opens**
  (`android.md`, "The age of a picture in the layer"). On the Steam Frame the
  headset shows pictures of the land from some time before, in a flicker with
  the new ones, when the menu of the game opens and in play when the game is
  slower than the display. The ring and the layer never name a frame below the
  frame before (the math test, and the lines "the projection layer shows the
  picture of frame N" of the app). The cause is not known. Next on the Frame:
  a session with `take_age_log=1` (the lines "take age" name a picture that is
  older than its frame number, a pair of two frames, the writes of the play
  thread soon after a frame named the swapchains, and the walks that started
  with the picture of the walk before), then the same view with
  `ring_slot_order=1` and 0 (the debug page, row "Slot order"), with
  `frame_queue=0` and 1, and with `xr_eye_direct=0` and 1.
- **N39. The control scheme "Trigger is grab"** (`controls=1`,
  `controllers.md`, 5.7). No headset session yet. Only a headset shows how
  the choice of the press feels, and whether the boost of 5 times is a good
  speed.
- **N28. The motion of the stick in the headset app: check it in a
  headset.** Each picture of the game shows at the place in the world where
  the game drew it, seen from the newest place of the player, and with
  `motion_comp=2` that place goes on with the walk of the stick to the
  display time (`android.md`, "The motion of the stick"). The time log has
  the parts of the age of a new picture ("The time log of the app"). Next in
  the headset: a walk with the stick with `motion_comp` 0, 1 and 2 (the row
  "Picture follows the stick (headset app)"), `motion_comp_depth_cm` 100 and
  0, and the line of the parts of the age, to find the part that makes the
  age of about 85 ms. Not built: the depth of the picture for the runtime
  (`XR_KHR_composition_layer_depth`), which needs the swapchains of the
  depth and a copy of the depth in the ring.

### The repo, the release and the tools

- **R2. No desktop run chooses a Creature at the Creature select.** A
  Creature in a pen has no player. A grip on it starts no Creature state, a
  press of the trigger there starts a drag of the land, and the double click
  of A makes the jump of the game (`bwvr/tools/desk/README.md`, "Known
  limits of the recipes"). Find how the plain game chooses a Creature
  there, and make a recipe for it: no run shows that choice with the
  controllers. The recipes with the Creature state start in the test save
  `temple`.
- **R11. The run `vc` of `verify_build.ps1` does not reach the Creature.**
  At a render of 3600 by 2880 and with the profile of the new install, the
  recipe of the Creature at the door of the Temple (test save `temple`)
  has no Creature to touch, so "the Creature state starts" is missing in
  each build. The other runs of `verify_build.ps1` pass. The cause is the
  profile, not the recipe (`creature.md`, section 8): in the test save of
  the old profile the Creature draws only its shadow, and "Keep your old
  Creature" keeps no Creature with the new profile. Needs a profile with a
  Creature, and then `make_test_save.ps1 -Name temple`.
- **R3. The game window is in front in a desktop run.** Windows gives the
  front to the game window of a run on a PC that nobody uses, with no call
  of the game process (`bwvr/tools/desk/README.md`, and `backend.md`, "Who
  moves the cursor of Windows"). The window stays in front until the run
  ends. The real cursor stays where it is. Not tried: a start of the game
  that keeps its window behind.
- **R4. The names of the symbol file.** `bwvr/tools/check_names.py --all`
  lists about 70 decorated names whose parameter classes are not those of
  the Mac symbol, with each class by name (for example an `int` for an
  enum). Nobody has read them one by one. About 100 names with a class as
  a back-reference digit have no Mac symbol to compare with.
- **R5. A file of names of the project.** When contributors start to name
  functions that the symbol file does not have, put those names in
  `bwvr/ghidra/name-corrections.txt` (the tools read it).
- **R7. The word `aimfar` chooses a different object in the test save
  `pen`.** The head is 1 cm from its place in a new game, and some objects
  are at the least distance of the word. So `throw_queue.ps1 -Runs
  treeset,treelean,treeleft,treeright` shows its proof only with `-NewGame`
  (`bwvr/tools/desk/README.md`, "Known limits of the recipes"). Give these
  recipes an aim that does not depend on the least distance, as `aim_queue.ps1
  -Runs ring1,ring0` has with the word `aimturn`, or the start of a new game
  by default.
- **R8. The wait for the lock of a run can stop a caller.** `desk_run.ps1`
  reads the age of the lock after a failed open. When the other run removes
  the lock between those two lines, PowerShell gives the error "Cannot find
  an overload for op_Subtraction". A caller that has the call in a `try`
  block stops there, and no run starts. With no `try` block the wait goes
  on. It happened one time, with two queues of runs that waited for each
  other.
- **R10. Two test keys on one key.** Ctrl and the 9 key of the number pad is
  the test key of `creature_palm_point` (`hand_direct.cpp`) and of the take
  from a forest (`forest_take.cpp`). Ctrl and the 1 key of the number pad
  is the test key of the ways of the ring (`radial_menu.cpp`) and of
  `god_camera_at` (`stereo.cpp`). The debug page has one switch for each of
  the four. The keys of the keyboard that no code of the game or of
  the mod reads are few: a new test key needs a search first (`backend.md`,
  the hotkey table).
- **R12. Checks that a desktop run can prove.** These checks need no person
  in the headset: the scripted controller, the log and the eye pictures of
  both eyes can prove them. Prove a line with a desktop run, then remove the
  line and write the result in the doc of the area. A fault gets its own
  item. Each line names the nearest recipe, or says "needs a recipe".
  - The God view:
    - A steep slope that keeps the land near the hands
      (`move_stick_follow_land`):
      `village_run.ps1 -Kind walk` goes over the sea and a coast, and no
      hill comes within 10 cm of the head. Needs a walk over a hill.
    - The Island Keeper of the Gods' Playground with `script_view=1`:
      the way of `god-view.md`, section 7, from the test save `temple`.
      Needs the measured point of "Yes." and the place of the Keeper. The
      quest `FollowUs`: `spawn_run.ps1 -Choice normal` to the first story of
      a new game.
  - The Hand and grabs:
    - A finger that touches a villager, and the shape of each pose of the
      mesh in the eye pictures: `hand_run.ps1 -Kind one` with a villager and
      a nearer zoom.
    - The shape of the left hand model (`left_hand=1`) in the eye pictures:
      `hand_run.ps1 -Kind left` with the zoom steps of `-Kind near`. At the
      scale of `-Kind left` the hands are too small to judge.
    - The limits of the far grab near an object (10 and 15 cm, `hand
      pick`): `hand_run.ps1 -Kind one` with `far_grab=1`.
    - The glow on a mesh of the sorted list (P15). Needs a recipe: no
      target of `village_run.ps1 -Kind tglow` is a mesh of the sorted list.
    - A living thing beside a rock or a pile gets the halo, and the halo
      stays on it: `village_run.ps1 -Kind choice` at a rock and at a pile.
    - A signpost in the aim, and a signpost beside a miracle or the Creature
      at the Hand: `village_run.ps1 -Kind aimsign`. The word `aimfar` does
      not find the silver scroll at the door, because the draw gives scrolls
      a list of their own.
    - The grip on the Creature at 12 to 15 cm: `creature_recipe.ps1 -Kind
      wide` has grips at 9.7 and 11.1 cm only (the steps `slap` and the
      pull). Needs a step to that distance.
    - A far aim past a near house, and past a pile that is not in a
      building: `village_run.ps1 -Kind aimpast`. At the house of the test
      save `temple` a villager and a scroll are near the Hand's point, so
      the far grab stays off. The pile there is in the body of the Village
      Store.
    - The far grab of a villager in a crowd. Needs a recipe.
    - A tree that comes to rest in a take, at the Hand and from a distance
      (B11). Needs a recipe: `throw_queue.ps1 -Runs treedropend` gives no
      line `hand take ... came to rest`, at 60 frames a second and with no
      pace.
    - A take at a distance from a pile of Food, and from a pile that
      `miracle_run.ps1` makes (B19): `village_run.ps1 -Kind fartake`.
    - The Hand stays at the controller in a bulk take (`hand bulk`). Needs a
      log line: no line of the take frames of `village_run.ps1 -Kind store`
      gives the place of the Hand against the controller.
    - A finger that bends at the wall of a house, a rock and the crown of a
      tree as a body, a villager that walks into a still hand, and a tree
      with the palm to the side (N2): `village_run.ps1 -Kind wall`. At 44.4
      units a meter the Hand's point goes into the body of the house and no
      finger does. At 13.2 units a meter no house is within 2 m of the room.
    - A small object in a pinch does not glide at a change of the zoom:
      `village_run.ps1 -Kind pinch` keeps the pinch, but the rock is too
      small in the eye pictures. Needs a nearer zoom, or a log line of the
      place of the object in the Hand.
  - Throws:
    - A rock thrown into a pile (N14): `village_run.ps1 -Kind throwpile`
      with a rock in place of the wood.
    - A tree thrown fast across the land and onto a slope comes to rest in
      about 1 s, and a tree still rolls on a slope: `throw_queue.ps1 -Runs
      tree` with a fast throw that lands on the land, not in the sea.
  - The Creature:
    - The Creature tutorial goes on after a press on the Creature
      (`creature_camera`). Needs a recipe.
    - A command: a tap at a place on the land, at a villager and at a
      building: `creature_recipe.ps1 -Kind walk` and `aim_queue.ps1 -Runs
      cmd1b` with these targets.
  - The village:
    - The fish of a fish farm: the halo, the grip in the fish and the far
      grab (B36). Needs a recipe.
    - Wood onto a scaffold. Needs a recipe: `village_test_scaffolds` makes
      the first scaffold at the place of the pile in the store, so the wood
      goes onto the pile. The scaffolds must be away from the store.
    - The reach of a scroll near its limit of 10 cm (`scroll_reach_cm`) at
      each scale, and a scroll near the Hand at about 4000 units a meter:
      `hand_run.ps1 -Kind one` at other scales.
  - Miracles:
    - The limits of the target of a Creature miracle (3 cm, 6 degrees), and
      the limit of 0.6 m/s of a shake that takes a miracle out. Needs a
      recipe near each limit: `miracle_run.ps1 -Kind creature` and `-Kind
      cancel` stay far inside them.
    - The effect of each miracle: each line "To prove" of `headset-checks.md`,
      "The miracles of the game" (N6): `miracle_run.ps1` with `-Seeds`.
      Water on a field and on a forest needs a test save with a field or a
      forest near the Hand.
  - The Temple:
    - The place of each eye in a room with one walk and with `one_walk=0`,
      and the fade in both eyes with `one_walk=0` (`temple_fade_eyes`, B13):
      `temple_run.ps1` with the view recipe of `controllers.md`, 11.9. Open:
      in the Save Game Room the word `look` does not turn the camera with
      `temple_camera_screen` at 1 or at 0. Find what the plain game does
      there.
    - The game's Hand does not show in the Save Game Room with
      `temple_hide_hand=1`: `temple_run.ps1` with the door recipe and a shot
      with the ray on in that room.
    - A click on a pad of the pool, on a scroll, and on a control of the
      Challenge Room and of the Library (`temple       : the game gets the
      mouse message`): `temple_run.ps1 -Rooms`.
  - Menus and cards:
    - The keyboard for the name of a new god at the first start of a profile
      (`keyboard     :`). Needs a recipe.
    - A help text of the game with the words of the mouse in an eye picture,
      after a change from the controller to the mouse:
      `creature_recipe.ps1 -Kind leash` with `help_test=1` and
      `mouse_owner_test_frame`. The census changes at once, but no help text
      with a word of the device shows after the change.
    - The hint at the game's Hand (`hint_hand_depth`) in an eye picture at a
      nearer zoom.
      Needs a recipe: `village_run.ps1 -Kind tooltip` is the recipe of the
      tooltip.
  - The intro:
    - A still hand with no flicker of one frame: `intro_run.ps1` with the
      scripted hands of its header. Needs a log line: a count of the changes
      of the show of a hand.
  - The picture and the backend:
    - The sun with a head that moves in steps. A still head gives the same
      sun with `sun_glare` 0 and 1, and 2 hides the glare. Needs a recipe
      with head steps: `temple_run.ps1 -NoEnter` with `sky_test_hour=13`.
    - The merge of draw calls (`draw_merge_test=1`) in a fight of Creatures:
      0 eye passes with a different checksum. Needs a recipe with a fight.
- **R13. A fault of a desktop script.** `temple_run.ps1 -SaveTest` with
  `menu_room_test=1` makes no save: the start dialog of the new game stays
  up, and the ring does not open.
