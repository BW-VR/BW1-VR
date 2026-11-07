// The math of the controller hints (hint_style=2), as pure functions with no
// game state. See docs/controller-hints.md, "The controller in the hand".
//
// hint_model.cpp calls these with the poses of the frame, and
// test/controller_math calls them with numbers it makes up.
//
// THE MODELS are the WebXR Input Profiles (bwvr/assets/controllers). Each
// controller is one glTF file for each hand, in the grip space of the hand:
// meters, right handed, y up and -z ahead, as the grip pose of OpenXR. Each
// control has a node "<base>_pressed_value" that holds the meshes of that
// control and moves when the player presses it, between the nodes
// "<base>_pressed_min" and "<base>_pressed_max". A stick has the same three
// nodes for "<base>_xaxis_pressed" and "<base>_yaxis_pressed" inside.
//
// THE SPACE of the cards is the LOCAL space of OpenXR, as in
// hint_card_math.h.

#pragma once

#include <ctype.h>
#include <math.h>
#include <string.h>
#include <wchar.h>

#include "button_map_math.h"
#include "controller_art_math.h"
#include "controller_math.h"
#include "hint_card_math.h"

namespace hint_model {

// --- the model of a controller ---------------------------------------------------

// True when `text` has `part`, with no regard to the case of the letters.
inline bool HasNoCase(const char* text, const char* part) {
  if (text == nullptr || part == nullptr) return false;
  const size_t n = strlen(part);
  for (const char* p = text; *p != 0; ++p) {
    size_t i = 0;
    while (i < n && p[i] != 0 &&
           tolower(static_cast<unsigned char>(p[i])) ==
               tolower(static_cast<unsigned char>(part[i]))) {
      ++i;
    }
    if (i == n) return true;
  }
  return false;
}

// The folder of the model (a profile id of the WebXR Input Profiles) for the
// interaction profile that the runtime reports and the name of the headset,
// or null when no model fits. Then the cards keep the look of hint_style=1.
//
// A runtime can give one profile for several controllers. Virtual Desktop
// gives "oculus/touch_controller" for each Quest, so the name of the headset
// tells the Touch controllers apart: "Meta Quest 3" and "Meta Quest 3S" have
// the Touch Plus, a Quest Pro has the Touch Pro, a Quest 2 has the Touch of
// the third version, and a Quest of the first version and a Rift S have the
// second version. A Pico headset can give the Touch profile too.
inline const char* ModelOfProfile(const char* profile, const char* system) {
  if (profile == nullptr || profile[0] == 0) return nullptr;
  if (HasNoCase(system, "pico") || HasNoCase(profile, "pico")) {
    return "pico-4";
  }
  if (HasNoCase(profile, "touch_controller_pro") ||
      HasNoCase(profile, "touch_pro")) {
    return "meta-quest-touch-pro";
  }
  if (HasNoCase(profile, "touch_controller_plus") ||
      HasNoCase(profile, "touch_plus")) {
    return "meta-quest-touch-plus";
  }
  if (HasNoCase(profile, "oculus/touch_controller") ||
      strcmp(profile, "fake") == 0) {
    if (HasNoCase(system, "pro")) return "meta-quest-touch-pro";
    if (HasNoCase(system, "quest 2") || HasNoCase(system, "quest2")) {
      return "oculus-touch-v3";
    }
    if (HasNoCase(system, "rift s")) return "oculus-touch-v2";
    if (HasNoCase(system, "quest") && !HasNoCase(system, "quest 3") &&
        !HasNoCase(system, "quest3")) {
      return "oculus-touch-v2";
    }
    // A Quest 3 or 3S, the fake controller of a desktop run, and a headset
    // with an other name.
    return "meta-quest-touch-plus";
  }
  if (HasNoCase(profile, "valve/index")) return "valve-index";
  if (HasNoCase(profile, "vive_cosmos")) return "htc-vive-cosmos";
  if (HasNoCase(profile, "vive_focus3")) return "htc-vive-focus-3";
  if (HasNoCase(profile, "htc/vive_controller")) return "htc-vive";
  if (HasNoCase(profile, "hp/mixed_reality")) return "hp-mixed-reality";
  // The first Mixed Reality controllers have a touchpad and no A, B, X or Y.
  // The HP model does not fit them, and the mod ships no model of them.
  if (HasNoCase(profile, "microsoft/motion_controller")) {
    if (HasNoCase(system, "hp") || HasNoCase(system, "reverb")) {
      return "hp-mixed-reality";
    }
    return nullptr;
  }
  if (HasNoCase(profile, "khr/simple")) return "generic-trigger";
  return nullptr;
}

// The node names that can hold a control of the drawing of the page
// Controls (controller_art::kFirst to kPad), in the order to try them. Each
// name is a base: the node is base + "_pressed_value". The left Touch
// controllers have X and Y. An Index controller has A and B on both hands.
// The pad of a wand is its stick. Returns the count of names.
const int kMostBases = 2;

inline int NodeBases(int control, bool left, const char* out[kMostBases]) {
  namespace art = controller_art;
  switch (control) {
    case art::kFirst:
      if (left) {
        out[0] = "x_button";
        out[1] = "a_button";
        return 2;
      }
      out[0] = "a_button";
      return 1;
    case art::kSecond:
      if (left) {
        out[0] = "y_button";
        out[1] = "b_button";
        return 2;
      }
      out[0] = "b_button";
      return 1;
    case art::kStickClick:
    case art::kStick:
      out[0] = "xr_standard_thumbstick";
      out[1] = "xr_standard_touchpad";
      return 2;
    case art::kMenu:
      out[0] = "menu";
      return 1;
    case art::kTrigger:
      out[0] = "xr_standard_trigger";
      return 1;
    case art::kGrip:
      out[0] = "xr_standard_squeeze";
      return 1;
    case art::kPad:
      out[0] = "xr_standard_touchpad";
      return 1;
    default:
      return 0;
  }
}

// THE CONTROL OF A BUTTON WORD of a card (the words of
// controller_hints.cpp), as a control of controller_art, or -1. `plain` is
// true when the word is the name of the control only. A word with " x2" or
// " hold" is not plain: the card keeps its seal, so the player sees the
// double press or the hold.
inline int ControlOfWord(int kind, bool left, const wchar_t* word,
                         bool* plain) {
  namespace art = controller_art;
  if (plain != nullptr) *plain = false;
  if (word == nullptr || word[0] == 0) return -1;
  struct Name {
    const wchar_t* name;
    int control;
  };
  Name names[button_map::kButtons + 5];
  int n = 0;
  for (int b = 0; b < button_map::kButtons; ++b) {
    const wchar_t* w = button_map::ButtonWord(kind, left, b);
    if (w != nullptr) names[n++] = {w, b};
  }
  names[n++] = {L"Trigger", art::kTrigger};
  names[n++] = {L"Select", art::kTrigger};
  names[n++] = {L"Grip", art::kGrip};
  names[n++] = {L"Stick", art::kStick};
  names[n++] = {L"Pad", art::kStick};
  // The longest name first, so "Stick click" is not "Stick".
  int best = -1;
  size_t best_length = 0;
  for (int i = 0; i < n; ++i) {
    const size_t length = wcslen(names[i].name);
    if (length <= best_length || wcsncmp(word, names[i].name, length) != 0) {
      continue;
    }
    const wchar_t* rest = word + length;
    if (rest[0] == 0 || wcscmp(rest, L" x2") == 0 ||
        wcscmp(rest, L" hold") == 0) {
      best = i;
      best_length = length;
    }
  }
  if (best < 0) return -1;
  if (plain != nullptr) *plain = word[best_length] == 0;
  return names[best].control;
}

// --- the press of a control -------------------------------------------------------

// A local transform of a node of glTF: translation, rotation x y z w, scale.
struct Trs {
  float t[3];
  float q[4];
  float s[3];
};

// The transform between `a` (w = 0) and `b` (w = 1): the line between the
// two places and the two scales, and the shorter way between the two turns.
inline void LerpTrs(const Trs& a, const Trs& b, float w, Trs* out) {
  if (!(w > 0.0f)) w = 0.0f;
  if (w > 1.0f) w = 1.0f;
  float dot = 0.0f;
  for (int i = 0; i < 4; ++i) dot += a.q[i] * b.q[i];
  const float sign = dot < 0.0f ? -1.0f : 1.0f;
  float length = 0.0f;
  for (int i = 0; i < 4; ++i) {
    out->q[i] = a.q[i] + (sign * b.q[i] - a.q[i]) * w;
    length += out->q[i] * out->q[i];
  }
  length = sqrtf(length);
  if (length > 1e-6f) {
    for (int i = 0; i < 4; ++i) out->q[i] /= length;
  } else {
    for (int i = 0; i < 4; ++i) out->q[i] = a.q[i];
  }
  for (int i = 0; i < 3; ++i) {
    out->t[i] = a.t[i] + (b.t[i] - a.t[i]) * w;
    out->s[i] = a.s[i] + (b.s[i] - a.s[i]) * w;
  }
}

// The weight between the min node and the max node for a value of the
// controller. A button and a trigger give 0 to 1. An axis gives -1 to 1, and
// its weight is (value + 1) / 2, as the WebXR Input Profiles do it. The y
// axis of a gamepad of WebXR is down, so a stick that goes ahead (y = 1 in
// the mod) has the weight 0.
enum Axis { kAxisButton = 0, kAxisX = 1, kAxisY = 2 };

inline float ResponseWeight(int axis, float value) {
  float w = value;
  if (axis == kAxisX) w = 0.5f * (value + 1.0f);
  if (axis == kAxisY) w = 0.5f * (1.0f - value);
  if (!(w > 0.0f)) return 0.0f;
  return w > 1.0f ? 1.0f : w;
}

// A 3 by 4 matrix: columns x, y, z and the place, each of 3 floats.
struct Matrix {
  float m[12];
};

inline void MatrixOfTrs(const Trs& trs, Matrix* out) {
  const float x = trs.q[0], y = trs.q[1], z = trs.q[2], w = trs.q[3];
  const float r[9] = {1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y + z * w),
                      2.0f * (x * z - y * w),        2.0f * (x * y - z * w),
                      1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z + x * w),
                      2.0f * (x * z + y * w),        2.0f * (y * z - x * w),
                      1.0f - 2.0f * (x * x + y * y)};
  for (int c = 0; c < 3; ++c) {
    for (int k = 0; k < 3; ++k) out->m[c * 3 + k] = r[c * 3 + k] * trs.s[c];
  }
  for (int k = 0; k < 3; ++k) out->m[9 + k] = trs.t[k];
}

