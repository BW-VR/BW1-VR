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

// ---- The time of the calls -------------------------------------------------

// Integer ticks of QueryPerformanceCounter. The conversion to us is for the
// line only.
struct Stat {
  int64_t calls = 0;
  int64_t sum = 0;
  int64_t most = 0;
  int64_t long_calls = 0;  // calls of `long_ticks` or more

  void Add(int64_t ticks, int64_t long_ticks) {
    if (ticks < 0) ticks = 0;
    ++calls;
    sum += ticks;
    if (ticks > most) most = ticks;
    if (ticks >= long_ticks) ++long_calls;
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
