// The mixer of the mod (docs/audio.md, "7. The mixer of the mod").
//
// QMixer pans a positional sound between two speakers. It has no cue for up
// and down, a near sound sits hard in one ear, and a far sound is only
// quieter. To change that, the mod must have the signal of each source. This
// file takes it where the audio DLL gives it to QMixer:
//
// - QSWaveMixPlayEx gets the wave of a sample: 16 bit PCM that the DLL owns
//   (MIXWAVE: the format at +0x00, the data at +0x10, the bytes at +0x14).
//   For a sample with a place, the hook copies the wave and starts a voice of
//   the mod on it. QMixer plays the same channel with the volume 0. So QMixer
//   stays the clock: it still tells the DLL when the sample is done, and the
//   game's logic does not change.
// - The voice follows the channel: the hooks on SetVolume, SetFrequency,
//   StopChannel, FlushChannel, PauseChannel and RestartChannel move it, and
//   the hooks of audio_rolloff.cpp give it the place and the distance
//   mapping of the game.
// - audio_capture.cpp calls Render for each block that QMixer gives to the
//   sound device. Render adds the voices to the block, each one through the
//   model of audio_spatial.h: the direction with its height for two ears,
//   the near field, and the low pass by distance.
//
// WHAT STAYS WITH QMIXER: each flat sound, so the advisors, the story
// speech, the interface, and the music. They sound as in the plain game.
//
// SAFE BY DEFAULT. A voice starts only when the stream is 16 bit stereo and
// the wave is 16 bit PCM. In each other case QMixer plays the sample as in
// the plain game. Each level and each filter moves in small steps (15 ms), a
// stop fades in 8 ms, and the sum is kept inside 16 bits. The waves are copies
// in buffers that the slots keep and use again.
//
// THE WAIT OF A STOP (docs/audio.md, "7.6 The wait of a stop"). The audio DLL
// stops a sample that plays in three steps. The volume of the channel goes
// to 0 with a pan rate of 20. The DLL calls Sleep(20). Then it flushes
// the channel. Three functions of the DLL have these steps (RVA 0x12CC1,
// 0x12D79 and 0x12E6E). The Sleep is on the thread of the caller.
//
// In each game turn, GAudio::ProcessAudioGameTurn (0x00427080) stops each
// positional sample that is out of range (LHSampleUpdate3DChannels). So each
// such stop holds the frame for 20 ms, and two stops in one turn hold it for
// 40 ms.
//
// QMixer has the volume 0 for a channel that a voice of the mod plays, from
// the start of the play. So the wait lets nothing fade there, and the mod
// skips that Sleep for such a channel. The voice of the mod gets the 20 ms
// in its own fade, so it sounds the same. A channel that QMixer plays keeps
// the wait.
//
// THE PUMP IN QSWaveMixPlayEx (docs/audio.md, "7.8 The pump of a play").
// QSWaveMixPlayEx runs one pump of QMixer on the thread of the caller after it
// has started the sample. On the Steam Frame a play of the atmosphere takes
// 17 to 30 ms in this call. With audio_play_now the mod hooks the pump and the
// sort of the channels of QMixer. For a play that the mode defers they return
// at once on the thread of that play, and the pump of the timer thread of the
// audio DLL does the work within 20 ms. No byte of the session changes.
// audio_play_fix does the same with the byte at +5 of the session, which the
// timer thread reads too. audio_play_parts times the functions of the call.
//
// THE OPEN IN QSWaveMixPlayEx. In the same call QMixer opens the wave of the
// play (RVA 0xC230). A wave of the format PCM or float opens at once, on the
// thread of the caller. The formats MS ADPCM, A-law and IMA ADPCM open at once
// too, as soon as the audio compression manager has loaded once. Each other
// format goes to the loader thread of the session. audio_play_parts times
// each function below the open, the system calls of QMixer in it, and the
// lock wait of the timer pump, and logs the format of the first opens.
// audio_open_loader sends the open of a sound of the atmosphere to the loader
// thread. The sound then starts when the loader has opened the wave, and the
// frame thread does not wait for the open.
//
// Ini: audio_mixer (default 1, 0 is QMixer's own panning), audio_height,
// audio_near, audio_far, audio_bed, audio_atmos_mixer (each default 1),
// audio_height_pct (100, the strength of the height band, 0 to 200),
// audio_stop_wait (default 0, 1 is the wait of the audio DLL at each stop),
// audio_play_now (default 0, the modes of audio_play_math.h: 0 is the work of
// QMixer in the call, as in the game), audio_play_fix (default 0, the byte of
// the session, the same modes), audio_play_parts (default 0, test only: the
// time of the functions of QMixer in a play),
// audio_open_loader (default 0, the open as QMixer has it; 1 sends the open of
// a sound of the atmosphere to the loader thread, 2 each open).
// Test only: audio_play_fix_test, audio_play_now_test, audio_open_loader_test,
// audio_test_voice, audio_test_path, audio_test_step_ms, audio_test_solo.

#include <windows.h>
#include <mmsystem.h>
#include <intrin.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MinHook.h"
#include "audio.h"
#include "audio_watch.h"
#include "audio_math.h"
#include "audio_play_math.h"
#include "audio_spatial.h"
#include "trace_rec.h"
#include "debug_menu.h"

void ModLog(const char* format, ...);
float TabletopViewScale();
float HeadPoseScale();
// controllers.cpp: a pulse on the pointing hand, for a test key. The frame
// of the controllers, and the number that a test key with a frame compares
// with its frame (fake_controller_play_frame).
void ControllersGesturePulse(float amplitude, float seconds);
uint64_t ControllersFrameCount();
uint64_t ControllersTestFrame(uint64_t key_frame, uint64_t frame);

