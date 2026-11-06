// The menus of the game on a panel that is fixed in the room (docs/ui-layer.md,
// "The menus in the room").
//
// The game draws each menu (the Main Menu, the game menu of Esc, the
// options, the load and save boxes, the boxes of the start) as a SetupBox
// dialog in screen space. In the eye pictures it is then fixed to the head.
// With menu_room=1 and a headset, the menu goes to a panel that stays at one
// place in the room:
//
//   A FRAME WITH THE SCENE. The draws of the Setup dialogs go to the panel
//   target in the first eye pass, and not to the eye pictures
//   (draw_hooks.cpp, UiPanelRouteMenu). The menu rect of the target goes out
//   as one quad layer over the projection layer (stereo.cpp), as with
//   ui_panel=1. The rest of the interface keeps its own route.
//
//   A FRAME WITH NO SCENE (the front end of the start, FrontEnd::JustDoABox,
//   and the other loops with no eye passes). The game draws the whole frame
//   flat. The menu rect of that picture goes out as a screen at the place of
//   the panel, the same picture for both eyes (XrScreen::StageWindow with a
//   rect). The hands, the pointer and the keyboard of the frame run here,
//   because no eye pass runs them.
//
// THE PLACE. The panel goes in front of the head in the first frame of a
// menu: at the yaw of the head and the height of the head, ui_menu_distance_cm
// (140) away, ui_menu_pitch_deg (-10) below eye level, at
// ui_menu_px_per_deg (16). A change from one menu to the next keeps the
// place: the menu must be closed for kKeepMs before the next one gets a new
// place. A recenter (F11, or the headset menu) puts the panel in front of
// the head of that moment. The panel never follows the head.
//
// THE POINTER. The aim ray of the pointing hand meets the panel, and the
// pixel under the ray is the pixel of the render that the game drew the
// dialog at. So the hit test of the game reads that pixel (controllers.cpp,
// Compute). A ray past the edge of the panel keeps the pointer at the edge,
// and a press there does not reach the game.
//
//   menu_room=1       0: the menus stay in the eye pictures, fixed to the head
//   menu_room_test=1  a test key: the route of a frame with the scene runs
//                     with no headset, so a desktop run shows the eye
//                     pictures with no menu, and the pointer on the panel

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>

#include "../common/bw_addresses.h"
#include "../vr/xr_screen.h"
#include "menu_room.h"
#include "menu_room_math.h"
#include "ui_panel.h"

extern void ModLog(const char* format, ...);

// head_pose.cpp
uint64_t HeadPoseOriginSerial();
// controllers.cpp
void ControllersFrame();
// pointer_map.cpp
void PointerMapApply();
// ui_panel.cpp
void UiPanelMenuRect(uint32_t w, uint32_t h, int out[4]);
// draw_hooks.cpp
uint64_t UiDepthMenuBatches();

