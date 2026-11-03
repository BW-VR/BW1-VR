// THE PARTS OF THE GAME TURN (turn_parts, turn_parts_math.h, docs/backend.md,
// "The long game turn").
//
// The game turn is the call of GGame::ProcessNetworkPackets that runs
// GGame::ProcessOneGameTurn: StartTurn, ProcessTurn and EndTurn. A sampler of
// the place of the frame thread does not work under a translator of the code
// (FEX), so the mod times the large functions that the turn calls with the
// clock. The ways to find the large functions are the sampler of the turn on a
// PC (profile_sample_turn) and the timers of this file with turn_parts_add.
//
// HOW A TIMER WORKS (the same way as walk_parts.cpp). Each timed function gets
// a hook of MinHook to a stub of its own. The stub keeps each register and the
// flags, takes the clock, puts the address of ExitThunk in the place of the
// return address, and jumps to the function. At the return ExitThunk takes the
// clock, closes the timer and jumps to the return address of the caller. So a
// stub does not need the arguments or the calling convention of the function.
// The exit code does not use the x87 unit (the build makes SSE2 code for float
// and double): the x87 unit can hold the value that the function returns.
//
// The function that a stub times sees the address of ExitThunk as its return
// address. The code of the game does not read it, but a hook of the mod can
// (_ReturnAddress). So a function that the mod hooks already (the same
// address) gets its timer on the trampoline of that hook (FindTrampoline): the
// code of the game alone.
//
// ALL HOOKS AT THE START. TurnPartsInstall queues the hooks and applies them
// with one MH_ApplyQueued, from PassGuardInstall, as anim_trig.cpp does. MinHook
// stops each other thread while it writes hooks, and a stop of a thread that a
// translator of the code translates can hang the process (docs/android.md,
// "SuspendThread under FEX in Wine"). No hook comes after the start of the game.
//
// Only the frame thread in a game turn counts. A call on another thread, or
// outside the turn, goes through the stub with no clock read. A part counts
// once, at its outer call. Timer 0 is the game turn, so its exclusive time is
// the rest: the turn outside each part.
//
//   turn_parts=0    no hook and no line (the default)
//   turn_parts=1    the timers on the parts of kDefs, each 10 s one line with
//                   the parts of the long turns (1 ms or more) of the window,
//                   and with slow_frame_ms the parts of the turn in each line
//                   of a long frame
//   turn_parts_add  more addresses to time, for example
//                   turn_parts_add=0x00730760,0x0063E090. The name of such a
//                   part is its address (fn_00730760)
//
// It needs time_log=1, which writes the lines.
//
// THE TIMELINE (trace=1, trace_rec.h). The same stubs also give a bar to the
// timeline for each part that returns, so with trace=1 the hooks go in even
// with turn_parts=0 (no line is written then). A second group of stubs times
// the callees of GGame::ProcessGraphicsEngine, two levels deep (kGraphicsDefs):
// the scene walk, the draws of the land, the interface and the particles. The
// group counts only between TurnPartsGraphicsBegin and TurnPartsGraphicsEnd
// (the call of ProcessGraphicsEngine on the frame thread), and only with
// trace=1.
//
// THE CALLEES OF GLandscape::Draw (trace_draw_parts=1, kDrawDefs): its callees
// and their callees, in the same group. A timer that is called more than
// bar_split_math::kCallsCap (200) times in a frame is a helper: the timeline
// leaves it out from then on, and the log names it.
//
// THE DEVICE IN A BAR (trace_device=1, bar_split_math.h). The device of the mod
// adds each call of Direct3D 7 to the running sum g_chain_acc (render_chain.h).
// A timer of the group keeps the sum at its call. At its return the bar gets
// four numbers: the time in the calls of the device inside the bar that no bar
// in it covers (device_us), and the draws, the state calls (render states and
// stage states) and the textures of those calls. So the own time of a bar
// splits into the device and the game (the rest). After each frame of the
// graphics engine three counters have the calls and the time of the device in
// the whole call. Each 900 frames the log has the table of the timers.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MinHook.h"
#include "audio_watch.h"
#include "bar_split_math.h"
#include "render_chain.h"
#include "thread_id.h"
#include "time_log.h"
#include "trace_rec.h"
#include "turn_parts.h"
#include "turn_parts_math.h"
#include "walk_parts_math.h"

void ModLog(const char* format, ...);

bool g_turn_parts = false;