namespace {

namespace sp = audio_spatial;

typedef DWORD(WINAPI* PlayExFn)(void*, int, DWORD, void*, int, void*);
typedef DWORD(WINAPI* Channel4Fn)(void*, int, DWORD, DWORD);
typedef DWORD(WINAPI* Channel3Fn)(void*, int, DWORD);
typedef DWORD(WINAPI* ActivateFn)(void*, BOOL);
// QSWaveMixIsChannelDone(session, channel). stdcall, ret 8. Not 0 when the
// channel plays nothing.
typedef BOOL(WINAPI* IsDoneFn)(void*, int);
IsDoneFn f_IsDone = nullptr;
DWORD g_last_watch = 0;
int g_watch_lines = 0;

PlayExFn o_PlayEx = nullptr;
Channel4Fn o_SetVolume = nullptr;
Channel4Fn o_SetFrequency = nullptr;
Channel3Fn o_Stop = nullptr;
Channel3Fn o_Pause = nullptr;
Channel3Fn o_Restart = nullptr;
Channel3Fn o_Flush = nullptr;
ActivateFn o_Activate = nullptr;

// MIXWAVE of QMixer (QSWaveMixPlayEx checks 0x48 bytes of it).
const int kWaveFormat = 0x00;   // PCMWAVEFORMAT
const int kWaveData = 0x10;     // the samples
const int kWaveBytes = 0x14;
const int kWaveSize = 0x48;
// The full volume of QSWaveMixSetVolume.
const float kFullVolume = 32767.0f;
const DWORD kVolumeFlags = 0x20;
// The level of a voice with a place at its minimum distance, for each ear.
// With 0.315 (QMixer's own number for a positional channel) the two ears have
// 7 dB less power than QMixer's two channels for the same sounds. With 0.7
// the power is the same for a source at the side, and QMixer is 1 to 2 dB
// louder for a source ahead: its pan gives a source ahead more power. 0.77 is
// between the two, within about 1 dB of QMixer in each direction.
const float kPlaceGain = 0.77f;
// A wave larger than this stays with QMixer.
const DWORD kMaxWaveBytes = 16u * 1024u * 1024u;
// A buffer larger than this many samples is freed when its voice ends. A
// slot keeps a smaller one for its next voice, so 40 slots keep 8 MB at most.
const int kKeepSamples = 100 * 1024;
const float kReleaseSeconds = audio_math::kReleaseSeconds;
// A voice this near its end, in seconds, plays to its end when QMixer stops
// its channel. QMixer is the clock, and the voice can be a block behind.
const float kTailSeconds = 0.30f;

const int kChannels = 128;
const int kSlots = 40;
const int kChunk = 512;

enum Kind { kPlace, kBed, kTest, kCue, kKinds };

struct Slot {
  bool used;
  int channel;          // the QMixer channel, or -1 when it has none now
  int kind;
  int16_t* data;        // the copy of the wave
  int capacity;         // samples that data can hold
  int frames;
  int channels;         // 1 or 2
  float wave_rate;      // Hz, with SetFrequency
  double cursor;        // frames
  int loops_left;       // more plays after this one, -1 without end
  bool paused;
  bool releasing;       // it fades out, then the slot is free
  float volume;         // 0..1, the volume of the channel
  float position[3];
  float map[3];         // the game's min, max and rolloff
  bool have_map;
  float fixed_gain;     // a level that does not change with distance, or -1
  float test_part;      // TEST: the part of the range, or -1
  float pan_left, pan_right;   // the two gains of a cue
  float fade;           // the release, 1 down to 0
  sp::Voice dsp;
  sp::Bed bed;
};

struct ChannelState {
  DWORD volume;         // the last volume that the DLL set
  DWORD volume_flags;
  DWORD frequency;      // the last frequency that the DLL set, or 0
  int frequency_play;   // the play in which it was set
  bool shadowed;        // a voice of the mod plays this channel's sample
  bool muted;           // QMixer has the volume 0 for it, from the mod
  bool stop_skipped;    // the mod skipped the wait of the stop that runs now
  float point[3];
  int point_play;       // the play in which the place was set
  float map[3];
  int map_play;         // the play in which the mapping was set
  bool map_positional;
  float polar_range;    // the range of the polar position of the DLL
  int polar_play;       // the play in which it was set
  int done_looks;       // looks in a row at which QMixer played nothing
};

struct Listener {
  float position[3];
  float basis[9];       // columns right, up and forward
  float k;              // the room factor
  float units_per_meter;
  bool room;            // the room distances apply: no story owns the view
  bool have;
};

CRITICAL_SECTION g_lock;
Slot g_slots[kSlots];
ChannelState g_channels[kChannels];
Listener g_listener;

bool g_on = false;          // audio_mixer, and the hooks are in
bool g_audible = true;      // the test key: false gives QMixer's panning
bool g_height = true;       // audio_height
float g_height_pct = 1.0f;  // audio_height_pct / 100
bool g_near = true;         // audio_near
bool g_far = true;          // audio_far
bool g_bed = true;          // audio_bed
bool g_atmos = true;        // audio_atmos_mixer
volatile LONG g_started_atmos = 0;
int g_solo = 0;             // audio_test_solo
void* g_session = nullptr;

// The play of LHSamplePlay in progress.
bool g_in_play = false;
int g_play = 0;             // goes up with each play
int g_play_kind = 0;
int g_play_flag3d = 0;
int g_play_relative = 0;
// A place that the mod made for this play: a single sound of the atmosphere
// (audio_sources.cpp).
bool g_play_has_place = false;
float g_play_place[3];

// The test voice.
int g_test_voice = 0;
int g_test_step_ms = 3000;
struct TestStep { float azimuth, elevation, meters, part; };
TestStep g_test_steps[24];
int g_test_count = 0;
int g_test_step = -1;
long g_test_frame = 0;      // the capture frame at which the step started
long g_test_seen = 0;       // the capture frame of the last frame
int g_test_slot = -1;
int g_test_wait = 0;

// The wait of a stop. A site is the address that the Sleep of a stop returns
// to, in the audio DLL: its RVA. The 8 bytes before each site start with
// "push 20" and the call through the import slot.
const int kStopSites = 3;
const uintptr_t kStopSleepReturn[kStopSites] = {0x12CEB, 0x12DAD, 0x12EA2};
const uint8_t kStopSleepBytes[4] = {0x6A, 0x14, 0xFF, 0x15};  // push 20, call
const DWORD kStopWaitMs = 20;
typedef void(WINAPI* SleepFn)(DWORD);
SleepFn o_StopSleep = nullptr;
int g_stop_wait = 0;          // audio_stop_wait
// The sites in this process. All 0 when the audio DLL is not the known one.
uintptr_t g_stop_site[kStopSites] = {0, 0, 0};
bool g_stop_hooked = false;
// The channel that the DLL set to the volume 0 for a stop, and the thread
// that did it. The channel is -1 when the last volume call was no such call.
int g_stop_channel = -1;
DWORD g_stop_thread = 0;
volatile LONG g_stop_skipped = 0;
volatile LONG g_stop_kept = 0;
LONG g_stop_reported = -1;
// audio_stop_wait_test: the frame of the controllers at which the test key
// acts by itself, for a desktop run. 0 is off.
int g_stop_test_frame = 0;
uint64_t g_stop_test_seen = 0;

// The pump of QSWaveMixPlayEx. g_play_fix is the mode of audio_play_math.h.
// g_pump_known is true when the code of the known QMixer.dll was found.
// The times are in ticks of QueryPerformanceCounter, for the report.
int g_play_fix = 0;
bool g_pump_known = false;
int64_t g_qpc_hz = 0;
int g_play_test_frame = 0;      // audio_play_fix_test
uint64_t g_play_test_seen = 0;
typedef void(WINAPI* PumpFn)();
PumpFn o_Pump = nullptr;
CRITICAL_SECTION g_play_cs;
// [0] a play of the atmosphere, [1] another play. Second index: 0 with the
// pump of QMixer, 1 with no pump.
audio_play::Stat g_play_stat[2][2];
audio_play::Stat g_pump_stat;
int64_t g_play_changed = 0;     // calls with no pump, since the start
DWORD g_play_report_at = 0;

// audio_play_now. The play in progress on this thread. A hook of QMixer reads
// it, so the other threads (the timer thread) always run in full.
struct PlayCall {
  bool defer;         // the sort and the pump return at once
  bool timing;        // audio_play_parts
  bool atmosphere;    // a sound of the atmosphere
  bool force_loader;  // audio_open_loader: the open goes to the loader thread
  bool in_open;       // inside the open of the wave
  bool forced;        // the format test was changed in this call
  int fmt_result;     // the answer of the format test, -1 for none
  const void* wave;   // the wave of QSWaveMixPlayEx
  audio_play::PlayParts parts;
};
__declspec(thread) PlayCall* t_call = nullptr;
// True on the timer thread inside QSWaveMixPump.
__declspec(thread) bool t_in_pump = false;
// Sets the call of this thread, and puts the one before back at the end.
struct CallGuard {
  explicit CallGuard(PlayCall* call) : previous_(t_call) { t_call = call; }
  ~CallGuard() { t_call = previous_; }
  PlayCall* previous_;
};
int g_play_now = 0;             // audio_play_now
bool g_now_known = false;       // the hooks on the sort and the pump are in
bool g_parts_on = false;        // audio_play_parts
int g_play_now_test_frame = 0;  // audio_play_now_test
uint64_t g_play_now_test_seen = 0;
volatile LONG g_defer_sort = 0;   // runs of the sort that the hook skipped
volatile LONG g_defer_pump = 0;   // runs of the pump that the hook skipped
// The functions of QMixer that the hooks time, by part.
void* o_part[audio_play::kParts] = {};
// Per play, [0] a sound of the atmosphere, [1] another sound.
struct PartsGroup {
  int64_t plays = 0;
  audio_play::Stat part[audio_play::kParts];
};
PartsGroup g_parts[2];
bool g_part_hooked[audio_play::kParts] = {};  // the hook of a part is in
const uint8_t* g_qm_base = nullptr;           // the base of QMixer.dll
uint32_t g_qm_size = 0;                       // its size in memory

// audio_open_loader and the log of the opens.
int g_open_mode = 0;                // audio_open_loader
bool g_open_known = false;          // the hook on the format test is in
int g_open_test_frame = 0;          // audio_open_loader_test
uint64_t g_open_test_seen = 0;
volatile LONG g_open_forced = 0;    // opens that the mode sent to the loader
volatile LONG g_open_path[2][2] = {};  // [atmosphere, other][at once, loader]
int g_open_logged[2] = {0, 0};      // lines of opens so far
const int kOpenLogMax = 20;
const int kVtableMax = 24;
uint32_t g_vtable_seen[kVtableMax] = {};
int g_vtable_count = 0;
const int kNamesMax = 64;
char g_names[kNamesMax][48] = {};   // names that QMixer got from the system
int g_name_count = 0;
audio_play::Stat g_lock_stat;       // the lock wait of QSWaveMixPump

// For the report.
volatile LONG g_started[kKinds] = {0, 0, 0, 0};
volatile LONG g_refused_format = 0;
volatile LONG g_refused_slots = 0;
volatile LONG g_clipped = 0;
volatile LONG g_render_calls = 0;
double g_render_us = 0.0, g_render_worst_us = 0.0;
int g_most_voices = 0;
DWORD g_last_report = 0;
LONG g_reported = -1;
bool g_first_logged = false;

bool Readable(const void* p, size_t n) {
  return p != nullptr && !IsBadReadPtr(p, n);
}

struct Lock {
  Lock() {
    // audio_turn_watch=1: a wait for the lock on the frame thread in an
    // audio turn has a timer.
    if (g_audio_watch && !TryEnterCriticalSection(&g_lock)) {
      const int64_t token = AudioWatchBegin(kAwModLock, 0);
      EnterCriticalSection(&g_lock);
      AudioWatchEnd(kAwModLock, token);
    } else if (!g_audio_watch) {
      EnterCriticalSection(&g_lock);
    }
  }
  ~Lock() { LeaveCriticalSection(&g_lock); }
};

float UnitsPerMeter() {
  const float table = TabletopViewScale();
  if (table > 0.0f) return table;
  const float life = HeadPoseScale();
  return life > 0.0f ? life : 7.5f;
}

Slot* SlotOfChannel(int channel) {
  for (int i = 0; i < kSlots; ++i) {
    if (g_slots[i].used && g_slots[i].channel == channel) return &g_slots[i];
  }
  return nullptr;
}

// What a voice with a place gets now. Under the lock.
sp::Target TargetOf(const Slot& s) {
  sp::Target t = {0.0f, 0.0f, 0.0f, 0.0f, 12000.0f, 0.0f, g_height_pct};
  const Listener& l = g_listener;
  float distance = 0.0f;
  sp::Aim aim = {0.0f, 0.0f, 0.0f};
  if (l.have) aim = sp::AimOf(l.basis, l.position, s.position, &distance);
  const float k = l.k > 0.0f ? l.k : 1.0f;
  const float design = distance / k;
  float gain = 1.0f, part = 0.0f;
  if (s.fixed_gain >= 0.0f) {
    gain = s.fixed_gain;
  } else if (s.have_map) {
    gain = audio_math::QMixerDistanceGain(design, s.map[0], s.map[1],
                                          s.map[2]);
    part = sp::RangePart(design, s.map[0], s.map[1]);
  }
  if (s.test_part >= 0.0f) part = s.test_part;
  const float meters =
      l.units_per_meter > 0.0f ? distance / l.units_per_meter : distance;
  const float focus = g_near ? sp::NearFocus(meters) : 1.0f;
  t.lateral = aim.lateral * focus;
  t.elevation = g_height ? aim.elevation : 0.0f;
  t.back = aim.back * focus;
  t.gain = s.volume * gain * kPlaceGain;
  // In a story the game puts its camera where it wants the speech to be
  // heard, so a far sound keeps its high frequencies there, as in the game.
  if (g_far && (l.room || s.test_part >= 0.0f)) {
    t.corner = sp::DistanceCorner(part);
    t.diffuse = sp::DiffuseShare(part);
  }
  return t;
}

// Reads `count` frames of the wave of a slot at the rate of the output, with
// a straight line between two samples. Mono goes to out[i]. Stereo goes to
// out[2 i] and out[2 i + 1], or to its mean when `mono` is set. Returns
// false when the wave ended in this chunk.
bool ReadWave(Slot* s, float rate, int count, bool mono, float* out) {
  const double step = s->wave_rate / rate;
  const int16_t* d = s->data;
  const int frames = s->frames;
  bool alive = true;
  for (int i = 0; i < count; ++i) {
    if (alive && s->cursor >= frames) {
      if (s->loops_left != 0) {
        if (s->loops_left > 0) --s->loops_left;
        s->cursor -= frames;
        if (s->cursor >= frames) s->cursor = 0.0;
      } else {
        alive = false;
      }
    }
    float a = 0.0f, b = 0.0f;
    if (alive) {
      const int i0 = static_cast<int>(s->cursor);
      const float part = static_cast<float>(s->cursor - i0);
      int i1 = i0 + 1;
      if (i1 >= frames) i1 = s->loops_left != 0 ? 0 : i0;
      if (s->channels == 1) {
        a = d[i0] + (d[i1] - d[i0]) * part;
        b = a;
      } else {
        a = d[2 * i0] + (d[2 * i1] - d[2 * i0]) * part;
        b = d[2 * i0 + 1] + (d[2 * i1 + 1] - d[2 * i0 + 1]) * part;
      }
      s->cursor += step;
    }
    if (mono || s->channels == 1) {
      out[i] = 0.5f * (a + b);
    } else {
      out[2 * i] = a;
      out[2 * i + 1] = b;
    }
  }
  return alive;
}

void EndSlot(Slot* s) {
  s->used = false;
  s->channel = -1;
}

// From audio_capture.cpp, on the mixer thread of QMixer: add the voices.
void Render(int16_t* stereo, int frames, int rate) {
  if (!g_on) return;
  LARGE_INTEGER start;
  QueryPerformanceCounter(&start);
  static float left[kChunk], right[kChunk], source[2 * kChunk];
  const float out_rate = static_cast<float>(rate);
  const float release = 1.0f / (kReleaseSeconds * out_rate);
  {
    Lock lock;
    const float yaw = g_listener.have ? sp::HeadYaw(g_listener.basis) : 0.0f;
    for (int done = 0; done < frames; done += kChunk) {
      const int n = frames - done < kChunk ? frames - done : kChunk;
      memset(left, 0, sizeof(float) * n);
      memset(right, 0, sizeof(float) * n);
      int voices = 0;
      for (int v = 0; v < kSlots; ++v) {
        Slot& s = g_slots[v];
        if (!s.used) continue;
        ++voices;
        // A paused voice fades first, and then it keeps its place.
        const bool hold = s.paused && !s.releasing;
        if (hold && (s.kind == kBed ? s.bed.gain_now : s.dsp.now.gain) <
                        1e-4f) {
          continue;
        }
        if (g_solo == 2 && s.kind != kTest && s.kind != kCue &&
            v != g_test_slot) {
          // TEST: the other voices keep their time and make no sound.
          if (!ReadWave(&s, out_rate, n, s.kind != kBed, source)) EndSlot(&s);
          continue;
        }
        bool alive = true;
        if (s.kind == kCue) {
          // A cue is flat. Its wave starts and ends at zero.
          alive = ReadWave(&s, out_rate, n, true, source);
          for (int i = 0; i < n; ++i) {
            left[i] += s.pan_left * source[i];
            right[i] += s.pan_right * source[i];
          }
        } else if (s.kind == kBed) {
          alive = ReadWave(&s, out_rate, n, false, source);
          float gain = hold ? 0.0f : s.volume;
          if (s.releasing) {
            // The level goes to 0 in the steps of ProcessBed. The slot is
            // free when it is there.
            s.fade -= release * n;
            gain = 0.0f;
            if (s.fade <= audio_math::kReleaseEnd) alive = false;
          }
          sp::ProcessBed(&s.bed, yaw, gain, out_rate, source, n, left, right);
        } else {
          alive = ReadWave(&s, out_rate, n, true, source);
          sp::Target t = TargetOf(s);
          if (hold) t.gain = 0.0f;
          if (s.releasing) {
            // The gain goes to 0 in the steps of Process. The slot is free
            // when it is there (80 ms, more than 5 time constants).
            s.fade -= release * n;
            t.gain = 0.0f;
            if (s.fade <= audio_math::kReleaseEnd) alive = false;
          }
          sp::Process(&s.dsp, t, out_rate, source, n, left, right);
        }
        if (!alive) EndSlot(&s);
      }
      if (voices > g_most_voices) g_most_voices = voices;
      if (g_solo != 0) memset(stereo + 2 * done, 0, sizeof(int16_t) * 2 * n);
      if (g_audible) {
        for (int i = 0; i < n; ++i) {
          const float mix[2] = {left[i], right[i]};
          for (int c = 0; c < 2; ++c) {
            const float sum = stereo[2 * (done + i) + c] + mix[c];
            const float bent = sp::SoftLimit(sum);
            if (bent != sum) InterlockedIncrement(&g_clipped);
            stereo[2 * (done + i) + c] = static_cast<int16_t>(
                bent + (bent < 0.0f ? -0.5f : 0.5f));
          }
        }
      }
    }
  }
  LARGE_INTEGER end, frequency;
  QueryPerformanceCounter(&end);
  QueryPerformanceFrequency(&frequency);
  if (g_trace) {
    static const trace_events::Def kRenderDef = {"mod voices render (mixer thread)", "audio", {nullptr, nullptr, nullptr, nullptr}};
    TraceNameThread("QMixer mixer (render)", 13);
    TraceBar(&kRenderDef, start.QuadPart, end.QuadPart);
  }
  const double us = 1e6 * static_cast<double>(end.QuadPart - start.QuadPart) /
                    static_cast<double>(frequency.QuadPart);
  g_render_us += us;
  if (us > g_render_worst_us) g_render_worst_us = us;
  InterlockedIncrement(&g_render_calls);
}

// A free slot with room for `frames` frames of `channels` channels, or
// null. Only the game thread starts a voice, so a free slot stays free
// until StartSlot, and the buffer can be made outside the lock.
Slot* TakeSlot(int frames, int channels) {
  Slot* s = nullptr;
  {
    Lock lock;
    for (int i = 0; i < kSlots && s == nullptr; ++i) {
      if (!g_slots[i].used && i != g_test_slot) s = &g_slots[i];
    }
  }
  if (s == nullptr) return nullptr;
  const int need = frames * channels;
  if (s->data == nullptr || s->capacity < need) {
    free(s->data);
    s->data = static_cast<int16_t*>(malloc(sizeof(int16_t) * need));
    s->capacity = s->data != nullptr ? need : 0;
    if (s->data == nullptr) return nullptr;
  }
  return s;
}

// Puts a filled slot in use. Under the lock, the last step of a start.
void StartSlot(Slot* s, int channel, int kind, int frames, int channels,
               float wave_rate, int loops) {
  s->channel = channel;
  s->kind = kind;
  s->frames = frames;
  s->channels = channels;
  s->wave_rate = wave_rate;
  s->cursor = 0.0;
  s->loops_left = loops;
  s->paused = false;
  s->releasing = false;
  s->fade = audio_math::ReleaseStart(false);
  s->test_part = -1.0f;
  s->have_map = false;
  s->fixed_gain = -1.0f;
  sp::Reset(&s->dsp);
  sp::ResetBed(&s->bed);
  s->used = true;
}

// The voice of a channel ends. With `tail`, a voice near its own end plays
// to it. Under the lock.
void ReleaseChannel(int channel, bool tail) {
  Slot* s = SlotOfChannel(channel);
  if (s == nullptr) return;
  s->channel = -1;
  if (tail && s->loops_left == 0 && s->wave_rate > 0.0f &&
      (s->frames - s->cursor) / s->wave_rate < kTailSeconds) {
    return;
  }
  s->releasing = true;
}

// ---- The hooks on the functions of QMixer in a play -------------------------

// The def of a part in the timeline (trace_rec.h).
const trace_events::Def* PartDef(int part) {
  static trace_events::Def defs[audio_play::kParts];
  if (defs[part].name == nullptr) {
    defs[part].cat = "audio";
    defs[part].name = audio_play::kPartInfo[part].name;
  }
  return &defs[part];
}

// Times one run of a part on the thread of a play with audio_play_parts. The
// system calls count only inside the open (RVA 0xC230).
struct PartTimer {
  PartTimer(int part) : part_(part), call_(t_call), start_(0) {
    if (call_ != nullptr && call_->timing &&
        (!audio_play::IsSysPart(part) || call_->in_open)) {
      LARGE_INTEGER t;
      QueryPerformanceCounter(&t);
      start_ = t.QuadPart;
    }
  }
  ~PartTimer() {
    if (start_ != 0) {
      LARGE_INTEGER t;
      QueryPerformanceCounter(&t);
      call_->parts.Add(part_, t.QuadPart - start_);
      if (g_trace) TraceBar(PartDef(part_), start_, t.QuadPart);
    }
  }
  int part_;
  PlayCall* call_;
  int64_t start_;
};

// The functions are thiscall in the game. A fastcall function gets the same
// ecx, and the stack is the same for the arguments after the first.
typedef void*(__fastcall* Ctor5Fn)(void*, void*, int, int, int, int, int);
typedef void*(__fastcall* Ctor3Fn)(void*, void*, int, int, int);
typedef int(__fastcall* ThisPtrFn)(void*, void*, void*);
typedef int(__fastcall* ThisIntFn)(void*, void*, int);
typedef void(__fastcall* ThisFn)(void*);
typedef void(__fastcall* VoiceFn)(void*, void*, int);
typedef bool(__fastcall* FlagsFn)(void*, void*, unsigned);
typedef void(__fastcall* LoaderFn)(void*, void*, int, int, int, int, int);
typedef unsigned(__fastcall* FormatFn)(void*);
typedef int(__fastcall* InitFn)(void*);

void* __fastcall h_PartCtor(void* self, void* edx, int a, int b, int c, int d,
                            int e) {
  PartTimer timer(audio_play::kPartCtor);
  return reinterpret_cast<Ctor5Fn>(o_part[audio_play::kPartCtor])(
      self, edx, a, b, c, d, e);
}
int __fastcall h_PartParams(void* self, void* edx, void* p) {
  PartTimer timer(audio_play::kPartParams);
  return reinterpret_cast<ThisPtrFn>(o_part[audio_play::kPartParams])(self, edx,
                                                                      p);
}
int __fastcall h_PartQueue(void* self, void* edx, void* p) {
  PartTimer timer(audio_play::kPartQueue);
  return reinterpret_cast<ThisPtrFn>(o_part[audio_play::kPartQueue])(self, edx,
                                                                     p);
}
int __fastcall h_PartAdd(void* self, void* edx, void* p) {
  PartTimer timer(audio_play::kPartAdd);
  return reinterpret_cast<ThisPtrFn>(o_part[audio_play::kPartAdd])(self, edx,
                                                                   p);
}
int __fastcall h_PartAppend(void* self, void* edx, void* p) {
  PartTimer timer(audio_play::kPartAppend);
  return reinterpret_cast<ThisPtrFn>(o_part[audio_play::kPartAppend])(self, edx,
                                                                      p);
}
void __fastcall h_PartClear(void* self) {
  PartTimer timer(audio_play::kPartClear);
  reinterpret_cast<ThisFn>(o_part[audio_play::kPartClear])(self);
}
void __fastcall h_PartVoice(void* self, void* edx, int on) {
  PartTimer timer(audio_play::kPartVoice);
  reinterpret_cast<VoiceFn>(o_part[audio_play::kPartVoice])(self, edx, on);
}
// The sort of the channels and the pump. In a play that the mode defers they
// return at once. The flag of the session that asks for a sort stays set, so
// the next run on the timer thread does the work.
void __fastcall h_PartSort(void* session) {
  PlayCall* call = t_call;
  if (call != nullptr && call->defer) {
    InterlockedIncrement(&g_defer_sort);
    return;
  }
  PartTimer timer(audio_play::kPartSort);
  reinterpret_cast<ThisFn>(o_part[audio_play::kPartSort])(session);
}
void __fastcall h_PartPump(void* session) {
  PlayCall* call = t_call;
  if (call != nullptr && call->defer) {
    InterlockedIncrement(&g_defer_pump);
    return;
  }
  PartTimer timer(audio_play::kPartPump);
  reinterpret_cast<ThisFn>(o_part[audio_play::kPartPump])(session);
}

// The parts below the open.
bool __fastcall h_PartFlags(void* self, void* edx, unsigned flags) {
  PartTimer timer(audio_play::kPartFlags);
  return reinterpret_cast<FlagsFn>(o_part[audio_play::kPartFlags])(self, edx,
                                                                   flags);
}
void __fastcall h_PartLoader(void* self, void* edx, int a, int b, int c, int d,
                             int e) {
  PartTimer timer(audio_play::kPartLoader);
  reinterpret_cast<LoaderFn>(o_part[audio_play::kPartLoader])(self, edx, a, b,
                                                              c, d, e);
}
// The Open of a source (the call at +0x10 of its vtable). Which one runs
// depends on the class of the source.
int __fastcall h_PartSrcMap(void* self, void* edx, void* out) {
  PartTimer timer(audio_play::kPartSrcMap);
  return reinterpret_cast<ThisPtrFn>(o_part[audio_play::kPartSrcMap])(self, edx,
                                                                      out);
}
int __fastcall h_PartSrcBase(void* self, void* edx, void* out) {
  PartTimer timer(audio_play::kPartSrcBase);
  return reinterpret_cast<ThisPtrFn>(o_part[audio_play::kPartSrcBase])(
      self, edx, out);
}
int __fastcall h_PartSrcMem(void* self, void* edx, void* out) {
  PartTimer timer(audio_play::kPartSrcMem);
  return reinterpret_cast<ThisPtrFn>(o_part[audio_play::kPartSrcMem])(self, edx,
                                                                      out);
}
int __fastcall h_PartSrcFile(void* self, void* edx, void* out) {
  PartTimer timer(audio_play::kPartSrcFile);
  return reinterpret_cast<ThisPtrFn>(o_part[audio_play::kPartSrcFile])(
      self, edx, out);
}
void* __fastcall h_PartPcmCtor(void* self, void* edx, int a, int b, int c) {
  PartTimer timer(audio_play::kPartPcmCtor);
  return reinterpret_cast<Ctor3Fn>(o_part[audio_play::kPartPcmCtor])(self, edx,
                                                                     a, b, c);
}
void* __fastcall h_PartDecCtor(void* self, void* edx, int a, int b, int c) {
  PartTimer timer(audio_play::kPartDecCtor);
  return reinterpret_cast<Ctor3Fn>(o_part[audio_play::kPartDecCtor])(self, edx,
                                                                     a, b, c);
}
int __fastcall h_PartDecInit(void* self) {
  PartTimer timer(audio_play::kPartDecInit);
  return reinterpret_cast<InitFn>(o_part[audio_play::kPartDecInit])(self);
}
int __fastcall h_PartSeekPcm(void* self, void* edx, int position) {
  PartTimer timer(audio_play::kPartSeekPcm);
  return reinterpret_cast<ThisIntFn>(o_part[audio_play::kPartSeekPcm])(
      self, edx, position);
}
// The lock of the session. QSWaveMixPump takes it on the timer thread, and the
// time of that wait goes to g_lock_stat. A play takes it too.
void __fastcall h_PartLock(void* session) {
  if (!t_in_pump) {
    PartTimer timer(audio_play::kPartLock);
    reinterpret_cast<ThisFn>(o_part[audio_play::kPartLock])(session);
    return;
  }
  LARGE_INTEGER t0, t1;
  QueryPerformanceCounter(&t0);
  reinterpret_cast<ThisFn>(o_part[audio_play::kPartLock])(session);
  QueryPerformanceCounter(&t1);
  EnterCriticalSection(&g_play_cs);
  g_lock_stat.Add(t1.QuadPart - t0.QuadPart, g_qpc_hz / 200);
  LeaveCriticalSection(&g_play_cs);
}

// The test at RVA 0x265D0 says if the open of a wave runs at once (0) or goes
// to the loader thread of the session (1). audio_open_loader answers 1 for a
// play of the mode: the loader thread then opens the wave, as it does for each
// wave of a format that is not PCM or float. Only the thread of that play
// has a call, so the loader thread and the timer thread see no change.
unsigned __fastcall h_PartFormat(void* source) {
  PlayCall* call = t_call;
  unsigned r;
  {
    PartTimer timer(audio_play::kPartFormat);
    r = reinterpret_cast<FormatFn>(o_part[audio_play::kPartFormat])(source) &
        0xFF;
  }
  if (call != nullptr) {
    call->fmt_result = static_cast<int>(r);
    if (r == 0 && call->force_loader) {
      call->forced = true;
      InterlockedIncrement(&g_open_forced);
      r = 1;
    }
  }
  return r;
}

// ---- The log of the opens (audio_play_parts) --------------------------------

// What the open of one play reads, before it runs.
struct OpenInfo {
  uint32_t vtable;       // the RVA of the vtable of the source, 0 for none
  bool have_format;      // the source has a format
  uint32_t tag, channels, rate, bits, bytes;
  bool have_wave;        // the wave object of the play has a format
  uint32_t wave_tag, wave_channels, wave_rate, wave_bits, wave_bytes;
  bool same_wave;        // the wave object is the wave of QSWaveMixPlayEx
  bool loader_thread;    // the session has a loader thread
  bool acm_ready;        // the audio compression manager has loaded
};

bool ReadU32(const uint8_t* p, uint32_t* v) {
  if (!Readable(p, 4)) return false;
  *v = *reinterpret_cast<const uint32_t*>(p);
  return true;
}

// The offsets are from the decompile of the known QMixer.dll: the play object
// has the session at +0x34 and the wave object at +0x40. The wave object has
// the source at +0x44. A source has the data at +8, the size at +0xC and a
// pointer to a copy of the WAVEFORMATEX at +0x1C.
void ReadOpenInfo(const uint8_t* play, const void* wave_of_call, OpenInfo* o) {
  memset(o, 0, sizeof(*o));
  uint32_t v = 0;
  o->acm_ready = Readable(g_qm_base + audio_play::kAcmReadyRva, 1) &&
                 g_qm_base[audio_play::kAcmReadyRva] != 0;
  if (ReadU32(play + 0x34, &v) && v != 0) {
    uint32_t loader = 0, thread = 0;
    const uint8_t* session = reinterpret_cast<const uint8_t*>(v);
    if (ReadU32(session + audio_play::kLoaderOffset, &loader) && loader != 0 &&
        ReadU32(reinterpret_cast<const uint8_t*>(loader) +
                    audio_play::kLoaderThreadOffset, &thread)) {
      o->loader_thread = thread != 0;
    }
  }
  if (!ReadU32(play + 0x40, &v) || v == 0) return;
  const uint8_t* wave = reinterpret_cast<const uint8_t*>(v);
  o->same_wave = wave == wave_of_call;
  if (Readable(wave, kWaveSize)) {
    const PCMWAVEFORMAT* f =
        reinterpret_cast<const PCMWAVEFORMAT*>(wave + kWaveFormat);
    o->have_wave = true;
    o->wave_tag = f->wf.wFormatTag;
    o->wave_channels = f->wf.nChannels;
    o->wave_rate = f->wf.nSamplesPerSec;
    o->wave_bits = f->wBitsPerSample;
    o->wave_bytes = *reinterpret_cast<const DWORD*>(wave + kWaveBytes);
  }
  if (!ReadU32(wave + 0x44, &v) || v == 0) return;
  const uint8_t* source = reinterpret_cast<const uint8_t*>(v);
  uint32_t vt = 0, fmt = 0;
  if (ReadU32(source, &vt)) o->vtable = vt - reinterpret_cast<uint32_t>(g_qm_base);
  ReadU32(source + 0xC, &o->bytes);
  if (ReadU32(source + 0x1C, &fmt) && fmt != 0 &&
      Readable(reinterpret_cast<const void*>(fmt), 16)) {
    const uint8_t* f = reinterpret_cast<const uint8_t*>(fmt);
    o->have_format = true;
    o->tag = *reinterpret_cast<const uint16_t*>(f);
    o->channels = *reinterpret_cast<const uint16_t*>(f + 2);
    o->rate = *reinterpret_cast<const uint32_t*>(f + 4);
    o->bits = *reinterpret_cast<const uint16_t*>(f + 14);
  }
}

// The part that has the function at this RVA, or -1.
int PartOfRva(uint32_t rva) {
  if (rva == 0) return -1;
  for (int i = 0; i < audio_play::kPartCodeEnd; ++i) {
    if (audio_play::kPartInfo[i].rva == rva) return i;
  }
  return -1;
}

// Logs the first slots of a vtable that the open has not shown yet: the RVA of
// the vtable and the target of each slot, and if the target has a timer. A new
// target with no timer is a function to time in the next run.
void NoteVtable(const char* kind, uint32_t vtable_rva, const uint8_t* vtable) {
  if (vtable_rva == 0 || !Readable(vtable, 28)) return;
  for (int i = 0; i < g_vtable_count; ++i) {
    if (g_vtable_seen[i] == vtable_rva) return;
  }
  if (g_vtable_count >= kVtableMax) return;
  g_vtable_seen[g_vtable_count++] = vtable_rva;
  char text[640];
  size_t n = 0;
  text[0] = 0;
  const uint32_t* slots = reinterpret_cast<const uint32_t*>(vtable);
  for (int slot = 3; slot <= 6; ++slot) {
    const uint32_t rva = slots[slot] - reinterpret_cast<uint32_t>(g_qm_base);
    const bool inside = rva < g_qm_size;
    const int part = inside ? PartOfRva(rva) : -1;
    n += snprintf(text + n, sizeof(text) - n, "%s slot +0x%X is 0x%X (%s)",
                  slot > 3 ? "," : "", slot * 4,
                  inside ? rva : slots[slot],
                  !inside ? "outside QMixer"
                  : part >= 0 && g_part_hooked[part]
                      ? audio_play::kPartInfo[part].name
                  : part >= 0 ? "has a hook that did not go in"
                              : "NO TIMER, a new target");
    if (n >= sizeof(text) - 100) break;
  }
  ModLog("audio open   : %s vtable at QMixer RVA 0x%X:%s", kind, vtable_rva,
         text);
}

// One line for each of the first kOpenLogMax opens of the atmosphere and of the
// other sounds.
void LogOpen(const PlayCall* call, const OpenInfo& in, int64_t ticks,
             int result) {
  const int group = call->atmosphere ? 0 : 1;
  const bool unknown = in.loader_thread && call->fmt_result < 0;
  const bool loader = in.loader_thread && (call->fmt_result != 0 || call->forced);
  if (!unknown) InterlockedIncrement(&g_open_path[group][loader ? 1 : 0]);
  if (g_open_logged[group] >= kOpenLogMax) return;
  ++g_open_logged[group];
  const bool model_at_once =
      audio_play::OpensAtOnce(in.loader_thread, in.have_format, in.tag,
                              in.acm_ready);
  const double us = g_qpc_hz > 0
                        ? 1e6 * static_cast<double>(ticks) /
                              static_cast<double>(g_qpc_hz)
                        : 0.0;
  ModLog("audio open   : %s %d: source format tag 0x%X (%s), %u ch, %u Hz, "
         "%u bits, %u bytes of data. Wave object: tag 0x%X, %u ch, %u Hz, %u "
         "bits, %u bytes, %s the wave of the play. Source vtable RVA 0x%X. "
         "Loader thread: %s. Compression manager loaded: %s. Path: %s%s "
         "(the model says %s). The open took %.0f us, result %d",
         group == 0 ? "atmosphere" : "other sound", g_open_logged[group],
         in.tag, audio_play::TagName(in.tag), in.channels, in.rate, in.bits,
         in.bytes, in.wave_tag, in.wave_channels, in.wave_rate, in.wave_bits,
         in.wave_bytes, in.same_wave ? "the same object as" : "not",
         in.vtable, in.loader_thread ? "yes" : "no",
         in.acm_ready ? "yes" : "no",
         unknown ? "unknown" : loader ? "loader thread" : "at once",
         call->forced ? " (forced by audio_open_loader)" : "",
         model_at_once ? "at once" : "loader thread", us, result);
}

// The open of the wave of a play (RVA 0xC230).
int __fastcall h_PartOpen(void* self, void* edx, int flags) {
  PlayCall* call = t_call;
  if (call == nullptr || !call->timing) {
    return reinterpret_cast<ThisIntFn>(o_part[audio_play::kPartOpen])(self, edx,
                                                                      flags);
  }
  const uint8_t* play = static_cast<const uint8_t*>(self);
  OpenInfo info;
  ReadOpenInfo(play, call->wave, &info);
  if (info.vtable != 0) {
    NoteVtable("source", info.vtable, g_qm_base + info.vtable);
  }
  call->in_open = true;
  call->fmt_result = -1;
  LARGE_INTEGER t0, t1;
  QueryPerformanceCounter(&t0);
  const int r = reinterpret_cast<ThisIntFn>(o_part[audio_play::kPartOpen])(
      self, edx, flags);
  QueryPerformanceCounter(&t1);
  call->in_open = false;
  call->parts.Add(audio_play::kPartOpen, t1.QuadPart - t0.QuadPart);
  if (g_trace) TraceBar(PartDef(audio_play::kPartOpen), t0.QuadPart, t1.QuadPart);
  // The reader that the open made: a play that opened at once has it now.
  uint32_t reader = 0, reader_vt = 0;
  if (ReadU32(play + 0xE4, &reader) && reader != 0 &&
      ReadU32(reinterpret_cast<const uint8_t*>(reader), &reader_vt)) {
    NoteVtable("reader", reader_vt - reinterpret_cast<uint32_t>(g_qm_base),
               reinterpret_cast<const uint8_t*>(reader_vt));
  }
  LogOpen(call, info, t1.QuadPart - t0.QuadPart, r);
  return r;
}

// ---- The system calls of QMixer inside the open ------------------------------

// QMixer calls the system through its import table. With audio_play_parts the
// mod puts a timer in each entry of the list below. A call counts only on the
// thread of a play, inside its open. The functions of the audio compression
// manager come by GetProcAddress, and the mod gives QMixer a timer for them.
#define SYS_FN(ret, name, part, params, args)                                 \
  ret WINAPI name params {                                                    \
    PartTimer timer(part);                                                    \
    return reinterpret_cast<ret(WINAPI*) params>(o_part[part]) args;          \
  }
SYS_FN(void, w_EnterCS, audio_play::kSysEnter, (LPCRITICAL_SECTION cs), (cs))
SYS_FN(void, w_LeaveCS, audio_play::kSysLeave, (LPCRITICAL_SECTION cs), (cs))
SYS_FN(BOOL, w_SetEvent, audio_play::kSysSetEvent, (HANDLE h), (h))
SYS_FN(DWORD, w_Wait, audio_play::kSysWait, (HANDLE h, DWORD ms), (h, ms))
SYS_FN(void, w_Sleep, audio_play::kSysSleep, (DWORD ms), (ms))
SYS_FN(HANDLE, w_CreateFile, audio_play::kSysCreateFile,
       (LPCSTR a, DWORD b, DWORD c, LPSECURITY_ATTRIBUTES d, DWORD e, DWORD f,
        HANDLE g),
       (a, b, c, d, e, f, g))
SYS_FN(BOOL, w_ReadFile, audio_play::kSysReadFile,
       (HANDLE a, LPVOID b, DWORD c, LPDWORD d, LPOVERLAPPED e),
       (a, b, c, d, e))
SYS_FN(DWORD, w_SetFilePtr, audio_play::kSysSetFilePtr,
       (HANDLE a, LONG b, PLONG c, DWORD d), (a, b, c, d))
SYS_FN(HANDLE, w_Mapping, audio_play::kSysMapping,
       (HANDLE a, LPSECURITY_ATTRIBUTES b, DWORD c, DWORD d, DWORD e, LPCSTR f),
       (a, b, c, d, e, f))
SYS_FN(LPVOID, w_MapView, audio_play::kSysMapView,
       (HANDLE a, DWORD b, DWORD c, DWORD d, SIZE_T e), (a, b, c, d, e))
SYS_FN(LPVOID, w_HeapAlloc, audio_play::kSysHeapAlloc,
       (HANDLE a, DWORD b, SIZE_T c), (a, b, c))
SYS_FN(LPVOID, w_VirtAlloc, audio_play::kSysVirtAlloc,
       (LPVOID a, SIZE_T b, DWORD c, DWORD d), (a, b, c, d))
SYS_FN(HRESULT, w_CoCreate, audio_play::kSysCoCreate,
       (const void* a, void* b, DWORD c, const void* d, void** e),
       (a, b, c, d, e))
SYS_FN(HMMIO, w_MmioOpen, audio_play::kSysMmioOpen,
       (LPSTR a, LPMMIOINFO b, DWORD c), (a, b, c))
SYS_FN(LONG, w_MmioRead, audio_play::kSysMmioRead, (HMMIO a, HPSTR b, LONG c),
       (a, b, c))
SYS_FN(LONG, w_MmioSeek, audio_play::kSysMmioSeek, (HMMIO a, LONG b, int c),
       (a, b, c))
SYS_FN(MMRESULT, w_MmioDescend, audio_play::kSysMmioDescend,
       (HMMIO a, LPMMCKINFO b, const MMCKINFO* c, UINT d), (a, b, c, d))
// The functions of msacm32.dll, with the prototypes of the documentation.
SYS_FN(UINT, w_AcmSuggest, audio_play::kSysAcmSuggest,
       (void* a, void* b, void* c, DWORD d, DWORD e), (a, b, c, d, e))
SYS_FN(UINT, w_AcmOpen, audio_play::kSysAcmOpen,
       (void* a, void* b, void* c, void* d, void* e, DWORD_PTR f, DWORD_PTR g,
        DWORD h),
       (a, b, c, d, e, f, g, h))
SYS_FN(UINT, w_AcmSize, audio_play::kSysAcmSize,
       (void* a, DWORD b, LPDWORD c, DWORD d), (a, b, c, d))
SYS_FN(UINT, w_AcmPrepare, audio_play::kSysAcmPrepare,
       (void* a, void* b, DWORD c), (a, b, c))
#undef SYS_FN

// The names that QMixer gets from the system, once each: the first
// kNamesMax. A name comes from GetProcAddress or LoadLibraryA.
void NoteName(const char* what, const char* name) {
  bool fresh = false;
  EnterCriticalSection(&g_play_cs);
  for (int i = 0; i < g_name_count; ++i) {
    if (strcmp(g_names[i], name) == 0) {
      LeaveCriticalSection(&g_play_cs);
      return;
    }
  }
  if (g_name_count < kNamesMax) {
    strncpy_s(g_names[g_name_count], sizeof(g_names[0]), name, _TRUNCATE);
    ++g_name_count;
    fresh = true;
  }
  LeaveCriticalSection(&g_play_cs);
  if (!fresh) return;
  const PlayCall* call = t_call;
  ModLog("audio open   : QMixer calls %s for %s (%s)", what, name,
         call != nullptr && call->in_open ? "on the frame thread, in the open"
                                          : "on another thread, or outside "
                                            "the open");
}

FARPROC WINAPI w_GetProc(HMODULE module, LPCSTR name) {
  FARPROC r;
  {
    PartTimer timer(audio_play::kSysGetProc);
    r = reinterpret_cast<FARPROC(WINAPI*)(HMODULE, LPCSTR)>(
        o_part[audio_play::kSysGetProc])(module, name);
  }
  if (r == nullptr || reinterpret_cast<uintptr_t>(name) <= 0xFFFF) return r;
  NoteName("GetProcAddress", name);
  struct Acm {
    const char* name;
    int part;
    void* wrapper;
  };
  static const Acm kAcm[] = {
      {"acmFormatSuggest", audio_play::kSysAcmSuggest,
       reinterpret_cast<void*>(&w_AcmSuggest)},
      {"acmStreamOpen", audio_play::kSysAcmOpen,
       reinterpret_cast<void*>(&w_AcmOpen)},
      {"acmStreamSize", audio_play::kSysAcmSize,
       reinterpret_cast<void*>(&w_AcmSize)},
      {"acmStreamPrepareHeader", audio_play::kSysAcmPrepare,
       reinterpret_cast<void*>(&w_AcmPrepare)},
  };
  for (const Acm& a : kAcm) {
    if (strcmp(name, a.name) == 0) {
      o_part[a.part] = reinterpret_cast<void*>(r);
      return reinterpret_cast<FARPROC>(a.wrapper);
    }
  }
  return r;
}

HMODULE WINAPI w_LoadLib(LPCSTR name) {
  HMODULE r;
  {
    PartTimer timer(audio_play::kSysLoadLib);
    r = reinterpret_cast<HMODULE(WINAPI*)(LPCSTR)>(
        o_part[audio_play::kSysLoadLib])(name);
  }
  if (name != nullptr) NoteName("LoadLibraryA", name);
  return r;
}

struct SysImport {
  const char* dll;
  const char* name;
  int part;
  void* wrapper;
};
const SysImport kSysImport[] = {
    {"kernel32.dll", "EnterCriticalSection", audio_play::kSysEnter,
     reinterpret_cast<void*>(&w_EnterCS)},
    {"kernel32.dll", "LeaveCriticalSection", audio_play::kSysLeave,
     reinterpret_cast<void*>(&w_LeaveCS)},
    {"kernel32.dll", "SetEvent", audio_play::kSysSetEvent,
     reinterpret_cast<void*>(&w_SetEvent)},
    {"kernel32.dll", "WaitForSingleObject", audio_play::kSysWait,
     reinterpret_cast<void*>(&w_Wait)},
    {"kernel32.dll", "Sleep", audio_play::kSysSleep,
     reinterpret_cast<void*>(&w_Sleep)},
    {"kernel32.dll", "GetProcAddress", audio_play::kSysGetProc,
     reinterpret_cast<void*>(&w_GetProc)},
    {"kernel32.dll", "LoadLibraryA", audio_play::kSysLoadLib,
     reinterpret_cast<void*>(&w_LoadLib)},
    {"kernel32.dll", "CreateFileA", audio_play::kSysCreateFile,
     reinterpret_cast<void*>(&w_CreateFile)},
    {"kernel32.dll", "ReadFile", audio_play::kSysReadFile,
     reinterpret_cast<void*>(&w_ReadFile)},
    {"kernel32.dll", "SetFilePointer", audio_play::kSysSetFilePtr,
     reinterpret_cast<void*>(&w_SetFilePtr)},
    {"kernel32.dll", "CreateFileMappingA", audio_play::kSysMapping,
     reinterpret_cast<void*>(&w_Mapping)},
    {"kernel32.dll", "MapViewOfFile", audio_play::kSysMapView,
     reinterpret_cast<void*>(&w_MapView)},
    {"kernel32.dll", "HeapAlloc", audio_play::kSysHeapAlloc,
     reinterpret_cast<void*>(&w_HeapAlloc)},
    {"kernel32.dll", "VirtualAlloc", audio_play::kSysVirtAlloc,
     reinterpret_cast<void*>(&w_VirtAlloc)},
    {"ole32.dll", "CoCreateInstance", audio_play::kSysCoCreate,
     reinterpret_cast<void*>(&w_CoCreate)},
    {"winmm.dll", "mmioOpenA", audio_play::kSysMmioOpen,
     reinterpret_cast<void*>(&w_MmioOpen)},
    {"winmm.dll", "mmioRead", audio_play::kSysMmioRead,
     reinterpret_cast<void*>(&w_MmioRead)},
    {"winmm.dll", "mmioSeek", audio_play::kSysMmioSeek,
     reinterpret_cast<void*>(&w_MmioSeek)},
    {"winmm.dll", "mmioDescend", audio_play::kSysMmioDescend,
     reinterpret_cast<void*>(&w_MmioDescend)},
};

// Puts the wrappers in the import table of QMixer.dll. One aligned store of 4
// bytes for each entry, so a call in progress sees the old or the new pointer.
// Returns the number of entries changed.
int PatchImports(HMODULE module) {
  const uint8_t* base = reinterpret_cast<const uint8_t*>(module);
  if (!Readable(base, sizeof(IMAGE_DOS_HEADER))) return 0;
  const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
  const IMAGE_NT_HEADERS* nt =
      reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  if (!Readable(nt, sizeof(IMAGE_NT_HEADERS)) ||
      nt->Signature != IMAGE_NT_SIGNATURE) {
    return 0;
  }
  const IMAGE_DATA_DIRECTORY& dir =
      nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (dir.VirtualAddress == 0) return 0;
  const IMAGE_IMPORT_DESCRIPTOR* d =
      reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
  int patched = 0;
  for (; Readable(d, sizeof(*d)) && d->Name != 0; ++d) {
    const char* dll = reinterpret_cast<const char*>(base + d->Name);
    const uintptr_t* names = reinterpret_cast<const uintptr_t*>(
        base + (d->OriginalFirstThunk != 0 ? d->OriginalFirstThunk
                                           : d->FirstThunk));
    uintptr_t* slots = reinterpret_cast<uintptr_t*>(
        const_cast<uint8_t*>(base) + d->FirstThunk);
    for (int i = 0; names[i] != 0; ++i) {
      if ((names[i] & 0x80000000u) != 0) continue;
      const char* fn = reinterpret_cast<const char*>(base + names[i] + 2);
      for (const SysImport& imp : kSysImport) {
        if (_stricmp(dll, imp.dll) != 0 || strcmp(fn, imp.name) != 0) continue;
        if (o_part[imp.part] != nullptr) continue;
        DWORD old = 0;
        if (!VirtualProtect(&slots[i], sizeof(slots[i]), PAGE_READWRITE, &old)) {
          continue;
        }
        o_part[imp.part] = reinterpret_cast<void*>(slots[i]);
        slots[i] = reinterpret_cast<uintptr_t>(imp.wrapper);
        VirtualProtect(&slots[i], sizeof(slots[i]), old, &old);
        ++patched;
      }
    }
  }
  return patched;
}

// The hook function of each part that has a code hook, in the order of the
// enum. A part with no hook has nullptr.
void* const kPartHook[] = {
    nullptr,
    reinterpret_cast<void*>(&h_PartCtor),
    reinterpret_cast<void*>(&h_PartParams),
    reinterpret_cast<void*>(&h_PartQueue),
    reinterpret_cast<void*>(&h_PartOpen),
    reinterpret_cast<void*>(&h_PartAdd),
    reinterpret_cast<void*>(&h_PartClear),
    reinterpret_cast<void*>(&h_PartAppend),
    reinterpret_cast<void*>(&h_PartSort),
    reinterpret_cast<void*>(&h_PartPump),
    reinterpret_cast<void*>(&h_PartVoice),
    reinterpret_cast<void*>(&h_PartFlags),
    reinterpret_cast<void*>(&h_PartLoader),
    reinterpret_cast<void*>(&h_PartFormat),
    reinterpret_cast<void*>(&h_PartSrcMap),
    reinterpret_cast<void*>(&h_PartSrcBase),
    reinterpret_cast<void*>(&h_PartSrcMem),
    reinterpret_cast<void*>(&h_PartSrcFile),
    reinterpret_cast<void*>(&h_PartPcmCtor),
    reinterpret_cast<void*>(&h_PartDecCtor),
    reinterpret_cast<void*>(&h_PartDecInit),
    reinterpret_cast<void*>(&h_PartSeekPcm),
    reinterpret_cast<void*>(&h_PartLock),
};
static_assert(sizeof(kPartHook) / sizeof(kPartHook[0]) ==
                  audio_play::kPartCodeEnd,
              "one hook for each part of the code");

DWORD WINAPI h_PlayEx(void* session, int channel, DWORD flags, void* wave,
                      int loops, void* params) {
  AudioCaptureSession(session);
  g_session = session;
  bool shadow = false;
  const bool valid = channel >= 0 && channel < kChannels;
  if (g_on && valid) {
    ChannelState& c = g_channels[channel];
    {
      Lock lock;
      ReleaseChannel(channel, true);
    }
    c.shadowed = false;
    const uint8_t* w = static_cast<const uint8_t*>(wave);
    const int rate = AudioOutRate();
    // A place that was set in this play makes it a positional sample. The
    // bed of the atmosphere is the flat stereo wave of LHAtmosProcess.
    // Before the first ears are known a voice has no direction and no
    // distance, so QMixer plays the sample.
    const bool place = g_in_play && c.point_play == g_play &&
                       g_play_relative == 0 &&
                       g_play_kind != kAudioAdvisor && g_listener.have;
    // A single sound of the atmosphere (a bird, a wave) has a place in the
    // room from the mod. The DLL plays it with a polar position, and its
    // range in that call gives its level.
    const bool made = g_in_play && g_play_has_place && g_atmos &&
                      c.polar_play == g_play && !place;
    int kind = -1;
    if (rate > 0 && g_in_play && Readable(w, kWaveSize)) {
      const PCMWAVEFORMAT* f =
          reinterpret_cast<const PCMWAVEFORMAT*>(w + kWaveFormat);
      const uint8_t* data = *reinterpret_cast<uint8_t* const*>(w + kWaveData);
      const DWORD bytes = *reinterpret_cast<const DWORD*>(w + kWaveBytes);
      const int channels = f->wf.nChannels;
      const bool pcm = f->wf.wFormatTag == WAVE_FORMAT_PCM &&
                       f->wBitsPerSample == 16 &&
                       (channels == 1 || channels == 2) &&
                       f->wf.nSamplesPerSec >= 4000 &&
                       f->wf.nSamplesPerSec <= 96000;
      if (place || (made && pcm && channels == 1)) {
        kind = kPlace;
      } else if (g_bed && g_play_kind == kAudioAtmos && g_play_flag3d == 0 &&
                 pcm && channels == 2) {
        kind = kBed;
      }
      if (kind >= 0 && (!pcm || bytes < 8 || bytes > kMaxWaveBytes ||
                        !Readable(data, bytes))) {
        InterlockedIncrement(&g_refused_format);
        kind = -1;
      }
      if (kind >= 0) {
        const int frames = static_cast<int>(bytes / (2 * channels));
        Slot* s = TakeSlot(frames, channels);
        if (s == nullptr) {
          InterlockedIncrement(&g_refused_slots);
        } else {
          memcpy(s->data, data, sizeof(int16_t) * frames * channels);
          float wave_rate = static_cast<float>(f->wf.nSamplesPerSec);
          if (c.frequency_play == g_play && c.frequency >= 1000) {
            wave_rate = static_cast<float>(c.frequency);
          }
          Lock lock;
          StartSlot(s, channel, kind, frames, channels, wave_rate, loops);
          s->volume = static_cast<float>(c.volume) / kFullVolume;
          memcpy(s->position, c.point, sizeof(c.point));
          if (!place) {
            // The level that QMixer gives this range with the mapping of
            // the channel. The DLL sends 1, 9999, 4 for these sounds.
            const bool mapped = c.map_play == g_play;
            s->fixed_gain = audio_math::QMixerDistanceGain(
                c.polar_range, mapped ? c.map[0] : 1.0f,
                mapped ? c.map[1] : 9999.0f, mapped ? c.map[2] : 4.0f);
            memcpy(s->position, g_play_place, sizeof(g_play_place));
            InterlockedIncrement(&g_started_atmos);
          } else if (c.map_play == g_play && c.map_positional) {
            memcpy(s->map, c.map, sizeof(c.map));
            s->have_map = true;
          }
          shadow = true;
          InterlockedIncrement(&g_started[kind]);
        }
      }
    }
    // QMixer plays a shadowed channel with no sound. A channel that was
    // shadowed gets its volume back for a sample that QMixer plays.
    if (shadow) {
      c.shadowed = true;
      if (g_audible) {
        o_SetVolume(session, channel, kVolumeFlags, 0);
        c.muted = true;
      }
    } else if (c.muted) {
      o_SetVolume(session, channel, kVolumeFlags, c.volume);
      c.muted = false;
    }
  }
  // No pump of QMixer in this call (audio_play_math.h). The byte is 0 again
  // when the call returns.
  volatile uint8_t* pump_flag = nullptr;
  const bool atmosphere = g_in_play && g_play_kind == kAudioAtmos;
  if (g_pump_known && g_play_fix > 0 && Readable(session, 8) &&
      audio_play::SkipPump(g_play_fix, shadow && g_audible, atmosphere)) {
    const int at = audio_play::SessionFlagOffset(
        static_cast<const uint8_t*>(session));
    if (at >= 0) {
      pump_flag = static_cast<volatile uint8_t*>(session) + at;
      *pump_flag = 1;
    }
  }
  // audio_play_now: the hooks on the sort and the pump read this call on the
  // thread of the play.
  PlayCall call;
  call.defer = g_now_known && g_play_now > 0 &&
               audio_play::SkipPump(g_play_now, shadow && g_audible, atmosphere);
  call.timing = g_parts_on;
  call.atmosphere = atmosphere;
  call.force_loader =
      g_open_known && audio_play::ForceLoader(g_open_mode, atmosphere);
  call.in_open = false;
  call.forced = false;
  call.fmt_result = -1;
  call.wave = wave;
  call.parts.Clear();
  if (call.timing) {
    // QMixer tests the wave with this call first. Its time is a part.
    LARGE_INTEGER b0, b1;
    QueryPerformanceCounter(&b0);
    IsBadWritePtr(wave, 0x48);
    QueryPerformanceCounter(&b1);
    call.parts.Add(audio_play::kPartBadPtr, b1.QuadPart - b0.QuadPart);
  }
  const bool track = call.defer || call.timing || call.force_loader;
  LARGE_INTEGER t0, t1;
  DWORD r;
  {
    // A play that no hook needs to see runs with no call on this thread.
    CallGuard guard(track ? &call : nullptr);
    QueryPerformanceCounter(&t0);
    r = o_PlayEx(session, channel, flags, wave, loops, params);
    QueryPerformanceCounter(&t1);
  }
  if (g_trace) {
    static const trace_events::Def kPlayDef = {"QMixer play (PlayEx)", "audio", {"atmosphere", nullptr, nullptr, nullptr}};
    TraceBar(&kPlayDef, t0.QuadPart, t1.QuadPart, atmosphere ? 1.0f : 0.0f);
  }
  if (pump_flag != nullptr) *pump_flag = 0;
  if (g_pump_known) {
    const bool changed = pump_flag != nullptr || call.defer;
    EnterCriticalSection(&g_play_cs);
    g_play_stat[atmosphere ? 0 : 1][changed ? 1 : 0].Add(
        t1.QuadPart - t0.QuadPart, g_qpc_hz / 200);
    if (changed) ++g_play_changed;
    if (call.timing) {
      PartsGroup& group = g_parts[atmosphere ? 0 : 1];
      ++group.plays;
      for (int i = 0; i < audio_play::kParts; ++i) {
        if (call.parts.runs[i] > 0) {
          group.part[i].AddPlay(call.parts.ticks[i], call.parts.runs[i],
                                g_qpc_hz / 200);
        }
      }
    }
    LeaveCriticalSection(&g_play_cs);
  }
  if (r != 0 && shadow) {
    Lock lock;
    Slot* s = SlotOfChannel(channel);
    if (s != nullptr) EndSlot(s);
    g_channels[channel].shadowed = false;
  }
  return r;
}

// QSWaveMixPump runs on the timer thread of the audio DLL each 20 ms. The mod
// only times it, so that the report shows where the work of the pump goes.
void WINAPI h_Pump() {
  LARGE_INTEGER t0, t1;
  t_in_pump = true;
  TraceNameThread("QMixer timer (pump)", 12);
  QueryPerformanceCounter(&t0);
  o_Pump();
  QueryPerformanceCounter(&t1);
  t_in_pump = false;
  if (g_trace) {
    static const trace_events::Def kPumpDef = {"QSWaveMixPump", "audio", {nullptr, nullptr, nullptr, nullptr}};
    TraceBar(&kPumpDef, t0.QuadPart, t1.QuadPart);
  }
  EnterCriticalSection(&g_play_cs);
  g_pump_stat.Add(t1.QuadPart - t0.QuadPart, g_qpc_hz / 200);
  LeaveCriticalSection(&g_play_cs);
}

DWORD WINAPI h_SetVolume(void* session, int channel, DWORD flags,
                         DWORD volume) {
  g_stop_channel = -1;
  if (g_on && channel >= 0 && channel < kChannels) {
    ChannelState& c = g_channels[channel];
    c.volume = volume;
    c.volume_flags = flags;
    // The first step of a stop of the DLL: the volume 0 with the flags 0.
    // h_StopSleep reads the channel.
    if (volume == 0 && flags == 0) {
      g_stop_channel = channel;
      g_stop_thread = GetCurrentThreadId();
    }
    if (c.shadowed) {
      {
        Lock lock;
        Slot* s = SlotOfChannel(channel);
        if (s != nullptr) {
          s->volume = static_cast<float>(volume) / kFullVolume;
        } else {
          c.shadowed = false;
        }
      }
      if (c.shadowed && g_audible) {
        c.muted = true;
        return o_SetVolume(session, channel, flags, 0);
      }
    }
    c.muted = false;
  }
  return o_SetVolume(session, channel, flags, volume);
}

DWORD WINAPI h_SetFrequency(void* session, int channel, DWORD flags,
                            DWORD frequency) {
  if (g_on && channel >= 0 && channel < kChannels) {
    ChannelState& c = g_channels[channel];
    c.frequency = frequency;
    c.frequency_play = g_in_play ? g_play : -1;
    if (c.shadowed && frequency >= 1000) {
      Lock lock;
      Slot* s = SlotOfChannel(channel);
      if (s != nullptr) s->wave_rate = static_cast<float>(frequency);
    }
  }
  return o_SetFrequency(session, channel, flags, frequency);
}

void StopVoice(int channel) {
  if (!g_on || channel < 0 || channel >= kChannels) return;
  Lock lock;
  ChannelState& c = g_channels[channel];
  // The mod skipped the wait of this stop, so the flush comes 20 ms sooner.
  // The voice gets those 20 ms in its fade: its slot is free at the same
  // time after the volume 0 as with the wait.
  Slot* s = c.stop_skipped ? SlotOfChannel(channel) : nullptr;
  ReleaseChannel(channel, true);
  if (s != nullptr && s->releasing) s->fade = audio_math::ReleaseStart(true);
  c.stop_skipped = false;
  c.shadowed = false;
}

// THE SLEEP OF THE AUDIO DLL. Each Sleep of the DLL comes here. The mod
// skips only the Sleep of a stop of a sample. The channel of that stop must
// be silent in QMixer because a voice of the mod plays its sample. See the
// note at the top of this file.
void WINAPI h_StopSleep(DWORD ms) {
  const uintptr_t site = AudioWatchCaller(reinterpret_cast<uintptr_t>(_ReturnAddress()));
  bool stop = false;
  for (int i = 0; i < kStopSites; ++i) {
    if (site == g_stop_site[i] && site != 0) stop = true;
  }
  if (!stop) {
    o_StopSleep(ms);
    return;
  }
  const int channel =
      g_stop_thread == GetCurrentThreadId() ? g_stop_channel : -1;
  g_stop_channel = -1;
  if (g_stop_wait == 0 && g_on && ms == kStopWaitMs && channel >= 0 &&
      channel < kChannels) {
    ChannelState& c = g_channels[channel];
    if (c.shadowed && c.muted) {
      c.stop_skipped = true;
      InterlockedIncrement(&g_stop_skipped);
      return;
    }
  }
  InterlockedIncrement(&g_stop_kept);
  o_StopSleep(ms);
}

// The hook of the Sleep of the audio DLL, when the DLL is the known one.
void InstallStopWait() {
  g_stop_wait = AudioIni("audio_stop_wait", 0) != 0 ? 1 : 0;
  g_stop_test_frame = AudioIni("audio_stop_wait_test", 0);
  const HMODULE dll = AudioAudioDll();
  const uint8_t* base = reinterpret_cast<const uint8_t*>(dll);
  for (int i = 0; i < kStopSites; ++i) {
    const uint8_t* call =
        base != nullptr ? base + kStopSleepReturn[i] - 8 : nullptr;
    if (call == nullptr || !Readable(call, 8) ||
        memcmp(call, kStopSleepBytes, sizeof(kStopSleepBytes)) != 0) {
      ModLog("audio mixer  : the audio DLL has no call of Sleep(20) at its "
             "RVA 0x%X, so each stop of a sound keeps its wait of 20 ms on "
             "the thread of the game",
             static_cast<unsigned>(kStopSleepReturn[i] - 8));
      return;
    }
  }
  if (!AudioPatchImport(dll, "KERNEL32.dll", "Sleep",
                        reinterpret_cast<void*>(&h_StopSleep),
                        reinterpret_cast<void**>(&o_StopSleep))) {
    ModLog("audio mixer  : the audio DLL has no import of Sleep, so each stop "
           "of a sound keeps its wait of 20 ms on the thread of the game");
    return;
  }
  for (int i = 0; i < kStopSites; ++i) {
    g_stop_site[i] = reinterpret_cast<uintptr_t>(base) + kStopSleepReturn[i];
  }
  g_stop_hooked = true;
  ModLog("audio mixer  : audio_stop_wait=%d. The audio DLL calls Sleep(20) on "
         "the thread of the game when it stops a sound that plays. %s",
         g_stop_wait,
         g_stop_wait == 0
             ? "The mod skips that wait for a sound that the mixer of the mod "
               "plays: QMixer is silent for it, and the voice of the mod "
               "fades the same way. A sound that QMixer plays keeps the wait"
             : "Each stop keeps that wait, as in the game");
}

DWORD WINAPI h_Stop(void* session, int channel, DWORD flags) {
  StopVoice(channel);
  return o_Stop(session, channel, flags);
}

DWORD WINAPI h_Flush(void* session, int channel, DWORD flags) {
  StopVoice(channel);
  return o_Flush(session, channel, flags);
}

void PauseVoice(int channel, bool paused) {
  if (!g_on || channel < 0 || channel >= kChannels) return;
  Lock lock;
  Slot* s = SlotOfChannel(channel);
  if (s != nullptr) s->paused = paused;
}

DWORD WINAPI h_Pause(void* session, int channel, DWORD flags) {
  PauseVoice(channel, true);
  return o_Pause(session, channel, flags);
}

DWORD WINAPI h_Restart(void* session, int channel, DWORD flags) {
  PauseVoice(channel, false);
  return o_Restart(session, channel, flags);
}

DWORD WINAPI h_Activate(void* session, BOOL active) {
  const DWORD r = o_Activate(session, active);
  if (active) AudioCaptureSession(session);
  return r;
}

bool Hook(HMODULE dll, const char* name, void* hook, void** original) {
  void* target = dll == nullptr ? nullptr : GetProcAddress(dll, name);
  if (target == nullptr) {
    ModLog("audio mixer  : %s not found", name);
    return false;
  }
  MH_STATUS s = MH_CreateHook(target, hook, original);
  if (s == MH_OK) s = MH_EnableHook(target);
  if (s != MH_OK) ModLog("audio mixer  : the hook on %s FAILED", name);
  return s == MH_OK;
}

// ---- The test voice --------------------------------------------------------

// TEST: a stereo wave with low noise at the left and high noise at the
// right, so a capture shows which channel is in which ear.
void FillTestBed(int16_t* out, int frames) {
  uint32_t seed = 4242u;
  float low_a = 0.0f, low_b = 0.0f, slow = 0.0f;
  for (int i = 0; i < frames; ++i) {
    seed = seed * 1664525u + 1013904223u;
    const float a =
        static_cast<float>(static_cast<int32_t>(seed)) / 2147483648.0f;
    seed = seed * 1664525u + 1013904223u;
    const float b =
        static_cast<float>(static_cast<int32_t>(seed)) / 2147483648.0f;
    low_a += 0.08f * (a - low_a);
    low_b += 0.08f * (low_a - low_b);
    slow += 0.45f * (b - slow);
    out[2 * i] = static_cast<int16_t>(low_b * 60000.0f);
    out[2 * i + 1] = static_cast<int16_t>((b - slow) * 9000.0f);
  }
}

// Noise with more level at the low end, 2 s, the same in each run.
void FillNoise(int16_t* out, int frames) {
  uint32_t seed = 12345u;
  float low = 0.0f, lower = 0.0f;
  for (int i = 0; i < frames; ++i) {
    seed = seed * 1664525u + 1013904223u;
    const float white =
        static_cast<float>(static_cast<int32_t>(seed)) / 2147483648.0f;
    low += 0.15f * (white - low);
    lower += 0.02f * (white - lower);
    const float v = 0.5f * white + 1.5f * low + 3.0f * lower;
    out[i] = static_cast<int16_t>(v * 9000.0f);
  }
}

void ParseTestPath(const char* text) {
  g_test_count = 0;
  const char* p = text;
  while (*p != 0 && g_test_count < 24) {
    TestStep step = {0.0f, 0.0f, 1.0f, -1.0f};
    const int got = sscanf_s(p, "%f,%f,%f,%f", &step.azimuth, &step.elevation,
                             &step.meters, &step.part);
    if (got >= 2) g_test_steps[g_test_count++] = step;
    // The steps have a slash between them: a run script splits its list of
    // keys at each semicolon.
    p = strpbrk(p, "/;");
    if (p == nullptr) break;
    ++p;
  }
}

// TEST ONLY. A voice of noise that goes through a list of places around the
// head. Each step writes a log line with the frame of the capture, and
// tools/audio_measure.py cuts the capture at these frames.
void TestVoiceFrame() {
  if (g_test_voice == 2 && AudioOutRate() != 0 && g_test_slot < 0 &&
      g_listener.have && TabletopViewScale() > 0.0f) {
    // The test bed: the view turns under it, and the capture shows it.
    const int frames = 44100;
    Slot* s = TakeSlot(frames, 2);
    if (s == nullptr) return;
    FillTestBed(s->data, frames);
    Lock lock;
    StartSlot(s, -1, kBed, frames, 2, 22050.0f, -1);
    s->volume = 1.0f;
    g_test_slot = static_cast<int>(s - g_slots);
    return;
  }
  if (g_test_voice != 1 || g_test_count == 0 || AudioOutRate() == 0) return;
  if (!g_listener.have || !(TabletopViewScale() > 0.0f)) {
    g_test_wait = 0;
    return;
  }
  // The steps count frames of the capture, so each one has its full time in
  // the file. QMixer gives no blocks while no channel plays, and then the
  // test waits.
  const long now = AudioCaptureFrames();
  const bool flows = now != g_test_seen;
  g_test_seen = now;
  if (!flows) return;
  if (++g_test_wait < 200) return;
  const long step_frames =
      static_cast<long>(g_test_step_ms) * AudioOutRate() / 1000;
  if (g_test_slot < 0) {
    const int frames = 44100;
    Slot* s = TakeSlot(frames, 1);
    if (s == nullptr) return;
    FillNoise(s->data, frames);
    Lock lock;
    StartSlot(s, -1, kTest, frames, 1, 22050.0f, -1);
    s->volume = 1.0f;
    g_test_slot = static_cast<int>(s - g_slots);
    g_test_step = -1;
    g_test_frame = now - step_frames;
  }
  if (g_test_step >= g_test_count) return;
  if (now - g_test_frame >= step_frames) {
    g_test_frame = now;
    ++g_test_step;
    if (g_test_step >= g_test_count) {
      Lock lock;
      g_slots[g_test_slot].releasing = true;
      ModLog("audio test   : step %d end, capture frame %ld", g_test_step,
             AudioCaptureFrames());
      return;
    }
    const TestStep& t = g_test_steps[g_test_step];
    ModLog("audio test   : step %d az%+.0f el%+.0f %.2fm part%.2f, capture "
           "frame %ld", g_test_step, t.azimuth, t.elevation, t.meters,
           t.part, AudioCaptureFrames());
  }
  // The place follows the head, so the direction is the one of the step.
  const TestStep& t = g_test_steps[g_test_step];
  const float az = t.azimuth / 57.29578f, el = t.elevation / 57.29578f;
  const float local[3] = {sinf(az) * cosf(el), sinf(el), cosf(az) * cosf(el)};
  Lock lock;
  Slot& s = g_slots[g_test_slot];
  const Listener& l = g_listener;
  const float reach = t.meters * l.units_per_meter;
  for (int i = 0; i < 3; ++i) {
    s.position[i] = l.position[i] +
                    (l.basis[i * 3 + 0] * local[0] +
                     l.basis[i * 3 + 1] * local[1] +
                     l.basis[i * 3 + 2] * local[2]) * reach;
  }
  s.test_part = t.part >= 0.0f ? t.part : 0.0f;
}

// The line of the parts of a play (audio_play_parts): for each part that ran,
// the plays in which it ran, its runs for each play, and its time in one play
// (the sum of its runs). The times include the parts below a part.
void PartsReport(const PartsGroup (&group)[2]) {
  char text[9000];
  size_t n = 0;
  text[0] = 0;
  const char* const names[2] = {"a sound of the atmosphere", "another sound"};
  for (int g = 0; g < 2; ++g) {
    n += snprintf(text + n, sizeof(text) - n, "%s%s: %lld plays", g ? ". " : "",
                  names[g], static_cast<long long>(group[g].plays));
    for (int i = 0; i < audio_play::kParts && n < sizeof(text) - 200; ++i) {
      const audio_play::Stat& s = group[g].part[i];
      if (s.calls == 0) continue;
      n += snprintf(text + n, sizeof(text) - n,
                    ", %s in %lld (%.1f runs, mean %.0f us, most %.0f us, "
                    "%lld of 5 ms or more)",
                    audio_play::kPartInfo[i].name,
                    static_cast<long long>(s.calls),
                    static_cast<double>(s.runs) / static_cast<double>(s.calls),
                    s.MeanUs(g_qpc_hz), s.MostUs(g_qpc_hz),
                    static_cast<long long>(s.long_calls));
    }
  }
  ModLog("audio play   : parts of a play on the frame thread in the last 10 s "
         "(each time includes the parts below it). %s", text);
}

// The line of the pump of QSWaveMixPlayEx with the times of the last 10 s.
// The time of a play is the time of the call of QMixer. A time of 5 ms or more
// is long.
void PlayReport() {
  if (!g_pump_known) return;
  const DWORD now = GetTickCount();
  if (g_play_report_at == 0) g_play_report_at = now;
  if (now - g_play_report_at < 10000) return;
  g_play_report_at = now;
  audio_play::Stat play[2][2], pump, lock;
  PartsGroup parts[2];
  int64_t changed;
  EnterCriticalSection(&g_play_cs);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      play[i][j] = g_play_stat[i][j];
      g_play_stat[i][j] = audio_play::Stat();
    }
  }
  pump = g_pump_stat;
  g_pump_stat = audio_play::Stat();
  lock = g_lock_stat;
  g_lock_stat = audio_play::Stat();
  for (int i = 0; i < 2; ++i) {
    parts[i] = g_parts[i];
    g_parts[i] = PartsGroup();
  }
  changed = g_play_changed;
  LeaveCriticalSection(&g_play_cs);
  if (play[0][0].calls + play[0][1].calls + play[1][0].calls +
          play[1][1].calls + pump.calls == 0) {
    return;
  }
  const int64_t hz = g_qpc_hz;
