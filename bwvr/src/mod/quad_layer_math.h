// THE QUAD LAYERS OF A FRAME FOR THE HEADSET APP (docs/android.md, "The quad
// layers"). No Windows code, so the mod, the headset app, the test reader of
// the PC and the math test use it (test/controller_math/quad_layer_checks.h).
//
// With xr_input=1 the mod has no OpenXR session of its own, and the app shows
// the eye pictures of the live stream. Over them a session of the mod has
// quad layers (XrScreen, src/vr/xr_screen.h):
//
//   - THE CARDS (StageCard): the hint cards of both hands, the subtitle card
//     and the ring. Each has a picture of its own that the mod draws on the
//     CPU.
//   - THE PANEL QUADS (SetPanelQuads, StagePanelQuad): the menu, the help and
//     the Creature panel, rects of the panel image of the renderer.
//   - THE CUT SCENE WINDOW (StageWindow): the eye pictures on a screen in the
//     room, or the four walls around the opening while the window opens and
//     closes.
//
// So for each frame the mod writes what the session takes at its end of the
// frame into the frame of the stream (scene_stream.h: kCard, kCardPicture,
// kPanel, kWindow), after the walk that ends the picture and before
// kFrameEnd. A picture of a card goes only when it changed (CardPictures,
// SentPictures). The panel picture is the panel image of the renderer, which
// the app has itself.
//
// THE ORDER OF THE LAYERS (Order) is the order of XrScreen::BuildFrame:
//
//   - A frame of the world: the projection layer, the panel quads (the menu,
//     the help, the Creature panel), the cards (the subtitle card, the hint
//     card of the hand that points, the hint card of the other hand, the
//     ring), then the four walls of the cut scene window.
//   - A frame of the cut scene window as a screen (WindowBody::screen): a
//     quad of the eye picture for each eye at the pose of the window, then
//     the subtitle card. No panel and no other card.
//
// THE FRAME OF THE LAYERS IN THE APP (PictureLayers). The layers of a frame
// go with the picture of that frame at its kFrameEnd, when a walk of the
// frame ended the picture. A frame that lost its walks gives its layers to no
// picture, and its picture records still give the newest picture of a card.
#pragma once

#include <stdint.h>
#include <string.h>

#include <vector>

#include "../vr/xr_screen.h"
#include "controller_math.h"
#include "scene_stream.h"

