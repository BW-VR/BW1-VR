#include "scene_player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <map>
#include <memory>

#include "../../src/mod/quad_layer_math.h"
#include "../../src/mod/scene_stream.h"
#include "../../src/mod/stream_split.h"
#include "../../src/mod/view_layer_math.h"
#include "../../src/mod/vk_platform_posix.h"
#include "../../src/mod/vk_scene.h"
#include "app_log.h"
#include "game_folder.h"
#include "scene_host.h"

namespace {

namespace ss = scene_stream;

// A surface of the game from the stream: its caps and its last pixels.
struct StreamSurface : IDirectDrawSurface7 {
  DWORD caps = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> rgba;

  HRESULT GetCaps(DDSCAPS2* out) override {
    memset(out, 0, sizeof(*out));
    out->dwCaps = caps;
    return S_OK;
  }
  bool ReadRgba(uint32_t* w, uint32_t* h, uint8_t** pixels) override {
    if (rgba.empty()) return false;
    *pixels = static_cast<uint8_t*>(malloc(rgba.size()));
    if (*pixels == nullptr) return false;
    memcpy(*pixels, rgba.data(), rgba.size());
    *w = width;
    *h = height;
    return true;
  }
};

// The device of the game at the start of a walk.
struct StreamDevice : IDirect3DDevice7 {
  ss::WalkBeginBody body = {};
  IDirectDrawSurface7* textures[2] = {nullptr, nullptr};

  HRESULT GetRenderState(D3DRENDERSTATETYPE state, DWORD* value) override {
    const int i = static_cast<int>(state);
    if (i < 0 || i >= ss::kStates) return E_FAIL;
    *value = body.rs[i];
    return S_OK;
  }
  HRESULT GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type,
                               DWORD* value) override {
    const int i = static_cast<int>(type);
    if (stage > 1 || i < 0 || i >= ss::kStageStates) return E_FAIL;
    *value = body.ts[stage][i];
    return S_OK;
  }
  HRESULT GetTexture(DWORD stage, IDirectDrawSurface7** texture) override {
    if (stage > 1) return E_FAIL;
    *texture = textures[stage];
    return S_OK;
  }
  HRESULT GetViewport(D3DVIEWPORT7* v) override {
    v->dwX = body.viewport.x;
    v->dwY = body.viewport.y;
    v->dwWidth = body.viewport.width;
    v->dwHeight = body.viewport.height;
    v->dvMinZ = body.viewport.min_z;
    v->dvMaxZ = body.viewport.max_z;
    return S_OK;
  }
};

class Player {
 public:
  explicit Player(PlayResult* result) : result_(result) {}
  void SetResult(PlayResult* result) { result_ = result; }

  // The surface of an id. The stream gives each surface before its first
  // use, so an id that is new here is a surface with no pixels.
  StreamSurface* Surface(uint64_t id) {
    if (id == 0) return nullptr;
    std::unique_ptr<StreamSurface>& s = surfaces_[id];
    if (!s) s.reset(new StreamSurface());
    return s.get();
  }

  void State(const ss::StreamState& s) {
    // The angles of the camera stay from the start of the walk.
    SceneHostState h = SceneHostGetState();
    h.pass_eye = s.pass_eye;
    h.intro_eye = s.intro_eye;
    h.one_walk = s.one_walk != 0;
    h.shift_per_rhw = s.shift_per_rhw;
    h.bound_target = Surface(s.bound_target);
    h.one_walk_mode = s.one_walk_mode;
    h.panel_target = Surface(s.panel_target);
    SceneHostSetState(h);
  }

  const view_layer_math::PictureView& View() const { return view_; }
  const quad_layer_math::PictureLayers& Layers() const { return layers_; }
  const quad_layer_math::AppPictures& Pictures() const { return pictures_; }
  const screen_quad_math::ScreenPicture& Screen() const { return screen_; }
  // The surface of an id that the stream gave, or null.
  const StreamSurface* Find(uint64_t id) const {
    auto it = surfaces_.find(id);
    return it != surfaces_.end() ? it->second.get() : nullptr;
  }

  void Start(const ss::StartBody& o) {
    // The order of the setters of ReadSettings in mod_main.cpp, then the
    // backend of the mod (vk_ddraw.cpp).
    VkSceneSetOptions(o.mode, o.test_frame);
    VkSceneSetEyes(o.eyes != 0);
    VkSceneSetFollow(o.follow);
    VkSceneSetPanel(o.panel);
    VkSceneSetOnly(o.only, o.only_test);
    VkSceneSetUploads(o.upload_queue, o.reread_test);
    VkSceneSetPresentLate(o.present_late);
    VkSceneSetTextureReuse(o.texture_reuse);
    VkSceneSetKeepPicture(o.keep_picture);
    VkSceneSetBlackCheck(o.black_check);
    VkSceneSetUntextured(o.untextured);
    VkSceneSetStage1(o.stage1);
    VkSceneSetFoveation(o.fovea_level, o.fovea_inner, o.fovea_outer, o.fovea_show);
    VkSceneSetFoveaRes(o.mres_on, o.mres_outer, o.mres_center, o.mres_inset, o.mres_down);
    VkSceneSetDirect(o.direct != 0);
    AppLog("replay       : the stream starts: mode %d, test frame %d, direct %d, "
           "follow %d, panel %d, only %d, keep picture %d, stage1 %d",
           o.mode, o.test_frame, o.direct, o.follow, o.panel, o.only,
           o.keep_picture, o.stage1);
  }