#define PLAY_TIMES(s) \
  static_cast<long long>((s).calls), (s).MeanUs(hz), (s).MostUs(hz), \
      static_cast<long long>((s).long_calls)
  ModLog("audio play   : audio_play_fix=%d, audio_play_now=%d, %lld plays "
         "have run with no pump since the start (the hooks skipped %ld sorts "
         "and %ld pumps). In the last 10 s, a sound of the atmosphere: %lld "
         "plays with the work of QMixer in the call (mean %.0f us, most %.0f "
         "us, %lld of 5 ms or more) and %lld with no pump (mean %.0f us, "
         "most %.0f us, %lld of 5 ms or more). Another sound: %lld plays with "
         "the work (mean %.0f us, most %.0f us, %lld of 5 ms or more) and "
         "%lld with no pump (mean %.0f us, most %.0f us, %lld of 5 ms or "
         "more). QSWaveMixPump of the timer thread: %lld calls (mean %.0f us, "
         "most %.0f us, %lld of 5 ms or more)",
         g_play_fix, g_play_now, static_cast<long long>(changed),
         g_defer_sort, g_defer_pump,
         PLAY_TIMES(play[0][0]), PLAY_TIMES(play[0][1]),
         PLAY_TIMES(play[1][0]), PLAY_TIMES(play[1][1]), PLAY_TIMES(pump));
