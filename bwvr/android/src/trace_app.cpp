// THE TIMELINE OF THE HEADSET APP (trace_app.h, src/common/trace_events.h).
//
// The join: the worker thread writes the part of the app into the file
// trace-<time>.json, then the part of the game (bwvr-trace-game-<kind><count>.json,
// which the mod wrote for this save), then the end of the file. Both parts have their events in
// microseconds from the same start (the end of the window less trace_seconds),
// on the clock CLOCK_BOOTTIME. Perfetto shows two processes, "game" and "app".

#include "trace_app.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <string>

#include "app_log.h"

namespace te = trace_events;
namespace ic = input_channel;

bool g_app_trace = false;

namespace {

te::Recorder g_rec;
std::string g_files;
std::string g_game;
int g_seconds = 10;
int g_combo_ms = 300;
te::ComboLatch g_latch;
bool g_game_ask_init = false;
uint32_t g_game_ask_seen = 0;
std::atomic<bool> g_pulse{false};
std::atomic<ic::File*> g_channel{nullptr};

struct Job {
  bool from_app = false;  // the combination: the game must answer with trace_done == ask
  uint32_t ask = 0;       // the count of the request (from_app)
  uint32_t game_count = 0;  // the count of the saves of the game (not from_app)
  int64_t end_ns = 0;
};
constexpr int kJobs = 8;
Job g_jobs[kJobs];
int g_job_count = 0;
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
pthread_t g_thread;
bool g_thread_on = false;
bool g_stop = false;

uint32_t ThisThread() {
  static thread_local uint32_t id = 0;
  if (id == 0) id = static_cast<uint32_t>(syscall(SYS_gettid));
  return id;
}

struct Out {
  FILE* file = nullptr;
  char* buf = nullptr;
  size_t used = 0;
  size_t total = 0;
  static constexpr size_t kBuf = 256 * 1024;
  bool Open(const char* path) {
    file = fopen(path, "wb");
    buf = static_cast<char*>(malloc(kBuf));
    return file != nullptr && buf != nullptr;
  }
  void Flush() {
    if (used > 0 && file != nullptr) fwrite(buf, 1, used, file);
    total += used;
    used = 0;
  }
  char* Room(size_t need) {
    if (used + need > kBuf) Flush();
    return buf + used;
  }
  void Added(size_t n) { used += n; }
  void Text(const char* s, size_t n) {
    if (n > kBuf) {
      Flush();
      if (file != nullptr) fwrite(s, 1, n, file);
      total += n;
      return;
    }
    char* p = Room(n);
    memcpy(p, s, n);
    Added(n);
  }
  void Close() {
    Flush();
    if (file != nullptr) fclose(file);
    free(buf);
    file = nullptr;
    buf = nullptr;
  }
};

void StampText(char* out, size_t size) {
  const time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  snprintf(out, size, "%04d%02d%02d-%02d%02d%02d", local.tm_year + 1900, local.tm_mon + 1,
           local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
}

// The part of the app: the name of the process, then each thread.
void WritePart(Out* out, int64_t from_ns, int64_t to_ns, uint32_t* events, uint32_t* threads,
               uint32_t* overrun_rings) {
  char* p = out->Room(512);
  out->Added(te::FormatProcessName(p, 512, 2, "app", false));
  for (int i = 0; i < g_rec.Count(); ++i) {
    const te::Ring* r = g_rec.At(i);
    if (r == nullptr || r->events == nullptr) continue;
    te::Event* copy = static_cast<te::Event*>(malloc(sizeof(te::Event) * r->capacity));
    if (copy == nullptr) continue;
    bool overrun = false;
    const uint32_t count = te::CopyWindow(*r, from_ns, to_ns, copy, r->capacity, &overrun);
    if (count == 0) {
      free(copy);
      continue;
    }
    ++*threads;
    if (overrun) ++*overrun_rings;
    p = out->Room(768);
    out->Added(te::FormatThreadName(p, 768, 2, r->tid, r->name[0] != 0 ? r->name : "thread",
                                    r->sort, true));
    for (uint32_t k = 0; k < count; ++k) {
      p = out->Room(1024);
      const size_t n = te::FormatEvent(p, 1024, copy[k], 2, r->tid, 1000000000LL, from_ns, true);
      out->Added(n);
      if (n > 0) ++*events;
    }
    free(copy);
  }
}

void DoJob(const Job& job) {
  const int64_t t0 = AppTraceNow();
  ic::File* channel = g_channel.load();
  const int64_t from_ns = job.end_ns - static_cast<int64_t>(g_seconds) * 1000000000LL;
  char stamp[32];
  StampText(stamp, sizeof(stamp));
  const std::string path = g_files + "/trace-" + stamp + ".json";
  const std::string tmp = path + ".tmp";
  Out out;
  if (!out.Open(tmp.c_str())) {
    AppLog("trace        : cannot write %s (error %d)", tmp.c_str(), errno);
    out.Close();
    return;
  }
  char* p = out.Room(512);
  out.Added(te::FormatHeader(p, 512, from_ns, static_cast<double>(g_seconds),
                             job.from_app ? "Black and White VR Mod, saved by the controllers"
                                          : "Black and White VR Mod, saved by the game"));
  uint32_t events = 0, threads = 0, overrun = 0;
  WritePart(&out, from_ns, job.end_ns, &events, &threads, &overrun);

  // The part of the game. For a save of the app the mod answers in the head of
  // the channel. For a save of the game it wrote its part before it counted.
  bool game_ok = true;
  const char* game_note = "";
  if (job.from_app && channel != nullptr) {
    game_ok = false;
    for (int waited = 0; waited < 8000; waited += 20) {
      // The game answers the newest request, so a done count above ours means
      // that it skipped our request and has no part for it.
      if (static_cast<int32_t>(ic::LoadAcquire(&channel->head.trace_done) - job.ask) >= 0) {
        game_ok = true;
        break;
      }
      usleep(20000);
    }
    if (!game_ok) game_note = " (the game did not answer in 8 s, so the file has no part of the game)";
  } else if (channel == nullptr) {
    game_ok = false;
    game_note = " (no input channel, so the file has no part of the game)";
  }
  size_t game_bytes = 0;
  if (game_ok) {
    // The part of this job by name: another save close to this one has its own
    // file. The app deletes the file after the join.
    char part_name[64];
    te::FormatPartName(part_name, sizeof(part_name), job.from_app,
                       job.from_app ? job.ask : job.game_count, false);
    const std::string game_path = g_game + "/" + part_name;
    FILE* gf = fopen(game_path.c_str(), "rb");
    if (gf != nullptr) {
      out.Text(",\n", 2);
      char chunk[64 * 1024];
      size_t n;
      while ((n = fread(chunk, 1, sizeof(chunk), gf)) > 0) {
        out.Text(chunk, n);
        game_bytes += n;
      }
      fclose(gf);
      remove(game_path.c_str());
    } else {
      game_note = " (the game part is not there)";
    }
  }
  p = out.Room(16);
  out.Added(te::FormatFooter(p, 16));
  out.Close();
  rename(tmp.c_str(), path.c_str());
  AppLog("trace        : saved %s (%s): the last %d s, the app part has %u events of %u threads "
         "(%u rings had run over), the game part has %zu bytes%s, the file has %zu bytes, "
         "written in %.1f ms on a thread of low priority",
         path.c_str(), job.from_app ? "the controllers" : "the game", g_seconds, events, threads,
         overrun, game_bytes, game_note, out.total,
         static_cast<double>(AppTraceNow() - t0) / 1e6);
  g_pulse.store(true);
}

void* Worker(void*) {
  setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 10);
  pthread_mutex_lock(&g_lock);
  for (;;) {
    while (g_job_count == 0 && !g_stop) pthread_cond_wait(&g_cond, &g_lock);
    if (g_stop && g_job_count == 0) break;
    const Job job = g_jobs[0];
    for (int i = 1; i < g_job_count; ++i) g_jobs[i - 1] = g_jobs[i];
    --g_job_count;
    pthread_mutex_unlock(&g_lock);
    DoJob(job);
    pthread_mutex_lock(&g_lock);
  }
  pthread_mutex_unlock(&g_lock);
  return nullptr;
}

void Enqueue(const Job& job) {
  pthread_mutex_lock(&g_lock);
  if (g_job_count < kJobs) g_jobs[g_job_count++] = job;
  pthread_cond_signal(&g_cond);
  pthread_mutex_unlock(&g_lock);
}

}  // namespace

