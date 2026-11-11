# Miracles and gestures

This doc has two parts. The first part says how the plain game does gestures
and miracles. The second part says what the mod does so that a player can
make a miracle active, cast it and cancel it with the real hand.

Marks: [V] is verified in the disassembly. [I] is inferred. The facts come
from `disasm.py` and the Ghidra project on `runblack.exe` 1.42, and from
`Scripts\info.dat` and `Data\Gestures.jty`.

Related docs: `grab.md` (the grip), `throw.md` (the throw), `creature.md`
(the Creature), `village.md` (the Worship Site, the icons and the totem),
`hand-model.md` (the halo and the haptics).

## Summary

- A gesture is a path of the pointer on the land. The game records it, finds
  the corners and matches it to a template of `Gestures.jty`. A miracle is a
  chain of one to three gestures. No button starts a gesture.
- A miracle is active when a `SpellSeed` is in the Hand (hand state 8,
  Grain). It comes from a one-off bubble (a tap) or from a Worship Site (the
  chain of gestures or a tap on the icon).
- The right button of the mouse casts. The cast kind (pour, release, press)
  is a field of the seed info.
- A shake of the pointer (the scribble gesture) cancels.
- The mod puts the Hand at the real hand in the Grain state. The grip casts.
  A shake of the real hand or a short press of the second face button
  cancels. The trigger makes a carried seed active. The mod gives a target
  with a halo to a Creature miracle.
- The first face button of the hand that does not point opens the spell
  book. It shows the miracles that the player can ask for now, with the
  pictures of their gestures. The grip of the pointing hand holds one, and a
  pull of 10 cm takes it out of the book, with the game's own request. A
  grip that opens before that puts the miracle back. The debug switch "All
  miracles" gives up to 6 miracles with no cost in prayer power, for tests.
  The palm of that hand that turns up and stays up for 0.4 s also opens the
  book.
- While that palm is up, the gesture pictures of the game show in a row over
  it: what the player can draw now. While B is down, or while a chain or a
  power up goes on, the palm shows the row and not the book.
- The Forest miracle has a camera path of the game that turns the camera
  around the new forest. With the head on the camera the mod starts no such
  path, so the camera of the player stays (`miracle_camera_still=1`).
- Outside the influence of the player the hands fade, and a soft red glow
  shows the edge where each hand comes near it (`influence_fade`,
  `influence_wall`). With `influence_stream_hand=1` the stream of the game
  ends at the Hand's point in the air.

## Gestures in the plain game [V unless marked]

### Storage

| Where | What |
| --- | --- |
| `GGame+0x250064` | `GestureSystemDataList*`, the 81 templates of `Data\Gestures.jty` (loaded by `GGame::LoadFiles` at `0x005503BF`). The templates at `+0x08`, the count at `+0x0C` |
| `GGame+0x250068` | `GestureSystemData*`, a scratch path. Each match builds it again from the ring |
| `GGame+0x25006C` | `GestureSystem*`, the ring |
| `GGame+0x250070` | `GestureSystemResult*`: the type at `+0x00`, the start and end corners at `+0x08` and `+0x09`, the template number at `+0x0A` |
| `GestureSystem+0x08` | a ring of 80 samples, `0x28` bytes each |
| `+0xC88` | byte, the sample count |
| `+0xC8C` | the count of repeated samples |
| `+0xC90` | byte, the ring head |

The constructor of `GGame` (`0x0054B6D5` to `0x0054B772`) makes the list,
the path (`0x65C` bytes), the ring (`0xC98` bytes) and the result in that
order, at `+0x250064` to `+0x250070`.

One sample: `+0x00` screen x, `+0x08` screen y (floats from int pixels),
`+0x0C` the turn at this corner, `+0x10` the direction of the segment that
starts here, `+0x14..+0x1C` the land point under the pixel, `+0x20` the flags
and `+0x24` the angle of the segment. The flags are 1 for the oldest sample
(the start), 2 for a corner, 4 for the newest after a corner and 8 for the
newest. The direction is the angle in eight steps: 0 up, 2 right, 4 down, 6
left, with the screen's y down (`fn_00578730`).

A template (`0x65C` bytes, 1628 in the file after a 4 byte count) holds up to
80 corners of five floats (x, a pad, y, the turn, the direction), then the
corner count (`+0x648`), the gesture type (`+0x649`, `GESTURE_TYPE`), a kind
(`+0x64A`, 2 in every template), the aspect of its box (`+0x64C`), and three
switches: test the start direction (`+0x650`), accept the path backward
(`+0x654`) and test the aspect (`+0x658`). The corners are on a square of side
1. The game's own names of the 23 types are at `0x00BEF308`: spiral, inverse
spiral, S shape, circle, scribble, three, vertical, star, the four forks,
heart, R shape, square spiral, Cyrillic L, E shape, reverse S, infinity, W
shape, house, inverse square spiral and square wave. Several templates share
a type.

### Recording

- No button starts or ends a gesture. Each pointer move reaches the interface
  as message 0 (handler `0x005D99E0`). The handler copies the pointer to
  `GInterface+0x428` (`0x005CECF0`) and calls `fn_005CEAD0(pointer)`.
- `fn_005CEAD0` is thiscall on `GInterface`, one argument (the `int[2]`
  pixel), `ret 4`. It skips while the game is paused (`GGame+0x14` bit 2) or
  while the cooldown `GInterface+0x378` is above 0. It finds the land under
  the pixel with `fn_005E5620` (edx = the pixel, the land out and 0 on the
  stack, `ret 8`). With land it adds a sample with `fn_0057BBC0(&land,
  pixel)` (thiscall on the ring, `ret 8`). Without land it adds the pixel with
  the last land point (`fn_0057C8F0`), unless the ring is empty.
- A sample at the same pixel as the last one still goes in, and adds to
  `+0xC8C`. After 70 of them the ring clears. So a still pointer clears the
  gesture.
- `fn_0057C3F0` marks the corners after each sample. It looks back to the
  last corner for the sample with the largest turn, at least 0.2945 rad
  (`0x00D064FC`). Samples closer than 4 px do not count as a segment
  (`fn_0057C5C0`). Two corners closer than 12 px, or in a box under 50 px,
  merge into one (`fn_0057C630`). These are the only sizes in pixels.
- The ring clears on a pick up (`fn_005DC330`), a drop (`fn_005DC1E0`), a
  match (`fn_005CE420`), in fights (`0x005D4AC0`), when a leash goes on
  (`fn_005E6B00`) and when a Miracle leaves the Hand (`0x005D3340`).
  `ProcessPowerUpSystem` also clears it at its start in the Temple
  (`GGame+0x205A28 == 1`), while the interface is not active
  (`GInterface::IsActive`, `+0x28` bit 0), and while `GCamera::IsMoving`
  (`+0x74`). The camera mode can allow gestures during a move
  (`CanPlayerGestureWhenCameraMoving`). `CameraModeNew3` allows it only in a
  fight (`+0x44` and `+0x48`). So a land drag clears the ring, and a mouse
  gesture is drawn with no button held.

### Recognition

`GInterface::ProcessPowerUpSystem` `0x005CF300` (thiscall, no arguments)
runs once a frame from `fn_005D1120`. It makes a "looking for" list of the
gesture types that fit the moment, and it tests each of them with
`fn_005CF220(type)`:

| Moment | Types | On a match |
| --- | --- | --- |
| A Miracle in the Hand with its own gesture, and bit 1 of `GInterface+0x39` | its own gesture (`SpellSeedInfo+0x10C`), the circle | `fn_0057A5E0` places the circle, then `fn_005CE420(1)`. See "The circle of a cast" |
| A Miracle in the Hand (`fn_005CEF50`) | the scribble (5), and up to three types at `GInterface+0x16C` | the scribble drops the Miracle. The others power it up (`fn_005D0000`) |
| A chain of a miracle has started (`GInterface+0x364`, `fn_005CF1C0`) | the next gesture of each candidate miracle, and the scribble | `fn_005CFAE0`, see below |
| The Creature's leash is on (`fn_005CEFB0`) | the leash gestures | `GInterface::DoLeashSelectionSystem` |
| Other states of the interface status (`+0x90`, `+0x12C`), not traced | the scribble (5) | packet `0x1E` or `0x5F`, or `fn_005D4350` |
| An empty Hand (`IsHandReadyForObject`) | the two start types of `GSpellSystemInfo` (`+0x14` and `+0x18`, at `0x00DA0574` and `0x00DA0578`). Each only when a Worship Site of the player has a miracle icon that may be asked for (`WorshipSpellIcon::ValidForRequestSpell`) and whose first gesture is that type | `fn_005CF010`: the chain starts |
| An empty Hand | the "again" type (`GSpellSystemInfo+0x10`, `0x00DA0570`), for the last miracle (`GInterfaceStatus+0x34`) | the same miracle again |

**A miracle is a chain of two or three gestures.** Each miracle icon has three
gesture types (`+0x100`, `+0x104` and `+0x108` of its `GSpellSeedInfo`). The first
is one of the two start types. A match of a start type (`fn_005CF040`) lists
the miracles that start with it, sets step 1 (`GInterface+0x360`) and turns
the chain on. Each next match (`fn_005CFAE0`) keeps the miracles whose gesture
at this step is the one drawn. When a miracle has no next gesture, or at step
2, the game sends packet `0x25` with its spell seed type (`fn_00550C20`), and
the Miracle comes into the Hand (hand state 8, Grain). The chain ends after
`GSpellSystemInfo+0x1C` seconds (`0x00DA057C`) with no match, and the
scribble ends it at once. Each piece is a match of its own, so each piece
clears the ring and starts the 0.4 s cooldown. [V for the code. I for how a
player sees it: as one gesture with a common start]

`fn_005CF220(type)` builds the path from the ring and calls the match.
`fn_00578C20` (thiscall on the path, the ring as the argument, `ret 4`) copies
the corner samples (flags `& 0xB`) and the aspect of their box.
`fn_00579F10` (thiscall on the list, `(type, path, result)`, `ret 0xC`) then
tests each template of the type with `fn_0057A050`:

- It tries each corner of the path as the start, so a gesture can end a
  longer path.
- The turns of the path and of the template must agree to 0.589 rad
  (`0x00D064BC`) at each corner. A turn under 0.515 rad (`0x00D064C0`) can
  merge with the next one.
- With `+0x650` the first direction must be the template's (out of eight).
  With `+0x654` the path may also run backward (`fn_0057A3E0`).
- With `+0x658` the aspect of the box must be in the same class as the
  template's: under 0.15, 0.15 to 4, or over 4 (`fn_00579FA0`). The aspect
  is the width over the height, divided by the width over the height of the
  game's screen (`0x00E85058`).

So the match reads angles and directions, not the size. The size only has to
be well above the corner rules (4, 12 and 50 px). The land points take no
part in it. `0x00579F70` (thiscall on the list, `(index, path, result)`,
`ret 0xC`) tests one template. An unused debug function (`0x0057B980`) runs
it on every template and prints the names.

**On a match** `fn_005CE420(1)` clears the ring, sets the cooldown
`GInterface+0x378` to 0.4 s (`0x00BF1AF8`) and starts Immersion effect 3. Its
first call (`0x00689790`, which the symbol file names
`GJMesh::GetBoundingBox`) makes a `PSysGesture` from the land points of the
ring: the game draws the matched gesture on the land. No trail shows while
the gesture is drawn [I: no other code reads the ring's land points to
draw]. `GInterface::Get3DSoundPos` puts a sound at the newest land point.

**How the game teaches it.** At the end of each run, `ProcessPowerUpSystem`
gives the "looking for" list to `fn_0068ABA0`. That makes a `DisplayGesture`
for each type the game is looking for, only when the list changes [V]. This
is the picture of the gestures that the player can draw now [I]. The matches
also start help events (`HelpProfile::Trigger`): `0xE` (again), `0xF` (a
chain starts), `0x10` (a next piece), `0x11` (the Miracle), `0x14` to `0x16`
(a scribble) and `0x17`. The `SpellDispenser` class (`0x00722730`) was not
traced.

### Feeding a 3D path

The mod draws a gesture with the real hand through the game's own ring.

- While the draw button is held, the point of the draw goes on a plane that
  faces the head at the start of the stroke, with the head's right and up
  axes. The point of the draw is the tip of the index finger (`gesture_point=1`,
  the default) or the grip point (0). The tip is the tip of the hand model's
  index finger at its rest curl, at the real hand's length, turned with the
  grip pose. The trigger does not move it, so a press does not bend a
  gesture. A turn of the wrist moves the tip in the plane, as a finger
  moves when it draws in the air.
