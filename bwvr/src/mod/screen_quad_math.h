// THE SCREENS WITH NO SCENE FOR THE HEADSET APP (docs/android.md, "The
// screens with no scene"). No Windows code, so the mod, the headset app, the
// test reader of the PC and the math test use it
// (test/controller_math/screen_quad_checks.h).
//
// The logos, the videos, the front end and the dialog boxes of the game
// (SetupBox) do not walk the scene: the game calls LH3DRender::FinishFrame
// outside GGame::Process3dEngine (docs/ui-layer.md, section 1.4). On the PC
// the mod shows such a frame on a screen in front of the player
// (mod_main.cpp, h_FinishFrame): the picture of the last walk, or the pixels
// that the game wrote into the render target with the CPU.
//
// With xr_input=1 the mod has no session, and the live stream starts its
// frames in the frame of the scene (stereo.cpp). So with vk_stream_screens=1
// each FinishFrame outside the eye passes is a frame of the stream too (a
// SCREEN FRAME): its walks, the records of state, and at its end a record
// kScreen with the source of the picture and the quad (ScreenRoute,
// MakeScreen). The app shows the picture on that quad, fixed in the room
// (ScreenPicture, Spec). It is never a projection layer.
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "quad_layer_math.h"
#include "scene_stream.h"
#include "stream_split.h"

namespace screen_quad_math {

namespace ss = scene_stream;

// THE NUMBER OF A SCREEN FRAME. The frame of the controllers does not move in
// these screens (it is 0 before the first game), so a screen frame has the
// number kScreenFrameBase + its count in kFrame. Then a record
// (vk_stream_record), a picture of a test (vk_scene_test_frame) and the test
// reader of the PC can name a screen frame. The frames of the scene keep the
// frame of the controllers, which stays far below the base.
constexpr uint64_t kScreenFrameBase = 1000000000ull;
inline uint64_t ScreenFrame(uint64_t count) { return kScreenFrameBase + count; }
inline bool IsScreenFrame(uint64_t frame) { return frame >= kScreenFrameBase; }

// THE ROUTE OF A FINISHFRAME (h_FinishFrame, after the game drew it).
//   kNone: not a screen frame. The key is off (vk_stream_screens=0), there
//     is no input channel (xr_input=0: the StereoIdle path of the PC shows
//     the frame), the FinishFrame is in an eye pass of the scene, or in an
//     eye pass of the intro (intro_vr.cpp, which gives its own frames).
//   kSilent: a screen frame that the PC does not show: in the 150 ms after
//     the last frame of the scene (StereoIdle), the PC holds the last picture
//     of the scene. The frame of the stream ends with no kScreen.
//   kShows: a screen frame that the PC shows on its screen. The frame ends
//     with a kScreen.
enum class Route { kNone, kSilent, kShows };
inline Route ScreenRoute(bool key, bool input_channel, bool in_pass, bool intro_pass,
                         bool stereo, bool idle) {
  if (!key || !input_channel || in_pass || intro_pass) return Route::kNone;
  return (!stereo || idle) ? Route::kShows : Route::kSilent;
}

// A picture of a test (vk_stream_screen_test=N, a test aid): each Nth screen
// frame saves the pictures of its walk (the capture of kFrame). 0: none.
inline bool ScreenCapture(uint64_t count, int every) {
  return every > 0 && count % static_cast<uint64_t>(every) == 0;
}

// THE SOURCE OF THE PICTURE, by the rule of h_FinishFrame and of
// VkSceneLastPicture (vk_scene.cpp): the picture of the last walk, unless
// the game wrote into the render target with the CPU after the end of that
// walk, or the walk is smaller than the render target. Then the picture is
// the pixels of the CPU of the render target. The mod follows the walks and
// the writes of each frame, also with no device of its own (vk_device=0).
class ScreenSource {
 public:
  // The start of a walk, with the size of its viewport (the size of the
  // target of the renderer).
  void OnWalkBegin(uint32_t width, uint32_t height) {
    next_width_ = width;
    next_height_ = height;
  }
  void OnWalkEnd() {
    walk_ = true;
    width_ = next_width_;
    height_ = next_height_;
    cpu_newer_ = false;
  }
  // A write of the CPU into a surface (a Lock or a Blt). render_target: the
  // surface is a target of the device (DDSCAPS_3DDEVICE).
  void OnWritten(bool render_target) {
    if (render_target) cpu_newer_ = true;
  }
  // True: the picture of the last walk. False: the pixels of the CPU of the
  // render target of target_width by target_height.
  bool Walk(uint32_t target_width, uint32_t target_height) const {
    return walk_ && !cpu_newer_ && width_ >= target_width && height_ >= target_height;
  }

