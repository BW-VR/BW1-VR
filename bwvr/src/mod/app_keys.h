// THE KEYS THAT THE DEBUG PAGE CHANGES FOR THE RENDERER AND THE APP (app_keys.cpp,
// docs/backend.md, "The compose of the eye picture"). The renderer runs in the
// headset app, and the game runs in Wine, so a key of the renderer or of the
// app cannot change in the game process. The mod keeps the value of each key
// (from bwvr.ini at the first use, then from the debug page) and puts 23 bits
// into the record of every frame of the stream (scene_stream.h,
// FrameBody::app_keys). The app plays the bits before the walks of the frame,
// so a change shows from the next picture.
#pragma once

#include <stdint.h>

namespace app_keys {

// The bits of FrameBody::app_keys (scene_stream.h).
constexpr uint32_t kSent = 1u << 31;  // the frame has the keys; 0 in a stream of the old kind
constexpr uint32_t kMresDraws = 1u << 0;
constexpr uint32_t kMresSort = 1u << 1;
constexpr uint32_t kComposeLight = 1u << 2;
constexpr uint32_t kPassStore = 1u << 3;
constexpr uint32_t kComposeDirect = 1u << 4;
constexpr uint32_t kPanelRect = 1u << 5;
constexpr uint32_t kRingOrder = 1u << 6;
constexpr uint32_t kLateCamera = 1u << 7;
constexpr uint32_t kReleaseTake = 1u << 8;
constexpr uint32_t kLateTake = 1u << 9;
constexpr uint32_t kBindless = 1u << 10;
constexpr uint32_t kPanelKeep = 1u << 11;
constexpr uint32_t kFastShaders = 1u << 12;
constexpr uint32_t kTinyDraws = 1u << 13;
constexpr uint32_t kComposeFast = 1u << 14;
constexpr uint32_t kListKeep = 1u << 15;
constexpr uint32_t kCheapDraws = 1u << 16;
constexpr uint32_t kLateGrab = 1u << 17;
constexpr uint32_t kLatePrepare = 1u << 18;
constexpr uint32_t kLateGap = 1u << 19;
constexpr uint32_t kLatePrebuild = 1u << 20;
constexpr uint32_t kLateFastCores = 1u << 21;
constexpr uint32_t kLateFastGame = 1u << 22;

// The keys of the debug page, in the order of kName in app_keys.cpp.
enum Key { kKeyMresDraws, kKeyMresSort, kKeyComposeLight, kKeyPassStore, kKeyComposeDirect,
           kKeyPanelRect, kKeyRingOrder, kKeyLateCamera, kKeyReleaseTake, kKeyLateTake, kKeyBindless, kKeyPanelKeep,
           kKeyFastShaders, kKeyTinyDraws, kKeyComposeFast, kKeyListKeep, kKeyCheapDraws, kKeyLateGrab, kKeyLatePrepare, kKeyLateGap, kKeyLatePrebuild, kKeyLateFastCores, kKeyLateFastGame, kKeys };

inline uint32_t BitOf(int key) { return 1u << key; }

// The values of the keys as the bits of one frame. Pure.
inline uint32_t Pack(const int values[kKeys]) {
  uint32_t bits = kSent;
  for (int i = 0; i < kKeys; ++i) {
    if (values[i] != 0) bits |= BitOf(i);
  }
  return bits;
}

}  // namespace app_keys

// The mod (app_keys.cpp). The bits of the current values, for the record of a frame.
uint32_t AppKeysBits();
// The value of a key of the debug page, and its change by a press or a row.
int AppKeyValue(int key);
void AppKeyChange(int key, const char* by);
