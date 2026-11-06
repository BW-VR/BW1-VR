// The scroll look of the cards of the mod (hint_card_style=1). See
// docs/controller-hints.md, "The look of the card".
//
// A card is a parchment with a rolled end at each side, dark brown ink, and a
// wax seal for each button word. All of it is drawn in code from one small
// noise tile that Start makes one time. No picture comes from a file. The
// fonts are Alegreya and Alegreya SC (SIL Open Font License) from the folder
// bwvr-fonts of the game folder. When a font file is not there, the words use
// Georgia, and then Segoe UI.
//
// THE GRAIN. The headset shows a card smaller than its picture, so a detail
// of 1 pixel would shimmer. The smallest cell of the noise is 4 pixels.
//
// THE FIRST DRAW OF A FONT. GDI makes each letter of a font at a size the
// first time that it draws it, and keeps it for the process. For the words
// of a card that is 7 to 30 ms. With font_warm=1 a thread of the mod draws
// each letter of each font of the cards one time at the start of the game
// (WarmStart), so the first card, the book, the ring, the keyboard and the
// panels show with no such cost on the frame thread.
//
// THE COST. A redraw makes no blur. The drop shadow comes from the outline of
// the shape (a distance test). Start bakes the color of the paper into the
// tile, so a pixel of paper is one read and the burn of its edge. A pixel
// deep in the paper is one read only: the tile also holds it with the burn.

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

namespace card_scroll {

// The parts of the scroll around the paper, in pixels of the card picture.
const int kRollWidth = 22;  // the width of a rolled end
const int kRollOver = 9;    // a roll goes this far above and below the paper
const int kInset = 14;      // from the inner side of a roll to the words
// The free edge of the picture for the drop shadow. The shadow falls down and
// to the right.
const int kEdgeLeft = 3;
const int kEdgeTop = 3;
const int kEdgeRight = 10;
const int kEdgeBottom = 12;
// From the left edge of the picture to the words, and from the end of the
// words to the right edge.
const int kWordsLeft = kEdgeLeft + kRollWidth + kInset;
const int kWordsRight = kInset + kRollWidth + kEdgeRight;
// From the top edge of the picture to the paper, and from the paper to the
// bottom edge.
const int kPaperTop = kEdgeTop + kRollOver;
const int kPaperBottom = kRollOver + kEdgeBottom;

// The wax of a seal, in the order of the tones of hint_card.cpp: 1 the
// trigger (green), 2 the grip (amber), 0 and 3 any other button (red).
const int kWaxTones = 4;

// A picture: BGRA, color multiplied by alpha. pitch is in pixels. The parts
// draw only in 0 to width and 0 to height.
struct Picture {
  uint8_t* bgra;
  int pitch;
  int width;
  int height;
};

// Makes the noise tile and loads the fonts, one time for all the cards. dc
// is a memory DC for the font tests. Each thread can call it: the second
// call waits for the first one to end. The fonts are in the folder bwvr-fonts
// beside runblack.exe, or in the folder of the test key
// hint_card_fonts_test_dir of bwvr.ini. False when GDI gives no font: the
// caller keeps its plain look.
bool Start(HDC dc);

// A font of the words or of the seals, with a capital letter of cap pixels.
HFONT WordsFont(HDC dc, int cap);
HFONT SealFont(HDC dc, int cap);

// A font for digits that are as high as a capital letter, with a capital
// letter of cap pixels: Palatino Linotype, then Book Antiqua, then Cambria
// (all fonts of Windows), then the font of the words. face gets the name of
// the face in use, for the log.
HFONT DigitsFont(HDC dc, int cap, const char** face);

// The fonts in use, for the log.
const char* FontNote();

// font_warm=1 and hint_card_style=1 in bwvr.ini: starts a thread of the mod
// that calls Start and then draws each letter of each font of the cards one
// time (the table kWarm of card_scroll.cpp). mod_main.cpp calls it at the
// install of the hooks. One time only.
void WarmStart();

// The time that Start took, in ms.
double StartMs();

// Clears the picture, then draws the drop shadow, the paper and the two
// rolls. The paper goes from x0 to x1 (the outer sides of the rolls) and from
// y0 to y1 (its top and bottom edges). The rolls go kRollOver more up and
// down. seed moves the grain, so two cards do not look the same.
void Paper(const Picture& picture, int x0, int y0, int x1, int y1, int seed);

// Clears the picture, then draws a page of the same paper from x0 to x1 and
// from y0 to y1, with a torn and burnt edge at each of its four sides, and
// with no roll and no shadow. For a card whose layout fills its picture.
void Page(const Picture& picture, int x0, int y0, int x1, int y1, int seed);

// A wax seal with round ends in the box, with its own small shadow.
void Seal(const Picture& picture, int x0, int y0, int x1, int y1, int tone,
          int seed);

// The ink of the words. letters is a GDI picture with the same pitch: white
// letters on black, and the green of a pixel is its cover. Only the box
// changes.
void Ink(const Picture& picture, const uint32_t* letters, int x0, int y0,
         int x1, int y1, int seed);

// The word on a seal, in the box of the seal, in the color for its wax.
void SealInk(const Picture& picture, const uint32_t* letters, int x0, int y0,
             int x1, int y1, int tone);

// The looks of a key of the keyboard in the headset (vr_keyboard.cpp).
const int kKeyPlain = 0;  // a frame of ink round a face pressed into the paper
const int kKeyHover = 1;  // the key under the ray: a gold face and a gold rim
const int kKeyFlash = 2;  // the key that the trigger pressed: bright gold
const int kKeyLooks = 3;
// The rim of a key goes this far out from the key.
const int kKeyRim = 3;

// A key from x0 to x1 and from y0 to y1, over the paper that is there. It
// changes only the box of the key and kKeyRim pixels round it. look is
// kKeyPlain, kKeyHover or kKeyFlash. The letter goes on with Ink.
void Key(const Picture& picture, int x0, int y0, int x1, int y1, int look,
         int seed);

}  // namespace card_scroll
