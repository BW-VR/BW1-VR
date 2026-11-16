// The frame of the controller on the grip pose of the runtime, and the trim
// of the player. See grip_fit.h, grip_fit_math.h and
// docs/controller-hints.md, "9.15 The place of the controller".
//
// WHY. The mod draws the model of the controller and the hand on one frame,
// and the Hand of the game takes its point from that frame. The models are
// made for the grip space of the browser of the Quest, and the hand is
// fitted to the models. Virtual Desktop, the Oculus runtime and SteamVR give
// a grip pose 16 to 18 degrees and 2.1 to 2.7 cm away from that frame. So
// the drawn controller and the hand show about 2.7 cm back from the real
// controller. The fit moves the grip pose to the frame of the model, for
// the runtimes and the models of the table (grip_fit_math.h). The aim pose
// does not change: it is the aim pose of the runtime.
//
// THE TRIM is for a player whose controller still does not fit: the
// settings window moves the controller ahead, up or out in steps of 5 mm,
// and writes the key into bwvr.ini.
//
// ONE PLACE. GripFitFrame changes the hands of the frame in ControllersFrame,
// before any code reads them. So the model, the hand, the cards at the wrist,
// the Hand of the game, the throw and the check of the view all use the same
// grip point. The hand turns from that frame by the angle of the hand
// (below). The log line of the grip to aim (controllers.cpp) reads the poses of
// the runtime before the change.
//
// THE ANGLE OF THE HAND (hand_angle_math.h, docs/hand.md, "The angle of the
// hand"). The model of the controller stays on the frame of the model. The
// drawn hand, the Hand of the game and the point of the Hand get their own
// orientation: the palm pose of the runtime (the grip pose of the runtime
// turned 20 degrees about the palm normal), at the grip point of the frame
// of the model, and then the trim of the player in degrees.
//
// THE KEYS of bwvr.ini:
//
//   grip_fit=1            1: the grip pose is the frame of the model of the
//                         controller, for the runtimes and the models of the
//                         table. 0: the grip pose of the runtime
//   grip_trim_ahead_mm=0  the trim of the player, -30 to 30 mm: ahead along
//   grip_trim_up_mm=0     the aim of the controller, up to its top, and out
//   grip_trim_out_mm=0    away from the other hand
//   grip_fit_test=0       a test key: 1 gives the hands of the scripted
//                         controller the fit of Virtual Desktop
//   hand_angle=1          1: the hand has the angle of the palm pose of the
//                         runtime. 0: the hand is on the frame of the model
//                         of the controller
//   hand_trim_yaw_deg=0   the trim of the hand, -45 to 45 degrees: about the
//   hand_trim_pitch_deg=0 palm normal (plus toward the thumb), about the line
//   hand_trim_roll_deg=0  of the knuckles (plus toward the palm) and about the
//                         fingers (plus turns the palm toward the thumb)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "grip_fit.h"
#include "grip_fit_math.h"
#include "hand_angle_math.h"
#include "hint_model_math.h"

extern void ModLog(const char* format, ...);

// controllers.cpp
const char* ControllersProfileOf(int side);
const char* ControllersSystemName();
const char* ControllersRuntimeName();
void ControllersGesturePulse(float amplitude, float seconds);

