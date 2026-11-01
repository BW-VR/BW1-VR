// The gaze of an eye tracker from OSC messages, a test (osc_gaze_math.h).
// main.cpp includes this file after Check and Near, just before main.
#pragma once

#include <string.h>

#include "../../src/mod/osc_gaze_math.h"
#include "../../src/mod/foveation_math.h"

namespace osc_gaze_checks {

// A packet of OSC, written as a sender writes it.
struct Packet {
  uint8_t data[256];
  size_t size = 0;

  void Word(uint32_t v) {
    data[size++] = static_cast<uint8_t>(v >> 24);
    data[size++] = static_cast<uint8_t>(v >> 16);
    data[size++] = static_cast<uint8_t>(v >> 8);
    data[size++] = static_cast<uint8_t>(v);
  }
  void Text(const char* text) {
    const size_t n = strlen(text);
    memcpy(data + size, text, n);
    size += n;
    do { data[size++] = 0; } while (size % 4 != 0);
  }
  void Float(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    Word(bits);
  }
  void Double(double d) {
    uint64_t bits;
    memcpy(&bits, &d, sizeof(bits));
    Word(static_cast<uint32_t>(bits >> 32));
    Word(static_cast<uint32_t>(bits));
  }
};

inline Packet Floats(const char* address, const float* values, int count) {
  Packet p;
  p.Text(address);
  char tags[16] = ",";
  for (int i = 0; i < count; ++i) strcat_s(tags, "f");
  p.Text(tags);
  for (int i = 0; i < count; ++i) p.Float(values[i]);
  return p;
}

struct Last {
  int messages = 0;
  int gazes = 0;
  float gaze[3] = {0.0f, 0.0f, 0.0f};
  void operator()(const osc_gaze_math::Message& m) {
    ++messages;
    if (osc_gaze_math::GazeOf(m, gaze)) ++gazes;
  }
};

inline void OscGazeChecks() {
  using namespace osc_gaze_math;
  printf("\nThe gaze of an eye tracker from OSC messages\n");

  // A message with three floats.
  const float ahead[3] = {0.0f, 0.0f, 1.0f};
  Packet p = Floats("/tracking/eye/CenterVec", ahead, 3);
  Check(p.size == 24 + 8 + 12, "CenterVec has 24 + 8 + 12 bytes",
        static_cast<double>(p.size), 44);
  Message m;
  Check(ParseMessage(p.data, p.size, &m), "a message of three floats", 1, 1);
  Check(strcmp(m.address, "/tracking/eye/CenterVec") == 0 && m.count == 3 &&
            !m.other,
        "its address and three numbers", m.count, 3);
  float g[3] = {9.0f, 9.0f, 9.0f};
  Check(GazeOf(m, g), "CenterVec is a gaze", 1, 1);
  Near("z forward of Unity is -z of OpenXR", g[2], -1.0, 1e-6);
  Near("and x is 0", g[0], 0.0, 1e-6);

  // A packet that is cut is not read.
  Check(!ParseMessage(p.data, p.size - 2, &m),
        "a message with its last float cut is not read", 0, 0);
  Check(!ParseMessage(p.data, 3, &m), "four bytes are the least", 0, 0);
  const uint8_t no_slash[8] = {'x', 'y', 0, 0, ',', 0, 0, 0};
  Check(!ParseMessage(no_slash, 8, &m), "an address starts with a slash", 0,
        0);
  const uint8_t no_end[4] = {'/', 'a', 'b', 'c'};
  Check(!ParseMessage(no_end, 4, &m), "an address with no 0 is not read", 0,
        0);

  // A gaze to the right and up, not of length 1.
  const float right_up[3] = {1.0f, 0.5f, 2.0f};
  p = Floats("/tracking/eye/CenterVecFull", right_up, 3);
  ParseMessage(p.data, p.size, &m);
  Check(GazeOf(m, g), "CenterVecFull is a gaze", 1, 1);
  Near("it has the length 1", sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]),
       1.0, 1e-5);
  float gx = 0.0f, gy = 0.0f;
  Check(foveation_math::GazeTangent(g, &gx, &gy),
        "it points forward for foveated rendering", 1, 1);
  Near("x tangent 0.5 (to the right)", gx, 0.5, 1e-5);
  Near("y tangent 0.25 (up)", gy, 0.25, 1e-5);

  // The two eyes: the mean.
  const float eyes[6] = {-0.1f, 0.0f, 1.0f, 0.1f, 0.0f, 1.0f};
  p = Floats("/tracking/eye/LeftRightVec", eyes, 6);
  ParseMessage(p.data, p.size, &m);
  Check(GazeOf(m, g), "LeftRightVec is a gaze", 1, 1);
  Near("the mean of the two eyes looks straight ahead: x", g[0], 0.0, 1e-6);
  Near("the mean of the two eyes looks straight ahead: z", g[2], -1.0, 1e-6);
  p = Floats("/tracking/eye/LeftRightVec", eyes, 5);
  ParseMessage(p.data, p.size, &m);
  Check(!GazeOf(m, g), "LeftRightVec with five numbers is not a gaze", 0, 0);

  // Pitch and yaw in degrees of Unity.
  const float yaw_right[2] = {0.0f, 30.0f};
  p = Floats("/tracking/eye/CenterPitchYaw", yaw_right, 2);
  ParseMessage(p.data, p.size, &m);
  Check(GazeOf(m, g) && foveation_math::GazeTangent(g, &gx, &gy),
        "CenterPitchYaw is a gaze", 1, 1);
  Near("a yaw of 30 degrees looks right: tan 30", gx, tanf(30.0f / 57.2957795f),
       1e-5);
  Near("and not up or down", gy, 0.0, 1e-6);
  const float pitch_down[3] = {20.0f, 0.0f, 1.5f};
  p = Floats("/tracking/eye/CenterPitchYawDist", pitch_down, 3);
  ParseMessage(p.data, p.size, &m);
  Check(GazeOf(m, g) && foveation_math::GazeTangent(g, &gx, &gy),
        "CenterPitchYawDist is a gaze", 1, 1);
  Near("a pitch of 20 degrees looks down: y tangent -tan 20", gy,
       -tanf(20.0f / 57.2957795f), 1e-5);
  const float both[4] = {10.0f, -20.0f, 10.0f, -20.0f};
  p = Floats("/tracking/eye/LeftRightPitchYaw", both, 4);
  ParseMessage(p.data, p.size, &m);
  Check(GazeOf(m, g) && foveation_math::GazeTangent(g, &gx, &gy),
        "LeftRightPitchYaw is a gaze", 1, 1);
  Near("two eyes 20 degrees to the left: x tangent -tan 20", gx,
       -tanf(20.0f / 57.2957795f), 1e-5);
  const float wrong[2] = {120.0f, 0.0f};
  p = Floats("/tracking/eye/CenterPitchYaw", wrong, 2);
  ParseMessage(p.data, p.size, &m);
  Check(!GazeOf(m, g), "a pitch of 120 degrees is not a gaze", 0, 0);

  // Other arguments and other addresses.
  const float zero[3] = {0.0f, 0.0f, 0.0f};
  p = Floats("/tracking/eye/CenterVec", zero, 3);
  ParseMessage(p.data, p.size, &m);
  Check(!GazeOf(m, g), "a direction of length 0 is not a gaze", 0, 0);
  const float lid[1] = {0.3f};
  p = Floats("/tracking/eye/EyesClosedAmount", lid, 1);
  ParseMessage(p.data, p.size, &m);
  Check(!GazeOf(m, g), "EyesClosedAmount is not a gaze", 0, 0);
  p = Packet();
  p.Text("/tracking/eye/CenterVec");
  p.Text(",dfi");
  p.Double(0.0);
  p.Float(0.0f);
  p.Word(1);
  ParseMessage(p.data, p.size, &m);
  Check(m.count == 3 && GazeOf(m, g), "a double and an int are numbers too",
        m.count, 3);
  Near("x tangent of 0, 0, 1 is 0", g[0], 0.0, 1e-6);
  p = Packet();
  p.Text("/avatar/parameters/Name");
  p.Text(",sf");
  p.Text("text");
  p.Float(1.0f);
  Check(ParseMessage(p.data, p.size, &m) && m.other && m.count == 0,
        "a string is another argument, and the reading stops", m.count, 0);

  // A bundle with two messages, and a bundle in it.
  Packet first = Floats("/tracking/eye/EyesClosedAmount", lid, 1);
  Packet second = Floats("/tracking/eye/CenterVec", right_up, 3);
  Packet inner;
  inner.Text("#bundle");
  inner.Word(0);
  inner.Word(1);
  inner.Word(static_cast<uint32_t>(second.size));
  memcpy(inner.data + inner.size, second.data, second.size);
  inner.size += second.size;
  Packet bundle;
  bundle.Text("#bundle");
  bundle.Word(0);
  bundle.Word(1);
  bundle.Word(static_cast<uint32_t>(first.size));
  memcpy(bundle.data + bundle.size, first.data, first.size);
  bundle.size += first.size;
  bundle.Word(static_cast<uint32_t>(inner.size));
  memcpy(bundle.data + bundle.size, inner.data, inner.size);
  bundle.size += inner.size;
  Last last;
  const int count = ForEachMessage(bundle.data, bundle.size, last);
  Check(count == 2 && last.messages == 2, "a bundle with a bundle in it has "
        "two messages", count, 2);
  Check(last.gazes == 1, "one of them is a gaze", last.gazes, 1);
  Near("the gaze of the inner bundle: z", last.gaze[2],
       -2.0 / sqrt(1.0 + 0.25 + 4.0), 1e-5);
  // An element larger than the bundle stops the reading.
  bundle.data[16 + 3] = 0xFF;
  Last cut;
  Check(ForEachMessage(bundle.data, bundle.size, cut) == 0,
        "an element larger than the bundle is not read", cut.messages, 0);

  // The sources and the test key.
  Check(SourceOf(0) == kStraight && SourceOf(3) == kOsc && SourceOf(7) == kAny,
        "foveation_gaze 0 to 3, and other values are 1", 1, 1);
  Check(UsesOpenXr(kAny) && UsesOsc(kAny) && !UsesOsc(kOpenXr) &&
            !UsesOpenXr(kOsc) && !UsesOpenXr(kStraight) &&
            !UsesOsc(kStraight),
        "the sources of each value", 1, 1);
  Check(NextSource(kAny) == kOpenXr && NextSource(kOpenXr) == kOsc &&
            NextSource(kOsc) == kStraight && NextSource(kStraight) == kAny,
        "the test key goes 1, 2, 3, 0, 1", 1, 1);
  Check(Fresh(0.0) && Fresh(kFreshMs) && !Fresh(kFreshMs + 1.0) &&
            !Fresh(-1.0),
        "a gaze of OSC is used for 250 ms", 1, 1);
}

}  // namespace osc_gaze_checks