- The plane becomes pixels at `gesture_size_cm` (30) for a third of the
  render height, around the controller's pixel at the start.
- A pre hook on `ProcessPowerUpSystem` adds a sample when the pixel has
  moved a fortieth of the render height, and first a corner between the two
  when there is one. It adds the samples through the game's own
  `fn_005CEAD0`, so the pause, the cooldown and the land under the pixel work
  as they do for the mouse. A still hand adds nothing, so the 70 repeat rule
  never clears a stroke.
- A pre hook on `fn_005CEAD0` drops the game's own samples of the pointer
  while the controller owns it.

## A miracle in the Hand in the plain game [V]

### Two classes

A miracle is in one of two objects:

- `OneOffSpellSeed` (vtable `0x008F3774`, a `MobileObject`): the bubble of a
  reward, of a spell dispenser or of a script. It is an object on the land.
  `OneOffSpellSeed::Create` `0x0072A2F0` makes it (cdecl: a `MapCoords*`, the
  seed type, the power up, a float, and the caller cleans `0x10` bytes).
  `Reward::InterfaceTap`, `SpellDispenser::CreateOneOffSpellSeed`
  `0x00722B80` and the scripts call it. The seed type is at `+0x68`.
- `SpellSeed` (vtable `0x00981FC0`, an `Object`): the miracle that is active
  in the Hand. Its info is at `+0x58`, the icon of its Worship Site at
  `+0x5C` (null for a seed from a bubble), the spell that runs at `+0x60`,
  the power up at `+0x6C`, and the byte "ready" at `+0x90`.

**A one-off seed becomes active with a tap.** A right press on the seed
starts interface action 13 (`FUN_005D1740`, with the time of the press at
`GInterface+0x454` and the game turn at `+0x458`). Its step `FUN_005D5250`
takes the time since the press: the real time, or 100 ms for each game turn,
the smaller of the two.

- The button goes up before 226 ms: `fn_005D3930` (the click) runs, and
  `fn_005D38A0` sends the tap, packet `0x20`. The tap needs
  `InterfaceValidToTap` and the influence at the land point of the record
  (`+0x48`).
- The button stays down for more than 224 ms: the game picks the seed up as
  an object. The Hand carries it in the Holding state.

Packet `0x20` runs `FUN_005DA650` and then `OneOffSpellSeed::InterfaceTap`
`0x0072A640`. That calls `CreateSpellIntoHand` `0x0072A730`: it needs
`IsHandReadyForObject`, makes a `SpellSeed`, puts it in the Hand
(`PlaceObjectInMagicHand`) and makes it ready. Then it starts Immersion
effect 14, plays sample `0x6D` and deletes the one-off seed.

**A seed from a Worship Site** comes from the chain of gestures above:
packet `0x25`, `FUN_005DABA0`, `FUN_0064BDB0`. It has the icon link at
`+0x5C`. `SpellSeed::ProcessInHand` `0x00729930` makes it ready after the
time at `0x00DA0584`. The icons, the chants and the tap on an icon are in
`village.md`.

### The states

With a `SpellSeed` in the Hand, `CHand::GetRequiredState` gives state 8,
Grain, and the hand mode chooser `fn_005D7F20` gives hand mode 5 (`0x18` for
each other object in the Hand). `HandStateGrain::Update` `0x005B3310` calls
`HandStateHolding::Update` at `0x005B3361`. So the Grain state places the
Hand as the Holding state does, with the ray, the distance and the pose
builder. Then it keeps the place of the Hand at `+0x48E0`, and a filter of
the speed of the Hand at `+0x48C8`: the throw velocity. The rate of the
filter is `-10 ln(1 - 0.8)` a second (`0x00BF014C`), a time constant of
62 ms. A one-off seed that the Hand carries is in state 4, Holding.

### The seed info

`GSpellSeedInfo`: 30 records of `0x190` bytes at `0x00D9D678`
(`load_variables`, `0x0042C28E`, the block `DETAIL_SPELL_SEEDS`). In
`Scripts\info.dat` the records are `0x180` bytes from offset `0x3D28`, and a
field at `+X` in memory is at `+X - 0x10` in the file.

| Field | Meaning |
| --- | --- |
| `+0x100` | The first gesture of the chain that asks for the miracle: 1 (the spiral) for a miracle of the god, 2 (the inverse spiral) for a Creature miracle |
| `+0x10C` | The gesture that the cast needs, or 0 |
| `+0x110` | How the miracle is cast: 0, 1 or 2 (below) |
| `+0x114` | 1: the seed stays in the Hand while its spell runs (`IsSpellKeptInHand`) |
| `+0x118` | 1: the game can cast it on an object (`SpellSeed::CanCast` `0x00729190`) |
| `+0x124` | The magic type of each power up, 4 of them |
| `+0x134` | The gesture of each power up |
| `+0x168` | 1: the seed is deleted after the cast (`IsThisSeedDeletedOnceCast`) |
| `+0x184` | The help text of the tooltip |

| Type | Seed | Cast | Kept | On an object | Its gesture | Power up gestures | Deleted |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | Storm | release | no | no | circle | inverse spiral, spiral | no |
| 1 | Forest | press | no | no | none | none | no |
| 2 | Fireball | release | no | no | none | inverse spiral, spiral | yes |
| 3 | Food | pour | yes | no | none | spiral | no |
| 4 | Spiritual Shield | release | no | no | circle | none | no |
| 5 | Physical Shield | release | no | no | circle | none | no |
| 6 | Lightning | pour | yes | no | none | inverse spiral, spiral | no |
| 7 | Heal | press | no | no | none | spiral | yes |
| 8 | Wood | pour | yes | no | none | none | no |
| 9 | Water | pour | yes | no | none | spiral | no |
| 10, 11 | The two flocks | release | no | no | none | none | yes |
| 12 to 27 | The Creature miracles (16 is Strong) | press | yes | yes | none | none | yes |
| 28 | Teleport | press | no | no | none | none | yes |
| 29 | Explosion | press | no | no | none | inverse spiral, spiral | yes |

### The buttons and the cast

The left button drags the land, as with an empty Hand. The right button, the
Action button, casts. `FUN_005D1330` (the right press) goes to `FUN_005D1560`
when the Hand holds an object (`GInterfaceStatus+0x90`):

1. The object is `CHand::GetCreatureToGiveTo`, or else the object of the next
   pick record (`+0x400`). When `ValidToApplyThisToObject` (slot `0x71C`,
   `0x007286D0`) gives 1 for it, the miracle is cast on it: `fn_005D30D0`
   sends packets `0x4D` and `0x11`. A miracle of cast kind 1 first waits for
   the release (action 9).
2. With no such object, and the land point of the record (`+0x3F0`) inside
   the influence (`+0x48`), the land point is the target. Cast kind 1 starts
   action 8 (`FUN_005D2730`). The other kinds go to `FUN_005D1850`, and that
   calls `FUN_005D3340`: packets `0x4D`, `0x16` and `0x12`.
3. With no such object and the land point outside the influence, the press
   does nothing: no cross and no sound.

`ValidToApplyThisToObject` needs the seed ready, an object that is no scroll,
and `SpellSeed::CanCast`. That passes for four kinds of object:

- The place of a Worship Site of the player that takes the miracle back
  (`IsSpellSeedReturnPoint`, slot `0x670`).
- A spell dispenser (slot `0x1F4`).
- A magic fireball (slot `0x218`).
- With `+0x118`: an object where the cast rule passes at its place, and that
  passes the test of the magic info (vtable `+0x2C`).

Only the Creature miracles have `+0x118`. So no miracle of the game is cast
on a villager: Heal is cast at a place.

**How each kind is cast** (`+0x110`):

- **0, the pour** (`IsSpellCastInHand`): food, wood, water, lightning. The
  press starts action 10 on the land (or 11 on an object). While the button
  is held, the step `FUN_005D4C10` sends the cast again at the land point of
  the record, once a game turn. The release sends packet `0x1A`
  (`ApplyUnlockProcess`). The seed stays in the Hand.
- **1, the release** (`ApplyOnlyAfterRecSystem`): the press starts action 8
  and clears the ring of the gestures. The game records the path of the
  pointer while the button is held. The release (`FUN_005D48D0`) calls
  `FUN_005D3340`, which sends the throw velocity `CHand+0x48C8` in packet
  `0x4D`. A fireball and a flock fly with that velocity. A storm and the two
  shields need their own gesture, the circle. See "The circle of a cast".
- **2, the press**: the cast is at once, on the object or at the land point.
  A Creature miracle stays in the Hand while its spell runs (`+0x114`), and
  `ProcessInHand` deletes the seed when the spell ends.

### The circle of a cast

Storm and the two shields have their own gesture (`GSpellSeedInfo+0x10C`,
the circle). `ProcessPowerUpSystem` tests it at `0x005CF4E0` when all of
these are true:

- Bit 1 of `GInterface+0x39` is set. The press of the right button sets it [I].
- The Hand holds a miracle (`fn_005D3880`), and the miracle has its own
  gesture (`fn_00729AC0`).
- The game holds no circle now (`GInterface+0x17C` is 0).

A match (`fn_005CF220`) of that type closes the circle. `fn_0057A5E0` gives
the place and the size of the circle from the path: for a template of kind
2 (`+0x64A`), the place is the middle of the box of the land points of the
path, and the size is the width of that box across the view of the camera,
times 1.05. The game keeps the circle at `GInterface+0x180` to `+0x190`,
sets `+0x17C` to 1, and clears the ring (`fn_005CE420(1)`). The circle
stays for 5 s (`+0x194`), and then `+0x17C` goes back to 0.

The release of action 8 (`FUN_005D48D0`) calls `FUN_005D3340`. With
`+0x17C` set, the cast takes the circle as its gesture (`+0x1A4`) and the
middle of the circle as its land point, and it clears `+0x17C`. With no
circle, the gesture test (`fn_00729AF0`) fails and the game refuses the
cast. A refused cast keeps the circle until its 5 s end.

### Power ups

Only a seed from a Worship Site can power up. `fn_005CEE80` lists the
gestures of the power ups that the player has and that the site can pay
(`fn_0077FBF0`), at `GInterface+0x16C`. `fn_005D0000` matches them, and a
match sends packet `0x6A` (`FUN_005DAC30`, `SpellSeed::SetPowerUp`
`0x00729B30`, which takes the chants from the site). The gestures are the
spiral and the inverse spiral. A shake of a seed with a power up takes the
power up off first.

### The cancel

The shake is the scribble gesture, type 5. For a seed from a Worship Site
`fn_005D0000` matches it. For each other object in the Hand
`ProcessPowerUpSystem` matches it at `0x005CF6ED`, when the land point is
inside the influence and `ValidToShakeFromHand` (slot `0x710`) gives 1. A
match calls `PHandFX::DoRemoveFromHandVisual` (`0x0068CE90`),
`fn_005CE420(0)` and `fn_005D4350`. That sends packet `0x4D` with no velocity
and packet `0x1D`. The handler `FUN_005DA8F0` calls
`SpellSeed::ThrowObjectFromHand(status, 1)` `0x0072ACD0`: a seed from a
Worship Site gives its chants back to the site, and each seed is deleted. So
a one-off miracle that is shaken out of the Hand is lost.

### The refusal

`fn_005D18F0` (the held object and a `MapCoords*`, `ret 8`) plays sample
`0x25` and sends packet `0x2B`, which makes the yellow cross at the land
point. Its callers for a miracle:

| Caller | The cause |
| --- | --- |
| `FUN_005D1560` (`0x005D16B4`) | Cast kind 1, and the land point is not valid |
| `FUN_005D1850` (`0x005D18D2`) | Cast kind 0 or 2 with no object, and the land point is not valid |
| `fn_005D30D0` (`0x005D32E4`) | The gesture at `+0x1A4` is not the seed's own gesture (`fn_00729AF0`) |
| `FUN_005D3340` (`0x005D364E`, `0x005D3663`) | The same gesture test, a land point off the map, a land point that is not valid, or no influence at it |

