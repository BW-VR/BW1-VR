// The pictures of the buttons on the hint cards (hint_card_pictures=1): a
// small drawing of the controller with one control lit. See hint_card.cpp
// and docs/controller-hints.md, "The pictures of the buttons". No Windows and
// no game here, so the math test compiles this file (hint_picture_checks.h).
//
// TWO VIEWS. The face buttons, the stick and the menu button are on the top
// of a controller, so their picture is a part of the drawing of the page
// Controls (controller_art_math.h), seen from above. The trigger and the grip
// are under the controller and on its handle, so their picture is a drawing
// of the side of the controller (SideOf). Both are our own art in code: a few
// circles, capsules and rounded boxes, with no outline of a product.
//
// THE WINDOW of a top view. A full controller in the height of a row of the
// card is too small to read in a headset: a face button would be 4 px of the
// view. So the picture shows a square window of kWindow units of the drawing
// around the lit control. The window holds the whole part of the body that
// has the control when that part fits (the ring of the face buttons of a
// Touch controller), else it has the control in its middle. All the windows
// of a top view have the same scale. A side view shows the whole controller.

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
// The side of the window of a top view in units of the drawing, the margin
// around the part of the body that the window holds, and the most that the
// window goes past the box of the drawing.
const float kWindow = 140.0f;
const float kHold = 3.0f;
const float kPast = 16.0f;
// A side view is drawn in a box of 120 units. Its window holds the whole
// controller, with kSideMargin units around it.
const float kSideMargin = 7.0f;

// The halo and the edge of the lit control, in px of the picture.
const float kLitStroke = 1.8f;
const float kHalo = 5.0f;
// The edge of the window fades out over kFade px, with round corners of
// kCorner px, so a cut outline does not end in a hard line.
const float kFade = 5.0f;
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

// --- the views -------------------------------------------------------------------

// The art of one picture, in units, as the right controller. lit is the
// shape of the lit control. The window has its middle at cx, cy and a side
// of `window` units.
const int kMostControls = 8;
struct View {
  int bodies;
  art::Shape body[3];
  int controls;  // the other controls, drawn dim
  art::Shape control[kMostControls];
  bool control_stick[kMostControls];  // a stick: a ring in it
  art::Shape lit;
  bool lit_stick;
  float cx, cy, window;
};

// True when the picture of a control is a side view: the trigger and the
// grip of a controller with a handle. The select button of a simple
// controller is on its top.
inline bool SideView(int kind, int control) {
  return (control == art::kTrigger || control == art::kGrip) &&
         kind != button_map::kSimple;
}

// The side of a controller, with the front at the right and the top up, as
// the right controller, in a box of 120 units. The body is the head and
// the handle. The trigger is under the front of the head, and the grip is on
// the handle. `stick` is the stick or the pad on the top, drawn dim.
inline void SideOf(int kind, View* v, art::Shape* trigger, art::Shape* grip,
                   art::Shape* stick) {
  using art::Shape;
  switch (kind) {
    case button_map::kIndex:
      // One long bar, with the trigger at the front of its top.
      v->bodies = 1;
      v->body[0] = Shape{art::kCapsule, 64, 24, 50, 98, 13};
      *stick = Shape{art::kCapsule, 58, 12, 58, 8, 4};
      *trigger = Shape{art::kCapsule, 82, 34, 80, 46, 6};
      *grip = Shape{art::kCapsule, 67, 58, 61, 88, 6};
      break;
    case button_map::kWand:
      // The ring on the top, seen from its edge, and a long handle. The grip
      // is a button on the side of the handle.
      v->bodies = 2;
      v->body[0] = Shape{art::kCapsule, 26, 24, 92, 20, 8};
      v->body[1] = Shape{art::kCapsule, 60, 32, 58, 98, 12};
      *stick = Shape{art::kCapsule, 56, 31, 64, 31, 3};
      *trigger = Shape{art::kCapsule, 74, 42, 72, 54, 6};
      *grip = Shape{art::kCapsule, 58, 68, 58, 80, 6};
      break;
    case button_map::kMixed:
      v->bodies = 2;
      v->body[0] = Shape{art::kCapsule, 24, 22, 90, 28, 8};
      v->body[1] = Shape{art::kCapsule, 58, 34, 54, 100, 13};
      *stick = Shape{art::kCircle, 50, 34, 0, 0, 4};
      *trigger = Shape{art::kCapsule, 76, 42, 74, 54, 6};
      *grip = Shape{art::kCapsule, 54, 62, 52, 86, 6};
      break;
    default:
      // Touch: a flat head, the stick on its top, and the handle down and to
      // the back. The grip is on the front of the handle.
      v->bodies = 2;
      v->body[0] = Shape{art::kBox, 58, 34, 36, 13, 12};
      v->body[1] = Shape{art::kCapsule, 50, 44, 40, 98, 14};
      *stick = Shape{art::kCapsule, 42, 20, 42, 13, 5};
      *trigger = Shape{art::kCapsule, 80, 50, 76, 62, 7};
      *grip = Shape{art::kCapsule, 57, 64, 51, 90, 7};
      break;
  }
}

