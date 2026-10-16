// THE STREAM OF THE SCENE WALKS (docs/android.md, "The stream of the
// walks"). No Windows code, so the mod and the headset app use it, and the
// math test checks it (test/controller_math/stream_checks.h).
//
// With backend=vulkan the device of the mod gives each call of a walk to the
// Vulkan renderer (vk_scene.h). The stream is a record of these calls: the
// mod writes it (vk_stream.cpp), and the app reads it and gives each call to
// the same renderer (android/src/scene_player.cpp). The calls of the
// renderer that read other parts of the mod (one walk, the eye pass, the
// panel target) get their values from the records too (StreamState).
//
// A record is a head of 8 bytes (the kind and the bytes of the body) and the
// body. Each record starts on 4 bytes. A surface of the game is its id: the
// address of the surface in the game process. A surface goes into the stream
// with its pixels (kSurface) before the first record that uses it, and again
// after the game writes it.
#pragma once

#include <stdint.h>
#include <string.h>

#include <vector>

namespace scene_stream {

constexpr uint32_t kMagic = 0x53535742u;  // "BWSS"
constexpr uint32_t kVersion = 2;

enum Kind : uint32_t {
  kStart = 1,         // StartBody: the options of the renderer
  kFrame = 2,         // FrameBody: VkSceneFrame
  kOnlyFrame = 3,     // u32: VkSceneOnlyFrame
  kGaze = 4,          // GazeBody: VkSceneSetGaze
  kWalkBegin = 5,     // WalkBeginBody: VkSceneWalkBegin and the device state
  kWalkEnd = 6,       // StreamState: VkSceneWalkEnd
  kRenderState = 7,   // u32 state, u32 value
  kStageState = 8,    // u32 stage, u32 type, u32 value
  kTexture = 9,       // u32 stage, u64 surface
  kDraw = 10,         // DrawBody, the vertices, the indices
  kTarget = 11,       // TargetBody: VkSceneTarget
  kViewport = 12,     // ViewportBody: VkSceneViewport
  kClear = 13,        // ClearBody, the rects (16 bytes each)
  kPanelBind = 14,    // u32: VkScenePanelBind
  kSurface = 15,      // SurfaceBody, the pixels (RGBA8)
  kWritten = 16,      // u64: VkSceneSurfaceWritten
  kFrameEnd = 17,     // u64 frame: the end of a frame of the game
};

// The values of the other parts of the mod that the renderer reads, at the
// time of a record (scene_host.h in the app).
struct StreamState {
  int32_t pass_eye;       // StereoPassEye
  int32_t intro_eye;      // IntroPassEye
  uint32_t one_walk;      // OneWalkActive
  float shift_per_rhw;    // OneWalkShiftPerRhw
  uint64_t bound_target;  // OneWalkBoundTarget
  int32_t one_walk_mode;  // OneWalkModeNow
  uint32_t reserved;
  uint64_t panel_target;  // the surface for which GpuBridgeIsPanel is true
};

// The options of the renderer at the start of the stream: the arguments of
// the setters of vk_scene.h, in the order of that header.
struct StartBody {
  int32_t mode, test_frame;               // VkSceneSetOptions
  int32_t follow;                         // VkSceneSetFollow
  int32_t eyes;                           // VkSceneSetEyes
  int32_t panel;                          // VkSceneSetPanel
  int32_t only, only_test;                // VkSceneSetOnly
  int32_t upload_queue, reread_test;      // VkSceneSetUploads
  int32_t direct;                         // VkSceneSetDirect
  int32_t keep_picture;                   // VkSceneSetKeepPicture
  int32_t black_check;                    // VkSceneSetBlackCheck
  int32_t untextured;                     // VkSceneSetUntextured
  int32_t stage1;                         // VkSceneSetStage1
  int32_t fovea_level, fovea_inner, fovea_outer, fovea_show;  // VkSceneSetFoveation
  int32_t mres_on, mres_outer, mres_center, mres_inset, mres_down;  // VkSceneSetFoveaRes
  int32_t texture_reuse;                  // VkSceneSetTextureReuse
  int32_t present_late;                   // VkSceneSetPresentLate
};

struct FrameBody {
  uint64_t frame;
  uint64_t name;
  uint32_t capture;
  uint32_t reserved;
};

struct GazeBody {
  uint32_t valid;
  float gx, gy;
};

constexpr int kStates = 64;       // vk_scene.cpp, kStates
constexpr int kStageStates = 32;  // vk_scene.cpp, kStageStates

// D3DVIEWPORT7.
struct ViewportBody {
  uint32_t x, y, width, height;
  float min_z, max_z;
};

// The angles of the camera of the game (bw::tech::InfoTransform), which
// VkSceneWalkBegin reads for the foveation.
struct TransformBody {
  float inv_half_tan_fov_x;
  int32_t res_x, res_y;
  uint32_t reserved;
};

// The state of the device that VkSceneWalkBegin reads.
struct WalkBeginBody {
  uint64_t device;
  StreamState state;
  uint32_t rs[kStates];
  uint32_t ts[2][kStageStates];
  uint64_t texture[2];
  ViewportBody viewport;
  TransformBody transform;
};

struct DrawBody {
  StreamState state;
  uint32_t type, fvf;
  uint32_t vertex_count, index_count;
  uint32_t vertex_bytes;  // the vertices follow, then index_count indices of 16 bits
  uint32_t reserved;
};

struct TargetBody {
  StreamState state;
  uint64_t surface;
};

struct ClearBody {
  uint32_t count, flags, color;
  float z;
};

struct SurfaceBody {
  uint64_t surface;
  uint32_t caps;  // DDSCAPS2::dwCaps
  uint32_t width, height;
  uint32_t has_pixels;  // 0: a render target with no pixels for a texture
};

// THE WRITER: records into a buffer.
class Writer {
 public:
  Writer() { Clear(); }
  void Clear() {
    bytes_.clear();
    const uint32_t head[2] = {kMagic, kVersion};
    Put(head, sizeof(head));
  }
  const std::vector<uint8_t>& Bytes() const { return bytes_; }

