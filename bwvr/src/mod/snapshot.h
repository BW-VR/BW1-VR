// The snapshot for a report: the item "snapshot" of the ring saves the eye
// pictures of the next frame and a copy of the log in a folder of its own.
// See snapshot.cpp and docs/controllers.md, "The snapshot".

#pragma once

#include <stdint.h>

// snapshot_item of bwvr.ini: 1 the ring has the item.
bool SnapshotOn();

// True while a snapshot waits for its pictures or writes its files.
bool SnapshotBusy();

// The ring chose the item at the frame of the controllers. The next frame
// saves the eye pictures, and a thread then writes the folder. False while
// a snapshot is busy.
bool SnapshotStart(uint64_t frame);

// Once a frame (radial_menu.cpp): 1 in the frame in which the files are
// saved, -1 in the frame in which the save failed, else 0.
int SnapshotFrame();

// The note of the card of the ring after a snapshot.
const wchar_t* SnapshotNote();
