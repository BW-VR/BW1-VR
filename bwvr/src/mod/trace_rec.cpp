// THE TIMELINE OF THE GAME (trace_rec.h, trace_events.h, docs/backend.md,
// "The timeline of the game and the app").
//
// THE CLOCK. The events keep ticks of QueryPerformanceCounter. In Wine this
// is CLOCK_BOOTTIME in 100 ns ticks, the clock that the app reads (docs/
// android.md, "The timeline"), so the two parts of a file are on one clock.
// On the PC the clock has its own start, and the file has the game alone.
//
// THE SAVE runs on a thread of its own at the lowest priority. The frame
// thread only sets a flag. The saver copies the last seconds of each ring (the
// frame thread keeps writing), makes the JSON in a buffer of 256 KB and writes
// the file in blocks. With the input channel of the app the game writes its
// part as a fragment (bwvr-trace-game-<a or g><count>.json, one file for each
// save) and tells the app by the head of
// the channel. Without the app it writes a whole file, bwvr-trace-<time>.json,
// in the game folder.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/input_channel.h"
#include "thread_id.h"
#include "trace_rec.h"
#include "turn_parts.h"
#include "xr_input.h"

void ModLog(const char* format, ...);
// vk_stream.cpp: the frame of the stream of this present.
uint64_t VkStreamPresentFrame();
// trace_waits.cpp: the import table of the game.
void TraceWaitsInstall();
// trace_waits.cpp: the calls of the frame thread that the wrappers saw.
uint32_t TraceWaitsCalls();

namespace te = trace_events;
namespace ic = input_channel;

bool g_trace = false;

