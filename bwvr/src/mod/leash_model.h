// The pictures of the three leashes on the sheet of the leashes of the ring
// (radial_menu.cpp). The mod reads the mesh of the leash and its texture from
// the files of the game on the player's PC, and draws each picture on the
// CPU. No picture and no mesh of the game is in the mod. See leash_model.cpp
// and docs/controllers.md, "The sheet of the leashes".

#pragma once

#include <stdint.h>

// leash_pictures=1 and the files of the game gave the mesh and the texture.
// The first call reads the files.
bool LeashPicturesOn();

// Draws the leash of `kind` (1 Aggression, 2 Learning, 3 Compassion), turned
// by `turn_deg` about the up axis, into a box of size by size pixels at x0,
// y0 of a BGRA picture with the color times the alpha (pitch in pixels). It
// draws over the pixels that are there, at `alpha` (1 for all of it). size
// is at most 192. False when there is no picture: the caller then draws the
// icon of the kind.
bool LeashModelDraw(uint8_t* bgra, int pitch, int kind, float turn_deg, int x0,
                    int y0, int size, float alpha);
