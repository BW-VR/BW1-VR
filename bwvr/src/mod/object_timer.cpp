// THE TIMER OF THE OBJECTS ON THE LAND (object_timer.h, docs/backend.md, "The
// timer of the objects on the land"), a test aid.
//
// fn_005E5CD0 draws the objects on the land. Its own loops are small. Its
// time is in the methods of the objects that it calls through their tables
// of methods (vtables). So the timers go on these calls:
//
// - The call of fn_005E5CD0 in GLandscape::Draw (timer 0). Its exclusive time
//   is the code of fn_005E5CD0 outside each other timer.
// - The calls of fn_005E5CD0 through a vtable: the draw of an object
//   (vtable+0x610), with one timer for each class (each vtable), and the
//   tests of the 3D part of an object (the object at +0x40: vtable+0x74,
//   +0xB0, +0x1A8), with one timer for each function.
// - The entries +0x34, +0x38 and +0x100 of the vtables of the 3D parts of the
//   objects in the list (the place, the detail and the draw of the part, from
//   fn_0063B5D0), with one timer for each function. The timer writes the
//   address of a stub in the entry of the vtable: data, not code.
// - The calls of object_timer_sites.
//
// HOW A TIMER WORKS. The mod writes a call of a stub in the place of the call
// of the game (5 or 6 bytes, a nop after a call of 5 bytes). The stub keeps
// each register and the flags. ObjectTimerEnter does what the bytes of the
// game did (object_timer_math.h, Target), reads the clock, puts the address
// of ExitThunk in the place of the return address, and the stub jumps to the
// function. At the return ExitThunk reads the clock and jumps back to the
// code of the game. So a stub does not need the arguments or the calling
// convention of the function. The exit code uses integers only: the x87 unit
// can hold the value that the function returns.
//
// NO STOP OF THE OTHER THREADS. MinHook stops each other thread while it
// writes a hook. The mod writes the timers itself, on the frame thread, at the
// end of a frame (FinishFrame): the frame thread is then not in the code of a
// timed call, and the draw of the objects runs only on the frame thread. A
// call on another thread goes through the stub with no clock read.
//
// THE START. The timers come in the play: when the list of the objects of
// fn_005E5CD0 (the count at 0x00D20198) has objects at the end of 300 frames
// in a row. So they never come in the load. With test_keys=1, Ctrl Shift F7
// starts and stops them. The stop writes the bytes of the game back.
//
//   object_timer=0      no timer and no line (the default)
//   object_timer=1      the timers, each 10 s the lines "object timer"
//   object_timer_sites  more calls to time, for example
//                       object_timer_sites=0x0074AB1F,0x0074AB57. Each must
//                       be a form of object_timer_math.h, and no jump of the
//                       game may go into its bytes

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "object_timer.h"
#include "object_timer_math.h"
#include "thread_id.h"
#include "time_log_math.h"
#include "walk_parts_math.h"

void ModLog(const char* format, ...);

