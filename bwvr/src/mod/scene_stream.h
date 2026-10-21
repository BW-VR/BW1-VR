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

#include <stddef.h>
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
  kView = 18,         // ViewBody: the projection layer of the frame (the back channel)
};
// The last kind. A record of a higher kind is not of the stream.
constexpr uint32_t kLastKind = kView;

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

// VkSceneTexture.
struct TextureBody {
  uint32_t stage;
  uint32_t reserved;
  uint64_t surface;
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

// THE PROJECTION LAYER OF A FRAME (kView), THE BACK CHANNEL TO THE HEADSET APP
// (docs/android.md, "The projection layer"). With xr_input=1 the mod has no
// OpenXR session, and the app shows the eye pictures. So the mod gives the
// app what it gives a session of its own for the frame (XrScreen,
// src/vr/xr_screen.h): SetProjection, SetRenderHalfAngle, SetRenderPose,
// the size of the eye pictures, and the angles of view that the layer gets
// from them (LayerFov). The record is in the frame of its eye pictures: after
// kFrame and before the first walk.
//
// The bits of ViewBody::flags.
constexpr uint32_t kViewProjection = 1u << 0;  // SetProjection(true): a projection layer, not a screen
constexpr uint32_t kViewRenderPose = 1u << 1;  // the poses of SetRenderPose. Else the views of the frame
constexpr uint32_t kViewValid = 1u << 2;       // both views of the frame are valid
constexpr uint32_t kViewFlat = 1u << 3;        // a frame of the cut scene window (a screen)

// One eye of the layer, in meters in the LOCAL space of the session.
struct ViewEye {
  float position[3];
  float orientation[4];  // x y z w
  float fov[4];          // left, right, up, down in radians, signed, as XrFovf
};

struct ViewBody {
  uint32_t flags;          // kViewProjection, kViewRenderPose, kViewValid, kViewFlat
  uint32_t input_frame;    // SlotData::frame of the input of the frame. 0 with no input channel
  uint64_t frame;          // the frame of the controllers, as kFrame
  int64_t display_time;    // ns, the display time of the input of the frame
  uint32_t width, height;  // the size of each eye picture of the layer
  float half_angle;        // SetRenderHalfAngle, radians. 0: none
  uint32_t reserved;
  ViewEye eyes[2];
};

// The mod is 32 bit and the app is 64 bit: each body has one size on both.
static_assert(sizeof(StreamState) == 40, "StreamState");
static_assert(sizeof(StartBody) == 100, "StartBody");
static_assert(sizeof(FrameBody) == 24, "FrameBody");
static_assert(sizeof(GazeBody) == 12, "GazeBody");
static_assert(sizeof(ViewportBody) == 24, "ViewportBody");
static_assert(sizeof(TransformBody) == 16, "TransformBody");
static_assert(sizeof(WalkBeginBody) == 616, "WalkBeginBody");
static_assert(sizeof(TextureBody) == 16, "TextureBody");
static_assert(sizeof(DrawBody) == 64, "DrawBody");
static_assert(sizeof(TargetBody) == 48, "TargetBody");
static_assert(sizeof(ClearBody) == 16, "ClearBody");
static_assert(sizeof(SurfaceBody) == 24, "SurfaceBody");
static_assert(sizeof(ViewEye) == 44, "ViewEye");
static_assert(sizeof(ViewBody) == 128, "ViewBody");
static_assert(offsetof(ViewBody, frame) == 8 && offsetof(ViewBody, display_time) == 16 &&
                  offsetof(ViewBody, width) == 24 && offsetof(ViewBody, half_angle) == 32 &&
                  offsetof(ViewBody, eyes) == 40,
              "ViewBody offsets");
static_assert(offsetof(ViewEye, orientation) == 12 && offsetof(ViewEye, fov) == 28,
              "ViewEye offsets");

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
  // Whole records of another writer (with no head of a stream), in their
  // order. n is a multiple of 4.
  void Append(const uint8_t* records, size_t n) {
    bytes_.insert(bytes_.end(), records, records + n);
  }

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
  // Records with no head of a stream before them: a frame of the live
  // stream (stream_ring.h).
  static Reader Records(const uint8_t* data, size_t size) {
    Reader r(data, 0);
    r.size_ = size;
    r.ok_ = true;
    r.at_ = 0;
    return r;
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