namespace {

namespace tp = turn_parts_math;
namespace wp = walk_parts_math;
namespace rc = render_chain_math;

// What a part counts besides its place.
enum Filter {
  kAll = 0,           // each call
  kMadeAnimation = 1  // only a call of Morphable::GetSetAnim that makes an animation
};

struct Def {
  uint32_t address;
  const char* name;
  int filter;
};

// The callees of GGame::ProcessGraphicsEngine (0x0054D850) and of its callees,
// for the timeline. The names are those of the symbol file of the decompilation
// project that the mod uses for its names (docs/address-map.md). GGame::
// Process3dEngine has its own bars (stereo.cpp), and LH3DRender::FinishFrame
// has its own bar in h_FinishFrame (mod_main.cpp). LH3DTech::UpdateCamera
// (head_camera.cpp) and PhysicsObject::DrawAll (throw_physics.cpp) are under a
// hook of the mod that the stubs cannot time, so their hooks give the bars.
// A part that the game turn also calls has one timer, in kDefs (physics_turn is
// PhysicsObject::GameTurnUpdate at 0x00644FC0, and fn_007DEE00). That timer
// counts in the call of ProcessGraphicsEngine too, so kGraphicsDefs does not
// list it again.
const Def kGraphicsDefs[] = {
    // The callees of ProcessGraphicsEngine.
    {0x007E4810, "LHMouse::UpdateDeltaPos", kAll}, {0x00441F80, "GCamera::Update", kAll},
    {0x005CE9E0, "GInterface::PreDrawProcess", kAll},
    {0x005CEAB0, "GInterface::PostDrawProcess", kAll},
    {0x005C5EB0, "HelpSystem::PostDrawProcess", kAll},
    // Their callees.
    {0x00442720, "Zoomer::Update", kAll},
    {0x00803090, "LH3DIsland::GetAltitude", kAll},
    {0x00442EF0, "GCamera::UpdateGameThingWithPosData", kAll},
    {0x005D0610, "GInterface::UpdateHandRenderCollide", kAll},
    {0x00756170, "Villager::ShowDiscipleIcon", kAll}, {0x005D5980, "fn_005D5980", kAll},
    {0x005D9130, "GInterface::UpdateAllLeashes", kAll},
    {0x005D5A70, "GInterface::UpdateInterfaceCollide", kAll},
    {0x005D8320, "GInterface::UpdateCollideImmersion", kAll},
    // The callees of Process3dEngine.
    {0x0082F0E0, "LH3DRender::StartFrame", kAll}, {0x008357A0, "LH3DAtmos::Update3D", kAll},
    {0x00825190, "TemporaryShadow::UpdateAll", kAll},
    {0x005E2160, "GLandAlignement::DrawSky", kAll}, {0x00794370, "Temple::Draw", kAll},
    {0x00793EE0, "Temple::Update", kAll}, {0x005E3F60, "GLandscape::PreDraw", kAll},
    {0x006CA6E0, "PSysLightMaps::AddDrawing", kAll},
    {0x00871F00, "LH3DLandscape::TextureUpdateThread", kAll},
    {0x005E42E0, "GLandscape::Draw", kAll}, {0x0048DD70, "LH3DCreature::DrawFightSparkles", kAll},
    {0x005D9310, "GInterface::DrawAllLeashes", kAll},
    {0x0046E490, "CHand::UpdateHeldObject", kAll}, {0x0046D100, "CHand::AddDrawing", kAll},
    {0x00518640, "GInterface::Draw", kAll}, {0x00845D00, "UpdateLiquidParticles", kAll},
    {0x00845C50, "DrawLiquidParticles", kAll}, {0x005533B0, "GGame::Draw", kAll},
    {0x00520E70, "EditorIconBase::DrawMouseOver", kAll},
    {0x0068F5E0, "PSysGlobal::DrawLoop", kAll}, {0x007203F0, "Spell::DrawSpells", kAll},
    {0x0063E0D0, "GParticleContainer::DrawParticleContainers", kAll},
    {0x0064C140, "GPlayer::DrawPlayers", kAll}, {0x007447F0, "TownCentre::DrawAll", kAll},
    {0x00836250, "LH3DAtmos::Render3D", kAll}, {0x008366A0, "LH3DAtmos::Render2D", kAll},
    {0x006EB9D0, "GScript::ProcessFade", kAll}, {0x00794280, "Temple::UpdateFade", kAll},
    {0x005C59A0, "HelpSystem::Draw3D", kAll}, {0x005E57B0, "ClearLight", kAll},
    {0x00826C90, "InfluenceCircle::Draw", kAll}, {0x007627E0, "VillagerNameBlock::DeleteAll", kAll},
    {0x007635D0, "Town::DisplayHowImpressed", kAll},
    {0x0064C1A0, "GPlayer::DrawComputerPlayers", kAll},
    {0x00445E10, "GCameraEditor::Draw3DPart", kAll},
};
constexpr int kGraphicsDefCount = sizeof(kGraphicsDefs) / sizeof(kGraphicsDefs[0]);

// The callees of GLandscape::Draw (0x005E42E0) and their callees, found in the
// disassembly of the exe: each direct callee of 96 bytes or more, and from
// those each direct callee of 96 bytes or more. A helper that the game calls
// at many places (the access to the interface, the conversion of a float, the
// random number, the altitude, the allocator, the small math functions) has no
// timer. A function with a timer already (kDefs, kGraphicsDefs) has no second
// one. The names are those of the symbol file.
const Def kDrawDefs[] = {
    {0x004059F0, "Windmill::PreDraw", kAll}, {0x00442C20, nullptr, kAll},
    {0x004670D0, "CitadelHeart::DrawNow", kAll}, {0x005DF1B0, nullptr, kAll},
    {0x005DFCE0, nullptr, kAll}, {0x007FAE60, nullptr, kAll}, {0x00839980, nullptr, kAll},
    {0x005DFF20, "PetitNavire::PreDraw", kAll}, {0x005E1020, "PetitNavire::PetitNavire", kAll},
    {0x005E13C0, nullptr, kAll}, {0x00801C90, nullptr, kAll}, {0x0083AC70, nullptr, kAll},
    {0x005E1DE0, nullptr, kAll}, {0x0086A2C0, nullptr, kAll}, {0x0086A330, nullptr, kAll},
    {0x005E2D10, "LandFeature::DrawWorm", kAll}, {0x007FEB30, nullptr, kAll},
    {0x0080B4D0, "LH3DObject::Create", kAll}, {0x005E3F70, nullptr, kAll},
    {0x0081C090, "LH3DTech::Draw3DWorldTriangle", kAll}, {0x005E5100, nullptr, kAll},
    {0x00840530, "LH3DSprite::Draw", kAll}, {0x005E5830, nullptr, kAll},
    {0x005E25C0, nullptr, kAll}, {0x0081E1F0, nullptr, kAll}, {0x008229B0, nullptr, kAll},
    {0x00823460, nullptr, kAll}, {0x00823690, nullptr, kAll},
    {0x0086A1B0, "LH3DSky::Time2SkyType", kAll}, {0x005E5CD0, nullptr, kAll},
    {0x00429E30, "GAudio::PlaySoundEffect", kAll}, {0x00439B90, nullptr, kAll},
    {0x0043A9E0, nullptr, kAll}, {0x005D8A10, "GInterface::StartImmersion", kAll},
    {0x005DF640, nullptr, kAll}, {0x005E03F0, "PetitNavire::PostDraw", kAll},
    {0x005E3770, "DesignedWaterFall", kAll}, {0x005E66A0, nullptr, kAll},
    {0x00823570, nullptr, kAll}, {0x00824140, nullptr, kAll}, {0x00825350, nullptr, kAll},
    {0x008254A0, nullptr, kAll}, {0x00826470, nullptr, kAll}, {0x008274A0, nullptr, kAll},
    {0x00827820, nullptr, kAll}, {0x0083F310, "LH3DZSorter::NewZObject", kAll},
    {0x005FFBB0, nullptr, kAll}, {0x00646FE0, nullptr, kAll}, {0x005167D0, nullptr, kAll},
    {0x007FB5C0, nullptr, kAll}, {0x007FCE80, nullptr, kAll}, {0x0074A7C0, "Tree::PreDraw", kAll},
    {0x00774E30, nullptr, kAll}, {0x007FF4F0, nullptr, kAll},
    {0x007FF2D0, "LH3DIsland::PreDraw", kAll}, {0x00874AA0, nullptr, kAll},
    {0x00877D20, nullptr, kAll}, {0x007FF610, nullptr, kAll}, {0x00878350, nullptr, kAll},
    {0x00802550, nullptr, kAll}, {0x00802680, nullptr, kAll}, {0x0081F820, nullptr, kAll},
    {0x00822560, nullptr, kAll}, {0x00824B90, nullptr, kAll}, {0x00824DA0, nullptr, kAll},
    {0x00829BC0, nullptr, kAll}, {0x00806D00, "LH3DMesh::Release", kAll},
    {0x00825E70, nullptr, kAll}, {0x008296D0, nullptr, kAll}, {0x00874850, nullptr, kAll},
    {0x00806F60, nullptr, kAll}, {0x00874600, nullptr, kAll}, {0x008801D0, nullptr, kAll},
    {0x00879930, nullptr, kAll}, {0x0082A500, "DrawAndClip", kAll}, {0x00879500, nullptr, kAll},
    {0x0087A090, nullptr, kAll},
};
constexpr int kDrawDefCount = sizeof(kDrawDefs) / sizeof(kDrawDefs[0]);

// The parts: GGame::ProcessOneGameTurn (0x0054D620), the three phases of the
// turn, and the functions that the phases call. Every direct callee of the
// phases has a timer, so the parts add up to the turn and the rest (the turn
// outside each part) stays near 0. A part with no name has the name fn_ and its
// address. `process_turn` has a call in each real turn. The helpers that the
// phases call many times (fn_00552F80, fn_00555850, fn_007AEE98) have no timer.
const Def kDefs[] = {
    {0x0054E5C0, "process_turn", kAll},
    {0x0054E4F0, "start_turn", kAll},
    {0x0054E960, "end_turn", kAll},
    {0x0054D620, "one_game_turn", kAll},
    {0x00649A20, "players_towns", kAll},
    {0x005EC810, nullptr, kAll},
    {0x006EB6B0, "script", kAll},
    {0x00619500, "morph_texture", kAll},
    {0x0043B7E0, "building_site", kAll},
    {0x008067F0, "mesh_load", kAll},
    {0x00619690, "new_animation", kMadeAnimation},
    {0x00427080, "audio_turn", kAll},
    {0x00644FC0, "physics_turn", kAll},
    // The other callees of ProcessTurn.
    {0x00555280, nullptr, kAll}, {0x00441F50, nullptr, kAll}, {0x0054E910, nullptr, kAll},
    {0x0050BB60, nullptr, kAll}, {0x0071A650, nullptr, kAll}, {0x008356E0, nullptr, kAll},
    {0x00775140, nullptr, kAll}, {0x004380B0, nullptr, kAll}, {0x0071D1C0, nullptr, kAll},
    {0x0071B020, nullptr, kAll}, {0x005E1FE0, nullptr, kAll}, {0x00720300, nullptr, kAll},
    {0x00539D70, nullptr, kAll}, {0x0070A460, nullptr, kAll}, {0x00633EF0, nullptr, kAll},
    {0x00550DD0, nullptr, kAll}, {0x0072E310, nullptr, kAll}, {0x0070D360, nullptr, kAll},
    {0x005C8FE0, nullptr, kAll}, {0x006E3B50, nullptr, kAll}, {0x007741A0, nullptr, kAll},
    {0x0046E4E0, nullptr, kAll}, {0x006EB710, nullptr, kAll}, {0x007E1420, nullptr, kAll},
    {0x00435F30, nullptr, kAll}, {0x006F6910, nullptr, kAll}, {0x00439DD0, nullptr, kAll},
    {0x0056FB10, nullptr, kAll}, {0x00730760, nullptr, kAll}, {0x005CDB90, nullptr, kAll},
    {0x0052B7A0, nullptr, kAll}, {0x00557B60, nullptr, kAll}, {0x00771BE0, nullptr, kAll},
    {0x0067D630, nullptr, kAll}, {0x006EBBC0, nullptr, kAll}, {0x00591370, nullptr, kAll},
    {0x00441D40, nullptr, kAll}, {0x0076EAF0, nullptr, kAll}, {0x0063E090, nullptr, kAll},
    {0x00606FC0, nullptr, kAll}, {0x0070B220, nullptr, kAll}, {0x0068F590, nullptr, kAll},
    {0x0068F5B0, nullptr, kAll}, {0x00552640, nullptr, kAll}, {0x006E6890, nullptr, kAll},
    {0x005C4660, nullptr, kAll},
    // The callees of StartTurn.
    {0x00511F50, nullptr, kAll}, {0x00557BA0, nullptr, kAll},
    // The callees of EndTurn.
    {0x007DEE00, nullptr, kAll}, {0x0071E5F0, nullptr, kAll}, {0x00564050, nullptr, kAll},
    {0x00565110, nullptr, kAll}, {0x0071D6F0, nullptr, kAll}, {0x006360B0, nullptr, kAll},
    {0x006016D0, nullptr, kAll}, {0x00601850, nullptr, kAll}, {0x004286C0, nullptr, kAll},
};
constexpr int kDefCount = sizeof(kDefs) / sizeof(kDefs[0]);
// The index of process_turn in kDefs: a game turn with a call of it is a real
// turn.
constexpr int kProcessTurnDef = 0;
// GAudio::ProcessAudioGameTurn, the audio turn.
constexpr uint32_t kAudioTurnAddress = 0x00427080;

// The animations of a Morphable (anim_watch.cpp): a table of the object at
// +0x1D4, one slot for each number (0x2B8) and each kind. An empty slot means
// that GetSetAnim makes the animation.
const int kSlotsOffset = 0x1D4;
const int kAnimsPerKind = 0x2B8;
const int kKinds = 6;

struct Timer {
  uint32_t shown = 0;       // the address of the function
  uint32_t at = 0;          // the code that has the hook: shown, or its trampoline
  void* original = nullptr; // the trampoline that the stub jumps to
  int filter = kAll;
  bool graphics_only = false;  // a callee of ProcessGraphicsEngine: counts only there
  bool draw_part = false;      // a callee of GLandscape::Draw (kDrawDefs): the cap of calls
  char name[48] = {};
};
// The bar of a part in the timeline: the name and the address.
trace_events::Def g_trace_defs[tp::kMaxParts];
char g_trace_names[tp::kMaxParts][72];

// The split of the bars into the game and the device (trace_device=1).
bool g_split_on = false;
bar_split_math::Stack g_split;
bar_split_math::ClockRate g_rate;
bar_split_math::Acc g_frame_acc0;  // the running sum at the start of the graphics call
// The numbers of the bar of a timer of the group, and the counters of a frame.
const char* const kSplitArgs[4] = {"device_us", "draws", "state_calls", "textures"};
const trace_events::Def kDeviceCallsDef = {
    "device calls (a frame)", "game", {"draws", "render states", "stage states", "textures"}};
const trace_events::Def kDeviceUsDef = {
    "device us (a frame)", "game", {"draws", "render states", "stage states", "textures"}};
const trace_events::Def kDeviceOtherDef = {
    "device other (a frame)", "game", {"calls", "us", nullptr, nullptr}};
// The timers that got a clock read in one graphics call.
const trace_events::Def kTimedCallsDef = {
    "timed calls (a frame)", "game", {"calls", nullptr, nullptr, nullptr}};
// The write of the table to the log, on the frame thread.
const trace_events::Def kCensusLogDef = {
    "census log (mod)", "mod", {nullptr, nullptr, nullptr, nullptr}};
bool g_census_on = false;     // the table, the counter of the timed calls (trace_draw_parts or trace_device)
bool g_in_probe = false;      // the measure of the cost of a timed call: no bar
int g_probe_id = 0;
uint32_t g_timed0 = 0;        // g_timed_calls at the start of the graphics call
// The calls of each timer of the group in this frame, and the timers that the
// timeline leaves out (more than kCallsCap calls in a frame).
uint32_t g_frame_calls[tp::kMaxParts];
bool g_left_out[tp::kMaxParts];
bool g_left_new = false;
bool g_left_logged[tp::kMaxParts];

// The sums of a window of graphics frames, for the table in the log.
constexpr uint64_t kCensusFrames = 900;
struct Census {
  uint64_t frames = 0;
  uint64_t calls[tp::kMaxParts] = {};
  uint64_t entered[tp::kMaxParts] = {};      // each call of the stub, also of a timer that is left out
  uint64_t timed = 0;                        // the calls that got a clock read
  int64_t own_qpc[tp::kMaxParts] = {};       // own time, ticks of the performance counter
  uint64_t device_tsc[tp::kMaxParts] = {};   // device time in it, ticks of the CPU clock
  uint64_t short_bars[tp::kMaxParts] = {};   // bars under trace_min_us (not in the file)
  bar_split_math::Acc device;                // the device in the whole graphics call
};
Census g_census;

Timer g_timers[tp::kMaxParts];
const char* g_names[tp::kMaxParts];
int g_count = 1;  // timer 0 is the rest
int g_process_turn_id = 0;
int g_audio_turn_id = 0;  // the timer of audio_turn, for audio_turn_watch
uint8_t* g_stubs = nullptr;
const int kStubBytes = 10;

DWORD g_thread = 0;  // the frame thread
bool g_in_turn = false;
bool g_in_graphics = false;  // in the call of GGame::ProcessGraphicsEngine (trace=1)
bool g_turn_lines = false;   // turn_parts=1: the lines of the log
volatile LONG g_timed_calls = 0;  // the calls that got a clock read, for the log of a save
uint64_t g_seen_in_turn = 0;       // calls of a stub in a game turn on the frame thread
uint64_t g_seen_outside = 0;       // calls of a stub on the frame thread outside a game turn
uint64_t g_seen_other_thread = 0;  // calls of a stub on another thread
uint64_t g_turn_function_calls = 0;
LARGE_INTEGER g_hz = {};
double g_ms_per_tick = 0.0;
wp::Stack g_stack;
tp::Frame g_frame;
tp::Window g_window;
uint64_t g_lost = 0;  // open calls that no return closed, in all windows

int64_t Now() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

bool SlotEmpty(uint32_t self, uint32_t anim, uint32_t kind) {
  if (self == 0 || anim >= static_cast<uint32_t>(kAnimsPerKind) ||
      kind >= static_cast<uint32_t>(kKinds)) {
    return false;
  }
  const uint8_t* slot = reinterpret_cast<const uint8_t*>(self) + kSlotsOffset +
                        (anim + kind * kAnimsPerKind) * 4;
  return *reinterpret_cast<const uint32_t*>(slot) == 0;
}

void __cdecl ExitThunk();
void __cdecl NoReturn();

}  // namespace

