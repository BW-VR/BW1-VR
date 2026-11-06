// THE CORES OF THE HEADSET (thread_cores, thread_bind, docs/android.md, "The
// cores of the headset"). The rules of the log of the cores and of the
// binding of the threads to the cores: the lists of cores of Linux, the fast
// cores, the cores of each thread. No Android code.
//
//   thread_cores=0       no line (the default)
//   thread_cores=1       each second one line with the core and the CPU time
//                        of each thread of the game and of some threads of
//                        the app
//   thread_bind=0        no binding: Android chooses the cores (the default)
//   thread_bind=1        the main thread of the game, the session thread and
//                        the play thread of the app on the fast cores, each
//                        other thread of the game and of the app on the other
//                        cores
//   thread_bind=2        the same three threads on the fast cores, the other
//                        threads on each core that the app can use
//   thread_bind_cores    empty (the default): the fast cores are the cores
//                        with a higher top speed than the slowest core. A
//                        list of cores (for example 4-5 or 3,5) gives the
//                        fast cores
#pragma once

#include <stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

namespace core_rules {

// A list of cores as Linux writes it ("0-3,5", the format of
// /sys/devices/system/cpu/online and of Cpus_allowed_list): the cores in
// order, each one time. Each part that is not a number or a range of numbers
// makes the list empty. Cores at 1024 or more are left out.
inline std::vector<int> ParseCpuList(const std::string& text) {
  std::vector<int> out;
  size_t at = 0;
  const size_t n = text.size();
  while (at < n) {
    while (at < n && (text[at] == ' ' || text[at] == '\t' || text[at] == '\r' ||
                      text[at] == '\n' || text[at] == ',')) {
      ++at;
    }
    if (at >= n) break;
    size_t end = at;
    while (end < n && text[end] != ',' && text[end] != ' ' && text[end] != '\t' &&
           text[end] != '\r' && text[end] != '\n') {
      ++end;
    }
    const std::string part = text.substr(at, end - at);
    at = end;
    const size_t dash = part.find('-');
    const std::string first = dash == std::string::npos ? part : part.substr(0, dash);
    const std::string last = dash == std::string::npos ? part : part.substr(dash + 1);
    if (first.empty() || last.empty()) return {};
    for (char c : first + last) {
      if (c < '0' || c > '9') return {};
    }
    if (first.size() > 4 || last.size() > 4) return {};
    const int a = atoi(first.c_str());
    const int b = atoi(last.c_str());
    if (b < a) return {};
    for (int c = a; c <= b && c < 1024; ++c) out.push_back(c);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

// The cores as a list of Linux ("0-3,5"), or "none".
inline std::string CpuListText(const std::vector<int>& cores) {
  if (cores.empty()) return "none";
  std::vector<int> c = cores;
  std::sort(c.begin(), c.end());
  c.erase(std::unique(c.begin(), c.end()), c.end());
  std::string out;
  size_t i = 0;
  while (i < c.size()) {
    size_t j = i;
    while (j + 1 < c.size() && c[j + 1] == c[j] + 1) ++j;
    if (!out.empty()) out += ",";
    out += std::to_string(c[i]);
    if (j > i) out += "-" + std::to_string(c[j]);
    i = j + 1;
  }
  return out;
}

inline bool Has(const std::vector<int>& cores, int core) {
  return std::find(cores.begin(), cores.end(), core) != cores.end();
}

// The cores of `a` that are in `b`.
inline std::vector<int> Both(const std::vector<int>& a, const std::vector<int>& b) {
  std::vector<int> out;
  for (int c : a) {
    if (Has(b, c)) out.push_back(c);
  }
  return out;
}

// The cores of `a` that are not in `b`.
inline std::vector<int> Without(const std::vector<int>& a, const std::vector<int>& b) {
  std::vector<int> out;
  for (int c : a) {
    if (!Has(b, c)) out.push_back(c);
  }
  return out;
}

// One core of the device: its top speed (cpufreq/cpuinfo_max_freq in kHz,
// 0 when the file is not there) and its capacity (cpu_capacity, 0 when the
// file is not there).
struct Core {
  int index = 0;
  long max_khz = 0;
  long capacity = 0;
};

// The speed of a core for the choice of the fast cores: the top speed, or
// the capacity when no core has a top speed.
inline long SpeedOf(const Core& c, bool by_khz) { return by_khz ? c.max_khz : c.capacity; }

// THE FAST CORES: of the cores that the app can use (`allowed`), each core
// with a higher speed than the slowest of them. Empty when all of them have
// the same speed, or when no core has a speed. The top speed counts first,
// the capacity only when no allowed core has a top speed.
inline std::vector<int> FastCores(const std::vector<Core>& cores, const std::vector<int>& allowed) {
  bool any_khz = false;
  for (const Core& c : cores) {
    if (Has(allowed, c.index) && c.max_khz > 0) any_khz = true;
  }
  long lowest = 0;
  bool first = true;
  for (const Core& c : cores) {
    if (!Has(allowed, c.index)) continue;
    const long s = SpeedOf(c, any_khz);
    if (s <= 0) continue;
    if (first || s < lowest) lowest = s;
    first = false;
  }
  std::vector<int> out;
  if (first) return out;
  for (const Core& c : cores) {
    if (Has(allowed, c.index) && SpeedOf(c, any_khz) > lowest) out.push_back(c.index);
  }
  std::sort(out.begin(), out.end());
  return out;
}

// THE TOP CORES: of the cores that the app can use, each core with the highest capacity. The
// capacity counts first, because a core with a lower clock can do more work. The top speed counts
// only when no allowed core has a capacity. Empty when no core has a speed.
inline std::vector<int> TopCores(const std::vector<Core>& cores, const std::vector<int>& allowed) {
  bool any_capacity = false;
  for (const Core& c : cores) {
    if (Has(allowed, c.index) && c.capacity > 0) any_capacity = true;
  }
  long highest = 0;
  for (const Core& c : cores) {
    if (Has(allowed, c.index) && SpeedOf(c, !any_capacity) > highest) highest = SpeedOf(c, !any_capacity);
  }
  std::vector<int> out;
  if (highest <= 0) return out;
  for (const Core& c : cores) {
    if (Has(allowed, c.index) && SpeedOf(c, !any_capacity) == highest) out.push_back(c.index);
  }
  std::sort(out.begin(), out.end());
  return out;
}

// The value of thread_bind: 0, 1 or 2. Each other value is 0.
inline int ParseBind(int value) { return value == 1 || value == 2 ? value : 0; }

// The cores for the binding of one game.
struct Plan {
  int bind = 0;               // thread_bind
  std::vector<int> allowed;   // the cores that the app can use
  std::vector<int> fast;      // the fast cores (a part of allowed)
  std::vector<int> other;     // the other cores for the other threads, or empty: no change
  std::string why;            // with no binding: why
};

// The plan: the fast cores from thread_bind_cores (`list`, a list of cores)
// or from the speeds, then the other cores. No binding when there are no fast
// cores or when the fast cores are each core that the app can use. With
// thread_bind=1 the other threads go to the cores that are not fast.
inline Plan MakePlan(int bind, const std::string& list, const std::vector<Core>& cores,
                     const std::vector<int>& allowed) {
  Plan p;
  p.bind = ParseBind(bind);
  p.allowed = allowed;
  if (p.bind == 0) {
    p.why = "thread_bind=0";
    return p;
  }
  bool blank = true;
  for (char c : list) {
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') blank = false;
  }
  if (!blank) {
    const std::vector<int> want = ParseCpuList(list);
    p.fast = Both(want, allowed);
    if (want.empty()) {
      p.why = "thread_bind_cores=" + list + " is not a list of cores";
    } else if (p.fast.empty()) {
      p.why = "no core of thread_bind_cores=" + list + " is a core that the app can use (" +
              CpuListText(allowed) + ")";
    }
  } else {
    p.fast = FastCores(cores, allowed);
    if (p.fast.empty()) p.why = "the cores that the app can use have one speed, or no speed is known";
  }
  if (!p.fast.empty() && Without(allowed, p.fast).empty()) {
    p.why = "the fast cores are each core that the app can use";
    p.fast.clear();
  }
  if (p.fast.empty()) {
    p.bind = 0;
    return p;
  }
  if (p.bind == 1) p.other = Without(allowed, p.fast);
  return p;
}

// What a thread gets: no change, the fast cores, or the other cores.
enum class Place { kFree, kFast, kOther };

// The place of one thread in a plan. key: the main thread of the game, the
// session thread or the play thread of the app.
inline Place PlaceOf(const Plan& p, bool key) {
  if (p.bind == 0) return Place::kFree;
  if (key) return Place::kFast;
  return p.other.empty() ? Place::kFree : Place::kOther;
}

// The cores of a place, or empty for kFree.
inline const std::vector<int>& CoresOf(const Plan& p, Place place) {
  static const std::vector<int> kNone;
  if (place == Place::kFast) return p.fast;
  if (place == Place::kOther) return p.other;
  return kNone;
}

// THE LINE OF THE CORES: the share of each core in the CPU time of the game
// in one second, by the core that each thread ran on last. A thread that
// moved in the second counts on its last core, so the share is an estimate.
struct CoreShare {
  int core = -1;
  double percent = 0.0;  // percent of one core
};

inline std::vector<CoreShare> Shares(const std::vector<int>& core_of_thread,
                                     const std::vector<double>& percent_of_thread) {
  std::vector<CoreShare> out;
  const size_t n = core_of_thread.size() < percent_of_thread.size() ? core_of_thread.size()
                                                                     : percent_of_thread.size();
  for (size_t i = 0; i < n; ++i) {
    if (percent_of_thread[i] <= 0.0) continue;
    bool found = false;
    for (CoreShare& s : out) {
      if (s.core == core_of_thread[i]) {
        s.percent += percent_of_thread[i];
        found = true;
      }
    }
    if (!found) out.push_back({core_of_thread[i], percent_of_thread[i]});
  }
  std::sort(out.begin(), out.end(),
            [](const CoreShare& a, const CoreShare& b) { return a.core < b.core; });
  return out;
}

}  // namespace core_rules