namespace {

bwvr::XrScreen* g_screen = nullptr;
bool g_on = false;        // menu_room=1 and ui_panel=0
bool g_test = false;      // menu_room_test=1
bool g_said_options = false;

// SetupBox::GetCurrentActiveBox 0x00407ED0 returns [0x00C4CD20]. A value
// other than 0 is a box: a menu is up.
const uintptr_t kActiveBox = 0x00C4CD20;
bool MenuUp() { return *reinterpret_cast<const uintptr_t*>(kActiveBox) != 0; }

// The keyboard: below the menu panel and nearer, tilted to face the head.
// It is a size that a ray can hit with ease: 70 cm at 1 m is 38 degrees.
const float kKeyboardDistance = 1.0f;     // meters from the anchor
const float kKeyboardPitch = -0.6109f;    // 35 degrees below eye level
const float kKeyboardWidth = 0.70f;       // meters
const float kKeyboardAspect = 0.375f;     // the card is 1024 by 384 pixels

// A menu that closes and opens again in this time keeps its place. A box
// that leads to the next box closes for some frames between the two.
const ULONGLONG kKeepMs = 1000;

// The head of the last frame, in the LOCAL space.
float g_head[3] = {0.0f, 0.0f, 0.0f};
float g_head_q[4] = {0.0f, 0.0f, 0.0f, 1.0f};
bool g_have_head = false;

// THE PLACE.
bool g_placed = false;
float g_anchor[3] = {0.0f, 0.0f, 0.0f};
float g_yaw = 0.0f;
uint64_t g_placed_serial = 0;
ULONGLONG g_last_up = 0;
menu_room_math::Panel g_panel = {};
menu_room_math::Panel g_keyboard = {};
int g_rect[4] = {0, 0, 0, 0};
uint32_t g_rect_w = 0, g_rect_h = 0;  // the render of g_rect

// The latches of a frame.
bool g_frame_on = false;   // this stereo frame has the menus on the panel
bool g_flat_on = false;    // the last frame with no scene had them
ULONGLONG g_flat_tick = 0;
float g_trigger = 0.0f;
bool g_have_key_ray = false;
float g_key_u = 0.0f, g_key_v = 0.0f;

// The log: one line for each change.
int g_said_route = -1;
const char* g_said_why = nullptr;
bool g_said_flat = false;
uint64_t g_frames_room = 0;   // stereo frames with the menus on the panel
uint64_t g_frames_flat = 0;   // frames with no scene on the screen
uint64_t g_places = 0;
uint64_t g_ray_frames = 0;    // frames with the ray on the panel
uint64_t g_menu_batches = 0;  // UiDepthMenuBatches at the last frame
ULONGLONG g_drew_tick = 0;    // the last frame with no scene that drew a menu
int g_ray_logs = 0;
const int kMaxRayLogs = 20;

bool RenderSize(uint32_t* w, uint32_t* h) {
  const bw::tech::InfoTransform* info =
      reinterpret_cast<const bw::tech::InfoTransform*>(bw::tech::kInfoTransform);
  if (IsBadReadPtr(info, sizeof(*info)) || info->res_x <= 0 ||
      info->res_y <= 0) {
    return false;
  }
  *w = static_cast<uint32_t>(info->res_x);
  *h = static_cast<uint32_t>(info->res_y);
  return true;
}

// The panel and the keyboard for the anchor, the rect of a render of w by h.
bool Build(uint32_t w, uint32_t h) {
  const UiPanelSettings& set = UiPanelOptions();
  UiPanelMenuRect(w, h, g_rect);
  g_rect_w = w;
  g_rect_h = h;
  const float distance = set.menu_distance_cm * 0.01f;
  const float pitch = set.menu_pitch_deg * 0.01745329f;
  if (!menu_room_math::PlaceMenu(g_anchor, g_yaw, distance, pitch,
                                 static_cast<float>(g_rect[2]),
                                 static_cast<float>(g_rect[3]),
                                 static_cast<float>(set.menu_px_per_deg),
                                 &g_panel)) {
    return false;
  }
  menu_room_math::PoseAt(g_anchor, g_yaw, kKeyboardDistance, kKeyboardPitch,
                         &g_keyboard);
  g_keyboard.width = kKeyboardWidth;
  g_keyboard.height = kKeyboardWidth * kKeyboardAspect;
  return true;
}

// Once a frame with a menu up: the place of the panel. False when there is
// no place yet (no head).
bool Track(uint32_t w, uint32_t h, uint64_t frame) {
  const ULONGLONG now = GetTickCount64();
  const uint64_t serial = HeadPoseOriginSerial();
  const bool closed_long = g_last_up == 0 || now - g_last_up > kKeepMs;
  const bool recenter = g_placed && serial != g_placed_serial;
  g_last_up = now;
  if (g_placed && !closed_long && !recenter) {
    // The render changed its size: the same place, a new rect.
    if (w != g_rect_w || h != g_rect_h) Build(w, h);
    return true;
  }
  if (!g_have_head) return g_placed;
  for (int i = 0; i < 3; ++i) g_anchor[i] = g_head[i];
  g_yaw = menu_room_math::YawOf(g_head_q);
  g_placed_serial = serial;
  if (!Build(w, h)) {
    g_placed = false;
    return false;
  }
  g_placed = true;
  ++g_places;
  ModLog("menu room    : frame %llu, the menu panel goes in front of the head%s: "
         "%.2f by %.2f m, its center at %.2f %.2f %.2f m, the head at %.2f "
         "%.2f %.2f m with a yaw of %.1f degrees. It shows the rect %d %d %d "
         "%d of the %u by %u render", frame,
         recenter ? " after a recenter" : "", g_panel.width, g_panel.height,
         g_panel.position[0], g_panel.position[1], g_panel.position[2],
         g_head[0], g_head[1], g_head[2], g_yaw * 57.29578f, g_rect[0],
         g_rect[1], g_rect[2], g_rect[3], w, h);
  return true;
}

void SayRoute(bool on, const char* why, uint64_t frame) {
  const int now = on ? 1 : 0;
  if (now == g_said_route && why == g_said_why) return;
  g_said_route = now;
  g_said_why = why;
  if (on) {
    ModLog("menu room    : frame %llu, the menus go to the panel in the room, "
           "and not to the eye pictures%s", frame,
           g_screen != nullptr && g_screen->running()
               ? ""
               : " (menu_room_test=1: no headset)");
  } else {
    ModLog("menu room    : frame %llu, the menus stay in the eye pictures: %s",
           frame, why);
  }
}

}  // namespace

void MenuRoomSetScreen(bwvr::XrScreen* screen) { g_screen = screen; }