  void Play(ss::Kind kind, const uint8_t* body, uint32_t bytes) {
    ++result_->records;
    switch (kind) {
      case ss::kStart: {
        ss::StartBody o;
        if (ss::Body(body, bytes, &o)) Start(o);
        break;
      }
      case ss::kFrame: {
        ss::FrameBody f;
        if (!ss::Body(body, bytes, &f)) break;
        VkSceneFrame(f.frame, f.name, f.capture != 0);
        view_.OnFrame();
        layers_.OnFrame();
        screen_.OnFrame();
        ++result_->frames;
        break;
      }
      case ss::kOnlyFrame: {
        uint32_t can = 0;
        if (ss::Body(body, bytes, &can)) VkSceneOnlyFrame(can != 0);
        break;
      }
      case ss::kGaze: {
        ss::GazeBody g;
        if (ss::Body(body, bytes, &g)) VkSceneSetGaze(g.valid != 0, g.gx, g.gy);
        break;
      }
      case ss::kSurface: {
        ss::SurfaceBody s;
        if (!ss::Body(body, bytes, &s)) break;
        StreamSurface* surface = Surface(s.surface);
        surface->caps = s.caps;
        if (s.has_pixels != 0) {
          const uint64_t n = static_cast<uint64_t>(s.width) * s.height * 4;
          if (bytes >= sizeof(s) + n) {
            surface->width = s.width;
            surface->height = s.height;
            surface->rgba.assign(body + sizeof(s), body + sizeof(s) + n);
          }
        }
        ++result_->surfaces;
        break;
      }
      case ss::kWalkBegin: {
        if (!ss::Body(body, bytes, &device_.body)) break;
        State(device_.body.state);
        SceneHostState h = SceneHostGetState();
        h.inv_half_tan_fov_x = device_.body.transform.inv_half_tan_fov_x;
        h.res_x = device_.body.transform.res_x;
        h.res_y = device_.body.transform.res_y;
        SceneHostSetState(h);
        device_.textures[0] = Surface(device_.body.texture[0]);
        device_.textures[1] = Surface(device_.body.texture[1]);
        VkSceneWalkBegin(&device_);
        ++result_->walks;
        break;
      }
      case ss::kWalkEnd: {
        ss::StreamState s;
        const bool have = ss::Body(body, bytes, &s);
        if (have) State(s);
        VkSceneWalkEnd();
        // The picture of the frame ends with this walk: the view of the
        // frame goes with it.
        if (have) view_.OnWalkEnd(stream_split::WalkEndsFrame(s));
        if (have) layers_.OnWalkEnd(stream_split::WalkEndsFrame(s));
        if (have) screen_.OnWalkEnd(s);
        break;
      }
      case ss::kView: {
        ss::ViewBody v;
        if (!ss::Body(body, bytes, &v)) break;
        view_.OnView(v);
        // The place of the player of the frame after the ViewBody.
        if (bytes >= sizeof(v) + sizeof(input_channel::World)) {
          input_channel::World w;
          memcpy(&w, body + sizeof(v), sizeof(w));
          view_.OnWorld(w);
        }
        ++result_->views;
        break;
      }
      case ss::kRenderState: {
        uint32_t v[2];
        if (ss::Body(body, bytes, &v)) VkSceneRenderState(v[0], v[1]);
        break;
      }
      case ss::kStageState: {
        uint32_t v[3];
        if (ss::Body(body, bytes, &v)) VkSceneStageState(v[0], v[1], v[2]);
        break;
      }
      case ss::kTexture: {
        ss::TextureBody t;
        if (ss::Body(body, bytes, &t)) VkSceneTexture(t.stage, Surface(t.surface));
        break;
      }
      case ss::kDraw: {
        ss::DrawBody d;
        if (!ss::Body(body, bytes, &d)) break;
        const uint64_t need = sizeof(d) + static_cast<uint64_t>(d.vertex_bytes) +
                              static_cast<uint64_t>(d.index_count) * 2;
        if (bytes < need) break;
        State(d.state);
        // The indices follow the vertices with no gap, so they can start on
        // 2 bytes only. A copy puts them on their own alignment.
        const uint8_t* vertices = body + sizeof(d);
        indices_.resize(d.index_count);
        if (d.index_count > 0) {
          memcpy(indices_.data(), vertices + d.vertex_bytes, d.index_count * 2);
        }
        VkSceneDraw(d.type, d.fvf, d.vertex_bytes > 0 ? vertices : nullptr,
                    d.vertex_count, d.index_count > 0 ? indices_.data() : nullptr,
                    d.index_count);
        ++result_->draws;
        break;
      }
      case ss::kMeshData: {
        // The data of a mesh of the game (gpu_mesh). It is a record of state:
        // a frame that loses its walks keeps it.
        ss::MeshDataBody d;
        if (!ss::Body(body, bytes, &d)) break;
        const uint64_t vertex_bytes = static_cast<uint64_t>(d.vertex_count) * 32;
        const uint64_t index_bytes = static_cast<uint64_t>(d.triangle_count) * 6;
        if (bytes < sizeof(d) + vertex_bytes + index_bytes) break;
        const uint8_t* vertices = body + sizeof(d);
        // The vertices and the indices follow the body with no gap. A copy
        // puts each on its own alignment.
        indices_.resize(static_cast<size_t>(d.triangle_count) * 3);
        memcpy(indices_.data(), vertices + vertex_bytes, index_bytes);
        mesh_vertices_.resize(vertex_bytes);
        memcpy(mesh_vertices_.data(), vertices, vertex_bytes);
        VkSceneMeshData(d.mesh, d.hash, mesh_vertices_.data(), d.vertex_count, indices_.data(),
                        d.triangle_count);
        ++result_->mesh_data;
        break;
      }
      case ss::kMeshDraw: {
        ss::MeshDrawBody d;
        if (!ss::Body(body, bytes, &d)) break;
        const uint64_t offset_bytes = static_cast<uint64_t>(d.offset_floats) * 4;
        if (bytes < sizeof(d) + offset_bytes) break;
        State(d.state);
        offsets_.resize(d.offset_floats);
        if (d.offset_floats > 0) memcpy(offsets_.data(), body + sizeof(d), offset_bytes);
        VkSceneDrawMesh(d.mesh, d.consts, d.offset_floats > 0 ? offsets_.data() : nullptr,
                        d.offset_floats);
        ++result_->mesh_draws;
        break;
      }
      case ss::kTarget: {
        ss::TargetBody t;
        if (!ss::Body(body, bytes, &t)) break;
        State(t.state);
        VkSceneTarget(Surface(t.surface));
        break;
      }
      case ss::kViewport: {
        ss::ViewportBody v;
        if (!ss::Body(body, bytes, &v)) break;
        D3DVIEWPORT7 d = {v.x, v.y, v.width, v.height, v.min_z, v.max_z};
        VkSceneViewport(&d);
        break;
      }
      case ss::kClear: {
        ss::ClearBody c;
        if (!ss::Body(body, bytes, &c)) break;
        if (bytes < sizeof(c) + static_cast<uint64_t>(c.count) * sizeof(D3DRECT)) break;
        rects_.resize(c.count);
        if (c.count > 0) memcpy(rects_.data(), body + sizeof(c), c.count * sizeof(D3DRECT));
        VkSceneClear(c.count, c.count > 0 ? rects_.data() : nullptr, c.flags,
                     c.color, c.z);
        break;
      }
      case ss::kPanelBind: {
        uint32_t panel = 0;
        if (ss::Body(body, bytes, &panel)) VkScenePanelBind(panel != 0);
        break;
      }
      case ss::kWritten: {
        uint64_t id = 0;
        if (ss::Body(body, bytes, &id)) VkSceneSurfaceWritten(Surface(id));
        break;
      }
      case ss::kCard: {
        ss::CardBody c;
        if (!ss::Body(body, bytes, &c)) break;
        layers_.OnCard(c);
        ++result_->layers;
        break;
      }
      case ss::kCardPicture:
        if (pictures_.OnPicture(body, bytes)) ++result_->card_pictures;
        break;
      case ss::kPanel: {
        ss::PanelBody p;
        if (!ss::Body(body, bytes, &p)) break;
        layers_.OnPanel(p);
        ++result_->layers;
        break;
      }
      case ss::kWindow: {
        ss::WindowBody w;
        if (!ss::Body(body, bytes, &w)) break;
        layers_.OnWindow(w);
        ++result_->layers;
        break;
      }
      case ss::kScreen: {
        ss::ScreenBody b;
        if (!ss::Body(body, bytes, &b)) break;
        screen_.OnScreen(b);
        ++result_->screens;
        break;
      }
      case ss::kFrameEnd:
        // The layers of the frame go with its picture, and the screen of a
        // frame with no scene gives the screen picture.
        layers_.OnFrameEnd();
        screen_.OnFrameEnd();
        break;
      default:
        ++result_->unknown;
        break;
    }
  }