namespace {

namespace ot = object_timer_math;
namespace wp = walk_parts_math;

const uint32_t kOuterSite = 0x005E4EDD;  // call fn_005E5CD0, in GLandscape::Draw
const uint32_t kDrawObjects = 0x005E5CD0;
const uint32_t kListCount = 0x00D20198;  // the count of the list of objects
const uint32_t kList = 0x00D1D28C;       // the list, 2999 entries at most
const int kListMost = 2999;
const int kPlayFrames = 300;

// The code and the tables of runblack.exe.
bool InText(uint32_t a) { return a >= 0x00401000 && a < 0x008A0000; }
bool InTables(uint32_t a) { return a >= 0x008A0000 && a < 0x00C00000; }

enum KeyKind { kKeySite = 0, kKeyClass, kKeyFunction };

struct Fixed {
  uint32_t at;
  int key;
  const char* name;
};
const Fixed kFixed[] = {
    {0x005E5E40, kKeyFunction, "vtable+0x74 of the 3D part (+0x40), in the scan of the cells"},
    {0x005E5E99, kKeyFunction, "vtable+0x74 of the 3D part (+0x40), in the scan of 0x00D199E8"},
    {0x005E6086, kKeyClass, "the draw (vtable+0x610) with a new test of what shows"},
    {0x005E60A6, kKeyFunction, "vtable+0xB0 of the 3D part (+0x40), after a draw with no show"},
    {0x005E60B5, kKeyFunction, "vtable+0x1A8 of the 3D part (+0x40), after a draw with no show"},
    {0x005E610F, kKeyClass, "the draw (vtable+0x610) of an object that showed in the last test"},
    {0x005E6165, kKeyClass, "the draw (vtable+0x610) of the list at +0x205CD4"},
    {0x005E62E6, kKeyFunction, "vtable+0x100 near the end of fn_005E5CD0"},
};

// The entries of the vtables of the 3D parts that get a timer.
const uint32_t kPartSlots[] = {0x34, 0x38, 0x100};

// The draw methods (vtable+0x610), with the names of bw1-decomp.
struct Name {
  uint32_t fn;
  const char* name;
};
const Name kDrawNames[] = {
    {0x0051C310, "Animal::Draw"},        {0x004648B0, "Object::Draw"},
    {0x0041F680, "Dove::Draw"},          {0x00422770, "ChessPion::Draw"},
    {0x00518150, "MobileObject::Draw"},  {0x005FFDC0, "LandscapeVortex::Draw"},
    {0x00518090, "MultiMapFixed::Draw"}, {0x00515F70, "Abode::Draw"},
    {0x0051BB70, "Pot::Draw"},           {0x00519650, "SpellIcon::Draw"},
    {0x00518690, "Feature::Draw"},       {0x0074AB00, "Tree::Draw"},
    {0x0051BF80, "PileFood::Draw"},      {0x0051BC40, "PileWood::Draw"},
    {0x0051ABA0, "Totem::Draw"},         {0x00518100, "SingleMapFixed::Draw"},
    {0x00774E10, "Whale::Draw"},         {0x0076EC00, "Fragment::Draw"},
    {0x00746A30, "TownDesireFlags::Draw"}, {0x00738960, "TotemStatue::Draw"},
    {0x00734A70, "GStreetLantern::Draw"}, {0x0072CEA0, "PhysicalShield::Draw"},
    {0x0072C300, "MagicShield::Draw"},   {0x00722940, "SpellDispenser::Draw"},
    {0x00720430, "Spell::Draw"},         {0x0071F3D0, "SpecialVillager::Draw"},
    {0x00709C60, "ScriptHighlight::Draw"}, {0x006EA5C0, "Scaffold::Draw"},
    {0x006E6380, "Reward::Draw"},        {0x00682F20, "MagicFireBall::Draw"},
    {0x006709F0, "PrayerIcon::Draw"},    {0x00643FD0, "PFootball::Draw"},
    {0x0063E980, "PBall::Draw"},         {0x00609540, "GBaseOnly::Draw"},
    {0x005FCCC0, "MagicTeleport::Draw"}, {0x00561A20, "PuzzleGrain::Draw"},
    {0x00531610, "Football::Draw"},      {0x0052C830, "FishFarm::Draw"},
    {0x0052AA90, "FireFly::Draw"},       {0x00528570, "Field::Draw"},
    {0x0051CBF0, "Workshop::Draw"},      {0x0051C560, "SpellWolf::Draw"},
    {0x0051B940, "Villager::Draw"},      {0x0051AEC0, "Living::Draw"},
    {0x00519640, "TownCentreSpellIcon::Draw"}, {0x005193D0, "WorshipSite::Draw"},
    {0x00519360, "WorshipTotem::Draw"},  {0x00519350, "StoragePit::Draw"},
    {0x00518E90, "OneOffSpellSeed::Draw"}, {0x00518710, "SpellSeed::Draw"},
    {0x005186A0, "Creed::Draw"},         {0x00517F60, "MobileStatic::Draw"},
    {0x00517F10, "Rock::Draw"},          {0x00517910, "Creature::Draw"},
    {0x00516870, "Ball::Draw"},          {0x00516450, "TownCentre::Draw"},
    {0x00516320, "Windmill::Draw"},      {0x00511990, "FelledTree::Draw"},
    {0x005107F0, "DeadTree::Draw"},      {0x00464B90, "CitadelHeart::Draw"},
    {0x00439A90, "Bonfire::Draw"},       {0x00438F60, "BigForest::Draw"},
    {0x004256C0, "ArenaSpellIcon::Draw"},
};

const char* DrawName(uint32_t fn) {
  for (const Name& n : kDrawNames) {
    if (n.fn == fn) return n.name;
  }
  return "a draw";
}

// A place with a timer: a call in the code, or an entry of a vtable.
struct Site {
  uint32_t at = 0;
  ot::Call call;
  int key = kKeySite;
  int timer = -1;  // kKeySite: the timer of the site
  uint8_t saved[8] = {};
  bool on = false;
};

struct Timer {
  uint32_t shown = 0;
  char name[112] = {};
};

const int kMaxSites = 512;
const int kStubBytes = 10;
const int kProbeTimer = wp::kMaxTimers - 1;

int g_mode = 0;
bool g_test_keys = false;
char g_sites_text[512] = {};
bool g_on = false;        // the timers are written
bool g_failed = false;
bool g_in = false;        // in the call of fn_005E5CD0 on the frame thread
bool g_calibrate = false;
bool g_key_down = false;
int g_play_frames = 0;
unsigned long g_thread = 0;
uint8_t* g_stubs = nullptr;
uint8_t* g_probe = nullptr;
int g_probe_site = -1;
uint32_t g_probe_table[1] = {};
Site g_sites[kMaxSites];
int g_site_count = 0;
Timer g_timers[wp::kMaxTimers];
int g_count = 1;  // timer 0 is the call of fn_005E5CD0
ot::KeyTable g_keys;    // the vtable or the function of a timer
ot::KeyTable g_slots;   // the entries of the vtables that have a site
wp::Stack g_stack;
wp::Window g_window;
time_log_math::Stat g_objects;
double g_pair_ns = 0.0;
double g_tick_mhz = 0.0;
int64_t g_window_tsc = 0;
LARGE_INTEGER g_window_qpc = {};
LARGE_INTEGER g_hz = {};
uint32_t g_no_timer = 0;  // calls with no room for a timer

int64_t Now() { return static_cast<int64_t>(__rdtsc()); }

int64_t Qpc() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

uint32_t Read32(uint32_t a) { return *reinterpret_cast<const uint32_t*>(a); }

void __cdecl ExitThunk();
void __cdecl NoReturn();

// The timer of a call to `target` from `s`. A new key gets a new timer.
int TimerOf(const Site& s, const uint32_t* regs, uint32_t target) {
  if (s.key == kKeySite) return s.timer;
  uint32_t key = target;
  if (s.key == kKeyClass) {
    const uint32_t self = regs[ot::PushadIndex(1)];  // ecx
    key = self != 0 ? Read32(self) : 0;
  }
  if (key == 0) return -1;
  int t = g_keys.Find(key);
  if (t >= 0) return t;
  if (g_count >= kProbeTimer) {
    ++g_no_timer;
    return -1;
  }
  t = g_count++;
  Timer& tm = g_timers[t];
  if (s.key == kKeyClass) {
    tm.shown = target;
    _snprintf_s(tm.name, sizeof(tm.name), _TRUNCATE, "%s, the class with the vtable 0x%08X",
                DrawName(target), static_cast<unsigned>(key));
  } else {
    tm.shown = target;
    _snprintf_s(tm.name, sizeof(tm.name), _TRUNCATE, "fn_%08X, %s",
                static_cast<unsigned>(target),
                s.call.form == ot::kSlot ? "an entry of a vtable of a 3D part" : "a method");
  }
  g_keys.Add(key, t);
  return t;
}

}  // namespace

