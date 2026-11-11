// The check of the view (docs/controller-hints.md, "The place of the
// controller").
//
// In each frame of the God view, the overlay puts the grip point of each
// hand on the screen of each eye pass with the matrix of the game
// (OverlayDraw). The same grip point, as the runtime gave it in its LOCAL
// space, goes through the view that the projection layer declares for each
// eye (StereoDeclaredView). The two pixels of each eye must be the same
// pixel. The two eyes together give the place where the player sees the
// grip point, and its error along the line of sight is the error of depth
// (view_check_math.h).
//
// So the log says if the chain of the mod (the room, the God view, the head
// on the camera of the game, the eye offset, one walk, the field of view and
// the declared views) puts the controller and the hand at the grip pose that
// the runtime reports. An error that the check does not see is in the grip
// pose itself, or in the model of the controller.
//
// THE SCALE CHECK (docs/controller-hints.md, "The scale that the eyes see").
// The same passes give the units per meter that the player sees, from the
// matrices of the game and the poses of the runtime, not from the numbers of
// the God view:
//
//   the eyes   the cameras of the two eye passes (CameraPosition), or the
//              shift of one walk (OneWalkSeparation), apart in world units,
//              over the declared eyes apart in meters
//   the head   a move of the declared left eye of 5 cm or more while the
//              version of the God view stays: the move of the camera of the
//              left eye pass in world units, over the move in meters. Once
//              with the declared eye of the same frame and once with the
//              declared eye of the frame before (ScaleTake says why), and a
//              line for each of the first 40 moves
//   the hand   the same for the drawn grip point of each hand over the grip
//              of the runtime, and for the Hand of the game (0x00C5E680)
//              over the grip of the pointing hand
//   the walk   the camera that the game gets (0x00EA1DB8) in the frames of a
//   the grab   walk or of a grab of the world, in world units for each
//              second, over the units per meter of the eyes: the speed in
//              the room that the player sees, in all, across the land, and
//              up and down (the follow of the land)
//
// One line for each report window, and one when the scale of the God view
// changes and the frames at the scale before were 30 or more. Each line
// gives the units per meter of the God view beside the measured ones.
//
// THE KEY of bwvr.ini:
//
//   view_check=1   1 the check runs and writes its log lines. 0 no check

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "controllers.h"
#include "view_check_math.h"

extern void ModLog(const char* format, ...);

// controllers.cpp
bool ControllersGripLocal(int index, float out[3]);
uint64_t ControllersFrameCount();
// stereo.cpp
bool StereoDeclaredView(int eye, float position[3], float orientation[4],
                        float fov[4], float size[2]);
// one_walk.cpp
bool OneWalkActive();
float OneWalkShiftPerRhw();
// tabletop.cpp
bool TabletopOwnsView();
float TabletopViewScale();
uint64_t TabletopGodViewVersion();
bool TabletopGodWalkActive();
float TabletopGodMoveScaleFactor();
// controllers.cpp: a grip holds the world. world_grab.cpp: the gain of the
// grab in the last solve.
bool ControllersGrabHeld();
float WorldGrabGainNow();

