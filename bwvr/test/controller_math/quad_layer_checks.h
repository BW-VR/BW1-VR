// The quad layers of a frame for the headset app, a test
// (src/mod/quad_layer_math.h, the records kCard, kCardPicture, kPanel and
// kWindow of src/mod/scene_stream.h). main.cpp includes this file after
// Check and Near, just before main.
#pragma once

#include <stddef.h>

#include <vector>

#include "../../src/mod/quad_layer_math.h"
#include "../../src/mod/stream_ring.h"
#include "../../src/mod/stream_split.h"

namespace quad_layer_checks {

namespace ss = scene_stream;
namespace ql = quad_layer_math;

// A card picture of w by h pixels in a texture of tw by th, each pixel from
// its place and seed.
inline std::vector<uint8_t> Pixels(uint32_t tw, uint32_t th, uint32_t seed) {
  std::vector<uint8_t> p(static_cast<size_t>(tw) * th * 4);
  for (size_t i = 0; i < p.size(); ++i) p[i] = static_cast<uint8_t>((i * 7 + seed * 13) & 0xFF);
  return p;
}

inline bwvr::CardLayer Card(const std::vector<uint8_t>& pixels, uint32_t tw, uint32_t th,
                            int32_t x, int32_t y, int32_t w, int32_t h) {
  bwvr::CardLayer l = {};
  l.visible = true;
  l.pixels = pixels.data();
  l.texture_width = tw;
  l.texture_height = th;
  l.rect[0] = x;
  l.rect[1] = y;
  l.rect[2] = w;
  l.rect[3] = h;
  l.position[0] = 0.3f;
  l.position[1] = -0.1f;
  l.position[2] = -0.4f;
  l.orientation[1] = 0.2f;
  l.orientation[3] = 0.98f;
  l.width = 0.08f;
  l.height = 0.02f;
  return l;
}

// The kinds of the records of a buffer with no head of a stream.
inline std::vector<uint32_t> Kinds(const std::vector<uint8_t>& records) {
  std::vector<uint32_t> kinds;
  ss::Reader r = ss::Reader::Records(records.data(), records.size());
  ss::Kind kind;
  const uint8_t* body = nullptr;
  uint32_t bytes = 0;
  while (r.Next(&kind, &body, &bytes)) kinds.push_back(kind);
  return kinds;
}

inline std::vector<uint8_t> NoHead(const ss::Writer& w) {
  return std::vector<uint8_t>(w.Bytes().begin() + 8, w.Bytes().end());
}

// The records of a stream into PictureLayers and AppPictures, as the player
// of the app (scene_player.cpp) gives them.
inline void Play(const std::vector<uint8_t>& records, ql::PictureLayers* layers,
                 ql::AppPictures* pictures) {
  ss::Reader r = ss::Reader::Records(records.data(), records.size());
  ss::Kind kind;
  const uint8_t* body = nullptr;
  uint32_t bytes = 0;
  while (r.Next(&kind, &body, &bytes)) {
    if (kind == ss::kFrame) layers->OnFrame();
    if (kind == ss::kCardPicture) pictures->OnPicture(body, bytes);
    if (kind == ss::kCard) {
      ss::CardBody c;
      if (ss::Body(body, bytes, &c)) layers->OnCard(c);
    }
    if (kind == ss::kPanel) {
      ss::PanelBody p;
      if (ss::Body(body, bytes, &p)) layers->OnPanel(p);
    }
    if (kind == ss::kWindow) {
      ss::WindowBody w;
      if (ss::Body(body, bytes, &w)) layers->OnWindow(w);
    }
    if (kind == ss::kWalkEnd) {
      ss::StreamState s;
      if (ss::Body(body, bytes, &s)) layers->OnWalkEnd(stream_split::WalkEndsFrame(s));
    }
    if (kind == ss::kFrameEnd) layers->OnFrameEnd();
  }
}

// One frame of the stream as the mod writes it with xr_input=1 and
// xr_input_layers=1: kFrame, the walks, the layers, kFrameEnd. walks 0: a
// frame that lost its walks.
inline std::vector<uint8_t> Frame(uint64_t frame, int walks, const ql::FrameLayers& f,
                                  const ql::CardPictures& pictures, ql::SentPictures* sent) {
  ss::Writer w;
  const ss::FrameBody fb = {frame, frame, 0, 0};
  w.Record(ss::kFrame, &fb, sizeof(fb));
  for (int k = 0; k < walks; ++k) {
    ss::WalkBeginBody begin = {};
    begin.state.one_walk = walks == 1 ? 1u : 0u;
    begin.state.pass_eye = walks == 1 ? -1 : k;
    w.Record(ss::kWalkBegin, &begin, sizeof(begin));
    w.Record(ss::kWalkEnd, &begin.state, sizeof(begin.state));
  }
  ql::WriteFrame(f, pictures, sent, &w);
  w.Record(ss::kFrameEnd, &frame, sizeof(frame));
  return NoHead(w);
}

inline void QuadLayerChecks() {
  printf("\nThe quad layers of a frame for the headset app\n");

  // THE LAYOUT has one form on 32 and on 64 bits. static_assert in
  // scene_stream.h holds each size and offset. These lines put them in the
  // output.
  Check(sizeof(ss::CardBody) == 80 && offsetof(ss::CardBody, rect) == 16 &&
            offsetof(ss::CardBody, position) == 32 && offsetof(ss::CardBody, picture) == 72 &&
            sizeof(ss::CardPictureBody) == 24,
        "the records kCard and kCardPicture have one layout for the mod and the app",
        static_cast<double>(sizeof(ss::CardBody)), 80);
  Check(sizeof(ss::PanelQuadBody) == 56 && sizeof(ss::PanelBody) == 176 &&
            offsetof(ss::PanelBody, quads) == 8 && sizeof(ss::WindowBody) == 136 &&
            offsetof(ss::WindowBody, wall_position) == 52 &&
            offsetof(ss::WindowBody, alpha) == 132,
        "the records kPanel and kWindow have one layout for the mod and the app",
        static_cast<double>(sizeof(ss::PanelBody) + sizeof(ss::WindowBody)), 312);
  Check(ss::kCard == 19 && ss::kCardPicture == 20 && ss::kPanel == 21 &&
            ss::kWindow == 22 && ss::kLastKind == ss::kScreen && ss::kScreen == 23,
        "the kinds of the quad layers are 19 to 22, and kScreen (23) is the last kind",
        static_cast<double>(ss::kLastKind), 23);
  {
    // The reader of the ring takes the new kinds, and no kind after them.
    ss::Writer w;
    const ss::FrameBody fb = {5, 5, 0, 0};
    w.Record(ss::kFrame, &fb, sizeof(fb));
    const ss::WindowBody wb = {};
    w.Record(ss::kWindow, &wb, sizeof(wb));
    const uint64_t five = 5;
    w.Record(ss::kFrameEnd, &five, sizeof(five));
    std::vector<uint8_t> unit = NoHead(w);
    bool bad = false, whole = false;
    uint64_t frame = 0;
    const size_t n = stream_ring::UnitBytes(unit.data(), unit.size(), 1u << 20, &bad, &frame, &whole);
    uint32_t next = ss::kLastKind + 1;
    std::vector<uint8_t> wrong = unit;
    memcpy(wrong.data() + 32, &next, 4);  // the head of the kWindow record
    bool bad2 = false;
    stream_ring::UnitBytes(wrong.data(), wrong.size(), 1u << 20, &bad2, &frame, &whole);
    Check(n == unit.size() && !bad && bad2,
          "a frame of the ring with a kWindow is whole, and a record of the kind 24 is not of the stream",
          static_cast<double>(n), static_cast<double>(unit.size()));
  }

  // WHERE A FRAME ENDS. With the layers the walk does not end the frame,
  // and a frame with no eye camera ends with no layer record.
  {
    ql::CardPictures pictures;
    ql::SentPictures sent;
    const std::vector<uint8_t> none = Frame(20, 0, ql::FrameLayers(), pictures, &sent);
    const std::vector<uint32_t> k = Kinds(none);
    ql::PictureLayers layers;
    ql::AppPictures app;
    Play(none, &layers, &app);
    Check(ql::WhereFrameEnds(true, false) == ql::FrameEnd::kAtWalk &&
              ql::WhereFrameEnds(true, true) == ql::FrameEnd::kAtLayers &&
              ql::WhereFrameEnds(false, true) == ql::FrameEnd::kAtNextFrame &&
              k.size() == 2 && k[0] == ss::kFrame && k[1] == ss::kFrameEnd &&
              layers.Pictures() == 0 && !layers.Get().Any(),
          "with the layers the layers end a frame, and a frame with no eye camera ends with no layer record and gives no layers",
          static_cast<double>(k.size()), 2);
  }

  // THE HASH OF A PICTURE.
  {
    const std::vector<uint8_t> a = Pixels(16, 8, 1);
    std::vector<uint8_t> b = a;
    b[5 * 64 + 9] ^= 1;
    const uint64_t ha = ql::PictureHash(a.data(), 64, 16, 8);
    const uint64_t hb = ql::PictureHash(b.data(), 64, 16, 8);
    const uint64_t ha2 = ql::PictureHash(a.data(), 64, 16, 8);
    // The same 64 bytes as 16 by 1 and as 4 by 4.
    const uint64_t line = ql::PictureHash(a.data(), 64, 16, 1);
    const uint64_t square = ql::PictureHash(a.data(), 16, 4, 4);
    Check(ha == ha2 && ha != hb && ha != 0 && line != square,
          "the hash of a picture: the same for the same pixels, another for one bit more, the size is in it",
          ha != hb ? 1 : 0, 1);
  }

  // THE PICTURES OF THE MOD (CardPictures::Take).
  {
    ql::CardPictures pictures;
    std::vector<uint8_t> px = Pixels(64, 32, 2);
    const bwvr::CardLayer l = Card(px, 64, 32, 0, 0, 40, 20);
    const uint64_t h1 = pictures.Take(bwvr::kCardHintPointing, l);
    const uint64_t h2 = pictures.Take(bwvr::kCardHintPointing, l);
    // A pixel outside the rect does not change the picture.
    px[(25 * 64 + 50) * 4] ^= 0xFF;
    const uint64_t h3 = pictures.Take(bwvr::kCardHintPointing, l);
    Check(h1 != 0 && h1 == h2 && h2 == h3 && pictures.Count(bwvr::kCardHintPointing) == 1,
          "a card with the same pixels in its rect keeps its picture, and the pixels outside the rect do not count",
          static_cast<double>(pictures.Count(bwvr::kCardHintPointing)), 1);
    px[(3 * 64 + 7) * 4 + 1] ^= 0x10;
    const uint64_t h4 = pictures.Take(bwvr::kCardHintPointing, l);
    Check(h4 != h3 && pictures.Count(bwvr::kCardHintPointing) == 2 &&
              pictures.Width(bwvr::kCardHintPointing) == 40 &&
              pictures.Height(bwvr::kCardHintPointing) == 20 &&
              pictures.Pixels(bwvr::kCardHintPointing).size() == 40u * 20u * 4u,
          "a pixel in the rect gives a new picture of the size of the rect",
          static_cast<double>(pictures.Pixels(bwvr::kCardHintPointing).size()), 3200);
    // The picture is the rect: row 3, column 7 of the texture.
    Check(pictures.Pixels(bwvr::kCardHintPointing)[(3 * 40 + 7) * 4 + 1] == px[(3 * 64 + 7) * 4 + 1],
          "the pixels of a picture are the rows of the rect, with no gap",
          pictures.Pixels(bwvr::kCardHintPointing)[(3 * 40 + 7) * 4 + 1], px[(3 * 64 + 7) * 4 + 1]);
    // The same pixels at another place of the texture are the same picture.
    std::vector<uint8_t> moved(64 * 32 * 4, 0);
    for (int r = 0; r < 20; ++r) {
      memcpy(moved.data() + ((r + 10) * 64 + 20) * 4, px.data() + (r * 64) * 4, 40 * 4);
    }
    const uint64_t h5 = pictures.Take(bwvr::kCardHintOther, Card(moved, 64, 32, 20, 10, 40, 20));
    Check(h5 == h4, "the hash is of the pixels of the rect, not of its place", h5 == h4 ? 1 : 0, 1);
    bwvr::CardLayer outside = Card(px, 64, 32, 30, 0, 40, 20);
    bwvr::CardLayer none = l;
    none.pixels = nullptr;
    Check(pictures.Take(bwvr::kCardRing, outside) == 0 && pictures.Take(bwvr::kCardRing, none) == 0 &&
              pictures.Count(bwvr::kCardRing) == 0,
          "a rect past the texture or a card with no pixels has no picture",
          static_cast<double>(pictures.Count(bwvr::kCardRing)), 0);
  }

  // A PICTURE GOES ONLY WHEN THE READER DOES NOT HAVE IT (SentPictures,
  // WriteFrame).
  {
    ql::CardPictures pictures;
    ql::SentPictures sent;
    std::vector<uint8_t> px = Pixels(64, 32, 3);
    const bwvr::CardLayer l = Card(px, 64, 32, 0, 0, 40, 20);
    ql::FrameLayers f;
    f.card[bwvr::kCardSubtitles] = true;
    f.cards[bwvr::kCardSubtitles] =
        ql::MakeCard(bwvr::kCardSubtitles, l, pictures.Take(bwvr::kCardSubtitles, l));
    const std::vector<uint32_t> k1 = Kinds(Frame(1, 1, f, pictures, &sent));
    const std::vector<uint32_t> k2 = Kinds(Frame(2, 1, f, pictures, &sent));
    sent.Reset();
    const std::vector<uint32_t> k3 = Kinds(Frame(3, 1, f, pictures, &sent));
    px[0] ^= 1;
    f.cards[bwvr::kCardSubtitles] =
        ql::MakeCard(bwvr::kCardSubtitles, l, pictures.Take(bwvr::kCardSubtitles, l));
    const std::vector<uint32_t> k4 = Kinds(Frame(4, 1, f, pictures, &sent));
    auto count = [](const std::vector<uint32_t>& k, uint32_t kind) {
      int n = 0;
      for (uint32_t x : k) n += x == kind ? 1 : 0;
      return n;
    };
    Check(count(k1, ss::kCardPicture) == 1 && count(k2, ss::kCardPicture) == 0 &&
              count(k3, ss::kCardPicture) == 1 && count(k4, ss::kCardPicture) == 1 &&
              count(k2, ss::kCard) == 1,
          "a picture goes in the first frame, not again in the next, again after the whole state, and again when it changes",
          count(k2, ss::kCardPicture), 0);
    // The order of a frame: kFrame, the walk, the picture before its card,
    // then kFrameEnd.
    Check(k1.size() == 6 && k1[0] == ss::kFrame && k1[1] == ss::kWalkBegin && k1[2] == ss::kWalkEnd &&
              k1[3] == ss::kCardPicture && k1[4] == ss::kCard && k1[5] == ss::kFrameEnd,
          "the layers of a frame are after its walk and before kFrameEnd, the picture before its card",
          static_cast<double>(k1.size()), 6);
  }

  // A FRAME IN ALL ITS PARTS: the panel, the cards in the order of their
  // index, the window. MakeCard, MakePanel and MakeWindow keep each value.
  {
    ql::CardPictures pictures;
    ql::SentPictures sent;
    std::vector<uint8_t> px = Pixels(64, 32, 4);
    ql::FrameLayers f;
    for (int c = bwvr::kCards - 1; c >= 0; --c) {
      bwvr::CardLayer l = Card(px, 64, 32, 0, 0, 30 + c, 10);
      f.card[c] = true;
      f.cards[c] = ql::MakeCard(c, l, pictures.Take(c, l));
    }
    bwvr::PanelQuad q[2] = {};
    q[0].visible = true;
    q[0].width = 1.2f;
    q[0].height = 0.6f;
    q[0].rect[2] = 800;
    q[0].rect[3] = 400;
    q[0].position[2] = -1.5f;
    q[0].orientation[3] = 1.0f;
    q[1] = q[0];
    q[1].visible = false;
    bwvr::PanelQuad creature = q[0];
    creature.rect[0] = 100;
    f.panel = true;
    f.panel_body = ql::MakePanel(2000, 1600, q, true, creature);
    bwvr::WindowLayer wl = {};
    wl.mask = true;
    wl.alpha = 0.5f;
    wl.crop = 12;
    wl.edge = 2;
    wl.position[2] = -2.0f;
    wl.orientation[3] = 1.0f;
    wl.width = 2.6f;
    wl.height = 1.46f;
    for (int k = 0; k < 4; ++k) {
      wl.wall_size[k][0] = 1.0f + k;
      wl.wall_size[k][1] = 2.0f;
      wl.wall_position[k][0] = static_cast<float>(k);
    }
    f.window = true;
    f.window_body = ql::MakeWindow(wl);
    const std::vector<uint32_t> k = Kinds(Frame(7, 1, f, pictures, &sent));
    const uint32_t want[] = {ss::kFrame, ss::kWalkBegin, ss::kWalkEnd, ss::kPanel,
                             ss::kCardPicture, ss::kCard, ss::kCardPicture, ss::kCard,
                             ss::kCardPicture, ss::kCard, ss::kCardPicture, ss::kCard,
                             ss::kWindow, ss::kFrameEnd};
    bool same = k.size() == sizeof(want) / sizeof(want[0]);
    for (size_t i = 0; same && i < k.size(); ++i) same = k[i] == want[i];
    Check(same, "a frame: the panel, each card in the order of its index after its picture, then the window",
          static_cast<double>(k.size()), 14);
    Check(f.cards[2].card == 2 && f.cards[2].rect[2] == 32 && f.cards[2].texture_width == 64 &&
              f.cards[2].position[2] == -0.4f && f.cards[2].orientation[1] == 0.2f &&
              f.cards[2].width == 0.08f && f.cards[2].flags == ss::kCardVisible &&
              f.panel_body.width == 2000 && f.panel_body.quads[0].rect[2] == 800 &&
              f.panel_body.quads[1].visible == 0 && f.panel_body.quads[2].rect[0] == 100 &&
              f.window_body.mask == 1 && f.window_body.screen == 0 && f.window_body.crop == 12 &&
              f.window_body.wall_size[3][0] == 4.0f && f.window_body.wall_position[2][0] == 2.0f,
          "the records keep each value of the card, the panel quads and the window", 1, 1);
    const ss::PanelBody no_creature = ql::MakePanel(2000, 1600, q, false, creature);
    Check(no_creature.quads[2].visible == 0,
          "a frame that staged no Creature panel has no Creature panel", no_creature.quads[2].visible, 0);

    // THE PLAYER OF THE APP: the layers go with the picture of the frame.
    ql::PictureLayers layers;
    ql::AppPictures app;
    ql::SentPictures sent2;
    Play(Frame(7, 1, f, pictures, &sent2), &layers, &app);
    bool pictures_ok = true;
    for (int c = 0; c < bwvr::kCards; ++c) {
      pictures_ok = pictures_ok && app.Has(c, f.cards[c].picture) &&
                    app.Pixels(c) == pictures.Pixels(c) &&
                    ql::PictureHash(app.Pixels(c).data(), app.Width(c) * 4, app.Width(c),
                                    app.Height(c)) == f.cards[c].picture;
    }
    Check(layers.Pictures() == 1 && layers.Get().panel && layers.Get().window &&
              layers.Get().card[3] && memcmp(&layers.Get().cards[3], &f.cards[3], sizeof(f.cards[3])) == 0 &&
              pictures_ok,
          "the app has the layers of the frame of the picture, and the pixels of each card with the hash of its card",
          static_cast<double>(layers.Pictures()), 1);
    // A frame that lost its walks gives its layers to no picture, and its
    // pictures still come.
    ql::FrameLayers g = f;
    std::vector<uint8_t> px2 = Pixels(64, 32, 9);
    bwvr::CardLayer l2 = Card(px2, 64, 32, 0, 0, 30, 10);
    g.cards[0] = ql::MakeCard(0, l2, pictures.Take(0, l2));
    g.cards[0].position[0] = 9.0f;
    Play(Frame(8, 0, g, pictures, &sent2), &layers, &app);
    Check(layers.Pictures() == 1 && layers.Get().cards[0].position[0] != 9.0f &&
              app.Has(0, g.cards[0].picture),
          "a frame with no walks gives its layers to no picture, and the app has its new picture of a card",
          layers.Get().cards[0].position[0], f.cards[0].position[0]);
    // The next frame with a walk and no new pixels takes that picture.
    Play(Frame(9, 1, g, pictures, &sent2), &layers, &app);
    Check(layers.Pictures() == 2 && layers.Get().cards[0].position[0] == 9.0f &&
              app.Has(0, layers.Get().cards[0].picture),
          "the next frame with a picture shows the card with the picture of the frame before",
          static_cast<double>(layers.Pictures()), 2);
    // The picture of card 0 of f is the newest one again.
    pictures.Take(0, Card(px, 64, 32, 0, 0, 30, 10));
    // Two walks: the walk of the left eye does not end the picture, the walk
    // of the right eye does.
    ql::PictureLayers two;
    ql::AppPictures app2;
    ql::SentPictures sent3;
    Play(Frame(10, 2, f, pictures, &sent3), &two, &app2);
    Check(two.Pictures() == 1 && two.Get().panel,
          "with two walks the layers go with the picture of the walk of the right eye",
          static_cast<double>(two.Pictures()), 1);
    // A frame with no layers (a stream with no layers) gives no layer.
    ql::FrameLayers empty;
    Play(Frame(11, 1, empty, pictures, &sent3), &two, &app2);
    Check(two.Pictures() == 2 && !two.Get().Any(),
          "a picture of a frame with no layer records has no quad layer", two.Get().Any() ? 1 : 0, 0);

    // THE STATE: a frame that the app skips or the writer drops keeps the
    // layers and the pictures, and a picture of a card keeps the frame.
    std::vector<uint8_t> state;
    ql::SentPictures fresh;
    const std::vector<uint8_t> frame_pictures = Frame(12, 1, f, pictures, &fresh);
    const uint32_t gone =
        stream_split::StateOnly(frame_pictures.data(), frame_pictures.size(), &state);
    const std::vector<uint32_t> ks = Kinds(state);
    int layer_records = 0;
    for (uint32_t x : ks) {
      layer_records += x == ss::kCard || x == ss::kCardPicture || x == ss::kPanel || x == ss::kWindow;
    }
    const stream_ring::UnitInfo with = stream_ring::Inspect(frame_pictures.data(), frame_pictures.size());
    const std::vector<uint8_t> frame_cards = Frame(14, 1, f, pictures, &fresh);
    const stream_ring::UnitInfo without = stream_ring::Inspect(frame_cards.data(), frame_cards.size());
    Check(gone == 1 && layer_records == 10 && stream_ring::KeepKind(ss::kCardPicture) &&
              !stream_ring::KeepKind(ss::kCard) && !stream_ring::KeepKind(ss::kPanel) &&
              !stream_ring::KeepKind(ss::kWindow) && with.keep && !without.keep,
          "a frame with no walks keeps its layers and its pictures, and a picture of a card is kept as the pixels of a surface",
          static_cast<double>(layer_records), 10);

    // THE SAME CALLS (Normal): two frames with the same layers have the same
    // normal records, with a picture or without.
    ql::SentPictures again;
    const std::vector<uint8_t> a = Frame(15, 1, f, pictures, &again);
    const std::vector<uint8_t> b = Frame(15, 1, f, pictures, &again);
    ql::FrameLayers moved = f;
    moved.cards[1].position[1] += 0.001f;
    const std::vector<uint8_t> c = Frame(15, 1, moved, pictures, &again);
    Check(a.size() > b.size() && ql::Normal(a.data(), a.size()) == ql::Normal(b.data(), b.size()) &&
              ql::Normal(a.data(), a.size()) != ql::Normal(c.data(), c.size()),
          "the same calls with and without the pictures have the same normal records, and 1 mm more does not",
          static_cast<double>(ql::Normal(a.data(), a.size()).size()), 4 * 88 + 184 + 144);
  }

  // THE RULES OF XrScreen::BuildFrame.
  {
    std::vector<uint8_t> px = Pixels(64, 32, 5);
    ss::CardBody c = ql::MakeCard(0, Card(px, 64, 32, 0, 0, 40, 20), 7);
    ss::CardBody hidden = c, wide = c, flat = c, other = c, high = c;
    hidden.flags = 0;
    wide.rect[0] = 30;
    flat.height = 0.0f;
    other.card = 4;
    high.texture_height = 9000;
    Check(ql::CardShows(c) && !ql::CardShows(hidden) && !ql::CardShows(wide) &&
              !ql::CardShows(flat) && !ql::CardShows(other) && !ql::CardShows(high),
          "a card shows only when it is visible, its rect is in its texture and it has a size (SendCard)",
          ql::CardShows(c) ? 1 : 0, 1);
    ss::PanelQuadBody q = {};
    q.visible = 1;
    q.width = 1.0f;
    q.height = 0.5f;
    q.rect[0] = -10;
    q.rect[1] = 1500;
    q.rect[2] = 2100;
    q.rect[3] = 200;
    int32_t r[4] = {0, 0, 0, 0};
    const bool cut = ql::PanelQuadRect(q, 2000, 1600, r);
    ss::PanelQuadBody off = q;
    off.visible = 0;
    ss::PanelQuadBody out = q;
    out.rect[0] = 2000;
    Check(cut && r[0] == 0 && r[1] == 1500 && r[2] == 2000 && r[3] == 100 &&
              !ql::PanelQuadRect(off, 2000, 1600, r) && !ql::PanelQuadRect(out, 2000, 1600, r),
          "a panel quad is cut to the panel image, and a quad that is not visible or outside does not show",
          r[3], 100);
    ss::WindowBody w = {};
    w.mask = 1;
    w.alpha = 0.5f;
    for (int k = 0; k < 4; ++k) {
      w.wall_size[k][0] = 1.0f;
      w.wall_size[k][1] = 1.0f;
    }
    ss::WindowBody clear = w, thin = w;
    clear.alpha = 0.004f;
    thin.wall_size[2][1] = 0.0f;
    Check(ql::WallsShow(w) && !ql::WallsShow(clear) && !ql::WallsShow(thin) &&
              ql::WallAlpha(w) == 128,
          "the walls show with an alpha above 0.004 and four walls with a size, black with the alpha of the frame",
          ql::WallAlpha(w), 128);
    ss::WindowBody s = {};
    s.screen = 1;
    s.crop = 40;
    s.edge = 3;
    int32_t x0 = 0, x1 = 0, shown = 0;
    ql::WindowColumns(s, 1000, 0, &x0, &shown);
    ql::WindowColumns(s, 1000, 1, &x1, &shown);
    ss::WindowBody big = s;
    big.crop = 500;
    big.edge = 300;
    int32_t xb = 0, shown_b = 0;
    ql::WindowColumns(big, 1000, 0, &xb, &shown_b);
    Check(x0 == 43 && x1 == 3 && shown == 954 && xb == 0 && shown_b == 1000,
          "the cut scene window: the left eye from column crop plus edge, the right eye from column edge, and too much is none",
          shown, 954);
  }

  // THE ORDER OF THE LAYERS (XrScreen::BuildFrame).
  {
    std::vector<uint8_t> px = Pixels(64, 32, 6);
    ql::FrameLayers f;
    for (int c = 0; c < bwvr::kCards; ++c) {
      f.card[c] = true;
      f.cards[c] = ql::MakeCard(c, Card(px, 64, 32, 0, 0, 40, 20), 7);
    }
    f.panel = true;
    f.panel_body.width = 2000;
    f.panel_body.height = 1600;
    for (int i = 0; i < 3; ++i) {
      f.panel_body.quads[i].visible = 1;
      f.panel_body.quads[i].width = 1.0f;
      f.panel_body.quads[i].height = 0.5f;
      f.panel_body.quads[i].rect[2] = 100;
      f.panel_body.quads[i].rect[3] = 50;
    }
    f.window = true;
    f.window_body.mask = 1;
    f.window_body.alpha = 0.7f;
    for (int k = 0; k < 4; ++k) {
      f.window_body.wall_size[k][0] = 1.0f;
      f.window_body.wall_size[k][1] = 1.0f;
    }
    const bool all[4] = {true, true, true, true};
    ql::LayerRef o[ql::kMostLayers];
    const int n = ql::Order(f, true, true, all, o);
    const int want_kind[12] = {ql::kLayerProjection, ql::kLayerPanel, ql::kLayerPanel,
                               ql::kLayerPanel, ql::kLayerCard, ql::kLayerCard,
                               ql::kLayerCard, ql::kLayerCard, ql::kLayerWall,
                               ql::kLayerWall, ql::kLayerWall, ql::kLayerWall};
    const int want_index[12] = {0, 0, 1, 2, bwvr::kCardSubtitles, bwvr::kCardHintPointing,
                                bwvr::kCardHintOther, bwvr::kCardRing, 0, 1, 2, 3};
    bool same = n == 12;
    for (int i = 0; same && i < n; ++i) same = o[i].kind == want_kind[i] && o[i].index == want_index[i];
    Check(same,
          "a frame of the world: the projection layer, the menu, the help, the Creature panel, the subtitle card, the hint cards, the ring, the four walls",
          n, 12);
    const bool no_ring[4] = {true, true, true, false};
    const int n2 = ql::Order(f, true, false, no_ring, o);
    Check(n2 == 8 && o[1].kind == ql::kLayerCard && o[3].index == bwvr::kCardHintOther &&
              o[4].kind == ql::kLayerWall,
          "with no panel image there is no panel quad, and a card with no picture does not show",
          n2, 8);
    ql::FrameLayers s = f;
    s.window_body.screen = 1;
    const int n3 = ql::Order(s, true, true, all, o);
    Check(n3 == 3 && o[0].kind == ql::kLayerEye && o[0].index == 0 && o[1].index == 1 &&
              o[2].kind == ql::kLayerCard && o[2].index == bwvr::kCardSubtitles,
          "a frame of the cut scene window as a screen: a quad of each eye, then the subtitle card, and no other layer",
          n3, 3);
    ql::FrameLayers none;
    const int n4 = ql::Order(none, true, true, all, o);
    const int n5 = ql::Order(f, false, true, all, o);
    Check(n4 == 1 && o[0].kind == ql::kLayerProjection && n5 == 0,
          "a frame of the world with no layer records has the projection layer alone, and a frame that is neither a world nor a screen has none",
          n4, 1);

    // THE QUADS (Spec), as BuildFrame makes them.
    ql::QuadSpec q;
    ql::FrameLayers w = s;
    w.window_body.crop = 40;
    w.window_body.edge = 3;
    w.window_body.width = 2.6f;
    w.window_body.position[2] = -2.0f;
    w.window_body.orientation[3] = 1.0f;
    const bool left = ql::Spec(w, {ql::kLayerEye, 0}, 1000, 800, &q);
    const ql::QuadSpec ql0 = q;
    const bool right = ql::Spec(w, {ql::kLayerEye, 1}, 1000, 800, &q);
    Check(left && right && ql0.rect[0] == 43 && ql0.rect[2] == 954 && ql0.rect[3] == 800 &&
              ql0.eye == ql::kEyeLeft && ql0.flags == 0 && q.rect[0] == 3 && q.eye == ql::kEyeRight &&
              q.size[0] == 2.6f && q.position[2] == -2.0f,
          "the quad of each eye of the cut scene window: its columns, its eye, the pose and the size of the window, no blend",
          ql0.rect[0], 43);
    f.window_body.wall_position[2][1] = 0.7f;
    f.window_body.orientation[1] = 0.3f;
    const bool wall = ql::Spec(f, {ql::kLayerWall, 2}, 1000, 800, &q);
    Check(wall && q.position[1] == 0.7f && q.orientation[1] == 0.3f && q.size[0] == 1.0f &&
              q.rect[2] == 16 && q.rect[3] == 16 && q.flags == ql::kBlendSourceAlpha,
          "a wall: its place, the turn of the window, its size, the 16 by 16 pixels of black, the source alpha",
          q.rect[2], 16);
    f.panel_body.quads[1].rect[0] = 1950;
    const bool panel = ql::Spec(f, {ql::kLayerPanel, 1}, 1000, 800, &q);
    const bool card = ql::Spec(f, {ql::kLayerCard, 3}, 1000, 800, &q);
    Check(panel && card && q.rect[2] == 40 && q.size[1] == 0.02f && q.eye == ql::kEyeBoth &&
              !ql::Spec(f, {ql::kLayerProjection, 0}, 1000, 800, &q),
          "a panel quad and a card are quads for both eyes, and the projection layer is no quad",
          q.rect[2], 40);
    f.panel_body.quads[1].rect[0] = 2000;
    Check(!ql::Spec(f, {ql::kLayerPanel, 1}, 1000, 800, &q),
          "a panel quad with a rect outside the panel image is no quad", 0, 0);
  }
}

}  // namespace quad_layer_checks
