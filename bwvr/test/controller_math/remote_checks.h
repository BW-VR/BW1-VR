// The file of the remote control (remote_math.h). main.cpp includes this file after
// Check and Near, just before main.
#pragma once

#include <string.h>

#include "../../src/mod/remote_math.h"

namespace remote_checks {

inline void RemoteChecks() {
  namespace m = remote_math;
  {
    const char* text = "serial=12\r\nstep=detail 3x\ndetail_distance=2\n\nno equal\n=x\nprompt=\n";
    const int size = static_cast<int>(strlen(text));
    Check(m::SerialOf(text, size) == 12, "the first line has the serial", m::SerialOf(text, size),
          12);
    int at = 0;
    m::Line line;
    Check(m::NextLine(text, size, &at, &line) && m::KeyIs(line, "serial"),
          "the first line is the serial", line.key_size, 6);
    int value = 0;
    Check(m::IntOf(line, &value) && value == 12, "a carriage return is not part of the value",
          value, 12);
    Check(m::IsAppKey(line), "the serial is a key of the app", 1, 1);
    Check(m::NextLine(text, size, &at, &line) && m::KeyIs(line, "step"), "the second line is the step",
          line.key_size, 4);
    Check(line.value_size == 9 && memcmp(line.value, "detail 3x", 9) == 0,
          "a value can have a space", line.value_size, 9);
    Check(!m::IntOf(line, &value), "a word is not a number", 0, 0);
    Check(m::IsAppKey(line), "the step is a key of the app", 1, 1);
    Check(m::NextLine(text, size, &at, &line) && m::KeyIs(line, "detail_distance"),
          "the third line is a switch", line.key_size, 15);
    Check(m::IntOf(line, &value) && value == 2, "the value of a switch", value, 2);
    Check(!m::IsAppKey(line), "a switch is not a key of the app", 0, 0);
    Check(m::NextLine(text, size, &at, &line) && m::KeyIs(line, "prompt"),
          "a line with no equal sign and a line with no key do not count", line.key_size, 6);
    Check(line.value_size == 0, "an empty text hides the text", line.value_size, 0);
    Check(!m::NextLine(text, size, &at, &line), "the text ends", 0, 0);
  }
  {
    Check(m::SerialOf("", 0) == -1, "an empty file has no serial", m::SerialOf("", 0), -1);
    const char* other = "step=a\nserial=3\n";
    Check(m::SerialOf(other, static_cast<int>(strlen(other))) == -1,
          "the serial must be the first line", m::SerialOf(other, static_cast<int>(strlen(other))),
          -1);
    const char* cut = "serial=4";
    Check(m::SerialOf(cut, 8) == 4, "the last line needs no end of line", m::SerialOf(cut, 8), 4);
    m::Line line;
    line.value = "-3";
    line.value_size = 2;
    int value = 0;
    Check(m::IntOf(line, &value) && value == -3, "a number below 0", value, -3);
    line.value = "-";
    line.value_size = 1;
    Check(!m::IntOf(line, &value), "a sign alone is not a number", 0, 0);
    line.value = "1234567890";
    line.value_size = 10;
    Check(!m::IntOf(line, &value), "a number of 10 digits is too long", 0, 0);
  }
}

}  // namespace remote_checks
