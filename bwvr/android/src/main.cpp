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

#include <pthread.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

#include "../../src/common/version.h"
#include "../../src/mod/app_keys.h"
#include "../../src/mod/eye_copy_math.h"
#include "../../src/mod/late_camera_math.h"
#include "../../src/mod/vk_compose_math.h"
#include "app_log.h"
#include "build_id.h"
#include "display_rate.h"
#include "flat_view.h"
#include "frame_loop.h"
#include "game_folder.h"
#include "input_live.h"
#include "keyboard_layout.h"
#include "menu_layout.h"
#include "menu_model.h"
#include "menu_painter.h"
#include "picture_ring.h"
#include "play_rules.h"
#include "frame_defaults.h"
#include "pose_log.h"
#include "remote_control.h"
#include "steamvr_settings.h"
#include "../../src/mod/osc_gaze_math.h"
#include "../../src/mod/screen_quad_math.h"
#include "../../src/mod/stream_ring.h"
#include "../../src/mod/stream_split.h"
#include "../../src/mod/view_layer_math.h"
#include "../../src/mod/vk_platform_posix.h"
#include "../../src/mod/vk_scene.h"
#include "scene_player.h"
#include "stream_reader.h"
#include "thread_cores.h"
#include "time_log.h"
#include "long_frames.h"
#include "trace_app.h"
#include "walk_probe.h"
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

// THE PLAY OF THE LIVE STREAM (PlayBatch): the batch, the counts and the
// state of the play. The thread that plays the renderer has it: the thread
// of the session in the old loop, the play thread with frame_loop=1.
// THE LATE CAMERA (docs/android.md, "The late camera"): what each frame of the
// session gives the play thread.
struct LateTick {
  uint64_t serial = 0;       // one more for each tick, 0 with none
  double sent_ms = 0.0;      // the clock when the session sent it
  uint32_t slot = 0;         // the newest slot of the input channel
  int64_t period = 0;        // the display period, ns
  bool head_ok = false;
  float head[3] = {0.0f, 0.0f, 0.0f};  // the middle of the eyes at the next display time
  bool has_world = false;
  input_channel::World world = {};     // the newest place of the player of the mod
  int count = 0;
  motion_comp_math::SlotPush slots[64];  // the slots after the slot of that place
  late_camera_math::GrabFilter grab;     // the grips of the late grab at the next display time
};

struct LivePlay {
  PlayResult result;
  std::vector<QueuedFrame> batch;
  std::vector<uint8_t> state;
  int32_t test_frame = 0;  // vk_scene_test_frame of the last kStart
  uint64_t frames = 0;     // the frames played since frames came
  uint64_t skipped = 0;    // the frames with no walks: a newer frame replaced them
  uint64_t pictures = 0;   // the batches: one new picture each
  uint64_t first = 0;
  uint64_t last = 0;
  double last_ms = 0.0;  // the time of the last frame
  double play_ms = 0.0;
  double play_most = 0.0;
  bool quiet = true;  // no frame came for 1 s
  int space = -1;  // the last frame of the stream: 1 a screen frame, 0 a frame of the scene
  double play_start_ms = 0.0;  // the start of the play of the last batch
  double arrival_ms = 0.0;     // the arrival of the newest frame of the last batch (time_log=1)
  double gap_ms = 0.0;  // and its time from the frame of the game before (slow_frame_ms)
};

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
  size_t live_whole = 3;  // stream_live_frames
  LivePlay live_play;
  uint64_t shown_last = 0;  // the newest frame of the game of the shown picture
  double shown_play_ms = 0.0;  // the start of the play of the shown picture
  double shown_arrival_ms = 0.0;  // the arrival of its newest frame in the app
  // The other times of the shown picture for the parts of its age
  // (frame_loop::AgeTimes): its copies into the ring, its take by the session,
  // and the slot of the input of its frame.
  double shown_ready_ms = 0.0;
  double shown_taken_ms = 0.0;
  uint32_t shown_input_frame = 0;

  // THE FRAME LOOP (frame_loop=1, frame_loop.h): from Play to the end of the
  // game the play thread plays the batches into the ring of the pictures.
  PictureRing ring;
  pthread_t play_thread = {};
  bool play_running = false;
  std::atomic<bool> play_stop{false};
  std::mutex play_stats_lock;
  PlayStats play_stats;
  int gpu_depth = 1;  // frame_loop_gpu_depth, read at each Play
  bool compose_direct = false;  // xr_compose_direct, read at each Play and from the debug page
  // xr_compose_take. key: this game can do it, from each Play. compose_take: it is on now
  // (the same row of the debug page as xr_compose_direct).
  bool compose_take_key = false;
  bool compose_take = false;
  std::atomic<bool> release_take{true};  // xr_release_take, read at each Play and from the debug page
  std::atomic<bool> panel_keep{false};   // xr_panel_keep, from the debug page
  bool gpu_test = false;  // gpu_test, read at each Play
  // THE ONE PASS (xr_density_pass, read at each Play): the walks of both eyes draw
  // into a swapchain image of the slot of the picture. The size of the render of
  // this game, and a fault (the renderer took no view of an image) that sends the
  // rest of the game to the old way.
  bool density_pass = false;
  bool density_off = false;
  uint32_t render_w = 0, render_h = 0;
  std::atomic<bool> play_ok{false};  // the play thread runs (the late compose may be on)
  // THE LATE CAMERA (late_camera, read at each Play, docs/android.md, "The late
  // camera"). Each frame of the session gives the play thread a tick, and the play
  // thread draws the newest list of draws for the next display time. late_on: this
  // game can do it. late_live: the debug page has it on.
  bool late_on = false;
  std::atomic<bool> late_live{true};
  // THE LATE TAKE (late_take, late_take_ms): the longest wait of a frame of the
  // session for the picture of the tick before, the row of the debug page, and the
  // newest tick that the play thread ended its work for.
  int late_take_ms = 4;
  std::atomic<bool> late_take_live{false};
  std::atomic<bool> late_grab_live{false};     // the row "Late grab"
  std::atomic<bool> late_prepare_live{false};  // the row "Prepare early"
  std::atomic<bool> late_gap_live{false};      // the row "Prepare gap"
  late_camera_math::GrabFilter late_grab_filter;
  float late_grab_ms = 35.0f;                  // grab_smoothing_ms
  std::atomic<uint64_t> late_handled{0};
  uint64_t late_serial = 0;  // the tick of the picture that the play thread writes now
  // The counts of the line of the log, on the thread of the session.
  uint64_t take_all = 0, take_waits = 0, take_over = 0;
  double take_sum = 0.0, take_most = 0.0, take_log_at = 0.0;
  double take_wait_now = 0.0;  // the wait of this frame
  late_camera_math::Keys late_keys;
  std::mutex late_lock;
  std::condition_variable late_cond;
  LateTick late_tick;
  // The picture that the play thread writes now, for WriteSlot.
  bool late_again = false;   // a picture of the list again: its panel too
  bool late_moved = false;   // the eyes of the view go to the place of the new picture
  float late_head_move[3] = {0.0f, 0.0f, 0.0f};
  float late_move[3] = {0.0f, 0.0f, 0.0f};
  bool late_has_map = false;
  motion_comp_math::Map late_map = {};
  // The evenness of a walk, for the log: the map of the last tick, and the counts
  // of the steps.
  bool late_step_have = false;
  float late_step_at[3] = {0.0f, 0.0f, 0.0f};
  uint32_t late_step_slot = 0;
  uint64_t late_steps = 0, late_uneven = 0;
  double late_step_most = 0.0;
  // A TEST AID (pose_log, pose_log.h): one record for each frame of the session,
  // with the values of the shown picture.
  PoseLog pose_log;
  RecordLog<PictureRecord> picture_log;
  // A TEST AID (remote_control): the file of the test run, and the text for the player.
  RemoteControl remote;
  std::string remote_prompt;
  bool shown_again = false;
  uint64_t shown_tick = 0;
  double shown_work_ms = 0.0;
  float shown_move[3] = {0.0f, 0.0f, 0.0f};
  scene_stream::ViewBody shown_body = {};
  float shown_place[3] = {0.0f, 0.0f, 0.0f};
  // THE EVEN RATE (frame_loop_hold, read at each Play): the frames of the
  // session that showed the shown picture, and if a picture showed.
  int hold = 0;
  uint64_t hold_shown = 0;
  bool hold_any = false;

  // THE INPUT CHANNEL (input_live=1, docs/android.md, "The input channel").
  InputLive input_live;

  // WINE (docs/android.md, "Wine in the app"): a thread sets it up.
  WineRuntime wine;

  // THE GAME ("Play", docs/android.md, "Play in the app").
  bool play_game = true;         // play_game
  int play_menu_hold_ms = 1000;  // play_menu_hold_ms
  bool play_menu_press = true;   // play_menu_press
  bool play_menu_sticks = true;  // play_menu_sticks
  bool play_notice = true;       // play_notice
  bool exit_at_end = true;       // exit_at_end
  play_rules::MenuButtonRule menu_rule;  // the menu button of this game
  play_rules::MenuStep menu_step;        // of this frame of the session
  play_rules::StickChordRule stick_rule; // the two sticks of this game
  bool menu_mod_said = false;  // the log has the press that goes to the mod now
  bool pc_profile = true;        // pc_profile
  std::string prefix;            // the Wine prefix, empty with no internal folder
  std::string profile_said;      // the last line of the profile in the log
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
  // THE DISPLAY RATE (display_hz): the value of the row at the last request
  // to the runtime. 0: no request yet, or a request for no rate.
  int display_asked = 0;
  // The session waits for the rate of the last request, for the line of the log.
  bool display_waits = false;
  double display_asked_ms = 0.0;
  // THE TIME LOG (time_log=1, time_log.h).
  TimeLog time;
  // THE LONG FRAMES (slow_frame_ms, long_frames.h): the clock of the frames
  // of the game in the reader, the takes of the session, the time from the
  // frame of the game before the shown picture, and the display period of
  // the newest slot of the input channel (0 when not known).
  GameFrameClock frame_clock;
  LatchLog latch_log;
  double shown_gap_ms = 0.0;
  double period_ms = 0.0;
  // THE PHASE OF THE TAKE (take_phase_math.h): the line of the phase
  // (long_frames.h).
  TakePhase take_phase;
  // THE CORES OF THE HEADSET (thread_cores, thread_bind, thread_cores.h).
  ThreadCores cores;
};

// The renderer of the mod takes one device in a process (see android_main).
bool g_renderer_ran = false;

double NowMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

// THE PICTURE OF THE RENDERER after a batch: what the session shows. The old
// loop gathers it on the thread of the session and shows it at once
// (PlayLive). The play thread (frame_loop=1) gathers it into a slot of the
// ring of the pictures (WriteSlot). A screen picture is read only when its
// count is not screen_have, the count that the receiver has.
void GatherPicture(App* app, uint64_t screen_have, Picture* p) {
  namespace sq = screen_quad_math;
  static const quad_layer_math::FrameLayers kNoLayers;
  *p = Picture();
  const sq::ScreenPicture& s = StreamScreen();
  p->last = s.LastPicture();
  if (!app->xr_on) return;
  if (p->last == sq::Last::kScreen && app->xr.ScreenQuadOn()) {
    p->screen_pictures = s.Pictures();
    if (p->screen_pictures == screen_have) return;
    p->screen_body = s.Get();
    const scene_stream::ScreenBody& b = p->screen_body;
    if ((b.flags & scene_stream::kScreenCpu) != 0) {
      p->screen_cpu = true;
      p->screen_ok = StreamSurfacePixels(b.surface, &p->screen_rgba, &p->screen_width,
                                         &p->screen_height);
    } else {
      uint64_t image = 0;
      p->screen_ok = VkSceneLastPicture(&image, &p->screen_layer, &p->screen_width,
                                        &p->screen_height);
      p->screen_image = reinterpret_cast<VkImage>(image);
    }
    return;
  }
  uint64_t image = 0;
  p->eyes_ok = VkSceneEyes(&image, &p->width, &p->height);
  if (!p->eyes_ok) return;
  p->eyes = reinterpret_cast<VkImage>(image);
  VkSceneEyesAtlas(&p->atlas);
  // The values of the projection layer of the frame of this picture, and
  // the quad layers of that frame (stream_layers=1).
  p->have_view = StreamPictureView(&p->view);
  p->have_world = StreamPictureWorld(&p->world);
  p->layers = app->xr.LayersOn() ? &StreamPictureLayers() : &kNoLayers;
  p->cards = &StreamCardPictures();
  // The panel quads show the panel image of the renderer of this frame.
  uint64_t panel = 0;
  if (p->layers->panel && VkScenePanelFor(p->width, p->height, &panel)) {
    p->panel = reinterpret_cast<VkImage>(panel);
  } else if (p->layers->panel && app->late_again &&
             VkScenePanelAgain(p->width, p->height, &panel)) {
    p->panel = reinterpret_cast<VkImage>(panel);
  }
}

// The eye picture of a Picture in the session: in the projection layer with
// the values of the mod for its frame (kView, stream_projection=1), else on
// the stereo screen. A new picture in the same image gets a new revision, so
// the swapchains copy it.
bool ShowEyes(App* app, const Picture& p) {
  if (!app->xr_on) return false;
  if (!p.eyes_ok) return false;
  const uint32_t w = p.width, h = p.height;
  if (!app->xr.HasScreen() && !app->xr.MakeScreen(w / 2, h / 2)) return false;
  // With no swapchains of the projection layer (MakeEyes failed one time)
  // the picture shows on the stereo screen.
  const bool has = app->xr.ProjectionOn() && p.have_view &&
                   view_layer_math::ShowsProjection(p.view);
  // A frame of the cut scene window as a screen shows on the swapchains of
  // the eyes too.
  const quad_layer_math::FrameLayers& layers = *p.layers;
  const bool window = app->xr.ProjectionOn() && quad_layer_math::AsScreen(layers);
  // A picture in the swapchains of its slot (xr_eye_direct=1) needs no
  // swapchains of the projection layer.
  if ((has || window) && !app->xr.HasEyes() && p.direct_slot < 0) app->xr.MakeEyes(w, h);
  if (has && (p.view.width != w || p.view.height != h) && !app->said_view_size) {
    app->said_view_size = true;
    AppLog("xr           : the mod gives the angles of view for pictures of %u by "
           "%u, and the picture of the renderer has %u by %u",
           p.view.width, p.view.height, w, h);
  }
  if (p.compose_ticket != 0) {
    app->xr.SetComposeSource(p.compose_ticket, w, h);
    app->screen_image = VK_NULL_HANDLE;
  } else if (p.direct_slot >= 0) {
    app->xr.SetDirectSource(p.direct_slot);
    app->screen_image = VK_NULL_HANDLE;
  } else if (p.from_ring) {
    // Each slot is a new picture.
    app->xr.SetScreenSource(p.eyes, w, h);
    app->screen_image = VK_NULL_HANDLE;
  } else if (p.eyes != app->screen_image) {
    app->xr.SetScreenSource(p.eyes, w, h);
    app->screen_image = p.eyes;
  } else {
    app->xr.MarkScreenChanged();
  }
  app->xr.SetScreenView(has, p.view);
  app->xr.SetScreenWorld(has && p.have_world, p.world);
  app->xr.SetScreenAtlas(p.atlas);
  if (layers.panel && p.panel == VK_NULL_HANDLE && !app->said_no_panel) {
    app->said_no_panel = true;
    AppLog("xr           : the mod gives panel quads, but the renderer has no panel "
           "image of %u by %u for the picture. The panel quads do not show", w, h);
  }
  app->xr.SetScreenLayers(layers, *p.cards, p.panel, w, h, p.panel_part, p.panel_hash, p.panel_same);
  return true;
}

