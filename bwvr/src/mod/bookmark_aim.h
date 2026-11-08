// The aim of a new bookmark: after the seal with the plus on the sheet of the
// places (radial_menu.cpp), the pointing hand shows its ray, and the grip
// sets the bookmark at the land point or on the object under the ray. See
// bookmark_aim.cpp and docs/controllers.md, "The sheet of the places".

#pragma once

#include "miracle.h"

struct FarDraw;
struct TeleportDraw;

// bookmark_aim=1: the seal with the plus waits for the grip at the aim. 0:
// it sets the bookmark at the Hand, as Ctrl and the number do.
bool BookmarkAimOn();

// radial_menu.cpp, at the choice of the seal with the plus: the aim starts
// for bookmark `mark` (0 to 6). False when it cannot start.
bool BookmarkAimStart(int mark);

// True while the aim waits for the grip.
bool BookmarkAimActive();

// controllers.cpp, once a frame, after RadialMenuFrame: the target of the
// aim (an object or the land), and the end of the aim when the game takes no
// bookmark now.
void BookmarkAimFrame();

// hand_direct.cpp, once a frame: true while the aim needs the cone of the aim
// ray, so the draw offers the objects in it.
bool BookmarkAimWantsCone();

// controllers.cpp, once a pump, before the grip goes to the far grab and the
// grip machine. grip is the grip of the pointing hand. A press sets the
// bookmark at the target. The answer is true while the grip belongs to the
// aim: from the start of the aim until the grip that set the bookmark goes
// up.
bool BookmarkAimPumpGrip(bool grip);

// controllers.cpp: B of the pointing hand and the menu button stop the aim,
// and so does the ring. True when the aim was on: then the press does no
// more.
bool BookmarkAimStopBy(const char* why);

// controllers.cpp, ControllersMenuRay: true while the aim is on. meters gets
// the length of the ray to the target, in meters of the room, or 0 for a
// ray with no target.
bool BookmarkAimRay(float* meters);

// overlay.cpp: a gold ring on the land round the land point of the aim.
// False with no land target.
bool BookmarkAimRing(TeleportDraw* out);

// hand_glow.cpp: the gold halo of the object of the aim. False with no
// object target.
bool BookmarkAimHalo(FarDraw* out);

// hint_card.cpp: the lines of the pointing card while the aim is on. The
// return value is the count of lines, 0 with no aim.
int BookmarkAimHint(MiracleLine lines[kMiracleLines]);

// controllers.cpp, for the word "aimmark" of the scripted controller: the
// point of the next target of the test key bookmark_test_aim, in the world.
bool BookmarkAimTestPoint(float out[3]);

// Once a report window.
void BookmarkAimReport();