  // A record of one body and up to two blocks after it.
  void Record(Kind kind, const void* body, uint32_t body_bytes,
              const void* a = nullptr, uint32_t a_bytes = 0,
              const void* b = nullptr, uint32_t b_bytes = 0) {
    const uint32_t size = body_bytes + a_bytes + b_bytes;
    const uint32_t head[2] = {static_cast<uint32_t>(kind), size};
    Put(head, sizeof(head));
    Put(body, body_bytes);
    Put(a, a_bytes);
    Put(b, b_bytes);
    while ((bytes_.size() & 3) != 0) bytes_.push_back(0);
  }

 private:
  void Put(const void* p, uint32_t n) {
    if (n == 0 || p == nullptr) return;
    const uint8_t* b = static_cast<const uint8_t*>(p);
    bytes_.insert(bytes_.end(), b, b + n);
  }
  std::vector<uint8_t> bytes_;
};

// THE READER: the records of a buffer, one at a time.
class Reader {
 public:
  Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {
    uint32_t head[2] = {0, 0};
    if (size_ >= sizeof(head)) memcpy(head, data_, sizeof(head));
    ok_ = head[0] == kMagic && head[1] == kVersion;
    at_ = ok_ ? sizeof(head) : size_;
  }
  // False when the buffer does not start with the head of a stream.
  bool Ok() const { return ok_; }

  // The next record. False at the end, or when the rest is too short for
  // the record (then Broken() is true).
  bool Next(Kind* kind, const uint8_t** body, uint32_t* bytes) {
    if (at_ + 8 > size_) return false;
    uint32_t head[2];
    memcpy(head, data_ + at_, sizeof(head));
    if (at_ + 8 + head[1] > size_) {
      broken_ = true;
      return false;
    }
    *kind = static_cast<Kind>(head[0]);
    *body = data_ + at_ + 8;
    *bytes = head[1];
    at_ += 8 + head[1];
    at_ = (at_ + 3) & ~static_cast<size_t>(3);
    return true;
  }
  bool Broken() const { return broken_; }

 private:
  const uint8_t* data_;
  size_t size_;
  size_t at_ = 0;
  bool ok_ = false;
  bool broken_ = false;
};

// A body of a known size from a record. False when the record is shorter.
template <typename T>
bool Body(const uint8_t* body, uint32_t bytes, T* out) {
  if (bytes < sizeof(T)) return false;
  memcpy(out, body, sizeof(T));
  return true;
}

}  // namespace scene_stream
