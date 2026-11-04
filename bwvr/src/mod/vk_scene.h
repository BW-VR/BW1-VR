// The Vulkan renderer of the scene (vk_scene.cpp, docs/backend.md, "The
// Vulkan renderer").

#pragma once

#include <stdint.h>

#include "fovea_res_math.h"
#include "gpu_mesh_math.h"

// vk_scene of bwvr.ini: 0 (the default) no Vulkan renderer. 1: with
// d3d7_thread=1 the D3D7 thread gives each call of a walk also to the Vulkan
// renderer, which draws both eyes into an image of its own, a layer for each
// eye. vk_scene_test_frame=N: the images of the walk of frame N go to
// bwvr-vk-left-NNNNN.bmp and bwvr-vk-right-NNNNN.bmp beside runblack.exe
// (VkSceneFrame).
void VkSceneSetOptions(int mode, int test_frame);
bool VkSceneOn();

// stereo.cpp, on the frame thread at the start of a frame, before the
// controllers count the frame. `frame` is the count of the controllers, which
// render_thread_test_frame also reads there. When it is vk_scene_test_frame,
// the first walk of this frame writes its images, with the number `name`:
// the number of the eye captures of this frame. So the image of the Vulkan
// renderer and the picture of Direct3D 9 with one number are of one walk.
// capture: the frame saves pictures (F12, eye_capture.cpp), so the walk
// writes its images too.
void VkSceneFrame(uint64_t frame, uint64_t name, bool capture);

// d3d7_thread.cpp, on the frame thread at the start of a walk, before the
// queue takes calls: the state of the device for the walk. Then the start
// goes into the queue.
void VkSceneWalkBegin(void* device7);
// d3d7_thread.cpp, on the frame thread at the end of a walk, before the
// queue empties.
void VkSceneWalkEnd();

// d3d7_thread.cpp, on the D3D7 thread, for each call of the queue.
void VkSceneRenderState(uint32_t state, uint32_t value);
void VkSceneStageState(uint32_t stage, uint32_t type, uint32_t value);
void VkSceneTexture(uint32_t stage, void* surface);
void VkSceneDraw(uint32_t type, uint32_t fvf, const void* vertices,
                 uint32_t vertex_count, const uint16_t* indices,
                 uint32_t index_count);

// vk_scene_follow of bwvr.ini: 1 (the default) in a walk of one walk the
// renderer follows render target 0 of Direct3D 9, the viewport of Direct3D
// 7 and the clears of Direct3D 9: a draw or a clear for another target than
// the eye pictures does not go into them. 0: each draw goes into the eye
// pictures, in the viewport of the start of the walk, and the renderer makes
// no clear. Shift X (test_keys=1) changes it from the next walk.
void VkSceneSetFollow(int on);
void VkSceneFollowToggle(const char* keys);
// one_walk.cpp, in a pass: render target 0 of Direct3D 9 is this surface
// now. On the frame thread or on the D3D7 thread.
void VkSceneTarget(void* surface);
// d3d7_thread.cpp, after a SetViewport of Direct3D 7 in a walk: a
// D3DVIEWPORT7.
void VkSceneViewport(const void* viewport7);
// one_walk.cpp, in a pass: a Clear of Direct3D 9 (rects are D3DRECT).
void VkSceneClear(uint32_t count, const void* rects, uint32_t flags,
                  uint32_t color, float z);

// draw_merge.cpp, on the frame thread: the game writes this surface (a Lock
// or a Blt to it). Its copy in Vulkan is read again at its next use.
void VkSceneSurfaceWritten(void* surface);

void VkSceneReport(uint64_t frames);

// On the frame thread after the end of a walk: the image of the last walk
// of one walk, once for each walk. image is a VkImage with a layer for each
// eye, in the layout of a color attachment and in vk::Core::color_format.
// False when no walk of one walk ended since the last call.
bool VkSceneEyes(uint64_t* image, uint32_t* width, uint32_t* height);
// On the same thread after VkSceneEyes gave true: the rects of the outer and
// the centre image in that picture (fovea_res_layer=1, docs/backend.md, "The
// middle as its own layer"). Atlas::on is false for a picture that is one
// image, and Atlas::mres is false for a picture with no foveated resolution.
void VkSceneEyesAtlas(fovea_res_math::Atlas* out);