namespace {

te::Recorder g_rec;
int64_t g_freq = 0;
uint32_t g_frame_tid = 0;
int g_seconds = 10;
int g_auto_ms = 40;
int g_min_us = 30;
int g_wait_us = 100;
int g_ring_k = 128;
bool g_keyboard = true;
int g_test_save_s = 0;
char g_dir[MAX_PATH] = {};

// A request of a save, from any thread. The end of the window is a tick.
volatile LONG g_request = 0;
volatile LONGLONG g_request_end = 0;
char g_request_why[48] = {};

// The bars of the frames (present to present) cross the bars of the frame
// thread, so they go on a row of their own, with a number that no thread has.
constexpr uint32_t kFrameRow = 0x7F000001u;
bool g_scene_frame = false;  // a scene walk ran since the last present
uint32_t g_wait_bars = 0;    // the waits that got a bar

const te::Def kFrameDef = {"frame", "game", {"frame", "ms", nullptr, nullptr}};
const te::Def kFpsDef = {"game frames a second", "game", {"fps", nullptr, nullptr, nullptr}};
const te::Def kLongDef = {"long frame", "game", {"frame", "ms", nullptr, nullptr}};

int IniKey(const char* key, int fallback) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return fallback;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

// A key pressed in another program is not for the game: GetAsyncKeyState reads
// the keyboard of the whole system.
bool GameInFront() {
  const HWND window = GetForegroundWindow();
  if (window == nullptr) return false;
  DWORD process = 0;
  GetWindowThreadProcessId(window, &process);
  return process == GetCurrentProcessId();
}

int Clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------------------------------------------------------------------------
// The file.
struct Out {
  FILE* file = nullptr;
  char* buf = nullptr;
  size_t used = 0;
  size_t total = 0;
  static constexpr size_t kBuf = 256 * 1024;
  bool Open(const char* path) {
    if (fopen_s(&file, path, "wb") != 0) file = nullptr;
    buf = static_cast<char*>(malloc(kBuf));
    return file != nullptr && buf != nullptr;
  }
  void Flush() {
    if (used > 0 && file != nullptr) fwrite(buf, 1, used, file);
    total += used;
    used = 0;
  }
  // Room for `need` bytes at buf + used.
  char* Room(size_t need) {
    if (used + need > kBuf) Flush();
    return buf + used;
  }
  void Added(size_t n) { used += n; }
  void Close() {
    Flush();
    if (file != nullptr) fclose(file);
    free(buf);
    file = nullptr;
    buf = nullptr;
  }
};

// The summary of one save, for the log.
struct Saved {
  uint32_t events = 0;
  uint32_t threads = 0;
  uint32_t overrun_rings = 0;
  size_t bytes = 0;
};

// Writes the part of the game: the name of the process, then the name and the
// events of each thread. Gives false when the file cannot be written.
bool WritePart(Out* out, int64_t from, int64_t to, int64_t base_ns, Saved* saved) {
  char* p = out->Room(512);
  size_t n = te::FormatProcessName(p, 512, 1, "game", false);
  out->Added(n);
  bool comma = true;
  for (int i = 0; i < g_rec.Count(); ++i) {
    const te::Ring* r = g_rec.At(i);
    if (r == nullptr || r->events == nullptr) continue;
    te::Event* copy = static_cast<te::Event*>(malloc(sizeof(te::Event) * r->capacity));
    if (copy == nullptr) continue;
    bool overrun = false;
    const uint32_t count = te::CopyWindow(*r, from, to, copy, r->capacity, &overrun);
    if (count == 0) {
      free(copy);
      continue;
    }
    ++saved->threads;
    if (overrun) ++saved->overrun_rings;
    p = out->Room(768);
    n = te::FormatThreadName(p, 768, 1, r->tid, r->name[0] != 0 ? r->name : "thread", r->sort,
                             comma);
    out->Added(n);
    for (uint32_t k = 0; k < count; ++k) {
      p = out->Room(1024);
      n = te::FormatEvent(p, 1024, copy[k], 1, r->tid, g_freq, base_ns, true);
      out->Added(n);
      if (n > 0) ++saved->events;
    }
    free(copy);
  }
  return out->file != nullptr;
}

// Parts that an earlier run wrote and no app joined.
void RemoveOldParts() {
  char pattern[MAX_PATH];
  _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%sbwvr-trace-game-*", g_dir);
  WIN32_FIND_DATAA found;
  const HANDLE find = FindFirstFileA(pattern, &found);
  if (find == INVALID_HANDLE_VALUE) return;
  do {
    char file[MAX_PATH];
    _snprintf_s(file, sizeof(file), _TRUNCATE, "%s%s", g_dir, found.cFileName);
    DeleteFileA(file);
  } while (FindNextFileA(find, &found));
  FindClose(find);
}

void TimeText(char* out, size_t size) {
  SYSTEMTIME t;
  GetLocalTime(&t);
  _snprintf_s(out, size, _TRUNCATE, "%04d%02d%02d-%02d%02d%02d", t.wYear, t.wMonth, t.wDay,
              t.wHour, t.wMinute, t.wSecond);
}

// One save. ask: the count of the request of the app that it answers (0: the
// request is the game's own).
void DoSave(int64_t end_ticks, const char* why, uint32_t ask) {
  const double start_ms = static_cast<double>(TraceNow()) * 1000.0 / static_cast<double>(g_freq);
  ic::File* channel = XrInputChannel();
  const int64_t from = end_ticks - static_cast<int64_t>(g_seconds) * g_freq;
  const int64_t base_ns = te::TicksToNs(from, g_freq);
  char path[MAX_PATH], tmp[MAX_PATH], stamp[32];
  TimeText(stamp, sizeof(stamp));
  const bool part = channel != nullptr;
  // The number of the part: the count of the request of the app, or the count
  // of the saves of the game after this one.
  const uint32_t number =
      ask != 0 ? ask : (channel != nullptr ? ic::LoadAcquire(&channel->head.trace_game_ask) + 1 : 0);
  if (part) {
    char name[64];
    te::FormatPartName(name, sizeof(name), ask != 0, number, false);
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s%s", g_dir, name);
    te::FormatPartName(name, sizeof(name), ask != 0, number, true);
    _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, "%s%s", g_dir, name);
  } else {
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%sbwvr-trace-%s.json", g_dir, stamp);
    _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, "%s", path);
  }
  Out out;
  if (!out.Open(tmp)) {
    ModLog("trace        : cannot write %s (error %lu)", tmp, GetLastError());
    out.Close();
    return;
  }
  if (!part) {
    char* p = out.Room(512);
    char note[160];
    _snprintf_s(note, sizeof(note), _TRUNCATE,
                "Black and White VR Mod, the game alone (%s), the last %d s", why, g_seconds);
    out.Added(te::FormatHeader(p, 512, base_ns, static_cast<double>(g_seconds), note));
  }
  Saved saved;
  WritePart(&out, from, end_ticks, base_ns, &saved);
  if (!part) {
    char* p = out.Room(16);
    out.Added(te::FormatFooter(p, 16));
  }
  out.Close();
  saved.bytes = out.total;
  if (part) {
    DeleteFileA(path);
    MoveFileA(tmp, path);
    // The head of the channel tells the app. For a request of the app the
    // answer is its count. For a request of the game the end of the window
    // goes first and its count after it.
    if (ask != 0) {
      ic::StoreRelease(&channel->head.trace_done, ask);
    } else {
      uint32_t lo, hi;
      te::SplitNs(te::TicksToNs(end_ticks, g_freq), &lo, &hi);
      ic::StoreRelease(&channel->head.trace_game_lo, lo);
      ic::StoreRelease(&channel->head.trace_game_hi, hi);
      ic::StoreRelease(&channel->head.trace_game_ask, number);
    }
  }
  const double spent =
      static_cast<double>(TraceNow()) * 1000.0 / static_cast<double>(g_freq) - start_ms;

  ModLog("trace        : saved (%s) the last %d s: %u events of %u threads, %zu bytes, %s%s, "
         "in %.1f ms on the saver thread. %u rings had run over%s. Since the start the "
         "frame thread made %u calls through the import slots (Sleep, wait, lock, file), "
         "%u waits got a bar, and the timers of the game turn and of the graphics engine "
         "read the clock for %u calls%s",
         why, g_seconds, saved.events, saved.threads, saved.bytes, part ? "the game part " : "",
         path, spent, saved.overrun_rings,
         g_rec.rejected() != 0 ? ". Some threads had no ring (too many)" : "",
         TraceWaitsCalls(), g_wait_bars, TurnPartsTimedCalls(), part ? " (the app joins the part)" : "");
}

