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
// THE FIRST DRAW OF A FONT. The first DrawTextW of a font at a size costs 6
// to 11 ms: GDI makes each letter, and the text layout of Windows makes its
// data for the font. Both are kept for the process. With font_warm=1 a
// thread of the mod draws each letter of each font of the cards one time
// with DrawTextW at the start of the game (WarmStart), so the first card,
// the book, the ring, the keyboard and the panels show with no such cost on
// the frame thread.
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

// A job for the thread of the warm, after the letters of the fonts: the page
// and the parts of a panel (panel_warm, panel_kit.h). mod_main.cpp adds each
// job before WarmStart. At most 4 jobs. A job runs on the thread of the
// warm, so the frame thread must not use what the job makes until WarmWait
// returns.
typedef void (*WarmJob)();
void WarmAdd(WarmJob job);

// Returns when the thread of the warm has ended, with its jobs. It returns
// at once when the thread has ended, when no thread started, and on the
// thread of the warm. A panel that a job draws calls it before it uses its
// picture. The answer is the time of the wait in ms.
double WarmWait();

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

// THE WAX OF A CHOICE. The ring (radial_menu.cpp) and the spell book use the
// same seals. Seal gives the red wax. The seal under the hand has the gold wax
// with dark letters (the tone 2 of SealInk), a gold rim and a soft glow. A
// seal that cannot act now has a pale wax.
const int kWaxGold = 0;      // the seal under the hand
const int kWaxPaleGold = 1;  // the seal under the hand that cannot act now
const int kWaxPale = 2;      // a seal that cannot act now
// The edge round a seal for its shadow, its rim and its glow.
const int kSealEdge = 22;
// THE ROUND SEAL of an item with an icon: kMedal px across, and kMedalChosen
// px under the hand. Seal draws a box of at most 102 px high. The icon is
// kMedalIcon px in a seal of kMedal px, and the ring of the stamp is
// kStampInset px in from the edge.
const int kMedal = 96;
const int kMedalChosen = 102;
const int kMedalIcon = 56;
const int kStampInset = 10;

// Gives the red wax of the seal in the picture the wax kWaxGold, kWaxPaleGold
// or kWaxPale. Each pixel keeps its brightness against the middle brightness
// of the opaque wax, and a brighter pixel (the lit side, the sheen) goes
// toward the light of that wax. The picture must hold the seal only.
void Rewax(const Picture& picture, int wax);

// The gold rim and the glow of the seal under the hand, under the seal: a
// band of 4 px at 5 px out from the box with round ends, and a glow that ends
// 20 px out. strength 1 is the full rim, 0.5 the rim of a seal that cannot
// act now.
void Rim(const Picture& picture, int x0, int y0, int x1, int y1,
         float strength);

// The ring of the stamp in a round seal (the edge of the die that pressed the
// wax) and an icon of ring_icon_math, into a letters picture of GDI: the green
// of a pixel is its cover, and a pixel keeps the larger cover. SealInk then
// presses them into the wax as it presses letters.
void StampLetters(uint32_t* letters, int pitch, int height, float cx, float cy,
                  float radius);
void IconLetters(uint32_t* letters, int pitch, int height, int icon, float x0,
                 float y0, float size);

// THE SCROLL of the spell book (spell_book_look=1). Clears the picture, then
// draws the drop shadow, the paper from x0 to x1 and from y0 to y1 with a torn
// and burnt edge at the left and the right, and a roll of the paper at the top
// and at the bottom. Each roll has its middle on the edge of the paper, is a
// little longer than the paper is wide, and has a rod of dark wood with a
// turned knob at each end. kScrollRoll is the thickness of a roll, and the
// rod and its knobs go kScrollKnobOut px out from each side of the paper.
const int kScrollRoll = 48;
const int kScrollKnobOut = 52;
void Scroll(const Picture& picture, int x0, int y0, int x1, int y1, int seed);

// THE SOCKETS of the spell book. A socket is a round hole in the page with a
// rim of dull brass and a ring of ink round it. The hole has the radius hole
// and its middle at cx, cy. The rim and the ring of ink go out to
// kSocketOut px more than the hole.
const int kSocketOut = 13;
const int kSocketFull = 0;   // a socket with a miracle: the hole stays paper
                             // here, the caller clears it for the orb
const int kSocketEmpty = 1;  // no miracle: the hole is a dip in the paper with
                             // a faint star of ink
const int kSocketLit = 2;    // the socket under the hand: a gold rim and glow
const int kSocketDim = 4;    // a miracle that the game refuses now: pale brass
void Socket(const Picture& picture, float cx, float cy, float hole, int look,
            int seed);

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
