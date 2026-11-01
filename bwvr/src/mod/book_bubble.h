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
// True after a check of the game's code failed: then the book uses its own
// picture of the bubble.
bool BookBubbleFailed();
// The counts of the report window, for the report line of the book.
void BookBubbleCounts(int* shown_frames, int* draws, int* made);
