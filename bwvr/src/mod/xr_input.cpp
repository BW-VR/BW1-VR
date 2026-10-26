// THE INPUT CHANNEL in the mod. See xr_input.h and docs/android.md, "The
// input channel".

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "../common/input_channel.h"
#include "frame_pace_math.h"
#include "thread_wait.h"
#include "time_log.h"
#include "xr_input.h"
#include "xr_input_math.h"
#include "view_layer_math.h"

extern void ModLog(const char* format, ...);
// controllers.cpp: the frames that the controllers saw.
uint64_t ControllersFrameCount();
// vk_scene.cpp: false with vk_device=0.
bool VkSceneDevice();

namespace {

namespace ic = input_channel;
namespace xm = xr_input_math;
namespace fp = frame_pace_math;

XrInputOptions g_options;

// THE FILE.
char g_path[MAX_PATH] = "";
HANDLE g_file = INVALID_HANDLE_VALUE;
HANDLE g_mapping = nullptr;
ic::File* g_chan = nullptr;
// The pulses can come from more than one thread of the game.
CRITICAL_SECTION g_push_lock;
bool g_push_lock_made = false;

// THE LATCH of this frame of the game.
bool g_new_frame = true;
bwvr::EyeView g_eyes[2] = {};
bwvr::HandInput g_hands[2] = {};
char g_profiles[2][ic::kProfileChars] = {};
bool g_focused = false;
bool g_have_slot = false;
uint32_t g_slot_frame = 0;
int64_t g_display_time = 0;
int64_t g_display_period = 0;
bool g_gaze_valid = false;
float g_gaze[3] = {0.0f, 0.0f, -1.0f};
bool g_have_recenter = false;
uint32_t g_recenters_seen = 0;
bool g_have_session = false;
uint32_t g_session = 0;
bool g_recenter_due = false;
// The time of the last new slot, for xr_input_stale_ms.
ULONGLONG g_slot_tick = 0;
bool g_stale = false;
int g_stale_ms = 500;
// The lockstep of a test. It goes off after one wait that ran out.
bool g_lockstep = false;

// THE PACE (vk_pace). Due once for each FinishFrame. The counts go into the
// log each 30 s.
fp::State g_pace;
bool g_pace_due = true;
int64_t g_pace_frequency = 0;
int64_t g_pace_report_ns = 0;
uint64_t g_pace_waits = 0;
uint64_t g_pace_writes = 0;  // waits that a new write ended
uint64_t g_pace_limits = 0;  // waits that the time limit ended
uint64_t g_pace_idle = 0;    // waits with the limit of no fresh slot
uint64_t g_early_reads = 0;  // reads before the wait (frame_pace_math.h, ReadKind)
uint64_t g_after_early = 0;  // latches of a frame after a read before its wait
bool g_early_in_frame = false;
bool g_said_after_early = false;
double g_pace_ms = 0.0;
double g_pace_ms_most = 0.0;

// THE COUNTS since the last report.
uint64_t g_latches = 0;
uint64_t g_new_slots = 0;
uint64_t g_torn = 0;
uint64_t g_kept = 0;  // latches that kept the values before: each try was torn
uint64_t g_waits = 0;
double g_wait_ms = 0.0;
double g_wait_ms_most = 0.0;
uint64_t g_pulses = 0;
uint64_t g_pulses_dropped = 0;

int64_t PaceNow() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return fp::CountToNs(now.QuadPart, g_pace_frequency);
}

// One try of a copy of the slot, as ic::TryRead, with the sequence of the
// write that the copy has. A torn try counts in *torn.
bool ReadOnce(ic::SlotData* d, uint32_t* seq, uint32_t* torn) {
  *seq = ic::ReadBegin(&g_chan->slot);
  bool whole = false;
  if ((*seq & 1u) == 0) {
    memcpy(d, &g_chan->slot.data, sizeof(*d));
    whole = ic::ReadEnd(&g_chan->slot, *seq);
  }
  if (!whole) ++*torn;
  return whole;
}

