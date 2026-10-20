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

// THE RECORD (xr_input_record). True while frames go into the record.
bool XrInputRecording();
// One frame of the controllers. source: 0 the session, 1 the fake
// controller, 2 the input channel.
void XrInputRecordFrame(uint64_t frame, int source, const bwvr::EyeView eyes[2],
                        const bwvr::HandInput hands[2],
                        const char* const profiles[2], bool focused,
                        int64_t display_time, int64_t display_period);

// The counts, for the log at the end and at each report.
void XrInputReport();
