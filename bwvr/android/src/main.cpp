// THE HEADSET APP OF THE BLACK & WHITE VR MOD (docs/android.md).
//
// A native activity with no Java code. In a headset it starts an OpenXR
// session and shows the menu of the mod as a quad in front of the player:
// the settings of the launcher in the form of the menu of the game. With no
// headset (the Android emulator) it shows the same menu in its window.
//
// The game itself is a Windows program for x86. The app finds the game
// folder and checks runblack.exe, and it sets up Wine from the APK
// (wine_runtime.h). "Play in VR" and "Play, new game" start the game in
// Wine with the mod as its DirectDraw (play_rules.h, docs/android.md, "Play
// in the app"): the mod gives its frames through the live stream, the app
// draws them, and the app gives the head and the hands to the mod through
// the input channel.

#include <android/input.h>
#include <android/keycodes.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "../../src/common/version.h"
#include "app_log.h"
#include "build_id.h"
#include "flat_view.h"
#include "game_folder.h"
#include "input_live.h"
#include "keyboard_layout.h"
#include "menu_layout.h"
#include "menu_model.h"
#include "menu_painter.h"
#include "play_rules.h"
#include "../../src/mod/screen_quad_math.h"
#include "../../src/mod/stream_ring.h"
#include "../../src/mod/stream_split.h"
#include "../../src/mod/view_layer_math.h"
#include "../../src/mod/vk_platform_posix.h"
#include "../../src/mod/vk_scene.h"
#include "scene_player.h"
#include "stream_reader.h"
#include "wine_runtime.h"
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

  // The picture of the renderer on the stereo screen of the session.
  VkImage screen_image = VK_NULL_HANDLE;
  bool said_view_size = false;
  bool said_no_panel = false;

  // THE LIVE STREAM (stream_live=1, docs/android.md, "The live stream").
  bool live = false;
  StreamReader live_reader;
  PlayResult live_result;
  std::vector<QueuedFrame> live_batch;
  size_t live_whole = 3;  // stream_live_frames
  std::vector<uint8_t> live_state;
  int32_t live_test_frame = 0;  // vk_scene_test_frame of the last kStart
  uint64_t live_frames = 0;    // the frames played since frames came
  uint64_t live_skipped = 0;   // the frames with no walks: a newer frame replaced them
  uint64_t live_pictures = 0;  // the batches: one new picture each
  uint64_t live_first = 0;
  uint64_t live_last = 0;
  double live_last_ms = 0.0;  // the time of the last frame
  double live_play_ms = 0.0;
  double live_play_most = 0.0;
  bool live_quiet = true;  // no frame came for 1 s

  // THE INPUT CHANNEL (input_live=1, docs/android.md, "The input channel").
  InputLive input_live;

  // WINE (docs/android.md, "Wine in the app"): a thread sets it up.
  WineRuntime wine;

  // THE GAME ("Play", docs/android.md, "Play in the app").
  bool play_game = true;         // play_game
  int play_menu_hold_ms = 1000;  // play_menu_hold_ms
  bool play_menu_press = true;   // play_menu_press
  bool play_notice = true;       // play_notice
  bool exit_at_end = true;       // exit_at_end
  play_rules::MenuButtonRule menu_rule;  // the menu button of this game
  play_rules::MenuStep menu_step;        // of this frame of the session
  bool menu_mod_said = false;  // the log has the press that goes to the mod now
  bool can_play = false;  // the APK has Wine for this ABI, wine_setup=1, play_game=1
  bool game_on = false;   // a game starts or runs
  bool menu_over_game = false;  // the menu shows over the game
  double play_ms = 0.0;    // the time of the last Play
  std::string play_clock;  // and its time of the day
  bool first_frame = false;  // a frame of the game came after Play
  // THE SCREEN QUAD (stream_screen=1): the screen pictures of the stream that
  // went to the quad.
  uint64_t screen_pictures = 0;
  bool said_no_screen = false;
  int live_space = -1;  // the last frame of the stream: 1 a screen frame, 0 a frame of the scene
  // THE KEYBOARD OF THE APP (screen_keyboard=1): it shows while the newest
  // screen picture has a dialog box of the game that takes text.
  bool keyboard_on = true;     // screen_keyboard
  bool panel_keyboard_on = true;  // panel_keyboard: also at the menu panel quad
  bool keyboard_shown = false;
  bool keyboard_on_panel = false;  // the keyboard shows at the menu panel quad
  keyboard_layout::Quad keyboard_quad = {};
  int keyboard_lit = -1;       // the key under the ray
  uint64_t keys_typed = 0;
  uint64_t rate_frame = 0;   // the frame and the time of the last line of the frame rate
  double rate_ms = 0.0;
};

// The renderer of the mod takes one device in a process (see android_main).
bool g_renderer_ran = false;

double NowMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

