// THE BACKEND OF THE MOD ON VULKAN (backend=vulkan, vk_ddraw.cpp,
// docs/backend.md, "The backend of the mod on Vulkan").
//
// DirectDraw 7 and Direct3D 7 of the mod in place of dxwrapper and
// Direct3D 9. The surfaces keep their pixels in the memory of the CPU, and
// the device gives each scene to the Vulkan renderer (vk_scene.cpp).
#pragma once

#include "gpu_mesh_math.h"

// The proxy loaded no backend DLL for backend=vulkan. This takes the two
// exports that make a DirectDraw object (ProxySetCreateHooks) and puts the
// Vulkan renderer in its direct mode. In DllMain, before the game makes its
// DirectDraw object.
// lines: vk_lines of bwvr.ini (1: a draw of points or lines becomes
// triangles, 0: it is not drawn).
void VkDdrawInstall(int lines);
bool VkDdrawOn();
// THE MESHES ON THE GPU (gpu_mesh.cpp): the render states of the device that
// decide if a mesh takes the GPU path. `device` is the Direct3D 7 device of the
// game (0x00ECA638). False when the backend is not on.
bool VkDdrawMeshState(void* device, gpu_mesh_math::DeviceState* out);
// A count of the changes of a render state, a stage state or a texture of the
// device (a call that the device did not drop). The hook of the meshes asks if
// the material of a mesh changed the device (gpu_mesh_timer, gpu_mesh_material_skip).
uint32_t VkDdrawStateSerial();

// For the report of each 450 frames: the calls of each method that the
// backend does not make, and the counts of the objects.
void VkDdrawReport(unsigned long long frames);
