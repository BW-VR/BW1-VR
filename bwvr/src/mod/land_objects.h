// The objects on the land with SSE and worker threads (land_objects.cpp,
// docs/backend.md, "The objects on the land").

#pragma once

// land_objects of bwvr.ini: 0 the vertex loop of the reflection meshes
// (fn_00852C40) on the x87 unit, 1 the loop with SSE, 2 a test aid: the loop
// of the game runs, and the mod compares a copy that the SSE loop makes with
// it. land_objects_threads: 0 one thread, N more worker threads for the SSE
// loop of a large mesh. With test_keys=1, Ctrl Shift F8 steps land_objects
// through 1, 0 and 2, and Ctrl Shift F9 switches the threads off and on.
void LandObjectsSetOptions(int mode, int threads, bool test_keys);
void LandObjectsInstall();
// h_FinishFrame, after the present: the test keys and the report.
void LandObjectsFrame();