namespace quad_layer_math {

namespace ss = scene_stream;

// THE PICTURE OF A CARD. A hash of the pixels of the rect of a card, row by
// row (pitch bytes from one row to the next), with the size in it. FNV-1a of
// 64 bits over words of 32 bits. 0 is never a hash, so 0 can stand for no
// picture.
inline uint64_t PictureHash(const uint8_t* pixels, uint32_t pitch, uint32_t width,
                            uint32_t height) {
  const uint64_t kPrime = 1099511628211ull;
  uint64_t h = 14695981039346656037ull;
  h = (h ^ width) * kPrime;
  h = (h ^ height) * kPrime;
  for (uint32_t y = 0; y < height; ++y) {
    const uint8_t* row = pixels + static_cast<size_t>(y) * pitch;
    for (uint32_t x = 0; x < width; ++x) {
      uint32_t word;
      memcpy(&word, row + static_cast<size_t>(x) * 4, 4);
      h = (h ^ word) * kPrime;
    }
  }
  return h == 0 ? 1 : h;
}

// The name of a card for the log.
inline const char* CardName(int card) {
  switch (card) {
    case bwvr::kCardHintPointing: return "the hint card of the hand that points";
    case bwvr::kCardHintOther: return "the hint card of the other hand";
    case bwvr::kCardSubtitles: return "the subtitle card";
    case bwvr::kCardRing: return "the ring";
    default: return "a card";
  }
}

// THE CHECKS OF XrScreen::Impl::SendCard: a card that the session cannot
// send does not show.
inline bool CardShows(const ss::CardBody& c) {
  const int32_t x = c.rect[0], y = c.rect[1], w = c.rect[2], h = c.rect[3];
  if ((c.flags & ss::kCardVisible) == 0) return false;
  if (c.card >= static_cast<uint32_t>(bwvr::kCards)) return false;
  if (c.texture_width == 0 || c.texture_height == 0 || c.texture_width > 8192 ||
      c.texture_height > 8192 || x < 0 || y < 0 || w <= 0 || h <= 0 ||
      x + w > static_cast<int32_t>(c.texture_width) ||
      y + h > static_cast<int32_t>(c.texture_height) || !(c.width > 0.0f) ||
      !(c.height > 0.0f)) {
    return false;
  }
  return true;
}

// The record of a card that the mod staged. picture: its hash.
inline ss::CardBody MakeCard(int card, const bwvr::CardLayer& l, uint64_t picture) {
  ss::CardBody c;
  memset(&c, 0, sizeof(c));
  c.card = static_cast<uint32_t>(card);
  c.flags = l.visible ? ss::kCardVisible : 0u;
  c.texture_width = l.texture_width;
  c.texture_height = l.texture_height;
  for (int i = 0; i < 4; ++i) c.rect[i] = l.rect[i];
  for (int i = 0; i < 3; ++i) c.position[i] = l.position[i];
  for (int i = 0; i < 4; ++i) c.orientation[i] = l.orientation[i];
  c.width = l.width;
  c.height = l.height;
  c.picture = picture;
  return c;
}

inline ss::PanelQuadBody MakePanelQuad(const bwvr::PanelQuad& q) {
  ss::PanelQuadBody b;
  memset(&b, 0, sizeof(b));
  b.visible = q.visible ? ss::kPanelQuadShows : 0u;
  for (int i = 0; i < 4; ++i) b.rect[i] = q.rect[i];
  for (int i = 0; i < 3; ++i) b.position[i] = q.position[i];
  for (int i = 0; i < 4; ++i) b.orientation[i] = q.orientation[i];
  b.width = q.width;
  b.height = q.height;
  return b;
}

// The panel quads of a frame: the two quads of SetPanelQuads and the
// Creature panel that SetPanelQuads takes (StagePanelQuad). have_creature
// false: the frame staged no Creature panel, so it does not show.
inline ss::PanelBody MakePanel(uint32_t width, uint32_t height,
                               const bwvr::PanelQuad quads[2],
                               bool have_creature, const bwvr::PanelQuad& creature) {
  ss::PanelBody p;
  memset(&p, 0, sizeof(p));
  p.width = width;
  p.height = height;
  p.quads[0] = MakePanelQuad(quads[0]);
  p.quads[1] = MakePanelQuad(quads[1]);
  p.quads[2] = MakePanelQuad(creature);
  if (!have_creature) p.quads[2].visible = 0;
  return p;
}

// THE RECT OF A PANEL QUAD, as XrScreen::BuildFrame cuts it to the panel
// image of width by height. False when the quad does not show.
inline bool PanelQuadRect(const ss::PanelQuadBody& q, uint32_t width, uint32_t height,
                          int32_t out[4]) {
  if ((q.visible & ss::kPanelQuadShows) == 0 || !(q.width > 0.0f) || !(q.height > 0.0f)) {
    return false;
  }
  const int32_t x = q.rect[0] < 0 ? 0 : q.rect[0];
  const int32_t y = q.rect[1] < 0 ? 0 : q.rect[1];
  int32_t w = q.rect[2];
  int32_t h = q.rect[3];
  if (x + w > static_cast<int32_t>(width)) w = static_cast<int32_t>(width) - x;
  if (y + h > static_cast<int32_t>(height)) h = static_cast<int32_t>(height) - y;
  if (w <= 0 || h <= 0) return false;
  out[0] = x;
  out[1] = y;
  out[2] = w;
  out[3] = h;
  return true;
}

// THE PART OF THE PANEL IMAGE THAT THE QUADS NEED (xr_panel_rect=1): the
// smallest rect that holds the rect of each panel quad, shown or not, cut to
// the panel image. A quad that is off keeps its rect, so a quad that comes on
// later with no new panel image still lies in the part. False when no quad
// has a rect in the image: then the whole image is the part.
inline bool PanelUnion(const ss::PanelBody& p, int32_t out[4]) {
  int32_t x0 = 0x7fffffff, y0 = 0x7fffffff, x1 = 0, y1 = 0;
  bool any = false;
  for (int i = 0; i < 3; ++i) {
    const ss::PanelQuadBody& q = p.quads[i];
    const int32_t x = q.rect[0] < 0 ? 0 : q.rect[0];
    const int32_t y = q.rect[1] < 0 ? 0 : q.rect[1];
    int32_t w = q.rect[2], h = q.rect[3];
    if (x + w > static_cast<int32_t>(p.width)) w = static_cast<int32_t>(p.width) - x;
    if (y + h > static_cast<int32_t>(p.height)) h = static_cast<int32_t>(p.height) - y;
    if (w <= 0 || h <= 0) continue;
    any = true;
    if (x < x0) x0 = x;
    if (y < y0) y0 = y;
    if (x + w > x1) x1 = x + w;
    if (y + h > y1) y1 = y + h;
  }
  if (!any) return false;
  out[0] = x0;
  out[1] = y0;
  out[2] = x1 - x0;
  out[3] = y1 - y0;
  return true;
}

// True when the rect `inner` (x, y, w, h) lies in the rect `outer`.
inline bool RectInside(const int32_t inner[4], const int32_t outer[4]) {
  return inner[0] >= outer[0] && inner[1] >= outer[1] &&
         inner[0] + inner[2] <= outer[0] + outer[2] &&
         inner[1] + inner[3] <= outer[1] + outer[3];
}

// THE KEYBOARD OF THE APP IN A FRAME OF THE SCENE (docs/android.md, "The
// dialog boxes by the controllers"). A dialog box of the game is on the menu
// panel quad (quads[0]). While it takes text, the mod sets kPanelQuadText on
// that quad, and only on a quad that shows.
inline void MarkPanelText(ss::PanelBody* p, bool text) {
  ss::PanelQuadBody& q = p->quads[0];
  if (text && (q.visible & ss::kPanelQuadShows) != 0) {
    q.visible |= ss::kPanelQuadText;
  } else {
    q.visible &= ~ss::kPanelQuadText;
  }
}
// True when the menu panel quad shows and asks for the keyboard. rect gets
// the rect of the quad in the panel image.
inline bool PanelText(const ss::PanelBody& p, int32_t rect[4]) {
  return (p.quads[0].visible & ss::kPanelQuadText) != 0 &&
         PanelQuadRect(p.quads[0], p.width, p.height, rect);
}

// The panel quads of a frame as targets of the ray of a hand
// (controller_math.h, RayPanels): out[0] to out[2], with the rect of each
// quad cut to the panel image as the app shows it.
inline void PanelTargets(const ss::PanelBody& p, controller_math::RayTarget out[3]) {
  for (int i = 0; i < 3; ++i) {
    const ss::PanelQuadBody& q = p.quads[i];
    controller_math::RayTarget& t = out[i];
    memset(&t, 0, sizeof(t));
    t.panel = true;
    t.on = PanelQuadRect(q, p.width, p.height, t.rect);
    for (int k = 0; k < 3; ++k) t.position[k] = q.position[k];
    for (int k = 0; k < 4; ++k) t.orientation[k] = q.orientation[k];
    t.size[0] = q.width;
    t.size[1] = q.height;
  }
}

inline ss::WindowBody MakeWindow(const bwvr::WindowLayer& l) {
  ss::WindowBody w;
  memset(&w, 0, sizeof(w));
  w.screen = l.screen ? 1u : 0u;
  w.mask = l.mask ? 1u : 0u;
  w.crop = l.crop;
  w.edge = l.edge;
  for (int i = 0; i < 3; ++i) w.position[i] = l.position[i];
  for (int i = 0; i < 4; ++i) w.orientation[i] = l.orientation[i];
  w.width = l.width;
  w.height = l.height;
  for (int k = 0; k < 4; ++k) {
    for (int i = 0; i < 3; ++i) w.wall_position[k][i] = l.wall_position[k][i];
    w.wall_size[k][0] = l.wall_size[k][0];
    w.wall_size[k][1] = l.wall_size[k][1];
  }
  w.alpha = l.alpha;
  return w;
}

// The walls of XrScreen::Impl::SendMask: an alpha that shows, and four walls
// with a size.
inline bool WallsShow(const ss::WindowBody& w) {
  if (w.mask == 0 || !(w.alpha > 0.004f)) return false;
  for (int k = 0; k < 4; ++k) {
    if (!(w.wall_size[k][0] > 0.0f) || !(w.wall_size[k][1] > 0.0f)) return false;
  }
  return true;
}

// The alpha byte of the walls (SendMask): black with this alpha.
inline uint8_t WallAlpha(const ss::WindowBody& w) {
  const float a = w.alpha > 1.0f ? 1.0f : w.alpha;
  return static_cast<uint8_t>(a * 255.0f + 0.5f);
}

// THE COLUMNS OF THE CUT SCENE WINDOW, as XrScreen::BuildFrame takes them from
// an eye picture of `width` columns: rect gets the x and the width of the
// part of `eye` that shows (WindowLayer, crop and edge).
inline void WindowColumns(const ss::WindowBody& w, uint32_t width, int eye,
                          int32_t* x, int32_t* shown) {
  const int32_t full = static_cast<int32_t>(width);
  int32_t crop = w.crop;
  if (crop < 0 || crop >= full / 2) crop = 0;
  int32_t edge = w.edge;
  if (edge < 0 || full - crop - 2 * edge < full / 2) edge = 0;
  *shown = full - crop - 2 * edge;
  *x = (eye == 0 ? crop : 0) + edge;
}

// THE LAYERS OF ONE FRAME, as the session takes them at its end of the frame.
struct FrameLayers {
  bool panel = false;
  ss::PanelBody panel_body = {};
  bool card[bwvr::kCards] = {false, false, false, false};
  ss::CardBody cards[bwvr::kCards] = {};
  bool window = false;
  ss::WindowBody window_body = {};
  bool Any() const {
    return panel || window || card[0] || card[1] || card[2] || card[3];
  }
};

// True when the frame goes out as the screen of the cut scene window.
inline bool AsScreen(const FrameLayers& f) {
  return f.window && f.window_body.screen != 0;
}

// One layer of a frame.
enum LayerKind : int {
  kLayerProjection = 0,  // the eye pictures, at the poses of the view
  kLayerEye = 1,         // index: the eye. A quad of the eye picture at the window
  kLayerPanel = 2,       // index: the panel quad
  kLayerCard = 3,        // index: the card
  kLayerWall = 4,        // index: the wall
};
struct LayerRef {
  int kind;
  int index;
};
// The most layers of a frame: the projection layer, 3 panel quads, 4 cards,
// 4 walls.
constexpr int kMostLayers = 12;

// THE ORDER of the layers of a frame (XrScreen::BuildFrame). world: the frame
// shows in a projection layer (view_layer_math.h, ShowsProjection), and it is
// not a screen of the cut scene window. panel_image: the frame has the panel
// image of the renderer, of panel_width by panel_height. card_picture[i]:
// the picture of card i is there. Gives the count. A frame that is neither
// a world nor a screen of the window gives 0: it shows on the stereo screen.
inline int Order(const FrameLayers& f, bool world, bool panel_image,
                 const bool card_picture[bwvr::kCards], LayerRef out[kMostLayers]) {
  int n = 0;
  if (world && !AsScreen(f)) {
    out[n++] = {kLayerProjection, 0};
    if (f.panel && panel_image) {
      int32_t rect[4];
      for (int i = 0; i < bwvr::kPanelQuads; ++i) {
        if (PanelQuadRect(f.panel_body.quads[i], f.panel_body.width,
                          f.panel_body.height, rect)) {
          out[n++] = {kLayerPanel, i};
        }
      }
    }
    const int card_order[bwvr::kCards] = {bwvr::kCardSubtitles, bwvr::kCardHintPointing,
                                          bwvr::kCardHintOther, bwvr::kCardRing};
    for (int i = 0; i < bwvr::kCards; ++i) {
      const int c = card_order[i];
      if (f.card[c] && card_picture[c] && CardShows(f.cards[c])) out[n++] = {kLayerCard, c};
    }
    if (f.window && WallsShow(f.window_body)) {
      for (int k = 0; k < 4; ++k) out[n++] = {kLayerWall, k};
    }
    return n;
  }
  if (!AsScreen(f)) return 0;
  out[n++] = {kLayerEye, 0};
  out[n++] = {kLayerEye, 1};
  const int c = bwvr::kCardSubtitles;
  if (f.card[c] && card_picture[c] && CardShows(f.cards[c])) out[n++] = {kLayerCard, c};
  return n;
}

// THE QUAD OF A LAYER, as XrScreen::BuildFrame makes it. The values of
// XrEyeVisibility and of XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT
// of openxr.h (the app checks them with static_assert).
constexpr uint32_t kEyeBoth = 0;
constexpr uint32_t kEyeLeft = 1;
constexpr uint32_t kEyeRight = 2;
constexpr uint32_t kBlendSourceAlpha = 0x2;
// The walls are black with one alpha: a swapchain of this many pixels on
// each side (kMaskSize of xr_screen.cpp).
constexpr uint32_t kWallPixels = 16;
struct QuadSpec {
  float position[3];
  float orientation[4];
  float size[2];
  int32_t rect[4];  // x, y, width, height in the pixels of its swapchain
  uint32_t eye;     // kEyeBoth, kEyeLeft or kEyeRight
  uint32_t flags;   // 0 or kBlendSourceAlpha
};

// The quad of a layer of Order. picture_width and picture_height: the size of
// the eye pictures (the swapchains of the eyes). False for the projection
// layer, which is no quad.
inline bool Spec(const FrameLayers& f, const LayerRef& r, uint32_t picture_width,
                 uint32_t picture_height, QuadSpec* q) {
  memset(q, 0, sizeof(*q));
  q->flags = kBlendSourceAlpha;
  q->eye = kEyeBoth;
  auto pose = [q](const float position[3], const float orientation[4]) {
    for (int i = 0; i < 3; ++i) q->position[i] = position[i];
    for (int i = 0; i < 4; ++i) q->orientation[i] = orientation[i];
  };
  switch (r.kind) {
    case kLayerPanel: {
      const ss::PanelQuadBody& p = f.panel_body.quads[r.index];
      if (!PanelQuadRect(p, f.panel_body.width, f.panel_body.height, q->rect)) return false;
      pose(p.position, p.orientation);
      q->size[0] = p.width;
      q->size[1] = p.height;
      return true;
    }
    case kLayerCard: {
      const ss::CardBody& c = f.cards[r.index];
      pose(c.position, c.orientation);
      q->size[0] = c.width;
      q->size[1] = c.height;
      for (int i = 0; i < 4; ++i) q->rect[i] = c.rect[i];
      return true;
    }
    case kLayerWall: {
      const ss::WindowBody& w = f.window_body;
      pose(w.wall_position[r.index], w.orientation);
      q->size[0] = w.wall_size[r.index][0];
      q->size[1] = w.wall_size[r.index][1];
      q->rect[2] = static_cast<int32_t>(kWallPixels);
      q->rect[3] = static_cast<int32_t>(kWallPixels);
      return true;
    }
    case kLayerEye: {
      const ss::WindowBody& w = f.window_body;
      pose(w.position, w.orientation);
      q->size[0] = w.width;
      q->size[1] = w.height;
      WindowColumns(w, picture_width, r.index, &q->rect[0], &q->rect[2]);
      q->rect[3] = static_cast<int32_t>(picture_height);
      q->eye = r.index == 0 ? kEyeLeft : kEyeRight;
      q->flags = 0;
      return true;
    }
    default:
      return false;
  }
}

// WHERE A FRAME OF THE STREAM ENDS (vk_stream.cpp). vk_stream_live_end=1:
// at its first walk with both eyes, or with the quad layers
// (VkStreamSetLayers) at the end of the frame of the scene, after the layers.
// A frame of the scene with no eye passes (no camera of the eyes) ends there
// too, with no layer record. vk_stream_live_end=0: at the start of the next
// frame.
enum class FrameEnd { kAtWalk, kAtLayers, kAtNextFrame };
inline FrameEnd WhereFrameEnds(bool end_at_walk, bool layers) {
  if (!end_at_walk) return FrameEnd::kAtNextFrame;
  return layers ? FrameEnd::kAtLayers : FrameEnd::kAtWalk;
}

// THE CARD PICTURES OF THE MOD: the newest picture of each card that the mod
// staged, and its hash. Take compares the pixels of the rect with the
// picture before, so a hash is made only for a new picture.
class CardPictures {
 public:
  // The hash of the picture of a staged card. 0 for a card with no pixels.
  uint64_t Take(int card, const bwvr::CardLayer& l) {
    if (card < 0 || card >= bwvr::kCards || l.pixels == nullptr) return 0;
    Picture& p = pictures_[card];
    const int32_t x = l.rect[0], y = l.rect[1];
    const int32_t w = l.rect[2], h = l.rect[3];
    if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
        x + w > static_cast<int32_t>(l.texture_width) ||
        y + h > static_cast<int32_t>(l.texture_height)) {
      return 0;
    }
    const uint32_t pitch = l.texture_width * 4;
    const uint8_t* first = l.pixels + static_cast<size_t>(y) * pitch + static_cast<size_t>(x) * 4;
    const size_t row = static_cast<size_t>(w) * 4;
    bool same = p.hash != 0 && p.width == static_cast<uint32_t>(w) &&
                p.height == static_cast<uint32_t>(h);
    for (int32_t r = 0; same && r < h; ++r) {
      same = memcmp(p.pixels.data() + r * row, first + static_cast<size_t>(r) * pitch, row) == 0;
    }
    if (same) return p.hash;
    p.width = static_cast<uint32_t>(w);
    p.height = static_cast<uint32_t>(h);
    p.pixels.resize(row * static_cast<size_t>(h));
    for (int32_t r = 0; r < h; ++r) {
      memcpy(p.pixels.data() + r * row, first + static_cast<size_t>(r) * pitch, row);
    }
    p.hash = PictureHash(p.pixels.data(), static_cast<uint32_t>(row), p.width, p.height);
    ++p.count;
    return p.hash;
  }
  // The pixels of the newest picture of a card: width * height * 4 bytes.
  const std::vector<uint8_t>& Pixels(int card) const { return pictures_[card].pixels; }
  uint32_t Width(int card) const { return pictures_[card].width; }
  uint32_t Height(int card) const { return pictures_[card].height; }
  uint64_t Hash(int card) const { return pictures_[card].hash; }
  // The count of the new pictures of a card.
  uint64_t Count(int card) const { return pictures_[card].count; }

