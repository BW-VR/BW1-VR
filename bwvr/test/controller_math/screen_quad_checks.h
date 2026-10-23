// The screens with no scene for the headset app, a test
// (src/mod/screen_quad_math.h, the record kScreen of src/mod/scene_stream.h),
// and the name of the dialog box that is up (src/mod/setup_box_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include <vector>

#include "../../src/mod/screen_quad_math.h"
#include "../../src/mod/setup_box_math.h"
#include "../../src/mod/stream_ring.h"
#include "../../src/mod/stream_split.h"

namespace screen_quad_checks {

namespace ss = scene_stream;
namespace sq = screen_quad_math;

// The records of a writer with no head of a stream: a frame of the ring.
inline std::vector<uint8_t> Records(const ss::Writer& w) {
  return std::vector<uint8_t>(w.Bytes().begin() + 8, w.Bytes().end());
}

// One screen frame as the mod writes it (vk_stream.cpp): kFrame, a walk
// outside the eye passes, then kScreen and kFrameEnd. walk false: the frame
// has no walk.
inline void ScreenFrame(ss::Writer* w, uint64_t count, bool walk, const ss::ScreenBody& b) {
  const uint64_t frame = sq::ScreenFrame(count);
  const ss::FrameBody fb = {frame, frame, 0, 0};
  w->Record(ss::kFrame, &fb, sizeof(fb));
  if (walk) {
    ss::WalkBeginBody begin = {};
    begin.state.pass_eye = -1;
    begin.state.intro_eye = -1;
    w->Record(ss::kWalkBegin, &begin, sizeof(begin));
    const ss::DrawBody draw = {};
    w->Record(ss::kDraw, &draw, sizeof(draw));
    w->Record(ss::kWalkEnd, &begin.state, sizeof(begin.state));
  }
  w->Record(ss::kScreen, &b, sizeof(b));
  w->Record(ss::kFrameEnd, &frame, sizeof(frame));
}

// The records of a unit into the player of the screen, as scene_player.cpp
// gives them.
inline void Play(const std::vector<uint8_t>& unit, sq::ScreenPicture* p) {
  ss::Reader r = ss::Reader::Records(unit.data(), unit.size());
  ss::Kind kind;
  const uint8_t* body = nullptr;
  uint32_t bytes = 0;
  while (r.Next(&kind, &body, &bytes)) {
    if (kind == ss::kFrame) p->OnFrame();
    if (kind == ss::kWalkEnd) {
      ss::StreamState s;
      if (ss::Body(body, bytes, &s)) p->OnWalkEnd(s);
    }
    if (kind == ss::kScreen) {
      ss::ScreenBody b;
      if (ss::Body(body, bytes, &b)) p->OnScreen(b);
    }
    if (kind == ss::kFrameEnd) p->OnFrameEnd();
  }
}

inline void ScreenQuadChecks() {
  printf("\nThe screens with no scene for the headset app\n");

  // THE RECORD. One layout on 32 bit x86 (the mod) and 64 bit ARM (the app):
  // the static_assert of scene_stream.h holds in both builds, and the
  // values here.
  Check(sizeof(ss::ScreenBody) == 72 && offsetof(ss::ScreenBody, count) == 8 &&
            offsetof(ss::ScreenBody, surface) == 16 && offsetof(ss::ScreenBody, width) == 24 &&
            offsetof(ss::ScreenBody, position) == 32 &&
            offsetof(ss::ScreenBody, orientation) == 44 && offsetof(ss::ScreenBody, size) == 60,
        "the record kScreen has one layout for the mod and the app",
        static_cast<double>(sizeof(ss::ScreenBody)), 72);
  Check(ss::kScreen == 23 && ss::kLastKind == ss::kScreen && ss::kScreenCpu == 1,
        "kScreen is kind 23, the last kind", static_cast<double>(ss::kScreen), 23);
  {
    // A stream of version 2 (no kScreen, the streams of the runs before)
    // still plays, a stream of version 3 plays, and a later version does not.
    const uint32_t v2[2] = {ss::kMagic, 2}, v3[2] = {ss::kMagic, 3}, v4[2] = {ss::kMagic, 4};
    ss::Reader r2(reinterpret_cast<const uint8_t*>(v2), sizeof(v2));
    ss::Reader r3(reinterpret_cast<const uint8_t*>(v3), sizeof(v3));
    ss::Reader r4(reinterpret_cast<const uint8_t*>(v4), sizeof(v4));
    ss::Writer w;
    uint32_t head[2];
    memcpy(head, w.Bytes().data(), sizeof(head));
    Check(r2.Ok() && r3.Ok() && !r4.Ok() && head[1] == 3 && ss::kVersion == 3,
          "the reader plays a stream of version 2 and 3, and the writer writes 3",
          static_cast<double>(head[1]), 3);
  }

  // THE NUMBERS OF THE SCREEN FRAMES. Above each frame of the scene, in 32
  // bits for the head of the ring, and as an int of the ini
  // (vk_stream_record, vk_scene_test_frame).
  Check(sq::ScreenFrame(1) == 1000000001ull && sq::IsScreenFrame(sq::ScreenFrame(0)) &&
            !sq::IsScreenFrame(999999999ull) && !sq::IsScreenFrame(0),
        "a screen frame has the number 1000000000 + its count", 0, 0);
  Check(sq::ScreenFrame(1000000000ull) < 0xFFFFFFFFull &&
            sq::ScreenFrame(100000000ull) < static_cast<uint64_t>(INT_MAX),
        "the number of a screen frame fits in 32 bits, and the first 100000000 fit in an int",
        0, 0);

  // THE ROUTE OF A FINISHFRAME.
  {
    using sq::Route;
    Check(sq::ScreenRoute(true, true, false, false, true, true) == Route::kShows,
          "outside the eye passes, with the input channel and no scene for 150 ms: it shows",
          0, 0);
    Check(sq::ScreenRoute(true, true, false, false, true, false) == Route::kSilent,
          "in the 150 ms after the last frame of the scene: a screen frame with no screen", 0,
          0);
    Check(sq::ScreenRoute(true, true, false, false, false, false) == Route::kShows,
          "with stereo=0 each FinishFrame outside a pass shows, as on the PC", 0, 0);
    Check(sq::ScreenRoute(false, true, false, false, true, true) == Route::kNone &&
              sq::ScreenRoute(true, false, false, false, true, true) == Route::kNone &&
              sq::ScreenRoute(true, true, true, false, true, true) == Route::kNone &&
              sq::ScreenRoute(true, true, false, true, true, true) == Route::kNone,
          "no screen frame with vk_stream_screens=0, with xr_input=0, in an eye pass and in "
          "an eye pass of the intro",
          0, 0);
    Check(sq::ScreenCapture(500, 250) && !sq::ScreenCapture(501, 250) &&
              !sq::ScreenCapture(500, 0),
          "vk_stream_screen_test=250: each 250th screen frame saves its pictures", 0, 0);
  }

  // THE SOURCE OF THE PICTURE (the rule of h_FinishFrame and
  // VkSceneLastPicture).
  {
    sq::ScreenSource s;
    const bool none = !s.Walk(800, 600);
    s.OnWalkBegin(800, 600);
    s.OnWalkEnd();
    const bool walk = s.Walk(800, 600);
    s.OnWritten(false);  // a texture
    const bool texture = s.Walk(800, 600);
    s.OnWritten(true);  // the render target
    const bool written = !s.Walk(800, 600);
    s.OnWalkBegin(800, 600);
    const bool begun = !s.Walk(800, 600);  // the walk has not ended
    s.OnWalkEnd();
    const bool again = s.Walk(800, 600);
    s.OnWalkBegin(400, 300);
    s.OnWalkEnd();
    const bool smaller = !s.Walk(800, 600) && s.Walk(400, 300);
    Check(none && walk && texture && written && begun && again && smaller,
          "the picture of the last walk, unless the CPU wrote the render target after it or "
          "the walk is smaller than the render target",
          0, 0);
  }

  // THE QUAD: the screen of XrScreen::BuildFrame with no projection layer,
  // with the values of the PC (screen_distance_cm=200, screen_width_cm=260)
  // and a render of 2200 by 1760.
  {
    const ss::ScreenBody b = sq::MakeScreen(7, false, 0x1234, 2200, 1760, 2.0f, 2.6f);
    Check(b.flags == 0 && b.count == 7 && b.surface == 0 && b.width == 2200 &&
              b.height == 1760,
          "a screen of the picture of the last walk has no surface", 0, 0);
    Near("the quad is 2.6 m wide", b.size[0], 2.6, 1e-6);
    Near("and as high as the shape of the picture: 2.6 * 1760 / 2200", b.size[1], 2.08, 1e-5);
    Check(b.position[0] == 0.0f && b.position[1] == 0.0f && b.position[2] == -2.0f &&
              b.orientation[0] == 0.0f && b.orientation[1] == 0.0f &&
              b.orientation[2] == 0.0f && b.orientation[3] == 1.0f,
          "2 m in front of the origin of the LOCAL space, at its height, with no turn", 0, 0);
    Check(sq::ScreenShows(b), "the app can show it", 0, 0);
    quad_layer_math::QuadSpec q;
    const bool spec = sq::Spec(b, 2200, 1760, &q);
    Check(spec && q.rect[0] == 0 && q.rect[1] == 0 && q.rect[2] == 2200 && q.rect[3] == 1760 &&
              q.eye == quad_layer_math::kEyeBoth && q.flags == 0 && q.size[0] == b.size[0] &&
              q.size[1] == b.size[1] && q.position[2] == -2.0f,
          "one opaque quad for both eyes with the whole picture, as the screen of the PC", 0,
          0);
    const ss::ScreenBody c = sq::MakeScreen(8, true, 0x1234, 640, 480, 1.5f, 2.0f);
    Check(c.flags == ss::kScreenCpu && c.surface == 0x1234 && sq::ScreenShows(c),
          "a screen of the pixels of the CPU names the render target", 0, 0);
    Near("its height has the shape of 640 by 480", c.size[1], 1.5, 1e-6);
    ss::ScreenBody bad = b;
    bad.size[0] = 0.0f;
    ss::ScreenBody turn = b;
    turn.orientation[3] = 0.5f;
    ss::ScreenBody no_surface = c;
    no_surface.surface = 0;
    ss::ScreenBody no_size = b;
    no_size.width = 0;
    Check(!sq::ScreenShows(bad) && !sq::ScreenShows(turn) && !sq::ScreenShows(no_surface) &&
              !sq::ScreenShows(no_size) && !sq::Spec(bad, 2200, 1760, &q) &&
              !sq::Spec(b, 0, 1760, &q),
          "no screen with no size, a turn that is not of length 1, the pixels of the CPU of "
          "no surface, or no picture",
          0, 0);
  }

  // THE SCREEN IN THE PLAYER OF THE APP, through the stream.
  {
    sq::ScreenPicture p;
    Check(p.LastPicture() == sq::Last::kNothing && p.Pictures() == 0,
          "before a frame there is no screen", 0, 0);
    const ss::ScreenBody b = sq::MakeScreen(1, false, 0, 2200, 1760, 2.0f, 2.6f);
    ss::Writer w;
    ScreenFrame(&w, 1, true, b);
    Play(Records(w), &p);
    Check(p.LastPicture() == sq::Last::kScreen && p.Pictures() == 1 && p.Get().count == 1 &&
              memcmp(&p.Get(), &b, sizeof(b)) == 0,
          "a screen frame with a walk gives its screen with the values of its kScreen", 0, 0);
    // The writer or the app strips a frame: the kScreen stays, the walk goes.
    ss::Writer w2;
    const ss::ScreenBody b2 = sq::MakeScreen(2, false, 0, 2200, 1760, 2.0f, 2.6f);
    ScreenFrame(&w2, 2, true, b2);
    std::vector<uint8_t> state;
    const std::vector<uint8_t> whole = Records(w2);
    const uint32_t walks = stream_split::StateOnly(whole.data(), whole.size(), &state);
    Play(state, &p);
    Check(walks == 1 && !stream_split::WalkKind(ss::kScreen) && p.Pictures() == 1 &&
              p.Get().count == 1,
          "a screen frame that lost its walk keeps its kScreen, and gives no picture of a walk",
          walks, 1);
    // The pixels of the CPU are state: the frame gives its screen with no walk.
    ss::Writer w3;
    const ss::ScreenBody b3 = sq::MakeScreen(3, true, 0x55, 640, 480, 2.0f, 2.6f);
    ScreenFrame(&w3, 3, false, b3);
    Play(Records(w3), &p);
    Check(p.Pictures() == 2 && p.Get().count == 3 && (p.Get().flags & ss::kScreenCpu) != 0,
          "a screen of the pixels of the CPU needs no walk in its frame", 0, 0);
    // A frame of the scene after it: the world is the last picture.
    ss::Writer w4;
    const ss::FrameBody fb = {3000, 3001, 0, 0};
    w4.Record(ss::kFrame, &fb, sizeof(fb));
    ss::StreamState one = {};
    one.pass_eye = 0;
    one.one_walk = 1;
    w4.Record(ss::kWalkEnd, &one, sizeof(one));
    const uint64_t f = 3000;
    w4.Record(ss::kFrameEnd, &f, sizeof(f));
    Play(Records(w4), &p);
    Check(p.LastPicture() == sq::Last::kWorld && p.Pictures() == 2,
          "a frame of the scene with one walk makes the world the last picture", 0, 0);
    // A walk of the left eye alone does not end the picture of the world.
    sq::ScreenPicture left;
    ss::Writer w5;
    w5.Record(ss::kFrame, &fb, sizeof(fb));
    ss::StreamState eye0 = {};
    eye0.pass_eye = 0;
    w5.Record(ss::kWalkEnd, &eye0, sizeof(eye0));
    w5.Record(ss::kFrameEnd, &f, sizeof(f));
    Play(Records(w5), &left);
    Check(left.LastPicture() == sq::Last::kNothing,
          "a walk of the left eye alone ends no picture of the world", 0, 0);
    // A frame of the ring with a kScreen is whole for the reader.
    bool bad = false, starts = false;
    uint64_t frame = 0;
    const std::vector<uint8_t> unit = Records(w);
    const size_t n = stream_ring::UnitBytes(unit.data(), unit.size(), 1u << 20, &bad, &frame,
                                            &starts);
    Check(n == unit.size() && !bad && frame == sq::ScreenFrame(1) &&
              !stream_ring::KeepKind(ss::kScreen),
          "a screen frame of the ring is whole for the reader, and kScreen alone does not keep "
          "it from a drop",
          static_cast<double>(n), static_cast<double>(unit.size()));
  }

  // THE NAME OF THE DIALOG BOX that is up (setup_box_math.h).
  printf("\nThe name of the dialog box that is up\n");
  {
    char out[64];
    const bool a = setup_box_math::RttiClassName(".?AVNewProfileBox@@", 64, out, sizeof(out)) &&
                   strcmp(out, "NewProfileBox") == 0;
    const bool b = setup_box_math::RttiClassName(".?AUSetupBox@@", 64, out, sizeof(out)) &&
                   strcmp(out, "SetupBox") == 0;
    Check(a && b, "the RTTI names .?AVNewProfileBox@@ and .?AUSetupBox@@ give their classes",
          0, 0);
    char tiny[8];
    Check(!setup_box_math::RttiClassName(".?AV?$List@H@@", 64, out, sizeof(out)) &&
              !setup_box_math::RttiClassName(".?AVBox", 64, out, sizeof(out)) &&
              !setup_box_math::RttiClassName("NewProfileBox", 64, out, sizeof(out)) &&
              !setup_box_math::RttiClassName(".?AVA@B@@", 64, out, sizeof(out)) &&
              !setup_box_math::RttiClassName(".?AVNewProfileBox@@", 64, tiny, sizeof(tiny)) &&
              !setup_box_math::RttiClassName(".?AVNewProfileBox@@", 10, out, sizeof(out)),
          "no class from a template, a cut name, another text, a name in a namespace, a name "
          "that does not fit, or a name past the bytes",
          0, 0);
    setup_box_math::BoxState x, y;
    x.box = 0x100;
    x.dialog = 0x200;
    y = x;
    const bool same = setup_box_math::SameBox(x, y);
    y.callback = 0x53D000;
    Check(same && !setup_box_math::SameBox(x, y) && setup_box_math::kLocatorBeforeVTable == 4 &&
              setup_box_math::kTypeOfLocator == 12 && setup_box_math::kNameOfType == 8,
          "a change of the callback is a change of the box, and the offsets of the RTTI of "
          "MSVC on 32 bit",
          0, 0);
  }
}

}  // namespace screen_quad_checks
