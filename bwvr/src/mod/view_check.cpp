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
// THE KEY of bwvr.ini:
//
//   view_check=1   1 the check runs and writes its log lines. 0 no check

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>
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

namespace {

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
  const bool walk_both = eye == 0 && OneWalkActive();
  OverlayHands hands;
  if (!ControllersOverlayHands(&hands)) return;
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
