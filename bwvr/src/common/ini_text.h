// AN INI FILE AS TEXT, with no Windows code.
//
// The launcher of the PC reads and writes bwvr.ini with GetPrivateProfile*
// of Windows. The headset app has no such functions, so it edits the text of
// the file with this class. It follows the rules of Windows that the mod
// relies on: the names of the sections and of the keys have no case, the
// spaces around a key and a value do not count, a write replaces the line
// of the key in its section or adds a line at the end of the section, and a
// write to a section that is not there adds it at the end of the file. The
// other lines (comments, other sections, the order) stay as they are.
#pragma once

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

namespace bwvr_ini {

class IniText {
 public:
  IniText() = default;
  explicit IniText(const std::string& text) { Parse(text); }

  void Parse(const std::string& text) {
    lines_.clear();
    size_t start = 0;
    while (start < text.size()) {
      size_t end = text.find('\n', start);
      if (end == std::string::npos) end = text.size();
      std::string line = text.substr(start, end - start);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      lines_.push_back(line);
      start = end + 1;
    }
  }

  // The text of the file, with CRLF at the end of each line, as Windows
  // writes it.
  std::string Text() const {
    std::string out;
    for (const std::string& line : lines_) {
      out += line;
      out += "\r\n";
    }
    return out;
  }

  bool Has(const char* section, const char* key) const {
    return FindKey(section, key) >= 0;
  }

  std::string Get(const char* section, const char* key,
                  const char* fallback) const {
    const int at = FindKey(section, key);
    if (at < 0) return fallback;
    const std::string& line = lines_[at];
    const size_t equal = line.find('=');
    return Trim(line.substr(equal + 1));
  }

  // As GetPrivateProfileInt: the number at the start of the value, and the
  // fallback when the key is not there.
  int GetInt(const char* section, const char* key, int fallback) const {
    const int at = FindKey(section, key);
    if (at < 0) return fallback;
    const std::string value = Get(section, key, "");
    return atoi(value.c_str());
  }

  void Set(const char* section, const char* key, const std::string& value) {
    const std::string line = std::string(key) + "=" + value;
    const int at = FindKey(section, key);
    if (at >= 0) {
      lines_[at] = line;
      return;
    }
    int head = FindSection(section);
    if (head < 0) {
      if (!lines_.empty() && !Trim(lines_.back()).empty()) lines_.push_back("");
      lines_.push_back(std::string("[") + section + "]");
      lines_.push_back(line);
      return;
    }
    // After the last line of the section that is not empty.
    int last = head;
    for (int i = head + 1; i < static_cast<int>(lines_.size()); ++i) {
      if (IsSection(lines_[i])) break;
      if (!Trim(lines_[i]).empty()) last = i;
    }
    lines_.insert(lines_.begin() + last + 1, line);
  }

  void SetInt(const char* section, const char* key, int value) {
    Set(section, key, std::to_string(value));
  }

  // As WritePrivateProfileString with a null value: the line goes.
  void Remove(const char* section, const char* key) {
    for (;;) {
      const int at = FindKey(section, key);
      if (at < 0) return;
      lines_.erase(lines_.begin() + at);
    }
  }

 private:
  static std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
  }

  static bool SameName(const std::string& a, const char* b) {
    const size_t n = strlen(b);
    if (a.size() != n) return false;
    for (size_t i = 0; i < n; ++i) {
      char x = a[i], y = b[i];
      if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
      if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
      if (x != y) return false;
    }
    return true;
  }

  static bool IsSection(const std::string& line) {
    const std::string t = Trim(line);
    return t.size() >= 2 && t.front() == '[' && t.find(']') != std::string::npos;
  }

  int FindSection(const char* section) const {
    for (int i = 0; i < static_cast<int>(lines_.size()); ++i) {
      if (!IsSection(lines_[i])) continue;
      const std::string t = Trim(lines_[i]);
      const std::string name = Trim(t.substr(1, t.find(']') - 1));
      if (SameName(name, section)) return i;
    }
    return -1;
  }

  int FindKey(const char* section, const char* key) const {
    const int head = FindSection(section);
    if (head < 0) return -1;
    for (int i = head + 1; i < static_cast<int>(lines_.size()); ++i) {
      const std::string& line = lines_[i];
      if (IsSection(line)) break;
      const std::string t = Trim(line);
      if (t.empty() || t[0] == ';') continue;
      const size_t equal = line.find('=');
      if (equal == std::string::npos) continue;
      if (SameName(Trim(line.substr(0, equal)), key)) return i;
    }
    return -1;
  }

  std::vector<std::string> lines_;
};

}  // namespace bwvr_ini
