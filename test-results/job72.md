# Job 72: the bookmark on an object (agent/bookmark fe14af3)

`.\dev.ps1 build -Clean`: 6306 checks, 0 failed. Each log has "build fe14af3" and "vr           : game renders at 2760x2208". `ring_run.ps1 -Kind places`, test save temple. bk1, bk2 and bk3 are RUN DONE. bk2 took 663 s because the test-now flag was out of date, so desk_run waited 10 minutes before the game started. The game part of the run was normal.

## bk1 (far_grab=0): PASS

- "far grab    : far_grab=0, the Hand takes only what it touches".
- "bookmark aim : bookmark_aim_hold=1: the aim offers the cone of its ray also with far_grab=0, and an object that the draw does not offer keeps the aim at the place of its mesh while the ray is on it".
- Bookmark 1: "the grip sets bookmark 1 on the land at 2250.8 2473.7 (Bookmark::SetBookMark), 3.0 m of the room along the ray, frame 331", then "bookmark 1 holds no object: its map point is 2250.8 2473.7" at frames 333, 451 and 631.
- Bookmark 2: "the aim is on the Sheep 12AEFD30, frame 593". No "the aim is on the land" line from frame 593 to the grip.
- "the grip sets bookmark 2 on the LeashObj 12AEFD30 at 1996.9 2330.4 (fn_00439F70), 2.0 m of the room along the ray, frame 631", "the game: bookmark 2 is set, frame 632".
- "bookmark 2 holds the Sheep 12AEFD30: its map point is 1996.9 2330.4, and the object is at 1996.9 2330.4" at frames 633, 751 and 931.
- "the game: bookmark 2 is not set, frame 1362". The third aim starts at 1611 and "the aim of bookmark 2 stops: the player pressed B, frame 1651".

## bk2 (far_grab=1): PASS

- "far grab    : on. Point the hand at an object, close the grip and pull toward the head ...", and the same bookmark_aim_hold=1 line.
- Bookmark 1 is set on the land at frame 331 (Bookmark::SetBookMark) and "holds no object" at 333, 451 and 631.
- "the aim is on the Sheep 13AC5D30, frame 593". No "the aim is on the land" line before the grip.
- "the grip sets bookmark 2 on the LeashObj 13AC5D30 at 1996.9 2330.4 (fn_00439F70), 2.0 m of the room along the ray, frame 631".
- "bookmark 2 holds the Sheep 13AC5D30: its map point is 1996.9 2330.4, and the object is at 1996.9 2330.4" at frames 633, 751 and 931.
- Remove bookmark 2 at 1361 gives "bookmark 2 is not set, frame 1362". B stops the third aim at 1651.

## bk3 (far_grab=0, bookmark_aim_hold=0, the old behavior): PASS

- "bookmark aim : bookmark_aim_hold=0: the cone of the aim needs far_grab=1, and an object that the draw does not offer keeps the aim for 10 frames".
- "the aim is on the Sheep 13ACED30, frame 593", then "the aim is on the land at 1998.3 2324.4, frame 605" (12 frames later).
- "the grip sets bookmark 2 on the land at 1998.3 2324.4 (Bookmark::SetBookMark), 2.0 m of the room along the ray, frame 631".
- "bookmark 2 holds no object: its map point is 1998.3 2324.4" at frames 633, 751 and 931.

## Notes

- The grip line names the object class "LeashObj", and the "holds" lines name the same pointer "Sheep".
- The Sheep did not walk in any run: the map point and the place of the object are 1996.9 2330.4 in each line. So these runs do not show the map point following a moving object.
- After the runs, the newest file of Profiles in the game folder and in its VirtualStore copy is 20.sav of 6 November 13:07. The newest file of local\test-saves is of 6 November 13:09. The profile and the test save are clean.
