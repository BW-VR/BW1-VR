// THE PLAYER OF THE STREAM OF THE WALKS (docs/android.md, "The stream of the
// walks"). It gives each record of a stream (scene_stream.h) to the Vulkan
// renderer of the mod in the app (vk_scene.h), with the state of the walk
// for scene_host.cpp. The surfaces and the device of the game are objects of
// the stream (scene_player.cpp).
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../../src/mod/scene_stream.h"

struct PlayResult {
  bool loaded = false;   // the file is a stream
  bool broken = false;   // the file stops in a record
  uint64_t records = 0;
  uint64_t frames = 0;
  uint64_t walks = 0;
  uint64_t draws = 0;
  uint64_t surfaces = 0;
  uint64_t views = 0;    // kView: the projection layer of a frame
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

// The whole stream of `path` into the renderer. `folder` is the folder for
// the pictures of the renderer (vk_scene_test_frame of the stream).
PlayResult PlayStreamFile(const std::string& path, const std::string& folder);