// vk_eyes of bwvr.ini, with xr_vulkan=1: 1 (the default) the eye pictures of
// a frame of one walk come from the Vulkan renderer, 0 from Direct3D 9.
// Shift C (test_keys=1) changes it in the session.
void VkSceneSetEyes(bool on);
bool VkSceneEyesWanted();
void VkSceneEyesToggle(const char* keys);
// The image for the eye swapchains of this frame: as VkSceneEyes, and only
// when the walk had the size of the swapchains. One line of the log when it
// did not.
bool VkSceneEyesFor(uint32_t width, uint32_t height, uint64_t* image);

// THE PANEL (vk_panel of bwvr.ini, 1 is the default). In a walk the mod
// binds the panel target of Direct3D 9 for the help text, the subtitle box,
// the Creature panel and the menus (gpu_bridge.cpp, GpuBridgePanelBind).
// With 1 the renderer draws the draws for that target into a panel image of
// its own, of the size of the eyes, cleared to 0 and the depth to 1 at its
// first draw in a walk. The subtitle card and the panel layer of a session
// with Vulkan then take that image. 0: the draws for the panel target are
// not drawn, and the card and the layer take the panel of Direct3D 9.
// Shift Y (test_keys=1) changes it from the next walk.
void VkSceneSetPanel(int on);
void VkScenePanelToggle(const char* keys);
// True when the card and the panel layer take the panel of the renderer.
bool VkScenePanelOn();
// backend=vulkan (gpu_bridge.cpp, GpuBridgePanelBind): the draws after this
// go into the panel (true) or into the eye pictures (false). With no
// Direct3D 9 the panel target is not a surface, so the bind comes here.
void VkScenePanelBind(bool panel);
// On the frame thread after the walks of a frame: the panel image of the
// last walk of this frame that drew into the panel, when it has this size.
// image is a VkImage of one layer, in the layout of a color attachment and
// in vk::Core::color_format. False when no walk of this frame drew into the
// panel, or when the panel has another size.
bool VkScenePanelFor(uint32_t width, uint32_t height, uint64_t* image);
// True when a walk of this frame drew into the panel and the panel has this
// size, on any device and with no device (vk_device=0). With no session
// (xr_input=1) the headset app draws the same panel image itself
// (xr_layers.h).
bool VkScenePanelDrew(uint32_t width, uint32_t height);
// THE SUBTITLE CARD, as GpuBridgePanelReadHalf: the rect x, y, width and
// height of the panel of this frame at half the size, as pixels for the
// CPU: BGRA, the color multiplied by the alpha, each pixel the mean of four.
// The pixels stay valid until VkScenePanelReadDone. Null when no walk of
// this frame drew into the panel, when a step failed, or with no device
// (vk_device=0). It waits for the GPU.
const void* VkScenePanelReadHalf(const int rect[4], int* pitch,
                                 uint32_t* width, uint32_t* height);
void VkScenePanelReadDone();

// THE WALK WITHOUT DIRECT3D 9 (vk_only of bwvr.ini, 1 is the default). In a
// walk of one walk whose eye pictures come from the renderer, the D3D7
// thread gives no draw to dxwrapper: the renderer alone draws them. The
// calls that set a state still go to dxwrapper, because the next walk reads
// its state from the device. It needs vk_panel=1, so the help text has a
// panel. stereo.cpp says for each frame when its walk can go without
// Direct3D 9 (VkSceneOnlyFrame): in a session with Vulkan with the eyes of
// the renderer, or on the desktop with vk_only_test=1. 0: Direct3D 9 draws
// each walk too. Shift U (test_keys=1) changes it from the next walk.
void VkSceneSetOnly(int on, int test);
void VkSceneOnlyToggle(const char* keys);
// vk_only_test of bwvr.ini: the walks of the desktop go without Direct3D 9
// too, so a desktop run shows the pictures of the renderer alone.
bool VkSceneOnlyTest();
// stereo.cpp, on the frame thread before the walks of a frame: the walk of
// one walk of this frame can go without Direct3D 9.
void VkSceneOnlyFrame(bool can);
// d3d7_thread.cpp, on the D3D7 thread: the walk now goes without
// dxwrapper, so a draw goes to the renderer alone.
bool VkSceneOnlyNow();