// The middle of the window of a top view on one axis: lo and hi are the box
// of the part of the body that has the control, c0 and c1 the box of the
// control, and size the length of the box of the drawing.
inline float WindowAxis(float lo, float hi, float c0, float c1, float size) {
  const float half = kWindow * 0.5f;
  const float m = (c0 + c1) * 0.5f;
  lo = (lo < c0 ? lo : c0) - kHold;
  hi = (hi > c1 ? hi : c1) + kHold;
  if (hi - lo <= kWindow) {
    // The whole part fits: the window holds it, as near to the middle of
    // the control as it can be.
    const float a = hi - half, b = lo + half;
    return m < a ? a : (m > b ? b : m);
  }
  const float a = half - kPast, b = size - half + kPast;
  return m < a ? a : (m > b ? b : m);
}

// The middle of the window of a control in a top view, in units of the frame
// of the right controller. The box is the same for the mirror, so the left
// controller has the same window in its frame.
inline void Window(int kind, int control, float* cx, float* cy) {
  const art::Art a = art::ArtOf(kind);
  float c[4];
  art::Bounds(a.control[control], c);
  // The part of the body that has the control: the nearest body to its
  // middle.
  const float mx = (c[0] + c[2]) * 0.5f, my = (c[1] + c[3]) * 0.5f;
  int part = 0;
  float best = 1e9f;
  for (int n = 0; n < a.bodies; ++n) {
    const float d = art::Distance(a.body[n], mx, my);
    if (d < best) {
      best = d;
      part = n;
    }
  }
  float b[4];
  art::Bounds(a.body[part], b);
  *cx = WindowAxis(b[0], b[2], c[0], c[2], art::kArtWidth);
  *cy = WindowAxis(b[1], b[3], c[1], c[3], art::kArtHeight);
}

// The view of a control lit on the controller of `kind` at the side `left`.
// False when the drawing does not show the control.
inline bool ViewOf(int kind, bool left, int control, View* v) {
  *v = View{};
  if (control < 0 || control >= art::kControls ||
      !art::Shows(kind, left, control)) {
    return false;
  }
  if (SideView(kind, control)) {
    art::Shape trigger, grip, stick;
    SideOf(kind, v, &trigger, &grip, &stick);
    const bool is_trigger = control == art::kTrigger;
    v->lit = is_trigger ? trigger : grip;
    v->control[v->controls++] = is_trigger ? grip : trigger;
    v->control[v->controls++] = stick;
    // The window: the box of all the shapes, square, with the margin.
    float box[4] = {1e9f, 1e9f, -1e9f, -1e9f};
    const auto grow = [&](const art::Shape& shape) {
      float b[4];
      art::Bounds(shape, b);
      for (int k = 0; k < 2; ++k) {
        if (b[k] < box[k]) box[k] = b[k];
        if (b[k + 2] > box[k + 2]) box[k + 2] = b[k + 2];
      }
    };
    for (int n = 0; n < v->bodies; ++n) grow(v->body[n]);
    grow(trigger);
    grow(grip);
    grow(stick);
    const float w = box[2] - box[0], h = box[3] - box[1];
    v->cx = (box[0] + box[2]) * 0.5f;
    v->cy = (box[1] + box[3]) * 0.5f;
    v->window = (w > h ? w : h) + 2.0f * kSideMargin;
    return true;
  }
  const art::Art a = art::ArtOf(kind);
  v->bodies = a.bodies;
  for (int n = 0; n < a.bodies; ++n) v->body[n] = a.body[n];
  // A stick click is the stick: one shape, drawn one time.
  const int lit = control == art::kStickClick ? art::kStick : control;
  for (int c = 0; c < art::kControls; ++c) {
    if (c == art::kStickClick || c == lit || !art::Shows(kind, left, c)) {
      continue;
    }
    v->control_stick[v->controls] = c == art::kStick;
    v->control[v->controls++] = a.control[c];
  }
  v->lit = a.control[lit];
  v->lit_stick = lit == art::kStick;
  Window(kind, lit, &v->cx, &v->cy);
  v->window = kWindow;
  return true;
}

// A pixel of the picture in units of the frame of the controller, and back.
// The left controller is the mirror of the frame (controller_art_math.h).
inline void ToUnits(const View& v, bool left, float px, float py, float* ux,
                    float* uy) {
  const float s = kSize / v.window;
  const float dx = (px - kSize * 0.5f) / s;
  *ux = left ? v.cx - dx : v.cx + dx;
  *uy = v.cy + (py - kSize * 0.5f) / s;
}
inline void ToPixel(const View& v, bool left, float ux, float uy, float* px,
                    float* py) {
  const float s = kSize / v.window;
  const float dx = left ? v.cx - ux : ux - v.cx;
  *px = dx * s + kSize * 0.5f;
  *py = (uy - v.cy) * s + kSize * 0.5f;
}