#undef PLAY_TIMES
  if (g_open_known) {
    ModLog("audio play   : audio_open_loader=%d. %ld opens went to the loader "
           "thread because of it since the start%s", g_open_mode, g_open_forced,
           g_parts_on ? "" : ". Start with audio_play_parts=1 to count the "
                             "paths");
  }
  if (g_parts_on) {
    PartsReport(parts);
    ModLog("audio play   : opens in the last 10 s (audio_play_parts). A sound "
           "of the atmosphere: %ld at once, %ld on the loader thread. Another "
           "sound: %ld at once, %ld on the loader thread. The lock of the "
           "session in QSWaveMixPump on the timer thread: %lld calls (mean "
           "%.0f us, most %.0f us, %lld of 5 ms or more)",
           InterlockedExchange(&g_open_path[0][0], 0),
           InterlockedExchange(&g_open_path[0][1], 0),
           InterlockedExchange(&g_open_path[1][0], 0),
           InterlockedExchange(&g_open_path[1][1], 0),
           static_cast<long long>(lock.calls), lock.MeanUs(hz),
           lock.MostUs(hz), static_cast<long long>(lock.long_calls));
  }
}

void Report() {
  const DWORD now = GetTickCount();
  if (g_last_report == 0) g_last_report = now;
  if (now - g_last_report < 30000) return;
  g_last_report = now;
  const LONG total = g_started[kPlace] + g_started[kBed];
  if (total == g_reported) return;
  g_reported = total;
  const LONG calls = InterlockedExchange(&g_render_calls, 0);
  const double mean = calls > 0 ? g_render_us / calls : 0.0;
  ModLog("audio mixer  : %ld sounds with a place (%ld of them single "
         "sounds of the atmosphere) and %ld beds of the "
         "atmosphere went through the mixer of the mod. %ld stayed with "
         "QMixer for their format and %ld for want of a slot. At most %d "
         "voices at one time. A block took %.0f us on average and %.0f us "
         "at most. %ld samples were over the knee of the limiter",
         g_started[kPlace], g_started_atmos, g_started[kBed],
         g_refused_format,
         g_refused_slots, g_most_voices, mean, g_render_worst_us, g_clipped);
  g_render_us = 0.0;
  g_render_worst_us = 0.0;
  if (g_stop_hooked && g_stop_skipped + g_stop_kept != g_stop_reported) {
    g_stop_reported = g_stop_skipped + g_stop_kept;
    ModLog("audio mixer  : the audio DLL stopped %ld sounds that played. The "
           "mod skipped the wait of 20 ms for %ld of them, and %ld kept it "
           "(audio_stop_wait=%d)", g_stop_reported, g_stop_skipped,
           g_stop_kept, g_stop_wait);
  }
  float lead = 0.0f, most = 0.0f, least = 0.0f;
  long passed = 0;
  if (AudioCaptureLead(&lead, &most, &least, &passed)) {
    ModLog("audio mixer  : QMixer writes the stream %.0f ms ahead of the "
           "device on average (%.0f to %.0f ms). A turn of the head is heard "
           "that much later. The device passed a write %ld times (a gap in "
           "the sound)", lead, least, most, passed);
  }
}

