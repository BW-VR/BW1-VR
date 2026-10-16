// THE HEADSET APP OF THE BLACK & WHITE VR MOD (docs/android.md).
//
// A native activity with no Java code. In a headset it starts an OpenXR
// session and shows the menu of the mod as a quad in front of the player:
// the settings of the launcher in the form of the menu of the game. With no
// headset (the Android emulator) it shows the same menu in its window.
//
// The game itself is a Windows program for x86. The app finds the game
// folder and checks runblack.exe, but it cannot start the game yet: that
// needs the x86 runtime (docs/android.md, "What the app cannot do yet").

#include <android/input.h>
#include <android/keycodes.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <string>
#include <vector>

#include "../../src/common/version.h"
#include "app_log.h"
#include "build_id.h"
#include "flat_view.h"
#include "game_folder.h"
#include "menu_layout.h"
#include "menu_model.h"
#include "menu_painter.h"
#include "scene_player.h"
#include "xr_app.h"

namespace {

namespace ml = menu_layout;
using menu_model::Action;
using menu_model::Item;
using menu_model::Menu;

constexpr float kCursorMeters = 0.03f;
// The picture of the cursor (menu_painter.cpp).
constexpr uint32_t kCursorPixels = 64;

struct App {
  android_app* android = nullptr;
  JNIEnv* env = nullptr;
  std::string folder;  // the folder of the app on the shared storage
  GameFolder game;
  bwvr_ini::IniText ini;
  Menu menu;
  std::string status;
  std::string base_status;
  bool capture = false;  // menu_capture=1: each paint goes to bwvr-menu.bmp

  MenuPainter painter;
  std::vector<uint8_t> pixels;  // the last picture of the menu
  bool flat_dirty = true;       // the window needs a frame

  bool xr_on = false;
  XrApp xr;
  FlatView flat;
  bool window = false;
  bool resumed = false;
  bool quit = false;

