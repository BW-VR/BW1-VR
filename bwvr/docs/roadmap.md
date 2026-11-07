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

- **B35. A thrown object that hits the water turns fast.** A rock of
  `throw_queue.ps1 -Runs rockfast` that lands in the sea turns 2.36 times a
  second of the picture with no point that touches, over the cap of 1.5
  turns a second (`throw_turn`, `throw.md`, "The turn of a thrown
  object"). The cap acts in 0 steps there. The table of `throw.md` for a
  tree and a rock needs a run in which each throw lands on the land.
- **P20. The spell book: the open parts.** X opens a spell book over the
  hand that does not point, and the pointing hand pulls a miracle out of it
  with the grip: the game's own request (`spell_book=1`,
  `spell_book_pull_cm=10`, `debug_all_miracles` for tests, `miracles.md`,
  "The spell book"). The recipe `village_run.ps1 -Kind book` shows the
  open, the halo, the card, the pull, the spring back, the take and a
  refusal. The checks of the book, the row of gestures and the draw are in
  `headset-checks.md`, "What the owner checks", and in R12. Open:
  - No line of a card names the draw button (B) while the Hand is empty. So a
    player of release 1.2.0, which has no book and no row of gestures, does
    not find how to ask for a miracle with a gesture. The tap of the grip on
    an icon of a Worship Site works there.
  - `debug_all_miracles=1` gives only 6 miracles: the Village Centre has 6
    slots for icons, and the Worship Site copies them (`miracles.md`, "The
    limit of 6 icons"). Every miracle needs icons of the site that do not
    come from the Village Centre (`WorshipSite::AddSpellIconIfNecessary`
    `0x0077C9E0`).
  - The items in the book are flat pictures with the gestures. Only the
    bubble that the hand pulls out is the game's own bubble.
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
- **B41. Water does not put out a burning house.** A pour of 62 casts on
  a burning house does not put out its fire, and 27 casts put out burning
  trees (`miracle_run.ps1 -Kind fire`). In the game the Water spell sends
  its effect (burn -4000, radius 1) each turn at a random point near the
  spell, at the height of the land plus 0.4 (`SpellWater::Process`). An
  object gets it when the point is inside its fire radius plus 1 to the
  side and inside its height plus 1 up and down
  (`EffectValues::ApplyEffectToMapPos`). Each drop lowers the heat by a
  part that the heat capacity divides, and a house near other fires can
  catch fire again. So a house goes out after many drops, and maybe only
  for a while. Find whether the place of the spell in the mod (at the Hand,
  over the roof) moves the points of the effect away from the house, and
  how many drops the plain game needs.
- **B42. A thrown object goes into the Physical Shield of the player.** A
  pig thrown at the player's own dome goes in, also with the game's own
  physics (`throw_law=1`). In the game the shield is a body of the physics
  that a thrown object hits and bounces off, with no test of the owner
  (`PhysicalShield::ReactToPhysicsImpact`). Each hit takes strength from
  the shield in proportion to the momentum. So the plain game stops it.
  Find why the throw of the mod does not meet the body of the shield.
- **B43. The Storm cloud is a flat grey band.** In the eye pictures the
  cloud of a Storm is a flat grey band with straight edges, also with
  `cloud_pole_pct=0`. The game draws the cloud as many particles (type 106,
  `PARTICLE_TYPE_STORM_CAST`, the rules `UR_StormCast` and
  `UR_CloudGather`), so the plain game has no flat band. Find what flattens
  it: the face of the sprites (`clouds_head_free`), one walk, or the near
  plane.
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
- **P5. Hand tracking.** On hold. The player plays with the hands and no
  controllers: the Hand follows the real hand and its fingers, one to one,
  as it follows a controller. First find what the headset gives to the
  game: whether Virtual Desktop (VDXR) gives a 32 bit OpenXR program the
  extension `XR_EXT_hand_tracking`, and which gestures it gives. Then
  choose a gesture for each control: the grip, the trigger, the stick, and
  A, B, X and Y. The rules of the Hand stay: the real hand, one to one, and
  no motion that the player did not make.
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
- **P9. The pointer on the interface panel**, and the screens with no
  scene (menus, loading, videos) on a panel in the room (`ui-layer.md`).
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
  the throw with the mod's own physics first. A fireball leaves at the
  game's filtered speed of the Hand (`CHand+0x48C8`), and no run had a real
  swing. If it flies short in a headset, a key scales that speed. For other
  objects, a key `throw_speed_pct` (100: the real speed) can make a throw
  stronger.
**Comfort and moving**

- **F9. A smooth turn in the God view.** The value Smooth of the row "Turn"
  (`snap_turn=4`, `smooth_turn_speed`) is built (`god-view.md`, section 4).
  Open: the desktop runs with a full push and a half push of 1 s, and
  `snap_turn=2` to show the snap turn.
- **F11. The controls of the player in the headset.** The settings window,
  the welcome page and the page Controls are in (`ui-layer.md`, 24 and
  24.6, `controllers.md`, 5.7). Open:
  - The desktop runs of the page Controls (`village_run.ps1 -Kind
    controls`, with the map, the defaults and `hint_profile`). A desktop run
    has a Touch controller only: the profile of a real Index, wand or Mixed
    Reality controller needs a headset with that controller.
  - A 3D model of the controller in the hints (`controller-hints.md`,
    section 5). The hint cards have a picture of each button
    (`hint_card_pictures`, `controller-hints.md`, 9.13). Only a headset can
    show whether that picture is clear at 55 cm.
  - The art of each controller must be our own drawing, or a model with a
    license that lets us ship it (for example a model that the runtime
    gives through an OpenXR extension at run time).

**The village, the Creature and the help**

- **F18. The job icon of a villager in the Hand:** large, just over his head,
  and only for the villager in the Hand or under it. First the owner checks
  in a play whether the plain icon is a problem in a headset
  (`headset-checks.md`). Build it only if it is. The draw of the icon is
  not traced (`Abode::DiscipleInHandNear` `0x00407420`,
  `GGuidance::MakeDiscipleSFX` `0x0071BF10`).
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
- **F26. The first open of the settings window.** The warm draws the page,
  the tabs and the rows of the first group of the settings, and a thread
  draws the drawing of the page Controls (`panel_warm`, `ui-layer.md`, 23.3
  and 24.6). With the page and the tabs only, the first picture took 4.3
  ms (4.8 ms with no warm), and the first show of the page Controls 15.4 ms
  (17.9 ms), with about 11 ms for the drawing. Open: the times with the
  rows and the drawing from the warm, in a desktop run.

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

- **F5. The stream of the influence at the hand.** The hook that ends the
  stream of the game at the Hand's point is built, but no desktop run had a
  stream of the game, so `influence_stream_hand` is 0. The update of the
  game keeps the land point of the interface, which in the God view can be
  away from the land under the Hand (`miracles.md`, "The influence at the
  hand"). The report of `scene_run.ps1 -Kind edge` has the state of the
  stream. Open: which state stops it, and then a run that shows the stream
  at the Hand.
- **F5. The red glow at the edge.** The glow of the edge of the influence is
  a soft red patch at each hand, with the color of the game's influence ring
  (`influence_wall`, `miracles.md`, "The glow at the edge"). It waits for its
  desktop run (`scene_run.ps1 -Kind edge`).
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
    3.3 to 4.8 ms of a frame in scenes 1 to 5, and the game makes 3200 to 5400
    state calls in a frame. A thread that gets a record of these calls and
    gives them to Direct3D takes that time off the frame thread. The thread
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
    `xrEndFrame` 6.3 to 7.4 ms in place of 8.6 to 9.1 ms. Desktop runs give
    the same picture with `render_thread` at 1 and 0: 0.06 to 0.15 percent of
    the pixels differ, as between two frames of one value. Open: the cost in
    `headset-checks.md`, "The cost of a key in a headset", then the default
    value of `render_thread`. One start stopped at the logos with `gpu_flush=128` (`hang_watch=1` shows the
    place if it comes again). The two vertex loops of the engine run with
    SSE, with the bits of the game
    (`fast_transform`, `backend.md`, "The vertex loops with SSE"): 0.25 ms
    less of the scene walks in a desktop run. The calls of Direct3D 7 of a
    walk can go to a thread of their own (`d3d7_thread`, `backend.md`, "The
    thread of Direct3D 7", on by default), with the same pixels in a desktop
    run and no fault in a headset at 144 Hz. The end of a walk does not wait
    for the render thread (`render_thread_late_end`, `backend.md`, "The late
    end of a walk"): the scene walks over the island are 2.6 to 3.0 ms in a
    headset at 144 Hz. The threads of the mod spin only for a short time and
    then sleep (`thread_wait`, `backend.md`, "How the threads wait"): in
    desktop runs with 4 to 16 cores the frames are as fast, and the process
    uses about 0.5 cores less. The D3D7 thread merges the rows of draws with
    one state (`d3d7_thread_merge`), so the frame thread copies the vertices
    of a draw one time. **The goal of a port to the Steam Frame and the Quest
    3 standalone** needs fewer layers of x86 code for each draw. The plan, in
    steps that a desktop run can check: (A) a Vulkan renderer of the scene
    walks in place of dxwrapper and Direct3D 9 (`backend.md`, "The Vulkan
    renderer"; the same picture as Direct3D 9, with the hand and the overlays
    of the mod and both eyes in one pass). Its eye pictures go straight into
    an OpenXR session with Vulkan (`xr_vulkan=1`, "The session with Vulkan"),
    and so do the pixels of the CPU: the cards, the walls of the cut scene
    window and an eye picture of the CPU (`xr_vulkan_upload`). The draws for
    the panel target go into a panel of the renderer, which the subtitle card
    and the panel layer take (`vk_panel`). In a session with Vulkan a walk of
    one walk goes to the renderer alone, and the D3D7 thread gives no draw of
    it to dxwrapper (`vk_only`, "The walk without Direct3D 9"; on the desktop
    the render thread then has 1 draw of a frame in place of 773). Still on
    Direct3D 9: a menu, a loading screen, a video and a frame of two walks,
    which go through the GPU bridge and a relay of Direct3D 11, and the calls
    that set a state. (A2) A backend of the mod makes DirectDraw 7 and
    Direct3D 7 on Vulkan in place of dxwrapper (`backend=vulkan`,
    `backend.md`, "The backend of the mod on Vulkan"): on the desktop the game
    runs from the logos to the start dialog of a new game with no Direct3D 9,
    and the renderer draws each draw. One walk, or the two walks of the eyes,
    give one image with both eyes for a session with Vulkan, and the help text
    goes into the panel of the renderer for the subtitle card.
    `ddraw_census=1` with `backend=system` lists the calls that the game
    makes. With `backend=vulkan` the mod uses only Vulkan (`vulkan_only`,
    `backend.md`, "Only Vulkan"): no part for Direct3D 9 starts, the session
    has no Direct3D 11 device, and the window of the game has a swapchain of
    Vulkan (`vk_window`, "The window with Vulkan"). A session in the headset
    ran with no Direct3D 11 device. The installer keeps dxwrapper: it is the
    renderer Direct3D 9 for a PC with no Vulkan 1.3, and the renderer of
    "Play, no VR". Next for the backend: find the part of the long frames
    after the load of a saved game that the hold of the last frame does not
    take (`xr_hold_ms`, the line `slow frame`, `backend.md`, "The hold of the
    last frame"). Foveated rendering (`foveation`, `backend.md`, "Foveated
    rendering") gives the eye pictures of the renderer fewer shades away from
    the gaze of an eye tracker, or from straight ahead, and foveated
    resolution (`fovea_res`, "Foveated resolution") makes each eye picture
    from an outer image with fewer pixels and a centre image with more. On a
    Radeon RX 7900 XTX neither takes time off the GPU, because the time is in
    the draws. Not run with an eye tracker. (B) one draw for many copies of a
    mesh (about 870 draws of a frame are 36 meshes); (C) a new renderer of the
    land, after a count of its draws. The Quest 3 cannot run `runblack.exe`; a
    port there needs an x86 emulator with Wine or a new engine. With the
    render thread, one walk draws most batches in one draw for both eyes
    (`one_walk_merge`, `backend.md`, "One draw for both eyes"). Before the
    thread, with one walk, a view of the whole island has about 1500 batches
    and a scene walk of 9.5 to 11 ms, and the runtime then shows 45 frames a
    second. Of that walk about 6 ms are Direct3D 9, the driver of AMD and the
    kernel, and less than 3 ms are the game (`profile_sample`, `backend.md`,
    "Where the time of the scene walk goes"). So the thread takes the calls of
    dxwrapper to Direct3D 9, not the calls of the game to dxwrapper. For 90
    frames a second the scene walk must be under about 6 ms, because the frame
    waits 3 to 4 ms for the GPU after it. Most of that wait is the driver,
    which gives the calls to the GPU late: the GPU works about 2 ms on the
    walks of a frame (`profile_gpu`). DXVK can take the place of the Direct3D
    9 of Windows (`d3d9_backend=1`, `backend.md`, "DXVK under dxwrapper"): the
    render thread is busy 0.95 ms in place of 2.1 ms in a desktop run, with
    the same picture. Open: the check in `headset-checks.md`, a GPU of NVIDIA
    or Intel, a PC with two GPUs, and then the default value. The installer
    does not ship DXVK yet.
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
  Suspects: the picture of a new save is flat magenta, the waist point is
  8.4 units higher after a load, the ray ends short of the pointer in the
  left eye in a dialog box inside the Temple, and the leash posts are 1.4
  cm from each other at the scale of the start. In the ring menu: with
  `button_layout=0` a thumb on the left stick can open it, and a jump to the
  town or to a bookmark takes the heading of the game camera.
- **N5. The village.** Not run: food, an animal or crops set down at a
  store, the altar, a gift to the Creature, the red halo outside the
  influence, a Workshop that gives scaffolds, villagers that make chants,
  and Heal from its icon at the Worship Site.
- **N6. Miracles.** Not run: each reason of a refusal but "needs a
  Creature" and "needs a circle on the land". Open:
  - Teleport: each cast makes one site, and the game adds it to the list of
    sites of the player. A villager or the Creature that goes into a site
    comes out at the other site of that player that is nearest its
    destination. So a second site needs a second cast. Not run: two casts.
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
  section 9). Not run: a scripted click with the menu only on the panel,
  and the quads in a headset.
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
    - The limit of 40 cm of the ring of the teleport. With rule 1 the ring
      is 11 units of the world, so it reaches 40 cm only below about 27.5
      units a meter: a recipe with zoom steps in. The lower limit of 4 cm
      holds at 3844 units a meter (`aim_queue.ps1 -Runs ring4k1,ring4k0`).
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
    - The hint at the game's Hand (B34) in an eye picture at a nearer zoom.
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
