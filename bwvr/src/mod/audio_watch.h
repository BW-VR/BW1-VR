// THE WAIT IN THE AUDIO TURN (audio_turn_watch, audio_watch_math.h,
// docs/audio.md, "7.7 The watch of the audio turn"). While GAudio::
// ProcessAudioGameTurn runs on the frame thread, the mod times each call that
// the turn makes into LHaudiodllR.dll and QMixer.dll, and each wait call of
// the two DLLs, with the clock. On the mixer thread of QMixer it times the
// DirectSound calls and waveOutWrite.

#pragma once

#include <stddef.h>
#include <stdint.h>

// True with audio_turn_watch=1 when the watch is in. The callers read it
// first, so with audio_turn_watch=0 they make no call below.
extern bool g_audio_watch;

// The kinds of call that the hooks of the mod time themselves. The first
// four and the render run on the mixer thread. The lock of the mod is timed
// on the frame thread only, and only when it must wait.
enum AudioWatchKind {
  kAwLock = 0,       // IDirectSoundBuffer::Lock
  kAwUnlock,         // IDirectSoundBuffer::Unlock
  kAwPosition,       // IDirectSoundBuffer::GetCurrentPosition
  kAwWrite,          // waveOutWrite
  kAwRender,         // the render of the mixer of the mod in a block
  kAwModLock,        // the lock of the mixer of the mod (audio_voices.cpp)
  kAwKinds
};

// turn_parts.cpp, in TurnPartsInstall on the frame thread, when the timer of
// audio_turn is in. Reads audio_turn_watch. The import slots change at the
// first audio turn.
void AudioWatchInit();
// turn_parts.cpp: the audio turn starts and ends on the frame thread, at the
// tick `now` of the performance counter.
void AudioWatchTurnBegin(int64_t now);
void AudioWatchTurnEnd(int64_t now);
// turn_parts.cpp, at the end of a game turn: an audio turn that no return
// closed goes away.
void AudioWatchGameTurnEnd();
// turn_parts.cpp, in TurnPartsFrameText: the text of the audio turn of the
// frame that ended at this present when it took 5 ms or more. It gives the
// length of the text (0 when there is none). AudioWatchFrameEnd clears the
// frame.
int AudioWatchFrameText(char* out, size_t size);
void AudioWatchFrameEnd();
// time_log.cpp, at the end of its window of 10 s. True when a line went to
// the log.
bool AudioWatchWindowLine(double seconds);

// The hooks of the mod that time their own call: Begin gives a token (0 when
// the call is not timed), and End gets it with the return address of the
// call. The token is the clock.
int64_t AudioWatchBegin(int kind, uintptr_t caller);
void AudioWatchEnd(int kind, int64_t token);

// A hook of the mod that reads its own return address calls this with it.
// When a timer of the watch is on the function that the hook belongs to, the
// return address is the exit thunk of the timer, and this gives the address
// that the game would see.
uintptr_t AudioWatchCallerSlow(uintptr_t ret);
inline uintptr_t AudioWatchCaller(uintptr_t ret) {
  return g_audio_watch ? AudioWatchCallerSlow(ret) : ret;
}
