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

#include "gpu_mesh_math.h"

namespace scene_stream {

constexpr uint32_t kMagic = 0x53535742u;  // "BWSS"
// Version 3 has the record kScreen. Version 4 has the records kMeshData and
// kMeshDraw. The reader plays a stream of version 2 or 3 too: it has none of
// them.
constexpr uint32_t kVersion = 4;
constexpr uint32_t kOldestVersion = 2;

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
  kCard = 19,         // CardBody: a card of the frame (XrScreen::StageCard)
  kCardPicture = 20,  // CardPictureBody, the pixels: a new picture of a card
  kPanel = 21,        // PanelBody: the panel quads of the frame (XrScreen::SetPanelQuads)
  kWindow = 22,       // WindowBody: the cut scene window of the frame (XrScreen::StageWindow)
  kScreen = 23,       // ScreenBody: the screen of a frame with no scene (screen_quad_math.h)
  kMeshData = 24,     // MeshDataBody, the vertices (32 bytes each), the indices (16 bit): a mesh of the game
  kMeshDraw = 25,     // MeshDrawBody, the offsets: a draw of a mesh (gpu_mesh_math.h)
};
// The last kind. A record of a higher kind is not of the stream.
constexpr uint32_t kLastKind = kMeshDraw;

// The values of the other parts of the mod that the renderer reads, at the
// time of a record (scene_host.h in the app).
struct StreamState {
  int32_t pass_eye;       // StereoPassEye
  int32_t intro_eye;      // IntroPassEye
  uint32_t one_walk;      // OneWalkActive
  float shift_per_rhw;    // OneWalkShiftPerRhw
  uint64_t bound_target;  // OneWalkBoundTarget
  int32_t one_walk_mode;  // OneWalkModeNow
  uint32_t equal_depth;   // VkSceneEqualDepthWanted (vk_equal_depth)
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
  // The keys of the renderer and of the app that the debug page of the mod
  // changes (app_keys.h): the bit kSent and one bit for each key. 0: the frame
  // has none (a stream of an older mod), and the app keeps its values.
  uint32_t app_keys;
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
// kFrame and before the first walk. After the ViewBody the record can have
// an input_channel::World, the place of the player in the world of the frame
// (motion_comp_math.h). A reader that takes only the ViewBody reads the
// record as before.
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

// THE QUAD LAYERS OF A FRAME (kCard, kCardPicture, kPanel, kWindow), THE BACK
// CHANNEL TO THE HEADSET APP (docs/android.md, "The quad layers"). The mod
// gives the app what it gives a session of its own for the frame, at the
// place where the session takes it (XrScreen::EndFrame): the cards, the
// panel quads and the cut scene window. The records are in the frame of
// their eye pictures: after the walk that ends the picture and before
// kFrameEnd (src/mod/quad_layer_math.h).
//
// The bits of CardBody::flags.
constexpr uint32_t kCardVisible = 1u << 0;  // CardLayer::visible

// A card of the frame (bwvr::CardLayer). picture is the hash of the pixels
// of the rect (quad_layer_math.h, PictureHash): a kCardPicture with the same
// hash gives them, in this frame or in an earlier one.
struct CardBody {
  uint32_t card;    // kCardHintPointing, kCardHintOther, kCardSubtitles or kCardRing
  uint32_t flags;   // kCardVisible
  uint32_t texture_width, texture_height;
  int32_t rect[4];  // the part that shows: x, y, width, height
  float position[3];     // meters, the center, in the LOCAL space
  float orientation[4];  // x y z w, the front of the quad is its +z side
  float width, height;   // meters
  uint32_t reserved;
  uint64_t picture;
};

// A new picture of a card: then width * height pixels follow, BGRA, top row
// first, with the color multiplied by the alpha. They are the pixels of the
// rect of the card, and they go only when the picture changed.
struct CardPictureBody {
  uint32_t card;
  uint32_t width, height;  // rect[2] and rect[3] of the card
  uint32_t reserved;
  uint64_t picture;        // the hash of the pixels
};

// The bits of PanelQuadBody::visible. A reader that knows only kPanelQuadShows
// takes a value other than 0 as a quad that shows, so the mod sets
// kPanelQuadText only together with kPanelQuadShows.
constexpr uint32_t kPanelQuadShows = 1u << 0;  // bwvr::PanelQuad::visible
constexpr uint32_t kPanelQuadText = 1u << 1;   // a dialog box of the game on this quad takes text: the app shows its keyboard

// One panel quad (bwvr::PanelQuad). rect is in the pixels of the panel image.
struct PanelQuadBody {
  uint32_t visible;  // kPanelQuadShows, kPanelQuadText
  int32_t rect[4];
  float position[3];
  float orientation[4];
  float width, height;
};

// The panel quads of the frame: the menu and the help (SetPanelQuads) and the
// Creature panel (StagePanelQuad). The picture is the panel image of the
// renderer of the frame (VkScenePanelFor, vk_panel=1), which the app draws
// itself. width and height: the size of the panel image (the render size).
struct PanelBody {
  uint32_t width, height;
  PanelQuadBody quads[3];  // kPanelQuads of src/vr/xr_screen.h
};

// The cut scene window of the frame (bwvr::WindowLayer).
struct WindowBody {
  uint32_t screen, mask;
  int32_t crop, edge;
  float position[3];
  float orientation[4];
  float width, height;
  float wall_position[4][3];
  float wall_size[4][2];
  float alpha;
};

// THE SCREEN OF A FRAME WITH NO SCENE (kScreen), THE BACK CHANNEL TO THE
// HEADSET APP (docs/android.md, "The screens with no scene"). The logos, the
// videos, the front end and the dialog boxes of the game do not walk the
// scene. On the PC the mod shows such a frame on a screen fixed in the room
// (mod_main.cpp, h_FinishFrame). With xr_input=1 the mod gives the app the
// same screen in the frame of the stream: the source of the picture and the
// quad (src/mod/screen_quad_math.h). The record is the last record of its
// frame before kFrameEnd.
//
// The bits of ScreenBody::flags.
constexpr uint32_t kScreenCpu = 1u << 0;  // the picture is the pixels of `surface`. Else the picture of the last walk
constexpr uint32_t kScreenText = 1u << 1;  // a dialog box of the game takes text: the app shows its keyboard

struct ScreenBody {
  uint32_t flags;          // kScreenCpu
  uint32_t reserved0;
  uint64_t count;          // the screen frames of the mod since its start
  uint64_t surface;        // kScreenCpu: the render target. A kSurface with its pixels is before this record
  uint32_t width, height;  // the size of the render target
  float position[3];       // meters, the center, in the LOCAL space
  float orientation[4];    // x y z w, the front of the quad is its +z side
  float size[2];           // meters
  uint32_t reserved;
};


// THE MESHES ON THE GPU (kMeshData, kMeshDraw, docs/software-tnl.md, "The
// meshes on the GPU"). A mesh of the game goes into the stream one time, with
// the id of its address and a hash of its data (gpu_mesh_math.h, MeshHash).
// The renderer keeps it by the id. The writer sends it again when the hash
// changes, and with the whole state of a frame. A record of data is not a
// record of a walk (stream_split.h), so a frame that loses its walks keeps it.
// kMeshDraw names the mesh and has the values of the draw.
struct MeshDataBody {
  uint64_t mesh;            // the address of the mesh in the game process
  uint64_t hash;
  uint32_t vertex_count;    // vertices of 32 bytes follow
  uint32_t triangle_count;  // then 3 indices of 16 bits for each triangle
  uint32_t group_count;     // 0, or the groups of the mesh: then 16 bits for each vertex follow, its group
  uint32_t reserved;
};

// A kMeshDraw with the id kInlineMesh has its vertices in the record (a batch
// of vertices that comes with the draw, fn_0081C090): offset_floats is 8 for
// each vertex, group_count is the count of triangles, and the vertices (32 bytes
// each) and the indices (6 bytes for each triangle) follow the body.
constexpr uint64_t kInlineMesh = ~0ull;

// The offsets of kMeshDraw: 0 none, else 3 floats for each vertex follow the
// body (a person or an animal, fn_00859530). Then, for a mesh with groups
// (gpu_mesh_math::kGroup), 12 floats for each group (its matrix in the rows of
// the shader), and with kLit 4 floats for each group (its light).
struct MeshDrawBody {
  StreamState state;
  uint64_t mesh;
  gpu_mesh_math::Consts consts;
  uint32_t offset_floats;   // 0 or 3 * vertex_count
  uint32_t group_count;     // 0 or the groups of the draw
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
static_assert(sizeof(CardBody) == 80, "CardBody");
static_assert(offsetof(CardBody, texture_width) == 8 && offsetof(CardBody, rect) == 16 &&
                  offsetof(CardBody, position) == 32 && offsetof(CardBody, orientation) == 44 &&
                  offsetof(CardBody, width) == 60 && offsetof(CardBody, reserved) == 68 &&
                  offsetof(CardBody, picture) == 72,
              "CardBody offsets");
static_assert(sizeof(CardPictureBody) == 24 && offsetof(CardPictureBody, picture) == 16,
              "CardPictureBody");
static_assert(sizeof(PanelQuadBody) == 56 && offsetof(PanelQuadBody, rect) == 4 &&
                  offsetof(PanelQuadBody, position) == 20 &&
                  offsetof(PanelQuadBody, orientation) == 32 &&
                  offsetof(PanelQuadBody, width) == 48,
              "PanelQuadBody");
static_assert(sizeof(PanelBody) == 176 && offsetof(PanelBody, quads) == 8, "PanelBody");
static_assert(sizeof(WindowBody) == 136 && offsetof(WindowBody, crop) == 8 &&
                  offsetof(WindowBody, position) == 16 &&
                  offsetof(WindowBody, orientation) == 28 &&
                  offsetof(WindowBody, width) == 44 &&
                  offsetof(WindowBody, wall_position) == 52 &&
                  offsetof(WindowBody, wall_size) == 100 &&
                  offsetof(WindowBody, alpha) == 132,
              "WindowBody");
static_assert(sizeof(MeshDataBody) == 32, "MeshDataBody");
static_assert(sizeof(MeshDrawBody) == 40 + 8 + 96 + 8, "MeshDrawBody");
static_assert(sizeof(ScreenBody) == 72 && offsetof(ScreenBody, count) == 8 &&
                  offsetof(ScreenBody, surface) == 16 && offsetof(ScreenBody, width) == 24 &&
                  offsetof(ScreenBody, position) == 32 &&
                  offsetof(ScreenBody, orientation) == 44 && offsetof(ScreenBody, size) == 60 &&
                  offsetof(ScreenBody, reserved) == 68,
              "ScreenBody");

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

