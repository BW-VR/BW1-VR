// The hint card: one small picture in the room, beside the wrist of the
// hand that points. It holds the hint that the game shows beside the Hand,
// and the prompts of the controller. See hint_card.cpp and
// docs/controller-hints.md, "The hint card".

#pragma once

#include <stdint.h>

namespace bwvr {
struct CardLayer;
}

// Once a stereo frame, before the eye passes (ui_panel.cpp passes it on).
// to_headset: the frame goes to a headset. projection: as a projection layer.
// capture: this frame saves the eye pictures, so it saves the card too.
void HintCardBeginFrame(bool to_headset, bool projection, uint64_t frame,
                        bool capture);

// At the start of the left eye pass, after ControllerHintsPass. True when the
// hints of this frame go to the card, and then no hint draws in an eye
// picture. The answer holds for both eye passes.
bool HintCardRoute();

// A hint of the game that did not draw: the word of the controller button (or
// null for a hint with no button) and the caption. `button_code` is the mouse
// button code of the game's call, 0 for none.
void HintCardGameHint(int eye, const wchar_t* button, uint32_t button_code,
                      const wchar_t* text);

// Inside the left eye pass, after the world drew: what the Hand can do now.
void HintCardLatchContext(int eye);

// After both eye passes: the card of this frame goes to the headset.
void HintCardEndFrame();

// The save of the game starts (mod_main.cpp): the card of the pointing hand
// says "Saving...", at the place of its card in the last frame. The caller
// gives the layer to XrScreen::HoldCard. False when the cards did not show in
// the last frame, or with hint_card_saving=0.
bool HintCardSavingLayer(bwvr::CardLayer* out);

// The test key (Ctrl Shift F11 with ui_panel=0): the card, or the hints in
// the eye pictures.
void HintCardToggle();
// For the debug page: 1 while the hints show on the cards, 0 while they draw
// in the eye pictures, kDebugCannot with hint_card=0.
int HintCardCardsValue();

// The test key of prompt_moves (Ctrl, Shift and the period key, with
// test_keys=1), and the word "promptkey" of the scripted controller: the key
// changes between 1 and 0. `by` names the cause in the log.
void HintCardMovesToggle(const char* by);

// The player did the action that a line of a card teaches, and it worked
// (hint_learn=1). skill is a hint_learn_math::Skill, and `how` says what
// worked, for the log. The callers: teleport.cpp, tabletop.cpp (the walk),
// world_grab.cpp, far_grab.cpp and creature_command.cpp. This file sees the
// take, the drop and the ring itself.
void HintCardLearnDone(int skill, const char* how);

// controller_hints.cpp, once a frame: the ToolTips option of the game
// (TOOLTIP_LEVEL, 0 to 3). With hints_follow_game=1 it chooses whether a line
// of a card goes away when the player knows it (hint_learn_math.h).
void HintCardFollowTooltips(int tooltip_level);
// Once a report window.
void HintCardReport();

// The check of the words (text_fit_test=1, text_fit.h): `text` in the box of
// a line of a card, in each look. spell_book.cpp gives its lines here too.
void HintCardCheckLine(const wchar_t* text);