// The last eye picture of the renderer (VkSceneEyes) in the session: in the
// projection layer with the values of the mod for its frame (kView,
// stream_projection=1), else on the stereo screen. A new picture in the same
// image gets a new revision, so the swapchains copy it.
bool ShowEyes(App* app) {
  if (!app->xr_on) return false;
  uint64_t image = 0;
  uint32_t w = 0, h = 0;
  if (!VkSceneEyes(&image, &w, &h)) return false;
  if (!app->xr.HasScreen() && !app->xr.MakeScreen(w / 2, h / 2)) return false;
  // The values of the projection layer of the frame of this picture. With
  // no swapchains of the projection layer (MakeEyes failed one time) the
  // picture shows on the stereo screen.
  scene_stream::ViewBody view = {};
  const bool have_view = StreamPictureView(&view);
  const bool has = app->xr.ProjectionOn() && have_view &&
                   view_layer_math::ShowsProjection(view);
  // THE QUAD LAYERS of the frame of this picture (stream_layers=1). A frame
  // of the cut scene window as a screen shows on the swapchains of the eyes
  // too.
  static const quad_layer_math::FrameLayers kNoLayers;
  const quad_layer_math::FrameLayers& layers =
      app->xr.LayersOn() ? StreamPictureLayers() : kNoLayers;
  const bool window = app->xr.ProjectionOn() && quad_layer_math::AsScreen(layers);
  if ((has || window) && !app->xr.HasEyes()) app->xr.MakeEyes(w, h);
  if (has && (view.width != w || view.height != h) && !app->said_view_size) {
    app->said_view_size = true;
    AppLog("xr           : the mod gives the angles of view for pictures of %u by "
           "%u, and the picture of the renderer has %u by %u",
           view.width, view.height, w, h);
  }
  const VkImage now = reinterpret_cast<VkImage>(image);
  if (now != app->screen_image) {
    app->xr.SetScreenSource(now, w, h);
    app->screen_image = now;
  } else {
    app->xr.MarkScreenChanged();
  }
  app->xr.SetScreenView(has, view);
  // The panel quads show the panel image of the renderer of this frame.
  uint64_t panel = 0;
  if (layers.panel && !VkScenePanelFor(w, h, &panel)) panel = 0;
  if (layers.panel && panel == 0 && !app->said_no_panel) {
    app->said_no_panel = true;
    AppLog("xr           : the mod gives panel quads, but the renderer has no panel "
           "image of %u by %u for the picture. The panel quads do not show", w, h);
  }
  app->xr.SetScreenLayers(layers, StreamCardPictures(), reinterpret_cast<VkImage>(panel), w,
                          h);
  return true;
}

// THE SCREEN QUAD (stream_screen=1, docs/android.md, "The screens with no
// scene"): the newest screen picture of the stream on the quad of its kScreen,
// the picture of the last walk of the renderer or the pixels of the CPU of
// the render target. False when the renderer has no picture for it.
bool ShowScreen(App* app) {
  if (!app->xr_on) return false;
  const screen_quad_math::ScreenPicture& s = StreamScreen();
  if (s.Pictures() == app->screen_pictures) return true;
  app->screen_pictures = s.Pictures();
  const scene_stream::ScreenBody& b = s.Get();
  if ((b.flags & scene_stream::kScreenCpu) != 0) {
    const std::vector<uint8_t>* rgba = nullptr;
    uint32_t w = 0, h = 0;
    if (StreamSurfacePixels(b.surface, &rgba, &w, &h)) {
      app->xr.SetScreenQuadPixels(b, *rgba, w, h);
      return true;
    }
  } else {
    uint64_t image = 0;
    uint32_t layer = 0, w = 0, h = 0;
    if (VkSceneLastPicture(&image, &layer, &w, &h)) {
      app->xr.SetScreenQuadImage(b, reinterpret_cast<VkImage>(image), layer, w, h);
      return true;
    }
  }
  if (!app->said_no_screen) {
    app->said_no_screen = true;
    AppLog("xr           : the mod gives screen %llu of a frame with no scene, but the "
           "renderer has no picture for it (%s)",
           static_cast<unsigned long long>(b.count),
           (b.flags & scene_stream::kScreenCpu) != 0 ? "no pixels of the render target"
                                                     : "no picture of a walk");
  }
  return false;
}

// The newest picture of the stream: a screen on the screen quad, or the
// world in the projection layer, on the stereo screen or in the cut scene
// window (ShowEyes).
bool ShowPicture(App* app) {
  namespace sq = screen_quad_math;
  const sq::Last last = StreamScreen().LastPicture();
  if (last == sq::Last::kScreen && app->xr.ScreenQuadOn()) return ShowScreen(app);
  if (last == sq::Last::kWorld) app->xr.ClearScreenQuad();
  return ShowEyes(app);
}

void LiveSummary(App* app, const char* why) {
  const PlayResult& r = app->live_result;
  AppLog("live         : %s: %llu frames played and %llu skipped (frames %llu to "
         "%llu), %llu pictures. In all %llu records, %llu walks, %llu draws, "
         "%llu surfaces, %llu of an unknown kind%s. The play of a picture took "
         "%.1f ms as a mean and %.1f ms at most. The queue held %zu bytes at "
         "most (the limit is %zu bytes, 0 is none)",
         why, static_cast<unsigned long long>(app->live_frames),
         static_cast<unsigned long long>(app->live_skipped),
         static_cast<unsigned long long>(app->live_first),
         static_cast<unsigned long long>(app->live_last),
         static_cast<unsigned long long>(app->live_pictures),
         static_cast<unsigned long long>(r.records),
         static_cast<unsigned long long>(r.walks),
         static_cast<unsigned long long>(r.draws),
         static_cast<unsigned long long>(r.surfaces),
         static_cast<unsigned long long>(r.unknown),
         r.broken ? ", a frame stops in a record" : "",
         app->live_pictures > 0 ? app->live_play_ms / app->live_pictures : 0.0,
         app->live_play_most, app->live_reader.MostBytes(),
         app->live_reader.LimitBytes());
}

void ShowNotice(App* app);

