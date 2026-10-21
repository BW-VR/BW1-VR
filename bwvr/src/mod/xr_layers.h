// THE QUAD LAYERS OF THE SESSION FOR THE HEADSET APP (docs/android.md, "The
// quad layers", src/mod/quad_layer_math.h).
//
// With xr_input=1 the mod has no OpenXR session. The cards, the panel quads
// and the cut scene window are layers of a session (XrScreen::StageCard,
// SetPanelQuads, StagePanelQuad, StageWindow). So with xr_input_layers=1 the
// interface of each frame goes as a session with a projection layer routes
// it, and at the place where the session takes the layers (the end of the
// frame, and SetPanelQuads) the mod writes them into the frame of the live
// stream. The app shows them as quad layers over its projection layer.
#pragma once

#include <stdint.h>

#include "../vr/xr_screen.h"

struct XrLayersOptions {
  // xr_input_layers: 1 (the default) with xr_input=1 the interface goes as in
  // a session with a projection layer (the hint cards, the subtitle card, the
  // ring, the panel quads, the cut scene window), and the layers go into the
  // frames of the live stream. 0: the interface goes as with no headset, and
  // the stream has no layers.
  int input_layers = 1;
  // fake_head_layers (a test aid): 1 the invented head (fake_head=1) routes
  // the interface as a session does, so a desk run with no headset has the
  // layers of a session for the record. 0 (the default): the routes of no
  // headset.
  int fake_layers = 0;
  // xr_layer_record=N (a test aid): the layers of the first N frames of the
  // scene go to bwvr-layer.rec beside runblack.exe, from the source of the
  // run: the session, the invented head, or the input channel. 0: no record.
  int record_frames = 0;
};

void XrLayersSetOptions(const XrLayersOptions& options, bool input_channel);
// Opens the record. dir: the folder of runblack.exe, with a slash at its end.
void XrLayersStart(const char* dir);

// True when the interface of this frame goes as in a session with layers:
// the session, the input channel with xr_input_layers=1, or the invented head
// with fake_head_layers=1.
bool XrLayersRoute(bool to_headset, bool channel, bool fake_views);
// True while frames go into the record.
bool XrLayersRecording();

// THE PANEL QUADS of this frame, at the place of XrScreen::SetPanelQuads:
// the two quads and the Creature panel that SetPanelQuads takes. width and
// height: the size of the panel image. session: a session takes the stage
// after this call, so the stage stays.
void XrLayersPanel(uint32_t width, uint32_t height, const bwvr::PanelQuad quads[2],
                   bool session);

// THE END OF THE FRAME, at the place of XrScreen::EndFrame. frame: the frame
// of the controllers of the frame (as kView). source: 0 the session (the
// stage stays for EndFrame), 1 the invented head, 2 the input channel.
// to_stream: the layers go into the frame of the live stream.
void XrLayersEndFrame(uint64_t frame, int source, bool to_stream);
// A frame that ends with no picture: the stage goes, as EndFrame of a frame
// with no eye picture shows no layer.
void XrLayersDrop();

void XrLayersReport();