 private:
  PlayResult* result_;
  std::map<uint64_t, std::unique_ptr<StreamSurface>> surfaces_;
  StreamDevice device_;
  view_layer_math::PictureView view_;
  quad_layer_math::PictureLayers layers_;
  quad_layer_math::AppPictures pictures_;
  screen_quad_math::ScreenPicture screen_;
  std::vector<uint16_t> indices_;
  std::vector<uint8_t> mesh_vertices_;
  std::vector<float> offsets_;
  std::vector<D3DRECT> rects_;
};

double NowMs() {
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

// The renderer and SceneHostState keep the addresses of the surfaces of the
// stream (the textures, the targets), so the one player of the process and
// its surfaces live until the end of the process. The app plays one stream
// in a process (main.cpp): the file or the live stream. So the map of the
// surfaces holds the surfaces of that stream alone. A play gives the
// player its result and takes it back before it returns.
Player* ThePlayer(PlayResult* result) {
  static Player* s_player = nullptr;
  if (s_player == nullptr) s_player = new Player(result);
  s_player->SetResult(result);
  return s_player;
}

}  // namespace

bool StreamPictureView(scene_stream::ViewBody* out) {
  return ThePlayer(nullptr)->View().Get(out);
}

bool StreamPictureWorld(input_channel::World* out) {
  return ThePlayer(nullptr)->View().GetWorld(out);
}

