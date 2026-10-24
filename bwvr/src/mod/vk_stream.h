// THE STREAM OF THE WALKS FOR THE HEADSET APP (vk_stream.cpp, docs/android.md,
// "The stream of the walks" and "The live stream").
//
// vk_stream_record of bwvr.ini (a test aid): 0 (the default) no record. N:
// from the frame that starts with the count N of the controllers (as
// vk_scene_test_frame), each call of the Vulkan renderer of vk_stream_frames
// frames (3 by default) goes into the stream format of scene_stream.h, and
// the stream goes to bwvr-stream.bin beside runblack.exe. The headset app
// gives the same calls to its own renderer.
//
// vk_stream_live of bwvr.ini: 0 (the default) no live stream. 1: the ring
// bwvr-stream.ring beside runblack.exe (stream_ring.h). When a reader asks,
// each frame of the game goes into the ring at the start of the next frame.
// A record and the live stream get the same bytes for a frame, and the
// first frame of a record has the whole state for both.
// vk_stream_live_drop_us: the wait in microseconds for room for a frame
// before the writer drops it. 0 (the default) drops at once. 4000 is a wait
// of 4 ms.
// vk_stream_live_drop: 1 (the default) a dropped frame keeps its records of
// state and loses its walks (stream_split.h), so each frame can drop and the
// frame thread waits only for the records of state. 0: a frame with a
// kStart, kSurface or kWritten record never drops, and a frame with none
// drops whole.
// vk_stream_live_end: 1 (the default) a frame goes to the record and to the
// ring at the end of its first walk with both eyes (stream_split.h), or with
// the quad layers (VkStreamSetLayers) after its layers, and a later walk of
// the frame goes at the start of the next frame. 0: at the
// start of the next frame.
//
// Both only with backend=vulkan: there the device of the mod gives each call
// on the frame thread, in the order of the walk.
//
// The files hold the pixels of the textures of the game. They stay on the
// PC of the person who made them, and never go into the repo.
#pragma once

#include <stdint.h>

#include "scene_stream.h"

void VkStreamSetOptions(int record_frame, int frames, int live, int drop_us,
                        int drop, int end);

// True while the record runs or a reader takes the frames. The calls below
// do nothing when it is false.
extern bool g_vk_stream_on;
inline bool VkStreamOn() { return g_vk_stream_on; }

// The latest arguments of the setters of vk_scene.h, for the record kStart.
scene_stream::StartBody* VkStreamOptions();

// vk_scene.cpp, at the start of each call of vk_scene.h. VkStreamFrame
// comes with each frame, also when no record runs: it starts and ends the
// record.
void VkStreamFrame(uint64_t frame, uint64_t name, bool capture);
void VkStreamOnlyFrame(bool can);
void VkStreamGaze(bool valid, float gx, float gy);
void VkStreamWalkBegin(void* device7);
void VkStreamWalkEnd();
void VkStreamRenderState(uint32_t state, uint32_t value);
void VkStreamStageState(uint32_t stage, uint32_t type, uint32_t value);
void VkStreamTexture(uint32_t stage, void* surface);
void VkStreamDraw(uint32_t type, uint32_t fvf, const void* vertices,
                  uint32_t vertex_count, const uint16_t* indices,
                  uint32_t index_count);
void VkStreamTarget(void* surface);
void VkStreamViewport(const void* viewport7);
void VkStreamClear(uint32_t count, const void* rects, uint32_t flags,
                   uint32_t color, float z);
void VkStreamPanelBind(bool panel);
void VkStreamWritten(void* surface);
// THE PROJECTION LAYER of the frame for the headset app (kView,
// view_layer_math.h). stereo.cpp gives it with xr_input=1, after
// VkSceneFrame and before the walks of the frame.
void VkStreamView(const scene_stream::ViewBody& view);
// THE QUAD LAYERS of the frame for the headset app (kCard, kCardPicture,
// kPanel, kWindow, quad_layer_math.h). VkStreamSetLayers(true), with
// xr_input=1 and xr_input_layers=1: the first walk with both eyes does not
// end the frame, and VkStreamLayers at the end of the frame of the scene
// writes the layers into the frame and then ends it (vk_stream_live_end=1).
// So the layers are in the frame of their eye pictures. A picture of a card
// goes only when the reader does not have it. False when no frame of the
// stream is open.
namespace quad_layer_math {
struct FrameLayers;
class CardPictures;
}  // namespace quad_layer_math
void VkStreamSetLayers(bool on);
bool VkStreamLayers(const quad_layer_math::FrameLayers& layers,
                    const quad_layer_math::CardPictures& pictures);

// THE SCREENS WITH NO SCENE (docs/android.md, "The screens with no scene",
// screen_quad_math.h). vk_stream_screens=1 (the default) with xr_input=1: on
// means each FinishFrame outside the eye passes is a screen frame of the
// stream (number screen_quad_math::ScreenFrame of its count), and a frame
// that the PC shows ends with a record kScreen: the source of the picture
// and the quad of distance_m and width_m (screen_distance_cm and
// screen_width_cm). 0, or xr_input=0: no screen frame. test_every
// (vk_stream_screen_test, a test aid): each Nth screen frame saves the
// pictures of its walk, as F12 does. 0: none.
void VkStreamSetScreens(bool on, int test_every, float distance_m, float width_m);
// mod_main.cpp, at the end of each FinishFrame outside the eye passes
// (screen_quad_math.h, ScreenRoute). shows: the PC shows this frame on its
// screen. flags: more flags of the kScreen (kScreenText). Ends the screen
// frame of this FinishFrame and starts the next one.
void VkStreamScreen(bool shows, float distance_m, float width_m, uint32_t flags);
// mod_main.cpp, at the end of a FinishFrame of an eye pass of the intro
// (intro_vr.cpp). An open screen frame ends with no kScreen, and the stream
// takes no record until its next frame (the intro gives no frame to the
// stream), so the walks of the intro do not collect in the open frame.
void VkStreamScreenStop();
// vk_scene.cpp, at the start and the end of each walk and at each write of
// the CPU into a surface, also with no record and no reader: the source of
// the picture (screen_quad_math.h, ScreenSource).
void VkStreamScreenWalkBegin(void* device7);
void VkStreamScreenWalkEnd();
void VkStreamScreenWritten(void* surface);
// One line with the counts of the screen frames.
void VkStreamScreenReport();

// vk_scene.cpp gives the read of a surface of the game as RGBA8 (ReadSurface),
// so the stream has the same pixels as the renderer of the PC. pixels must
// be freed with free().
bool VkSceneReadSurfaceRgba(void* surface, uint32_t* width, uint32_t* height,
                            uint8_t** pixels);

// A call to the record. In the build of the app (no _WIN32) there is no
// record, and the line is empty.
#ifdef _WIN32
#define BWVR_STREAM(call)      \
  do {                         \
    if (VkStreamOn()) call;    \
  } while (0)
#define BWVR_STREAM_OPTION(set) \
  do {                          \
    VkStreamOptions()->set;     \
  } while (0)
#else
#define BWVR_STREAM(call) \
  do {                    \
  } while (0)
#define BWVR_STREAM_OPTION(set) \
  do {                          \
  } while (0)
#endif
