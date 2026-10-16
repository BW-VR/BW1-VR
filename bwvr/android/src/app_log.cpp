#include "app_log.h"

#include <android/log.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

namespace {

FILE* g_file = nullptr;
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

}  // namespace

void AppLogOpen(const char* folder) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/bwvr-quest.log", folder);
  pthread_mutex_lock(&g_lock);
  if (g_file == nullptr) g_file = fopen(path, "w");
  pthread_mutex_unlock(&g_lock);
}

void AppLog(const char* format, ...) {
  char line[2048];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  __android_log_write(ANDROID_LOG_INFO, "bwvr", line);
  pthread_mutex_lock(&g_lock);
  if (g_file != nullptr) {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    fprintf(g_file, "%6ld.%03ld %s\n", static_cast<long>(now.tv_sec),
            now.tv_nsec / 1000000, line);
    fflush(g_file);
  }
  pthread_mutex_unlock(&g_lock);
}
