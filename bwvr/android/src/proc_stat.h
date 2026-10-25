// THE CPU TIME OF THE THREADS AND OF THE CORES (time_log, docs/android.md,
// "The time log of the app"): the lines of /proc/<pid>/task/<tid>/stat and of
// /proc/stat, and the load of a thread in a time. No Android code.
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

namespace proc_stat {

// One thread (or one process) from its line of stat (proc(5)):
// "tid (name) state ppid ...". utime is field 14, stime field 15 and
// processor field 39, in ticks of 1 / sysconf(_SC_CLK_TCK) s.
struct Task {
  int id = 0;
  std::string name;
  char state = '?';
  uint64_t user = 0;    // utime
  uint64_t system = 0;  // stime
  int processor = -1;   // the core that the thread ran on last
};

// The name can have spaces and brackets in it (Wine gives a thread the name
// of its description), so the name ends at the last ')' of the line. False
// for a line with no name or with less than 15 fields.
inline bool ParseTask(const std::string& line, Task* out) {
  const size_t open = line.find('(');
  const size_t close = line.rfind(')');
  if (open == std::string::npos || close == std::string::npos || close < open) return false;
  Task t;
  t.id = atoi(line.c_str());
  if (t.id <= 0) return false;
  t.name = line.substr(open + 1, close - open - 1);
  // The fields after the name: field 3 (the state) is the first.
  std::vector<std::string> f;
  size_t i = close + 1;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\n')) ++i;
    const size_t start = i;
    while (i < line.size() && line[i] != ' ' && line[i] != '\n') ++i;
    if (i > start) f.push_back(line.substr(start, i - start));
  }
  // f[0] is field 3, so field N is f[N - 3].
  if (f.size() < 13) return false;
  t.state = f[0].empty() ? '?' : f[0][0];
  t.user = strtoull(f[11].c_str(), nullptr, 10);
  t.system = strtoull(f[12].c_str(), nullptr, 10);
  if (f.size() >= 37) t.processor = atoi(f[36].c_str());
  *out = t;
  return true;
}

// One line "cpu..." of /proc/stat: index -1 for the line of all cores
// ("cpu"), else the number of the core. busy is user, nice, system, irq,
// softirq and steal, total is busy with idle and iowait. The times of a
// guest are in user already.
struct Cpu {
  int index = -1;
  uint64_t busy = 0;
  uint64_t total = 0;
};

inline bool ParseCpu(const std::string& line, Cpu* out) {
  if (line.compare(0, 3, "cpu") != 0) return false;
  Cpu c;
  size_t i = 3;
  if (i < line.size() && line[i] >= '0' && line[i] <= '9') {
    c.index = atoi(line.c_str() + i);
  } else if (i < line.size() && line[i] != ' ') {
    return false;
  }
  while (i < line.size() && line[i] != ' ') ++i;
  uint64_t v[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  int n = 0;
  const char* p = line.c_str() + i;
  while (n < 8) {
    char* end = nullptr;
    const unsigned long long x = strtoull(p, &end, 10);
    if (end == p) break;
    v[n++] = x;
    p = end;
  }
  if (n < 4) return false;
  // user nice system idle iowait irq softirq steal
  c.busy = v[0] + v[1] + v[2] + v[5] + v[6] + v[7];
  c.total = c.busy + v[3] + v[4];
  *out = c;
  return true;
}

// The percent of one core of `ticks` in `seconds`, with `per_second` ticks a
// second.
inline double Percent(uint64_t ticks, double seconds, long per_second) {
  if (seconds <= 0.0 || per_second <= 0) return 0.0;
  return 100.0 * static_cast<double>(ticks) / (seconds * static_cast<double>(per_second));
}

// The load of one thread in a time: the ticks since its last sample. A
// thread with no last sample (a new thread) counts each of its ticks.
struct Load {
  int process = 0;  // the pid of its process
  bool game = false;  // a thread of the game, else of the app
  Task now;
  uint64_t user = 0;    // the ticks in the time
  uint64_t system = 0;
};

inline Load LoadOf(int process, bool game, const Task& before, bool had_before, const Task& now) {
  Load l;
  l.process = process;
  l.game = game;
  l.now = now;
  if (had_before && now.user >= before.user && now.system >= before.system) {
    l.user = now.user - before.user;
    l.system = now.system - before.system;
  } else {
    l.user = now.user;
    l.system = now.system;
  }
  return l;
}

// The threads with the most ticks first, at most `count`. Two threads with
// the same ticks go in the order of their ids.
inline std::vector<Load> Top(std::vector<Load> loads, size_t count) {
  std::stable_sort(loads.begin(), loads.end(), [](const Load& a, const Load& b) {
    const uint64_t ta = a.user + a.system, tb = b.user + b.system;
    if (ta != tb) return ta > tb;
    return a.now.id < b.now.id;
  });
  if (loads.size() > count) loads.resize(count);
  return loads;
}

// The main thread of a process has the id of the process.
inline bool IsMainThread(const Load& l) { return l.now.id == l.process; }

}  // namespace proc_stat