namespace {

// The game's addresses that the scale check reads. The Hand's world
// position is where sub_0046E160 copies it (0x005B725F). The camera that the
// game gets is the camera that the camera hook hands back (pass_guard.cpp).
const uintptr_t kGameHandPosition = 0x00C5E680;
const uintptr_t kGameCamera = 0x00EA1DB8;
// A move of the head or of a hand counts when it is this long, in meters.
const float kScaleMoveM = 0.05f;
// A line for a scale that changed needs this many frames at the scale
// before.
const uint64_t kScaleLeastFrames = 30;
const int kScaleMostLines = 400;

bool ReadGame3(uintptr_t address, float out[3]) {
  const float* p = reinterpret_cast<const float*>(address);
  if (IsBadReadPtr(p, sizeof(float) * 3)) return false;
  for (int i = 0; i < 3; ++i) out[i] = p[i];
  return out[0] == out[0] && out[1] == out[1] && out[2] == out[2];
}

// The numbers of one frame for the scale check.
struct ScaleFrame {
  uint64_t frame;
  float scale;              // units per meter of the God view
  uint64_t version;         // of the God view
  bool walk, grab;
  float move_factor;        // the factor of the speed by the scale
  float grab_gain;          // the larger gain of the grab (WorldGrabGainNow)
  double seconds;           // the clock at the first pass of the frame
  bool have_cam[2];
  float cam[2][3];          // the cameras of the eye passes, world
  float shift_separation;   // one walk: the eyes apart, world units
  bool have_view[2];
  float declared[2][3];     // the declared eyes, LOCAL meters
  bool have_grip[2];
  float grip[2][3];         // the grips of the runtime, LOCAL meters
  float drawn[2][3];        // the drawn grip points, world
  int pointing;
  bool have_hand;
  float hand[3];            // the Hand of the game, world
  bool have_camera;
  float camera[3];          // the camera that the game gets, world
};
ScaleFrame g_scale_frame = {};

// A place to measure a move from: the eye or the grip in meters, and the
// game point in world units, at one version of the view.
struct Anchor {
  bool have;
  uint64_t version;
  uint64_t frame;
  float room[3];
  float world[3];
};

// One move that AnchorMove measured.
struct Move {
  uint64_t from, to;  // the frames of the anchor and of the move
  float room_m;       // the move of the pose of the runtime
  float units;        // the move of the point of the game
};

// The sums of one scale.
struct ScaleTotals {
  float scale;
  uint64_t frames;
  double eyes_sum;
  float eyes_least, eyes_most;
  uint64_t eyes_n;
  double head_sum;
  uint64_t head_n;
  double head_late_sum;  // with the declared eye of the frame before
  uint64_t head_late_n;
  double step_sum;       // the step watch: the scale at the lag
  uint64_t step_n, step_lost;
  int step_lag_most;
  double drawn_sum;
  uint64_t drawn_n;
  double hand_sum;
  uint64_t hand_n;
  uint64_t walk_frames;
  double walk_units, walk_seconds;
  double walk_across, walk_up;  // the parts across and up or down, units
  double walk_factor_sum;       // the factor of the speed by the scale
  uint64_t grab_frames;
  double grab_units, grab_hand_m;
  float grab_gain_least, grab_gain_most;  // the gain of the grab
};
ScaleTotals g_scale_totals = {};
Anchor g_head_anchor = {};
Anchor g_head_late_anchor = {};
Anchor g_drawn_anchor[2] = {};
Anchor g_hand_anchor = {};
ScaleFrame g_scale_last = {};
bool g_have_scale_last = false;
int g_scale_lines = 0;

double Clock() {
  static LARGE_INTEGER frequency = {};
  if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER count;
  QueryPerformanceCounter(&count);
  return frequency.QuadPart > 0
             ? static_cast<double>(count.QuadPart) / frequency.QuadPart
             : 0.0;
}

// One move against its anchor: the ratio of the world move to the room move
// when the room move is kScaleMoveM or more and the view did not move. A
// change of the version sets a new anchor and gives no ratio.
bool AnchorMove(Anchor* a, uint64_t version, uint64_t frame,
                const float room[3], const float world[3], Move* move) {
  if (!a->have || a->version != version) {
    a->have = true;
    a->version = version;
    a->frame = frame;
    for (int i = 0; i < 3; ++i) {
      a->room[i] = room[i];
      a->world[i] = world[i];
    }
    return false;
  }
  const float moved = view_check::Distance3(room, a->room);
  if (moved < kScaleMoveM) return false;
  move->from = a->frame;
  move->to = frame;
  move->room_m = moved;
  move->units = view_check::Distance3(world, a->world);
  a->frame = frame;
  for (int i = 0; i < 3; ++i) {
    a->room[i] = room[i];
    a->world[i] = world[i];
  }
  return true;
}

// The log lines of single moves of the head, at most this many in a game.
const int kHeadMoveLines = 40;
int g_head_move_lines = 0;

void HeadMoveLine(const Move& m, const char* pairing) {
  if (g_head_move_lines >= kHeadMoveLines) return;
  ++g_head_move_lines;
  ModLog("scale check  : a move of the head of %.2f cm from frame %llu to "
         "frame %llu, with %s: the camera of the left eye pass went %.4f "
         "units, %.3f units per meter", m.room_m * 100.0f, m.from, m.to,
         pairing, m.units, m.units / m.room_m);
}

double Mean(double sum, uint64_t n) { return n > 0 ? sum / n : 0.0; }

// THE STEP WATCH (ScaleTake): the frame of a step of the declared left eye
// and the 5 frames after it.
const uint64_t kStepFrames = 6;
struct StepWatch {
  bool on;
  uint64_t frame;     // the frame of the step
  uint64_t version;   // of the God view
  float base[3];      // the camera of the left eye pass in the frame before
  float step_m;       // the step of the declared eye
  float upm;          // the units per meter of the eyes
  float moved[kStepFrames];  // the camera from base, in each frame
};
StepWatch g_step_watch = {};
int g_step_lines = 0;

// The end of a watch: the lag and the scale at the lag go into the sums,
// and one line (at most 40 in a game).
void StepLine(const StepWatch& w, ScaleTotals* t) {
  const float full = w.step_m * w.upm;
  int lag = -1;
  for (uint64_t k = 0; k < kStepFrames; ++k) {
    if (w.moved[k] >= 0.9f * full) {
      lag = static_cast<int>(k);
      break;
    }
  }
  if (lag >= 0) {
    t->step_sum += w.moved[lag] / w.step_m;
    ++t->step_n;
    if (lag > t->step_lag_most) t->step_lag_most = lag;
  } else {
    ++t->step_lost;
  }
  if (g_step_lines >= 40) return;
  ++g_step_lines;
  char lagged[160];
  if (lag >= 0) {
    sprintf_s(lagged, "the camera has the full move %d frames after the "
              "declared eye, and with the declared eye of %d frames before "
              "the head gives %.3f units per meter", lag, lag,
              w.moved[lag] / w.step_m);
  } else {
    sprintf_s(lagged, "the camera does not have the full move in 6 frames");
  }
  ModLog("scale check  : a step of the head of %.2f cm at frame %llu. The "
         "camera of the left eye pass from its place in the frame before: "
         "%.4f %.4f %.4f %.4f %.4f %.4f units in that frame and the 5 after "
         "(the full move is %.4f units at %.3f units per meter of the eyes). "
         "So %s", w.step_m * 100.0f, w.frame, w.moved[0], w.moved[1],
         w.moved[2], w.moved[3], w.moved[4], w.moved[5], full, w.upm,
         lagged);
}

// The line of a scale. why is the end of the sums: a report window or a new
// scale.
void ScaleLine(const char* why) {
  ScaleTotals& t = g_scale_totals;
  if (t.eyes_n == 0 || g_scale_lines >= kScaleMostLines) return;
  ++g_scale_lines;
  const double eyes = Mean(t.eyes_sum, t.eyes_n);
  char walk[400] = "";
  if (t.walk_frames > 0 && t.walk_seconds > 0.0 && eyes > 0.0) {
    const double to_cm = 100.0 / t.walk_seconds / eyes;
    const double factor = Mean(t.walk_factor_sum, t.walk_frames);
    sprintf_s(walk, " In %llu frames of a walk the camera of the game went "
              "%.2f units in %.2f s: %.1f cm of the room each second at the "
              "units per meter of the eyes, %.1f across the land and %.1f up "
              "and down. The scale makes the speed of the stick %.2f times "
              "move_stick_speed_cm (move_stick_scale), and the speed across "
              "over that factor is %.1f cm each second.", t.walk_frames, t.walk_units, t.walk_seconds,
              t.walk_units * to_cm, t.walk_across * to_cm,
              t.walk_up * to_cm, factor,
              factor > 0.0 ? t.walk_across * to_cm / factor : 0.0);
  }
  char grab[320] = "";
  if (t.grab_frames > 0 && eyes > 0.0) {
    sprintf_s(grab, " In %llu frames of a grab of the world the camera of "
              "the game went %.2f units, %.1f cm of the room at the units "
              "per meter of the eyes, and the grips went %.1f cm. The gain "
              "of the grab was %.2f to %.2f (grab_boost, 1 is one to one).",
              t.grab_frames, t.grab_units, t.grab_units / eyes * 100.0,
              t.grab_hand_m * 100.0,
              t.grab_gain_least > t.grab_gain_most ? t.grab_gain_most
                                                   : t.grab_gain_least,
              t.grab_gain_most);
  }
  ModLog("scale check  : %s. The God view is at %.3f units per meter. The "
         "eye cameras of the game give %.3f units per meter (%.3f to %.3f, "
         "%llu frames), a move of the head %.3f with the declared eye of the "
         "frame before (%llu moves of 5 cm) and %.3f with the declared eye of "
         "the same frame (%llu), a step of the head %.3f at the lag (%llu "
         "steps, the camera at most %d frames after the declared eye, %llu "
         "steps with no full move in 6 frames), a move of the drawn grip "
         "%.3f (%llu), and a move of the Hand of the game %.3f (%llu).%s%s",
         why, t.scale, eyes, t.eyes_least, t.eyes_most, t.eyes_n,
         Mean(t.head_late_sum, t.head_late_n), t.head_late_n,
         Mean(t.head_sum, t.head_n), t.head_n, Mean(t.step_sum, t.step_n),
         t.step_n, t.step_lag_most, t.step_lost,
         Mean(t.drawn_sum, t.drawn_n), t.drawn_n, Mean(t.hand_sum, t.hand_n),
         t.hand_n, walk, grab);
}

// The numbers of the frame that ended go into the sums.
void ScaleTake(const ScaleFrame& f) {
  if (!(f.scale > 0.0f) || !f.have_cam[0] || !f.have_view[0] ||
      !f.have_view[1]) {
    return;
  }
  ScaleTotals& t = g_scale_totals;
  if (t.frames > 0 && fabsf(f.scale - t.scale) > 0.005f * t.scale) {
    if (t.frames >= kScaleLeastFrames) ScaleLine("the scale changed");
    t = {};
    g_head_anchor = {};
    g_head_late_anchor = {};
    g_drawn_anchor[0] = g_drawn_anchor[1] = {};
    g_hand_anchor = {};
  }
  if (t.frames == 0) t.scale = f.scale;
  ++t.frames;
  // The eyes.
  const float ipd = view_check::Distance3(f.declared[0], f.declared[1]);
  float apart = 0.0f;
  if (f.have_cam[1]) {
    apart = view_check::Distance3(f.cam[0], f.cam[1]);
  } else {
    apart = f.shift_separation;
  }
  if (ipd > 0.03f && apart > 0.0f) {
    const float upm = apart / ipd;
    if (t.eyes_n == 0 || upm < t.eyes_least) t.eyes_least = upm;
    if (upm > t.eyes_most) t.eyes_most = upm;
    t.eyes_sum += upm;
    ++t.eyes_n;
  }
  // THE HEAD, with two pairings. With the head on the camera of the game
  // (head_camera.cpp), the camera of an eye pass comes from the main camera
  // call of the game. That call runs before the head pose of its frame is
  // read, so it has the head pose of the frame before (stereo.cpp). In a
  // session the projection layer declares that same pose
  // (g_rendered_position), so the declared eye of the same frame pairs with
  // the camera. On the desktop the declared view of the invented head is the
  // lean of this frame, and the step watch below measures how many frames
  // the camera comes after it. The line gives both pairings.
  Move move = {};
  const ScaleFrame& b = g_scale_last;
  const bool after = g_have_scale_last && b.frame + 1 == f.frame;
  // THE STEP WATCH. A move of the declared left eye of 5 cm or more from one
  // frame to the next is a step (a lean of the invented head, or a
  // recenter). For that frame and the 5 after it the watch takes how far the
  // camera of the left eye pass is from its place in the frame before the
  // step. The first frame with 90 percent of the move at the scale of the
  // eyes gives the lag. A change of the version of the view ends the watch.
  StepWatch& w = g_step_watch;
  if (w.on) {
    const uint64_t k = f.frame - w.frame;
    if (f.version != w.version || k >= kStepFrames) {
      w.on = false;
    } else {
      w.moved[k] = view_check::Distance3(f.cam[0], w.base);
      if (k + 1 == kStepFrames) {
        StepLine(w, &t);
        w.on = false;
      }
    }
  }
  if (!w.on && after && b.have_cam[0] && b.have_view[0] &&
      f.version == b.version && ipd > 0.03f && apart > 0.0f) {
    const float step = view_check::Distance3(f.declared[0], b.declared[0]);
    if (step >= kScaleMoveM) {
      w = {};
      w.on = true;
      w.frame = f.frame;
      w.version = f.version;
      w.step_m = step;
      w.upm = apart / ipd;
      for (int i = 0; i < 3; ++i) w.base[i] = b.cam[0][i];
      w.moved[0] = view_check::Distance3(f.cam[0], w.base);
    }
  }
  if (AnchorMove(&g_head_anchor, f.version, f.frame, f.declared[0], f.cam[0],
                 &move)) {
    t.head_sum += move.units / move.room_m;
    ++t.head_n;
    HeadMoveLine(move, "the declared eye of the same frame");
  }
  if (after && b.have_view[0] &&
      AnchorMove(&g_head_late_anchor, f.version, f.frame, b.declared[0],
                 f.cam[0], &move)) {
    t.head_late_sum += move.units / move.room_m;
    ++t.head_late_n;
    HeadMoveLine(move, "the declared eye of the frame before");
  }
  // The drawn grips and the Hand of the game.
  for (int hand = 0; hand < 2; ++hand) {
    if (!f.have_grip[hand]) continue;
    if (AnchorMove(&g_drawn_anchor[hand], f.version, f.frame, f.grip[hand],
                   f.drawn[hand], &move)) {
      t.drawn_sum += move.units / move.room_m;
      ++t.drawn_n;
    }
  }
  if (f.have_hand && f.pointing >= 0 && f.pointing < 2 &&
      f.have_grip[f.pointing] &&
      AnchorMove(&g_hand_anchor, f.version, f.frame, f.grip[f.pointing],
                 f.hand, &move)) {
    t.hand_sum += move.units / move.room_m;
    ++t.hand_n;
  }
  // The walk and the grab, from the frame before.
  if (after && f.have_camera && b.have_camera &&
      fabsf(b.scale - f.scale) <= 0.005f * f.scale) {
    const float units = view_check::Distance3(f.camera, b.camera);
    if (f.walk && b.walk && f.seconds > b.seconds) {
      ++t.walk_frames;
      t.walk_units += units;
      t.walk_seconds += f.seconds - b.seconds;
      // The world is y up: the part across the land and the part up or down.
      const float dx = f.camera[0] - b.camera[0];
      const float dz = f.camera[2] - b.camera[2];
      t.walk_across += sqrtf(dx * dx + dz * dz);
      t.walk_up += fabsf(f.camera[1] - b.camera[1]);
      t.walk_factor_sum += f.move_factor;
    }
    if (f.grab && b.grab) {
      if (t.grab_frames == 0 || f.grab_gain < t.grab_gain_least) {
        t.grab_gain_least = f.grab_gain;
      }
      if (t.grab_frames == 0 || f.grab_gain > t.grab_gain_most) {
        t.grab_gain_most = f.grab_gain;
      }
      ++t.grab_frames;
      t.grab_units += units;
      float most = 0.0f;
      for (int hand = 0; hand < 2; ++hand) {
        if (!f.have_grip[hand] || !b.have_grip[hand]) continue;
        const float m = view_check::Distance3(f.grip[hand], b.grip[hand]);
        if (m > most) most = m;
      }
      t.grab_hand_m += most;
    }
  }
  g_scale_last = f;
  g_have_scale_last = true;
}


bool g_read = false;
bool g_on = true;  // view_check

void ReadSettings() {
  if (g_read) return;
  g_read = true;
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return;
  char* slash = strrchr(path, '\\');
  if (slash == nullptr) return;
  slash[1] = 0;
  strcat_s(path, "bwvr.ini");
  g_on = GetPrivateProfileIntA("bwvr", "view_check", 1, path) != 0;
  ModLog("view check   : view_check=%d. %s", g_on ? 1 : 0,
         g_on ? "In the God view the mod compares the place where it draws "
                "the grip point of each hand in each eye picture with the "
                "place that the declared views give it"
              : "No check of the view");
}

// The pixels of the game of one frame: [hand][eye].
struct Sample {
  uint64_t frame;
  bool have[2][2];
  float pixel[2][2][2];
  float grip[2][3];        // LOCAL, the point of each hand
  view_check::View view[2];
  bool have_view[2];
  bool done[2];  // the hand has its result in this frame
};
Sample g_sample = {};

// The results of a report window.
struct Totals {
  uint64_t points;
  double along, along_abs, across;
  float along_worst, across_worst, pixel_worst;
  double distance;
  uint64_t no_view;
};
Totals g_totals = {};
bool g_said_first = false;
uint64_t g_report_windows = 0;

const char* HandName(int hand) { return hand == 0 ? "left" : "right"; }

void Finish(int hand) {
  Sample& s = g_sample;
  if (s.done[hand] || !s.have[hand][0] || !s.have[hand][1] ||
      !s.have_view[0] || !s.have_view[1]) {
    return;
  }
  s.done[hand] = true;
  view_check::Result r;
  if (!view_check::CheckPoint(s.view, s.pixel[hand], s.grip[hand], &r)) {
    return;
  }
  Totals& t = g_totals;
  ++t.points;
  t.along += r.along;
  t.along_abs += fabs(r.along);
  t.across += r.across;
  t.distance += r.distance;
  if (fabsf(r.along) > fabsf(t.along_worst)) t.along_worst = r.along;
  if (r.across > t.across_worst) t.across_worst = r.across;
  for (int e = 0; e < 2; ++e) {
    if (r.pixel_error[e] > t.pixel_worst) t.pixel_worst = r.pixel_error[e];
  }
  if (!g_said_first) {
    g_said_first = true;
    float want[2][2];
    for (int e = 0; e < 2; ++e) {
      view_check::DeclaredPixel(s.view[e], s.grip[hand], want[e]);
    }
    const float ipd[3] = {s.view[1].position[0] - s.view[0].position[0],
                          s.view[1].position[1] - s.view[0].position[1],
                          s.view[1].position[2] - s.view[0].position[2]};
    ModLog("view check   : frame %llu, the %s hand. The grip point is %.1f cm "
           "from the middle of the head. The game draws it at pixel %.2f "
           "%.2f of the left eye and %.2f %.2f of the right eye, and the "
           "declared views put it at %.2f %.2f and %.2f %.2f (%.2f and %.2f "
           "pixels apart). The two eyes see it %.2f cm %s the head, and "
           "%.2f cm to the side. The declared eyes are %.1f mm apart, the "
           "pictures are %.0f by %.0f with the tangents %.4f %.4f %.4f %.4f",
           s.frame, HandName(hand), r.distance * 100.0f, s.pixel[hand][0][0],
           s.pixel[hand][0][1], s.pixel[hand][1][0], s.pixel[hand][1][1],
           want[0][0], want[0][1], want[1][0], want[1][1], r.pixel_error[0],
           r.pixel_error[1], fabsf(r.along) * 100.0f,
           r.along < 0.0f ? "nearer to" : "farther from", r.across * 100.0f,
           sqrtf(ipd[0] * ipd[0] + ipd[1] * ipd[1] + ipd[2] * ipd[2]) *
               1000.0f,
           s.view[0].width, s.view[0].height, s.view[0].tan_left,
           s.view[0].tan_right, s.view[0].tan_up, s.view[0].tan_down);
  }
}

}  // namespace

