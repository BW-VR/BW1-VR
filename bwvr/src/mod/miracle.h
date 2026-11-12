// A miracle in the Hand (docs/miracles.md). The Hand
// with a miracle is the real hand, the cast has its target in the pick record
// of the game, a shake or a button takes the miracle out of the Hand, and the
// card says what each control does.

#pragma once

#include <stdint.h>

struct FarDraw;

// mod_main.cpp, with the direct hand: reads the keys and installs the hooks.
void MiracleInstall();

// village.cpp: the name of a seed type for the log, "a miracle" for a type
// that the table does not have.
const char* MiracleSeedName(int type);

// stereo.cpp, once a frame, after HandDirectFrame.
void MiracleFrame();

// stereo.cpp, once a report window.
void MiracleReport();

// hand_direct.cpp: miracle_hand is on, so the controller places the Hand in
// the Grain state too (the state of a miracle in the Hand).
bool MiraclePlacesHand();

// The debug row "Spawn orbs" (debug_menu.cpp): one one-off seed of each
// miracle on the land in front of the head, made by the game. The value is 1
// while each of these orbs is on the land. A press makes the orbs that are
// gone.
int MiracleOrbsValue();
void MiracleOrbsChange(const char* by);

// mod_main.cpp, at the start of GameOSFile::SaveAllGame: the game deletes
// each of these orbs that lies on the land, so no save holds them.
void MiracleBeforeSave();

// tabletop.cpp, after GGame::ClearMap (a load, a new game, a new land): the
// list of the orbs is stale, so the mod drops it and deletes nothing.
void MiracleMapCleared();

// hand_direct.cpp: a miracle is in the Hand, so the draw must offer the
// objects in the cone of the aim to the pick, as it does for a far grab.
bool MiracleWantsCone();

// hand_direct.cpp, at the end of the first eye pass: the target of a miracle
// for an object, from the objects that the draw offered.
void MiracleSelect();

// hand_direct.cpp, after each eye pass, before each interface step and
// before each press of the Action button: the pick record holds the target of
// the miracle, or no object.
void MiracleKeepRecord();

// hand_direct.cpp, before the interface step of the game (fn_005D1120): a
// cancel or a tap that the player asked for runs here, where the game itself
// runs them.
void MiracleInterfaceStep(void* face);

// controllers.cpp, once a pump. The trigger makes a seed that the Hand
// carries active. True while the trigger must not reach the game.
bool MiraclePumpTrigger(bool trigger, bool ok);

// controllers.cpp, once a pump: the grip looks open to the grip machine while
// the mod lets a seed go, until the real grip opens.
bool MiracleGripMasked(bool grip_down);

// hand_glow.cpp: the halo of the target of the miracle.
bool MiracleHalo(FarDraw* out);

// hint_card.cpp: the lines of the card while a miracle, or a seed, is in the
// Hand. word is a ControllerHintWord, or -1 for a line with no button. The
// return value is the count of lines, 0 with no miracle.
struct MiracleLine {
  int word;
  const wchar_t* text;
};
const int kMiracleLines = 3;
int MiracleHint(MiracleLine lines[kMiracleLines]);

// miracle_watch.cpp: the log of what a cast makes in the world. miracle.cpp
// calls the frame step once a frame with the state of the Hand, the circle
// at a cast at the circle of a Storm or a shield (the middle of the circle),
// and the Creature at a cast on a Creature. The report runs once a report
// window.
// MiracleWatchInstall puts in the hooks of the watch, which only read and log:
// the drops of Water, the bodies near a Physical Shield, and a teleport.
void MiracleWatchInstall();
void MiracleWatchFrame(uint64_t frame, int hand_state);
void MiracleWatchCircle(const char* name, float x, float z);
void MiracleWatchCreature(void* creature, const char* name);
void MiracleWatchReport();

// gesture_air.cpp, through controllers.cpp: the grip is closed with a miracle
// that needs a circle on the land, so the path of the hand goes to the game.
bool MiracleDrawsCircle();

// temple.cpp, in GInterface::SendInvisibleDrawCollision: the game offers an
// object with no mesh to its pick as a ball. A teleport site becomes a
// candidate of the real hand (miracle_site_pick=1). True when it does.
bool MiracleOfferSite(void* object, const float point[3], float radius);
