// A TEST AID: the file of the remote control (remote_control of bwvr.ini,
// docs/android.md, "The test run"). The file bwvr-control.txt has lines
// "key=value". The first line is "serial=N". A reader acts only on a new serial,
// so the writer replaces the whole file for each step of a test.
#pragma once

#include <string.h>

namespace remote_math {

constexpr int kMostBytes = 4096;

struct Line {
  const char* key = nullptr;
  int key_size = 0;
  const char* value = nullptr;
  int value_size = 0;
};

// The next line "key=value" from *at. A line with no "=" does not count. The end
// of a line can have a carriage return. False at the end of the text.
inline bool NextLine(const char* text, int size, int* at, Line* line) {
  while (*at < size) {
    const int start = *at;
    int end = start;
    while (end < size && text[end] != '\n') ++end;
    *at = end < size ? end + 1 : end;
    int last = end;
    if (last > start && text[last - 1] == '\r') --last;
    int equal = start;
    while (equal < last && text[equal] != '=') ++equal;
    if (equal == last || equal == start) continue;
    line->key = text + start;
    line->key_size = equal - start;
    line->value = text + equal + 1;
    line->value_size = last - equal - 1;
    return true;
  }
  return false;
}

inline bool KeyIs(const Line& line, const char* key) {
  const int size = static_cast<int>(strlen(key));
  return line.key_size == size && memcmp(line.key, key, static_cast<size_t>(size)) == 0;
}

// The value as a number. False with no digit or with another character.
inline bool IntOf(const Line& line, int* value) {
  int at = 0;
  bool negative = false;
  if (at < line.value_size && line.value[at] == '-') {
    negative = true;
    ++at;
  }
  if (at >= line.value_size || line.value_size - at > 9) return false;
  int result = 0;
  for (; at < line.value_size; ++at) {
    const char c = line.value[at];
    if (c < '0' || c > '9') return false;
    result = result * 10 + (c - '0');
  }
  *value = negative ? -result : result;
  return true;
}

// The serial of the file, or -1 when its first line is not a serial.
inline int SerialOf(const char* text, int size) {
  int at = 0;
  Line line;
  int value = 0;
  if (!NextLine(text, size, &at, &line) || !KeyIs(line, "serial") || !IntOf(line, &value)) {
    return -1;
  }
  return value;
}

// The keys that the app reads. The mod does not look for a switch with these names.
inline bool IsAppKey(const Line& line) {
  return KeyIs(line, "serial") || KeyIs(line, "step") || KeyIs(line, "prompt") ||
         KeyIs(line, "gpu_state");
}

}  // namespace remote_math
