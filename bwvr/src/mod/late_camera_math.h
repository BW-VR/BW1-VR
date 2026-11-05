// THE LATE CAMERA (late_camera, docs/android.md, "The late camera"). No Windows
// code, so the renderer, the headset app and the math test use it
// (test/controller_math/late_camera_checks.h).
//
// The game makes the draws of a frame with vertices in pixels of the left eye,
// and with x, y and rhw of a vertex the place of the vertex in the space of the
// eye is known. So the app can draw the newest list of draws again at each
// frame of its session, from the place where the head and the player are now.
// A vertex shader (vk_scene.vert, vk_mesh.vert) gives each vertex that place:
//
//   d      = k / rhw                        meters in front of the eye
//   point  = (tx * d, ty * d, -d) + move    the space of the eye, x right, y up
//   tx, ty = the tangents of the pixel
//
// k comes from the move of the right eye: per_rhw * rhw is the difference of
// the eyes in pixels, and that is separation * pixels_of_a_tangent / d.
//
// THE TURN of the head stays with the runtime. The layer keeps the orientation
// of the picture, and the runtime turns the picture to the head, so the sky and
// each flat thing stay right. The shader takes only the move of the place.
//
// A THING OF THE ROOM (a hand, VkSceneRoomDraws) goes with the player: it takes
// the move of the head, and not the walk of the player in the world.
//
// THE FRUSTUM of the game is symmetric with square pixels (SetRenderHalfAngle),
// so one number gives the tangent of each pixel: the pixels of a tangent. The
// shader takes half the size of the picture from the viewport of the draw.
#pragma once

#include <math.h>
#include <stdint.h>

#include "motion_comp_math.h"