// audio_play_now, audio_open_loader and audio_play_parts. The hooks on the sort
// and the pump go in when the mode is above 0 or the test key can change it.
// The hook on the format test goes in for audio_open_loader. The hooks that
// time the other functions go in with audio_play_parts=1. Each function is
// tested for its first 8 bytes, so another QMixer.dll gets no hook. Each hook
// is queued and one MH_ApplyQueued writes them all, with one stop of the other
// threads. The system calls get a timer by the import table, with no hook.
void InstallPlayNow(const uint8_t* base) {
  g_qm_base = base;
  const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const IMAGE_NT_HEADERS* nt =
      reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  g_qm_size = nt->OptionalHeader.SizeOfImage;
  g_play_now = audio_play::ClampMode(AudioIni("audio_play_now", 0));
  g_play_now_test_frame = AudioIni("audio_play_now_test", 0);
  g_open_mode = audio_play::ClampOpenMode(AudioIni("audio_open_loader", 0));
  g_open_test_frame = AudioIni("audio_open_loader_test", 0);
  g_parts_on = AudioIni("audio_play_parts", 0) != 0;
  const bool test_keys = AudioIni("test_keys", 0) != 0;
  const bool deferring = g_play_now > 0 || test_keys || g_play_now_test_frame > 0;
  const bool opening =
      g_open_mode > 0 || test_keys || g_open_test_frame > 0 || g_parts_on;
  const uint8_t* queued[audio_play::kPartCodeEnd] = {};
  int hooked = 0, failed = 0;
  for (int i = 0; i < audio_play::kPartCodeEnd; ++i) {
    const audio_play::PartInfo& info = audio_play::kPartInfo[i];
    const bool defer_part =
        i == audio_play::kPartSort || i == audio_play::kPartPump;
    const bool wanted = g_parts_on || (deferring && defer_part) ||
                        (opening && i == audio_play::kPartFormat);
    if (info.rva == 0 || !wanted) continue;
    const uint8_t* at = base + info.rva;
    if (!Readable(at, 8) || !audio_play::PartMatches(i, at, 8)) {
      ModLog("audio play   : the part %s at QMixer RVA 0x%X has other bytes, "
             "so it gets no hook", info.name, static_cast<unsigned>(info.rva));
      ++failed;
      continue;
    }
    uint8_t* target = const_cast<uint8_t*>(at);
    if (MH_CreateHook(target, kPartHook[i], &o_part[i]) != MH_OK) {
      ModLog("audio play   : the hook on the part %s FAILED", info.name);
      o_part[i] = nullptr;
      ++failed;
      continue;
    }
    if (MH_QueueEnableHook(target) != MH_OK) {
      MH_RemoveHook(target);
      o_part[i] = nullptr;
      ModLog("audio play   : the hook on the part %s FAILED", info.name);
      ++failed;
      continue;
    }
    queued[i] = at;
    ++hooked;
  }
  if (hooked > 0 && MH_ApplyQueued() != MH_OK) {
    ModLog("audio play   : MH_ApplyQueued FAILED, so no hook on QMixer is in");
    for (int i = 0; i < audio_play::kPartCodeEnd; ++i) {
      if (queued[i] == nullptr) continue;
      MH_RemoveHook(const_cast<uint8_t*>(queued[i]));
      o_part[i] = nullptr;
      queued[i] = nullptr;
    }
    failed += hooked;
    hooked = 0;
  }
  for (int i = 0; i < audio_play::kPartCodeEnd; ++i) {
    g_part_hooked[i] = queued[i] != nullptr;
  }
  g_now_known = g_part_hooked[audio_play::kPartSort] &&
                g_part_hooked[audio_play::kPartPump];
  g_open_known = g_part_hooked[audio_play::kPartFormat];
  int imports = 0;
  if (g_parts_on) imports = PatchImports(AudioMixerDll());
  if (g_play_now > 0 && !g_now_known) {
    ModLog("audio play   : audio_play_now=%d, but the hooks on the sort and "
           "the pump of QMixer are not in. QMixer does its work in each play "
           "as in the game", g_play_now);
    g_play_now = 0;
  }
  if (g_open_mode > 0 && !g_open_known) {
    ModLog("audio play   : audio_open_loader=%d, but the hook on the format "
           "test of QMixer is not in. QMixer opens each wave as in the game",
           g_open_mode);
    g_open_mode = 0;
  }
  ModLog("audio play   : audio_play_now=%d (%s), audio_play_parts=%d. %d "
         "functions of QMixer have a hook, %d failed, %d entries of the import "
         "table have a timer. %s", g_play_now,
         g_play_now == 0 ? "the work of QMixer runs in the call, as in the game"
         : g_play_now == 1
             ? "the sort and the pump return at once in the call of a sound "
               "that the mixer of the mod plays"
         : g_play_now == 2
             ? "the sort and the pump return at once in the call of a sound "
               "of the mixer of the mod and of each sound of the atmosphere"
             : "the sort and the pump return at once in each call (test "
               "value)",
         g_parts_on ? 1 : 0, hooked, failed, imports,
         g_play_now > 0
             ? "The pump of the timer thread does the work within 20 ms. The "
               "session is not changed"
             : "");
  ModLog("audio play   : audio_open_loader=%d (%s)", g_open_mode,
         g_open_mode == 0
             ? "the open of a wave runs as QMixer has it"
         : g_open_mode == 1
             ? "the open of the wave of a sound of the atmosphere goes to the "
               "loader thread of QMixer"
             : "the open of each wave goes to the loader thread of QMixer (test "
               "value)");
}

