// The values of the input channel (src/common/input_channel.h) as the values
// of XrScreen (src/vr/xr_screen.h), and back. No Windows code, so the math
// test checks it (test/controller_math/input_channel_checks.h).
//
// The values go across as they are: no smoothing and no prediction. A pose
// of the channel is the pose of the runtime at its display time.
#pragma once

#include <stdint.h>
#include <string.h>

#include "../common/input_channel.h"
#include "../vr/xr_screen.h"

namespace xr_input_math {

namespace ic = input_channel;

inline void ToEye(const ic::Eye& in, bwvr::EyeView* out) {
  memcpy(out->position, in.position, sizeof(out->position));
  memcpy(out->orientation, in.orientation, sizeof(out->orientation));
  out->fov_left = in.fov[0];
  out->fov_right = in.fov[1];
  out->fov_up = in.fov[2];
  out->fov_down = in.fov[3];
  out->valid = in.valid != 0;
}

inline void FromEye(const bwvr::EyeView& in, ic::Eye* out) {
  memset(out, 0, sizeof(*out));
  memcpy(out->position, in.position, sizeof(out->position));
  memcpy(out->orientation, in.orientation, sizeof(out->orientation));
  out->fov[0] = in.fov_left;
  out->fov[1] = in.fov_right;
  out->fov[2] = in.fov_up;
  out->fov[3] = in.fov_down;
  out->valid = in.valid ? 1u : 0u;
}

inline void ToHand(const ic::Hand& in, bwvr::HandInput* out) {
  memset(out, 0, sizeof(*out));
  out->active = (in.flags & ic::kActive) != 0;
  out->aim_valid = (in.flags & ic::kAimValid) != 0;
  out->grip_valid = (in.flags & ic::kGripValid) != 0;
  out->aim_tracked = (in.flags & ic::kAimTracked) != 0;
  out->grip_tracked = (in.flags & ic::kGripTracked) != 0;
  memcpy(out->aim_position, in.aim_position, sizeof(out->aim_position));
  memcpy(out->aim_orientation, in.aim_orientation, sizeof(out->aim_orientation));
  memcpy(out->grip_position, in.grip_position, sizeof(out->grip_position));
  memcpy(out->grip_orientation, in.grip_orientation,
         sizeof(out->grip_orientation));
  out->trigger = in.trigger;
  out->squeeze = in.squeeze;
  out->stick[0] = in.stick[0];
  out->stick[1] = in.stick[1];
  out->stick_click = (in.flags & ic::kStickClick) != 0;
  out->primary = (in.flags & ic::kPrimary) != 0;
  out->secondary = (in.flags & ic::kSecondary) != 0;
  out->menu = (in.flags & ic::kMenu) != 0;
  out->serial = in.serial;
}

inline void FromHand(const bwvr::HandInput& in, const char* profile,
                     ic::Hand* out) {
  memset(out, 0, sizeof(*out));
  out->flags = (in.active ? ic::kActive : 0u) |
               (in.aim_valid ? ic::kAimValid : 0u) |
               (in.grip_valid ? ic::kGripValid : 0u) |
               (in.aim_tracked ? ic::kAimTracked : 0u) |
               (in.grip_tracked ? ic::kGripTracked : 0u) |
               (in.stick_click ? ic::kStickClick : 0u) |
               (in.primary ? ic::kPrimary : 0u) |
               (in.secondary ? ic::kSecondary : 0u) |
               (in.menu ? ic::kMenu : 0u);
  memcpy(out->aim_position, in.aim_position, sizeof(out->aim_position));
  memcpy(out->aim_orientation, in.aim_orientation, sizeof(out->aim_orientation));
  memcpy(out->grip_position, in.grip_position, sizeof(out->grip_position));
  memcpy(out->grip_orientation, in.grip_orientation,
         sizeof(out->grip_orientation));
  out->trigger = in.trigger;
  out->squeeze = in.squeeze;
  out->stick[0] = in.stick[0];
  out->stick[1] = in.stick[1];
  out->serial = static_cast<uint32_t>(in.serial);
  if (profile != nullptr) {
    size_t n = strlen(profile);
    if (n >= ic::kProfileChars) n = ic::kProfileChars - 1;
    memcpy(out->profile, profile, n);
    out->profile[n] = 0;
  }
}

// A hand of a slot with no input: not active, each button up, the
// orientations the identity. The serial stays.
inline void NoHand(bwvr::HandInput* out) {
  const uint64_t serial = out->serial;
  memset(out, 0, sizeof(*out));
  out->serial = serial;
  out->aim_orientation[3] = 1.0f;
  out->grip_orientation[3] = 1.0f;
}

// One more recenter of the slot than the reader saw: the reader recenters
// one time. The first slot that the reader sees sets the count with no
// recenter, because a recenter before the start of the mod is not for it.
// THE SESSION OF THE APP. The count of the recenters starts at 0 in each
// start of the writer of the app. A new session is true one time, and the
// mod then takes the count of that session as it is, with no recenter.
inline bool NewSession(uint32_t session, bool* have, uint32_t* seen) {
  if (*have && session == *seen) return false;
  const bool changed = *have;
  *have = true;
  *seen = session;
  return changed;
}

inline bool TakeRecenter(uint32_t slot_recenters, bool* have, uint32_t* seen) {
  if (!*have) {
    *have = true;
    *seen = slot_recenters;
    return false;
  }
  if (slot_recenters == *seen) return false;
  *seen = slot_recenters;
  return true;
}

// THE LOCKSTEP OF A TEST (xr_input_lockstep_ms). The slot is for the frame
// of the controllers `want` when the writer tagged it with that frame. The
// frames count from 0, and the writer writes frame + 1 in SlotData::frame,
// so a slot of 0 is no slot. A slot of a later frame is a slot of an earlier
// run of the writer, and it is not for the frame.
inline bool SlotFor(uint32_t slot_frame, uint32_t want) {
  return slot_frame != 0 && slot_frame - 1 == want;
}

}  // namespace xr_input_math