// The stub of a site pushes its id and jumps to EnterThunk. The block of
// pushad is at regs, the flags at regs[8], the id at regs[9], the return
// address at regs[10].
extern "C" uint32_t __cdecl ObjectTimerEnter(uint32_t* regs) {
  const int id = static_cast<int>(regs[9]);
  const Site& s = g_sites[id];
  const uint32_t target = ot::Target(s.call, regs, Read32);
  if ((g_on || g_calibrate) && ThisThreadId() == g_thread) {
    int timer = -1;
    if (id == g_probe_site) {
      if (g_calibrate) timer = kProbeTimer;
    } else if (id == 0) {
      if (g_stack.depth == 0) {
        timer = 0;
        g_in = true;
      }
    } else if (g_in) {
      timer = TimerOf(s, regs, target);
    }
    if (timer >= 0) {
      uint32_t* slot = &regs[10];
      if (g_stack.Push(timer, Now(), *slot,
                       static_cast<uint32_t>(reinterpret_cast<uintptr_t>(slot)))) {
        *slot = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ExitThunk));
      } else if (id == 0) {
        g_in = false;
      }
    }
  }
  return target;
}

// ExitThunk comes here with the stack place after the return of the
// function. Integers only.
extern "C" uint32_t __cdecl ObjectTimerExit(uint32_t esp) {
  const uint32_t ret = g_stack.Return(Now(), esp);
  if (g_stack.depth == 0) g_in = false;
  if (ret != 0) return ret;
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&NoReturn));
}

