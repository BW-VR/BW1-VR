# Checks in a headset

This file is the one list of the checks that only a headset, or a person in
a real play, can show. A desktop run cannot show them: it has no head, no
real hand and no ears. The last section is a guide to each miracle of the
game. It says what the miracle does in the plain game, how to cast it in the
mod, and how to see that it worked.

The doc of each area has the detail of how the mod works. This file has
only what to look at. When a check is done, remove it from this file, and
write the result in the doc of the area (or open a roadmap item for a
fault).

## How to use this list

- **The switches.** Turn on the launcher row "Debug menu" (`debug_menu=1`,
  group "Testing"). The ring of the left hand then has the item Debug, with
  pages of switches in groups (View, Picture, Render, Interface, Hand,
  Throw, Creature, Sound, Temple). A choice changes the rule at once, so a
  comparison needs no keyboard. A line such as `A/B: debug page, Hand,
  "Catch at once"` names the group and the switch. A long pulse is 1 (or
  on), and a short pulse is 0 (or off) (`controllers.md`, 10.8).
- **No switch.** Some rules have only a launcher row or an ini key. Then the
  line says "no switch" and gives the key and its values. Change the key in
  the launcher or in `bwvr.ini`, and start the game again. A key that the
  launcher does not show must be set after the last save of the launcher.
- **The log.** The mod writes `bwvr.log` beside `runblack.exe` in the game
  folder. "Log:" gives the start of the line that tells the result. Send
  `bwvr.log` and each `bwvr-old-*.log` of that day with a report.
- **The ids.** B, P, F and N with a number are the items of
  `roadmap.md`. The roadmap keeps the work to build, and this file keeps
  the check.
- **The words of a report.** Say what you saw in your own words. The agents
  read a symptom literally.

## The God view and moving

- [ ] **The numbers of the God view** (`god-view.md`, section 13)
  - Do: recenter, and zoom through each scale with the stick. Bring the Hand
    near the eyes at the life size scale.
  - Right: the ground at 60 cm below the head (`god_ground_down_cm`), a zoom
    step of 1.5 times (`god_zoom_step_pct=50`) and the start scale of 100
    (`god_units_per_meter`) are comfortable. The Hand near the eyes stays
    whole with the near plane of 5 cm (`near_plane_cm`). The land and the
    clouds look right at each scale. Wrong: a step that is too large or too
    small, a Hand that the near plane cuts, or land or clouds that look
    wrong at a scale.
- [ ] **The zoom in steps or smooth** (P4, `god-view.md`, section 2)
  - Do: zoom in and out with a small push and with a full push of the stick,
    in steps and then smooth. Choose the default.
  - Right: a scale that changes in each frame is comfortable, with no blink.
    500 ms for a step (`god_zoom_smooth_ms`) is a good top rate. The dead
    zone of 30 percent fits a real stick. Wrong: discomfort, a blink, a rate
    that is too fast or too slow, or a zoom from a thumb at rest.
  - A/B: debug page, View, "Smooth zoom".
- [ ] **The walk with the stick** (P13, `god-view.md`, section 5.7)
  - Do: walk near the land and far out, on hills and on a steep slope. Walk
    with the head as the direction, and then with the controller as the
    direction. Try a small push, and the edge of the map.
  - Right: the walk is comfortable in each direction. 150 cm of the room in
    a second (`move_stick_speed_cm`) is a good top speed near and far. The
    dead zone of 30 percent fits a real stick. The follow of the land (the
    launcher row "Walk follows the land") is calm on the hills. On a steep
    slope it keeps the land near the hands. Wrong:
    discomfort, a speed that is wrong near or far, a view that bobs on the
    hills, or land that goes far from the hands.
  - A/B: debug page, View, "Stick walk" (off, head, controller).
- [ ] **The grab of the world** (`god-view.md`, section 13)
  - Do: grab the land with a grip, hold the hand still, and then move it.
  - Right: the land looks glued to the hand, with no shake while the hand is
    still. Wrong: the land shakes while the hand is still.
  - A/B: debug page, View, "Grab test" (as set, one to one, off).
  - Log: `the held camera moved X units a frame on average, Y at most`.
- [ ] **The mouse pan with a real mouse** (`god-view.md`, section 13)
  - Do: pan the land with the real mouse, and let the button go.
  - Right: the land stays under the Hand, and it does not move after the
    release. Wrong: the land slides from the Hand, or it moves after the
    release.
- [ ] **The arc and the ring of the teleport** (`god-view.md`, 14.10)
  - Do: hold A and aim the arc at near and far land, also at 4000 units a
    meter.
  - Right: the arc of 6 mm and the ring are easy to see, the colors are
    clear, and the ring limits of 4 and 40 cm (`teleport_ring_min_cm`,
    `teleport_ring_max_cm`) fit. The arc is easy to aim at 4000 units a
    meter, and the point of the controller feels right as the start of the
    arc. Wrong: an arc or a ring that is hard to see or to aim.
  - A/B: debug page, View, "Ring size".
- [ ] **The way to the village of Land 1** (`god-view.md`, 14.10)
  - Do: go on the walk to the village of Land 1 with the teleport and the
    stick only.
  - Right: you find the way to the village, and the teleport acts on the
    walk around the mountain. Wrong: you cannot find the way.
  - A/B: debug page, View, "Jump switch".
- [ ] **The place at a camera of the game** (`god-view.md`, 15.10)
  - Do: play the quests where the game moves its camera, and the choice
    "Keep your old Creature".
  - Right: 50 cm ahead (`god_spawn_ahead_cm`) is a good distance, the height
    of the look point is right on a slope, and the cut 1 s after "Keep your
    old Creature" is good. A quest puts you at a good place. Wrong: a place
    that is too near, too far, too high or too low, or a cut at a bad time.
  - A/B: debug page, View, "Follow camera".
- [ ] **The player that a script sees** (`god-view.md`, 16.10)
  - Do: play the Island Keeper of the Gods' Playground and the quest of the
    mother (`FollowUs`).
  - Right: the quests go on. A script that deletes a thing behind the head
    is good in VR. Wrong: a quest that waits for ever, or a thing that goes
    away in the view.
  - A/B: debug page, View, "Script player".
- [ ] **The camera zone at the Creature select** (`god-view.md`, 17.7)
  - Do: at the Creature select, walk with the stick past the edge of the
    zone.
  - Right: the margin past the zone gives a comfortable view
    (`god_zone_margin`, 50 by default, the desktop runs used 100). The sound
    and the pulse of a bump each 500 ms feel right, and the force field
    looks right. Wrong: a view that is too tight, or a bump that is too loud
    or too frequent.
  - A/B: debug page, View, "Camera zone".
  - Log: `camera zone  : the walk with the stick asked for your place`.
- [ ] **The tutorial in play** (B26, `god-view.md`, section 7)
  - Do: play the tutorial to the end of the build of the Temple, the
    missionaries, the lessons of Land 2 (the gestures, the shield, the
    fireball) and the lessons of the Gods' Playground after the step of the
    turn.
  - Right: you come through each step with the controls of the mod. Wrong:
    a step that does not end, or an Island Keeper that does not start his
    first walk. Say which step.

## The Hand and grabs

