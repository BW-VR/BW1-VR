// THE PAINT THREAD OF THE MENU (docs/android.md, "The menu").
//
// A paint of the whole menu with the canvas of Android takes from some ms
// to more than 100 ms (the emulator with the test runtime). The frame loop
// of the session must not wait for it, so a thread of its own paints. The
// frame loop gives it a copy of the menu and the status line, and takes the
// pixels when they are ready. The frames show the picture before until
// then.
#pragma once

#include <jni.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "menu_model.h"

class MenuPainter {
 public:
  // capture_path: with menu_capture=1, each paint goes there as a BMP.
  // Empty: no file.
  bool Start(JavaVM* vm, const std::string& capture_path);
  void Stop();

  // A paint of this menu and status, when they differ from the last asked
  // paint.
  void Ask(const menu_model::Menu& menu, const std::string& status);
  // True once for each paint that is done since the last call, with its
  // pixels (RGBA, the color multiplied by the alpha).
  bool Take(std::vector<uint8_t>* pixels);
  // The pixels of the cursor, painted one time at the start.
  const std::vector<uint8_t>& Cursor() const { return cursor_; }
  // THE KEYBOARD OF THE APP (keyboard_layout.h): a paint with the key `lit`
  // lit (-1: none), when it differs from the last asked paint of the
  // keyboard. TakeKeyboard is true once for each paint that is done.
  void AskKeyboard(int lit);
  bool TakeKeyboard(std::vector<uint8_t>* pixels);

 private:
  void Run(JavaVM* vm);

  std::thread thread_;
  std::mutex lock_;
  std::condition_variable wake_;
  std::condition_variable started_;
  bool start_done_ = false;
  bool ready_ = false;
  bool stop_ = false;
  bool asked_ = false;
  bool done_ = false;
  menu_model::Menu menu_;
  std::string status_;
  unsigned asked_revision_ = ~0u;
  std::string asked_status_;
  std::vector<uint8_t> pixels_;
  std::vector<uint8_t> cursor_;
  std::string capture_path_;
  bool keyboard_asked_ = false;
  bool keyboard_done_ = false;
  int keyboard_lit_ = -1;
  int asked_keyboard_lit_ = -2;
  std::vector<uint8_t> keyboard_pixels_;
};
