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
  bool unique = true, ordered_names = true, sys_no_rva = true;
  for (int i = ap::kSysFirst; i < ap::kParts; ++i) {
    if (ap::kPartInfo[i].rva != 0) sys_no_rva = false;
  }
  for (int i = 0; i < ap::kParts; ++i) {
    if (ap::kPartInfo[i].rva != 0) ++hooked;
    if (ap::kPartInfo[i].name == nullptr || ap::kPartInfo[i].name[0] == 0) ordered_names = false;
    for (int j = i + 1; j < ap::kParts; ++j) {
      if (ap::kPartInfo[i].rva != 0 && ap::kPartInfo[i].rva == ap::kPartInfo[j].rva) unique = false;
    }
  }
  Check(hooked == ap::kPartCodeEnd - 1, "all code parts but the pointer test have a hook", hooked, ap::kPartCodeEnd - 1);
  Check(sys_no_rva && ap::kSysFirst == ap::kPartCodeEnd, "the system parts follow the code parts and have no RVA", sys_no_rva ? 1 : 0, 1);
  Check(ap::IsSysPart(ap::kSysEnter) && ap::IsSysPart(ap::kSysAcmPrepare) && !ap::IsSysPart(ap::kPartLock) &&
            !ap::IsSysPart(ap::kParts) && !ap::IsSysPart(-1),
        "IsSysPart marks the system parts only", ap::IsSysPart(ap::kPartLock) ? 1 : 0, 0);
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
  play.Add(ap::kSysAcmOpen, 50);
  Check(play.runs[ap::kSysAcmOpen] == 1 && play.ticks[ap::kSysAcmOpen] == 50,
        "a system part is a part of a play too", static_cast<double>(play.ticks[ap::kSysAcmOpen]), 50);

  // THE PARTS BELOW THE OPEN. The RVAs are those of the known QMixer.dll.
  Check(ap::kPartInfo[ap::kPartOpen].rva == 0xC230 && ap::kPartInfo[ap::kPartLoader].rva == 0x2B670 &&
            ap::kPartInfo[ap::kPartFormat].rva == 0x265D0 && ap::kPartInfo[ap::kPartLock].rva == 0xA2D0,
        "the open, the loader, the format test and the lock have the RVAs of the decompile", ap::kPartInfo[ap::kPartFormat].rva, 0x265D0);
  uint8_t fmt[12];
  memset(fmt, 0x90, sizeof(fmt));
  memcpy(fmt, ap::kPartInfo[ap::kPartFormat].bytes, 8);
  Check(ap::PartMatches(ap::kPartFormat, fmt, sizeof(fmt)) && !ap::PartMatches(ap::kPartLoader, fmt, sizeof(fmt)),
        "the bytes of the format test match it only", 1, 1);

  // THE FORMAT TEST (RVA 0x265D0). PCM and float open at once. The three ADPCM
  // style tags go to the loader until the compression manager has loaded. Each
  // other tag, and a source with no format, go to the loader.
  Check(!ap::FormatWantsLoader(true, ap::kTagPcm, false) && !ap::FormatWantsLoader(true, ap::kTagFloat, true),
        "PCM and float open at once", 0, 0);
  Check(ap::FormatWantsLoader(true, ap::kTagImaAdpcm, false) && !ap::FormatWantsLoader(true, ap::kTagImaAdpcm, true),
        "IMA ADPCM goes to the loader until the compression manager is loaded", 1, 1);
  Check(ap::FormatWantsLoader(true, ap::kTagMsAdpcm, false) && !ap::FormatWantsLoader(true, ap::kTagMsAdpcm, true) &&
            ap::FormatWantsLoader(true, ap::kTagAlaw, false) && !ap::FormatWantsLoader(true, ap::kTagAlaw, true),
        "MS ADPCM and A-law follow the same rule", 1, 1);
  Check(ap::FormatWantsLoader(true, 0x55, true) && ap::FormatWantsLoader(true, 0x161, false),
        "MP3 and WMA always go to the loader", 1, 1);
  Check(ap::FormatWantsLoader(false, 0, true), "a source with no format goes to the loader", 1, 1);
  Check(ap::OpensAtOnce(false, true, 0x55, false), "a session with no loader thread opens at once", 1, 1);
  Check(!ap::OpensAtOnce(true, true, 0x55, false) && ap::OpensAtOnce(true, true, ap::kTagPcm, false),
        "with a loader thread the format decides", 1, 1);

  // audio_open_loader. 0 changes nothing. 1 sends the atmosphere. 2 sends each.
  Check(!ap::ForceLoader(0, true) && !ap::ForceLoader(0, false), "mode 0 forces no open", 0, 0);
  Check(ap::ForceLoader(1, true) && !ap::ForceLoader(1, false), "mode 1 forces the open of the atmosphere only", 1, 1);
  Check(ap::ForceLoader(2, false) && ap::ForceLoader(2, true), "mode 2 forces each open", 1, 1);
  Check(ap::ClampOpenMode(-3) == 0 && ap::ClampOpenMode(1) == 1 && ap::ClampOpenMode(7) == 2,
        "an open mode is clamped to 0 to 2", ap::ClampOpenMode(7), 2);
  Check(ap::NextOpenMode(0) == 1 && ap::NextOpenMode(1) == 2 && ap::NextOpenMode(2) == 0 && ap::NextOpenMode(9) == 0,
        "the test key steps 0, 1, 2 and back", ap::NextOpenMode(2), 0);
  Check(strcmp(ap::TagName(0x11), "IMA ADPCM") == 0 && strcmp(ap::TagName(1), "PCM") == 0 &&
            strcmp(ap::TagName(0x7777), "other") == 0,
        "the names of the format tags", 1, 1);

  // THE CACHE OF acmFormatSuggest (audio_acm_cache). The input is the source
  // format, the target format, its size and the flags. The answer is kept only
  // for the result 0.
  Check(ap::ClampAcmMode(-1) == 0 && ap::ClampAcmMode(1) == 1 && ap::ClampAcmMode(5) == 2,
        "an ACM mode is clamped to 0 to 2", ap::ClampAcmMode(5), 2);
  Check(ap::NextAcmMode(0) == 1 && ap::NextAcmMode(1) == 2 && ap::NextAcmMode(2) == 0 && ap::NextAcmMode(8) == 0,
        "the test key steps 0, 1, 2 and back", ap::NextAcmMode(2), 0);
  Check(ap::AcmFormatSize(1, 0) == 16 && ap::AcmFormatSize(1, 22) == 16 && ap::AcmFormatSize(2, 32) == 50 &&
            ap::AcmFormatSize(0x11, 2) == 20,
        "the size of a format: 16 for PCM, else 18 and the extra bytes", ap::AcmFormatSize(2, 32), 50);
  Check(ap::AcmSuggestCacheable(true, 50, 18, 0x10000) && !ap::AcmSuggestCacheable(false, 50, 18, 0x10000),
        "a call with a driver handle is not cached", 0, 0);
  Check(!ap::AcmSuggestCacheable(true, 15, 18, 0x10000) && !ap::AcmSuggestCacheable(true, 50, 15, 0x10000) &&
            !ap::AcmSuggestCacheable(true, 129, 18, 0x10000) && !ap::AcmSuggestCacheable(true, 50, 129, 0x10000),
        "a format that is too small or too large is not cached", 0, 0);
  Check(!ap::AcmSuggestCacheable(true, 16, 18, 0x10000) && ap::AcmSuggestCacheable(true, 18, 18, 0x10000),
        "a PCM source of 16 bytes is not cached", 0, 0);
  Check(ap::AcmSuggestCacheable(true, 50, 18, 0x000F0000) && !ap::AcmSuggestCacheable(true, 50, 18, 0x10001) &&
            !ap::AcmSuggestCacheable(true, 50, 18, 0x00100000),
        "a flag that the cache does not know is not cached", 0, 0);

  // The source is a MS ADPCM format of 50 bytes (the cbSize is 32). The target
  // is 18 bytes with the tag 1 and zero in the rest.
  uint8_t src[50], src2[50], dst_in[18], dst_out[18], got[18];
  memset(src, 0, sizeof(src));
  src[0] = 2; src[2] = 2; src[4] = 0x22; src[5] = 0x56; src[16] = 32;
  memcpy(src2, src, sizeof(src));
  memset(dst_in, 0, sizeof(dst_in));
  dst_in[0] = 1;
  memset(dst_out, 0, sizeof(dst_out));
  dst_out[0] = 1; dst_out[2] = 2; dst_out[4] = 0x22; dst_out[5] = 0x56; dst_out[8] = 0x88; dst_out[9] = 0x58;
  dst_out[12] = 4; dst_out[14] = 16;
  ap::AcmCache cache;
  cache.Clear();
  Check(ap::AcmFind(cache, src, 50, dst_in, 18, 0x10000) == -1, "an empty cache has no entry", 0, 0);
  Check(ap::AcmStore(&cache, src, 50, dst_in, 18, 0x10000, dst_out, 0x20) == -1 &&
            ap::AcmFind(cache, src, 50, dst_in, 18, 0x10000) == -1,
        "an answer that is not 0 is not kept", 0, 0);
  const int first = ap::AcmStore(&cache, src, 50, dst_in, 18, 0x10000, dst_out, 0);
  Check(first == 0 && ap::AcmFind(cache, src2, 50, dst_in, 18, 0x10000) == 0,
        "an answer is found for the same bytes", first, 0);
  memcpy(got, cache.entry[0].dst_out, sizeof(got));
  Check(memcmp(got, dst_out, sizeof(got)) == 0 && cache.entry[0].result == 0,
        "the entry gives back the bytes that the ACM wrote", 1, 1);
  src2[4] = 0x44;
  Check(ap::AcmFind(cache, src2, 50, dst_in, 18, 0x10000) == -1, "another rate is another entry", -1, -1);
  src2[4] = 0x22; src2[2] = 1;
  Check(ap::AcmFind(cache, src2, 50, dst_in, 18, 0x10000) == -1, "another channel count is another entry", -1, -1);
  src2[2] = 2; src2[40] = 7;
  Check(ap::AcmFind(cache, src2, 50, dst_in, 18, 0x10000) == -1,
        "a byte of the extra data of the source is part of the key", -1, -1);
  uint8_t dst_in2[18];
  memcpy(dst_in2, dst_in, sizeof(dst_in));
  dst_in2[17] = 9;
  Check(ap::AcmFind(cache, src, 50, dst_in2, 18, 0x10000) == -1,
        "a byte of the target that the caller sets is part of the key", -1, -1);
  Check(ap::AcmFind(cache, src, 50, dst_in, 18, 0x20000) == -1 && ap::AcmFind(cache, src, 50, dst_in, 20, 0x10000) == -1,
        "the flags and the size of the target are part of the key", -1, -1);
  Check(ap::AcmStore(&cache, src, 50, dst_in, 18, 0x10000, dst_out, 0) == 0 && cache.next == 1,
        "the same input replaces its entry", cache.next, 1);
  // Sixteen other inputs fill the cache. The 17th replaces the oldest slot.
  uint8_t other[50];
  memcpy(other, src, sizeof(other));
  for (int i = 0; i < ap::kAcmEntries; ++i) {
    other[4] = static_cast<uint8_t>(0x30 + i);
    ap::AcmStore(&cache, other, 50, dst_in, 18, 0x10000, dst_out, 0);
  }
  other[4] = 0x30;
  Check(ap::AcmFind(cache, src, 50, dst_in, 18, 0x10000) == -1 && ap::AcmFind(cache, other, 50, dst_in, 18, 0x10000) >= 0,
        "a full cache replaces the oldest entry", ap::AcmFind(cache, src, 50, dst_in, 18, 0x10000), -1);

  // THE DRIVERS FOR acmStreamOpen (mode 2).
  ap::AcmDrivers drivers;
  drivers.Clear();
  uint8_t pcm[18];
  memset(pcm, 0, sizeof(pcm));
  pcm[0] = 1; pcm[2] = 2; pcm[4] = 0x22; pcm[5] = 0x56; pcm[14] = 16;
  Check(ap::AcmDriverFind(drivers, src, 50, pcm, 18) == -1, "an empty list has no driver", -1, -1);
  Check(ap::AcmDriverStore(&drivers, src, 50, pcm, 18, 0) == -1, "a handle 0 is not kept", -1, -1);
  const int d0 = ap::AcmDriverStore(&drivers, src, 50, pcm, 18, 0x1234);
  src2[40] = 7;
  Check(d0 == 0 && ap::AcmDriverFind(drivers, src2, 50, pcm, 18) == -1 && drivers.entry[0].handle == 0x1234,
        "a driver is found for its pair of formats only", d0, 0);
  src2[40] = 0;
  Check(ap::AcmDriverFind(drivers, src2, 50, pcm, 18) == 0 && ap::AcmDriverStore(&drivers, src, 50, pcm, 18, 0x99) == -1,
        "a pair has one driver", 0, 0);

  // A pair with no driver is kept as bad: the next open finds it.
  ap::AcmDrivers bad_list;
  bad_list.Clear();
  const int b0 = ap::AcmDriverStoreBad(&bad_list, src, 50, pcm, 18);
  Check(b0 == 0 && bad_list.entry[0].bad && bad_list.entry[0].handle == 0 &&
            ap::AcmDriverFind(bad_list, src, 50, pcm, 18) == 0,
        "a pair with no driver is stored as bad and found", b0, 0);
  Check(ap::AcmDriverStoreBad(&bad_list, src, 50, pcm, 18) == -1 && ap::AcmDriverStore(&bad_list, src, 50, pcm, 18, 0x77) == -1 &&
            bad_list.entry[0].bad && bad_list.entry[0].handle == 0,
        "a bad pair is stored once and a handle does not replace it", 0, 0);
  Check(ap::AcmDriverStoreBad(&bad_list, src, 129, pcm, 18) == -1 && ap::AcmDriverStoreBad(&bad_list, src, 50, pcm, 15) == -1,
        "a pair with sizes that do not fit is not stored as bad", -1, -1);

  // A full list does not store, and it keeps each entry as it is.
  ap::AcmDrivers full;
  full.Clear();
  uint8_t other_src[50];
  memcpy(other_src, src, sizeof(other_src));
  int filled = 0;
  for (int i = 0; i < ap::kAcmDrivers; ++i) {
    other_src[4] = static_cast<uint8_t>(0x30 + i);
    // The last entry is a bad pair, so a full list holds both kinds.
    const int at = (i == ap::kAcmDrivers - 1) ? ap::AcmDriverStoreBad(&full, other_src, 50, pcm, 18)
                                               : ap::AcmDriverStore(&full, other_src, 50, pcm, 18, 0x100 + i);
    if (at == i) ++filled;
  }
  Check(filled == ap::kAcmDrivers, "the list takes kAcmDrivers pairs", filled, ap::kAcmDrivers);
  other_src[4] = 0x99;
  Check(ap::AcmDriverStore(&full, other_src, 50, pcm, 18, 0x555) == -1 &&
            ap::AcmDriverStoreBad(&full, other_src, 50, pcm, 18) == -1 &&
            ap::AcmDriverFind(full, other_src, 50, pcm, 18) == -1,
        "a full list does not store a new pair, the caller closes its handle", -1, -1);
  bool kept = true;
  for (int i = 0; i < ap::kAcmDrivers; ++i) {
    other_src[4] = static_cast<uint8_t>(0x30 + i);
    const bool is_bad = i == ap::kAcmDrivers - 1;
    kept = kept && ap::AcmDriverFind(full, other_src, 50, pcm, 18) == i && full.entry[i].bad == is_bad &&
           full.entry[i].handle == (is_bad ? 0u : static_cast<uintptr_t>(0x100 + i));
  }
  Check(kept, "a full list keeps each handle that it holds (no entry is replaced)", kept ? 1 : 0, 1);
}

}  // namespace audio_play_checks