 private:
  struct Picture {
    uint64_t hash = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t count = 0;
    std::vector<uint8_t> pixels;
  };
  Picture pictures_[bwvr::kCards];
};

// THE PICTURES THAT ONE READER HAS: the hash that went last for each card. A
// frame with the whole state (kStart) starts with no picture, so the next
// card of each kind gives its picture again.
class SentPictures {
 public:
  void Reset() {
    for (uint64_t& h : sent_) h = 0;
  }
  // True when the picture must go with this card.
  bool Needs(int card, uint64_t hash) const {
    return card >= 0 && card < bwvr::kCards && hash != 0 && sent_[card] != hash;
  }
  void Sent(int card, uint64_t hash) {
    if (card >= 0 && card < bwvr::kCards) sent_[card] = hash;
  }

 private:
  uint64_t sent_[bwvr::kCards] = {0, 0, 0, 0};
};

// THE RECORDS OF THE LAYERS OF A FRAME, in this order: the panel, the cards
// in the order of their index (each after its picture when the picture is
// new for the reader), then the window. pictures: the card pictures of the
// mod. sent: what the reader has. Writer: scene_stream::Writer or a class
// with the same Record.
template <typename Writer>
void WriteFrame(const FrameLayers& f, const CardPictures& pictures, SentPictures* sent,
                Writer* w) {
  if (f.panel) w->Record(ss::kPanel, &f.panel_body, sizeof(f.panel_body));
  for (int c = 0; c < bwvr::kCards; ++c) {
    if (!f.card[c]) continue;
    const ss::CardBody& body = f.cards[c];
    if (sent->Needs(c, body.picture) && pictures.Hash(c) == body.picture) {
      ss::CardPictureBody p;
      memset(&p, 0, sizeof(p));
      p.card = static_cast<uint32_t>(c);
      p.width = pictures.Width(c);
      p.height = pictures.Height(c);
      p.picture = body.picture;
      w->Record(ss::kCardPicture, &p, sizeof(p), pictures.Pixels(c).data(),
                static_cast<uint32_t>(pictures.Pixels(c).size()));
      sent->Sent(c, body.picture);
    }
    w->Record(ss::kCard, &body, sizeof(body));
  }
  if (f.window) w->Record(ss::kWindow, &f.window_body, sizeof(f.window_body));
}