A land point is valid (`ValidToApplyThisToMapCoord` `0x00728720`) when the
seed is ready, the cast rule of the magic info (`GMagicInfo+0x34`,
`fn_005FB5D0`: 0 anywhere, 1 on land, 2 in the influence, 3 on land in the
influence) passes, and the test of the magic info (vtable `+0x30`) passes. A
Creature miracle fails that last test at each land point. No test of the
power is in these functions: the game takes the power when it makes the seed
and at a power up.

### The Immersion effects

The names are in the game's own enum (`IMMERSION_EFFECT_TYPE` of `Enum.h`).
Those of a miracle: 3 a gesture is known, 8 the trail of a gesture, 9 a
miracle crackles, 14 a one-off seed pops, 18 to 20 a fireball, 21 lightning,
23 a miracle is cast, 24 a shield, 25 water, 26 and 27 a flock, 28 a
Creature miracle. `DoPostCastThings` `0x00729260` starts the effect of the
magic info (`+0x14`) when the spell starts.

## What the mod does (`miracle_hand=1`)

The code is in `src/mod/miracle.cpp`, with the pure math in
`miracle_math.h`. `hand_direct.cpp` places the Hand and holds the pick
record, `controllers.cpp` gives the buttons, `hand_glow.cpp` draws the halo
and `hint_card.cpp` the lines of the card.

### The Hand is the real hand

`DirectState` of `hand_direct.cpp` has the Grain state. The Grain state runs
the update of the Holding state, so the hooks of the ray, the distance and
the pose builder place it with no other change. The land point of the pick
record is the land under the Hand, as in the Holding state. So the
influence, the pour and a cast at a place follow the real hand. With
`miracle_hand=0` the game places the Hand with a miracle, and the Hand of
the game is then away from the real hand.

### The target

`MiracleSelect` runs at the end of the first eye pass. A candidate is an
object that the game casts this miracle on: `ValidAsInterfaceTarget` and
`ValidToApplyThisToObject`. The body is a ball of the 2D radius of the
object, about the body bone of a Creature. The target is
(`miracle_math::AimAt`, `Better`):

1. The body within `miracle_touch_cm` (3 cm of the room) of the Hand's
   point. Of two, the nearer one.
2. Else the body within `miracle_cone_deg` (6 degrees) of the aim ray, and
   `miracle_max_m` (4 m of the room) from its start. Of two, the one nearer
   to the ray.

**The candidates.** The Creature of each player comes from the game
(`GPlayer+0xA4C`, 8 players), with its body from `BodyPlace`. Each other
candidate is an object that the draw offered: near the Hand, or in the aim
cone, which the draw offers while a miracle is in the Hand
(`MiracleWantsCone`). The draw alone is not enough for a Creature. It offers
an object in the aim only when the point of its mesh, the feet of a
Creature, is in the cone of the far grab, and the body of a Creature is 7
units and more above its feet. So the mod takes the Creature from the game.

**The target stays** (`miracle_math::Takes`). The target of the last frame
has wider limits: `miracle_keep_deg` (10 degrees), twice the touch and a
quarter more range. A new body takes its place only when it is at the Hand
and the target is in the aim, when it is nearer at the Hand, or when it is 2
degrees nearer to the ray. So a Creature that walks keeps the halo.

The target has the gold halo of a target of the grip (`MiracleHalo`,
`kHaloMiracle`). A Creature that the game does not let the miracle act on
has the red halo, and the card says why.

**The pick record holds the target.** `MiracleKeepRecord` runs after each
eye pass, before each interface step and before each right press. It puts
the target in the next record with a distance that the game keeps
(`near_math::RecordDistance`). With no target it takes each object out of
the record. So a press casts on the object with the halo, and on no object
that is behind the pixel of the Hand.

### The grip casts

The grip is the game's Action button one to one while a miracle is in the
Hand (see `grab.md`, "The grip holds").

| Kind of miracle | What the player does | What the game does |
| --- | --- | --- |
| A Creature miracle | Close the grip with the halo on a Creature | `fn_005D30D0`: the cast on the object |
| Heal, Forest, Teleport, Explosion | Close the grip with the Hand over the place | `FUN_005D3340`: the cast at the land under the Hand |
| Food, Wood, Water, Lightning | Keep the grip closed over the place | Action 10: a cast each game turn. The opening ends the pour |
| Fireball, the flocks | Close the grip, swing the hand, open the grip | Action 8: the release sends the speed of the Hand |
| Storm, the two shields | Close the grip and draw a circle with the hand | The game matches the circle, and the cast comes at once, at its place. The grip that opens then does nothing more |

**The throw** uses the game's own speed of the Hand: the Grain state filters
the motion of the Hand, which is the motion of the real hand. The mod does
not change it.

**The circle.** The mod feeds the ring of the gestures while the grip is
closed with a miracle that needs a circle, and the interface action is 8 or
9. The path of the hand goes to the ring as it does for the draw button
(`MiracleDrawsCircle`, `ControllersGestureInput`, "Feeding a 3D path").

**The circle is under the path of the real hand** (`miracle_circle_hand=1`).
The game makes the circle from the land under the pixels of the path,
through its own camera (`fn_0057A5E0`). The mod feeds the path as pixels on
a plane around the pixel of the controller, so the land under those pixels
is far from the hand: the middle of the circle can be 4 m of the room away,
and the size more than 2 m. The mod keeps the Hand's point of each frame
while the grip draws the circle. After the match, and before the cast reads
the circle, `CircleFromHand` puts the middle of the circle (`GInterface+0x180`)
on the land under the middle of that path, and the size (`+0x18C`) at 1.05
times the widest span of the path across the land. That is the rule of the
game with the real hand. The cast copies the circle to `+0x1AC`, and the log
line `the cast of ... goes to the middle of the circle` gives that place and
size. The line `the game matched the circle of ...` gives the circle of the
game, the path of the Hand and the new circle. A shield or a storm costs
prayer power for each second in the square of its size
(`SpellShield::CalculateCostToMaintain` `0x0072B7F0`). A one-off miracle has
no Worship Site that pays (`GPlayer::MaintainSpell` `0x0064C430` gives 0 to
a player), so the life of its spell is the life at the cast.

**The circle casts when it closes** (`miracle_circle_cast=1`). When
`GInterface+0x17C` goes from 0 to 1 while the grip holds action 8, the grip
looks open to the game until the real grip opens (`MiracleFrame`, the same
mask as the use of a seed). The game's own release then casts at the circle
in the next step, with the grip still closed. The opening of the real grip
then sends no second release, so it gives no second cast, no throw and no
drop. Only a new match counts: a circle that a refused cast left at
`+0x17C` does not cast at the next press, and the release casts it as the
game does. With `miracle_circle_cast=0` the cast comes when the grip opens.

**The bolt of Lightning starts at the Hand's point.** A fork of lightning
in the Hand starts at the gesture place of its particle manager
(`UR_Lightning::ActualModify` `0x006914C0`, the manager `+0x30`, with the
property `CastingFromHand` of `SF_LightningBolt`). The game gives the
gesture place to the spell on each game turn
(`GInterfaceStatus::UpdateSpellInfo` `0x005DC8F0`, called by
`Spell::ProcessMaintainRequest`): a copy of `GInterfaceStatus+0xC8`, the
Hand (`CHand+0x78`) that packet `0x15` or `0x16` sent and that the last game
turn took over. The spell then puts its cast place (`Spell+0xCC`) under that
place. The mod puts `CHand+0x78` at the Hand's point, the tip of the index
finger at the rest curl (`hand-model.md`), so the bolt starts there with no
other change. The fork ends on objects and on the land within 60 units
(`SearchRadius`) of the Hand's point, in a cone around the direction in
which the camera of the game looks, not the head. Near the Temple the fork
goes to the Temple, so the bolt runs between the spire and the Hand's point.

The log line `the spell ... gets its gesture place from the game at ...`
comes for each new spell of a pour and for each 20 updates. It gives the
place of the game, its distance from the Hand's point and its height over
it, and the distance of `GInterfaceStatus+0xC8`, `+0xA4` and `CHand+0x78`
from the Hand's point. A desktop run shows 0.0 cm for each one.

Open: the eye pictures of a desktop run show a red beam from the sky to the
spire of the Temple at the cast. `SF_LightningBolt` has the fork, a glow and
a light map, and no beam from the sky. The game draws two streams of light
in the color of the player, the red of the influence ring for player 0.
Each is a `SpellPointInf` (`0x0069AC10`) from one point to another:

- The stream of the influence (`GVirtualInfluence::Draw`,
  `PSysGlobal::AddManaPathSprite`, `influence_edge.cpp`), from the edge of
  the influence to the land under the Hand.
- The prayer of a Worship Site for a spell (`Spell::CreateSpellPoint`
  `0x007213D0`, `WorshipSite::CreateManaPathSprite` `0x0077B2C0`), between the
  place of the spell (`Spell+0x14` or `Spell+0xCC`) and the thing at
  `Spell+0xA0`. It comes for each 100 of the power of a spell that has a
  Worship Site (`Spell::GetWorshipSite`).

The mod does not change these streams by default. With the test key
`influence_stream_test_log=1` the log has a line for each stream, 24 times
and then each 200th time: the caller, the two ends, the height of each end
over the land, the color and the Hand's point. Each report window also has
the count of the streams, also when it is 0. A desktop run of a Lightning
from a one-off seed with the key logged no stream. A one-off seed has no
Worship Site, and the Hand stayed in the influence [I]. Its eye picture after the
pour shows a column in the sky in the red of the influence ring (255 70 70),
and not at the spire.

The mod draws one thing in that red: the glow at the edge of the influence
(`influence_wall`, "The glow at the edge"), a patch of 12 cm of the room at
the edge nearest to each hand. Its lines `influence    : the glow at the
edge` say when it shows and where. The game draws its influence ring in the
same red. A run with `influence_wall=0` tells the two apart.

### A teleport site in the pick (`miracle_site_pick=1`)

`MagicTeleport::Draw` (`0x005FCCC0`) gives the site to the pick of the game
as a ball of 3 units (`GInterface::SendInvisibleDrawCollision`
`0x00519960`) while its spell has a seed (`Spell+0xAC`) [V]. So in the plain
game the pointer can take a site and move it, and a villager in the Hand
goes onto a site: `Villager::ApplyThisToObject` calls
`MagicTeleport::ApplyVillagerDirectlyToTeleport` (`0x005FC4F0`), and
`MagicTeleport::DoTeleport` sends him to the other site that is nearest his
destination. With one site, `DoTeleport` has no other site and gives 0.

The pick of the real hand takes its candidates from the meshes that the
draw offers, and a site has no mesh. So the hook on
`SendInvisibleDrawCollision` (`temple.cpp`, which also offers the leash
posts) gives a site the ball of the game as a candidate of the real hand
(`MiracleOfferSite`, `HandDirectOffer`). The game's own tests then say what
the grip does to it, and a villager in the Hand goes onto a site as onto a
house ("An object in the Hand that goes onto a thing", `grab.md`). The log
line `the teleport site ... is a candidate of the real hand` comes for each
new site. With `miracle_site_pick=0` the real hand has no site as a
candidate.

A desktop run (`miracle_run.ps1 -Kind teleport`) makes two sites about 35
units apart before the door of the Temple. The villager that the Hand puts
on site A goes into it and comes out at site B: 0.5 s to 5 s after the
teleport he is 1.4 to 5.1 units from site B. With `miracle_site_pick=0`
the site is no candidate, the game sets the villager down, and no teleport
comes.

### The way out

Two inputs take a miracle out of the Hand:

- A shake of the real hand: the detector of the leash
  (`command_math::ShakeStep`, 4 strokes of 6 cm at `miracle_shake_mps`,
  0.6 m/s).
- A short press of the second face button of the pointing hand (B, or Y for
  a left handed player): at most `miracle_cancel_ms` (400 ms) and 5 cm of
  hand motion. A longer press, or a press with motion, is the draw of a
  gesture.

The cancel runs before the next interface step (`MiracleInterfaceStep`). It
does what the game does after a match of the scribble: the visual of the
Hand, `fn_005CE420(0)` and `fn_005D4350`. The mod makes no test of the
influence, so the way out works at each place. The game's rule stays: a
one-off miracle that is cancelled is lost, and a miracle of a Worship Site
gives its power back.

### A seed that the Hand carries

The grip picks a one-off seed up as an object, so a player can move a seed.
A press shorter than 225 ms makes it active at once, by the rule of the
game. While the Hand carries the seed, the trigger makes it active
(`miracle_seed_use=1`):