  // A record of one body and up to three blocks after it.
  void Record(Kind kind, const void* body, uint32_t body_bytes,
              const void* a = nullptr, uint32_t a_bytes = 0,
              const void* b = nullptr, uint32_t b_bytes = 0,
              const void* c = nullptr, uint32_t c_bytes = 0) {
    const uint32_t size = body_bytes + a_bytes + b_bytes + c_bytes;
    const uint32_t head[2] = {static_cast<uint32_t>(kind), size};
    // Range inserts: a copy of the bytes of each block, with no fill of new
    // bytes first and no push of a byte at a time (the buffer grows by the
    // factor of the vector). The padding to 4 bytes is the only fill.
    const size_t padded = (static_cast<size_t>(size) + sizeof(head) + 3) & ~static_cast<size_t>(3);
    const uint8_t* h = reinterpret_cast<const uint8_t*>(head);
    bytes_.insert(bytes_.end(), h, h + sizeof(head));
    Put(body, body_bytes);
    Put(a, a_bytes);
    Put(b, b_bytes);
    Put(c, c_bytes);
    const size_t pad = padded - (static_cast<size_t>(size) + sizeof(head));
    if (pad > 0) bytes_.insert(bytes_.end(), pad, 0);
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
    ok_ = head[0] == kMagic && head[1] >= kOldestVersion && head[1] <= kVersion;
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