// THE PICTURES OF THE CARDS IN THE APP: the newest picture of each card that
// came in the stream. A card shows only with the picture of its hash.
class AppPictures {
 public:
  // A record kCardPicture. False when it is too short.
  bool OnPicture(const uint8_t* body, uint32_t bytes) {
    ss::CardPictureBody p;
    if (!ss::Body(body, bytes, &p)) return false;
    if (p.card >= static_cast<uint32_t>(bwvr::kCards)) return false;
    const uint64_t n = static_cast<uint64_t>(p.width) * p.height * 4;
    if (bytes < sizeof(p) + n) return false;
    Picture& q = pictures_[p.card];
    q.width = p.width;
    q.height = p.height;
    q.hash = p.picture;
    q.pixels.assign(body + sizeof(p), body + sizeof(p) + n);
    ++q.count;
    return true;
  }
  // True when card c has the picture of hash.
  bool Has(int c, uint64_t hash) const {
    return c >= 0 && c < bwvr::kCards && hash != 0 && pictures_[c].hash == hash;
  }
  const std::vector<uint8_t>& Pixels(int c) const { return pictures_[c].pixels; }
  uint32_t Width(int c) const { return pictures_[c].width; }
  uint32_t Height(int c) const { return pictures_[c].height; }
  uint64_t Hash(int c) const { return pictures_[c].hash; }
  uint64_t Count(int c) const { return pictures_[c].count; }

