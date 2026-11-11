// THE BACKEND OF THE MOD ON VULKAN (backend=vulkan, vk_ddraw.cpp,
// docs/backend.md, "The backend of the mod on Vulkan").
//
// DirectDraw 7 and Direct3D 7 of the mod in place of dxwrapper and
// Direct3D 9. The surfaces keep their pixels in the memory of the CPU, and
// the device gives each scene to the Vulkan renderer (vk_scene.cpp).
#pragma once

// The proxy loaded no backend DLL for backend=vulkan. This takes the two
// exports that make a DirectDraw object (ProxySetCreateHooks) and puts the
// Vulkan renderer in its direct mode. In DllMain, before the game makes its
// DirectDraw object.
// lines: vk_lines of bwvr.ini (1: a draw of points or lines becomes
// triangles, 0: it is not drawn). surface_heap: vk_surface_heap of bwvr.ini
// (1: the pixels of a surface come from the heap, 0: from a block of
// VirtualAlloc for each surface).
void VkDdrawInstall(int lines, int surface_heap);
// vk_blt_blocks of bwvr.ini (vk_ddraw.cpp): 1 a Blt makes DXT1 blocks from
// RGB pixels and copies the blocks of a rect, 0 the old way.
void VkDdrawSetBlocks(int on);
// vk_dxt_fit of bwvr.ini (vk_ddraw.cpp): 1 the blocks of that Blt get the
// fit of least squares (save_picture::FitDxt1Block), 0 EncodeDxt1Block alone.
void VkDdrawSetBlockFit(int on);
bool VkDdrawOn();

// For the report of each 450 frames: the calls of each method that the
// backend does not make, and the counts of the objects.
void VkDdrawReport(unsigned long long frames);
