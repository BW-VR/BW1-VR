// The gaze of an eye tracker from OSC messages (eye_osc.cpp, docs/backend.md,
// "Foveated rendering"). Pure: no Windows, no sockets. Checks in
// test/controller_math/osc_gaze_checks.h.
//
// OSC 1.0: a packet is a message or a bundle. A message is an address, a
// string of type tags (",fff") and the arguments, each padded to 4 bytes,
// with the numbers big-endian. A bundle is "#bundle", a time tag of 8 bytes,
// and elements, each a size of 4 bytes and a packet.
//
// The addresses of the gaze are the ones of the eye tracking of VRChat. Steam
// Link sends them for the eye tracker of the Steam Frame, and the senders of
// other eye trackers (for example VRCFaceTracking) use them too. The space of
// these messages is the space of the head of Unity: x to the right, y up, z
// forward. A pitch and a yaw are degrees of Unity: a positive pitch looks
// down, a positive yaw looks to the right. The gaze of this file is in the
// space of the head of OpenXR: x to the right, y up, -z forward.
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace osc_gaze_math {

// The most arguments of a message that the mod reads.
const int kMaxArgs = 8;
// The most levels of bundles in bundles.
const int kMaxDepth = 4;

struct Message {
  const char* address;  // in the packet, ends with a 0
  float args[kMaxArgs];
  int count;  // the number of arguments that are numbers
  bool other;  // the message has an argument that is not a number
};

inline uint32_t BigEndian(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

// The bytes of the string at data with its 0 and its padding to 4 bytes, or
// 0 when the string has no 0 within size.
inline size_t PaddedLength(const uint8_t* data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (data[i] == 0) {
      const size_t padded = (i + 4) & ~static_cast<size_t>(3);
      return padded <= size ? padded : 0;
    }
  }
  return 0;
}

// One message. False for a packet that is not a message of OSC. The
// arguments f (float), i (int32) and d (double) become numbers. T, F, N and
// I have no data. A message with other arguments (s, b, h, t and others)
// has other set and keeps the numbers before the first such argument.
inline bool ParseMessage(const uint8_t* data, size_t size, Message* out) {
  out->address = nullptr;
  out->count = 0;
  out->other = false;
  if (size < 4 || data[0] != '/') return false;
  const size_t address = PaddedLength(data, size);
  if (address == 0) return false;
  out->address = reinterpret_cast<const char*>(data);
  size_t at = address;
  // A message with no type tags is old OSC. It has no arguments to read.
  if (at >= size || data[at] != ',') return true;
  const size_t tags = PaddedLength(data + at, size - at);
  if (tags == 0) return false;
  const char* tag = reinterpret_cast<const char*>(data + at + 1);
  at += tags;
  for (; *tag != 0; ++tag) {
    float value = 0.0f;
    if (*tag == 'f' || *tag == 'i') {
      if (at + 4 > size) return false;
      const uint32_t bits = BigEndian(data + at);
      at += 4;
      if (*tag == 'f') {
        memcpy(&value, &bits, sizeof(value));
      } else {
        value = static_cast<float>(static_cast<int32_t>(bits));
      }
    } else if (*tag == 'd') {
      if (at + 8 > size) return false;
      const uint64_t bits =
          (static_cast<uint64_t>(BigEndian(data + at)) << 32) |
          BigEndian(data + at + 4);
      at += 8;
      double d = 0.0;
      memcpy(&d, &bits, sizeof(d));
      value = static_cast<float>(d);
    } else if (*tag == 'T' || *tag == 'F' || *tag == 'N' || *tag == 'I') {
      continue;
    } else {
      out->other = true;
      return true;
    }
    if (out->count < kMaxArgs) out->args[out->count++] = value;
  }
  return true;
}

// Calls visit(const Message&) for each message of a packet, also for each
// message of a bundle. Gives the number of messages.
template <typename Visit>
int ForEachMessage(const uint8_t* data, size_t size, Visit& visit,
                   int depth = 0) {
  static const char kBundle[8] = {'#', 'b', 'u', 'n', 'd', 'l', 'e', 0};
  if (size >= 16 && memcmp(data, kBundle, 8) == 0) {
    if (depth >= kMaxDepth) return 0;
    int messages = 0;
    size_t at = 16;  // after "#bundle" and the time tag
    while (at + 4 <= size) {
      const uint32_t element = BigEndian(data + at);
      at += 4;
      if (element > size - at) break;
      messages += ForEachMessage(data + at, element, visit, depth + 1);
      at += element;
    }
    return messages;
  }
  Message message;
  if (!ParseMessage(data, size, &message)) return 0;
  visit(message);
  return 1;
}

// A direction of Unity (x right, y up, z forward) in the space of OpenXR,
// with the length 1. False for a direction of length 0 or with no number.
inline bool FromUnity(float x, float y, float z, float out[3]) {
  const float length = sqrtf(x * x + y * y + z * z);
  if (!(length > 1e-6f) || !(length < 1e30f)) return false;
  out[0] = x / length;
  out[1] = y / length;
  out[2] = -z / length;
  return true;
}

// The direction of a pitch and a yaw of Unity in degrees, in the space of
// OpenXR: the forward axis turned by Quaternion.Euler(pitch, yaw, 0).
inline bool FromPitchYaw(float pitch_deg, float yaw_deg, float out[3]) {
  if (!(fabsf(pitch_deg) <= 90.0f) || !(fabsf(yaw_deg) <= 180.0f)) {
    return false;
  }
  const float pitch = pitch_deg / 57.2957795f;
  const float yaw = yaw_deg / 57.2957795f;
  return FromUnity(sinf(yaw) * cosf(pitch), -sinf(pitch),
                   cosf(yaw) * cosf(pitch), out);
}

// The mean of two directions of length 1, with the length 1.
inline bool Mean(const float a[3], const float b[3], float out[3]) {
  return FromUnity(a[0] + b[0], a[1] + b[1], -(a[2] + b[2]), out);
}

// The gaze of one message, in the space of the head of OpenXR. False for a
// message that is not a gaze, or a gaze with wrong numbers.
//   /tracking/eye/CenterVec, /tracking/eye/CenterVecFull: x, y, z
//   /tracking/eye/LeftRightVec: x, y, z of the left eye, then of the right
//   /tracking/eye/CenterPitchYaw, /tracking/eye/CenterPitchYawDist: pitch,
//     yaw (and a distance that the mod does not use)
//   /tracking/eye/LeftRightPitchYaw: pitch, yaw of the left eye, then of
//     the right
inline bool GazeOf(const Message& m, float out[3]) {
  if (m.address == nullptr) return false;
  const char* a = m.address;
  if (strcmp(a, "/tracking/eye/CenterVec") == 0 ||
      strcmp(a, "/tracking/eye/CenterVecFull") == 0) {
    return m.count >= 3 && FromUnity(m.args[0], m.args[1], m.args[2], out);
  }
  if (strcmp(a, "/tracking/eye/LeftRightVec") == 0) {
    float left[3], right[3];
    return m.count >= 6 &&
           FromUnity(m.args[0], m.args[1], m.args[2], left) &&
           FromUnity(m.args[3], m.args[4], m.args[5], right) &&
           Mean(left, right, out);
  }
  if (strcmp(a, "/tracking/eye/CenterPitchYaw") == 0 ||
      strcmp(a, "/tracking/eye/CenterPitchYawDist") == 0) {
    return m.count >= 2 && FromPitchYaw(m.args[0], m.args[1], out);
  }
  if (strcmp(a, "/tracking/eye/LeftRightPitchYaw") == 0) {
    float left[3], right[3];
    return m.count >= 4 && FromPitchYaw(m.args[0], m.args[1], left) &&
           FromPitchYaw(m.args[2], m.args[3], right) &&
           Mean(left, right, out);
  }
  return false;
}

// THE SOURCES OF THE GAZE (the key foveation_gaze).
enum Source {
  kStraight = 0,  // no gaze: the centre of the fovea is straight ahead
  kAny = 1,       // the gaze of OpenXR, else the gaze of OSC
  kOpenXr = 2,    // only XR_EXT_eye_gaze_interaction
  kOsc = 3,       // only OSC
};

inline Source SourceOf(int key) {
  return key >= kStraight && key <= kOsc ? static_cast<Source>(key) : kAny;
}

inline bool UsesOpenXr(Source s) { return s == kAny || s == kOpenXr; }
inline bool UsesOsc(Source s) { return s == kAny || s == kOsc; }

// The next source of the test key: 1, 2, 3, 0, 1.
inline Source NextSource(Source s) {
  switch (s) {
    case kAny: return kOpenXr;
    case kOpenXr: return kOsc;
    case kOsc: return kStraight;
    default: return kAny;
  }
}

// A gaze of OSC is used when it is at most this old. A sender sends a gaze
// each frame of the eye tracker, so an older gaze is from a sender that
// stopped.
const double kFreshMs = 250.0;

inline bool Fresh(double age_ms) { return age_ms >= 0.0 && age_ms <= kFreshMs; }

}  // namespace osc_gaze_math
