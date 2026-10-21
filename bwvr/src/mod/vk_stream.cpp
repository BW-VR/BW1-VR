// THE STREAM OF THE WALKS FOR THE HEADSET APP (vk_stream.h): the record to a
// file and the live stream through a ring.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdlib.h>

#include <set>
#include <vector>

#include "../common/bw_addresses.h"
#include "one_walk.h"
#include "quad_layer_math.h"
#include "stream_ring.h"
#include "stream_split.h"
#include "ui_panel.h"
#include "vk_prim_math.h"
#include "vk_scene.h"
#include "vk_stream.h"

void ModLog(const char* format, ...);
int StereoPassEye();
int IntroPassEye();

bool g_vk_stream_on = false;

namespace {

namespace ss = scene_stream;
namespace sr = stream_ring;

// THE RECORD (vk_stream_record).
int g_record_frame = 0;
int g_frames = 3;
uint64_t g_first = 0;
uint64_t g_last = 0;
bool g_record_on = false;
bool g_done = false;
std::vector<uint8_t> g_record;  // the head of a stream, then the frames
uint64_t g_draws = 0;
uint64_t g_surface_bytes = 0;

// THE FRAME THAT THE CALLS GO TO NOW. g_writer holds the head of a stream and
// then the records of one frame of the game. At the end of the first walk of
// the frame that has both eyes (vk_stream_live_end=1, stream_split.h
// WalkEndsFrame), or at the start of the next frame, the records go to the
// record and to the ring, so both get the same bytes. A record after the end of the frame (the tail)
// goes at the start of the next frame, in its order.
ss::StartBody g_options = {};
ss::Writer g_writer;
bool g_end_at_walk = true;  // vk_stream_live_end=1
// The layers of the frame end it (VkStreamSetLayers): the walk with both
// eyes does not.
bool g_end_at_layers = false;
// The pictures of the cards that the reader and the record have. A frame
// with the whole state starts with none.
quad_layer_math::SentPictures g_sent_pictures;
bool g_unit_ended = false;  // the frame went at its walk with both eyes, or at its layers
bool g_unit_open = false;
bool g_unit_record = false;
bool g_unit_live = false;
bool g_unit_keep = false;  // the frame has a kStart, a kSurface or a kWritten
uint64_t g_unit_frame = 0;
bool g_was_on = false;
// The surfaces in the stream, and the ones that the game wrote since their
// pixels went in.
std::set<uint64_t> g_known;
std::set<uint64_t> g_with_pixels;

// THE RING (vk_stream_live, stream_ring.h).
struct Live {
  bool want = false;  // vk_stream_live=1
  bool tried = false;
  HANDLE file = INVALID_HANDLE_VALUE;
  HANDLE mapping = nullptr;
  uint8_t* view = nullptr;
  sr::Head* head = nullptr;
  uint8_t* data = nullptr;
  char path[MAX_PATH] = {};
  bool reader = false;  // a reader asked, and the writer did not give up on it
  uint32_t answered = 0;
  uint32_t bye_seen = 0;
  uint32_t lost = 0;
  sr::Policy policy;
  // vk_stream_live_drop=1: a drop keeps the records of state of the frame
  // and loses its walks. 0: a frame with a surface waits, and a frame with
  // no surface drops whole.
  bool strip = true;
  std::vector<uint8_t> state;  // the records of state of a dropped frame
  // The reader made progress at this time: it took a byte, or the ring was
  // empty.
  uint32_t last_read = 0;
  LARGE_INTEGER progress = {};
  // The counts since the last ask, for the log.
  uint64_t units = 0;
  uint64_t bytes = 0;
  uint64_t dropped = 0;
  uint64_t drops_now = 0;  // the frames dropped since the reader was slow
  LARGE_INTEGER last_drop = {};
  uint64_t drops_said = 0;  // drops_now at the last line of the log
  uint64_t first_drop = 0;
  double wait_ms = 0.0;
  double wait_ms_max = 0.0;
  uint32_t wait_us_part = 0;  // the part of a ms of the waits for the head
};
Live g_live;
LARGE_INTEGER g_ticks = {};

uint64_t Id(const void* p) { return reinterpret_cast<uintptr_t>(p); }

double MsSince(const LARGE_INTEGER& t) {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return static_cast<double>(now.QuadPart - t.QuadPart) * 1000.0 /
         static_cast<double>(g_ticks.QuadPart);
}

// A file beside runblack.exe.
void GamePath(char* path, const char* name) {
  GetModuleFileNameA(GetModuleHandleA(nullptr), path, MAX_PATH);
  char* slash = strrchr(path, '\\');
  if (slash != nullptr) slash[1] = 0;
  strcat_s(path, MAX_PATH, name);
}

ss::StreamState State() {
  ss::StreamState s = {};
  s.pass_eye = StereoPassEye();
  s.intro_eye = IntroPassEye();
  s.one_walk = OneWalkActive() ? 1u : 0u;
  s.shift_per_rhw = OneWalkShiftPerRhw();
  s.bound_target = Id(OneWalkBoundTarget());
  s.one_walk_mode = OneWalkModeNow();
  return s;
}

// A surface goes into the stream before the first record that names it.
// pixels: also its pixels, for a texture. A surface that the game wrote goes
// in again at its next use as a texture.
void Surface(void* surface, bool pixels) {
  if (surface == nullptr) return;
  const uint64_t id = Id(surface);
  const bool known = g_known.count(id) != 0;
  const bool have = g_with_pixels.count(id) != 0;
  if (known && (!pixels || have)) return;
  ss::SurfaceBody body = {};
  body.surface = id;
  DDSCAPS2 caps = {};
  if (SUCCEEDED(static_cast<IDirectDrawSurface7*>(surface)->GetCaps(&caps))) {
    body.caps = caps.dwCaps;
  }
  uint8_t* rgba = nullptr;
  if (pixels && VkSceneReadSurfaceRgba(surface, &body.width, &body.height, &rgba)) {
    body.has_pixels = 1;
    const uint32_t bytes = body.width * body.height * 4;
    g_writer.Record(ss::kSurface, &body, sizeof(body), rgba, bytes);
    if (g_unit_record) g_surface_bytes += bytes;
    free(rgba);
    g_with_pixels.insert(id);
  } else {
    g_writer.Record(ss::kSurface, &body, sizeof(body));
  }
  g_unit_keep = true;
  g_known.insert(id);
}

void Finish() {
  g_record_on = false;
  g_done = true;
  char path[MAX_PATH];
  GamePath(path, "bwvr-stream.bin");
  FILE* f = nullptr;
  bool ok = false;
  if (fopen_s(&f, path, "wb") == 0 && f != nullptr) {
    ok = fwrite(g_record.data(), 1, g_record.size(), f) == g_record.size();
    fclose(f);
  }
  ModLog("vk stream    : %s frames %llu to %llu, %llu draws, %zu surfaces "
         "(%llu bytes of pixels), %zu bytes, to %s",
         ok ? "wrote" : "could not write", g_first, g_last, g_draws,
         g_known.size(), g_surface_bytes, g_record.size(), path);
  std::vector<uint8_t>().swap(g_record);
}

// THE RING FILE beside runblack.exe. Each start of the game makes a new
// head, so a reader of a run before asks again.
bool LiveOpen() {
  g_live.tried = true;
  QueryPerformanceFrequency(&g_ticks);
  LARGE_INTEGER start;
  QueryPerformanceCounter(&start);
  GamePath(g_live.path, "bwvr-stream.ring");
  const uint32_t bytes = sr::kHeadBytes + sr::kCapacity;
  g_live.file = CreateFileA(g_live.path, GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (g_live.file != INVALID_HANDLE_VALUE) {
    g_live.mapping = CreateFileMappingA(g_live.file, nullptr, PAGE_READWRITE, 0,
                                        bytes, nullptr);
  }
  if (g_live.mapping != nullptr) {
    g_live.view = static_cast<uint8_t*>(
        MapViewOfFile(g_live.mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes));
  }
  if (g_live.view == nullptr) {
    const DWORD error = GetLastError();
    // The handles of the steps that worked go, so the file is free again.
    if (g_live.mapping != nullptr) CloseHandle(g_live.mapping);
    if (g_live.file != INVALID_HANDLE_VALUE) CloseHandle(g_live.file);
    g_live.mapping = nullptr;
    g_live.file = INVALID_HANDLE_VALUE;
    ModLog("vk stream    : vk_stream_live=1, but the ring %s cannot be made "
           "(error %lu). There is no live stream",
           g_live.path, error);
    return false;
  }
  g_live.head = reinterpret_cast<sr::Head*>(g_live.view);
  g_live.data = g_live.view + sr::kHeadBytes;
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  sr::MakeHead(g_live.head, sr::kCapacity,
               GetCurrentProcessId() ^ static_cast<uint32_t>(now.QuadPart));
  ModLog("vk stream    : vk_stream_live=1. The ring %s has %u MB (made in "
         "%.1f ms). The frames of the walks go into it when a reader asks for "
         "them. vk_stream_live_drop_us=%u: a frame with no surface waits up "
         "to that time for room, then it drops",
         g_live.path, sr::kCapacity >> 20, MsSince(start), g_live.policy.drop_us);
  return true;
}

void LiveSummary(const char* why) {
  ModLog("vk stream    : %s. Since the last ask: %llu frames, %llu bytes, %llu "
         "frames dropped, a wait for room of %.1f ms in all and %.2f ms at most "
         "for one frame",
         why, g_live.units, g_live.bytes, g_live.dropped, g_live.wait_ms,
         g_live.wait_ms_max);
}

// The frame into the ring, by the choice of stream_ring.h.
void LivePut(uint64_t frame, const uint8_t* bytes, uint32_t n, bool keep) {
  sr::Head* h = g_live.head;
  LARGE_INTEGER start;
  QueryPerformanceCounter(&start);
  uint32_t done = 0;
  bool dropped = false;
  bool stripped = false;
  // With vk_stream_live_drop=1 each frame can drop its walks, so the whole
  // frame waits only up to vk_stream_live_drop_us for room.
  if (g_live.strip) keep = false;
  double wait_for_room = 0.0;
  for (;;) {
    const uint32_t r = sr::LoadAcquire(&h->read);
    if (r != g_live.last_read || r == h->write) {
      g_live.last_read = r;
      QueryPerformanceCounter(&g_live.progress);
    }
    const double waited = MsSince(start);
    const double stalled = MsSince(g_live.progress);
    // The whole frame of a writer that can drop the walks leaves room for
    // the records of state of the frames that come.
    uint32_t room = sr::Room(h->capacity, h->write, r);
    if (g_live.strip && !stripped && done == 0) {
      room = sr::RoomForWalks(room, g_live.policy.state_reserve);
    }
    const sr::Step step = sr::Choose(
        h->capacity, room, n - done, done > 0,
        keep, static_cast<uint32_t>(waited * 1000.0),
        static_cast<uint32_t>(stalled * 1000.0), g_live.policy);
    if (step == sr::Step::kWrite) {
      done += sr::Put(h, g_live.data, bytes + done, n - done);
      if (done == n) break;
      continue;
    }
    if (step == sr::Step::kDrop && g_live.strip && !stripped) {
      // The records of state go into the ring in parts, as the reader makes
      // room, and the walks go.
      stream_split::StateOnly(bytes, n, &g_live.state);
      bytes = g_live.state.data();
      n = static_cast<uint32_t>(g_live.state.size());
      done = 0;
      keep = true;
      stripped = true;
      dropped = true;
      continue;
    }
    if (step == sr::Step::kDrop) {
      dropped = true;
      break;
    }
    if (step == sr::Step::kLost) {
      g_live.reader = false;
      sr::StoreRelease(&h->lost, ++g_live.lost);
      char why[160];
      sprintf_s(why, "the reader took no byte for %.0f ms in frame %llu. The "
                "writer stops until a reader asks again", stalled, frame);
      LiveSummary(why);
      return;
    }
    // A wait spins up to vk_stream_live_drop_us. A longer wait (a frame
    // with a surface) sleeps, so the frame thread spins no longer than the
    // key says.
    LARGE_INTEGER wait_start;
    QueryPerformanceCounter(&wait_start);
    if (waited * 1000.0 < g_live.policy.drop_us) {
      YieldProcessor();
    } else {
      Sleep(1);
    }
    wait_for_room += MsSince(wait_start);
  }
  const double took = MsSince(start);
  const double waited = wait_for_room;
  // The stats go in before the write index of the next frame, so a reader
  // that has the next frame sees them.
  sr::FrameStat* stat = &h->stats[frame % sr::kStats];
  stat->frame = static_cast<uint32_t>(frame);
  stat->bytes = n;
  stat->wait_us = static_cast<uint32_t>(waited * 1000.0);
  stat->put_us = static_cast<uint32_t>(took * 1000.0);
  stat->dropped = dropped ? 1u : 0u;  // with vk_stream_live_drop=1: the walks went
  g_live.wait_ms += waited;
  if (waited > g_live.wait_ms_max) g_live.wait_ms_max = waited;
  g_live.wait_us_part += static_cast<uint32_t>(waited * 1000.0);
  sr::StoreRelease(&h->wait_ms, h->wait_ms + g_live.wait_us_part / 1000);
  g_live.wait_us_part %= 1000;
  if (stat->wait_us > h->wait_us_max) sr::StoreRelease(&h->wait_us_max, stat->wait_us);
  if (dropped) {
    ++g_live.dropped;
    if (stat->put_us > h->drop_us_max) sr::StoreRelease(&h->drop_us_max, stat->put_us);
    sr::StoreRelease(&h->dropped, h->dropped + 1);
    QueryPerformanceCounter(&g_live.last_drop);
    // One line when the reader becomes slow, then one for each 10000 drops.
    const bool first = g_live.drops_now++ == 0;
    if (!first && g_live.drops_now - g_live.drops_said >= 10000) {
      ModLog("vk stream    : the reader is still slow: %llu frames dropped since "
             "frame %llu was the first",
             g_live.drops_now, g_live.first_drop);
      g_live.drops_said = g_live.drops_now;
    }
    if (first) {
      g_live.first_drop = frame;
      g_live.drops_said = 0;
      if (stripped) {
        ModLog("vk stream    : the reader is slow. Frame %llu went into the "
               "ring with no walks: its %u bytes of state, in %.2f ms",
               frame, n, took);
      } else {
        ModLog("vk stream    : the reader is slow. Frame %llu (%u bytes, no "
               "surface) did not go into the ring after a wait of %.1f ms",
               frame, n, took);
      }
    }
    if (!stripped) return;
  }
  ++g_live.units;
  g_live.bytes += n;
  sr::StoreRelease(&h->units, h->units + 1);
  if (dropped) return;
  // The reader is not slow when 1 s went with no drop.
  if (g_live.drops_now > 0 && MsSince(g_live.last_drop) > 1000.0) {
    ModLog("vk stream    : the reader takes each frame again at frame %llu, after "
           "%llu dropped frames",
           frame, g_live.drops_now);
    g_live.drops_now = 0;
  }
}

// The frame in g_writer is whole: it goes to the record and to the ring.
void Deliver() {
  const std::vector<uint8_t>& b = g_writer.Bytes();
  const uint8_t* unit = b.data() + 8;  // after the head of a stream
  const size_t n = b.size() - 8;
  if (g_unit_record) g_record.insert(g_record.end(), unit, unit + n);
  if (g_unit_live && g_live.reader) {
    LivePut(g_unit_frame, unit, static_cast<uint32_t>(n), g_unit_keep);
  }
}

// At the start of a frame: true when a reader takes this frame. reset: the
// frame must have the whole state.
bool LiveFrame(uint64_t frame, bool* reset) {
  if (!g_live.want) return false;
  if (!g_live.tried && !LiveOpen()) g_live.want = false;
  if (g_live.head == nullptr) return false;
  sr::Head* h = g_live.head;
  sr::StoreRelease(&h->frame, static_cast<uint32_t>(frame));
  if (sr::TakeBye(h, &g_live.bye_seen) && g_live.reader) {
    g_live.reader = false;
    LiveSummary("the reader left. The writer stops until a reader asks again");
  }
  if (sr::TakeAsk(h, &g_live.answered)) {
    if (g_live.reader) LiveSummary("the reader asks again");
    g_live.reader = true;
    g_live.units = g_live.bytes = g_live.dropped = g_live.drops_now = 0;
    g_live.wait_ms = g_live.wait_ms_max = 0.0;
    g_live.last_read = sr::LoadAcquire(&h->read);
    QueryPerformanceCounter(&g_live.progress);
    *reset = true;
    ModLog("vk stream    : a reader asks for the whole state. Frame %llu has "
           "the options and each surface again",
           frame);
  }
  return g_live.reader;
}

}  // namespace

void VkStreamSetOptions(int record_frame, int frames, int live, int drop_us,
                        int drop, int end) {
  g_record_frame = record_frame > 0 ? record_frame : 0;
  g_frames = frames > 0 ? frames : 3;
  g_live.want = live != 0;
  g_live.policy.drop_us = drop_us > 0 ? static_cast<uint32_t>(drop_us) : 0u;
  g_live.strip = drop != 0;
  g_end_at_walk = end != 0;
  if (g_record_frame > 0) {
    ModLog("vk stream    : vk_stream_record=%d, vk_stream_frames=%d. The "
           "calls of the Vulkan renderer of these frames go to "
           "bwvr-stream.bin (a test aid for the headset app)",
           g_record_frame, g_frames);
  }
}

scene_stream::StartBody* VkStreamOptions() { return &g_options; }

void VkStreamFrame(uint64_t frame, uint64_t name, bool capture) {
  if ((g_record_frame == 0 || g_done) && !g_live.want && !g_unit_open) return;
  // intro_vr.cpp gives VkSceneFrame(0, ...) for its pictures. It is not a
  // frame of the game, so it ends no frame of the stream.
  if (g_unit_open && frame < g_unit_frame) {
    ss::FrameBody body = {frame, name, capture ? 1u : 0u, 0};
    g_writer.Record(ss::kFrame, &body, sizeof(body));
    return;
  }
  // The tail: the records after the walk with both eyes of the frame
  // before. They go at the start of this frame.
  std::vector<uint8_t> tail;
  bool tail_keep = false;
  if (g_unit_open && g_unit_ended) {
    tail.assign(g_writer.Bytes().begin() + 8, g_writer.Bytes().end());
    tail_keep = g_unit_keep;
  } else if (g_unit_open) {
    g_writer.Record(ss::kFrameEnd, &g_unit_frame, sizeof(g_unit_frame));
    Deliver();
  }
  g_unit_open = false;
  g_unit_ended = false;
  if (g_options.direct == 0) {
    // Only with backend=vulkan each call comes on the frame thread in the
    // order of the walk.
    if (g_record_frame > 0 && !g_done && frame == static_cast<uint64_t>(g_record_frame)) {
      ModLog("vk stream    : no record: it needs backend=vulkan");
      g_done = true;
    }
    if (g_live.want && frame > 0) {
      ModLog("vk stream    : no live stream: vk_stream_live needs backend=vulkan");
      g_live.want = false;
    }
    g_vk_stream_on = false;
    return;
  }
  bool reset = false;
  if (g_record_on && frame >= g_first + static_cast<uint64_t>(g_frames)) Finish();
  if (!g_record_on && !g_done && g_record_frame > 0 &&
      frame == static_cast<uint64_t>(g_record_frame)) {
    g_record_on = true;
    g_first = frame;
    g_writer.Clear();
    g_record.assign(g_writer.Bytes().begin(), g_writer.Bytes().end());
    reset = true;
  }
  const bool live = LiveFrame(frame, &reset);
  g_vk_stream_on = g_record_on || live;
  if (!g_vk_stream_on) {
    g_was_on = false;
    return;
  }
  // The first frame after a frame with no stream has the whole state: the
  // stream did not see what the game wrote in between.
  if (!g_was_on) reset = true;
  g_was_on = true;
  g_unit_record = g_record_on;
  g_unit_live = live;
  g_unit_keep = false;
  g_writer.Clear();
  if (reset) {
    // A frame with the whole state starts with kStart. The tail belongs to
    // a frame that the reader or the record does not have.
    g_known.clear();
    g_with_pixels.clear();
    g_sent_pictures.Reset();
    g_writer.Record(ss::kStart, &g_options, sizeof(g_options));
    g_unit_keep = true;
  } else if (!tail.empty()) {
    g_writer.Append(tail.data(), tail.size());
    g_unit_keep = tail_keep;
  }
  if (g_record_on) g_last = frame;
  g_unit_open = true;
  g_unit_frame = frame;
  ss::FrameBody body = {frame, name, capture ? 1u : 0u, 0};
  g_writer.Record(ss::kFrame, &body, sizeof(body));
}

void VkStreamOnlyFrame(bool can) {
  const uint32_t v = can ? 1u : 0u;
  g_writer.Record(ss::kOnlyFrame, &v, sizeof(v));
}

void VkStreamGaze(bool valid, float gx, float gy) {
  ss::GazeBody body = {valid ? 1u : 0u, gx, gy};
  g_writer.Record(ss::kGaze, &body, sizeof(body));
}

void VkStreamWalkBegin(void* device7) {
  if (device7 == nullptr) return;
  IDirect3DDevice7* d = static_cast<IDirect3DDevice7*>(device7);
  ss::WalkBeginBody body = {};
  body.device = Id(device7);
  body.state = State();
  for (int i = 1; i < ss::kStates; ++i) {
    DWORD v = 0;
    if (SUCCEEDED(d->GetRenderState(static_cast<D3DRENDERSTATETYPE>(i), &v))) body.rs[i] = v;
  }
  for (int stage = 0; stage < 2; ++stage) {
    for (int i = 1; i < ss::kStageStates; ++i) {
      DWORD v = 0;
      if (SUCCEEDED(d->GetTextureStageState(stage, static_cast<D3DTEXTURESTAGESTATETYPE>(i), &v))) {
        body.ts[stage][i] = v;
      }
    }
    IDirectDrawSurface7* texture = nullptr;
    if (SUCCEEDED(d->GetTexture(stage, &texture)) && texture != nullptr) {
      Surface(texture, true);
      body.texture[stage] = Id(texture);
      texture->Release();
    }
  }
  D3DVIEWPORT7 v = {};
  d->GetViewport(&v);
  body.viewport = {v.dwX, v.dwY, v.dwWidth, v.dwHeight, v.dvMinZ, v.dvMaxZ};
  const bw::tech::InfoTransform* info =
      reinterpret_cast<const bw::tech::InfoTransform*>(bw::tech::kInfoTransform);
  body.transform = {info->inv_half_tan_fov_x, info->res_x, info->res_y, 0};
  if (body.state.bound_target != 0) Surface(OneWalkBoundTarget(), false);
  g_writer.Record(ss::kWalkBegin, &body, sizeof(body));
}

void VkStreamWalkEnd() {
  const ss::StreamState state = State();
  g_writer.Record(ss::kWalkEnd, &state, sizeof(state));
  // The first walk with both eyes ends the frame, so the reader has it in the
  // game frame of its walks. With the layers (VkStreamSetLayers) the layers
  // end it.
  if (g_end_at_walk && !g_end_at_layers && g_unit_open && !g_unit_ended &&
      stream_split::WalkEndsFrame(state)) {
    g_writer.Record(ss::kFrameEnd, &g_unit_frame, sizeof(g_unit_frame));
    Deliver();
    g_unit_ended = true;
    g_unit_keep = false;
    g_writer.Clear();
  }
}

void VkStreamRenderState(uint32_t state, uint32_t value) {
  const uint32_t body[2] = {state, value};
  g_writer.Record(ss::kRenderState, body, sizeof(body));
}

void VkStreamStageState(uint32_t stage, uint32_t type, uint32_t value) {
  const uint32_t body[3] = {stage, type, value};
  g_writer.Record(ss::kStageState, body, sizeof(body));
}

void VkStreamTexture(uint32_t stage, void* surface) {
  Surface(surface, true);
  const ss::TextureBody body = {stage, 0, Id(surface)};
  g_writer.Record(ss::kTexture, &body, sizeof(body));
}

void VkStreamDraw(uint32_t type, uint32_t fvf, const void* vertices,
                  uint32_t vertex_count, const uint16_t* indices,
                  uint32_t index_count) {
  ss::DrawBody body = {};
  body.state = State();
  body.type = type;
  body.fvf = fvf;
  body.vertex_count = vertex_count;
  body.index_count = indices != nullptr ? index_count : 0;
  body.vertex_bytes = vk_prim_math::FvfStride(fvf) * vertex_count;
  if (vertices == nullptr) body.vertex_bytes = 0;
  g_writer.Record(ss::kDraw, &body, sizeof(body), vertices, body.vertex_bytes,
                  indices, body.index_count * 2);
  if (g_unit_record) ++g_draws;
}

void VkStreamTarget(void* surface) {
  Surface(surface, false);
  ss::TargetBody body = {};
  body.state = State();
  body.state.panel_target = GpuBridgeIsPanel(surface) ? Id(surface) : 0;
  body.surface = Id(surface);
  g_writer.Record(ss::kTarget, &body, sizeof(body));
}

void VkStreamViewport(const void* viewport7) {
  if (viewport7 == nullptr) return;
  const D3DVIEWPORT7* v = static_cast<const D3DVIEWPORT7*>(viewport7);
  ss::ViewportBody body = {v->dwX, v->dwY, v->dwWidth, v->dwHeight, v->dvMinZ, v->dvMaxZ};
  g_writer.Record(ss::kViewport, &body, sizeof(body));
}

void VkStreamClear(uint32_t count, const void* rects, uint32_t flags,
                   uint32_t color, float z) {
  ss::ClearBody body = {rects != nullptr ? count : 0, flags, color, z};
  g_writer.Record(ss::kClear, &body, sizeof(body), rects,
                  body.count * static_cast<uint32_t>(sizeof(D3DRECT)));
}

void VkStreamPanelBind(bool panel) {
  const uint32_t v = panel ? 1u : 0u;
  g_writer.Record(ss::kPanelBind, &v, sizeof(v));
}

void VkStreamView(const ss::ViewBody& view) {
  if (!g_unit_open) return;
  g_writer.Record(ss::kView, &view, sizeof(view));
}

void VkStreamSetLayers(bool on) { g_end_at_layers = on; }

bool VkStreamLayers(const quad_layer_math::FrameLayers& layers,
                    const quad_layer_math::CardPictures& pictures) {
  if (!g_vk_stream_on || !g_unit_open || g_unit_ended) return false;
  // A picture of a card is state that a later frame needs, as the pixels of
  // a surface: the frame keeps it when the writer drops the walks.
  for (int c = 0; c < bwvr::kCards; ++c) {
    if (layers.card[c] && g_sent_pictures.Needs(c, layers.cards[c].picture)) {
      g_unit_keep = true;
    }
  }
  quad_layer_math::WriteFrame(layers, pictures, &g_sent_pictures, &g_writer);
  if (g_end_at_walk) {
    g_writer.Record(ss::kFrameEnd, &g_unit_frame, sizeof(g_unit_frame));
    Deliver();
    g_unit_ended = true;
    g_unit_keep = false;
    g_writer.Clear();
  }
  return true;
}

void VkStreamWritten(void* surface) {
  if (surface == nullptr) return;
  Surface(surface, false);
  g_with_pixels.erase(Id(surface));
  const uint64_t id = Id(surface);
  g_writer.Record(ss::kWritten, &id, sizeof(id));
  g_unit_keep = true;
}