void MenuRoomSetOptions(bool on, bool test, bool keyboard, bool panel_off) {
  g_on = on && panel_off;
  g_test = test && g_on;
  VrKeyboardSetOptions(keyboard);
  if (g_said_options) return;
  g_said_options = true;
  ModLog("menu room    : menu_room=%d, %s%s. vr_keyboard=%d, %s",
         on ? 1 : 0,
         !on ? "the menus stay in the eye pictures, fixed to the head"
             : (panel_off ? "in the headset the menus show on a panel that "
                            "is fixed in the room"
                          : "but ui_panel is not 0, so the interface panel "
                            "of ui_panel has the menus"),
         g_test ? ". menu_room_test=1: the route runs with no headset" : "",
         keyboard ? 1 : 0,
         keyboard ? "a keyboard shows under the menu panel while a text box "
                    "has the focus"
                  : "no keyboard in the headset");
}

bool MenuRoomEnabled() { return g_on; }

void MenuRoomHead(const float center[3], const float orientation[4]) {
  for (int i = 0; i < 3; ++i) g_head[i] = center[i];
  for (int i = 0; i < 4; ++i) g_head_q[i] = orientation[i];
  g_have_head = true;
}

bool MenuRoomHeadPose(float center[3], float orientation[4]) {
  if (!g_have_head) return false;
  for (int i = 0; i < 3; ++i) center[i] = g_head[i];
  for (int i = 0; i < 4; ++i) orientation[i] = g_head_q[i];
  return true;
}

bool MenuRoomBeginFrame(bool to_headset, bool projection, uint64_t frame) {
  g_frame_on = false;
  g_flat_on = false;
  g_have_key_ray = false;
  if (!g_on) return false;
  if (!MenuUp()) {
    if (g_said_route == 1) SayRoute(false, "no menu is up", frame);
    VrKeyboardHide();
    return false;
  }
  const char* why = nullptr;
  if (to_headset) {
    if (!projection) {
      why = "the frame has no projection layer";
    } else if (!GpuBridgePanelShared()) {
      why = "the panel target is not shared with the headset";
    } else if (g_screen == nullptr || !g_screen->panel_ready()) {
      why = "the headset session has no swapchain for the panel";
    } else if (bwvr::XrScreen::PanelFailed()) {
      why = "the runtime refused a frame with the panel";
    }
  } else if (!g_test) {
    why = "no headset";
  } else if (!GpuBridgePanelUsable()) {
    why = "there is no panel target";
  }
  uint32_t w = 0, h = 0;
  if (why == nullptr && !RenderSize(&w, &h)) why = "the render has no size";
  if (why == nullptr && !Track(w, h, frame)) why = "there is no head pose";
  if (why != nullptr) {
    SayRoute(false, why, frame);
    VrKeyboardHide();
    return false;
  }
  SayRoute(true, nullptr, frame);
  g_frame_on = true;
  ++g_frames_room;
  return true;
}

bool MenuRoomFrame() { return g_frame_on; }

bool MenuRoomQuad(uint32_t render_w, uint32_t render_h,
                  bwvr::PanelQuad* out) {
  out->visible = false;
  if (!g_frame_on || !g_placed) return false;
  if (render_w != g_rect_w || render_h != g_rect_h) Build(render_w, render_h);
  for (int i = 0; i < 3; ++i) out->position[i] = g_panel.position[i];
  for (int i = 0; i < 4; ++i) out->orientation[i] = g_panel.orientation[i];
  out->width = g_panel.width;
  out->height = g_panel.height;
  for (int i = 0; i < 4; ++i) out->rect[i] = g_rect[i];
  out->visible = UiPanelDrew(kPanelMenu) && g_panel.width > 0.0f;
  return out->visible;
}

