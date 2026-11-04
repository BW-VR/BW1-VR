// The meshes of the 3D parts on the GPU (gpu_mesh.cpp, docs/software-tnl.md,
// "The meshes on the GPU").

#pragma once

#include <stdint.h>

// gpu_mesh of bwvr.ini: 0 (the default) the meshes take the CPU path of the
// game. 1: the meshes of fn_00856D40, fn_0085AB10, fn_0084BA90, fn_0085BA30
// and fn_00859530 that are opaque, with one matrix, take the GPU path: the
// renderer transforms, lights, clips and culls them. 2 (a test aid on the PC,
// with one_walk=1): the CPU path draws each such mesh into the left eye layer,
// and the GPU path draws the same mesh into the right eye layer, and a test
// frame (vk_scene_test_frame) gives the difference of the two layers in the
// log. It needs backend=vulkan. With test_keys=1, Ctrl Shift 7 steps gpu_mesh
// between 1 and 0 (the debug page has the same switch). With gpu_mesh=2 in
// bwvr.ini the key leaves mode 2 for 0.
// gpu_mesh_compare_every=N (a test aid, only with gpu_mesh=2): each Nth frame the
// log has the difference of the two layers, for the functions on the GPU path
// and then for each function alone.
// gpu_mesh_kinds=N: bit k of N leaves function k of the list (fn_00856D40 0,
// fn_0085AB10 1, fn_0084BA90 2, fn_0085BA30 3, fn_00859530 4) on the GPU path.
// 31 is the default.
// gpu_mesh_blend, gpu_mesh_stage1, gpu_mesh_uvshift and gpu_mesh_group (1 is the
// default): 0 leaves that kind of mesh on the CPU path. With test_keys=1, Ctrl
// Shift 6 turns the four and the depth off and on.
// gpu_mesh_depth (1 is the default): 1 the vertex shader makes the depth and
// the w as the game does, so two layers on one surface (the snow on a roof)
// keep the depth order of the CPU path. 0 the linear form. It is one of the
// parts that Ctrl Shift 6 turns off and on.
// gpu_mesh_check_frames=N (30 by default): a mesh has a full sum over its
// words each N frames, and a cheap sample at each draw. 0: the full sum at
// each draw. gpu_mesh_timer=1: the log line has the time of each part of the
// hook (it costs a clock read for each part).
// gpu_mesh_vertices (0 is the default): 1 the batches of vertices that fn_0081C090
// draws (text, icons, effects: positions, a color and texture coordinates for
// each vertex, given by the caller in each call) take the GPU path too: the
// vertices go with the draw (a kMeshDraw with the id kInlineMesh) and the vertex
// shader of the meshes transforms them. With test_keys=1, Ctrl Shift 4 turns it
// on and off.
// gpu_mesh_latch (1 is the default): a layer (blend or no depth write, depth test
// EQUAL or LESSEQUAL) from a function that draws a mesh class on the CPU path,
// right after a mesh of the five functions, sends the meshes of that 3D part to
// the CPU path for the next 90 frames, so the layer and the mesh under it have the
// same path. 0: no latch. With test_keys=1, Ctrl Shift 3 turns it on and off.
// gpu_mesh_material_skip (0 is the default): 1 leaves the run of the material out
// for the mesh of the run before when the device had no change since.
struct GpuMeshOptions {
  int mode = 0;
  bool test_keys = false;
  int compare_every = 0;
  int kinds = 31;
  int blend = 1, stage1 = 1, uvshift = 1, group = 1, depth = 1;
  int check_frames = 30;
  int timer = 0;
  int vertices = 0;
  int latch = 1;
  int material_skip = 0;
};
void GpuMeshSetOptions(const GpuMeshOptions& options);
void GpuMeshInstall();
// h_FinishFrame, after the present: the test key and the report.
void GpuMeshFrame();
// h_DrawTriangle (via 0) and h_DrawAndClip (via 1), at their start: the count of
// the layers on the CPU path right after a mesh on the GPU path.
void GpuMeshCpuBatch(uint32_t caller, int via);
// The debug page: the value (0 CPU, 1 GPU) or kDebugCannot, and the switch.
// With gpu_mesh=0 and test_keys=1 the hooks are put in at the first switch.
int GpuMeshValue();
void GpuMeshToggle(const char* keys);
// The second switch: the new parts of the path (blend, stage 1, the shift of
// the texture and the groups) on as the keys say, or all off.
int GpuMeshPartsValue();
void GpuMeshPartsToggle(const char* keys);
