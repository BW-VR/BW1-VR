// THE TEST READER OF THE LIVE STREAM OF THE WALKS (docs/android.md, "The
// live stream"). A Windows program for a desktop run, with the ring code of
// the mod and the app (src/mod/stream_ring.h).
//
//   bwvr_stream_reader read <ring> <out folder> [attach frame] [last frame] [seconds]
//     Waits for a new head in the ring file, then for the frame <attach> of
//     the game, and asks for the whole state as a reader that comes late.
//     Takes each frame to the frame <last>, then leaves. Writes
//     ring-capture.bin (a stream: the head of a stream and the frames that it
//     got) and ring-frames.txt (each frame: its bytes, its records of a
//     surface, the wait of the writer).
//
//   bwvr_stream_reader compare <ring-capture.bin> <bwvr-stream.bin>
//     Compares the bytes of each frame of the record with the frame of the
//     same number in the capture. Exit code 0 when each one is the same.
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

#include "../../src/mod/stream_ring.h"

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
};

int Read(const char* ring, const char* out, uint32_t attach, uint32_t last, double seconds) {
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
  std::vector<uint8_t> chunk(4u << 20);
  std::vector<Got> got;
  std::map<uint32_t, sr::FrameStat> stats;
  bool broken = false;
  bool done = false;
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
      if (done && NowMs() - done_at > 200.0) break;
      Sleep(1);
      continue;
    }
    pending.insert(pending.end(), chunk.begin(), chunk.begin() + n);
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
      if (!done) {
        Got g = {};
        g.frame = frame;
        g.at = capture.size() - 8;
        g.bytes = len;
        g.whole = whole;
        g.ms = NowMs() - start;
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
         "dropped, a wait for room of %u ms in all, %u us at most for one frame\n",
         sr::LoadAcquire(&h->units), sr::LoadAcquire(&h->dropped),
         sr::LoadAcquire(&h->wait_ms), sr::LoadAcquire(&h->wait_us_max));
  m.Close();

  const std::string folder(out);
  const bool wrote = WriteFile(folder + "\\ring-capture.bin", capture);
  std::string text;
  char line[256];
  sprintf_s(line, "frame\tbytes\tkStart\tsurfaces\tpixel bytes\twriter wait us\t"
                  "writer put us\treader ms\n");
  text += line;
  for (const Got& g : got) {
    const auto s = stats.find(static_cast<uint32_t>(g.frame));
    if (s != stats.end()) {
      sprintf_s(line, "%llu\t%zu\t%d\t%d\t%llu\t%u\t%u\t%.1f\n", g.frame, g.bytes,
                g.whole ? 1 : 0, g.surfaces, g.pixels, s->second.wait_us,
                s->second.put_us, g.ms);
    } else {
      sprintf_s(line, "%llu\t%zu\t%d\t%d\t%llu\t-\t-\t%.1f\n", g.frame, g.bytes,
                g.whole ? 1 : 0, g.surfaces, g.pixels, g.ms);
    }
    text += line;
  }
  WriteFile(folder + "\\ring-frames.txt", std::vector<uint8_t>(text.begin(), text.end()));
  printf("reader       : %zu frames (%llu to %llu), %zu bytes, to %s\\ring-capture.bin%s\n",
         got.size(), got.empty() ? 0ull : got.front().frame,
         got.empty() ? 0ull : got.back().frame, capture.size() - 8, out,
         wrote ? "" : " (FAULT: cannot write it)");
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
  int same = 0, differ = 0;
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
    } else {
      printf("compare      : frame %llu: FAULT %zu bytes in the record, %zu in the "
             "ring, the first different byte at %zu\n",
             r.first, r.second.second, c->second.second, first);
      ++differ;
    }
  }
  printf("compare      : %d frames the same, %d different\n", same, differ);
  return differ == 0 && same > 0 && ok_a && ok_b ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 4 && strcmp(argv[1], "read") == 0) {
    const uint32_t attach = argc > 4 ? static_cast<uint32_t>(strtoul(argv[4], nullptr, 10)) : 0;
    const uint32_t last = argc > 5 ? static_cast<uint32_t>(strtoul(argv[5], nullptr, 10)) : 0xFFFFFFFFu;
    const double seconds = argc > 6 ? atof(argv[6]) : 120.0;
    return Read(argv[2], argv[3], attach, last, seconds);
  }
  if (argc == 4 && strcmp(argv[1], "compare") == 0) return Compare(argv[2], argv[3]);
  printf("bwvr_stream_reader read <ring> <out folder> [attach frame] [last frame] [seconds]\n"
         "bwvr_stream_reader compare <ring-capture.bin> <bwvr-stream.bin>\n");
  return 1;
}