namespace {

__declspec(naked) void EnterThunk() {
  __asm {
    pushfd
    pushad
    mov eax, esp
    push eax
    call ObjectTimerEnter
    add esp, 4
    // The place of the id gets the address of the function.
    mov [esp + 36], eax
    popad
    popfd
    ret
  }
}

__declspec(naked) void __cdecl ExitThunk() {
  __asm {
    // A place for the return address of the caller.
    push eax
    pushfd
    pushad
    lea eax, [esp + 40]
    push eax
    call ObjectTimerExit
    add esp, 4
    mov [esp + 36], eax
    popad
    popfd
    ret
  }
}

// A return with no open call on the stack of the timers. It cannot occur when
// each call returns. The fault goes to the crash report of the mod.
__declspec(naked) void __cdecl NoReturn() {
  __asm {
    ud2
  }
}

volatile int g_sink = 0;
__declspec(noinline) void __cdecl ProbeTarget() { g_sink = g_sink + 1; }

uint32_t StubOf(int id) {
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_stubs + id * kStubBytes));
}

void MakeStub(int id) {
  uint8_t* at = g_stubs + id * kStubBytes;
  at[0] = 0x68;  // push id
  const uint32_t v = static_cast<uint32_t>(id);
  memcpy(at + 1, &v, 4);
  at[5] = 0xE9;  // jmp EnterThunk
  const int32_t rel = static_cast<int32_t>(reinterpret_cast<intptr_t>(&EnterThunk) -
                                           reinterpret_cast<intptr_t>(at + 10));
  memcpy(at + 6, &rel, 4);
}

// Writes `n` bytes at `at`. The frame thread writes, and no other thread runs
// this code (see the head of the file).
bool Write(uint32_t at, const void* bytes, size_t n, bool code) {
  void* p = reinterpret_cast<void*>(at);
  DWORD old = 0;
  if (!VirtualProtect(p, n, code ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE, &old)) return false;
  memcpy(p, bytes, n);
  DWORD back = 0;
  VirtualProtect(p, n, old, &back);
  if (code) FlushInstructionCache(GetCurrentProcess(), p, n);
  return true;
}

// A site for the call at `at`. Gives its id, or -1 and a line.
int AddCall(uint32_t at, int key, const char* name) {
  if (g_site_count >= kMaxSites) return -1;
  const uint8_t* b = reinterpret_cast<const uint8_t*>(at);
  if (IsBadReadPtr(b, 8)) return -1;
  const ot::Call c = ot::DecodeCall(b, at);
  if (c.form == ot::kNoCall) {
    ModLog("object timer : no timer at 0x%08X: the bytes %02X %02X %02X %02X %02X %02X are "
           "not a call that the timer takes",
           static_cast<unsigned>(at), b[0], b[1], b[2], b[3], b[4], b[5]);
    return -1;
  }
  const int id = g_site_count++;
  Site& s = g_sites[id];
  s.at = at;
  s.call = c;
  s.key = key;
  memcpy(s.saved, b, 8);
  MakeStub(id);
  if (key == kKeySite) {
    if (g_count >= kProbeTimer) {
      --g_site_count;
      return -1;
    }
    s.timer = g_count++;
    Timer& t = g_timers[s.timer];
    t.shown = at;
    _snprintf_s(t.name, sizeof(t.name), _TRUNCATE, "%s", name);
  }
  return id;
}

