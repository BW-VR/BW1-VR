// The controller in the hand (hint_style=2): the model of the real
// controller at the grip pose, with the buttons of the hints lit, and a card
// for each lit button with a line of ink to it. See hint_model.cpp and
// docs/controller-hints.md, "The controller in the hand".

#pragma once

#include <stdint.h>

struct IDirect3DDevice7;
struct OverlayHands;

// The look of the hints now: 2 the controller in the hand, 1 the cards at
// the wrists. The key hint_style, and its test key.
int HintModelStyle();

// True when the model of the controller of one hand (0 the left hand, 1 the
// right hand) is ready. The first call for a model starts its load on a
// thread, so the answer is false until the load is done. False with
// hint_style=1 and when no model fits the profile.
bool HintModelReady(int side);

// True when the model of that hand has a node for the control
// (controller_art::kFirst to kPad).
bool HintModelHasControl(int side, int control);

// THE CARDS OF THE BUTTONS of one hand (0 the hand that points, 1 the other
// hand), from hint_card.cpp after both eye passes. control is the lit
// control of each card, or -1 for a card with no button. width and height
// are in meters. With show false the cards and the controller fade out. On
// return the place of each card in the LOCAL space. False when the hand has
// no place this frame.
const int kHintModelCards = 4;
struct HintModelCard {
  int control;
  float width;
  float height;
};
struct HintModelPlaced {
  float center[kHintModelCards][3];
  float orientation[4];
  int side;  // -1 the cards are left of the controller in the view, 1 right
  bool changed;
};
bool HintModelPlace(int which, const HintModelCard* cards, int count,
                    bool show, float dt, HintModelPlaced* out);

// overlay.cpp, once a frame in BuildFrame: the meshes of the controllers
// and the lines in the world, from the hands of the overlay. stop is the
// stop at the land of each hand (hand_land.cpp) in the world: the controller
// goes with the drawn hand, and the line still ends at its card.
void HintModelBuild(const OverlayHands& hands, const float stop[2][3]);
// True when the build of this frame has something to draw.
bool HintModelHasDraw();
// The share of the opacity of a hand (0 the left hand, 1 the right hand)
// while its controller shows.
float HintModelHandOpacity(int side);
// In an eye pass, after the opaque meshes of the overlay and before the
// mesh of the hands. Returns the count of triangles, or -1 on a failure.
int HintModelDraw(IDirect3DDevice7* device, const float camera[12],
                  float near_plane, float half_x, float half_y);

// The test key (Ctrl Shift H with test_keys=1): hint_style 2 or 1.
void HintModelPollKey();

// Once a report window.
void HintModelReport();
