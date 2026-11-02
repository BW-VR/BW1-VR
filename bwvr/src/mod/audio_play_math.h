// The pump in QSWaveMixPlayEx (audio_play_fix, audio_voices.cpp). Pure math,
// with checks in test/controller_math/audio_play_checks.h.
//
// After it has started a sample, QSWaveMixPlayEx of QMixer.dll runs one pump
// on the thread of the caller, unless bit 1 of the flags of QSWaveMixInitEx
// was set. The pump sorts the channels, gives a voice to a new channel and
// mixes the stream ahead of the device. QSWaveMixPump does the same, and the
// audio DLL calls it each 20 ms from a timer thread of winmm. The game thread
// of the Steam Frame waits 26 to 54 ms in that pump for a sound of the
// atmosphere.
//
// QMixer keeps bit 1 of the flags in the byte at +5 of the session. The mod
// sets that byte for the time of one QSWaveMixPlayEx call. Then the pump of
// the timer thread does the work a little later, and the game thread goes on.
//
// The mode (audio_play_fix):
//   0  no change: the pump runs in each QSWaveMixPlayEx
//   1  no pump in the call for a channel that the mixer of the mod plays.
//      QMixer is silent for it, so the sound is the same
//   2  also no pump for each sound of the atmosphere that QMixer plays
//   3  no pump in any QSWaveMixPlayEx (a test value)
//
// audio_play_now uses the same modes, and it does not touch the session. The
// mod hooks the pump (RVA 0xA310) and the sort of the channels (RVA 0x67E0).
// The hooks return at once when the frame thread is inside a play that the
// mode defers. The timer thread runs both functions in full, so the work
// comes up to 20 ms later. QMixer also runs the sort in the clear of the
// queue (flag 1 of the play), whatever the byte at +5 says.
#pragma once

#include <stdint.h>
#include <string.h>