namespace late_camera_math {

namespace mc = motion_comp_math;

// late_camera of bwvr.ini. 0: the picture of each frame of the game shows as the
// game drew it. 1: each frame of the session draws the newest list of draws.
inline bool On(int key) { return key != 0; }

// The last 32 bytes of the push constants of the renderer (vk_scene.vert).
struct Push {
  float move[4];  // x y z: the move of a point of the world in the space of the eye,
                  // meters. w: the separation of the eyes times room[3]. 0: no late camera
  float room[4];  // x y z: the move of a thing of the room. w: the pixels of a tangent
};
static_assert(sizeof(Push) == 32, "Push");

inline Push NoPush() {
  Push p = {};
  return p;
}

struct Keys {
  float most_step = 2.0f;    // meters at the head: more is a cut of the view
  float most_turn = 2.0f;    // degrees: more is a cut of the view
  float most_scale = 0.02f;  // the change of the scale: more is a cut of the view
  int predict_ms = 250;      // the longest walk ahead of the newest map of the mod
  float still = 0.0001f;     // meters: a smaller move is no move
};

// THE WALK AHEAD OF THE MAP, in seconds of the walk at the push of its frame. A
// picture shows the player at the display time of the slot before the newest
// slot, also when the mod is late or when its newest frame is younger than that
// slot. So each frame of the session moves the view by one slot of the stick,
// whichever frame of the mod came. slots: the slots after the slot of the map, up
// to newest_slot. A negative value goes back along the walk of the frame.
inline float AheadSeconds(const input_channel::World& w, const mc::SlotPush* slots, int count,
                          uint32_t newest_slot, float period_s, int most_ms) {
  if ((w.flags & input_channel::kWorldWalk) == 0 || !(w.walk_share > 0.0f)) return 0.0f;
  const uint32_t want = newest_slot > 0 ? newest_slot - 1 : 0;
  if (w.input_frame > want) {
    return -static_cast<float>(w.input_frame - want) * period_s;
  }
  float span = 0.0f;
  return mc::AheadSeconds(w, slots, count, most_ms, &span);
}

// The map of the player now: the newest map, with the walk ahead.
inline mc::Map MapNow(const input_channel::World& newest, float ahead_s) {
  mc::Map m = mc::MapOf(newest);
  for (int i = 0; i < 3; ++i) m.offset[i] += newest.walk_velocity[i] * ahead_s;
  return m;
}

// THE LATE GRAB (late_grab). The mod moves the land with the eased grips once for each frame
// of the game, so with no more the land goes in steps while the session has more frames. One
// hand holds a point of the land under it, so the map at the place of the hand is the same point
// of the world for each place of the hand. The app eases the grips of its own frames in the same
// way, and moves the newest map by the way from the grips of that map to its own. The result
// does not depend on the time of the frames of the game. A turn and a zoom of two hands keep the
// steps, because the shader takes only a move.
struct GrabFilter {
  bool on = false;
  uint32_t hands = 0;  // World::grab_hands
  float hand[2][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
};

// One frame of the session. world: the newest map, or null when the key is off. raw: the grips
// at the display time of the new picture, in the LOCAL space. A hand that joins starts at the
// grip of the map. share: 1 - exp(-period / grab_smoothing_ms).
inline void GrabStep(const input_channel::World* world, const float raw[2][3], const bool ok[2],
                     float share, GrabFilter* f) {
  if (world == nullptr || (world->flags & input_channel::kWorldGrab) == 0) {
    *f = GrabFilter();
    return;
  }
  for (int h = 0; h < 2; ++h) {
    const uint32_t bit = 1u << h;
    if ((world->grab_hands & bit) == 0) continue;
    if (!f->on || (f->hands & bit) == 0) {
      for (int i = 0; i < 3; ++i) f->hand[h][i] = world->grab_hand[h][i];
    }
    if (ok[h]) {
      for (int i = 0; i < 3; ++i) f->hand[h][i] += (raw[h][i] - f->hand[h][i]) * share;
    }
  }
  f->on = true;
  f->hands = world->grab_hands;
}

// The map of a grab now: the land under the hand stays under the hand. Two hands hold the middle
// between them, and only across when the land does not go up and down.
inline void GrabMove(const input_channel::World& world, const GrabFilter& f, mc::Map* map) {
  const uint32_t both = 3u;
  if (!f.on || (world.flags & input_channel::kWorldGrab) == 0 ||
      (world.grab_hands & both) == 0 || (world.grab_hands & both) != (f.hands & both)) {
    return;
  }
  float way[3] = {0.0f, 0.0f, 0.0f};
  int count = 0;
  for (int h = 0; h < 2; ++h) {
    if ((world.grab_hands & (1u << h)) == 0) continue;
    for (int i = 0; i < 3; ++i) way[i] += f.hand[h][i] - world.grab_hand[h][i];
    ++count;
  }
  for (float& v : way) v /= static_cast<float>(count);
  if ((world.grab_hands & input_channel::kGrabVertical) == 0) way[1] = 0.0f;
  float at[3];
  mc::Rotate(map->linear, way, at);
  for (int i = 0; i < 3; ++i) map->offset[i] -= at[i];
}

// THE EVENNESS OF A WALK, for the log: the step of the map now from the tick
// before, against the step that the speed of the walk gives. True when the step
// is more than a third away from it. A step of 0 at a speed counts: the view stood
// for that refresh.
inline bool StepUneven(float step, float speed, float seconds) {
  const float want = speed * seconds;
  if (!(want > 0.0f)) return false;
  return fabsf(step - want) > want / 3.0f;
}

struct Input {
  float eye_position[2][3];   // the eyes of the picture, in the LOCAL space
  float eye_orientation[4];   // the left eye of the picture, x y z w
  float fov[4];               // left, right, up, down in radians, as XrFovf
  uint32_t width, height;     // the pixels of the eye picture
  float head_now[3];          // the middle of the eyes at the display time of the new picture
  bool have_maps;             // both maps are valid
  mc::Map drawn;              // the map of the picture
  mc::Map now;                // the map now (MapNow)
};

struct Result {
  Push push = {};
  float head_move[3] = {0.0f, 0.0f, 0.0f};  // the head of the new picture less the head of the picture
  float world_cm = 0.0f;   // the part of the move that the map gives, centimeters
  float move_cm = 0.0f;    // the whole move, centimeters
  bool cut = false;        // the maps are too far apart: the world stays as it is drawn
};

// The eye space move of a point for the new picture, and the values of the shader.
inline Result Compute(const Input& in, const Keys& keys) {
  Result r;
  const float head[3] = {(in.eye_position[0][0] + in.eye_position[1][0]) * 0.5f,
                         (in.eye_position[0][1] + in.eye_position[1][1]) * 0.5f,
                         (in.eye_position[0][2] + in.eye_position[1][2]) * 0.5f};
  float world[3] = {0.0f, 0.0f, 0.0f};
  if (in.have_maps) {
    mc::Map d;
    if (mc::Difference(in.now, in.drawn, &d)) {
      const mc::Motion m = mc::Measure(d, head);
      mc::Limits limits;
      limits.step = keys.most_step;
      limits.turn = keys.most_turn;
      limits.scale = keys.most_scale;
      if (mc::Within(m, limits)) {
        float moved[3];
        mc::Point(d, head, moved);
        for (int i = 0; i < 3; ++i) world[i] = moved[i] - head[i];
      } else {
        r.cut = true;
      }
    }
  }
  float local[3], local_room[3];
  for (int i = 0; i < 3; ++i) {
    r.head_move[i] = in.head_now[i] - head[i];
    local[i] = world[i] - r.head_move[i];
    local_room[i] = -r.head_move[i];
  }
  r.world_cm = sqrtf(world[0] * world[0] + world[1] * world[1] + world[2] * world[2]) * 100.0f;
  const float length = sqrtf(local[0] * local[0] + local[1] * local[1] + local[2] * local[2]);
  const float length_room = sqrtf(local_room[0] * local_room[0] + local_room[1] * local_room[1] +
                                  local_room[2] * local_room[2]);
  r.move_cm = length * 100.0f;
  // The separation of the eyes and the frustum of the picture. No size, no eyes
  // or another frustum: no late camera, and the picture shows as the game drew it.
  const float dx = in.eye_position[1][0] - in.eye_position[0][0];
  const float dy = in.eye_position[1][1] - in.eye_position[0][1];
  const float dz = in.eye_position[1][2] - in.eye_position[0][2];
  const float separation = sqrtf(dx * dx + dy * dy + dz * dz);
  const float tan_left = tanf(in.fov[0]);
  const float tan_right = tanf(in.fov[1]);
  const float tan_up = tanf(in.fov[2]);
  const float tan_down = tanf(in.fov[3]);
  const bool symmetric = fabsf(tan_left + tan_right) < 0.001f * tan_right &&
                         fabsf(tan_up + tan_down) < 0.001f * tan_up;
  if ((!(length >= keys.still) && !(length_room >= keys.still)) || !(separation > 0.0001f) ||
      !(tan_right > tan_left) || !(tan_up > tan_down) || !symmetric || in.width == 0 ||
      in.height == 0) {
    for (float& v : r.head_move) v = 0.0f;
    r.move_cm = 0.0f;
    r.world_cm = 0.0f;
    return r;
  }
  float turn[9];
  mc::QuatMatrix(in.eye_orientation, turn);
  // The moves in the space of the eye: the matrix of the eye turned back.
  for (int i = 0; i < 3; ++i) {
    r.push.move[i] = turn[0 + i] * local[0] + turn[3 + i] * local[1] + turn[6 + i] * local[2];
    r.push.room[i] =
        turn[0 + i] * local_room[0] + turn[3 + i] * local_room[1] + turn[6 + i] * local_room[2];
  }
  const float pixels = static_cast<float>(in.width) / (tan_right - tan_left);
  r.push.move[3] = separation * pixels;
  r.push.room[3] = pixels;
  return r;
}

// THE PLACE OF A VERTEX, as vk_scene.vert makes it, for the math test: the clip
// position from the pixel, with the viewport of the draw (x, y, 2 / width,
// 2 / height). room: a thing of the room.
struct Vertex {
  float x, y, z, rhw;
};
struct Clip {
  float x, y, z, w;
};
inline Clip ClipOf(const Push& p, const Vertex& v, float per_rhw, const float viewport[4],
                   float min_z, float inv_range, bool room) {
  Clip c = {};
  const float a = p.room[3];
  const float k = p.move[3] / fabsf(per_rhw);
  const float d = k / v.rhw;
  const float tan_x = -1.0f / (viewport[2] * a);
  const float tan_y = 1.0f / (viewport[3] * a);
  const float* move = room ? p.room : p.move;
  const float px = ((v.x + 0.5f) / a + tan_x) * d + move[0];
  const float py = (tan_y - (v.y + 0.5f) / a) * d + move[1];
  const float pz = -d + move[2];
  const float dn = -pz;
  c.w = dn / k;
  c.x = ((px - tan_x * dn) * a / k - viewport[0] * c.w) * viewport[2] - c.w;
  c.y = ((tan_y * dn - py) * a / k - viewport[1] * c.w) * viewport[3] - c.w;
  c.z = ((v.z + v.rhw - min_z) * c.w - 1.0f) * inv_range;
  return c;
}

// The pixels that a draw can move in the picture with a move of `meters`, for the
// test of a draw against the rect of the centre image: a thing at `nearest` meters
// moves the most.
inline float MarginPixels(const Push& p, float nearest) {
  if (!(p.move[3] > 0.0f) || !(nearest > 0.0f)) return 0.0f;
  const float length = sqrtf(p.move[0] * p.move[0] + p.move[1] * p.move[1] + p.move[2] * p.move[2]);
  return length / nearest * p.room[3] + 2.0f;
}

// THE LATE TAKE (late_take_ms, docs/android.md, "The late camera"). A frame of the
// session shows the picture of the tick of the frame before. The xrEndFrame of a
// frame can come some ms after its take, so a picture a little after the start of
// the frame still reaches the display time that the picture is for. What the ring
// has for a tick:
enum class TickPicture {
  kNone,   // no picture of that tick or of a newer tick
  kOnGpu,  // a picture that the GPU did not finish
  kDone,   // a finished picture
};

// True when the wait of the take ends. serial: the tick before, 0 with none.
// handled: the newest tick that the play thread ended its work for, so kNone then
// means that the tick made no picture (the place did not change, no walk).
inline bool TakeWaitEnds(TickPicture picture, uint64_t handled, uint64_t serial, double waited_ms,
                         int most_ms) {
  if (serial == 0 || picture == TickPicture::kDone) return true;
  if (picture == TickPicture::kNone && handled >= serial) return true;
  return waited_ms >= static_cast<double>(most_ms);
}

}  // namespace late_camera_math
