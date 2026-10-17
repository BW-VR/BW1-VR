// THE SPLIT OF A FRAME OF THE STREAM OF THE WALKS (docs/android.md, "The live
// stream"). No Windows code, so the mod, the headset app, the test reader of
// the PC and the math test use it (test/controller_math/stream_ring_checks.h).
//
// A frame of the stream (stream_ring.h) has records of state and records of
// its walks. A frame that the app skips or that the writer drops keeps its
// records of state and loses its walks:
//
// - THE STATE stays: kStart (the options), kFrame and kFrameEnd, kGaze,
//   kOnlyFrame, kTarget and kPanelBind (the target of the draws of the
//   renderer, which a walk does not set again), kSurface and kWritten (the
//   pixels of the surfaces). A later frame needs them.
// - THE WALKS go: kWalkBegin to kWalkEnd with the draws, the clears, the
//   render states, the stage states, the textures and the viewports. Each
//   walk starts from the state of the device in its kWalkBegin
//   (WalkBeginBody: each render state, each stage state, both textures and
//   the viewport, BeginOnWorker of vk_scene.cpp), so a walk needs nothing of
//   an earlier walk.
//
// A known limit: with vk_keep_picture=1 a walk with no clear of the whole
// target starts with the picture of the walk before (the trails of the
// intro). After a frame that lost its walks, such a walk starts with an older
// picture. The walks of "pen" start with a clear of the whole target.
#pragma once

#include <stdint.h>
#include <string.h>

#include <vector>

#include "scene_stream.h"

namespace stream_split {

namespace ss = scene_stream;

// True for a record of a walk, which a frame with no walks does not have.
inline bool WalkKind(uint32_t kind) {
  return kind == ss::kWalkBegin || kind == ss::kWalkEnd || kind == ss::kDraw ||
         kind == ss::kClear || kind == ss::kRenderState || kind == ss::kStageState ||
         kind == ss::kTexture || kind == ss::kViewport;
}

// The records of state of a frame (records with no head of a stream, as in
// the ring), in their order, into out. Gives the count of the walks that
// went.
inline uint32_t StateOnly(const uint8_t* unit, size_t n, std::vector<uint8_t>* out) {
  out->clear();
  uint32_t walks = 0;
  size_t at = 0;
  while (at + 8 <= n) {
    uint32_t head[2];
    memcpy(head, unit + at, sizeof(head));
    const size_t end = (at + 8 + head[1] + 3) & ~static_cast<size_t>(3);
    if (at + 8 + head[1] > n) break;
    if (head[0] == ss::kWalkBegin) ++walks;
    if (!WalkKind(head[0])) {
      const size_t stop = end <= n ? end : n;
      out->insert(out->end(), unit + at, unit + stop);
      while ((out->size() & 3) != 0) out->push_back(0);
    }
    at = end;
  }
  return walks;
}

// The count of the walks of a frame.
inline uint32_t Walks(const uint8_t* unit, size_t n) {
  uint32_t walks = 0;
  size_t at = 0;
  while (at + 8 <= n) {
    uint32_t head[2];
    memcpy(head, unit + at, sizeof(head));
    if (at + 8 + head[1] > n) break;
    if (head[0] == ss::kWalkBegin) ++walks;
    at = (at + 8 + head[1] + 3) & ~static_cast<size_t>(3);
  }
  return walks;
}

// THE END OF A FRAME (vk_stream_live_end=1). A walk ends its frame when it is
// the last walk of the frame: the one walk of one_walk=1, the walk of the
// right eye (the second eye pass), or the walk of a frame with no eye passes.
// The rule is the rule of vk_scene.cpp for the walk of a frame that has both
// eyes (the picture of vk_scene_test_frame).
inline bool WalkEndsFrame(const ss::StreamState& s) {
  return s.one_walk != 0 || s.pass_eye != 0;
}

// THE QUEUE OF THE APP (stream_live_frames=N). The reader thread keeps the
// newest N frames whole, and each older frame loses its walks. A frame with a
// picture of a test stays whole. index 0 is the oldest of count frames.
inline bool KeepWalks(size_t index, size_t count, size_t bound, bool picture) {
  if (picture) return true;
  return index + bound >= count;
}

}  // namespace stream_split