// out = a * b.
inline void MatrixMultiply(const Matrix& a, const Matrix& b, Matrix* out) {
  Matrix r;
  for (int c = 0; c < 4; ++c) {
    for (int k = 0; k < 3; ++k) {
      float v = a.m[k] * b.m[c * 3 + 0] + a.m[3 + k] * b.m[c * 3 + 1] +
                a.m[6 + k] * b.m[c * 3 + 2];
      if (c == 3) v += a.m[9 + k];
      r.m[c * 3 + k] = v;
    }
  }
  *out = r;
}

inline void MatrixPoint(const Matrix& a, const float p[3], float out[3]) {
  for (int k = 0; k < 3; ++k) {
    out[k] = a.m[k] * p[0] + a.m[3 + k] * p[1] + a.m[6 + k] * p[2] +
             a.m[9 + k];
  }
}

// --- the cards of the buttons -------------------------------------------------------

// HL:A STYLE. Each lit button has its own small card beside the controller,
// with a thin line of ink from the card to the button. The cards of one hand
// are a column on the outer side of the hand, as the head sees it, at the
// distance of the controller from the head. The inner edges of the cards are
// on one line, `gap` past the outline of the hand and of the controller. A
// card with no button (a hint of the game with no button, or a button that
// the model does not show) is the top card, with no line.
//
// THE ORDER. The lines go from the buttons to the middle of the inner edge of
// each card. The column is outside the outline, so a line does not go over a
// card. Two lines can cross when the cards are in the wrong order, so Stack
// tries each order of the cards with a button and takes the order with the
// fewest crossings in the view, and then the one with the least vertical
// distance from each button to its card.
const int kMaxCards = 4;

