// THE PHYSICS SWITCH (docs/throw.md, "The physics switch"). physics=1 (the
// launcher row "Physics", VR) keeps the throw physics of the mod. physics=0
// (Original) gives each key of the preset the value of the plain game, so a
// thrown object and a thrown Fireball fly, land, bounce, turn and come to
// rest as in the plain game. A key of the preset that bwvr.ini sets itself
// keeps the value of bwvr.ini.

#pragma once

// mod_main.cpp, in ReadSettings, before it reads the keys of the throw:
// reads physics and which keys of the preset bwvr.ini sets.
void PhysicsSwitchRead(const char* ini_path);

// The default of a key of the preset for the value of physics now: the
// value of the mod or the value of the plain game. fallback for a key that
// is not in the preset. Read the key with this default, so that a key in
// bwvr.ini wins.
int PhysicsSwitchDefault(const char* key, int fallback);

// True when the switch sets the key: it is in the preset and bwvr.ini does
// not set it.
bool PhysicsSwitchSets(const char* key);

// mod_main.cpp, after the throw is set up: one log line with physics and
// the value of each key of the preset.
void PhysicsSwitchLog();

// The debug row "Physics" (debug_menu.cpp). The change gives each key of
// the preset that bwvr.ini does not set its new value. The next throw uses
// it.
int PhysicsSwitchValue();
void PhysicsSwitchChange(const char* by);