namespace {

namespace gf = grip_fit_math;

const char* const kTrimKey[3] = {"grip_trim_ahead_mm", "grip_trim_up_mm",
                                 "grip_trim_out_mm"};

namespace ha = hand_angle_math;

const char* const kHandTrimKey[3] = {"hand_trim_yaw_deg", "hand_trim_pitch_deg",
                                     "hand_trim_roll_deg"};

bool g_read = false;
int g_fit = 1;        // grip_fit
int g_fit_test = 0;   // grip_fit_test
gf::Trim g_trim = {0, 0, 0};
int g_hand_angle = 1;  // hand_angle
ha::Trim g_hand_trim = {0, 0, 0};
char g_ini[MAX_PATH] = "";
uint64_t g_frame = 0;
// The turn of the hand from the frame of the model of each hand, as the log
// gave it last, so a change writes one line.
float g_said_turn[2] = {-1.0f, -1.0f};
int g_hand_lines = 0;
const int kMostHandLines = 16;

// What each hand had at its last line of the log, so a change writes one.
struct Said {
  bool said;
  bool on;
  char runtime[64];
  char model[48];
};
Said g_said[2];
int g_lines = 0;
const int kMostLines = 16;

int ReadKey(const char* path, const char* key, int fallback) {
  return path[0] != 0 ? static_cast<int>(GetPrivateProfileIntA(
                            "bwvr", key, fallback, path))
                      : fallback;
}

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  char path[MAX_PATH] = "";
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash != nullptr && strlen(path) + 9 < MAX_PATH) {
    slash[1] = 0;
    strcat_s(path, "bwvr.ini");
  } else {
    path[0] = 0;
  }
  g_fit = ReadKey(path, "grip_fit", 1) != 0 ? 1 : 0;
  g_fit_test = ReadKey(path, "grip_fit_test", 0) != 0 ? 1 : 0;
  g_trim.ahead_mm = gf::ClampTrim(ReadKey(path, kTrimKey[0], 0));
  g_trim.up_mm = gf::ClampTrim(ReadKey(path, kTrimKey[1], 0));
  g_trim.out_mm = gf::ClampTrim(ReadKey(path, kTrimKey[2], 0));
  g_hand_angle = ReadKey(path, "hand_angle", 1) != 0 ? 1 : 0;
  g_hand_trim.yaw_deg = ha::ClampTrim(ReadKey(path, kHandTrimKey[0], 0));
  g_hand_trim.pitch_deg = ha::ClampTrim(ReadKey(path, kHandTrimKey[1], 0));
  g_hand_trim.roll_deg = ha::ClampTrim(ReadKey(path, kHandTrimKey[2], 0));
  strcpy_s(g_ini, path);
  ModLog("hand angle   : hand_angle=%d. %s. The trim of the hand is %d "
         "degrees of yaw, %d of pitch and %d of roll (hand_trim_yaw_deg, "
         "hand_trim_pitch_deg, hand_trim_roll_deg)",
         g_hand_angle,
         g_hand_angle != 0
             ? "The hand has the angle of the palm pose of the runtime: the "
               "grip pose turned 20.0 degrees about the palm normal, the "
               "fingers toward the thumb end of the handle. The model of the "
               "controller stays on its own frame"
             : "The hand is on the frame of the model of the controller",
         g_hand_trim.yaw_deg, g_hand_trim.pitch_deg, g_hand_trim.roll_deg);
  ModLog("grip fit     : grip_fit=%d. %s. The trim of the player is %d mm "
         "ahead, %d mm up and %d mm out (grip_trim_ahead_mm, grip_trim_up_mm, "
         "grip_trim_out_mm)%s",
         g_fit,
         g_fit != 0
             ? "On Virtual Desktop, the Oculus runtime and SteamVR, the grip "
               "pose of a Meta Touch controller is the frame of its model: "
               "the model, the hand and the Hand of the game go on that "
               "frame"
             : "The model, the hand and the Hand of the game are on the grip "
               "pose of the runtime",
         g_trim.ahead_mm, g_trim.up_mm, g_trim.out_mm,
         g_fit_test != 0
             ? ". grip_fit_test=1: the scripted controller gets the fit of "
               "Virtual Desktop"
             : "");
}

// The fit of a hand now, or null. runtime and model get the names for the
// log.
const gf::Fit* FitOfHand(int side, bool fake, const char** runtime,
                         const char** model) {
  *runtime = fake ? (g_fit_test != 0 ? "VirtualDesktopXR" : "")
                  : ControllersRuntimeName();
  *model = hint_model::ModelOfProfile(ControllersProfileOf(side),
                                      ControllersSystemName());
  if (*runtime == nullptr) *runtime = "";
  return gf::FitOf(*runtime, *model);
}

