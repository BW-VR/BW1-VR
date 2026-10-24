// THE INPUT CHANNEL in the mod (xr_input=1, docs/android.md, "The input
// channel"). The views of the eyes, the hands, the display time, the focus
// and the recenter come from bwvr-input.chan beside runblack.exe, which the
// headset app writes at each frame of its session
// (src/common/input_channel.h). The pulses of the hands go back to the app
// through the same file. The mod then starts no OpenXR session, and its
// frame waits for none.
//
// Each place that reads the session (XrScreen) reads the channel at the same
// place: the head in stereo.cpp and intro_vr.cpp, the hands in
// controllers.cpp and intro_skip.cpp, the display time in mod_main.cpp and
// stereo.cpp.
//
// THE LATCH. The first reader of a frame of the game reads the slot. Each
// later reader of the same frame gets the same values, because game logic
// reads back what the hooks store.
#pragma once

#include <stdint.h>

#include "../vr/xr_screen.h"
#include "scene_stream.h"

struct XrInputOptions {
  // xr_input: 0 the session or the fake sources (fake_head,
  // fake_controller). 1 the input channel.
  int mode = 0;
  // xr_input_record=N (a test aid): the input of the first N frames of the
  // controllers goes to bwvr-input.rec beside runblack.exe, from the source
  // of the run. 0: no record.
  int record_frames = 0;
  // xr_input_lockstep_ms=N (a test aid, with xr_input=1): each latch waits up
  // to N ms for the slot of its frame of the controllers. 0: the newest slot,
  // with no wait.
  int lockstep_ms = 0;
  // xr_view_record=N (a test aid): the projection layer of the first N
  // frames of the scene (view_layer_math.h) goes to bwvr-view.rec beside
  // runblack.exe, from the source of the run: what the mod gives its
  // session, the back channel of xr_input=1, or the invented head. 0: no
  // record.
  int view_record_frames = 0;
  // vk_pace: 0 (the default) no wait. 1: with vk_device=0 each frame of the
  // game waits for the next write of the slot of the app (XrInputPace).
  int pace = 0;
};

void XrInputSetOptions(const XrInputOptions& options);
bool XrInputOn();
// Maps the channel file. dir ends with a backslash. False with the reason
// in the log, and then the mod has no input.
bool XrInputStart(const char* dir);
// A new frame of the game: the next latch reads the slot again.
void XrInputNewFrame();
// Reads the slot, once a frame of the game.
void XrInputLatch();
// THE PACE (vk_pace=1 with vk_device=0, frame_pace_math.h): waits until the
// app writes a slot that no frame took, or until the time limit. Once a frame
// of the game: a second call before the next FinishFrame does not wait. A
// frame of the scene and of the intro calls it before its first latch, where
// a session of the PC waits in xrWaitFrame. A frame with no scene calls it at
// the end of its FinishFrame, after its screen frame went into the live
// stream. A latch after the wait marks its write as taken. A latch before
// it (the intro skip in FinishFrame) reads the newest slot only: it takes no
// write and leaves the latch of the frame open (frame_pace_math.h,
// ReadKind). True when the frame waited.
bool XrInputPace();

// The values of the last latch.
const bwvr::EyeView& XrInputEye(int index);
const bwvr::HandInput& XrInputHand(int index);
const char* XrInputProfile(int index);
bool XrInputFocused();
// True once for each recenter of the app.
bool XrInputTakeRecenter();
bool XrInputDisplayTime(int64_t* time, int64_t* period);
double XrInputDisplaySeconds();
bool XrInputGaze(float direction[3]);
// A pulse into the ring of the events.
void XrInputHaptic(int index, float amplitude, float seconds);

// The frame of the slot of the last latch (SlotData::frame). 0 with no slot.
uint32_t XrInputSlotFrame();

// THE RECORD (xr_input_record). True while frames go into the record.
bool XrInputRecording();
// One frame of the controllers. source: 0 the session, 1 the fake
// controller, 2 the input channel.
void XrInputRecordFrame(uint64_t frame, int source, const bwvr::EyeView eyes[2],
                        const bwvr::HandInput hands[2],
                        const char* const profiles[2], bool focused,
                        int64_t display_time, int64_t display_period);

// THE RECORD OF THE VIEWS (xr_view_record). True while frames go into it.
// source: 0 the session, 1 the invented head, 2 the input channel.
bool XrInputViewRecording();
void XrInputRecordView(const scene_stream::ViewBody& view, int source);

// The counts, for the log at the end and at each report.
void XrInputReport();