struct StackIn {
  float head[3];
  // The middle of the controller, and the points of its outline (the corners
  // of its box and the outline of the hand, hint_card_math::HandOutline).
  float center[3];
  const float (*outline)[3];
  int outline_count;
  // The side of the last frame (-1, 1, or 0 for none yet), the side to start
  // on, and right in the view in the last frame.
  int side;
  int first;
  float last_right[3];
  float gap;         // from the outline to the inner edge of the cards
  float spacing;     // between two cards
  // Right and up of the head, unit vectors. All zero: a head with no roll
  // that looks at the controller.
  float head_right[3];
  float head_up[3];
  int count;
  float width[kMaxCards];
  float height[kMaxCards];
  bool has_anchor[kMaxCards];
  float anchor[kMaxCards][3];  // the button of each card
  // The order of the last frame, from the top, when the cards are the same
  // (keep_count is their count, else 0). Stack tries it first, and searches
  // all orders only when it has a crossing.
  int keep_count;
  int keep_column[kMaxCards];
};

struct StackOut {
  int side;          // -1 left of the controller in the view, 1 right
  bool changed;      // the side changed on this call
  float right[3];    // right in the view
  float up[3];       // up in the view
  float extent;      // how far the outline goes to the side of the cards
  // The turn of each card: square to the line of sight from the head to its
  // own middle, with no roll.
  float orientation[kMaxCards][4];
  float center[kMaxCards][3];
  float edge[kMaxCards][3];   // the middle of the inner edge of each card
  int slot[kMaxCards];        // the place of each card from the top, 0 first
  int crossings;              // of the lines in the view, 0 when it can be
};