// The stub of a timer pushes its id and jumps to EnterThunk. The stack then has
// the id at [esp] and the return address of the caller at [esp+4], and the
// arguments of the function after it. `self` is ecx at the call.
extern "C" uint32_t __cdecl TurnPartsEnter(uint32_t id, uint32_t* ret_slot, uint32_t self) {
  const Timer& t = g_timers[id];
  const bool frame_thread = ThisThreadId() == g_thread;
  // The counts of the other threads are not exact (no lock). They are a hint.
  if (!frame_thread) {
    ++g_seen_other_thread;
  } else if (!g_in_turn) {
    ++g_seen_outside;
  } else {
    ++g_seen_in_turn;
  }
  // A timer of the graphics engine counts only in its call: the helpers that
  // the game turn calls many times must not pay for two clock reads a call.
  bool counts = t.graphics_only ? g_in_graphics : (g_in_turn || g_in_graphics);
  // Each call of the stub in the graphics call counts. A callee of
  // GLandscape::Draw that is called more than kCallsCap times in a frame is a
  // helper and is left out from then on, but its calls go on counting.
  if (frame_thread && g_in_graphics && t.graphics_only) {
    const uint32_t n = ++g_frame_calls[id];
    if (t.draw_part && counts) {
      if (g_left_out[id]) {
        counts = false;
      } else if (bar_split_math::LeaveOut(n)) {
        g_left_out[id] = true;
        g_left_new = true;
        counts = false;
      }
    }
  }
  if (counts && frame_thread && tp::Timed(true, true, g_stack.nest[id], g_stack.depth)) {
    const bool take = t.filter != kMadeAnimation || SlotEmpty(self, ret_slot[1], ret_slot[2]);
    const int64_t now = Now();
    if (take &&
        g_stack.Push(static_cast<int>(id), now, *ret_slot,
                     static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ret_slot)))) {
      *ret_slot = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ExitThunk));
      if (g_split_on) g_split.Push(g_stack.depth - 1, g_chain_acc);
      if (static_cast<int>(id) == g_audio_turn_id) AudioWatchTurnBegin(now);
      if (g_trace) ++g_timed_calls;
    }
  }
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t.original));
}

