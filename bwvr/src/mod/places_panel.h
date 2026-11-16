// The panel of places: a page of buttons for the places of the game and its
// 7 bookmarks, fixed in the room in front of the player. The item "places"
// of the ring opens it, and the ray and the trigger of the pointing hand
// press its buttons. See places_panel.cpp and docs/ui-layer.md, "The panel
// of places".

#pragma once

// places_panel of bwvr.ini: 1 the ring has the item "places".
bool PlacesPanelOn();

// radial_menu.cpp, at the choice of the item "places": the panel opens in
// front of the head. False when the head has no pose yet.
bool PlacesPanelOpenFromRing();

// True while the panel is open. It uses the quad layer of the ring, so
// RadialMenuShows is true then too.
bool PlacesPanelOpen();

// controllers.cpp, once a frame, after RadialMenuFrame: the ray on the
// panel, a press of the trigger, and the picture of the frame.
void PlacesPanelFrame();

// controllers.cpp, the pump: true while the trigger of the pointing hand
// belongs to the panel, so the game does not get its press. That is while
// the panel is open, and after a press that closed it until the trigger
// goes up.
bool PlacesPanelHoldsTrigger();

// controllers.cpp, ControllersMenuRay: true while the panel is open. meters
// gets the length of the aim ray to the panel, or 0 when the ray is not on
// the panel.
bool PlacesPanelRay(float* meters);

// controllers.cpp: B of the pointing hand and the menu button close the
// panel (places_close_buttons=1). True when the panel was open and the key
// is on: then the press does no more.
bool PlacesPanelCloseBy(const char* why);

// controllers.cpp, for the word "aimplaces" of the scripted controller: the
// middle of the next button of the test key places_test_buttons, in the
// LOCAL space of OpenXR. False when the panel is closed or the list is
// empty.
bool PlacesPanelTestTarget(float out[3]);

// A job of the thread of the warm of the fonts (card_scroll::WarmAdd), with
// panel_warm=1: the page of the parchment, and each button and seal in the
// looks of a first picture, so the first open copies them.
void PlacesPanelWarm();

// Once a report window.
void PlacesPanelReport();