// A point in the view: its offset from `center` along r and u, at the depth
// of the center. A nearer point looks farther from the center.
inline void ViewPoint(const float p[3], const float head[3],
                      const float center[3], const float v[3],
                      const float r[3], const float u[3], float out[2]) {
  const float d_center = (center[0] - head[0]) * v[0] +
                         (center[1] - head[1]) * v[1] +
                         (center[2] - head[2]) * v[2];
  float d = (p[0] - head[0]) * v[0] + (p[1] - head[1]) * v[1] +
            (p[2] - head[2]) * v[2];
  if (d < 0.05f) d = 0.05f;
  const float scale = d_center / d;
  const float q[3] = {p[0] - head[0], p[1] - head[1], p[2] - head[2]};
  const float c[3] = {center[0] - head[0], center[1] - head[1],
                      center[2] - head[2]};
  out[0] = (q[0] * r[0] + q[1] * r[1] + q[2] * r[2]) * scale -
           (c[0] * r[0] + c[1] * r[1] + c[2] * r[2]);
  out[1] = (q[0] * u[0] + q[1] * u[1] + q[2] * u[2]) * scale -
           (c[0] * u[0] + c[1] * u[1] + c[2] * u[2]);
}

// True when the segments a0-a1 and b0-b1 cross in the plane. Segments that
// only touch at an end do not cross.
inline bool SegmentsCross(const float a0[2], const float a1[2],
                          const float b0[2], const float b1[2]) {
  auto turn = [](const float p[2], const float q[2], const float r[2]) {
    return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0]);
  };
  const float d1 = turn(b0, b1, a0), d2 = turn(b0, b1, a1);
  const float d3 = turn(a0, a1, b0), d4 = turn(a0, a1, b1);
  return ((d1 > 0.0f && d2 < 0.0f) || (d1 < 0.0f && d2 > 0.0f)) &&
         ((d3 > 0.0f && d4 < 0.0f) || (d3 < 0.0f && d4 > 0.0f));
}

// One column of cards from the top, in the order `column`. Each card faces
// the head from its own middle, so a card above or below the controller is
// tilted to the head, and in the view it is taller than in the plane of the
// controller. So each card goes under the card above it as the head sees
// them: its top corners are `spacing` under the bottom corners of the card
// above, in the view. The column has its middle at the height of the middle
// of the controller in the view, so it stays beside the controller. The
// cards with no button are at its top. `x_inner` is the inner edge of the
// column in the plane of the controller.
struct ColumnFrame {
  const float* head;
  const float* center;
  const float* v;
  const float* r;
  const float* u;
  const float* head_right;
  float side;
  float x_inner;
  // The line of the inner edges: through `origin`, along `dir`. dir is the
  // up of the head made square to the line of sight to origin, so the line
  // is vertical in the view.
  float origin[3];
  float dir[3];
};

