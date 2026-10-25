// The log of the headset app: logcat (tag "bwvr") and the file
// bwvr-quest.log in the folder of the app (docs/android.md, "The log").
#pragma once

// The folder of the app on the shared storage. The log file goes there. Until
// a call of this, the lines go to logcat only.
void AppLogOpen(const char* folder);
// THE TIME LOG (time_log=1, time_log.cpp): names the thread that calls it
// ("bwvr reader", at most 15 characters), so the lines of the threads name
// it. With time_log=0 the thread keeps its name.
void TimeLogNameThread(const char* name);
void AppLog(const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