// ---------------------------------------------------------------------------
// The saver thread.
DWORD WINAPI SaverMain(LPVOID) {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
  uint32_t ask_seen = 0;
  bool ask_known = false;
  bool key_down = false;
  bool test_done = false;
  const int64_t started = TraceNow();
  for (;;) {
    Sleep(25);
    int64_t end = 0;
    char why[48] = {};
    uint32_t answer = 0;
    // The request of the app: the count in the head, and the end of the window.
    // A count in the head at the first sight is from an earlier session of the
    // app and is no request.
    ic::File* channel = XrInputChannel();
    if (channel != nullptr) {
      if (channel->head.trace_on == 0) ic::StoreRelease(&channel->head.trace_on, 1);
      const uint32_t ask = ic::LoadAcquire(&channel->head.trace_ask);
      if (!ask_known) {
        ask_known = true;
        ask_seen = ask;
      } else if (ask != ask_seen) {
        ask_seen = ask;
        const int64_t ns = te::JoinNs(ic::LoadAcquire(&channel->head.trace_ask_lo),
                                      ic::LoadAcquire(&channel->head.trace_ask_hi));
        end = te::NsToTicks(ns, g_freq);
        const int64_t now = TraceNow();
        if (end <= 0 || end > now) end = now;
        answer = ask;
        _snprintf_s(why, sizeof(why), _TRUNCATE, "the app");
      }
    }
    if (end == 0 && InterlockedCompareExchange(&g_request, 0, 1) == 1) {
      end = g_request_end;
      lstrcpynA(why, g_request_why, sizeof(why));
    }
    if (end == 0 && g_keyboard) {
      const bool down = GameInFront() && (GetAsyncKeyState('G') & 0x8000) != 0 &&
                        (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                        (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
      if (down && !key_down) {
        end = TraceNow();
        _snprintf_s(why, sizeof(why), _TRUNCATE, "the key Ctrl Shift G");
      }
      key_down = down;
    }
    if (end == 0 && g_test_save_s > 0 && !test_done &&
        TraceNow() - started >= static_cast<int64_t>(g_test_save_s) * g_freq) {
      test_done = true;
      end = TraceNow();
      _snprintf_s(why, sizeof(why), _TRUNCATE, "trace_test_save_s");
    }
    if (end != 0) DoSave(end, why, answer);
  }
}

}  // namespace

int64_t TraceNow() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

void TraceInstall() {
  if (IniKey("trace", 0) == 0) return;
  LARGE_INTEGER hz;
  QueryPerformanceFrequency(&hz);
  g_freq = hz.QuadPart;
  if (g_freq <= 0) {
    ModLog("trace        : trace=1, but the clock has no frequency. No recorder");
    return;
  }
  g_seconds = Clamp(IniKey("trace_seconds", 10), 1, 60);
  g_auto_ms = Clamp(IniKey("trace_auto_ms", 40), 0, 100000);
  g_min_us = Clamp(IniKey("trace_min_us", 30), 0, 1000000);
  g_wait_us = Clamp(IniKey("trace_wait_us", 100), 0, 1000000);
  g_ring_k = Clamp(IniKey("trace_ring_k", 128), 4, 4096);
  g_keyboard = IniKey("trace_keyboard", 1) != 0;
  g_test_save_s = Clamp(IniKey("trace_test_save_s", 0), 0, 3600);
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr) {
    ModLog("trace        : trace=1, but the game folder is not known. No recorder");
    return;
  }
  slash[1] = 0;
  lstrcpynA(g_dir, path, MAX_PATH);
  RemoveOldParts();
  g_rec.Init(g_freq, static_cast<int64_t>(g_min_us) * g_freq / 1000000, 16384);
  g_frame_tid = ThisThreadId();
  g_rec.Name(g_frame_tid, "frame thread", 1, static_cast<uint32_t>(g_ring_k) * 1024u);
  g_rec.Name(kFrameRow, "frames (present to present)", 0, 32768);
  CreateThread(nullptr, 0, &SaverMain, nullptr, 0, nullptr);
  TraceWaitsInstall();
  g_trace = true;
  ModLog("trace        : trace=1. Each thread records bars, points and counters into a ring "
         "(%d K events for the frame thread, 16 K for the others, %zu bytes an event). A save "
         "writes the last %d s as a file of the Trace Event Format (ui.perfetto.dev). Bars "
         "under %d us are not kept, waits under %d us are not kept. A save comes from the app "
         "(the button combination), after a frame longer than %d ms (at most one a minute)%s%s",
         g_ring_k, sizeof(te::Event), g_seconds, g_min_us, g_wait_us, g_auto_ms,
         g_keyboard ? ", and from the key Ctrl Shift G" : "",
         g_test_save_s > 0 ? ", and once by trace_test_save_s" : "");
}