namespace audio_play {

const int kModes = 4;

inline int ClampMode(int mode) { return mode < 0 ? 0 : (mode >= kModes ? kModes - 1 : mode); }

// The test key steps through 0, 1 and 2. A 3 goes to 0.
inline int NextMode(int mode) { return (mode >= 0 && mode < 2) ? mode + 1 : 0; }

// Whether the call runs with no pump. `mod_plays` is true for a channel that
// the mixer of the mod plays while QMixer is silent for it.
inline bool SkipPump(int mode, bool mod_plays, bool atmosphere) {
  switch (mode) {
    case 1: return mod_plays;
    case 2: return mod_plays || atmosphere;
    case 3: return true;
    default: return false;
  }
}

// ---- The code and the session of the known QMixer.dll ----------------------

// The RVA of QSWaveMixPlayEx in the known QMixer.dll.
const uint32_t kPlayExRva = 0x37B0;
// At this RVA the function that adds a play to the session tests the byte at
// +5 of the session and calls the pump when it is 0:
//   mov al, [esi+5]  test al, al  jnz +7  mov ecx, esi  call pump
const uint32_t kPumpTestRva = 0x60CC;
const uint8_t kPumpTest[14] = {0x8A, 0x46, 0x05, 0x84, 0xC0, 0x75, 0x07,
                               0x8B, 0xCE, 0xE8, 0x36, 0x42, 0x00, 0x00};

inline bool PumpTestMatches(const uint8_t* code, size_t size) {
  return code != nullptr && size >= sizeof(kPumpTest) &&
         memcmp(code, kPumpTest, sizeof(kPumpTest)) == 0;
}

// The first bytes of a session: the word 0x5432, a byte for the active state,
// a byte, the byte for the thread, and the byte at +5 that is bit 1 of the
// flags of QSWaveMixInitEx.
const int kSessionFlagOffset = 5;

// The offset of the byte to set, or -1 when the session is not one that the
// mod knows: the word is wrong, or the byte is not 0 (the pump is off already,
// or another caller has set it).
inline int SessionFlagOffset(const uint8_t* session8) {
  if (session8 == nullptr) return -1;
  if (session8[0] != 0x32 || session8[1] != 0x54) return -1;
  if (session8[kSessionFlagOffset] != 0) return -1;
  return kSessionFlagOffset;
}

// ---- The parts of a play (audio_play_parts) --------------------------------

// The functions of QMixer that one play runs on the thread of the caller.
// Each is timed from entry to return, so a part includes the parts below it.
// The names are the own names of the mod.
enum Part {
  kPartBadPtr = 0,  // IsBadWritePtr(wave, 0x48), timed in the hook of the play
  kPartCtor,        // RVA 0xBFF0, the constructor of the play object
  kPartParams,      // RVA 0xC1A0, the parameters of the play
  kPartQueue,       // RVA 0x5FF0, the play goes to the session
  kPartOpen,        // RVA 0xC230, the check of the wave and its open
  kPartAdd,         // RVA 0x60F0, the play goes to the channel
  kPartClear,       // RVA 0x7580, the clear of the queue of the channel
  kPartAppend,      // RVA 0x7660, the play goes into the queue of the channel
  kPartSort,        // RVA 0x67E0, the sort of the channels and the voices
  kPartPump,        // RVA 0xA310, the pump
  kPartVoice,       // RVA 0x72E0, the voice of a channel goes on or off
  kParts
};

struct PartInfo {
  const char* name;
  uint32_t rva;      // 0 for a part with no hook
  uint8_t bytes[8];  // the first bytes of the function in the known dll
};

const PartInfo kPartInfo[kParts] = {
    {"bad_ptr", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"ctor", 0xBFF0, {0x56, 0x8B, 0xF1, 0xE8, 0xD8, 0xCF, 0x01, 0x00}},
    {"params", 0xC1A0, {0x83, 0xEC, 0x2C, 0x56, 0x8B, 0x74, 0x24, 0x34}},
    {"queue", 0x5FF0, {0xA1, 0xC0, 0x77, 0x04, 0x18, 0x53, 0x8B, 0x5C}},
    {"open", 0xC230, {0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x40, 0x85, 0xC9}},
    {"add", 0x60F0, {0x55, 0x56, 0x8B, 0x74, 0x24, 0x0C, 0x57, 0x8B}},
    {"clear", 0x7580, {0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x68, 0x01, 0x00}},
    {"append", 0x7660, {0x83, 0xEC, 0x2C, 0x53, 0x56, 0x8B, 0xF1, 0x8B}},
    {"sort", 0x67E0, {0x83, 0xEC, 0x14, 0x53, 0x8B, 0xD9, 0x55, 0x56}},
    {"pump", 0xA310, {0x56, 0x8B, 0xF1, 0xE8, 0xC8, 0xC4, 0xFF, 0xFF}},
    {"voice", 0x72E0, {0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x34, 0x8A}},
};

// Whether `code` (at least 8 bytes) is the start of the function of a part.
inline bool PartMatches(int part, const uint8_t* code, size_t size) {
  if (part < 0 || part >= kParts || kPartInfo[part].rva == 0) return false;
  return code != nullptr && size >= 8 &&
         memcmp(code, kPartInfo[part].bytes, 8) == 0;
}

// The time of the parts in one play: the ticks and the number of runs of each
// part. A part can run more than once in a play.
struct PlayParts {
  int64_t ticks[kParts];
  int32_t runs[kParts];
  void Clear() {
    for (int i = 0; i < kParts; ++i) {
      ticks[i] = 0;
      runs[i] = 0;
    }
  }
  void Add(int part, int64_t t) {
    if (part < 0 || part >= kParts) return;
    ticks[part] += t < 0 ? 0 : t;
    ++runs[part];
  }
};

// ---- The time of the calls -------------------------------------------------

// Integer ticks of QueryPerformanceCounter. The conversion to us is for the
// line only.
struct Stat {
  int64_t calls = 0;
  int64_t sum = 0;
  int64_t most = 0;
  int64_t long_calls = 0;  // calls of `long_ticks` or more
  int64_t runs = 0;        // runs of a part in these calls (AddPlay)

  void Add(int64_t ticks, int64_t long_ticks) {
    if (ticks < 0) ticks = 0;
    ++calls;
    sum += ticks;
    if (ticks > most) most = ticks;
    if (ticks >= long_ticks) ++long_calls;
  }
  // One play with `play_runs` runs of a part and `ticks` in all.
  void AddPlay(int64_t ticks, int32_t play_runs, int64_t long_ticks) {
    Add(ticks, long_ticks);
    runs += play_runs;
  }
  double MeanUs(int64_t frequency) const {
    return (calls > 0 && frequency > 0)
               ? 1e6 * static_cast<double>(sum) / (static_cast<double>(calls) * static_cast<double>(frequency))
               : 0.0;
  }
  double MostUs(int64_t frequency) const {
    return frequency > 0 ? 1e6 * static_cast<double>(most) / static_cast<double>(frequency) : 0.0;
  }
};

}  // namespace audio_play
