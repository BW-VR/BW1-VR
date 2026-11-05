// The file of the remote control (remote_math.h). main.cpp includes this file after
// Check and Near, just before main.
#pragma once

#include <math.h>
#include <string.h>

#include "../../src/common/settings_table.h"
#include "../../src/mod/detail_scale_math.h"
#include "../../src/mod/remote_math.h"
#include "../../src/mod/vk_draw_math.h"

namespace remote_checks {

inline void DetailScaleChecks() {
  namespace m = detail_scale_math;
  m::Curve c;
  c.units_1x = 800.0f;
  c.units_2x = 200.0f;
  c.units_3x = 100.0f;
  Check(fabsf(m::FactorOf(2000.0f, c, 5.0f) - 1.0f) < 0.001f, "a small world has the game's own distances",
        m::FactorOf(2000.0f, c, 5.0f), 1.0f);
  Check(fabsf(m::FactorOf(800.0f, c, 5.0f) - 1.0f) < 0.001f, "the mark of 1x still has 1x",
        m::FactorOf(800.0f, c, 5.0f), 1.0f);
  Check(fabsf(m::FactorOf(400.0f, c, 5.0f) - 1.5f) < 0.001f,
        "the middle on a scale of ratios: 800 to 200 has 400 in the middle",
        m::FactorOf(400.0f, c, 5.0f), 1.5f);
  Check(fabsf(m::FactorOf(200.0f, c, 5.0f) - 2.0f) < 0.001f, "the mark of 2x has 2x",
        m::FactorOf(200.0f, c, 5.0f), 2.0f);
  Check(fabsf(m::FactorOf(100.0f, c, 5.0f) - 3.0f) < 0.001f, "the mark of 3x has 3x",
        m::FactorOf(100.0f, c, 5.0f), 3.0f);
  Check(fabsf(m::FactorOf(100.0f / 1.5f, c, 5.0f) - 5.0f) < 0.001f, "5x comes soon after the mark of 3x",
        m::FactorOf(100.0f / 1.5f, c, 5.0f), 5.0f);
  Check(fabsf(m::FactorOf(40.0f, c, 5.0f) - 5.0f) < 0.001f, "a large world has 5x",
        m::FactorOf(40.0f, c, 5.0f), 5.0f);
  Check(fabsf(m::FactorOf(40.0f, c, 2.0f) - 2.0f) < 0.001f, "the key detail_distance is the highest factor",
        m::FactorOf(40.0f, c, 2.0f), 2.0f);
  Check(fabsf(m::FactorOf(0.0f, c, 5.0f) - 5.0f) < 0.001f, "with no table the factor of the key counts",
        m::FactorOf(0.0f, c, 5.0f), 5.0f);
  // No jump: a zoom in small steps changes the factor by little, and only upward.
  float before = m::FactorOf(2000.0f, c, 5.0f);
  float most_step = 0.0f;
  bool upward = true;
  for (float units = 2000.0f; units > 40.0f; units *= 0.99f) {
    const float now = m::FactorOf(units, c, 5.0f);
    if (now < before - 0.0001f) upward = false;
    if (now - before > most_step) most_step = now - before;
    before = now;
  }
  Check(upward, "a larger world never has a lower factor", upward ? 1 : 0, 1);
  Check(most_step < 0.1f, "one percent of zoom changes the factor by little", most_step, 0.1f);
  m::Curve wrong;
  wrong.units_1x = 100.0f;
  wrong.units_2x = 200.0f;
  wrong.units_3x = 50.0f;
  Check(!m::Valid(wrong), "values out of order are no curve", 0, 0);
  Check(fabsf(m::FactorOf(150.0f, wrong, 5.0f) - 5.0f) < 0.001f, "with no curve the factor of the key counts",
        m::FactorOf(150.0f, wrong, 5.0f), 5.0f);
}

// The launchers and the mod have one default for each point of the curve.
inline void DetailTableChecks() {
  namespace st = bwvr_settings;
  const detail_scale_math::Curve c;
  Check(detail_scale_math::Valid(c), "the default curve is in order", 1, 1);
  int found = 0;
  for (int i = 0; i < st::kSettingCount; ++i) {
    const st::Setting& s = st::kSettings[i];
    if (s.key == nullptr) continue;
    float want = -1.0f;
    if (strcmp(s.key, "detail_scale_1x") == 0) want = c.units_1x;
    if (strcmp(s.key, "detail_scale_2x") == 0) want = c.units_2x;
    if (strcmp(s.key, "detail_scale_3x") == 0) want = c.units_3x;
    if (strcmp(s.key, "detail_by_scale") == 0) ++found;
    if (want < 0.0f) continue;
    ++found;
    Check(st::kSlider == s.kind, "a point of the curve is a slider", s.kind, st::kSlider);
    Check(static_cast<float>(s.fallback) == want, "the table and the mod have one default", s.fallback,
          want);
  }
  Check(found == 4, "the table has the switch and the three points of the curve", found, 4);
}

inline void TinyChecks() {
  namespace d = vk_draw_math;
  Check(d::IsTiny(0.5f, 0.5f, 1.0f), "a draw below the limit in both directions is tiny", 1, 1);
  Check(!d::IsTiny(30.0f, 0.5f, 1.0f), "a long thin draw is not tiny", 0, 0);
  Check(!d::IsTiny(0.5f, 30.0f, 1.0f), "a tall thin draw is not tiny", 0, 0);
  Check(!d::IsTiny(1.0f, 1.0f, 1.0f), "the limit itself is not tiny", 0, 0);
  Check(!d::IsTiny(0.0f, 0.0f, 0.0f), "with no limit no draw is tiny", 0, 0);
}

inline void RemoteChecks() {
  namespace m = remote_math;
  DetailScaleChecks();
  DetailTableChecks();
  TinyChecks();
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
