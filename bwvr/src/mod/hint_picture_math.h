// The pictures of the buttons on the hint cards (hint_card_pictures=1): a
// small part of the drawing of the page Controls (controller_art_math.h)
// around one control, with that control lit. See hint_card.cpp and
// docs/controller-hints.md, "The pictures of the buttons". No Windows and no
// game here, so the math test compiles this file (hint_picture_checks.h).
//
// THE WINDOW. A full controller in the height of a row of the card is too
// small to read in a headset: a face button would be 4 px of the view. So
// the picture shows a square window of kWindow units of the drawing, with
// the lit control near its middle. The window holds the controls around the
// lit one and the outline of the controller, so the player sees where the
// control is. All the windows have the same scale.

#pragma once

#include <math.h>
#include <stdint.h>
#include <wchar.h>

#include "button_map_math.h"
#include "controller_art_math.h"

namespace hint_picture {

namespace art = controller_art;

// The picture is kSize by kSize px of the card, and kGap px from the button
// word. A card has 40 px a cm, so the picture is 1.5 cm. A row of the card
// is 66 px.
const int kSize = 60;
const int kGap = 10;
// The side of the window in units of the drawing, and the most that the
// window goes past the box of the drawing.
const float kWindow = 120.0f;
const float kPast = 16.0f;
inline float Scale() { return kSize / kWindow; }

// The strokes and the halo, in px of the picture.
const float kBodyStroke = 2.6f;
const float kControlStroke = 2.0f;
const float kLitStroke = 1.6f;
const float kHalo = 5.0f;
// The edge of the window fades out over kFade px, with round corners of
// kCorner px, so a cut outline does not end in a hard line.
const float kFade = 6.0f;
const float kCorner = 16.0f;

// --- the control of a word -------------------------------------------------------

// True when `word` is `name`, or `name` and " x2" or " hold" (hint_card.cpp,
// AddPromptTwice and AddPromptHold).
inline bool WordIs(const wchar_t* word, const wchar_t* name) {
  if (word == nullptr || name == nullptr) return false;
  const size_t n = wcslen(name);
  if (wcsncmp(word, name, n) != 0) return false;
  const wchar_t* rest = word + n;
  return rest[0] == 0 || wcscmp(rest, L" x2") == 0 ||
         wcscmp(rest, L" hold") == 0;
}

// The control of the drawing that a button word of a card names, on the
// controller of `kind` at the side `left`, or -1 when the drawing does not
// show it. The words are those of controller_hints.cpp (BuildWords).
inline int ControlOfWord(int kind, bool left, const wchar_t* word) {
  if (word == nullptr || word[0] == 0) return -1;
  int control = -1;
  for (int b = 0; b < button_map::kButtons && control < 0; ++b) {
    if (WordIs(word, button_map::ButtonWord(kind, left, b))) control = b;
  }
  if (control < 0) {
    if (WordIs(word, L"Trigger") || WordIs(word, L"Select")) {
      control = art::kTrigger;
    } else if (WordIs(word, L"Grip")) {
      control = art::kGrip;
    } else if (WordIs(word, L"Stick") || WordIs(word, L"Pad")) {
      control = art::kStick;
    }
  }
  if (control < 0 || !art::Shows(kind, left, control)) return -1;
  return control;
}

// --- the window ------------------------------------------------------------------

// The middle of the window of a control, in units of the frame of the right
// controller: the middle of the box of the control, kept so that the window
// goes at most kPast past the box of the drawing. The box is the same for
// the mirror, so the left controller has the same window in its frame.
inline void Window(int kind, int control, float* cx, float* cy) {
  const art::Art a = art::ArtOf(kind);
  float b[4];
  art::Bounds(a.control[control], b);
  float x = (b[0] + b[2]) * 0.5f, y = (b[1] + b[3]) * 0.5f;
  const float half = kWindow * 0.5f;
  const float x0 = half - kPast, x1 = art::kArtWidth - half + kPast;
  const float y0 = half - kPast, y1 = art::kArtHeight - half + kPast;
  x = x < x0 ? x0 : (x > x1 ? x1 : x);
  y = y < y0 ? y0 : (y > y1 ? y1 : y);
  *cx = x;
  *cy = y;
}

// A pixel of the picture in units of the frame of the controller. The left
// controller is the mirror of the frame (controller_art_math.h).
inline void ToUnits(bool left, float cx, float cy, float px, float py,
                    float* ux, float* uy) {
  const float s = Scale();
  const float dx = (px - kSize * 0.5f) / s;
  *ux = left ? cx - dx : cx + dx;
  *uy = cy + (py - kSize * 0.5f) / s;
}

// --- the picture -----------------------------------------------------------------

struct Paint {
  float r, g, b;
};

// The colors of one look of the card. ink: the outlines. lit: the fill of the
// lit control, and its halo. edge: the outline of the lit control. wash: the
// inside of the controller, at wash_a. dim: the strength of the outlines of
// the controls that are not lit.
struct Look {
  Paint ink;
  Paint lit;
  Paint edge;
  Paint wash;
  float wash_a;
  float dim;
};

// A color over a pixel: BGRA, color multiplied by alpha.
inline void Over(uint8_t* out, const Paint& c, float a) {
  if (a <= 0.0f) return;
  if (a > 1.0f) a = 1.0f;
  const float k = 1.0f - a;
  out[0] = static_cast<uint8_t>(c.b * a + out[0] * k + 0.5f);
  out[1] = static_cast<uint8_t>(c.g * a + out[1] * k + 0.5f);
  out[2] = static_cast<uint8_t>(c.r * a + out[2] * k + 0.5f);
  out[3] = static_cast<uint8_t>(255.0f * a + out[3] * k + 0.5f);
}

// The cover of a pixel at a signed distance of d px from an edge.
inline float Cover(float d) {
  return d <= -0.5f ? 1.0f : (d >= 0.5f ? 0.0f : 0.5f - d);
}

// The strength of a pixel at px, py of the picture: 1 inside, 0 at the edge
// of the round box of the picture, and a smooth step over kFade px.
inline float EdgeFade(float px, float py) {
  const float h = kSize * 0.5f;
  const float qx = fabsf(px - h) - (h - kCorner);
  const float qy = fabsf(py - h) - (h - kCorner);
  const float ox = qx > 0.0f ? qx : 0.0f, oy = qy > 0.0f ? qy : 0.0f;
  const float in = qx > qy ? qx : qy;
  const float d = sqrtf(ox * ox + oy * oy) + (in < 0.0f ? in : 0.0f) - kCorner;
  float t = -d / kFade;
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  return t * t * (3.0f - 2.0f * t);
}

// Makes the picture of `control` lit on the controller of `kind` at the side
// `left`: kSize by kSize px, BGRA, color multiplied by alpha, transparent
// where the drawing has nothing. out has kSize * kSize * 4 bytes. False, and
// a transparent picture, when the drawing does not show the control.
inline bool Bake(int kind, bool left, int control, const Look& look,
                 uint8_t* out) {
  for (int i = 0; i < kSize * kSize * 4; ++i) out[i] = 0;
  if (control < 0 || control >= art::kControls ||
      !art::Shows(kind, left, control)) {
    return false;
  }
  const art::Art a = art::ArtOf(kind);
  // A stick click is the stick: one shape, drawn one time.
  const int lit = control == art::kStickClick ? art::kStick : control;
  float cx = 0.0f, cy = 0.0f;
  Window(kind, lit, &cx, &cy);
  const float s = Scale();
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      uint8_t* p = out + (y * kSize + x) * 4;
      float ux, uy;
      ToUnits(left, cx, cy, x + 0.5f, y + 0.5f, &ux, &uy);
      float body = 1e9f;
      for (int n = 0; n < a.bodies; ++n) {
        const float d = art::Distance(a.body[n], ux, uy);
        if (d < body) body = d;
      }
      body *= s;
      if (look.wash_a > 0.0f) Over(p, look.wash, look.wash_a * Cover(body));
      Over(p, look.ink, Cover(fabsf(body) - kBodyStroke * 0.5f));
      for (int c = 0; c < art::kControls; ++c) {
        if (c == art::kStickClick || c == lit || !art::Shows(kind, left, c)) {
          continue;
        }
        const float d = art::Distance(a.control[c], ux, uy) * s;
        Over(p, look.ink, look.dim * Cover(fabsf(d) - kControlStroke * 0.5f));
        if (c == art::kStick) {
          art::Shape inner = a.control[c];
          inner.r *= 0.5f;
          const float e = art::Distance(inner, ux, uy) * s;
          Over(p, look.ink, look.dim * 0.7f * Cover(fabsf(e) - 0.8f));
        }
      }
      // The lit control: a soft halo, the fill and its edge.
      const float d = art::Distance(a.control[lit], ux, uy) * s;
      if (d > 0.0f && d < kHalo) {
        const float t = 1.0f - d / kHalo;
        Over(p, look.lit, 0.45f * t * t);
      }
      Over(p, look.lit, 0.95f * Cover(d));
      Over(p, look.edge, Cover(fabsf(d) - kLitStroke * 0.5f));
      if (lit == art::kStick) {
        art::Shape inner = a.control[lit];
        inner.r *= 0.5f;
        const float e = art::Distance(inner, ux, uy) * s;
        Over(p, look.edge, 0.8f * Cover(fabsf(e) - 0.8f));
      }
      const float f = EdgeFade(x + 0.5f, y + 0.5f);
      if (f < 1.0f) {
        for (int c = 0; c < 4; ++c) {
          p[c] = static_cast<uint8_t>(p[c] * f + 0.5f);
        }
      }
    }
  }
  return true;
}

// The picture over a picture of a card at x, y: both BGRA, color multiplied
// by alpha. pitch is in pixels. Only the part in width by height draws.
inline void Put(const uint8_t* picture, uint8_t* to, int pitch, int width,
                int height, int x, int y) {
  for (int j = 0; j < kSize; ++j) {
    const int ty = y + j;
    if (ty < 0 || ty >= height) continue;
    for (int i = 0; i < kSize; ++i) {
      const int tx = x + i;
      if (tx < 0 || tx >= width) continue;
      const uint8_t* in = picture + (j * kSize + i) * 4;
      if (in[3] == 0) continue;
      uint8_t* o = to + (ty * pitch + tx) * 4;
      const int k = 255 - in[3];
      for (int c = 0; c < 4; ++c) {
        o[c] = static_cast<uint8_t>(in[c] + (o[c] * k + 127) / 255);
      }
    }
  }
}

}  // namespace hint_picture