// While a game runs: the first frame of the game after Play, and the frame
// rate of the game each 10 s, from the frame numbers of the mod in the live
// stream. With vk_device=0 nothing paces the frames of the game.
void GameFrames(App* app) {
  if (!app->game_on) return;
  const double now = NowMs();
  if (!app->first_frame) {
    app->first_frame = true;
    app->rate_frame = app->live_last;
    app->rate_ms = now;
    AppLog("game         : the first frame of the game (frame %llu%s) came %.1f s after Play",
           static_cast<unsigned long long>(app->live_last),
           screen_quad_math::IsScreenFrame(app->live_last) ? ", a screen with no scene" : "",
           (now - app->play_ms) / 1000.0);
    app->status = "The game runs (started " + app->play_clock + ").";
    ShowNotice(app);
    return;
  }
  // The screen frames have numbers of their own (screen_quad_math.h), so the
  // rate starts again when the frames of the scene start or stop.
  if (screen_quad_math::IsScreenFrame(app->rate_frame) !=
      screen_quad_math::IsScreenFrame(app->live_last)) {
    AppLog("game         : %s at frame %llu, %.1f s after Play",
           screen_quad_math::IsScreenFrame(app->live_last)
               ? "the screens with no scene start"
               : "the frames of the scene start",
           static_cast<unsigned long long>(app->live_last), (now - app->play_ms) / 1000.0);
    app->rate_frame = app->live_last;
    app->rate_ms = now;
    return;
  }
  if (now - app->rate_ms < 10000.0) return;
  AppLog("game         : frame %llu, %.1f frames a second in the last %.1f s, %llu pictures "
         "of the app since frames came",
         static_cast<unsigned long long>(app->live_last),
         play_rules::FramesPerSecond(app->rate_frame, app->rate_ms, app->live_last, now),
         (now - app->rate_ms) / 1000.0, static_cast<unsigned long long>(app->live_pictures));
  app->rate_frame = app->live_last;
  app->rate_ms = now;
}

// The frames of the live stream into the renderer, at each frame of the
// session. The renderer is not thread safe, so only this thread plays. The
// batch is each frame that waits. With stream_live_frames=N (N > 0) the app
// plays the walks of the newest frame and of each frame with a picture of a
// test, and only the records of state of the other frames (stream_split.h).
// With 0 it plays each frame that changes a surface or the options, each
// frame with a picture of a test and the newest frame (stream_ring.h,
// PlayInBatch). The stereo screen gets one new picture.
void PlayLive(App* app) {
  if (!app->live_reader.TakeAll(&app->live_batch)) {
    if (!app->live_quiet && NowMs() - app->live_last_ms > 1000.0) {
      LiveSummary(app, "no new frame for 1 s");
      app->live_quiet = true;
    }
    return;
  }
  const double start = NowMs();
  const size_t n = app->live_batch.size();
  for (size_t i = 0; i < n; ++i) {
    QueuedFrame& q = app->live_batch[i];
    const std::vector<uint8_t>& f = q.bytes;
    const stream_ring::UnitInfo u = stream_ring::Inspect(f.data(), f.size());
    if (u.start) app->live_test_frame = u.test_frame;
    if (app->live_quiet) {
      AppLog("live         : frames come. The first is frame %llu%s",
             static_cast<unsigned long long>(u.frame),
             u.start ? ", with the whole state" : "");
      app->live_quiet = false;
      app->live_frames = app->live_skipped = app->live_pictures = 0;
      app->live_first = u.frame;
      app->live_play_ms = app->live_play_most = 0.0;
    }
    app->live_last = u.frame;
    // The screen frames have numbers of their own (screen_quad_math.h): one
    // line when the frames go from the screens to the scene or back.
    const int space = screen_quad_math::IsScreenFrame(u.frame) ? 1 : 0;
    if (app->live_space >= 0 && space != app->live_space) {
      AppLog("live         : %s at frame %llu",
             space != 0 ? "the screens with no scene start" : "the frames of the scene start",
             static_cast<unsigned long long>(u.frame));
    }
    app->live_space = space;
    if (app->live_whole == 0) {
      if (!stream_ring::PlayInBatch(u, i + 1 == n, app->live_test_frame)) {
        ++app->live_skipped;
        continue;
      }
      PlayStreamRecords(f.data(), f.size(), &app->live_result);
      ++app->live_frames;
      continue;
    }
    if (q.whole && stream_split::KeepWalks(i, n, 1, q.picture)) {
      PlayStreamRecords(f.data(), f.size(), &app->live_result);
      ++app->live_frames;
      continue;
    }
    // A frame that a newer frame replaces: its records of state, no walks.
    if (q.whole) {
      stream_split::StateOnly(f.data(), f.size(), &app->live_state);
      PlayStreamRecords(app->live_state.data(), app->live_state.size(), &app->live_result);
    } else {
      PlayStreamRecords(f.data(), f.size(), &app->live_result);
    }
    ++app->live_skipped;
  }
  const double ms = NowMs() - start;
  app->live_play_ms += ms;
  if (ms > app->live_play_most) app->live_play_most = ms;
  ++app->live_pictures;
  app->live_last_ms = NowMs();
  ShowPicture(app);
  GameFrames(app);
}

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

// THE RENDERER OF THE MOD on the device of the session. It takes one device
// in a process. False when it ran in this process already. what: the log
// name of the stream.
bool StartRenderer(App* app, const char* what) {
  if (g_renderer_ran) {
    AppLog("%s : the renderer ran in this process already, and it takes no "
           "second device. Start the app again", what);
    return false;
  }
  g_renderer_ran = true;
  AppLog("xr           : stream_projection=%d. %s", app->xr.ProjectionOn() ? 1 : 0,
         app->xr.ProjectionOn()
             ? "A picture of the renderer with the values of the projection "
               "layer of the mod shows in a projection layer, a picture with "
               "none on the stereo screen"
             : "Each picture of the renderer shows on the stereo screen");
  AppLog("xr           : stream_layers=%d. %s", app->xr.LayersOn() ? 1 : 0,
         app->xr.LayersOn()
             ? "The cards, the panel quads and the cut scene window of the mod "
               "show as quad layers, as the session of the mod shows them"
             : "No quad layer of the mod");
  AppLog("xr           : stream_screen=%d. %s", app->xr.ScreenQuadOn() ? 1 : 0,
         app->xr.ScreenQuadOn()
             ? "A frame with no scene of the mod shows on a quad fixed in the room, "
               "with the screen of the PC"
             : "No screen quad: a frame with no scene of the mod does not show");
  if (app->xr_on && app->xr.Renderer13()) {
    vk::External device;
    app->xr.RendererDevice(&device);
    vk::UseExternal(device);
  }
  return true;
}

