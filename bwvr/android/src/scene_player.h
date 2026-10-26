// THE PLAYER OF THE STREAM OF THE WALKS (docs/android.md, "The stream of the
// walks"). It gives each record of a stream (scene_stream.h) to the Vulkan
// renderer of the mod in the app (vk_scene.h), with the state of the walk
// for scene_host.cpp. The surfaces and the device of the game are objects of
// the stream (scene_player.cpp).
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../../src/common/input_channel.h"
#include "../../src/mod/quad_layer_math.h"
#include "../../src/mod/scene_stream.h"
#include "../../src/mod/screen_quad_math.h"

struct PlayResult {
  bool loaded = false;   // the file is a stream
  bool broken = false;   // the file stops in a record
  uint64_t records = 0;
  uint64_t frames = 0;
  uint64_t walks = 0;
  uint64_t draws = 0;
  uint64_t surfaces = 0;
  uint64_t views = 0;    // kView: the projection layer of a frame
  uint64_t layers = 0;   // kCard, kPanel, kWindow: the quad layers of a frame
  uint64_t card_pictures = 0;  // kCardPicture: a new picture of a card
  uint64_t screens = 0;  // kScreen: the screen of a frame with no scene
  uint64_t unknown = 0;  // records of a kind that the player does not know
  double ms = 0.0;
};

// The records of a buffer with no head of a stream into the renderer: a
// frame of the live stream (stream_reader.h). The counts go into result. The
// same player as PlayStreamFile, so the surfaces of a file and of the live
// stream are one set.
void PlayStreamRecords(const uint8_t* bytes, size_t size, PlayResult* result);

// THE PROJECTION LAYER OF THE LAST PICTURE of the renderer: the kView of the
// frame of that picture (src/mod/view_layer_math.h, PictureView). False when
// the frame of that picture has none.
bool StreamPictureView(scene_stream::ViewBody* out);
// The place of the player in the world of the frame of that picture (the
// World after the ViewBody of its kView, motion_comp_math.h). False when the
// frame of that picture has none.
bool StreamPictureWorld(input_channel::World* out);

// THE QUAD LAYERS OF THE LAST PICTURE: the cards, the panel quads and the cut
// scene window of the frame of that picture (src/mod/quad_layer_math.h,
// PictureLayers), and the newest picture of each card of the stream. Empty
// when the frame of that picture has none.
const quad_layer_math::FrameLayers& StreamPictureLayers();
const quad_layer_math::AppPictures& StreamCardPictures();

// THE SCREENS WITH NO SCENE (src/mod/screen_quad_math.h, ScreenPicture):
// which picture came last, a screen or the world, the screen of the newest
// screen picture, and the count of the screen pictures.
const screen_quad_math::ScreenPicture& StreamScreen();
// The last pixels of a surface of the stream (RGBA, top row first). False
// when the stream gave none.
bool StreamSurfacePixels(uint64_t id, const std::vector<uint8_t>** rgba, uint32_t* width,
                         uint32_t* height);

// The whole stream of `path` into the renderer. `folder` is the folder for
// the pictures of the renderer (vk_scene_test_frame of the stream).
PlayResult PlayStreamFile(const std::string& path, const std::string& folder);
