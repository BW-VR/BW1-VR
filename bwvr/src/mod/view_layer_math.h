// THE PROJECTION LAYER OF A FRAME FOR THE HEADSET APP (docs/android.md, "The
// projection layer"). No Windows code, so the mod, the headset app, the test
// reader of the PC and the math test use it
// (test/controller_math/view_layer_checks.h).
//
// With xr_input=1 the mod has no OpenXR session of its own. The app has the
// session and shows the eye pictures of the live stream. So for each frame
// the mod writes what it gives a session of its own (XrScreen,
// src/vr/xr_screen.h) into the frame of the stream, as a record kView
// (scene_stream.h). The record is in the frame of its eye pictures, so the
// app has the values of the picture that it shows, with no frame number to
// match.
//
// THE VALUES. XrScreen::BuildFrame makes the projection layer of a frame
// from these values, and MakeView takes them in the same way:
//
//   - SetProjection: a projection layer, or the screen of two quads.
//   - SetRenderPose: the pose that each eye was drawn from. With no such
//     pose (the first frame), the views of the frame.
//   - SetRenderHalfAngle: the half angle of the symmetric frustum that the
//     game drew. With it LayerFov gives the four angles of each eye. With no
//     half angle, the angles of the views of the frame.
//   - The size of each eye picture: the rect of each eye is the whole
//     picture (offset 0 0), layer 0 of its swapchain.
//   - The display time of the frame (the input of the frame).
//   - The cut scene window: a frame that the mod shows as a screen.
//
// THE ORDER IN THE APP. The player of the app takes the kView of a frame, and
// the view goes with the picture only at the end of the walk that ends the
// picture of that frame (stream_split.h, WalkEndsFrame). A frame that a newer
// frame replaced keeps its records of state with no walks, so its view never
// goes with a picture (PictureView).
#pragma once

#include <stdint.h>
#include <string.h>

#include "../vr/xr_screen.h"
#include "scene_stream.h"

namespace view_layer_math {

namespace ss = scene_stream;

// What the mod gives its session for one frame.
struct LayerInput {
  bool projection = false;   // SetProjection
  float half_angle = 0.0f;   // SetRenderHalfAngle
  bool render_pose = false;  // SetRenderPose for both eyes
  float pose_position[2][3] = {};
  float pose_orientation[2][4] = {};
  bwvr::EyeView views[2] = {};  // the views of the frame (BeginFrame, or the input channel)
  uint32_t width = 0;           // the size of each eye picture
  uint32_t height = 0;
  bool flat = false;            // a frame of the cut scene window
  uint64_t frame = 0;           // the frame of the controllers
  uint32_t input_frame = 0;     // SlotData::frame of the input of the frame
  int64_t display_time = 0;     // ns
};

// The record of the layer of a frame, as XrScreen::BuildFrame makes the
// layer.
inline ss::ViewBody MakeView(const LayerInput& in) {
  ss::ViewBody v;
  memset(&v, 0, sizeof(v));
  if (in.projection) v.flags |= ss::kViewProjection;
  if (in.render_pose) v.flags |= ss::kViewRenderPose;
  if (in.views[0].valid && in.views[1].valid) v.flags |= ss::kViewValid;
  if (in.flat) v.flags |= ss::kViewFlat;
  v.input_frame = in.input_frame;
  v.frame = in.frame;
  v.display_time = in.display_time;
  v.width = in.width;
  v.height = in.height;
  v.half_angle = in.half_angle > 0.0f ? in.half_angle : 0.0f;
  for (int e = 0; e < 2; ++e) {
    ss::ViewEye& eye = v.eyes[e];
    const bwvr::EyeView& view = in.views[e];
    for (int i = 0; i < 3; ++i) {
      eye.position[i] = in.render_pose ? in.pose_position[e][i] : view.position[i];
    }
    for (int i = 0; i < 4; ++i) {
      eye.orientation[i] =
          in.render_pose ? in.pose_orientation[e][i] : view.orientation[i];
    }
    bwvr::LayerFov(v.half_angle, in.width, in.height, view, eye.fov);
  }
  return v;
}

// True when the app shows the eye pictures of the frame of v in a projection
// layer: the mod gives a projection layer, both views are valid, the frame
// is not a frame of the cut scene window, and the pictures have a size.
// Else the session of the mod shows a screen, and the app shows its stereo
// screen.
inline bool ShowsProjection(const ss::ViewBody& v) {
  return (v.flags & ss::kViewProjection) != 0 && (v.flags & ss::kViewValid) != 0 &&
         (v.flags & ss::kViewFlat) == 0 && v.width > 0 && v.height > 0;
}

// THE VIEW OF THE PICTURE in the player of the app. OnFrame at each kFrame,
// OnView at each kView, OnWalkEnd at each kWalkEnd. The view of a frame goes
// with the picture at the walk that ends the picture of the frame. A picture
// of a frame with no kView has no view.
class PictureView {
 public:
  void OnFrame() { frame_has_view_ = false; }
  void OnView(const ss::ViewBody& v) {
    frame_view_ = v;
    frame_has_view_ = true;
  }
  // ends_frame: stream_split::WalkEndsFrame of the state of the kWalkEnd.
  void OnWalkEnd(bool ends_frame) {
    if (!ends_frame) return;
    has_view_ = frame_has_view_;
    if (has_view_) view_ = frame_view_;
    ++pictures_;
  }
  // The view of the last picture. False when that picture has none.
  bool Get(ss::ViewBody* out) const {
    if (!has_view_) return false;
    *out = view_;
    return true;
  }
  // The count of the pictures that ended.
  uint64_t Pictures() const { return pictures_; }

 private:
  ss::ViewBody frame_view_ = {};
  bool frame_has_view_ = false;
  ss::ViewBody view_ = {};
  bool has_view_ = false;
  uint64_t pictures_ = 0;
};

// THE RECORD OF THE VIEWS (xr_view_record, a test aid of the mod). A head
// and then one ViewBody for each frame, in the order of the frames.
constexpr uint32_t kRecordMagic = 0x56565742u;  // "BWVV"
constexpr uint32_t kRecordVersion = 1;
struct RecordHead {
  uint32_t magic;
  uint32_t version;     // kRecordVersion
  uint32_t view_bytes;  // sizeof(ViewBody)
  uint32_t source;      // 0 the session, 1 the invented head, 2 the input channel
};
static_assert(sizeof(RecordHead) == 16, "RecordHead");

// Two views of one frame of two runs with the same input. The frame of the
// input of the channel (input_frame) is a value of the channel, not of the
// layer, so it is not compared.
inline bool SameLayer(const ss::ViewBody& a, const ss::ViewBody& b) {
  ss::ViewBody x = a, y = b;
  x.input_frame = 0;
  y.input_frame = 0;
  return memcmp(&x, &y, sizeof(x)) == 0;
}

}  // namespace view_layer_math
