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
// log. It needs backend=vulkan. With test_keys=1, Ctrl Shift F10 steps
// gpu_mesh through 1, 0 and 2.
void GpuMeshSetOptions(int mode, bool test_keys);
void GpuMeshInstall();
// h_FinishFrame, after the present: the test key and the report.
void GpuMeshFrame();
// The debug page: the value (0 CPU, 1 GPU) or kDebugCannot, and the switch.
int GpuMeshValue();
void GpuMeshToggle(const char* keys);
