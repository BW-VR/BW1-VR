// THE QUAD LAYERS OF THE SESSION FOR THE HEADSET APP. See xr_layers.h and
// docs/android.md, "The quad layers".

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "quad_layer_math.h"
#include "vk_stream.h"
#include "xr_layers.h"

extern void ModLog(const char* format, ...);
// controllers.cpp: a dialog box of the game takes text in a frame of the
// scene, and panel_keys=1.
bool ControllersPanelText();

namespace {

namespace ss = scene_stream;
namespace ql = quad_layer_math;

XrLayersOptions g_options;

// The panel quads of this frame (XrLayersPanel), until the end of the frame.
bool g_have_panel = false;
ss::PanelBody g_panel = {};
// The newest panel quads for the pointer on the panel, and the time of them.
ss::PanelBody g_last_panel = {};
ULONGLONG g_last_panel_tick = 0;
// The text flag of the last kPanel, for one line of the log at each change.
int g_last_text = 0;
int g_text_lines = 0;

// The newest picture of each card, for the hash of a picture and the pixels
// of a new one.
ql::CardPictures g_pictures;

// THE RECORD (xr_layer_record).
FILE* g_record = nullptr;
uint64_t g_recorded = 0;
ql::SentPictures g_record_sent;

// THE COUNTS, for the report.
uint64_t g_frames = 0;
uint64_t g_card_frames[bwvr::kCards] = {0, 0, 0, 0};
uint64_t g_panel_frames = 0;
uint64_t g_window_frames = 0;
uint64_t g_streamed = 0;
bool g_said_first = false;

// The layers of the frame as the session takes them. clear: the stage is
// empty after it, as after EndFrame.
void Collect(bool clear, ql::FrameLayers* f) {
  bwvr::StagedLayers s;
  bwvr::XrScreen::TakeStaged(&s, clear);
  f->panel = g_have_panel;
  f->panel_body = g_panel;
  g_have_panel = false;
  for (int c = 0; c < bwvr::kCards; ++c) {
    if (!s.card[c]) continue;
    f->card[c] = true;
    f->cards[c] = ql::MakeCard(c, s.cards[c], g_pictures.Take(c, s.cards[c]));
  }
  if (s.window) {
    f->window = true;
    f->window_body = ql::MakeWindow(s.window_layer);
  }
}

void Record(uint64_t frame, const ql::FrameLayers& f) {
  ss::Writer w;
  const ss::FrameBody body = {frame, 0, 0, 0};
  w.Record(ss::kFrame, &body, sizeof(body));
  ql::WriteFrame(f, g_pictures, &g_record_sent, &w);
  w.Record(ss::kFrameEnd, &frame, sizeof(frame));
  // The head of the stream goes only before the first frame.
  const size_t skip = g_recorded == 0 ? 0 : 8;
  const std::vector<uint8_t>& b = w.Bytes();
  fwrite(b.data() + skip, 1, b.size() - skip, g_record);
  ++g_recorded;
  if (g_recorded >= static_cast<uint64_t>(g_options.record_frames)) {
    fclose(g_record);
    g_record = nullptr;
    ModLog("xr layers    : the record of the layers has %llu frames of the "
           "scene, to frame %llu", g_recorded, frame);
  }
}

}  // namespace

void XrLayersSetOptions(const XrLayersOptions& options, bool input_channel) {
  g_options = options;
  VkStreamSetLayers(input_channel && options.input_layers != 0);
  if (input_channel) {
    ModLog("xr layers    : xr_input_layers=%d. %s", options.input_layers != 0 ? 1 : 0,
           options.input_layers != 0
               ? "The interface goes as in a session with a projection layer, and "
                 "the cards, the panel quads and the cut scene window go into the "
                 "frames of the live stream for the headset app"
               : "The interface goes as with no headset, and the live stream has "
                 "no layers");
  }
  if (options.fake_layers != 0) {
    ModLog("xr layers    : fake_head_layers=1. The invented head routes the "
           "interface as a session with a projection layer does (a test aid)");
  }
}