bool PutSite(int id) {
  Site& s = g_sites[id];
  if (s.on) return true;
  if (s.call.form == ot::kSlot) {
    const uint32_t stub = StubOf(id);
    s.on = Write(s.at, &stub, 4, false);
    return s.on;
  }
  uint8_t bytes[8];
  if (!ot::CallBytes(s.at, StubOf(id), s.call.length, bytes)) return false;
  // The bytes of the game must still be there.
  if (memcmp(reinterpret_cast<const void*>(s.at), s.saved, s.call.length) != 0) return false;
  s.on = Write(s.at, bytes, s.call.length, true);
  return s.on;
}

void TakeSite(int id) {
  Site& s = g_sites[id];
  if (!s.on) return;
  if (s.call.form == ot::kSlot) {
    Write(s.at, &s.call.target, 4, false);
  } else {
    Write(s.at, s.saved, s.call.length, true);
  }
  s.on = false;
}

// The timers of the entries of the vtables of the 3D parts of the objects in
// the list. Gives the count of new ones.
int ScanParts() {
  int n = *reinterpret_cast<const int32_t*>(kListCount);
  if (n < 0) n = 0;
  if (n > kListMost) n = kListMost;
  int added = 0;
  for (int i = 0; i < n; ++i) {
    const uint32_t object = Read32(kList + 4 * i);
    if (object == 0 || IsBadReadPtr(reinterpret_cast<const void*>(object), 0x44)) continue;
    const uint32_t part = Read32(object + 0x40);
    if (part == 0 || IsBadReadPtr(reinterpret_cast<const void*>(part), 4)) continue;
    const uint32_t table = Read32(part);
    if (!InTables(table)) continue;
    for (uint32_t off : kPartSlots) {
      const uint32_t entry = table + off;
      if (g_slots.Find(entry) >= 0) continue;
      const uint32_t fn = Read32(entry);
      if (!InText(fn) || g_site_count >= kMaxSites) continue;
      const int id = g_site_count++;
      Site& s = g_sites[id];
      s.at = entry;
      s.call.form = ot::kSlot;
      s.call.length = 4;
      s.call.target = fn;
      s.key = kKeyFunction;
      MakeStub(id);
      g_slots.Add(entry, id);
      if (PutSite(id)) ++added;
    }
  }
  return added;
}

// The cost of one timed call: calls of the probe with a timer, less the same
// calls with no timer. The least time of 5 rounds of each counts.
double MeasureCost() {
  g_probe = static_cast<uint8_t*>(
      VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
  if (g_probe == nullptr) return 0.0;
  // mov eax, g_probe_table; call dword ptr [eax + 0]; ret
  g_probe_table[0] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ProbeTarget));
  const uint32_t table = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_probe_table));
  g_probe[0] = 0xB8;
  memcpy(g_probe + 1, &table, 4);
  const uint8_t call[6] = {0xFF, 0x90, 0, 0, 0, 0};
  memcpy(g_probe + 5, call, 6);
  g_probe[11] = 0xC3;
  FlushInstructionCache(GetCurrentProcess(), g_probe, 16);
  const uint32_t site = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_probe + 5));
  void (*volatile probe)() = reinterpret_cast<void (*)()>(g_probe);
  const int kCalls = 20000;
  const int kRounds = 5;
  int64_t bare = INT64_MAX;
  for (int r = 0; r < kRounds; ++r) {
    const int64_t t0 = Now();
    for (int i = 0; i < kCalls; ++i) probe();
    const int64_t t = Now() - t0;
    if (t < bare) bare = t;
  }
  g_probe_site = AddCall(site, kKeyFunction, "the probe");
  if (g_probe_site < 0 || !PutSite(g_probe_site)) return 0.0;
  const int64_t q0 = Qpc();
  const int64_t n0 = Now();
  int64_t timed = INT64_MAX;
  for (int r = 0; r < kRounds; ++r) {
    g_calibrate = true;
    const int64_t t0 = Now();
    for (int i = 0; i < kCalls; ++i) probe();
    const int64_t t = Now() - t0;
    g_calibrate = false;
    if (t < timed) timed = t;
  }
  const int64_t n1 = Now();
  const int64_t q1 = Qpc();
  g_stack.NewFrame();
  const double span_ms = static_cast<double>(q1 - q0) * 1000.0 / static_cast<double>(g_hz.QuadPart);
  if (n1 > n0 && span_ms > 0.0) g_tick_mhz = static_cast<double>(n1 - n0) / span_ms / 1000.0;
  if (g_tick_mhz <= 0.0) return 0.0;
  const double ns = static_cast<double>(timed - bare) * 1000.0 / g_tick_mhz / kCalls;
  return ns > 0.0 ? ns : 0.0;
}

