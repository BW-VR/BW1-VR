// THE TEST READER OF THE LIVE STREAM OF THE WALKS (docs/android.md, "The
// live stream"). A Windows program for a desktop run, with the ring code of
// the mod and the app (src/mod/stream_ring.h).
//
//   bwvr_stream_reader read <ring> <out folder> [attach frame] [last frame] [seconds] [slow ms] [lag frame]
//     Waits for a new head in the ring file, then for the frame <attach> of
//     the game, and asks for the whole state as a reader that comes late.
//     Takes each frame to the frame <last>, then leaves. Writes
//     ring-capture.bin (a stream: the head of a stream and the frames that it
//     got) and ring-frames.txt (each frame: its bytes, its records of a
//     surface, the wait of the writer). With slow ms the reader takes 1 MB
//     and then sleeps that time, so the ring is full (a test of the drop).
//     With lag frame the reader reads on to that frame, and counts the
//     frames that are at the reader in their game frame.
//
//   bwvr_stream_reader compare <ring-capture.bin> <bwvr-stream.bin>
//     Compares the bytes of each frame of the record with the frame of the
//     same number in the capture. Exit code 0 when each one is the same.
//
//   bwvr_stream_reader views <stream> [<bwvr-view.rec>]
//     Writes each record of the projection layer of a frame (kView,
//     src/mod/view_layer_math.h) of a stream or a capture. With a record of
//     the views of the same run (xr_view_record), compares each kView with
//     the view of the same frame in the record. Exit code 0 when the stream
//     has a kView and each one is the same as in the record.
//
//   bwvr_stream_reader view-compare <bwvr-view.rec> <bwvr-view.rec>
//     Compares two records of the views frame by frame (SameLayer: each
//     value but the frame of the input channel). Exit code 0 when each frame
//     of both is the same.
//
//   bwvr_stream_reader layers <stream> [<bwvr-layer.rec> or -] [<folder>]
//     Writes the quad layers of each frame of a stream or a capture (kCard,
//     kCardPicture, kPanel, kWindow, src/mod/quad_layer_math.h): their order
//     in the app, and each quad as the OpenXR test runtime of the app reports
//     it. Checks that the pixels of each picture of a card have its hash, and
//     that each card has a picture. With a record of the layers of the same
//     run (xr_layer_record), compares the layer records of each frame with
//     the frame in the record (Normal). With a folder, writes each picture of
//     a card there as card-<card>-<hash>.bmp. Exit code 0 when the stream has
//     layers, each picture is whole and each frame is the same as in the
//     record.
//
//   bwvr_stream_reader layer-compare <bwvr-layer.rec> <bwvr-layer.rec>
//     Compares the layers of two records frame by frame (Normal), and counts
//     the frames that differ only in the picture of a card, only in a card
//     that one record has, or in other values. Exit code 0 when no frame
//     differs in other values and both have the same frames.
//
//   bwvr_stream_reader window-feed <stream> <out stream> <first frame> <last frame> <crop> <edge>
//     A test feed of the cut scene window for the app: a copy of the stream
//     in which each frame from <first frame> to <last frame> is a frame of
//     the window as a screen (kViewFlat, and a kWindow of the test before kFrameEnd). The
//     values of the window are of the test, not of a run of the mod.
//
// The files hold the pixels of the textures of the game: keep them in the
// folder of the desktop run.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "../../src/mod/quad_layer_math.h"
#include "../../src/mod/stream_ring.h"
#include "../../src/mod/stream_split.h"
#include "../../src/mod/view_layer_math.h"