// ExitThunk comes here with the stack place after the return of the function.
// No x87 instruction.
extern "C" uint32_t __cdecl TurnPartsExit(uint32_t esp) {
  const int64_t now = Now();
  const int top = g_stack.Match(esp);
  const bool audio_turn = g_audio_turn_id != 0 && top >= 0 && g_stack.open[top].id == g_audio_turn_id;
  // The bar of the part in the timeline, from its call to its return.
  const int trace_id = top >= 0 ? g_stack.open[top].id : 0;
  const int64_t trace_begin = top >= 0 ? g_stack.open[top].start : 0;
  // The device calls in the bar that no kept bar in it has.
  const bool keeps = g_trace && trace_id > 0 && TraceKeeps(trace_begin, now);
  bar_split_math::Acc own;
  bar_split_math::Acc all_own;
  if (g_split_on && top >= 0) own = g_split.Pop(top, g_chain_acc, keeps, &all_own);
  const uint32_t ret = g_stack.Return(now, esp);
  if (g_trace && ret != 0 && trace_id > 0 && !g_in_probe) {
    if (g_split_on) {
      TraceBar(&g_trace_defs[trace_id], trace_begin, now,
               static_cast<float>(g_rate.Us(own.Ticks())), static_cast<float>(own.calls[rc::kDraw]),
               static_cast<float>(own.StateCalls()), static_cast<float>(own.calls[rc::kTexture]));
    } else {
      TraceBar(&g_trace_defs[trace_id], trace_begin, now);
    }
    if (g_census_on && g_in_graphics) {
      g_census.device_tsc[trace_id] += all_own.Ticks();
      if (!keeps) ++g_census.short_bars[trace_id];
    }
  }
  if (audio_turn) AudioWatchTurnEnd(now);
  if (ret != 0) return ret;
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&NoReturn));
}