void SayFit(int side, bool on, const char* runtime, const char* model,
            const gf::Fit* fit) {
  Said& s = g_said[side];
  const char* m = model != nullptr ? model : "";
  if (s.said && s.on == on && strcmp(s.runtime, runtime) == 0 &&
      strcmp(s.model, m) == 0) {
    return;
  }
  s.said = true;
  s.on = on;
  strcpy_s(s.runtime, runtime);
  strcpy_s(s.model, m);
  if (g_lines >= kMostLines) return;
  ++g_lines;
  const char* hand = side == 0 ? "left" : "right";
  if (!on) {
    ModLog("grip fit     : the %s hand, runtime \"%s\", model %s: %s. The "
           "model and the hand are on the grip pose of the runtime, frame "
           "%llu",
           hand, runtime, m[0] != 0 ? m : "none",
           g_fit == 0           ? "grip_fit=0"
           : fit == nullptr     ? "the table has no fit for them"
                                : "no grip pose",
           g_frame);
    return;
  }
  float q[4], offset[3];
  gf::Mirror(*fit, side == 0, q, offset);
  ModLog("grip fit     : the %s hand, runtime \"%s\", model %s: the frame of "
         "the model is turned %.1f degrees from the grip pose of the "
         "runtime, at %.1f %.1f %.1f cm in the frame of the grip (x right, y "
         "up, z back). The middle of the controller moves %.1f cm, frame "
         "%llu",
         hand, runtime, m, gf::TurnDegrees(q), offset[0] * 100.0f,
         offset[1] * 100.0f, offset[2] * 100.0f,
         gf::MiddleMove(q, offset, side == 0) * 100.0f, g_frame);
}

void LogTrim(const char* by) {
  ModLog("grip fit     : %s: the trim is %d mm ahead, %d mm up and %d mm out "
         "now (grip_trim_ahead_mm, grip_trim_up_mm, grip_trim_out_mm), frame "
         "%llu",
         by, g_trim.ahead_mm, g_trim.up_mm, g_trim.out_mm, g_frame);
}

// A line for each hand when the turn of the hand from the frame of the model
// changes by 0.5 degrees or more: at the first frame, at a switch and at a
// trim. The turn is in the frame of the model (x right, y up, z back).
void SayHand(int side, const float frame_q[4], const float hand_q[4]) {
  float axis[3];
  const float turn = ha::TurnBetween(frame_q, hand_q, axis);
  if (g_said_turn[side] >= 0.0f && fabsf(turn - g_said_turn[side]) < 0.5f) {
    return;
  }
  g_said_turn[side] = turn;
  if (g_hand_lines >= kMostHandLines) return;
  ++g_hand_lines;
  float fingers[3], old_fingers[3];
  ha::Fingers(hand_q, fingers);
  ha::Fingers(frame_q, old_fingers);
  ModLog("hand angle   : the %s hand turns %.1f degrees from the frame of the "
         "model of the controller, about %.2f %.2f %.2f of that frame (%.1f, "
         "%.1f and %.1f degrees about x, y and z). Its fingers are %.1f "
         "degrees from the fingers on that frame, frame %llu",
         side == 0 ? "left" : "right", turn, axis[0], axis[1], axis[2],
         turn * axis[0], turn * axis[1], turn * axis[2],
         ha::AngleOf(fingers, old_fingers), g_frame);
}

}  // namespace

void GripFitFrame(bwvr::HandInput hands[2], bool fake, uint64_t frame,
                  float hand_q[2][4]) {
  ReadSettings();
  g_frame = frame;
  for (int side = 0; side < 2; ++side) {
    bwvr::HandInput& h = hands[side];
    // The orientation of the drawn hand, for a hand with no fit or no pose
    // too: the pose that the hand uses, with the trim.
    float runtime_q[4];
    const float* used = h.grip_valid ? h.grip_orientation : h.aim_orientation;
    for (int i = 0; i < 4; ++i) runtime_q[i] = used[i];
    if (!h.active) {
      for (int i = 0; i < 4; ++i) hand_q[side][i] = runtime_q[i];
      continue;
    }
    const char* runtime = "";
    const char* model = nullptr;
    const gf::Fit* fit = FitOfHand(side, fake, &runtime, &model);
    const bool on = g_fit != 0 && fit != nullptr && h.grip_valid;
    SayFit(side, on, runtime, model, fit);
    // The orientation of the aim pose of the runtime, for the axes of the
    // trim. A runtime with no aim pose gives the grip pose.
    float axes[4];
    const float* from = h.aim_valid ? h.aim_orientation : h.grip_orientation;
    for (int i = 0; i < 4; ++i) axes[i] = from[i];
    if (on) {
      float q[4], offset[3];
      gf::Mirror(*fit, side == 0, q, offset);
      gf::Apply(q, offset, h.grip_position, h.grip_orientation);
    }
    if (!gf::TrimZero(g_trim)) {
      float move[3];
      gf::TrimMove(g_trim, side == 0, axes, move);
      for (int i = 0; i < 3; ++i) {
        h.grip_position[i] += move[i];
        h.aim_position[i] += move[i];
      }
    }
    // The palm pose is a turn of the grip pose. A hand with only an aim pose
    // gets the trim alone.
    const float* frame_q =
        h.grip_valid ? h.grip_orientation : h.aim_orientation;
    ha::HandOrientation(runtime_q, frame_q, g_hand_angle != 0 && h.grip_valid,
                        g_hand_trim, side == 0, hand_q[side]);
    SayHand(side, frame_q, hand_q[side]);
  }
}