// The mode of audio_play_fix, the check of the code of the known QMixer.dll
// and the timer on QSWaveMixPump. With another QMixer.dll the pump stays as
// the game has it.
void InstallPlayFix(HMODULE q) {
  g_play_fix = audio_play::ClampMode(AudioIni("audio_play_fix", 0));
  g_play_test_frame = AudioIni("audio_play_fix_test", 0);
  const uint8_t* base = reinterpret_cast<const uint8_t*>(q);
  const uint8_t* play =
      reinterpret_cast<const uint8_t*>(GetProcAddress(q, "QSWaveMixPlayEx"));
  const uint8_t* test = base + audio_play::kPumpTestRva;
  g_pump_known = base != nullptr && play == base + audio_play::kPlayExRva &&
                 Readable(test, sizeof(audio_play::kPumpTest)) &&
                 audio_play::PumpTestMatches(test,
                                             sizeof(audio_play::kPumpTest));
  if (!g_pump_known) {
    ModLog("audio play   : audio_play_fix=%d, but QMixer.dll is not the "
           "known one (QSWaveMixPlayEx or the test of the pump has another "
           "place or other bytes). QMixer pumps in each QSWaveMixPlayEx as in "
           "the game", g_play_fix);
    g_play_fix = 0;
    return;
  }
  LARGE_INTEGER hz;
  QueryPerformanceFrequency(&hz);
  g_qpc_hz = hz.QuadPart;
  Hook(q, "QSWaveMixPump", reinterpret_cast<void*>(&h_Pump),
       reinterpret_cast<void**>(&o_Pump));
  InstallPlayNow(base);
  ModLog("audio play   : audio_play_fix=%d. QSWaveMixPlayEx of QMixer runs one "
         "pump on the thread of the caller (it sorts the channels, gives a "
         "voice to the new one and mixes the stream ahead). The mode sets the "
         "byte at +5 of the session for the time of the call. %s", g_play_fix,
         g_play_fix == 0
             ? "The mod leaves it so"
         : g_play_fix == 1
             ? "The mod keeps the pump out of the call for a sound that the "
               "mixer of the mod plays: QMixer is silent for it. The pump of "
               "the timer thread does the work within 20 ms"
         : g_play_fix == 2
             ? "The mod keeps the pump out of the call for a sound that the "
               "mixer of the mod plays and for each sound of the atmosphere. "
               "The pump of the timer thread does the work within 20 ms"
             : "The mod keeps the pump out of each call (test value). The "
               "pump of the timer thread does the work within 20 ms");
}

}  // namespace

