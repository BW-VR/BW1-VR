// THE DIALOG BOX THAT IS UP, for the log (setup_box_math.h). Each FinishFrame
// reads SetupBox::GetCurrentActiveBox and the dialog that keeps the box, and
// one line of the log says each change. It also says whether the box takes
// text, for the keyboard of the headset app (controllers.cpp). It only
// reads.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../common/bw_addresses.h"
#include "setup_box_math.h"

void ModLog(const char* format, ...);
uint64_t ControllersFrameCount();

namespace {

namespace sb = setup_box_math;

sb::BoxState g_before;
int g_lines = 0;
const int kMaxLines = 200;

bool ReadU32(uintptr_t at, uint32_t* out) {
  if (at == 0 || IsBadReadPtr(reinterpret_cast<const void*>(at), 4)) return false;
  *out = *reinterpret_cast<const uint32_t*>(at);
  return true;
}

// The name of the class of an object with this vtable, from the RTTI of the
// game. "?" when the game has none for it.
void ClassOf(uint32_t vtable, char* out, size_t out_size) {
  uint32_t locator = 0, type = 0;
  out[0] = '?';
  out[1] = 0;
  if (!ReadU32(vtable - sb::kLocatorBeforeVTable, &locator) ||
      !ReadU32(locator + sb::kTypeOfLocator, &type)) {
    return;
  }
  const uintptr_t name = static_cast<uintptr_t>(type) + sb::kNameOfType;
  char text[96] = {};
  if (IsBadReadPtr(reinterpret_cast<const void*>(name), sizeof(text) - 1)) return;
  memcpy(text, reinterpret_cast<const void*>(name), sizeof(text) - 1);
  if (!sb::RttiClassName(text, sizeof(text) - 1, out, out_size)) {
    out[0] = '?';
    out[1] = 0;
  }
}

// The dialog object of the front end that keeps this SetupBox, or 0.
uint32_t DialogOf(uint32_t box) {
  for (const auto& range : bw::setup::kFrontEndDialogs) {
    for (uintptr_t at = range[0]; at < range[1]; at += 4) {
      uint32_t dialog = 0, kept = 0;
      if (ReadU32(at, &dialog) && dialog != 0 &&
          ReadU32(dialog + bw::setup::kDialogBox, &kept) && kept == box) {
        return dialog;
      }
    }
  }
  return 0;
}

}  // namespace

// The dialog box that is up takes text now: the control with its focus is a
// SetupEdit (bw::setup, kBoxFocus), for example the name in NewProfileBox.
bool SetupBoxTakesText() {
  uint32_t box = 0, focus = 0, vtable = 0;
  return ReadU32(bw::setup::kActiveBox, &box) && box != 0 &&
         ReadU32(box + bw::setup::kBoxFocus, &focus) && focus != 0 &&
         ReadU32(focus, &vtable) && vtable == bw::setup::kSetupEditVTable;
}

void SetupBoxWatchFrame(uint64_t finish_frame) {
  sb::BoxState now;
  if (ReadU32(bw::setup::kActiveBox, &now.box) && now.box != 0) {
    ReadU32(now.box, &now.vtable);
    ReadU32(now.box + bw::setup::kBoxCallback, &now.callback);
  }
  // The dialog only when the box changed: the search reads some globals.
  if (now.box == g_before.box && now.vtable == g_before.vtable) {
    now.dialog = g_before.dialog;
    now.dialog_vtable = g_before.dialog_vtable;
  } else if (now.box != 0) {
    now.dialog = DialogOf(now.box);
    if (now.dialog != 0) ReadU32(now.dialog, &now.dialog_vtable);
  }
  if (sb::SameBox(now, g_before)) return;
  g_before = now;
  if (g_lines >= kMaxLines) return;
  ++g_lines;
  if (now.box == 0) {
    ModLog("setup box    : no dialog box is up now. FinishFrame %llu, frame %llu of "
           "the controllers",
           finish_frame, ControllersFrameCount());
    return;
  }
  char box_class[64], dialog_class[64];
  ClassOf(now.vtable, box_class, sizeof(box_class));
  if (now.dialog != 0) {
    ClassOf(now.dialog_vtable, dialog_class, sizeof(dialog_class));
    ModLog("setup box    : the dialog box %s is up now: the %s at %08X (vtable "
           "%08X), kept by the %s at %08X (vtable %08X), the callback of its "
           "controls %08X. FinishFrame %llu, frame %llu of the controllers",
           dialog_class, box_class, now.box, now.vtable, dialog_class, now.dialog,
           now.dialog_vtable, now.callback, finish_frame, ControllersFrameCount());
  } else {
    ModLog("setup box    : a dialog box is up now: the %s at %08X (vtable %08X), "
           "the callback of its controls %08X. No dialog object of the front end "
           "keeps it. FinishFrame %llu, frame %llu of the controllers",
           box_class, now.box, now.vtable, now.callback, finish_frame,
           ControllersFrameCount());
  }
}