int64_t AppTraceNow() {
  timespec t;
  clock_gettime(CLOCK_BOOTTIME, &t);
  return static_cast<int64_t>(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

void AppTraceStart(bool on, int seconds, int min_us, int combo_ms, const char* files,
                   const char* game) {
  if (!on) return;
  g_seconds = seconds < 1 ? 1 : (seconds > 60 ? 60 : seconds);
  g_combo_ms = combo_ms < 50 ? 50 : combo_ms;
  g_files = files != nullptr ? files : "";
  g_game = game != nullptr ? game : "";
  g_rec.Init(1000000000LL, static_cast<int64_t>(min_us) * 1000LL, 16384);
  g_app_trace = true;
  AppTraceNameThread("session thread", 0, 65536);
  g_thread_on = pthread_create(&g_thread, nullptr, &Worker, nullptr) == 0;
  AppLog("trace        : trace=1. Each thread records bars, points and counters into a ring "
         "(%zu bytes an event) on the clock CLOCK_BOOTTIME. Hold the X button and the stick "
         "click of the left controller together for %d ms to save the last %d s into %s as "
         "trace-<time>.json (the part of the game joins it). Bars under %d us are not kept",
         sizeof(te::Event), g_combo_ms, g_seconds, g_files.c_str(), min_us);
}

void AppTraceNameThread(const char* name, int sort, unsigned events) {
  if (!g_app_trace) return;
  g_rec.Name(ThisThread(), name, sort, events);
}

void AppTraceBar(const te::Def* def, int64_t begin, int64_t end, float a, float b, float c,
                 float d) {
  g_rec.Bar(ThisThread(), def, begin, end, a, b, c, d);
}

void AppTracePoint(const te::Def* def, float a, float b, float c, float d) {
  g_rec.Point(ThisThread(), def, AppTraceNow(), a, b, c, d);
}

void AppTraceCounter(const te::Def* def, float a, float b, float c, float d) {
  g_rec.Counter(ThisThread(), def, AppTraceNow(), a, b, c, d);
}

bool AppTraceSession(const ic::SlotData& slot, ic::File* channel) {
  if (!g_app_trace) return false;
  g_channel.store(channel);
  const int64_t now = AppTraceNow();
  const uint32_t need = ic::kPrimary | ic::kStickClick;
  const bool held = (slot.hands[0].flags & ic::kActive) != 0 &&
                    (slot.hands[0].flags & need) == need;
  if (g_latch.Step(held, now, static_cast<int64_t>(g_combo_ms) * 1000000LL)) {
    Job job;
    job.from_app = true;
    job.end_ns = now;
    if (channel != nullptr) {
      uint32_t lo, hi;
      te::SplitNs(now, &lo, &hi);
      ic::StoreRelease(&channel->head.trace_ask_lo, lo);
      ic::StoreRelease(&channel->head.trace_ask_hi, hi);
      job.ask = channel->head.trace_ask + 1;
      ic::StoreRelease(&channel->head.trace_ask, job.ask);
    }
    AppLog("trace        : the combination of the left controller. Save of the last %d s", g_seconds);
    Enqueue(job);
  }
  if (channel != nullptr) {
    const uint32_t g = ic::LoadAcquire(&channel->head.trace_game_ask);
    if (!g_game_ask_init) {
      // A count from an earlier run of the game is no save for this run.
      g_game_ask_init = true;
      g_game_ask_seen = g;
    } else if (g != g_game_ask_seen) {
      g_game_ask_seen = g;
      Job job;
      job.from_app = false;
      job.game_count = g;
      job.end_ns = te::JoinNs(ic::LoadAcquire(&channel->head.trace_game_lo),
                              ic::LoadAcquire(&channel->head.trace_game_hi));
      if (job.end_ns <= 0 || job.end_ns > now) job.end_ns = now;
      AppLog("trace        : the game saved its part (a long frame or the key). The app joins it");
      Enqueue(job);
    }
  }
  return g_pulse.exchange(false);
}

void AppTraceStop() {
  if (!g_thread_on) return;
  pthread_mutex_lock(&g_lock);
  g_stop = true;
  pthread_cond_signal(&g_cond);
  pthread_mutex_unlock(&g_lock);
  pthread_join(g_thread, nullptr);
  g_thread_on = false;
}