void XrLayersStart(const char* dir) {
  if (g_options.record_frames <= 0) return;
  char rec[MAX_PATH];
  lstrcpynA(rec, dir, MAX_PATH);
  lstrcatA(rec, "bwvr-layer.rec");
  if (fopen_s(&g_record, rec, "wb") != 0) g_record = nullptr;
  if (g_record != nullptr) {
    setvbuf(g_record, nullptr, _IOFBF, 1 << 20);
    ModLog("xr layers    : xr_layer_record=%d. The layers of the first %d frames "
           "of the scene go to %s (a test aid)",
           g_options.record_frames, g_options.record_frames, rec);
  } else {
    ModLog("xr layers    : xr_layer_record=%d, but %s cannot be made",
           g_options.record_frames, rec);
  }
}

bool XrLayersRoute(bool to_headset, bool channel, bool fake_views) {
  return to_headset || (channel && g_options.input_layers != 0) ||
         (fake_views && g_options.fake_layers != 0);
}

bool XrLayersRecording() { return g_record != nullptr; }

void XrLayersPanel(uint32_t width, uint32_t height, const bwvr::PanelQuad quads[2],
                   bool session) {
  bwvr::PanelQuad creature;
  const bool have = bwvr::XrScreen::TakeStagedPanelQuad(&creature, !session);
  g_panel = ql::MakePanel(width, height, quads, have, creature);
  // A dialog box of the game that takes text asks the app for its keyboard
  // (panel_keys=1, controllers.cpp).
  ql::MarkPanelText(&g_panel, ControllersPanelText());
  const int text = (g_panel.quads[0].visible & ss::kPanelQuadText) != 0 ? 1 : 0;
  if (text != g_last_text && g_text_lines < 40) {
    ++g_text_lines;
    ModLog("xr layers    : %s", text != 0
                                    ? "a dialog box on the menu panel quad takes text, so the "
                                      "kPanel asks the app for its keyboard"
                                    : "no dialog box on the menu panel quad takes text");
  }
  g_last_text = text;
  g_have_panel = true;
  g_last_panel = g_panel;
  g_last_panel_tick = GetTickCount64();
}

bool XrLayersLastPanel(ss::PanelBody* out, uint64_t* age_ms) {
  if (g_last_panel_tick == 0) return false;
  *out = g_last_panel;
  *age_ms = GetTickCount64() - g_last_panel_tick;
  return true;
}

void XrLayersEndFrame(uint64_t frame, int source, bool to_stream) {
  ql::FrameLayers f;
  Collect(source != 0, &f);
  ++g_frames;
  for (int c = 0; c < bwvr::kCards; ++c) {
    if (f.card[c] && ql::CardShows(f.cards[c])) ++g_card_frames[c];
  }
  if (f.panel) ++g_panel_frames;
  if (f.window) ++g_window_frames;
  if (to_stream) {
    if (VkStreamLayers(f, g_pictures)) {
      ++g_streamed;
      if (!g_said_first && f.Any()) {
        g_said_first = true;
        ModLog("xr layers    : frame %llu has the first layers in the live stream: "
               "%s%s%s%s%s%s", frame, f.panel ? "the panel quads, " : "",
               f.card[bwvr::kCardHintPointing] ? "the hint card of the hand that points, " : "",
               f.card[bwvr::kCardHintOther] ? "the hint card of the other hand, " : "",
               f.card[bwvr::kCardSubtitles] ? "the subtitle card, " : "",
               f.card[bwvr::kCardRing] ? "the ring, " : "",
               f.window ? "the cut scene window" : "");
      }
    }
  }
  if (g_record != nullptr) Record(frame, f);
}

void XrLayersDrop(bool to_stream) {
  bwvr::StagedLayers s;
  bwvr::XrScreen::TakeStaged(&s, true);
  g_have_panel = false;
  // The frame of the stream ends here with no layer record, as a frame with
  // layers ends at its end (quad_layer_math.h, WhereFrameEnds).
  if (to_stream) VkStreamLayers(ql::FrameLayers(), g_pictures);
}

void XrLayersReport() {
  if (g_frames == 0) return;
  ModLog("xr layers    : %llu frames, %llu of them into the live stream. Frames "
         "that show: %llu with %s, %llu with %s, %llu with %s, %llu with %s, %llu "
         "with the panel quads, %llu with the cut scene window. New pictures: "
         "%llu, %llu, %llu and %llu",
         g_frames, g_streamed, g_card_frames[0], ql::CardName(0), g_card_frames[1],
         ql::CardName(1), g_card_frames[2], ql::CardName(2), g_card_frames[3],
         ql::CardName(3), g_panel_frames, g_window_frames, g_pictures.Count(0),
         g_pictures.Count(1), g_pictures.Count(2), g_pictures.Count(3));
}