// overlay.cpp, in each eye pass of the overlay, with the matrix of the eye
// pass (OverlayEyeMatrix), the near plane and half the size of the picture.
// With one walk the right eye has no pass of its own: its pixel is the pixel
// of the left eye moved by the move of one walk (one_walk_math.h).
void ViewCheckEye(int eye, const float m[12], float near_plane, float half_x,
                  float half_y) {
  ReadSettings();
  if (!g_on || eye < 0 || eye > 1 || !TabletopOwnsView()) return;
  const uint64_t frame = ControllersFrameCount();
  Sample& s = g_sample;
  if (s.frame != frame) {
    memset(&s, 0, sizeof(s));
    s.frame = frame;
  }
  // THE SCALE CHECK: the frame before goes into the sums, and this frame
  // gets its numbers.
  ScaleFrame& f = g_scale_frame;
  if (f.frame != frame) {
    if (f.frame != 0) ScaleTake(f);
    memset(&f, 0, sizeof(f));
    f.frame = frame;
    f.scale = TabletopViewScale();
    f.version = TabletopGodViewVersion();
    f.walk = TabletopGodWalkActive();
    f.grab = ControllersGrabHeld();
    f.move_factor = TabletopGodMoveScaleFactor();
    f.grab_gain = WorldGrabGainNow();
    f.seconds = Clock();
    f.pointing = -1;
    f.have_hand = ReadGame3(kGameHandPosition, f.hand);
    f.have_camera = ReadGame3(kGameCamera, f.camera);
  }
  f.have_cam[eye] = view_check::CameraPosition(m, f.cam[eye]);
  const bool walk_both = eye == 0 && OneWalkActive();
  if (walk_both) {
    f.shift_separation = view_check::OneWalkSeparation(
        m, OneWalkShiftPerRhw(), near_plane, half_x);
  }
  OverlayHands hands;
  if (!ControllersOverlayHands(&hands)) return;
  f.pointing = hands.pointing;
  for (int e = 0; e < 2; ++e) {
    if (e != eye && !(walk_both && e == 1)) continue;
    float position[3], orientation[4], fov[4], size[2];
    if (!StereoDeclaredView(e, position, orientation, fov, size)) {
      ++g_totals.no_view;
      return;
    }
    view_check::MakeView(position, orientation, fov[0], fov[1], fov[2],
                         fov[3], size[0], size[1], &s.view[e]);
    s.have_view[e] = true;
    f.have_view[e] = true;
    for (int i = 0; i < 3; ++i) f.declared[e][i] = position[i];
  }
  if (eye == 0) {
    for (int hand = 0; hand < 2; ++hand) {
      const OverlayHand& o = hands.hand[hand];
      f.have_grip[hand] = o.valid && ControllersGripLocal(hand, f.grip[hand]);
      for (int i = 0; i < 3; ++i) f.drawn[hand][i] = o.position[i];
    }
  }
  for (int hand = 0; hand < 2; ++hand) {
    const OverlayHand& o = hands.hand[hand];
    if (!o.valid || !ControllersGripLocal(hand, s.grip[hand])) continue;
    float pixel[2], depth = 0.0f;
    if (!view_check::GamePixel(m, half_x, half_y, o.position, pixel, &depth)) {
      continue;
    }
    s.pixel[hand][eye][0] = pixel[0];
    s.pixel[hand][eye][1] = pixel[1];
    s.have[hand][eye] = true;
    if (walk_both && near_plane > 0.0f) {
      // x(right) = x(left) + per_rhw * rhw, with rhw = near / depth.
      s.pixel[hand][1][0] = pixel[0] + OneWalkShiftPerRhw() * near_plane / depth;
      s.pixel[hand][1][1] = pixel[1];
      s.have[hand][1] = true;
    }
    if (s.have[hand][0] && s.have[hand][1]) Finish(hand);
  }
}

// Once a report window, when the check had points.
void ViewCheckReport() {
  // The scale check of the window. Its anchors stay, so a move that spans
  // the end of a window counts.
  ScaleLine("a report window");
  g_scale_totals = {};
  Totals& t = g_totals;
  if (t.points == 0) {
    t = {};
    return;
  }
  ++g_report_windows;
  const double n = static_cast<double>(t.points);
  ModLog("view check   : %llu points of the grip. On average the two eyes "
         "see the grip %.2f cm %s the head (%.2f cm on average without the "
         "sign, the worst %.2f cm), %.2f cm to the side (the worst %.2f cm), "
         "at %.1f cm from the head. The worst gap of a pixel of the game and "
         "a pixel of the declared view is %.2f pixels",
         t.points, fabs(t.along / n) * 100.0, t.along < 0.0 ? "nearer to"
                                                            : "farther from",
         t.along_abs / n * 100.0, t.along_worst * 100.0f,
         t.across / n * 100.0, t.across_worst * 100.0f,
         t.distance / n * 100.0, t.pixel_worst);
  t = {};
}
