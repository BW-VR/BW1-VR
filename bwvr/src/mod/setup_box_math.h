// THE DIALOG BOX THAT IS UP, for the log (setup_box.cpp, docs/ui-layer.md,
// section 5.2). No Windows code, so the math test checks it
// (test/controller_math/screen_quad_checks.h).
//
// SetupBox::GetCurrentActiveBox gives the SetupBox of the dialog that is up.
// Each dialog of the game is an object of its own class (NewProfileBox,
// StartGameBox, MainMenu and more) that keeps its SetupBox
// (src/common/bw_addresses.h, bw::setup). The game has the RTTI of MSVC, so
// the mod reads the name of the class of an object from the game itself:
// the vtable of an object, the Complete Object Locator before the vtable,
// and the name of its type descriptor.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace setup_box_math {

// THE NAME OF A CLASS from the name of an RTTI type descriptor of MSVC:
// ".?AVName@@" (a class) or ".?AUName@@" (a struct) gives "Name". text has
// at most n bytes and ends at its first 0. False for another text, a name of
// a template or a namespace (another "@" in it), or a name that does not fit
// in out_size with its 0.
inline bool RttiClassName(const char* text, size_t n, char* out, size_t out_size) {
  if (text == nullptr || out == nullptr || out_size == 0) return false;
  size_t len = 0;
  while (len < n && text[len] != 0) ++len;
  if (len < 7 || text[0] != '.' || text[1] != '?' || text[2] != 'A' ||
      (text[3] != 'V' && text[3] != 'U') || text[len - 1] != '@' || text[len - 2] != '@') {
    return false;
  }
  const size_t name = len - 6;
  if (name + 1 > out_size) return false;
  for (size_t i = 0; i < name; ++i) {
    const char c = text[4 + i];
    if (c == '@' || c == '?' || c == '$') return false;
    out[i] = c;
  }
  out[name] = 0;
  return true;
}

// The offsets of the RTTI of MSVC on 32 bit x86: the pointer to the Complete
// Object Locator is just before the vtable, the type descriptor is at +12
// of the locator, and the name is at +8 of the type descriptor.
constexpr uint32_t kLocatorBeforeVTable = 4;
constexpr uint32_t kTypeOfLocator = 12;
constexpr uint32_t kNameOfType = 8;

// THE STATE THAT THE LOG FOLLOWS: the SetupBox that is up, its vtable, the
// dialog object that keeps it and the vtable of that object, and the
// callback of its controls. A new line of the log for each change.
struct BoxState {
  uint32_t box = 0;
  uint32_t vtable = 0;
  uint32_t dialog = 0;
  uint32_t dialog_vtable = 0;
  uint32_t callback = 0;
};
inline bool SameBox(const BoxState& a, const BoxState& b) {
  return a.box == b.box && a.vtable == b.vtable && a.dialog == b.dialog &&
         a.dialog_vtable == b.dialog_vtable && a.callback == b.callback;
}

}  // namespace setup_box_math
