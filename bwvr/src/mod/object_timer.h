// THE TIMER OF THE OBJECTS ON THE LAND (object_timer.cpp, a test aid,
// docs/backend.md, "The timer of the objects on the land"). Each 10 s a block
// of lines in bwvr.log with the time of fn_005E5CD0 (the draw of the objects
// on the land) and of each class of object and each method that it calls.
#pragma once

// mod_main.cpp, at the start. mode: object_timer=0 (the default) no timer
// and no line. 1: the timers start in the play (see object_timer.cpp).
// `sites` is object_timer_sites: more calls to time, as addresses with a
// comma between them, or empty. With test_keys=1, Ctrl Shift F7 starts and
// stops the timers.
void ObjectTimerSetOptions(int mode, const char* sites, bool test_keys);
// h_FinishFrame, after the present of the game: the end of a frame.
void ObjectTimerFrame();