// THE SCREEN QUAD (stream_screen=1, docs/android.md, "The screens with no
// scene"): the newest screen picture of the stream on the quad of its kScreen,
// the picture of the last walk of the renderer or the pixels of the CPU of
// the render target. False when the renderer has no picture for it.
bool ShowScreen(App* app, const Picture& p) {
  if (!app->xr_on) return false;
  if (p.screen_pictures == app->screen_pictures) {
    // A slot of the ring holds the screen picture in an image of its own.
    if (p.from_ring && p.screen_ok && !p.screen_cpu) app->xr.MoveScreenQuadImage(p.screen_image);
    return true;
  }
  app->screen_pictures = p.screen_pictures;
  const scene_stream::ScreenBody& b = p.screen_body;
  if (p.screen_ok) {
    if (p.screen_cpu) {
      app->xr.SetScreenQuadPixels(b, *p.screen_rgba, p.screen_width, p.screen_height);
    } else {
      app->xr.SetScreenQuadImage(b, p.screen_image, p.screen_layer, p.screen_width,
                                 p.screen_height);
    }
    return true;
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
bool ShowPicture(App* app, const Picture& p) {
  namespace sq = screen_quad_math;
  if (p.last == sq::Last::kScreen && app->xr.ScreenQuadOn()) return ShowScreen(app, p);
  if (p.last == sq::Last::kWorld) app->xr.ClearScreenQuad();
  return ShowEyes(app, p);
}

void LiveSummary(App* app, const LivePlay& live, const char* why) {
  const PlayResult& r = live.result;
  AppLog("live         : %s: %llu frames played and %llu skipped (frames %llu to "
         "%llu), %llu pictures. In all %llu records, %llu walks, %llu draws, "
         "%llu surfaces, %llu of an unknown kind%s. The play of a picture took "
         "%.1f ms as a mean and %.1f ms at most. The queue held %zu bytes at "
         "most (the limit is %zu bytes, 0 is none)",
         why, static_cast<unsigned long long>(live.frames),
         static_cast<unsigned long long>(live.skipped),
         static_cast<unsigned long long>(live.first),
         static_cast<unsigned long long>(live.last),
         static_cast<unsigned long long>(live.pictures),
         static_cast<unsigned long long>(r.records),
         static_cast<unsigned long long>(r.walks),
         static_cast<unsigned long long>(r.draws),
         static_cast<unsigned long long>(r.surfaces),
         static_cast<unsigned long long>(r.unknown),
         r.broken ? ", a frame stops in a record" : "",
         live.pictures > 0 ? live.play_ms / live.pictures : 0.0,
         live.play_most, app->live_reader.MostBytes(),
         app->live_reader.LimitBytes());
}

void ShowNotice(App* app);

// While a game runs: the first frame of the game after Play, and the frame
// rate of the game each 10 s, from the frame numbers of the mod in the live
// stream (last, the newest frame of the shown picture). With vk_device=0
// nothing paces the frames of the game. pictures: the pictures of the play
// since frames came.
void GameFrames(App* app, uint64_t last, uint64_t pictures) {
  if (!app->game_on) return;
  const double now = NowMs();
  if (!app->first_frame) {
    app->first_frame = true;
    app->rate_frame = last;
    app->rate_ms = now;
    AppLog("game         : the first frame of the game (frame %llu%s) came %.1f s after Play",
           static_cast<unsigned long long>(last),
           screen_quad_math::IsScreenFrame(last) ? ", a screen with no scene" : "",
           (now - app->play_ms) / 1000.0);
    app->status = "The game runs (started " + app->play_clock + ").";
    ShowNotice(app);
    return;
  }
  // The screen frames have numbers of their own (screen_quad_math.h), so the
  // rate starts again when the frames of the scene start or stop.
  if (screen_quad_math::IsScreenFrame(app->rate_frame) !=
      screen_quad_math::IsScreenFrame(last)) {
    AppLog("game         : %s at frame %llu, %.1f s after Play",
           screen_quad_math::IsScreenFrame(last)
               ? "the screens with no scene start"
               : "the frames of the scene start",
           static_cast<unsigned long long>(last), (now - app->play_ms) / 1000.0);
    app->rate_frame = last;
    app->rate_ms = now;
    return;
  }
  if (now - app->rate_ms < 10000.0) return;
  AppLog("game         : frame %llu, %.1f frames a second in the last %.1f s, %llu pictures "
         "of the app since frames came",
         static_cast<unsigned long long>(last),
         play_rules::FramesPerSecond(app->rate_frame, app->rate_ms, last, now),
         (now - app->rate_ms) / 1000.0, static_cast<unsigned long long>(pictures));
  app->rate_frame = last;
  app->rate_ms = now;
}

// The frames of the batch of `live` into the renderer. The renderer is not
// thread safe, so one thread plays: the thread of the session (the old
// loop) or the play thread (frame_loop=1), never both. The batch is each
// frame that waits. With stream_live_frames=N (N > 0) the app plays the
// walks of the newest frame and of each frame with a picture of a test, and
// only the records of state of the other frames (stream_split.h). With 0 it
// plays each frame that changes a surface or the options, each frame with a
// picture of a test and the newest frame (stream_ring.h, PlayInBatch). The
// result is the ms of the play.
double PlayBatch(App* app, LivePlay* live) {
  const double start = NowMs();
  live->play_start_ms = start;
  live->arrival_ms = live->batch.empty() ? 0.0 : live->batch.back().arrival_ms;
  live->gap_ms = live->batch.empty() ? 0.0 : live->batch.back().gap_ms;
  const size_t n = live->batch.size();
  for (size_t i = 0; i < n; ++i) {
    QueuedFrame& q = live->batch[i];
    const std::vector<uint8_t>& f = q.bytes;
    const stream_ring::UnitInfo u = stream_ring::Inspect(f.data(), f.size());
    if (u.start) live->test_frame = u.test_frame;
    if (live->quiet) {
      AppLog("live         : frames come. The first is frame %llu%s",
             static_cast<unsigned long long>(u.frame),
             u.start ? ", with the whole state" : "");
      live->quiet = false;
      live->frames = live->skipped = live->pictures = 0;
      live->first = u.frame;
      live->play_ms = live->play_most = 0.0;
    }
    live->last = u.frame;
    // The screen frames have numbers of their own (screen_quad_math.h): one
    // line when the frames go from the screens to the scene or back.
    const int space = screen_quad_math::IsScreenFrame(u.frame) ? 1 : 0;
    if (live->space >= 0 && space != live->space) {
      AppLog("live         : %s at frame %llu",
             space != 0 ? "the screens with no scene start" : "the frames of the scene start",
             static_cast<unsigned long long>(u.frame));
    }
    live->space = space;
    if (app->live_whole == 0) {
      if (!stream_ring::PlayInBatch(u, i + 1 == n, live->test_frame)) {
        ++live->skipped;
        continue;
      }
      PlayStreamRecords(f.data(), f.size(), &live->result);
      ++live->frames;
      continue;
    }
    if (q.whole && stream_split::KeepWalks(i, n, 1, q.picture)) {
      PlayStreamRecords(f.data(), f.size(), &live->result);
      ++live->frames;
      continue;
    }
    // A frame that a newer frame replaces: its records of state, no walks.
    if (q.whole) {
      stream_split::StateOnly(f.data(), f.size(), &live->state);
      PlayStreamRecords(live->state.data(), live->state.size(), &live->result);
    } else {
      PlayStreamRecords(f.data(), f.size(), &live->result);
    }
    ++live->skipped;
  }
  const double ms = NowMs() - start;
  live->play_ms += ms;
  if (ms > live->play_most) live->play_most = ms;
  ++live->pictures;
  live->last_ms = NowMs();
  return ms;
}

// One line with the counts when no frame came for 1 s.
void QuietCheck(App* app, LivePlay* live) {
  if (!live->quiet && NowMs() - live->last_ms > 1000.0) {
    LiveSummary(app, *live, "no new frame for 1 s");
    live->quiet = true;
  }
}

// THE OLD LOOP (frame_loop=0, and with no game): at each frame of the
// session the thread of the session plays the batch and shows its picture.
// True when this frame has a new picture.
bool PlayLive(App* app) {
  LivePlay* live = &app->live_play;
  if (!app->live_reader.TakeAll(&live->batch)) {
    QuietCheck(app, live);
    return false;
  }
  // A renderer on the second queue (frame_loop_queues=2): the CPU waits for
  // the copies of the session before the walks and for the walks after
  // them, as the old loop waits for the GPU before each copy.
  app->xr.OldLoopSync(true);
  const double ms = PlayBatch(app, live);
  WalkProbeInstall();
  app->xr.OldLoopSync(false);
  if (app->game_on) {
    app->time.Batch(live->last_ms, live->batch.size(), ms, app->xr.LastWaitMs());
  }
  Picture p;
  GatherPicture(app, app->screen_pictures, &p);
  ShowPicture(app, p);
  app->shown_last = live->last;
  app->shown_play_ms = live->play_start_ms;
  app->shown_arrival_ms = live->arrival_ms;
  app->shown_ready_ms = live->last_ms;
  app->shown_taken_ms = NowMs();
  app->shown_input_frame = p.have_view ? p.view.input_frame : 0u;
  GameFrames(app, live->last, live->pictures);
  return true;
}

// THE FRAME LOOP (frame_loop=1, docs/android.md, "The frame loop").
//
// THE PLAY THREAD: the picture of the renderer after a batch into a slot of
// the ring of the pictures (picture_ring.h), with copies of the values that
// the session needs. A screen picture goes into the slot only when the slot
// does not hold it yet. The result is the ms that the play thread waited for
// the fence of the slot.
//
// THE ONE PASS (xr_density_pass=1): the play thread took the slot before the
// play (early, with the ms that it waited), and the walks of the batch drew
// into a swapchain image of the slot (density). The picture of the batch
// is in that image when the renderer says so (VkSceneDensityPicture).
double WriteSlot(App* app, const LivePlay& live, int early = -1, bool density = false,
                 double early_waited = 0.0) {
  namespace sq = screen_quad_math;
  double waited = early_waited;
  bool direct = false;
  const int i = early >= 0 ? early : app->ring.BeginWrite(&waited);
  if (i < 0) return waited;
  PictureRing::Slot& s = app->ring.At(i);
  Picture g;
  GatherPicture(app, s.screen_held, &g);
  Picture& p = s.picture;
  p = Picture();
  p.from_ring = true;
  p.last = g.last;
  p.last_frame = live.last;
  p.pictures = live.pictures;
  p.play_start_ms = live.play_start_ms;
  p.arrival_ms = live.arrival_ms;
  p.gap_ms = live.gap_ms;
  if (g.last == sq::Last::kScreen && app->xr.ScreenQuadOn()) {
    p.screen_pictures = g.screen_pictures;
    if (g.screen_pictures != s.screen_held) {
      s.screen_held = g.screen_pictures;
      s.screen_body = g.screen_body;
      s.screen_cpu = g.screen_cpu;
      s.screen_width = g.screen_width;
      s.screen_height = g.screen_height;
      s.screen_ok = g.screen_ok;
      if (g.screen_ok && g.screen_cpu) {
        s.screen_rgba = *g.screen_rgba;
      } else if (g.screen_ok) {
        // The last walk can have left its eye picture to the app.
        if (VkSceneComposePending(reinterpret_cast<uint64_t>(g.screen_image))) {
          app->ring.ComposeEyes(i, eye_copy_math::Whole(g.screen_width, g.screen_height));
        }
        s.screen_ok = app->ring.CopyScreen(i, g.screen_image, g.screen_layer, g.screen_width,
                                           g.screen_height);
      }
    }
    p.screen_body = s.screen_body;
    p.screen_ok = s.screen_ok;
    p.screen_cpu = s.screen_cpu;
    p.screen_rgba = &s.screen_rgba;
    p.screen_image = s.screen.image;
    p.screen_layer = 0;
    p.screen_width = s.screen_width;
    p.screen_height = s.screen_height;
  } else if (g.eyes_ok) {
    // THE EYES STRAIGHT INTO THE SWAPCHAINS (xr_eye_direct=1): a picture of
    // the projection layer goes into the two swapchains of the slot, with no
    // copy of the session. A picture of the stereo screen or of the cut scene
    // window takes the old way.
    const quad_layer_math::FrameLayers& layers = *g.layers;
    VkImage targets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    const bool shows_projection = app->xr.ProjectionOn() && g.have_view &&
                                  view_layer_math::ShowsProjection(g.view) &&
                                  !quad_layer_math::AsScreen(layers);
    // THE ONE PASS: the picture is in the swapchain image of the slot already.
    // It shows in the projection layer with no copy, else it is copied from
    // that image (the stereo screen, the cut scene window).
    const bool in_swapchain =
        density && VkSceneDensityPicture(reinterpret_cast<uint64_t>(g.eyes));
    if (in_swapchain) {
      app->xr.CountDensity(shows_projection);
      if (shows_projection) {
        app->ring.FreeEyes(i);
        app->ring.DensityMark(i, g.eyes);
        direct = true;
        p.eyes_ok = true;
        p.direct_slot = i;
        p.eyes = VK_NULL_HANDLE;
      } else {
        p.eyes_ok = app->ring.CopyEyes(i, g.eyes, g.width, g.height,
                                       eye_copy_math::Whole(g.width, g.height));
        p.eyes = s.eyes.image;
      }
    } else {
      // A slot with an image of the one pass in hand never makes swapchains of
      // its own here.
      direct = !density && app->xr.EyeDirect() && shows_projection &&
               app->xr.DirectBegin(i, g.width, g.height, g.atlas, targets);
      // THE LATE COMPOSE (xr_compose_direct=1): the walk left the eye picture to
      // the app. It has no atlas, so the part is the whole picture.
      const bool late = VkSceneComposePending(reinterpret_cast<uint64_t>(g.eyes));
      // THE COMPOSE AT THE TAKE (xr_compose_take=1): no eye picture and no copy now.
      // The session composes the picture when it takes it.
      const uint64_t ticket =
          !direct && late && shows_projection && app->compose_take && app->xr.ComposeFormatOk()
              ? VkSceneComposeTicket(reinterpret_cast<uint64_t>(g.eyes))
              : 0;
      if (ticket != 0) {
        app->ring.FreeEyes(i);
        p.compose_ticket = ticket;
        p.eyes_ok = true;
        p.eyes = VK_NULL_HANDLE;
      } else if (direct) {
        // The slot has no image of its own for the eyes now.
        app->ring.FreeEyes(i);
        const eye_copy_math::Rect part =
            eye_copy_math::CopyPart(true, g.atlas, g.width, g.height);
        if (!(late && app->xr.ComposeFormatOk() &&
              app->ring.ComposeDirect(i, targets, app->xr.SwapFormat(), part))) {
          // The old way, or the pass cannot go into this format: the pass into
          // the image of the renderer, and the copy from it.
          if (late) app->ring.ComposeEyes(i, part);
          app->ring.CopyDirect(i, g.eyes, part, targets);
        }
        p.eyes_ok = true;
        p.direct_slot = i;
        p.eyes = VK_NULL_HANDLE;
      } else {
        if (late) {
          app->ring.ComposeEyes(i, eye_copy_math::Whole(g.width, g.height));
        }
        // xr_eye_rects=1: only the part of the picture that the layers show.
        p.eyes_ok = app->ring.CopyEyes(
            i, g.eyes, g.width, g.height,
            eye_copy_math::CopyPart(app->xr.EyeRects(), g.atlas, g.width, g.height));
        p.eyes = s.eyes.image;
      }
    }
    p.width = g.width;
    p.height = g.height;
    p.have_view = g.have_view;
    p.view = g.view;
    p.have_world = g.have_world;
    p.world = g.world;
    p.late_again = app->late_again;
    p.late_tick = app->late_serial;
    memcpy(p.late_move, app->late_move, sizeof(p.late_move));
    // THE LATE CAMERA: the layer has the eyes where the new picture has them, and
    // the place of the player of the new picture.
    if (app->late_moved) {
      for (int e = 0; e < 2; ++e) {
        for (int k = 0; k < 3; ++k) p.view.eyes[e].position[k] += app->late_head_move[k];
      }
      if (app->late_has_map) {
        memcpy(p.world.linear, app->late_map.linear, sizeof(p.world.linear));
        memcpy(p.world.offset, app->late_map.offset, sizeof(p.world.offset));
      }
    }
    p.atlas = g.atlas;
    s.layers = *g.layers;
    p.layers = &s.layers;
    // The pictures of the cards change seldom: a copy only after a change.
    bool same = true;
    for (int c = 0; c < bwvr::kCards; ++c) {
      if (s.cards.Hash(c) != g.cards->Hash(c) || s.cards.Count(c) != g.cards->Count(c)) {
        same = false;
      }
    }
    if (!same) s.cards = *g.cards;
    p.cards = &s.cards;
    if (g.panel != VK_NULL_HANDLE) {
      const eye_copy_math::Rect panel_part =
          PanelPart(app->xr.PanelRect(), *g.layers, g.width, g.height);
      // THE SAME PANEL (xr_panel_keep): a panel with the pixels in the swapchain takes no
      // copy, and its layer shows those pixels. The rect of the copy counts as a part of
      // the pixels.
      p.panel_hash = VkScenePanelHash() ^ (static_cast<uint64_t>(panel_part.x) << 48) ^
                     (static_cast<uint64_t>(panel_part.y) << 32) ^
                     (static_cast<uint64_t>(panel_part.w) << 16) ^
                     static_cast<uint64_t>(panel_part.h);
      p.panel_same = app->panel_keep.load() && p.panel_hash != 0 &&
                     p.panel_hash == app->xr.PanelCopiedHash();
      if (!p.panel_same && app->ring.CopyPanel(i, g.panel, g.width, g.height, panel_part)) {
        p.panel = s.panel.image;
        p.panel_part = panel_part;
      }
    }
  }
  p.ready_ms = NowMs();
  // THE RELEASE OF THE IMAGES OF A SLOT (xr_release_take). 1: the swapchain images
  // of xr_eye_direct=1 go back to the runtime at the take of the picture
  // (TakePicture), just before the frame that names them, as in a session with
  // one swapchain. So the newest released image of a swapchain is the picture of
  // the frame that names it, also for a runtime that reads the swapchain later.
  // 0: they go back after the submit and before the slot is ready. The image of the
  // one pass goes back there also when the picture took a copy from it, or was
  // no eye picture.
  if (density || (direct && !app->release_take.load())) {
    app->ring.EndWrite(i, [app, i]() { app->xr.DirectEnd(i); });
  } else {
    app->ring.EndWrite(i);
  }
  return waited;
}

double ThreadCpuMs() {
  timespec t;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

// THE LATE CAMERA (late_camera_math.h, docs/android.md, "The late camera").
//
// The newest tick after `seen`, up to timeout_ms. False with no new tick.
bool WaitLateTick(App* app, LateTick* out, uint64_t* seen, int timeout_ms) {
  std::unique_lock<std::mutex> lock(app->late_lock);
  if (app->late_tick.serial == *seen) {
    app->late_cond.wait_for(lock, std::chrono::milliseconds(timeout_ms));
  }
  if (app->late_tick.serial == *seen) return false;
  *seen = app->late_tick.serial;
  *out = app->late_tick;
  return true;
}

// The move of the picture of the newest list for the display time of the tick.
// False when the picture has no place: it shows as the game drew it.
bool LateMoveOf(App* app, const LateTick& tick, late_camera_math::Result* out) {
  namespace lc = late_camera_math;
  namespace mc = motion_comp_math;
  app->late_moved = false;
  app->late_has_map = false;
  *out = lc::Result();
  scene_stream::ViewBody view;
  if (tick.serial == 0 || !tick.head_ok || !StreamPictureView(&view) ||
      !view_layer_math::ShowsProjection(view)) {
    return false;
  }
  input_channel::World world = {};
  const bool have_world = StreamPictureWorld(&world) && mc::Valid(world);
  const input_channel::World* newest =
      mc::Newest(tick.has_world ? &tick.world : nullptr, have_world ? &world : nullptr);
  lc::Input in = {};
  for (int e = 0; e < 2; ++e) {
    for (int k = 0; k < 3; ++k) in.eye_position[e][k] = view.eyes[e].position[k];
  }
  memcpy(in.eye_orientation, view.eyes[0].orientation, sizeof(in.eye_orientation));
  memcpy(in.fov, view.eyes[0].fov, sizeof(in.fov));
  in.width = view.width;
  in.height = view.height;
  memcpy(in.head_now, tick.head, sizeof(in.head_now));
  in.have_maps = have_world && newest != nullptr;
  if (in.have_maps) {
    const float period_s = static_cast<float>(static_cast<double>(tick.period) / 1e9);
    const float ahead = lc::AheadSeconds(*newest, tick.slots, tick.count, tick.slot, period_s,
                                         app->late_keys.predict_ms);
    in.drawn = mc::MapOf(world);
    in.now = lc::MapNow(*newest, ahead);
    lc::GrabMove(*newest, tick.grab, &in.now);
    app->late_map = in.now;
    app->late_has_map = true;
    // THE EVENNESS OF THE WALK, one count for each new slot of the walk.
    const bool walks = (newest->flags & input_channel::kWorldWalk) != 0;
    if (walks && app->late_step_have && tick.slot > app->late_step_slot &&
        tick.slot - app->late_step_slot <= 4) {
      float step = 0.0f, speed = 0.0f;
      for (int k = 0; k < 3; ++k) {
        const float d = in.now.offset[k] - app->late_step_at[k];
        step += d * d;
        speed += newest->walk_velocity[k] * newest->walk_velocity[k];
      }
      step = sqrtf(step);
      speed = sqrtf(speed);
      const float seconds = period_s * static_cast<float>(tick.slot - app->late_step_slot);
      ++app->late_steps;
      if (lc::StepUneven(step, speed, seconds)) ++app->late_uneven;
      const double slots = speed > 0.0f ? step / (speed * period_s) : 0.0;
      if (slots > app->late_step_most) app->late_step_most = slots;
    }
    app->late_step_have = walks;
    app->late_step_slot = tick.slot;
    memcpy(app->late_step_at, in.now.offset, sizeof(app->late_step_at));
  }
  *out = lc::Compute(in, app->late_keys);
  app->late_moved = out->push.move[3] > 0.0f;
  if (app->late_moved) memcpy(app->late_head_move, out->head_move, sizeof(app->late_head_move));
  memcpy(app->late_move, out->push.move, sizeof(app->late_move));
  return true;
}

// The play thread takes each batch of the reader as it comes, plays it into
// the renderer and writes its picture into a slot. It never holds the thread
// of the session. With time_log=1 it gives its stats to the session
// (PlayStats): the frames of a batch but the newest did not show, by the
// largest part of the time of the picture before: its waits for the GPU (the
// fence of a slot of the renderer or of the ring), its waits for the lock of
// the queue, or its own work.
void* PlayThread(void* arg) {
  App* app = static_cast<App*>(arg);
  TimeLogNameThread("bwvr play");
  AppTraceNameThread("play thread (renderer)", 2);
  ThreadCores::MarkPlay();
  const bool timing = app->time.On();
  XrApp::TimeQueueWaits(timing);
  LivePlay* live = &app->live_play;
  double before_total = 0.0, before_gpu = 0.0, before_queue = 0.0;
  double cpu_at = ThreadCpuMs();
  const int depth = app->gpu_depth;
  // THE LATE CAMERA: the newest tick, the move of the last picture of the list,
  // and the counts of its line in the log.
  LateTick late_tick;
  uint64_t late_seen = 0;
  float late_last[3] = {0.0f, 0.0f, 0.0f};
  float late_last_room[3] = {0.0f, 0.0f, 0.0f};
  bool late_last_on = false;
  uint64_t late_new = 0, late_replays = 0, late_same = 0, late_cuts = 0, late_plain = 0;
  double late_move_sum = 0.0, late_move_most = 0.0, late_world_sum = 0.0, late_world_most = 0.0;
  double late_log_at = NowMs();
  PictureRecord picture_record = {};
  bool picture_pending = false;
  // THE PREPARE AHEAD (late_prepare): the frames of the game that the walk has ready for the
  // next tick, and the ms of their play.
  size_t prepared_n = 0;
  double prepared_ms = 0.0;
  // THE GAP BETWEEN TWO TICKS (late_prepare_gap): the ms that a frame of the game takes, and the
  // ticks that frames of the game waited for a gap.
  double prepare_cost = 3.0;
  int prepare_waited = 0;
  // A TEST AID (remote_control): the limit of the tiny draws and the rate of the compose
  // that the file asks for.
  int tiny_seen = -1;
  int rate_seen = -1;
  // A TEST AID (gpu_test): the state, its start, and the GPU time of its walks.
  int test_state = 0;
  double test_at = 0.0, test_sum = 0.0, test_most = 0.0;
  uint64_t test_count = 0;
  while (!app->play_stop.load()) {
    // THE QUEUE AT THE GPU (frame_loop_gpu_depth): no new play while the GPU
    // has `depth` pictures that are not finished. The reader keeps the frames
    // of the game meanwhile, so the next batch has the newest one.
    const bool late = app->late_on && app->late_live.load();
    VkSceneLateDefer(late);
    // THE PREPARE AHEAD (late_prepare=1): a frame of the game goes into the renderer when it
    // comes, also while the GPU draws the picture before, and not after the next tick. So the
    // tick has only the commands of the list before it, and a new list does not make its
    // picture late. One frame waits for each tick: a newer frame stays with the reader.
    const bool prepare = late && app->late_prepare_live.load() && !app->xr.DensityPass();
    // THE GAP (late_prepare_gap=1): a frame of the game that would still run at the next tick
    // makes that picture late, so it starts only when it fits before the next tick. The tick then
    // draws the list before again, which is right for its place. A frame waits for 2 ticks at
    // most, and with no list to draw again it does not wait.
    const bool gap = prepare && app->late_gap_live.load();
    const auto prepare_now = [&](bool at_tick) {
      if (!prepare || prepared_n > 0) return;
      if (gap && prepare_waited < 2 && VkSceneLateCanReplay() &&
          StreamScreen().LastPicture() == screen_quad_math::Last::kWorld) {
        const double period_ms = static_cast<double>(late_tick.period) / 1e6;
        const double left = late_tick.sent_ms + period_ms - NowMs();
        const bool ticks_run = left > -period_ms;
        if (ticks_run && (at_tick || left < prepare_cost + 0.5)) {
          if (at_tick && app->live_reader.HasFrames()) ++prepare_waited;
          return;
        }
      }
      if (!app->live_reader.TakeAll(&live->batch)) return;
      prepare_waited = 0;
      prepared_n = live->batch.size();
      prepared_ms = PlayBatch(app, live);
      // A rare long frame does not close the gaps: 5 ms still fits after the submit of a picture.
      const double decayed = prepare_cost * 0.98;
      const double cost = prepared_ms > decayed ? prepared_ms : decayed;
      prepare_cost = cost < 5.0 ? cost : 5.0;
    };
    double gate_ms = -1.0;
    if (depth > 0) {
      // Each wait counts, also one that ends in the first call.
      const double gate_start = NowMs();
      const int64_t trace_gate = g_app_trace ? AppTraceNow() : 0;
      bool waited_gpu = false;
      for (;;) {
        bool waited = false;
        const bool may = app->ring.WaitGpu(depth, prepare ? 1000000ull : 5000000ull, &waited);
        if (waited) waited_gpu = true;
        if (may || app->play_stop.load()) break;
        prepare_now(false);
      }
      // The phase of the take: with a depth of 1 the GPU finished each
      // picture now, the picture before too.
      if (trace_gate != 0 && waited_gpu) {
        static const trace_events::Def kGateDef = {"wait for the GPU before a play (WaitGpu)", "wait", {nullptr, nullptr, nullptr, nullptr}};
        AppTraceBar(&kGateDef, trace_gate, AppTraceNow());
      }
      if (depth == 1) app->take_phase.GpuDone(NowMs(), waited_gpu);
      if (waited_gpu) gate_ms = NowMs() - gate_start;
      if (app->play_stop.load()) break;
      // A TEST AID (pose_log.h): the picture before is finished now.
      if (picture_pending) {
        picture_record.done_ms = NowMs();
        app->picture_log.Add(picture_record);
        picture_pending = false;
      }
    }
    // THE LATE CAMERA: a picture for each frame of the session, with the frames
    // of the game that came since the last one. With no tick for 50 ms (a session
    // that does not run) the frames of the game still go on.
    bool got_tick = false;
    if (prepare) {
      const double wait_at = NowMs();
      for (;;) {
        got_tick = WaitLateTick(app, &late_tick, &late_seen, 1);
        if (got_tick || app->play_stop.load()) break;
        prepare_now(false);
        if (NowMs() - wait_at >= 50.0) break;
      }
      if (app->play_stop.load()) break;
      prepare_now(got_tick);
      if (prepared_n == 0 && !got_tick) {
        QuietCheck(app, live);
        continue;
      }
    } else if (late) {
      got_tick = WaitLateTick(app, &late_tick, &late_seen, 50);
      if (app->play_stop.load()) break;
      if (!app->live_reader.TakeAll(&live->batch) && !got_tick) {
        QuietCheck(app, live);
        continue;
      }
    } else if (!app->live_reader.WaitAll(&live->batch, 50)) {
      QuietCheck(app, live);
      continue;
    }
    const double start = NowMs();
    const size_t n = prepare ? prepared_n : live->batch.size();
    const int64_t trace_play = g_app_trace ? AppTraceNow() : 0;
    // THE ONE PASS (xr_density_pass=1): the slot of the picture and its
    // swapchain image first, so the walks of the batch draw into the image. A
    // slot that has no image for the pass keeps its place, and the batch goes
    // the old way.
    int early = -1;
    bool density = false;
    double early_waited = 0.0;
    if (app->xr.DensityPass() && !app->density_off) {
      early = app->ring.BeginWrite(&early_waited);
      VkImage image = VK_NULL_HANDLE;
      if (early >= 0 && app->xr.DensityBegin(early, app->render_w, app->render_h, &image)) {
        const int taken = VkSceneDensityTarget(reinterpret_cast<uint64_t>(image),
                                               app->render_w, app->render_h);
        density = taken > 0;
        if (!density) {
          // The image goes back unused. Result 0: the renderer is not ready yet,
          // so the next batch asks again. Else this game has no more tries.
          app->xr.DirectEnd(early);
          if (taken < 0) {
            app->density_off = true;
            AppLog("game         : xr_density_pass=1, but the renderer cannot draw into the "
                   "swapchain image (see the line of vk scene before). The rest of this game "
                   "goes the old way");
          }
        }
      }
    }
    const double ms = prepare ? prepared_ms : n > 0 ? PlayBatch(app, live) : 0.0;
    if (prepare && n > 0) live->play_start_ms = start;
    prepared_n = 0;
    prepared_ms = 0.0;
    if (density) VkSceneDensityTargetEnd();
    WalkProbeInstall();
    // THE LATE CAMERA: the walk of this batch gets the move of its picture, or
    // with no walk the last list draws again when the place changed.
    bool write = n > 0;
    app->late_again = false;
    app->late_moved = false;
    app->late_serial = late ? late_seen : 0;
    if (late) {
      namespace lc = late_camera_math;
      const bool open = VkSceneLateOpen();
      const bool world_last =
          StreamScreen().LastPicture() == screen_quad_math::Last::kWorld;
      const bool can_again = world_last && VkSceneLateCanReplay();
      bool replayed = false;
      if (open || (got_tick && can_again)) {
        lc::Result moved;
        const bool placed = LateMoveOf(app, late_tick, &moved);
        const lc::Push& push = moved.push;
        // A new picture of the same list only when its place changed. A session
        // frame with no new picture shows the picture before.
        bool same = !open && late_last_on == (push.move[3] > 0.0f);
        for (int k = 0; k < 3 && same; ++k) {
          if (fabsf(push.move[k] - late_last[k]) >= app->late_keys.still ||
              fabsf(push.room[k] - late_last_room[k]) >= app->late_keys.still) {
            same = false;
          }
        }
        if (open) {
          VkSceneLateFinish(push.move, push.room);
          write = true;
          ++late_new;
        } else if (same) {
          ++late_same;
        } else if (VkSceneLateReplay(push.move, push.room)) {
          app->late_again = true;
          live->play_start_ms = start;
          replayed = true;
          ++late_replays;
        }
        if (open || replayed) {
          for (int k = 0; k < 3; ++k) {
            late_last[k] = push.move[k];
            late_last_room[k] = push.room[k];
          }
          late_last_on = push.move[3] > 0.0f;
          if (moved.cut) ++late_cuts;
          if (!placed) ++late_plain;
          late_move_sum += moved.move_cm;
          late_world_sum += moved.world_cm;
          if (moved.move_cm > late_move_most) late_move_most = moved.move_cm;
          if (moved.world_cm > late_world_most) late_world_most = moved.world_cm;
        }
      }
      // A batch with no walk has no eye picture. The slot of a picture stays for
      // a picture of the list: a slot with no eyes would take the projection
      // layer from the frame of the session.
      if (!open && can_again) write = replayed;
      const double log_now = NowMs();
      if (log_now - late_log_at >= 10000.0) {
        const uint64_t pictures = late_new + late_replays;
        AppLog("late camera  : %.1f s: %llu pictures from a new frame of the game, %llu of the "
               "list again, %llu ticks with no change of the place. %llu cuts of the view, %llu "
               "pictures with no place. The move of a picture: %.2f cm as a mean and %.2f at "
               "most, of it the walk and the map %.2f and %.2f. The steps of the walk from a tick "
               "to the next: %llu, %llu of them more than a third from the speed of the walk, "
               "the longest %.2f slots of the stick",
               (log_now - late_log_at) / 1000.0, static_cast<unsigned long long>(late_new),
               static_cast<unsigned long long>(late_replays),
               static_cast<unsigned long long>(late_same),
               static_cast<unsigned long long>(late_cuts),
               static_cast<unsigned long long>(late_plain),
               pictures > 0 ? late_move_sum / static_cast<double>(pictures) : 0.0, late_move_most,
               pictures > 0 ? late_world_sum / static_cast<double>(pictures) : 0.0,
               late_world_most, static_cast<unsigned long long>(app->late_steps),
               static_cast<unsigned long long>(app->late_uneven), app->late_step_most);
        app->late_steps = app->late_uneven = 0;
        app->late_step_most = 0.0;
        late_log_at = log_now;
        late_new = late_replays = late_same = late_cuts = late_plain = 0;
        late_move_sum = late_move_most = late_world_sum = late_world_most = 0.0;
      }
    }
    if (!write) {
      if (late) app->late_handled.store(late_seen, std::memory_order_release);
      continue;
    }
    const int64_t trace_slot = g_app_trace ? AppTraceNow() : 0;
    const double waited = WriteSlot(app, *live, early, density, early_waited);
    if (late) app->late_handled.store(late_seen, std::memory_order_release);
    // A TEST AID: the test run sets the test state of the renderer.
    {
      const int want = app->remote.GpuState();
      if (want >= 0 && want != test_state && want < VkSceneGpuTestStates()) {
        test_state = want;
        AppLog("remote       : the test state of the renderer is %s", VkSceneGpuTest(test_state));
      }
      const int tiny = app->remote.TinyPixels();
      if (tiny >= 0 && tiny != tiny_seen) {
        tiny_seen = tiny;
        VkSceneSetTinyPixels(static_cast<float>(tiny) * 0.1f);
        AppLog("remote       : the limit of the tiny draws is %.1f pixels", tiny * 0.1);
      }
      const int rate = app->remote.ComposeRate();
      if (rate >= 0 && rate != rate_seen) {
        rate_seen = rate;
        VkSceneComposeRate(rate);
        AppLog("remote       : the shading rate of the compose in parts is %d", rate);
      }
    }
    if (late && app->picture_log.On()) {
      picture_record = PictureRecord();
      picture_record.tick_ms = late_tick.sent_ms;
      picture_record.start_ms = start;
      picture_record.submit_ms = NowMs();
      picture_record.tick = static_cast<uint32_t>(late_seen);
      picture_record.flags = (n > 0 ? 1u : 0u) | (app->late_again ? 2u : 0u);
      picture_record.gate_ms = gate_ms > 0.0 ? static_cast<float>(gate_ms) : 0.0f;
      VkSceneLastDraws(&picture_record.outer_draws, &picture_record.center_draws);
      picture_record.compose_ms = copy_time::LastComposeMs();
      if (app->late_has_map) {
        const float* l = app->late_map.linear;
        picture_record.world_scale = sqrtf(l[0] * l[0] + l[1] * l[1] + l[2] * l[2]);
      }
      picture_pending = true;
    }
    if (trace_play != 0) {
      static const trace_events::Def kPlayDef = {"play of a batch of frames (renderer)", "play", {"#last frame", "frames", nullptr, nullptr}};
      static const trace_events::Def kSlotDef = {"copy of the picture into a slot (WriteSlot)", "play", {"#last frame", "wait_ms", nullptr, nullptr}};
      AppTraceBar(&kPlayDef, trace_play, trace_slot, trace_events::FrameArg(live->last), static_cast<float>(n));
      AppTraceBar(&kSlotDef, trace_slot, AppTraceNow(), trace_events::FrameArg(live->last), static_cast<float>(waited));
    }
    if (n > 0) app->take_phase.Played(live->last, live->arrival_ms, live->play_start_ms, NowMs());
    if (!timing) continue;
    PlayStats s;
    s.pictures = 1;
    s.frames = n;
    s.play.Add(ms);
    if (gate_ms >= 0.0) {
      s.gate.Add(gate_ms);
      if (n > 1) s.gate_dropped += n - 1;
      // The frames of the batch came while the play thread waited for the GPU.
      before_total += gate_ms;
      before_gpu += gate_ms;
    }
    VkSceneGpuTime g;
    VkSceneTakeGpuTime(&g);
    picture_record.gpu_ms = static_cast<float>(g.ms);
    if (app->gpu_test) {
      const double test_now = NowMs();
      if (test_at <= 0.0) {
        test_at = test_now;
        VkSceneGpuTest(test_state);
      }
      // The walks of the state before end in the first part of a state.
      if (test_now - test_at >= 500.0) {
        test_sum += g.ms;
        test_count += g.walks;
        if (g.most_ms > test_most) test_most = g.most_ms;
      } else {
        uint64_t outer, center, walks;
        int kinds;
        VkSceneGpuTestCounts(&outer, &center, &walks, &kinds);
      }
      if (test_now - test_at >= 4000.0) {
        uint64_t outer, center, walks;
        int kinds;
        VkSceneGpuTestCounts(&outer, &center, &walks, &kinds);
        const double per = walks > 0 ? static_cast<double>(walks) : 1.0;
        AppLog("gpu test     : %s: a walk %.2f ms as a mean and %.2f at most (%llu walks), "
               "%.0f + %.0f draws a walk, %d kinds of draws",
               VkSceneGpuTest(test_state),
               test_count > 0 ? test_sum / static_cast<double>(test_count) : 0.0, test_most,
               static_cast<unsigned long long>(test_count), static_cast<double>(outer) / per,
               static_cast<double>(center) / per, kinds);
        test_state = (test_state + 1) % VkSceneGpuTestStates();
        VkSceneGpuTest(test_state);
        test_at = test_now;
        test_sum = test_most = 0.0;
        test_count = 0;
      }
    }
    s.walk_gpu.count = g.walks;
    s.walk_gpu.sum = g.ms;
    s.walk_gpu.most = g.most_ms;
    s.walk_wait.count = g.fence_waits;
    s.walk_wait.sum = g.fence_wait_ms;
    s.walk_wait.most = g.fence_wait_most_ms;
    if (n > 1) {
      const time_log_math::Cause c =
          time_log_math::CauseOf(before_total, before_gpu, before_queue);
      s.skipped[static_cast<int>(c)] += n - 1;
    }
    const double cpu = ThreadCpuMs();
    s.cpu_ms = cpu - cpu_at;
    cpu_at = cpu;
    before_total = NowMs() - start;
    before_gpu = g.fence_wait_ms + waited;
    before_queue = XrApp::TakeQueueWaitMs();
    std::lock_guard<std::mutex> lock(app->play_stats_lock);
    app->play_stats.Merge(s);
  }
  VkSceneLateDefer(false);
  XrApp::TimeQueueWaits(false);
  return nullptr;
}

// THE KEYS OF THE DEBUG PAGE (src/mod/app_keys.h): the player calls this on the
// play thread when the bits of a frame change. xr_compose_direct needs
// xr_eye_direct=1. The late compose goes on only while the play thread runs.
App* g_hook_app = nullptr;
void AppKeysApply(uint32_t bits) {
  App* app = g_hook_app;
  if (app == nullptr) return;
  app->compose_direct = app->xr.EyeDirect() && (bits & app_keys::kComposeDirect) != 0;
  app->compose_take = app->compose_take_key && (bits & app_keys::kComposeDirect) != 0;
  app->xr.SetPanelRect((bits & app_keys::kPanelRect) != 0);
  app->ring.SetLeastRecent((bits & app_keys::kRingOrder) != 0);
  app->late_live.store((bits & app_keys::kLateCamera) != 0);
  app->release_take.store((bits & app_keys::kReleaseTake) != 0);
  app->late_take_live.store((bits & app_keys::kLateTake) != 0);
  app->late_grab_live.store((bits & app_keys::kLateGrab) != 0);
  app->late_prepare_live.store((bits & app_keys::kLatePrepare) != 0);
  app->late_gap_live.store((bits & app_keys::kLateGap) != 0);
  app->panel_keep.store((bits & app_keys::kPanelKeep) != 0);
  VkSceneSetBindless((bits & app_keys::kBindless) != 0);
  VkSceneSetFastShaders((bits & app_keys::kFastShaders) != 0);
  VkSceneSetTinyDraws((bits & app_keys::kTinyDraws) != 0);
  VkSceneSetListKeep((bits & app_keys::kListKeep) != 0);
  VkSceneSetCheapDraws((bits & app_keys::kCheapDraws) != 0);
  VkSceneComposeFast((bits & app_keys::kComposeFast) != 0);
  app->xr.SetComposeBytes((bits & app_keys::kComposeFast) != 0);
  if (app->play_ok.load()) VkSceneComposeLate(app->compose_direct || app->compose_take);
  AppLog("game         : the debug page: xr_compose_direct=%d (at the take %d), xr_panel_rect=%d, ring_slot_order=%d, "
         "late_camera=%d, xr_release_take=%d, late_take=%d",
         app->compose_direct ? 1 : 0, app->compose_take ? 1 : 0,
         (bits & app_keys::kPanelRect) != 0 ? 1 : 0,
         (bits & app_keys::kRingOrder) != 0 ? 1 : 0, (bits & app_keys::kLateCamera) != 0 ? 1 : 0,
         (bits & app_keys::kReleaseTake) != 0 ? 1 : 0, (bits & app_keys::kLateTake) != 0 ? 1 : 0);
}

void StartPlayThread(App* app) {
  if (app->play_running) return;
  app->play_stop.store(false);
  // The keys of the debug page come with the frames of the stream.
  g_hook_app = app;
  SetAppKeysHook(&AppKeysApply);
  VkSceneComposeSets(app->compose_take_key ? vk_compose_math::kMostSets : 1);
  {
    std::lock_guard<std::mutex> lock(app->play_stats_lock);
    app->play_stats = PlayStats();
  }
  app->play_running = pthread_create(&app->play_thread, nullptr, &PlayThread, app) == 0;
  if (!app->play_running) {
    AppLog("game         : the play thread did not start");
  } else {
    // The play thread records the compose of the walks that leave their eye
    // picture to it (xr_compose_direct), so the mode goes on only now.
    app->play_ok.store(true);
    VkSceneComposeLate(app->compose_direct || app->compose_take);
  }
}

// The thread of the session owns the renderer again after the join.
void StopPlayThread(App* app) {
  if (!app->play_running) return;
  app->play_stop.store(true);
  app->play_ok.store(false);
  pthread_join(app->play_thread, nullptr);
  app->play_running = false;
  // Nobody records the compose of a walk any more.
  VkSceneComposeLate(false);
}

// THE LATE TAKE (late_take_ms, docs/android.md, "The late camera"): the thread of
// the session looks for the picture of the tick before, up to most_ms. The tick of
// this frame went out already, so the next picture does not start later. With the
// line of the log each 10 s.
void WaitTickPicture(App* app, uint64_t serial, int most_ms) {
  namespace lc = late_camera_math;
  const double start = NowMs();
  double waited = 0.0;
  bool over = false;
  for (;;) {
    const lc::TickPicture picture = app->ring.PictureOfTick(serial);
    waited = NowMs() - start;
    if (lc::TakeWaitEnds(picture, app->late_handled.load(std::memory_order_acquire), serial, waited,
                         most_ms)) {
      over = picture != lc::TickPicture::kDone && waited >= static_cast<double>(most_ms);
      break;
    }
    usleep(100);
  }
  app->take_wait_now = waited;
  ++app->take_all;
  if (waited >= 0.2) {
    ++app->take_waits;
    app->take_sum += waited;
    if (waited > app->take_most) app->take_most = waited;
  }
  if (over) ++app->take_over;
  const double now = start + waited;
  if (app->take_log_at <= 0.0) app->take_log_at = now;
  if (now - app->take_log_at >= 10000.0) {
    AppLog("late take    : %.1f s: %llu frames of the session, %llu of them waited for the picture "
           "of the tick before, %.2f ms as a mean and %.2f at most. %llu came to the limit of %d ms "
           "and show the picture before",
           (now - app->take_log_at) / 1000.0, static_cast<unsigned long long>(app->take_all),
           static_cast<unsigned long long>(app->take_waits),
           app->take_waits > 0 ? app->take_sum / static_cast<double>(app->take_waits) : 0.0,
           app->take_most, static_cast<unsigned long long>(app->take_over), most_ms);
    app->take_all = app->take_waits = app->take_over = 0;
    app->take_sum = app->take_most = 0.0;
    app->take_log_at = now;
  }
}

// THE SESSION: the picture of the ring that the take picks (the newest
// finished one, or the next in order with frame_queue=1), at each frame of
// the session. True when it is a new picture. With none the session shows
// the picture before again. facts: the delay and the dropped pictures of the
// take (slow_frame_ms).
bool TakePicture(App* app, PictureRing::TakeFacts* facts) {
  const Picture* p = app->ring.Take(app->xr.SessionSerial(), facts);
  if (p == nullptr) return false;
  // xr_release_take=1: the images of the picture go back to the runtime now. With 0
  // they went back already, and this does nothing.
  if (p->direct_slot >= 0) app->xr.DirectEnd(p->direct_slot);
  // The slot before is free now: no layer copies from it again. The new
  // picture gives the new sources.
  app->xr.FreezeSlotSources();
  // A picture that takes an early return of ShowPicture (a screen, no eyes)
  // names no slot, and the slot of the picture before is free.
  app->xr.ClearDirectSource();
  ShowPicture(app, *p);
  if (app->xr.AgeLog()) {
    take_age::TakeFacts t;
    t.screen = p->last == screen_quad_math::Last::kScreen && app->xr.ScreenQuadOn();
    t.eyes = t.screen ? p->screen_ok : p->eyes_ok;
    t.frame = p->last_frame;
    t.have_view = p->have_view;
    t.view_frame = p->view.frame;
    app->xr.AgeTake(t, p->direct_slot);
  }
  app->shown_last = p->last_frame;
  app->shown_play_ms = p->play_start_ms;
  app->shown_arrival_ms = p->arrival_ms;
  app->shown_ready_ms = p->ready_ms;
  app->shown_taken_ms = NowMs();
  app->shown_gap_ms = p->gap_ms;
  app->shown_input_frame = p->have_view ? p->view.input_frame : 0u;
  if (app->pose_log.On()) {
    app->shown_again = p->late_again;
    app->shown_tick = p->late_tick;
    app->shown_work_ms = p->ready_ms - p->play_start_ms;
    memcpy(app->shown_move, p->late_move, sizeof(app->shown_move));
    app->shown_body = p->view;
    memcpy(app->shown_place, p->world.offset, sizeof(app->shown_place));
  }
  GameFrames(app, p->last_frame, p->pictures);
  return true;
}

void Save(App* app) {
  app->menu.Store(&app->ini);
  // motion_comp takes the new value at once (the A/B of the menu).
  app->xr.Motion().SetMode(app->ini.GetInt("bwvr", "motion_comp", app->xr.Motion().Mode()));
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
    // THE QUEUE OF THE RENDERER: the second queue with the frame loop
    // (frame_loop=1 and frame_loop_queues=2), to the end of the process.
    app->xr.SetRendererSecondQueue(frame_loop::On(app->menu.FrameLoop()));
    AppLog("xr           : the renderer of the mod takes queue %d%s",
           app->xr.RendererOnSecondQueue() ? 1 : 0,
           app->xr.RendererOnSecondQueue()
               ? " (frame_loop=1, frame_loop_queues=2), and the session queue 0"
               : ", the queue of the session");
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
  app->live_reader.SetTimeLog(app->time.On());
  // slow_frame_ms (0 by default): the lines of the long frames of the game
  // (long_frames.h).
  const int slow_frame_ms = app->ini.GetInt("bwvr", "slow_frame_ms", 0);
  app->frame_clock.Start(slow_frame_ms);
  app->latch_log.Start(slow_frame_ms);
  app->live_reader.SetFrameClock(&app->frame_clock);
  // The line of the phase of the take (long_frames.h): with slow_frame_ms
  // above 0 or time_log=1.
  app->take_phase.Start(slow_frame_ms > 0 || app->time.On());
  app->live_reader.SetTakePhase(&app->take_phase);
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
  // A TEST AID: the text of the test run takes the same place.
  const bool remote = !app->remote_prompt.empty() && app->game_on && !app->menu_over_game;
  if (remote) {
    app->menu.SetNotice(app->remote_prompt);
  } else if (notice) {
    const int ms = app->play_menu_hold_ms;
    char text[320];
    if (ms > 0) {
      snprintf(text, sizeof(text),
               "The game starts in Wine. The picture of the game comes with its first "
               "frame. Hold the left menu button%s for %.1f s to show the menu with "
               "\"Quit game\".",
               app->play_menu_sticks ? ", or the clicks of both sticks," : "", ms / 1000.0);
    } else {
      snprintf(text, sizeof(text),
               "The game starts in Wine. The picture of the game comes with its first "
               "frame. play_menu_hold_ms=0: the menu comes back when the game ends.");
    }
    app->menu.SetNotice(text);
  } else {
    app->menu.SetNotice("");
  }
  app->xr.SetMenuShown(app->menu_over_game || notice || remote || !app->game_on);
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

// THE PROFILE OF THE GAME in the help of "Play" (play_rules.h, ProfileHelp):
// the profile of bwvr-profile.reg in the game folder (pc_profile=1), else
// the profile in the registry of the prefix, else none. At the start of the
// app and at the end of each game.
void ShowProfile(App* app) {
  const std::string text = ReadTextFile(app->game.folder + "/" + play_rules::kProfileFile);
  const play_rules::ProfileFile file = play_rules::ParseProfileFile(text);
  std::string done, prefix_name;
  if (!app->prefix.empty()) {
    done = ReadTextFile(play_rules::AppFolder(app->prefix) + "/" + play_rules::kProfileDone);
    prefix_name = play_rules::PrefixProfileName(ReadTextFile(app->prefix + "/user.reg"));
  }
  const bool imported =
      file.ok && !play_rules::NeedsProfileImport(file, play_rules::ProfileStamp(text), done);
  const std::string help =
      play_rules::ProfileHelp(app->pc_profile, !text.empty(), file, imported, prefix_name);
  app->menu.SetProfile(help);
  if (help != app->profile_said) {
    app->profile_said = help;
    AppLog("profile      : the help of Play says: %s", help.c_str());
  }
}

// The profile of the PC for this Play (pc_profile=1): the values of
// bwvr-profile.reg go into the game registry of the start when the prefix
// does not have them from an earlier Play.
void TakeProfile(App* app, WineRuntime::Game* game) {
  if (!app->pc_profile) return;
  const std::string path = app->game.folder + "/" + play_rules::kProfileFile;
  const std::string text = ReadTextFile(path);
  if (text.empty()) {
    AppLog("profile      : the game folder has no %s. Play imports no profile",
           play_rules::kProfileFile);
    return;
  }
  const play_rules::ProfileFile file = play_rules::ParseProfileFile(text);
  if (!file.ok) {
    AppLog("profile      : %s is not right: %s. Play imports no value of it",
           play_rules::kProfileFile, file.why.c_str());
    return;
  }
  const std::string stamp = play_rules::ProfileStamp(text);
  const std::string done =
      app->prefix.empty()
          ? ""
          : ReadTextFile(play_rules::AppFolder(app->prefix) + "/" + play_rules::kProfileDone);
  struct stat st;
  const std::string folder = app->game.folder + "/Profiles/" + file.folder;
  const bool has_folder = stat(folder.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
  if (!play_rules::NeedsProfileImport(file, stamp, done)) {
    AppLog("profile      : the prefix has the profile %s of %s from an earlier Play (the "
           "stamp %s). Play imports nothing",
           file.name.c_str(), play_rules::kProfileFile, stamp.c_str());
    return;
  }
  game->profile_reg = play_rules::ProfileRegistry(file);
  game->profile_stamp = stamp;
  game->profile_name = file.name;
  AppLog("profile      : Play imports the profile %s of %s: the value Profile and %zu values "
         "of the key %s. The filter dropped %d keys and values. The game folder %s the folder "
         "Profiles/%s",
         file.name.c_str(), play_rules::kProfileFile, file.values.size(), file.folder.c_str(),
         file.dropped, has_folder ? "has" : "does not have", file.folder.c_str());
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
  TakeProfile(app, &game);
  // box64_strongmem (the row of the page Testing) and box64_env (bwvr.ini
  // only): the variables of Box64 for the game. 0 and empty (the
  // defaults): no variable.
  game.box64_strongmem = app->menu.StrongMem();
  game.box64_env = app->ini.Get("bwvr", "box64_env", "");
  // wow64_cpu (the row of the page Testing): box64 (the default) or fex,
  // the CPU of the 32 bit code on arm64-v8a. fex_env (bwvr.ini only): the
  // variables of FEX for the game, with wow64_cpu=fex. Empty (the default):
  // no variable.
  game.cpu = app->menu.Wow64Cpu();
  game.fex_env = app->ini.Get("bwvr", "fex_env", "");
  // fex_disk_cache (bwvr.ini only, 0 by default): 1 gives FEX a code cache
  // in files of the prefix (play_rules.h, FexVars). 0: no code cache.
  game.fex_disk_cache = app->ini.GetInt("bwvr", "fex_disk_cache", 0) != 0;
  if (!app->wine.StartGame(game)) {
    app->status = "Saved. A game runs already.";
    AppLog("menu         : %s. A game runs already", name);
    return;
  }
  app->play_ms = NowMs();
  app->play_clock = ClockNow();
  app->first_frame = false;
  app->game_on = true;
  app->time.GameStarts(app->play_ms);
  app->latch_log.GameStarts(app->play_ms);
  app->frame_clock.GameStarts();
  // THE CORES (core_rules.h, bwvr.ini only): thread_cores=1 logs the core
  // and the CPU time of each thread each second, thread_bind=1 or 2 binds
  // threads to the cores. 0 (the defaults): no line and no binding.
  {
    ThreadCores::Options cores;
    cores.log = app->ini.GetInt("bwvr", "thread_cores", 0) != 0;
    cores.bind = app->ini.GetInt("bwvr", "thread_bind", 0);
    cores.bind_cores = app->ini.Get("bwvr", "thread_bind_cores", "");
    app->cores.GameStarts(cores);
  }
  app->menu.SetGameRunning(true);
  app->menu_over_game = true;
  ShowMenuOverGame(app, false);
  app->status = "Starting the game (" + app->play_clock + ")...";
  if (app->play_notice) AppLog("game         : play_notice=1. The quad of the menu shows a notice until the first frame of the game");
  AppLog("game         : %s at %s, the render %d by %d", name, app->play_clock.c_str(),
         game.width, game.height);
  // THE FRAME LOOP (frame_loop, the row of the page Testing), at each Play.
  // 1: the play thread plays the pictures into the ring, and the session
  // shows the newest finished one. 0: the old loop.
  bool loop = frame_loop::On(app->menu.FrameLoop());
  if (loop && !app->ring.Start(app->xr.SessionGpu(), app->xr.RendererQueue(),
                               app->xr.SessionTimeline(), app->xr.SwapRedBlue())) {
    loop = false;
  }
  // frame_queue (0 by default): 1, each take shows the next finished picture
  // in the order of the pictures, with a delay of at most one take more, in
  // a ring of 4 slots. 0: each take shows the newest finished picture, in a
  // ring of 3 slots.
  const bool queue = frame_loop::QueueOn(app->ini.GetInt("bwvr", "frame_queue", 0));
  if (loop) app->ring.SetQueue(queue);
  AppLog("game         : frame_loop=%d. %s", app->menu.FrameLoop(),
         loop ? "A play thread plays each picture of the game into a slot of the ring of the "
                "pictures, and each frame of the session shows a slot that the GPU finished, "
                "with no wait on the CPU for the GPU"
              : app->menu.FrameLoop() != 0
                    ? "The app has no ring of the pictures, so the thread of the session plays "
                      "the pictures (the old loop)"
                    : "The thread of the session plays the pictures and waits for the GPU "
                      "before each copy (the old loop)");
  app->time.SetFrameLoop(loop);
  // frame_loop_image_wait (0 by default): with the frame loop the session
  // does not wait for a swapchain image that the runtime does not give free
  // at once. 1: it waits, as the old loop.
  const bool image_wait = app->ini.GetInt("bwvr", "frame_loop_image_wait", 0) != 0;
  if (loop) {
    AppLog("game         : frame_loop_image_wait=%d. %s", image_wait ? 1 : 0,
           image_wait ? "The session waits for each swapchain image"
                      : "A swapchain image that is not free at once waits for the next frame "
                        "of the session, and its layer shows the image before");
  }
  app->xr.SetImageWait(image_wait);
  // frame_loop_gpu_depth (1 by default): the play thread starts the play of a
  // new picture only while fewer pictures than this are not finished on the
  // GPU. 0: no limit, the play thread plays each batch at once.
  app->gpu_depth = frame_loop::GpuDepth(app->ini.GetInt("bwvr", "frame_loop_gpu_depth", 1));
  app->time.SetGpuDepth(app->gpu_depth);
  if (loop) {
    AppLog("game         : frame_loop_gpu_depth=%d. %s", app->gpu_depth,
           app->gpu_depth > 0
               ? "The play thread starts a new picture only while fewer pictures than that are "
                 "not finished on the GPU, and then plays the newest frame of the game"
               : "No limit: the play thread plays each batch of frames at once");
  }
  // xr_eye_direct (0 by default): 1, the play thread copies each picture of
  // the projection layer straight into two swapchains of its slot of the
  // ring, and the session copies nothing for the eyes.
  const bool eye_direct = loop && app->ini.GetInt("bwvr", "xr_eye_direct", 0) != 0;
  app->xr.SetEyeDirect(eye_direct);
  if (loop) {
    AppLog("game         : xr_eye_direct=%d. %s", eye_direct ? 1 : 0,
           eye_direct ? "The play thread copies each picture of the projection layer straight "
                        "into two swapchains of its slot of the ring, and the session copies "
                        "nothing for the eyes"
                      : "The play thread copies each picture into its slot of the ring, and "
                        "the session copies it from there into the swapchains of the eyes");
  }
  // xr_compose_direct (0 by default, needs xr_eye_direct=1): 1, a walk with the
  // outer image and the centre image (fovea_res=1, fovea_res_layer=0) leaves
  // its eye picture to the app, and the play thread draws the eye picture of
  // each eye straight into the swapchain image of its slot, in one pass, with
  // no eye image of the renderer and no copy of it.
  app->compose_direct = eye_direct && app->ini.GetInt("bwvr", "xr_compose_direct", 0) != 0;
  if (loop) {
    AppLog("game         : xr_compose_direct=%d. %s", app->compose_direct ? 1 : 0,
           app->compose_direct
               ? "A walk with the outer image and the centre image leaves its eye picture to "
                 "the app, and the play thread draws it into the swapchain image of each eye "
                 "of its slot in one pass (no copy of the eye picture)"
               : (eye_direct ? "The renderer makes the eye picture of each walk, and the play "
                               "thread copies it"
                             : "Needs xr_eye_direct=1. The renderer makes the eye picture of "
                               "each walk"));
  }
  // xr_compose_take (0 by default, needs xr_eye_direct=0, xr_compose_direct=1 for the
  // row of the debug page, frame_loop_gpu_depth=1 and one queue): a walk with the outer
  // image and the centre image leaves its eye picture to the app, and the session
  // composes it into the one pair of swapchains at the take of the picture.
  {
    const bool key = app->ini.GetInt("bwvr", "xr_compose_take", 0) != 0;
    const char* why = nullptr;
    if (!key) why = nullptr;
    else if (!loop) why = "the frame loop is off";
    else if (eye_direct) why = "xr_eye_direct is 1";
    else if (app->ini.GetInt("bwvr", "xr_compose_direct", 0) == 0) why = "xr_compose_direct is 0";
    else if (app->gpu_depth != 1) why = "frame_loop_gpu_depth is not 1";
    else if (app->xr.RendererOnSecondQueue()) why = "the renderer has a second queue";
    app->compose_take_key = key && why == nullptr;
    app->compose_take = app->compose_take_key;
    if (key || loop) {
      AppLog("game         : xr_compose_take=%d. %s", key ? 1 : 0,
             app->compose_take
                 ? "A walk with the outer image and the centre image leaves its eye picture to "
                   "the app, and the session composes it into one pair of swapchains at the take"
                 : "The session does not compose the pictures");
      if (key && why != nullptr) AppLog("game         : xr_compose_take is off: %s", why);
    }
  }
  // xr_density_pass (0 by default, needs xr_eye_direct=1, fovea_res=1 with
  // fovea_res_layer=0, and a device with the density map): 1, the walks of both
  // eyes draw one time into a swapchain image of the slot, with a fragment density
  // map from the keys of fovea_res. No outer image, no centre image, no compose
  // and no copy.
  app->render_w = game.width;
  app->render_h = game.height;
  app->density_off = false;
  {
    const bool key = app->ini.GetInt("bwvr", "xr_density_pass", 0) != 0;
    const bool res = app->ini.GetInt("bwvr", "fovea_res", 0) != 0;
    const bool atlas = app->ini.GetInt("bwvr", "fovea_res_layer", 0) != 0;
    const char* why = nullptr;
    if (!key) why = nullptr;
    else if (!loop) why = "the frame loop is off";
    else if (!eye_direct) why = "xr_eye_direct is 0";
    else if (!res) why = "fovea_res is 0 (its keys set the map)";
    else if (atlas) why = "fovea_res_layer is not 0 (the centre as its own layer has an atlas)";
    else if (!app->xr.DensityMap()) {
      why = "the device has no VK_EXT_fragment_density_map (the GPU lacks it, or the key was 0 "
            "at the start of the app)";
    }
    app->density_pass = key && why == nullptr;
    app->xr.SetDensityPass(app->density_pass);
    if (key || loop) {
      AppLog("game         : xr_density_pass=%d. %s", key ? 1 : 0,
             app->density_pass
                 ? "A walk of both eyes draws one time into a swapchain image of two layers of the "
                   "slot, with a fragment density map from fovea_res_center_deg, "
                   "fovea_res_outer_pct and vk_density_ring_deg (no outer image, no centre image, "
                   "no compose, no copy)"
                 : (key ? "The one pass is off for this game: see the reason below"
                        : "The walks draw into the images of the renderer"));
      if (key && why != nullptr) AppLog("game         : the one pass is off: %s", why);
    }
  }
  // THE LATE CAMERA (late_camera, 1 by default, needs xr_compose_direct=1 with
  // xr_eye_direct=1, or xr_compose_take=1, and no one pass): each frame of the session
  // draws the newest list of draws of the
  // game again, from the place of the head and of the player at the next display
  // time. 0: a new picture only for a new frame of the game. It shows the newest
  // picture, so frame_queue has no effect then.
  {
    const bool key = late_camera_math::On(app->ini.GetInt("bwvr", "late_camera", 1));
    app->late_on = key && loop && (app->compose_direct || app->compose_take) && !app->density_pass;
    app->late_live.store(true);
    app->late_keys = late_camera_math::Keys();
    app->late_keys.most_step = app->ini.GetInt("bwvr", "late_camera_most_cm", 200) * 0.01f;
    app->late_keys.most_turn = static_cast<float>(app->ini.GetInt("bwvr", "late_camera_most_deg", 2));
    app->late_keys.predict_ms = app->ini.GetInt("bwvr", "late_camera_predict_ms", 250);
    app->late_keys.still = app->ini.GetInt("bwvr", "late_camera_still_um", 100) * 1e-6f;
    if (app->late_on && queue) app->ring.SetQueue(false);
    // late_take (0 by default, the row "Late take" of the debug page), and its limit.
    {
      const int most = app->ini.GetInt("bwvr", "late_take_ms", 4);
      app->late_take_ms = most < 0 ? 0 : (most > 8 ? 8 : most);
      app->late_take_live.store(app->ini.GetInt("bwvr", "late_take", 0) != 0);
      app->late_grab_live.store(app->ini.GetInt("bwvr", "late_grab", 0) != 0);
      app->late_prepare_live.store(app->ini.GetInt("bwvr", "late_prepare", 0) != 0);
      app->late_gap_live.store(app->ini.GetInt("bwvr", "late_prepare_gap", 0) != 0);
      app->late_grab_ms = static_cast<float>(app->ini.GetInt("bwvr", "grab_smoothing_ms", 35));
      app->late_handled.store(0);
      app->take_all = app->take_waits = app->take_over = 0;
      app->take_sum = app->take_most = app->take_log_at = 0.0;
      if (app->late_on) {
        AppLog("game         : late_take=%d, late_take_ms=%d. %s", app->late_take_live.load() ? 1 : 0,
               app->late_take_ms,
               app->late_take_live.load() && app->late_take_ms > 0
                   ? "The tick of a frame of the session goes out before its take, and the take "
                     "waits up to that time for the picture of the tick before"
                   : "The take of a frame of the session does not wait, and the tick of the frame "
                     "goes out after the take");
      }
    }
    // The new pictures have the place of the player at their display time, so the
    // poses of the layer take no difference on top.
    if (app->late_on) app->xr.Motion().SetMode(motion_comp_math::kOff);
    // pose_log (bwvr.ini only, 0 by default): one record for each frame of the
    // session, for tools/android/pose_report.py. Memory for 15 minutes at 90 Hz.
    if (app->ini.GetInt("bwvr", "pose_log", 0) != 0 && !app->pose_log.On()) {
      const std::string pose_path = app->folder + "/bwvr-pose.bin";
      app->pose_log.Start(pose_path, 81000);
      app->picture_log.Start(app->folder + "/bwvr-pictures.bin", 81000, 0x53434950u);
      AppLog("game         : pose_log=1. One record of %u bytes for each frame of the session "
             "goes into %s", static_cast<unsigned>(sizeof(PoseRecord)), pose_path.c_str());
    }
    {
      std::lock_guard<std::mutex> lock(app->late_lock);
      app->late_tick.serial = 0;
    }
    if (loop) {
      AppLog("game         : late_camera=%d. %s", key ? 1 : 0,
             app->late_on
                 ? "Each frame of the session draws the newest list of draws of the game again "
                   "for the next display time, so the camera does not wait for the game"
             : key ? "It needs xr_compose_direct=1 or xr_compose_take=1 and no one pass, so "
                     "each frame of the game makes one picture"
                   : "Each frame of the game makes one picture");
    }
  }
  // gpu_test (0 by default, bwvr.ini only): a test aid. The renderer goes through its test
  // states, 4 s each, and the log has the GPU time of each state.
  app->gpu_test = app->ini.GetInt("bwvr", "gpu_test", 0) != 0;
  // vk_tiny_pixels (10 by default, bwvr.ini only): the limit of the tiny draws in tenths of
  // a pixel. The row "Tiny draws" of the debug page turns them on.
  VkSceneSetTinyPixels(static_cast<float>(app->ini.GetInt("bwvr", "vk_tiny_pixels", 10)) * 0.1f);
  // vk_compose_rate (1 by default, bwvr.ini only): the shading rate of the cells outside the
  // centre with vk_compose_fast=1. xr_eye_mutable (0 by default, bwvr.ini only): the swapchains
  // of the eyes allow a view of the UNORM format, so the compose writes the bytes as they are.
  VkSceneComposeRate(app->ini.GetInt("bwvr", "vk_compose_rate", 1));
  app->xr.SetEyeMutable(app->ini.GetInt("bwvr", "xr_eye_mutable", 0) != 0);
  // remote_control (0 by default, bwvr.ini only): a test aid. The test run on the headset
  // changes the switches of the debug page while the game runs ("The test run").
  {
    const bool remote = app->ini.GetInt("bwvr", "remote_control", 0) != 0;
    VkSceneListMeasure(remote);
    if (remote) {
      app->remote_prompt.clear();
      app->remote.Start(app->game.folder + "/bwvr-control.txt", app->folder + "/bwvr-steps.csv");
    }
  }
  // xr_release_take (1 by default): see WriteSlot.
  app->release_take.store(app->ini.GetInt("bwvr", "xr_release_take", 1) != 0);
  if (loop) {
    AppLog("game         : xr_release_take=%d. %s", app->release_take.load() ? 1 : 0,
           app->release_take.load()
               ? "The images of a picture in the swapchains of its slot go back to the runtime at "
                 "the take of the picture, just before the frame that names them"
               : "The images of a picture in the swapchains of its slot go back to the runtime "
                 "after the submit of the picture");
  }
  // xr_panel_rect (0 by default): 1, the copies of the panel image (into the
  // slot and into its swapchain) take only the part that the panel quads show.
  const bool panel_rect = app->ini.GetInt("bwvr", "xr_panel_rect", 0) != 0;
  app->xr.SetPanelRect(panel_rect);
  if (loop) {
    AppLog("game         : xr_panel_rect=%d. %s", panel_rect ? 1 : 0,
           panel_rect ? "The copies of the panel image take only the smallest rect that holds "
                        "the rects of the panel quads"
                      : "The copies of the panel image take the whole image");
  }
  // frame_loop_hold (0 by default): N of 2 or more, the session takes a new
  // picture only after the shown picture showed in N frames of the session.
  app->hold = eye_copy_math::Hold(app->ini.GetInt("bwvr", "frame_loop_hold", 0));
  app->hold_shown = 0;
  app->hold_any = false;
  if (loop) {
    AppLog("game         : frame_loop_hold=%d. %s", app->hold,
           app->hold >= 2 ? "Each picture shows in at least that many frames of the session, "
                            "so the pictures come at an even rate when the GPU is slower than "
                            "the display"
                          : "Each frame of the session shows the newest finished picture");
    AppLog("game         : frame_queue=%d. %s", queue ? 1 : 0,
           queue ? "A ring of 4 slots. Each take shows the oldest finished picture that is "
                   "newer than the shown one, so the pictures show in their order with a delay "
                   "of at most one take more. When more than one finished picture waits after "
                   "a take, the older ones go"
                 : "A ring of 3 slots. Each take shows the newest finished picture");
  }
  // THE PHASE OF THE TAKE (docs/android.md, "The long frames"): a new window
  // of its line, with the input channel for the world of the mod.
  app->take_phase.GameStarts(app->play_ms, &app->input_live);
  // xr_resize_free (1 by default): a new Play frees the swapchains that have
  // another size than the render of this game and makes them again at the new
  // size, and a slot of xr_eye_direct=1 does the same for a picture that does
  // not fit. 0: the sets stay as they are.
  const bool resize_free = app->ini.GetInt("bwvr", "xr_resize_free", 1) != 0;
  app->xr.SetResizeFree(resize_free);
  AppLog("game         : xr_resize_free=%d. %s", resize_free ? 1 : 0,
         resize_free ? "A set of swapchains of another size than the render goes at this "
                       "Play and is made again at the new size, and a slot of the ring "
                       "whose swapchains do not fit a picture makes new ones"
                     : "The swapchains stay as they are, and a picture that does not fit "
                       "takes the old way");
  // ring_slot_order (1 by default, a key of the debug page too): 1, the play
  // thread writes the free slot that showed the longest ago, so the swapchains
  // that a frame of the session named last are written again as late as the
  // ring allows. 0: the lowest free index, which is often the slot that the
  // take freed just now.
  const bool slot_order = app->ini.GetInt("bwvr", "ring_slot_order", 1) != 0;
  if (loop) {
    app->ring.SetLeastRecent(slot_order);
    AppLog("game         : ring_slot_order=%d. %s", slot_order ? 1 : 0,
           slot_order ? "The play thread writes the free slot that showed the longest ago"
                      : "The play thread writes the free slot with the lowest index");
  }
  // take_age_log (0 by default): 1, the log says which picture is older than
  // the picture before and why, how soon the play thread writes the
  // swapchains of a slot that a frame named, and how many walks started with
  // the picture of the walk before (docs/android.md, "The age of a picture in
  // the layer").
  const bool age_log = loop && app->ini.GetInt("bwvr", "take_age_log", 0) != 0;
  app->xr.SetAgeLog(age_log);
  if (loop) {
    AppLog("game         : take_age_log=%d. %s", age_log ? 1 : 0,
           age_log ? "Each take of a picture that is older than the picture before gets a line "
                     "with the reason, and each 10 s a line counts the reasons"
                   : "No age lines");
  }
  // NewPlay frees sets that a play thread would write: it runs only while
  // no play thread runs.
  if (!app->play_running) {
    app->xr.NewPlay(game.width, game.height);
  } else {
    AppLog("game         : a play thread runs, so the swapchains of the Play before stay");
  }
  app->xr.SetFrameLoop(loop);
  if (loop) {
    StartPlayThread(app);
    // With no play thread the thread of the session plays: the old loop.
    if (!app->play_running) {
      app->xr.SetFrameLoop(false);
      app->time.SetFrameLoop(false);
      AppLog("game         : no play thread, so this game has the old loop");
    }
  }
}

// THE MENU BUTTON at each frame of the session while the game runs
// (play_rules.h, StepMenuButton): a hold shows the menu over the game, a
// press hides it, and the step has the menu bits for the slot of the mod.
void MenuButton(App* app, const XrInput& in) {
  if (!app->game_on) {
    app->menu_rule = play_rules::MenuButtonRule();
    app->menu_step = play_rules::MenuStep();
    app->stick_rule = play_rules::StickChordRule();
    return;
  }
  app->menu_step = play_rules::StepMenuButton(&app->menu_rule, in.menu, NowMs(),
                                              app->menu_over_game, app->play_menu_hold_ms,
                                              app->play_menu_press);
  const play_rules::MenuAction chord =
      app->play_menu_sticks
          ? play_rules::StepStickChord(&app->stick_rule, in.stick_click, NowMs(),
                                       app->menu_over_game, app->play_menu_hold_ms)
          : play_rules::MenuAction::kNone;
  if (app->menu_step.action == play_rules::MenuAction::kNone &&
      chord != play_rules::MenuAction::kNone) {
    app->menu_step.action = chord;
    AppLog("game         : the clicks of both sticks %s the menu over the game "
           "(play_menu_sticks=1)",
           chord == play_rules::MenuAction::kShow ? "show" : "hide");
  }
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
  if (app->remote.TakePrompt(&app->remote_prompt)) ShowNotice(app);
  int code = 0;
  std::string why;
  if (app->wine.State(&code, &why) != WineRuntime::GameState::kEnded) return;
  app->game_on = false;
  app->remote.Stop();
  app->remote_prompt.clear();
  // THE FRAME LOOP ends with the game: the thread of the session owns the
  // renderer again, and no slot of the ring shows.
  if (app->play_running) {
    StopPlayThread(app);
    app->xr.FreezeSlotSources();
    app->ring.Clear(app->xr.SessionSerial());
  }
  app->xr.SetFrameLoop(false);
  app->time.GameEnds();
  app->cores.GameEnds();
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
  ShowProfile(app);
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

// THE DISPLAY RATE (display_hz, docs/android.md, "The display rate"). The
// app asks the runtime for the rate of the row "Display rate" when the
// session runs (its start) and at each change of the row, with no new start.
// display_hz=0 (the default): no request, and the headset sets the rate. A
// change back to 0 after a request asks for no rate ("no preference" of
// XR_FB_display_refresh_rate). SteamVR takes the rate from its settings
// (steamvr_settings.h). There 0 leaves the value of SteamVR as it is. The display
// period of the frames of the session says if the headset took the rate.
void ApplyDisplayRate(App* app) {
  const int hz = app->menu.DisplayHz();
  const int64_t period = app->xr.DisplayPeriod();
  app->menu.SetDisplayNow(display_rate::HzOfPeriod(period));
  if (hz != app->display_asked) {
    app->display_asked = hz;
    app->display_waits = hz > 0;
    app->display_asked_ms = NowMs();
    if (steamvr_settings::On()) {
      if (hz > 0) {
        const bool took = steamvr_settings::WriteRate(static_cast<float>(hz));
        AppLog("steamvr      : the app sets preferredRefreshRate of %s to %d Hz (%s). SteamVR "
               "reads %.2f Hz, and the headset runs at %d Hz now",
               steamvr_settings::AppKey().c_str(), hz, took ? "taken" : "refused",
               steamvr_settings::ReadRate(), display_rate::HzOfPeriod(period));
      }
    } else {
      app->xr.RequestDisplayRate(display_rate::RuntimeRate(hz, app->xr.DisplayRates()));
    }
  }
  if (!app->display_waits) return;
  const double waited = NowMs() - app->display_asked_ms;
  if (display_rate::Reached(hz, period)) {
    app->display_waits = false;
    AppLog("xr           : the headset runs at %d Hz, %.1f s after the request", hz,
           waited / 1000.0);
  } else if (waited > 5000.0) {
    app->display_waits = false;
    AppLog("xr           : 5 s after the request of %d Hz the headset still runs at %d Hz. It "
           "can take the rate only at a new start of the app",
           hz, display_rate::HzOfPeriod(period));
  }
}

// The rates of the runtime into the menu, after the start of the session.
void TakeDisplayRates(App* app) {
  // steamvr_app_key (bwvr.ini only, empty by default): the section of the app in the
  // settings of SteamVR, in place of SteamAppId of the environment.
  const bool steamvr = steamvr_settings::Start(app->ini.Get("bwvr", "steamvr_app_key", ""));
  const std::vector<int> offered =
      steamvr ? display_rate::SteamVrOffered() : display_rate::Offered(app->xr.DisplayRates());
  app->menu.SetDisplayRates(offered);
  if (offered.empty()) {
    AppLog("menu         : display_hz=%d. The runtime gives none of the rates 72, 80, 90, "
           "120 and 144 Hz, so the menu has no row Display rate and the app asks for no rate",
           app->menu.DisplayWanted());
    return;
  }
  // The log has no word "default": the tests of the APK stop at a line with
  // "FAULT" in any case.
  std::string list;
  for (int hz : offered) list += std::to_string(hz) + " Hz, ";
  AppLog("menu         : the row Display rate of the page Performance offers %sand 0 "
         "(the headset sets the rate)",
         list.c_str());
  if (app->menu.DisplayWanted() != app->menu.DisplayHz()) {
    AppLog("menu         : display_hz=%d is not a rate of the row. The app takes "
           "display_hz=0 and asks for no rate",
           app->menu.DisplayWanted());
  } else {
    AppLog("menu         : display_hz=%d. %s", app->menu.DisplayHz(),
           app->menu.DisplayHz() > 0 ? "The app asks for this rate when the session runs"
                                     : "The app asks for no rate");
  }
}

// THE VALUES OF THE STEAM FRAME (frame_defaults.h, frame_defaults=1 by default): before the
// Vulkan instance of the session, so that the driver gets tu_debug at this start too.
void RuntimeKnown(App* app, const std::string& runtime) {
  if (!frame_defaults::IsFrameRuntime(runtime)) return;
  if (app->ini.GetInt("bwvr", "frame_defaults", 1) == 0) {
    AppLog("menu         : frame_defaults=0. The app adds no values of the Steam Frame");
    return;
  }
  const int added = frame_defaults::Seed(&app->ini);
  if (added == 0) return;
  if (!WriteTextFile(app->game.ini_path, app->ini.Text())) {
    AppLog("menu         : the values of the Steam Frame cannot go into %s, so they count only "
           "for this start of the app", app->game.ini_path.c_str());
  }
  app->menu.Load(app->ini);
  const std::string tu_debug = app->ini.Get("bwvr", "tu_debug", "");
  if (!tu_debug.empty()) setenv("TU_DEBUG", tu_debug.c_str(), 1);
  AppLog("menu         : frame_defaults=1. The runtime is %s, and %d values of the Steam Frame "
         "went into bwvr.ini", runtime.c_str(), added);
}

void Load(App* app) {
  app->game = FindGameFolder(app->folder);
  app->ini.Parse(ReadTextFile(app->game.ini_path));
  app->menu.Load(app->ini);
  // wow64_cpu (the row "Translator of the game" of the page Testing) chooses
  // the CPU DLL only on arm64-v8a. On x86_64 the row says that it does
  // nothing.
  app->menu.SetCpuChoiceCounts(wine_rules::HasCpuChoice(WineRuntime::Abi()));
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
  // time_log=1 (the default): while the game runs, each 10 s the lines of the
  // times (time_log.h). 0: no lines. Before the threads of Wine and of the
  // paint start, so they get their names.
  app.time.Start(app.ini.GetInt("bwvr", "time_log", 1) != 0);
  // trace=1 (0 by default): the timeline of the app (trace_app.h).
  AppTraceStart(app.ini.GetInt("bwvr", "trace", 0) != 0, app.ini.GetInt("bwvr", "trace_seconds", 10),
                app.ini.GetInt("bwvr", "trace_min_us", 30),
                app.ini.GetInt("bwvr", "trace_combo_ms", 300),
                app.ini.GetInt("bwvr", "trace_combo2", 1) != 0,
                app.ini.GetInt("bwvr", "trace_shift", 1) != 0, app.folder.c_str(),
                app.game.folder.c_str());
  TimeLogNameThread("bwvr main");
  ThreadCores::MarkSession();
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
    // wine_owner_check=2: Wine accepts each owner of its folders only in a
    // container that shows another owner (Lepton of the Steam Frame).
    wine.owner_check = app.ini.GetInt("bwvr", "wine_owner_check", 2);
    // wine_sigsys=2: in that container a system call that its seccomp
    // filter traps gives -ENOSYS in Wine, and does not stop the process.
    wine.sigsys = app.ini.GetInt("bwvr", "wine_sigsys", 2);
    // The tree must be in the internal folder: Android runs no program of
    // the shared storage.
    if (android->activity->internalDataPath == nullptr) {
      AppLog("wine         : Android gives the app no internal folder. The app does not "
             "set up Wine");
    } else {
      const std::string files = android->activity->internalDataPath;
      mkdir(files.c_str(), 0700);
      app.prefix = wine_rules::MakePaths(files, app.folder, WineRuntime::Abi()).prefix;
      app.wine.Start(android->activity->assetManager, files, app.folder, app.game.folder, wine);
    }
  }
  // pc_profile=1 (the default): "Play" imports the values of the profile of
  // bwvr-profile.reg in the game folder (push_profile.ps1) into the registry
  // of the prefix, one time for each new file (play_rules.h). 0: no import,
  // the game has the profile that it made in the headset, or none.
  app.pc_profile = app.ini.GetInt("bwvr", "pc_profile", 1) != 0;
  AppLog("profile      : pc_profile=%d. %s", app.pc_profile ? 1 : 0,
         app.pc_profile ? "Play imports the profile of bwvr-profile.reg one time for each new file"
                        : "Play imports no profile of the PC");
  ShowProfile(&app);
  if (!app.painter.Start(android->activity->vm,
                         app.capture ? app.game.folder + "/bwvr-menu.bmp" : "")) {
    AppLog("app          : the canvas of Android did not start. The app stops");
    ANativeActivity_finish(android->activity);
  }

  app.xr.SetCopyOnChange(app.ini.GetInt("bwvr", "xr_copy_on_change", 1) != 0);
  // THE MOTION OF THE STICK (motion_comp.h). The menu of the app changes
  // motion_comp while the game runs (Save).
  {
    MotionComp::Keys k;
    k.mode = app.ini.GetInt("bwvr", "motion_comp", k.mode);
    k.depth_cm = app.ini.GetInt("bwvr", "motion_comp_depth_cm", k.depth_cm);
    k.most_cm = app.ini.GetInt("bwvr", "motion_comp_most_cm", k.most_cm);
    k.most_deg = app.ini.GetInt("bwvr", "motion_comp_most_deg", k.most_deg);
    k.most_scale_pct = app.ini.GetInt("bwvr", "motion_comp_most_scale_pct", k.most_scale_pct);
    k.predict_ms = app.ini.GetInt("bwvr", "motion_comp_predict_ms", k.predict_ms);
    app.xr.Motion().SetKeys(k);
  }
  // xr_layer_rect (0 by default) and xr_staging_direct (1 by default): the
  // pixels of the CPU of the layers with the frame loop (XrApp::SetLayerRect,
  // SetStagingDirect).
  const bool layer_rect = app.ini.GetInt("bwvr", "xr_layer_rect", 0) != 0;
  const bool staging_direct = app.ini.GetInt("bwvr", "xr_staging_direct", 1) != 0;
  app.xr.SetLayerRect(layer_rect);
  app.xr.SetStagingDirect(staging_direct);
  AppLog("xr           : xr_layer_rect=%d, xr_staging_direct=%d. %s, and %s", layer_rect ? 1 : 0,
         staging_direct ? 1 : 0,
         layer_rect ? "A card copies only the rect that its quad shows"
                    : "A card copies its whole image",
         staging_direct ? "new pixels of the CPU go straight into a free staging buffer"
                        : "new pixels of the CPU go into a copy in memory first");
  // gpu_parts=1 (the default): the GPU parts of the walk in the time log
  // (walk_probe.h). 0, or time_log=0: the table of the renderer stays as it
  // is.
  WalkProbeSetOn(walk_parts::On(app.ini.GetInt("bwvr", "gpu_parts", 1)) && app.time.On());
  if (!WalkProbeOn()) {
    AppLog("gpu parts    : gpu_parts=%d, time_log=%d. The time log has no GPU parts of the walk",
           app.ini.GetInt("bwvr", "gpu_parts", 1), app.time.On() ? 1 : 0);
  }
  // frame_loop_queues=2 (the default): with the frame loop the renderer has
  // a queue of its own when the device has one. 1: one queue for all.
  app.xr.SetQueues(app.ini.GetInt("bwvr", "frame_loop_queues", 2));
  // xr_density_pass=1 (0 by default): the device gets the extension of the
  // density map, and the runtime gets the probe of the one pass. The key is read
  // again at each Play, but the device is made one time, at the start.
  app.xr.SetDensityWanted(app.ini.GetInt("bwvr", "xr_density_pass", 0) != 0);
  // tu_debug (bwvr.ini only, empty by default): TU_DEBUG of the Vulkan driver Turnip, for a
  // test of its ways to draw, for example sysmem. The driver reads it when the session
  // makes its Vulkan instance, so a change counts at the next start of the app.
  {
    const std::string tu_debug = app.ini.Get("bwvr", "tu_debug", "");
    if (!tu_debug.empty()) {
      setenv("TU_DEBUG", tu_debug.c_str(), 1);
      AppLog("gpu          : tu_debug=%s. The Vulkan driver gets TU_DEBUG with this value",
             tu_debug.c_str());
    }
  }
  // eye_size_max=1 (the default): the size of the eyes that the app takes
  // from the runtime has a cap, the panel of a Quest 3 (2064 by 2208) with the
  // aspect of the runtime (eye_size.h). 0: the size of the runtime.
  app.xr.SetEyeSizeMax(app.ini.GetInt("bwvr", "eye_size_max", 1));
  app.xr.SetTimeLog(app.time.On());
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
  // play_menu_sticks=1: a hold of the clicks of both sticks for
  // play_menu_hold_ms shows the menu over the game, and a press of both
  // hides it (the Steam Frame keeps the menu button for the system). 0: only
  // the menu button.
  app.play_menu_sticks = app.ini.GetInt("bwvr", "play_menu_sticks", 1) != 0;
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
  // THE GAZE OF AN EYE TRACKER for the fovea of the mod (docs/backend.md,
  // "Foveated rendering"): with foveation or fovea_res on (or test_keys=1,
  // whose keys can turn them on) and a source of foveation_gaze that reads
  // OpenXR.
  {
    const osc_gaze_math::Source source = osc_gaze_math::SourceOf(
        app.ini.GetInt("bwvr", "foveation_gaze", 1));
    const bool fovea = app.ini.GetInt("bwvr", "foveation", 0) != 0 ||
                       app.ini.GetInt("bwvr", "fovea_res", 0) != 0 ||
                       app.ini.GetInt("bwvr", "test_keys", 0) != 0;
    const bool gaze = fovea && (osc_gaze_math::UsesOpenXr(source) ||
                                app.ini.GetInt("bwvr", "test_keys", 0) != 0);
    app.xr.SetEyeGaze(gaze);
    AppLog("gaze         : foveation_gaze=%d. %s", static_cast<int>(source),
           gaze ? "The app reads the gaze of an eye tracker for the fovea"
                : "The app reads no gaze: no foveated rendering, or a source "
                  "with no OpenXR");
  }
  app.xr.SetRuntimeKnown([&app](const std::string& runtime) { RuntimeKnown(&app, runtime); });
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
    app.xr.OldLoopSync(false);
    Picture picture;
    if (app.xr_on) GatherPicture(&app, app.screen_pictures, &picture);
    if (app.xr_on && !ShowPicture(&app, picture)) {
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
    TakeDisplayRates(&app);
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
      ApplyDisplayRate(&app);
      XrInput input;
      if (!app.xr.BeginFrame(&input)) continue;
      // THE STEPS of this frame for the time log (XrApp::Step).
      const bool steps = app.time.On();
      double step_at = steps ? NowMs() : 0.0;
      const auto step = [&](XrApp::Step which) {
        if (!steps) return;
        const double now = NowMs();
        app.xr.AddStep(which, now - step_at);
        step_at = now;
      };
      MenuButton(&app, input);
      step(XrApp::kStepUi);
      // The input of the mod: after xrSyncActions of BeginFrame, with the
      // views of xrLocateViews at the display time of this frame.
      input_channel::SlotData slot = {};
      if (app.input_live.On()) {
        const int64_t trace_slot = g_app_trace ? AppTraceNow() : 0;
        app.xr.InputSlot(&slot);
        if (trace_slot != 0) {
          static const trace_events::Def kInputDef = {"input slot (xrLocateViews, xrLocateSpace)", "session", {"slot", nullptr, nullptr, nullptr}};
          AppTraceBar(&kInputDef, trace_slot, AppTraceNow(), static_cast<float>(slot.frame));
          // The button combination, and the saves of the game.
          if (AppTraceSession(slot, app.input_live.Channel())) {
            app.xr.PulseWith(0, 0.8f, 0.15f);
            app.xr.PulseWith(1, 0.8f, 0.15f);
          }
        }
        if (app.game_on) {
          play_rules::MaskInput(&slot, app.menu_over_game);
          if (!app.menu_over_game) play_rules::ApplyMenuBits(&slot, app.menu_step);
        }
        app.input_live.Write(&slot);
        if (app.take_phase.On() && app.game_on) app.take_phase.Wrote(slot.frame, NowMs());
        app.input_live.PlayPulses(&app.xr);
        app.period_ms = slot.display_period > 0 ? static_cast<double>(slot.display_period) / 1e6 : 0.0;
      }
      step(XrApp::kStepSlot);
      // THE PLACE OF THE PLAYER NOW and THE TICK OF THE PLAY THREAD of this frame.
      // With the late take they come before the take, so a take that waits does not
      // make the next picture late. Else they come after the take.
      input_channel::World now_world;
      bool now_has = false;
      motion_comp_math::SlotPush now_slots[64];
      int now_count = 0;
      const auto read_place = [&]() {
        now_has = app.input_live.ReadWorld(&now_world);
        now_count = app.input_live.SlotsAfter(now_has ? now_world.input_frame : 0u, now_slots, 64);
      };
      float late_head[3] = {0.0f, 0.0f, 0.0f};
      bool late_head_ok = false;
      const auto locate_head = [&]() {
        const int64_t next_time = app.xr.DisplayTime() + app.xr.DisplayPeriod();
        late_head_ok = app.xr.LocateHead(next_time, late_head);
        // THE LATE GRAB: the grips at the same time, eased as the mod eases them.
        const bool grab = now_has && app.late_grab_live.load() &&
                          (now_world.flags & input_channel::kWorldGrab) != 0;
        float grips[2][3] = {};
        bool grips_ok[2] = {false, false};
        if (grab) app.xr.LocateGrips(next_time, grips, grips_ok);
        const float period_ms = static_cast<float>(static_cast<double>(app.xr.DisplayPeriod()) / 1e6);
        const float share = app.late_grab_ms > 0.0f ? 1.0f - expf(-period_ms / app.late_grab_ms) : 1.0f;
        late_camera_math::GrabStep(grab ? &now_world : nullptr, grips, grips_ok, share,
                                   &app.late_grab_filter);
      };
      const auto send_tick = [&]() {
        {
          std::lock_guard<std::mutex> lock(app.late_lock);
          LateTick& t = app.late_tick;
          ++t.serial;
          t.sent_ms = NowMs();
          t.slot = app.input_live.NewestSlot();
          t.period = app.xr.DisplayPeriod();
          t.head_ok = late_head_ok;
          if (late_head_ok) memcpy(t.head, late_head, sizeof(t.head));
          t.has_world = now_has;
          if (now_has) t.world = now_world;
          t.count = now_count;
          memcpy(t.slots, now_slots, static_cast<size_t>(now_count) * sizeof(now_slots[0]));
          t.grab = app.late_grab_filter;
        }
        app.late_cond.notify_one();
      };
      const bool late_frame = app.late_on && app.play_running && app.input_live.On();
      const int take_most = late_frame && app.late_live.load() && app.late_take_live.load()
                                ? app.late_take_ms
                                : 0;
      // The thread of the session alone changes the serial.
      const uint64_t tick_before = app.late_tick.serial;
      if (take_most > 0) {
        read_place();
        locate_head();
        send_tick();
      }
      app.take_wait_now = 0.0;
      // THE PICTURE OF THIS FRAME at the place of the play of the old loop:
      // with frame_loop=1 the newest picture of the ring that the GPU
      // finished, else the play of the batch on this thread.
      bool fresh = false;
      double taken_ms = -1.0;  // the clock at the take of the frame loop, -1 with none
      if (app.play_running) {
        // frame_loop_hold: a new picture only after the hold of the shown one.
        if (eye_copy_math::HoldTakes(app.hold, app.hold_shown, app.hold_any)) {
          if (take_most > 0) WaitTickPicture(&app, tick_before, take_most);
          const double take_ms =
              app.latch_log.On() || app.take_phase.On() ? NowMs() : 0.0;
          taken_ms = take_ms;
          const int64_t trace_take = g_app_trace ? AppTraceNow() : 0;
          PictureRing::TakeFacts facts;
          fresh = TakePicture(&app, &facts);
          if (trace_take != 0) {
            static const trace_events::Def kTakeDef = {"take of the newest picture (TakePicture)", "session", {"new", "#frame", nullptr, nullptr}};
            AppTraceBar(&kTakeDef, trace_take, AppTraceNow(), fresh ? 1.0f : 0.0f,
                        trace_events::FrameArg(app.shown_last));
          }
          // slow_frame_ms: why a take had no new picture (long_frames.h).
          if (app.latch_log.On() && app.game_on) {
            app.latch_log.Take(take_ms, fresh, app.shown_arrival_ms, app.shown_ready_ms,
                               app.shown_gap_ms, app.shown_last, app.period_ms);
            app.latch_log.Cadence(fresh, facts.delay, facts.replaced, facts.dropped);
          }
        }
        if (g_app_trace && app.game_on && app.first_frame && !fresh) {
          static const trace_events::Def kNoPicDef = {"frame of the session with no new picture", "session", {"#shown frame", nullptr, nullptr, nullptr}};
          AppTracePoint(&kNoPicDef, trace_events::FrameArg(app.shown_last));
        }
        if (fresh) app.hold_any = true;
        app.hold_shown = eye_copy_math::NextShown(app.hold_shown, fresh);
        step(XrApp::kStepTake);
      } else if (app.live) {
        fresh = PlayLive(&app);
        step(XrApp::kStepPlay);
      }
      // THE AGE of the shown picture at the display time of this frame.
      if (app.game_on && app.first_frame) {
        app.xr.ShownPicture(app.shown_play_ms, app.shown_arrival_ms, fresh);
        app.xr.ShownParts(app.input_live.SlotMs(app.shown_input_frame), app.shown_ready_ms,
                          app.shown_taken_ms);
      }
      // THE PLACE OF THE PLAYER NOW for the views of this frame (motion_comp,
      // docs/android.md, "The motion of the stick"): the newest place that
      // the mod wrote into the input channel, after the take of the picture.
      {
        if (take_most <= 0) read_place();
        app.xr.Motion().FrameNow(now_has, now_world, now_slots, now_count);
        // THE LATE CAMERA: the tick of the play thread, with the head at the next
        // display time.
        if (late_frame) {
          if (take_most <= 0) locate_head();
          // A TEST AID (pose_log.h).
          if (app.pose_log.On() && app.game_on) {
            PoseRecord r = {};
            r.t_ms = NowMs();
            r.display_time = app.xr.DisplayTime();
            r.slot = slot.frame;
            r.fresh = fresh ? 1u : 0u;
            for (int e = 0; e < 2; ++e) memcpy(r.eye[e], slot.eyes[e].position, sizeof(r.eye[e]));
            memcpy(r.orientation, slot.eyes[0].orientation, sizeof(r.orientation));
            memcpy(r.fov, slot.eyes[0].fov, sizeof(r.fov));
            memcpy(r.next_head, late_head, sizeof(r.next_head));
            r.pic_frame = app.shown_last;
            r.pic_input = app.shown_body.input_frame;
            r.pic_again = app.shown_again ? 1u : 0u;
            memcpy(r.pic_eye, app.shown_body.eyes[0].position, sizeof(r.pic_eye));
            memcpy(r.pic_orientation, app.shown_body.eyes[0].orientation, sizeof(r.pic_orientation));
            memcpy(r.pic_place, app.shown_place, sizeof(r.pic_place));
            memcpy(r.pic_move, app.shown_move, sizeof(r.pic_move));
            r.wait_ms = static_cast<float>(app.xr.LastWaitMs());
            r.tick = static_cast<uint32_t>(tick_before + 1);
            r.pic_tick = static_cast<uint32_t>(app.shown_tick);
            r.take_wait_ms = static_cast<float>(app.take_wait_now);
            r.pic_work_ms = static_cast<float>(app.shown_work_ms);
            app.pose_log.Add(r);
          }
          if (take_most <= 0) send_tick();
        }
      }
      UpdateKeyboard(&app);
      bool show_cursor = false;
      float cursor[3] = {0, 0, 0};
      OnControllers(&app, input, &show_cursor, cursor);
      Paint(&app);
      step(XrApp::kStepUi);
      const int64_t trace_end = g_app_trace ? AppTraceNow() : 0;
      app.xr.EndFrame(quad.center, menu_size, show_cursor, cursor,
                      kCursorMeters);
      if (trace_end != 0) {
        static const trace_events::Def kEndDef = {"EndFrame of the app (layers, copies, xrEndFrame)", "session", {nullptr, nullptr, nullptr, nullptr}};
        AppTraceBar(&kEndDef, trace_end, AppTraceNow());
      }
      // THE AGE LOG: the line of the window each 10 s (take_age_log=1).
      if (app.game_on) app.xr.AgeTick(NowMs());
      // THE PHASE OF THE TAKE of this frame of the session (long_frames.h).
      if (app.take_phase.On() && app.game_on) {
        if (app.first_frame) {
          app.take_phase.SessionFrame(app.xr.WaitEndMs(), app.xr.LastWaitMs(), taken_ms, NowMs(),
                                      app.period_ms, app.xr.DisplayTime(),
                                      app.xr.DisplayPeriod(), app.xr.DisplayTimeMs());
        }
        app.take_phase.Tick(NowMs());
      }
      // THE TIME LOG of this frame of the session.
      if (app.time.On()) {
        XrApp::FrameTimes times;
        app.xr.TakeTimes(&times);
        if (app.game_on) {
          const double now = NowMs();
          if (app.play_running) {
            PlayStats s;
            {
              std::lock_guard<std::mutex> lock(app.play_stats_lock);
              s = app.play_stats;
              app.play_stats = PlayStats();
            }
            PictureRing::Stats r;
            app.ring.TakeStats(&r);
            s.slot_wait.Merge(r.slot_wait);
            s.busy.Merge(r.busy);
            s.dropped += r.dropped;
            s.replaced += r.replaced;
            app.time.Play(s);
          }
          app.time.Shown(fresh);
          app.time.SessionFrame(now, times);
          app.time.Tick(now, app.live_reader.Arrived(),
                        app.first_frame ? app.live_reader.LastArrivalMs() : 0.0, app.shown_last);
        }
      }
      // THE LONG FRAMES (slow_frame_ms): the process of the game for the
      // reader (the time log finds it), and the line of each 10 s.
      if (app.latch_log.On() && app.game_on) {
        app.frame_clock.SetGamePid(app.time.GamePid());
        app.latch_log.Tick(NowMs());
      }
    } else {
      // With no session the loop runs each 5 ms for the live stream.
      if (app.live) PlayLive(&app);
      if (!app.window || !app.flat.Ready()) continue;
      Paint(&app);
      if (app.flat_dirty && app.flat.Present()) app.flat_dirty = false;
    }
  }

  StopPlayThread(&app);
  AppTraceStop();
  app.cores.GameEnds();
  if (app.live) {
    app.live_reader.Stop();
    if (!app.live_play.quiet) LiveSummary(&app, app.live_play, "the app ends");
  }
  app.input_live.WriteNoInput();
  app.input_live.Stop();
  if (app.game_on) AppLog("game         : the app ends, so the game ends");
  app.wine.Stop();
  AppLog("app          : the app ends");
  app.ring.Stop();
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
