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

  // audio_play_now. The hooks use the modes of the pump (SkipPump), so a
  // mode 1 play of a sound that QMixer plays keeps its work in the call.
  Check(!ap::SkipPump(1, false, true) && ap::SkipPump(2, false, true),
        "audio_play_now 1 keeps the work for a sound of QMixer, 2 defers a sound of the atmosphere", 0, 0);

  // THE PARTS. Each hooked part has an RVA and 8 bytes. A part with no hook
  // (the test of the pointer) has RVA 0 and never matches.
  int hooked = 0;
  bool unique = true, ordered_names = true;
  for (int i = 0; i < ap::kParts; ++i) {
    if (ap::kPartInfo[i].rva != 0) ++hooked;
    if (ap::kPartInfo[i].name == nullptr || ap::kPartInfo[i].name[0] == 0) ordered_names = false;
    for (int j = i + 1; j < ap::kParts; ++j) {
      if (ap::kPartInfo[i].rva != 0 && ap::kPartInfo[i].rva == ap::kPartInfo[j].rva) unique = false;
    }
  }
  Check(hooked == ap::kParts - 1, "all parts but the pointer test have a hook", hooked, ap::kParts - 1);
  Check(unique && ordered_names, "the RVAs are all different and each part has a name", unique ? 1 : 0, 1);
  Check(ap::kPartInfo[ap::kPartSort].rva == 0x67E0 && ap::kPartInfo[ap::kPartPump].rva == 0xA310,
        "the sort is at 0x67E0 and the pump at 0xA310", ap::kPartInfo[ap::kPartPump].rva, 0xA310);
  uint8_t fn[12];
  memset(fn, 0x90, sizeof(fn));
  memcpy(fn, ap::kPartInfo[ap::kPartSort].bytes, 8);
  Check(ap::PartMatches(ap::kPartSort, fn, sizeof(fn)), "the bytes of the sort are accepted", 1, 1);
  Check(!ap::PartMatches(ap::kPartPump, fn, sizeof(fn)), "the bytes of the sort are not those of the pump", 0, 0);
  fn[7] ^= 1;
  Check(!ap::PartMatches(ap::kPartSort, fn, sizeof(fn)), "a changed last byte is refused", 0, 0);
  fn[7] ^= 1;
  Check(!ap::PartMatches(ap::kPartSort, fn, 7), "a short read is refused", 0, 0);
  Check(!ap::PartMatches(ap::kPartSort, nullptr, 8), "a null pointer is refused", 0, 0);
  Check(!ap::PartMatches(ap::kPartBadPtr, fn, sizeof(fn)), "a part with no hook never matches", 0, 0);
  Check(!ap::PartMatches(-1, fn, sizeof(fn)) && !ap::PartMatches(ap::kParts, fn, sizeof(fn)),
        "a part number out of range is refused", 0, 0);

  // THE TIME OF A PLAY. The runs and the ticks of a part add up in one play.
  ap::PlayParts play;
  play.Clear();
  play.Add(ap::kPartSort, 400);
  play.Add(ap::kPartSort, 100);
  play.Add(ap::kPartPump, -7);
  play.Add(99, 5);
  Check(play.ticks[ap::kPartSort] == 500 && play.runs[ap::kPartSort] == 2,
        "two runs of a part add up", static_cast<double>(play.ticks[ap::kPartSort]), 500);
  Check(play.ticks[ap::kPartPump] == 0 && play.runs[ap::kPartPump] == 1,
        "a negative time counts as 0 and the run counts", static_cast<double>(play.ticks[ap::kPartPump]), 0);
  Check(play.runs[ap::kPartCtor] == 0, "a part that did not run has no run", play.runs[ap::kPartCtor], 0);
  ap::Stat part;
  part.AddPlay(play.ticks[ap::kPartSort], play.runs[ap::kPartSort], 5000);
  part.AddPlay(300, 1, 5000);
  Check(part.calls == 2 && part.runs == 3 && part.sum == 800,
        "a stat of plays counts the plays, the runs and the time", static_cast<double>(part.runs), 3);
  play.Clear();
  Check(play.ticks[ap::kPartSort] == 0 && play.runs[ap::kPartSort] == 0, "Clear empties a play", 0, 0);
}

}  // namespace audio_play_checks