// THE UPLOADS OF A WALK (vk_upload_queue of bwvr.ini, 1 is the default). A
// texture that the game wrote again (a Lock or a Blt) is read at its next
// use in a walk. 1: its copy to the GPU goes into a command buffer that the
// end of the walk submits before the buffer of the walk, with no wait on the
// D3D7 thread. 0: each copy goes to the GPU at once, and the D3D7 thread
// waits until the GPU is done. Shift I (test_keys=1) changes it from the
// next walk. vk_scene_reread_test=N (a test key): each walk reads N
// textures again, in turn, so a desktop run makes many reads in a short
// time.
void VkSceneSetUploads(int queue, int reread_test);
void VkSceneUploadsToggle(const char* keys);

// THE BACKEND OF THE MOD (backend=vulkan, vk_ddraw.cpp). When it is on, the
// device of the mod gives each call to the renderer on the frame thread,
// with no D3D7 thread: VkSceneWalkBegin and VkSceneWalkEnd at each
// BeginScene and EndScene, and the state, the textures, the viewport, the
// clears and the draws as they come. It turns the renderer on.
void VkSceneSetDirect(bool on);
// backend=vulkan, on the frame thread after a FinishFrame: the image of the
// last walk (a VkImage in the layout of a color attachment), the layer
// that it drew into, and its size, for the session (XrScreen::SubmitVulkan,
// SetEyeVulkan). False when there is none, or when the CPU wrote into the
// render target after it: then the pixels of the CPU are the picture.
bool VkSceneLastPicture(uint64_t* image, uint32_t* layer, uint32_t* width,
                        uint32_t* height);
// THE LATE COMPOSE (vk_compose.h, docs/backend.md, "The compose of the eye
// picture"). The headset app calls VkSceneComposeLate(true) before the first
// walk of a game that it plays with the ring of the pictures. Then a walk
// with fovea_res=1 and the outer image and the centre image in the eye picture
// (fovea_res_layer=0) leaves the eye picture to the app: the image that
// VkSceneEyes gives has no pixels until the app calls VkSceneComposeRecord for
// it. A walk of another kind composes itself.
void VkSceneComposeLate(bool on);
// True when the eye image (the image that VkSceneEyes or
// VkSceneLastPicture gave) is a picture that nobody composed.
bool VkSceneComposePending(uint64_t eye_image);
// Records the compose of the last walk into the command buffer of the app (a
// VkCommandBuffer in the recording state, on the device of the renderer). images
// null: into the eye image of the walk, which is then composed (the app copies
// it as it copies a picture of the walk). images not null: the two images
// (VkImage), one for each eye, of the format `format` (a VkFormat): whole
// image, no load, left in the layout of a color attachment. slot: 0 to 7, a
// place for the vertices that the caller keeps free until the GPU finished the
// command buffer. False when nothing is pending or the module cannot work.
bool VkSceneComposeRecord(void* command_buffer, uint32_t slot, const uint64_t images[2],
                          uint32_t format);
// The images of the compose of a swapchain image that goes.
void VkSceneComposeForget(uint64_t image);
// The views, pipelines and buffers of the compose go (the device must be idle).
void VkSceneComposeStop();
// THE KEYS OF THE DEBUG PAGE (app_keys.h): vk_mres_draws, vk_mres_sort,
// vk_compose_light and vk_pass_store from the next walk. After a call the
// renderer does not read these four keys from bwvr.ini again.
void VkSceneSetAppKeys(int mres_draws, int mres_sort, int compose_light, int pass_store);
bool VkSceneDirect();
// THE VULKAN DEVICE OF THE MOD (vk_device of bwvr.ini, docs/backend.md, "No
// Vulkan device of the mod"). on: 1 (the default) the renderer draws on a
// Vulkan device of its own or of the session. 0 (only with backend=vulkan
// and xr_input=1, mod_main.cpp): the headset app draws the frames of the
// live stream, so the mod makes no Vulkan instance and no device. Each call
// still goes into the stream (vk_stream.h). A walk keeps only what the
// panel quads need: the walk that drew into the panel, and its size
// (VkScenePanelDrew). Each reader of a picture of the renderer gets none.
// Before the first walk.
void VkSceneSetDevice(bool on);
bool VkSceneDevice();
// vk_keep_picture of bwvr.ini, backend=vulkan: 1 (the default) a walk that
// draws, or clears a part, before a clear of the whole target starts with
// the picture of the walk before it, as a render target of Direct3D 7 keeps
// its pixels. 0: each walk starts black. Shift T (test_keys=1) changes it
// from the next walk.
void VkSceneSetKeepPicture(int on);
// A TEST AID, on the frame thread with backend=vulkan: the counts of the
// report since its last line (the walks, the draws, the clears, the walks
// that took the picture of the walk before, the walks that cleared the
// whole target first).
void VkSceneCounts(uint64_t* walks, uint64_t* draws, uint64_t* clears,
                   uint64_t* kept, uint64_t* whole);