  // The controller ray.
  int pointer_hand = 1;
  bool select_was[2] = {false, false};
  bool back_was[2] = {false, false};
  int stick_y_was = 0;
  int stick_x_was = 0;
  bool dragging = false;
};

void Save(App* app) {
  app->menu.Store(&app->ini);
  if (WriteTextFile(app->game.ini_path, app->ini.Text())) {
    app->menu.MarkSaved();
    app->status = "Saved.";
    AppLog("menu         : saved %s", app->game.ini_path.c_str());
  } else {
    app->status = "Cannot write bwvr.ini in the game folder.";
    AppLog("menu         : cannot write %s", app->game.ini_path.c_str());
  }
}

void Do(App* app, Action action) {
  switch (action) {
    case Action::kSave:
      Save(app);
      break;
    case Action::kPlay:
    case Action::kNewGame:
      // As the launcher: each start saves the settings first.
      Save(app);
      if (!app->game.exe_right) {
        app->status = app->game.message;
      } else {
        app->status = "Saved. This build cannot start the game: it has no x86 "
                      "runtime yet.";
      }
      AppLog("menu         : %s. %s",
             action == Action::kPlay ? "Play in VR" : "Play, new game",
             app->status.c_str());
      break;
    case Action::kNone:
      break;
  }
}

// The menu goes to the paint thread after each change, and the pixels of a
// paint that is done go to the quad or to the window.
void Paint(App* app) {
  const std::string& status = app->status.empty() ? app->base_status : app->status;
  app->painter.Ask(app->menu, status);
  if (!app->painter.Take(&app->pixels)) return;
  if (app->xr_on) {
    app->xr.SetMenuPixels(app->pixels);
  } else {
    app->flat.SetMenuPixels(app->pixels);
  }
  app->flat_dirty = true;
}

// A key of a keyboard, of a game pad or of the remote of the emulator.
bool OnKey(App* app, int32_t code) {
  Menu& m = app->menu;
  switch (code) {
    case AKEYCODE_DPAD_UP:
    case AKEYCODE_W:
      m.Up();
      return true;
    case AKEYCODE_DPAD_DOWN:
    case AKEYCODE_S:
      m.Down();
      return true;
    case AKEYCODE_DPAD_LEFT:
    case AKEYCODE_A:
      m.Step(-1);
      return true;
    case AKEYCODE_DPAD_RIGHT:
    case AKEYCODE_D:
      m.Step(1);
      return true;
    case AKEYCODE_ENTER:
    case AKEYCODE_DPAD_CENTER:
    case AKEYCODE_BUTTON_A:
    case AKEYCODE_SPACE:
      app->status.clear();
      Do(app, m.Select());
      return true;
    case AKEYCODE_BACK:
    case AKEYCODE_ESCAPE:
    case AKEYCODE_BUTTON_B:
      if (m.OnMain()) return false;  // the system closes the app
      m.Back();
      return true;
    default:
      return false;
  }
}

// A touch on the window: the row under it, and on a slider its value.
void OnTouch(App* app, float x, float y) {
  float rx, ry, rw, rh;
  app->flat.MenuRect(&rx, &ry, &rw, &rh);
  if (rw <= 0.0f || rh <= 0.0f) return;
  const double px = (x - rx) / rw * ml::kWidth;
  const double py = (y - ry) / rh * ml::kHeight;
  const std::vector<Item> items = app->menu.Items();
  const int row = ml::RowAt(py, static_cast<int>(items.size()));
  if (row < 0) return;
  app->menu.SetCursor(row);
  app->status.clear();
  if (app->menu.SliderFraction(items[row]) >= 0.0 && ml::OnBar(px)) {
    app->menu.SetSliderAt(ml::BarFraction(px));
  } else {
    Do(app, app->menu.Select());
  }
}

int32_t OnInput(android_app* android, AInputEvent* event) {
  App* app = static_cast<App*>(android->userData);
  const int32_t type = AInputEvent_getType(event);
  if (type == AINPUT_EVENT_TYPE_KEY) {
    if (AKeyEvent_getAction(event) != AKEY_EVENT_ACTION_DOWN) {
      // The up of a key that the down took.
      const int32_t code = AKeyEvent_getKeyCode(event);
      return code == AKEYCODE_BACK && app->menu.OnMain() ? 0 : 1;
    }
    return OnKey(app, AKeyEvent_getKeyCode(event)) ? 1 : 0;
  }
  if (type == AINPUT_EVENT_TYPE_MOTION && !app->xr_on) {
    const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    if (action == AMOTION_EVENT_ACTION_UP) {
      OnTouch(app, AMotionEvent_getX(event, 0), AMotionEvent_getY(event, 0));
    }
    return 1;
  }
  return 0;
}

void OnCommand(android_app* android, int32_t command) {
  App* app = static_cast<App*>(android->userData);
  switch (command) {
    case APP_CMD_INIT_WINDOW:
      app->window = true;
      if (!app->xr_on && android->window != nullptr) {
        app->flat.Start(android->window, ml::kWidth, ml::kHeight);
        if (!app->pixels.empty()) app->flat.SetMenuPixels(app->pixels);
        app->flat_dirty = true;
      }
      break;
    case APP_CMD_CONTENT_RECT_CHANGED:
      // A bar of the system (the task bar of a tablet) can cover a part of
      // the window. The menu goes into the rest.
      app->flat.SetContentRect(android->contentRect.left, android->contentRect.top,
                               android->contentRect.right,
                               android->contentRect.bottom);
      app->flat_dirty = true;
      AppLog("app          : the content of the window is %d %d %d %d",
             android->contentRect.left, android->contentRect.top,
             android->contentRect.right, android->contentRect.bottom);
      break;
    case APP_CMD_TERM_WINDOW:
      app->window = false;
      if (!app->xr_on) app->flat.Stop();
      break;
    case APP_CMD_RESUME:
      app->resumed = true;
      break;
    case APP_CMD_PAUSE:
      app->resumed = false;
      break;
    case APP_CMD_DESTROY:
      app->quit = true;
      break;
    default:
      break;
  }
}

// THE CONTROLLERS on the menu quad.
void OnControllers(App* app, const XrInput& in, bool* show_cursor,
                   float cursor[3]) {
  Menu& m = app->menu;
  const ml::Quad quad = ml::MenuQuad();
  // The hand that pressed its trigger last points.
  for (int h = 0; h < 2; ++h) {
    if (in.select[h] && !app->select_was[h]) app->pointer_hand = h;
  }
  const int p = app->pointer_hand;
  double px = 0.0, py = 0.0;
  bool hit = false;
  if (in.aim_valid[p]) {
    float forward[3];
    ml::Forward(in.aim_orientation[p], forward);
    hit = ml::RayToPixel(in.aim_position[p], forward, quad, &px, &py);
  }
  const std::vector<Item> items = m.Items();
  const int row = hit ? ml::RowAt(py, static_cast<int>(items.size())) : -1;
  if (row >= 0 && row != m.Cursor() && !app->dragging) {
    m.SetCursor(row);
    app->xr.Pulse(p);
  }
  const bool press = in.select[p] && !app->select_was[p];
  if (press && row >= 0) {
    app->status.clear();
    if (m.SliderFraction(items[row]) >= 0.0 && ml::OnBar(px)) {
      app->dragging = true;
    } else {
      Do(app, m.Select());
    }
  }
  if (app->dragging) {
    if (!in.select[p]) {
      app->dragging = false;
    } else if (hit) {
      m.SetSliderAt(ml::BarFraction(px));
    }
  }
  for (int h = 0; h < 2; ++h) {
    if (in.back[h] && !app->back_was[h]) m.Back();
  }

  // The sticks: up and down move the highlight, left and right change the
  // value. One step for each push.
  float sx = 0.0f, sy = 0.0f;
  for (int h = 0; h < 2; ++h) {
    if (fabsf(in.stick[h][0]) > fabsf(sx)) sx = in.stick[h][0];
    if (fabsf(in.stick[h][1]) > fabsf(sy)) sy = in.stick[h][1];
  }
  const int ystep = sy > 0.6f ? 1 : (sy < -0.6f ? -1 : 0);
  const int xstep = sx > 0.6f ? 1 : (sx < -0.6f ? -1 : 0);
  if (ystep != 0 && app->stick_y_was == 0) {
    if (ystep > 0) m.Up(); else m.Down();
  }
  if (xstep != 0 && app->stick_x_was == 0) m.Step(xstep);
  if (fabsf(sy) < 0.3f) app->stick_y_was = 0; else if (ystep != 0) app->stick_y_was = ystep;
  if (fabsf(sx) < 0.3f) app->stick_x_was = 0; else if (xstep != 0) app->stick_x_was = xstep;

  for (int h = 0; h < 2; ++h) {
    app->select_was[h] = in.select[h];
    app->back_was[h] = in.back[h];
  }
  *show_cursor = hit;
  if (hit) ml::PixelToPoint(quad, px, py, cursor);
}

void Load(App* app) {
  app->game = FindGameFolder(app->folder);
  app->ini.Parse(ReadTextFile(app->game.ini_path));
  app->menu.Load(app->ini);
  app->capture = app->ini.GetInt("bwvr", "menu_capture", 0) != 0;
  if (app->capture) {
    AppLog("menu         : menu_capture=1. Each paint of the menu goes to "
           "bwvr-menu.bmp in the game folder");
  }
}

// What the headset asks for goes into the ini, as the mod does on the PC, so
// the size of the render in the menu fits this headset.
void TakeHeadsetSize(App* app) {
  const int w = static_cast<int>(app->xr.EyeWidth());
  const int h = static_cast<int>(app->xr.EyeHeight());
  if (w <= 0 || h <= 0) return;
  app->menu.SetHeadsetSize(w, h);
  if (app->ini.GetInt("bwvr", "headset_width", 0) != w ||
      app->ini.GetInt("bwvr", "headset_height", 0) != h) {
    app->ini.SetInt("bwvr", "headset_width", w);
    app->ini.SetInt("bwvr", "headset_height", h);
    if (WriteTextFile(app->game.ini_path, app->ini.Text())) {
      AppLog("menu         : headset_width=%d and headset_height=%d in bwvr.ini",
             w, h);
    }
  }
}

}  // namespace