- [ ] **The hand on the controller** (`hand-model.md`, "The game's hand on
  both real hands")
  - Do: hold each controller and look at the hand. Move the trigger and the
    grip slowly from open to closed. Bring a hand near the face in the God
    view.
  - Right: each hand sits on the controller with the size of a real hand,
    and the wrist fades. The fingers follow the trigger and the grip at each
    point of the way. A hand stays in view to 5 cm from the eye. Wrong: the
    hand is off the controller, turned, too large or too small, or a finger
    jumps.
  - A/B: debug page, Hand, "Hand model" (low poly, game, file). No switch
    for the turn of the mesh: `hand_mesh_yaw_deg`, `hand_mesh_pitch_deg` and
    `hand_mesh_roll_deg` in the ini.
- [ ] **The Hand of the game on the real hand** (`hand.md`, section 7)
  - Do: drag the land, tug an object, carry a miracle, and hold objects of
    each kind.
  - Right: the Hand sits on the real hand with the right rest turn in each
    state. Each object has the right place and opening of the hand in the
    palm. Wrong: a Hand that is off the real hand, or an object that looks
    wrong in the palm (name the object).
  - A/B: debug page, Hand, "Game Hand".
- [ ] **The poses of the mesh** (`hand-model.md`, "The game's hand on both
  real hands")
  - Do: aim a teleport, stroke the Creature and drag the land. Take a rock, a
    villager and a tree.
  - Right: the point, the flat hand and the fist look right, and a finger
    touches the object. Wrong: a finger goes into the object, or stays far
    from it.
- [ ] **The joints of a hand that you edited** (`hand-model.md`, "A hand that
  the player edited")
  - Do: use a hand file with more bones, and bend the fingers.
  - Right: the joints bend smoothly. Wrong: the mesh folds or tears at a
    joint.
  - A/B: debug page, Hand, "Hand model" (file).
- [ ] **The squeeze of the grip** (`god-view.md`, section 13)
  - Do: grab and let go with real controllers. Hold a loose hand. Move the
    hands fast.
  - Right: a press at 70 percent and a release at 50 percent
    (`grab_press_pct`, `grab_release_pct`) fit a real hand. A loose hand does
    not grab, and the hands do not lag the real hands. Wrong: a grab from a
    loose hand, a grip that does not let go, or hands that lag.
- [ ] **The trigger and the grip in each state of the Hand**
  (`controllers.md`, section 7)
  - Do: use the trigger and the grip in each state of the Hand.
  - Right: each button feels right for its job. Wrong: a job that needs the
    other button. Say which job.
- [ ] **The left hand** (`controllers.md`, 5.6)
  - Do: set the left handed option, and play with the left controller as the
    Hand.
  - Right: the left hand acts as the right hand does, the hand model and the
    Hand are on the left controller, and its menu button acts as the button
    of the pointing hand. Wrong: a job on the wrong hand.
  - A/B: no switch: the launcher row "Hand" (`left_hand`: -1 the option of
    the game, 0 the right controller, 1 the left controller).
  - Log: `the left controller is the Hand, and the right controller is the other hand`.
- [ ] **The aim ray and its presets** (`controllers.md`, section 8)
  - Do: open the game menu and the Quest menu, and compare the two rays.
    Then use the grab from a distance. Do this with each preset, and with
    each hand as the pointing hand.
  - Right: one preset matches the ray of the Quest menu, also for the left
    hand, and the halo, the arc and the green halo show where the hand
    points. Wrong: the rays differ with each preset. Then change the trim.
  - A/B: no switch: `aim_preset` in the ini (0 the ray of the runtime, 1 the
    Meta pointer, 2 the Half-Life: Alyx pointer), and the trim
    `aim_pitch_deg` and `aim_yaw_deg` (-30 to 30).
  - Log: `controllers  : right hand "oculus/touch_controller": the runtime puts the aim pose`.
- [ ] **The pick at the Hand** (`grab.md`, "One target")
  - Do: take objects near the Hand, scrolls among them, and hold an object at
    the palm. Work near objects in a village.
  - Right: 4 cm (`hand_pick_cm`) is a good reach: an object is easy to take,
    and an object at the palm is not taken. 10 and 15 cm
    (`far_grab_near_cm`, `far_grab_near_out_cm`) keep the far grab out of
    the way near an object, and do not turn it off too often in a village.
    1 cm (`hand_touch_cm`) is a good touch distance. Wrong: a miss, a take
    at the palm, or a far grab that comes and goes.
  - Log: `hand pick`.
- [ ] **The rope of the leash** (`grab.md`, "One target")
  - Do: put the leash on the Creature.
  - Right: the rope comes out of the hand. Wrong: the rope starts away from
    the hand.
- [ ] **The halo of a target** (`grab.md`, "One target" and "The grab from a
  distance")
  - Do: aim at near and far targets against the land, and aim the Hand with
    no target.
  - Right: the halo is easy to see and not too strong, also against the
    land. The Hand with no glow is still easy to aim. The paler gold of the
    look of a use is easy to tell from the gold of a take. Wrong: a halo that
    is lost on the land or too strong.
  - A/B: debug page, Hand, "Target glow" and "Idle glow".
- [ ] **The glow on the mesh of a target** (P15, `hand-model.md`, "The glow
  on the mesh of a target")
  - Do: point at a villager and at a house by day and by night. Look at the
    Creature and at a building in work beside a target that glows.
  - Right: the glow at 45 percent (`target_glow_pct`) is easy to see by day
    and by night. The pulse and the band of light are calm at the size of
    the God view, and a small target needs nothing more. Wrong: a glow that
    is too faint or too bright, or a motion that distracts.
  - A/B: debug page, Hand, "Target glow" (halo, mesh, both).
- [ ] **The glow at the Hand's point** (B22, `hand-model.md`, "The glow at
  the Hand's point")
  - Do: turn on the glow. Point at bright land, at the sea, at the Hand, at a
    scroll, at a signpost and at a building, and turn the hand.
  - Right: the disc has a good size and color on each surface. It stays at
    one place on the hand when the hand turns, in both eyes. It does not go
    into the finger tip, and a hill in front still hides it. Decide if the
    glow is on by default: testers find that it helps the aim when the view
    is zoomed out. Wrong: the disc goes into the finger tip, shows through a
    hill, moves in one eye, or flickers when its look changes.
  - A/B: no switch for the glow: the launcher row "Glow at the Hand's point"
    (`hand_glow`, 0 or 1). Debug page, Picture, "Walk marks" for the place in
    one walk.
  - Log: `hand glow`.
- [ ] **The glow of the rays** (`hand-model.md`, "Overlay renderer")
  - Do: use the menu ray, the tether of the far grab and the arc of the
    teleport on bright land, and the menu ray in the Temple.
  - Right: the glow reads as energy in motion and does not distract. Wrong:
    it distracts, or the menu ray in the Temple has no glow.
  - A/B: debug page, Picture, "Ray glow".
- [ ] **A living thing beside a tree or a rock** (P1, `grab.md`, "The cone
  and the choice")
  - Do: play the quest in which a man leads the children away. Aim at the
    man with trees around him, and at the children behind him. Then aim at a
    tree, a rock and a pile of a store beside a villager.
  - Right: the halo goes to the man, and it stays on him while the hand
    shakes. The aim on one child takes the halo from the next one when it is
    1 degree nearer to the ray. The cone of 4 degrees (`far_grab_living_deg`)
    and the margin of 3 degrees (`far_grab_stay_deg`) still let you take the
    tree, the rock and the pile with no effort. The least size of 2 degrees
    feels right. Wrong: the halo goes to a tree, or a villager takes the halo
    from the thing that you aim at.
  - A/B: debug page, Hand, "Far choice" and "Far stay".
- [ ] **The Creature, a miracle and a signpost in the aim** (P1, `grab.md`,
  "The cone and the choice")
  - Do: aim at the body of your Creature with a tree or a signpost beside it.
    Aim at a miracle beside a signpost, and at a signpost alone.
  - Right: the Creature gets the halo, and a tree beside it is still easy to
    take. The miracle takes the halo and keeps it. A signpost is easy to
    choose with the aim on it. Wrong: one of them is hard to choose.
- [ ] **A signpost and the Creature at the Hand** (P1, `grab.md`, "The rule
  of the choice")
  - Do: bring the Hand to a miracle or to the Creature beside a signpost.
    Tap the signpost. Close the grip on the Creature from 4 to 15 cm.
  - Right: the miracle or the Creature is the target of the grip, and the
    aim keeps its target beside the signpost. The signpost is easy to tap
    with the Hand's point in it. The grip on the Creature at 15 cm gives the
    leash click, and the Creature state starts with the hand that far from
    the body. Wrong: the signpost takes the grip, or the Creature does not
    answer in the wide reach.
- [ ] **A tree, a house or a pile beside the Hand** (P1, `grab.md`, "Near
  objects win")
  - Do: with the hand beside a tree, a house or a pile, aim past it at a far
    target, also at the Creature. Then take or tap the near thing.
  - Right: the far target gets the halo. The near thing is still easy to
    take or to tap with the Hand's point in it. Wrong: the near thing blocks
    the far aim, or it is hard to take.
- [ ] **The range and the pull of the far grab** (`grab.md`, "The grab from
  a distance")
  - Do: aim at villagers in a crowd, and pull targets to the hand. Aim at a
    ball on the sea.
  - Right: 2 m (`far_grab_max_m`) is a good range, and the cone of 8 degrees
    (`far_grab_cone_deg`) feels right. The pull at 0.8 m/s or 12 cm is easy
    to do and does not fire by accident, and the glide reads well. The ball
    on the sea gets the halo, the lock and the fetch. Wrong: the wrong
    villager, or a pull by accident.
- [ ] **A catch at once** (B2, `grab.md`, "A catch at once")
  - Do: close the grip on a rock in its fall, on a rock that you threw up,
    and on a rock that rolls, 15 to 23 cm from the finger tip. Throw and drop
    after a catch. Fetch a tree that still moves from a distance.
  - Right: the rock is in the hand at the press. It does not move on and does
    not jump, and its way to the hand (0.12 s) looks right. It leaves the
    hand as each held object does. The far tree comes with the glide from
    the press and does not wait at its place first. No grip on a thing that
    moves breaks a rock. The guard of a throw does not stop a catch. Wrong: a
    miss, a jump, a split rock, or a wrong throw after the catch.
  - A/B: debug page, Hand, "Catch at once", "Catch moving" and "Press hold".
- [ ] **A tree that comes to rest in a take** (B11, `grab.md`, "An object
  that comes to rest while the Hand takes it")
  - Do: take a tree that fell, at the moment that it comes to rest, at the
    Hand and from a distance.
  - Right: the tree comes to the hand each time. Wrong: the Hand is empty
    with the grip closed, or it gets a dead tree.
  - A/B: debug page, Hand, "Take guard".
- [ ] **The roots pile of a far tree** (B1b, `grab.md`, "A tree in the
  Hand")
  - Do: take a tree from a distance, and look at the pile at its roots.
  - Right: the pile at the roots is what you mean by the "pit". Wrong: the
    pile is at a place that you do not expect.
  - A/B: debug page, Hand, "Roots pile".
- [ ] **A take at a distance** (P14, `grab.md`, "The stream of a take at a
  distance")
  - Do: take wood or food from a store at a distance, with a still hand and
    with a hand that moves, on a way of about 2 m.
  - Right: the halo, the tether and the count at the Hand read well. The
    curve of the stream reads well in depth, also with a hand that moves.
    The pieces (1 s of life, 8 in a second, 25 cm apart on 2 m) read well.
    Wrong: a stream that is hard to see or that looks wrong in depth.
  - A/B: debug page, Hand, "Take stream".
- [ ] **A take at a distance from the pile of a miracle** (B19, `grab.md`,
  "A bulk source at a distance")
  - Do: cast Wood or Food, and take all of its pile from a distance.
  - Right: the game deletes the pile, and the grip carries the load to the
    hand. Wrong: the Hand lets the load go when the pile goes away.
  - A/B: debug page, Hand, "Take carry".
- [ ] **The bulk take at the Hand** (`grab.md`, "The log, tests and keys of
  the take")
  - Do: take from a pile with the grip closed, and give a pile a short
    squeeze.
  - Right: the count reads at the hand, the stream reads well, and the Hand
    stays at the controller. A short squeeze stops when the grip opens.
    Wrong: a take that goes on after the grip opens, or a Hand that leaves
    the controller.
  - Log: `hand bulk`.
- [ ] **Many trees from a forest** (`grab.md`, "The log and tests of the
  forest take")
  - Do: hold the grip on a forest, take one whole tree too, and fetch one
    tree from a distance.
  - Right: 600 ms for the start and 400 ms for each tree
    (`forest_take_wait_ms`, `forest_take_ms`) feel right, and 10 cm
    (`forest_take_cm`) is a good reach. One whole tree is still easy to take.
    After the fetch of one tree, no take of a bundle starts by accident.
    Wrong: a bundle by accident, or a wait that is too long or too short.
  - A/B: debug page, Hand, "Forest take".
- [ ] **The touch of the land** (`hand.md`, section 5)
  - Do: put the Hand on the land and on a villager, and push the hand into
    the land.
  - Right: the glow and the Hand reach the land and the villager, and the
    mesh looks right at the land. Wrong: a gap to the land.
  - Log: `hand` (the height of the point over the land).
- [ ] **The fingers on the land** (`hand-model.md`, "Hands that react to the
  world")
  - Do: put a hand down on the land slowly, fingers first. Lay a flat hand
    palm down on a slope and across a ridge. Press the trigger and the grip
    with the hand on the land.
  - Right: the fingers bend as they meet the land, with no jump. A flat hand
    lies on the land. A still hand has no finger that shakes. Wrong: a finger
    jumps, shakes or goes into the land.
  - A/B: debug page, Hand, "Finger feel".
- [ ] **The fingers against a body** (`hand-model.md`, "Hands that react to
  the world")
  - Do: put a hand at the wall of a house, of the Workshop and of the Village
    Store, at a trunk, at a villager and at the Creature. Let a villager walk
    into a still hand. Use both hands.
  - Right: a finger bends at the body and does not go into it. Wrong: a
    finger bends in the air beside the thing, or goes into it.
  - A/B: no switch: `hand_feel_bodies`, `hand_feel_joints`, `hand_feel_offer`
    and `hand_feel_shape` in the ini (1 or 0).
- [ ] **The hand that opens toward a target** (`hand-model.md`, "Hands that
  react to the world")
  - Do: bring the tip of the index finger to a rock, a villager and a tree.
  - Right: the hand opens to a good form at the right time. Wrong: a form
    that looks wrong, or that comes too late or too early.
  - A/B: no switch: `hand_feel_approach` in the ini (1 or 0).
- [ ] **The fingers round an object in the Hand** (`hand-model.md`, "Hands
  that react to the world")
  - Do: take a rock, a villager, a boulder and a tree. Take a tree with the
    palm to the side, as a pole.
  - Right: the fingers close on the object, the thumb goes round a villager
    and a boulder, and the trunk lies in the hand. Wrong: a finger goes into
    the object or stays far from it.
  - A/B: no switch: `hand_feel_hold`, `hand_feel_shape` and `hand_hold_trunk`
    in the ini (1 or 0).
- [ ] **The pinch of a small object** (`hand-model.md`, "Hands that react to
  the world")
  - Do: take a small rock or a mushroom, and zoom out and in with it in the
    Hand.
  - Right: the thumb and the index finger hold it in a pinch, not a claw, and
    it does not glide at the change. Wrong: a claw, or an object that glides.
  - A/B: debug page, Hand, "Pinch".
- [ ] **The hand in the sea** (`hand-model.md`, "Hands that react to the
  world")
  - Do: put a hand into the sea.
  - Right: the fingers go into the water with one pulse at the water line.
    The part in the water looks under the surface, and the ring on the water
    has a good size. Wrong: the fingers stop on the water, or the part in the
    water looks as in the air.
  - A/B: no switch: `hand_feel_sea`, `hand_feel_water_look` and
    `hand_feel_ripple` in the ini (1 or 0).
- [ ] **The pulse at the first touch** (`hand-model.md`, "Hands that react
  to the world")
  - Do: touch the land and a body with a finger tip.
  - Right: one faint pulse at the first touch. Wrong: a pulse that is too
    weak, too strong or too frequent.
  - A/B: no switch: `hand_feel_pulse` in the ini (1 or 0).
- [ ] **The idle wave of the fingers** (`hand-model.md`, "Hands that react
  to the world")
  - Do: set `hand_feel_idle=1`, and hold a relaxed hand still.
  - Right: decide if the slow wave of the fingers is wanted. It is motion
    that the player did not make, so it is off by default.
- [ ] **How each pulse feels** (`hand-model.md`, "Haptics")
  - Do: set `haptics_pct=50`, then 200. Pick up, take, let go, stroke and
    slap the Creature, put on the leash and give a command.
  - Right: each kind of pulse is clear and not too strong. Wrong: say which
    kinds are too faint or too strong.
  - Log: `haptics      : N pulses this window`.
- [ ] **The haptics of the world** (`hand-model.md`, "Haptics from the
  world")
  - Do: stroke the Creature, hold a villager, throw objects far from the
    hand, drag the land, and take from a store.
  - Right: the purr reads as a purr, the beat as a heartbeat, and the ticks
    of the land as land. The thump of a far landing feels right. Wrong: a
    pattern feels like a buzz, or it hides the pulse of a control.
  - A/B: no switch: `world_haptics` in the ini (1 or 0).
  - Log: `haptics      : the pattern`.

## Throws and catches

- [ ] **The release when the grip opens** (`grab.md`, "The grip holds", and
  `throw.md`, "The release at once")
  - Do: throw objects with a fast swing, set objects down with a still hand,
    and hold objects that you want to keep.
  - Right: the throw leaves the hand when the hand opens, in the direction
    that you meant. A still hand sets the object down where it is. The fast
    opening does not let go of an object that you want to keep. Wrong: a
    late release, a throw in a wrong direction, or a drop by accident.
  - A/B: debug page, Throw, "Fast release".
  - Log: `grip` (the values of the grip before each opening).
- [ ] **The feel of a throw** (`throw.md`, "The throw physics")
  - Do: throw rocks and villagers at 7.5, 67, 400 and 1000 units a meter.
    Put an object down fast. Throw a rock at a house.
  - Right: a throw feels like a real throw at each scale. A quick put down
    does not throw. A villager lives through a soft throw and dies from a
    hard one. A rock still breaks a house. Wrong: a throw that is too weak or
    too strong, or a throw by accident.
  - A/B: debug page, Throw, "Throw law", "Throw slower" and "Throw faster".
  - Log: `hand throw`.
- [ ] **A thrown object that touches another** (B10, `throw.md`, "The
  picture between the steps")
  - Do: throw a villager or a rock at a group of objects.
  - Right: at the touch the object moves at the speed of the game's own time,
    and the slower move looks right. Wrong: the picture stands and jumps.
  - A/B: debug page, Throw, "Throw picture".
- [ ] **A thrown object that turns fast** (B35, `throw.md`, "The turn of a
  thrown object")
  - Do: throw a tree, a rock and a villager with a real swing.
  - Right: the object turns at most about 1.5 turns a second, also at the
    first contact with the land. A throw does not look too still. Wrong: a
    fast spin in the flight or at the first contact. Say which object.
  - A/B: debug page, Throw, "Throw turn" (game, pace).
- [ ] **The landing of a thrown object** (`throw.md`, "The landing of a
  thrown object")
  - Do: throw rocks onto flat land and onto the slope of the pen.
  - Right: the landing looks natural and not too slow. The change of the
    rate does not show at the first contact. A rock that stops its turn fast
    looks right, and a rock on a steep slope goes down at a good pace.
    Wrong: a slow slide, a jump at the first contact, or a rock that turns
    in place.
  - A/B: debug page, Throw, "Land rest", "Landing", "Land slower" and "Land
    faster".
- [ ] **A tree that falls or that you throw** (`throw.md`, "The landing of a
  tree")
  - Do: throw a tree, let a tree fall from the Hand, and throw a tree fast
    across the land and onto a slope.
  - Right: the tree looks natural in its flight and its landing, and it comes
    to rest in about 1 s. A tree on a slope still rolls. Wrong: a tree that
    tumbles in place, or a tree that stops on a slope.
  - A/B: debug page, Throw, "Tree settle".
- [ ] **The height of a release** (`throw.md`, "The height of a release")
  - Do: open the grip with a still hand high over the land, and low over the
    land.
  - Right: 15 cm (`release_fall_cm`) is a good limit. Over it the object
    falls, and below it the Hand sets it down. Wrong: a fall from a low hand,
    or a set down from a high hand.
  - A/B: debug page, Throw, "Fall rule".
  - Log: `hand throw` (`The hand was still, but its point was N cm of the room over the land`).
- [ ] **The throw of a Fireball** (`miracles.md`, "The grip casts")
  - Do: swing the hand and throw a Fireball at near and far targets.
  - Right: the Fireball goes as far as you mean. Wrong: it goes too short or
    too far. F6 makes a key for the speed only if it flies short.

## The Creature

- [ ] **The feel of a stroke and a slap** (`creature.md`, section 2)
  - Do: stroke the Creature slowly with the flat hand. Slap it with a
    backhand and a forehand, with the arm and with the wrist.
  - Right: at `creature_slap_mps` 1.0 and 5 cm of reach (`creature_touch_cm`)
    a stroke and a slap feel right. A slow stroke is never a slap. Wrong: a
    slap that does not count, or a stroke that counts as a slap.
  - A/B: debug page, Creature, "Slap speed".
- [ ] **The hand of a stroke** (`creature.md`, section 2)
  - Do: stroke the Creature at each zoom.
  - Right: the hand opens flat or as a finger at the right zoom, and 0.75 is
    a good change point. The tickle of the fingers reads as a stroke, and
    the glow at the palm reads well. Wrong: the wrong hand for the size, or
    a tickle that looks like a fault.
  - A/B: debug page, Creature, "Stroke hand", "Stroke limits" and "Palm
    point".
- [ ] **The body of the Creature** (`creature.md`, section 2)
  - Do: touch the Creature at each part of its body.
  - Right: the hand touches where the body is. Wrong: a touch in the air, or
    a hand in the body with no touch.
- [ ] **The camera at a press on the Creature** (`creature.md`, section 3)
  - Do: press on the Creature, and play the Creature tutorial.
  - Right: the camera stays still at a press, and the tutorial goes on.
    Wrong: the camera moves, or the tutorial stops and waits.
  - A/B: no switch: `creature_camera` in the ini (2, 1 or 0). The fall back
    for the tutorial is 1.
- [ ] **The gift of an object** (B30, `creature.md`, section 2.9)
  - Do: give a pig to the Creature with the leash on, then with no leash.
    Open the grip one time with the hand at rest. Then move the hand away
    before the gift.
  - Right: the Creature takes the pig at the first opening of the grip. The
    gift goes on while the hand rests, and a move of the hand away keeps the
    object in the Hand. 20 cm (`creature_gift_cm`) is a good reach. The
    Creature stays the target to 6 cm (`hold_target_cm`), and its halo is
    steady. Wrong: the gift needs a second try, or the object goes while
    the hand moves away.
  - A/B: debug page, Creature, "Hold target". No switch for the gift:
    `creature_gift` in the ini (1 or 0).
- [ ] **The halo and the leash from a distance** (`creature.md`, section 5)
  - Do: aim near the Creature with no wish to choose it. Give a short grip
    at the Creature from a distance.
  - Right: the halo of the Creature shows only when you want it. A short
    grip puts the leash on, and the Creature does not turn to the hand.
    Wrong: a halo by accident, or a Creature that turns to the hand.
- [ ] **The stay** (P2, `creature.md`, section 5.3)
  - Do: hold the grip on the far Creature for a stay, with a Creature that
    stands and with one that walks. Give short grips too. Move the hand far
    from the Creature during the stay.
  - Right: 350 ms (`creature_hold_stay_ms`) is a good hold time: a short grip
    does not start a stay, and a hold does not feel late. The Creature turns
    to a good place, and its head follows the real hand. The halo, the pulse
    and the card show that the stay is on. The animation of what the
    Creature thinks of reads well with no stroke after it. Wrong: a stay by
    accident, a Creature that looks away, or a stay that you cannot see.
  - A/B: debug page, Creature, "Hold stay". No switch for the look:
    `creature_stay_look` in the ini (1 or 0).
- [ ] **A command with the trigger** (P1, `creature.md`, section 4.1)
  - Do: with the leash on, tap the trigger at places on the land and on each
    kind of object. Press on an object, and move the aim off it. Hold the
    controller with a relaxed finger.
  - Right: a tap sends the Creature to the place that you pointed at, and it
    does what you expect on each kind of object. A press becomes a walk 100
    ms after the aim leaves the object, with a tick. Wrong: the Creature goes
    to a different place, or a trigger that closes by accident sends it too
    often.
  - A/B: debug page, Creature, "Command rule" (4, 3, 2, 1).
- [ ] **The aim of a command** (P1, `creature.md`, section 4)
  - Do: aim at objects near and far while the leash is on.
  - Right: 3 degrees (`creature_command_cone_deg`) and 10 m
    (`creature_command_max_m`) fit a real hand, and the wait of 100 ms is
    right. Wrong: the aim misses objects, or the halo is on almost each
    object in the cone.
- [ ] **The double press** (`creature.md`, section 4)
  - Do: press the trigger two times fast on an object.
  - Right: 500 ms (`creature_command_double_ms`) fits a real finger, and it
    feels right that the first tap sends the Creature before the second
    press ties the leash. Wrong: a double press that is too hard to make.
- [ ] **The land point of a press** (`creature.md`, section 4)
  - Do: press the trigger on the land at near and far places.
  - Right: each press has its land point. Wrong: no land point.
  - Log: the line of the press says "The arc of the press meets", not "has
    no land point: the arc meets no land".
- [ ] **The shake that takes the leash off** (`creature.md`, section 4)
  - Do: shake the hand that holds the leash. Then move the hand in a normal
    way.
  - Right: a real shake (0.6 m/s, 4 strokes, `leash_shake_mps`) takes the
    leash off, and a normal move never does. Wrong: a shake that does
    nothing, or a normal move that takes the leash off.
- [ ] **The eye contact of the Creature** (`creature.md`, 5.4)
  - Do: stand near a large Creature while it looks at you, and lean.
  - Right: the Creature looks into your eyes, and its eyes follow your head.
    Wrong: it looks past your head, at the place of the game camera.
  - A/B: no switch: `creature_look_head` in the ini (1 or 0).
  - Log: `creature look:`.
- [ ] **A fight of Creatures** (B32, `creature.md`, section 6)
  - Do: in a fight, press the grip at the other Creature to attack and to
    block. Look at the marks of the fight.
  - Right: the attack and the block act at the first grip, at the place of
    the body that is nearest to the Hand's point. The stars sit on the body
    in both eyes. Wrong: a press that misses, or marks that float half way to
    the camera.
  - A/B: no switch: `fight_grip` and `fight_mark_depth` in the ini (1 or 0).
  - Log: `hand fight   :` and `fight marks  :`.

## The village

- [ ] **The job icon of a villager in the Hand** (F18)
  - Do: in a play, take a villager in the Hand, and look at the plain job
    icon of the game over him.
  - Right: the icon is easy to see and to read. Wrong: the icon is too small
    or hard to see. Say which, so that F18 is built only if it is needed.
- [ ] **The fish in the sea** (B36, `grab.md`, "The fish in the sea")
  - Do: find a fish farm at a coast. Grip in the fish, and take fish from a
    distance.
  - Right: the fish get the halo. The grip in the fish and the far grab take
    fish. The halo that moves with the fish is calm. Wrong: no halo, no take,
    or a halo that jumps.
- [ ] **An object onto a store or a scaffold** (`grab.md`, "An object in the
  Hand that goes onto a thing")
  - Do: hold wood over the Village Store and over a scaffold.
  - Right: 6 cm (`hold_target_cm`) is a good reach, and the halo of a large
    thing reads as "the wood goes here". Wrong: the wood goes to the wrong
    thing.
  - A/B: debug page, Creature, "Hold target".
- [ ] **A villager of a quest who looks at you** (`village.md`)
  - Do: go near a person of a quest whom a script turns to the player.
  - Right: his turn to your head reads as a look at you. He does not look up:
    the game has no pitch for a villager. Wrong: he turns to a different
    place.
  - A/B: debug page, View, "Script player".
- [ ] **The totem with the real hand** (`village.md`)
  - Do: close the grip on the pole of the totem and move it. Set the totem
    from a distance, to a count of worshippers that you want.
  - Right: the pole feels held, and it moves with the hand. The whole travel
    of 18 cm gives the count with no effort. Wrong: the pole slides away from
    the hand, or the count is hard to hit.
  - A/B: debug page, Creature, "Totem hand".
- [ ] **The icons of the Worship Site** (`village.md`)
  - Do: tap the grip on each icon of a Worship Site.
  - Right: each icon is easy to hit (they are 4 to 8 cm apart). Wrong: a tap
    hits the icon beside the one that you want.
- [ ] **The reach of a scroll** (`village.md`, "Tests and limits of the
  scroll")
  - Do: reach for a scroll at each scale of the God view.
  - Right: the reach of 10 cm (`scroll_reach_cm`) is good, and the glow and
    the halo show at each scroll. Wrong: the hand must touch the scroll
    exactly.
  - A/B: debug page, Hand, "Scroll draw".

## Miracles and gestures

Each miracle of the game, with what it does, is in "The miracles of the
game" below. These checks are about the controls of the mod.

- [ ] **The spell book** (P20, `miracles.md`, "The spell book")
  - Do: press X, and read the book over the open palm. Reach to each item.
  - Right: the book is 12 cm from the grip, easy to read and to reach, and
    the cells have a good size. Wrong: the book is hard to read, out of
    reach, or missing in Virtual Desktop.
  - A/B: debug page, Creature, "Spell book" and "All miracles".
- [ ] **The pull out of the book** (P20, `miracles.md`, "The spell book")
  - Do: pull a miracle out of the book, and let one spring back.
  - Right: a pull of 10 cm and the buzz feel like an elastic band. The
    bubble that springs back reads as a miracle that goes back into the
    book. Wrong: a pull that feels stiff or loose.
  - A/B: debug page, Creature, "Book pull".
- [ ] **The bubble of the miracle** (P20, `miracles.md`, "The game's
  bubble")
  - Do: pull a miracle out with Direct3D 9 and with Vulkan, and look at the
    bubble from near.
  - Right: the game's bubble sits on the band between the book and the hand.
    Its hole in the page looks right in both eyes, and the icon shows
    through the glass. Wrong: a bubble beside the band, an icon that is hard
    to see, or no bubble with Vulkan.
  - A/B: debug page, Creature, "Book bubble" (flat, game).
- [ ] **The gesture row over the palm** (P20, `miracles.md`, "The gesture row
  over the palm")
  - Do: turn the palm of the hand that does not point up while you hold B.
  - Right: the row is easy to read, 30 cm wide (`gesture_panel_cm`) and 12
    cm over the palm, and the bubbles and the dot read well. Wrong: a row
    that is too small, too large or too far.
  - A/B: no switch: `gesture_panel` in the ini (1 or 0).
- [ ] **The palm that opens the book** (P20, `miracles.md`, "The book on the
  palm")
  - Do: turn the palm up and hold it. Move the hand in a normal way.
  - Right: a real palm turns up past 40 degrees with no effort, and 0.4 s
    opens the book. Wrong: the book opens by accident.
  - A/B: no switch: `spell_book_palm` in the ini (1 or 0).
- [ ] **A gesture with the real hand** (P20, `miracles.md`, "Feeding a 3D
  path")
  - Do: hold B and draw the spiral and then the gesture of a miracle in the
    air, with the wrist turned and straight.
  - Right: the game matches the draw, and the miracle comes into the Hand.
    The tip of the index finger is a good point to draw with, and
    `gesture_size_cm` (30) fits it. Wrong: the game does not match a draw
    that looks right. Say which gesture.
  - A/B: debug page, Creature, "Gesture point" (grip, fingertip). Choose it
    between two draws.
- [ ] **The trail at the fingertip** (`miracles.md`, "The trail at the
  fingertip")
  - Do: draw gestures over bright land and over the sky.
  - Right: the gold trail of 1.8 cm reads well, and it stays 1.5 s. Wrong:
    the trail hides the Hand's point.
  - A/B: debug page, Creature, "Gesture trail".
- [ ] **The miracle in the real hand** (`miracles.md`, "The Hand is the real
  hand")
  - Do: take a miracle and look at it in the hand.
  - Right: the miracle is in the real hand, at a good size. Wrong: it is
    beside the hand.
  - A/B: debug page, Creature, "Miracle hand".
- [ ] **The target of a Creature miracle** (`miracles.md`, "The target")
  - Do: aim a Creature miracle at the Creature at the Hand and from a
    distance.
  - Right: 3 cm (`miracle_touch_cm`) and 6 degrees (`miracle_cone_deg`) find
    the Creature with no effort. Wrong: the aim misses the Creature.
- [ ] **The circle of a cast** (N6, `miracles.md`, "The circle of a cast")
  - Do: draw a circle over the land with the real hand for Storm or a
    shield, at the size that you want.
  - Right: the circle of the hand fits the circle of the game, and the cast
    at the close of the circle, with the grip still closed, feels right.
    Wrong: the game does not match the circle, or its size on the land is
    not the size of your circle. Testers find the shield circle hard to draw
    to the correct size.
  - A/B: no switch: `miracle_circle_cast` in the ini (1 or 0).
- [ ] **The shake that takes a miracle out** (`miracles.md`, "The way out")
  - Do: shake the hand with a miracle in it. Then move the hand in a normal
    way.
  - Right: a real shake (0.6 m/s, `miracle_shake_mps`) takes the miracle
    out, and a normal move does not. Wrong: a shake that does nothing, or a
    normal move that takes the miracle out.
- [ ] **The cancel with B** (`miracles.md`, "The way out")
  - Do: tap B to cancel a miracle. Hold B for longer.
  - Right: 400 ms (`miracle_cancel_ms`) is a good limit of the tap. Wrong: a
    tap that does not cancel, or a hold that cancels.

## The Temple

- [ ] **The view of a room and the fade** (B13, `controllers.md`, 11.4)
  - Do: in a room, turn, tilt and lean the head. Come into the Temple, change
    the room, and leave. Look down at the pool of the hall.
  - Right: the room stays still and level when the head moves, and it has
    depth. The left eye is not dark at the entry, at a change of the room or
    at the way out. A look down at the pool does not move you. Wrong: a room
    that turns or tilts with the head, one dark eye, or a move at the pool.
  - A/B: debug page, Temple, "Room view" and "Temple fade". No switch for the
    pool: `temple_camera_screen` in the ini (1 or 0).
- [ ] **The black frames of the guide** (B13, `controllers.md`, 11.9)
  - Do: make a first visit to the Temple, and watch the guide of the hall.
  - Right: no black frames. Wrong: single black frames at random times. Then
    play that part again with `eye_test_luma=1` in the ini, and send the
    log.
  - Log: `eye test` (each run of frames with one dark eye).
- [ ] **The entry and the way through a door** (`controllers.md`, 11.9)
  - Do: come into the Temple, and go through a door to a room.
  - Right: the room comes out of black at the rest pose, with no turn and no
    move after it. Wrong: a turn or a move after the black.
  - A/B: debug page, Temple, "Room flight".
- [ ] **The ray and the cross in a room** (`controllers.md`, 11.9)
  - Do: in each room, point at things near and far.
  - Right: the line ends on the thing in both eyes, and the cross has a good
    size on a far wall. Wrong: a line that ends before or behind the thing.
- [ ] **The size and the turn of a room** (`controllers.md`, 11.9)
  - Do: recenter, come into the rooms, point around, and turn with the
    stick.
  - Right: the size of a room and the height of the floor look right (the
    camera of the hall is about 1.3 m over the floor). The angle of the stick
    turn is good, and the room stays level. Wrong: a room that looks too
    small or too large, or that tilts.
- [ ] **The click in the Temple** (`controllers.md`, 11.9)
  - Do: click the controls of the rooms with the trigger and with the grip.
  - Right: each click arrives. Wrong: a click that does not arrive, or a
    click by accident.
  - A/B: debug page, Temple, "Temple pointer".
  - Log: `temple       : the game gets the mouse message`.
- [ ] **The pace of the Temple** (`controllers.md`, 11.9)
  - Do: in a game from a saved game, go through a door and look at a
    picture.
  - Right: the way through a door (5 s) and the look at a picture (1.4 s)
    have a good pace. Wrong: a pace that is too fast or too slow.
  - A/B: debug page, Render, "Real time".
- [ ] **A save in Program Files** (B37, `backend.md`, "A save in the
  VirtualStore")
  - Do: with the game in Program Files and no administrator rights, save a
    game in the Save Game Room. Then load it.
  - Right: the Save Game Room shows the slot, and it loads. Wrong: no slot,
    or the load fails.
- [ ] **The leash posts at the door of the Temple** (`village.md`, "Tests
  and limits of the Temple")
  - Do: tap each leash post with the real hand at the scale of the start.
  - Right: you hit the post that you want. Wrong: the nearest post wins when
    you aim at a different one.
  - A/B: no switch: `temple_taps` in the ini (1 or 0).
  - Log: `temple land  : the chosen leash post is`.

## Menus, cards and the ring

- [ ] **The menus in the room** (P18, `ui-layer.md`, section 20)
  - Do: start a new game with the name of a new god in the front end. In
    play, open the game menu, the options and the load box. Turn the head
    while a menu is open, and click with the ray.
  - Right: the panel has a good place, it stays in the room while the head
    turns, and a frame with no scene shows its screen. The ray and the arrow
    of the game meet on the panel. Wrong: the panel turns with the head, or
    the arrow is not at the end of the ray.
  - A/B: no switch: `menu_room` in the ini (1 the panel in the room, 0 the
    eye pictures).
  - Log: `menu room    : frame N, the menu panel goes in front of the head`.
- [ ] **The keyboard in the headset** (P18, F25, `ui-layer.md`, section 21)
  - Do: type a name for a new god, and a message in the box "Save Game
    Message" of the Save Game Room, with the ray and the trigger.
  - Right: the keyboard has a good place under the panel. The keys and the
    gold of the key under the ray read at 55 to 80 cm. Wrong: a keyboard at
    a wrong place, or keys that are hard to read.
  - A/B: debug page, Interface, "Card look" (scroll, plain).
  - Log: `keyboard     : frame N, the key A goes to the game`.
- [ ] **The ring** (`controllers.md`, 10.7)
  - Do: open the ring, and move the hand to the items.
  - Right: the ring is sharp, 5 cm is a good reach and 32 cm a good size
    (`radial_menu_reach_cm`, `radial_menu_size_pct`). The card in the middle
    is not in the way, and the order of the items is good. Wrong: a blurred
    ring, or a card that hides an item.
- [ ] **The hold of Y for the ring** (P13, `controllers.md`, 5.1)
  - Do: hold Y while the hand moves to an item.
  - Right: Y is easy to hold while the hand moves. Wrong: the thumb lets go.
  - A/B: debug page, View, "Button layout" and "Ring ways".
- [ ] **The menu button of the left controller** (P13, `controllers.md`,
  section 6)
  - Do: press the menu button of the left controller in play.
  - Right: the game menu opens each time. Wrong: Virtual Desktop keeps the
    button for its own menu. Then no other button opens the game menu.
  - A/B: debug page, View, "Button layout".
- [ ] **The look of the scroll cards and the ring** (F25,
  `controller-hints.md`, 9.11, and `controllers.md`, 10.4)
  - Do: look at the hint cards and the ring against the sky and the land, and
    work with both hands.
  - Right: the scroll has a good contrast, the grain and the ink do not
    shimmer, and the scroll does not hide the other hand more often than the
    plain card. The gold of the chosen seal stands out at a glance, the
    faded seals and the two lines of a switch read at 32 cm, and the thin
    ring helps. Wrong: low contrast, a shimmer, or a card that hides the
    other hand.
  - A/B: debug page, Interface, "Card look".
- [ ] **The hint card at the wrist** (`controller-hints.md`, 9.8)
  - Do: play with both hands. Move the pointing hand, turn it, and work with
    it.
  - Right: a letter of 8 mm reads at 40 to 60 cm. The side of the wrist is a
    good place, and the card does not often change side. The card does not
    step against the hand at a low frame rate. Wrong: a card that covers the
    hand, changes side often, or steps.
  - A/B: debug page, Interface, "UI layer". The size: `hint_card_size_pct`
    in the ini (100, 50 to 200).
- [ ] **The prompts in play** (`controller-hints.md`, 9.8)
  - Do: play the first minutes of a game. Take, fetch, and move the Hand to
    targets.
  - Right: the prompts come and go at good times. The words are right at the
    end of a take and of a fetch, and when the Hand leaves a target. Wrong: a
    prompt that is late or stays too long, or a word that flashes.
  - A/B: debug page, Interface, "Prompt timing" and "Hint words".
- [ ] **The left card and the lines of the moves** (`controller-hints.md`,
  9.8)
  - Do: start the tutorial, and work with the right hand while the left card
    shows.
  - Right: the left card is easy to see, and "Stick Move" and "A Teleport"
    teach the two ways to move. Wrong: you miss the left card.
  - A/B: debug page, Interface, "Prompt moves".
- [ ] **The lines that the player knows** (`controller-hints.md`, 9.8)
  - Do: play as a new player for some time.
  - Right: a line goes away after 3 uses and 10 minutes of play, and you do
    not miss it. Wrong: a line goes away too soon, or stays too long.
  - A/B: no switch: the launcher row "Hints" (`hint_learn`).
- [ ] **The words of the key caps and of the help text**
  (`controller-hints.md`, 8.6)
  - Do: play with `help_test=1`. Put the controller down, use the mouse, and
    take the controller again. Send the log.
  - Right: the word in a key cap reads at its size. The help text has the
    words of the device that you use. Wrong: a cap word that is too small,
    or the words of the wrong device.
  - A/B: no switch: `controller_hints` in the ini (1 or 0).
  - Log: `hints : census` (the action numbers `$m1` and `$m2`).
- [ ] **The prompt of a text that waits for a click** (`controller-hints.md`,
  10.5)
  - Do: play a quest dialogue that waits for a click while the text is on the
    subtitle card.
  - Right: you see "[Trigger] Continue" on the card at the wrist. Wrong: you
    do not see it.
  - A/B: debug page, Interface, "Text click".
- [ ] **The subtitle card** (`ui-layer.md`, section 17)
  - Do: play a story with subtitles while the hands work. Turn the head away
    for more than 1 s. Open the Creature panel.
  - Right: the letters read well 17 degrees below eye level at 1.6 m. After
    the turn the card moves one time, with its fade. The Creature panel
    shows on the card. Wrong: the card moves while the head moves, the
    letters are hard to read, or the text is cut.
  - A/B: debug page, Interface, "UI layer". The place:
    `subtitle_card_distance_cm` and `subtitle_card_down_deg` in the ini.
- [ ] **The name of a villager** (B17, `ui-layer.md`, section 18)
  - Do: put the Hand on a villager and choose the ring item `details`.
  - Right: the name reads as well as the bubble of a signpost. Wrong: the
    name is too small or double.
  - A/B: debug page, Interface, "Bubble card".
- [ ] **The card of a signpost in one walk** (B23, `ui-layer.md`, section 18)
  - Do: read the text of a signpost.
  - Right: the text is in its box and easy to read, in both eyes. Wrong: the
    text is outside its box in one eye.
  - A/B: debug page, Picture, "Walk marks".
- [ ] **A note over the Creature** (B33, `ui-layer.md`, section 19)
  - Do: cast a miracle where the Creature sees it, so that the game shows the
    yellow percent of a lesson over the Creature.
  - Right: the note shows one time, over the head of the Creature, at its
    depth. Wrong: the note is double.
  - A/B: debug page, Interface, "Note depth".
  - Log: `note depth   : the game shows a note over a thing`.
- [ ] **A hint at the game's Hand** (B34, `ui-layer.md`, 15.6)
  - Do: let the game place the Hand (the mouse, or controllers that are not
    tracked), and move it over a thing with a tooltip.
  - Right: the yellow text shows one time, at the depth of the Hand. Wrong:
    the text is double.
  - A/B: debug page, Interface, "Hint depth".
- [ ] **The tooltip at the Hand's point** (`ui-layer.md`, 15.5)
  - Do: with the hint cards off, move the Hand's point over things that have
    a tooltip.
  - Right: the label of 2.5 cm reads well at 40 to 80 cm, at the depth of the
    Hand. Wrong: a double label, or a label that the Hand hides.
  - A/B: debug page, Interface, "UI layer".
- [ ] **The panel layers of the interface** (N15, `ui-layer.md`, section 9)
  - Do: set `ui_panel=1`. Play, open a menu, let a subtitle and a help text
    show, and recenter.
  - Right: the menu quad and the help quad have a good place and size, a
    clean alpha, sharp menu letters and help letters that do not crawl. Both
    stay at their place after the recenter. Wrong: a halo, a hole, dark
    letter edges, or a quad at a wrong place.
  - A/B: debug page, Interface, "UI layer".
- [ ] **The Creature panel on its quad** (`ui-layer.md`, 13.3)
  - Do: with `ui_panel=1` and `creature_panel=2`, open the Creature panel.
  - Right: the quad is near the Creature, 15 cm to the left of the Hand, with
    a clean alpha. Wrong: the quad covers the Hand.
  - Log: `ui panel     : the Creature panel shows`.
- [ ] **The debug page** (`controllers.md`, 10.8)
  - Do: open the debug page, and go through three pages to a switch.
  - Right: the badges of two lines are easy to read at 32 cm, and 0.45 s on
    each link is not too slow. Wrong: badges that are hard to read.

## Cut scenes and stories

- [ ] **A cut scene around you** (P17, `cutscene-window.md`)
  - Do: set the launcher row "Cut scenes" to "Around you", and play stories
    and a video.
  - Right: the cut scene plays around you with the camera of the game, and
    its motion is comfortable. A video plays on the screen of the window.
    Wrong: the motion makes you sick. Say which scene.
  - A/B: no switch: the launcher row "Cut scenes" (`cutscene_immersive`).
- [ ] **The layers of the cut scene window** (`cutscene-window.md`, section
  11)
  - Do: play a story of the God view.
  - Right: the walls, the two eye pictures with their crop, the black past
    them and the subtitle card over them all show. Wrong: a picture shows the
    wrong part, or the card is not over it.
  - A/B: debug page, Picture, "Scene window".
- [ ] **The opening and the depth of a shot** (`cutscene-window.md`, section
  11)
  - Do: watch stories with close shots, cuts, and things that come near.
  - Right: the left edge of the opening has no column of light, and the
    opening reads as one solid rectangle. A close shot is comfortable and
    its scale looks right. The change of the depth at a cut and at a near
    thing does not show much, and the depth comes back at a good speed.
    Wrong: a column of light, a step of the depth that you see, or a close
    shot that hurts the eyes.
  - A/B: debug page, Picture, "Scene depth".
- [ ] **The video after the drowning boy** (`cutscene-window.md`, section 11)
  - Do: play that video, and turn and tilt the head.
  - Right: the video stays on the screen of the window. Wrong: it moves with
    the head.
  - A/B: no switch: `cutscene_video` in the ini (1 or 0).
- [ ] **The place of the screen and of the subtitles** (`cutscene-window.md`,
  section 11)
  - Do: start a story while the head looks down, and again while it looks to
    the side.
  - Right: the screen comes at a good place, and the subtitles under it have
    a good size (21 to 26 degrees below eye level for two lines). Wrong: a
    screen out of view, or subtitles that are too low or too small.
  - A/B: no switch: `cutscene_face_head` and `cutscene_subtitle_place` in the
    ini.
- [ ] **The advisors** (`cutscene-window.md`, section 11, and `tabletop.md`,
  "The spirits beside the table")
  - Do: watch an advisor in a story, and in the God view. Lean and walk.
  - Right: an advisor in the window is at a good place (1.33 m), and its
    depth looks right as it comes in from a side. In the God view the
    advisors float beside you at a place that is easy to see, and they stay
    fixed in the room. Wrong: an advisor that jumps in depth, is too low
    (a desktop run measured 83 cm below the head), or moves with the head.
  - A/B: no switch: `cutscene_advisors` and `cutscene_advisor_distance_cm` in
    the ini.

## The intro

Each check needs the launcher row "Intro in stereo" (`intro_vr=1`). The ring
does not open in the intro, so each A/B is an ini key. After these checks,
choose the default of `intro_motion`, and decide if `intro_vr` goes on (P11).

- [ ] **The doubling and the distance of the logo** (P11, `intro.md`)
  - Do: watch the Lionhead logo.
  - Right: the eye sees each orb one time, and the logo is in the reach of
    the arms. Wrong: orbs that show double, or a logo too far or too near.
  - A/B: no switch: `intro_eye_trails` (1 or 0), `intro_text_place` (1 or
    0) and `intro_distance_cm` (300) in the ini.
- [ ] **The motion of the intro** (P11, `intro.md`)
  - Do: watch the logo with `intro_motion=1`, then 2, then 3.
  - Right: one value is comfortable. 2 (the orbs as a model in a room that
    stays still) keeps the rule of no motion that the player did not make.
    Wrong: a motion that makes you sick, or an orb large in one eye only.
  - A/B: no switch: `intro_motion` (1, 2, 3 or 0) and `intro_speed_pct` in
    the ini.
- [ ] **The size of the logo** (`intro.md`)
  - Do: watch the start of the logo.
  - Right: the logo has its full size. Wrong: the logo is smaller.
  - Log: `intro` (the width of the logo at the first stereo frame).
- [ ] **The trails in each eye** (`intro.md`, "The trails")
  - Do: watch the logo with the renderer Vulkan, and the same effect while
    the game loads a saved game.
  - Right: each eye has its trails, and no eye flickers. Wrong: one eye
    flickers.
- [ ] **The push of the hands** (P11, `intro.md`)
  - Do: move the real hands through the orbs.
  - Right: the orbs move with each hand in its reach (30 cm,
    `intro_push_radius_cm`), and an orb in front of a hand draws over it.
    Wrong: orbs that move at a distance from the hand, or not at all.
  - A/B: no switch: `intro_push_3d` (1 or 0) and `intro_hand_depth` (1 or 0)
    in the ini.
- [ ] **The show of the hands** (P11, `intro.md`)
  - Do: hold a hand still, move it, and let it rest.
  - Right: a hand at rest does not flicker in and out, and the times to come
    and to go feel right. Wrong: a hand that flickers.
  - A/B: no switch: `intro_hand_show` in the ini (0, 1 or 2).
- [ ] **The skip of the logos** (`intro.md`, "The skip")
  - Do: press the trigger in the logo screens. On another start, press the
    menu button in the logo in stereo.
  - Right: the press skips the rest of the logos. Note how early in the first
    logo screen a press works. Wrong: a press does nothing.
  - A/B: no switch: `intro_skip` in the ini (1, 2 or 0).
  - Log: `intro skip   : a press of the right trigger skips the logo screens`.

## The picture in each eye

- [ ] **No old picture in one eye** (B16, `stereo-artifacts.md`, "The frame
  before of each eye")
  - Do: play the land for a long time, and look for faint pictures that come
    late in one eye.
  - Right: no eye shows an old picture. Wrong: one eye shows faint late
    pictures. Then send the log.
  - A/B: debug page, Picture, "Eye clean" (shared, own).
  - Log: `eye clean : an eye pass drew no sky` names the scene of that frame.
- [ ] **A lightning in both eyes** (B18, `backend.md`, "The lights on the
  land in the second eye pass")
  - Do: look at the land at a lightning of a storm, at the end of the quest
    of the singing stones, and at the Lightning Bolt miracle.
  - Right: the land is white in both eyes. Wrong: white in the left eye only.
    Then send the log: a flash with no `land light` line is not a light on
    the land.
  - A/B: debug page, Picture, "Land light".
  - Log: `land light` (the counts of the lights of each group).
- [ ] **The margin around the view** (P16, `backend.md`, "A margin around the
  view")
  - Do: set the launcher row "Margin around the view" to 10 and to 20. Turn
    the head fast, and look at the Hand, the pointer, the Creature panel and
    a tooltip.
  - Right: the world keeps its place and its scale, and the Hand and the
    pointer are where the controller is. A fast turn shows less black at the
    edge. Say which margin is enough at 90 and at 120 Hz. Wrong: the world
    moves or scales, or the interface is at a wrong place.
  - A/B: no switch: the launcher row "Margin around the view"
    (`fov_margin_pct`, 0 to 30). Use `profile=1` for the cost.
- [ ] **The light maps of the Temple with Vulkan** (`backend.md`, "The
  second stage")
  - Do: with the renderer Vulkan, go into the hall of the Temple.
  - Right: the hall has the light of the chandeliers and dark corners.
    Wrong: the walls and the floor have the full color of their texture and
    no shadow.
  - A/B: debug page, Picture, "Stage 1".
- [ ] **The colors with the renderer Vulkan** (`backend.md`, "The backend of
  the mod on Vulkan")
  - Do: play with the renderer Vulkan, and look at the land, the Temple and a
    cut scene.
  - Right: the colors are as with Direct3D 9, and no eye flickers. Wrong:
    other colors, or a flicker.
  - A/B: no switch: the launcher row "Renderer".
- [ ] **White or invisible story people with Vulkan** (B38)
  - Do: with the renderer Vulkan, play the quest of the Lost Brother and the
    Lost Sister of Land 1, and the cow stone with Sable.
  - Right: the people have their normal colors, during and after the quest.
    Wrong: they are flat white, or invisible with only the eyes. Then send
    the log.
  - Log: `a texture with no new image`.
- [ ] **The eyes of the Creature with the Vulkan eyes** (`backend.md`, "The
  session with Vulkan")
  - Do: with the renderer Direct3D 9 and the launcher row "Headset session
    with Vulkan" on, look at the face of the Creature from near.
  - Right: the Creature has its eyeballs. Wrong: empty or flat eyes.
  - A/B: debug page, Picture, "Vulkan eyes".
- [ ] **No black flicker with the Vulkan eyes** (`backend.md`, "The session
  with Vulkan")
  - Do: with the renderer Direct3D 9 and "Headset session with Vulkan" on,
    look at the first seconds of the session, and stroke the Creature.
  - Right: no black flicker. Wrong: black frames.
  - A/B: debug page, Render, "Follow target".
- [ ] **Foveated resolution** (`backend.md`, "Foveated resolution")
  - Do: turn on the launcher row "Fewer pixels away from the middle"
    (`fovea_res`), and look around with the eyes still and to the side.
  - Right: no edge shows between the center and the outer part. Wrong: an
    edge, or a coarse picture near the middle.
  - A/B: debug page, Render, "Fovea images". Foveated rendering: debug page,
    Render, "Foveation".
- [ ] **The sun with each value of the sun glare** (`backend.md`, "The sun
  glare")
  - Do: look at the sun and move the head from side to side, with each value
    of the launcher row "Sun glare".
  - Right: the sun stays at one place in the sky. Say which value stops a
    bob. Wrong: the sun bobs a little as the head moves.
  - A/B: no switch: `sun_glare` (0, 1 or 2).
- [ ] **The merge of draw calls** (N1, `backend.md`, "One draw call for a row
  of draw calls")
  - Do: set `draw_merge_test=1` in the ini for one session. Play in the
    Temple, in a story, in a fight of Creatures and with the effect of a
    miracle.
  - Right: the picture has no fault, and the log has 0 eye passes with a
    different checksum. Wrong: a missing triangle, or a count over 0.
  - A/B: no switch: `draw_merge` in the ini (1 or 0).
  - Log: `draw merge   : the check of the triangles`.

## Sound

For a comparison of the sound of the world, use the debug page, Sound,
"Sound step". "QMixer pan" is the sound of QMixer, and "as set" is the
sound of the mod. Use a game with the Creature at the door of the
Temple (`audio.md`, section 14).

- [ ] **The height of the sounds** (`audio.md`, section 14)
  - Do: stand still and look ahead, with the Creature on the land at your
    waist. Listen to its steps, its voice and a bird.
  - Right: the steps and the voice come from below and ahead, and the bird
    from the height of your ears or over them. Wrong: the Creature sounds at
    the height of your ears.
- [ ] **A sound near one ear** (`audio.md`, section 14)
  - Do: put your head 20 cm from the Creature, or hold a thing that makes a
    sound near one ear.
  - Right: the sound stays in both ears. Wrong: it sits hard in one ear.
- [ ] **The sea** (`audio.md`, section 14)
  - Do: go to the shore, and note the ear of the sea. Turn the view with the
    stick 4 steps, and then turn the head.
  - Right: the sea stays where it is in the world, and a wave comes from the
    side of the sea. Wrong: the sea turns with the view.
- [ ] **The steps of the Creature** (`audio.md`, section 14)
  - Do: put the head near the Creature, move away, and turn the head.
  - Right: the steps are clear in the reach of the arms, become quiet at 1 to
    2 m, and stay where the Creature is. Wrong: the steps move with the head.
- [ ] **The zoom** (`audio.md`, section 14)
  - Do: stand in a village and zoom out to the far end. Throw a rock at a
    house, or cast a miracle. Zoom in again.
  - Right: the voices and the small sounds go away, the crash carries, low
    and dull, and the small sounds come back. Wrong: small sounds at a far
    zoom, or no crash.
- [ ] **Distance and loops** (`audio.md`, section 14)
  - Do: teleport away from a sound that loops (a fire, the Temple, a vortex),
    and back.
  - Right: the loop gets quiet and dull, and it is there again with no cut.
    Wrong: the loop stops, or it cuts in.
- [ ] **The cues of the moves** (`audio.md`, section 14)
  - Do: make a turn step, a zoom step, a teleport and a grip on the land.
  - Right: each has a soft sound at a good level. Wrong: no sound, or a loud
    one.
  - A/B: no switch: `audio_cues_pct` (100, 0 to 300) in the ini.
- [ ] **The advisors and the stories** (`audio.md`, section 14)
  - Do: listen to the advisors and to the speech of a story.
  - Right: they sound as in the game. Wrong: a voice from a side, or faint.
- [ ] **The loop of a bulk take** (B20, `audio.md`, 6.4)
  - Do: take wood or grain with the Hand, and listen while the take runs.
  - Right: the loop sounds good, at a good level beside the other sounds.
    Wrong: a loop that is too loud, too quiet, or gone.
  - A/B: debug page, Sound, "Take loop".
- [ ] **The stop of a sound with no wait** (`audio.md`, 7.6)
  - Do: listen at the village and at the door of the Temple with the head
    down, where the game stops many sounds.
  - Right: a sound that stops sounds the same with "skip" and with "wait".
    Wrong: a click, a cut, or a sound that does not stop with "skip".
  - A/B: debug page, Sound, "Stop wait".
- [ ] **Faults of the sound** (`audio.md`, section 14)
  - Do: listen all the time while you play.
  - Right: no click, no sound from a wrong side, and no sound that lags a
    fast turn of the head. Wrong: one of these. Say when, and send the log.
- [ ] **The latency of the sound** (`audio.md`, section 14)
  - Do: if the sound lags a turn of the head, set `audio_latency_ms=25` in
    the ini and play again.
  - Right: the log says about 66 ms and "passed a write 0 times". Wrong: a
    crackle, or a count over 0. Then set the key to 0 again.
  - Log: `QMixer writes the stream`.

## Performance

For each check of this section, set `profile=1` in `bwvr.ini` after the last
save of the launcher (the launcher removes the key), and send the log.

- [ ] **A session with `profile=1`** (N1, `backend.md`, "Frame cost")
  - Do: play a normal session with light and heavy scenes.
  - Right: the log has the parts that only a headset has: the wait for the
    runtime, the copy to the bridge and the frame to the runtime, also in a
    slow frame.
  - Log: `frame cost   : window` and `slow frame   :`.
- [ ] **Frames of about 100 ms** (N1, `backend.md`, "The slow frames")
  - Do: play with the performance overlay of Virtual Desktop on. Turn the
    head, and then keep it still. Do it again with the renderer Direct3D 9
    and the launcher row "Headset session with Vulkan" off.
  - Right: the worst frame of a report window stays far below 94 ms. Wrong:
    frames of 94 to 108 ms. Say if they come at a turn of the head, and what
    the overlay shows (the latency of the network, the frames that the
    headset drops).
  - Log: `runtime calls:` and `worst frame`.
- [ ] **About 100 frames a second at 120 Hz** (N1, `backend.md`, "The
  refresh rate of the headset")
  - Do: run `bwvr-xrtest 30 vulkan` with no game. Then play one session with
    a smaller render size, and one with the performance overlay of Virtual
    Desktop on.
  - Right: the pace of the runtime with no game, and the time of the encoder
    and of the network, are in the log and the overlay.
- [ ] **The walk with the stick at 120 Hz and at 90 Hz** (B27, `god-view.md`,
  5.7)
  - Do: walk with the stick at 120 Hz in Virtual Desktop. Then walk at 90 Hz,
    or with Space Warp on.
  - Right: the walk is smooth. If it is smooth at 90 Hz only, the fault is
    the frame rate. Wrong: the move stops for a refresh and jumps.
  - Log: `Of N frames of the headset, M came more than one refresh` (the end
    of each walk).
- [ ] **One wait a frame, and a second thread** (N1, `backend.md`, "One wait
  a frame, and a second thread")
  - Do: at one place, with the head still, play with each value of the
    switch, also with the subtitle card on.
  - Right: the frame rate is the same or higher than with "each eye", and
    the picture has no fault. Wrong: a lower frame rate, or a fault.
  - A/B: debug page, Render, "Frame send". Debug page, Interface, "Subtitle
    copy" for the card.
  - Log: `frame age    :`.
- [ ] **The feel of one walk for both eyes** (N1, `backend.md`, "One walk for
  both eyes")
  - Do: play with one walk on and off, and move the head and the Hand.
  - Right: one walk feels the same or smoother, and nothing is missing at the
    outer edge of the right eye. Wrong: a thing in one eye only.
  - A/B: debug page, Render, "One walk".
- [ ] **The render thread in more scenes** (N1, `backend.md`, "The render
  thread")
  - Do: play the Temple, a story and the load of a saved game with the
    render thread on. Then choose the default of `render_thread`.
  - Right: no fault of the picture and a shorter frame. Wrong: a fault, or a
    stop at the logos. `hang_watch=1` shows the place of a stop.
  - A/B: debug page, Render, "Render thread".
- [ ] **The vertex loops with SSE** (N1, `backend.md`, "The vertex loops
  with SSE")
  - Do: play a heavy scene with each value.
  - Right: the same picture, and a shorter scene walk.
  - A/B: debug page, Render, "SSE loops".
- [ ] **The threads spin, then sleep** (N1, `backend.md`, "How the threads
  wait")
  - Do: play a heavy scene with each value.
  - Right: the frame rate is the same with "sleep". Wrong: a lower frame
    rate, or more slow frames.
  - A/B: debug page, Render, "Thread wait".
- [ ] **The parts of the Vulkan session** (N1, `backend.md`, "The session
  with Vulkan" and "The Vulkan renderer")
  - Do: play with each of the three switches on and off.
  - Right: the picture is the same, and the frame is shorter or the same.
    With a hint card in view no report window has frames of 32 ms. Wrong: a
    fault of the picture, of a card or of the panel.
  - A/B: debug page, Render, "Vulkan upload" and "Vulkan only", and
    Interface, "Vulkan panel".
- [ ] **The frame time with the Vulkan eyes** (`backend.md`, "The session
  with Vulkan")
  - Do: with the renderer Direct3D 9 and "Headset session with Vulkan" on,
    play one scene with each value.
  - Right: the frame is shorter with the Vulkan eyes.
  - A/B: debug page, Picture, "Vulkan eyes".
- [ ] **The read ahead of the files** (N1, `backend.md`, "The read ahead of
  the files of the game")
  - Do: load a saved game and play the first minutes, with the read ahead on
    and off.
  - Right: with the read ahead on, the load and the first minutes have no
    hitch. Wrong: a hitch when the Creature does a thing for the first time.
  - A/B: debug page, Render, "Read ahead".
  - Log: `read ahead   :`.
- [ ] **The hold of the last frame** (N1, `backend.md`, "The hold of the
  last frame")
  - Do: load a saved game, and turn the head while the game loads the land.
  - Right: the last picture stays and turns with the head. Wrong: black
    flicker, or a picture that does not turn.
  - A/B: debug page, Render, "Frame hold".
  - Log: `hold         :` and `slow frame   :`.
- [ ] **A session with DXVK** (N1, `backend.md`, "DXVK under dxwrapper")
  - Do: with the renderer Direct3D 9, turn on the launcher row "Direct3D 9
    through DXVK" (`d3d9_backend=1`), and play.
  - Right: the session starts, and the picture is the same. Wrong: no
    session, a black picture, or a crash.
  - Log: `gpu bridge   :`.
- [ ] **The copy to the monitor** (`backend.md`, "The window with Vulkan")
  - Do: set `skip_desktop_present=0` in the ini, and play with the renderer
    Vulkan.
  - Right: the monitor shows the picture, and the frame rate in the headset
    stays at the rate of the headset. Wrong: a black window, or a lower
    frame rate.
- [ ] **The cost of the overlay** (`hand-model.md`, "Overlay renderer")
  - Do: play with the hands, the rays and the glow ribbons in view.
  - Right: the overlay adds little to the frame time.
- [ ] **The frame rate of the stereo intro** (`intro.md`)
  - Do: watch the logo in stereo.
  - Right: the frame rate holds. Wrong: it falls in the logo.

## Stability and the start of a session

- [ ] **No crash in a long play** (B38, `backend.md`, "The address space")
  - Do: with the launcher row "Use 4 GB of memory" on, play for an hour, to
    the choice of the Creature and past the quest of the Lost Brother. Do it
    with the God view and with the flat view.
  - Right: no crash. Wrong: the game stops (for example in `nvoglv32.dll`),
    and the dump fails. Then send the log.
  - A/B: no switch: the launcher row "Use 4 GB of memory" (`large_address`).
  - Log: `address space`.
- [ ] **The start of the session** (B21, `backend.md`, "The start of the
  headset session")
  - Do: start the game with the headset connected. Then start it with the
    headset not connected, and connect it in the 2 minutes of the tries.
  - Right: the session starts in each case. Wrong: the game stays on the
    monitor.
  - Log: `vr start     : try N found the headset`.
- [ ] **The fall back of the start** (B21, `backend.md`, "The start of the
  headset session")
  - Do: set `vr_start_test_fail_session=1` in the ini, and start the game
    with the headset connected.
  - Right: the session starts on the frame thread. Wrong: no session.
  - Log: `the start through the thread of the tries failed at`.
- [ ] **The end of the session at Quit** (`backend.md`, "The end of the
  mod")
  - Do: quit the game from the game menu.
  - Right: the game closes with no fault. Wrong: a fault, for example
    `0xc0000409` in `VirtualDesktop.LibOVRRT32_1.dll`.
  - Log: the last line `shutdown     : the mod is shut down`.
- [ ] **The fall back of the renderer Vulkan** (`backend.md`, "The fall back
  of the renderer Vulkan")
  - Do: play some seconds with the renderer Vulkan, quit, and open the
    launcher.
  - Right: the row "Renderer" shows Vulkan, and the launcher does not name
    `bwvr-vulkan-pending.txt`. Wrong: the next start uses Direct3D 9.
- [ ] **A controller after a long cut scene** (B24, `controllers.md`,
  section 6)
  - Do: turn off the setting of the headset that switches between the
    controllers and the hands. Hold a controller still in a long cut scene.
  - Right: the controller stays on. Wrong: the controllers do nothing until a
    press.
- [ ] **The menu of Virtual Desktop** (`controllers.md`, 5.5)
  - Do: open the menu of Virtual Desktop while the trigger is held, and close
    it with the trigger still down.
  - Right: the button lets go when the menu opens, and no press arrives until
    the trigger goes up and down again. Wrong: a button that stays down, or a
    press after the menu closes.
  - A/B: debug page, View, "Pointer owner".
- [ ] **The game window behind** (`controllers.md`, 9.5)
  - Do: start the game, click a different window on the desktop, and put the
    headset on. Then open the menu of the headset.
  - Right: the hands, the buttons and the sticks act. Nothing acts in the
    game while the menu of the headset is open. Wrong: a control that does
    not act, or an action while the menu is open.
  - Log: `the game window is behind`.

## The miracles of the game

This section says what each miracle does in the plain game, so that you can
compare the mod with it.

**The sources.** The game's own text (the help text, the tooltips, the tips
of the loading screen) is in `Scripts\InfoScript2.txt` of the game folder.
The costs and the gestures are in `Scripts\info.dat`. The words here are a
short summary of that text. Where the game files do not say a thing, this
section says "not confirmed".

**Prayer power.** Each miracle costs prayer power from the Worship Site.
The worshippers of the village make it. The costs below are the cost field
of `info.dat`. Desktop runs confirm it for Water (5000), Food (7000), Heal
(6000) and Forest (13000).

**The gestures.** A miracle of the god starts with the spiral. A Creature
miracle starts with the inverse spiral. Then comes the gesture of the
miracle. The names of the shapes below come from the order of the game's
list of gesture types. The spell book and the row over the palm show the
real picture of each one.

**No shower.** The game has no shower or mega version of a miracle. A
stronger version is a power up: draw its gesture (the spiral or the inverse
spiral) while the miracle is in the Hand. Only a miracle from a Worship Site
can power up. The announcer calls the steps "Increase", "Extreme" and
"Maximus". "Shower of Life" is only the name of the Water miracle.

**How to get a miracle in the mod** (`miracles.md`, `village.md`):

1. The spell book: press X (or turn the palm of that hand up), put the
   pointing hand on a miracle, close the grip, and pull 10 cm.
2. A gesture in the air: hold B and draw the spiral and then the gesture of
   the miracle.
3. The Worship Site: tap the grip on the icon of the miracle.
4. A one-off bubble on the land: a short grip on it makes it active. Or
   carry it, and press the trigger.

**How to cast it in the mod** (`miracles.md`, "The grip casts"):

| Kind | Miracles | What you do |
| --- | --- | --- |
| At a place | Heal, Forest, Teleport, Explosion | Close the grip with the Hand over the place |
| A pour | Water, Food, Wood, Lightning | Keep the grip closed over the place. Open it to stop |
| A throw | Fireball, the two flocks | Close the grip, swing the hand, and open it |
| A circle | Storm, the two shields | Close the grip and draw a circle over the land. The cast comes when the circle closes |
| On a Creature | The Creature miracles | Close the grip with the gold halo on a Creature (at the Hand, or in the aim) |

Take a miracle out: tap B, or shake the hand. A miracle from a Worship Site
gives its prayer power back. A one-off miracle that you take out is lost.

**What the desktop runs show.** A desktop run shows that the game gets the
cast (the log line `the game casts ...`). It does not show the effect in the
world over time, for example a field that grows. That is for you to see.

**Which miracles you have.** The towns of each land give their miracles.
The debug switch "All miracles" (debug page, Creature) gives each town all
miracles with no cost. But a Worship Site shows only the first 6 that get a
slot, for example Storm, Forest, Fireball, Food and the two shields. Then
Water and Heal do not show.

### Water ("Shower of Life")

- **Plain game.** It makes a flow of water where the Hand is, for as long as
  you hold the cast. The game's tips give three uses:
  - It puts out a fire.
  - It waters a field, so that the crops grow faster.
  - It waters a forest, so that the forest grows larger and gives more wood.
  
  The Creature can learn it and water fields, trees and fires. Not
  confirmed: what it does to a villager or to the Creature (no game file says
  it does anything).
- **How to see that it worked.** The fire test is the clear one: set a house
  on fire with a Fireball, and pour Water on it. The fire must go out. The
  growth of a field or a forest is slow, and the game gives no message for
  it. Not confirmed: how fast the growth is, and whether a field shows a
  change at once.
- **Cost.** 5000. The power up "Water Miracle Increase" (spiral): 7000.
- **Gesture.** Spiral, then the W shape. A pour.
- **In the mod.** Keep the grip closed over the place, and open it to stop.
- **Proved.** Desktop runs: the pour, the request from the spell book, and
  a power up from a Worship Site.
- [ ] Pour Water on a fire. The fire goes out.
- [ ] Pour Water on a field and on a forest. Note what you see.

### Food ("Horn of Plenty")

- **Plain game.** It makes food, which drops on the land under the Hand while
  you hold the cast. The Creature can learn it.
- **How to see that it worked.** Food lies on the land under the Hand. Not
  confirmed: the look of the pile, and whether the villagers carry it to the
  Village Store by themselves.
- **Cost.** 7000. The power up (spiral): 10000. Not confirmed: what the power
  up makes. One tooltip calls it "Food Miracle Extreme". The help text and
  another tooltip call it "Poisoned Food", food that makes people sick.
- **Gesture.** Spiral, then the S shape. A pour.
- **In the mod.** Keep the grip closed over the place.
- **Proved.** Desktop runs: the pour, the request from the spell book, and a
  power up.
- [ ] Pour Food near a village. Food lies on the land under the Hand.
- [ ] Power up Food, and pour it. Say what it makes (more food, or food that
  makes people sick).

### Wood ("Structure Elemental")

- **Plain game.** It makes wood on the land under the Hand while you hold the
  cast. The villagers use wood to build. The Creature can learn it.
- **How to see that it worked.** Wood lies on the land, and you can take it
  to the Village Store or to a scaffold.
- **Cost.** 7000. No power up.
- **Gesture.** Spiral, then the three. A pour.
- **In the mod.** Keep the grip closed over the place.
- **Proved.** Desktop runs: the pour.
- [ ] Pour Wood, and take the wood to the Village Store.

### Forest ("Seeds of Existence")

- **Plain game.** It puts a forest at the place that you choose. The trees
  give wood to the foresters. The Creature can learn it.
- **How to see that it worked.** New trees stand at the place.
- **Cost.** 13000. No power up.
- **Gesture.** Spiral, then the Cyrillic L. At a place.
- **In the mod.** Close the grip with the Hand over the place.
- **Proved.** Desktop runs: the cast at a place.
- **A fault of the mod.** Testers find that the camera turns in a circle at
  the Forest miracle (B39). No game file says that the plain game turns the
  camera there.
- [ ] Cast Forest. Trees stand at the place under the Hand.

### Heal ("Wellness")

- **Plain game.** It heals people. The power up heals all the people of an
  area. The plague quest of the game uses it. The Creature can learn it.
- **How to see that it worked.** A sick or hurt villager recovers. In the
  plague quest the good spirit says when you heal one. Not confirmed: a
  picture that shows the heal on a villager.
- **Cost.** 6000. The power up (spiral): 9000.
- **Gesture.** Spiral, then the heart. At a place: the game casts it at a
  place on the land, not on one villager.
- **In the mod.** Close the grip with the Hand over the people.
- **Proved.** Desktop runs: the cast at a place, and the request from the
  spell book.
- [ ] Cast Heal over sick villagers (for example in the plague quest). They
  recover.

### Fireball ("Sphere Aflame")

- **Plain game.** A ball of fire that you throw. The first power up is a
  multi-Fireball, and the second is the largest Fireball. The Creature can
  learn it. The help text says only how to throw it. That it sets the place
  where it lands on fire is known from play, not from a game file.
- **How to see that it worked.** The ball flies from the hand, and the place
  where it lands burns.
- **Cost.** 3500. The power ups (inverse spiral, then spiral): 7000 and
  10000.
- **Gesture.** Spiral, then the vertical line. A throw.
- **In the mod.** Close the grip, swing the hand, and open the grip. It flies
  with the speed of the hand.
- **Proved.** Desktop runs: the throw with a step of the hand, not with a
  real swing.
- [ ] Throw a Fireball at a house. It flies where you swing, and the house
  burns.

### Lightning ("Forked Fire")

- **Plain game.** A bolt of lightning at the target, for as long as you hold
  the cast. The power ups give a larger, more forked bolt. The Creature can
  learn it.
- **How to see that it worked.** The bolt starts at the tip of the index
  finger and hits the place under the Hand. The land goes white in both eyes
  (B18). Not confirmed: what the bolt does to each kind of target.
- **Cost.** 5000. The power ups (inverse spiral, then spiral): 7500 and
  10000.
- **Gesture.** Spiral, then a fork. A pour.
- **In the mod.** Keep the grip closed over the target.
- **Proved.** Desktop runs: the pour.
- [ ] Cast Lightning. The bolt comes from your index finger and hits the
  place under the Hand.

### Storm ("Storm of Ages")

- **Plain game.** A storm over a circle that you draw. The plain storm is
  wind. The first power up adds rain, and the second adds rain and
  lightning. A storm puts out fires. The game also has a help text for a
  tornado. The Creature can learn the wind, the rain storm, the lightning
  storm and the tornado. Not confirmed: how a player makes a tornado, what
  damage a storm does, and whether its rain waters crops.
- **How to see that it worked.** Wind, then rain and lightning with the power
  ups, over the circle. A fire in the circle goes out.
- **Cost.** 8000. The power ups (inverse spiral, then spiral): 15000 and
  20000.
- **Gesture.** Spiral, then a fork. It needs its own circle.
- **In the mod.** Close the grip and draw a circle over the land. The cast
  comes when the game matches the circle.
- **Proved.** Desktop runs: the circle and the cast.
- [ ] Cast Storm over a village. The storm stands over the circle.

### Spiritual Shield ("Sanctuary")

- **Plain game.** A shield over a circle that you draw. Miracles cannot go
  in, but solid objects pass through. The shield takes damage when it is
  attacked, and it uses prayer power for as long as it stands. People under
  a shield cannot be impressed. The Creature can learn it.
- **How to see that it worked.** A dome stands over the circle. A miracle
  cast at it does not get in.
- **Cost.** 7000, and more for each second that it stands. No power up in
  `info.dat`, although a help text names a larger shield.
- **Gesture.** Spiral, then a fork. It needs its own circle.
- **In the mod.** Close the grip and draw a circle over the land.
- **Proved.** Desktop runs: the circle and the cast.
- [ ] Cast the shield over a village. The dome is over the circle that you
  drew, at its size.

### Physical Shield ("Armor of Faith")

- **Plain game.** A shield over a circle that you draw. It stops rocks and
  other solid objects, but miracles go through. It uses prayer power for as
  long as it stands. The Creature can learn it.
- **How to see that it worked.** A dome stands over the circle. A rock that
  you throw at it stops.
- **Cost.** 7000, and more for each second that it stands. No power up.
- **Gesture.** Spiral, then a fork. It needs its own circle.
- **In the mod.** Close the grip and draw a circle over the land.
- **Proved.** Desktop runs: only the refusal with no circle ("needs a circle
  on the land first"). Not run: a cast.
- [ ] Cast the shield, and throw a rock at it. The rock stops.

### Teleport ("Instant Journey")

- **Plain game.** You put two teleport sites on the land. Villagers then go
  from one to the other at once. A tip of the game says to put them at the
  Village Store and the Worship Site, a forest or the fields. The Creature
  can learn it.
- **How to see that it worked.** Two sites stand on the land, and villagers
  go in at one and come out at the other.
- **Cost.** 5000. No power up.
- **Gesture.** Spiral, then the house. At a place, two times.
- **In the mod.** Close the grip with the Hand over the place. Not confirmed:
  whether the second site needs a second cast.
- **Proved.** Not run (N6).
- [ ] Put the two sites. Villagers go in at one and come out at the other.

### Explosion ("Blast Extremis", the tooltip "Mega-Blast")

- **Plain game.** A large blast at the place that you aim at. The power ups
  make it larger. Only Land 5 gives it. The Creature can learn it.
- **How to see that it worked.** A blast at the place. Not confirmed: what
  the blast breaks.
- **Cost.** 16000. The power ups (inverse spiral, then spiral): 32000 and
  60000.
- **Gesture.** Spiral, then the star. At a place.
- **In the mod.** Close the grip with the Hand over the place.
- **Proved.** Desktop runs: the cast at a place.
- [ ] Cast the Explosion. The blast is at the place under the Hand.

### Flying Flock ("Sky Full", the tooltip "Winged Creatures")

- **Plain game.** A large flock of flying creatures that impresses people.
  The game gives it only as a reward. Not confirmed: whether a good god and
  an evil god get different birds.
- **How to see that it worked.** A flock flies from the hand.
- **Cost.** 12000. No power up.
- **Gesture.** Spiral, then the square spiral. A throw.
- **In the mod.** Close the grip, swing the hand, and open it.
- **Proved.** Desktop runs: the throw.
- [ ] Throw the flock at a village. The birds fly there.

### Ground Flock ("Beasts of Earth", the tooltip "Wolves")

- **Plain game.** A pack of dangerous wolves on the land. The game gives it
  only as a reward.
- **How to see that it worked.** Wolves run on the land where the throw goes.
- **Cost.** 14000. No power up.
- **Gesture.** Spiral, then the inverse square spiral. A throw.
- **In the mod.** Close the grip, swing the hand, and open it.
- **Proved.** Desktop runs: the throw.
- [ ] Throw the pack. The wolves come down where you throw.

### The Creature miracles

Each one acts on the target Creature, your own or a different one, for a
time. The narrator says when it "has worn off". Not confirmed: how long each
one lasts. Each is a cast on a Creature: close the grip with the gold halo
on the Creature. The gesture is the inverse spiral and then the shape below.

| Miracle | What it does to the target Creature | Cost | Gesture after the inverse spiral |
| --- | --- | --- | --- |
| Freeze | It freezes it | 8000 | The star |
| Shrink | It makes it small | 6000 | A fork |
| Enlarge | It makes it large | 7000 | A fork |
| Weaken | It makes it weak | 8000 | A fork |
| Strengthen | It makes it strong | 8000 | A fork |
| Invisibility | It makes it invisible | 9000 | The infinity |
| Compassion ("Loving") | It makes it friendly and loving | 6000 | The heart |
| Anger ("Aggressive") | It makes it furious | 6000 | The vertical line |
| Itchy ("Holy Flies") | It irritates it | 8000 | The reverse S |

- **Proved.** Desktop runs: Strengthen and Freeze on a Creature. Not run:
  the other seven (N6).
- [ ] Cast Freeze on your Creature. It stands frozen, and the narrator later
  says that the miracle has worn off.
- [ ] Cast Shrink, Enlarge, Invisibility, Weaken, Strengthen, Compassion,
  Anger and Itchy. Each one changes the Creature as the table says.
- [ ] Cast a Creature miracle at a Creature outside your influence. The halo
  is red, and the card says why.

**Miracles that a player does not cast.** The game has seven more Creature
miracles: Fat, Thin, Hungry, Frightened, Tired, Ill and Thirsty. Each has a
record of a seed and a line "Your Creature has learned how to cast ...".
They have no gesture, and their records of magic have the name NONE. So a
player cannot ask for one. Not confirmed: whether a Creature or a script can
cast one in v1.42.

**The Creature casts miracles too.** The Creature can learn each miracle of
the god above, and the game says when it learns one. A tip of the game says
that a Creature casts a miracle at once when you give it the miracle.
- [ ] Give a one-off miracle to your Creature. It casts the miracle. (Not run
  in the mod.)