void VkSceneKeepPictureToggle(const char* keys);
// FEWER STATE CHANGES AND FEWER DRAWS (vk_bind_skip, vk_draw_sort,
// vk_draw_merge, docs/backend.md): the counts since the start, as the end of
// the last walk left them. Any thread. draws: the draws of the game that
// went into a command buffer of a walk with no foveated resolution.
// commands: the vkCmdDraw and vkCmdDrawIndexed calls for them. moved: the
// draws that the sort gave another place. joined: the draws in a draw of the
// merge. no_room: the draws that a merge left alone, because the ring had
// no room for its indices. skipped: the
// binds and the dynamic states that the renderer left out, because the
// command buffer had the same value.
struct VkSceneDrawCount {
  uint64_t walks;
  uint64_t draws;
  uint64_t commands;
  uint64_t moved;
  uint64_t joined;
  uint64_t no_room;
  uint64_t skipped;
};
void VkSceneDrawCounts(VkSceneDrawCount* out);
// THE TIME OF THE GPU FOR THE TIME LOG OF THE HEADSET APP (time_log,
// docs/android.md, "The time log of the app"), on the thread of the walks:
// since the last call, the walks with a time of the GPU (the timestamps at
// the start and at the end of the command buffer of a walk), the sum and the
// most of those times in ms, and the time that the walks waited on the CPU
// for the fence of their slot (the GPU was not done with the walk two walks
// before), its sum and its most in ms. The counts start again at each call.
// The report of 450 frames has counts of its own.
struct VkSceneGpuTime {
  uint64_t walks;
  double ms;
  double most_ms;
  uint64_t fence_waits;
  double fence_wait_ms;
  double fence_wait_most_ms;
};
void VkSceneTakeGpuTime(VkSceneGpuTime* out);
// The counts of VkSceneTakeGpuTime add up only after a call with true (the
// time log of the headset app). The mod never makes the call.
void VkSceneSetGpuTake(bool on);
// vk_black_check of bwvr.ini, backend=vulkan, a test aid: 1 (the default)
// 16 by 16 points of each eye picture go to the CPU, and a black picture
// gives a line of the log with what its walk drew. 0: no check.
void VkSceneSetBlackCheck(int on);
// vk_untextured of bwvr.ini: 1 (the default) a stage 0 with no texture
// gives the diffuse color for an operation with the texture as an argument,
// as Direct3D does. 0: it reads a white texel with an alpha of 1. Shift R
// (test_keys=1) changes it.
void VkSceneSetUntextured(int on);
void VkSceneUntexturedToggle(const char* keys);
// vk_stage1 of bwvr.ini: 1 (the default) a draw with a color operation and a
// texture on texture stage 1 gets that stage, for example the light maps of
// the Temple. 0: the renderer makes stage 0 alone. Shift E (test_keys=1)
// changes it from the next walk.
void VkSceneSetStage1(int on);
void VkSceneStage1Toggle(const char* keys);
// The picture of the last walk of the left eye (or of a scene with no eye
// pass) into the client area of `window` (an HWND), width by height, with a
// swapchain of Vulkan (vk_window.h). False when it did not go there: then
// VkScenePresentPixels gives it for GDI. With no device (vk_device=0) both
// give no picture.
bool VkScenePresentWindow(void* window, uint32_t width, uint32_t height);
// The picture of the last walk of the left eye (or of a scene with no eye
// pass), at width by height with a linear filter, as BGRA pixels for the
// CPU, for the window on the monitor (see vk_present_late). Null before the
// first walk, and with vk_present_late=1 also at the first present after a
// change of the size.
const uint8_t* VkScenePresentPixels(uint32_t width, uint32_t height,
                                    int* pitch);