void android_main(android_app* android) {
  App app;
  app.android = android;
  android->userData = &app;
  android->onAppCmd = OnCommand;
  android->onInputEvent = OnInput;
  android->activity->vm->AttachCurrentThread(&app.env, nullptr);

  app.folder = android->activity->externalDataPath != nullptr
                   ? android->activity->externalDataPath
                   : android->activity->internalDataPath;
  mkdir(app.folder.c_str(), 0770);
  AppLogOpen(app.folder.c_str());
  AppLog("%s %s (build %s), the headset app", BWVR_PRODUCT_NAME,
         BWVR_VERSION_TEXT, BWVR_BUILD_ID);
  AppLog("app          : the folder of the app is %s", app.folder.c_str());

  Load(&app);
  if (!app.painter.Start(android->activity->vm,
                         app.capture ? app.game.folder + "/bwvr-menu.bmp" : "")) {
    AppLog("app          : the canvas of Android did not start. The app stops");
    ANativeActivity_finish(android->activity);
  }

  // A TEST AID (stream_replay=1 in bwvr.ini): the stream of the walks in
  // bwvr-stream.bin of the game folder goes to the Vulkan renderer of the
  // mod, which writes the pictures of vk_scene_test_frame of the stream
  // into the game folder (docs/android.md, "The stream of the walks").
  if (app.ini.GetInt("bwvr", "stream_replay", 0) != 0) {
    PlayStreamFile(app.game.folder + "/bwvr-stream.bin", app.game.folder);
  }

  app.xr_on = app.xr.Start(android->activity, ml::kWidth, ml::kHeight,
                           kCursorPixels);
  if (app.xr_on) {
    TakeHeadsetSize(&app);
    app.base_status = app.game.exe_right
                          ? "Ready. OpenXR runtime: " + app.xr.RuntimeName()
                          : app.game.message;
    app.xr.SetCursorPixels(app.painter.Cursor());
  } else {
    app.xr.Stop();
    app.base_status = app.game.exe_right
                          ? "No headset: the menu shows on the screen."
                          : app.game.message;
    AppLog("app          : no OpenXR session. The menu shows in the window");
  }

  const ml::Quad quad = ml::MenuQuad();
  const float menu_size[2] = {quad.width, quad.height};
  while (!android->destroyRequested && !app.quit) {
    // With a session the frame loop sets the pace. Else the app waits for
    // an event, or for 50 ms.
    const int timeout = (app.xr_on && app.xr.Running()) ? 0 : 50;
    int events = 0;
    android_poll_source* source = nullptr;
    while (ALooper_pollOnce(timeout, nullptr, &events,
                            reinterpret_cast<void**>(&source)) >= 0) {
      if (source != nullptr) source->process(android, source);
      if (android->destroyRequested) break;
      if (timeout != 0) break;
    }
    if (android->destroyRequested) break;

    if (app.xr_on) {
      bool quit = false;
      app.xr.PollEvents(&quit);
      if (quit) {
        ANativeActivity_finish(android->activity);
        break;
      }
      if (!app.xr.Running()) continue;
      XrInput input;
      if (!app.xr.BeginFrame(&input)) continue;
      bool show_cursor = false;
      float cursor[3] = {0, 0, 0};
      OnControllers(&app, input, &show_cursor, cursor);
      Paint(&app);
      app.xr.EndFrame(quad.center, menu_size, show_cursor, cursor,
                      kCursorMeters);
    } else if (app.window && app.flat.Ready()) {
      Paint(&app);
      if (app.flat_dirty && app.flat.Present()) app.flat_dirty = false;
    }
  }

  AppLog("app          : the app ends");
  if (app.xr_on) app.xr.Stop();
  app.flat.Stop();
  app.painter.Stop();
  android->activity->vm->DetachCurrentThread();
}