// THE LIVE STREAM (stream_live=1): a thread reads the frames of the walks of
// the mod from the ring bwvr-stream.ring of the game folder, and at each
// step the loop gives the frames that wait to the renderer (PlayLive,
// docs/android.md, "The live stream"). stream_live_feed=1 (a test aid): a
// child process writes the frames of bwvr-stream-feed.bin into the ring, as
// the mod does, one each stream_live_feed_ms.
void StartLiveReader(App* app) {
  app->live = true;
  const std::string ring = app->game.folder + "/bwvr-stream.ring";
  AppLog("live         : stream_live=1. The app reads the frames of the walks "
         "from %s", ring.c_str());
  VkPlatformSetFolder(app->game.folder.c_str());
  if (app->ini.GetInt("bwvr", "stream_live_feed", 0) != 0) {
    app->live_reader.StartFeed(ring, app->game.folder + "/bwvr-stream-feed.bin",
                               app->ini.GetInt("bwvr", "stream_live_feed_ms", 0));
  }
  // stream_live_frames=N (3 by default): the newest N frames of the queue
  // stay whole, and each older frame keeps only its records of state. 0: up
  // to 64 MB of whole frames wait for the session.
  const int whole = app->ini.GetInt("bwvr", "stream_live_frames", 3);
  app->live_whole = whole > 0 ? static_cast<size_t>(whole) : 0;
  AppLog("live         : stream_live_frames=%d. %s", whole,
         app->live_whole > 0 ? "The newest frames of the queue stay whole, and "
                               "an older frame keeps only its records of state"
                             : "Up to 64 MB of whole frames wait for the session");
  // stream_live_queue_mb (64 by default): with stream_live_frames > 0 a
  // queue that passes this limit is cleared, for example while the session
  // does not run, and the app asks again for the whole state. 0: no limit.
  const int queue_mb = app->ini.GetInt("bwvr", "stream_live_queue_mb", 64);
  const size_t queue_bytes =
      app->live_whole > 0 ? static_cast<size_t>(queue_mb > 0 ? queue_mb : 0) << 20
                          : static_cast<size_t>(64u << 20);
  app->live_reader.Start(ring, app->live_whole, queue_bytes);
}

std::string ClockNow() {
  const time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  return play_rules::Clock(local.tm_hour, local.tm_min, local.tm_sec);
}

// THE NOTICE while the game starts (play_notice=1): from Play to the first
// frame of the game the quad of the menu shows a notice in place of the
// rows, because the headset shows nothing of the game before its first
// frame. The menu over the game takes the place of the notice.
void ShowNotice(App* app) {
  const bool notice = app->play_notice && app->game_on && !app->first_frame &&
                      !app->menu_over_game;
  if (notice) {
    const int ms = app->play_menu_hold_ms;
    char text[320];
    if (ms > 0) {
      snprintf(text, sizeof(text),
               "The game starts in Wine. The picture of the game comes with its first "
               "frame. Hold the left menu button for %.1f s to show the menu with "
               "\"Quit game\".", ms / 1000.0);
    } else {
      snprintf(text, sizeof(text),
               "The game starts in Wine. The picture of the game comes with its first "
               "frame. play_menu_hold_ms=0: the menu comes back when the game ends.");
    }
    app->menu.SetNotice(text);
  } else {
    app->menu.SetNotice("");
  }
  app->xr.SetMenuShown(app->menu_over_game || notice || !app->game_on);
}

// The menu over the game, or the game with no menu.
void ShowMenuOverGame(App* app, bool on) {
  if (app->menu_over_game == on) return;
  app->menu_over_game = on;
  ShowNotice(app);
  app->dragging = false;
  AppLog("game         : %s", on ? "the menu shows over the game. The mod gets no hand"
                                 : "the menu goes. The game has the hands again");
}

// "Play in VR" and "Play, new game" (docs/android.md, "Play in the app"). As
// the launcher, each start saves the settings first. Then the keys of the
// run, the live stream and the input channel in this process, and the game
// in Wine.
void Play(App* app, bool new_game) {
  const char* name = new_game ? "Play, new game" : "Play in VR";
  Save(app);
  std::string why;
  if (!app->game.exe_right) {
    why = app->game.message;
  } else if (!app->play_game) {
    why = "Saved. play_game=0: the app does not start the game.";
  } else if (!app->can_play) {
    why = "Saved. The app has no Wine, so it cannot start the game.";
  } else if (!app->wine.Ready()) {
    const std::string wine = app->wine.Status();
    why = "Saved. Wine is not ready yet" + (wine.empty() ? std::string(".") : ": " + wine);
  } else if (!app->xr_on) {
    why = "Saved. The game needs a headset: the app has no OpenXR session.";
  } else if (!app->xr.Renderer13()) {
    why = "Saved. The renderer of the mod needs Vulkan 1.3, and this headset has none.";
  } else if (!app->live) {
    // The reader starts with the renderer, so a fault of a later step does
    // not block the next Play: the renderer takes one device in a process.
    if (StartRenderer(app, "game        ")) {
      StartLiveReader(app);
    } else {
      why = "Saved. The renderer ran in this start of the app. Start the app again.";
    }
  }
  if (!why.empty()) {
    app->status = why;
    AppLog("menu         : %s. %s", name, why.c_str());
    return;
  }
  // The keys of the run, after the save: a save deletes the retired keys.
  play_rules::SetRunKeys(&app->ini);
  if (!WriteTextFile(app->game.ini_path, app->ini.Text())) {
    app->status = "Cannot write bwvr.ini in the game folder.";
    AppLog("menu         : %s. Cannot write the keys of the run to %s", name,
           app->game.ini_path.c_str());
    return;
  }
  std::string keys;
  for (const play_rules::RunKey& k : play_rules::kRunKeys) {
    keys += std::string(keys.empty() ? "" : ", ") + k.key + "=" + std::to_string(k.value);
  }
  AppLog("menu         : %s. bwvr.ini has the keys of the run: %s", name, keys.c_str());
  if (!app->input_live.On()) app->input_live.Start(app->game.folder + "/bwvr-input.chan", 0);
  WineRuntime::Game game;
  game.new_game = new_game;
  app->menu.RenderSize(&game.width, &game.height);
  if (!app->wine.StartGame(game)) {
    app->status = "Saved. A game runs already.";
    AppLog("menu         : %s. A game runs already", name);
    return;
  }
  app->play_ms = NowMs();
  app->play_clock = ClockNow();
  app->first_frame = false;
  app->game_on = true;
  app->menu.SetGameRunning(true);
  app->menu_over_game = true;
  ShowMenuOverGame(app, false);
  app->status = "Starting the game (" + app->play_clock + ")...";
  if (app->play_notice) AppLog("game         : play_notice=1. The quad of the menu shows a notice until the first frame of the game");
  AppLog("game         : %s at %s, the render %d by %d", name, app->play_clock.c_str(),
         game.width, game.height);
}

