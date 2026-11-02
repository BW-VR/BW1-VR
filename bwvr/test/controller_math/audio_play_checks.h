// The pump in QSWaveMixPlayEx, a test: the modes, the check of the code and
// the session of the known QMixer.dll, and the time of the calls
// (src/mod/audio_play_math.h). main.cpp includes this file after Check and
// Near, just before main.
#pragma once

#include <stdint.h>
#include <string.h>

#include "../../src/mod/audio_play_math.h"

namespace audio_play_checks {

inline void AudioPlayChecks() {
  namespace ap = audio_play;
  printf("\nThe pump in QSWaveMixPlayEx (audio_play_fix)\n");

  // THE MODES. 0 never skips. 1 skips for a channel of the mod only. 2 adds
  // the sounds of the atmosphere. 3 skips each call.
  Check(!ap::SkipPump(0, true, true), "mode 0 never skips the pump", 0, 0);
  Check(ap::SkipPump(1, true, false), "mode 1 skips for a channel of the mod", 1, 1);
  Check(ap::SkipPump(1, true, true), "mode 1 skips for a sound of the atmosphere of the mod", 1, 1);
  Check(!ap::SkipPump(1, false, true), "mode 1 keeps the pump for a sound of the atmosphere of QMixer", 0, 0);
  Check(!ap::SkipPump(1, false, false), "mode 1 keeps the pump for another sound", 0, 0);
  Check(ap::SkipPump(2, false, true), "mode 2 skips for a sound of the atmosphere of QMixer", 1, 1);
  Check(ap::SkipPump(2, true, false), "mode 2 skips for a channel of the mod", 1, 1);
  Check(!ap::SkipPump(2, false, false), "mode 2 keeps the pump for another sound", 0, 0);
  Check(ap::SkipPump(3, false, false), "mode 3 skips each call", 1, 1);
  Check(ap::ClampMode(-4) == 0 && ap::ClampMode(2) == 2 && ap::ClampMode(9) == 3,
        "a mode is clamped to 0 to 3", ap::ClampMode(9), 3);
  Check(ap::NextMode(0) == 1 && ap::NextMode(1) == 2 && ap::NextMode(2) == 0 && ap::NextMode(3) == 0,
        "the test key steps 0, 1, 2 and back, and 3 goes to 0", ap::NextMode(3), 0);

  // THE CODE OF QMixer.dll. The test must see the bytes of the known dll and
  // refuse a byte that is different.
  uint8_t code[20];
  memset(code, 0x90, sizeof(code));
  memcpy(code, ap::kPumpTest, sizeof(ap::kPumpTest));
  Check(ap::PumpTestMatches(code, sizeof(code)), "the known code is accepted", 1, 1);
  code[5] = 0x74;
  Check(!ap::PumpTestMatches(code, sizeof(code)), "a changed jump is refused", 0, 0);
  code[5] = 0x75;
  code[11] ^= 1;
  Check(!ap::PumpTestMatches(code, sizeof(code)), "a changed call target is refused", 0, 0);
  Check(!ap::PumpTestMatches(code, 13), "a short read is refused", 0, 0);
  Check(!ap::PumpTestMatches(nullptr, 20), "a null pointer is refused", 0, 0);

  // THE SESSION. The word 0x5432 at +0 (bytes 32 54), and 0 at +5.
  uint8_t session[16] = {0x32, 0x54, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  Check(ap::SessionFlagOffset(session) == 5, "a session with the byte 0 gives offset 5", ap::SessionFlagOffset(session), 5);
  session[5] = 1;
  Check(ap::SessionFlagOffset(session) == -1, "a session whose byte is 1 is not touched", ap::SessionFlagOffset(session), -1);
  session[5] = 0;
  session[1] = 0x55;
  Check(ap::SessionFlagOffset(session) == -1, "a wrong word is refused", ap::SessionFlagOffset(session), -1);
  Check(ap::SessionFlagOffset(nullptr) == -1, "a null session is refused", ap::SessionFlagOffset(nullptr), -1);

  // THE TIME. Frequency 10 MHz: 10 ticks are 1 us. Three calls of 100, 300
  // and 9000 ticks (10, 30, 900 us), the long limit 5000 ticks (500 us).
  ap::Stat s;
  Check(s.calls == 0 && s.MeanUs(10000000) == 0.0 && s.MostUs(10000000) == 0.0,
        "an empty stat has zeros", s.MeanUs(10000000), 0);
  s.Add(100, 5000);
  s.Add(300, 5000);
  s.Add(9000, 5000);
  Check(s.calls == 3, "three calls", static_cast<double>(s.calls), 3);
  Check(s.long_calls == 1, "one call of 500 us or more", static_cast<double>(s.long_calls), 1);
  Check(fabs(s.MeanUs(10000000) - 313.333) < 0.01, "the mean in us", s.MeanUs(10000000), 313.333);
  Check(fabs(s.MostUs(10000000) - 900.0) < 0.001, "the most in us", s.MostUs(10000000), 900.0);
  s.Add(-5, 5000);
  Check(s.calls == 4 && s.sum == 9400, "a negative time counts as 0", static_cast<double>(s.sum), 9400);
  Check(s.MeanUs(0) == 0.0 && s.MostUs(0) == 0.0, "a frequency of 0 gives 0", s.MeanUs(0), 0);
}

}  // namespace audio_play_checks