 private:
  bool walk_ = false;
  bool cpu_newer_ = false;
  uint32_t width_ = 0, height_ = 0;
  uint32_t next_width_ = 0, next_height_ = 0;
};

// THE QUAD of the screen, as XrScreen::BuildFrame makes the screen of a frame
// with no projection layer (src/vr/xr_screen.cpp): the LOCAL space, no turn,
// distance_m in front of the origin at its height (screen_distance_cm),
// width_m wide (screen_width_cm), and the height of the shape of the
// picture. count: the count of the screen frames. cpu: the source is the
// pixels of the CPU of `surface`.
inline ss::ScreenBody MakeScreen(uint64_t count, bool cpu, uint64_t surface, uint32_t width,
                                 uint32_t height, float distance_m, float width_m) {
  ss::ScreenBody b;
  memset(&b, 0, sizeof(b));
  b.flags = cpu ? ss::kScreenCpu : 0u;
  b.count = count;
  b.surface = cpu ? surface : 0u;
  b.width = width;
  b.height = height;
  b.position[2] = -distance_m;
  b.orientation[3] = 1.0f;
  b.size[0] = width_m;
  b.size[1] = width > 0 ? width_m * static_cast<float>(height) / static_cast<float>(width) : 0.0f;
  return b;
}

// True when the app can show the screen: a picture size, a quad with a size,
// a pose with finite values and a turn of length 1, and a surface for the
// pixels of the CPU.
inline bool ScreenShows(const ss::ScreenBody& b) {
  if (b.width == 0 || b.height == 0 || b.width > 16384 || b.height > 16384) return false;
  if (!(b.size[0] > 0.0f) || !(b.size[1] > 0.0f) || !(b.size[0] < 100.0f) ||
      !(b.size[1] < 100.0f)) {
    return false;
  }
  for (float v : b.position) {
    if (!(v > -1000.0f && v < 1000.0f)) return false;
  }
  float n = 0.0f;
  for (float v : b.orientation) n += v * v;
  if (!(fabsf(n - 1.0f) < 0.01f)) return false;
  if ((b.flags & ss::kScreenCpu) != 0 && b.surface == 0) return false;
  return true;
}

// THE LAYER of the screen in the app: one quad for both eyes (the PC sends
// the same picture to both eyes), opaque as the screen of the PC, with the
// whole picture of picture_width by picture_height.
inline bool Spec(const ss::ScreenBody& b, uint32_t picture_width, uint32_t picture_height,
                 quad_layer_math::QuadSpec* q) {
  memset(q, 0, sizeof(*q));
  if (!ScreenShows(b) || picture_width == 0 || picture_height == 0) return false;
  for (int i = 0; i < 3; ++i) q->position[i] = b.position[i];
  for (int i = 0; i < 4; ++i) q->orientation[i] = b.orientation[i];
  q->size[0] = b.size[0];
  q->size[1] = b.size[1];
  q->rect[2] = static_cast<int32_t>(picture_width);
  q->rect[3] = static_cast<int32_t>(picture_height);
  q->eye = quad_layer_math::kEyeBoth;
  q->flags = 0;
  return true;
}

// THE SCREEN IN THE PLAYER OF THE APP. OnFrame at each kFrame, OnWalkEnd at
// each kWalkEnd, OnScreen at kScreen, OnFrameEnd at kFrameEnd. A frame with a
// kScreen gives a new screen picture at its kFrameEnd: with the pixels of the
// CPU always (they are state), with the picture of the last walk only when a
// walk of the frame ended (a frame that lost its walks keeps its kScreen,
// but gives no picture). A frame of the scene gives a picture of the world
// when a walk of an eye pass ended the picture (stream_split.h,
// WalkEndsFrame). Last says which of the two came last.
enum class Last { kNothing, kWorld, kScreen };
class ScreenPicture {
 public:
  void OnFrame() {
    screen_ = false;
    walk_ended_ = false;
    world_ended_ = false;
  }
  void OnWalkEnd(const ss::StreamState& s) {
    walk_ended_ = true;
    if (s.pass_eye >= 0 && stream_split::WalkEndsFrame(s)) world_ended_ = true;
  }
  void OnScreen(const ss::ScreenBody& b) {
    screen_ = true;
    body_ = b;
  }
  void OnFrameEnd() {
    if (screen_ && ScreenShows(body_) &&
        ((body_.flags & ss::kScreenCpu) != 0 || walk_ended_)) {
      newest_ = body_;
      ++pictures_;
      last_ = Last::kScreen;
    } else if (world_ended_) {
      last_ = Last::kWorld;
    }
    screen_ = false;
    walk_ended_ = false;
    world_ended_ = false;
  }
  Last LastPicture() const { return last_; }
  // The screen of the newest screen picture, and the count of the screen
  // pictures.
  const ss::ScreenBody& Get() const { return newest_; }
  uint64_t Pictures() const { return pictures_; }

 private:
  bool screen_ = false;
  bool walk_ended_ = false;
  bool world_ended_ = false;
  ss::ScreenBody body_ = {};
  ss::ScreenBody newest_ = {};
  uint64_t pictures_ = 0;
  Last last_ = Last::kNothing;
};

}  // namespace screen_quad_math
