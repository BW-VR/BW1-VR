// THE TIMELINE OF THE HEADSET APP (trace_app.h, src/common/trace_events.h).
//
// The join: the worker thread writes the part of the app into the file
// trace-<time>.json, then the part of the game (the file that the mod wrote for
// this save, bwvr-trace-game-<kind><count>.json), then the end of the file.
// Both parts have their events in microseconds from the same start (the end of
// the window less trace_seconds). Perfetto shows two processes, "game" and
// "app". The clock of the game can have another start than the clock of the
// app, so the join finds the shift from the frames that both processes know
// (the end of the write in the game, the arrival in the reader of the app,
// trace_join.h), adds it to each time of the game part and logs it
// (trace_shift=1).

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
#include <vector>

#include "app_log.h"
#include "../../src/common/trace_combo.h"
#include "../../src/common/trace_join.h"

namespace te = trace_events;
namespace tc = trace_combo;
namespace tj = trace_join;
namespace ic = input_channel;

bool g_app_trace = false;

namespace {

te::Recorder g_rec;
std::string g_files;
std::string g_game;
int g_seconds = 10;
int g_combo_ms = 300;
bool g_combo2 = true;
bool g_shift_join = true;
te::ComboLatch g_latch;
// The change of the buttons that the combinations read goes to the log, at most
// kButtonLines times.
constexpr int kButtonLines = 60;
int g_button_lines = 0;
uint32_t g_button_bits = 0xFFFFFFFFu;
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
               uint32_t* overrun_rings, std::vector<tj::Sample>* arrivals) {
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
      if (copy[k].kind == te::kPoint && copy[k].def != nullptr && copy[k].def->name != nullptr &&
          strcmp(copy[k].def->name, te::kStreamArriveName) == 0) {
        tj::Sample s;
        s.frame = te::FrameFromArg(copy[k].v[0]);
        s.us = te::UsFrom(copy[k].ts, 1000000000LL, from_ns);
        arrivals->push_back(s);
      }
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
  std::vector<tj::Sample> arrivals;
  WritePart(&out, from_ns, job.end_ns, &events, &threads, &overrun, &arrivals);

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
  std::string join_note;
  if (game_ok) {
    // The part of this job by name: another save close to this one has its own
    // file. The app deletes the file after the join.
    char part_name[64];
    te::FormatPartName(part_name, sizeof(part_name), job.from_app,
                       job.from_app ? job.ask : job.game_count, false);
    const std::string game_path = g_game + "/" + part_name;
    FILE* gf = fopen(game_path.c_str(), "rb");
    if (gf != nullptr) {
      std::string text;
      char chunk[64 * 1024];
      size_t n;
      while ((n = fread(chunk, 1, sizeof(chunk), gf)) > 0) text.append(chunk, n);
      fclose(gf);
      remove(game_path.c_str());
      game_bytes = text.size();
      // The shift of the clock of the game from the frames that both know.
      std::vector<tj::Sample> puts;
      tj::ScanPuts(text.c_str(), text.size(), &puts);
      const tj::Shift shift = tj::EstimateShift(puts, arrivals);
      const bool applied = g_shift_join && shift.apply;
      char note[640];
      if (shift.pairs == 0) {
        snprintf(note, sizeof(note),
                 "clock of the game: no frame is in both parts (%zu writes, %zu arrivals), "
                 "no shift",
                 puts.size(), arrivals.size());
      } else {
        snprintf(note, sizeof(note),
                 "clock of the game: %d frames in both parts, arrival less end of the write "
                 "least %.2f, 10th %.2f, median %.2f, 90th %.2f, most %.2f ms (spread %.2f ms). "
                 "Shift of the game %+.3f ms (median less %.1f ms), %s",
                 shift.pairs, shift.least_us / 1000.0, shift.p10_us / 1000.0,
                 shift.median_us / 1000.0, shift.p90_us / 1000.0, shift.most_us / 1000.0,
                 shift.spread_us / 1000.0, shift.shift_us / 1000.0, tj::kArrivalUs / 1000.0,
                 applied ? "applied"
                         : (!g_shift_join ? "not applied (trace_shift=0)"
                                          : (shift.pairs < tj::kMinPairs
                                                 ? "not applied (too few frames)"
                                                 : "not applied (the spread is too wide)")));
      }
      join_note = note;
      out.Text(",\n", 2);
      if (applied) {
        std::string shifted;
        tj::ShiftPart(text.c_str(), text.size(), shift.shift_us, &shifted);
        out.Text(shifted.data(), shifted.size());
      } else {
        out.Text(text.data(), text.size());
      }
      // A marker at the start of the file, with the numbers of the join.
      char marker[512];
      const int mw = snprintf(
          marker, sizeof(marker),
          ",\n{\"name\":\"%s\",\"cat\":\"join\",\"ph\":\"i\",\"s\":\"g\",\"ts\":0.000,"
          "\"pid\":2,\"tid\":0,\"args\":{\"pairs\":%d,\"shift_ms\":%.3f,\"applied\":%d,"
          "\"median_ms\":%.3f,\"spread_ms\":%.3f}}",
          te::kJoinName, shift.pairs, applied ? shift.shift_us / 1000.0 : 0.0, applied ? 1 : 0,
          shift.median_us / 1000.0, shift.spread_us / 1000.0);
      if (mw > 0 && static_cast<size_t>(mw) < sizeof(marker)) {
        out.Text(marker, static_cast<size_t>(mw));
      }
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
  if (!join_note.empty()) AppLog("trace        : %s", join_note.c_str());
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

void AppTraceStart(bool on, int seconds, int min_us, int combo_ms, bool combo2, bool shift_join,
                   const char* files, const char* game) {
  if (!on) return;
  g_seconds = seconds < 1 ? 1 : (seconds > 60 ? 60 : seconds);
  g_combo_ms = combo_ms < 50 ? 50 : combo_ms;
  g_combo2 = combo2;
  g_shift_join = shift_join;
  g_files = files != nullptr ? files : "";
  g_game = game != nullptr ? game : "";
  g_rec.Init(1000000000LL, static_cast<int64_t>(min_us) * 1000LL, 16384);
  g_app_trace = true;
  AppTraceNameThread("session thread", 0, 65536);
  g_thread_on = pthread_create(&g_thread, nullptr, &Worker, nullptr) == 0;
  AppLog("trace        : trace=1. Each thread records bars, points and counters into a ring "
         "(%zu bytes an event) on the clock CLOCK_BOOTTIME. Bars under %d us are not kept. "
         "The last %d s are saved into %s as trace-<time>.json (the part of the game joins "
         "it). These save: (1) the X button and the click of the left stick held together "
         "for %d ms%s. The log writes each change of the buttons of the left hand and of "
         "the click of the right stick (at most %d lines). The join %s the clock of the game "
         "from the frames of both parts (trace_shift=%d)",
         sizeof(te::Event), min_us, g_seconds, g_files.c_str(), g_combo_ms,
         g_combo2 ? " (2) the left trigger and the click of the right stick held together "
                    "for the same time (trace_combo2=1)"
                  : ". The second combination is off (trace_combo2=0)",
         kButtonLines, g_shift_join ? "shifts" : "does not shift", g_shift_join ? 1 : 0);
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
  const uint32_t bits = tc::Bits(slot);
  if (bits != g_button_bits) {
    // What arrives from the runtime: a line for each change.
    g_button_bits = bits;
    if (g_button_lines < kButtonLines) {
      ++g_button_lines;
      const ic::Hand& left = slot.hands[0];
      AppLog("trace        : buttons, slot %u: left hand %s, stick click %d, X %d, Y %d, menu "
             "%d, trigger %.2f. Right hand %s, stick click %d%s",
             slot.frame, (bits & tc::kLeftActive) != 0 ? "active" : "not active",
             (bits & tc::kLeftStick) != 0, (bits & tc::kLeftX) != 0, (bits & tc::kLeftY) != 0,
             (bits & tc::kLeftMenu) != 0, left.trigger,
             (bits & tc::kRightActive) != 0 ? "active" : "not active",
             (bits & tc::kRightStick) != 0,
             g_button_lines == kButtonLines ? ". No more lines of this kind" : "");
    }
  }
  const int which = tc::Held(bits, g_combo2);
  if (g_latch.Step(which != 0, now, static_cast<int64_t>(g_combo_ms) * 1000000LL)) {
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
    AppLog("trace        : the combination %d (%s). Save of the last %d s", which,
           which == 2 ? "the left trigger and the click of the right stick"
                      : "the X button and the click of the left stick",
           g_seconds);
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