int GripFitHandAngleValue() {
  ReadSettings();
  return g_hand_angle;
}

void GripFitHandAngleChange(const char* by) {
  ReadSettings();
  g_hand_angle = g_hand_angle != 0 ? 0 : 1;
  g_said_turn[0] = g_said_turn[1] = -1.0f;
  ControllersGesturePulse(0.5f, g_hand_angle != 0 ? 0.30f : 0.08f);
  ModLog("hand angle   : %s: hand_angle is %d now, %s, frame %llu", by,
         g_hand_angle,
         g_hand_angle != 0 ? "the angle of the palm pose of the runtime"
                           : "the hand on the frame of the model",
         g_frame);
}

int GripFitHandTrimValue(int axis) {
  ReadSettings();
  if (axis == 0) return g_hand_trim.yaw_deg;
  if (axis == 1) return g_hand_trim.pitch_deg;
  if (axis == 2) return g_hand_trim.roll_deg;
  return 0;
}

void GripFitHandTrimStep(int axis, int steps, const char* by) {
  ReadSettings();
  if (axis < 0 || axis > 2) return;
  int* value = axis == 0   ? &g_hand_trim.yaw_deg
               : axis == 1 ? &g_hand_trim.pitch_deg
                           : &g_hand_trim.roll_deg;
  *value = ha::ClampTrim(*value + steps * ha::kTrimStepDeg);
  g_said_turn[0] = g_said_turn[1] = -1.0f;
  ControllersGesturePulse(0.4f, 0.05f);
  bool wrote = false;
  if (g_ini[0] != 0) {
    char text[16];
    sprintf_s(text, "%d", *value);
    wrote = WritePrivateProfileStringA("bwvr", kHandTrimKey[axis], text,
                                       g_ini) != 0;
  }
  ModLog("hand angle   : %s: the trim of the hand is %d degrees of yaw, %d of "
         "pitch and %d of roll now (%s %s into bwvr.ini), frame %llu",
         by, g_hand_trim.yaw_deg, g_hand_trim.pitch_deg, g_hand_trim.roll_deg,
         kHandTrimKey[axis], wrote ? "written" : "not written", g_frame);
}

int GripFitValue() {
  ReadSettings();
  return g_fit;
}

void GripFitChange(const char* by) {
  ReadSettings();
  g_fit = g_fit != 0 ? 0 : 1;
  ControllersGesturePulse(0.5f, g_fit != 0 ? 0.30f : 0.08f);
  ModLog("grip fit     : %s: grip_fit is %d now, %s, frame %llu", by, g_fit,
         g_fit != 0 ? "the grip pose is the frame of the model of the "
                      "controller"
                    : "the grip pose of the runtime",
         g_frame);
}

void GripFitSetTrim(int axis, int mm, const char* by) {
  ReadSettings();
  if (axis < 0 || axis > 2) return;
  mm = gf::ClampTrim(mm);
  if (axis == 0) g_trim.ahead_mm = mm;
  if (axis == 1) g_trim.up_mm = mm;
  if (axis == 2) g_trim.out_mm = mm;
  LogTrim(by);
}