// THE MENU BUTTON at each frame of the session while the game runs
// (play_rules.h, StepMenuButton): a hold shows the menu over the game, a
// press hides it, and the step has the menu bits for the slot of the mod.
void MenuButton(App* app, const XrInput& in) {
  if (!app->game_on) {
    app->menu_rule = play_rules::MenuButtonRule();
    app->menu_step = play_rules::MenuStep();
    return;
  }
  app->menu_step = play_rules::StepMenuButton(&app->menu_rule, in.menu, NowMs(),
                                              app->menu_over_game, app->play_menu_hold_ms,
                                              app->play_menu_press);
  if (app->menu_step.action == play_rules::MenuAction::kShow) {
    ShowMenuOverGame(app, true);
  } else if (app->menu_step.action == play_rules::MenuAction::kHide) {
    ShowMenuOverGame(app, false);
  }
  if (app->menu_step.mod_menu[0] || app->menu_step.mod_menu[1]) {
    if (!app->menu_mod_said) {
      app->menu_mod_said = true;
      AppLog("game         : a press of the menu button goes to the mod %s",
             app->play_menu_press && app->play_menu_hold_ms > 0
                 ? "at its release (play_menu_press=1)"
                 : "at once");
    }
  } else {
    app->menu_mod_said = false;
  }
}

// At each step: the end of the game brings the menu back, with the exit
// code in the status line.
void GameTick(App* app) {
  if (!app->game_on) return;
  int code = 0;
  std::string why;
  if (app->wine.State(&code, &why) != WineRuntime::GameState::kEnded) return;
  app->game_on = false;
  app->menu.SetGameRunning(false);
  ShowMenuOverGame(app, true);
  // The last picture of the game goes out of the layers.
  app->xr.ClearScreenSource();
  app->xr.ClearScreenQuad();
  app->screen_image = VK_NULL_HANDLE;
  const std::string clock = ClockNow();
  app->status = "The game ended with the exit code " + std::to_string(code) + " (" + clock +
                ", " + why + ").";
  AppLog("game         : the game ended with the exit code %d at %s (%s), %.1f s after Play",
         code, clock.c_str(), why.c_str(), (NowMs() - app->play_ms) / 1000.0);
}

void Do(App* app, Action action) {
  switch (action) {
    case Action::kSave:
      Save(app);
      break;
    case Action::kPlay:
    case Action::kNewGame:
      Play(app, action == Action::kNewGame);
      break;
    case Action::kQuit:
      app->wine.StopGame();
      app->status = "Quitting the game...";
      AppLog("game         : Quit game");
      break;
    case Action::kExit:
      // The end of the activity ends android_main, which stops the game and
      // Wine (and, with exit_at_end=1, the process).
      AppLog("menu         : Exit%s", app->game_on ? ". The game ends" : "");
      app->status = "Exit...";
      ANativeActivity_finish(app->android->activity);
      break;
    case Action::kNone:
      break;
  }
}

