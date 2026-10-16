// The log of the headset app: logcat (tag "bwvr") and the file
// bwvr-quest.log in the folder of the app (docs/android.md, "The log").
#pragma once

// The folder of the app on the shared storage. The log file goes there. Until
// a call of this, the lines go to logcat only.
void AppLogOpen(const char* folder);
void AppLog(const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
