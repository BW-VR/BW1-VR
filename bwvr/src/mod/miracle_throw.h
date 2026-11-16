// A thrown miracle (docs/throw.md, "A thrown miracle"). A Fireball and a
// flock leave the Hand with the velocity of the real hand, as an object does,
// and the log follows the flight of a Fireball.

#pragma once

#include <stdint.h>

// miracle.cpp, in its Init: reads miracle_throw and installs the hook that
// follows a Fireball.
void MiracleThrowInstall();

// miracle.cpp, in the hook of the cast at the land point (FUN_005D3340),
// before the game reads the throw velocity, for a miracle that is cast at the
// release with no gesture of its own (a Fireball and the two flocks). hand is
// the CHand. With miracle_throw=1 the velocity of the real hand goes to
// CHand+0x48C8.
void MiracleThrowBeforeCast(uint8_t* hand, const char* name);

// miracle.cpp, after that call: sent is true when the game sent the cast.
void MiracleThrowAfterCast(bool sent);

// miracle.cpp, once a frame: the end of the log of a flight.
void MiracleThrowFrame();

// The debug row "Miracle throw" (debug_menu.cpp).
int MiracleThrowValue();
void MiracleThrowChange(const char* by);
