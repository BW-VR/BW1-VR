// The parts of a panel in the room that the ray and the trigger of the
// pointing hand press. The panel of places (places_panel.cpp) is built on
// it. See docs/ui-layer.md, "The panel of places", 22.6.
//
//   Room     the place of the panel in the room, the ray on it, the part
//            under the ray with a light pulse, the press of the trigger
//            with a stronger pulse, the flash of a pressed part, and the
//            quad layer of the frame
//   Canvas   the picture: a GDI picture for the letters and for the dark
//            look, and the parchment (card_scroll.h) as a page that is
//            drawn one time and a card that each frame shows
//   Tiles    a copy of the box of each part in each look, so a new picture
//            copies the parts that changed and draws a part one time only
//
// The math (the place, the hit test, the trigger) is in panel_kit_math.h.

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "panel_kit_math.h"

namespace panel_kit {

// --- the room --------------------------------------------------------------------

struct Room {
  panel_kit_math::Panel panel;
  float distance, pitch, width, height;  // the place of the last Open
  uint64_t serial;                       // HeadPoseOriginSerial at the place
  bool open;
  bool on_panel;   // the ray meets the panel in this frame
  float u, v;      // where, 0 to 1 across and down
  float meters;    // the length of the ray to the panel, 0 when it is off
  int hover;       // the part under the ray, or -1
  int flash;       // the part that the trigger pressed, for kFlashMs
  ULONGLONG flash_until;
  panel_kit_math::Press press;
};

// The part at u, v, or -1 for none.
typedef int (*HitFn)(float u, float v, void* context);

// Opens the panel in front of the head of now (MenuRoomHeadPose):
// `distance` meters out, `pitch` radians above eye level, width by height
// meters. A trigger that is down now is no press. False when there is no
// head pose yet.
bool Open(Room* room, float distance, float pitch, float width, float height);
void Close(Room* room);

// Once a frame while the panel is open. A recenter of the headset places the
// panel again in front of the head, and the answer is then true.
bool FollowRecenter(Room* room);

// Once a frame while the panel is open: the ray of the pointing hand on the
// panel, the part under it (a light pulse at each new part), and the press
// of the trigger. The answer is the part that the trigger pressed in this
// frame, or -1. The caller then calls Pressed (a pulse and a flash) or
// gives the pulse "refused". trigger gets the value of the trigger, 0 with
// no hand.
int Step(Room* room, HitFn hit, void* context, float* trigger);

// A press that acts: the stronger pulse, and the part flashes for 120 ms.
void Pressed(Room* room, int part);

// The part that flashes now, or -1.
int Flashing(Room* room);

// The point of the room at u, v of the panel (LOCAL space), for the word of
// a test of the scripted controller.
void PointOf(const Room& room, float u, float v, float out[3]);

// The quad layer of this frame: the picture of width by height pixels
// (BGRA, the color multiplied by the alpha) at the place of the panel. The
// panel uses the quad layer of the ring (kCardRing). False when the runtime
// refused that layer.
bool StageLayer(const Room& room, const uint8_t* pixels, int width,
                int height);

// --- the picture -----------------------------------------------------------------

struct Font {
  HFONT font;
  int cap;  // the capital letter, in pixels
  int ascent;
  int height;
};

struct Canvas {
  int width, height;
  const char* tag;    // for the log, for example "places      "
  HDC dc;
  HBITMAP dib;
  uint32_t* bits;     // the GDI picture: the letters, and the dark look
  uint8_t* page;      // the parchment with no parts
  uint8_t* card;      // the parchment that the layer shows
  bool failed;
  bool parchment_failed;
  bool parchment_ready;
};

// The GDI picture. False when GDI gives none: the panel does not show.
bool Start(Canvas* canvas, int width, int height, const char* tag);

// The parchment: the fonts of the cards, then the page. draw_page draws the
// parts of the page that never change (a title) into canvas->page, after
// the paper. False when there are no fonts or no memory: the caller uses
// the dark look.
typedef void (*PageFn)(Canvas* canvas, void* context);
bool StartParchment(Canvas* canvas, PageFn draw_page, void* context);

// A font of the words (Alegreya Bold), of the seals (Alegreya SC) or of the
// digits, with a capital letter of `cap` pixels. Null when GDI gives none.
Font WordsFont(Canvas* canvas, int cap);
Font SealFont(Canvas* canvas, int cap);
Font DigitsFont(Canvas* canvas, int cap, const char** face);

// Words of dark ink in the box, with the capital in the middle of the box.
// `to_page` draws into the page, else into the card.
void InkWords(Canvas* canvas, const Font& font, const wchar_t* words,
              const int box[4], int seed, bool to_page);

// The box (x0, y0, x1, y1) of the card gets the page again.
void PageToCard(Canvas* canvas, const int box[4]);

// A part that cannot act now: its box of the card goes 65 percent of the
// way back to the page.
void Fade(Canvas* canvas, const int box[4]);

// The card starts again from the page, with no part on it.
void ClearCard(Canvas* canvas);

// --- the parts -------------------------------------------------------------------

// The looks of a button.
enum Look { kPlain = 0, kHover = 1, kFlash = 2, kDim = 3, kLooks = 4 };

// A BUTTON of the parchment: a key of ink (card_scroll::Key) with its word.
// Under the ray it is gold, after a press bright gold, and a dim button is
// faded. rect is x, y, w, h. The button changes its rect and a rim of
// panel_kit::kRim pixels round it.
const int kRim = 3;
void Button(Canvas* canvas, const int rect[4], int look, const wchar_t* word,
            const Font& font, int seed);

// The copies of the boxes of the parts, for `parts` parts in `looks` looks.
struct Tiles {
  int parts;
  int looks;
  uint8_t** tiles;  // parts * looks, null for a box that has no copy yet
};
bool MakeTiles(Tiles* tiles, int parts, int looks);

// Puts part `part` in `look` into the card: a copy of its box, or a draw
// with `draw` that then keeps a copy. box is x0, y0, x1, y1, and the draw
// must change no pixel out of it. True for a copy.
typedef void (*DrawFn)(Canvas* canvas, int part, int look, void* context);
bool ShowPart(Canvas* canvas, Tiles* tiles, int part, int look,
              const int box[4], DrawFn draw, void* context);

// THE DARK LOOK (hint_card_style=0), drawn all of it in the GDI picture.
void PlainFill(Canvas* canvas, const int box[4], COLORREF color);
void PlainText(Canvas* canvas, HFONT font, COLORREF color,
               const wchar_t* words, const int box[4]);
void PlainButton(Canvas* canvas, const int rect[4], int look,
                 const wchar_t* word, HFONT font);
// After a dark picture: each pixel opaque.
void PlainDone(Canvas* canvas);

// hint_card_style now: the switch "Card look" of the debug page, else
// `ini` (the value of bwvr.ini).
int CardStyle(int ini);

}  // namespace panel_kit