// The turn of a quad whose axes are x, y and z (unit vectors, right handed).
inline void QuatFromAxes(const float x[3], const float y[3], const float z[3],
                         float q[4]) {
  const float trace = x[0] + y[1] + z[2];
  if (trace > 0.0f) {
    const float s = sqrtf(trace + 1.0f) * 2.0f;
    q[3] = 0.25f * s;
    q[0] = (y[2] - z[1]) / s;
    q[1] = (z[0] - x[2]) / s;
    q[2] = (x[1] - y[0]) / s;
  } else if (x[0] > y[1] && x[0] > z[2]) {
    const float s = sqrtf(1.0f + x[0] - y[1] - z[2]) * 2.0f;
    q[3] = (y[2] - z[1]) / s;
    q[0] = 0.25f * s;
    q[1] = (y[0] + x[1]) / s;
    q[2] = (z[0] + x[2]) / s;
  } else if (y[1] > z[2]) {
    const float s = sqrtf(1.0f + y[1] - x[0] - z[2]) * 2.0f;
    q[3] = (z[0] - x[2]) / s;
    q[0] = (y[0] + x[1]) / s;
    q[1] = 0.25f * s;
    q[2] = (z[1] + y[2]) / s;
  } else {
    const float s = sqrtf(1.0f + z[2] - x[0] - y[1]) * 2.0f;
    q[3] = (x[1] - y[0]) / s;
    q[0] = (z[0] + x[2]) / s;
    q[1] = (z[1] + y[2]) / s;
    q[2] = 0.25f * s;
  }
}

// A card that faces the head from `center` and is level in the view: its
// front (+z) goes to the head, and its width (+x) is in the plane of the
// line of sight and the right of the head, so the head sees it with no roll.
inline void LevelFacePose(const float center[3], const float head[3],
                          const float head_right[3], float q[4]) {
  float z[3] = {head[0] - center[0], head[1] - center[1], head[2] - center[2]};
  overlay_math::Normalize(z);
  const float along = head_right[0] * z[0] + head_right[1] * z[1] +
                      head_right[2] * z[2];
  float x[3] = {head_right[0] - along * z[0], head_right[1] - along * z[1],
                head_right[2] - along * z[2]};
  overlay_math::Normalize(x);
  const float y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2],
                      z[0] * x[1] - z[1] * x[0]};
  QuatFromAxes(x, y, z, q);
}

inline void PlaceCard(const StackIn& in, const ColumnFrame& f, int i,
                      float y, StackOut* out, float* top, float* bottom) {
  // The inner edge of the card is on the line of the column, and the card
  // goes out from it along its own width. Its width depends on its turn, and
  // its turn on its place, so two steps.
  float e[3];
  for (int k = 0; k < 3; ++k) e[k] = f.origin[k] + f.dir[k] * y;
  const float half = 0.5f * in.width[i] * f.side;
  const float x_axis_first[3] = {1.0f, 0.0f, 0.0f};
  LevelFacePose(e, f.head, f.head_right, out->orientation[i]);
  for (int step = 0; step < 2; ++step) {
    float cx[3];
    controller_math::QuatRotate(out->orientation[i], x_axis_first, cx);
    for (int k = 0; k < 3; ++k) out->center[i][k] = e[k] + cx[k] * half;
    LevelFacePose(out->center[i], f.head, f.head_right, out->orientation[i]);
  }
  const float x_axis[3] = {1.0f, 0.0f, 0.0f};
  const float y_axis[3] = {0.0f, 1.0f, 0.0f};
  float card_right[3], card_up[3];
  controller_math::QuatRotate(out->orientation[i], x_axis, card_right);
  controller_math::QuatRotate(out->orientation[i], y_axis, card_up);
  *top = -1e9f;
  *bottom = 1e9f;
  for (int c = 0; c < 4; ++c) {
    const float sx = (c & 1) ? 0.5f : -0.5f;
    const float sy = (c & 2) ? 0.5f : -0.5f;
    float corner[3];
    for (int k = 0; k < 3; ++k) {
      corner[k] = out->center[i][k] + card_right[k] * sx * in.width[i] +
                  card_up[k] * sy * in.height[i];
    }
    float view[2];
    ViewPoint(corner, f.head, f.center, f.v, f.r, f.u, view);
    if (sy > 0.0f && view[1] > *top) *top = view[1];
    if (sy < 0.0f && view[1] < *bottom) *bottom = view[1];
  }
  for (int k = 0; k < 3; ++k) {
    out->edge[i][k] =
        out->center[i][k] - f.side * card_right[k] * 0.5f * in.width[i];
  }
}

