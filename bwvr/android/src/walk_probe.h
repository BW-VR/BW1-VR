// THE GPU PARTS OF THE WALK (gpu_parts, docs/android.md, "The GPU parts of
// the walk"). The renderer of the mod (vk_scene.cpp) calls Vulkan through the
// table vk::f of vk_core.cpp. After its start the app puts its own functions
// into some entries of that table: they count the draws, the pipelines, the
// descriptor writes and the uploads of each command buffer of the renderer,
// put a timestamp at the bounds of each pass, and then call the function of
// the driver. The rules of the parts are in walk_parts.h.
//
//   gpu_parts=1  the counts and the timestamps (the default)
//   gpu_parts=0  the table of the renderer stays as it is
#pragma once

#include "walk_parts.h"

// gpu_parts of bwvr.ini, at the start of the app.
void WalkProbeSetOn(bool on);
bool WalkProbeOn();
// On the thread that plays, after a play of a batch: when the renderer has
// started and the table does not have the functions of the app yet, they go
// in. Nothing when gpu_parts=0.
void WalkProbeInstall();
// The totals since the last call (the time log, each 10 s), then a reset.
void WalkProbeTake(walk_parts::Totals* out);