// --- the picture -----------------------------------------------------------------

struct Paint {
  float r, g, b;
};

// The colors and the strokes of one look of the card. ink: the outlines.
// fill: the inside of the controller, at fill_a, so its shape reads on the
// card. lit: the fill of the lit control and its halo. edge: the outline of
// the lit control. dim: the strength of the outlines of the other controls.
// The strokes are in px of the picture.
struct Look {
  Paint ink;
  Paint fill;
  float fill_a;
  Paint lit;
  Paint edge;
  float dim;
  float body_stroke;
  float control_stroke;
};

// The look of a picture on a card: the scroll (hint_card_style=1) or the
// plain card. The tone is that of the line (hint_card.cpp): 1 the trigger, 2
// the grip, 0 and 3 any other button. The lit control has the color of the
// wax of the seal (card_scroll.cpp) or of the badge (hint_card.cpp). On the
// scroll the ink is darker than the ink of the words and the body has a pale
// fill, so the shape reads on the paper.
inline Look LookOf(bool scroll, int tone) {
  static const Paint kWax[4] = {{160.0f, 30.0f, 26.0f},
                                {36.0f, 124.0f, 60.0f},
                                {204.0f, 128.0f, 22.0f},
                                {160.0f, 30.0f, 26.0f}};
  static const Paint kBadge[4] = {{232.0f, 236.0f, 242.0f},
                                  {90.0f, 235.0f, 150.0f},
                                  {255.0f, 196.0f, 60.0f},
                                  {232.0f, 236.0f, 242.0f}};
  Look look;
  if (scroll) {
    look.ink = {30.0f, 16.0f, 6.0f};
    look.fill = {252.0f, 244.0f, 226.0f};
    look.fill_a = 0.92f;
    look.lit = kWax[tone & 3];
    look.edge = look.ink;
    look.dim = 0.85f;
    look.body_stroke = 3.4f;
    look.control_stroke = 2.6f;
  } else {
    look.ink = {236.0f, 236.0f, 236.0f};
    look.fill = {62.0f, 70.0f, 86.0f};
    look.fill_a = 0.9f;
    look.lit = kBadge[tone & 3];
    look.edge = {16.0f, 18.0f, 22.0f};
    look.dim = 0.75f;
    look.body_stroke = 3.0f;
    look.control_stroke = 2.2f;
  }
  return look;
}

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

// The cover of the ring in a stick (at half its radius) at a point, with a
// scale of s px a unit.
inline float StickRing(const art::Shape& stick, float ux, float uy, float s) {
  art::Shape inner = stick;
  inner.r *= 0.5f;
  return Cover(fabsf(art::Distance(inner, ux, uy) * s) - 0.9f);
}

// Makes the picture of `control` lit on the controller of `kind` at the side
// `left`: kSize by kSize px, BGRA, color multiplied by alpha, transparent
// where the drawing has nothing. out has kSize * kSize * 4 bytes. False, and
// a transparent picture, when the drawing does not show the control.
inline bool Bake(int kind, bool left, int control, const Look& look,
                 uint8_t* out) {
  for (int i = 0; i < kSize * kSize * 4; ++i) out[i] = 0;
  View v;
  if (!ViewOf(kind, left, control, &v)) return false;
  const float s = kSize / v.window;
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      uint8_t* p = out + (y * kSize + x) * 4;
      float ux, uy;
      ToUnits(v, left, x + 0.5f, y + 0.5f, &ux, &uy);
      float body = 1e9f;
      for (int n = 0; n < v.bodies; ++n) {
        const float d = art::Distance(v.body[n], ux, uy);
        if (d < body) body = d;
      }
      body *= s;
      Over(p, look.fill, look.fill_a * Cover(body));
      Over(p, look.ink, Cover(fabsf(body) - look.body_stroke * 0.5f));
      for (int c = 0; c < v.controls; ++c) {
        const float d = art::Distance(v.control[c], ux, uy) * s;
        Over(p, look.ink,
             look.dim * Cover(fabsf(d) - look.control_stroke * 0.5f));
        if (v.control_stick[c]) {
          Over(p, look.ink, look.dim * 0.7f * StickRing(v.control[c], ux, uy, s));
        }
      }
      // The lit control: a soft halo, the fill and its edge.
      const float d = art::Distance(v.lit, ux, uy) * s;
      if (d > 0.0f && d < kHalo) {
        const float t = 1.0f - d / kHalo;
        Over(p, look.lit, 0.45f * t * t);
      }
      Over(p, look.lit, 0.95f * Cover(d));
      Over(p, look.edge, Cover(fabsf(d) - kLitStroke * 0.5f));
      if (v.lit_stick) {
        Over(p, look.edge, 0.8f * StickRing(v.lit, ux, uy, s));
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
