#include "menu_painter.h"

#include <stdio.h>
#include <time.h>

#include "app_log.h"
#include "canvas.h"
#include "keyboard_layout.h"
#include "menu_layout.h"
#include "menu_paint.h"

namespace {

constexpr int kCursorSize = 64;

double Now() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}

// A TEST AID (menu_capture=1 in bwvr.ini): the picture of the menu as a BMP
// of 32 bits, so a run on a device or in the emulator can show the menu.
void WriteBmp(const std::string& path, const std::vector<uint8_t>& rgba, int w,
              int h) {
  FILE* f = fopen(path.c_str(), "wb");
  if (f == nullptr) return;
  const uint32_t data = static_cast<uint32_t>(w) * h * 4;
  uint8_t header[54] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) {
    for (int i = 0; i < 4; ++i) header[at + i] = static_cast<uint8_t>(v >> (8 * i));
  };
  put32(2, 54 + data);
  put32(10, 54);
  put32(14, 40);
  put32(18, static_cast<uint32_t>(w));
  put32(22, static_cast<uint32_t>(h));
  header[26] = 1;
  header[28] = 32;
  put32(34, data);
  fwrite(header, 1, sizeof(header), f);
  std::vector<uint8_t> row(static_cast<size_t>(w) * 4);
  for (int y = h - 1; y >= 0; --y) {
    const uint8_t* src = rgba.data() + static_cast<size_t>(y) * w * 4;
    for (int x = 0; x < w; ++x) {
      row[x * 4 + 0] = src[x * 4 + 2];
      row[x * 4 + 1] = src[x * 4 + 1];
      row[x * 4 + 2] = src[x * 4 + 0];
      row[x * 4 + 3] = src[x * 4 + 3];
    }
    fwrite(row.data(), 1, row.size(), f);
  }
  fclose(f);
}

}  // namespace

bool MenuPainter::Start(JavaVM* vm, const std::string& capture_path) {
  capture_path_ = capture_path;
  thread_ = std::thread([this, vm] { Run(vm); });
  std::unique_lock<std::mutex> hold(lock_);
  started_.wait(hold, [this] { return start_done_; });
  return ready_;
}

void MenuPainter::Stop() {
  {
    std::lock_guard<std::mutex> hold(lock_);
    stop_ = true;
  }
  wake_.notify_all();
  if (thread_.joinable()) thread_.join();
}

void MenuPainter::Ask(const menu_model::Menu& menu, const std::string& status) {
  {
    std::lock_guard<std::mutex> hold(lock_);
    if (menu.Revision() == asked_revision_ && status == asked_status_) return;
    asked_revision_ = menu.Revision();
    asked_status_ = status;
    menu_ = menu;
    status_ = status;
    asked_ = true;
  }
  wake_.notify_all();
}

bool MenuPainter::Take(std::vector<uint8_t>* pixels) {
  std::lock_guard<std::mutex> hold(lock_);
  if (!done_) return false;
  done_ = false;
  pixels->swap(pixels_);
  return true;
}

void MenuPainter::AskKeyboard(int lit) {
  {
    std::lock_guard<std::mutex> hold(lock_);
    if (lit == asked_keyboard_lit_) return;
    asked_keyboard_lit_ = lit;
    keyboard_lit_ = lit;
    keyboard_asked_ = true;
  }
  wake_.notify_all();
}

bool MenuPainter::TakeKeyboard(std::vector<uint8_t>* pixels) {
  std::lock_guard<std::mutex> hold(lock_);
  if (!keyboard_done_) return false;
  keyboard_done_ = false;
  pixels->swap(keyboard_pixels_);
  return true;
}

void MenuPainter::Run(JavaVM* vm) {
  JNIEnv* env = nullptr;
  vm->AttachCurrentThread(&env, nullptr);
  Canvas2D canvas;
  Canvas2D cursor;
  Canvas2D keyboard;
  const bool ok = canvas.Start(env, menu_layout::kWidth, menu_layout::kHeight) &&
                  cursor.Start(env, kCursorSize, kCursorSize) &&
                  keyboard.Start(env, keyboard_layout::kWidth, keyboard_layout::kHeight);
  if (ok) {
    PaintCursor(&cursor);
    cursor.ReadPixels(&cursor_);
  }
  {
    std::lock_guard<std::mutex> hold(lock_);
    ready_ = ok;
    start_done_ = true;
  }
  started_.notify_all();

  int logged = 0;
  int keyboard_logged = 0;
  std::vector<uint8_t> pixels;
  std::vector<uint8_t> keyboard_pixels;
  for (;;) {
    menu_model::Menu menu;
    std::string status;
    bool paint_menu = false;
    bool paint_keyboard = false;
    int lit = -1;
    {
      std::unique_lock<std::mutex> hold(lock_);
      wake_.wait(hold, [this] { return stop_ || asked_ || keyboard_asked_; });
      if (stop_) break;
      paint_menu = asked_;
      paint_keyboard = keyboard_asked_;
      menu = menu_;
      status = status_;
      lit = keyboard_lit_;
      asked_ = false;
      keyboard_asked_ = false;
    }
    if (!ok) continue;
    if (paint_keyboard) {
      const double start = Now();
      PaintKeyboard(&keyboard, lit);
      keyboard.ReadPixels(&keyboard_pixels);
      if (keyboard_logged < 4) {
        ++keyboard_logged;
        AppLog("keyboard     : a paint of the keyboard took %.1f ms", (Now() - start) * 1000.0);
      }
      std::lock_guard<std::mutex> hold(lock_);
      keyboard_pixels_.swap(keyboard_pixels);
      keyboard_done_ = true;
    }
    if (!paint_menu) continue;
    const double start = Now();
    PaintMenu(&canvas, menu, status);
    canvas.ReadPixels(&pixels);
    if (!capture_path_.empty()) {
      WriteBmp(capture_path_, pixels, menu_layout::kWidth, menu_layout::kHeight);
    }
    if (logged < 8) {
      ++logged;
      AppLog("menu         : paint %u took %.1f ms", menu.Revision(),
             (Now() - start) * 1000.0);
    }
    std::lock_guard<std::mutex> hold(lock_);
    pixels_.swap(pixels);
    done_ = true;
  }
  canvas.Stop();
  cursor.Stop();
  keyboard.Stop();
  vm->DetachCurrentThread();
}
