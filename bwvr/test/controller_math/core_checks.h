// The cores of the headset, a test: the lists of cores of Linux, the fast
// cores, the plan of thread_bind and the place of each thread
// (android/src/core_rules.h). main.cpp includes this file after Check and
// Near, just before main.
#pragma once

#include <string>
#include <vector>

#include "../../android/src/core_rules.h"

namespace core_checks {

inline bool Same(const std::vector<int>& a, const std::vector<int>& b) { return a == b; }

inline void ListChecks() {
  namespace cr = core_rules;
  printf("\nThe cores of the headset: the lists of cores\n");
  Check(Same(cr::ParseCpuList("0-3,5\n"), {0, 1, 2, 3, 5}), "0-3,5 is the cores 0, 1, 2, 3 and 5",
        static_cast<double>(cr::ParseCpuList("0-3,5\n").size()), 5);
  Check(Same(cr::ParseCpuList("4"), {4}) && Same(cr::ParseCpuList(" 3 , 5 "), {3, 5}) &&
            Same(cr::ParseCpuList("5,4-5,4"), {4, 5}),
        "one core, spaces, and a core two times (one time in the list)", 0, 0);
  Check(cr::ParseCpuList("").empty() && cr::ParseCpuList("a").empty() &&
            cr::ParseCpuList("3-1").empty() && cr::ParseCpuList("1-").empty() &&
            cr::ParseCpuList("0-3,x").empty() && cr::ParseCpuList("-1").empty(),
        "an empty text and each wrong part give no core", 0, 0);
  Check(cr::CpuListText({0, 1, 2, 3, 5}) == "0-3,5" && cr::CpuListText({5, 4}) == "4-5" &&
            cr::CpuListText({2}) == "2" && cr::CpuListText({}) == "none" &&
            cr::CpuListText({0, 2, 4}) == "0,2,4",
        "the text of a list: ranges, in order, \"none\" for no core", 0, 0);
  Check(Same(cr::Without({0, 1, 2, 3}, {1, 3}), {0, 2}) && Same(cr::Both({0, 1, 2}, {2, 3}), {2}),
        "the cores without and with the cores of another list", 0, 0);
}

inline void FastChecks() {
  namespace cr = core_rules;
  printf("\nThe cores of the headset: the fast cores\n");
  // Six cores: four slow ones and two fast ones, as a chip with two kinds.
  const std::vector<cr::Core> six = {{0, 1804800, 0}, {1, 1804800, 0}, {2, 1804800, 0},
                                     {3, 1804800, 0}, {4, 2361600, 0}, {5, 2361600, 0}};
  Check(Same(cr::FastCores(six, {0, 1, 2, 3, 4, 5}), {4, 5}),
        "two kinds: the cores with the higher top speed are fast", 0, 0);
  Check(Same(cr::FastCores(six, {3, 4, 5}), {4, 5}) && cr::FastCores(six, {0, 1}).empty() &&
            cr::FastCores(six, {4, 5}).empty(),
        "only the cores that the app can use count: no fast core when they have one speed", 0, 0);
  const std::vector<cr::Core> three = {{0, 1000, 0}, {1, 2000, 0}, {2, 3000, 0}};
  Check(Same(cr::FastCores(three, {0, 1, 2}), {1, 2}),
        "three kinds: each core above the slowest is fast", 0, 0);
  const std::vector<cr::Core> capacity = {{0, 0, 400}, {1, 0, 400}, {2, 0, 1024}};
  Check(Same(cr::FastCores(capacity, {0, 1, 2}), {2}),
        "no top speed: the capacity gives the fast cores", 0, 0);
  const std::vector<cr::Core> none = {{0, 0, 0}, {1, 0, 0}};
  Check(cr::FastCores(none, {0, 1}).empty(), "no speed is known: no fast core", 0, 0);
  // The cores of the Steam Frame: core 7 has a lower clock than the cores 2 to 4 and the highest capacity.
  const std::vector<cr::Core> frame = {{0, 2265600, 216},  {1, 2265600, 216},  {2, 3148800, 855},
                                       {3, 3148800, 855},  {4, 3148800, 855},  {5, 2956800, 803},
                                       {6, 2956800, 803},  {7, 3052800, 1024}};
  Check(Same(cr::TopCores(frame, {0, 1, 2, 3, 4, 5, 6, 7}), {7}),
        "the top core is the core with the highest capacity, not the highest clock", 0, 0);
  Check(Same(cr::TopCores(frame, {0, 1, 2, 3}), {2, 3}),
        "only the cores that the app can use count, and two cores can be the top", 0, 0);
  Check(Same(cr::TopCores(six, {0, 1, 2, 3, 4, 5}), {4, 5}),
        "no capacity: the top speed gives the top cores", 0, 0);
  Check(cr::TopCores(none, {0, 1}).empty(), "no speed is known: no top core", 0, 0);
}

inline void PlanChecks() {
  namespace cr = core_rules;
  printf("\nThe cores of the headset: the plan of thread_bind\n");
  const std::vector<cr::Core> six = {{0, 1804800, 0}, {1, 1804800, 0}, {2, 1804800, 0},
                                     {3, 1804800, 0}, {4, 2361600, 0}, {5, 2361600, 0}};
  const std::vector<int> all = {0, 1, 2, 3, 4, 5};
  cr::Plan p = cr::MakePlan(0, "", six, all);
  Check(p.bind == 0 && cr::PlaceOf(p, true) == cr::Place::kFree &&
            cr::PlaceOf(p, false) == cr::Place::kFree,
        "thread_bind=0 (the default): no thread gets a place", 0, 0);
  Check(cr::ParseBind(1) == 1 && cr::ParseBind(2) == 2 && cr::ParseBind(3) == 0 &&
            cr::ParseBind(-1) == 0,
        "thread_bind takes 0, 1 and 2. Each other value is 0", 0, 0);
  p = cr::MakePlan(1, "", six, all);
  Check(p.bind == 1 && Same(p.fast, {4, 5}) && Same(p.other, {0, 1, 2, 3}) &&
            cr::PlaceOf(p, true) == cr::Place::kFast &&
            cr::PlaceOf(p, false) == cr::Place::kOther &&
            Same(cr::CoresOf(p, cr::Place::kFast), {4, 5}) &&
            Same(cr::CoresOf(p, cr::Place::kOther), {0, 1, 2, 3}),
        "thread_bind=1: the key threads on the fast cores, each other thread on the other "
        "cores",
        0, 0);
  p = cr::MakePlan(2, "", six, all);
  Check(p.bind == 2 && Same(p.fast, {4, 5}) && p.other.empty() &&
            cr::PlaceOf(p, true) == cr::Place::kFast &&
            cr::PlaceOf(p, false) == cr::Place::kFree,
        "thread_bind=2: the key threads on the fast cores, the other threads stay as they are",
        0, 0);
  p = cr::MakePlan(1, "2-3", six, all);
  Check(p.bind == 1 && Same(p.fast, {2, 3}) && Same(p.other, {0, 1, 4, 5}),
        "thread_bind_cores=2-3 gives the fast cores", 0, 0);
  p = cr::MakePlan(1, " 7,9 ", six, all);
  Check(p.bind == 0 && p.fast.empty() && !p.why.empty(),
        "thread_bind_cores with no core that the app can use: no binding, and why", 0, 0);
  p = cr::MakePlan(1, "x", six, all);
  Check(p.bind == 0 && p.why == "thread_bind_cores=x is not a list of cores",
        "thread_bind_cores that is not a list: no binding, and why", 0, 0);
  p = cr::MakePlan(1, "0-5", six, all);
  Check(p.bind == 0 && p.why == "the fast cores are each core that the app can use",
        "fast cores that are each core of the app: no binding", 0, 0);
  const std::vector<cr::Core> same = {{0, 2000, 0}, {1, 2000, 0}, {2, 2000, 0}, {3, 2000, 0}};
  p = cr::MakePlan(1, "", same, {0, 1, 2, 3});
  Check(p.bind == 0 && !p.why.empty() && cr::PlaceOf(p, true) == cr::Place::kFree,
        "cores with one speed (the emulator): no binding with no thread_bind_cores", 0, 0);
  p = cr::MakePlan(1, "2-3", same, {0, 1, 2, 3});
  Check(p.bind == 1 && Same(p.fast, {2, 3}) && Same(p.other, {0, 1}),
        "cores with one speed and thread_bind_cores=2-3: the binding of the list", 0, 0);
  p = cr::MakePlan(1, "", six, {3, 4, 5});
  Check(p.bind == 1 && Same(p.fast, {4, 5}) && Same(p.other, {3}),
        "only the cores that the app can use: fast 4-5, other 3", 0, 0);
}

inline void ShareChecks() {
  namespace cr = core_rules;
  printf("\nThe cores of the headset: the share of each core\n");
  const std::vector<cr::CoreShare> s =
      cr::Shares({3, 5, 3, 4, 4}, {88.0, 4.0, 3.0, 0.0, 20.0});
  Check(s.size() == 3 && s[0].core == 3 && s[0].percent == 91.0 && s[1].core == 4 &&
            s[1].percent == 20.0 && s[2].core == 5 && s[2].percent == 4.0,
        "the CPU time of the threads by their last core, in the order of the cores, with no "
        "thread of no CPU time",
        static_cast<double>(s.size()), 3);
  Check(cr::Shares({}, {}).empty() && cr::Shares({1}, {}).empty(),
        "no thread: no share", 0, 0);
}

inline void CoreChecks() {
  ListChecks();
  FastChecks();
  PlanChecks();
  ShareChecks();
}

}  // namespace core_checks