// The menu goes to the paint thread after each change, and the pixels of a
// paint that is done go to the quad or to the window.
void Paint(App* app) {
  // The status of the setup of Wine follows the status of the game folder.
  std::string status = app->status;
  if (status.empty()) {
    const std::string wine = app->wine.Status();
    status = wine.empty() ? app->base_status : app->base_status + "  " + wine;
  }
  app->painter.Ask(app->menu, status);
  if (app->xr_on && app->keyboard_shown) app->painter.AskKeyboard(app->keyboard_lit);
  {
    std::vector<uint8_t> keys;
    if (app->painter.TakeKeyboard(&keys)) {
      app->xr.SetKeyboardPixels(keys, keyboard_layout::kWidth, keyboard_layout::kHeight);
    }
  }
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
  // While the game runs: the menu key shows or hides the menu over the game,
  // and Back shows it. With no menu the other keys have no job in the app.
  // play_menu_hold_ms=0: no menu over the game, and the app takes Back.
  if (app->game_on) {
    const bool can_show = app->play_menu_hold_ms > 0;
    if (code == AKEYCODE_MENU) {
      if (can_show) ShowMenuOverGame(app, !app->menu_over_game);
      return true;
    }
    if (!app->menu_over_game) {
      const bool back = code == AKEYCODE_BACK || code == AKEYCODE_ESCAPE;
      if (back && can_show) ShowMenuOverGame(app, true);
      return back;
    }
    if ((code == AKEYCODE_BACK || code == AKEYCODE_ESCAPE || code == AKEYCODE_BUTTON_B) &&
        m.OnMain()) {
      ShowMenuOverGame(app, false);
      return true;
    }
  }
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
  if (app->game_on && !app->menu_over_game) return;
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
      return code == AKEYCODE_BACK && app->menu.OnMain() && !app->game_on ? 0 : 1;
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

// THE KEYBOARD OF THE APP (screen_keyboard=1, docs/android.md, "The keyboard
// of the app"): while the game runs with no menu over it, and the newest
// screen picture has a dialog box of the game that takes text (kScreenText
// of its kScreen), the keyboard shows on a quad under the screen quad. In a
// frame of the scene (panel_keyboard=1) the menu panel quad of the mod asks
// for it (kPanelQuadText of its kPanel), and the keyboard shows in front of
// the lower part of that quad (keyboard_layout.h, QuadOnPanel).
void UpdateKeyboard(App* app) {
  bool shown = false;
  bool on_panel = false;
  float panel_position[3], panel_orientation[4], panel_size[2];
  if (app->keyboard_on && app->game_on && !app->menu_over_game &&
      app->xr.ScreenQuadShown()) {
    const scene_stream::ScreenBody& b = app->xr.ScreenQuadBody();
    shown = (b.flags & scene_stream::kScreenText) != 0 &&
            keyboard_layout::QuadUnder(b.position, b.size, &app->keyboard_quad);
  } else if (app->panel_keyboard_on && app->game_on && !app->menu_over_game &&
             app->xr.PanelTextQuad(panel_position, panel_orientation, panel_size)) {
    shown = keyboard_layout::QuadOnPanel(panel_position, panel_orientation, panel_size,
                                         &app->keyboard_quad);
    on_panel = shown;
  }
  if (shown != app->keyboard_shown || on_panel != app->keyboard_on_panel) {
    app->keyboard_shown = shown;
    app->keyboard_on_panel = on_panel;
    app->keyboard_lit = -1;
    const keyboard_layout::Quad& q = app->keyboard_quad;
    if (shown) {
      AppLog("keyboard     : a dialog box of the game takes text. The keyboard shows %s, "
             "%.2f by %.2f m at %.2f %.2f %.2f m, turned %.1f degrees to the head",
             on_panel ? "at the menu panel quad" : "under the screen quad", q.size[0],
             q.size[1], q.position[0], q.position[1], q.position[2],
             -2.0f * asinf(q.orientation[0]) * 57.29578f);
    } else {
      AppLog("keyboard     : the keyboard goes. %llu keys went to the mod",
             static_cast<unsigned long long>(app->keys_typed));
    }
  }
  app->xr.SetKeyboard(shown, app->keyboard_quad.position, app->keyboard_quad.orientation,
                      app->keyboard_quad.size);
}

// The ray and the trigger on the keyboard. The hand that pulled its trigger
// last points, as on the menu. The trigger types the key under the ray: the
// key goes to the mod through the ring of the keys of the input channel. The
// mod takes the trigger only on the screen quad or on a panel quad, and the
// keyboard at the menu panel quad stops its ray, so a press on the keyboard
// clicks nothing in the game.
void OnKeyboard(App* app, const XrInput& in, bool* show_cursor, float cursor[3]) {
  for (int h = 0; h < 2; ++h) {
    if (in.select[h] && !app->select_was[h]) app->pointer_hand = h;
  }
  const int p = app->pointer_hand;
  float px = 0.0f, py = 0.0f;
  bool hit = false;
  if (in.aim_valid[p]) {
    float forward[3];
    controller_math::AimForward(in.aim_orientation[p], forward);
    hit = keyboard_layout::RayToPixel(in.aim_position[p], forward, app->keyboard_quad, &px,
                                      &py);
  }
  const int key = hit ? keyboard_layout::KeyIndexAt(px, py) : -1;
  if (key != app->keyboard_lit) {
    app->keyboard_lit = key;
    if (key >= 0) app->xr.Pulse(p);
  }
  if (in.select[p] && !app->select_was[p] && key >= 0) {
    keyboard_layout::Key k;
    keyboard_layout::Rect r;
    keyboard_layout::KeyAt(key, &k, &r);
    const bool in_ring = app->input_live.PushKey(k.code);
    if (in_ring) ++app->keys_typed;
    if (app->keys_typed <= 64 || !in_ring) {
      AppLog("keyboard     : the key %s %s", k.label,
             in_ring ? "goes to the mod" : "does not fit into the ring of the keys");
    }
  }
  *show_cursor = hit;
  if (hit) keyboard_layout::PixelToPoint(app->keyboard_quad, px, py, cursor);
}

// THE CONTROLLERS on the menu quad.
void OnControllers(App* app, const XrInput& in, bool* show_cursor,
                   float cursor[3]) {
  Menu& m = app->menu;
  const ml::Quad quad = ml::MenuQuad();
  // While the game runs, the menu button shows and hides the menu over the
  // game (MenuButton, before this call). With no menu the controllers are
  // the hands of the game, and the menu takes no input.
  if (app->game_on && !app->menu_over_game) {
    *show_cursor = false;
    if (app->keyboard_shown) OnKeyboard(app, in, show_cursor, cursor);
    for (int h = 0; h < 2; ++h) {
      app->select_was[h] = in.select[h];
      app->back_was[h] = in.back[h];
    }
    return;
  }
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
    if (!in.back[h] || app->back_was[h]) continue;
    // B or Y on the main page of the menu over the game goes back to the
    // game.
    if (app->game_on && m.OnMain()) {
      ShowMenuOverGame(app, false);
    } else {
      m.Back();
    }
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
  // WINE: the thread unpacks the tree of the APK into the internal folder of
  // the app, makes the prefix and, with wine_probe=1, runs the test program
  // of the game folder. wine_setup=0: no Wine, as an APK with no Wine tree.
  // wine_debug: the value of WINEDEBUG (empty: the default of Wine).
  // wine_audio=0: no audio driver in the prefix, so the game has no sound.
  // wine_audio_low_latency=0: the audio driver asks AAudio for the mode none.
  {
    WineRuntime::Options wine;
    wine.setup = app.ini.GetInt("bwvr", "wine_setup", 1) != 0;
    wine.probe = app.ini.GetInt("bwvr", "wine_probe", 0) != 0;
    wine.debug = app.ini.Get("bwvr", "wine_debug", "");
    wine.audio = app.ini.GetInt("bwvr", "wine_audio", 1) != 0;
    wine.low_latency = app.ini.GetInt("bwvr", "wine_audio_low_latency", 1) != 0;
    // The tree must be in the internal folder: Android runs no program of
    // the shared storage.
    if (android->activity->internalDataPath == nullptr) {
      AppLog("wine         : Android gives the app no internal folder. The app does not "
             "set up Wine");
    } else {
      const std::string files = android->activity->internalDataPath;
      mkdir(files.c_str(), 0700);
      app.wine.Start(android->activity->assetManager, files, app.folder, app.game.folder, wine);
    }
  }
  if (!app.painter.Start(android->activity->vm,
                         app.capture ? app.game.folder + "/bwvr-menu.bmp" : "")) {
    AppLog("app          : the canvas of Android did not start. The app stops");
    ANativeActivity_finish(android->activity);
  }

  app.xr.SetCopyOnChange(app.ini.GetInt("bwvr", "xr_copy_on_change", 1) != 0);
  // stream_projection=1 (the default): a picture of the renderer with the
  // values of the projection layer of the mod for its frame shows in a
  // projection layer. 0: each picture shows on the stereo screen.
  app.xr.SetProjection(app.ini.GetInt("bwvr", "stream_projection", 1) != 0);
  // stream_layers=1 (the default): the cards, the panel quads and the cut
  // scene window of the mod for the frame of a picture show as quad layers,
  // as the session of the mod shows them. 0: no quad layer of the mod.
  app.xr.SetLayersOn(app.ini.GetInt("bwvr", "stream_layers", 1) != 0);
  // stream_screen=1 (the default): a frame with no scene of the mod (the
  // logos, the videos, the front end, a dialog box) shows on a quad fixed in
  // the room, with the screen of the PC. 0: no screen quad, the app shows no
  // frame with no scene.
  app.xr.SetScreenQuadOn(app.ini.GetInt("bwvr", "stream_screen", 1) != 0);
  // input_live=1: the session also reads the input of the mod, and each frame
  // goes into bwvr-input.chan of the game folder. 0: the app reads only the
  // input of its menu.
  const bool input_live = app.ini.GetInt("bwvr", "input_live", 0) != 0;
  // play_game=1 (the default): "Play" starts the game in Wine, when the APK
  // has Wine for this ABI and wine_setup=1. Then the session has the actions
  // of the mod from its start, because a session takes its actions one
  // time, and "Play" starts the input channel. 0: "Play" saves and starts
  // nothing.
  app.play_game = app.ini.GetInt("bwvr", "play_game", 1) != 0;
  app.can_play = app.play_game && app.ini.GetInt("bwvr", "wine_setup", 1) != 0 &&
                 android->activity->internalDataPath != nullptr &&
                 WineRuntime::ApkHasTree(android->activity->assetManager);
  // play_menu_hold_ms=N (1000 by default): while the game runs, a hold of
  // the menu button of a controller for N ms (or the key Menu, or Back with
  // no headset) shows the menu of the app over the game. 0: no menu of the
  // app over the game, it comes back when the game ends.
  // play_menu_press=1 (the default): the app holds the menu bit back while
  // the button is down, a short press goes to the mod at its release, and a
  // hold or the press that hides the menu never reaches the mod. 0: the mod
  // gets each press at once (a hold opens the menu of the game too).
  app.play_menu_hold_ms = app.ini.GetInt("bwvr", "play_menu_hold_ms", 1000);
  if (app.play_menu_hold_ms < 0) app.play_menu_hold_ms = 0;
  // play_notice=1 (the default): from Play to the first frame of the game
  // the quad of the menu shows a notice with the way to the menu. 0: the
  // headset shows nothing until the first frame.
  app.play_notice = app.ini.GetInt("bwvr", "play_notice", 1) != 0;
  // exit_at_end=1 (the default): the process ends with android_main. A
  // headset keeps the process of an app when the app goes to the back or
  // the headset sleeps, and the next start of the app is a new android_main
  // in the same process. The renderer of the mod takes one device in a
  // process, so a new android_main in an old process cannot play. 0: the
  // process stays, as the system wants.
  app.exit_at_end = app.ini.GetInt("bwvr", "exit_at_end", 1) != 0;
  app.play_menu_press = app.ini.GetInt("bwvr", "play_menu_press", 1) != 0;
  // screen_keyboard=1 (the default): while a dialog box of the game takes
  // text, the app shows its keyboard under the screen quad, and the trigger
  // on a key gives the key to the mod. 0: no keyboard.
  app.keyboard_on = app.ini.GetInt("bwvr", "screen_keyboard", 1) != 0;
  AppLog("keyboard     : screen_keyboard=%d. %s", app.keyboard_on ? 1 : 0,
         app.keyboard_on ? "While a dialog box of the game takes text, the keyboard of the "
                           "app shows under the screen quad"
                         : "No keyboard of the app");
  // panel_keyboard=1 (the default): while a dialog box of the game in a frame
  // of the scene takes text (kPanelQuadText of the menu panel quad), the app
  // shows its keyboard in front of the lower part of that quad. 0: no
  // keyboard in a frame of the scene.
  app.panel_keyboard_on = app.ini.GetInt("bwvr", "panel_keyboard", 1) != 0;
  AppLog("keyboard     : panel_keyboard=%d. %s", app.panel_keyboard_on ? 1 : 0,
         app.panel_keyboard_on
             ? "While a dialog box of the game in a frame of the scene takes text, "
               "the keyboard of the app shows at the menu panel quad"
             : "No keyboard of the app in a frame of the scene");
  AppLog("game         : play_game=%d, play_menu_hold_ms=%d, play_menu_press=%d. %s",
         app.play_game ? 1 : 0, app.play_menu_hold_ms, app.play_menu_press ? 1 : 0,
         app.can_play ? "Play starts the game in Wine"
                      : "Play saves and does not start the game: the app has no Wine, or "
                        "play_game=0");
  app.xr.SetInputLive(input_live || app.can_play);
  app.xr_on = app.xr.Start(android->activity, ml::kWidth, ml::kHeight,
                           kCursorPixels);

  // The renderer of the mod (docs/android.md, "The renderer in the app"). It
  // plays one stream in a process: a file (stream_replay=1) or the live
  // stream (stream_live=1). In a session it draws on the device of the
  // session, and its last picture shows on a stereo screen behind the menu.
  // The renderer keeps the images and the pipelines of its first device to
  // the end of the process, so it takes one device in a process. The glue
  // code of Android can start android_main again in the same process.
  bool replay = app.ini.GetInt("bwvr", "stream_replay", 0) != 0;
  const bool live = app.ini.GetInt("bwvr", "stream_live", 0) != 0;
  if (replay && live) {
    AppLog("replay       : stream_live=1, so no replay of bwvr-stream.bin. The "
           "app plays one stream in a process");
    replay = false;
  }
  bool live_on = false;
  if (replay || live) {
    if (StartRenderer(&app, replay ? "replay      " : "live        ")) {
      live_on = live;
    } else {
      AppLog("%s : no %s", replay ? "replay      " : "live        ",
             replay ? "replay" : "live stream, no reader thread and no feed process");
      replay = false;
    }
  }
  // A TEST AID (stream_replay=1 in bwvr.ini): the stream of the walks in
  // bwvr-stream.bin of the game folder goes to the Vulkan renderer of the
  // mod, which writes the pictures of vk_scene_test_frame of the stream
  // into the game folder (docs/android.md, "The stream of the walks").
  if (replay) {
    PlayStreamFile(app.game.folder + "/bwvr-stream.bin", app.game.folder);
    if (app.xr_on && !ShowPicture(&app)) {
      AppLog("replay       : no picture of the renderer for the session");
    }
  }
  // THE LIVE STREAM (stream_live=1 in bwvr.ini, StartLiveReader). "Play"
  // starts it too.
  if (live_on) StartLiveReader(&app);
  if (input_live && app.xr_on) {
    const int test_pulse = app.ini.GetInt("bwvr", "input_live_test_pulse", 0);
    app.input_live.Start(app.game.folder + "/bwvr-input.chan",
                         test_pulse > 0 ? static_cast<uint32_t>(test_pulse) : 0);
  } else if (input_live) {
    AppLog("input        : input_live=1, but there is no OpenXR session, so "
           "the app writes no input");
  }
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
    const int timeout = (app.xr_on && app.xr.Running()) ? 0 : (app.live ? 5 : 50);
    int events = 0;
    android_poll_source* source = nullptr;
    while (ALooper_pollOnce(timeout, nullptr, &events,
                            reinterpret_cast<void**>(&source)) >= 0) {
      if (source != nullptr) source->process(android, source);
      if (android->destroyRequested) break;
      if (timeout != 0) break;
    }
    if (android->destroyRequested) break;
    GameTick(&app);

    if (app.xr_on) {
      bool quit = false;
      app.xr.PollEvents(&quit);
      if (quit) {
        ANativeActivity_finish(android->activity);
        break;
      }
      if (!app.xr.Running()) {
        app.input_live.WriteNoInput();
        continue;
      }
      XrInput input;
      if (!app.xr.BeginFrame(&input)) continue;
      MenuButton(&app, input);
      // The input of the mod: after xrSyncActions of BeginFrame, with the
      // views of xrLocateViews at the display time of this frame.
      if (app.input_live.On()) {
        input_channel::SlotData slot;
        app.xr.InputSlot(&slot);
        if (app.game_on) {
          play_rules::MaskInput(&slot, app.menu_over_game);
          if (!app.menu_over_game) play_rules::ApplyMenuBits(&slot, app.menu_step);
        }
        app.input_live.Write(&slot);
        app.input_live.PlayPulses(&app.xr);
      }
      if (app.live) PlayLive(&app);
      UpdateKeyboard(&app);
      bool show_cursor = false;
      float cursor[3] = {0, 0, 0};
      OnControllers(&app, input, &show_cursor, cursor);
      Paint(&app);
      app.xr.EndFrame(quad.center, menu_size, show_cursor, cursor,
                      kCursorMeters);
    } else {
      // With no session the loop runs each 5 ms for the live stream.
      if (app.live) PlayLive(&app);
      if (!app.window || !app.flat.Ready()) continue;
      Paint(&app);
      if (app.flat_dirty && app.flat.Present()) app.flat_dirty = false;
    }
  }

  if (app.live) {
    app.live_reader.Stop();
    if (!app.live_quiet) LiveSummary(&app, "the app ends");
  }
  app.input_live.WriteNoInput();
  app.input_live.Stop();
  if (app.game_on) AppLog("game         : the app ends, so the game ends");
  app.wine.Stop();
  AppLog("app          : the app ends");
  if (app.xr_on) app.xr.Stop();
  app.flat.Stop();
  app.painter.Stop();
  android->activity->vm->DetachCurrentThread();
  if (app.exit_at_end) {
    AppLog("app          : exit_at_end=1. The process ends, so the next start of the app "
           "is a new process");
    _exit(0);
  }
}
