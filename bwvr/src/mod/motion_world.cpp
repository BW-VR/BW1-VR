// THE PLACE OF THE PLAYER IN THE WORLD of a frame (motion_world.h).
#include "motion_world.h"

#include <math.h>
#include <string.h>

#include "motion_comp_math.h"

bool HeadCameraWorldMap(float camera[3], float basis[9], float* units, float origin[3],
                        float rotation[4]);
bool TabletopGodWalkRate(float velocity[3], float* share);
void ControllersWalkStick(int* hand, float* dead, float* full);
void HeadCameraMapSlot(uint32_t* slot, int64_t* display_time);
bool WorldGrabHandsOfMap(int* mask, bool* vertical, float room[2][3]);
bool HeadPoseRoomMetersToLocal(const float room[3], float local[3]);
int ControllersGrabHandIndex(int slot);

float MotionWorldViewError(const input_channel::World& world, const float q[4],
                           const float matrix[12]) {
  if ((world.flags & input_channel::kWorldValid) == 0) return -1.0f;
  namespace mc = motion_comp_math;
  float turn[9];
  mc::QuatMatrix(q, turn);
  const float back[3] = {-turn[2], -turn[5], -turn[8]};
  float a[3];
  mc::Rotate(world.linear, back, a);
  const float b[3] = {matrix[2], matrix[5], matrix[8]};
  const float la = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  const float lb = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
  if (!(la > 0.0f) || !(lb > 0.0f)) return -1.0f;
  float dot = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / (la * lb);
  if (dot > 1.0f) dot = 1.0f;
  if (dot < -1.0f) dot = -1.0f;
  return acosf(dot) * 57.29578f;
}

bool MotionWorldOfFrame(uint64_t frame, uint32_t input_frame, int64_t display_time,
                        bool projection, input_channel::World* out) {
  memset(out, 0, sizeof(*out));
  out->frame = frame;
  out->input_frame = input_frame;
  out->display_time = display_time;
  out->frame_slot = input_frame;
  out->walk_hand = -1;
  float camera[3], basis[9], units = 1.0f, origin[3], rotation[4];
  if (!projection || !HeadCameraWorldMap(camera, basis, &units, origin, rotation)) {
    return false;
  }
  const motion_comp_math::Map m =
      motion_comp_math::MakeMap(camera, basis, units, origin, rotation);
  memcpy(out->linear, m.linear, sizeof(out->linear));
  memcpy(out->offset, m.offset, sizeof(out->offset));
  out->flags |= input_channel::kWorldValid;
  // The map is the camera of the main call, and that call comes before the latch
  // of the frame. So the place of the map has the slot of that call.
  HeadCameraMapSlot(&out->input_frame, &out->display_time);
  float share = 0.0f;
  if (TabletopGodWalkRate(out->walk_velocity, &share)) {
    out->flags |= input_channel::kWorldWalk;
    out->walk_share = share;
    int hand = -1;
    ControllersWalkStick(&hand, &out->walk_dead, &out->walk_full);
    out->walk_hand = hand;
  }
  // THE LATE GRAB: the grips that gave this map, so the app can follow the hand between the
  // frames of the game.
  int grab_mask = 0;
  bool grab_vertical = false;
  float grab_room[2][3];
  if (WorldGrabHandsOfMap(&grab_mask, &grab_vertical, grab_room)) {
    for (int slot = 0; slot < 2; ++slot) {
      if (((grab_mask >> slot) & 1) == 0) continue;
      const int hand = ControllersGrabHandIndex(slot);
      if (hand < 0 || hand > 1) continue;
      if (HeadPoseRoomMetersToLocal(grab_room[slot], out->grab_hand[hand])) {
        out->grab_hands |= 1u << hand;
      }
    }
    if (out->grab_hands != 0) {
      out->flags |= input_channel::kWorldGrab;
      if (grab_vertical) out->grab_hands |= input_channel::kGrabVertical;
    }
  }
  return true;
}
