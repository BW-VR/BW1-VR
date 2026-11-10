# The God view (play mode 4)

The God view puts the land at the player's waist, in the manner of the VR
game "How to God". The player's hands are the Hand, one to one in place and
size, and the world moves and scales around the player so that the hands can
reach it. There is no reach gain and no Hand that grows. The reach comes from
the world (`hand.md`).

The God view is the main mode and the default. It works with motion
controllers and with the mouse. The launcher has no row for the play mode,
and a save of the launcher removes `play_mode` from `bwvr.ini`, so a player
always has the God view.

The God view is the camera of the tabletop mode (`tabletop.md`) with a
different room: the land is at the waist and there is no edge. The two modes
share the code, the table transform and the grab solve. `tabletop.md` has
that math. This file has everything that is specific to the God view.

## Play modes

| `play_mode` | View |
| --- | --- |
| 4 | The God view. The default |
| 3 | The tabletop: the island is a model on a table in the room (`tabletop.md`). For tests |
| 2 | A window that follows the head. For tests |
| 1 | The stereoscopic window: a 3D screen in front of the player. The head does not move the camera. For tests |
| 0 | Not a mode. A value of 0 in `bwvr.ini` gives the God view, and the mod logs it |

```
settings     : play_mode=0, the immersive mode, is now the God view, play mode 4
```

The immersive view (the world all around the player, with the game camera
moved by the head) stays in the code for two cases. A story shows in it while
the God view stands aside (section 7). Ctrl F11 turns the God view off and
on, and off is the immersive view. The kPlayModes table in `mod_main.cpp`
sets the flags of each mode.

## Summary

- **The map.** The land under the recenter pose sits `god_ground_down_cm`
  (60) below the head. There is no edge, no key color and no sea disc: the
  normal sky and sea go to the horizon.
- **The zoom** changes the scale about the land point that the player looks
  at (`god_zoom_about`, 1), so the thing in the middle of the view stays where
  it is and only its size changes. The ground keeps its height. It goes from
  1 unit per meter, where a villager is as high as the player, to 4000, in
  steps of 1.5 times.
  With `god_zoom_smooth=1` the stick zooms while it is held, with no steps.
- **The ground height** goes up and down with the left grip: the hand pulls
  the land up and down one to one. The mouse player has steps of 5 cm (the
  tilt keys). The view never follows the land up or down by itself.
- **The grab of the world** moves the land one to one with one hand, across
  and up and down, and two hands turn and scale it. Nothing moves after the
  release.
- **The walk and the fly with the stick.** The stick of the hand that does
  not point moves the player while it is pushed, along the direction of the
  head or of the controller (`move_stick`). In the walk (`move_type=0`) the
  player moves over the land, and the height of the view follows the land
  under the head through a smooth filter (`move_stick_follow_land`). In the
  fly (`move_type=1`) the player moves along that direction in 3D, also up
  and down, and the view does not follow the land. It stops when the stick
  comes back (section 5.7).
- **The turn.** The stick of the Hand to the side turns the view about the
  head: in one step of 30, 45 or 90 degrees, or with `snap_turn=4` smoothly
  while it is held, faster with a larger push (section 4).
- **Comfort.** `comfort_vignette` darkens the edges of the view during a move
  that the player makes with a control. While a dialog box is up the stick
  does not turn or zoom, and a hold of the stick click of the hand that does
  not point recenters the view (section 5.8).
- **The Hand** is always the real hand (`hand_real_cm`, 19 cm) at the scale
  now. Zoomed out, it is large against the villagers.
- **The game camera** stays over the player's place at 400 units, for the
  sound, the detail and the game's limits. It moves only when the player
  moves the view.
- **A story** shows as the game made it, at the life size scale. The God
  view stands aside and comes back where the story leaves the camera.
- **The mouse** drags the land, and the land point under the mouse stays
  under it. The wheel zooms, the tilt keys move the ground, and the turn keys
  and the middle mouse turn.
- **The menu.** The menu button of the left controller sends Esc, and a
  dialog box takes the pointing hand as a pointer (section 10).
- **The tutorial.** The zoom, the pan, the walk, the teleport and the turn
  keep to the game's camera switches, and they send the game's camera
  events. So a tutorial step that waits for a zoom or a drag can end. The
  pan, the walk and the teleport have one switch, so the player has all
  three from the first step of the tutorial.
- **A load** builds the view again at the camera of the loaded game.
- **The game moves its camera.** When the game puts its own camera at a new
  place (a new game, the end of a story, its jump to the Temple), the player
  goes there as one cut. The land point that the camera looks at is 50 cm in
  front of the head, and the player faces the way that the camera faces
  (section 15).
- **The teleport.** Hold A to aim an arc. A ring shows the target, cyan when it
  is valid and red when it is not. Let go of A to go there, as one cut. B
  cancels. A double click of the mouse goes to the land under the mouse
  (section 14).
- **The scripts see the player.** A script of the game that asks how far the
  camera is from a thing gets the distance from the head or the nearest
  hand. A script that asks if the camera sees a thing gets the view of the
  head. In a story the script gets the camera of the story (section 16).
- **The camera zone.** Where a script of Land 1 keeps the camera in an area,
  for example at the Creature select, the game camera stays in that area,
  so the game does not push it back or shake the view. The player turns and
  zooms freely and moves up to `god_zone_margin` (50) units past the edge.
  There the view stops, and the game plays its sound of the block (section
  17).

## 1. The map

The table maps the room to the world with one similarity (`tabletop.md`,
"The table camera"):

```
world = G + s * Lt * (room - Tg)       Tg = (0, -down, 0)
```

| Symbol | Meaning |
| --- | --- |
| `G` | the waist point: the world point that sits right under the recenter pose, `down` meters below it |
| `s` | the scale, in units per meter |
| `Lt` | the basis: world up, and the heading `psi` of the room forward |
| `Tg` | the waist point in the room, `god_ground_down_cm` below the recenter pose |

On the table `C` is at sea level. In the God view `G` has a height of its
own: it is the world height at the waist level. At the start it is the height
of the land under the recenter pose, so that land is exactly at the waist.

`TabletopCamera` gives `head_camera.cpp` the camera at `P0` (the recenter
pose in the world) and the lean gain `s / 7.5`, as on the table. The render
bracket, the eye separation of the player's IPD times `s`, the room of the
controllers (`TabletopRoom`), the spirits and the ears all work as on the
table, with the God view's `P0`, `Lt` and `s`.

The land under the head is 60 cm below the head only where it has the height
of the waist point. A hill that the player pans to rises toward the head, and
a valley falls away. That is the rule: the view does not rise and fall with
the land. The player moves the ground with the left grip.

## 2. The zoom

One zoom step multiplies the scale by 1.5 (`god_zoom_step_pct`, 50), or
divides it by 1.5. A zoom keeps one point of the world at its place in the
room:

```
W  = the point of the zoom
s' = s * ratio,  kept between the lowest scale and god_far_units_per_meter
G' = W + (s' / s) * (G - W)
```

`W` keeps its world point and its room point. A land point that is `d` from
`W` in the room is `d * s / s'` from it after the zoom, so the land comes
nearer to `W` when the view zooms out. The heading does not change. The game
camera stays over the land under the head (section 7), so the game logic, the
sound and the detail keep their distances to the player.

**The point of the zoom.** `god_zoom_about` chooses where `W` is across the
room. `GodZoomPoint` in `tabletop.cpp` is the one place that chooses the
point, for a step and for the smooth zoom. `PointAcross` in `zoom_math.h` has
the rule.

| `god_zoom_about` | The point | What the player sees |
| --- | --- | --- |
| 1, the default | The land point that the head looks at | The thing in the middle of the view stays where it is, and only its size changes |
| 0 | The land point straight under the head | The player keeps their place in the world. Each thing in front of the player comes 1.5 times nearer in a step out and goes 1.5 times away in a step in. That can look like a move forward or back |
| 2 | The Hand's point of the pointing hand | The land at the hand stays at the hand |

- **1, the look.** The line of the look starts at the middle of the head and
  goes along the forward direction of the head. The point is the first land
  on that line. The land test is the one of the mouse pan (`RayLand`), in
  steps of 1 cm of the room and of 2 units at least. The test stops
  `god_zoom_about_far_cm` (300) from the head. When the line meets no land
  before that (a look at the sky or at the horizon), the point is the point
  of the line at that distance. So the point is at most 3 m across from the
  head, and a look straight up gives the land under the head. The two cases
  meet at the limit, so a small turn of the head never makes a jump of the
  point. A head with no direction gives the land under the head.
- **2, the Hand.** The point is the Hand's point (`HandDirectRealPoint`) of
  the frame before. While no controller places the Hand (the mouse has the
  pointer, or a dialog box is up), the point is the land under the head.
- **The level.** The height of `W` is the same for each value: the height of
  the land under the head. Land of that height keeps its height in the room,
  so the ground stays where the player put it (section 3). Only the place of
  `W` across the room changes with the key. On a slope the land point that
  the player looks at is higher or lower than the level, and it goes up or
  down in a zoom by that height times the change of the scale.
- **The head moves over the land** with 1 and 2. In a step in, the land under
  the head goes away from `W` by half of its distance to `W`. In a step out it
  comes nearer by a third of that distance. A look at the horizon and a step
  in thus move the land 1.5 m under the player. The game camera follows the
  head (section 7).
- **A hill at the new place of the head.** When the land under the new place
  of the head is nearer than 10 cm below the head, all of the land goes down
  until it is 10 cm below (`ZoomClearHead`), as it does when the view goes to
  a place (section 15). The log line of the zoom says so.

**A hill over the head.** At the life size scale a hill of 100 units is 13 m
high. A pan can bring one over the head. So the level is first kept between
10 cm and 2.5 m below the head, the range of a ground step (section 3). With
`god_zoom_about=0`, each zoom step out then brings the hill top 1.5 times
nearer to that height. After 14 steps out at the life size scale, a hill top
12.73 m over the head is 5.6 cm under it.

**The limits.** The lowest scale is 1 unit per meter
(`god_near_units_per_meter_x10`, 10). A villager is about 1.6 units high: the
body that the hand pick finds has a half height of 0.70 to 0.85 units, and
the game holds a villager with a height of 1.66 units. So at 1 unit per meter
a villager is 1.6 m high in the room, as high as the player, and the eyes of
a player who stands with the land at the feet are at the eyes of a villager.
A value of 75 stops the zoom at the life size scale (7.5 units per meter,
`head_units_per_meter_x10`), and 0 takes that scale from
`head_units_per_meter_x10`. At 4000 an arm of 70 cm reaches 2800 units, which
is inside the Hand's reach of 5600 (section 8). `god_far_units_per_meter` can
go up to 8000. A step at a limit does nothing and is logged. From the start
scale of 100, 12 steps in go to 1 unit per meter. From 1 to 4000 is 21
steps, and 5 steps out from 1 go past the life size scale, to 7.6.

**Below the life size scale.** Each distance of the room stays the same as at
the other scales: the Hand is 19 cm long (size 0.06), the near plane is 5 cm
(0.05 units, section 8), the walk goes 150 cm a second (1.5 units), the reach
of the teleport arc is 10 units. The distance factor of the ears is 0.25,
so a sound with the maximum 300 carries 75 m of the room (`audio.md`, 6.3). The values of section 8 that follow the scale stay the
game's own below a gain of 1. The land under the head keeps the height that
the player gave it. A zoom in does not lower it: the player pulls the land
down with the grip of the other hand, to at most 2.5 m below the head
(section 3). At 1 unit per meter with the land 60 cm below the head, a
villager stands 1 m over the eyes. With `god_zoom_about` 1 and 2 a zoom
moves the head over the land, and the land under the new place of the head
can be higher or lower than the level of the zoom. At a small scale a small
change of the land is a large change in the room: 0.5 units are 50 cm at 1
unit per meter. So after a zoom in about a point that is not under the head,
the land under the head can be 10 cm below it (`ZoomClearHead`) or far
below it. With the land 1.4 to 1.6 m below the head,
the player sees the village from the eyes of a villager.

**Houses at a small scale.** The player walks among the houses there, so two
rules keep a house from covering the view of a head that is not in it:

- **The walls** (`solid_house_inset_pct`, 20). The box of the mesh of a house
  holds the eaves of its roof, which go past its walls. Below the life size
  scale the box of a house is 20 percent smaller across, in each level
  direction, and it keeps its height (`solid_math.h`, `InsetLevel`). A house
  of Land 1 has a half width of about 3.4 units, so the box comes about
  0.7 units (70 cm at 1 unit per meter) nearer to the middle on each side. A
  head under the eaves is then out of the house, and a head that is deep in
  the house is still in it: the head blinder covers the view there, and the
  drawn hand stops at the smaller box. A rock and a tree trunk keep their
  shape. At the life size scale and above a house keeps its whole box,
  because the Hand there touches its roof from above. 0 keeps the whole box
  at each scale.
- **The zoom stops at a house** (`god_zoom_solid_stop`, 1). A zoom step
  brings the head nearer to the land, and with `god_zoom_about` 1 and 2 it
  also moves the head over the land. So near the lowest scale a step can put
  the head into a house. A step, and each frame of the smooth zoom, stops
  where the head is 30 cm of the room out of the nearest house, rock or
  trunk (`god_zoom_solid_cm`, `StepShare` in `zoom_math.h`). That leaves
  room for the Hand between the eyes and the wall, and the eyes see the
  wall with the land beside it. The overlay of the head blinder starts at
  5 cm, and a value of 5 stops the step there. A step that cannot go at all
  is refused. Only a step in stops: a step out makes the world smaller about
  the point of the zoom and lifts the head over the land, so it always goes,
  and the player can zoom out of each place. A head that is already nearer than the limit may zoom away
  from the solid, but not nearer. The eaves of the roof can be nearer than
  the limit, because the box of a house below the life size scale is its
  walls. The
  solids are the ones near the hands and the head of the frame before.
  Below twice the life size scale (15 units per meter) the head wants them
  from 1.5 m of the room, so a house that the next step brings to the head
  is in the list. 0 lets a zoom step put the head into a house.

The head blinder still covers the view when the player walks or leans into
the walls of a house (section 5.9). The land of the game has cells of 10 units, which are 10 m at
1 unit per meter, so the land looks flat and its texture is soft there.

Below twice the life size scale (15 units per meter) the zoom check of each
step has one more line: the height of the eye over the land, the near plane,
the fog of the game and the overlay of the head blinder after the step. A
zoom step that a solid stops logs one line, and a hold of the smooth zoom
logs one line for each hold:

```
god view     : the zoom step stops short at 1.93 units per meter, not 1.73: the whole step puts your head 38.8 cm into the Abode 13AF65F0, so your head stays 30 cm out of it (god_zoom_solid_stop=1)
god view     : the zoom step is refused at 1.93 units per meter: it puts your head 21.0 cm into the Abode 13AF65F0 (god_zoom_solid_stop=1)
```

A zoom in that comes to the lowest scale logs one line, and it comes again
only after the scale went above it:

```
god view     : the zoom is at its lowest scale, 1.00 units per meter (god_near_units_per_meter_x10=10). A villager of 1.6 units is 1.60 m high in the room, and the land under your head is 0.60 m below it
god view     : zoom check, the scene at 1.00 units per meter: the eye is 0.60 units over the land, the near plane is 0.050 units (5.0 cm of the room), the fog of the game is on from 400 to 900 units, and the head blinder covers 0% (your head is 60.0 cm from the land)
```

**The inputs.** The right stick forward zooms out, and back zooms in
(`god_zoom_forward_in=0`, the checkbox "Invert zoom direction" of the
launcher is off). With 1 the stick
forward zooms in. The desktop test scripts set 1, so the word `stick_up` of a
recipe zooms in. With
`god_zoom_smooth=0`, the default, one push past 70 percent is one step. The
stick must come back below 30 percent before the next push counts. The mouse
wheel also steps the God view, one step a click, at most one each 150 ms, and
the game hears no wheel. The wheel zooms in steps with each value of
`god_zoom_smooth`.

A zoom step needs the game's camera switch for the zoom (section 7, "The
tutorial"). While the switch is off, a push or a click does nothing, the
controller gives no pulse, and the log says why. A step that the view takes
sends the game's zoom event, 0x304.

The log has two lines for a step, and one more for a check in the next
frame. They give the point of the zoom and why it is that point, how far the
point moved in the room, and how far the land under the head moved. The
lines of a zoom step and of a smooth zoom hold have their own count, 150
lines for a session (`kMaxZoomLogs`). The lines of a turn, a ground step, a
pan of the mouse and a zoom that the game does not allow share 60 lines
(`kMaxGodLogs`).

```
god view     : zoomed in, 100.0 to 66.7 units per meter, about the point at 2052.0 37.6 2489.7, 0.44 m below your head: the land that you look at, 0.77 m across from your head (god_zoom_about=1)
god view     : zoom check, the point of the zoom moved 0.00 cm in the room. The land under your head was at 1977.5 2468.3 and is at 2002.3 2475.4, 38.75 cm of the room apart. The recenter place moves 53.73 cm of the room, and the head is 30 cm right and 40 cm forward of it
god view     : zoom check, the drawn head moved 38.75 cm of the room over the land in the step (1977.5 2468.3 before, 2002.3 2475.4 after). The point of the zoom moved 0.00 cm from the drawn head in the room.
```

**The smooth zoom.** With `god_zoom_smooth=1` the stick changes the scale
while it is held, and the zoom stops in the frame in which the stick comes
back. Nothing glides after that, and a stick at rest moves nothing. The ratio
of the scale for one frame is

```
share = (|y| - 0.3) / (0.95 - 0.3), kept between 0 and 1
ratio = 1.5 ^ (-direction * share * dt / T)
```

`y` is the stick forward or back, and `direction` is 1 for forward. `dt` is
the real time of the frame, and at most 50 ms, so a frame that took long makes
no jump. `T` is `god_zoom_smooth_ms`, 500: a full push changes the scale by
one zoom step (`god_zoom_step_pct`) in that time, and a small push is slow.
The change is a ratio, so the zoom feels the same at each scale, and it
follows the time and not the frame rate. `zoom_math.h` holds these functions.

- **The point** is the point of a zoom step (`god_zoom_about`), with the
  rule for a hill over the head. With 1 and 2 the hold takes the point one
  time, at its start, and keeps that point of the world. So the point does
  not move while the head turns a little. With 0 each frame takes the land
  under the head again. The rule for a hill at the new place of the head acts
  on each frame.
- **The limits** are the limits of a step. At a limit the scale stops, the
  stick still counts as held, and the other direction zooms at once.
- **Once a frame.** The pump of the controllers asks the view again before
  each frame (`TabletopGodZoomHold`), and the main camera call changes the
  scale one time (`ApplyGodZoomHold`). A frame with no request changes
  nothing.
- **Which axis.** The stick zooms while it is past 30 percent forward or back
  and that axis is larger than the side axis. A snap turn needs the side axis
  past 70 percent and larger than the other one, as with the steps. So one
  frame never zooms and turns. A hold starts with a fresh push only: a hold
  that stopped because the stick went to the side, and a push that the view
  or a grab refused, must come back below 30 percent first.
- **The game's switch.** Where a zoom step is refused, the smooth zoom is
  refused too: a push with the zoom switch off does nothing, gives no pulse,
  is counted one time and must come back. A switch that goes off during a
  hold stops the hold. A grab of the world refuses the stick in the same way
  as for a step.
- **The zoom event.** Each frame that changes the scale sends 0x304 with the
  input bit of the wheel. The game does the same: it sends 0x304 on each
  update in which its own camera zooms (the test at `0x0045C38E`), and
  `CameraHelpAccumulator::Trigger` `0x00449040` drops a trigger while its
  flag at `+0x0A` is set. So a tutorial step that waits for a zoom hears the
  first frame of a hold. A frame at a limit sends no event.
- **The game camera.** With `god_zoom_about=0` the point of the zoom is the
  place of the game camera, so a hold does not move that place. The version
  of the view goes up on the first frame of a hold that changes the scale and
  one more time at its end, so `GodAfter` writes the game camera at most two
  times for a hold (section 7). With 1 and 2 the head moves over the land in
  each frame. The version then goes up on each frame that changes the scale,
  as for a grab, so the game camera follows the head.
- **No blink and no cue.** The blink of a step and the sound of a zoom step
  (`audio.md`) do not come with the smooth zoom. The controller gives one
  pulse at the start of a hold.

With `test_keys=1`, Ctrl, Shift and Insert change `god_zoom_smooth` between 0
and 1 while the game runs. The pointing controller gives a long pulse for 1
and a short pulse for 0. A stick that is pushed at that moment must come back
before it zooms. The test `god_zoom_test_toggle=N` does the same at frame `N`
of the controllers, for a desktop run.

With `test_keys=1`, Ctrl, Shift and Delete change `god_zoom_about` from 1 to
0, to 2 and back to 1 while the game runs. The pointing controller gives a
short pulse for 0, a medium pulse for 1 and a long pulse for 2. A hold of the
smooth zoom that runs stops, and a stick that is still pushed starts a new
hold with the new point. The test `god_zoom_about_test=N` does the same at
frame `N` of the controllers, for a desktop run.

The log has one line for the start of a hold, one for its end, and one for a
check 30 frames later:

```
god view     : the smooth zoom starts to zoom in at 100.00 units per meter, about the point at 1996.0 29.5 2520.3, 0.60 m below your head: the land that you look at, 1.10 m across from your head (god_zoom_about=1). The hold keeps this point of the world. A full push changes the scale 1.50 times in 500 ms.
god view     : the smooth zoom stopped (the stick asks for no zoom now). It zoomed in from 100.00 to 54.75 units per meter, which is 1.49 zoom steps. The stick was held for 0.743 s, and the scale changed on 45 of 45 frames. The point of the zoom at 1996.0 29.5 2520.3 moved 0.00 cm in the room. The land under your head was at 1890.4 2490.0 and is at 1938.2 2503.7, 90.79 cm of the room apart, and the drawn head moved 90.79 cm of the room.
god view     : smooth zoom check, 30 frames after the stop the scale is 54.75 units per meter, and it was 54.75 at the stop. The drawn head moved 0.00 cm of the room in those frames.
```

## 3. The ground height

The left grip moves the ground up and down (section 5). With the grip closed,
a hand that moves up raises the land by the same distance, and a hand that
moves down lowers it. A raise is `G.y -= s * d`, where `d` is the rise of the
hand in meters: every land point goes `d` up in the room, and the scale and
the heading stay. The left stick does not move the ground: it moves the
player over the land (section 5.7).

With the mouse, the game's two tilt keys move the ground in steps of 5 cm
(`god_ground_step_cm`), one step for each press (`GodTiltKeys` in
`comfort_camera.cpp`). The key that sets a tilt of more than 0 at
`0x00C5B0C4` raises the ground, and the other key lowers it. In the God view
the game camera does not tilt. A tilt key is refused while a grip holds the
world.

**The limits.** The land under the head stays between 10 cm and 2.5 m below
the head. A tilt step that would put it nearer or farther is refused and
logged. A step toward the range is always allowed. A grab stops the land at
the limit and does not refuse it: `GodGroundHeight` in `table_math.h` cuts the
height that the hand asks for to the range. The land under the head is found
in the pose that the solve made, so a pan over a hill is judged where the head
will be.

- At a limit the land stops, and it does not jump. The result is the request
  cut to a range, so it is continuous in the hand. When the hand comes back
  into the range, the land follows it again from the limit.
- A land that is already outside the range (a hill under the head) can only
  move toward the range. The cut never takes the height farther out than it
  is now.
- A head that moves while the land is held at a limit moves the limit with it.
  With the land inside the range, a head that moves and a hand that stays
  still move nothing.

The ground height belongs to the player. The view keeps it through a zoom, a
pan, a turn and a story. A recenter (F11) moves the room, so the waist point
goes under the new head position.

## 4. The turn

The launcher row "Turn" (`snap_turn`) has four values: Smooth (4), and 30,
45 and 90 degrees (1 to 3). The default is 45 degrees. The mod also takes
`snap_turn=0` (no turn) from bwvr.ini, for a desktop test. The launcher and
the settings window do not offer it: for 0 they show the default, and a save
of the launcher writes the default.

**The snap turn** (1 to 3). The right stick left or right turns the view by
the step. The turn keys and the middle mouse do the same. The request goes
through `comfort_camera.cpp`, so the tutorial's turn switch and `MayTurn`
still apply, and the tutorial still hears the turn. The view turns about the
head: the world point of the head stays, and the heading changes by the
step. A 90 degree turn right puts the land that was on the right in front.

**The smooth turn** (4). While the right stick is held to the side, the view
turns about the head, as for a snap turn, by a small angle in each frame
(`ApplyGodTurnHold` in `tabletop.cpp`). The speed follows the push:

- Up to 0.3 of the travel of the stick (the dead zone of the smooth zoom),
  the view does not turn.
