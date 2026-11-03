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
// The open of a wave (audio_open_loader). In the same call QMixer opens the
// wave of the play (RVA 0xC230). If the wave format is PCM or float, or the
// session has no loader thread, the open runs at once on the thread of the
// caller. Else the open goes to the loader thread of the session ("Wave
// Loader"), and the play has no sound until the loader has done it. The
// formats MS ADPCM, A-law and IMA ADPCM go to the loader thread until the
// audio compression manager has loaded once, and run at once on the thread of
// the caller after that.
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
  // Below the open (RVA 0xC230).
  kPartFlags,       // RVA 0xD020, the test of the flags of the wave
  kPartLoader,      // RVA 0x2B670, the open at once, or the request to the loader
  kPartFormat,      // RVA 0x265D0, the format says: at once, or the loader
  kPartSrcMap,      // RVA 0x26510, Open of a source (PCM or the decoder)
  kPartSrcBase,     // RVA 0x2C0F0, Open of the base source (the PCM source)
  kPartSrcMem,      // RVA 0x246E0, Open of a source that copies the data
  kPartSrcFile,     // RVA 0x25E40, Open of a source of another kind
  kPartPcmCtor,     // RVA 0x29290, the constructor of the PCM reader
  kPartDecCtor,     // RVA 0x256E0, the constructor of the decoder
  kPartDecInit,     // RVA 0x257B0, the decoder opens the codec (ACM)
  kPartSeekPcm,     // RVA 0x29350, the PCM reader goes to the start position
  kPartLock,        // RVA 0xA2D0, the lock of the session
  kPartCodeEnd,
  // The system calls of QMixer inside the open: its imports (IAT), and the
  // functions of the audio compression manager that it gets by GetProcAddress.
  kSysFirst = kPartCodeEnd,
  kSysEnter = kSysFirst,  // EnterCriticalSection
  kSysLeave,              // LeaveCriticalSection
  kSysSetEvent,           // SetEvent
  kSysWait,               // WaitForSingleObject
  kSysSleep,              // Sleep
  kSysGetProc,            // GetProcAddress
  kSysLoadLib,            // LoadLibraryA
  kSysCreateFile,         // CreateFileA
  kSysReadFile,           // ReadFile
  kSysSetFilePtr,         // SetFilePointer
  kSysMapping,            // CreateFileMappingA
  kSysMapView,            // MapViewOfFile
  kSysHeapAlloc,          // HeapAlloc
  kSysVirtAlloc,          // VirtualAlloc
  kSysCoCreate,           // CoCreateInstance
  kSysMmioOpen,           // mmioOpenA
  kSysMmioRead,           // mmioRead
  kSysMmioSeek,           // mmioSeek
  kSysMmioDescend,        // mmioDescend
  kSysAcmSuggest,         // acmFormatSuggest
  kSysAcmOpen,            // acmStreamOpen
  kSysAcmSize,            // acmStreamSize
  kSysAcmPrepare,         // acmStreamPrepareHeader
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
    // The bytes 1 to 4 of "queue" are an absolute address of QMixer.dll (the
    // pointer to the session). The check holds for the one known dll only.
    {"queue", 0x5FF0, {0xA1, 0xC0, 0x77, 0x04, 0x18, 0x53, 0x8B, 0x5C}},
    {"open", 0xC230, {0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x40, 0x85, 0xC9}},
    {"add", 0x60F0, {0x55, 0x56, 0x8B, 0x74, 0x24, 0x0C, 0x57, 0x8B}},
    {"clear", 0x7580, {0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x68, 0x01, 0x00}},
    {"append", 0x7660, {0x83, 0xEC, 0x2C, 0x53, 0x56, 0x8B, 0xF1, 0x8B}},
    {"sort", 0x67E0, {0x83, 0xEC, 0x14, 0x53, 0x8B, 0xD9, 0x55, 0x56}},
    {"pump", 0xA310, {0x56, 0x8B, 0xF1, 0xE8, 0xC8, 0xC4, 0xFF, 0xFF}},
    {"voice", 0x72E0, {0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x34, 0x8A}},
    {"flags", 0xD020, {0x8B, 0x41, 0x44, 0x8B, 0x4C, 0x24, 0x04, 0x8B}},
    {"loader", 0x2B670, {0x83, 0xEC, 0x14, 0x53, 0x8B, 0xD9, 0x56, 0x8B}},
    {"format", 0x265D0, {0x8B, 0x41, 0x1C, 0x85, 0xC0, 0x75, 0x03, 0xB0}},
    {"src_map", 0x26510, {0x56, 0x8B, 0xF1, 0x57, 0x33, 0xFF, 0x8B, 0x46}},
    {"src_base", 0x2C0F0, {0x56, 0x8B, 0xF1, 0x6A, 0x48, 0xE8, 0x51, 0x27}},
    {"src_mem", 0x246E0, {0x56, 0x8B, 0xF1, 0x57, 0x8B, 0x46, 0x0C, 0x50}},
    {"src_file", 0x25E40, {0x56, 0x8B, 0xF1, 0x68, 0xC4, 0x00, 0x00, 0x00}},
    {"pcm_ctor", 0x29290, {0x8B, 0x44, 0x24, 0x0C, 0x56, 0x8B, 0xF1, 0x50}},
    {"dec_ctor", 0x256E0, {0x8B, 0x44, 0x24, 0x0C, 0x56, 0x8B, 0xF1, 0x57}},
    // The bytes 1 to 4 of "dec_init" are the absolute address of a flag of
    // QMixer.dll.
    {"dec_init", 0x257B0, {0xA1, 0xCC, 0x57, 0x04, 0x18, 0x83, 0xEC, 0x1C}},
    {"seek_pcm", 0x29350, {0x83, 0xEC, 0x14, 0x8B, 0xC1, 0x56, 0x8B, 0x74}},
    {"lock", 0xA2D0, {0x8B, 0x81, 0x14, 0x01, 0x00, 0x00, 0x85, 0xC0}},
    // The system parts follow (kSysFirst is kPartCodeEnd).
    {"k32:EnterCS", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:LeaveCS", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:SetEvent", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:Wait", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:Sleep", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:GetProc", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:LoadLib", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:CreateFile", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:ReadFile", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:SetFilePtr", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:Mapping", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:MapView", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:HeapAlloc", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"k32:VirtAlloc", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"ole:CoCreate", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"winmm:mmioOpen", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"winmm:mmioRead", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"winmm:mmioSeek", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"winmm:mmioDescend", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"acm:FormatSuggest", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"acm:StreamOpen", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"acm:StreamSize", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
    {"acm:StreamPrepare", 0, {0, 0, 0, 0, 0, 0, 0, 0}},
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

// ---- The open of a wave (audio_open_loader) -------------------------------

// The wave format tags that the open tests (RVA 0x265D0).
const uint32_t kTagPcm = 0x01;
const uint32_t kTagMsAdpcm = 0x02;
const uint32_t kTagFloat = 0x03;
const uint32_t kTagAlaw = 0x06;
const uint32_t kTagImaAdpcm = 0x11;

// The RVA of the byte that QMixer sets to 1 when the audio compression manager
// has loaded (the format test reads it for the three ADPCM style tags). The
// session keeps the pointer to the loader object at +0x158, and the loader has
// the handle of its thread at +0x18 (0 for no thread).
const uint32_t kAcmReadyRva = 0x47828;
const int kLoaderOffset = 0x158;
const int kLoaderThreadOffset = 0x18;

// What the format test at RVA 0x265D0 answers: true when the open goes to the
// loader thread (if the session has one), false when it runs at once.
inline bool FormatWantsLoader(bool have_format, uint32_t tag, bool acm_ready) {
  if (!have_format) return true;
  if (tag == kTagPcm || tag == kTagFloat) return false;
  if (tag == kTagMsAdpcm || tag == kTagAlaw || tag == kTagImaAdpcm) return !acm_ready;
  return true;
}

// Whether the open of a wave runs at once on the thread of the caller.
inline bool OpensAtOnce(bool loader_thread, bool have_format, uint32_t tag,
                        bool acm_ready) {
  return !loader_thread || !FormatWantsLoader(have_format, tag, acm_ready);
}

// audio_open_loader, the modes. 0: the open runs as QMixer has it. 1: the open
// of a wave of the atmosphere goes to the loader thread. 2: each open does.
const int kOpenModes = 3;
inline int ClampOpenMode(int mode) {
  return mode < 0 ? 0 : (mode >= kOpenModes ? kOpenModes - 1 : mode);
}
inline int NextOpenMode(int mode) { return (mode >= 0 && mode < kOpenModes - 1) ? mode + 1 : 0; }
inline bool ForceLoader(int mode, bool atmosphere) {
  return mode == 1 ? atmosphere : mode >= 2;
}

// A short name for a format tag, for the log.
inline const char* TagName(uint32_t tag) {
  switch (tag) {
    case kTagPcm: return "PCM";
    case kTagMsAdpcm: return "MS ADPCM";
    case kTagFloat: return "float";
    case kTagAlaw: return "A-law";
    case 0x07: return "mu-law";
    case kTagImaAdpcm: return "IMA ADPCM";
    case 0x55: return "MP3";
    case 0x161: return "WMA";
    default: return "other";
  }
}

// A part that is a system call (an import or an ACM function).
inline bool IsSysPart(int part) { return part >= kSysFirst && part < kParts; }

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