// FOVEATED RENDERING (foveation of bwvr.ini, docs/backend.md, "Foveated
// rendering"). level: 0 (the default) each pixel of the eye pictures has the
// full rate. 1 light, 2 medium, 3 strong: the part around the centre of the
// fovea has the full rate and the rest fewer shades (VK_KHR_fragment_shading_
// rate). inner_deg and outer_deg (foveation_inner_deg, foveation_outer_deg)
// replace the degrees of the level when they are not 0. Only the eye
// pictures of the Vulkan renderer have it. show (foveation_show, a test
// key): a colour over the coarse parts, blue for 2 by 2 and red for 4 by 4.
void VkSceneSetFoveation(int level, int inner_deg, int outer_deg, int show);
// The test key: foveation goes on, on with the colour of show, and off.
void VkSceneFoveationToggle(const char* keys);
// FOVEATED RESOLUTION (fovea_res of bwvr.ini, docs/backend.md, "Foveated
// resolution"). on: 0 (the default) the eye pictures have the pixels of the
// render. 1: the draws of the eyes of a walk go into an outer image with
// outer_pct percent of the pixels on each side (fovea_res_outer_pct, 50)
// and into a centre image with center_pct percent (fovea_res_center_pct,
// 100) of a rect inset_deg degrees on each side of the centre of the fovea
// (fovea_res_center_deg, 25). The eye picture is made from the two. Only the
// eye pictures of the Vulkan renderer have it. With no eye tracker the
// centre is down_deg below straight ahead (fovea_res_center_down_deg, 0,
// 0 to 15).
void VkSceneSetFoveaRes(int on, int outer_pct, int center_pct, int inset_deg,
                        int down_deg);
// The test key: fovea_res goes between 0 and 1.
void VkSceneFoveaResToggle(const char* keys);
// stereo.cpp, on the frame thread before the walks of a frame: the centre of
// the fovea as tangents of the eye (x right, y up), from an eye tracker.
// valid false: straight ahead.
void VkSceneSetGaze(bool valid, float gx, float gy);
// vk_texture_reuse of bwvr.ini: 1 (the default) a texture that a walk reads
// again with the same size goes into the image it has, when no draw of the
// walk drew with it yet, and its pixels go into a ring of the slot. 0: each
// read makes a new image and a staging buffer.
void VkSceneSetTextureReuse(int on);
// vk_present_late of bwvr.ini: 1 (the default) VkScenePresentPixels gives
// the picture of the present before, with no wait for the GPU. 0: it waits
// for the picture of this present.
void VkSceneSetPresentLate(int on);
// vk_equal_depth of bwvr.ini (docs/backend.md, "The depth test EQUAL"): 1 (the
// default) draws a draw with ZFUNC EQUAL as LESS_OR_EQUAL with a depth bias of
// a few steps toward the camera (equal_depth_math.h), so a layer passes over the
// mesh under it when the depths differ by the rounding. 0: the test EQUAL of
// the game. The game process has the key and sends it with each draw.
void VkSceneSetEqualDepth(int on);
bool VkSceneEqualDepthWanted();
// The row of the debug menu and the test key (Ctrl Shift 2).
int VkSceneEqualDepthValue();
void VkSceneEqualDepthToggle(const char* keys);
// The draws with ZFUNC EQUAL since the last call: with the bias, and without.
void VkSceneEqualDepthCounts(uint64_t* biased, uint64_t* plain);


// THE MESHES ON THE GPU (gpu_mesh of bwvr.ini, gpu_mesh_math.h, docs/software-tnl.md,
// "The meshes on the GPU"). VkSceneMeshData gives the renderer a mesh of the
// game: `vertices` of 32 bytes (x, y, z, u, v, nx, ny, nz) and `triangles`
// indices of 3 words, kept by the id `mesh`. A hash that the renderer has
// already is no change. VkSceneDrawMesh draws the mesh of an id with the
// state of the device (the texture, the render states and the cull mode,
// as a call of VkSceneDraw) in the left eye of the walk, and in the right eye
// as one walk moves it. `offsets` (3 floats for each vertex, or null) are
// added to the vertices in the space of the mesh. Only with backend=vulkan
// (VkSceneDirect). The calls go into the stream (kMeshData, kMeshDraw).
// It returns false when the renderer of this process dropped the data (not
// ready yet): the mod gives them again with VkSceneMeshRetry, which does not
// write the stream. With no renderer in this process (the game under Wine
// that writes the stream only) it returns true. The shaders and the arena of
// the meshes (64 MB) are made at the first data, not at the start. When the
// arena is full, the meshes that no walk drew for 600 walks give their range
// back and the data wait for it (up to 16 MB).
bool VkSceneMeshData(uint64_t mesh, uint64_t hash, const void* vertices,
                     uint32_t vertex_count, const uint16_t* indices,
                     uint32_t triangles, const uint16_t* group_of_vertex = nullptr,
                     uint32_t group_count = 0);