// THE STALE RULE. With no new slot for xr_input_stale_ms the hands read as
// not active and the focus as off. Not in the lockstep of a test: there the
// writer writes a new slot only for a new frame of the controllers, and a
// load of the game has many frames of the game with no frame of the
// controllers. The latch and the read before the wait of the pace call it.
void NoInput();
void StaleRule() {
  if (g_have_slot && g_stale_ms > 0 && !g_stale && !g_lockstep &&
      GetTickCount64() - g_slot_tick > static_cast<ULONGLONG>(g_stale_ms)) {
    g_stale = true;
    ModLog("xr input     : no new slot of the app for %d ms (slot %u). The "
           "hands are not active and the focus is off until the app writes "
           "again. The head keeps its last pose, frame %llu",
           g_stale_ms, g_slot_frame, ControllersFrameCount());
  }
  if (g_stale) NoInput();
}

// A look at the sequence of the slot. The display period comes from a whole
// copy of a new write.
void PaceLook(int64_t now) {
  if (!fp::Observe(&g_pace, ic::LoadAcquire(&g_chan->slot.seq), now)) return;
  ic::SlotData d;
  if (ic::TryRead(&g_chan->slot, &d)) fp::SetPeriod(&g_pace, d.display_period);
}

// THE RECORD.
FILE* g_record = nullptr;
uint64_t g_recorded = 0;
// THE RECORD OF THE VIEWS.
FILE* g_view_record = nullptr;
uint64_t g_views_recorded = 0;

double MsSince(const LARGE_INTEGER& from) {
  LARGE_INTEGER now, hz;
  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&hz);
  return hz.QuadPart > 0
             ? (now.QuadPart - from.QuadPart) * 1000.0 / hz.QuadPart
             : 0.0;
}

void NoInput() {
  for (int h = 0; h < 2; ++h) xm::NoHand(&g_hands[h]);
  g_focused = false;
}

// The values of a whole slot into the latch.
void Take(const ic::SlotData& d) {
  if (!g_have_slot || d.frame != g_slot_frame) {
    ++g_new_slots;
    g_slot_tick = GetTickCount64();
    if (g_stale) {
      g_stale = false;
      ModLog("xr input     : the app writes slots again, slot %u, frame %llu",
             d.frame, ControllersFrameCount());
    }
  }
  const bool first = !g_have_slot;
  g_have_slot = true;
  g_slot_frame = d.frame;
  for (int e = 0; e < 2; ++e) xm::ToEye(d.eyes[e], &g_eyes[e]);
  for (int h = 0; h < 2; ++h) {
    xm::ToHand(d.hands[h], &g_hands[h]);
    memcpy(g_profiles[h], d.hands[h].profile, ic::kProfileChars);
    g_profiles[h][ic::kProfileChars - 1] = 0;
  }
  g_focused = (d.flags & ic::kFocused) != 0;
  g_display_time = d.display_time;
  g_display_period = d.display_period;
  g_gaze_valid = (d.flags & ic::kGazeValid) != 0;
  for (int i = 0; i < 3; ++i) g_gaze[i] = d.gaze[i];
  // A new start of the app counts its recenters from 0 again. That is no
  // recenter of the player.
  const uint32_t session = ic::LoadAcquire(&g_chan->head.app_session);
  if (xm::NewSession(session, &g_have_session, &g_session)) {
    g_have_recenter = false;
    ModLog("xr input     : a new session of the app (%u), slot %u. Its count "
           "of the recenters starts with no recenter, frame %llu",
           session, d.frame, ControllersFrameCount());
  }
  if (xm::TakeRecenter(d.recenters, &g_have_recenter, &g_recenters_seen)) {
    g_recenter_due = true;
    ModLog("xr input     : the app recentered (%u recenters), frame %llu",
           d.recenters, ControllersFrameCount());
  }
  if (first) {
    ModLog("xr input     : the first slot of the app: slot %u, session %u, "
           "eyes %s, focus %s, profiles \"%s\" and \"%s\", the headset asks "
           "for %u by %u, display period %.2f ms, frame %llu",
           d.frame, g_chan->head.app_session,
           g_eyes[0].valid && g_eyes[1].valid ? "valid" : "not valid",
           g_focused ? "yes" : "no", g_profiles[0], g_profiles[1],
           d.recommended_width, d.recommended_height,
           d.display_period / 1e6, ControllersFrameCount());
  }
}

}  // namespace

void XrInputSetOptions(const XrInputOptions& options) {
  g_options = options;
  g_lockstep = options.mode != 0 && options.lockstep_ms > 0;
  for (int h = 0; h < 2; ++h) xm::NoHand(&g_hands[h]);
}

bool XrInputOn() { return g_options.mode != 0; }

