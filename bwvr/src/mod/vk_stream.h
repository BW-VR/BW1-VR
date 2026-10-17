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
// ring at the end of its last walk. 0: at the start of the next frame.
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