// `passes` and `steps` set the precision: the tilt of a card changes with
// its height, so its place takes some steps, and the middle of the column
// some passes. The search of the order uses few, and the order it takes
// uses more.
inline void PlaceColumn(const StackIn& in, const ColumnFrame& f,
                        const int column[], int passes, int steps,
                        StackOut* out) {
  float start = 0.0f;
  for (int pass = 0; pass < passes; ++pass) {
    float cursor = start;
    float block_top = 0.0f, block_bottom = 0.0f;
    for (int s = 0; s < in.count; ++s) {
      const int i = column[s];
      // The middle for a top at the cursor. The top in the view does not
      // move one to one with the middle (the tilt changes with the height),
      // so each step is a secant step.
      float y = cursor - 0.5f * in.height[i];
      float top = 0.0f, bottom = 0.0f;
      PlaceCard(in, f, i, y, out, &top, &bottom);
      float y_last = y, top_last = top;
      y += cursor - top;
      for (int step = 1; step < steps; ++step) {
        PlaceCard(in, f, i, y, out, &top, &bottom);
        const float slope = fabsf(y - y_last) > 1e-7f
                                ? (top - top_last) / (y - y_last)
                                : 1.0f;
        y_last = y;
        top_last = top;
        y += (cursor - top) / (slope > 0.2f ? slope : 0.2f);
      }
      PlaceCard(in, f, i, y, out, &top, &bottom);
      if (s == 0) block_top = top;
      block_bottom = bottom;
      cursor = bottom - in.spacing;
    }
    if (pass + 1 < passes) start -= 0.5f * (block_top + block_bottom);
  }
}