bool XrInputStart(const char* dir) {
  if (!g_push_lock_made) {
    InitializeCriticalSection(&g_push_lock);
    g_push_lock_made = true;
  }
  // The stale rule: with no new slot for this time the hands read as not
  // active and the focus as off. The head keeps its last pose.
  {
    char ini[MAX_PATH];
    lstrcpynA(ini, dir, MAX_PATH);
    lstrcatA(ini, "bwvr.ini");
    g_stale_ms = static_cast<int>(
        GetPrivateProfileIntA("bwvr", "xr_input_stale_ms", 500, ini));
  }
  if (g_options.record_frames > 0) {
    char rec[MAX_PATH];
    lstrcpynA(rec, dir, MAX_PATH);
    lstrcatA(rec, "bwvr-input.rec");
    if (fopen_s(&g_record, rec, "wb") != 0) g_record = nullptr;
    if (g_record != nullptr) {
      setvbuf(g_record, nullptr, _IOFBF, 1 << 20);
      ModLog("xr input     : xr_input_record=%d. The input of the first %d "
             "frames of the controllers goes to %s (a test aid)",
             g_options.record_frames, g_options.record_frames, rec);
    } else {
      ModLog("xr input     : xr_input_record=%d, but %s cannot be made",
             g_options.record_frames, rec);
    }
  }
  if (g_options.view_record_frames > 0) {
    char rec[MAX_PATH];
    lstrcpynA(rec, dir, MAX_PATH);
    lstrcatA(rec, "bwvr-view.rec");
    if (fopen_s(&g_view_record, rec, "wb") != 0) g_view_record = nullptr;
    if (g_view_record != nullptr) {
      ModLog("xr input     : xr_view_record=%d. The projection layer of the "
             "first %d frames of the scene goes to %s (a test aid)",
             g_options.view_record_frames, g_options.view_record_frames, rec);
    } else {
      ModLog("xr input     : xr_view_record=%d, but %s cannot be made",
             g_options.view_record_frames, rec);
    }
  }
  if (g_options.mode == 0) return false;
  {
    LARGE_INTEGER hz;
    QueryPerformanceFrequency(&hz);
    g_pace_frequency = hz.QuadPart;
  }

  lstrcpynA(g_path, dir, MAX_PATH);
  lstrcatA(g_path, "bwvr-input.chan");
  g_file = CreateFileA(g_path, GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (g_file != INVALID_HANDLE_VALUE) {
    g_mapping = CreateFileMappingA(g_file, nullptr, PAGE_READWRITE, 0,
                                   ic::kFileBytes, nullptr);
  }
  void* view = nullptr;
  if (g_mapping != nullptr) {
    view = MapViewOfFile(g_mapping, FILE_MAP_ALL_ACCESS, 0, 0, ic::kFileBytes);
  }
  if (view == nullptr) {
    const DWORD error = GetLastError();
    if (g_mapping != nullptr) CloseHandle(g_mapping);
    if (g_file != INVALID_HANDLE_VALUE) CloseHandle(g_file);
    g_mapping = nullptr;
    g_file = INVALID_HANDLE_VALUE;
    ModLog("xr input     : xr_input=1, but the channel %s cannot be made "
           "(error %lu). The mod has no head and no hands", g_path, error);
    return false;
  }
  g_chan = static_cast<ic::File*>(view);
  const bool whole = ic::HeadOk(g_chan, ic::kFileBytes);
  if (!whole) ic::MakeHead(g_chan);
  // The keys of the app from before this start are not for this game.
  const uint32_t old_keys = ic::DropOldKeys(g_chan);
  if (old_keys > 0) {
    ModLog("xr input     : %u keys of the app from before the start of the game "
           "are dropped", old_keys);
  }
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  ic::StoreRelease(&g_chan->head.mod_session,
                   GetCurrentProcessId() ^ static_cast<uint32_t>(now.QuadPart));
  ModLog("xr input     : xr_input=1. The views of the eyes, the hands, the "
         "display time, the focus and the recenter come from the input "
         "channel %s (%s), and the pulses go back through it. The mod starts "
         "no OpenXR session. xr_input_stale_ms=%d", g_path,
         whole ? "the app made it" : "a new head", g_stale_ms);
  if (g_options.pace != 0 && !VkSceneDevice()) {
    ModLog("xr input     : vk_pace=1 with vk_device=0. Each frame of the game "
           "waits for the next write of the slot of the app, up to 5/4 of its "
           "display period after the start of the frame before. With no "
           "slot, a slot that does not change for %lld periods or no display "
           "period, up to one period of %lld Hz",
           static_cast<long long>(fp::kStalePeriods),
           static_cast<long long>(fp::kIdleHz));
  } else if (g_options.pace != 0) {
    ModLog("xr input     : vk_pace=1 needs vk_device=0. The mod has a Vulkan "
           "device, so the frames of the game do not wait for the app");
  }
  if (g_lockstep) {
    ModLog("xr input     : xr_input_lockstep_ms=%d (a test aid). Each latch "
           "waits up to %d ms for the slot of its frame of the controllers",
           g_options.lockstep_ms, g_options.lockstep_ms);
  }
  return true;
}

void XrInputNewFrame() {
  g_new_frame = true;
  g_pace_due = true;
  g_early_in_frame = false;
}

bool XrInputPace() {
  if (!g_pace_due) return false;
  g_pace_due = false;
  if (g_chan == nullptr || g_options.pace == 0 || VkSceneDevice()) return false;
  const int64_t start = PaceNow();
  PaceLook(start);
  const int64_t deadline = fp::Deadline(g_pace, start);
  const bool fresh = fp::Fresh(g_pace, start);
  int64_t now = start;
  fp::Step step;
  while ((step = fp::Next(g_pace, deadline, now)) == fp::Step::kWait) {
    const int64_t ns = fp::SleepNs(g_pace, deadline, now);
    if (!ThreadWaitSleep(fp::NsToCount(ns, g_pace_frequency))) Sleep(1);
    now = PaceNow();
    PaceLook(now);
  }
  fp::End(&g_pace, now);
  const double ms = (now - start) / 1e6;
  TimeLogPace(ms);
  ++g_pace_waits;
  g_pace_ms += ms;
  if (ms > g_pace_ms_most) g_pace_ms_most = ms;
  if (step == fp::Step::kGo) {
    ++g_pace_writes;
  } else {
    ++g_pace_limits;
  }
  if (!fresh) ++g_pace_idle;
  if (g_pace_report_ns == 0) g_pace_report_ns = start;
  if (now - g_pace_report_ns >= 30000000000LL) {
    ModLog("xr input     : the pace: %llu frames in %.1f s (%.1f a second), %llu "
           "started at a new write of the slot and %llu at the time limit, "
           "%llu with no fresh slot. A frame waited %.3f ms as a mean and "
           "%.3f ms at most. The display period of the slot is %.2f ms, "
           "frame %llu",
           g_pace_waits, (now - g_pace_report_ns) / 1e9,
           g_pace_waits * 1e9 / (now - g_pace_report_ns), g_pace_writes,
           g_pace_limits, g_pace_idle, g_pace_ms / g_pace_waits,
           g_pace_ms_most, g_pace.period_ns / 1e6, ControllersFrameCount());
    g_pace_report_ns = now;
    g_pace_waits = g_pace_writes = g_pace_limits = g_pace_idle = 0;
    g_pace_ms = g_pace_ms_most = 0.0;
  }
  return true;
}

void XrInputLatch() {
  if (g_chan == nullptr || !g_new_frame) return;
  // THE READ BEFORE THE WAIT (frame_pace_math.h, ReadKind). With the pace on,
  // the intro skip reads the slot in FinishFrame, before the wait of the
  // frame. That read takes the newest slot only: no want for the writer, no
  // lockstep, no write taken for the pace, and the latch of the frame stays
  // open for the first read after the wait.
  const bool pace_on = g_options.pace != 0 && !VkSceneDevice();
  if (fp::ReadKind(pace_on, g_pace_due) == fp::Read::kBeforeWait) {
    ic::SlotData early = {};
    uint32_t early_seq = 0, early_torn = 0;
    bool early_whole = false;
    for (int i = 0; i < 64 && !early_whole; ++i) {
      early_whole = ReadOnce(&early, &early_seq, &early_torn);
    }
    if (early_torn > 0) {
      g_torn += early_torn;
      ic::StoreRelease(&g_chan->head.torn, g_chan->head.torn + early_torn);
    }
    ++g_early_reads;
    g_early_in_frame = true;
    if (early_whole && early.frame != 0) Take(early);
    StaleRule();
    return;
  }
  g_new_frame = false;
  ++g_latches;
  if (g_early_in_frame) {
    g_early_in_frame = false;
    ++g_after_early;
    if (!g_said_after_early) {
      g_said_after_early = true;
      ModLog("xr input     : the first latch of a frame after a read before "
             "its wait (vk_pace=1): the latch of the frame comes after the "
             "wait, frame %llu of the controllers",
             ControllersFrameCount());
    }
  }
  const uint32_t want = static_cast<uint32_t>(ControllersFrameCount());
  ic::StoreRelease(&g_chan->head.want, want);
  ic::StoreRelease(&g_chan->head.latches, g_chan->head.latches + 1);

  ic::SlotData d = {};
  uint32_t torn = 0;
  bool whole = false;
  // The sequence of the write of the copy, for the pace.
  uint32_t seq = 0;
  if (g_lockstep) {
    LARGE_INTEGER start;
    QueryPerformanceCounter(&start);
    int spins = 0;
    for (;;) {
      whole = ReadOnce(&d, &seq, &torn);
      if (whole && xm::SlotFor(d.frame, want)) break;
      const double ms = MsSince(start);
      if (ms >= g_options.lockstep_ms) {
        g_lockstep = false;
        ModLog("xr input     : no slot for frame %u of the controllers in %d "
               "ms. The lockstep stops, and each latch takes the newest slot",
               want, g_options.lockstep_ms);
        break;
      }
      if (++spins > 64) {
        SwitchToThread();
      } else {
        YieldProcessor();
      }
    }
    const double ms = MsSince(start);
    ++g_waits;
    g_wait_ms += ms;
    if (ms > g_wait_ms_most) g_wait_ms_most = ms;
  } else {
    for (int i = 0; i < 64 && !whole; ++i) whole = ReadOnce(&d, &seq, &torn);
  }
  if (torn > 0) {
    g_torn += torn;
    ic::StoreRelease(&g_chan->head.torn, g_chan->head.torn + torn);
  }
  if (!whole) {
    ++g_kept;
  } else if (d.frame != 0) {
    Take(d);
  }
  // The next frame of the pace waits for a write after this one. With the
  // pace on this latch comes after the wait of its frame (the read before
  // the wait returns above).
  if (whole && pace_on) {
    const int64_t now = PaceNow();
    fp::Took(&g_pace, seq, now);
    fp::SetPeriod(&g_pace, d.display_period);
  }
  StaleRule();
}

const bwvr::EyeView& XrInputEye(int index) { return g_eyes[index ? 1 : 0]; }
const bwvr::HandInput& XrInputHand(int index) { return g_hands[index ? 1 : 0]; }
const char* XrInputProfile(int index) { return g_profiles[index ? 1 : 0]; }
bool XrInputFocused() { return g_focused; }

bool XrInputTakeRecenter() {
  const bool due = g_recenter_due;
  g_recenter_due = false;
  return due;
}

bool XrInputDisplayTime(int64_t* time, int64_t* period) {
  if (!g_have_slot || g_display_time == 0) return false;
  *time = g_display_time;
  *period = g_display_period;
  return true;
}

double XrInputDisplaySeconds() {
  return g_have_slot ? static_cast<double>(g_display_time) * 1e-9 : 0.0;
}

bool XrInputGaze(float direction[3]) {
  if (!g_have_slot || !g_gaze_valid) return false;
  for (int i = 0; i < 3; ++i) direction[i] = g_gaze[i];
  return true;
}

void XrInputHaptic(int index, float amplitude, float seconds) {
  if (g_chan == nullptr) return;
  ic::Event e;
  e.frame = static_cast<uint32_t>(ControllersFrameCount());
  e.hand = index ? 1u : 0u;
  e.amplitude = amplitude;
  e.seconds = seconds;
  EnterCriticalSection(&g_push_lock);
  if (ic::PushEvent(g_chan, e)) {
    ++g_pulses;
  } else {
    ++g_pulses_dropped;
  }
  LeaveCriticalSection(&g_push_lock);
}

uint32_t XrInputSlotFrame() { return g_have_slot ? g_slot_frame : 0u; }

void XrInputWriteWorld(const ic::World& world) {
  if (g_chan == nullptr) return;
  ic::WriteWorld(&g_chan->world, world);
}

uint32_t XrInputTakeKeys(uint32_t* codes, uint32_t max) {
  if (g_chan == nullptr || max == 0) return 0;
  ic::KeyEvent keys[ic::kKeys];
  bool broken = false;
  const uint32_t n = ic::TakeKeys(g_chan, keys, max < ic::kKeys ? max : ic::kKeys, &broken);
  if (broken) {
    ModLog("xr input     : the indices of the keys of the app are not valid. The mod "
           "drops the keys that wait, frame %llu", ControllersFrameCount());
  }
  for (uint32_t i = 0; i < n; ++i) codes[i] = keys[i].code;
  return n;
}

bool XrInputRecording() { return g_record != nullptr; }

void XrInputRecordFrame(uint64_t frame, int source, const bwvr::EyeView eyes[2],
                        const bwvr::HandInput hands[2],
                        const char* const profiles[2], bool focused,
                        int64_t display_time, int64_t display_period) {
  if (g_record == nullptr) return;
  if (g_recorded == 0) {
    const ic::RecordHead head = {ic::kRecordMagic, ic::kVersion,
                                 static_cast<uint32_t>(sizeof(ic::SlotData)),
                                 static_cast<uint32_t>(source)};
    fwrite(&head, sizeof(head), 1, g_record);
  }
  ic::SlotData d;
  memset(&d, 0, sizeof(d));
  d.frame = static_cast<uint32_t>(frame);
  d.flags = focused ? ic::kFocused : 0u;
  d.display_time = display_time;
  d.display_period = display_period;
  for (int e = 0; e < 2; ++e) xm::FromEye(eyes[e], &d.eyes[e]);
  for (int h = 0; h < 2; ++h) xm::FromHand(hands[h], profiles[h], &d.hands[h]);
  fwrite(&d, sizeof(d), 1, g_record);
  ++g_recorded;
  if (g_recorded >= static_cast<uint64_t>(g_options.record_frames)) {
    fclose(g_record);
    g_record = nullptr;
    ModLog("xr input     : the record has the input of %llu frames of the "
           "controllers, to frame %llu", g_recorded, frame);
  }
}

bool XrInputViewRecording() { return g_view_record != nullptr; }

void XrInputRecordView(const scene_stream::ViewBody& view, int source) {
  if (g_view_record == nullptr) return;
  if (g_views_recorded == 0) {
    const view_layer_math::RecordHead head = {
        view_layer_math::kRecordMagic, view_layer_math::kRecordVersion,
        static_cast<uint32_t>(sizeof(view)), static_cast<uint32_t>(source)};
    fwrite(&head, sizeof(head), 1, g_view_record);
  }
  fwrite(&view, sizeof(view), 1, g_view_record);
  ++g_views_recorded;
  if (g_views_recorded >= static_cast<uint64_t>(g_options.view_record_frames)) {
    fclose(g_view_record);
    g_view_record = nullptr;
    ModLog("xr input     : the record of the views has %llu frames of the "
           "scene, to frame %llu", g_views_recorded, view.frame);
  }
}

void XrInputReport() {
  if (g_chan == nullptr || g_latches == 0) return;
  ModLog("xr input     : %llu latches, %llu new slots of the app (the last "
         "slot %u), %llu torn reads read again, %llu latches that kept the "
         "values before, %llu pulses into the ring and %llu dropped (the app "
         "read %u of %u)%s",
         g_latches, g_new_slots, g_slot_frame, g_torn, g_kept, g_pulses,
         g_pulses_dropped, ic::LoadAcquire(&g_chan->head.event_read),
         g_chan->head.event_write, g_stale ? ". No new slot now" : "");
  if (g_early_reads > 0) {
    ModLog("xr input     : %llu reads of the newest slot before the wait of "
           "their frame (the intro skip in FinishFrame, vk_pace=1), and %llu "
           "latches of a frame after such a read",
           g_early_reads, g_after_early);
    g_early_reads = g_after_early = 0;
  }
  if (g_waits > 0) {
    ModLog("xr input     : the lockstep waited %.3f ms as a mean and %.3f ms "
           "at most, %llu waits", g_wait_ms / g_waits, g_wait_ms_most, g_waits);
  }
  g_latches = g_new_slots = g_torn = g_kept = g_pulses = g_pulses_dropped = 0;
  g_waits = 0;
  g_wait_ms = g_wait_ms_most = 0.0;
}