// The first start makes the sites. Each start writes them.
void Start(const char* why) {
  if (g_on || g_failed) return;
  if (g_stubs == nullptr) {
    g_stubs = static_cast<uint8_t*>(VirtualAlloc(nullptr, kMaxSites * kStubBytes,
                                                 MEM_COMMIT | MEM_RESERVE,
                                                 PAGE_EXECUTE_READWRITE));
    if (g_stubs == nullptr || g_hz.QuadPart == 0) {
      g_failed = true;
      ModLog("object timer : no memory for the stubs, or no clock. No timer");
      return;
    }
    g_thread = ThisThreadId();
    g_timers[0].shown = kDrawObjects;
    _snprintf_s(g_timers[0].name, sizeof(g_timers[0].name), _TRUNCATE,
                "fn_005E5CD0, the draw of the objects, outside each timer");
    // Site 0 is the call of fn_005E5CD0, with timer 0.
    const int outer = AddCall(kOuterSite, kKeyFunction, "fn_005E5CD0");
    if (outer != 0 || g_sites[0].call.target != kDrawObjects) {
      g_failed = true;
      ModLog("object timer : the code at 0x%08X is not the call of fn_005E5CD0. No timer",
             static_cast<unsigned>(kOuterSite));
      return;
    }
    for (const Fixed& f : kFixed) AddCall(f.at, f.key, f.name);
    char list[sizeof(g_sites_text)];
    memcpy(list, g_sites_text, sizeof(list));
    char* next = nullptr;
    for (char* tok = strtok_s(list, ", ", &next); tok != nullptr;
         tok = strtok_s(nullptr, ", ", &next)) {
      const uint32_t a = static_cast<uint32_t>(strtoul(tok, nullptr, 16));
      if (!InText(a)) continue;
      const uint8_t* b = reinterpret_cast<const uint8_t*>(a);
      char name[64];
      _snprintf_s(name, sizeof(name), _TRUNCATE, "the call at 0x%08X (object_timer_sites)",
                  static_cast<unsigned>(a));
      const bool direct = !IsBadReadPtr(b, 1) && b[0] == 0xE8;
      AddCall(a, direct ? kKeySite : kKeyFunction, name);
    }
    g_pair_ns = MeasureCost();
  }
  if (g_thread != ThisThreadId()) return;
  int on = 0;
  int sites = 0;
  for (int i = 0; i < g_site_count; ++i) {
    if (i == g_probe_site || g_sites[i].call.form == ot::kSlot) continue;
    ++sites;
    if (PutSite(i)) ++on;
  }
  if (!g_sites[0].on) {
    ModLog("object timer : the call of fn_005E5CD0 at 0x%08X has other bytes now. No timer",
           static_cast<unsigned>(kOuterSite));
    for (int i = 0; i < g_site_count; ++i) TakeSite(i);
    return;
  }
  int slots = 0;
  for (int i = 0; i < g_site_count; ++i) {
    if (g_sites[i].call.form != ot::kSlot) continue;
    if (PutSite(i)) ++slots;
  }
  g_on = true;
  slots += ScanParts();
  g_stack = wp::Stack();
  g_window.Reset();
  g_objects = time_log_math::Stat();
  if (g_tick_mhz > 0.0) g_window.ms_per_tick = 1.0 / (g_tick_mhz * 1000.0);
  g_window_tsc = Now();
  QueryPerformanceCounter(&g_window_qpc);
  ModLog("object timer : %s. %d of %d calls and %d entries of vtables have a timer, on the frame "
         "thread, with no stop of the other threads. One timed call costs %.0f ns (the least of "
         "5 rounds of 20000 calls of a probe with and without a timer). The clock of the timers "
         "runs at %.3f MHz. Each 10 s the lines of the objects",
         why, on, sites, slots, g_pair_ns, g_tick_mhz);
}

