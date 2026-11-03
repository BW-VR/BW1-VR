// THE BUTTON COMBINATIONS THAT SAVE THE TIMELINE (trace=1, docs/android.md,
// "The timeline"). The pure part: it reads the slot of the input channel that
// the app writes, so the math test can use it (test/controller_math/
// trace_checks.h).
//
// Two combinations save, and each one needs no other button:
//
//   1. The X button and the click of the left stick, held together.
//   2. The left trigger (squeezed to 0.7 or more) and the click of the right
//      stick, held together (trace_combo2=1, the default).
//
// Each one has no action in the game and in the mod with the defaults
// (docs/controls-audit.md, docs/controllers.md). Both clicks of the sticks
// together are not free: the app shows its menu over the game for them
// (play_menu_sticks). The second combination avoids the X button and the click
// of the left stick, so it still works when the headset keeps one of them.
#pragma once

#include <stdint.h>

#include "input_channel.h"

namespace trace_combo {

namespace ic = input_channel;

// The buttons as bits, for the log line of a change.
constexpr uint32_t kLeftActive = 1u << 0;
constexpr uint32_t kLeftStick = 1u << 1;
constexpr uint32_t kLeftX = 1u << 2;
constexpr uint32_t kLeftY = 1u << 3;
constexpr uint32_t kLeftMenu = 1u << 4;
constexpr uint32_t kLeftTrigger = 1u << 5;
constexpr uint32_t kRightActive = 1u << 6;
constexpr uint32_t kRightStick = 1u << 7;

constexpr float kTriggerHeld = 0.7f;

inline uint32_t Bits(const ic::SlotData& slot) {
  const ic::Hand& left = slot.hands[0];
  const ic::Hand& right = slot.hands[1];
  uint32_t bits = 0;
  if ((left.flags & ic::kActive) != 0) bits |= kLeftActive;
  if ((left.flags & ic::kStickClick) != 0) bits |= kLeftStick;
  if ((left.flags & ic::kPrimary) != 0) bits |= kLeftX;
  if ((left.flags & ic::kSecondary) != 0) bits |= kLeftY;
  if ((left.flags & ic::kMenu) != 0) bits |= kLeftMenu;
  if (left.trigger >= kTriggerHeld) bits |= kLeftTrigger;
  if ((right.flags & ic::kActive) != 0) bits |= kRightActive;
  if ((right.flags & ic::kStickClick) != 0) bits |= kRightStick;
  return bits;
}

constexpr uint32_t kFirstNeed = kLeftActive | kLeftStick | kLeftX;
constexpr uint32_t kSecondNeed = kLeftActive | kLeftTrigger | kRightActive | kRightStick;

inline bool FirstHeld(uint32_t bits) { return (bits & kFirstNeed) == kFirstNeed; }
inline bool SecondHeld(uint32_t bits) { return (bits & kSecondNeed) == kSecondNeed; }

// Which combination is held: 0 none, 1 the first, 2 the second. The second
// counts only with second_on (trace_combo2=1).
inline int Held(uint32_t bits, bool second_on) {
  if (FirstHeld(bits)) return 1;
  if (second_on && SecondHeld(bits)) return 2;
  return 0;
}

}  // namespace trace_combo