namespace {

__declspec(naked) void EnterThunk() {
  __asm {
    pushfd
    pushad
    // [esp+36] is the id, [esp+40] the return address. pushad keeps ecx at
    // [esp+24].
    mov eax, [esp + 24]
    push eax
    lea eax, [esp + 44]
    push eax
    push dword ptr [esp + 44]
    call TurnPartsEnter
    add esp, 12
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
    call TurnPartsExit
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

// The us of a tick of the clock of the CPU, from a spin of 3 ms against the
// performance counter. The rate of the whole run replaces it (ClockRate).
double SeedRate() {
  LARGE_INTEGER a, b;
  QueryPerformanceCounter(&a);
  const uint64_t t0 = __rdtsc();
  do {
    QueryPerformanceCounter(&b);
  } while ((b.QuadPart - a.QuadPart) * 1000 < g_hz.QuadPart * 3);
  const uint64_t t1 = __rdtsc();
  if (t1 <= t0) return 0.0;
  const double us = static_cast<double>(b.QuadPart - a.QuadPart) * 1e6 /
                    static_cast<double>(g_hz.QuadPart);
  return us / static_cast<double>(t1 - t0);
}

// THE COST OF A TIMED CALL. CostProbe gets a timer like the other functions
// (queued with them, so it is in the one MH_ApplyQueued), and stays on. It does
// nothing, and the game never calls it. MeasureCost calls it with and without
// the timer.
volatile int g_probe_sink = 0;
__declspec(noinline) int __cdecl CostProbe(int x) {
  g_probe_sink = g_probe_sink + x;
  return g_probe_sink;
}

bool QueueCostProbe() {
  if (g_count >= tp::kMaxParts) return false;
  void* original = nullptr;
  if (MH_CreateHook(reinterpret_cast<void*>(&CostProbe), g_stubs + g_count * kStubBytes,
                    &original) != MH_OK) {
    return false;
  }
  if (MH_QueueEnableHook(reinterpret_cast<void*>(&CostProbe)) != MH_OK) {
    MH_RemoveHook(reinterpret_cast<void*>(&CostProbe));
    return false;
  }
  Timer& t = g_timers[g_count];
  t.shown = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&CostProbe));
  t.at = t.shown;
  t.original = original;
  t.graphics_only = true;
  _snprintf_s(t.name, sizeof(t.name), _TRUNCATE, "cost_probe");
  g_names[g_count] = t.name;
  MakeStub(g_count);
  g_probe_id = g_count;
  ++g_count;
  return true;
}

// The ns of one timed call: the time of calls of CostProbe through its timer
// (two clock reads of the performance counter, the stack, the split of the
// device, the check of the cap), less the time of the same calls through the
// trampoline. The least time of 5 rounds of each counts. The counts of the
// probe go away.
double MeasureCost() {
  if (g_probe_id == 0 || g_hz.QuadPart <= 0) return 0.0;
  const int kCalls = 20000;
  const int kRounds = 5;
  int (*volatile probe)(int) = &CostProbe;
  int (*volatile bare_probe)(int) =
      reinterpret_cast<int (*)(int)>(g_timers[g_probe_id].original);
  if (bare_probe == nullptr) return 0.0;
  const LONG timed_before = g_timed_calls;
  int64_t bare = INT64_MAX;
  int64_t timed = INT64_MAX;
  for (int r = 0; r < kRounds; ++r) {
    int64_t t0 = Now();
    for (int i = 0; i < kCalls; ++i) bare_probe(i);
    int64_t t = Now() - t0;
    if (t < bare) bare = t;
    g_in_graphics = true;
    g_in_probe = true;
    t0 = Now();
    for (int i = 0; i < kCalls; ++i) probe(i);
    t = Now() - t0;
    g_in_graphics = false;
    g_in_probe = false;
    if (t < timed) timed = t;
  }
  g_stack = wp::Stack();
  memset(g_frame_calls, 0, sizeof(g_frame_calls));
  g_timed_calls = timed_before;
  const double ns = static_cast<double>(timed - bare) * 1e9 /
                    static_cast<double>(g_hz.QuadPart) / kCalls;
  return ns > 0.0 ? ns : 0.0;
}

bool Known(uint32_t address) {
  for (int i = 1; i < g_count; ++i) {
    if (g_timers[i].shown == address || g_timers[i].at == address) return true;
  }
  return false;
}

// True when the code at `t` is a trampoline of MinHook for `target`: memory of
// no module, with a jump back into `target` 5 to 32 bytes after its start in
// its first 32 bytes.
bool IsTrampolineOf(uint32_t t, uint32_t target) {
  MEMORY_BASIC_INFORMATION mbi = {};
  if (VirtualQuery(reinterpret_cast<const void*>(t), &mbi, sizeof(mbi)) == 0 ||
      mbi.Type != MEM_PRIVATE || mbi.State != MEM_COMMIT ||
      (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0 ||
      (mbi.Protect & PAGE_GUARD) != 0) {
    return false;
  }
  // A trampoline can be the last one of a block of MinHook, so the bytes after
  // it can be past the end of the memory.
  const uint32_t end = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mbi.BaseAddress)) +
                       static_cast<uint32_t>(mbi.RegionSize);
  const int room = static_cast<int>(end - t);
  const uint8_t* b = reinterpret_cast<const uint8_t*>(t);
  for (int j = 0; j < 32 && j + 5 <= room; ++j) {
    if (b[j] != 0xE9) continue;
    int32_t rel = 0;
    memcpy(&rel, b + j + 1, 4);
    const uint32_t dest = t + j + 5 + static_cast<uint32_t>(rel);
    if (dest >= target + 5 && dest <= target + 32) return true;
  }
  return false;
}

// THE CODE OF THE GAME UNDER A HOOK OF THE MOD. A stub on the hook of the mod
// itself would change the return address that the hook reads, so the timer goes
// on the trampoline of MinHook, which the hook calls for the code of the game.
// The hook reads the address of the trampoline from a variable of the mod (o_
// and the name of the function). The search reads each 4 bytes of the first 2
// KiB of the code of the hook as an address in the image of the mod, and takes
// the value there when it is a trampoline of `target`. 0 when there is none.
uint32_t FindTrampoline(uint32_t hook, uint32_t target) {
  HMODULE mod = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(hook), &mod)) {
    return 0;
  }
  const uint8_t* base = reinterpret_cast<const uint8_t*>(mod);
  const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
      base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
  const uint32_t lo = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base));
  const uint32_t hi = lo + nt->OptionalHeader.SizeOfImage;
  const uint8_t* code = reinterpret_cast<const uint8_t*>(hook);
  for (int i = 0; i < 2048; ++i) {
    if (IsBadReadPtr(code + i, 4)) break;
    uint32_t v = 0;
    memcpy(&v, code + i, 4);
    if (v < lo || v + 4 > hi || (v & 3) != 0) continue;
    if (IsBadReadPtr(reinterpret_cast<const void*>(v), 4)) continue;
    const uint32_t t = *reinterpret_cast<const uint32_t*>(v);
    if (t != 0 && IsTrampolineOf(t, target)) return t;
  }
  return 0;
}

