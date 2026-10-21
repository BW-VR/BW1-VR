// The projection layer of a frame for the headset app, a test
// (src/mod/view_layer_math.h, the record kView of src/mod/scene_stream.h,
// and LayerFov of src/vr/xr_screen.h). main.cpp includes this file after
// Check and Near, just before main.
#pragma once

#include <math.h>
#include <stddef.h>

#include <vector>

#include "../../src/mod/stream_ring.h"
#include "../../src/mod/stream_split.h"
#include "../../src/mod/view_layer_math.h"

namespace view_layer_checks {

namespace ss = scene_stream;
namespace vl = view_layer_math;

inline bwvr::EyeView TestView(float x, const float fov[4]) {
  bwvr::EyeView v = {};
  v.position[0] = x;
  v.position[1] = 1.6f;
  v.position[2] = -0.1f;
  v.orientation[1] = 0.1f;
  v.orientation[3] = 0.995f;
  v.fov_left = fov[0];
  v.fov_right = fov[1];
  v.fov_up = fov[2];
  v.fov_down = fov[3];
  v.valid = true;
  return v;
}

// A view of a frame, with the frame in each value that a test reads.
inline ss::ViewBody FrameView(uint64_t frame) {
  vl::LayerInput in;
  const float fov[4] = {-0.9425f, 0.8203f, 0.8901f, -0.9599f};
  in.projection = true;
  in.half_angle = 0.96f;
  in.render_pose = true;
  for (int e = 0; e < 2; ++e) {
    in.views[e] = TestView(e == 0 ? -0.032f : 0.032f, fov);
    in.pose_position[e][0] = static_cast<float>(frame) + e;
    in.pose_orientation[e][3] = 1.0f;
  }
  in.width = 1600;
  in.height = 1200;
  in.frame = frame;
  return vl::MakeView(in);
}

// The records of a stream, as the player of the app (scene_player.cpp)
// gives them to PictureView.
inline void Play(const std::vector<uint8_t>& records, vl::PictureView* view) {
  ss::Reader r = ss::Reader::Records(records.data(), records.size());
  ss::Kind kind;
  const uint8_t* body = nullptr;
  uint32_t bytes = 0;
  while (r.Next(&kind, &body, &bytes)) {
    if (kind == ss::kFrame) view->OnFrame();
    if (kind == ss::kView) {
      ss::ViewBody v;
      if (ss::Body(body, bytes, &v)) view->OnView(v);
    }
    if (kind == ss::kWalkEnd) {
      ss::StreamState s;
      if (ss::Body(body, bytes, &s)) view->OnWalkEnd(stream_split::WalkEndsFrame(s));
    }
  }
}

// One frame of the stream as the mod writes it with xr_input=1: kFrame,
// kView, then the walks (one walk with both eyes, or two walks of one eye
// each), then kFrameEnd. with_view false: a frame with no kView.
inline std::vector<uint8_t> Frame(uint64_t frame, bool with_view, bool two_walks) {
  ss::Writer w;
  const ss::FrameBody f = {frame, frame, 0, 0};
  w.Record(ss::kFrame, &f, sizeof(f));
  if (with_view) {
    const ss::ViewBody v = FrameView(frame);
    w.Record(ss::kView, &v, sizeof(v));
  }
  const int walks = two_walks ? 2 : 1;
  for (int k = 0; k < walks; ++k) {
    ss::WalkBeginBody begin = {};
    begin.state.one_walk = two_walks ? 0u : 1u;
    begin.state.pass_eye = two_walks ? k : -1;
    w.Record(ss::kWalkBegin, &begin, sizeof(begin));
    const uint32_t rs[2] = {27, 1};
    w.Record(ss::kRenderState, rs, sizeof(rs));
    w.Record(ss::kWalkEnd, &begin.state, sizeof(begin.state));
  }
  w.Record(ss::kFrameEnd, &frame, sizeof(frame));
  // Without the head of a stream, as in the ring.
  return std::vector<uint8_t>(w.Bytes().begin() + 8, w.Bytes().end());
}

inline void ViewLayerChecks() {
  printf("\nThe projection layer of a frame for the headset app\n");

  // THE LAYOUT has one form on 32 and on 64 bits. static_assert in
  // scene_stream.h holds each size and offset. These lines put them in the
  // output.
  Check(sizeof(ss::ViewBody) == 128 && sizeof(ss::ViewEye) == 44 &&
            offsetof(ss::ViewBody, frame) == 8 &&
            offsetof(ss::ViewBody, display_time) == 16 &&
            offsetof(ss::ViewBody, width) == 24 &&
            offsetof(ss::ViewBody, half_angle) == 32 &&
            offsetof(ss::ViewBody, eyes) == 40 &&
            offsetof(ss::ViewEye, fov) == 28,
        "the record kView has one layout for the mod and the app",
        static_cast<double>(sizeof(ss::ViewBody)), 128);
  Check(sizeof(vl::RecordHead) == 16 && ss::kView == 18 && ss::kLastKind >= ss::kView,
        "the record of the views has a head of 16 bytes, and kView is the kind 18",
        static_cast<double>(ss::kView), 18);

  // THE ANGLES OF VIEW (LayerFov, the rule of XrScreen::BuildFrame).
  {
    const float fov[4] = {-0.9425f, 0.8203f, 0.8901f, -0.9599f};
    const bwvr::EyeView view = TestView(0.0f, fov);
    float out[4];
    bwvr::LayerFov(0.9f, 1000, 800, view, out);
    const float up = atanf(tanf(0.9f) * 800.0f / 1000.0f);
    Check(out[0] == -0.9f && out[1] == 0.9f && out[2] == up && out[3] == -up,
          "with a half angle the layer has a symmetric frustum, the vertical "
          "half angle from the size of the picture",
          out[2], up);
    bwvr::LayerFov(0.0f, 1000, 800, view, out);
    Check(out[0] == fov[0] && out[1] == fov[1] && out[2] == fov[2] && out[3] == fov[3],
          "with no half angle the layer has the angles of the view", out[0], fov[0]);
    bwvr::LayerFov(-0.5f, 1000, 800, view, out);
    Check(out[0] == fov[0] && out[3] == fov[3],
          "a negative half angle is no half angle", out[3], fov[3]);
  }

  // THE VALUES OF THE LAYER (MakeView, as XrScreen::BuildFrame).
  {
    const float fov[4] = {-0.9f, 0.8f, 0.85f, -0.95f};
    vl::LayerInput in;
    in.projection = true;
    in.half_angle = 0.9f;
    in.views[0] = TestView(-0.03f, fov);
    in.views[1] = TestView(0.03f, fov);
    in.width = 1200;
    in.height = 1200;
    in.frame = 41;
    in.input_frame = 9;
    in.display_time = 123456789;
    in.pose_position[0][0] = 5.0f;
    in.pose_orientation[0][3] = 1.0f;
    ss::ViewBody v = vl::MakeView(in);
    Check(v.flags == (ss::kViewProjection | ss::kViewValid) && v.eyes[0].position[0] == -0.03f &&
              v.eyes[1].orientation[1] == 0.1f && v.frame == 41 && v.input_frame == 9 &&
              v.display_time == 123456789,
          "with no pose of SetRenderPose the layer has the views of the frame",
          v.eyes[0].position[0], -0.03);
    Check(v.eyes[0].fov[0] == -0.9f && v.eyes[1].fov[1] == 0.9f &&
              fabsf(v.eyes[0].fov[2] - 0.9f) < 1e-6f,
          "a square picture has the same half angle up and down", v.eyes[0].fov[2], 0.9);
    in.render_pose = true;
    v = vl::MakeView(in);
    Check((v.flags & ss::kViewRenderPose) != 0 && v.eyes[0].position[0] == 5.0f &&
              v.eyes[0].orientation[3] == 1.0f && v.eyes[0].orientation[1] == 0.0f,
          "with the pose of SetRenderPose the layer has that pose", v.eyes[0].position[0], 5);
    Check(vl::ShowsProjection(v), "a frame with a projection layer and valid views shows it",
          1, 1);
    in.flat = true;
    Check(!vl::ShowsProjection(vl::MakeView(in)),
          "a frame of the cut scene window shows no projection layer", 0, 0);
    in.flat = false;
    in.projection = false;
    Check(!vl::ShowsProjection(vl::MakeView(in)),
          "a frame of a screen (SetProjection off) shows no projection layer", 0, 0);
    in.projection = true;
    in.views[1].valid = false;
    Check(!vl::ShowsProjection(vl::MakeView(in)),
          "a frame with a view that is not valid shows no projection layer", 0, 0);
    in.views[1].valid = true;
    in.width = 0;
    Check(!vl::ShowsProjection(vl::MakeView(in)),
          "a frame with no size of the picture shows no projection layer", 0, 0);
    in.width = 1200;
    in.half_angle = -1.0f;
    v = vl::MakeView(in);
    Check(v.half_angle == 0.0f && v.eyes[0].fov[0] == -0.9f && v.eyes[0].fov[1] == 0.8f,
          "a negative half angle goes as 0, with the angles of the views", v.half_angle, 0);

    ss::ViewBody a = FrameView(7), b = FrameView(7);
    b.input_frame = 99;
    Check(vl::SameLayer(a, b), "two runs: the frame of the input is not a value of the layer",
          1, 1);
    b.eyes[1].fov[2] += 1e-6f;
    Check(!vl::SameLayer(a, b), "two runs: an angle of view of a different value is not the same",
          0, 0);
  }

  // THE RECORD IN THE STREAM: a kind of the stream, a record of state, and
  // no record of a walk.
  {
    const std::vector<uint8_t> f = Frame(50, true, false);
    bool bad = true, whole = true;
    uint64_t frame = 0;
    const size_t len = stream_ring::UnitBytes(f.data(), f.size(), 1u << 20, &bad, &frame, &whole);
    Check(len == f.size() && !bad && frame == 50,
          "a frame with a kView is a whole frame of the ring", static_cast<double>(len),
          static_cast<double>(f.size()));
    std::vector<uint8_t> state;
    const uint32_t walks = stream_split::StateOnly(f.data(), f.size(), &state);
    bool view_kept = false;
    ss::Reader r = ss::Reader::Records(state.data(), state.size());
    ss::Kind kind;
    const uint8_t* body = nullptr;
    uint32_t bytes = 0;
    while (r.Next(&kind, &body, &bytes)) {
      if (kind == ss::kView) view_kept = true;
    }
    Check(walks == 1 && view_kept && !stream_split::WalkKind(ss::kView),
          "a frame that loses its walks keeps its kView", walks, 1);
    std::vector<uint8_t> junk = f;
    const uint32_t next = ss::kLastKind + 1;
    memcpy(junk.data(), &next, 4);
    Check(stream_ring::UnitBytes(junk.data(), junk.size(), 1u << 20, &bad, &frame, &whole) == 0 &&
              bad,
          "a record of the kind after the last kind is not of the stream", 0, 0);
  }

  // THE ORDER IN THE PLAYER (PictureView).
  {
    vl::PictureView pv;
    ss::ViewBody got;
    bool none = !pv.Get(&got) && pv.Pictures() == 0;
    Check(none, "before a picture there is no view", 0, 0);
    // One walk with both eyes.
    Play(Frame(100, true, false), &pv);
    bool ok = pv.Get(&got) && got.frame == 100 && pv.Pictures() == 1;
    Check(ok,
          "the view of a frame goes with the picture at the end of its walk",
          static_cast<double>(got.frame), 100);
    // Two walks: the picture ends at the walk of the right eye.
    std::vector<uint8_t> two = Frame(101, true, true);
    vl::PictureView left_only;
    Play(Frame(90, true, false), &left_only);
    // The records to the end of the first walk (the left eye).
    ss::Reader r = ss::Reader::Records(two.data(), two.size());
    ss::Kind kind;
    const uint8_t* body = nullptr;
    uint32_t bytes = 0;
    while (r.Next(&kind, &body, &bytes)) {
      if (kind == ss::kFrame) left_only.OnFrame();
      if (kind == ss::kView) {
        ss::ViewBody v;
        ss::Body(body, bytes, &v);
        left_only.OnView(v);
      }
      if (kind == ss::kWalkEnd) {
        ss::StreamState s;
        ss::Body(body, bytes, &s);
        left_only.OnWalkEnd(stream_split::WalkEndsFrame(s));
        break;
      }
    }
    ok = left_only.Get(&got) && got.frame == 90 && left_only.Pictures() == 1;
    Check(ok,
          "the walk of the left eye ends no picture: the view of the picture before stays",
          static_cast<double>(got.frame), 90);
    Play(two, &pv);
    ok = pv.Get(&got) && got.frame == 101 && pv.Pictures() == 2;
    Check(ok,
          "with two walks the view goes with the picture at the walk of the right eye",
          static_cast<double>(got.frame), 101);
    // A frame that a newer frame replaced: its records of state, no walks.
    std::vector<uint8_t> state;
    const std::vector<uint8_t> skipped = Frame(102, true, false);
    stream_split::StateOnly(skipped.data(), skipped.size(), &state);
    Play(state, &pv);
    ok = pv.Get(&got) && got.frame == 101 && pv.Pictures() == 2;
    Check(ok,
          "a frame with no walks gives its view to no picture",
          static_cast<double>(got.frame), 101);
    // A frame with no kView (a stream of xr_input=0).
    Play(Frame(103, false, false), &pv);
    ok = !pv.Get(&got) && pv.Pictures() == 3;
    Check(ok, "a picture of a frame with no kView has no view",
          static_cast<double>(pv.Pictures()), 3);
    Play(Frame(104, true, false), &pv);
    ok = pv.Get(&got) && got.frame == 104 && got.eyes[1].position[0] == 105.0f;
    Check(ok,
          "the next frame with a kView gives its view to its picture", got.eyes[1].position[0],
          105);
  }
}

}  // namespace view_layer_checks