namespace {

namespace sr = stream_ring;
namespace ss = scene_stream;

constexpr size_t kLimit = 256u << 20;  // the largest record that can be in the stream

struct Mapped {
  HANDLE file = INVALID_HANDLE_VALUE;
  HANDLE mapping = nullptr;
  uint8_t* view = nullptr;
  uint64_t bytes = 0;
  void Close() {
    if (view != nullptr) UnmapViewOfFile(view);
    if (mapping != nullptr) CloseHandle(mapping);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    *this = Mapped();
  }
};

bool Map(const char* path, Mapped* m) {
  m->file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (m->file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size;
  if (!GetFileSizeEx(m->file, &size) || size.QuadPart < sr::kHeadBytes) {
    m->Close();
    return false;
  }
  m->bytes = static_cast<uint64_t>(size.QuadPart);
  m->mapping = CreateFileMappingA(m->file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
  if (m->mapping != nullptr) {
    m->view = static_cast<uint8_t*>(MapViewOfFile(m->mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
  }
  if (m->view == nullptr) {
    m->Close();
    return false;
  }
  return true;
}

double NowMs() {
  static LARGE_INTEGER f = {};
  if (f.QuadPart == 0) QueryPerformanceFrequency(&f);
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return static_cast<double>(t.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart);
}

// The records of a kind in a frame, and the bytes of their pixels.
void CountSurfaces(const uint8_t* unit, size_t n, int* surfaces, uint64_t* pixels) {
  *surfaces = 0;
  *pixels = 0;
  size_t at = 0;
  while (at + 8 <= n) {
    uint32_t head[2];
    memcpy(head, unit + at, sizeof(head));
    if (head[0] == ss::kSurface) {
      ++*surfaces;
      if (head[1] > sizeof(ss::SurfaceBody)) *pixels += head[1] - sizeof(ss::SurfaceBody);
    }
    at = (at + 8 + head[1] + 3) & ~static_cast<size_t>(3);
  }
}

bool WriteFile(const std::string& path, const std::vector<uint8_t>& bytes) {
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "wb") != 0 || f == nullptr) return false;
  const bool ok = fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
  fclose(f);
  return ok;
}

bool ReadFile(const char* path, std::vector<uint8_t>* bytes) {
  FILE* f = nullptr;
  if (fopen_s(&f, path, "rb") != 0 || f == nullptr) return false;
  _fseeki64(f, 0, SEEK_END);
  const long long n = _ftelli64(f);
  _fseeki64(f, 0, SEEK_SET);
  bytes->resize(static_cast<size_t>(n));
  const bool ok = fread(bytes->data(), 1, bytes->size(), f) == bytes->size();
  fclose(f);
  return ok;
}

struct Got {
  uint64_t frame;
  size_t at;     // in the capture, after its head
  size_t bytes;
  bool whole;
  int surfaces;
  uint64_t pixels;
  double ms;     // the time of the reader when the frame was whole
  uint32_t walks;
  uint32_t game;  // the frame of the game (the head) when the frame was whole
};

int Read(const char* ring, const char* out, uint32_t attach, uint32_t last, double seconds,
         int slow_ms, uint32_t lag_until) {
  const double start = NowMs();
  const double deadline = start + seconds * 1000.0;
  // A head that is there now is of a run before.
  uint32_t old_session = 0;
  bool had_old = false;
  {
    Mapped m;
    if (Map(ring, &m)) {
      const sr::Head* h = reinterpret_cast<const sr::Head*>(m.view);
      if (sr::HeadOk(h, m.bytes)) {
        old_session = h->session;
        had_old = true;
      }
      m.Close();
    }
  }
  printf("reader       : waits for a new head in %s%s\n", ring,
         had_old ? " (the head there is of a run before)" : "");
  Mapped m;
  sr::Head* h = nullptr;
  while (NowMs() < deadline) {
    if (m.view == nullptr && !Map(ring, &m)) {
      Sleep(50);
      continue;
    }
    h = reinterpret_cast<sr::Head*>(m.view);
    if (sr::HeadOk(h, m.bytes) && (!had_old || h->session != old_session)) break;
    h = nullptr;
    // The writer can make the file larger: map it again.
    m.Close();
    Sleep(50);
  }
  if (h == nullptr) {
    printf("reader       : FAULT no new head in %.0f s\n", seconds);
    return 2;
  }
  const uint32_t session = h->session;
  printf("reader       : a head of session %08x, %u MB, at %.1f s\n", session,
         h->capacity >> 20, (NowMs() - start) / 1000.0);
  while (sr::LoadAcquire(&h->frame) < attach && NowMs() < deadline) Sleep(1);
  const uint32_t frame_at_ask = sr::LoadAcquire(&h->frame);
  const uint32_t ask = sr::Ask(h);
  uint32_t sync = 0;
  const double asked = NowMs();
  while (!sr::Answered(h, ask, &sync) && NowMs() < deadline) Sleep(1);
  if (NowMs() >= deadline) {
    printf("reader       : FAULT no answer to the ask\n");
    return 2;
  }
  printf("reader       : asked at frame %u, the answer came after %.1f ms\n",
         frame_at_ask, NowMs() - asked);
  sr::StoreRelease(&h->read, sync);
  const uint32_t lost0 = sr::LoadAcquire(&h->lost);

  std::vector<uint8_t> capture;
  {
    ss::Writer w;  // the head of a stream
    capture = w.Bytes();
  }
  std::vector<uint8_t> pending;
  // A slow reader (slow_ms > 0, a test of a full ring) takes 1 MB, then
  // sleeps slow_ms.
  std::vector<uint8_t> chunk(slow_ms > 0 ? (1u << 20) : (4u << 20));
  if (slow_ms > 0) printf("reader       : a slow reader: 1 MB, then a sleep of %d ms\n", slow_ms);
  std::vector<Got> got;
  std::map<uint32_t, sr::FrameStat> stats;
  bool broken = false;
  bool done = false;
  // After the frame last: the lag of each frame with no kStart, up to the
  // frame lag_until, and no copy.
  int lag[3] = {0, 0, 0};
  bool lag_done = lag_until <= last;
  int code = 0;
  double done_at = 0.0;
  while (NowMs() < deadline) {
    if (sr::LoadAcquire(&h->lost) != lost0) {
      printf("reader       : FAULT the writer gave up on the reader\n");
      code = 3;
      break;
    }
    if (h->session != session) {
      printf("reader       : FAULT a new head in the ring\n");
      code = 3;
      break;
    }
    const uint32_t n = sr::Take(h, m.view + sr::kHeadBytes, chunk.data(),
                                static_cast<uint32_t>(chunk.size()), &broken);
    if (broken) {
      printf("reader       : FAULT the indices of the ring are not valid\n");
      code = 3;
      break;
    }
    if (n == 0) {
      if (done && lag_done && NowMs() - done_at > 200.0) break;
      // A reader that sleeps 1 ms sleeps about 15 ms on Windows, many frames
      // of the game. So the reader looks again at once, and the time from
      // the writer to the reader is the time of the ring alone.
      if (slow_ms > 0) {
        Sleep(1);
      } else {
        SwitchToThread();
      }
      continue;
    }
    pending.insert(pending.end(), chunk.begin(), chunk.begin() + n);
    if (slow_ms > 0) Sleep(static_cast<DWORD>(slow_ms));
    size_t used = 0;
    for (;;) {
      bool bad = false, whole = false;
      uint64_t frame = 0;
      const size_t len = sr::UnitBytes(pending.data() + used, pending.size() - used,
                                       kLimit, &bad, &frame, &whole);
      if (bad) {
        printf("reader       : FAULT a record head that is not of the stream\n");
        code = 3;
        break;
      }
      if (len == 0) break;
      // The stats of a frame are in the head before the next frame.
      for (const Got& g : got) {
        const sr::FrameStat s = h->stats[g.frame % sr::kStats];
        if (s.frame == static_cast<uint32_t>(g.frame) && stats.count(s.frame) == 0) {
          stats[s.frame] = s;
        }
      }
      if (done && !lag_done) {
        const int64_t late = static_cast<int64_t>(sr::LoadAcquire(&h->frame)) -
                             static_cast<int64_t>(frame);
        if (!whole) ++lag[late <= 0 ? 0 : (late == 1 ? 1 : 2)];
        if (frame >= lag_until) {
          lag_done = true;
          done_at = NowMs();
        }
      }
      if (!done) {
        Got g = {};
        g.frame = frame;
        g.at = capture.size() - 8;
        g.bytes = len;
        g.whole = whole;
        g.ms = NowMs() - start;
        g.game = sr::LoadAcquire(&h->frame);
        g.walks = stream_split::Walks(pending.data() + used, len);
        CountSurfaces(pending.data() + used, len, &g.surfaces, &g.pixels);
        capture.insert(capture.end(), pending.begin() + used, pending.begin() + used + len);
        got.push_back(g);
        if (frame >= last) {
          done = true;
          done_at = NowMs();
        }
      }
      used += len;
    }
    if (code != 0) break;
    pending.erase(pending.begin(), pending.begin() + used);
  }
  for (const Got& g : got) {
    const sr::FrameStat s = h->stats[g.frame % sr::kStats];
    if (s.frame == static_cast<uint32_t>(g.frame) && stats.count(s.frame) == 0) {
      stats[s.frame] = s;
    }
  }
  sr::Leave(h);
  printf("reader       : left the ring. The writer: %u frames in the ring, %u "
         "dropped, a wait for room of %u ms in all, %u us at most for one frame, "
         "%u us at most in the writer for a dropped frame\n",
         sr::LoadAcquire(&h->units), sr::LoadAcquire(&h->dropped),
         sr::LoadAcquire(&h->wait_ms), sr::LoadAcquire(&h->wait_us_max),
         sr::LoadAcquire(&h->drop_us_max));
  m.Close();

  const std::string folder(out);
  const bool wrote = WriteFile(folder + "\\ring-capture.bin", capture);
  std::string text;
  char line[256];
  sprintf_s(line, "frame\tbytes\tkStart\tsurfaces\tpixel bytes\twalks\twriter wait us\t"
                  "writer put us\treader ms\tgame frame at the reader\n");
  text += line;
  int in_time = 0, one_late = 0, later = 0, walkless = 0;
  for (const Got& g : got) {
    const auto s = stats.find(static_cast<uint32_t>(g.frame));
    if (s != stats.end()) {
      sprintf_s(line, "%llu\t%zu\t%d\t%d\t%llu\t%u\t%u\t%u\t%.1f\t%u\n", g.frame, g.bytes,
                g.whole ? 1 : 0, g.surfaces, g.pixels, g.walks, s->second.wait_us,
                s->second.put_us, g.ms, g.game);
    } else {
      sprintf_s(line, "%llu\t%zu\t%d\t%d\t%llu\t%u\t-\t-\t%.1f\t%u\n", g.frame, g.bytes,
                g.whole ? 1 : 0, g.surfaces, g.pixels, g.walks, g.ms, g.game);
    }
    text += line;
    const int64_t behind = static_cast<int64_t>(g.game) - static_cast<int64_t>(g.frame);
    if (behind <= 0) {
      ++in_time;
    } else if (behind == 1) {
      ++one_late;
    } else {
      ++later;
    }
    if (g.walks == 0) ++walkless;
  }
  WriteFile(folder + "\\ring-frames.txt", std::vector<uint8_t>(text.begin(), text.end()));
  printf("reader       : %zu frames (%llu to %llu), %zu bytes, to %s\\ring-capture.bin%s\n",
         got.size(), got.empty() ? 0ull : got.front().frame,
         got.empty() ? 0ull : got.back().frame, capture.size() - 8, out,
         wrote ? "" : " (FAULT: cannot write it)");
  printf("reader       : %d frames at the reader in their game frame, %d one game "
         "frame later, %d later. %d frames with no walks\n",
         in_time, one_late, later, walkless);
  if (lag_until > last) {
    printf("reader       : the frames %u to %u with no kStart: %d at the reader in "
           "their game frame, %d one game frame later, %d later\n",
           last + 1, lag_until, lag[0], lag[1], lag[2]);
  }
  if (!done && code == 0) {
    printf("reader       : FAULT the frame %u did not come in %.0f s\n", last, seconds);
    code = 4;
  }
  return wrote ? code : 5;
}

// The frames of a stream file: the frame of each one, and its place.
bool Units(const std::vector<uint8_t>& file, std::map<uint64_t, std::pair<size_t, size_t>>* out) {
  ss::Reader check(file.data(), file.size());
  if (!check.Ok()) return false;
  size_t at = 8;
  while (at < file.size()) {
    bool bad = false, whole = false;
    uint64_t frame = 0;
    const size_t len = sr::UnitBytes(file.data() + at, file.size() - at, kLimit, &bad,
                                     &frame, &whole);
    if (bad || len == 0) return at == file.size();
    (*out)[frame] = std::make_pair(at, len);
    at += len;
  }
  return true;
}

int Compare(const char* capture_path, const char* record_path) {
  std::vector<uint8_t> capture, record;
  if (!ReadFile(capture_path, &capture) || !ReadFile(record_path, &record)) {
    printf("compare      : FAULT cannot read the files\n");
    return 2;
  }
  std::map<uint64_t, std::pair<size_t, size_t>> a, b;
  const bool ok_a = Units(capture, &a);
  const bool ok_b = Units(record, &b);
  printf("compare      : the capture has %zu frames%s, the record %zu frames%s\n",
         a.size(), ok_a ? "" : " (it stops in a frame)", b.size(),
         ok_b ? "" : " (it stops in a frame)");
  int same = 0, same_state = 0, differ = 0;
  std::vector<uint8_t> state;
  for (const auto& r : b) {
    const auto c = a.find(r.first);
    if (c == a.end()) {
      printf("compare      : frame %llu: %zu bytes in the record, not in the capture\n",
             r.first, r.second.second);
      ++differ;
      continue;
    }
    const uint8_t* x = record.data() + r.second.first;
    const uint8_t* y = capture.data() + c->second.first;
    const size_t n = r.second.second < c->second.second ? r.second.second : c->second.second;
    size_t first = n;
    for (size_t i = 0; i < n; ++i) {
      if (x[i] != y[i]) {
        first = i;
        break;
      }
    }
    if (r.second.second == c->second.second && first == n) {
      printf("compare      : frame %llu: %zu bytes, the same bytes in the ring and in the record\n",
             r.first, r.second.second);
      ++same;
    } else if (stream_split::Walks(y, c->second.second) == 0 &&
               stream_split::StateOnly(x, r.second.second, &state) > 0 &&
               state.size() == c->second.second &&
               memcmp(state.data(), y, state.size()) == 0) {
      // A frame that the writer dropped: its records of state, with no walks.
      printf("compare      : frame %llu: dropped in the ring: its %zu bytes of state "
             "are the same as the records of state of the record\n",
             r.first, state.size());
      ++same_state;
    } else {
      printf("compare      : frame %llu: FAULT %zu bytes in the record, %zu in the "
             "ring, the first different byte at %zu\n",
             r.first, r.second.second, c->second.second, first);
      ++differ;
    }
  }
  printf("compare      : %d frames the same, %d frames with the same state and no walks, "
         "%d different\n", same, same_state, differ);
  return differ == 0 && same + same_state > 0 && ok_a && ok_b ? 0 : 1;
}

// THE VIEWS (kView).
std::string ViewText(const ss::ViewBody& v) {
  char line[512];
  sprintf_s(line, "frame %llu flags 0x%X input %u time %lld size %ux%u half %.6f", v.frame,
            v.flags, v.input_frame, v.display_time, v.width, v.height, v.half_angle);
  std::string text = line;
  for (int e = 0; e < 2; ++e) {
    const ss::ViewEye& eye = v.eyes[e];
    sprintf_s(line, " eye %d pose %.4f %.4f %.4f %.4f %.4f %.4f %.4f fov %.4f %.4f %.4f %.4f", e,
              eye.position[0], eye.position[1], eye.position[2], eye.orientation[0],
              eye.orientation[1], eye.orientation[2], eye.orientation[3], eye.fov[0], eye.fov[1],
              eye.fov[2], eye.fov[3]);
    text += line;
  }
  return text;
}

// The kView records of a stream, in their order.
bool StreamViews(const std::vector<uint8_t>& file, std::vector<ss::ViewBody>* out) {
  ss::Reader reader(file.data(), file.size());
  if (!reader.Ok()) return false;
  ss::Kind kind;
  const uint8_t* body = nullptr;
  uint32_t n = 0;
  while (reader.Next(&kind, &body, &n)) {
    ss::ViewBody v;
    if (kind == ss::kView && ss::Body(body, n, &v)) out->push_back(v);
  }
  return !reader.Broken();
}

bool ReadViewRecord(const char* path, std::vector<ss::ViewBody>* out, uint32_t* source) {
  std::vector<uint8_t> bytes;
  if (!ReadFile(path, &bytes) || bytes.size() < sizeof(view_layer_math::RecordHead)) return false;
  view_layer_math::RecordHead head;
  memcpy(&head, bytes.data(), sizeof(head));
  if (head.magic != view_layer_math::kRecordMagic ||
      head.version != view_layer_math::kRecordVersion || head.view_bytes != sizeof(ss::ViewBody)) {
    return false;
  }
  *source = head.source;
  for (size_t at = sizeof(head); at + sizeof(ss::ViewBody) <= bytes.size(); at += sizeof(ss::ViewBody)) {
    ss::ViewBody v;
    memcpy(&v, bytes.data() + at, sizeof(v));
    out->push_back(v);
  }
  return true;
}

int Views(const char* stream_path, const char* record_path) {
  std::vector<uint8_t> file;
  std::vector<ss::ViewBody> views;
  if (!ReadFile(stream_path, &file) || !StreamViews(file, &views)) {
    printf("views        : FAULT %s is not a whole stream\n", stream_path);
    return 2;
  }
  std::vector<ss::ViewBody> record;
  uint32_t source = 0;
  if (record_path != nullptr && !ReadViewRecord(record_path, &record, &source)) {
    printf("views        : FAULT %s is not a record of the views\n", record_path);
    return 2;
  }
  int same = 0, differ = 0, missing = 0, projection = 0;
  for (const ss::ViewBody& v : views) {
    if (view_layer_math::ShowsProjection(v)) ++projection;
    const char* verdict = "";
    if (record_path != nullptr) {
      bool found = false, equal = false;
      for (const ss::ViewBody& r : record) {
        if (r.frame != v.frame) continue;
        found = true;
        if (memcmp(&r, &v, sizeof(v)) == 0) equal = true;
      }
      if (!found) {
        ++missing;
        verdict = ": not in the record";
      } else if (equal) {
        ++same;
        verdict = ": the same bytes as in the record";
      } else {
        ++differ;
        verdict = ": FAULT not the same as in the record";
      }
    }
    printf("views        : %s%s\n", ViewText(v).c_str(), verdict);
  }
  printf("views        : %zu records of the projection layer in %s, %d for a projection layer",
         views.size(), stream_path, projection);
  if (record_path != nullptr) {
    printf(". The record has %zu frames (source %u): %d the same, %d different, %d not in the record",
           record.size(), source, same, differ, missing);
  }
  printf("\n");
  if (views.empty()) return 1;
  return record_path == nullptr || (differ == 0 && missing == 0) ? 0 : 1;
}

int ViewCompare(const char* a_path, const char* b_path) {
  std::vector<ss::ViewBody> a, b;
  uint32_t source_a = 0, source_b = 0;
  if (!ReadViewRecord(a_path, &a, &source_a) || !ReadViewRecord(b_path, &b, &source_b)) {
    printf("view compare : FAULT a file is not a record of the views\n");
    return 2;
  }
  std::map<uint64_t, std::vector<ss::ViewBody>> by_a, by_b;
  for (const ss::ViewBody& v : a) by_a[v.frame].push_back(v);
  for (const ss::ViewBody& v : b) by_b[v.frame].push_back(v);
  int same = 0, differ = 0, only = 0, said = 0;
  for (const auto& f : by_a) {
    const auto other = by_b.find(f.first);
    if (other == by_b.end() || other->second.size() != f.second.size()) {
      ++only;
      continue;
    }
    for (size_t i = 0; i < f.second.size(); ++i) {
      if (view_layer_math::SameLayer(f.second[i], other->second[i])) {
        ++same;
        continue;
      }
      ++differ;
      if (said++ < 5) {
        printf("view compare : FAULT\n  %s\n  %s\n", ViewText(f.second[i]).c_str(),
               ViewText(other->second[i]).c_str());
      }
    }
  }
  for (const auto& f : by_b) {
    if (by_a.find(f.first) == by_a.end()) ++only;
  }
  int projection = 0;
  for (const ss::ViewBody& v : b) {
    if (view_layer_math::ShowsProjection(v)) ++projection;
  }
  printf("view compare : %zu views (source %u) and %zu views (source %u): %d the same, %d "
         "different, %d frames not in both. %d views of the second for a projection layer\n",
         a.size(), source_a, b.size(), source_b, same, differ, only, projection);
  if (!b.empty()) printf("view compare : the first: %s\n", ViewText(b.front()).c_str());
  if (!b.empty()) printf("view compare : the last: %s\n", ViewText(b.back()).c_str());
  return differ == 0 && only == 0 && same > 0 ? 0 : 1;
}

// THE QUAD LAYERS (kCard, kCardPicture, kPanel, kWindow).
namespace ql = quad_layer_math;

// A quad as the OpenXR test runtime of the app writes it in its report
// (android/test_runtime, the lines "frame N quads").
std::string QuadText(const ql::QuadSpec& q) {
  char line[320];
  sprintf_s(line, " quad %.4f %.4f %.4f turn %.4f %.4f %.4f %.4f size %.4f %.4f rect %d %d %dx%d eye %u flags %u",
            q.position[0], q.position[1], q.position[2], q.orientation[0], q.orientation[1],
            q.orientation[2], q.orientation[3], q.size[0], q.size[1], q.rect[0], q.rect[1],
            q.rect[2], q.rect[3], q.eye, q.flags);
  return line;
}

const char* const kLayerKindNames[] = {"projection", "eye", "panel", "card", "wall"};

// A picture of a card as a BMP of 32 bits, the bottom row first, as the test
// runtime writes its pictures.
bool WriteCardBmp(const std::string& path, uint32_t w, uint32_t h, const std::vector<uint8_t>& bgra) {
  std::vector<uint8_t> out(54 + static_cast<size_t>(w) * h * 4, 0);
  auto put32 = [&](size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) out[at + i] = static_cast<uint8_t>(v >> (8 * i));
  };
  out[0] = 'B';
  out[1] = 'M';
  put32(2, static_cast<uint32_t>(out.size()));
  put32(10, 54);
  put32(14, 40);
  put32(18, w);
  put32(22, h);
  out[26] = 1;
  out[28] = 32;
  put32(34, w * h * 4);
  for (uint32_t y = 0; y < h; ++y) {
    memcpy(out.data() + 54 + static_cast<size_t>(y) * w * 4,
           bgra.data() + static_cast<size_t>(h - 1 - y) * w * 4, static_cast<size_t>(w) * 4);
  }
  return WriteFile(path, out);
}

// The layers of each frame of a stream, as the player of the app reads them.
struct FrameOfLayers {
  bool has_view = false;
  ss::ViewBody view = {};
  ql::FrameLayers layers;
  std::vector<uint8_t> normal;
  int pictures = 0;      // kCardPicture records in the frame
  int bad_pictures = 0;  // a kCardPicture whose pixels do not have its hash
  bool card_picture[bwvr::kCards] = {false, false, false, false};
};

bool StreamLayers(const std::vector<uint8_t>& file, std::map<uint64_t, FrameOfLayers>* out,
                  const char* bmp_folder, int* bmps) {
  std::map<uint64_t, std::pair<size_t, size_t>> units;
  if (!Units(file, &units)) return false;
  ql::AppPictures pictures;
  std::map<uint64_t, bool> written;
  // In the order of the stream (Units is a map by frame, and the frames of a
  // stream go up).
  for (const auto& u : units) {
    FrameOfLayers& f = (*out)[u.first];
    const uint8_t* unit = file.data() + u.second.first;
    f.normal = ql::Normal(unit, u.second.second);
    ss::Reader r = ss::Reader::Records(unit, u.second.second);
    ss::Kind kind;
    const uint8_t* body = nullptr;
    uint32_t n = 0;
    while (r.Next(&kind, &body, &n)) {
      if (kind == ss::kStart) pictures = ql::AppPictures();
      if (kind == ss::kView) f.has_view = ss::Body(body, n, &f.view);
      if (kind == ss::kCardPicture) {
        ++f.pictures;
        ss::CardPictureBody p;
        if (!ss::Body(body, n, &p) || !pictures.OnPicture(body, n) ||
            ql::PictureHash(pictures.Pixels(p.card).data(), p.width * 4, p.width, p.height) !=
                p.picture) {
          ++f.bad_pictures;
          continue;
        }
        if (bmp_folder != nullptr && !written[p.picture]) {
          written[p.picture] = true;
          char name[64];
          sprintf_s(name, "\\card-%u-%016llx.bmp", p.card, p.picture);
          if (WriteCardBmp(std::string(bmp_folder) + name, p.width, p.height, pictures.Pixels(p.card))) {
            ++*bmps;
          }
        }
      }
      if (kind == ss::kCard) {
        ss::CardBody c;
        if (ss::Body(body, n, &c) && c.card < static_cast<uint32_t>(bwvr::kCards)) {
          f.layers.card[c.card] = true;
          f.layers.cards[c.card] = c;
          f.card_picture[c.card] = pictures.Has(static_cast<int>(c.card), c.picture);
        }
      }
      if (kind == ss::kPanel) f.layers.panel = ss::Body(body, n, &f.layers.panel_body);
      if (kind == ss::kWindow) f.layers.window = ss::Body(body, n, &f.layers.window_body);
    }
  }
  return true;
}

// A frame of the world: its kView is for a projection layer. A record of the
// layers has no kView, and its frames count as frames of the world.
bool World(const FrameOfLayers& f) {
  return !f.has_view || view_layer_math::ShowsProjection(f.view);
}

// The quads of the layers of a frame, in the order of the app, as the test
// runtime reports them.
std::string FrameQuads(const FrameOfLayers& f, int* count) {
  const bool world = World(f);
  ql::LayerRef order[ql::kMostLayers];
  const int n = ql::Order(f.layers, world, true, f.card_picture, order);
  std::string text;
  *count = 0;
  for (int i = 0; i < n; ++i) {
    ql::QuadSpec q;
    if (!ql::Spec(f.layers, order[i], f.view.width, f.view.height, &q)) continue;
    text += QuadText(q);
    ++*count;
  }
  return text;
}

std::string LayerNames(const FrameOfLayers& f) {
  const bool world = World(f);
  ql::LayerRef order[ql::kMostLayers];
  const int n = ql::Order(f.layers, world, true, f.card_picture, order);
  std::string text;
  for (int i = 0; i < n; ++i) {
    char part[32];
    sprintf_s(part, "%s%s %d", i == 0 ? "" : ", ", kLayerKindNames[order[i].kind], order[i].index);
    text += part;
  }
  return text.empty() ? "none" : text;
}

// The layer records of each frame of a record of the layers (xr_layer_record).
bool RecordLayers(const char* path, std::map<uint64_t, FrameOfLayers>* out) {
  std::vector<uint8_t> file;
  if (!ReadFile(path, &file)) return false;
  int bmps = 0;
  return StreamLayers(file, out, nullptr, &bmps);
}

int Layers(const char* stream_path, const char* record_path, const char* bmp_folder) {
  std::vector<uint8_t> file;
  std::map<uint64_t, FrameOfLayers> frames;
  int bmps = 0;
  if (!ReadFile(stream_path, &file) || !StreamLayers(file, &frames, bmp_folder, &bmps)) {
    printf("layers       : FAULT %s is not a whole stream\n", stream_path);
    return 2;
  }
  std::map<uint64_t, FrameOfLayers> record;
  if (record_path != nullptr && !RecordLayers(record_path, &record)) {
    printf("layers       : FAULT %s is not a record of the layers\n", record_path);
    return 2;
  }
  int with_layers = 0, pictures = 0, bad = 0, same = 0, differ = 0, missing = 0, no_picture = 0;
  for (const auto& e : frames) {
    const FrameOfLayers& f = e.second;
    pictures += f.pictures;
    bad += f.bad_pictures;
    if (!f.layers.Any()) continue;
    ++with_layers;
    for (int c = 0; c < bwvr::kCards; ++c) {
      if (f.layers.card[c] && !f.card_picture[c]) ++no_picture;
    }
    const char* verdict = "";
    if (record_path != nullptr) {
      const auto r = record.find(e.first);
      if (r == record.end()) {
        ++missing;
        verdict = ": not in the record";
      } else if (r->second.normal == f.normal) {
        ++same;
        verdict = ": the same bytes as in the record";
      } else {
        ++differ;
        verdict = ": FAULT not the same as in the record";
      }
    }
    int quads = 0;
    const std::string text = FrameQuads(f, &quads);
    printf("layers       : frame %llu: %s, %d pictures%s\n", e.first, LayerNames(f).c_str(),
           f.pictures, verdict);
    printf("layers       : frame %llu quads%s\n", e.first, text.c_str());
    std::string cards;
    for (int c = 0; c < bwvr::kCards; ++c) {
      if (!f.layers.card[c]) continue;
      char part[48];
      sprintf_s(part, " %d:%016llx", c, f.layers.cards[c].picture);
      cards += part;
    }
    if (!cards.empty()) printf("layers       : frame %llu cards%s\n", e.first, cards.c_str());
  }
  for (int c = 0; c < bwvr::kCards; ++c) {
    int count = 0;
    for (const auto& e : frames) {
      if (e.second.layers.card[c]) ++count;
    }
    if (count > 0) printf("layers       : %s is in %d frames\n", ql::CardName(c), count);
  }
  printf("layers       : %zu frames in %s, %d with layers, %d pictures of a card (%d with pixels "
         "that do not have their hash), %d cards with no picture",
         frames.size(), stream_path, with_layers, pictures, bad, no_picture);
  if (record_path != nullptr) {
    printf(". The record has %zu frames: %d the same, %d different, %d not in the record",
           record.size(), same, differ, missing);
  }
  if (bmp_folder != nullptr) printf(". %d pictures of a card to %s", bmps, bmp_folder);
  printf("\n");
  if (with_layers == 0 || bad > 0 || no_picture > 0) return 1;
  return record_path == nullptr || (differ == 0 && missing == 0) ? 0 : 1;
}

// THE SAME CALLS IN TWO RUNS: the layers of each frame of two records. A
// frame of both is the same (the same normal records), or it differs only in
// the hash of a picture of a card (the picture), or only in a card that one
// run has and the other does not, or in other values.
int LayerCompare(const char* a_path, const char* b_path) {
  std::map<uint64_t, FrameOfLayers> a, b;
  if (!RecordLayers(a_path, &a) || !RecordLayers(b_path, &b)) {
    printf("layer compare: FAULT a file is not a record of the layers\n");
    return 2;
  }
  int same = 0, picture = 0, presence = 0, other = 0, only = 0, said = 0, with_layers = 0;
  uint64_t first_other = 0;
  for (const auto& e : a) {
    const auto o = b.find(e.first);
    if (o == b.end()) {
      ++only;
      continue;
    }
    const FrameOfLayers& x = e.second;
    const FrameOfLayers& y = o->second;
    if (x.layers.Any() || y.layers.Any()) ++with_layers;
    if (x.normal == y.normal) {
      ++same;
      continue;
    }
    bool only_pictures = x.layers.panel == y.layers.panel && x.layers.window == y.layers.window &&
                         (!x.layers.panel || memcmp(&x.layers.panel_body, &y.layers.panel_body,
                                                    sizeof(ss::PanelBody)) == 0) &&
                         (!x.layers.window || memcmp(&x.layers.window_body, &y.layers.window_body,
                                                     sizeof(ss::WindowBody)) == 0);
    bool cards_same_set = true;
    for (int c = 0; c < bwvr::kCards; ++c) {
      if (x.layers.card[c] != y.layers.card[c]) {
        cards_same_set = false;
        continue;
      }
      if (!x.layers.card[c]) continue;
      ss::CardBody p = x.layers.cards[c], q = y.layers.cards[c];
      p.picture = q.picture = 0;
      if (memcmp(&p, &q, sizeof(p)) != 0) only_pictures = false;
    }
    if (only_pictures && cards_same_set) {
      ++picture;
    } else if (only_pictures) {
      ++presence;
    } else {
      ++other;
      if (first_other == 0) first_other = e.first;
      if (said++ < 5) {
        int nx = 0, ny = 0;
        printf("layer compare: frame %llu differs:\n  %s:%s\n  %s:%s\n", e.first,
               LayerNames(x).c_str(), FrameQuads(x, &nx).c_str(), LayerNames(y).c_str(),
               FrameQuads(y, &ny).c_str());
      }
    }
  }
  for (const auto& e : b) {
    if (a.find(e.first) == a.end()) ++only;
  }
  printf("layer compare: %zu and %zu frames, %d with layers: %d frames the same, %d differ only in "
         "the picture of a card, %d differ only in a card that one run has and the other does "
         "not, %d differ in other values (the first in frame %llu), %d frames not in both\n",
         a.size(), b.size(), with_layers, same, picture, presence, other, first_other, only);
  return other == 0 && only == 0 && same > 0 ? 0 : 1;
}

// A TEST FEED OF THE CUT SCENE WINDOW for the app (emu_xr_test.ps1): a copy
// of a stream in which each frame from `first` to `last` is a frame of the window
// as a screen. Its kView gets kViewFlat, and a kWindow with a screen goes
// before its kFrameEnd: 2.6 m wide, 2 m in front, the crop and the edge of
// the arguments. The values of the window are of the test, not of the mod.
int WindowFeed(const char* in_path, const char* out_path, uint64_t first, uint64_t last,
               int32_t crop, int32_t edge) {
  std::vector<uint8_t> file;
  if (!ReadFile(in_path, &file)) {
    printf("window feed  : FAULT cannot read %s\n", in_path);
    return 2;
  }
  ss::Reader r(file.data(), file.size());
  if (!r.Ok()) {
    printf("window feed  : FAULT %s is not a stream\n", in_path);
    return 2;
  }
  ss::Writer w;
  ss::Kind kind;
  const uint8_t* body = nullptr;
  uint32_t n = 0;
  uint64_t frame = 0;
  int windows = 0;
  uint32_t width = 0, height = 0;
  while (r.Next(&kind, &body, &n)) {
    if (kind == ss::kFrame && n >= 8) memcpy(&frame, body, 8);
    const bool in_window = frame >= first && frame <= last;
    if (kind == ss::kView && in_window) {
      ss::ViewBody v;
      if (ss::Body(body, n, &v)) {
        v.flags |= ss::kViewFlat;
        width = v.width;
        height = v.height;
        w.Record(ss::kView, &v, sizeof(v));
        continue;
      }
    }
    if (kind == ss::kFrameEnd && in_window) {
      ss::WindowBody win;
      memset(&win, 0, sizeof(win));
      win.screen = 1;
      win.crop = crop;
      win.edge = edge;
      win.position[1] = 0.1f;
      win.position[2] = -2.0f;
      win.orientation[3] = 1.0f;
      win.width = 2.6f;
      const float shown = static_cast<float>(width) - crop - 2.0f * edge;
      win.height = shown > 0.0f ? 2.6f * height / shown : 1.5f;
      w.Record(ss::kWindow, &win, sizeof(win));
      ++windows;
    }
    w.Record(kind, body, n);
  }
  if (!WriteFile(out_path, w.Bytes())) {
    printf("window feed  : FAULT cannot write %s\n", out_path);
    return 2;
  }
  printf("window feed  : %s: %d frames from frame %llu to frame %llu are frames of the cut "
         "scene window (crop %d, edge %d), %zu bytes\n", out_path, windows, first, last, crop,
         edge, w.Bytes().size());
  return windows > 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 4 && strcmp(argv[1], "read") == 0) {
    const uint32_t attach = argc > 4 ? static_cast<uint32_t>(strtoul(argv[4], nullptr, 10)) : 0;
    const uint32_t last = argc > 5 ? static_cast<uint32_t>(strtoul(argv[5], nullptr, 10)) : 0xFFFFFFFFu;
    const double seconds = argc > 6 ? atof(argv[6]) : 120.0;
    const int slow_ms = argc > 7 ? atoi(argv[7]) : 0;
    const uint32_t lag_until = argc > 8 ? static_cast<uint32_t>(strtoul(argv[8], nullptr, 10)) : 0;
    return Read(argv[2], argv[3], attach, last, seconds, slow_ms, lag_until);
  }
  if (argc == 4 && strcmp(argv[1], "compare") == 0) return Compare(argv[2], argv[3]);
  if ((argc == 3 || argc == 4) && strcmp(argv[1], "views") == 0) {
    return Views(argv[2], argc == 4 ? argv[3] : nullptr);
  }
  if (argc == 4 && strcmp(argv[1], "view-compare") == 0) return ViewCompare(argv[2], argv[3]);
  if (argc >= 3 && argc <= 5 && strcmp(argv[1], "layers") == 0) {
    const char* record = argc >= 4 && strcmp(argv[3], "-") != 0 ? argv[3] : nullptr;
    return Layers(argv[2], record, argc == 5 ? argv[4] : nullptr);
  }
  if (argc == 4 && strcmp(argv[1], "layer-compare") == 0) return LayerCompare(argv[2], argv[3]);
  if (argc == 8 && strcmp(argv[1], "window-feed") == 0) {
    return WindowFeed(argv[2], argv[3], strtoull(argv[4], nullptr, 10),
                      strtoull(argv[5], nullptr, 10), atoi(argv[6]), atoi(argv[7]));
  }
  printf("bwvr_stream_reader read <ring> <out folder> [attach frame] [last frame] [seconds] [slow ms] [lag frame]\n"
         "bwvr_stream_reader compare <ring-capture.bin> <bwvr-stream.bin>\n"
         "bwvr_stream_reader views <stream> [<bwvr-view.rec>]\n"
         "bwvr_stream_reader view-compare <bwvr-view.rec> <bwvr-view.rec>\n"
         "bwvr_stream_reader layers <stream> [<bwvr-layer.rec> or -] [<folder for the card pictures>]\n"
         "bwvr_stream_reader layer-compare <bwvr-layer.rec> <bwvr-layer.rec>\n"
         "bwvr_stream_reader window-feed <stream> <out stream> <first frame> <last frame> <crop> <edge>\n");
  return 1;
}