// A timer on the code at `address`, queued. When the mod hooks that code
// already, the timer goes on the trampoline of that hook. Gives false and a
// line when there is no timer.
bool Add(uint32_t address, const char* name, int filter) {
  if (g_count >= tp::kMaxParts || address == 0) return false;
  if (Known(address)) {
    ModLog("turn parts   : %s at 0x%08X has a timer already. No second timer", name,
           static_cast<unsigned>(address));
    return false;
  }
  uint32_t at = address;
  bool under_hook = false;
  void* original = nullptr;
  MH_STATUS st =
      MH_CreateHook(reinterpret_cast<void*>(at), g_stubs + g_count * kStubBytes, &original);
  if (st == MH_ERROR_ALREADY_CREATED) {
    // The hook of the mod is a jump at the start of the code.
    const uint8_t* code = reinterpret_cast<const uint8_t*>(address);
    uint32_t trampoline = 0;
    int32_t rel = 0;
    if (!IsBadReadPtr(code, 5) && code[0] == 0xE9) {
      memcpy(&rel, code + 1, 4);
      trampoline = FindTrampoline(address + 5 + static_cast<uint32_t>(rel), address);
    }
    if (trampoline == 0 || Known(trampoline)) {
      ModLog("turn parts   : no timer on %s at 0x%08X: the mod hooks it, and the trampoline of "
             "that hook is not found in the hook at 0x%08X",
             name, static_cast<unsigned>(address),
             static_cast<unsigned>(address + 5 + static_cast<uint32_t>(rel)));
      return false;
    }
    at = trampoline;
    under_hook = true;
    st = MH_CreateHook(reinterpret_cast<void*>(at), g_stubs + g_count * kStubBytes, &original);
  }
  if (st != MH_OK) {
    ModLog("turn parts   : no timer on %s at 0x%08X (%s)", name, static_cast<unsigned>(address),
           MH_StatusToString(st));
    return false;
  }
  if (MH_QueueEnableHook(reinterpret_cast<void*>(at)) != MH_OK) {
    MH_RemoveHook(reinterpret_cast<void*>(at));
    ModLog("turn parts   : no timer on %s at 0x%08X (the hook did not queue)", name,
           static_cast<unsigned>(address));
    return false;
  }
  Timer& t = g_timers[g_count];
  t.shown = address;
  t.at = at;
  t.original = original;
  t.filter = filter;
  _snprintf_s(t.name, sizeof(t.name), _TRUNCATE, "%s", name);
  g_names[g_count] = t.name;
  MakeStub(g_count);
  if (under_hook) {
    ModLog("turn parts   : %s at 0x%08X is under a hook of the mod, so its timer is on the code "
           "of the game alone",
           name, static_cast<unsigned>(address));
  }
  ++g_count;
  return true;
}

int IniKey(const char* key, int fallback) {
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return fallback;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  return static_cast<int>(GetPrivateProfileIntA("bwvr", key, fallback, path));
}

void IniText(const char* key, char* out, DWORD size) {
  out[0] = 0;
  char path[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  char* slash = length > 0 && length < MAX_PATH ? strrchr(path, '\\') : nullptr;
  if (slash == nullptr || strlen(path) + 9 >= MAX_PATH) return;
  slash[1] = 0;
  strcat_s(path, MAX_PATH, "bwvr.ini");
  GetPrivateProfileStringA("bwvr", key, "", out, size, path);
}

}  // namespace