bool AudioVoicesInstall(bool on) {
  HMODULE q = AudioMixerDll();
  const bool activate = Hook(q, "QSWaveMixActivate",
                             reinterpret_cast<void*>(&h_Activate),
                             reinterpret_cast<void**>(&o_Activate));
  if (!on) {
    ModLog("audio mixer  : audio_mixer=0, QMixer of the game pans each sound");
    return activate;
  }
  InitializeCriticalSection(&g_lock);
  InitializeCriticalSection(&g_play_cs);
  for (int i = 0; i < kChannels; ++i) {
    g_channels[i].volume = static_cast<DWORD>(kFullVolume);
    g_channels[i].volume_flags = kVolumeFlags;
    g_channels[i].point_play = -1;
    g_channels[i].map_play = -1;
    g_channels[i].frequency_play = -1;
  }
  bool ok = true;
#define HOOK(name, hook, original) \
  ok &= Hook(q, name, reinterpret_cast<void*>(&hook), \
             reinterpret_cast<void**>(&original))
  HOOK("QSWaveMixSetVolume", h_SetVolume, o_SetVolume);
  HOOK("QSWaveMixSetFrequency", h_SetFrequency, o_SetFrequency);
  HOOK("QSWaveMixStopChannel", h_Stop, o_Stop);
  HOOK("QSWaveMixPauseChannel", h_Pause, o_Pause);
  HOOK("QSWaveMixRestartChannel", h_Restart, o_Restart);
  HOOK("QSWaveMixFlushChannel", h_Flush, o_Flush);
  if (ok) HOOK("QSWaveMixPlayEx", h_PlayEx, o_PlayEx);
#undef HOOK
  f_IsDone = reinterpret_cast<IsDoneFn>(
      GetProcAddress(q, "QSWaveMixIsChannelDone"));
  g_height = AudioIni("audio_height", 1) != 0;
  g_height_pct = sp::Clamp(AudioIni("audio_height_pct", 100) / 100.0f, 0.0f,
                           2.0f);
  g_near = AudioIni("audio_near", 1) != 0;
  g_far = AudioIni("audio_far", 1) != 0;
  g_bed = AudioIni("audio_bed", 1) != 0;
  g_atmos = AudioIni("audio_atmos_mixer", 1) != 0;
  g_solo = AudioIni("audio_test_solo", 0);
  g_test_voice = AudioIni("audio_test_voice", 0);
  g_test_step_ms = AudioIni("audio_test_step_ms", 3000);
  char path[512];
  AudioIniText("audio_test_path", path, sizeof(path));
  ParseTestPath(path);
  g_on = ok;
  if (ok) AudioCaptureSetRender(&Render);
  if (ok) InstallStopWait();
  if (ok) InstallPlayFix(q);
  ModLog("audio mixer  : %s. Height %s, near field %s, far filter %s, the "
         "bed turns with the world %s",
         ok ? "the mixer of the mod plays the sounds with a place "
              "(audio_mixer=1)"
            : "a hook is missing, so QMixer of the game pans each sound",
         g_height ? "on" : "off", g_near ? "on" : "off",
         g_far ? "on" : "off", g_bed ? "on" : "off");
  if (g_test_voice != 0) {
    ModLog("audio test   : TEST, a voice of noise goes through %d places, "
           "%d ms each, solo %d", g_test_count, g_test_step_ms, g_solo);
  }
  return ok;
}