void TraceNameThread(const char* name, int sort, unsigned events) {
  if (!g_trace) return;
  g_rec.Name(ThisThreadId(), name, sort, events);
}

void TraceBar(const te::Def* def, int64_t begin, int64_t end, float a, float b, float c, float d) {
  g_rec.Bar(ThisThreadId(), def, begin, end, a, b, c, d);
}

void TracePoint(const te::Def* def, float a, float b, float c, float d) {
  g_rec.Point(ThisThreadId(), def, TraceNow(), a, b, c, d);
}

void TraceCounter(const te::Def* def, float a, float b, float c, float d) {
  g_rec.Counter(ThisThreadId(), def, TraceNow(), a, b, c, d);
}

void TraceWait(const te::Def* def, int64_t begin, int64_t end, float a) {
  if (end - begin < static_cast<int64_t>(g_wait_us) * g_freq / 1000000) return;
  ++g_wait_bars;
  g_rec.Bar(ThisThreadId(), def, begin, end, a);
}

void TraceSceneFrame() { g_scene_frame = true; }

void TraceSave(const char* why) {
  if (!g_trace) return;
  g_request_end = TraceNow();
  lstrcpynA(g_request_why, why != nullptr ? why : "", sizeof(g_request_why));
  InterlockedExchange(&g_request, 1);
}

void TraceFramePresent() {
  if (!g_trace) return;
  static int64_t last = 0;
  static uint64_t presents = 0;
  static int64_t fps_at = 0;
  static uint32_t fps_count = 0;
  static int64_t last_save_ns = 0;
  const int64_t now = TraceNow();
  ++presents;
  if (last != 0) {
    const double ms = static_cast<double>(now - last) * 1000.0 / static_cast<double>(g_freq);
    const float frame = static_cast<float>(VkStreamPresentFrame());
    g_rec.Bar(kFrameRow, &kFrameDef, last, now, frame, static_cast<float>(ms));
    if (ms > 20.0) g_rec.Point(kFrameRow, &kLongDef, now, frame, static_cast<float>(ms));
    // A load makes frames of seconds, and a frame with no scene is no frame of
    // the play: only a frame of the scene under 400 ms starts a save.
    const int64_t now_ns = te::TicksToNs(now, g_freq);
    if (g_scene_frame && presents > 200 && ms < 400.0 &&
        te::AutoShouldSave(ms, g_auto_ms, now_ns, last_save_ns)) {
      last_save_ns = now_ns;
      TraceSave("a long frame");
    }
  }
  last = now;
  g_scene_frame = false;
  ++fps_count;
  if (fps_at == 0) fps_at = now;
  if (now - fps_at >= g_freq / 4) {
    const double s = static_cast<double>(now - fps_at) / static_cast<double>(g_freq);
    g_rec.Counter(kFrameRow, &kFpsDef, now, static_cast<float>(fps_count / s));
    fps_at = now;
    fps_count = 0;
  }
}