static void InstallNow() {
  // turn_parts=1 writes the lines of the log. trace=1 needs the same stubs for
  // the bars of the timeline, with no line.
  g_turn_lines = IniKey("turn_parts", 0) != 0;
  if (g_turn_lines && IniKey("time_log", 1) == 0) {
    ModLog("turn parts   : turn_parts=1 needs time_log=1, which writes the lines. No line");
    g_turn_lines = false;
  }
  if (!g_turn_lines && !g_trace) return;
  QueryPerformanceFrequency(&g_hz);
  g_thread = ThisThreadId();
  g_stubs = static_cast<uint8_t*>(VirtualAlloc(nullptr, tp::kMaxParts * kStubBytes,
                                               MEM_COMMIT | MEM_RESERVE,
                                               PAGE_EXECUTE_READWRITE));
  if (g_stubs == nullptr || g_hz.QuadPart == 0) {
    ModLog("turn parts   : no memory for the stubs, or no clock. No hook");
    return;
  }
  g_ms_per_tick = 1000.0 / static_cast<double>(g_hz.QuadPart);
  g_names[0] = "outside_parts";
  _snprintf_s(g_timers[0].name, sizeof(g_timers[0].name), _TRUNCATE, "outside_parts");
  g_timers[0].shown = 0x0054CC30;  // GGame::ProcessNetworkPackets, the turn itself
  int in = 0;
  int defs_in = 0;
  int graphics_in = 0;
  for (int i = 0; i < kDefCount; ++i) {
    char auto_name[32];
    _snprintf_s(auto_name, sizeof(auto_name), _TRUNCATE, "fn_%08X",
                static_cast<unsigned>(kDefs[i].address));
    const bool ok = Add(kDefs[i].address, kDefs[i].name != nullptr ? kDefs[i].name : auto_name,
                        kDefs[i].filter);
    if (i == kProcessTurnDef) g_process_turn_id = ok ? g_count - 1 : 0;
    in += ok ? 1 : 0;
    defs_in += ok ? 1 : 0;
  }
  // The parts of the graphics engine, for the timeline only.
  if (g_trace) {
    for (int i = 0; i < kGraphicsDefCount; ++i) {
      if (Add(kGraphicsDefs[i].address, kGraphicsDefs[i].name, kGraphicsDefs[i].filter)) {
        g_timers[g_count - 1].graphics_only = true;
        ++in;
        ++graphics_in;
      }
    }
  }
  // The callees of GLandscape::Draw, for the timeline only.
  int draw_in = 0;
  const bool draw_parts = g_trace && IniKey("trace_draw_parts", 1) != 0;
  if (draw_parts) {
    for (int i = 0; i < kDrawDefCount; ++i) {
      char auto_name[32];
      _snprintf_s(auto_name, sizeof(auto_name), _TRUNCATE, "fn_%08X",
                  static_cast<unsigned>(kDrawDefs[i].address));
      if (Add(kDrawDefs[i].address, kDrawDefs[i].name != nullptr ? kDrawDefs[i].name : auto_name,
              kDrawDefs[i].filter)) {
        g_timers[g_count - 1].graphics_only = true;
        g_timers[g_count - 1].draw_part = true;
        ++in;
        ++graphics_in;
        ++draw_in;
      }
    }
  }
  char list[1536];
  IniText("turn_parts_add", list, sizeof(list));
  int more = 0;
  for (char* p = list; *p != 0;) {
    char* end = nullptr;
    const unsigned long a = strtoul(p, &end, 0);
    if (end == p) break;
    char name[32];
    _snprintf_s(name, sizeof(name), _TRUNCATE, "fn_%08lX", a);
    if (Add(static_cast<uint32_t>(a), name, kAll)) {
      ++in;
      ++more;
    }
    p = end;
    while (*p == ',' || *p == ' ') ++p;
  }
  // The probe of the cost of a timed call goes in with the others.
  if (g_trace) QueueCostProbe();
  // One stop of the other threads for all hooks (anim_trig.cpp, HookAll).
  if (in == 0 || g_process_turn_id == 0 || MH_ApplyQueued() != MH_OK) {
    for (int i = 1; i < g_count; ++i) MH_RemoveHook(reinterpret_cast<void*>(g_timers[i].at));
    ModLog("turn parts   : %s. No hook, no line",
           g_process_turn_id == 0 ? "the timer on ProcessTurn failed, so no turn counts"
                                  : "the hooks did not apply");
    g_count = 1;
    return;
  }
  g_turn_parts = true;
  // The device in the bars of the group (trace_device=1).
  g_split_on = g_trace && IniKey("trace_device", 1) != 0;
  g_census_on = g_trace && (draw_parts || g_split_on);
  if (g_split_on) {
    g_rate.Seed(SeedRate());
    g_chain_acc_on = true;
  }
  for (int i = 1; i < g_count; ++i) {
    if (g_timers[i].shown == kAudioTurnAddress) g_audio_turn_id = i;
    // The bar of the timeline: the name of the part and its address.
    _snprintf_s(g_trace_names[i], sizeof(g_trace_names[i]), _TRUNCATE, "%s 0x%08X",
                g_timers[i].name, static_cast<unsigned>(g_timers[i].shown));
    g_trace_defs[i].name = g_trace_names[i];
    g_trace_defs[i].cat = "game";
    if (g_split_on) {
      for (int k = 0; k < 4; ++k) g_trace_defs[i].args[k] = kSplitArgs[k];
    }
  }
  const double cost_ns = g_trace ? MeasureCost() : 0.0;
  if (g_trace) {
    ModLog("turn parts   : trace=1. %d timers give bars to the timeline, %d of them on the "
           "callees of ProcessGraphicsEngine (two levels deep)",
           in, graphics_in);
    ModLog("turn parts   : the cost of one timed call (the clock reads, the stack, the split of "
           "the device and the cap) is %.0f ns on the frame thread, as a measure of 5 rounds of "
           "20000 calls",
           cost_ns);
    ModLog("turn parts   : trace_draw_parts=%d. %s", draw_parts ? 1 : 0,
           draw_parts ? "The callees of GLandscape::Draw and their callees have bars too. A timer "
                        "that is called more than 200 times in a frame is left out from then on"
                      : "No bar on the callees of GLandscape::Draw");
    if (g_split_on) {
      ModLog("turn parts   : trace_device=1. Each bar of the group has the time and the calls of "
             "the device of the mod in it that no bar in it covers (device_us, draws, "
             "state_calls, textures), and each frame has three counters of the device. The "
             "clock of the CPU has %.6f us a tick at the start",
             g_rate.us_per_tick);
    } else {
      ModLog("turn parts   : trace_device=0. The bars have no numbers of the device");
    }
  }
  if (!g_turn_lines) return;
  if (g_audio_turn_id != 0) AudioWatchInit();
  ModLog("turn parts   : turn_parts=1. %d timers on the functions of the game turn (%d of %d "
         "parts and %d more by turn_parts_add). Each 10 s one line with the parts of the turns "
         "of %.0f ms or more, and with slow_frame_ms the parts of the turn in each line of a "
         "long frame. The time of a part is exclusive: less the parts that it calls",
         in, defs_in, kDefCount, more, tp::kLongTurnMs);
}

// From PassGuardInstall, on the frame thread. With trace=1 the install comes
// at the end of the hooks of the mod instead (TurnPartsInstallLate).
void TurnPartsInstall() {
  if (!g_trace) InstallNow();
}

// From mod_main.cpp, after the last hook of the mod, on the frame thread. A
// timer on a function that the mod hooks later would stop that hook
// (MH_ERROR_ALREADY_CREATED), and the many timers of the timeline all go in with
// one MH_ApplyQueued.
void TurnPartsInstallLate() {
  if (g_trace) InstallNow();
}

// h_ProcessNetworkPackets: the call starts. The stack starts with the turn as
// timer 0.
void TurnPartsTurnBegin(int64_t begin) {
  ++g_turn_function_calls;
  if (g_stack.depth != 0) g_stack = wp::Stack();
  g_stack.Push(0, begin, 0, 0);
  g_in_turn = true;
}

// h_ProcessNetworkPackets: the call ends. A call with no game turn (the game
// asks whether one is due) has no call of process_turn and is not a turn.
void TurnPartsTurnEnd(int64_t end) {
  g_in_turn = false;
  AudioWatchGameTurnEnd();
  while (g_stack.depth > 1) g_stack.DropTop();
  if (g_stack.depth == 1) g_stack.CloseTop(end);
  g_lost += g_stack.lost;
  if (g_stack.calls[g_process_turn_id] > 0) {
    static tp::Turn turn;
    turn.FromStack(g_stack, g_count, g_ms_per_tick);
    g_window.AddTurn(turn, g_count);
    if (g_time_log_slow && g_turn_lines) g_frame.AddTurn(turn, g_count);
  }
  g_stack = wp::Stack();
}

// frame_timers.cpp, GraphicsStub: the call of GGame::ProcessGraphicsEngine
// starts and ends on the frame thread. The timers of kGraphicsDefs count
// between the two.
void TurnPartsGraphicsBegin() {
  if (g_stack.depth != 0) g_stack = wp::Stack();
  g_in_graphics = true;
  memset(g_frame_calls, 0, sizeof(g_frame_calls));
  g_timed0 = g_timed_calls;
  if (g_split_on) {
    g_rate.Read(__rdtsc(), Now(), g_hz.QuadPart, 1e6);
    g_frame_acc0 = g_chain_acc;
  }
}