bool VkSceneMeshRetry(uint64_t mesh, uint64_t hash, const void* vertices,
                      uint32_t vertex_count, const uint16_t* indices,
                      uint32_t triangles, const uint16_t* group_of_vertex = nullptr,
                      uint32_t group_count = 0);
// `group_of_vertex` (a 16 bit ordinal for each vertex) and `group_count`: a
// mesh with a matrix for each group of vertices. A draw of such a mesh
// (gpu_mesh_math::kGroup in its flags) gives `groups`: 12 floats for each
// group (the matrix in the rows of the shader), and with kLit 4 floats for
// each group after them (the light in the space of the group). The renderer
// keeps them in a buffer of its own.
void VkSceneDrawMesh(uint64_t mesh, const gpu_mesh_math::Consts& consts,
                     const float* offsets, uint32_t offset_floats,
                     const float* groups = nullptr, uint32_t group_count = 0);
// gpu_mesh=2 (a test aid on the PC): the renderer draws no move of the right
// eye, so the mod can draw a mesh with the CPU path into the left eye layer
// and with the GPU path into the right eye layer (kOneWalkLeftOnly and
// kOneWalkRightOnly), and the images of a test frame (vk_scene_test_frame)
// give the difference of the two layers in the log.
// The draw of a batch of vertices that comes with the draw (fn_0081C090,
// gpu_mesh_vertices): `vertices` are 32 bytes each (x, y, z, u, v, the color of
// the vertex in the first word of the normal), `indices` 3 for each triangle.
// The vertex shader of the meshes transforms them with the matrix in `consts`.
// They go into the stream as a kMeshDraw with the mesh id kInlineMesh.
void VkSceneDrawVertices(const gpu_mesh_math::Consts& consts, const void* vertices,
                         uint32_t vertex_count, const uint16_t* indices, uint32_t triangles);
void VkSceneSetMesh(int compare);
// In the compare: the next walk with both eyes gives its difference to the log
// (with no file), with this label (the mask of the functions on the GPU path).
void VkSceneCompareNext(uint32_t label);
// The counts since the last call: the draws of meshes, the meshes in the
// arena, their bytes, the data records, the draws with no data, and the
// compare of the last test frame.
struct VkSceneMeshCounts {
  uint64_t draws, data_records, data_bytes, no_data, no_room;
  // Draws with groups that the guard stopped (the draw has fewer matrices than
  // the data have groups), draws with groups left out because the buffers of
  // the groups could not be made, the data that wait for a range (the count
  // and the bytes), the free ranges of the arena, the largest use of the
  // buffer of the groups in a walk (bytes), and the pipelines of the renderer.
  uint64_t bad_groups, groups_off;
  uint32_t wait_count;
  uint64_t wait_bytes;
  uint32_t free_ranges;
  uint64_t group_peak;
  uint32_t pipelines;
  uint32_t meshes;
  uint64_t arena_bytes;
};
void VkSceneTakeMeshCounts(VkSceneMeshCounts* out);

// The values of the test keys for the debug page (vk_eyes, vk_scene_follow,
// vk_panel, vk_only, vk_upload_queue, the step of the foveation key,
// fovea_res, vk_keep_picture, vk_stage1 and vk_untextured), or kDebugCannot
// when the renderer is off. vk_eyes and vk_only are kDebugCannot with
// vulkan_only=1 too.
int VkSceneEyesValue();
int VkSceneFollowValue();
int VkScenePanelValue();
int VkSceneOnlyValue();
int VkSceneUploadsValue();
int VkSceneFoveationValue();
int VkSceneFoveaResValue();
int VkSceneKeepPictureValue();
int VkSceneStage1Value();
int VkSceneUntexturedValue();
