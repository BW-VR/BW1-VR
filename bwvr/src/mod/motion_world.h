// THE PLACE OF THE PLAYER IN THE WORLD of a frame, for the headset app
// (motion_comp, docs/android.md, "The motion of the stick"). With xr_input=1
// stereo.cpp gives it to the app two ways: after the ViewBody of the kView of
// the frame in the live stream, so each picture has the map of its frame,
// and into the input channel (XrInputWriteWorld), so the session of the app
// has the newest map of the mod.
#pragma once

#include <stdint.h>

#include "../common/input_channel.h"

// A TEST AID: the angle in degrees between the direction of a view of the layer
// (q: x y z w in the LOCAL space, through the map of the frame) and the direction
// of the camera matrix of the engine (12 floats, the rows in the order of
// RightAxis of stereo.cpp). -1 with no map.
float MotionWorldViewError(const input_channel::World& world, const float q[4],
                           const float matrix[12]);

// The place of this frame: the map of the main camera call
// (head_camera.cpp, HeadCameraWorldMap), the recenter pose of head_pose.cpp,
// and the walk with the stick of the frame (tabletop.cpp). frame: the frame
// of the controllers, input_frame and display_time: the slot of the input of
// the frame. projection: the frame has a projection layer with the poses of
// SetRenderPose. Without a map, or with no projection, out has no
// kWorldValid. True when out has kWorldValid.
bool MotionWorldOfFrame(uint64_t frame, uint32_t input_frame, int64_t display_time,
                        bool projection, input_channel::World* out);