 private:
  struct Picture {
    uint64_t hash = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t count = 0;
    std::vector<uint8_t> pixels;
  };
  Picture pictures_[bwvr::kCards];
};

// THE LAYERS OF THE PICTURE in the player of the app. OnFrame at each kFrame,
// OnCard, OnPanel and OnWindow at their records, OnWalkEnd at each kWalkEnd,
// OnFrameEnd at kFrameEnd. The layers of a frame go with the picture of the
// frame at its kFrameEnd.
class PictureLayers {
 public:
  void OnFrame() {
    frame_ = FrameLayers();
    picture_ended_ = false;
  }
  void OnCard(const ss::CardBody& c) {
    if (c.card >= static_cast<uint32_t>(bwvr::kCards)) return;
    frame_.card[c.card] = true;
    frame_.cards[c.card] = c;
  }
  void OnPanel(const ss::PanelBody& p) {
    frame_.panel = true;
    frame_.panel_body = p;
  }
  void OnWindow(const ss::WindowBody& w) {
    frame_.window = true;
    frame_.window_body = w;
  }
  // ends_frame: stream_split::WalkEndsFrame of the state of the kWalkEnd.
  void OnWalkEnd(bool ends_frame) {
    if (ends_frame) picture_ended_ = true;
  }
  void OnFrameEnd() {
    if (!picture_ended_) return;
    layers_ = frame_;
    ++pictures_;
    picture_ended_ = false;
  }
  // The layers of the last picture. Empty when its frame has none.
  const FrameLayers& Get() const { return layers_; }
  uint64_t Pictures() const { return pictures_; }

