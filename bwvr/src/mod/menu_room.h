// The menus of the game on a panel that is fixed in the room, and the
// keyboard in the headset under it. See docs/ui-layer.md, "The menus in the
// room" and "The keyboard in the headset".
//
//   menu_room.cpp    the place of the panel, the route of each frame, the
//                    pointer on the panel, and the frames with no scene
//   vr_keyboard.cpp  the keyboard card and its keys
//
// This header has no Direct3D in it.

#pragma once

#include <stdint.h>

namespace bwvr {
class XrScreen;
struct PanelQuad;
}  // namespace bwvr

// --- menu_room.cpp -----------------------------------------------------------

void MenuRoomSetScreen(bwvr::XrScreen* screen);
// From UiPanelSetOptions: menu_room, menu_room_test and vr_keyboard of
// bwvr.ini, and whether ui_panel is 0.
void MenuRoomSetOptions(bool on, bool test, bool keyboard, bool panel_off);
// True when the menus can go to the room: menu_room=1 and ui_panel=0.
bool MenuRoomEnabled();
// stereo.cpp, once a frame, beside UiPanelHead: the head center and the
// orientation of the head in the LOCAL space.
void MenuRoomHead(const float center[3], const float orientation[4]);
// Once a stereo frame, before the eye passes (UiPanelBeginFrame). It latches
// whether the menus of this frame go to the panel in the room.
bool MenuRoomBeginFrame(bool to_headset, bool projection, uint64_t frame);
// The latch of this stereo frame.
bool MenuRoomFrame();
// The quad of the menu panel of this frame, for XrScreen::SetPanelQuads.
bool MenuRoomQuad(uint32_t render_w, uint32_t render_h, bwvr::PanelQuad* out);
// A frame with no scene (h_FinishFrame in mod_main.cpp), before the frame
// goes to the headset. With a menu up, the hands, the pointer and the
// keyboard of the frame run, and the frame goes out as a screen at the place
// of the panel. True when it staged the screen.
bool MenuRoomFlatFrame(uint64_t frame);
// True in a frame with no scene that shows the menus in the room.
bool MenuRoomFlatNow();
// True while the pointer is on the panel in the room: the menus of this
// stereo frame, or a frame with no scene with the menus in the room.
bool MenuRoomSurface();
// The aim ray of the pointing hand (LOCAL space) against the panel, once a
// frame from controllers.cpp. trigger is the value of its trigger, for the
// keyboard. x and y get the pixel of the
// render under the ray, clamped to the panel. off is true when the ray is
// past the edge of the panel or on the keyboard: a press then does not reach
// the game. meters is the length of the ray to the panel or the keyboard.
// False when the ray does not meet the plane of the panel.
bool MenuRoomRayPixel(float trigger, const float aim_position[3],
                      const float aim_orientation[4], float* x, float* y,
                      bool* off, float* meters);
void MenuRoomReport();

// THE KEYBOARD PLACE: the pose and the size of the keyboard card, below
// the menu panel and nearer. False while the panel has no place.
bool MenuRoomKeyboardPlace(float position[3], float orientation[4],
                           float* width, float* height);
// The ray of the pointing hand on the keyboard in this frame: u and v from
// 0 to 1. False when the ray is not on the keyboard card.
bool MenuRoomKeyboardRay(float* u, float* v);
// The trigger of the pointing hand in this frame, 0 to 1.
float MenuRoomTrigger();

// --- vr_keyboard.cpp ---------------------------------------------------------

void VrKeyboardSetOptions(bool on);
// Once a frame with the menus in the room, after the hands of the frame
// (ControllersFrame): the keyboard of the frame. It stages its card.
void VrKeyboardFrame(uint64_t frame);
// No keyboard in this frame, and none until a text box has the focus again.
void VrKeyboardHide();
// True while the keyboard shows.
bool VrKeyboardShown();
void VrKeyboardReport();
