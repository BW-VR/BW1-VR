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

// miracle_flight (docs/throw.md, "The flight of a thrown Fireball"): 1 lets
// a thrown Fireball fly the real arc of the throw until its first contact, 0
// keeps the game's flight. MiracleThrowInstall reads the key, and the
// physics switch (physics_switch.cpp) sets it when the switch changes.
void MiracleThrowSetFlight(int flight);

// The debug row "Miracle flight" (debug_menu.cpp).
int MiracleFlightValue();
void MiracleFlightChange(const char* by);

// The debug rows "Fireball gravity" and "Fireball speed" (debug_menu.cpp):
// miracle_flight_gravity_pct and miracle_flight_speed_pct, the light ball of
// the arc. A step goes to the next value of the list of the row, or to the
// value before it with `back`. The next throw uses the new value.
int MiracleFlightGravityValue();
int MiracleFlightSpeedValue();
void MiracleFlightGravityStep(bool back, const char* by);
void MiracleFlightSpeedStep(bool back, const char* by);

// The debug row "Fireball bounce" (miracle_bounce_scale): 1 lets a ball move
// in the room after its first contact as it does at 100 units per meter, 0
// keeps the game's constants. The next throw uses the new value.
int MiracleBounceValue();
void MiracleBounceChange(const char* by);

// The debug row "Fireball top speed" (miracle_flight_top_speed): the cap of
// the start speed of the arc, in world units a second. 0 is no cap. The next
// throw uses the new value.
int MiracleTopSpeedValue();
void MiracleTopSpeedStep(bool back, const char* by);