1. The trigger does not reach the game. The grip looks open to the grip
   machine, so the Hand lets the seed go (the click of "The grip holds" in
   `grab.md`).
2. One frame after the Hand is empty, the mod calls the game's tap
   (`fn_005D38A0`, packet `0x20`).
3. The miracle is in the Hand. The grip stays up for the game until the real
   grip opens, so a closed grip does not cast by accident.

The game needs the land under the Hand inside the influence for the let go
and for the tap. `miracle_math::UseStep` ends the use after 120 frames with
no miracle, and the card says "Use it inside your influence".

### The card

`MiracleHint` has the context "a miracle in the Hand". Its lines stay while
the miracle or the seed is in the Hand. The hints of the game for a button
go, and one line of its text stays (the name of the miracle).

| State | Lines |
| --- | --- |
| The Hand carries a one-off seed | "[Trigger] Use the miracle", "[Grip] Open to drop" |
| A miracle with a target | "[Grip] Cast on the Creature" (or "Give the miracle back", "Put it in the dispenser", "Cast on the target") |
| A Creature miracle with no target | "Touch a Creature, or point at one" |
| A miracle for a place | "[Grip] Cast here" |
| A miracle to pour | "[Grip] Hold to pour", and "[Grip] Open to stop" while it pours |
| A miracle to throw | "[Grip] Hold, swing, open to throw", and "[Grip] Open to throw" while the grip is closed |
| A miracle that needs a circle | "[Grip] Hold and draw a circle", and "[Grip] Draw a circle to cast" while the grip is closed ("[Grip] Draw a circle, then open" with `miracle_circle_cast=0`) |
| The spell of a Creature miracle runs | "The miracle acts now" |
| Each state of a miracle | "[B] Tap to cancel, or shake" |

After a refusal the card has the reason for 4 s (`miracle_math::RefusalOf`,
the tests of the game in its order): "The miracle is not ready yet", "The
Hand is not over the land", "This miracle needs land here", "This place is
outside your influence", "The Creature is outside your influence", "It needs
a Creature as its target", "Draw a circle on the land first" or "The game
refused the cast here".

### Haptics

The kind `cast` (0.45 for 30 ms) plays on the game's effects of a cast (18 to
21 and 23 to 28). `activate` plays when a one-off seed pops (effect 14).
`refused` plays once for each press that the game refuses. `let go` plays at
a cancel, and `target` for a new target.

### A sound with no Base

`PHandFX::DoRemoveFromHandVisual` plays sample `0x77` with the `PHandFX` of
the Hand as the object of the sound. The audio code asks each object of a
sound for its place through slot `0x10` of its vtable,
`Base::Get3DSoundPos` (one argument, `ret 4`). `PHandFX` derives from
`PSysHandFX` only, and its slot `0x10` is `PHandFX::DrawHandFX`, with no
argument and a plain `ret`. A call through that slot leaves 4 bytes on the
stack, and the stack check of the caller ends the game (`0xC0000409`,
`___report_gsfailure`). The game's own shake of a miracle has the same
fault.

`audio_object.h` (`AudioVtableIsBase`) reads the RTTI of the vtable: the
locator at `vt - 4`, the hierarchy at `+0x10`, and the names of the base
classes. `audio_sources.cpp` and `audio_trace.cpp` call slot `0x10` only for
a class that is `Base` or derives from it. On the exe 553 of the 949 classes
with RTTI derive from `Base`.

### The spell book (`spell_book=1`)

The code is in `src/mod/spell_book.cpp`, with the pure math in
`spell_book_math.h`. The book shows the miracles that the player can ask for
now, and the pointing hand pulls one out with the grip. No gesture is
necessary. The gestures stay as they are.

**The button.** With `button_layout=1` the first face button of the hand that
does not point (X, or A when the left hand points) opens the book. A second
press closes it. The book also closes when the pointing hand takes a
miracle, when the ring opens, and when a dialog box or the game menu is up.
The book does not open in the Temple, in a cut scene, while the ring shows,
or while the controllers cannot act. Then the press gives the pulse
`refused`. On an Index controller that button sends Esc, and with
`button_layout=0` it opens the ring, so it does not open the book there.

**The place.** The center of the book is 12 cm from the grip point along the
normal of the open palm, and 2 cm up (`BookCenter`). With the palm up, the
book is over the palm. With the palm to the side, it is between the two
hands. The book moves with the hand, one to one. The picture faces the head
with no roll, as the ring does. It has no motion of its own.

**The list is the game's own.** An item is a miracle with an icon at a Worship
Site of the player (`GPlayer+0xA48`, the Citadel, and its 6 sites at `+0x34`)
whose magic type is on (slot `0xD4` of the icon). The game's test of a
request (`fn_0064BEC0`, thiscall on the `GPlayer` with the
`GInterfaceStatus` and the seed type) says if the player can ask for it now.
An item that fails the test is dim, with the reason: "The Hand must be
empty", "The miracle charges now", "No prayer power. Villagers must
worship" or "The game refuses it now". The last miracle
(`GInterfaceStatus+0x34`, the miracle of the gesture "again") is the first
item. When the hand is on it, the title of the book has "(again)" after its
name.