namespace {

// The table of the timers of a window of graphics frames, in the log: the
// calls a frame, the own time a frame with the device in it, and the bars under
// trace_min_us a frame. The timers with the most own time come first.
void CensusLog() {
  const double f = static_cast<double>(g_census.frames);
  const double tick_ms = g_ms_per_tick;
  const double device_ms = g_rate.Us(g_census.device.Ticks()) / 1000.0 / f;
  uint64_t device_total = 0;
  for (int i = 1; i < g_count; ++i) device_total += g_census.device_tsc[i];
  int order[tp::kMaxParts];
  int n = 0;
  for (int i = 1; i < g_count; ++i) {
    if (g_census.calls[i] > 0 || g_census.entered[i] > 0) order[n++] = i;
  }
  for (int a = 1; a < n; ++a) {
    const int v = order[a];
    int b = a;
    while (b > 0 && g_census.own_qpc[order[b - 1]] < g_census.own_qpc[v]) {
      order[b] = order[b - 1];
      --b;
    }
    order[b] = v;
  }
  ModLog("turn parts   : graphics timers, %llu frames: %d timers had calls. The device of the mod "
         "in the graphics call: %.3f ms a frame, %.1f draws, %.1f render states, %.1f stage "
         "states, %.1f textures, %.1f other calls. The device in the bars below: %.3f ms a "
         "frame. %.1f timed calls a frame. Per timer: calls a frame, own ms a frame (the "
         "device in it), bars under trace_min_us a frame",
         static_cast<unsigned long long>(g_census.frames), n, device_ms,
         g_census.device.calls[rc::kDraw] / f, g_census.device.calls[rc::kRenderState] / f,
         g_census.device.calls[rc::kStageState] / f, g_census.device.calls[rc::kTexture] / f,
         g_census.device.calls[rc::kOther] / f, g_rate.Us(device_total) / 1000.0 / f,
         g_census.timed / f);
  char line[1400];
  line[0] = 0;
  int in_line = 0;
  for (int k = 0; k < n && k < 48; ++k) {
    const int i = order[k];
    const size_t used = strlen(line);
    _snprintf_s(line + used, sizeof(line) - used, _TRUNCATE, "%s%s 0x%08X: %.1f, %.3f (%.3f), %.1f%s",
                in_line > 0 ? " | " : "", g_timers[i].name,
                static_cast<unsigned>(g_timers[i].shown),
                (g_timers[i].graphics_only ? g_census.entered[i] : g_census.calls[i]) / f,
                g_census.own_qpc[i] * tick_ms / f,
                g_rate.Us(g_census.device_tsc[i]) / 1000.0 / f, g_census.short_bars[i] / f,
                g_left_out[i] ? " (left out)" : "");
    if (++in_line == 4 || k + 1 == n || k + 1 == 48) {
      ModLog("turn parts   : graphics timers: %s", line);
      line[0] = 0;
      in_line = 0;
    }
  }
  g_census = Census();
}

}  // namespace

void TurnPartsGraphicsEnd() {
  g_in_graphics = false;
  if (g_census_on) {
    ++g_census.frames;
    for (int i = 1; i < g_count; ++i) {
      g_census.calls[i] += g_stack.calls[i];
      g_census.entered[i] += g_frame_calls[i];
      g_census.own_qpc[i] += g_stack.excl[i];
    }
    const uint32_t timed = g_timed_calls - g_timed0;
    g_census.timed += timed;
    TraceCounter(&kTimedCallsDef, static_cast<float>(timed));
  }
  if (g_split_on) {
    // The device in the whole graphics call: three counters of the frame.
    const bar_split_math::Acc d = g_chain_acc.Minus(g_frame_acc0);
    g_census.device.Add(d);
    TraceCounter(&kDeviceCallsDef, static_cast<float>(d.calls[rc::kDraw]),
                 static_cast<float>(d.calls[rc::kRenderState]),
                 static_cast<float>(d.calls[rc::kStageState]),
                 static_cast<float>(d.calls[rc::kTexture]));
    TraceCounter(&kDeviceUsDef, static_cast<float>(g_rate.Us(d.ticks[rc::kDraw])),
                 static_cast<float>(g_rate.Us(d.ticks[rc::kRenderState])),
                 static_cast<float>(g_rate.Us(d.ticks[rc::kStageState])),
                 static_cast<float>(g_rate.Us(d.ticks[rc::kTexture])));
    TraceCounter(&kDeviceOtherDef, static_cast<float>(d.calls[rc::kOther]),
                 static_cast<float>(g_rate.Us(d.ticks[rc::kOther])));
  }
  // A call that no return closed (a long jump) goes away.
  while (g_stack.depth > 0) g_stack.DropTop();
  g_lost += g_stack.lost;
  g_stack = wp::Stack();
  if (g_left_new) {
    g_left_new = false;
    for (int i = 1; i < g_count; ++i) {
      if (g_left_out[i] && g_timers[i].graphics_only && !g_left_logged[i]) {
        g_left_logged[i] = true;
        ModLog("turn parts   : %s 0x%08X was called more than %u times in a frame, so it is a "
               "helper. The timeline leaves it out from now on",
               g_timers[i].name, static_cast<unsigned>(g_timers[i].shown),
               bar_split_math::kCallsCap);
      }
    }
  }
  if (g_census_on && g_census.frames >= kCensusFrames) {
    // The lines go to the log on the frame thread: the bar shows the time.
    const int64_t log_begin = Now();
    CensusLog();
    TraceBarKept(&kCensusLogDef, log_begin, Now());
  }
}

uint32_t TurnPartsTimedCalls() { return static_cast<uint32_t>(g_timed_calls); }

int TurnPartsFrameText(char* out, size_t size) {
  if (!g_turn_parts || !g_turn_lines) {
    if (size > 0) out[0] = 0;
    return 0;
  }
  return tp::FrameText(out, size, g_frame, g_count, g_names);
}

void TurnPartsFrameEnd() {
  if (g_turn_parts) g_frame.Clear();
  AudioWatchFrameEnd();
}

bool TurnPartsWindowLine(double seconds) {
  if (!g_turn_parts || !g_turn_lines) return false;
  static char line[8192];
  g_window.turn_function_calls = g_turn_function_calls;
  g_window.stub_in_turn = g_seen_in_turn;
  g_window.stub_outside = g_seen_outside;
  g_window.stub_other_thread = g_seen_other_thread;
  g_turn_function_calls = g_seen_in_turn = g_seen_outside = g_seen_other_thread = 0;
  tp::WindowLine(line, sizeof(line), g_window, g_count, g_names, seconds);
  ModLog("turn parts   : %s%s", line,
         g_lost > 0 ? ". A call that no return closed was lost (a fault)" : "");
  g_window.Reset();
  return true;
}