inline bool Stack(const StackIn& in, StackOut* out) {
  if (in.count <= 0 || in.count > kMaxCards) return false;
  // THE FRAME OF THE COLUMN. v is the line of sight to the middle of the
  // controller. r and u are the right and the up of the head, made square
  // to v. A line along u through the controller is vertical in the view of
  // the head, and a line along r is level, so the column is a column as the
  // head sees it.
  float v[3] = {in.center[0] - in.head[0], in.center[1] - in.head[1],
                in.center[2] - in.head[2]};
  const float distance = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (!(distance > 1e-4f)) return false;
  for (int k = 0; k < 3; ++k) v[k] /= distance;
  float head_right[3] = {in.head_right[0], in.head_right[1], in.head_right[2]};
  float head_up[3] = {in.head_up[0], in.head_up[1], in.head_up[2]};
  if (head_right[0] * head_right[0] + head_right[1] * head_right[1] +
          head_right[2] * head_right[2] < 0.5f) {
    // No head given: level, and to the right of the line of sight.
    head_right[0] = -v[2];
    head_right[1] = 0.0f;
    head_right[2] = v[0];
    overlay_math::Normalize(head_right);
    head_up[0] = 0.0f;
    head_up[1] = 1.0f;
    head_up[2] = 0.0f;
  }
  float r[3], u[3];
  {
    const float a = head_right[0] * v[0] + head_right[1] * v[1] +
                    head_right[2] * v[2];
    const float b = head_up[0] * v[0] + head_up[1] * v[1] + head_up[2] * v[2];
    for (int k = 0; k < 3; ++k) {
      r[k] = head_right[k] - a * v[k];
      u[k] = head_up[k] - b * v[k];
    }
  }
  if (r[0] * r[0] + r[1] * r[1] + r[2] * r[2] < 0.01f ||
      u[0] * u[0] + u[1] * u[1] + u[2] * u[2] < 0.01f) {
    return false;
  }
  overlay_math::Normalize(r);
  overlay_math::Normalize(u);
  // The side: the outer side of the hand, and it does not change.
  const int side = in.side == -1 || in.side == 1 ? in.side
                   : in.first < 0                 ? -1
                                                  : 1;
  // THE INNER EDGE, from the outline as the head sees it: x of a point in
  // the view is its offset along the right of the head over its depth along
  // the forward of the head. The edge is `gap` (at the depth of the
  // controller) past the outline point farthest to the side of the cards.
  const float fwd[3] = {head_up[1] * head_right[2] - head_up[2] * head_right[1],
                        head_up[2] * head_right[0] - head_up[0] * head_right[2],
                        head_up[0] * head_right[1] - head_up[1] * head_right[0]};
  auto view_x = [&](const float p[3]) {
    const float d[3] = {p[0] - in.head[0], p[1] - in.head[1],
                        p[2] - in.head[2]};
    float depth = d[0] * fwd[0] + d[1] * fwd[1] + d[2] * fwd[2];
    if (depth < 0.05f) depth = 0.05f;
    return (d[0] * head_right[0] + d[1] * head_right[1] +
            d[2] * head_right[2]) / depth;
  };
  const float c_depth = distance * (v[0] * fwd[0] + v[1] * fwd[1] +
                                    v[2] * fwd[2]);
  if (!(c_depth > 0.05f)) return false;
  float most = side * view_x(in.center);
  for (int p = 0; p < in.outline_count; ++p) {
    const float x = side * view_x(in.outline[p]);
    if (x > most) most = x;
  }
  const float target = side * (most + in.gap / c_depth);
  // The offset along r that puts the point center + offset * r at `target`.
  const float ca = (in.center[0] - in.head[0]) * head_right[0] +
                   (in.center[1] - in.head[1]) * head_right[1] +
                   (in.center[2] - in.head[2]) * head_right[2];
  const float r_right = r[0] * head_right[0] + r[1] * head_right[1] +
                        r[2] * head_right[2];
  const float r_fwd = r[0] * fwd[0] + r[1] * fwd[1] + r[2] * fwd[2];
  const float denominator = r_right - target * r_fwd;
  if (!(fabsf(denominator) > 1e-4f)) return false;
  float x_inner = side * (target * c_depth - ca) / denominator;
  if (x_inner < in.gap) x_inner = in.gap;
  const float extent = x_inner - in.gap;
  out->side = side;
  out->changed = false;
  out->extent = extent;
  for (int k = 0; k < 3; ++k) {
    out->right[k] = r[k];
    out->up[k] = u[k];
  }
  ColumnFrame frame = {in.head, in.center, v, r, u, head_right,
                       static_cast<float>(side), x_inner, {}, {}};
  {
    float d[3];
    for (int k = 0; k < 3; ++k) {
      frame.origin[k] = in.center[k] + r[k] * side * frame.x_inner;
      d[k] = frame.origin[k] - in.head[k];
    }
    overlay_math::Normalize(d);
    const float b = head_up[0] * d[0] + head_up[1] * d[1] + head_up[2] * d[2];
    for (int k = 0; k < 3; ++k) frame.dir[k] = head_up[k] - b * d[k];
    overlay_math::Normalize(frame.dir);
  }

  // Each button in the view.
  float anchor_view[kMaxCards][2];
  for (int i = 0; i < in.count; ++i) {
    if (in.has_anchor[i]) {
      ViewPoint(in.anchor[i], in.head, in.center, v, r, u, anchor_view[i]);
    }
  }

  // The cards with no button go first. The order of the others is the best
  // of all orders (4 cards have 24), with the cards as they are turned.
  int fixed[kMaxCards], movable[kMaxCards];
  int fixed_count = 0, movable_count = 0;
  for (int i = 0; i < in.count; ++i) {
    if (in.has_anchor[i]) {
      movable[movable_count++] = i;
    } else {
      fixed[fixed_count++] = i;
    }
  }
  // The order of the last frame, when its lines do not cross.
  if (in.keep_count == in.count) {
    bool valid = true;
    for (int s = 0; s < in.count; ++s) {
      if (in.keep_column[s] < 0 || in.keep_column[s] >= in.count) valid = false;
    }
    if (valid) {
      PlaceColumn(in, frame, in.keep_column, 3, 4, out);
      int crossings = 0;
      float edge_view[kMaxCards][2];
      for (int a = 0; a < in.count; ++a) {
        ViewPoint(out->edge[a], in.head, in.center, v, r, u, edge_view[a]);
      }
      for (int a = 0; a < in.count; ++a) {
        if (!in.has_anchor[a]) continue;
        for (int b = a + 1; b < in.count; ++b) {
          if (!in.has_anchor[b]) continue;
          if (SegmentsCross(anchor_view[a], edge_view[a], anchor_view[b],
                            edge_view[b])) {
            ++crossings;
          }
        }
      }
      if (crossings == 0) {
        out->crossings = 0;
        for (int s = 0; s < in.count; ++s) out->slot[in.keep_column[s]] = s;
        return true;
      }
    }
  }
  int best_column[kMaxCards];
  int best_crossings = 1 << 20;
  float best_cost = 1e30f;
  int order[kMaxCards];
  for (int i = 0; i < movable_count; ++i) order[i] = movable[i];
  // The permutations, in the order of Heap's method.
  int c[kMaxCards] = {0, 0, 0, 0};
  bool more = true;
  int step = 0;
  StackOut trial;
  while (more) {
    int column[kMaxCards];
    int n = 0;
    for (int i = 0; i < fixed_count; ++i) column[n++] = fixed[i];
    for (int i = 0; i < movable_count; ++i) column[n++] = order[i];
    PlaceColumn(in, frame, column, 3, 4, &trial);
    int crossings = 0;
    float cost = 0.0f;
    float edge_view[kMaxCards][2];
    for (int a = 0; a < in.count; ++a) {
      ViewPoint(trial.edge[a], in.head, in.center, v, r, u, edge_view[a]);
    }
    for (int a = 0; a < in.count; ++a) {
      if (!in.has_anchor[a]) continue;
      const float d = edge_view[a][1] - anchor_view[a][1];
      cost += d * d;
      for (int b = a + 1; b < in.count; ++b) {
        if (!in.has_anchor[b]) continue;
        if (SegmentsCross(anchor_view[a], edge_view[a], anchor_view[b],
                          edge_view[b])) {
          ++crossings;
        }
      }
    }
    if (crossings < best_crossings ||
        (crossings == best_crossings && cost < best_cost)) {
      best_crossings = crossings;
      best_cost = cost;
      for (int s = 0; s < n; ++s) best_column[s] = column[s];
    }
    // The next order (Heap's method, one step).
    more = false;
    while (step < movable_count) {
      if (c[step] < step) {
        const int j = (step % 2 == 0) ? 0 : c[step];
        const int t = order[j];
        order[j] = order[step];
        order[step] = t;
        ++c[step];
        step = 0;
        more = true;
        break;
      }
      c[step] = 0;
      ++step;
    }
  }
  PlaceColumn(in, frame, best_column, 3, 4, out);
  // The crossings of the lines as they are placed.
  out->crossings = 0;
  {
    float edge_view[kMaxCards][2];
    for (int a = 0; a < in.count; ++a) {
      ViewPoint(out->edge[a], in.head, in.center, v, r, u, edge_view[a]);
    }
    for (int a = 0; a < in.count; ++a) {
      if (!in.has_anchor[a]) continue;
      for (int b = a + 1; b < in.count; ++b) {
        if (!in.has_anchor[b]) continue;
        if (SegmentsCross(anchor_view[a], edge_view[a], anchor_view[b],
                          edge_view[b])) {
          ++out->crossings;
        }
      }
    }
  }
  (void)best_crossings;
  for (int s = 0; s < in.count; ++s) out->slot[best_column[s]] = s;
  return true;
}

// The fade of the controller of a hand: it comes in kModelFadeIn seconds
// and goes in kModelFadeOut seconds (hint_card_math::FadeStep).
const float kModelFadeIn = 0.20f;
const float kModelFadeOut = 0.35f;
// The alpha of the body of the controller, at full fade.
const float kBodyAlpha = 0.30f;
// The share of the opacity of the hand that goes while the controller
// shows: the hand stays, faint, so the player still sees where the Hand
// touches, and the controller shows through it.
const float kHandFaint = 0.65f;

inline float HandOpacity(float fade) {
  if (!(fade > 0.0f)) return 1.0f;
  if (fade > 1.0f) fade = 1.0f;
  return 1.0f - kHandFaint * fade;
}

}  // namespace hint_model