 private:
  FrameLayers frame_;
  bool picture_ended_ = false;
  FrameLayers layers_;
  uint64_t pictures_ = 0;
};

// THE RECORD OF THE LAYERS (xr_layer_record, a test aid of the mod): a stream
// (scene_stream.h, with its head) with one frame for each frame of the
// scene: kFrame, the layer records of WriteFrame, kFrameEnd. The pictures of
// the cards go when they changed since the frame before in the record.
//
// THE SAME CALLS. Two frames have the same calls when their records kCard,
// kPanel and kWindow have the same bytes. A kCardPicture goes only when its
// reader does not have the picture, and the kCard holds its hash, so it is
// not compared. Normal: the layer records of a frame with no kCardPicture,
// in their order.
inline std::vector<uint8_t> Normal(const uint8_t* records, size_t n) {
  std::vector<uint8_t> out;
  ss::Reader r = ss::Reader::Records(records, n);
  ss::Kind kind;
  const uint8_t* body = nullptr;
  uint32_t bytes = 0;
  while (r.Next(&kind, &body, &bytes)) {
    if (kind != ss::kCard && kind != ss::kPanel && kind != ss::kWindow) continue;
    const uint32_t head[2] = {static_cast<uint32_t>(kind), bytes};
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(head),
               reinterpret_cast<const uint8_t*>(head) + sizeof(head));
    out.insert(out.end(), body, body + bytes);
  }
  return out;
}

}  // namespace quad_layer_math
