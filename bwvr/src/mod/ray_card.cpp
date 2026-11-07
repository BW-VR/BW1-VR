// The ray of the pointing hand over the panels in the room. See
// docs/ui-layer.md, "The ray over the panels", and ray_card_math.h.
//
// WHY. A panel in the room is a quad layer, and the runtime draws a quad
// layer over the eye pictures. So a panel hides the part of the ray in the
// eye pictures that is in front of it, and the player cannot see where the
// ray points on the panel.
//
// WHAT. overlay.cpp calls RayCardFrame once a frame, when it draws the ray of
// the menu. While a panel in the room can hide that ray (ControllersPanelRay),
// this file draws the ray into a picture of its own and stages it as the
// quad layer kCardRay, which the runtime draws after all the panels. The
// overlay then draws no ray in the eye pictures, so the player sees one ray.
// The strip has the look of the ray in the eye pictures (ray_glow and
// ray_glow_only), and a round mark at the end.
//
//   ray_over_panels=1     0: the ray stays in the eye pictures, under the
//                         panels
//   ray_card_test_save=0  N: saves the first N pictures of the strip as
//                         bwvr-raycard-<frame>.bmp over a checkerboard (a
//                         test key)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdint.h>

#include "../vr/xr_screen.h"
#include "controllers.h"
#include "menu_room.h"
#include "ray_card_math.h"

extern void ModLog(const char* format, ...);

// eye_capture.cpp
void EyeCaptureSaveUi(const char* name, uint64_t frame, int style,
                      const void* pixels, int pitch, uint32_t width,
                      uint32_t height);

namespace {

namespace rc = ray_card_math;

bool g_on = true;
int g_save = 0;
int g_saved = 0;
uint8_t g_pixels[rc::kAcross * rc::kAlong * 4];
// The log: the first strip, each start and end of a run of frames with the
// strip (40 at the most), and the count in each report.
bool g_said_first = false;
bool g_said_failed = false;
bool g_was_on = false;
uint64_t g_run_start = 0;
int g_run_logs = 0;
const int kMaxRunLogs = 40;
uint64_t g_frames = 0;

}  // namespace

void RayCardSetOptions(bool on, int test_save) {
  g_on = on;
  g_save = test_save < 0 ? 0 : test_save;
  ModLog("ray card     : ray_over_panels=%d: %s", on ? 1 : 0,
         on ? "while a panel in the room is open, the ray of the pointing "
              "hand goes out as a quad layer over the panels"
            : "the ray stays in the eye pictures, and a panel in the room "
              "hides the part of it in front of the panel");
  if (g_save > 0) {
    ModLog("ray card     : ray_card_test_save=%d (a test)", g_save);
  }
}

int RayCardValue() { return g_on ? 1 : 0; }

void RayCardChange(const char* by) {
  g_on = !g_on;
  ModLog("ray card     : %s: ray_over_panels=%d. %s", by, g_on ? 1 : 0,
         g_on ? "The ray goes over the panels in the room"
              : "The ray stays in the eye pictures, under the panels");
}

// The run of frames with the strip ends in this frame.
void RayCardEndRun(uint64_t frame) {
  if (!g_was_on) return;
  g_was_on = false;
  if (g_run_logs < kMaxRunLogs) {
    ++g_run_logs;
    ModLog("ray card     : the ray is no longer over the panels, frame %llu, "
           "after %llu frames", frame, frame - g_run_start);
  }
}

// Once a frame. ray: the ray of the menu shows in this frame. True when it is
// on its own quad layer: then the caller draws no ray in the eye pictures.
// glow_half_m and tube_radius_m are those of the ray in the eye pictures, rgb
// its color, and seconds the clock of the glow of this frame.
bool RayCardFrame(bool ray, bool glow, bool only, float glow_half_m,
                  float tube_radius_m, const float rgb[3], float seconds,
                  uint64_t frame) {
  if (!ray || !g_on) {
    RayCardEndRun(frame);
    return false;
  }
  if (bwvr::XrScreen::CardFailed(bwvr::kCardRay)) {
    if (!g_said_failed) {
      g_said_failed = true;
      ModLog("ray card     : the runtime refused the quad layer of the ray, "
             "so the ray stays in the eye pictures, frame %llu", frame);
    }
    return false;
  }
  float from[3], to[3], head[3], head_q[4];
  if (!ControllersPanelRay(from, to) || !MenuRoomHeadPose(head, head_q)) {
    RayCardEndRun(frame);
    return false;
  }
  const rc::Look look = rc::LookOf(glow, only, glow_half_m, tube_radius_m);
  rc::Quad quad;
  float length = 0.0f;
  if (!rc::Place(from, to, head, look, &quad, &length)) {
    RayCardEndRun(frame);
    return false;
  }
  rc::Draw(look, rgb, length, seconds, g_pixels);
  bwvr::CardLayer layer = {};
  layer.visible = true;
  layer.pixels = g_pixels;
  layer.texture_width = rc::kAcross;
  layer.texture_height = rc::kAlong;
  layer.rect[0] = 0;
  layer.rect[1] = 0;
  layer.rect[2] = rc::kAcross;
  layer.rect[3] = rc::kAlong;
  for (int i = 0; i < 3; ++i) layer.position[i] = quad.position[i];
  for (int i = 0; i < 4; ++i) layer.orientation[i] = quad.orientation[i];
  layer.width = quad.width;
  layer.height = quad.height;
  // The bands of the glow move, so each frame is a new picture.
  layer.serial = bwvr::XrScreen::NewCardSerial();
  bwvr::XrScreen::StageCard(bwvr::kCardRay, layer);
  if (g_saved < g_save) {
    ++g_saved;
    EyeCaptureSaveUi("raycard", frame, 1, g_pixels, rc::kAcross * 4,
                     rc::kAcross, rc::kAlong);
  }
  ++g_frames;
  if (!g_was_on) {
    g_was_on = true;
    g_run_start = frame;
    // The angle between the front of the strip and the way to the head.
    float front[3];
    const float z[3] = {0.0f, 0.0f, 1.0f};
    rc::Rotate(quad.orientation, z, front);
    float to_head[3];
    for (int i = 0; i < 3; ++i) to_head[i] = head[i] - quad.position[i];
    const float lh = sqrtf(rc::Dot(to_head, to_head));
    const float facing =
        lh > 1e-6f ? acosf(fmaxf(-1.0f, fminf(1.0f, rc::Dot(front, to_head) /
                                                    lh))) * 57.29578f
                   : 0.0f;
    if (!g_said_first || g_run_logs < kMaxRunLogs) {
      if (g_said_first) ++g_run_logs;
      g_said_first = true;
      ModLog("ray card     : the ray goes over the panels as a quad layer, "
             "frame %llu: from %.3f %.3f %.3f to %.3f %.3f %.3f m (%.2f m), "
             "a strip %.1f cm across and %.2f m long at %.3f %.3f %.3f m. "
             "Its front is %.1f degrees from the way to the head. The look: "
             "%s, and a mark of %.1f cm at the end. The eye pictures have no "
             "ray", frame, from[0], from[1], from[2], to[0], to[1], to[2],
             length, quad.width * 100.0f, quad.height, quad.position[0],
             quad.position[1], quad.position[2], facing,
             look.only ? "the glow alone"
                       : (look.glow ? "a tube under a glow" : "a plain tube"),
             look.end_radius * 200.0f);
    }
  }
  return true;
}

void RayCardReport() {
  if (g_frames == 0) return;
  ModLog("ray card     : %llu frames had the ray over the panels", g_frames);
  g_frames = 0;
}
