// THE PLACE OF THE PLAYER IN THE WORLD of a frame (motion_world.h).
#include "motion_world.h"

#include <string.h>

#include "motion_comp_math.h"

bool HeadCameraWorldMap(float camera[3], float basis[9], float* units, float origin[3],
                        float rotation[4]);
bool TabletopGodWalkRate(float velocity[3], float* share);
void ControllersWalkStick(int* hand, float* dead, float* full);

bool MotionWorldOfFrame(uint64_t frame, uint32_t input_frame, int64_t display_time,
                        bool projection, input_channel::World* out) {
  memset(out, 0, sizeof(*out));
  out->frame = frame;
  out->input_frame = input_frame;
  out->display_time = display_time;
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
  float share = 0.0f;
  if (TabletopGodWalkRate(out->walk_velocity, &share)) {
    out->flags |= input_channel::kWorldWalk;
    out->walk_share = share;
    int hand = -1;
    ControllersWalkStick(&hand, &out->walk_dead, &out->walk_full);
    out->walk_hand = hand;
  }
  return true;
}