**The picture.** Each item is a bubble with the orb of the game in it: the
glass ball with the miracle in it, as on the land and in the Hand (see "The
orbs in the cells" below, `spell_book_orbs=1`). The name is under the
bubble. The cells are 6.5 cm for up to 9 items, 5 cm for up to 16 and 4 cm
for more, and a bubble is 0.66 of a cell across. The book uses the quad
layer of the ring (`kCardRing`), so it is sharp and never double. The bubble
that the hand pulls out is the bubble of the game too (see "The pull"). A
book with no item is 20 by 6 cm. It says "No miracle to ask for now" and the
reason: "You have no Worship Site" or "Your Worship Site has no miracle".

With `spell_book_orbs=0` each bubble is a flat picture of the mod with the
pictures of the two gestures of the miracle: the start of the chain (the
spiral or the inverse spiral) small at the left, and the gesture of the
miracle at the right. A dot marks where each gesture starts. The pictures
come from the first template of each gesture type in `Data\Gestures.jty`, as
the game holds them (`GGame+0x250064`). A miracle of the god has a blue
bubble, and a Creature miracle an amber one. When a check of the game's code
of the bubble fails, the book shows these pictures too.

**The words fit.** The title, the note and each name fit their box
(`text_fit.h`, `ui-layer.md`, "Words that fit"). A text that is too wide gets
a smaller font, down to 70 percent of its size. Then the title and the note
of a book with no item go on two lines. No text of the book ends in an
ellipsis.

With `hint_card_style=1` (the default, the launcher row "Card look") the
book is a page of the parchment of the hint cards (`controller-hints.md`,
9.11): opaque, with a torn and burnt edge 4 px inside the picture, so each
cell keeps its place. The words are in dark brown Alegreya at the capital
letter height of Segoe UI, a dim item is light brown, and the reason of a
refusal is red brown. The flat pictures (`spell_book_orbs=0`) keep their blue and amber fill
with white gestures. The cell under the hand is tan with a deep gold halo, and the
other bubbles have a brown ring. The halo stays inside the tan cell: its
outer edge is 4 px below the top of the cell or 0.16 of a cell out from the
bubble, and it is 0.10 of a cell wide or more, so the fill of that bubble
is a little smaller (`spell_book_math::BubbleRing`). With `hint_card_style=0` the book is the
dark panel with an alpha of 0.86 and white Segoe UI letters.

**The pick.** The grip point of the pointing hand within 45 percent of a cell
of the middle of an item, and 6 cm in front of or behind the book, picks it
(`Pick`). A hand in front of the page shows in front of it, in each eye, and
a hand behind it is under the page (`card_hand_front=1`, `ui-layer.md`, "A
hand in front of a card"). With `spell_book_point=1` the page shows a small
ring with a dot at the foot of the grip point while that point is 9 cm or
less from the page: it fades with the distance. While the book is open the
glow at the Hand's point does not show, because a grip on the page takes
from the book and not from the world. The item stays until the hand is 65 percent of a cell away, or 9
cm in front or behind. The item under the hand gets a gold halo and a light
cell. The title of the book and the card of the pointing hand name it:
"[Grip] Hold and pull out Fireball". Each new item gives the pulse `tick` on
the pointing hand and the cue "ring move".

**The pull** (`spell_book_pull_cm=10`). The grip on a picked item that the
game lets the player ask for does not take the miracle at once:

1. The bubble of the item comes out of the book on an elastic band. It is
   the game's own bubble of that miracle (`spell_book_bubble=1`, see "The
   game's bubble" below): a glass ball 6 cm across in the room, with the
   icon mesh or the particles of the miracle in it. It stays half way from
   its cell to the grip point of the pointing hand (`PullBubble`). Its cell
   in the book shows an empty socket. The pick stays on the item.
2. Each frame of the pull sends a buzz of 20 ms to both hands: the pointing
   hand at 0.03 to 0.45, and the hand of the book at half of that. The
   buzz goes up with the pull, faster near the end (`PullBuzz`), times
   `haptics_pct`. The grip at the start gives the pulse `take`.
3. When the gap from the cell to the hand is 10 cm more than at the grip
   (`PullShare`), the bubble snaps into the hand: the take below, a pulse of
   0.65 for 35 ms on the pointing hand and 0.26 on the other hand. The
   bubble stays at the hand until the Miracle is in the Hand, at most 20
   frames. Then the Miracle of the game is in the Hand.
4. A grip that opens before the snap puts the miracle back, with no cost.
   The pointing hand gets the pulse `let go`. The bubble springs back to its
   cell in 0.4 s: it goes through the cell one time and comes back
   (`BackScale`). When it first comes to the cell, the hand of the book gets
   the pulse `tick`.

**The game's bubble** (`book_bubble.cpp`). A bubble on the land has two
parts [V]: a glass ball (the mesh `O_Bibble_up.l3d` with a texture of 4 by
4 frames) and a `SpellSeedGraphic` in it, with the icon mesh of the miracle
(`GSpellSeedInfo+0x140`) and its particles (`+0x174`). Storm, Fire,
Lightning, Water and Teleport have particles only. The gesture help picture
of the game draws both parts at a free place, at once, with no object in
the world (`fn_0068B7E0`). The mod does the same:

- One glass ball (`LH3DObject::Create`, `GJUtils::GetSharedMesh`), and one
  graphic for each seed type (`SpellSeedGraphic::Create` `0x00726F60` with
  auto update off). They are made at the first pull and kept, as the
  gesture help keeps its own. The graphic is not on the map and no save
  writes it. Its particles use the local random of the game.
- In each eye pass, after the overlay's meshes at `0x0082F4C9`:
  `DrawUpdateAtPos` and `DrawSpellGraphic` (draw now) for the miracle, then
  the virtual function `+0x104` of the ball (draw now). The camera of the
  eye is put in place for the draw and put back after it. The depth test
  is on, so land in front hides the bubble.
- The icon turns about the up axis at 2 radians a second, and the texture of
  the ball steps at 18 frames a second, as on the land.
- The place is the point of the room in the world, with the map of the aim
  of this frame (`ControllersLocalToWorld`).

The book is a layer over the picture of the game, so it would hide the
bubble. When a part of the ball is in front of the page, the book has a
round hole where the head sees the ball (`BubbleHole`). The hole is larger
by 3.2 mm for each cm that the ball is off the page, for the two eyes. Its
edge is soft over 3 pixels. A ball behind the page makes no hole, so the
book hides it.

With `spell_book_bubble=0`, or when a check of the game's code fails, the
bubble is a flat picture on a quad of its own (`kCardBubble`): the gold halo,
the fill and the two gestures, facing the head.

**The orbs in the cells** (`spell_book_orbs=1`). Each item of the open book
has the orb of the game in its bubble, made and drawn as the bubble of the
pull (`BookBubbleShowOrb`). The orb has the radius of the bubble of the cell
(2.1 cm in a cell of 6.5 cm, 1.3 cm in a cell of 4 cm), and its middle is
the middle of the bubble on the plane of the book (`OrbCenter`). The mesh of
the ball has its dome along its y, so in each eye pass the ball turns to the
eye of that pass, as `OneOffSpellSeed::FaceCamera` turns it on the land
[V]: y to the eye, z to the up of the world across the line of view. So the
ball is round from each eye, and the middle of its mesh is at the middle of
the bubble, with the icon of the miracle. The icon turns and the particles
play as on the land: that is the game's own animation of the orb. The orb moves with the book, so with the hand, one to
one. The page has a round hole for each orb, in the base picture of the
book, with a soft edge over 1 pixel. The orb is on the plane of the page, so
the hole is the same for both eyes. The ring round the bubble and the gold
halo of the item under the hand stay on the page. An item that the game
refuses now keeps 45 percent of the page over its orb, so it is dim. The
item whose bubble the hand pulls out has its empty socket and no orb. Each
orb is one draw of the graphic and one of the ball in each eye pass: 6 orbs
are 12 draws of the game in a pass. The land in front of the book hides an
orb, as it hides the bubble of the pull. With `spell_book_orbs=0` the
bubbles are the flat pictures of the mod.

The two hands both make the pull: a move of the hand of the book away from
the pointing hand also pulls. The card has "Pull farther to take it" and
"[Grip] Open to put it back" while the bubble is out. A close of the book
stops the pull, and the miracle stays in the book. With
`spell_book_pull_cm=0` the grip takes the miracle at once.

**The take.** The book holds the press of the grip on an item: the game and
the grab of the world do not get it, until the grip opens (`GripStep`). So
the closed grip after a snap does not cast the miracle. The take is the
snap of the pull, or the grip itself with `spell_book_pull_cm=0`. Before the
next interface step of the game, the mod
makes the tests of the end of a chain of gestures again: the Hand is free
(`IsHandReadyForObject`), and `fn_0064BEC0` says yes. Then it sends the same
request: packet `0x25` with the seed type (`fn_00550C20` on the `GGame`).
The handler of the game (`fn_005DABA0`) makes its test again and lets the
best icon charge (`fn_0064BDB0`). So the game takes the prayer power and puts
the Miracle in the Hand, as for the gestures. The take gives the pulse
`activate` and the cue "ring choose". The miracle "again" goes with packet
`0x25` too, with its seed type, and not with packet `0x26` of the gesture
"again".

**A refusal.** A grip on a dim item takes nothing, and no bubble comes out.
When the game refuses a miracle at the snap, the bubble springs back. The
book stays open, the
pointing hand gets the pulse `refused`, and the card and the title of the
book give the reason for 4 s. The game itself shows nothing for a request
that it refuses.

**The card of the other hand.** On the land with an empty Hand the card of the
hand that does not point has "[X] Spells" (or "[A] Spells" when the left
hand points), while a press can open the book. While the book is open, that
card does not show.

### The book on the palm (`spell_book_palm=1`)

The book also opens when the palm of the hand that does not point turns up
and stays up for 0.4 s. It is the second way beside the button. The normal
of the palm is the axis of `BookCenter` (+x of the grip of the left hand,
-x of the right hand). The code is in `spell_book.cpp`, and the math
(`PalmBookStep`) in `gesture_panel_math.h`.

- The palm is up when its normal is 40 degrees or less from the up of the
  room. It stays up until the normal is more than 60 degrees from up.
- The palm opens the book only when a press of the button could open it, and
  only while the hand is free: no grip on the land or on the world
  (`GrabHeld`), the grip and the trigger of that hand are open, the ring
  button is up, and the ring does not show. A palm that is not free starts
  the 0.4 s again. So the release of a grip on the land with the palm up
  opens the book only 0.4 s later, while the palm stays up.
- The palm opens the book only one time. The palm must turn away from up
  (past 60 degrees) before it can open the book again. This is also true
  after the button or a take closed the book.
- A book that the palm opened closes when the palm is more than 70 degrees
  from up. A pull of a bubble out of the book holds it open until the pull
  ends. The button also closes it. The palm does not close a book that the
  button opened.
- The palm is also the place of the row of the gestures (see "The gesture
  row over the palm"). While the draw button is down, or while the game
  shows a gesture that goes on from a gesture that the player drew, the
  palm gives the row and does not open the book.

With `spell_book_palm=0` only the button opens the book.

### The debug switch "Unlock all" (`debug_all_miracles=1`)

A switch for tests, default 0. It is the first row of the group Miracles of
the debug page and of the debug window: the item "debug" of the ring
(`debug_menu=1`), then the tab Miracles, then "Unlock all miracles". The
trigger of the pointing hand changes it, so it needs no keyboard. With 1,
once a second:

- Each town of the player knows the magic types of the 30 miracles of the
  game, 24 magic types in all (`Town::AddMagicTypesHeld` `0x0073D380`, the
  function of the setup of a land and of a reward). This turns the magic type
  on for the player and gives the Village Centre the icon, if it has a free
  slot.
- The Worship Site of each town gets the icons of the Village Centre
  (`WorshipSite::AddTownSpells` `0x0077C910`, which adds only an icon that it
  does not have).
- Each Worship Site of the player has no limit of prayer power: the flag at
  `+0x108`, which `WorshipSite::GetTotalChantsAvailable` reads as 1,000,000
  chants.

**The limit of 6 icons.** The Village Centre has 6 slots for miracle icons
(`TownCentre::AddSpell` `0x00744050` at `+0xD0`, which gives a seventh miracle
no icon). The Worship Site copies the icons of the Village Centre. So the
switch gives each town all the magic types, but the book shows only the
miracles that got a slot first. In the test save `temple`, with the switch
on from the first frame, a new Worship Site gets Storm, Forest, Fireball,
Food, Spiritual Shield and Physical Shield. Water and Heal do not show,
although the town knows them. Every miracle in the book needs icons that do
not come from the Village Centre (an open item of P20).

A take costs no prayer power: with 0 chants in the pool, the game puts the
Miracle in the Hand. The take does not take chants from the pool, but a
cancel gives the chants of the miracle back to it (13000 chants for Forest).
So the pool gets larger with each cancel while the switch is on. The switch
needs a Worship Site of the player. With no site the log says so, and the
book stays empty. The switch at 0 puts the flags of the sites back. The
miracles that the towns know stay. A save of the game made while the switch
is on keeps the miracles of the towns, the icons and the flags of the sites.
The switch itself writes nothing to a saved game.

### The trail at the fingertip (`gesture_trail=1`)

While the draw button is held and the hand may draw, a ribbon follows the
point of the draw, so the player sees the gesture that he draws. The game
shows no trail of its own in the headset. The code is `overlay.cpp`
(`BuildGestureTrail`) with the pure math in `gesture_trail_math.h` and
`glow_math.h`. The ribbon is a glow ribbon (`hand-model.md`, "The glow
ribbons"): gold in the middle and amber at the rim, 1.8 cm across at the
newest end, with a light that adds to the picture.

- The trail draws while `GestureInput.held` is true, the same input that
  feeds the ring of the game. So it also draws for the circle of a shield,
  while the grip is closed with a miracle that needs a circle.
- The newest end is the point of the draw of this frame, so the ribbon
  always reaches the finger. A point goes in each 5 mm of the room. The
  trail keeps 96 points at the most.
- A point stays 1.5 s. Its light and its width fall to nothing over that
  time (width to 35 percent), so the old end of the trail goes out. A hand
  that holds still loses the trail in 1.5 s.
- When the draw ends, all the light falls to nothing over 0.3 s, and the
  trail is empty. A new draw starts a new trail.
- The points are in the room (game units), not in the world. In each frame
  they go into the world with the map of that frame, so a move of the game
  camera does not bend the trail. The hand and the trail move together.
- It draws with `overlay=0` too, and with the hand model, the low poly
  hand, and `hand_exact`.

### The gesture pictures of the game [V]

In each run of `ProcessPowerUpSystem` the game gives its "looking for" list
to `fn_0068ABA0`. When the list changes, that function makes one
`DisplayGesture` for each gesture type that the player can draw now. The
list of the pictures is at `0x00D4EAC8`, with the count at `0x00D4EAD0`.
`PSysGlobal::DrawLoop` (`fn_0068AA60`) puts each picture on the sorted list
of the frame with the draw `fn_0068B480`. A picture has three parts:

- The bubble (`fn_0068B7E0`): an empty glass ball for a chain start, the
  mesh of a leash, or the `SpellSeedGraphic` of the miracle. The bubble has
  a camera of its own (`fn_0068B740`, put back by `fn_0068B7D0`) and a move
  of the middle of the projection to the place of the picture.
- The card: a sprite with the picture of the gesture
  (`data\textures\S_Gesture*.raw`, `fn_0057D5D0`).
- A dot that runs along the path of the gesture.

The card and the dot get their place from `fn_0068B420`: a pixel of the
square of the picture, 10 units in front of the camera
(`LH3DTech::Get3DPointFromScreen`). The squares are at the lower right of
the game screen. Each one is 3.5 percent of the width of the screen, 0.5
percent apart, in a row from the right (`fn_0068AED0`). The card and the dot
draw with no depth test. The pictures fade out while the help system shows
a text (`HelpSystem+0x14`, `+0xB8`).

The list of an empty Hand at rest has the chain starts and "again"
(`GSpellSystemInfo+0x10` to `+0x18`): the game shows these whenever a
Worship Site of the player has a miracle. After a chain start the list has
the next gesture of each miracle and the scribble. With a Miracle in the
Hand it has the scribble and the gestures of the power ups.

### Where the gesture pictures of the game go

With `gesture_panel=0` the headset does not show the pictures, for three
reasons. A desktop run with dxwrapper shows each one (`village_run.ps1
-Kind palm`, the render of 3600 by 2880, the God view at 44.4 units for each
meter):

- The game draws the pictures: up to 1300 draws in a report window, with the
  fade at 1. They are at the lower right corner of the render of each eye,
  in the last 3 percent of its width and height (the middle of the first
  card at the pixel 3483 2817). The lens of the headset shows little or
  nothing of the corners of the render.
- The game puts each card 10 units deep on the ray of its pixel: 19.9 units
  from the eye, 45 cm of the room. With one walk the right eye sees that
  point 370 pixels (10 percent of the width) to the left of the place in
  the left eye. So the two eyes do not see one picture at one place. When
  one copy is in the part that the lens shows, it is a ghost in one eye.
- The bubbles have a camera of their own and a move of the middle of the
  projection. In the eye pictures they are cut at the bottom edge of the
  left eye, and a stray icon shows at the right edge of the right eye.

While the help system shows a text, the fade is 0, and the game draws no
picture.

### The gesture row over the palm (`gesture_panel=1`)

The code is in `src/mod/gesture_panel.cpp`, with the pure math in
`gesture_panel_math.h`. The row shows the gesture pictures of the game over
the palm of the hand that does not point, while that palm is up. The
pictures are the game's own, with the game's list, its order, its fade and
its animation of the dot.

**The place.** The middle of the row is where the middle of the spell book
is (`BookCenter`): 12 cm from the grip along the normal of the palm, and 2
cm up. The row faces the head with no roll, as the book does. The mod keeps
the layout of the game: it puts the row of squares of the game screen, with
the bubbles over them, on that plane. Three pictures or more are
`gesture_panel_cm` wide (30 cm). Fewer pictures keep the size of a row of
three, so one picture is about 9 cm.

**The draw.** The draw of the sorted list draws no picture. In each eye
pass, after the game's bubble of the spell book, the mod calls the draw of
the game (`fn_0068B480`) for each picture, with three changes:

- `fn_0068B420` gives a point of the plane over the palm, in place of the
  point 10 units in front of the camera.
- `fn_0068B7E0` draws the bubble at its point of the plane with the glass
  ball and the graphics of `book_bubble.cpp` (`BookBubbleDrawPart`), and not
  with a camera of its own. The bubble is 0.9 of the side of its card
  across. The meshes of a leash do not show.
- The sizes of the card and of the dot (`0x00C02674` and `0x00D4EAFC`) are
  scaled for the plane while the draw runs, and then put back.

The camera of the eye is put in place for the draw and put back after it.

**When.** The row shows while each of these is true:

- The palm is up: its normal is 40 degrees or less from the up of the
  room. It stays up until the normal is more than 60 degrees from up. A
  palm that is not up shows no row, so no picture stays at a place of the
  view.
- The game shows a gesture picture.
- The spell book is closed. One palm cannot hold the book and the row, so
  the row does not show while the book is open.
- The controllers can act, no dialog box is up, no ring shows, and the cut
  scene window is not up.

**The palm and the book.** With `spell_book_palm=1` the palm that turns up
also opens the book (see "The book on the palm"). The rule:

- While the draw button (B) is down, the palm shows the row.
- While the game shows a picture that goes on from a gesture that the
  player drew (a chain after a spiral, the gestures of a power up, the
  scribble with a Miracle in the Hand), the palm shows the row. The draw
  button is up between the pieces of a chain, and the row stays.
- At other times (an empty Hand at rest) the palm opens the book.

With `spell_book_palm=0` the palm always shows the row.

With `gesture_panel=0` the game draws its pictures as before, and the
headset does not show them.

### The camera path of a miracle (`miracle_camera_still=1`)

**What the game does [V].** A particle effect of the game can have a camera
path (`Particle3DAnimWithCamera`). The Forest miracle has one. At a cast of
the player, `Particle3DAnimWithCamera::GameUpdate` `0x006C86F0` pushes a
`CameraModePath` (`0x00460E50`). The camera then turns around the new forest
while the trees grow, and `StopCamera` `0x006C86C0` takes the path off at the
end of the effect. The update starts a path only for the interface that casts
(`PSysManager::NetUnsafeIsMyInterfaceCasting`), and only while the int at
`0x00C029E0` is not 0. The game has 1 there in its data and never writes it.
In the God view the path is a camera of a story (`script_camera.cpp`): the
God view stands aside, and the view turns with no move of the player. A
desktop run with no change has the line "the camera belongs to
CameraModePath" at the frame of the cast.

**What the mod does.** With `miracle_camera_still=1` and a play mode in
which the head carries the camera (2, 3 and 4), `script_camera.cpp` writes 0
at `0x00C029E0` one time, before the first frame of play. The effect of the
miracle plays as before, and the camera of the player stays. The God view
does not stand aside. The write is one time only: a change while a path
plays clears the mark of the path (`+0x34` of the effect), and then the game
does not take the path off. With `miracle_camera_still=0` the path plays as
the game makes it. Play mode 1 (the window) keeps the path.

### The influence at the hand

**What the plain game does at the edge [V].** The Hand of the mouse can go
past the edge of the player's influence. There it acts on nothing. The hand
mode of an object is `0x12` (a touch) and not a pick up
(`Object::InterfaceMustBeInInfluenceForInteraction`), and each game turn of a
take ends the take (`GInterfaceStatus::Process`, `0x005DC540`). The look of
the Hand does not change. `GVirtualInfluence` (`GInterfaceStatus + 0x130`)
keeps the last land point of the Hand inside the influence (`+0x08`). Its
update (`0x0076CD40`) gives a strength (`+0x30`) of 1 there. Outside, the
strength goes down with the distance from that point and with the time, to 0
under 0.1. While the strength is above 0, `GVirtualInfluence::Draw`
`0x0076D000` finds the edge between that point and the Hand (`+0x14`). Then
it calls `PSysGlobal::AddManaPathSprite` `0x0068FB20` at `0x0076D1C2`: a
stream of light from the edge to the land point under the Hand, and a sound.
A script can turn the stream off (`SET_VIRTUAL_INFLUENCE`,
`GPlayer::SetVirtualInfluence` `0x0064C3B0`).

**What the mod does** (`influence_edge.cpp`). The real hand is in the air
over the land, so three things change, each with a key:

- **The fade** (`influence_fade=1`). Outside the influence each hand fades to
  45 percent in 0.25 s, and inside it comes back in the same time
  (`HandModelSetOpacity`). The test is the game's own: the call of
  `Influence::CalculatePlayerInfluence` that `fn_005D1120` makes, at the land
  point of the grip of that hand. A hand under 90 percent writes no depth.
  The low poly hand has no alpha, so it does not fade.
- **The stream at the hand** (`influence_stream_hand`, 0 by default). A hook
  of `AddManaPathSprite` gives the land point of the Hand the height of the
  Hand's point over the land. That is the third field of the `MapCoords`,
  which `MapCoords::GetLHPoint` adds to the height of the land. So the
  stream of the game ends at the Hand's point in the air. Only the stream of
  the Hand of the controller changes. No desktop run has had a stream of the
  game yet, so the key is 0 until a run shows it. `CHand::Draw` calls
  `GVirtualInfluence::Draw` (`0x0046D2A7`) also while the mod hides the mesh
  of the game's Hand. But the update keeps the land point of the interface
  (`GInterfaceStatus + 0x14`), and `Draw` needs a last point inside (`+0x08`
  not 0). In the God view that land point can be away from the land under
  the Hand. The report of the log has the state of the stream.
- **The glow at the edge** (`influence_wall=1`). For each hand, a soft round
  patch of light lies on the edge at the point of the edge nearest to the
  Hand's point of that hand, at the height of that point
  (`influence_edge_math.h`). 24 rays go out from the land point of the hand,
  and each finds the first point where the influence changes. The patch is
  in the vertical plane through that point, facing the hand. It is 12 cm of
  the room from its middle to its rim, about the size of a hand. Its light
  is full in the middle, 0.55 of it at half the radius and 0 at the rim.
  - **The color** is the red of the game's own influence ring: the color of
    the ring of the player in the table at `0x00EA9EFC`. `fn_00826510` fills
    it, and `InfluenceCircle` (`fn_008265F0`) gives the vertices of the ring
    the color of the player index of `GetRemapedPlayer` `0x0064D790` [V]. The
    mod takes the same entry for the player of the interface. Entry 0 is 255
    70 70.
  - **The strength.** Inside the influence the glow starts 20 cm of the room
    from the edge, about one hand length, so it shows only when the hand is
    about to touch the edge and not while the hand works near it. It rises
    as `t * t * (3 - 2 t)` with `t = 1 - distance / 20 cm`, so the start is
    faint (0.16 at 15 cm) and the touch is full. Past the edge it falls to
    35 percent in 3 cm and to 0 at 30 cm: a faint patch where the hand went
    through, which shows the way back. The most light is 0.6 of full light.
  - The overlay draws the patches with the glow, as added light, with the
    depth test, so the land in front hides them. Most frames have no edge
    near a hand, and then the search costs 48 tests of the influence for
    each hand.

The fade and the glow act only while the controller places the Hand, in the
God view and on the table.

## What a cast makes

`miracle_watch.cpp` reads the game after a cast and writes lines with the tag
`miracle watch`. It changes nothing.

- **A shield or a storm.** Each `SpellShield` (the two shields) is in a list
  of the game (the first one at `0x00DA07F0`, the next at `+0x100`), and each
  `SpellStormAndTornado` in another one (`0x00DA07F8`, `+0xF4`). For each new
  spell a line says where it is, its distance from the land point of the
  last cast at a circle, its radius (`Spell+0xBC`) in units and in cm of the
  room, its life (`+0x38`), its strength (`+0x94`), its age and the limit of
  its age (`+0xB8`, `+0x50`), and its caster (`+0xA0`). A shield line also
  has the count of its domes (`MapShield`, `+0xF0`). A storm line says if
  its particle system (`+0xEC`) is there: `SpellStormAndTornado::Process`
  deletes it when the spell has no caster. A line comes every 2 s for 12 s,
  then every 10 s to 30 lines, and one line when the spell is gone, with its
  time.
- **A Creature after a Creature miracle.** For 25 s after a cast on a
  Creature, a line comes at each change and every 3 s. It gives the size of
  the Creature for the game (`fn_004F5E60`), the base size
  (`[Creature+0x160]+0x6C`), the size of the drawn body (the `LH3DCreature`
  at `[Creature+0x160]+0x58`, `+0x90`), the height of the base of the body
  over the land, the state of the Hand, the flag of invisibility
  (`Creature+0x106C`), the fizz (`+0x12B0`, its target at `+0x12B4`), the
  fizz that the draw of the body gets (`[LH3DCreature+0x482C]+0xA0` and the
  material at `+0x9C`), the switch `0x00D00DE4`, and each active entry of
  the table of the spells on the Creature (`CreatureReceiveSpell`,
  `Creature+0x370`, 16 entries of `0x18` bytes from `+4`, active while `+4`
  is not 0). A Creature miracle puts its entry there
  (`SpellCreature::InitWithObject` `0x007222D0`).
- **The drops of Water.** A hook on `Spell::ApplyDefaultSpellEffect`
  (`0x00720C30`) reads each drop of a Water spell (see "Water on a fire"
  below). For the first 12 drops of a spell and each 10th drop after them, a
  line gives the drop, its distance from the place of the spell and from the
  Hand's point, the burning object nearest to it with the terms of the test
  of the game (the distance from the middle of its fire, its fire radius, the
  height of the land under the drop over the base of the object, and the
  height of the object), its temperature before and after the drop, the
  temperature at which it burns, and its heat capacity. A line at the end of
  a pour gives the count of drops and of the drops that cooled a fire. Then
  for 30 s a line each second gives each burning object that was near a
  drop: its place, its temperature, its fire fraction (`GetFireFraction`
  `0x007303E0`) and whether it has a graphic of flames (`FireEffect+0x2C`,
  which `FireEffect::Draw` draws), or says that its fire is out.
- **A Physical Shield in the physics.** A line gives the body of each new
  shield: its middle, the radius of its ball, its points (their height and
  their farthest distance from the middle), its faces, and its rest byte,
  with the scale of the object and the radius of its spell. While a body
  that moves is within 3 times that radius plus 20 units of the middle, a
  line each 6 frames gives its distance from the middle, the sum of the two
  radii (the game tests the pair only when the distance is less), the count
  of its points in the ball of the shield, its speed, and the tests of
  `PhysOb::CheckVFaces` (`0x007FDA60`, hooked) between it and the shield,
  with their contacts. The line "first contact" of a throw names the class
  of the object that the thrown object touched (`throw.md`).
- **A teleport.** A line gives the teleport sites of the player when their
  list changes (`GPlayer+0xA58`, the next one at `+0x88`), each with its place,
  its spell and the seed of the spell. A hook on `MagicTeleport::DoTeleport`
  (`0x005FC790`) gives the Living that goes in, the site, its place before and
  after the call and the site nearest to it after the call. Lines 0.5 s, 2 s
  and 5 s later give its place and its distance from each site.

What the game does to the Creature [V]:

- Shrink (entry 1) and Enlarge (entry 2) move the base size of the Creature
  (`[Creature+0x160]+0x6C`) to the size of the spell and back
  (`0x004F5520` to `0x004F5672`). On each game turn `Creature::ProcessState`
  calls `fn_004EFCE0`, which gives a size to the body
  (`LH3DCreature::SetSize`, vtable 8, which writes `+0x90`). Away from the
  Temple that size is the base size. Near the pen of the Temple it is
  smaller: at 16 m of the game or less from the point of the Creature at
  the pen (`Creature+0x1200`), in a cone in front of the pen (the Citadel
  `GPlayer+0xA48`, its pen at `+0x30`), the size goes from the base size at
  16 m to 0.22 at 14 m and nearer. So at the door of the Temple the body
  keeps the size 0.22 with each spell, also in the plain game. A desktop
  run in the test save `temple` shows the base size at 0.200 with Shrink
  and 2.400 with Enlarge, and the drawn body at 0.220. Away from the pen
  (61 m of the game from its point, `miracle_run.ps1 -Kind away`) the drawn
  body goes to 2.000 with Enlarge and to 0.200 with Shrink. The watch line gives
  the metres from the point at the pen, and the size that the body gets in
  front of the pen. The walk also reads the base size
  (`LH3DCreature::GetNavRadius` `0x00480A60`).
- Invisibility (entry 7, `0x004F58B0` and `0x004F5960`) sets the flag at
  `Creature+0x106C` and starts the fizz (`Creature::SetFizz` `0x0047AB90`)
  to 0.75. `Creature::ProcessState` (`0x004737F9`) gives the fizz and the
  fizz material (`GlobalTextures::GetFizzMaterial`) to the draw of the body
  while the switch `0x00D00DE4` is set. A desktop run shows the fizz at
  0.75 in the draw, and only sparkles and a shadow in the eye pictures.

**Water on a fire** [V]. `SpellWater::Process` (`0x00724ED0`) makes one
drop each game turn: a point 0.3 to 0.3 + 0.7 R units from the place of the
spell (`Spell+0xCC`), in a random direction, with R from the info of Water
(`fn_005FACE0`). The place of the spell is the land under the gesture place
that the game gives the spell (see "The bolt of Lightning starts at the
Hand's point"), so in the mod it is the land under the Hand's point.
`Spell::ApplyDefaultSpellEffect` puts the place of the effect at the drop
with a height of 0, and `EffectValues::ApplyEffectToMapPos` (`0x00525100`)
gives the effect to each object when two tests pass: the drop is less than
the fire radius of the object (`GetDefaultFireRadius`, the 2D radius) plus
the radius of the effect from the middle of its fire (the place of the
object), and the land under the drop is less than the height of the object
plus that radius from its base. `FireEffect::ApplyEffectToFireEffectIfNecessary`
(`0x00730670`) then moves the temperature of the fire toward the temperature
of the land plus the burn of the effect, by 10 divided by the heat capacity
of the object (`GetHeatCapacity`, `[Object+0x28]+0xB0`) of the gap, at most
the full gap. The object burns while its temperature is at least its
combustion temperature (`[Object+0x28]+0xB4`, at least 40). So the number
of drops that a fire needs goes up with the heat capacity of the object.
The fire also heats itself in each game turn (`FireEffect::ProcessList`
`0x00730760`, `fn_0072EFB0`). A pour on a burning house in a desktop run
had its Hand's point 4 units from the middle of the house (inside its
radius), so the drops fall on the house. A house has a heat capacity of
2000 and burns at 150 or more. In a desktop run the first drop was 2.72
units from the middle of the house (its fire radius is 4.30), and the drops
took its temperature from 213.3 to 37.5 in 6 drops. A second run took it
from 196.7 to 19.1, under 150 from the fourth drop. So the house goes out
in the game's own way. The lines each second after the pour follow each
burning object that was near a drop, with its fire fraction and whether it
has a graphic of flames. In two runs the house was out after the pour,
with no flames on it. The flames that stay are on a pile of wood beside the
store (it burns at 90, and it stays at 190 to 247) and on trees near it
(they burn at 110, and they stay at 220). The pour did not fall on them, so
they burn on, as in the plain game.

**The cloud of a Storm.** The particle system of the cast
(`SF_StormCast`) ends at 6 s: its pieces die at the age of 6 s
(`RemoveRuleOldAgeOnly`, `DieAge`). The storm spell stands on, and it
drifts. Its cloud is an `LH3DMist`, as the clouds of the sky. The weather
of the game also makes mists (`GWeather::DrawClouds` `0x0083FC90`), and a
particle can make one (`ParticleMistCreator::CreateLH3DMist`
`0x006AA5A0`). The mod turns each mist to the point that it faces and limits
its pole to `cloud_pole_pct` percent of its distance (`backend.md`,
"Clouds"), so a storm cloud near the head is flat. The test keys
`sky_test_mist_log` and `sky_test_mist_hide` give a line for each mist that
is not a sky cloud and take those mists out of the picture from a frame
(`backend.md`, "Clouds"). `miracle_run.ps1 -Kind storm` uses them. In a
desktop run the storm had 17 such mists, for example a dome of 47 units 534
units away, and none of them was drawn after the frame of the hide, but the
band stayed. With `sky_test_mist_hide_all` the band goes and the red sky of
the storm shows. So the band is made of sky clouds of the storm. The engine
flattens a sky cloud near the horizon of the eye, and in the God view the
eye is low and near the clouds of the storm. `cloud_round_near` keeps a near
sky cloud round. A desktop run showed the band gone, but the clouds of the
storm hang 6 to 20 units from the eye, and a round one is a dark mass at the
face. So a sky cloud fades by its gap from the eye in the room
(`cloud_fade_near_cm`, `cloud_fade_far_cm`, `backend.md`, "Clouds").

Open (B43 in the roadmap): in the full view the cloud of a Storm is a flat
grey band with straight edges, also with `cloud_pole_pct=0`. The cast of
the game also starts many particles (type 106, `PARTICLE_TYPE_STORM_CAST`),
so the plain game has no flat band.

## Keys

| Key | Default | What it does |
| --- | --- | --- |
| `miracle_camera_still` | 1 | 1: a miracle with a camera path of the game (the Forest) starts no path while the head carries the camera, and the camera of the player stays. 0: the path turns the camera as the game makes it |
| `influence_fade` | 1 | 1: outside the influence of the player each hand fades to 45 percent. 0: the hands do not fade |
| `influence_stream_hand` | 0 | 1: the stream of the influence of the game ends at the Hand's point in the air. 0: it ends on the land under the Hand, as the game draws it. 0 until a desktop run shows the stream |
| `influence_wall` | 1 | 1: a soft red glow shows the edge of the influence at each hand, from 20 cm of the room before the edge (see "The glow at the edge"). 0: no glow |
| `miracle_hand` | 1 | 0: the game places the Hand with a miracle, and the mod gives no target, no cancel, no use of a seed and no lines |
| `miracle_shake_mps` | 0.6 | The mean speed of each stroke of a shake, in m/s. 0 turns the shake off. 0.2 to 5 |
| `miracle_cancel_button` | 1 | 0: the second face button does not cancel |
| `miracle_cancel_ms` | 400 | The longest press of that button that is a cancel. 50 to 2000 |
| `miracle_seed_use` | 1 | 0: the trigger does not make a carried seed active. Then only a short press of the grip does |
| `miracle_circle_hand` | 1 | 1: the circle of a Storm or a shield is on the land under the middle of the path of the real hand, with 1.05 times the widest span of that path as its size. 0: the circle that the game makes from the pixels of the path |
| `miracle_site_pick` | 1 | 1: a teleport site that the game offers to its pick is a candidate of the real hand, so the Hand can take a site and put a villager on it. 0: the real hand has no site as a candidate |
| `miracle_circle_cast` | 1 | 1: Storm and the two shields cast when the game matches the circle, with the grip still closed. 0: they cast when the grip opens |
| `miracle_touch_cm` | 3 | A body this near to the Hand's point is the target at the Hand, in cm of the room |
| `miracle_cone_deg` | 6 | The cone of the aim for a new target at a distance |
| `miracle_keep_deg` | the cone and 4 | The target of the last frame stays to this angle |
| `miracle_max_m` | 4 | The most distance of a target in the aim, in m of the room. 0 is no limit |
| `spell_book` | 1 | 0: the first face button of the hand that does not point has no job, and no book opens |
| `spell_book_pull_cm` | 10 | The pull that takes a miracle out of the book, in cm of hand motion, 3 to 30. 0: the grip takes the miracle at once |
| `spell_book_palm` | 1 | 0: the palm that turns up does not open the book. Only the button opens it |
| `gesture_panel` | 1 | 0: no row over the palm. The game draws its gesture pictures at the corner of its screen, and the headset does not show them |
| `gesture_panel_cm` | 30 | The width of the row of the gesture pictures over the palm, in cm, for 3 pictures or more, 10 to 80 |
| `spell_book_bubble` | 1 | 1: the bubble that the hand pulls out is the game's own bubble. 0: it is a flat picture with the two gestures |
| `spell_book_orbs` | 1 | 1: each cell of the book shows the orb of the game. 0: a flat picture with the two gestures of the miracle |
| `spell_book_point` | 1 | 1: the page shows the point of the pick, and the glow at the Hand's point does not show while the book is open. 0: no mark, and the glow as always |
| `card_hand_front` | 1 | 0: the page of the book is over the hands, also where a hand is in front of it (`ui-layer.md`, "A hand in front of a card") |
| `hint_card_style` | 1 | 1: the book is a page of parchment with dark ink, as the hint cards. 0: the dark panel with white letters. The key of the hint cards (`controller-hints.md`, 9.5) |
| `debug_all_miracles` | 0 | 1: each town of the player knows every miracle of the game, its Worship Site shows up to 6 of them, and each site has no limit of prayer power. For tests |
| `gesture_point` | 1 | 1: the tip of the index finger draws a gesture. 0: the grip point draws it. Not in the launcher |
| `gesture_trail` | 1 | 1: a ribbon follows the point of the draw (see "The trail at the fingertip"). 0: no trail. Not in the launcher: each player has the trail, and a save of the launcher removes the key (`launcher.md`, "Retired keys") |

The keys are read from `bwvr.ini`. None of them is in the launcher.

**The A/B key**, with `test_keys=1`: Ctrl with the 8 key of the number pad
changes `miracle_hand`. The pointing controller gives a long pulse for 1 and
a short pulse for 0.

**The debug page** (`debug_menu=1`, the group Miracles): "Unlock all"
changes `debug_all_miracles`, "Spell book" changes `spell_book`, "Book orbs"
changes `spell_book_orbs`, "Book point" changes `spell_book_point`, "Book
pull" changes `spell_book_pull_cm` between
0 and its value in `bwvr.ini` (10 when that is 0), "Book bubble" changes
`spell_book_bubble`, "Miracle hand" changes `miracle_hand`, "Gesture point"
changes `gesture_point` (grip or fingertip), and "Gesture trail" changes
`gesture_trail`. The hand that does not point gives a long pulse for
1 and a short pulse for 0. These switches have no key of the keyboard. Choose
"Gesture point" between two draws.

Test keys for the desktop runs only (the launcher removes them):

- `miracle_test_seeds`: a list `type@frame` with spaces between. At that
  frame the game makes a one-off seed of that type on the land under the
  Hand (`OneOffSpellSeed::Create`).
- `miracle_test_cancel`: a list of frames. At each one the mod asks for a
  cancel.
- `influence_stream_test_log=1`: a log line for each stream of light of the
  game (see "The bolt of Lightning").
- `spell_book_test_save`: the mod saves the first N pictures of the book as
  `bwvr-book-NNNNN.bmp`.
- The word `book` of the scripted controller steps the pointing hand to the
  middle of the next item of the open book.
- `fake_controller_left_rots` and the word `lrot` turn the left hand, so a
  run can turn its palm up and down (`controllers.md`, 4.2).
- `gesture_panel_test_frame`: at this frame of the scripted controller
  `gesture_panel` changes, so one run shows the corner of the game and the
  row over the palm.
- `influence_test_circle_cm` and `influence_test_circle_frame`: from the
  first frame of the controllers at that frame or later with the Hand of the
  controller, the influence of the player is a circle of this many cm of the
  room around the land point of the Hand's point of that frame. The fade, the
  glow and the stream see it (a test hook of
  `Influence::IsInPlayerRawInfluence` `0x005CD460`). Before that frame the
  fade and the glow count each point as inside. The pick of the game keeps
  the real influence.
- `influence_wall_test_light_pct`: the light of the glow at the edge times
  this percent, 100 to 300, for the eye pictures of a desktop run.

## The log

The tag `script camera` has one line at the start with
`miracle_camera_still`, the address of the switch and its old value. A
camera path that plays has the line "the camera belongs to CameraModePath".

The tag `influence` has the three keys at the start, and these lines: a hand
that goes out of the influence or comes back into it, with its land point.
The glow at the edge that shows or goes for a hand, with the distance of
the hand from the edge, inside or past it, the strength and the rays that
found the edge, and a line each 60 frames while it shows. The color of the
glow, with the player and the entry of the table of ring colors. The first stream that the game draws, with its two ends and the
Hand's point. The first stream that ends at the Hand's point, with its
height over the land. With the test circle or `influence_stream_hand=1` the
report has the state of the stream of the game: the last point inside, the
edge, the strength, the switch of a script, the land point of the interface
and the Hand's point. The test circle, with its middle and its radius. Each report
window has the frames with a fade and with the glow, and the calls of the
stream.

The tag `miracle` has one line for each change:

- The keys at the start, and each press of the A/B key.
- "the Hand holds the one-off seed ... as an object".
- "... is active in the Hand", with the seed, the power up, where it comes
  from, how it is cast, and the Hand state.
- "the Hand of the game with the miracle is at ..., and the point of the
  real hand is at ...: N cm of the room apart", 4 times after a miracle
  comes into the Hand, also with `miracle_hand=0`.
- "the target of ... is the ...", with the gap and the angle. "has no
  target now", with the cause: the gap and the angle of the old target
  against the keep limits, "the game does not let the miracle act on" it, or
  "the draw did not offer it". "the ... is at the Hand, but the game does
  not let ... act on it", with the reason.
- "the game casts ... on the ..." and "the game casts ... at the land
  point ...", with the speed of the Hand. For a pour: the first cast, and
  "the pour ends: the game sent N casts in M frames".
- "the game refused the cast of ... at ...", with the reason, the object of
  the pick record and the influence. Also with `miracle_hand=0`.
- "a shake of the hand: ...", "... the mod takes ... out of the Hand with
  the game's own drop".
- "the trigger closes with the one-off seed ...", "the Hand is empty, so the
  mod taps the seed ...", "the use of the seed ... ended with no miracle".
- "the spell ... runs", "... has power up N now", and "... left the Hand
  (the cause)", with whether the Hand is free.
- One line each report window: the frames with a miracle and with a seed,
  the casts, the refusals, the cancels, the seeds used, and the writes of
  the pick record.

The tag `spell book` has the book:

- The keys at the start, and each change from the debug page.
- "the book opens over the left hand", with its place, its size and the
  miracles (a dim one has "(dim)"), and "the book closes", with the cause.
- "the book does not open", with the cause.
- "the pointing hand is at Fireball", and "is at no miracle of the book".
- "the pointing hand grips Water and pulls it out of the book", with the gap
  from its cell. "the grip opens at N percent of the pull ..., so Water
  springs back into the book". "Water snaps into the hand", with the gap.
  "the book closes while the hand pulls Water".
- "the pointing hand takes Water from the book", or "... and the game
  refuses it", with the reason.
- "the request of Water goes to the game: packet 0x25 ...", and "the Miracle
  Water is in the Hand, N frames after the request".
- With `debug_all_miracles=1`: what each town now knows, the icons of its
  Worship Site, and each site that has no limit of prayer power now.
- One line each report window: the opens, the closes, the pulls (the snaps
  and the bubbles that went back), the takes, the
  requests, the Miracles in the Hand, the refusals and the draws of the
  picture. One more line has the frames with the game's bubble, its draws,
  and the parts that the game made.

The tag `gesture panel` has the gesture pictures:

- The keys at the start, and the change of the test key
  `gesture_panel_test_frame`.
- Each new list of the game: each picture with its gesture type, display
  order, seed type, leash and square, whether a picture goes on from a drawn
  gesture, and the fade.
- With `gesture_panel=0`, for each new list: the world point of the first
  card, its distance from the camera in units and in cm of the room, and
  its pixel in the camera of now.
- "the row of N gesture pictures shows over the left palm", "the row does
  not show", with the cause, and the first draw of the row with its size.
- One line each report window: the draws of the row, the corner draws that
  the mod skipped, the corner draws of the game, and the fade.

The tag `book bubble` has the game's bubble: the glass ball and each graphic
that the game makes, the first draw with its place and size, and a check of
the game's code that fails.

## Tests

The math test (`scene_checks.h`) covers the influence at the hand: the fade
in 0.25 s and back, a long frame, the edge of a circle from inside and from
outside, no edge for a hand far from it, the nearest edge point, and the
glow: none at the start of the range and farther, faint a quarter into it,
rising to full at the touch, the faint patch past the edge and none far
past it, and a round patch in the plane of the edge that faces the hand,
with its light in the middle and none at its rim.

`scene_run.ps1 -Kind forest` casts Forest from a seed of
`miracle_test_seeds`, and `-Kind edge` steps the hand across the edge of
the test circle with an eye picture at each step (`tools/desk/README.md`).

The math test (`miracle_checks.h`) covers the lines of the card for each kind
of miracle, each reason of a refusal in the order of the game, the target at
the Hand and in the aim (the gap, the angle, the range, no ray, and the
order), the target that stays, the cancel button, the steps of the use of a
seed, the shake of the desktop runs (5 steps of 15 cm), and the circle of a
path of the Hand (a circle on the land, an upright circle, and no path).

A desktop run (`bwvr/tools/desk/`) in the God view with the start "Keep your
old Creature" can test a miracle. `miracle_test_seeds` makes the seeds. A
press of 3 frames on a seed is the tap of the game. Desktop runs have shown
a cast of Strong and of Freeze on a Creature, the cancel by a shake and by
the button, a throw (Fireball and the two flocks), a pour (Water, Food, Wood
and Lightning), a circle (Spiritual Shield and Storm), a place (Heal, Forest
and Explosion), a carried seed with the trigger, and a seed from a Worship
Site (Water and Food) with a power up and the power that goes back at a
cancel. A Physical Shield with no circle gives the refusal "this miracle
needs a circle on the land first".

`miracle_run.ps1 -Kind shield` shows the circle that casts when it closes.
The game matches the circle of Spiritual Shield and of Storm at frames 445
and 1025, and casts each one 2 frames later, while the grip stays closed for
80 more frames. The opening of the grip then gives no second cast. With
`miracle_circle_cast=0` the cast comes at the opening of the grip, 82
frames after the match, at the same land point.

The math test (`spell_book_checks.h`) covers the layout of the book for 0 to
32 items, the place of the book from the grip pose, the plane that faces the
head, the pick with its two limits, the open and the close, the grip that
the book holds, and the pull: the share of the pull, the place of the
bubble, the buzz, the spring back, and the hole of the game's bubble in the
page.

The spell book has a recipe: `village_run.ps1 -Kind book` (the test save
`temple`, a Worship Site with Water, Food and Heal, 20000 chants). It opens
the book and steps the right hand to two items. It pulls the second one 9
cm and opens the grip, then pulls it 15 cm, cancels the miracle, and asks
for a miracle with no chants (a refusal). It saves each picture of the book
and of the bubble. What the recipe shows, with Vulkan and with dxwrapper
(the pull with Vulkan only):

- The card of the left hand has "[X] Spells". X opens the book over the left
  hand, 44 cm from the head: 20 by 10 cm with 3 items, 20 by 16 cm with 6.
  The other hand gets the pulse `tick`.
- Each step of the right hand to a new item gives a gold halo on it, the
  pulse `tick` on the pointing hand, and the card "[Grip] Hold and pull out
  Food".
- The grip on the item gives the bubble picture and the empty socket in the
  book, and the card "Pull farther to take it". The buzz goes from 0.03 to
  0.45 on the pointing hand, and half of that on the other hand.
- The grip that opens at 92 percent of the pull sends no request: the
  bubble goes back, and the book stays open.
- The game makes the glass ball and the graphic of Food at the first pull,
  and draws the bubble in each frame of the pull (one draw for each walk of
  the scene). In the eye pictures with dxwrapper, the glass ball is at the
  fingers of the right hand during the pull and at the snap, and the
  fingers in front of it hide a part of it. When the Miracle is in the
  Hand, the ball is gone and the game's own glow of Food is in the Hand.
  With Vulkan (and `test_frame_rate=60`) the log shows the same draws, and
  the eye pictures show the same bubble in both eyes.
- The pull of 15 cm: the bubble snaps into the hand at 10.1 cm, in the same
  frame as the take. With `spell_book_pull_cm=0` the grip is the take, and
  the card says "[Grip] Take Food".
- The take sends packet `0x25` with the seed type in the same frame. The
  Miracle is in the Hand 4 to 6 frames later, and the pool of the site goes
  from 20000 to 13000 chants.
- With 0 chants each item is dim with "No prayer power. Villagers must
  worship". A grip gives the pulse `refused`, and the card gives the reason.
  The game itself shows nothing.
- The last miracle is the first item when the book opens again.
- One picture of the book takes 3 to 6 ms to draw with
  `hint_card_style=0`. The book draws it only when it changes (2 or 3 times
  for each open). With `hint_card_style=1` the pictures are the page of
  parchment. At 624 by 304 px a picture takes 1.8 to 2.3 ms: the text of
  GDI 0.4 to 0.6 ms, the page 0.55, the cells 0.5 to 0.9 and the ink 0.25.
  The first picture takes 10 to 12 ms and the first note 7.5 ms, because
  GDI makes each letter of Alegreya the first time that it draws it at a
  size. The log has the parts of the first 12 pictures (`spell book : a new
  picture of the book`).
- With `spell_book=0`, X makes the log line of no job, and no book opens.
- With `debug_all_miracles=1` the book has 6 miracles, and a take with 0
  chants puts the Miracle in the Hand (see "The limit of 6 icons").

The math test (`glow_checks.h`) covers the trail and the glow ribbons: the
light by age, the points that go in and out, the keep time, the release, a
new draw, a full buffer, the same count of points at 72, 90 and 120 Hz, the
ribbon of a line and of a curve, and the place of the eye from the camera
matrix.

The trail has a recipe: `village_run.ps1 -Kind miracle -Icon 3` with
`-Set "backend=dxwrapper\ddraw.dll;test_frame_rate=60"` and the steps of the
recipe with `shot@1240+1 shot@1300+1 shot@1360+1` added to the first draw (the
word `trace` and B, 300 frames). What it shows:

- The log has "the trail of a gesture starts" and "ends" for each draw, and
  "the draw starts ... The point of the draw is the tip of the index finger,
  8.8 cm of the room from the grip".
- The eye pictures of both eyes show a gold ribbon that follows the path of
  the draw and reaches the fingertip of the right hand. Its old end is thin
  and faint. In the picture 60 frames later the hand has moved on and only
  the last part of the path shows.
- The game matched the spiral (template 64) 116 frames after the start of
  the first draw and 116 frames after the start of the third one, and the W
  shape 58 frames after the start of the fourth draw. With
  `gesture_point=0` it matched the same gestures at the same frames, with
  the same 44 samples, 5 corners and path of 3692 pixels. The scripted hand
  never turns, so the tip is at a fixed offset from the grip, and the two
  draws feed the game the same path. The run proves the way of the tip
  through the plane and the ring. It does not show what a turn of the wrist
  does to a gesture.
- The ribbons cost 0.02 to 0.03 ms a frame on the CPU for the two eyes, with
  80 to 140 triangles in each eye.

A miracle that pours stays in the Hand after the pour, until its chants are
used or the player takes it out. So a recipe with more than one seed must
tap B after a pour (`miracle_run.ps1 -Seeds` gives the seed types, and
`-Steps` the steps).

The math test (`gesture_panel_checks.h`) covers the palm that turns up for
each hand and the turns of the word `lrot`, the band of 40 and 60 degrees,
the book on the palm (the hold of 0.4 s, the grip on the land, the close by
the palm, the pull that holds the book, and the book of the button), and the
map of the row of the game onto the plane.

The gesture pictures and the palm have a recipe: `village_run.ps1 -Kind
palm` (the test save `temple`, with `backend=dxwrapper\ddraw.dll` for eye
pictures). What it shows:

- With `gesture_panel=0` the eye pictures have the pictures of the game at
  the lower right corner of each eye, 370 pixels apart (see "Where the
  gesture pictures of the game go").
- The list of an empty Hand at rest has the spiral, "again" and a leash
  gesture (the leash of the Creature is on in the save). After the spiral
  it has the next gestures of Water, Food and Heal. With Water in the Hand
  it has the power up and the scribble.
- With `gesture_panel=1` and the palm up, the row is over the left hand in
  both eyes: three cards 8.7 cm apart with the bubbles of the miracles over
  them, the empty glass ball over the spiral. The palm to the side hides the
  row. The corner draws stop.
- The palm up at rest opens the book 0.4 s later ("because its palm turned
  up"), and the palm to the side closes it. With the grip of the left hand
  on the land, the book opens only 0.4 s after the grip ends. With B held,
  the palm up shows the row and opens no book. When B ends, the palm opens
  the book.

**Not run on the desktop:**

- The spell book: the close when the ring or the game menu opens, an empty
  book, a Creature miracle in the book, and the switches of the debug page.
- A draw with a hand that turns: the scripted hand keeps its turn, so no run
  shows whether the tip or the grip gives a gesture that the game matches
  more often. The trail with the low poly hand, with `overlay=0` and on the
  table.

- Two casts of Teleport. Each cast makes one site, and a villager or the
  Creature that goes into a site comes out at the nearest other site of the
  player. `miracle_run.ps1 -Kind teleport` makes one cast. The Creature miracles
  other than Strong, Freeze, Shrink, Enlarge and Invisibility.
- A Creature outside the influence (the red halo and its line), each reason
  of a refusal other than "needs a Creature" and "needs a circle on the
  land", a Worship Site or a dispenser as the target, and a magic fireball.
- The line of the card with `miracle_seed_use=0`, and each key at a value
  that is not its default, other than `miracle_hand`.
- A left handed player, `controller_swap_buttons=1`, and a multiplayer game.
- Which other particle effects of the game have a camera path. Each one that
  has one plays no path with `miracle_camera_still=1`.
- The stream of the influence while a script has the virtual influence off.
  Then the game draws no stream, with each value of the key.

The checks in a headset are in `headset-checks.md`, and the checks that a
desktop run can prove are in `roadmap.md`, R12.

## Limits

- A cancel takes a power up off together with the miracle: the game takes
  the power up off first.
- The trigger does not make a carried seed active outside the influence.
- The aim finds a target only in the God view and on the table, where the
  cone of the far grab is installed.