const quad_layer_math::FrameLayers& StreamPictureLayers() {
  return ThePlayer(nullptr)->Layers().Get();
}

const quad_layer_math::AppPictures& StreamCardPictures() {
  return ThePlayer(nullptr)->Pictures();
}

const screen_quad_math::ScreenPicture& StreamScreen() { return ThePlayer(nullptr)->Screen(); }

bool StreamSurfacePixels(uint64_t id, const std::vector<uint8_t>** rgba, uint32_t* width,
                         uint32_t* height) {
  const StreamSurface* s = ThePlayer(nullptr)->Find(id);
  if (s == nullptr || s->rgba.empty()) return false;
  *rgba = &s->rgba;
  *width = s->width;
  *height = s->height;
  return true;
}

void PlayStreamRecords(const uint8_t* bytes, size_t size, PlayResult* result) {
  ss::Reader reader = ss::Reader::Records(bytes, size);
  Player* player = ThePlayer(result);
  ss::Kind kind;
  const uint8_t* body = nullptr;
  uint32_t n = 0;
  while (reader.Next(&kind, &body, &n)) player->Play(kind, body, n);
  if (reader.Broken()) result->broken = true;
  // result belongs to the caller (App, a local of android_main).
  player->SetResult(nullptr);
}

PlayResult PlayStreamFile(const std::string& path, const std::string& folder) {
  PlayResult result;
  const std::string bytes = ReadTextFile(path);
  ss::Reader reader(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
  if (!reader.Ok()) {
    AppLog("replay       : %s is not a stream of the walks (%zu bytes)",
           path.c_str(), bytes.size());
    return result;
  }
  result.loaded = true;
  VkPlatformSetFolder(folder.c_str());
  const double start = NowMs();
  Player* player = ThePlayer(&result);
  ss::Kind kind;
  const uint8_t* body = nullptr;
  uint32_t size = 0;
  while (reader.Next(&kind, &body, &size)) player->Play(kind, body, size);
  // result is on the stack of this call.
  player->SetResult(nullptr);
  result.broken = reader.Broken();
  result.ms = NowMs() - start;
  AppLog("replay       : %s: %llu records, %llu frames, %llu walks, %llu draws, "
         "%llu surfaces, %llu of an unknown kind%s, in %.0f ms",
         path.c_str(), static_cast<unsigned long long>(result.records),
         static_cast<unsigned long long>(result.frames),
         static_cast<unsigned long long>(result.walks),
         static_cast<unsigned long long>(result.draws),
         static_cast<unsigned long long>(result.surfaces),
         static_cast<unsigned long long>(result.unknown),
         result.broken ? ", the file stops in a record" : "", result.ms);
  if (result.views > 0) {
    AppLog("replay       : %llu records of the projection layer of a frame (kView)",
           static_cast<unsigned long long>(result.views));
  }
  if (result.screens > 0) {
    AppLog("replay       : %llu records of the screen of a frame with no scene (kScreen)",
           static_cast<unsigned long long>(result.screens));
  }
  if (result.layers + result.card_pictures > 0) {
    AppLog("replay       : %llu records of the quad layers of a frame and %llu "
           "pictures of a card", static_cast<unsigned long long>(result.layers),
           static_cast<unsigned long long>(result.card_pictures));
  }
  return result;
}