bool MenuRoomFlatFrame(uint64_t frame) {
  g_flat_on = false;
  g_frame_on = false;
  g_have_key_ray = false;
  if (!g_on || g_screen == nullptr || !g_screen->running() ||
      g_screen->eyes() < 2) {
    return false;
  }
  // A menu is up, and the Setup callback drew it in this frame. The value of
  // the active box is not 0 in some loops that show no menu, for example
  // while the Lionhead logo plays. A frame with no draw of the menu soon
  // after one with a draw keeps the screen: JustDoABox makes one frame with
  // no callbacks when a box opens.
  const uint64_t batches = UiDepthMenuBatches();
  const bool drew = batches != g_menu_batches;
  g_menu_batches = batches;
  const ULONGLONG now = GetTickCount64();
  if (!MenuUp() || (!drew && (g_drew_tick == 0 || now - g_drew_tick > 200))) {
    VrKeyboardHide();
    return false;
  }
  if (drew) g_drew_tick = now;
  const bwvr::EyeView& left = g_screen->eye(0);
  const bwvr::EyeView& right = g_screen->eye(1);
  if (left.valid && right.valid) {
    float center[3];
    for (int i = 0; i < 3; ++i) {
      center[i] = (left.position[i] + right.position[i]) * 0.5f;
    }
    MenuRoomHead(center, left.orientation);
  }
  // The picture of a frame with no scene has the size of the eye swapchain.
  const uint32_t w = g_screen->width();
  const uint32_t h = g_screen->height();
  if (w == 0 || h == 0 || !Track(w, h, frame)) return false;
  g_flat_on = true;
  g_flat_tick = GetTickCount64();
  ++g_frames_flat;

  // No eye pass runs the hands and the pointer in this frame.
  ControllersFrame();
  PointerMapApply();
  VrKeyboardFrame(frame);

  bwvr::WindowLayer layer = {};
  layer.screen = true;
  for (int i = 0; i < 3; ++i) layer.position[i] = g_panel.position[i];
  for (int i = 0; i < 4; ++i) layer.orientation[i] = g_panel.orientation[i];
  layer.width = g_panel.width;
  layer.height = g_panel.height;
  for (int i = 0; i < 4; ++i) layer.rect[i] = g_rect[i];
  bwvr::XrScreen::StageWindow(layer);
  if (!g_said_flat) {
    g_said_flat = true;
    ModLog("menu room    : frame %llu, a frame with no scene shows a menu, "
           "so the part %d %d %d %d of its picture goes to the panel in the "
           "room, and the controller points on it", frame, g_rect[0],
           g_rect[1], g_rect[2], g_rect[3]);
  }
  return true;
}

bool MenuRoomFlatNow() {
  return g_flat_on && GetTickCount64() - g_flat_tick < 250;
}

bool MenuRoomSurface() { return g_frame_on || MenuRoomFlatNow(); }

bool MenuRoomRayPixel(float trigger, const float aim_position[3],
                      const float aim_orientation[4], float* x, float* y,
                      bool* off, float* meters) {
  g_trigger = trigger;
  g_have_key_ray = false;
  if (!g_placed || !MenuRoomSurface()) return false;
  float direction[3];
  menu_room_math::AimForward(aim_orientation, direction);
  // The keyboard is nearer than the panel, so a ray on it ends there.
  float ku = 0.0f, kv = 0.0f, kt = 0.0f;
  if (VrKeyboardShown() &&
      menu_room_math::Hit(aim_position, direction, g_keyboard, &ku, &kv,
                          &kt) &&
      menu_room_math::Inside(ku, kv)) {
    g_have_key_ray = true;
    g_key_u = ku;
    g_key_v = kv;
  }
  float u = 0.0f, v = 0.0f, t = 0.0f;
  if (!menu_room_math::Hit(aim_position, direction, g_panel, &u, &v, &t)) {
    if (!g_have_key_ray) return false;
    // The pointer keeps its last pixel. A press does not reach the game.
    *off = true;
    *meters = kt;
    return false;
  }
  menu_room_math::Pixel(u, v, g_rect, x, y);
  *off = !menu_room_math::Inside(u, v) || g_have_key_ray;
  *meters = g_have_key_ray ? kt : t;
  ++g_ray_frames;
  if (g_ray_logs < kMaxRayLogs && (g_ray_frames % 45) == 1) {
    ++g_ray_logs;
    ModLog("menu room    : the ray meets the menu panel %.2f m from the hand "
           "at %.3f %.3f of the panel, and the pixel is %.1f %.1f%s", t, u, v,
           *x, *y,
           g_have_key_ray ? ". The ray is on the keyboard"
                          : (*off ? ". It is past the edge of the panel"
                                  : ""));
  }
  return true;
}

bool MenuRoomKeyboardPlace(float position[3], float orientation[4],
                           float* width, float* height) {
  if (!g_placed) return false;
  for (int i = 0; i < 3; ++i) position[i] = g_keyboard.position[i];
  for (int i = 0; i < 4; ++i) orientation[i] = g_keyboard.orientation[i];
  *width = g_keyboard.width;
  *height = g_keyboard.height;
  return true;
}

bool MenuRoomKeyboardRay(float* u, float* v) {
  if (!g_have_key_ray) return false;
  *u = g_key_u;
  *v = g_key_v;
  return true;
}

float MenuRoomTrigger() { return g_trigger; }

void MenuRoomReport() {
  if (g_frames_room + g_frames_flat == 0) return;
  ModLog("menu room    : %llu frames with the scene and %llu frames with no "
         "scene had the menus on the panel in the room, %llu places, %llu "
         "frames with the ray on the panel", g_frames_room, g_frames_flat,
         g_places, g_ray_frames);
  g_frames_room = 0;
  g_frames_flat = 0;
  g_places = 0;
  g_ray_frames = 0;
  VrKeyboardReport();
}
