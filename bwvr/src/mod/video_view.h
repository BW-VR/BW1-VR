// THE VIEW FOR VIDEOS (video_view.cpp, docs/backend.md, "The view for
// videos"). With video_view=1 and backend=vulkan the window of the game on
// the PC shows a 16:9 area of the left eye picture, for a capture with OBS
// or a similar tool while the headset plays. The area follows a smoothed
// head, and faint corner marks in the eye pictures show it in the headset.

#pragma once

// mod_main.cpp: the keys video_view, video_view_down_pct,
// video_view_fov_deg, video_view_smooth_ms and video_view_marks.
void VideoViewSetOptions(int on, int down_pct, int fov_deg, int smooth_ms,
                         int marks);

// stereo.cpp: the orientation (x y z w, in the space of the head poses) that
// the eye pictures of this frame are drawn from.
void VideoViewHead(const float orientation[4]);

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

// For the report of each 450 frames.
void VideoViewReport();

// THE SWITCHES of the debug page: the view, the marks and the height.
int VideoViewValue();
void VideoViewChange(const char* by);
int VideoViewMarksValue();
void VideoViewMarksChange(const char* by);
int VideoViewDownValue();
void VideoViewLowerChange(const char* by);
void VideoViewHigherChange(const char* by);
