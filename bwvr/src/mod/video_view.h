// THE VIEW FOR VIDEOS (video_view.cpp, docs/backend.md, "The view for
// videos"). With video_view=1 and backend=vulkan the window of the game on
// the PC shows a 16:9 area of the left eye picture, for a capture with OBS
// or a similar tool while the headset plays. The area follows a smoothed
// head, and faint corner marks in the eye pictures show it in the headset.

#pragma once

#include <stdint.h>

// mod_main.cpp: the keys video_view, video_view_down_pct,
// video_view_fov_deg, video_view_smooth_ms, video_view_marks and
// video_view_layers.
void VideoViewSetOptions(int on, int down_pct, int fov_deg, int smooth_ms,
                         int marks, int layers);

// stereo.cpp: the pose of the left eye (meters and x y z w, in the space of
// the head poses) that the eye pictures of this frame are drawn from.
void VideoViewHead(const float position[3], const float orientation[4]);

// stereo.cpp, after the eye passes of a frame with no headset: the record of
// the quad layers that the mod staged (XrScreen::RecordVideoLayers).
void VideoViewEndPasses(bool to_headset);

// stereo.cpp: once a frame, before the eye passes. It latches the area of
// the frame for the marks and for the window.
void VideoViewFrame();

// sky_detail.cpp, after the vignette in each eye pass: the corner marks.
void VideoViewDrawMarks();

// present_watch.cpp: true when the copy of the frame to the window goes out
// also in a headset session, because the window shows the video.
bool VideoViewPresentWanted();

// vk_scene.cpp: the part of the picture of the window. from_eye: the
// picture is the left eye of an eye pass of this frame. Writes the area as
// parts of the picture (x0, y0, x1, y1 from 0 to 1) and the shape (width
// over height) of its place in the window. False with video_view=0: the
// window shows the whole picture on the whole client area.
bool VideoViewWindow(bool from_eye, float uv[4], float* aspect);

// THE LAYERS IN THE VIDEO (video_view_layers=1): the quad layers of the
// headset over the area, in the order of the runtime.
const int kVideoWhite = 0;   // a color only (a wall of the cut scene window)
const int kVideoView = 1;    // a VkImageView of a copy of a card on the GPU
const int kVideoPixels = 2;  // the pixels of the CPU of a card
const int kVideoPanel = 3;   // the VkImage of the interface panel
const int kVideoDraws = 16;
struct VideoDraw {
  // Top left, top right, bottom left, bottom right: x and y from 0 to 1
  // across and down the area after the division by w, z for the cut at the
  // near limit, and w (video_layers_math.h, ToArea).
  float corner[4][4];
  float uv[4];     // the part of the texture: u0, v0, u1, v1
  float color[4];  // the factor of each texel, with the alpha multiplied in
  int source;      // kVideoWhite ...
  uint64_t handle;  // the view (kVideoView) or the panel image (kVideoPanel)
  uint32_t format;  // the VkFormat of the view
  // kVideoPixels: the pixels of the CPU of the card, BGRA, of a picture of
  // texture_width by texture_height, and the part that shows.
  const uint8_t* pixels;
  uint32_t texture_width;
  uint32_t texture_height;
  int32_t rect[4];
  int card;
};
// The draws over the area `uv` of VideoViewWindow, for this frame. 0 with
// video_view_layers=0, with no record of this frame, and with no quad in the
// area.
int VideoViewDraws(const float uv[4], VideoDraw out[kVideoDraws]);

// For the report of each 450 frames.
void VideoViewReport();

// THE SWITCHES of the debug page: the view, the marks and the height.
int VideoViewValue();
void VideoViewChange(const char* by);
int VideoViewMarksValue();
void VideoViewMarksChange(const char* by);
int VideoViewLayersValue();
void VideoViewLayersChange(const char* by);
int VideoViewDownValue();
void VideoViewLowerChange(const char* by);
void VideoViewHigherChange(const char* by);
