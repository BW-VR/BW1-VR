// The game's own bubble of a miracle that the hand pulls out of the spell
// book (book_bubble.cpp, docs/miracles.md, "The pull").
#pragma once

#include <stdint.h>

// The bubble of the seed type `seed` shows in this frame at the world point
// `world`, with this many world units in a meter of the room. The game thread
// calls it in each frame that the bubble shows.
void BookBubbleShow(int seed, const float world[3], float units_per_meter,
                    uint64_t frame);
// sky_detail.cpp: once in each eye pass, after the overlay's meshes.
void BookBubbleDraw();
// gesture_panel.cpp, inside its draw of an eye pass, with the camera of the
// eye in place: one part of a bubble at the world point. seed -1 is the empty
// glass ball, and a seed type is the graphic of that miracle. radius_units is
// the radius of the ball in world units, and alpha is 0 to 255.
void BookBubbleDrawPart(int seed, const float world[3], float radius_units,
                        int alpha);
// True after a check of the game's code failed: then the book uses its own
// picture of the bubble.
bool BookBubbleFailed();
// The counts of the report window, for the report line of the book.
void BookBubbleCounts(int* shown_frames, int* draws, int* made);