- From 0.3 to 0.95, the part t of that travel gives the share t (1 + t) / 2
  of the full speed (`comfort_math.h`, `SmoothTurnShare`). Half that travel
  gives 3/8 of the speed, and a push of the stick to 0.5 gives about a fifth.
  The curve starts at half the slope of a straight line, so a small push is
  a slow turn that is easy to hold.
- From 0.95 on, the view turns at `smooth_turn_speed` (90 degrees a second
  by default).

The default speed is a quarter turn a second. It is also the full turn speed
of the comfort vignette, so a full push gives the full darkness of the
vignette and a small push a light edge. The turn of a frame is the share
times the speed times the time of the frame, at most 0.05 s, so a long frame
does not make a jump.

The smooth turn, the walk with the stick and the smooth zoom take the time
of a frame by one rule, latched once for each frame at the main camera call:

- With a session of a headset and `god_display_time=1`, the time is the gap
  between the display times of two frames of the headset (section 5.7, "The
  time of a frame").
- With no session (a desktop run has `stereo_headless=1` and no session),
  or with `god_display_time=0`, the time is the time of the CPU between two
  main camera calls.

The stop line of a hold gives the time of its frames by that rule and by the
clock of the CPU. A desktop run with `test_frame_rate=60` has the paced
frames of 1/60 s only from frame 150 of the controllers. Before that frame
a desktop frame is shorter, so a hold of 60 frames there is shorter than
1 s, and it turns less than the speed for 1 s.

The turn stops in the frame in which the stick comes back into the dead
zone: a frame with no request turns nothing, and nothing glides. The larger
axis of the stick wins, so one frame never zooms and turns. A push that the
game refused (the turn switch of the tutorial, a fight, a drag of the Hand),
a grab of the world, a dialog box, and a hold that stopped because the stick
went forward or back, must come back into the dead zone before the stick
turns again. A held turn key turns the view at the full speed. A drag of the
middle mouse turns in steps of 45 degrees, and so do a view that is not the
God view and a room of the Temple.

The smooth turn writes as a snap turn writes. A turn alone turns the game
camera about its own focus (`god_turn_keeps_focus`), and the version of the
view goes up in each frame that turns, so the game camera follows. For the
tutorial, each `turn_help_degrees` of the turn (5) is one turn event of the
game's help, one in each game turn (45 degrees with `turn_help_degrees=0`).
The settings window changes the turn and its speed at once.

With the turn off (`snap_turn=0`), there is no turn in the God view: the
stick does not turn, and the game's own smooth turn of the turn keys and the
middle mouse turns only the game camera, not the view. A desktop run with
`snap_turn=0` shows the same picture before and after a push of the stick
and a hold of the turn key.

While a dialog box is up, the stick does not turn (`menu_stops_turn`,
section 5.8). A snap turn puts the comfort vignette at its full darkness at
once, and the smooth turn darkens the edges by its speed (`comfort_vignette`,
section 5.8).

The log of the smooth turn:

```
god view     : the smooth turn starts to the right, 100% of a full push, at 90 degrees a second for a full push (smooth_turn_speed), heading 56.14 degrees, frame 221
god view     : smooth turn frame 1 of the hold: 1.493 degrees in 0.0166 s at 100% of a full push, heading 57.634 degrees, frame 221
god view     : the smooth turn stops, the stick asks for no turn now. It turned 92.92 degrees right in 1.032 s of 60 frames (90.0 degrees a second), heading 56.14 to 149.06 degrees. The clock of the CPU gave 1.032 s for these frames, frame 281
god view     : turn check, 30 frames after the stop the heading is 149.061 degrees, and it was 149.061 degrees at the stop
```

The lines are from a desktop run with a full push of 1 s at 60 frames a
second. The log has one line for each frame of the first 4 holds, at most
240 frames each. The report of a window counts the requests that the God view took,
the ones that `MayTurn` and a grab refused, and the turn events that the
smooth turn owed to the help.

## 5. Moving around: the grab and the mouse

### 5.1 The grab of the land

The grips move the land. The solve (`TableGrabSolve`, in `tabletop.md`, "Moving
the table by the grips") is the same as on the table, with the vertical grab
and the ground limits added.

- One hand (the other hand's grip, the left grip by default): the land moves
  one to one with the hand, across and up and down. A hand that moves up
  raises the land by the same distance, and a hand that moves down lowers it
  (`TableGrabSolve` with `vertical`, then `GodGroundHeight` for the limits of
  section 3). At a limit the land stops and does not jump. The height of the
  waist point is the height of the press plus the rise of the hand.
- Both grips: the land turns about up with the hands and scales with their
  spread, about the land point under the middle of the hands. The height of
  the press stays, so two hands move the land only across.
- The solve is from the press every time, so a head that moves with still
  hands moves nothing, and nothing moves after the release.
- The scale stays between the lowest scale (section 2) and
  `god_far_units_per_meter`. The mouse pan
  never lifts or drops the view: its hand is on the level plane of the press.
- The game's camera switches hold for the grab (section 7, "The tutorial").
  While the switch for the drag is off, the grab moves nothing, and it
  anchors again on each frame, so the land does not jump when the switch
  comes on. A two hand grab turns only while the turn is allowed, and zooms
  only while the zoom is allowed. Otherwise the turn or the zoom keeps its
  last value, so nothing jumps. A move of the grab sends the game's events:
  0x308 for a pan, 0x300 and the direction for a turn, and 0x304 for a zoom.

The sticks are refused while a grip holds the world.

The code is in `world_grab.cpp` (the state machine, `TableAnchor`,
`TableSolve`) and `controllers.cpp` (the grip points and the buttons).

### 5.2 The buttons of the grab

| Input | During no grab | During a grab |
| --- | --- | --- |
| Other hand's grip | Starts a grab on a press above `grab_press_pct` | Holds it. Below `grab_release_pct` the hand lets go |
| Pointing grip | The right button (pick up) | Joins the grab when the claim rule holds. Otherwise it stays the right button |
| Pointing trigger | The left button | Held back from the game. It must open before it presses again |
| Any new left press | Goes to the game | Held back, the same way |
| Stick | Snap turn and zoom steps | Refused and counted. A refused push must come back before it acts |
| Stick of the other hand | The walk with the stick (5.7) | Refused, with one log line. The push must come back before it walks |
| A button | The teleport | Refused |
| Mouse wheel, turn keys, middle mouse | As usual | Refused and counted in `comfort_camera.cpp` |

The other hand is the main way in, for four reasons:

1. It cannot take pick up from the pointing grip.
2. It works while the pointing hand holds an object. The player can carry a
   villager and pull the world.
3. It never asks whether the Hand is over land.
4. It needs no change to what the game sees. No game button is sent for it.

A grab starts only on a press, never on a grip that is already closed. A
grip that closes while the trigger is down, or while the game holds the left
button, does not grab. The next grab needs a new press after the trigger is
up. A trigger that is already down at the press of a grip blocks the grab
until it is up. The grab needs the controller to own the pointer, a scene (no
menu), and the player's camera mode with its input on
(`WorldGrabCameraReady`). When one of them goes away, the hands let go of the
world.

**The claim rule** for the pointing grip (`grab_hand=2`). The mod claims the
press, and the game gets no right button, only when all of these hold at the
press frame: the controller places the Hand, the Hand is in the Normal state
(`hand+0x4878` is 1) and holds nothing (`hand+0x4904` is 0), the mod's pick has no choice, the game has no object under
the Hand (`+0x3C8`), no game button is held, and the trigger is up. The rule
is decided at the press and kept until the release. With `grab_hand=1`, the
pointing grip joins only when the other grip holds first.

The pump never sends the game's both button mode (`0x00C5B0B0`) for a grab,
because the trigger is held back. `controller_swap_buttons` exchanges the
trigger and the grip of the pointing hand.

`khr/simple_controller` has no squeeze, so there the other hand's select (its
trigger) grabs. A left handed player (`left_hand`, `controllers.md`, 5.6)
has the mirror: the left hand points and the right hand grabs.

| Profile | Grip source |
| --- | --- |
| `oculus/touch_controller` (Quest) | `squeeze/value`, analog. Press above 70 percent, release below 50 |
| `valve/index_controller` | `squeeze/value`. A loose hand can rest at 30 to 50 percent [I] |
| `htc/vive_controller`, `microsoft/motion_controller` | `squeeze/click`, a boolean. The levels do not matter |

### 5.3 The mouse pan

A land drag of the mouse pans the view (`MousePan` in `tabletop.cpp`). At the
press, the ray of the pointer from the head finds the land, in steps of 2
units, up to the Hand's reach of 5600 units (`RayLand` in `table_math.h`).
That land point is the grabbed point, and its height in the room sets a level
plane. On each frame the ray meets that plane (`RayLevel`), and the pan of one
hand of the grab (`TableGrabSolve`) puts the grabbed point there. So the land
point under the mouse at the press stays under the mouse. It also stays under
the Hand, which the game puts on the same ray.

- The ray is the exact pointer through the head of the frame, `H * ray`
  (`PointerMapMouseRay` in `pointer_map.cpp`). The pointer is locked to the
  room, so the ray turns only when the mouse moves. The ray starts at the
  head of the press, so a head that moves with a still mouse moves no land.
- The pan follows the game's own land drag: the left button is down, and the
  hold status of the camera mode is 3 (`+0x8C` and `+0x1AC`). So a click on
  a villager picks it up as usual, and a drag starts only where the game
  starts one. The drag smoothing of 35 ms (`backend.md`) still eases the
  pointer. The land moves only on a frame of a land drag, so nothing moves
  after the release.
- The game's drag code still runs. So the game knows the drag, and the
  tutorial hears it (event 0x308). After each update with a land drag,
  `comfort_camera.cpp` puts the game camera back where it was before the
  update (`GodDragAfter`). So the drag does not move the game camera, which
  the God view does not show. `GodAfter` then puts the game camera over the
  player's place, as for each move of the view. This holds for a drag of the
  trigger too: it moves nothing, and the grip moves the land.
- A ray that meets the plane past the Hand's reach, or goes over it, holds
  the land where it is until the ray comes back.
- A drag with no land under the ray at the press, a drag of a controller, a
  drag while a grip holds the world, and a drag that the game's switch does
  not allow pan nothing until the release.
- A zoom or a turn during the drag anchors the pan again, with no move.

A desktop run with the test mouse (`hand_run.ps1 -Kind mousepan -Save
temple`, no controller) shows the pan: the mouse takes the land 1.31 m from
the head, the grabbed land stays within 0.000 cm of the mouse ray, and the
view moves only on the frames in which the mouse moves.

### 5.4 What the game's land drag does

This is the game's own drag with the mouse or with the trigger of the
controller. The mod keeps it, and the grab above does not use it.

1. The trigger is the left button. `controllers.cpp` `PumpButtons` sends it
   through `LHMouse::SetButtons`.
2. `CameraModeNew3::Update` starts a land drag when the left button goes down
   on land. `UpdateClickParams` `0x00459610` stores the grab point, the drag
   plane (mode `+0x14C`, `+0x158`) and the grab camera.
3. Each frame the camera code casts two rays through
   `UnprojectScreenPoint` `0x0081B370`: the press pointer at `0x0045D444` and
   the pointer now at `0x0045D456`. It cuts both rays with the plane and
   moves the camera by the difference (`0x0045D365` to `0x0045D96E`).
4. The mod gives both rays the Hand's world direction. Only the direction
   changes. The origin stays the game's own camera (`h_Unproject`,
   `pointer_map.cpp`).
5. The step is capped by the pixel distance from the press times the view
   distance times 0.011. The mod sets that distance to the angle the Hand
   turned since the press (`KeepStepLimit`).
6. The drag smoothing of 35 ms eases the mouse motion into the Hand
   (`DragShare`, `TakePending`). At the release the rest arrives at once, so
   nothing moves after the release. The glide time `0x009CE614` is 0.

The game's drag keeps the grabbed land on a ray from the game camera through
the Hand's direction. The land moves by an angle times the range, not by the
hand's motion. At the life size scale the world moves 10 to 130 times as far
as the hand, and it ignores a pull of the hand toward the eye. So it is not a
grab, and a true grab (5.1 and 5.5) is the tool for a large move.

### 5.5 The grab on the game camera

When the God view and the table are off (Ctrl F11), the same grips move the
game camera and not the table. This is the grab of the immersive view. It
needs no hook. All the writes go through code that exists.

**The rule.** While a grab holds, one point of the world stays attached to the
hand, and no other motion is made. A still hand gives a still camera,
whatever the head does. The release stops the camera at once, with no glide.

**The solve.** The grip points are in the room, in units. The virtual hand
point is `v = r0 + G * (a - r0)`, with `r0` the head center at the press and
`G = diag(g, g_v, g)`.

- `g = 1` is true 1:1.
- With `grab_gain_mode=1`, `g = Dcam / dhand * grab_gain_pct / 100`, kept
  between 1 and `grab_gain_max` and fixed for the whole grab, so the land
  looks glued to the hand. `Dcam` is the distance between the GCamera Zoomer destinations for
  position and focus. `dhand` is the mean distance of the held grips from the
  head center, kept between 0.25 and 0.70 m. A new anchor measures both
  again.
- `g_v = g * grab_vertical_pct / 100`.

One hand: `C(t) = C_a - L_a * (v(t) - v_a)`, and the focus follows by the same
offset. The solve is absolute from the press, so a second call in one frame
gives the same answer, and rounding cannot drift.

Two hands: with `m` the midpoint, `k` the spread ratio and `theta` the change
of the yaw of the line between the hands, the world turns with the hands
about up (`L(t) = Rot(up, -theta) * L_a`), and the camera moves toward the
midpoint by the zoom (`C(t) = A + (C_r - A) / k`). The zoom is a dolly, not a
change of the world's size. Below 8 cm of real spread, or when the turn is not
allowed (`MayTurn`), `k` and `theta` hold their last values. `k` stays
between 0.25 and 4.

**Where it writes.** `ComfortCameraGrabMove` runs in `h_ModeUpdate`, after
`RunGameUpdate` and `FinishFlight`, where the snap turn writes. It writes the
Zoomers of position (`+0x118`) and focus (`+0x88`) with `Zoomer::SetPosition`
`0x00441AC0`, which also zeroes the speeds. It writes the mode's copies at
`+0xC`, `+0x18` and `+0x1D4`, the pivot at `+0x12C`, and for a turn the
heading at `+0x50` by the change of `GetHeadingAndPitchFromPoints`
`0x004428D0`. A flight in progress is finished first (`FinishFlight` and the flags at
`+0x100` and `+0x101`). The mode fields `+0x78` and `+0x50` also have to stay
in step, or the next zoom or turn pulls the camera back. The game camera is the
sink because the sound, the picking, the Hand's ray, the save game and the
3500 unit pull back then all see one camera.

**Smoothing.** The grip points ease in by `1 - exp(-dt / grab_smoothing_ms)`,
once for each new sample. The solve stays absolute from the eased points. At
the release the rest is dropped, and the camera stays where it is. The grab
never starts the game's glide.

**Anchors.** A new anchor starts at a press, when the hands change, after a
grip jump of 50 cm, at a recenter, and from the camera that the grab wrote
last. Another move of the camera (a script, a fight, input off) ends the grab.
The grip must open and close again. A jump in flight makes the grab wait, and
it anchors where the jump lands.

**The limits.**

| Limit | Value |
| --- | --- |
| Pull back | `GCamera::Update` pulls any camera more than 3500 from (2560, 0, 2560) back over 3 s. The height counts. Test against 1.225e7 at `0x00442288`, code `0x0044224A` to `0x0044232F` [V] |
| Grab radius | `grab_limit`, 3400. It applies to the camera position and to the focus |
| Height above land | `grab_min_height`, 6 units above `GetAltitude` `0x00803090` at the camera x and z |
| Start outside a limit | The wall for a grab is the larger of `grab_limit` and the start distances of the camera and its focus. The floor is lowered by how far the start was below it. So a press never moves the camera |

At a limit the camera stops at the wall. The hand keeps moving, and the
world slides through it. The solve is absolute, so there is no dead zone. A
short pulse marks the wall. The focus and pivot follow the position by the
same offset.

**What the game logic sees.** The sound listener moves with the game camera.
`GCamera::IsMoving` (`+0x74`) is true while the camera changes, so the
interface clears the gesture trail and a miracle stroke cannot run during a
grab. A story mode is never touched, because the grab needs `CameraModeNew3`
on top and `+0x2EC == 0`.

### 5.6 Keys of the grab

All in `[bwvr]`.

| Key | Default | Meaning |
| --- | --- | --- |
| `grab_world` | 1 | Master switch. 0 gives no grab |
| `grab_hand` | 1 | 0 off, 1 the other hand's grip, 2 also the pointing grip alone. The launcher row "Grab the world" |
| `grab_two_hands` | 1 | Both grips turn and zoom |
| `grab_turn` | 1 | The two hand turn |
| `grab_zoom` | 1 | The two hand zoom |
| `grab_smoothing_ms` | 35 | The ease of the grip points. 0 is no smoothing |
| `grab_press_pct` | 70 | The squeeze that starts a grab |
| `grab_release_pct` | 50 | The squeeze that ends it |
| `grab_gain_mode` | 1 | Game camera grab only. 1 the land looks glued to the hand, 0 one to one |
| `grab_gain_pct` | 100 | Game camera grab only. Multiplies `g` |
| `grab_gain_max` | 60 | Game camera grab only. The most `g` can be |
| `grab_vertical_pct` | 100 | Game camera grab only. The gain up and down, as a share of `g` |
| `grab_max_speed` | 0 | Game camera grab only. Units a second. 0 is no cap |
| `grab_limit` | 3400 | Game camera grab only. Units from (2560, 0, 2560) |
| `grab_min_height` | 6 | Game camera grab only. Units above the land |
| `grab_probe` | 0 | A test. Each land drag logs the motion of the pointing grip, the head and the camera (tag `drag probe`). The launcher removes it |

With `test_keys=1`, Shift F11 cycles the grab: as set, one to one, and off.
The log names the mode. Press it with the mouse at rest.

Test aids: `fake_controller_left_points` (right, up, forward in cm, not
mirrored), the script words `lpoint`, `lgrip`, `lean` and `look`, and
`fake_head_right_cm`, `fake_head_up_cm` and `fake_head_forward_cm`. `lean`
puts the fake head at that lean while the word holds. Without a `lean` word
the lean holds for the whole run. `look` turns the fake head 30 degrees left
and 20 degrees down while it holds. All are steps. The launcher removes these
keys. After a run, the report has one line, `the held camera moved X units a
frame on average, Y at most`. A real hand is never quite still, so in the
headset this line shows the tracking noise times the gain.

`hand_run.ps1 -Kind holdland` holds the grip on the land with still hands:
the held camera moves 0.0000 units on each of 249 frames. A step of the hand
of 10 cm up in a second grip moves the ground 10.00 cm up in the room, and
the still frames after it move nothing.

### 5.7 The walk with the stick

The stick of the hand that does not point (the left stick, by default) moves
the player over the land while it is pushed. The move is the pan of a grab of
the world (5.1): the waist point goes to the land that was one step from it
in the room, and the heading and the scale stay. With
`move_stick_follow_land=1` the height of the view follows the land under the
head (see "The land under the head" below). With 0 the height of the ground
stays too. With `move_type=1` the stick flies in place of the walk (see "The
fly" below).

```
G' = G + s * Lt * (step.x, 0, step.z)      step = speed * the time of the frame
G'.y = G.y + (F' - F)                      F = the filtered land under the head
```

The plain game has a move of its camera over the land by the keys. Its code
tests bit 8 of the camera switches (`0x0045C946`) and sends event 0x308
(`0x0045C981`) [V]. The walk keeps to the same switch and sends the same
event.

- **Forward** (`move_stick`). With 1 it is the forward direction of the head,
  flat on the land. It comes from the right axis of the head (`HeadYaw`), so
  a look down at the land does not change it. With 2 it is the direction in
  which the controller of the stick points (its aim ray, `controllers.md`,
  section 8), flat on the land. The mod takes the direction again in each
  frame, so a turn of the head or of the hand turns the walk. With 0 the
  stick does not move the player. The launcher and the settings window do
  not offer 0: for 0 they show the default (1), and a save of the launcher
  writes 1. The mod takes 0 from bwvr.ini for a desktop test.
- **A controller that points up or down.** A controller that points less than
  11.5 degrees from straight up or from straight down has no direction on
  the land. The walk then keeps the direction of the frame before, and a walk
  with no such frame does not move.
- **The stick.** Forward on the stick moves the player forward, back moves
  back, and left and right move to the side. The push has the dead zone and
  the curve of the smooth zoom along its length (`Push` in `walk_math.h`): no
  move below 30 percent, the top speed from 95 percent, and a straight line
  between the two. A diagonal is not faster than a straight push.
- **The speed** is a speed of the room: `move_stick_speed_cm` (150) cm of the
  room for each second at a full push. So the walk feels the same at each
  scale. At 100 units for each meter that is 150 units a second over the
  land, and at the life size scale it is 11 units a second. The step of a
  frame is the speed times the real time of the frame, and a frame counts at
  most 50 ms.
- **The speed ramp** (`move_stick_ramp`, 1). While the stick is pushed to 90
  percent of the top speed or more, the time of that push counts. After 1 s
  the speed doubles each 2 s, up to `move_stick_ramp_most` (32) times
  (`RampFactor` in `walk_math.h`):

  ```
  factor = 2 ^ ((t - 1 s) / 2 s), at most 32      t = the time of the full push
  ```

  A push under 90 percent sets the time to 0, so the speed goes back to the
  speed of the push in the same frame. A release stops the walk in the same
  frame, as before. The ramp acts in the walk and in the fly. At the scale
  of a villager (1 unit for each meter) a full push is 1.5 units each
  second, and 48 units each second after 11 s: 1000 units of the land take
  about 29 s from rest. At 100 units for each meter the top is 4800 units
  each second, so a player at that scale lets go before it. The comfort
  vignette is at its full darkness from a move of 150 cm of the room each
  second (section 5.8), so a faster move keeps it full. With
  `move_stick_ramp=0` the speed is the speed of the push only.
- **The time of a frame** (`god_display_time`, 1 is the default). The walk,
  the smooth zoom and the smoothing of the grips of a grab (5.1) take the
  time of a frame from the display times that the runtime predicts for two
  frames of the headset (`XrScreen::DisplaySeconds`), so each frame moves by
  the time between two pictures. With 0, or with no session, it is the time
  of the CPU between two main camera calls (or two samples of the grips),
  which changes by some milliseconds from frame to frame, and a fast move
  then judders. A time between two display times of 0, or of 100 ms or
  more, keeps the time of the CPU. The report has the count of the frames
  of the walk and the zoom that took the display time.
- **It stops at once.** The pump of the controllers asks the view again
  before each frame (`PumpWalk`, `TabletopGodWalk`), and the main camera call
  moves the view one time (`ApplyGodWalk`). A frame with no request moves
  nothing, so the walk stops in the frame in which the stick comes back.
  There is no glide and no ease.
- **The land under the head** (`move_stick_follow_land`, 1). While the
  player walks, the height of the view follows the land under the head, so
  the land stays at the same height below the hands. The land height under
  the head at the new place goes through two stages of a first order filter
  (`FollowStep` in `walk_math.h`), and the view goes up or down by the change
  of its output. The filter counts the way of the head in the room, not the
  time:
  - A frame with no step changes nothing. So the view does not move while the
    stick is let go, and it does not drift after the walk.
  - Each stage has a length of 25 cm of the room (0.17 s at a full push), so
    the view is about 50 cm of the walk behind the land. A swell of the land
    with a wavelength of 40 cm of the room moves the view by 6 percent of its
    height, one of 1 m by 29 percent, and a hill of 3 m by 78 percent. So a
    long hill moves the view, and a small bump does not shake it. The second
    stage starts from rest, so the view does not start to rise with a jolt.
  - The stages take the exact answer of the filter for each step, so the
    heights do not change with the frame rate.
  - The height that the player set (the ground step, the grab, a zoom) is
    the height above the land. The filter keeps its state from one walk to
    the next, so a climb in short pushes follows the land as a long push
    does. When something else moved the view since the last frame of a walk
    (a teleport, a grab, a zoom, a ground step, a new build), the filter
    starts again at the land under the head, and the land keeps the height
    below the head that it has then.
  - The teleport puts the land under the head at the same height in the room
    as before the teleport (section 14.4), with each value of the key. So
    after a teleport the land is at the height below the hands that the
    player has.
  - With `move_stick_follow_land=0` the view keeps its height in the world.
    The launcher has no row for the key: "Walk" of the row "Movement type"
    is 1, and a save of the launcher removes the key from bwvr.ini. The
    switch "Move kind" of the debug page sets it while the game runs.
- **A hill at the new place.** When the land at the new place of the head is
  nearer than 10 cm below the head, all of the land goes down to that limit
  (`ZoomClearHead`), as in a zoom that moves the head over the land. The
  land does not come up again by itself. With the follow of the land, only a
  short sharp hill that the filter does not follow comes that near. With
  `move_stick_follow_land=0` the limit acts on each frame of a hill near the
  head, so the view then goes up with each bump at once.
- **The fly** (`move_type=1`, the launcher row "Movement type", Fly). The
  stick moves the view along the forward direction in 3D, so a push while
  the player looks or points down goes down, and up goes up. Forward is the
  forward axis of the head (`move_stick=1`), with its pitch, or the aim ray
  of the controller of the stick (2). So with the controller the player
  flies where the controller points. The stick to the side moves level, at
  right angles to the flat part of forward (`FlyRight` in `walk_math.h`).
  When forward points less than 11.5 degrees from straight up or down, the
  side move goes along the level right of the head. The speed and the time
  of a frame are the ones of the walk, and the speed is along the 3D
  direction:

  ```
  G' = G + s * Lt * step                     step = speed * the time of the frame
  ```

  - The view does not follow the land.
  - The floor (`FlyFloor` in `walk_math.h`, `FlyLandClearance`). The
    clearance has the measure of the head blinder (section 5.9): the head
    point over the highest land at it and at four points 6 cm of the room
    around it. So a steep slope in front of the face counts, as for the
    overlay. The floor is 10 cm (`kFlyClearMeters`), 5 cm over the start of
    the overlay. The fly looks at the place of the head after the step.
    When the step brings the head nearer than the floor, the view goes up
    until the head is at the floor. So a fly down stops over the land, and
    a fly across a rising slope slides up along it while the stick is
    pushed. Nothing lifts the view when the stick is at rest. A head that
    is already nearer (the player bent down into the land) is held at its
    clearance, and the fly does not lift it out.
  - The ceiling (`FlyCeiling`). The fly has no ceiling of 2.5 m, so a player
    at the scale of a villager can fly over the island as a bird. The
    ceiling is 10000 world units over the land under the head, in world
    units so that a small scale does not get a low ceiling. The far zoom
    already puts the head that high (4000 units for each meter, the land
    2.5 m below the head). At the ceiling the move up stops, and the move
    across goes on.
  - The solids (`FlySolidStop`, `FlySlide` in `walk_math.h`). With
    `god_zoom_solid_stop=1` the head stays `god_zoom_solid_cm` (30) out of a
    house, a rock or a tree trunk (the solids of section 2, "Houses at a
    small scale"), with the solids that the first eye pass of the frame
    before kept within 1.5 m of the head (`TabletopZoomSolidReachM`). With
    `fly_solid_slide=1` a step that goes into that shell slides past the
    solid: only the part of the step along the outward direction of the
    solid at the head comes off (the gradient of the distance to the
    solid), and the part along the solid stays. A second pass does the same
    at the end of the step, for a corner. So a fly at a slant goes round a
    tree, and a fly forward with the head pitched up goes up the wall of a
    house and over its roof. Only a push straight at the middle of a solid
    stops (less than 1 percent of the step is along it). The slide adds no
    part away from the solid, and a stick at rest moves nothing. A head
    that is already nearer than the shell may go on while it does not come
    nearer. With `fly_solid_slide=0` a step stops short of the shell along
    its own direction, and the player steers away to go on. The switch "Fly
    at solids" of the debug page changes the key. The solids near the head
    need `head_blinder=1` and `head_blinder_objects=1` (the defaults): with
    0 the fly does not see a house.
  - High over the land at a small scale the God view keeps the fog of the
    sky and the distance of the map objects (`cull_distance`, `cull_fog`,
    `backend.md`), because it turns the fog off only above 3 times the life
    size scale (section 2). So from a great height the far land can be in
    the fog.
  - After a fly the land can be more than 2.5 m below the head. Nothing
    moves the view back by itself. The walk starts its filter again at the
    land under the head, so it keeps that height over the land and follows
    the hills from there. The grab, the ground step and a zoom keep their
    range of 10 cm to 2.5 m only for a move that goes farther out of it
    (`GodGroundHeight`): a grab or a step that lowers the view acts, a move
    up is stopped. A teleport and a move of the game camera to a new place
    (a story, a load) put the land in the range again, as they do in each
    view.
  - The fly stops in the frame in which the stick comes back, as the walk
    does. There is no glide and no ease.
  - The comfort vignette counts the move up and down too (section 5.8).
  - A change of `move_type` stops a walk or a fly that runs. The filter of
    the land starts again at the next walk.
- **The map and the game camera.** The waist point stays on the map, as for a
  grab. The version of the view goes up on each frame that moves, so the game
  camera follows the player (section 7).
- **The game's switch.** The walk needs the switch for the drag of the land
  (bit 8), as the grab and the teleport do. A push with the switch off does
  nothing, is counted one time and must come back. Each frame that moves
  sends event 0x308 with the input bit 2, as a move of the grab does.
- **The hint.** While a push of the stick moves the player, the card of the
  hand with that stick has the line "Stick Move" in the prompt of the land
  (`prompt_moves`, `controller-hints.md`, 9.4).
- **When the stick does not move the player.** While a grip holds the world.
  In a dialog box and in the game menu. While the God view does not own the
  view: a story, a cut scene, the Temple. While the camera is not the
  player's (`WorldGrabCameraReady`). While the controllers cannot act
  (`controllers.md`, section 9). While the stick drives the ring
  (`button_layout=0`, or a controller with no face buttons: `controllers.md`,
  5.1). A push that is refused, and a push that is there when the stick gets
  this job, must come back to the middle before it moves the player. The log
  has one line for a refused push.
- **The other stick** keeps its jobs. The stick of the pointing hand turns
  and zooms, also during a walk.
- **Comfort.** A walk with a stick makes some players ill. The grab and the
  teleport move the player with no such motion. `move_stick=0` turns the
  walk off (bwvr.ini only).

| Key | Default | Meaning |
| --- | --- | --- |
| `move_stick` | 1 | 0: the stick does not move the player (bwvr.ini only). 1: forward is the direction of the head. 2: forward is the direction in which the controller of the stick points. The launcher row "Movement direction" |
| `move_type` | 0 | 0: the stick walks over the land. 1: the stick flies along the forward direction in 3D, and the view does not follow the land. The launcher row "Movement type" |
| `move_stick_speed_cm` | 150 | The speed of a full push, in cm of the room for each second, 10 to 500 |
| `god_display_time` | 1 | 1: the walk, the smooth zoom and the smoothing of the grips take the time of a frame from the display times of the headset. 0: from the time of the CPU |
| `move_stick_follow_land` | 1 | 1: while the player walks, the height of the view follows the land under the head through the filter. 0: the view keeps its height. Not in the launcher: a save of the launcher removes it |
| `move_stick_trace` | 0 | A test for a desktop run: the log has one line for each of the first N frames of each walk or fly (at most 2000 lines in a game) |
| `move_stick_ramp` | 1 | 1: a push of 90 percent or more gets faster with its time, in the walk and in the fly. 0: the speed of the push only. The switch "Speed ramp" of the debug page. Not in the launcher |
| `move_stick_ramp_most` | 32 | The largest factor of the speed ramp, 1 to 64 |
| `fly_solid_slide` | 1 | With `god_zoom_solid_stop=1`, 1: a fly slides past a house, a rock or a tree trunk, and only a push straight at it stops. 0: a fly stops short of it. The switch "Fly at solids" of the debug page |

The switch "Move kind" of the debug page goes from the walk that follows the
land (`move_type=0`, `move_stick_follow_land=1`) to the walk that keeps its
height (`move_stick_follow_land=0`), to the fly (`move_type=1`) and back. The
pointing controller gives a short, a medium and a long pulse. A walk or a
fly that runs stops.

With `test_keys=1`, Shift and the Menu key of the keyboard change `move_stick`
from 1 to 2, to 0 and back to 1 while the game runs. The Menu key is the key
beside the right Ctrl key that opens a context menu. The pointing controller
gives a short pulse for 0, a medium pulse for 1 and a long pulse for 2. A
walk that runs stops, and a stick that is still pushed starts a new walk with
the new rule. The word `movekey` of the scripted controller does the same in
a desktop run.

The log has one line for the start of a walk, one for its end, and one for a
check 30 frames later:

```
god view     : the walk with the stick starts at 100.00 units per meter, with the land under your head at 1858.8 2528.7, 61.6 cm below your head. Forward is the direction of your head (move_stick=1), and it is 0.0 degrees to the right of straight ahead in the room. A full push moves you 150 cm of the room each second (move_stick_speed_cm). The height of the view follows the land under your head (move_stick_follow_land=1), frame 901.
god view     : the walk with the stick stopped (the stick is back). You went 789.67 units over the land, which is 789.67 cm of the room, 0.0 degrees to the right of straight ahead in the room. The land under your head was at 1858.8 2528.7 and is at 2625.1 2338.0. The stick was pushed for 5.264 s, which gives 150.0 cm of the room each second, and the view moved on 300 of 300 frames (move_stick=1, 100.00 to 100.00 units per meter), frame 1201. The land under your head was 61.6 cm below your head and is 63.8 cm below it. The view went 118.7 cm up and 130.4 cm down with the land (move_stick_follow_land=1). A frame took 16.2 to 27.4 ms, and a step was 2.43 to 4.11 cm of the room.
god view     : walk check, 30 frames after the stop the waist point is at 2625.12 2338.04, and it was at 2625.12 2338.04 at the stop: 0.000 units apart. In those frames the drawn head moved 0.00 cm of the room.
controllers  : the stick of the other hand does not move you: a grip holds the world, frame 1081
```

The line of the end also says how far the land went down for a hill, and on
how many frames the controller had no direction. The shortest and the
longest frame and step show whether the walk was even: at a steady push each
step is the speed times the time of its frame. In the headset the line also
counts the frames of the headset that came more than one refresh after the
frame before (see "An even walk" below):

```
Of N frames of the headset, M came more than one refresh of 8.3 ms after the frame before, and the headset showed the frame before again for them.
```

With `move_stick_trace=N` the log also has one line for each of the first N
frames of each walk:

```
god view     : walk trace, frame 61 of the walk: 18.05 ms, a step of 2.707 cm of the room (the speed times 1.00), the waist point at 2023.553 42.130 2487.737, the land under your head at 49.517 (filtered 40.547) and 52.61 cm below your head, frame 961
```

A fly has the same lines with the word "fly". The line of the start also
gives the degrees of forward below or above level, and the line of the end
gives how far the fly went up or down in the room (`move_type=1`). The line
of the start names the speed ramp, and the line of the end gives the
largest factor of the ramp when it went above 1 ("The speed ramp went up to
R times the speed of the push"). Each trace line has the factor of its
frame. The trace of a fly has the step up and the clearance of the head
(the measure of the head blinder) in each frame. The line of the end of a
fly also gives how far its floor lifted the view, the least clearance after
a step, how many solids the fly met, and how many steps slid past one or
stopped at one. Each solid that the fly slides past or stops at has one
line, and one more when the way changes from a slide to a stop:

```
god view     : the fly slides past the Tree 1632AEE0 at X Y Z: the whole step puts your head 26.7 cm out of it, and your head stays 30.0 cm out of it (the shell is 30 cm, fly_solid_slide=1). Your head is at X Y Z, and the middle of the solid is D degrees to the right of straight ahead in the room, frame N
```

The trace of a fly:

```
god view     : fly trace, frame F of the fly: T ms, a step of S cm of the room (U cm up, the speed times R), the waist point at X Y Z, the land under your head at H and B cm below your head, a clearance of C cm, frame N
```

**An even walk.** The step of each frame is the speed times the real time
of that frame, so the walk is even in the time of the game: each frame
moves the view at the speed of the push, and no limit, hill or switch of
the game stops it on some frames (section 12 has the run). In the headset
the walk can still look uneven. At about 97 frames each second on a display
of 120 Hz with Space Warp off, about one refresh in five has no new frame.
The headset then shows the frame before again, and with no Space Warp it
corrects only the turn of the head. So a move of the view stops for one
refresh and jumps at the next. A turn of the head hides this, but a walk
shows it on all of the land, and more where the frame rate is lower. A frame
rate at the refresh rate of the headset (for example 90 Hz in Virtual
Desktop), or Space Warp, gives an even walk. The count in the line of the
end shows how many frames came late. The report of a window has
the count of the walks, their time and the frames that moved the view.

### 5.8 The comfort of a move

Three things help a player who feels ill when the view moves: the comfort
vignette, a stick that does not turn or zoom while a dialog box is up, and a
recenter from the controllers.

**The comfort vignette** (`comfort_vignette`, `vignette.cpp`). The edges of
the view go dark during a move that the player makes with a control and that
the body does not feel. The middle of the view stays clear. These moves count:

- the walk and the fly with the stick, the smooth zoom, the smooth turn and
  the grab of the world with one hand or two hands. Each one reports how
  fast the view moved in the frame (for the fly, also the move up and down), as a share of the full speed of a control: 150 cm of the room each
  second, a turn of 90 degrees each second, or a zoom of 1.5 times in 500 ms.
  The largest of the three counts (`comfort_math.h`, `MoveShare`). So a slow
  push of the stick gives a light vignette, and a grab that holds still gives
  none.
- a snap turn, a zoom step, a step of the ground and a teleport. Each one is
  a step of one frame, and it puts the vignette at its full darkness at once.

A move that the player did not make (a story, a move of the game camera, the
view that is built again) and a move of the head give no vignette. The
vignette comes in 0.15 s and goes in 0.3 s, in a straight line. It never
moves the view.

| Value | The middle of the eye is clear to | Full darkness from | The darkness at full speed |
| --- | --- | --- | --- |
| 0 | The vignette is off (the default) | | |
| 1, gentle | 35 degrees | 58 degrees | 85%. The edge of the view of a Quest 3, about 50 degrees from the middle, gets about 60% |
| 2, strong | 22 degrees | 45 degrees | 100%, black |

The darkness goes along a smooth curve from the clear angle to the dark
angle. The vignette is the last draw of the mod in each eye pass, after the
world, the hands and the glow (`sky_detail.cpp`, `h_SunGlare`): 6 rings of 48
black triangles with an alpha at each corner, in the pixels of the eye. The
angles go to pixels with the projection of the game (`InfoTransform`). The
draw has no depth test, so the land never hides it. The interface of the
game draws after it, and the hint cards and the ring are layers of their own,
so they stay clear. One walk for both eyes (`one_walk.cpp`) copies the batch
into the right eye with no shift, because each vertex has an rhw of 1. The
hands of the player are in the world pass too, so the edge darkens them.

**No turn and no zoom while a dialog box is up** (`menu_stops_turn`,
`PumpStick` in `controllers.cpp`). While a box of the game is up, for
example the game menu of Esc, the walk, the teleport and the grab of the
world do not move the view. With `menu_stops_turn=1` the stick of the
pointing hand also does not turn and does not zoom the God view: the snap
turn, the smooth turn, the zoom step and the smooth zoom stop, and so does
the turn in a room
of the Temple. Each push is refused one time and counted, and it must come
back to the middle before it acts after the box closes. Outside the God view
the push forward and back is a wheel click, which a box can use, so only the
turn stops there.

**The recenter hold** (`recenter_hold`, `PumpRecenter` in
`controllers.cpp`). Hold the stick click of the hand that does not point for
1 s, with the stick at rest, and the view recenters as F11 and the
recenter of the headset do (`HeadPoseRecenter`). Both hands get a pulse. A
push of the stick in the hold starts the time again, so a click that comes
with a walk does not recenter. On a Vive wand the click of the pad is a part
of each push, and the same rule keeps a walk from a recenter. After a
recenter the click must go up before the next hold. The hold does not act
when the stick click of that hand has a different job: the ring with
`radial_menu_button=2` on the hand that does not point, and the draw of a
gesture with `gesture_button=2` on the pointing hand. The log says so one
time. The item `recenter` of the ring does the same (`controllers.md`,
10.2).

| Key | Default | Meaning |
| --- | --- | --- |
| `comfort_vignette` | 0 | 0: the edges of the view stay clear. 1: a gentle vignette. 2: a strong vignette. The launcher row "Comfort vignette" |
| `menu_stops_turn` | 1 | 1: while a dialog box is up, the stick does not turn and does not zoom the God view. 0: it turns and zooms then |
| `recenter_hold` | 1 | 1: a hold of 1 s of the stick click of the hand that does not point recenters the view. 2: the stick click of the pointing hand. 0: no hold recenters |

The debug page of the ring (`debug_menu=1`, the group "View") has the
switches "Vignette" (off, gentle, strong), "Menu turn" (turns, stops) and
"Recenter hold" (off, the other hand, the Hand). The pointing hand gives a
short pulse for the old value and a longer pulse for the others.

The log:

```
vignette     : comfort_vignette=1 (gentle). In a move that you make with a control, the edges of the view go dark: clear to 35 degrees from the middle, and 85% dark from 58 degrees at full speed. It comes in 0.15 s and goes in 0.30 s
vignette     : the edges of the view go dark (gentle), the walk with the stick, frame 272
vignette     : the edges of the view are clear again after 2.97 s, with a top level of 1.00, frame 440
vignette     : the edges of the view go dark (gentle), a snap turn, frame 772
controllers  : the stick turn is refused while a dialog box is up (menu_stops_turn=1), frame 1121
controllers  : a hold of the stick click of the other hand recenters the view, frame 1634
head         : recentered on the next frame
```

The report of a window counts the times that the vignette came, the steps
among them and the eye passes that drew it, and the pushes that a dialog box
refused and the recenters.

### 5.9 The head under the land

The land is at the waist, so a player who bends down can put the head into a
hill. The game draws no land from the inside, and the eyes then see what is
under the land. As in Half-Life: Alyx, an opaque overlay then fills the view
(`head_blinder`, `head_blinder.cpp`, math in `blinder_math.h`).

- **The head.** Once a frame the head point is the render head of the God
  view, in the middle between the eyes. The land counts at the head point
  and at four points 6 cm of the room around it, and the highest one counts,
  so a steep slope in front of the face counts too. The land is
  `LH3DIsland::GetAltitude`, as for the stop of the Hand at the land
  (`hand-model.md`, "The Hand stops at the land").
- **The solids** (`head_blinder_objects`). A house, a rock and a tree trunk
  count too, with the solids that stop the Hand (`hand-model.md`, "The
  Hand stops at the land"): the box of the mesh of a house and of a rock,
  and the trunk of a tree. The clearance is the least of the height over the
  land and the distance from the head point to each solid, so the overlay
  starts 5 cm out of a wall and is opaque in the house. The hook of the
  screen test of the draw offers each object whose ball round its box is
  within 15 cm of the room of the head, so the house that the head goes
  into is in the list also when the Hand is far from it. A solid stays in
  the list of the blinder while the head is within 15 cm of it, for 600
  frames at most after the last frame that had it. So a house that the draw
  stops to offer while the head is in it still counts. A field, a villager,
  an animal and the Creature do not count.
- **The strength.** From 5 cm over the land the overlay starts at the edge of
  the view and grows inward along a smooth curve. At the land and under it
  each eye sees only the overlay. The overlay comes in 0.1 s and goes in
  0.25 s, in a straight line. It never moves the view.
- **The picture.** A warm gold in the middle of the eye and a deep red from
  50 degrees out. It is the last draw of the mod in each eye pass, after the
  vignette (section 5.8), with the same states: no depth test, and the
  vertices in the pixels of the eye with an rhw of 1. So the land does not
  hide it, both renderers have it, and one walk for both eyes copies it into
  the right eye. The head point is between the eyes, so both eyes get the
  same overlay. The hint cards and the ring are layers of their own, and they
  show over it.
- **When.** Only while the God view owns the view and no dialog box is up. In
  a room of the Temple, in a menu, in a story and in a cut scene the overlay
  does nothing, and an overlay that shows goes at once.
- **The floor of the head.** When the view goes to a place of a game camera,
  and in a zoom, a walk and a grab, the land under the head stays 10 cm
  (`kGroundNearest`) below the head (`FloorHeadY` in `tabletop.cpp`). With
  `head_blinder=1` that is 10 cm below the height of the head at the recenter
  pose, not below the head now. So a head that goes down does not push the
  land down, and nothing moves the view that the player did not move: the
  head goes into the land, and the overlay covers it. With `head_blinder=0`
  the floor follows the head now. The log has the line "the
  floor of the head acts" each time that the floor moves the land down.

| Key | Default | Meaning |
| --- | --- | --- |
| `head_blinder` | 1 | 1: an opaque overlay when the head goes into the land, and the floor of the head is at the height of the head at the recenter pose. 0: the eyes see under the land, and the floor of the head follows the head. The switch "Head blinder" of the debug page. Not in the launcher, and a save of the launcher removes it (`launcher.md`, "Retired keys") |
| `head_blinder_objects` | 1 | 1: a house, a rock and a tree trunk cover the view as the land does. 0: only the land counts, and the eyes see into a house. The switch "Blinder solids" of the debug page |
| `solid_house_inset_pct` | 20 | Below the life size scale the box of a house is this percent smaller across, nearer to its walls (section 2, "Houses at a small scale"). It is the same box for the drawn hand (`hand-model.md`). 0 to 50. 0: the whole box at each scale |

The log has one line when the overlay starts, with the clearance of the
head, what it is near (the land, or the class and the address of a solid)
and its place on the map. One line when the view is opaque, with the same
facts. One line when the overlay is gone, with its time and the least
clearance of the head. The report of a window counts the times that the
overlay showed, the opaque frames and the eye passes that drew it.

The desktop run of the solids is `village_run.ps1 -Kind headhouse`. At 29.6
units a meter a house is about 11 cm high. The head goes over the middle of
a house (the seek word `gointo`), then 55 cm down into its box for 120
frames, and up again. No desktop run has shown the overlay in a house yet.
The Village Store is no solid: it is an open pit, and the game draws a
normal view with the head in it.

## 6. The Hand

The Hand's size is the real hand at the scale now:

```
size = hand_real_cm * hand_scale_pct / 100 * s / 3.2
```

It is 19 cm long in the room at every scale: size 0.06 at 1 unit per meter,
0.45 at 7.5 units per meter, 5.9 at 100, and 237.5 at 4000. So zoomed out it is large against the
villagers, and a held villager takes the size of the Hand, as the game does.

The controllers place the Hand through the God view's room with no reach
curve, one to one (`controller_math.h`, `TableHand`). A hand pushed into the
land stops 5 mm of the room over it, so the point and the glow touch the land
and the villagers on it (`hand.md`). The grip takes, carries and lets go like
a real hand (`hand.md`). The pick fix of the table (`tabletop.md`, "The hand
on the table") is installed for play mode 4 too, so a villager of a few
pixels can be picked at every scale: the screen test offers the objects near
the Hand, and the interface step gets the land under the Hand. The throw and
the set down use the rule of the table, with the gain `s / 7.5`. At the life
size scale that is the throw of the immersive mode. Zoomed out, a swing
carries the Hand far over the land, so a throw that ends outside the player's
influence goes from the last point of the swing inside it (`tabletop.md`,
"The throw from inside the influence").

## 7. The game camera

The picture does not come from the game camera, but the game logic does: the
sound starts, the detail lists, the help triggers and the pull back of
`GCamera::Update` at 3500 units from the map center. So after the game's
update of `CameraModeNew3`, `comfort_camera.cpp` (`GodAfter`) puts the game
camera over the player's place, in the fields the snap turn and the grab
write:

- The focus is the land point under the head (`god_camera_at`, 1), kept
  within 3000 units of the map center. With `god_camera_at=0` it is the land
  under the waist point, the recenter place. The place is taken once for each
  move of the view. With `god_zoom_about=0` a zoom keeps the land point under
  the head, so it keeps the game camera in its place too. With 1 and 2 the
  head moves over the land in a zoom, and the game camera goes with it. The
  recenter place moves in a zoom when the head is off the recenter place.
- The camera is 400 units from it, 50 degrees above, along the heading of the
  view.

It moves only when the player moves the view: once for each step, on each
frame of a grab or of a walk with the stick, and in a hold of the smooth zoom
(section 2). A hold that
moves the head over the land writes it on each frame, and a hold that does
not writes it at its start and at its end. It does not follow the head when
the view is still, because `GCamera::Update` sets
its moving flag for a camera that changes, and the interface then clears the
gesture trail. It does not undo the game's own moves of its camera either. At
the build the game camera stays where the game put it, and the view goes to
it (section 15). No write happens while a script holds the input or the
pivot, while a fight holds the camera, or while a jump flies. In a camera
zone of a script the game camera stays in the zone (section 17).

**A snap turn alone keeps the focus** (`god_turn_keeps_focus`, 1). When the
view only turned with snap turns since the last write
(`TabletopGodTurnedOnly`), the game camera turns about its own focus by the
same angle (`table_math::TurnAboutFocus`), and the focus stays. Over the land
under the head that is the place that the rule above gives, because a turn
about the head keeps that land point. It differs when a script put the
focus: the turn lesson of the tutorial (section 7, "The tutorial") points
the game camera at the Temple, and it tests each second that the focus is
less than 25 units from that point. With `god_turn_keeps_focus=0` the turn
puts the game camera over the land under the head, and the lesson says "No,
try rotating first." after three seconds and starts again.

**Stories.** While a story or a follow camera holds the view
(`script_camera.cpp`, `ScriptCameraStoryOwns`), the God view stands aside.
The story plays at the life size scale, as in the immersive mode. When the
player's camera is back, the God view is built again at the game camera, with
the last scale and the last ground height (section 15). Ctrl F11 and a new
land build it again the same way.

**Cut scenes.** A cut scene does not play in the full view. While the cinema
bars of the game are up, the view closes to a screen 2 m ahead, and the game
shows its cut scene there as play mode 1 does (`cutscene-window.md`). The God
view stands aside for those frames in the same way, and it is built again in
the same way when the cut scene ends. The rule above is for a story camera
with no bars, for example a camera that follows the Creature.
`cutscene_window=0` gives that rule to cut scenes too.

**A load.** A saved game of the land that is up has the same block table, so
the land does not look new. A post hook on `GGame::ClearMap` `0x00552BB0`
builds the view again at the next main camera call. `LoadAllGame`,
`StartPlaygroundGame` and `GGame::Close` call `ClearMap`. The hook assumes
that the load puts the game camera in place before the next main camera call.
The view also follows a camera that the game puts in place later (section 15).
The last scale lasts for the session only. The launcher owns `bwvr.ini`, so
the mod does not write the scale there.

**The tutorial.** Scripts and the tutorial turn the camera controls on one at
a time, with the switches at `CameraHelp::EnabledFeatures` `0x009CDD6C`: bit
1 the tilt, 2 the turn, 4 the zoom, 8 the drag of the land and 0x10 the
double click jump. The game's camera code tests each bit before it acts, for
example at `0x0045C0E7` for the zoom and at `0x0045D3A6` for the drag. The
God view keeps to the same bits:

- A zoom step and the smooth zoom need bit 4 (`TabletopGodStep`,
  `TabletopGodZoomHold`).
- The mouse pan, the grab, the walk with the stick and the teleport need bit
  8 (`MousePan`, `TabletopMove`, `TabletopGodWalk`, `TabletopMoveAllowed`).
  Each of them moves the player over the land, so one switch holds for all
  of them. With `teleport_switch=0` the teleport needs bit 0x10 (section
  14.1).
- A two hand grab turns only with bit 2 and zooms only with bit 4
  (`TabletopGrabAllows`, called by `world_grab.cpp`).
- A snap turn and the smooth turn need bit 2 (`MayTurn` in
  `comfort_camera.cpp`).
- The mouse double click needs bit 0x10, because it is the jump of the game.

**The interface levels** [V: the disassembly and
`Scripts\Quests\challenge.chl`]. A script sets all the bits at one time with
the script word "set interface interaction"
(`GScript::SetInterfaceInteraction` `0x0070B220`, function 63 of the script
machine). The word takes one of 16 levels (the jump table at `0x0070B7A8`),
and it calls `CameraHelp::EnableCameraFeatures` `0x00447430` with the bits of
the level and a mask of all bits. The table has each level that a script
uses:

| Level | Bits | The camera controls | The scripts |
| --- | --- | --- | --- |
| 0 | 0x1BF | All | Each script below, at its end |
| 1 | 8 | The drag | `FollowUs`, the walk to the village. `MoveAroundMountain`, at its start |
| 4 | 0x18 | The drag and the jump | `FollowUs`, the way from the village to the Temple |
| 8, 12 | 0 | None | `TheMissionaries`, `LearnGestures`, `PhysicalShieldChallenge`, `FireBallChallenge` |
| 10 | 0xA | The turn and the drag | `MoveAroundMountain` |
| 11 | 1 | The tilt | `MoveUpAndDownMountain` |
| 13 | 0x1A | The turn, the drag and the jump | `FollowUs`, the turn lesson |
| 14 | 0x1B | The tilt, the turn, the drag and the jump | `FollowUs`, the tilt lesson |
| 15 | 0x3F | The six low bits | `FollowUs`, the zoom lesson |

The levels 2 (bit 8), 3 and 6 (bit 2), 5 (0x24) and 7 (0x26) are in the code
of the game, and no script uses them. Level 9 is an error. The levels 1, 2
and 10 also call `fn_004473F0` `0x004473F0`, which turns on bit 0x40 and
writes `CameraHelp::AutoPitchParam1` and `AutoPitchParam2`: the automatic
pitch of the game camera. So the log has 0x48 for level 1. The scripts of the
source file `LandControlT.txt` (`MoveAroundMountain`, `MoveUpAndDownMountain`)
are the lessons of the Island Keeper in the Gods' Playground, and not a step
of Land 1 ("The steps of the tutorial", below).

**The walk to the village** [V: `challenge.chl`, the script `FollowUs`, source
file `followus.txt`]. At the start of a new game the first story ends with
level 1, and the villagers ask the player to follow them. Each game turn the
script measures the distance on the map from "camera position" to the end of
the path (`EndMarker`) and to the father. `EndMarker` is the focus of camera
219 of `Data\camera.edt`, at 1633.7 2308.0. The step ends when the first
distance is less than 50 units. The script counts no event of the help for
this step. A distance from "camera position" is the distance from the player
(section 16), so the player comes there with the grab, with the walk with the
stick or with the teleport. The next step has level 4 and the script
`TunnelOfUltimateDoom`: it ends when the player is less than 75 units from
the Temple.

The tutorial counts the events of `CameraHelp::CameraHelpCallback`
`0x00449140`. Only the 0x300 events reach `HelpProfile::Trigger`. The 0x100
and 0x200 events feed the help hints only. They are errors and collisions:
0x102 is "Hand is too far away" and 0x200 is "Collide with ground". The
numbers below come from the calls in `CameraModeNew3::Update` and from the
game's own names of the events at `0x009CDD70`. That table has no names for
0x301 and 0x302, so its names from 0x303 on are two places early.

| Event | Meaning | The game pushes it at | The God view |
| --- | --- | --- | --- |
| 0x300 | The camera turned | `0x0045C745`, for a turn key | A snap turn, and a turn of a two hand grab |
| 0x301, 0x302 | Then to the left, or to the right | `0x0045C772`, `0x0045C79F` | The same. A snap turn sends 0x301 for the left and 0x302 for the right, in every play mode |
| 0x303 | The camera tilted | `0x0045C82B` | Never: the view does not tilt |
| 0x304 | The camera zoomed | `0x0045C3A8`, on each update in which the camera zooms | A zoom step, each frame of the smooth zoom that changes the scale, and a zoom of a two hand grab |
| 0x305, 0x306 | A double click jump to the land, or to an object | `0x0045DE85`, `0x0045DE1E` | The game's own jump, and each move of the teleport |
| 0x307 | The zoom to the Temple | `0x00457EDE` | Never |
| 0x308 | A drag of the land, or a move by the keys | `0x0045D682`, `0x0045C981` | The game's drag code, for the mouse pan, a pan of the grab, and each frame of the walk with the stick that moves |

The third argument names what made the move. The game's names for its bits
at `0x009CDDF0` are "keyboard" (1), "mouse button" (2), "both mouse
buttons" (4), "wheel spin" (8) and "wheel down" (0x10, the middle button).
The God view sends 8 with a zoom step and with the smooth zoom, and 2 with a
move of the grab and of the walk with the stick.

A tutorial step that waits for a tilt (0x303) could not end in the God view,
because the view does not tilt. The head looks up and down instead, so a
look of the head counts as the game's tilt (`tilt_help_degrees`, 3): one
tilt event for each 3 degrees that the head pitches at 20 degrees a second
or faster (`table_math::CountedPitch`), one in a game turn, while the
script lets the camera tilt (bit 1). A still head and the noise of the
tracking count nothing. The tilt lesson (`TeachPitch`, "Good, now let's
look up and down") wants its count of event 28 plus 25, and one look down
and back up by 40 degrees gives 26. 0 sends no tilt events.

**The turn lesson** [V: the disassembly and `Scripts\Quests\challenge.chl`,
LHVM version 7]. The script `TeachRotate` (source file `HandDemos.txt`) is
the step "Try rotating now to see the Temple as it's being built". It reads
the count of turn events with the script word "get total events"
(`GScript::GetTotalEvents` `0x0070B910`, function 237 of the script machine)
for event 25, which `CameraHelpCallback` makes of 0x300. At its start it
wants that count plus 60, and the step ends when the count is more than
that. Each second it also tests that the camera position is less than 100
units and the camera focus less than 25 units from its point at the Temple
(`centre_pointer`). The third second that fails says "No, try rotating
first." (`HELP_TEXT_FINAL_BEGINNING_46`), puts the camera back and plays the
demo again.

A count takes at most one event in a game turn: `CameraHelpAccumulator::
Trigger` `0x00449040` sets a flag that `HelpProfile::Process` `0x005C4660`
clears in each game turn, and that call also adds 100 to the clock of the
help (`HelpProfile::AccumulatedTime` `0x00C5AFD8`). The game's own turn sends
one event in each frame while it runs, so the lesson ends after about 6 s of
turning. A snap turn sent one event, so it took more than 60 snap turns, and
the turn moved the focus of the game camera (`GodAfter`), so the lesson
started again after three seconds. Two rules make the lesson end with one
full turn of snap turns:

- `turn_help_degrees` (5): a snap turn counts as one turn event for each 5
  degrees of its angle (`table_math::SnapTurnEvents`): 9 for 45 degrees. It
  sends the first at once, and `PayTurnEvents` sends one of the others in
  each new game turn. One full turn of snap turns is 72 events for each size
  of the step. 0 gives one event for a snap turn. The smooth turn owes one
  event for each 5 degrees that it turned (`comfort_math::SmoothTurnEvents`),
  so one full turn is 72 events too.
- `god_turn_keeps_focus` (1): the game camera keeps the focus that the script
  put (section 7, "A snap turn alone keeps the focus").

The report gives the events that the snap turns sent and the count of the
help, and a line for each change of `CameraHelp::EnabledFeatures`, which is
how the tutorial turns on the next control:

```
comfort      : the camera controls of the game are 0x1BF (1 tilt, 2 turn, 4 zoom, 8 drag, 0x10 jump). The help has counted 12 turn events
comfort      : the snap turns sent 64 more turn events to the help of the game (turn_help_degrees=5), and 8 updates turned the game camera about its own focus (god_turn_keeps_focus=1). The help has counted 84 turn events in all
```

A hook on the callback (`h_Help` in `comfort_camera.cpp`) logs each change of
the event, up to 80 lines, and the report counts each event:

```
comfort      : camera help event 0x304, inputs 0x08
comfort      : camera help events: 0x300 x2, 0x302 x2, 0x304 x3, 0x308 x41, 6 others
```

**The zoom lesson** [V: the disassembly and `challenge.chl`]. The script
`TeachZoom` (source file `HandDemos.txt`) is the step "Now you try." after the
demo of the zoom. In each game turn it ends the step when one of two tests is
true:

- The count of zoom events (event 29, which `CameraHelpCallback` makes of
  0x304) is more than its count at the start of the step plus 15.
- A marker at "camera position" is more than 100 units over the land. The
  script reads property 24 of the marker. `GScript::GetProperty` `0x0070DAE0`
  gives the thing + 0x1C for it, and `GScript::GetPosition` adds the same
  value to the height of the land, so it is the height over the land.

While the game camera is lower than 100 units over the land, its focus is 50
units or more from the Temple, and the hand state is not 0x16 (section 16.6),
the script puts the camera back and says "No, no. Try zooming." The third
time it also plays the demo again. After 60 s with no end it says the line
and plays the demo again.

In the God view the step ends at the first zoom. After a move of the view,
`GodAfter` puts the game camera 400 units from the land under the head, 50
degrees above it: about 306 units over the land. So the height test is true
in the next game turn. A walk, a grab and a teleport also end the step. A
snap turn alone keeps the game camera at the place of the script
(`god_turn_keeps_focus`), about 25 units over the land.

**The steps of the tutorial.** The table has each step after the first story
of a new game, the script and its level (the table above), what the script
waits for, and what a VR player does in the God view. "Shown by" tells what
showed the step: a desktop run [R], the log of a session in the headset [H],
or the code only [C].

| Step | Script, level | What the script waits for [V] | What a VR player does | Shown by |
| --- | --- | --- | --- | --- |
| Follow the villagers | `FollowUs`, 1 | The player less than 50 units from the end of the path | The walk with the stick, the grab or the teleport | [R], [H] |
| Go to the Temple | `TunnelOfUltimateDoom`, 4 | The player less than 75 units from the Temple. A player 100 units or more away who does not look at the Temple for 2 s gets the camera at the Temple ("So this is our Temple.") | The same | [R], [H] |
| Rotate | `TeachRotate`, 13 | More than 60 turn events (event 25). Each second, the player within 100 units of the Temple and the focus of the game camera within 25 units ("The turn lesson", above) | About eight snap turns (`turn_help_degrees`, `god_turn_keeps_focus`) | [R], [H] |
| Look up and down | `TeachPitch`, 14 | More than 25 tilt events (event 28), and the player within 100 units of the Temple | Look down and up with the head (`tilt_help_degrees`) | [R], [H] |
| Zoom | `TeachZoom`, 15 | More than 15 zoom events, or the game camera more than 100 units over the land ("The zoom lesson", above) | One zoom step, or the smooth zoom | [R], [H] |
| The Temple | `FollowUs`, `GiveFood`, `CheckCitadel`, 0 | Texts that wait for a click. `GiveFood` is a demo of the Hand that waits for no input. Then the Temple is built (property 22 of the Temple is 1). A player 100 units or more from the Temple and from two places at the Village Store gets the camera at the Village Store ("Why not help the people finish building the Temple first?") | The trigger is the click (`controller-hints.md`, section 10). The Hand takes food and wood from the Village Store to the builders (`village.md`) | [R] and [H] for the texts and the demo. [H] for the gift of wood. The end of the build: [C] |
| The missionaries | `TheMissionaries` (Land 1), 8 | Grain (300) and the animals at the ark. Level 8 is on only while the sailors speak after a delivery | The Hand takes them there | [C] |
| The gestures | `LearnGestures` (Land 2), 8 | A gesture of a miracle (event 15 or 17), or 5 minutes. The step goes on after each of the two | The gesture in the air (`miracles.md`) | [C] |
| The shield | `PhysicalShieldChallenge` (Land 2), 12 | A throw that hits the shield of the other god, or 20 s. Then a shield cast from a seed over the hut | The Hand throws, takes a seed and casts it. A seed that the head does not see goes back to its place (`script_view`) | [C] |
| The fireball | `FireBallChallenge` (Land 2), 12 | Fireballs from seeds at the huts | The same | [C] |

At levels 8 and 12 the game turns each camera control off, and the God view
keeps to it: the player stays at the place of the script and uses the Hand.

**The run on the desktop.** `spawn_run.ps1 -Choice normal` in
`bwvr/tools/desk` plays a new game through the tutorial of Land 1 to the
texts after the zoom lesson. At a render of 3600 by 2880 the game runs at
about 47 frames a second. The recipe has 172 words of the scripted
controller:

- A press of the trigger each 300 frames from frame 7950. The trigger is the
  click of a text that waits (`text_click`).
- The word `seek` with `seek_plan=go` at nine frames from 10600 to 13200. The
  God view teleports to the end of the path (1633.7 2308.0), then to 1900
  2519, 18 units from the Temple. A teleport in a story is refused.
- `stick_right` each 150 frames from frame 13600, `look` each 300 frames from
  14550, and `stick_up` each 300 frames from 15650. The game refuses each
  turn and each zoom before the level that allows it.

The log has the levels 0x48 (frame 10662), 0x18 (11628), 0x1A (14384), 0x1B
(15580, after 69 turn events from eight snap turns), 0x3F (18763) and 0x1BF
(21954). In the zoom lesson the first zoom step ends the step:

```
god view     : zoomed in, 100.0 to 66.7 units per meter, about the point at 1872.1 28.5 2536.2, 0.63 m below your head: the land under your head (god_zoom_about=0)
scripts      : a script asked for the hand state. The game said 0x3, and the script gets 0x16: the view zoomed in the last two game turns, which is the hold of both buttons (script_camera_moves=1, camera controls 0x3F), frame 18952
audio adv    : line 40 starts, sample 402, [...]
```

Sample 402 is "That's it. You've done it." The log of a session in the
headset has the same sample after two zoom steps.

The Gods' Playground (land 6, `Scripts\LandT.txt`, the script
`LandControlT`) is a land for practice. The key F2 of the game asks "Are you
sure you want to go to the Gods' Playground?" (`GGame::ProcessKey`
`0x0063F1CD`), and the answer Yes starts the land and its lessons
(`0x0054C930`). The tutorial of Land 1 tells the player of F2. In VR the
ring item `playground` sends F2, and it is on no page of the ring by
default (`controllers.md`, section 10.2). The Island Keeper teaches six
lessons:

| Step | Script, level | What the script waits for [V] | What a VR player does | Shown by |
| --- | --- | --- | --- | --- |
| Around the mountain | `MoveAroundMountain`, 1, then 10 | The player within 100 units of the Keeper while he walks. While he is out of the view, the script stops until the player looks at him. When he comes to his fourth point, "Move the Hand to the left of the screen." waits for the rotate arrow, and "Now click and hold the Move Button." waits for hand state 0x14 (section 16.6). Then a second walk in 5 minutes, within 50 units of him and behind him: he stops while he can see the head | The walk, the grab or the teleport, and a look at him with the head (`script_view`). A hand near him counts as near (`script_distance`). A snap turn gives the rotate arrow and 0x14 (`script_camera_moves`), and the trigger held on the land gives 0x14 too | [R] with `script_view=0` to the end of the step of the turn |
| Up and down the mountain | `MoveUpAndDownMountain`, 11 | The Keeper in the view while he walks up the path and down again | Look at him with the head (`script_view`) | [C] |
| Down the valley | `MoveDownValley`, 0 | The player within 50 units of the Keeper, with him in the view, and behind him | The walk, the grab or the teleport, and the head | [C] |
| Zoom in and out | `ZoomInAndOut`, 0 | Hand state 0x16, then two stones in the view, then the two stones out of the view | A zoom step gives 0x16 (`script_camera_moves`). Zoom out and look down to see the two stones, then zoom in or look away | [C] |
| The teddy bears | `PickupTeddyBears`, 0 | Five teddy bears within 30 units of the crater | The Hand takes them there | [C] |
| The double click | `DoubleClicking`, 0 | The player within 150 units of the island, then of the Keeper again, with a double click jump on each way (event 30 or 31) | The teleport, which sends the jump event 0x305 | [C] |

A desktop run reaches the first lesson. It starts in the test save `temple`
with `radial_menu_items` beginning with `playground`. The ring sends F2, and
the trigger on "Yes." of the question of the game starts the land: the log
has "a new land of 64 blocks", the story of the Keeper, and level 1 (0x48).
Then the run teleports the head 50 units behind the Keeper (the word `go`),
and the right hand is 1 m forward, so the player is less than 100 units from
him on his way to the step of the turn. With `script_view=1` the Keeper did
not start his walk in three runs, also when an eye picture showed him in the
view, about 30 degrees below the middle of the view of a level head. With
`script_view=0` he walks, and after his fourth point a snap turn ends the
step of the turn (the run starts `stick_right` each 150 frames):

```
comfort      : the camera controls of the game are 0x4A (1 tilt, 2 turn, 4 zoom, 8 drag, 0x10 jump). [...]
scripts      : a script asked if the rotate arrow shows. The game said 0x0, and the script gets 0x2: the view turned in the last two game turns (script_camera_moves=1, camera controls 0x4A), frame 8072
scripts      : a script asked for the hand state. The game said 0x3, and the script gets 0x14: the view turned in the last two game turns, which is the hold of the Move Button at the rotate arrow (script_camera_moves=1, [...])
cut scene    : frame 8550: the cinema bars are up, and a story camera owns the view
```

The story after "That's it. Now pull the mouse back." starts at frame 8550.
With `script_camera_moves=0` the same recipe comes to level 0x4A at frame
7939, and 61 snap turns in the next 300 s do not end the step: the Keeper
waits for the rotate arrow of the mouse.

## 8. Fog, detail and the near plane

`TabletopApplyFrame` writes the values of `tabletop.md` ("What the engine does
at the table scale"), but they follow the scale. The gain is `s / 7.5`.

| Value | Address | In the God view |
| --- | --- | --- |
| Map objects stop | `0x00C37200` | the game's 700 times the gain, times the factor of `cull_distance`, at most 8000. The factor counts at each scale and in the frames that the view does not own |
| Size cap of the model detail | `0x00C37EC8` | 5 times the gain, at most 30 |
| Land detail lines | `0x00E9C4FC`, `0x00E9C4D4` | 1600 and 2500 times the gain |
| Land texture line | `0x00E9C524` | the game's value times the gain, unless `table_textures=0` |
| The Hand's reach | `CHand+0x4838` | 1800 times the gain, at most 5600 |
| Fog | `0x00C37204` | the game's below a gain of 3, off above it. Below a gain of 3, `cull_fog=1` moves the fog start and end out as far as `cull_distance` moves the map object distance out |
| Reflection | `0x00E9CD8C` | the game's below a gain of 3, off above it |

At the life size scale and below it every value is the game's own: a value
never goes below the game's. Above it a distance in the room stays the same
at every zoom. The fog ends at 900 units, which is 40 m of the
room at a gain of 3 (22.5 units per meter). Past that the fog would cover the
island, so it goes off, as on the table.

Below the life size scale each value is the game's own, so the map objects stop
at 700 units and the fog ends at 900. At the nearest zoom (1 unit per meter) that is 700 m
and 900 m of the room. `cull_distance` (default 5x, 3500 units) moves both out,
so a view high over the land at a small scale shows the far land and its
objects (`backend.md`, "Models are drawn further out").

**The near plane.** `LandFeature::GetNearClipping` `0x005E2F30` gives 3.5
units for a camera 20 units or more over the land. At the life size scale that
is 47 cm, and it would cut away a real hand in front of the eyes. A hook on
it (`near_plane.cpp`) gives at most 5 cm of the room, `0.05 * s`, with a
24 bit Z buffer. With a 16 bit Z buffer it gives at most 10 cm, `0.10 * s`.
The game goes down to 0.3 units near the ground itself, and 0.375 at 7.5
units per meter is inside that range. Above 70 units per meter (35 at 16
bit) the game's 3.5 is nearer, and it stays.

| Scale (units per meter) | Near plane, 24 bit | Near plane, 16 bit |
| --- | --- | --- |
| 1 (the lowest scale) | 0.05 units, 5 cm | 0.1 units, 10 cm |
| 7.5 (life size) | 0.375 units, 5 cm | 0.75 units, 10 cm |
| 22.5 | 1.125 units, 5 cm | 2.25 units, 10 cm |
| 35 | 1.75 units, 5 cm | 3.5 units, the game's |
| 70 and more | 3.5 units, the game's | 3.5 units, the game's |

The 24 bit Z buffer (`depth24=1`, `backend.md`, "The depth buffer") is what
makes the near plane of 5 cm cheap. At every scale its depth steps are at
least 27 times finer than those of a 16 bit buffer with the game's 3.5
units. Zoomed out at 4000 units per meter, a step at the far edge of the
island goes from 39 mm of the room to 0.15 mm. A 16 bit step is the likely
cause of a far flicker.

When a story owns the view, the God view stands aside, and the camera is at
the life size scale. The same hook then gives 5 cm (10 cm at 16 bit) at the
life size scale. The table has it at the table scale, where the game's value
stays. The interface depth does not change with the near plane (`backend.md`,
"The depth buffer").

## 9. Sound and the spirits

The ears are the head (`audio.md`). The distances of the positional sounds
are room distances: QMixer gets each distance mapping of the game with min
and max times `s / audio_units_per_meter` (40). So one meter in the room
sounds the same at every zoom, and a sound at the hand is clear. A positional
sound with no object, which the game puts at its camera, is at the ears. A
bird or a wave of the atmosphere stays where it started in the room, and does
not turn with the head.

The mod mixes each sound with a place itself, for two ears and with its
height (`audio.md`). A sound on the land at the waist comes from below. The
bed of the atmosphere stays fixed in the world when the view or the head
turns. A zoom step changes how far a sound carries: far out, small sounds go
away and loud events stay. A turn step, a zoom step and a teleport have a soft
sound of their own.

The head also decides which positional sounds start and stop
(`audio_range=1`, `audio.md`). The game made these tests with its camera,
which is 400 units from the player's place. With `audio_range=1` a sound
plays while it is in range of the head, at each zoom, and a loop out of range
stays silent and ready.

With `audio_room=0` the ears go straight down from the head to at most 400
units over the land under it (`TabletopEars`), because the game's world
distances make every sound silent farther from the land.

The spirits float beside the player in the room, as on the table
(`tabletop.md`, "The spirits beside the table"). Their size and depth are
`s / 7.5` times the game's, so they float 1 to 1.5 m away at every zoom. A
step of the view moves `P0` in the world, so the spirits stay where they are
in the room.

A script can send a spirit to a place of the world, for example to the
scroll of a Creature lesson. The game makes the spirit 4 times as large
there, which is 73 cm in the room. With `spirit_world_size=1` (default) the
spirit at the place has the angle of size that it has at its near depth,
about 9 degrees (`tabletop.md`, "A spirit at a place of the world"). With
`test_keys=1`, Shift and Home change the key.

## 10. Buttons and keys

| Input | In the God view |
| --- | --- |
| Right stick forward or back | Zoom out or in one step (`god_zoom_forward_in=1` turns the direction), about the land that the head looks at (`god_zoom_about`). With `god_zoom_smooth=1`, zoom while the stick is held |
| Right stick left or right | Turn one step about the head |
| Left stick | Walk over the land, or fly with `move_type=1`, while it is pushed (`move_stick`, section 5.7) |
| Left grip | Grab the land: pull to move it across, and up or down |
| Y (left controller) | Hold it for the ring menu, and move the hand to an item (`controllers.md`, section 10) |
| X (left controller) | No job |
| The left menu button | Esc: open the game menu (see below) |
| Both grips | Turn and scale the land |
| Right grip, trigger, B | Pick up, click, draw a gesture. A trigger drag on the land moves nothing |
| A | Hold to aim an arc and a ring. Let go to go there. B while A is down cancels (section 14) |
| Mouse: double click | Go to the land under the mouse (section 14) |
| Mouse: left drag on the land | Pan: the land under the mouse stays under it (section 5.3) |
| Mouse wheel | One zoom step a click |
| Tilt keys | Ground up or down 5 cm, one step for each press |
| Turn keys, middle mouse | One turn step |
| Ctrl F11 | The God view off and on. Off is the immersive view |
| F11 | Recenter: the waist point goes under your head again |

For a left handed player (the left controller is the Hand, `controllers.md`,
5.6) the hands change sides:
the pointing hand's stick zooms and turns, the other hand's grip moves the
land, across and up and down, and the other hand's stick moves the player.
The right hand then has the ring on B, and the draw button of the left hand
is Y. The table has the layout of `button_layout=1`. `controllers.md`, 5.1,
has the layout of `button_layout=0`, with Y for Esc and X for the ring.

**The menu button.** The menu button of the left controller sends the game's
Esc key. The pump sends WM_KEYDOWN and WM_KEYUP to the game window (scan code
1), one press for one Esc. The game opens its menu. The menu does not close
with Esc again: `MainMenu::CanESCOut` returns false, so the player closes it
with "Continue Game". A Quest gives a program no menu button of the right
controller, so a left handed player has the same button, on the controller of
the pointing hand (`controllers.md`, 3.6).

**The menu ray.** While a dialog box is up (`SetupBox::GetCurrentActiveBox`
`0x00407ED0`, which reads `0x00C4CD20`), the direct hand steps aside. The
pointing hand points like a mouse: its aim ray picks the render pixel (the
pointer mode of `controller_hand=0`), and the trigger clicks. A warm white
ray leaves the aim pose and shows where it points. With `ray_glow=1` and
`ray_glow_only=1` (the defaults) it is a glow of 0.96 cm of the room from the
middle to the edge, with a solid core and a light that runs along the ray
from the hand. With `ray_glow_only=0` it is a thin tube (2 mm) under a glow
of 1.2 cm, and with `ray_glow=0` the thin tube alone. It ends on the
cursor in both eyes (see "The depth of the menu" in `controllers.md`), and
its end is a round soft glow (`ray_tip=1`, `hand-model.md`, "The glow at the
end of the menu ray"). When the
box closes, the direct hand comes back on the same frame.

### Ini keys

All in `[bwvr]`. The launcher has the grab row, the two rows
of the zoom and the row of the walk with the stick. The keys of the teleport
are in section 14.6, the keys of the grab are in section 5.6, the keys of
the walk with the stick are in section 5.7, and the keys of the comfort of a
move (the vignette, the stick in a dialog box and the recenter hold) are in
section 5.8.

| Key | Default | Meaning |
| --- | --- | --- |
| `play_mode` | 4 | 4 is the God view. 0 gives the God view too. 1 is the stereoscopic window, and 3 is the table |
| `god_ground_down_cm` | 60 | The ground below the head at the last recenter, 10 to 200 |
| `god_units_per_meter` | 100 | The scale at the first start |
| `god_zoom_step_pct` | 50 | One zoom step changes the scale by this percent, 5 to 300 |
| `god_zoom_smooth` | 0 | The zoom of the stick (section 2). 0: one step for each push. 1: the smooth zoom, which changes the scale while the stick is held. The launcher row "Zoom with the stick". Ctrl, Shift and Insert change it with `test_keys=1` |
| `god_zoom_smooth_ms` | 500 | The top rate of the smooth zoom: the time in which a full push changes the scale by one zoom step, 100 to 5000 |
| `god_zoom_test_toggle` | 0 | A test with no key press: at this frame of the controllers the test key of `god_zoom_smooth` acts. 0 is never |
| `god_zoom_about` | 1 | The point that a zoom keeps at its place in the room (section 2). 1: the land point that the head looks at. 0: the land point under the head. 2: the Hand's point. The launcher row "Center of the zoom". Ctrl, Shift and Delete change it with `test_keys=1` |
| `god_zoom_about_far_cm` | 300 | With `god_zoom_about=1`, the point is at most this far from the head, 50 to 1000 |
| `god_zoom_about_test` | 0 | A test with no key press: at this frame of the controllers the test key of `god_zoom_about` acts. 0 is never |
| `god_ground_step_cm` | 5 | One ground step, 1 to 50 |
| `god_far_units_per_meter` | 4000 | The farthest zoom, 10 to 8000 |
| `god_zoom_solid_stop` | 1 | 1: a zoom step stops `god_zoom_solid_cm` before the head goes into a house, a rock or a tree trunk, or it is refused (section 2, "Houses at a small scale"). 0: a zoom step can put the head into a house |
| `god_zoom_solid_cm` | 30 | How far out of the solid the head stays after a zoom step, in cm of the room, 5 to 200. 5 stops the step where the overlay of the head blinder starts |
| `god_near_units_per_meter_x10` | 10 | The nearest zoom, in tenths of a unit per meter (section 2, "The limits"). 10: 1 unit per meter, where a villager is as high as the player. 75: the life size scale. 0: the life size scale of `head_units_per_meter_x10` |
| `god_spawn` | 1 | 1: the player goes where the game puts its own camera (section 15). 0: the view is built at the start, a load and the end of a story only, with the focus under the recenter pose |
| `god_spawn_ahead_cm` | 50 | How far the land point that the game camera looks at is in front of the head, 0 to 150 |
| `god_camera_at` | 1 | 1: the game camera is over the land point under the head. 0: over the recenter place (section 7). Ctrl and Numpad 1 change it with `test_keys=1` |
| `god_zone` | 1 | 1: in a camera zone of a script the game camera stays in the zone, the player's place goes at most `god_zone_margin` past it, and the view does not shake (section 17). 0: the game pushes its camera back into the zone, with its sound and its shake. The debug page has the switch "Camera zone" |
| `god_zone_margin` | 50 | How far the player's place goes past the camera zone, in world units, 0 to 2000 (section 17) |
| `god_turn_keeps_focus` | 1 | 1: a snap turn alone turns the game camera about its own focus (section 7). 0: it puts the game camera over the land under the head |
| `snap_turn` | 2 | The turn (section 4). 1, 2, 3: a snap turn of 30, 45 or 90 degrees. 4: the smooth turn of the God view. 0: off (bwvr.ini only, the launcher shows the default for it). The launcher row "Turn" |
| `smooth_turn_speed` | 90 | The degrees a second of the smooth turn at a full push of the stick, 10 to 360 (section 4). The settings window has the row "Turn speed" (45, 90, 135 and 180), and the launcher has no row for it |
| `tilt_help_degrees` | 3 | A look of the head up or down at 20 degrees a second or faster counts as the game's tilt: one tilt event of the help for each so many degrees, one in each game turn (section 7, "The tutorial"). 0: the head sends no tilt events |
| `turn_help_degrees` | 5 | A snap turn counts as one turn event of the help for each so many degrees, one in each game turn, so the turn lesson of the tutorial ends with one full turn (section 7, "The turn lesson"). 0: one event for a snap turn |
| `script_distance` | 1 | 1: a script that measures the distance from the camera gets the distance from the player (section 16). 0: the game's camera |
| `script_view` | 1 | 1: a script that asks if the camera sees a thing gets the view of the head (section 16). 0: the game's camera |
| `script_camera_moves` | 1 | 1: a script that asks how the player holds the land, or if the rotate arrow shows, gets the moves of the God view: a zoom is the hold of both buttons, a turn is the hold at the rotate arrow (section 16.6). 0: the game's answers |
| `cutscene_window` | 1 | 1 plays a cut scene on a screen in the room (`cutscene-window.md`). 0 plays it in the full view with held shots |
| `depth24` | 1 | 1 asks for a 24 bit Z buffer (section 8) |
| `near_plane_cm` | 5 | The near plane at most this far in the room, with a 24 bit Z buffer. 0 keeps the game's |
| `near_plane_cm_16` | 10 | The same, with a 16 bit Z buffer |

The test words of the fake controller, with `fake_controller=2`: `x` and `y`,
the face buttons of the left hand, `lmenu`, its menu button, `lgrip` and
`lpoint`, the grip and the points of the left hand
(`fake_controller_left_points`), `lstick_left`, `lstick_right`, `lstick_up`
and `lstick_down`, the stick of the left hand, and `movekey`, the test key of
`move_stick`. The test key `fake_controller_stick_pct` (100) sets how far
the words of the right stick push it, in percent of a full push, so a run
can push the stick by half for the smooth turn.

**The test mouse.** A desktop run must not move the real mouse. So
`fake_mouse_script` moves the pointer and presses the left button inside the
mod (`pointer_map.cpp`, `FakeMouseFrame`). Its words have the form
`word@frame+frames`, as in `fake_controller_script`. The frames count the
frames with a drawn scene, from 0.

| Word | What it does |
| --- | --- |
| `home` | The pointer goes to `fake_mouse_x_pct` and `fake_mouse_y_pct` of the render (50 and 50), on the first frame |
| `home2` | The same, to `fake_mouse_x2_pct` and `fake_mouse_y2_pct` (50 and 70) |
| `button` | The left button is down for the frames, through `LHMouse::SetButtons` |
| `double` | Down, up, double click and up, one step on each of 4 frames, through `LHMouse::SetButtons` |
| `move_right`, `move_left`, `move_up`, `move_down` | The pointer moves `fake_mouse_step_px` render pixels (8) on each frame, as a move of the real mouse |
| `wheel_up`, `wheel_down` | One click of the wheel, on the first frame |
| `tilt_up`, `tilt_down` | The tilt key, a tilt of 1 or -1 at `0x00C5B0C4`, for the frames |
| `turn_left`, `turn_right` | The turn key, a turn of 1 or -1 at `0x00C5B0C8`, for the frames |
| `god` | Ctrl F11, on the first frame |

The launcher removes these keys from `bwvr.ini` when it saves.

## 11. The code

| File | What it holds |
| --- | --- |
| `src/mod/table_math.h` | `GodRoom`, `PoseWorld`, `PoseRoom`, `GroundInRoom`, `ScaleAbout`, `GodZoomAbout`, `GodZoom`, `GodRaise`, `GodTurn`, `GameCameraOver`, `RayLevel`, `RayLand`, `GodSpawn`, `CameraLookPoint` and `HeadYaw`, as pure functions |
| `src/mod/zoom_math.h` | The smooth zoom: the share of the top rate for a push (`StickShare`), the time of a frame (`FrameSeconds`) and the ratio of a frame (`FrameRatio`). The point of a zoom: its place across the room for each value of `god_zoom_about` (`PointAcross`), the level (`ZoomLevel`), the point in the world (`PivotAt`) and the rule for a hill at the new place of the head (`ClearHead`). All are pure functions |
| `src/mod/walk_math.h` | The walk with the stick: the push (`Push`), the forward direction of the head and of the controller (`HeadForward`, `AimForward`), the speed in the room (`Speed`), the step of a frame (`Step`), the view after a step (`Walk`), the filter of the land that the view follows (`FollowStart`, `FollowStep`), and the fly: the key (`Type`), the kinds of the debug switch (`Kind`, `NextKind`), the level right (`FlyRight`), the speed and the step in 3D (`FlySpeed`, `FlyStep`) and the view after a step (`Fly`). All are pure functions |
| `src/mod/tabletop.cpp`, `tabletop.h` | The God view in the table state: the place at a game camera (`PlaceGod`, `ApplyGodSpawn`), the steps (`ApplyGodSteps`), the point of a zoom (`GodZoomPoint`, `ZoomClearHead`), the smooth zoom (`TabletopGodZoomHold`, `ApplyGodZoomHold`), the walk with the stick (`TabletopGodWalk`, `ApplyGodWalk`), the mouse pan (`MousePan`), the camera switches and events, the hook on `GGame::ClearMap`, the values that follow the scale, the Hand's size, the ears and the game camera it wants |
| `src/mod/camera_zone.cpp`, `camera_zone.h`, `camera_zone_math.h` | The camera zone of a script (section 17): the read of the zone of the game, the limit of a move (`LimitMove`), the game camera in the zone (`KeepCamera`, `PointInside`), the bump for the sound (`BumpPoint`), and the hook on the shake of the push |
| `src/mod/near_plane.cpp` | The near plane hook, for the God view, the table and the immersive view |
| `src/mod/teleport.cpp`, `teleport.h`, `aim_smooth.h` | The teleport: the states, the arc and the hit on the CPU, the refusals, the mouse double click, the aim filter, the log (section 14) |
| `src/mod/comfort_camera.cpp` | The game camera write (`GodAfter`), the watch of the game's own camera moves (`WatchBefore`, `WatchAfterGame`, `WatchEnd`), the turn, the wheel, the tilt keys (`GodTiltKeys`), the game camera after a land drag (`GodDragBefore`, `GodDragAfter`), the write of the grab on the game camera (`ComfortCameraGrabMove`) and the log of the camera events (`h_Help`) |
| `src/mod/pointer_map.cpp` | The mouse ray in the room (`PointerMapMouseRay`), the game's land drag hooks and the test mouse |
| `src/mod/world_grab.cpp` | The grab: the state machine, the table solve with its ground limits (`TableSolve`, `TabletopGodLimitHeight`), the camera switches for a turn and a zoom of the grab, and the solve on the game camera |
| `src/mod/controllers.cpp` | The sticks (`PumpStick`, and `PumpWalk` for the walk), the layout of the buttons (`ButtonLayout`), the menu button (`PumpEsc`), the pointer mode while a dialog box is up (`DirectHandNow`, `ControllersMenuRay`), the grip points and the fake left hand |
| `src/mod/script_camera.cpp` | `ScriptCameraStoryOwns`, and `ScriptCameraOwnerNow` for the scripts |
| `src/mod/hand_direct.cpp`, `near_math.h` | The player that a script sees: the hooks on the six script words, the latch of the player and the test of section 16.4 (`ScriptPlayerNow`, `PlayerDistance`, `PlayerIsCamera`, `HeadSees`), and the camera moves of section 16.6 (`ScriptMoveAnswer`, `ScriptHandState`, `ScriptArrows`, `HelpRecent`) |
| `src/mod/cutscene_window.cpp`, `cutscene_window_math.h` | The cut scene window: the signal, the state of each frame, the screen and the walls (`cutscene-window.md`) |
| `src/mod/mod_main.cpp`, `src/launcher/launcher.cpp` | The play modes, the default, and the keys |

## 12. Tests

**The math test** (`bwvr/test/controller_math/`, run by `.\dev.ps1 build`)
checks the pure functions at 7.5, 100 and 2000 units per meter:

- The recenter pose is 60 cm of the room over the waist point.
- The Hand is 19 cm long at every scale.
- A real hand 60 cm under the recenter pose is at the waist height, and a head
  that leans and turns with a still hand moves it by nothing.
- A zoom step in and out keeps the land under the head at its room point and
  at its height in the room, keeps the heading, and does nothing at a limit.
  A hill over the head comes under it after 14 zoom steps out.
- The smooth zoom (`zoom_checks.h`): a stick at the dead zone asks for no
  zoom, a stick at full asks for the top rate, and a larger push never asks
  for less. A stick at rest and a frame of no time give a ratio of exactly 1.
  A full push for 0.5 s is one step, at 90 and at 30 frames a second. A frame
  of 1 s counts as 50 ms. On each frame of a hold the land point under the
  head keeps its room point, and the heading stays. After the stick is back
  the view does not change in 30 frames. A hold stops at each limit and never
  passes it, and a zoom step is the zoom about the land point under the head.
- The point of a zoom (`zoom_checks.h`), with a head 30 cm right and 40 cm
  forward of the recenter place: the direction of a head from the real
  `head_pose.cpp`, the level, and the rule for a hill at the new place of the
  head. `god_zoom_about=0` gives the point under the head. With 1, a look 20
  degrees down at flat land gives the land point on the line of the look,
  1.511 m across from the head. A hill in front of the flat land is found
  first. A look at the sky, a level look and a look straight up give the
  point of the line at 3 m. A line that meets the land at 2.99 m and a line
  that meets it at 3.01 m give points 1 cm apart. A head with no direction
  gives the point under the head. With 2, the Hand's point is the point, and
  with no Hand it is the point under the head. A step in and a step out about
  the looked-at land and about the Hand's point keep that point at its room
  point within 0.01 cm. The land of the level keeps its height, the land
  under the head moves a half and a third of the distance to the point, and
  the step back puts it under the head again. The point of a hold keeps its
  room point while the head turns 9 degrees.
- The step that stops short of a solid (`zoom_checks.h`, `StepShareChecks`):
  a clearance that goes from 30 cm to -20 cm stops the step at its half, a
  step with no solid in the way goes all the way, and a head nearer than
  the limit may go away from the solid but not nearer. The walls of a house
  (`solid_checks.h`): an inset of 20 percent makes the half width of a box of
  10 units 8 units and keeps its height, a head under the eave is out of the
  walls, a head deep in the house is still in them, a trunk does not change,
  and the inset is at most a half.
- The lowest scale (`zoom_checks.h`, `LowestChecks`): the default key gives
  1 unit per meter, 75 and 0 give 7.5, and a villager of 1.6 units is 1.6 m
  high there. 12 steps in from 100 stop at 1, 5 steps out from 1 give 7.59,
  a key of 75 stops a step in at 7.5, and a step to the lowest scale keeps
  the point of the zoom in the room.
- A ground step raises every land point 5 cm in the room and not across, and a
  step down puts it back.
- A snap turn keeps the head's world point, and a turn of 90 degrees right
  puts the land that was on the right in front.
- A pan keeps the grabbed land in the hand, moves the land as far as the hand,
  and keeps the waist height. Without the vertical grab, a hand that moves up
  moves no land.
- The vertical grab (`GodGroundHeight`, `TableGrabSolve` with `vertical`): a
  hand 20 cm up raises the land 20 cm in the room and moves no land across,
  a hand 20 cm down lowers it 20 cm, a still hand moves nothing, a move
  across and up keeps the same move across, and two hands keep the height of
  the press. For the limits: a request inside the range is used, a request
  past 10 cm or 2.5 m stops there, a sweep through both limits never leaves
  the range and never moves more than the request, a land outside the range
  only moves toward it, and a request that comes back into the range is used
  at once.
- Two hands that spread to twice the distance halve the scale, down to life
  size, and keep the land under their middle.
- The mouse pan (`GodPanChecks`): the ray of the press finds the land at the
  height of the waist point in the room, on the ray. After the pointer turns,
  the grabbed land is on the new ray, within 0.0003 cm, at the point where the
  ray meets the level plane, and the scale, the heading and the height of the
  waist point stay. A still mouse moves no land. A hill in front of flat land
  is found first. A ray that goes up, a ray that meets the plane past the
  Hand's reach, and a level ray find nothing.
- The game camera is 400 units from its ground point, 50 degrees over it,
  along the heading.
- The walk with the stick (`walk_checks.h`): the values of the key and the
  order of the test key. A stick in the dead zone gives no push, a full push
  gives the top speed in each direction, and a diagonal is not faster. The
  forward direction of a head that is level, turned, turned with a look down,
  and straight down, and no direction for a head on its side. The forward
  direction of a controller that points down by 30 and by 75 degrees, and no
  direction at 80 degrees and straight down. Forward, back and to the side
  on the stick with the head 30 degrees left and right. One second of a full
  push is 1.5 m at 90 and at 45 frames a second, a frame of 0.5 s counts as
  50 ms, and a stick that is back gives no step. A step puts the land that
  was ahead under the head, keeps the height, the heading and the scale, is
  the pan of a grab with one hand, and is 400 units at 4000 units for each
  meter and 0.75 units at life size. A hill at the new place goes 10 cm below
  the head. The follow of the land (`FollowStep`): a step over flat land and
  a frame with no step move nothing, also over a hill. A step of 10 cm onto
  higher land gives the exact answer of two stages, and ten steps of 1 cm
  give the same. On a long hill the view rises with the land, 50 cm of the
  walk behind it. A bump of 10 cm moves the view by less than 15 percent of
  its height, and 80 cm later the view is back within 5 percent.
- The fly with the stick (`walk_checks.h`, `FlyChecks`): `move_type` 1 is
  the fly and each other value the walk, and the debug switch goes from the
  walk to the level walk, to the fly and back. With a look 45 degrees down a
  full push forward goes down and ahead at the top speed, a push to the side
  is level, and a push back goes up. A look straight down strafes along the
  right of the head and goes straight down. A controller 60 degrees up goes
  up. A frame with no time and a stick at rest give no step. A step of 10 cm
  down and 20 cm ahead at 100 units for each meter moves the waist point 10
  units down and 20 units ahead, also with a heading of 90 degrees. A fly up
  goes past 2.5 m
  and stops with the head 10000 units over the land, at 100 and at 1 unit
  for each meter. A view over the ceiling stays, cannot go up, and can come
  down. The speed ramp: a push of 90 percent
  or more counts its time and a push of 85 percent sets it to 0. The factor
  is 1 in the first second, 2 after 3 s, 4 after 5 s and 32 from 11 s, and
  a most of 1 is no ramp. At 1 unit for each meter and 60 frames a second,
  1000 units take 29 s. The floor of the fly: with the clearance of the
  head blinder a step to 8 cm over the land goes up 2 cm, a step to 50 cm
  stays, and a head bent down to 4 cm is held at 4 cm and not lifted out.
  A fly level into a slope of 45 degrees keeps the head at 10 cm and slides
  up along the slope.
- The teleport (14.8), the place at a game camera (15.9) and the player that a
  script sees (16.9) have their own checks.

**Desktop runs** use the rig of `backend.md` with `play_mode=4`,
`fake_controller=2`, `controller_hand=1`, `-NEWGAME` on Land 1, a level fake
head and the player's render size. Put the test keys under `[bwvr]`, and not
in the last section of `bwvr.ini`. Check the first lines of each log for the
render size, because the recipes depend on it.

- The start dialog needs a click on "Skip straight to the Creature select"
  and on OK. The dialog is a dialog box, so the fake hand points like a mouse
  there (the menu ray, section 10). Set the points `-14,2,100` and
  `-15,-11,100`, `fake_controller_yaw=8`, `fake_controller_pitch=1`,
  `fake_controller_sweep_yaw=8` and `fake_controller_sweep_pitch=-6`. The
  script is `point@400+1 trigger@440+8 pose2@500+1 point@520+1
  trigger@560+8`. At a render of 2880 by 2880 the aim gives the pixels 1298.6,
  1420.7 and 1296.2, 1547.9. The two eye pictures show the dialog about 48
  pixels apart, because of the interface depth, and the game's own place is
  the middle.
- The Creature select story holds the camera, and the God view stands aside
  until the pen.
- A zoom, ground and grab run at the pen: a log line shows each zoom step, the
  ground step ("the ground went 20.00 cm up in the room"), the limit ("the
  ground stops at its limit"), and a count of the frames in which the view
  moved with still hands (it must be 0).
- The menu button: the fake Y sends Esc ("the menu button sends Esc"), the box
  opens, and the game places the Hand while it is up. A second Y sends one
  more Esc and the menu stays, as `CanESCOut` says.
- The point of a zoom: `spawn_run.ps1 -Choice keep` with a head 30 cm right
  and 40 cm forward of the recenter place (`fake_head_right_cm`,
  `fake_head_forward_cm` and the word `lean@780+2400`),
  `god_zoom_about_test=1200` and the steps `stick_up@800+10 stick_down@860+10
  look@920+900 shot@960+1 stick_up@1000+10 shot@1040+1 stick_down@1080+10
  shot@1120+1 stick_up@1240+10 stick_down@1300+10`. The first step has a
  level head: the log must give the point of the line of the look at 3 m, and
  a move of the land under the head of 150 cm. The steps with the word `look`
  must give "the land that you look at", a point of the zoom that moved 0.00
  cm in the room and from the drawn head, and a move of the land under the
  head of half or a third of the distance to the point. The same thing is in
  the middle of the three eye pictures. After the test key the steps must
  give "the land under your head" and a move of 0.00 cm. For the Hand, set
  `god_zoom_about=2`, give a point of the hand with `-Points` and the word
  `point@900+1`, and step in and out: the log must give "the Hand's point"
  with the same world point for the two steps. For the smooth zoom, set
  `god_zoom_smooth=1` and hold `stick_up@1000+45 stick_down@1100+45` in a
  `look@920+520`, and `stick_up@1420+45` across the end of the look: each
  hold must give a point of the zoom that moved 0.00 cm in the room, also
  when the head turns back in the hold.
- The smooth zoom: `spawn_run.ps1 -Choice keep` with `god_zoom_smooth=1`,
  `god_zoom_about=0`,
  `god_zoom_test_toggle=1720`, a head 30 cm right and 40 cm forward of the
  recenter place (`fake_head_right_cm`, `fake_head_forward_cm` and the word
  `lean@780+1100`) and the steps `stick_up@800+45 stick_down@900+90
  stick_down@1040+10 stick_up@1100+40 stick_right@1110+10 stick_right@1180+10
  stick_left@1210+10 stick_up@1250+450 stick_down@1750+10 stick_down@1780+10`.
  Each hold must have its three log lines (section 2): the scale changes on
  each frame of the word, by 2 zoom steps a second, the land under the head
  and the drawn head move 0.00 cm, and the scale is the same 30 frames after
  the stop. The hold with `stick_right` stops when the two words are down
  together, and does not start again when `stick_right` is up. The two turns
  act. The long hold stops at 7.50 units per meter ("the zoom is at its
  limit"). After the test key the last two words are two zoom steps. The same
  steps with `god_zoom_smooth=0` give one zoom step for each word that
  starts with the stick at rest. The fake stick has full pushes only, so the
  rate of a small push has only the math test.

- The walk with the stick [R]: `spawn_run.ps1 -Save temple` with
  `move_stick_ramp=0`, `fake_controller_play_frame=800`, `fake_controller_yaw=30`,
  `fake_controller_sweep_yaw=30`, `fake_controller_sweep_pitch=-30`,
  `god_zoom_about=0` and the steps `look@830+1300 lstick_up@900+60
  lstick_right@1020+60 movekey@1130+1 lstick_up@1150+60 movekey@1260+1
  lstick_up@1280+60 movekey@1360+1 stick_up@1380+10 lstick_up@1440+60
  movekey@1540+1 lstick_up@1560+60 lstick_down@1680+60 lstick_left@1680+60
  lgrip@1790+90 lstick_up@1810+40 layoutkey@1920+1 lstick_up@1940+50`. The
  word `look` turns the head 30 degrees to the left, and the left controller
  points 30 degrees to the right. Each walk must have its three log lines
  (section 5.7). With `move_stick=1` the player goes 30.0 degrees to the left
  of straight ahead, and with 2 goes 30.0 degrees to the right. With 0 the
  log has no walk, and the next walk starts at the place where the last one
  stopped. A push to the right with the head as the direction goes 60.0
  degrees to the right. A push back and to the left with the controller as
  the direction goes 105.0 degrees to the left. Each walk moves the view on
  60 of 60 frames at 150.0 cm of the room each second, at 100 and at 66.67
  units for each meter: the distance in units is the time of the 60 frames
  times 1.5 m times the scale. 30 frames after each stop the waist point is
  0.000 units from its place at the stop, and the drawn head moved 0.00 cm.
  The push during the grab is refused with one log line. After the word
  `layoutkey` the stick opens the ring and moves nothing. The scripted stick
  has full pushes only, so the speed of a small push has only the math test.
- The follow of the land [R]: `spawn_run.ps1 -Save temple` with
  `move_stick_trace=320`, `move_stick_ramp=0` and the steps
  `lstick_up@900+300 lstick_right@1300+300 lstick_down@1700+200`, once with
  `move_stick_follow_land=1` and once with 0. The first walk goes 7.9 m of
  the room over the hills at the Temple, where the land under the head goes
  from 0 to 112 units (at 100 units for each meter), and the third walk is
  over the sea. In each walk of both runs each frame moves the view at
  149.93 to 150.07 cm of the room each second, with frames of 14.8 to 46.2
  ms. With `test_frame_rate=60` each frame is 16.7 ms, so the walks go
  7.5, 7.5 and 5.0 m.
  - With 1 and `test_frame_rate=60`: the land under the head is 61.6, 62.9
    and 61.6 cm below the head at the starts of the walks, and 62.9, 61.6
    and 61.6 cm at their ends. Each walk starts where the walk before it
    ended. Each trace line has "the speed times 1.00". In the hills it
    is 19 to 115 cm below the head (the filter is behind the land on the
    steep slopes), and the land never comes nearer than 10 cm. The height of
    the view changes by at most 2.5 cm in a frame, and its speed up or down
    changes by at most 14 cm each second from one frame to the next. Over
    the sea the height does not change. After each stop the waist point
    keeps its height in the report of each window.
  - With 0: the view keeps its height. The land comes to 10 cm below the
    head, and the limit puts it 32.8 cm down in the room. While the limit
    acts, the view goes up with each bump of the land at once: up to 5.5 cm
    in a frame, and its speed up or down changes by up to 181 cm each second
    from one frame to the next. After the hills the land is 108.2 cm below
    the head, and 122.3 cm after the second walk.
- The speed ramp [R]: `spawn_run.ps1 -Save temple` with
  `test_frame_rate=60`, `move_stick_trace=700`, `card_send_test_save=0`,
  `fake_controller_play_frame=800` and the steps `lstick_up@900+300
  lstick_half@1200+120 lstick_up@1320+180 lstick_up@1600+60`. For the first
  60 frames of the full push a step is 2.45 to 2.51 cm, "times 1.00", and
  1.01 at frame 61. Then the factor is 1.41 at frame 120, 2.00 at 180, 2.83
  at 240 and 4.00 at 300, with a step of 10.02 cm at 300. At the half push,
  in its first frame, the factor is 1.00 and the step 0.768 cm, with no stop
  line between the pushes. The next full push starts at 1.00 for 60 frames.
  The line of the end has "The speed ramp went up to 4.00 times the speed
  of the push". The last push starts again at 1.00 and stays at 1.00 for its
  60 frames.
- The fly with the stick [R]: `spawn_run.ps1 -Save temple` with
  `test_frame_rate=60`, `card_send_test_save=0`, `move_type=1`,
  `move_stick=1`, `move_stick_trace=700`, `fake_controller_play_frame=800`,
  `fake_head_pitch=-45`, `god_zoom_about=0`, `comfort_vignette=1` and the
  steps `lstick_up@900+240 lstick_down@1200+400 lstick_right@1700+60`. The
  line of the start has "45.0 degrees below level", and the vignette goes
  dark for "the fly with the stick" in each fly. The first fly goes down at
  45 degrees to the floor and on across. The second fly goes back and up:
  the land under the head goes past 250 cm below the head near fly frame
  130 and on to about 2000 cm, with the factor of the ramp from 1.00 to
  7.15 and no line of a limit. At about 20 m the camera zone of the script
  (`god_zone_margin=50`) holds the place across, and the fly goes on along
  the edge of the zone. The strafe has "0.000 cm up" in each frame. 30
  frames after each stop the waist point is 0.000 units from its place at
  the stop.
- The fly with the controller [R]: the same with `move_stick=2`,
  `fake_controller_sweep_pitch=40` (the word of the script sets the
  direction of the stick, not `fake_controller_pitch`), no
  `fake_head_pitch`, and the step `lstick_up@900+400`. The line of the start
  has "40.0 degrees above level". The land under the head goes from 62.9 to
  1895.4 cm below the head with each step up more than 0, and the factor is
  1.00 to fly frame 60, 2.83 at 240 and 7.13 at 400.
- The floor of the fly [R]: the fly with the stick above (the head 45
  degrees down) has no line of the head blinder. Each of its 135 trace lines
  of the first fly has a clearance of 10.00 cm or more, and where the slope
  rises at the floor the step up is more than 0 (42 lines, for example
  "0.633 cm up ... a clearance of 10.00 cm"). A level fly into the hills
  (`move_stick_ramp=0`, no `fake_head_pitch`, the step `lstick_up@900+300`)
  has no line of the head blinder. Over level land the steps are 2.49 to
  2.65 cm with "0.000 cm up", where the hill comes within 10 cm the steps
  go up ("a step of 4.597 cm of the room (3.855 cm up ...), a clearance of
  10.00 cm"), and past the top they are level again. The line of the end
  has "The floor of the fly lifted the view 37.3 cm on 18 frames, and the
  least clearance of your head after a step was 10.0 cm", and 150.0 cm of
  the room each second.

**The mouse pan and the tutorial events** need a run with the test mouse. Use
`play_mode=0` (it must log the God view), `-NEWGAME` on Land 1,
`fake_mouse_step_px=4`, home at 45 and 49 percent, and the second home at 50
and 70 percent. The words click the start dialog (`home@380+1 move_right@382+1
move_down@383+2 button@440+4 move_left@450+3 move_down@455+34 button@600+4`).
After the first story they drag the land (`home2@3000+1 button@3010+150
move_right@3030+50`), zoom, move the ground and turn (`wheel_up@3300+1
tilt_up@3400+5 tilt_down@3500+5 turn_left@3600+5`), and do the same with the
God view off (`god@3700+1 wheel_up@3800+1 home2@3900+1 button@3910+100
move_right@3930+15 turn_left@4100+5 god@4200+1`). The log must show the pan
("the mouse takes the land", "stayed within ... cm of the mouse ray"), no
move of the view after the release, the game camera put back after the drag,
and the camera events (0x304 for a wheel click and 0x308 for a land drag).

## 13. Open items and what needs the headset

- **Shown on the desktop:** a pick up, a set down and a throw in the God
  view, at 66.7, 100 and 225 units per meter (`hand_run.ps1` and
  `throw_queue.ps1` in `bwvr/tools/desk`).
- **Not tested on the desktop:** the two hand zoom by the grips has only the
  math test. An advisor goes to a place of the world in a desktop run only
  with a test (`tabletop.md`, "A spirit at a place of the world"). No desktop
  run has the sound.
- **The tutorial events** (section 7) have not run in the game. The
  numbers and the bits come from the binary. The run is in section 12.
- **The mouse** pans, zooms, turns and moves the ground. Its Hand still has
  the game's own size, which grows with the distance, and its throw has the
  speeds of the life size scale. These are in `hand_direct.cpp`.
- **A load** on the same land is not tested. Save, pan far away, and load:
  the view must come back at the place of the save, with the same scale. A
  load at the start of the game has a run: in a run with a test save
  (`tools/desk/README.md`, "A fast start") the view is built at the game
  camera of the saved game.
- **The tilt keys.** Which key of the game sets a tilt of more than 0 is not
  checked. If the keys move the ground the wrong way, change the sign in
  `GodTiltKeys`.
- **The grab on the game camera** (5.5): the game's own lift of the camera
  above the land inside `GCamera::Update` is not proved, and what
  `GetAltitude` returns outside the map is not known. A recenter and a grip
  that jumps 50 cm start a new anchor, and no run has shown it. What a right
  press on bare land does in the game, in each Hand state, decides whether
  the claim rule of `grab_hand=2` changes anything. A step "drag the land" of
  the tutorial waits for event 0x308, which a grab sends.
- **Not built for the grab:** a vignette while the camera moves, a cut for the
  advisor anchor (`SpiritAnchorCameraCut`) on a fast grab, and a hint text
  "Grab the world" (see `controller-hints.md`).
- **The smooth zoom** (section 2) is not the default. No run has shown
  a push with the zoom switch off, a small push, or the key Ctrl, Shift and
  Insert with a real key.
- **The point of the zoom** (section 2). A headset session compared the
  three points. The three stay as choices, and the land that the head looks
  at stays the default. The line of the look is the forward direction of the
  head, and the eyes of a player can look lower than that. No run has shown
  the Hand's point with no controller, a hill at the new place of the head,
  the mouse wheel with the look, or the key with a real key. These have the
  math test only.
- **The walk with the stick** (section 5.7). With `god_zone=0` a walk
  (`village_run.ps1 -Kind walk`) stops at the edge of the map at x 5120 and
  slides along it. No run has shown a small push, a hill within 10 cm of the
  head, a push with the switch for the drag off, a controller that points
  straight up or down, or the test key with a real key. These have the math
  test or no test.
- **The tutorial** (section 7, "The steps of the tutorial"). A desktop run
  and the log of a session in the headset show the tutorial of Land 1 from
  the walk to the village to the texts after the zoom lesson. On the walk to
  the village and on the way to the Temple the game has the turn and the zoom
  off, so the snap turn and the zoom do nothing there, and the player turns
  the head or the body. No run and no session has the end of the build of
  the Temple, the missionaries, the lessons of Land 2 (levels 8 and 12) and
  the lessons of the Gods' Playground after the step of the turn. With
  `script_view=1` the Island Keeper did not start his first walk in the
  desktop runs (section 7).

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

## 14. The teleport

Hold A to see where the teleport goes, and let go to go there. The code is in
`teleport.cpp`. The commands of the Creature are in `creature.md`.

**What the player does.** Hold A. An arc leaves the pointing hand and falls on
the land. A ring on the land shows the target. Let go of A to go there. The
arc and the ring are cyan when the target is valid. They are red when it is
not. B, while A is down, cancels the teleport and starts no gesture.

**What A does in each play mode.**

| View | Arc | The move on release |
| --- | --- | --- |
| God view (play mode 4) | A parabola | The head goes over the target. The land under the head keeps its height in the room. The scale and the heading stay |
| Table (play mode 3) | A straight ray | The target goes to the table center |
| Immersive view (Ctrl F11 off, with no story) | A parabola | The eye goes over the target at the same height over the land. The heading stays |
| The mouse, in the God view | None | A double click goes to the land under the mouse |

Each move is one cut. With `blink` above 0 a short black blink covers it
(`blink` is 0 by default). Nothing moves after it. The
teleport never glides. With `teleport=0` the A button sends the game's own
jump.

### 14.1 The press

A press of A is refused, with one buzz of 30 ms and no arc, when:

- a grip holds the world,
- a gesture is being drawn,
- a story or a follow camera owns the camera,
- a dialog box is up (`SetupBox::GetCurrentActiveBox` `0x00407ED0` reads
  `0x00C4CD20`),
- the camera input is off (`mode+0x2EC`), or there is a fight (`mode+0x44`),
- the game's switch for the drag of the land is off: bit 8 of
  `CameraHelp::EnabledFeatures` (section 7),
- the hand cannot act (a menu, or the game is not in front), or the aim pose
  is not tracked.

The log gives the reason. A reason that starts while the player aims cancels
the aim, with two ticks.

**The switch of the game** (`teleport_switch`, 1). The teleport moves the
player over the land, as the walk with the stick does (section 5.7). So the
two have one rule: the press of A and the push of the stick keep to the same
switch of the game, bit 8. When a script turns that bit off, the game holds
the player in place, and it refuses both. The first step of the tutorial has
bit 8 only (section 7, "The walk to the village"), and there the player goes
to the village with the teleport or with the stick. With `teleport_switch=0`
the press of A keeps to the switch of the double click jump, bit 0x10, which
the tutorial turns on after the village. `TeleportPossible` says whether a
press of A can move the player now, and the hint card shows the line of A
only then (`controller-hints.md`, 9.4).

### 14.2 The arc

The arc is a parabola in meters of the room, from the aim pose of the pointing
hand:

```
p(t) = o + v0 * d * t + 0.5 * (0, -g, 0) * t^2     v0 = sqrt(g R)
```

`g` is 9.8 and `R` is `teleport_reach_m` (10). `R` is the flat range of a 45
degree aim. There are 48 steps of equal time, so 49 points. The arc ends when
it falls 0.5 m under the height of the sea in the room, or after 3 s. An aim
more than 60 degrees up is the sky, and there is no arc.

The points go into the world with the map of the grip. It is the map that
`HandToWorld` uses: `world = base + map * room`. In the God view and on the
table that is `P0 + Lt * s * room`. In the immersive view it is the eye plus
the room basis. So the shape of the arc is the same at every zoom: the reach in
the world is `R * s`. It is 75 units at 7.5 units a meter, 1000 at 100, and
40000 at 4000. At 4000 the island is 1.3 m across, so a downward aim lands
within arm's reach.

The land hit is on the first segment of the polyline whose end is at or under
the land. Eight halvings of that segment find the point. So the hit is on the
tube that the player sees. The land height is `TeleportGround`: the height of
`GetAltitude` on the map, and 0 for the sea and for a point off the map. The
arc makes at most 56 lookups a frame, and the ring makes 72. A hand that is
already under the land has a hit at the hand. On the table the ray goes in
steps of 2 cm of the room (`RayLand`).

### 14.3 A valid target

A target is valid when all of these are true:

- It is on the map (0 to 5120).
- Its land height is more than 0. The sea is red.
- In the God view it is within 3000 units of the map center
  (`kGodGameRadius`). In the immersive view it is within 3400 units, which is
  inside the pull back of `GCamera::Update` at 3500.
- It is more than 30 cm of the room from the point under the head. A nearer
  target is red, because there is nothing to do.

Release on a red target, or with no hit, and nothing happens. The hand gives
two ticks of 0.2 for 30 ms. A move gives one pulse of 0.4 for 60 ms.

### 14.4 Going there

**The God view.** `TabletopGodTeleport` keeps the target. The next main camera
call (`ApplyGodSteps`) makes the move as one step: `GodTeleport`
(`table_math.h`), `ClampMap`, `Derive`, one more `g_god_version`, the blink,
and the help event. `GodAfter` then puts the game camera over the new place,
once. `GodTeleport` gives:

```
H   = the head in the world
r0  = the room height of the land under H, kept 0.10 m to 2.5 m under the head
G'  = (G.x + T.x - H.x, ground(T) - (r0 - Tg.y) * s, G.z + T.z - H.z)
```

The land under the head keeps its height in the room. That height is also
the level of a zoom (section 2). If the
waist point kept its height, a hill could come up through the head. The head
goes over `T` within 0.01 units.

**The table.** `TabletopTableTeleport` puts the target at the table center
(height 0). The scale and the heading stay.

**The immersive view.** `ComfortCameraTeleport` keeps the move
`(T.x - E.x, ground(T) - ground(E), T.z - E.z)` (`CameraTeleportDelta`). After
the game's update of the camera mode, the six Zoomers, the copies of the mode,
its fall back focus and its pivot get that move. The heading stays. The move
is dropped, with a log line, while a script holds the input or the pivot,
while a fight holds the camera, or while a jump flies.

**The help event.** Each move sends the game's own event for a jump:
`CameraHelpCallback` `0x305` (to the land) or `0x306` (when an object is under
the Hand, `+0x3C8` of the interface). So a tutorial step that waits for a jump
can end.

**The mouse.** In the God view, `h_ModeUpdate` calls
`TeleportMouseDoubleClick` before the game's update. It acts when the latch
`0x00E85350` has the double click bit `0x10`, the mouse owns the pointer, and
the game would allow its own jump: the reasons of the game for the press of
A, with the jump switch (bit 0x10) for each value of `teleport_switch`. The
mouse ray from the head finds the land (`RayLand`). The bit is cleared, so the
game does not fly its camera. Then the God view goes there as for A. The mouse
teleport works with `teleport=0` too, because the game's jump moves only a
camera that the God view does not show. A double click on a villager goes to
the land under the mouse ray, and not to the object.

### 14.5 The drawing

`overlay.cpp` has a second source for the arc, with its own switch, so the arc
draws with `overlay=0` too. A tube of 6 sides and 6 mm of the room across, with
no cap, 576 triangles at most. A ring of 24 points that follow the land, 2 cm
over it. The inner radius is 0.75 of the outer radius, so the band has a
width of a quarter of the radius at each size. A valid ring has a disc in the
middle. A red ring has a cross. The meshes are opaque, with the depth test and
the depth write on, as the hands. The tube of the arc and the lift of the ring
are in meters of the room, so they look the same at every scale. With
`ray_glow=1` (the default) the arc has a glow ribbon with a light that runs
along the arc from the hand (`hand-model.md`, "The glow ribbons"). With
`ray_glow_only=1` (the default) the glow is the whole arc, 2 cm of the room
from the middle to the edge, with no tube. With `ray_glow_only=0` the tube
stays, and the glow over it is 2.5 cm. The ring has no glow.

**The size of the ring.** By default (`teleport_ring_mode=1`) the ring has one
size in the world: an outer radius of 11 units (`teleport_ring_units`). The
God view scale goes from 1 unit a meter to 4000, and the start
scale is 100. At the start scale the ring is 11 cm of the room. It gets
larger in the room when the player zooms in and smaller when the player zooms
out, but never smaller than 4 cm (`teleport_ring_min_cm`) and never larger
than 40 cm (`teleport_ring_max_cm`). So the ring says how large the place is
from 27.5 to 275 units a meter (about 3 zoom steps in and 4 steps out from
the start scale). Outside this range it has the limit: 40 cm at life size
(3 units), 40 cm at 1 unit a meter (0.4 units), and 4 cm at 4000 units a meter (160 units). The lift of 2 cm
stays in the room at each size. The rule is in `teleport_math.h`.
`teleport_ring_mode=0` gives a ring of one size in the room
(`teleport_ring_cm`, 11 by default). The overlay
clips them at the near plane of 5 cm (section 8). `kMaxTriangles` has 648 more
triangles for this.

**The smooth aim.** A hand in the air shakes a little, and the shake grows
with the length of the arc. So `TeleportFrame` filters the aim before it finds
the arc. `aim_smooth.h` has a one euro filter. It has a low cutoff when the
aim is still, so the ring does not shake. The cutoff grows with the speed of
the aim, so a real move shows at once. The aim direction (a unit vector, in
radians a second) has a cutoff of 0.9 Hz plus 6 Hz for each radian a second.
The aim point (meters of the room) has 1.5 Hz plus 8 Hz for each meter a
second. The filter starts again at each press of A. The release goes to the
target of the last drawn frame, so it goes to the smooth target. The report
line of the teleport shows the largest gap between the raw aim and the smooth
aim.

### 14.6 The keys

All in `[bwvr]`. They are ini only. The launcher has no row for them.

| Key | Default | Meaning |
| --- | --- | --- |
| `teleport` | 1 | 1 is the arc. 0 gives the game's own A jump |
| `teleport_reach_m` | 10 | The flat range of a 45 degree aim, 2 to 100 |
| `teleport_ring_mode` | 1 | 1 is a ring of one size in the world (`teleport_ring_units`), 0 is a ring of one size in the room (`teleport_ring_cm`) |
| `teleport_ring_units` | 11 | The outer radius of the ring in world units for mode 1, 1 to 1000 |
| `teleport_ring_min_cm` | 4 | The smallest outer radius in the room for mode 1, 1 or more |
| `teleport_ring_max_cm` | 40 | The largest outer radius in the room for mode 1 |
| `teleport_ring_cm` | 11 | The outer radius of the ring for mode 0, 5 to 60 |
| `teleport_switch` | 1 | The switch of the game that the press of A keeps to (section 14.1). 1: the drag of the land (bit 8), as the walk with the stick. 0: the double click jump (bit 0x10) |

With `test_keys=1`, Ctrl with Insert changes `teleport_ring_mode` between 1
and 0. The pointing controller gives a long pulse for 1 and a short pulse for
0. Ctrl and Shift with the comma key change `teleport_switch` between 1 and
0, with the same pulses. The word `teleportkey` of the scripted controller
does the same in a desktop run.

### 14.7 The log

The tag is `teleport`. The lines are:

```
teleport     : on. Hold A to aim an arc with a reach of 10 m, ...
teleport     : teleport_switch=1 (start). The press of A keeps to the game's switch for the drag of the land (bit 8), as the walk with the stick does
teleport     : A does not teleport now: a story owns the camera (teleport_switch=1), frame N
teleport     : the view and the game let A teleport now (teleport_switch=1), frame N
teleport     : frame N, God view, aim from x y z m in the room, 28.0 degrees up, 31 arc points, 100.0 units a meter
teleport     : hit land at x y z, the target is land. Valid (cyan), 56 land lookups this frame
teleport     : no hit: the aim is the sky, so there is no arc
teleport     : A is refused: a story owns the camera, frame N
teleport     : A is up, so go to x y z (God view), frame N. The eye is at ...
teleport     : the ring has a radius of 11.0 cm of the room and 11.0 units of the world, at 100.0 units a meter, rule 1
teleport     : the God view moved. The head went from ... to ... off. The land under the head was 0.600 m below it, and now it is 0.600 m. Scale 100.00 to 100.00, heading ..., version N.
teleport     : A is up on a red target: the target is the sea, so nothing happens, frame N
teleport     : the aim is cancelled: B was pressed, and it starts no gesture, frame N
teleport     : the mouse double click goes to x y z, the target is land
teleport     : N aims over N frames (N on a valid target, ...). N releases went, ...
teleport     : N land lookups, at most N in one frame. The arc was drawn in N eye passes with N triangles.
```

The ring line goes out once for each aim.
The aim line goes out on the first frame, on a change of the hit or of the
reason, and every 90 frames, 90 lines at most. The lines "A does not teleport
now" and "let A teleport now" go out on each change of what the view and the
game say to a press of A, with no reason of the hand, 40 lines at most.

### 14.8 Tests

The math test (`TeleportChecks`) covers:

- The arc: 49 points, the first point is the origin, and the first segment
  goes along the aim within 1.5 degrees. The steps in x and z are equal. A 45
  degree aim over flat ground lands at `R` within 1 percent, at 4, 10 and 20
  m. A level aim from 1.5 m lands at `v0 * sqrt(2h/g)`. The heading of the
  landing point is the heading of the aim. A wall gives a hit at the wall. No
  crossing gives no hit. A start under the land is a hit at the start. A 70
  degree aim is the sky, and 59 degrees is not.
- `CameraTeleportDelta`: the eye ends over the target at the same height over
  the land.
- `GodTeleport` at 7.5, 100 and 2000 units a meter: the head goes over the
  target within 0.01 units, the land under the head keeps its room height
  within 1 mm, `s` and `psi` do not change, a target at the head gives the same
  pose, and a hill over the head is kept 10 cm under it.
- `RingRadiusCm` (`teleport_checks.h`): mode 0 gives 11 cm at each scale. Mode 1
  gives 11 cm at 100 units a meter, 7.3 cm at 150, 16.5 cm at 66.7, 40 cm at
  7.5 and 4 cm at 4000. The size is always from 4 to 40 cm, never grows with a
  zoom step out, and is 11 units of the world between the limits.
- `BuildTube`: 24, 12 and 576 triangles, every corner at the radius from the
  line (a straight tube within 1e-6 m, the arc within 2 percent), and unit
  normals. `BuildLandRing`: 72 triangles (valid) or 52 (red), every point at the
  land height plus the lift, the band from the inner to the outer radius.
- The aim filter: 0.76 degrees of tremor leave 0.12 degrees. A step of 20
  degrees is within 0.4 degrees after 5 frames, and a sweep of 60 degrees a
  second is 0.7 degrees late.

### 14.9 The run on the desktop

Use the rig of section 12, with `fake_head_pitch=0`. The Creature select story
holds the view until the pen. A press of A in a story is refused, and the log
says so.

`aim_queue.ps1 -Runs ring1,ring0` in `bwvr/tools/desk` makes three teleports
at the pen: at 100 units a meter, after two zoom steps in (44.4) and after five
zoom steps out (337.5). The aim is 60 degrees down, straight ahead, in the
test save `pen` and in a new game. The runs show for each teleport that the
head is over the target (0.000 units off), that the land under the head keeps
its room height, and that the scale and the heading stay. With
`teleport_ring_mode=1` the ring has a radius of 11.0 cm, 24.7 cm and 4.0 cm of
the room at the three scales. With `teleport_ring_mode=0` it has 11.0 cm at
each scale.

1. The switch of the game, at the start of the tutorial. `spawn_run.ps1
   -Choice normal` starts a new game with the row "Start the game normally":
   the whole first story, and then the walk to the village. Four texts of the
   story wait for a click, so the script has a press of the trigger at the
   frames 6750 and 7350, and then each 220 frames from 7950. The story ends
   at about frame 9700, and the log says "the camera controls of the game are
   0x48": the drag is on, and the jump is off. The run starts with
   `teleport_switch=0`, and the word `teleportkey` changes the switch.
   `fake_controller_turns=0,-40,0` and `aimturn@700+1` turn the aim to the
   land near the head. The log shows:
   - With `teleport_switch=0`, a press of A gives "A is refused: the game's
     jump switch is off (camera switch 0x10)".
   - After `teleportkey`, "the view and the game let A teleport now
     (teleport_switch=1)", and a press of A gives the aim, the hit and "A is
     up, so go to". The walk with the stick starts in the same state of the
     game.
   - The cards of the land (`prompts=2`): "[A] Teleport" at the right hand,
     and "[Grip] Move the land, [Stick] Move, [Y] Ring menu, [Menu] Game
     menu" at the left hand. With `far_grab=0` no far target takes the place
     of these cards.
   - One teleport of 140 units and four frames of the walk bring the player
     to the village: the story of the village plays, and then the log says
     "the camera controls of the game are 0x18".
2. With the switch on, two aims and a release: `a@N+90 pose2@N+30+1`. The
   log shows the aim, the hit, the reason, and "A is up, so go to". Then the
   line "the God view moved": the head is over the target within 0.01 units,
   the land under the head keeps its room height within 1 mm, `s` and `psi` do
   not change, and the version goes up by 1. The comfort report shows the game
   camera written once, and the help report shows 0x305.
3. Eye captures during the hold show the tube and the ring in both eyes, with
   a disparity of `fx * e / d * hx`.
4. A sweep toward the sea (`fake_controller_sweep_pitch`) shows a red arc, and
   the release does nothing.
5. An aim 70 degrees up has no arc ("the aim is the sky").
6. `b@N+5` during the hold cancels, and the log of the gesture stays silent.
7. The mouse: `double@N+4` after `home2@M+1`, with `fake_controller=0`. The
   log shows "the mouse double click goes to".
8. `teleport=0`: A sends the game's jump.

### 14.10 Not tested

- Not run on the desktop: the red arc on the sea, the aim at the sky, the
  cancel with B and the mouse double click (the steps 4 to 7 of 14.9).
- The immersive move and the table ray have no desktop run. Ctrl F11 gives
  the immersive view.
- `teleport_switch`: no run has the key Ctrl, Shift and comma with a real
  key, a switch that goes off while the player aims, or the teleport on the
  walk around the mountain, where the game has the drag on and the jump off
  too.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

## 15. The place at a camera of the game

The player goes where the game puts its own camera. The code is in
`comfort_camera.cpp` (the watch), `tabletop.cpp` (`PlaceGod`) and
`table_math.h` (`GodSpawn`, `CameraLookPoint`, `HeadYaw`).

### 15.1 Why the view follows the game camera

The view is built from the game camera at the first main camera call of a new
game, and again at the end of a story, at a load and at Ctrl F11. But the game
camera can go to a new place at other times. At the start of a new game the
start dialog is up, and the game camera is at the shore of Land 1: at 1486 27
1974, with its focus at 1443 1 2079. After the row "Keep your old Creature"
the game plays no story. 55 frames after OK it makes a new `CameraModeNew3`
object, and that mode brings the camera to the door of the Temple in about 1.1
s. A view that is built one time only would stay at the shore.

A script moves the camera only with a story camera (section 15.2). The focus
of a story camera can be under the recenter pose, or under the land. So the
view takes the look point of the camera (15.4) and not the focus, and it
follows each new place of the game camera (15.3).

### 15.2 When the game puts its camera at a place

`[V]` is from the disassembly.

| Case | What the game does | The view |
| --- | --- | --- |
| The first frame of a new game | The start camera of the land, with the start dialog over it | Built there |
| The row "Keep your old Creature" | No story. A new `CameraModeNew3` object, which brings the camera to the Temple: at 1891 33 2521, looks at 1907 30 2517, heading 104 degrees, 17 units away | One cut when the camera is at rest |
| The other rows of the start dialog | A story with the cinema bars | The cut scene window, then the place of 15.4 |
| The end of a story | `END_CAMERA_CONTROL` takes `CameraModeScript` off. A script can put the camera back with `RESTORE_CAMERA_DETAILS`, which calls `GCamera::SetPositionAndFocus` `0x004438C0` [V] | The place of 15.4, or the view of before the story (15.5) |
| A script with no story camera | `SET_CAMERA_POSITION` `0x006EC8F0` and `MOVE_CAMERA_FOCUS` `0x006ECBA0` cast the top mode to `CameraModeScript`, and they do nothing with `CameraModeNew3` on top [V]. So a script needs a story camera to move the camera | The view stands aside |
| `SET_CAMERA_POS_FOC_LENS` `0x006EE3C0`, `RESTORE_CAMERA_DETAILS` `0x006EE390`, `TempleChallenge::StartScript` | They call `SetPositionAndFocus` with no test of the mode [V, the callers] | A set between two updates |
| The Temple key | `GGame::DoAction` calls `fn_0064A7A0(0)`, which calls `CameraModeNew3::ZoomToCitadel` `0x00457B60` with the flag 1 [V]. The first press pulls the camera back at its place. A second press in 500 ms flies to the Temple, 130 units away and 45 degrees down | The first press is not a new place. The flight is a jump: one cut |
| The key for the whole land | `fn_0064A7A0(1)`: `ZoomToCitadel` to a land point, 2200 units away, with the flag 0 [V] | A jump: one cut |
| A double click jump | The game's flight, events 0x305 and 0x306 | The mouse: the teleport takes the click. A with `teleport=0`: the view goes to the place of the jump |
| A load | `GGame::ClearMap` calls `ResetCameraModeNew3` `0x00460B20`, and `GCamera::Load` `0x004433A0` clears the mode stack [V, the calls] | Built again after `ClearMap`. A camera that the load sets later is a new place |
| The Temple rooms | The game draws from a different camera | The view stands aside and comes back where it was. A game camera that is at a new place then is a set between two updates |
| The Creature key, a bookmark, a fight | Not read in the binary | A flight is a jump, and a set is a set between two updates |

### 15.3 The watch

`comfort_camera.cpp` hooks the update of the player's camera mode. The watch
reads the destinations of the six Zoomers of the game camera at three points
of each update:

| Point | A change there is |
| --- | --- |
| Before the game's update | From outside the update: a script, a load, a key. A new mode object is a change too |
| After the game's update | The game's own: a jump, or a camera that goes to its place |
| After the writes of the mod | The player's move. These values are the base for the next update |

- **A move of the player is not counted.** A change in an update with a
  camera event of the player (0x300 to 0x304 and 0x308) is the player's. A
  write of the mod is the player's too: a step, the grab, a teleport, a snap
  turn. It ends a move of the game that is not at rest yet.
- **The game brings its camera to rest after a write.** After each write of
  the mod, and after a story, the game fits its camera to the land again. It
  puts the focus on the land along the camera line, and it pulls the camera
  in when a hill hides the focus. The focus can move 25 units and the
  position 190 units, in 250 to 1300 ms. That is the end of the player's
  move, and the watch does not count it. A change from outside the update, a
  new mode and a jump are always counted.
- **One move, one cut.** The changes of the game make one move. The move ends
  when the camera is at rest: no change for 250 ms and no flight. A jump that
  landed at once needs a rest of 100 ms only: the game puts the focus on the
  land in its next update, and the look point is read after that. Then the
  view goes there, with the blink of a teleport (only when `blink` is above 0). A camera that
  the game moves for many frames gives one cut at its end, never a glide.
- **A small move is not a new place.** A new place needs a turn of 5 degrees,
  or a look point (15.4) that moved 5 percent of the camera distance and
  5 units at the least. The first press of the Temple key moves the camera
  41.7 units back and the focus 2.6 units: the view stays.

### 15.4 The place

`PlaceGod` puts the view at a game camera. `GodSpawn` is the math:

```
L     = the land point that the camera looks at (the look point)
theta = the yaw of the head in the room
psi   = the heading of the camera - theta
r     = head + ahead * (sin theta, 0, cos theta)       across only
G     = L - s * Lt(psi) * (r - Tg)                     across only
G.y   = L.y - (h - Tg.y) * s
```

- **The look point** (`CameraLookPoint`) is the first land point on the line
  from the camera to its focus, when the line meets the land before the
  focus. Otherwise it is the focus, put on the land. The player's camera mode
  keeps its focus on the land, so there the look point is the focus. A script
  can put the focus under the land or in the air. Both poses of such a camera
  give one look point.
- **Where the camera goes.** The view is built from the destinations of the
  Zoomers of the game camera, and not from the camera of the main call. A
  camera that still glides then gives its place at once. At the first frame
  of a new game the main call has its focus at 1415 -16 2146, and the
  destination is 1443 1 2079.
- **In front of the head.** The look point is `god_spawn_ahead_cm` (50) in
  front of the head of that frame, along the flat forward of the head. The
  flat forward of the head gets the heading of the camera. So a player who
  stands off the recenter pose, or who turned in the room, still has the
  place in front. With no head pose the recenter pose is used.
- **The height.** `h` is the height in the room that the land under the head
  had at the last view, kept between 10 cm and 2.5 m below the head. The land at the
  look point gets it. So the ground steps of the player stay. At the first
  start the look point is at the waist. A hill under the head stays 10 cm
  below the head.
- **Why 50 cm.** With the land 60 cm below the head, a point 50 cm ahead is
  78 cm from the eyes and 50 degrees below eye level. A bent arm reaches it,
  and the player sees it with the head a little down. At the Temple start
  the Creature stands 4.4 units from the look point, so it is 46 cm in front
  of the head at 100 units per meter.
- **The turn at a jump.** The game keeps the heading of its camera at a jump
  to the Temple, and the camera that the mod wrote has the heading of the
  room forward. So the jump turns the world by the yaw of the head. The place
  is in front of the head in each case.

### 15.5 The view comes back

A script often puts the camera back where it was before its story. In the
God view that camera is the game camera of the view: the camera that
`GodAfter` wrote, as the game brought it to rest. `tabletop.cpp` keeps the
pair of that camera and the view (`g_pair`). When the game camera is at that
place again, and the player did not move the view after it, the view comes
back exactly where it was. The limits are 5 units for the position, and for
the focus 5 percent of the camera distance and 5 units at the least. The log
then says "the view comes back where it was".

### 15.6 The scale

The scale stays. A game camera far from its focus shows more land, and a
scale from the camera distance could show the same land: `s = distance / 1.2
m` gives 14 units per meter at the Temple start (17 units) and 108 at the jump
to the Temple (130 units). That is not built, for three reasons. The scale
is the player's own choice, as the ground height is. A change of the scale
is a second change that the player did not make, at each jump of the game.
And the distance of a story camera is the choice of a shot, not of a view:
the start at the Temple would be 7 times larger than the view of the player.
The player zooms with the stick after the cut.

### 15.7 The keys

| Key | Default | Meaning |
| --- | --- | --- |
| `god_spawn` | 1 | 1 is this section. 0 builds the view at the start, a load and the end of a story only, with the focus under the recenter pose. The log still names each new place of the game camera |
| `god_spawn_ahead_cm` | 50 | The look point, in front of the head, 0 to 150 |

With `test_keys=1`, Ctrl and Numpad 0 change `god_spawn` between 1 and 0.
The view then goes to the game camera of that moment by the rule that it has.
So the two places can be compared at one camera. Numpad 0 with no Ctrl is
the test key of the cut scene window.

Three keys are tests with no key press, for a desktop run:

| Key | Meaning |
| --- | --- |
| `spawn_test_temple=N` | At update N of the player's camera mode the mod calls `ZoomToCitadel`, as the Temple key does, to the land point `spawn_test_x` and `spawn_test_z` (2238 and 3156, the Creature pen) |
| `spawn_test_flag` | The flag of that call. 0 goes at once. 1 is the key: the first press pulls the camera back |
| `spawn_test_toggle=N` | At update N the test key acts, and 300 updates later it acts again |

### 15.8 The log

The tag of the place is `god view`, and the tag of the watch is `comfort`.

```
god view     : god_spawn=1. When the game puts its camera at a new place, you go there: the land point that it looks at is 50 cm in front of your head (god_spawn_ahead_cm). Ctrl and Numpad 0 change it
comfort      : the game moved its own camera (a new camera mode of the player). It was at 1486.3 26.7 1973.7 and looked at 1442.9 0.8 2078.5. Now it is at rest at 1891.0 32.5 2520.7 and looks at 1907.3 29.6 2516.7, update 690: the land point that it looks at went 638.5 units, from 1442.9 2078.5 to 1907.3 2516.7, and the camera turned 126.5 degrees. The move took 1390 ms and 80 updates. Changes: 63 in the update, 0 between updates, 0 jumps, 1 new modes. Camera events 0x000, the camera input was the player's. This is a new place
god view     : the view goes to the game camera (a new camera mode of the player). The game camera is at 1891.0 32.5 2520.7 and looks at 1907.3 29.6 2516.7, heading 104.0 degrees, 17.1 units from its focus. The waist point was at 1462.1 0.0 2032.3, heading -22.5 degrees. Now it is at 1858.8 29.5 2528.7, heading 134.0 degrees, 100.0 units per meter. The land at 1907 29 2517 is 50 cm in front of your head and 0.60 m below it, and the land under your head is 0.62 m below it (god_spawn=1).
god view     : the game camera went to a new place (a jump of the game camera): it is at ... and looks at .... The view stays where it is (god_spawn=0).
comfort      : the player moved the view, so the move of the game camera is dropped, update N
comfort      : the game put its camera at a new place 1 times. 0 of its moves were too small for a new place, and 0 ended when the player moved the view. 0 updates had a camera move of the player. In 45 updates the game brought its camera to rest after a write.
test key     : god_spawn=0. The view does not follow the game camera, and the land point that it looks at is under the recenter pose
```

- The causes of a move are: "a new camera mode of the player", "the game set
  its camera between two updates", "a jump of the game camera" and "the game
  moved its camera in its update". The causes of a build are: "the first view
  of this land", "a new land", "a load or a new game", "Ctrl F11", "the end of
  a story", "the end of a cut scene" and "the test key".
- Each place has one line with the cause, the game camera and its focus, and
  the waist point before and after. The line of the watch comes before it,
  with the counts of the changes and the camera events of the game.
- A small move has a line only the first 4 times. The report counts each
  one.
- The log has at most 80 lines of the place and 60 of the watch.

### 15.9 Tests

The math test (`spawn_checks.h`) covers:

- `HeadYaw`: a level head, a head that turned 30 degrees, a head that looks
  60 degrees down, a head that looks back, and a head on its side (no yaw).
- `CameraLookPoint`: a focus on the land, under the land and in the air, a
  hill before the focus, a camera under the land, and the two poses of a
  story camera on one line, which give one look point.
- `GodSpawn` at 7.5, 100 and 2000 units per meter: the look point is 50 cm
  in front of the head, at the height that was asked, for a head at the
  recenter pose and for a head that stands off it and turned 40 degrees. The
  forward of the head has the heading of the camera. The scale does not
  change. The same camera gives the same pose two times. A camera that looks
  the other way puts the player on the other side. A head at the recenter
  pose with 0 cm ahead gives the rule with no look ahead.

**Desktop runs** use a new game on Land 1 with a choice of the start dialog,
the fake controller with `fake_controller_front=1`, and a render of 2560 by
1440. The fake head looks 30 degrees left and 20 degrees down from frame 600
(the word `look`), so the captures show the land in front. The runs to make:

- "Keep your old Creature": the log must show one place 1.4 s after the new
  mode, with the Temple door in front of the head in the capture.
- The test key (`spawn_test_toggle`): `god_spawn=0` puts the waist point at
  the look point with the heading of the camera, and `god_spawn=1` puts it
  back in front of the head.
- `ZoomToCitadel` with the flag 0 (`spawn_test_temple`): the jump lands at
  once, and the view goes there 125 ms and 9 updates later.
- The same with the flag 1: one press is not a new place ("this is not a new
  place, so the view stays").
- The story to the pen: one place, and no second place in the updates in which
  the game brings its camera to rest after a write.
- A grab for 400 frames, a zoom step in and a zoom step out: 0 new places.

### 15.10 Not tested, and what needs the headset

- **The view comes back** (15.5) has no run. A story that puts the camera
  back is needed.
- **A load, a fight and the Creature key** have no run. The watch has no
  rule of its own for them.
- **The Temple key, the Temple rooms, a bookmark, the next town and the key
  for the whole land** are jumps of the game camera, and the view goes to
  each one. In the Temple the God view stands aside, and after the Temple it
  is at its place from before the Temple. The ring does not send the Creature key in the
  God view: it makes one teleport to the Creature (`controllers.md`).
- **Ctrl and Numpad 0** has no run with a real key. The test
  `spawn_test_toggle` calls the same function.
- **A move by the arrow keys** counts as a move of the player (event 0x308),
  so the view does not follow it. That was not seen in a run.
- **A grab that moves the land, and the mouse pan,** have no run with the
  watch. Each one writes the game camera, which is a move of the player by
  the rule of 15.3.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

## 16. The player that a script sees

A script of the game asks about the camera to learn what the player does, and
in the God view the game's own camera is not the player. So a script that
asks about the camera gets the player. The code is in `hand_direct.cpp`
(`h_ScriptDistance`, `h_ScriptPosView`, `h_ScriptThingView`,
`h_ScriptThingSees`, `LatchPlayer`, `ScriptPlayerNow`) and `near_math.h`
(`PlayerDistance`, `PlayerIsCamera`, `HeadSees`).

### 16.1 What the scripts ask

`[V]` is from the disassembly and from the compiled scripts
(`Scripts\Quests\challenge.chl`, LHVM version 7). Each script word is a
cdecl function with no arguments. It takes its values from the stack of the
script machine with `ScriptDLL::POP` `0x006F6BC0`, and it gives its answer
back with `ScriptDLL::PUSH` `0x006F6BA0`. The script machine is at
`0x00D95C10`.

| Script word | Function | What it does [V] |
| --- | --- | --- |
| "camera position" | `GScript::GetCameraPos` `0x006ECFF0` | Gives `LH3DTech::g_camera` `0x00EA1DB8`, the camera of the last `LH3DTech::UpdateCamera` |
| "get distance" | `GScript::GetDistance` `0x006F8CA0` | Takes two points and measures them with `GUtils::GetDistance` `0x0074CDE0`, which uses x and z only: the distance on the map. Below 0.5 units it gives 0 |
| "position in the field of view" | `GScript::IsPosFieldOfView` `0x006F8060` | Asks `fn_0081F1D0`: the point is in front of the camera, and its picture is on the screen. The test uses the matrix that the engine has at that time (`0x00EA9E40`) |
| "thing in the field of view" | `GScript::IsGameThingFieldOfView` `0x006F8130` | For an object with a mesh it asks `LH3DBoundingBox::CheckRegionOnScreen` `0x00868C80` for the ball of the mesh. For each other thing it asks `fn_0081F1D0` for its place |
| "thing can view the camera" | `GScript::GameThingCanViewCamera` `0x006FE4D0` | Asks if `LH3DTech::g_camera` is in front of the thing, in an angle that the script gives |

The scripts use these words at many places:

- "camera position" is at 561 places. At 111 of them, in 70 scripts, the
  next word is the distance to a thing, and the script compares it with a
  limit. 95 limits are constants from 1 to 250 units: 27 are 100, 20 are 150
  and 11 are 200. Each test means "the player came to look". 21 tests have
  NOT after them: "the player went away".
- The two field of view words are at 107 places (53 for a thing, 54 for a
  point). 78 of them are beside a distance test: "the player is near and
  looks at it". 42 have NOT after them: for example, a script deletes or
  moves a thing only while the player does not look.
- "thing can view the camera" is at 4 places: the Island Keeper of the Gods'
  Playground and the monk.

Some of the tests:

| Script | Test | What it starts |
| --- | --- | --- |
| `PointOutScroll` | Less than 100 units from a place | The advisor points at the scroll of the missionaries |
| `ThrowingStonesChallengeHighlightNotify` | Less than 150, and in the view | The two advisors speak of the scroll |
| `LookAfterGuideWalkingAroundMountain` | Less than 10 and 25 | The Island Keeper of the Gods' Playground walks faster |
| `MeetTheGuide` | Less than 90, and in the view | The cut scene of the guide |
| `ZoomInAndOut` | Two stones in the view, then the two not in the view | The steps of the zoom lesson of the Gods' Playground |
| `LostBrotherDead` | Less than 20 | The dialogue at the body |
| `FollowUs`, `LostTheMother` | 50 to 100, with NOT | The mother waits, or the player lost her |
| `CreatureGuardian` | NOT less than 100, and NOT in the view | The script deletes the guardian |
| `HermitWardPact` | NOT less than 1, and NOT in the view | The player does not look at the hermit |

The scripts `ChallengeHighlightNotify` and its copies compare the distance
with the variable `NotifyDistance`. No script writes that variable. [I: its
value is 0, so the test is always false, in the plain game too.]

### 16.2 Why the game's camera is wrong for the tests

- **The distance.** After the player moves the view, `GodAfter` puts the
  game's own camera 400 units from the place of the player, 50 degrees above
  the land (section 7). That is 257 units on the map. No limit is more than
  250. So each distance test would be false for a thing at the place of the
  player, at each zoom.
- **The view.** `TabletopGraphicsDone` gives the engine the game's own camera
  again after each frame. The scripts run in the game turn, between two
  frames. So the view tests would use the camera 400 units behind the player,
  along the heading of the room. That camera sees almost all that is near
  the player, also behind the head. It does not turn with the head, and it
  does not change with the zoom. The zoom lesson of the Gods' Playground
  asks if two stones are in the view, so a zoom of the God view would not
  end that step.

### 16.3 Which point is the player

- **For a distance,** the player is the head and each hand that is tracked:
  the Hand's point, and the grip of each controller. The distance of the
  player is the least of their distances on the map
  (`near_math::PlayerDistance`). A hand at a thing is "the player came
  close". The tests with a limit of 10 to 30 units need that: at 100 units a
  meter the head is 60 cm over the land. A hand counts only within 2.5 m of
  the room from the head.
- **For a view test,** the player is the head: its place and its three axes,
  as the frame was drawn. The view angle is the angle of the game for that
  frame, which the mod sets from the headset (110 degrees in a desktop run).
  A point is in the view when it is in front of the head and inside that
  angle (`near_math::HeadSees`). This is the game's test for the picture
  that the headset shows.
- **For "thing can view the camera",** the camera is the head.
- **For a thing that a script turns to "camera position"** (`SET_FOCUS`),
  the point is the head (`villager_look_head`, `village.md`, "A villager
  that a script turns to the player").

The game measures a distance on the map, so the height of the head does not
count. At 100 units a meter a thing 50 cm in front of the head is 50 units
away, and at 29.6 units a meter 15 units.

### 16.4 When the player stands for the camera

The player is the answer only while the view of the player is at the place
of the game camera. The mod asks this at the moment of the script word
(`ScriptPlayerNow`, `near_math::PlayerIsCamera`), and not from a time limit.
In each other case the script gets the game's own answer, as in the plain
game.

| Case | What the mod reads | The script gets |
| --- | --- | --- |
| The menu, the intro, the first frames, a Temple room | The God view did not own the last frame | The game's camera |
| A load, Ctrl F11 | The God view does not own the view now | The game's camera |
| The first game turn of a story, from the instruction that starts it | The camera mode on top of the stack of `GCamera` is not a mode of the player (`ScriptCameraOwnerNow`) | The camera of the story |
| The cinema bars are up | `HelpSystem + 0x45E8` (`CutsceneWindowBarsUp`) | The camera of the story |
| A story or a cut scene plays | The God view stands aside (section 7), so it did not own the last frame | The camera of the story |
| The last frame of a story, and the frames until the window is open | The same | The game's camera, where the story left it |
| About 1 s after a story, and after a new game | The game moves its own camera to a new place, and the camera is not at rest (`ComfortCameraGameMoves`, section 15.3) | The game's camera |
| Each other frame of the God view | | The player |

**The place of now.** The player is latched once a frame, inside the draw
(`LatchPlayer`, from `HandDirectFrame`). The scripts run in the game turn
(`GGame::ProcessNetworkPackets`), before the next main camera call. Only
that call moves the view: a teleport, a zoom step, a turn, the place at a
game camera. So the latch is the view that the player has when a script
asks. A frame with no main camera call has no latch.

**Why not a time limit.** When the game makes a new camera mode, its camera
is at the new place at once, but the view goes there only when that camera is
at rest, about a second later (section 15.3). In that time the view of the
player is not at the game camera, so the script gets the game's camera.

**A story keeps its camera.** A script that keeps "camera position" to put
the camera back after a cut scene is not changed: only the three kinds of
test are. In a story each test gets the camera of the story. With
`cutscene_window=0` and a story camera with no bars, `script_camera.cpp`
holds the shot, and the script gets the held shot.

### 16.5 The tests that ask if the player went away

A test with NOT is true only when the head and each hand are away. So a
hand that stays at a thing keeps the player there.

- With the game's camera each such test would always be true in the God view:
  for the game the player would always be away. `FollowUs` would then say that
  the player lost the mother, and `CreatureGuardian` would delete the guardian
  while the player is beside it.
- The limit of 1 unit of `HermitWardPact` is true for the player as for the
  game's camera: no point of the player is that near in play.
- When the player stands for the camera in one game turn and not in the
  next, the answer can change in that turn. That is at the start of a story
  and for about 1 s after it. In that time the game's camera is the camera
  of the story, which is near the place of the story.
- A view test with NOT is true when the head does not face the thing. So a
  thing that a script deletes "while the player does not look" can go while
  the player is near with the head turned away. That is what the script
  asks for.

### 16.6 The camera moves that a script sees

Two script words ask what the player does with the camera, and not where the
camera is [V: the disassembly and `challenge.chl`]:

| Script word | Function | What it gives [V] | The scripts |
| --- | --- | --- | --- |
| "get hand state" | `GScript::GetHandState` `0x006FF730` | The interface hand mode (`GInterface + 0x3AC`). The interface state `LANDSCAPE_LOCK` (`0x005D8090`) gives 0x14 while the Move Button (the left button) holds the land: a drag, or a turn at the rotate arrow. It gives 0x16 while both buttons hold the land and the game lets the camera zoom (bit 4) | `TeachRotate` and `TeachZoom` (a guard: not 0x15, not 0x16), `MoveAroundMountain` (it waits for 0x14), `ZoomInAndOut` (it waits for 0x16) |
| "within rotation" | `GScript::WithinRotation` `0x0070B9D0` | The arrows of the camera that show (`0x00C5B0F4`, which `CameraModeNew3::UpdateTricons` `0x00459230` sets in each frame). Bit 2 is the rotate arrow: the pointer is at a side of the screen, or the camera turns. The game clears each arrow that its switches do not allow | `MoveAroundMountain` (it waits for the arrow) |

In the God view the stick turns and zooms the view, so the game never sees
these holds of the mouse. With `script_camera_moves=1` a move of the God view
gives the value that the game gives for the same move with the mouse
(`near_math::ScriptHandState`, `near_math::ScriptArrows`):

- A zoom of the view in the last two game turns is the hold of both buttons:
  hand state 0x16, while the game lets the camera zoom. A zoom is each event
  0x304: a zoom step, the smooth zoom, the zoom of a grab with two hands.
- A turn of the view in the last two game turns is the hold of the Move
  Button: hand state 0x14. A turn is each event 0x300: a snap turn, the turn
  events that it owes (`turn_help_degrees`), the turn of a grab with two
  hands. A grip that holds the land now is the hold of the Move Button too.
- A turn of the view in the last two game turns shows the rotate arrow (bit
  2), while the game lets the camera turn.

The game's word runs first. Then the mod takes its answer off the stack of
the script machine and pushes the new answer with the same type. A hold that
the game gives stays: the trigger held on the land gives 0x14 in the game's
own way. The two game turns are measured with the clock of the help
(`HelpProfile::AccumulatedTime` `0x00C5AFD8`, 100 for each game turn). The
hook on `CameraHelpCallback` keeps the clock of the last event of each number
(`h_Help`, `ComfortCameraHelpClock`, `near_math::HelpRecent`). A script gets
the moves only while the player stands for the camera (16.4). With 0, each
script gets the game's answer.

### 16.7 The keys

| Key | Default | Meaning |
| --- | --- | --- |
| `script_distance` | 1 | 1: a distance from the camera is the distance from the player. 0: the game's camera |
| `script_view` | 1 | 1: a view test uses the view of the head. 0: the game's camera |
| `script_camera_moves` | 1 | 1: a script that asks how the player holds the land, or if the rotate arrow shows, gets the moves of the God view (16.6). 0: the game's answers |
| `villager_look_head` | 1 | 1: a thing that a script turns to "camera position" turns to the head (`village.md`). 0: to the game's camera |

With `test_keys=1`, Ctrl and the 7 key of the number pad change between the
player (the four keys as the ini has them) and the game's camera for all
tests. The pointing controller gives a long pulse for the player and a short
pulse for the game's camera.

When a function does not have the bytes of the disassembly, the mod does
not hook it, the key is 0, and a line with the tag `hooks` says so.

### 16.8 The log

```
scripts      : script_distance=1, script_view=1. In the God view a script of the game that asks about the camera gets the player. [...] Ctrl and the 7 key of the number pad change both
god view     : the waist point is at 1830.0 37.8 2720.0, [...] The scripts of the game got the player in the place of the camera 1851 times.
scripts      : Ctrl and the 7 key: a script that asks about the camera gets the game's camera, frame N
```

The log has no line for a test. The count in the report line of the God
view is the number of answers from the player in that report window. A
window with a story has no such line.

The camera moves (16.6) have one line for each new pair of the game's answer
and the answer of the mod, 24 lines at most, and a count in each report
window:

```
scripts      : script_camera_moves=1. In the God view a script that asks how you hold the land, or if the rotate arrow shows, gets the moves of the view: [...]
scripts      : a script asked for the hand state. The game said 0x3, and the script gets 0x14: the view turned in the last two game turns, which is the hold of the Move Button at the rotate arrow (script_camera_moves=1, camera controls 0x1A), frame 14551
scripts      : 3 answers to a script that asked how you hold the land or if the rotate arrow shows came from the moves of the God view (script_camera_moves=1)
```

### 16.9 Tests

The math test covers `MapDistance` and `PlayerDistance` (the guide with the
head 20 units away and a hand 5 units away, a thing 300 units away, no point
of the player, a place that is not a number), the tests with NOT and the
limits of 1 and 250, each case of `PlayerIsCamera`, and `HeadSees` (ahead,
behind, 49 and 51 degrees to the side, the land at the feet, a head that
turned 90 degrees, 110 degrees). For the camera moves it covers
`ScriptHandState` (no move, a zoom with the zoom on and off, a turn, a grip
on the land, a grab of two hands, a hold that the game gives),
`ScriptArrows` (a turn with the turn on and off, a zoom, the arrows of the
game) and `HelpRecent` (this game turn and two before, three before, a clock
that went back).

The desktop run of the tutorial of Land 1 (section 7, "The steps of the
tutorial") shows the camera moves in the turn lesson (0x14 after a snap
turn) and in the zoom lesson (0x16 after a zoom step). With
`script_camera_moves=0` the same recipe has no such line, and the tutorial
of Land 1 ends the same way: the zoom lesson ends by the height of the game
camera. The run of the Gods' Playground (section 7) shows the rotate arrow
and 0x14 after snap turns, and the end of the step of the turn. With
`script_camera_moves=0` that step does not end.

**Desktop runs** use the start "Keep your old Creature", a render of 2880 by
2880, the fake head with the word `look`, and the test word `go` of
`seek_plan`, which teleports the head over a land point. After a teleport the
mod puts the game's camera 257 units on the map from the head. An advisor line
shows in the log as "audio adv : line N starts, sample S". The runs to make:

- `go` to 1830 2720, 80 units from the place of `PointOutScroll` (1755 2692,
  limit 100): the advisor comes out at once. With `script_distance=0` and
  `script_view=0` it does not.
- `go` to 2024.3 2382.3. The gold scroll of the throwing stones (1947.4 2360.2,
  limit 150 and in the view) is 80 units behind the head. With
  `script_view=1`, no line starts until a turn of 45 degrees brings the scroll
  into the view. With `script_view=0` the line starts at once, with the scroll
  behind the head.
- The start of the game and the story to the pen: each ask in a story gets the
  camera of the story, and each ask while the game brings its camera to rest
  gets the game's camera.

Check each log for input that was not from the test: no "the cursor moved"
line, and one "the mouse owns the pointer" line, at the start dialog.

### 16.10 Not changed, not tested, and what needs the headset

- **The first game turn of a story.** No run has a test in the game turn
  that starts a story, after the instruction that starts it. The two cases
  of 16.4 for that turn are from the code only.
- **A thing with a large mesh.** The game's test for an object counts the
  ball of its mesh. The mod tests the place of the thing. A house whose
  middle is out of the view, with a wall in the view, is "not in the view".
- **A marker at "camera position".** At about 114 places a script makes a
  marker thing at the camera, and at 150 it keeps the camera with its focus
  to put it back after a cut scene. These are not changed. [I: if a script
  uses such a marker as "where the player is", it is 257 units from the
  player on the map.]
- **`GetFacingCameraPosition`** `0x006EE710` (7 places) puts the Hand of
  another god in front of the camera. Each place is in a cut scene. Not
  changed.
- **The other readers of the engine camera** in the game logic (184
  instructions read `LH3DTech::g_camera`): the villagers, the Creature and
  the sounds. The sound has its own work (`audio.md`).
- **The Temple key, a load and a bookmark** have no run with a script test.
- **The camera moves** (16.6). No run has the step of the zoom of the Gods'
  Playground, which waits for the hold of both buttons. The pointer at a
  side of the screen also shows the rotate arrow of the game: no run has
  that in the God view.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

## 17. The camera zone

Some steps of Land 1 keep the camera in an area of the map: the camera zone.
The Creature select is one of them. The code is in `camera_zone.cpp`, and the
math is in `camera_zone_math.h`.

### 17.1 What the game does

[V: the disassembly, `Scripts\Quests\challenge.chl` and `Data\Zones`]

- **The script word.** "set camera zone" (`GScript::SetCameraZone`
  `0x006ED890`) loads a file of `Data\Zones`. The file gives a polygon on the
  map (`CameraModeNew3::ForceFieldPoints` `0x00C5B130`, the count at
  `0x00C5E130`) and a height limit. The word sets
  `CameraModeNew3::DrawForceField` `0x00C5E144`, and `0x009CE6B0` for the
  height limit. A save holds the zone (`CameraExclusion::SaveExclusionFile`).
- **The test.** On each update `CameraModeNew3::Update` tests the position
  of the game camera against the polygon (`0x0045F983` to `0x0045FCE1`,
  `CameraExclusion::InsideInclusion` `0x00455E20`). The test uses x and z
  only. It does not test the focus.
- **The push.** For a position outside the polygon the game looks for a
  point in it on a ring of 50 units about the position (20 units in a mode
  that is not 4), starting in the direction of the focus. When no point of
  the ring is inside, it takes the nearest point of the edge. The camera and
  its focus go there in one update. Then the game:
  - sends the help event 0x202,
  - starts the effect 42 of the force feedback (`GInterface::StartImmersion`),
    which the controllers give as a pulse of 0.20 for 15 ms,
  - plays the sound 0x2B at the camera (`fn_0042A000`, `GAudio::PlaySoundEffect`),
  - starts a shake of 400 ms (`LH3DCameraChecker::Create` `0x00821050`, from
    `0x0045FC89`),
  - and pulses the force field at the edge (`LightSheet::PulseForceField`).
- **The shake.** `LH3DTech::UpdateCamera` adds the shake to the camera that
  it gets (`fn_008210C0` at `0x00819A0B`): a random step of up to 1 unit on
  each axis of the position and of the focus, in each call, for 400 ms. It
  acts while the drawn camera is within 100 units of the place of the
  shake. The hook on `PSysGlobal::StartCameraShake` (`backend.md`, "No
  shakes") does not see this shake, because the push calls `Create` itself.
- **The height limit.** A camera higher than the limit goes down to it (event
  0x203). That move has no sound and no shake.

**The zones of Land 1.** `challenge.chl` names seven files. `Data\Zones` also
has `Land1Zone1.exc` and `Land1Zone4.exc`, which no script names. No other
land and no other script file has a camera zone.

| File | x | z |
| --- | --- | --- |
| `Land1Begin.exc` | 1439 to 1758 | 2027 to 2421 |
| `Land1TemplePath.exc` | 1450 to 1981 | 2089 to 2574 |
| `Land1Building.exc` | 1308 to 2038 | 1892 to 2878 |
| `Land1Zone2.exc` | 888 to 3013 | 1496 to 3161 |
| `Land1Zone3.exc` | 888 to 3013 | 1496 to 3287 |
| `Land1Choose.exc` | 2060 to 2295 | 2960 to 3208 |
| `Land1Zone5.exc` | 888 to 4120 | 1398 to 4379 |

`Land1Choose.exc` is the zone of the Creature select: 20 points, about 235
by 250 units, with the middle at 2166 3099. The game camera of the select is
at 2224.9 20.2 3146.3, in the zone, 35 units from its focus at the pens.

### 17.2 The God view and the zone

The God view puts the game camera 400 units from the land under the head, 50
degrees above it (section 7). That is 257 units back on the map. At the
Creature select the zone is smaller than that, so with no rule of the mod
(`god_zone=0`) each move of the view puts the game camera out of the zone.
On each update of a move the game then pushes its camera back, plays its
sound and starts its shake, and the shake goes into the view of the head.
The view does not follow the push.

### 17.3 What the God view does

With `god_zone=1` (the default) and a zone of three points or more:

- **The game camera stays in the zone.** The land point that it looks at goes
  into the zone, 5 units past the nearest point of the edge
  (`PointInside`). The position comes along the line to its focus, to 5
  units before the edge, so it keeps its direction and its pitch and comes
  nearer to its focus (`KeepCamera`). When that leaves less than 40 units
  across the map (`kLeastAcross`), the camera goes 40 units from its focus
  toward the middle of the zone, with the same pitch, and looks at its
  focus from that side. The test of the game casts a ray from the camera
  toward its focus: with the two at one place the ray has no direction, the
  test finds the camera outside on each update, and the game pushes it, with
  its sound, on each update. A snap turn that turns the game camera about its
  own focus (section 7) keeps to the same rule. So the game does not push
  its camera.
- **The player's place has a limit.** The waist point of the God view goes at
  most `god_zone_margin` (50) units past the zone. At the start scale of 100
  units for each meter that is 50 cm of the room. At the Creature select the
  player can stand 50 cm past the edge and see the three Creatures and the
  Hand. The limit holds for the walk with the stick, the grab, the mouse
  pan, a zoom step, the smooth zoom and the teleport.
- **At the limit the view stops.** A move past the limit goes to the point
  of the limit nearest to the place that the move asked for (`LimitMove`).
  So the part of the move along the edge stays, and the part out of the zone
  goes. There is no spring back and no bounce. With the grab the land stops
  and the hand keeps moving. A place that the game put farther out (a new
  place of the game camera, a load) may stay as far out, and no move takes
  it farther, so the limit never makes a jump.
- **The turn and the zoom stay free.** A snap turn is not limited. A zoom
  changes the scale freely. When the zoom also moves the place (the zoom
  about the land that the head looks at, section 2), the move of the place
  keeps to the limit.
- **The teleport.** A target farther than the limit is not valid: the arc
  and the ring are red ("the target is past the limit of the camera zone of
  the game").
- **The sound of the block.** While the player pushes past the limit, the
  game camera goes 10 units out of the zone for one update, at most once
  each 500 ms (`BumpPoint`). The game then plays its sound of the block,
  gives the pulse and the force field, and moves its camera back in. The view
  does not move with the game camera.
- **No shake.** While the God view owns the view, the shake of the push is
  not started (the hook on `LH3DCameraChecker::Create`, for the call from
  `0x0045FC89` only).

The height limit is not changed: a game camera above it goes down, with no
sound and no shake.

With `god_zone=0` the God view does none of this, and the game pushes its
camera with its sound and its shake. The debug page has the switch "Camera
zone" in the group View (`controllers.md`, 10.8).

### 17.4 The keys

| Key | Default | Meaning |
| --- | --- | --- |
| `god_zone` | 1 | 1: the God view keeps to the camera zone (17.3). 0: the game pushes its camera, with its shake |
| `god_zone_margin` | 50 | How far the player's place goes past the zone, in world units, 0 to 2000. 0 keeps the place in the zone |

### 17.5 The log

A line for each change of the zone, a line at the start and at the end of
each push past the limit, a line for the first bumps and for the first
writes of the game camera that the zone changed, and the counts of each
window of the report:

```
camera zone  : god_zone=1. In a camera zone of a script the game camera stays in the zone, so the game does not push it back or shake the view. Your place goes at most 50 units past the zone (god_zone_margin), and a push past that plays the sound of the game
camera zone  : a script set a camera zone of 20 points, x 2060.3 to 2294.7 and z 2960.1 to 3208.3, the middle at 2168.0 3099.2 (god_zone=1)
camera zone  : the game camera stays in the zone: at 2068.5 179.4 3041.4, not at 1977.9 309.5 2980.5, looking at 2191.4 3.1 3123.8
camera zone  : the walk with the stick asked for your place at 1979.6 2981.7, 100.4 units past the zone. It stops at 1980.0 2981.9, 100.0 units past it (the limit of god_zone_margin=100, or the place before the move)
camera zone  : you push past the limit, so the game camera goes to 2062.5 3017.9, 10 units out of the zone, for one update. The game plays its sound of the block and moves its camera back in
camera zone  : the push past the limit ended. Your place is at 2002.1 2941.1, 97.5 units past the zone
camera zone  : god_zone=1, a zone is set. The limit stopped 177 moves in 1 pushes, and 7 bumps asked the game for its sound. The game camera was kept in the zone on 278 writes, and its land point on 217. 6 shakes of the zone were not started, and the game started 0
```

### 17.6 Tests

The math test (`zone_checks.h`) covers the inside of a square and of an L,
the distance to the edge, the limit of a move (into the margin, past it,
along the edge, from a place far out, with no margin, and a sweep that never
passes the margin and never moves back), a point in the zone, the game
camera in the zone with its pitch kept, and the point of a bump.

The desktop runs start in the test save `pen` (`tools/desk/README.md`), at
the Creature select, with the zone `Land1Choose.exc`. The recipe walks with
the stick 5 s back out of the zone, 2.5 s forward, makes two snap turns,
two zoom steps in and one out, and walks 5 s to the side, out of the zone
again:

```powershell
$s = 'lstick_down@4700+300 shot@4900+1 shot@4960+1 lstick_up@5050+150 ' +
     'stick_right@5250+8 stick_right@5300+8 stick_up@5360+10 stick_up@5420+10 ' +
     'stick_down@5480+10 lstick_left@5550+300 shot@5800+1'
.\bwvr\tools\desk\hand_run.ps1 -Name zone1 -Dll <dll> -Kind fixed -Steps $s -Set 'god_zone=1;god_zone_margin=100;test_frame_rate=60'
.\bwvr\tools\desk\hand_run.ps1 -Name zone0 -Dll <dll> -Kind fixed -Steps $s -Set 'god_zone=0;test_frame_rate=60'
```

With the renderer Vulkan and `god_zone_margin=100`:

| | `god_zone=1` | `god_zone=0` |
| --- | --- | --- |
| The first walk out | Stops 100 units past the zone. 267.95 of 759 units | 759.78 units, to 1562 2702, 500 units out of the zone |
| The walk to the side | Stops 100 units past the zone, 89 units | 503 units |
| The turns and the zooms | Free | Free |
| Shakes that the game started | 0 | 75 |
| Shakes of the push that the mod did not start | 12, all after a bump | 0 |
| Bumps (the push of the game, with its sound) | 17, at most one each 30 frames (500 ms) while the walk pushes past the limit | none: the game pushes on each update of a move out of the zone |

The eye pictures of frames 374 and 433 of the run with `god_zone=1` are
the same view: the place stays at the limit while the stick pushes. They
show the force field of the game at the edge of the zone, which the bumps
pulse.

With the renderer Direct3D 9 (`backend=dxwrapper\ddraw.dll`) the walk of
`hand_run.ps1 -Kind zone` stops 50.0 units past the zone, and the game camera
goes 10 units out of the zone for one update with its sound. The limit
stopped 252 moves in 1 push, 9 bumps asked for the sound, and the game
started 0 shakes. In the test save `temple` the zone of the island (x 888 to
4120, z 1398 to 4379) stops a walk with the stick (`village_run.ps1 -Kind
walk`).

### 17.7 Not tested, and what needs the headset

- The other zones of Land 1 have the same rule. The verify runs show the zone
  `Land1Zone5.exc` in the test save `temple`, with no push. No session has
  shown a push at a zone other than the zone of the Creature select.
- `Influence::CalculatePlayerRawInfluence` and `CameraModeFollow::Update`
  also call `InsideInclusion`. What they do with the zone is not read.
- The two other callers of `LH3DCameraChecker::Create`
  (`AddSoundToAtom::ModifyAtomCore` and `fn_006E63C0`) still shake the
  view in the God view.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.