void AudioVoicesPlayBegin(int kind, const void* options) {
  ++g_play;
  g_in_play = true;
  g_play_kind = kind;
  g_play_flag3d = 0;
  g_play_relative = 0;
  g_play_has_place = false;
  const uint8_t* o = static_cast<const uint8_t*>(options);
  if (Readable(o, 0x24)) {
    g_play_flag3d = *reinterpret_cast<const int*>(o + 0x08);
    g_play_relative = *reinterpret_cast<const int*>(o + 0x14);
  }
}

void AudioVoicesPlayEnd() { g_in_play = false; }

void AudioVoicesPlayPlace(const float point[3]) {
  memcpy(g_play_place, point, sizeof(g_play_place));
  g_play_has_place = true;
}

void AudioVoicesPolar(int channel, const float polar[3]) {
  if (!g_on || !g_in_play || channel < 0 || channel >= kChannels) return;
  g_channels[channel].polar_range = polar[1];
  g_channels[channel].polar_play = g_play;
}

void AudioVoicesSource(int channel, const float point[3]) {
  if (!g_on || channel < 0 || channel >= kChannels) return;
  ChannelState& c = g_channels[channel];
  memcpy(c.point, point, sizeof(c.point));
  if (g_in_play) c.point_play = g_play;
  if (!c.shadowed) return;
  Lock lock;
  Slot* s = SlotOfChannel(channel);
  if (s != nullptr) memcpy(s->position, point, sizeof(s->position));
}

void AudioVoicesMapping(int channel, const float game[3], bool positional) {
  if (!g_on || channel < 0 || channel >= kChannels) return;
  ChannelState& c = g_channels[channel];
  memcpy(c.map, game, sizeof(c.map));
  c.map_positional = positional;
  c.map_play = g_in_play ? g_play : -1;
}

void AudioVoicesListener(const float position[3], const float forward[3],
                         const float up[3]) {
  if (!g_on) return;
  // right = up x forward, for the left handed world of the game with y up.
  const float right[3] = {up[1] * forward[2] - up[2] * forward[1],
                          up[2] * forward[0] - up[0] * forward[2],
                          up[0] * forward[1] - up[1] * forward[0]};
  Lock lock;
  Listener& l = g_listener;
  for (int i = 0; i < 3; ++i) {
    l.position[i] = position[i];
    l.basis[i * 3 + 0] = right[i];
    l.basis[i * 3 + 1] = up[i];
    l.basis[i * 3 + 2] = forward[i];
  }
  l.have = true;
}

void AudioVoicesFrame() {
  if (!g_on) return;
  // The test audio_stop_wait_test=N: the test key acts at one frame.
  if (g_stop_test_frame > 0) {
    const uint64_t frame = ControllersFrameCount();
    const uint64_t at = static_cast<uint64_t>(g_stop_test_frame);
    if (frame != g_stop_test_seen) {
      g_stop_test_seen = frame;
      if (ControllersTestFrame(at, frame) == at) AudioStopWaitToggle();
    }
  }
  // The test audio_play_fix_test=N: the test key acts at one frame.
  if (g_play_test_frame > 0) {
    const uint64_t frame = ControllersFrameCount();
    const uint64_t at = static_cast<uint64_t>(g_play_test_frame);
    if (frame != g_play_test_seen) {
      g_play_test_seen = frame;
      if (ControllersTestFrame(at, frame) == at) AudioPlayFixCycle();
    }
  }
  // The test audio_play_now_test=N: the same for audio_play_now.
  if (g_play_now_test_frame > 0) {
    const uint64_t frame = ControllersFrameCount();
    const uint64_t at = static_cast<uint64_t>(g_play_now_test_frame);
    if (frame != g_play_now_test_seen) {
      g_play_now_test_seen = frame;
      if (ControllersTestFrame(at, frame) == at) AudioPlayNowCycle();
    }
  }
  // The test audio_open_loader_test=N: the same for audio_open_loader.
  if (g_open_test_frame > 0) {
    const uint64_t frame = ControllersFrameCount();
    const uint64_t at = static_cast<uint64_t>(g_open_test_frame);
    if (frame != g_open_test_seen) {
      g_open_test_seen = frame;
      if (ControllersTestFrame(at, frame) == at) AudioOpenLoaderCycle();
    }
  }
  float k = 1.0f;
  const bool room = AudioRoomNow(&k);
  if (!room) k = 1.0f;
  const float units = UnitsPerMeter();
  {
    Lock lock;
    g_listener.k = k;
    g_listener.room = room;
    g_listener.units_per_meter = units;
    // A buffer that is much larger than the common sample goes back when its
    // voice has ended. So the memory of the mixer does not grow.
    for (int i = 0; i < kSlots; ++i) {
      Slot& s = g_slots[i];
      if (!s.used && s.data != nullptr && s.capacity > kKeepSamples &&
          i != g_test_slot) {
        free(s.data);
        s.data = nullptr;
        s.capacity = 0;
      }
    }
  }
  // QMIXER IS THE CLOCK, also for a loop. A loop of the mod has no end of
  // its own. If QMixer ended its channel with no stop call that the hooks
  // saw (a new session, a land that loads), the voice must end too, or it
  // plays without end. Two looks in a row must agree, because a channel is
  // empty for a moment between two plays.
  const DWORD tick = GetTickCount();
  if (f_IsDone != nullptr && g_session != nullptr &&
      tick - g_last_watch >= 500) {
    g_last_watch = tick;
    for (int channel = 0; channel < kChannels; ++channel) {
      ChannelState& c = g_channels[channel];
      if (!c.shadowed) {
        c.done_looks = 0;
        continue;
      }
      if (f_IsDone(g_session, channel) == FALSE) {
        c.done_looks = 0;
        continue;
      }
      if (++c.done_looks < 2) continue;
      c.done_looks = 0;
      bool loop = false;
      {
        Lock lock;
        Slot* s = SlotOfChannel(channel);
        if (s != nullptr && s->loops_left != 0) {
          loop = true;
          ReleaseChannel(channel, false);
        }
        if (s == nullptr || loop) c.shadowed = false;
      }
      if (loop && g_watch_lines++ < 20) {
        ModLog("audio mixer  : QMixer ended channel %d with no stop call, so "
               "the loop of the mod on it stops too", channel);
      }
    }
  }
  if (!g_first_logged && g_started[kPlace] + g_started[kBed] > 0) {
    g_first_logged = true;
    ModLog("audio mixer  : the first voice of the mod plays (the stream is "
           "%d Hz)", AudioOutRate());
  }
  TestVoiceFrame();
  PlayReport();
  Report();
}

// A cue of audio_cues.cpp: a short mono wave of 22050 Hz, flat, with a pan
// from -1 (left) to 1 (right). False when the mixer cannot play it now.
bool AudioVoicesCue(const int16_t* data, int frames, float gain, float pan) {
  if (!g_on || !g_audible || data == nullptr || frames <= 0 ||
      AudioOutRate() == 0) {
    return false;
  }
  Slot* s = TakeSlot(frames, 1);
  if (s == nullptr) return false;
  memcpy(s->data, data, sizeof(int16_t) * frames);
  const float angle = (sp::Clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.25f * sp::kPi;
  Lock lock;
  StartSlot(s, -1, kCue, frames, 1, 22050.0f, 0);
  s->volume = 1.0f;
  s->pan_left = gain * cosf(angle) * 1.41421f;
  s->pan_right = gain * sinf(angle) * 1.41421f;
  InterlockedIncrement(&g_started[kCue]);
  return true;
}

// The test key of audio_stop_wait (Ctrl and Pause, test_keys=1). The wait of
// the audio DLL at the stop of a sound goes off and on while the game runs.
// A long pulse says that the mod skips the wait. A short pulse says that
// each stop has it.
void AudioStopWaitToggle() {
  if (!g_on || !g_stop_hooked) {
    ModLog("audio mixer  : test key, but the mod has no hook on the wait of a "
           "stop (audio_mixer=0, or the audio DLL is not the known one)");
    return;
  }
  g_stop_wait = g_stop_wait == 0 ? 1 : 0;
  ControllersGesturePulse(0.5f, g_stop_wait == 0 ? 0.30f : 0.08f);
  ModLog("audio mixer  : test key, audio_stop_wait is %d now: %s. So far the "
         "mod skipped %ld waits, and %ld stops kept the wait", g_stop_wait,
         g_stop_wait == 0
             ? "the mod skips the wait of 20 ms at the stop of a sound that "
               "the mixer of the mod plays"
             : "each stop of a sound holds the thread of the game for 20 ms, "
               "as in the game",
         g_stop_skipped, g_stop_kept);
}

// The value of audio_stop_wait for the debug page.
int AudioStopWaitValue() {
  if (!g_on || !g_stop_hooked) return kDebugCannot;
  return g_stop_wait;
}

// The test key of audio_play_fix (Ctrl, Shift and P, test_keys=1). The mode
// steps through 0, 1 and 2. A short pulse is 0, a longer one is 1, the
// longest is 2.
void AudioPlayFixCycle() {
  if (!g_on || !g_pump_known) {
    ModLog("audio play   : test key, but the mod has no hook on "
           "QSWaveMixPlayEx or QMixer.dll is not the known one");
    return;
  }
  g_play_fix = audio_play::NextMode(g_play_fix);
  ControllersGesturePulse(0.5f, g_play_fix == 0 ? 0.08f
                                : g_play_fix == 1 ? 0.30f
                                                  : 0.55f);
  ModLog("audio play   : test key, audio_play_fix is %d now. %lld plays have "
         "run with no pump so far", g_play_fix,
         static_cast<long long>(g_play_changed));
}

// The value of audio_play_fix for the debug page.
int AudioPlayFixValue() {
  if (!g_on || !g_pump_known) return kDebugCannot;
  return g_play_fix > 2 ? 2 : g_play_fix;
}

// The test key of audio_play_now (Ctrl, Shift and K, test_keys=1). The mode
// steps through 0, 1 and 2. A short pulse is 0, a longer one is 1, the
// longest is 2.
void AudioPlayNowCycle() {
  if (!g_on || !g_now_known) {
    ModLog("audio play   : test key, but the hooks on the sort and the pump "
           "of QMixer are not in. Start with test_keys=1, or with "
           "audio_play_now above 0");
    return;
  }
  g_play_now = audio_play::NextMode(g_play_now);
  ControllersGesturePulse(0.5f, g_play_now == 0 ? 0.08f
                                : g_play_now == 1 ? 0.30f
                                                  : 0.55f);
  ModLog("audio play   : test key, audio_play_now is %d now. The hooks have "
         "skipped %ld sorts and %ld pumps so far", g_play_now, g_defer_sort,
         g_defer_pump);
}

// The value of audio_play_now for the debug page.
int AudioPlayNowValue() {
  if (!g_on || !g_now_known) return kDebugCannot;
  return g_play_now > 2 ? 2 : g_play_now;
}

// The test key of audio_open_loader (Ctrl, Shift and F6, test_keys=1). The
// mode steps through 0, 1 and 2. A short pulse is 0, a longer one is 1, the
// longest is 2.
void AudioOpenLoaderCycle() {
  if (!g_on || !g_open_known) {
    ModLog("audio play   : test key, but the hook on the format test of "
           "QMixer is not in. Start with test_keys=1, or with "
           "audio_open_loader above 0");
    return;
  }
  g_open_mode = audio_play::NextOpenMode(g_open_mode);
  ControllersGesturePulse(0.5f, g_open_mode == 0 ? 0.08f
                                : g_open_mode == 1 ? 0.30f
                                                   : 0.55f);
  ModLog("audio play   : test key, audio_open_loader is %d now. %ld opens "
         "went to the loader thread so far", g_open_mode, g_open_forced);
}

// The value of audio_open_loader for the debug page.
int AudioOpenLoaderValue() {
  if (!g_on || !g_open_known) return kDebugCannot;
  return g_open_mode;
}

// The test key. false gives each shadowed channel back to QMixer, at once,
// and the voices of the mod keep their time with no sound.
void AudioVoicesAudible(bool audible) {
  if (!g_on || audible == g_audible) return;
  g_audible = audible;
  if (g_session == nullptr) return;
  for (int i = 0; i < kChannels; ++i) {
    ChannelState& c = g_channels[i];
    if (!c.shadowed) continue;
    o_SetVolume(g_session, i, kVolumeFlags, audible ? 0 : c.volume);
    c.muted = audible;
  }
}