void Stop(const char* why) {
  if (!g_on) return;
  for (int i = g_site_count - 1; i >= 0; --i) {
    if (i != g_probe_site) TakeSite(i);
  }
  g_on = false;
  g_in = false;
  ModLog("object timer : %s. The bytes of the game and the entries of the vtables are back",
         why);
}

void Report() {
  LARGE_INTEGER q;
  QueryPerformanceCounter(&q);
  const int64_t now = Now();
  const double ms = static_cast<double>(q.QuadPart - g_window_qpc.QuadPart) * 1000.0 /
                    static_cast<double>(g_hz.QuadPart);
  if (now > g_window_tsc) g_window.ms_per_tick = ms / static_cast<double>(now - g_window_tsc);
  char line[512];
  ot::HeadLine(line, sizeof(line), g_window, g_count, ms / 1000.0, g_pair_ns, g_objects.Mean());
  ModLog("object timer : %s", line);
  int ids[wp::kMaxTimers];
  const int n = wp::Order(g_window, g_count, ids);
  const int shown = n < 40 ? n : 40;
  for (int r = 0; r < shown; ++r) {
    const Timer& t = g_timers[ids[r]];
    wp::TimerLine(line, sizeof(line), g_window, ids[r], r + 1, t.shown, t.name);
    ModLog("object timer : %s", line);
  }
  if (n > shown) ModLog("object timer : %d more timers with calls", n - shown);
  if (g_stack.lost > 0 || g_no_timer > 0) {
    ModLog("object timer : since the start, %llu timed calls had no return, and %u calls had "
           "no room for a timer",
           static_cast<unsigned long long>(g_stack.lost), g_no_timer);
  }
  const int more = ScanParts();
  if (more > 0) ModLog("object timer : %d more entries of vtables have a timer", more);
  g_window.Reset();
  g_objects = time_log_math::Stat();
  g_window_tsc = Now();
  QueryPerformanceCounter(&g_window_qpc);
}

void Keys() {
  if (!g_test_keys) return;
  const bool down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
  if (down && !g_key_down) {
    if (g_on) {
      Stop("Ctrl Shift F7 stops the timers");
    } else {
      Start("Ctrl Shift F7 starts the timers");
    }
  }
  g_key_down = down;
}

}  // namespace

void ObjectTimerSetOptions(int mode, const char* sites, bool test_keys) {
  g_mode = mode == 1 ? 1 : 0;
  g_test_keys = test_keys;
  g_sites_text[0] = 0;
  if (sites != nullptr) strncpy_s(g_sites_text, sizeof(g_sites_text), sites, _TRUNCATE);
  QueryPerformanceFrequency(&g_hz);
  if (g_mode != 0) {
    ModLog("object timer : object_timer=1, object_timer_sites=%s. The timers come when the list "
           "of the objects has objects at the end of %d frames in a row%s",
           g_sites_text[0] != 0 ? g_sites_text : "(none)", kPlayFrames,
           g_test_keys ? ", or at Ctrl Shift F7" : "");
  }
}

void ObjectTimerFrame() {
  if (g_mode == 0 && !g_test_keys) return;
  Keys();
  if (!g_on) {
    if (g_mode == 1 && !g_failed && g_stubs == nullptr) {
      const int32_t n = *reinterpret_cast<const int32_t*>(kListCount);
      g_play_frames = n > 0 ? g_play_frames + 1 : 0;
      if (g_play_frames >= kPlayFrames) Start("The play runs");
    }
    return;
  }
  if (ThisThreadId() != g_thread) return;
  // fn_005E5CD0 returned before FinishFrame. A call that is still open has
  // lost its return.
  while (g_stack.depth > 0) g_stack.DropTop();
  g_in = false;
  if (g_stack.calls[0] > 0) {
    g_window.AddFrame(g_stack, g_count);
    g_objects.Add(static_cast<double>(*reinterpret_cast<const int32_t*>(kListCount)));
  }
  g_stack.NewFrame();
  LARGE_INTEGER q;
  QueryPerformanceCounter(&q);
  const double ms = static_cast<double>(q.QuadPart - g_window_qpc.QuadPart) * 1000.0 /
                    static_cast<double>(g_hz.QuadPart);
  if (ms >= time_log_math::kWindowMs && g_window.frames > 0) Report();
}
